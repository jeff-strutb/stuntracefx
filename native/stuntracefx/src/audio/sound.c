/*
 * sound.c: the game's side of the sound.
 *
 * Commands queue for the driver and go out on odd frames. The original read
 * the driver's echo of a port and sent 0 while it showed a command; the
 * driver always answers within two frames (it looks at the ports every
 * 16 ms or sooner), so here the game knows the echo from what it sent.
 *
 * A load builds one image of the driver's memory on the game thread and hands
 * it over whole. The original's settling delays were time for the sound CPU
 * to act on a command and have no counterpart: the image is ready in the same
 * update.
 */
#include <string.h>

#include "audio/music.h"
#include "audio/sound.h"
#include "data/cart.h"
#include "data/unpack.h"
#include "system/state.h"

typedef struct {
    uint8_t ring[16], head, count, shown;
} Queue;

static Queue queue[2];
static uint8_t ports[4];
static uint8_t frame;
static uint8_t image[0x10000];
static uint8_t packed[0x8000];
static int battle;

uint8_t sound_muted;
uint8_t sound_hold_a, sound_hold_b;
uint8_t sound_alt_set_loaded;
uint8_t sound_port2, sound_port3, sound_mute_engines;

void sound_init(void)
{
    memset(image, 0, sizeof image);
}

/*
 * @routine 04:D6E1 SoundReset
 * WHAT IT DOES: Empties both sound queues, unmutes, and puts 0 on ports 3
 * and 2. A port's shown command stays shown, as a port the driver has not
 * answered does. (The original also cleared $0DD0, $0DD2, $0E1B and $0E1C,
 * which nothing reads.)
 */
void SoundReset(void)
{
    int q;

    sound_muted = 0;
    for (q = 0; q < 2; q++)
        queue[q].head = queue[q].count = 0;
    ports[2] = ports[3] = 0;
    music_ports(ports);
}

/* Append a byte to a ring, unless sound is muted or the ring is full. */
static void push(Queue *q, uint8_t value)
{
    if (sound_muted || q->count >= 16)
        return;
    q->ring[(q->head + q->count) & 15] = value;
    q->count++;
}

/*
 * @routine 04:D649 SoundPort0QueuePushRaw
 * WHAT IT DOES: Queues a sound command for port 0 exactly as given.
 */
void SoundPort0QueuePushRaw(uint8_t command)
{
    push(&queue[0], command);
}

/*
 * WHAT IT DOES: Queues a sound command for port 0 for the player whose car
 * is being processed: player 2's commands have bit 7 set.
 */
void SoundPort0QueuePush(uint8_t command)
{
    push(&queue[0], g.current_player ? (uint8_t)(command | 0x80) : command);
}

/*
 * @routine 04:D69F SoundPort1QueuePush
 * WHAT IT DOES: Queues a sound command for port 1.
 */
void SoundPort1QueuePush(uint8_t command)
{
    push(&queue[1], command);
}

void sound_set_port0(uint8_t value)
{
    ports[0] = value;
    queue[0].shown = value;
    music_ports(ports);
}

/* One ring to its port: 0 while the port still shows a command, else the
   oldest byte (the head steps on unless that was the last). */
static void ring_send(Queue *q, uint8_t *port)
{
    if (q->shown) {
        q->shown = 0;
        *port = 0;
        return;
    }
    if (!q->count)
        return;
    *port = q->shown = q->ring[q->head];
    q->count--;
    if (q->count)
        q->head = (q->head + 1) & 15;
}

/*
 * @routine 7E:B112 SoundPortsSend
 * WHAT IT DOES: The frame's sound output: unless a hold is set, on odd frames
 * each port gets 0 while it still shows the last command, else the next byte
 * of its queue, then ports 2 and 3 get their values (0 while the engine
 * sounds are muted). The original's direct bytes $10E2 and $10E3 are never
 * written anywhere, so that path does not exist here.
 */
void sound_frame(void)
{
    frame++;
    if (sound_hold_a || sound_hold_b || !(frame & 1))
        return;
    ring_send(&queue[0], &ports[0]);
    ring_send(&queue[1], &ports[1]);
    ports[2] = sound_mute_engines ? 0 : sound_port2;
    ports[3] = sound_mute_engines ? 0 : sound_port3;
    music_ports(ports);
}

/* ---- the driver's memory ---- */

void sound_upload_begin(void)
{
    music_stop();
}

/* A block list: length, address and bytes; a length of 0 ends it. Addresses
   wrap at the end of the driver's memory, as the sound CPU's loader's did. */
