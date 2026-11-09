/*
 * sound.c: the game's side of the sound.
 *
 * Commands queue for the driver and go out on odd frames. The original read
 * the driver's echo of a port and sent 0 while it showed a command; the
 * driver always answers within two frames (it looks at the ports every
 * 16 ms or sooner), so here the game knows the echo from what it sent.
 */
#include <string.h>

#include "audio/music.h"
#include "audio/sound.h"
#include "data/cart.h"
#include "data/unpack.h"

typedef struct {
    uint8_t ring[16], head, count, shown;
} Queue;

static Queue queue[2];
static uint8_t ports[4];
static uint8_t frame;
static uint8_t image[0x10000];
static uint8_t packed[0x8000];
static int battle;

uint8_t sound_muted, sound_player2;
uint8_t sound_hold_a, sound_hold_b;
uint8_t sound_port2, sound_port3, sound_mute_engines;

void sound_init(void)
{
    memset(image, 0, sizeof image);
}

void sound_queue_reset(void)
{
    sound_muted = 0;
    memset(queue, 0, sizeof queue);
    ports[2] = ports[3] = 0;
    music_ports(ports);
}

static void push(Queue *q, uint8_t value)
{
    if (sound_muted || q->count >= 16)
        return;
    q->ring[(q->head + q->count) & 15] = value;
    q->count++;
}

void sound_port0_push(uint8_t command)
{
    push(&queue[0], sound_player2 ? command | 0x80 : command);
}

void sound_port0_push_raw(uint8_t command)
{
    push(&queue[0], command);
}

void sound_port1_push(uint8_t command)
{
    push(&queue[1], command);
}

void sound_set_port0(uint8_t value)
{
    ports[0] = value;
    queue[0].shown = value;
    music_ports(ports);
}

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

void sound_upload_begin(void)
{
    music_stop();
}

/* A block list: length, address and bytes; a length of 0 ends it. */
static void take_blocks(const uint8_t *data, size_t room, int from_battle)
{
    size_t at = 0;

    while (at + 4 <= room) {
        uint16_t length = (uint16_t)(data[at] | data[at + 1] << 8);
        uint16_t to = (uint16_t)(data[at + 2] | data[at + 3] << 8);

        at += 4;
        if (!length)
            break;
        if (to <= 0x10B0 && (uint32_t)to + length > 0x10B0)
            battle = from_battle;
        memcpy(&image[to], &data[at], length);
        at += length;
    }
}

/* From the cartridge, where a list runs on across banks: the last list's
   room is a whole 32 KB bank at a time, so it is read through one copy. */
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
    music_bank_set(image, battle);
}