static void take_blocks(const uint8_t *data, size_t room, int from_battle)
{
    size_t at = 0;

    while (at + 4 <= room) {
        uint16_t length = (uint16_t)(data[at] | data[at + 1] << 8);
        uint16_t to = (uint16_t)(data[at + 2] | data[at + 3] << 8);
        uint32_t i;

        at += 4;
        if (!length)
            break;
        if (to <= 0x10B0 && (uint32_t)to + length > 0x10B0)
            battle = from_battle;
        for (i = 0; i < length && at + i < room; i++)
            image[(uint16_t)(to + i)] = data[at + i];
        at += length;
    }
}

/*
 * @routine 04:D720 ApuUpload
 * WHAT IT DOES: A block list from the cartridge, where a list runs on across
 * banks (the last list's room is a whole 32 KB bank at a time, so it is read
 * through one copy). The battle driver is the list at $1A:F091.
 */
void sound_upload_cart(uint8_t bank, uint16_t addr)
{
    static uint8_t copy[0x20000];
    size_t n = 0;
    uint8_t b = bank;
    uint32_t a = addr;

    while (n < sizeof copy && b < 0x20) {
        copy[n++] = cart8(b, (uint16_t)a);
        if (++a > 0xFFFF) {
            a = 0x8000;
            b++;
        }
    }
    take_blocks(copy, n, bank == 0x1A && addr == 0xF091);
}

/* A packed block: which is its end in the cartridge's bank. */
void sound_upload_packed(uint16_t which, uint8_t bank)
{
    uint32_t n = unpack((uint32_t)bank << 16 | which, packed, sizeof packed);

    take_blocks(packed, n, 0);
}

void sound_upload_end(void)
{
    int q;

    /* the driver starts by clearing what its ports show and what they read */
    memset(ports, 0, sizeof ports);
    for (q = 0; q < 2; q++)
        queue[q].shown = 0;
    music_ports(ports);
    music_bank_set(image, battle);
}

/* ---- the game's loads ---- */

/* The tables of block lists in bank $04, three bytes an entry: a position
   ($FFFF for none, 0 for the empty list at the start of work RAM) and a bank. */
enum {
    COURSE_SAMPLES = 0xD322,    /* a packed block and its bank, by course */
    COURSE_EFFECTS = 0xD376,
    COURSE_EXTRA = 0xD3CA,
    MUSIC_FIRST = 0xD41E,       /* by music */
    MUSIC_SECOND = 0xD42D
};

/* The block list in a table's entry, if it has one. An entry of 0 is the
   original's upload from the start of work RAM, which holds an empty list. */
static void upload_listed(uint16_t entry)
{
    uint16_t addr = cart16(0x04, entry);

    if (addr != 0xFFFF && addr != 0)
        sound_upload_cart(cart8(0x04, (uint16_t)(entry + 2)), addr);
}

/*
 * @routine 03:F05E SoundDataLoad
 * WHAT IT DOES: Sound block `which`: $8000, $91BC and $AFC6 are in bank $1E
 * as they are ($11F8, $11F8 and $0DAC bytes), anything else is packed and
 * `input` is its bank.
 */
void SoundDataLoad(uint16_t which, uint8_t input)
{
    if (which == 0x8000 || which == 0x91BC)
        take_blocks(cart_at(0x1E, which), 0x11F8, 0);
    else if (which == 0xAFC6)
        take_blocks(cart_at(0x1E, which), 0x0DAC, 0);
    else
        sound_upload_packed(which, input);
}

/*
 * @routine 04:D251 MusicLoadForCourse
 * WHAT IT DOES: Music `music`'s sound data: the first block list from the
 * music table; then, with two players on the split screen ($0D2B = 2), $1000
 * bytes of the block for player 2's car from bank $19, moved to their second
 * set of voices (+$10 on the block address, +$D30 on the sample directory
 * entries and the sample block's address); otherwise the second list.
 */
void MusicLoadForCourse(uint8_t music)
{
    static const struct { uint16_t off, add; } patch[4] = {
        {0x0002, 0x0010}, {0x0004, 0x0D30}, {0x0006, 0x0D30}, {0x0016, 0x0D30}};
    static uint8_t copy[0x1000];
    uint16_t i = (uint16_t)((music & 0x0F) * 3);
    uint16_t src;
    unsigned n;

    upload_listed((uint16_t)(MUSIC_FIRST + i));
    if (g.hud_mode != 2) {
        upload_listed((uint16_t)(MUSIC_SECOND + i));
        return;
    }
    i = (uint16_t)((g.car[1] & 0x0F) * 3);
    src = cart16(0x04, (uint16_t)(MUSIC_FIRST + i));
    for (n = 0; n < sizeof copy; n++)
        copy[n] = cart8(0x19, (uint16_t)(src + n));
    for (n = 0; n < 4; n++) {
        uint16_t v = (uint16_t)(copy[patch[n].off] | copy[patch[n].off + 1] << 8);

        v = (uint16_t)(v + patch[n].add);
        copy[patch[n].off] = (uint8_t)v;
        copy[patch[n].off + 1] = (uint8_t)(v >> 8);
    }
    take_blocks(copy, sizeof copy, 0);
}

/*
 * @routine 04:D44C ApuUploadBank19
 * WHAT IT DOES: Uploads the sound program, the block list at $19:C27C.
 */
void ApuUploadBank19(void)
{
    sound_upload_begin();
    sound_upload_cart(0x19, 0xC27C);
    sound_upload_end();
}

/*
 * @routine 04:D0DB CourseSoundLoad
 * WHAT IT DOES: Loads the course's sound unless the first hold is set. With
 * the second hold set it is the other set, loaded once. Otherwise, by the
 * course: the samples, the effects block, the music for the car and the
 * block for the course's extra sounds, then in the stunt mode without the
 * blanked HUD a last block. (The original last waits for the driver to
 * report its ports clear; the driver has not run yet when the load ends.)
 */
void CourseSoundLoad(void)
{
    uint16_t x;

    if (sound_hold_a)
        return;
    if (sound_hold_b) {
        if (sound_alt_set_loaded)
            return;
        sound_alt_set_loaded = 0xFF;
        sound_upload_begin();
        sound_upload_cart(0x19, 0x8D4C);
        take_blocks(cart_at(0x1E, 0x9B2C), 0x1518, 0);
        sound_upload_cart(0x1B, 0xB8A0);
        sound_upload_end();
        SoundReset();
        sound_set_port0(4);
        return;
    }
    x = (uint16_t)(g.course * 3 & 0xFF);
    sound_upload_begin();
    SoundDataLoad(cart16(0x04, (uint16_t)(COURSE_SAMPLES + x)), cart8(0x04, (uint16_t)(COURSE_SAMPLES + 2 + x)));
    upload_listed((uint16_t)(COURSE_EFFECTS + x));
    MusicLoadForCourse(g.car[0]);
    upload_listed((uint16_t)(COURSE_EXTRA + x));
    if (g.hud_mode == 1 && !g.hud_blank)
        SoundDataLoad(0xF9B8, 0x1D);
    sound_upload_end();
}

/*
 * @routine 03:F0E6 RaceLoad (its sound part)
 * WHAT IT DOES: Chooses the sounds (SoundIndicesFromHandlers) and loads the
 * race's sound data, then starts the driver with 1 on port 0.
 */
void RaceLoadSound(void)
{
    SoundIndicesFromHandlers();
    sound_upload_begin();
    SoundDataLoad(0xE049, 0x1E);
    sound_upload_cart(0x1B, 0xB8A0);
    sound_upload_cart(0x1B, 0xD5FC);
    sound_upload_end();
    SoundReset();
    sound_set_port0(1);
}

/*
 * @routine 03:D8B3 RaceModeSetup (the pick's sound)
 * WHAT IT DOES: The first racer's handler is kept in $0D8C and its sound
 * index in $0D8B.
 */
void RaceModeSetupPickSound(void)
{
    g.car_handler_kept = g.selection_handler[0];
    g.car_index_kept = HandlerSoundIndex(g.selection_handler[0]);
}

/*
 * @routine 03:D8B3 RaceModeSetup (the battle driver)
 * WHAT IT DOES: In mode 2 the sound program is replaced by the battle driver,
 * the block list at $1A:F091.
 */
void RaceModeSetupBattleSound(void)
{
    sound_upload_begin();
    sound_upload_cart(0x1A, 0xF091);
    sound_upload_end();
}

/*
 * @routine 07:AB8C SoftResetApu
 * WHAT IT DOES: Restarts the sound for the soft reset: the driver is told to
 * leave ($06 on port 1, the same stop here) and the sound program is uploaded
 * again.
 */
void SoftResetApu(void)
{
    ApuUploadBank19();
}

/*
 * @routine 07:EBE9 SoftResetToBootWarm (its sound part)
 * WHAT IT DOES: Reloads the title's sound: the block $F3B5 and the lists
 * $1B:85F8 and $19:9A98, then 1 on port 0.
 */
void SoftResetToBootWarmSound(void)
{
    sound_upload_begin();
    SoundDataLoad(0xF3B5, 0x1E);
    sound_upload_cart(0x1B, 0x85F8);
    sound_upload_cart(0x19, 0x9A98);
    sound_upload_end();
    SoundReset();
    sound_set_port0(1);
}
