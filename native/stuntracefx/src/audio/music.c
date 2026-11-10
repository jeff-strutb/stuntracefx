/*
 * music.c: the music code the engine runs on its audio thread, and the
 * handover from the game thread. Sound banks are built whole on the game
 * thread and passed by pointer, so the audio thread never waits.
 */
#include <string.h>

#include "driver.h"
#include "driver_sfx.h"
#include "music.h"

enum { MESSAGE_BANK, MESSAGE_STOP, MESSAGE_PUSH, MESSAGE_QUEUES_RESET, MESSAGE_PORT0, MESSAGE_FRAME, MESSAGE_PAUSE };

typedef struct {
    uint8_t kind, battle;
    uint8_t port, value;                /* a push, or port 0's byte */
    uint8_t held, port2, port3;         /* a frame */
    const SoundBank *bank;
} Message;

/*
 * The original's two command rings and its frame's sends to the ports live
 * here, beside the driver, because the sends wait on the driver's echo of each
 * port: a port showing a command gets 0 only once the driver shows that it
 * has taken it. The game thread pushes commands and says when a frame ends.
 */
typedef struct {
    uint8_t ring[16], head, count;
} Queue;

static Queue queue[2];
static uint8_t wire[4];                 /* what the game has on the four ports */
static uint32_t samples;
static uint8_t paused;                  /* the pause screen: the sound silent, the driver held */

/* Whether the echo on ports 0 and 1 answers what they now carry: a count of
   the changes to each port, the count a music tick's exchange took in, the
   count a sound tick then read, and the count the echo the next exchange
   shows was read from. The original sent the next byte as soon as a port's
   echo showed 0, which it also shows after the driver has read a new byte and
   before it has echoed it; a command sent twice running was then heard once
   (seen in a race: two of 227 commands). Here a port changes only on an echo
   of what it carries. */
static uint8_t wire_count[2], latched_count[2], read_count[2], echo_count[2];

static void wire_set(int q, uint8_t value)
{
    if (wire[q] != value)
        wire_count[q]++;
    wire[q] = value;
}

/* Append a byte unless the ring is full. An empty ring takes it at its head. */
static void queue_push(Queue *q, uint8_t value)
{
    if (q->count >= 16)
        return;
    q->ring[(q->head + q->count) & 15] = value;
    q->count++;
}

/* One ring to its port: 0 while the driver still shows a command there, else
   the oldest byte (the head steps on unless that was the last). */
static void ring_send(int port)
{
    Queue *q = &queue[port];

    if (echo_count[port] != wire_count[port])
        return;                         /* the echo is from before this port's last change */
    if (s.port_out[port]) {
        wire_set(port, 0);
        return;
    }
    if (!q->count)
        return;
    wire_set(port, q->ring[q->head]);
    q->count--;
    if (q->count)
        q->head = (q->head + 1) & 15;
}

/*
 * @routine 7E:B112 SoundPortsSend
 * WHAT IT DOES: The frame's sound output: unless a hold is set, each port
 * gets 0 while the driver still shows its last command, else the next byte of
 * its queue, then ports 2 and 3 get their values. (The original sent on odd
 * frames only, its interrupt's pace; here every frame, each send still waiting
 * on the driver's echo.)
 */
static void ports_send(const Message *m)
{
    if (m->held)
        return;
    ring_send(0);
    ring_send(1);
    wire[2] = m->port2;
    wire[3] = m->port3;
}

/* The four port exchanges after a music tick: the driver shows the bytes it
   keeps for the ports and reads what the game has put there. */
static void port_exchange(void)
{
    echo_count[0] = read_count[0];
    echo_count[1] = read_count[1];
    s.port_out[0] = s.port_seen;
    s.port_out[1] = s.port_copy[1];
    s.port_out[2] = s.port_copy[2];
    s.port_out[3] = s.port_copy[3];
    memcpy(s.port_latch, wire, 4);
    latched_count[0] = wire_count[0];
    latched_count[1] = wire_count[1];
}

void music_pass(void)
{
    uint8_t tempo;

    d.sfx_acc = (uint8_t)(d.sfx_acc + 0x20);
    if (d.sfx_acc < 0x20) {
        if (s.battle)
            battle_sound_tick();
        else
            race_sound_tick();
        read_count[0] = latched_count[0];
        read_count[1] = latched_count[1];
        if (d.echo_delay_wait != d.echo_delay)
            d.echo_delay_wait++;
    }
    tempo = (uint8_t)(d.tempo >> 8);
    d.music_acc = (uint8_t)(d.music_acc + tempo);
    if (d.music_acc < tempo) {
        driver_music_tick();
        port_exchange();
    } else {
        driver_between_ticks();
    }
    driver_output();
}

/* 32,040 times a second: the driver's timer is 64 samples */
void music_tick(void)
{
    Message m;
    int q;

    if (++samples < 64)
        return;
    samples = 0;
    while (nshvc_music_receive(&m, sizeof m)) {
        switch (m.kind) {
        case MESSAGE_BANK:              /* the driver starts: its ports read 0 and show 0 */
            d.bank = m.bank;
            driver_reset();
            s.battle = m.battle;
            memset(wire, 0, sizeof wire);
            for (q = 0; q < 2; q++)
                wire_count[q] = latched_count[q] = read_count[q] = echo_count[q] = 0;
            break;
        case MESSAGE_STOP:
            driver_leave();
            break;
        case MESSAGE_PUSH:
            queue_push(&queue[m.port & 1], m.value);
            break;
        case MESSAGE_QUEUES_RESET:
            for (q = 0; q < 2; q++)
                queue[q].head = queue[q].count = 0;
            wire[2] = wire[3] = 0;
            break;
        case MESSAGE_PORT0:
            wire_set(0, m.value);
            break;
        case MESSAGE_FRAME:
            ports_send(&m);
            break;
        case MESSAGE_PAUSE:
            paused = m.value;
            nshvc_sound_mute(paused);
            break;
        default:
            break;
        }
    }
    if (d.bank && !paused)
        music_pass();
}

/* ---- the game thread ---- */

static void send(const Message *m) { nshvc_music_send(m, sizeof *m); }

void music_push(int port, uint8_t value)
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_PUSH;
    m.port = (uint8_t)port;
    m.value = value;
    send(&m);
}

void music_queues_reset(void)
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_QUEUES_RESET;
    send(&m);
}

void music_port0(uint8_t value)
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_PORT0;
    m.value = value;
    send(&m);
}

void music_frame(int held, uint8_t port2, uint8_t port3)
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_FRAME;
    m.held = (uint8_t)(held != 0);
    m.port2 = port2;
    m.port3 = port3;
    send(&m);
}

void music_stop(void)
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_STOP;
    send(&m);
}

void music_pause(int on)
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_PAUSE;
    m.value = (uint8_t)(on != 0);
    send(&m);
}

/* the banks: two buffers, the audio thread using one while the game fills the other */
static SoundBank banks[2];
static int bank_in_use = -1;

/* Each distinct sample is loaded once. */
#define KNOWN_SAMPLES 1024
static struct { uint64_t hash; nshvc_sample sample; } known[KNOWN_SAMPLES];
static int known_count;

static nshvc_sample sample_of(const uint8_t *data, uint32_t size, uint32_t loop)
{
    uint64_t h = 14695981039346656037ull ^ loop;
    uint32_t i;
    int k;
    nshvc_sample sample;

    for (i = 0; i < size; i++)
        h = (h ^ data[i]) * 1099511628211ull;
    for (k = 0; k < known_count; k++)
        if (known[k].hash == h)
            return known[k].sample;
    sample = nshvc_sample_load(data, size, NSHVC_SAMPLE_BRR, loop);
    if (known_count < KNOWN_SAMPLES) {
        known[known_count].hash = h;
        known[known_count].sample = sample;
        known_count++;
    }
    return sample;
}

/*
 * The original's source $23 is 28 blocks at $FF00 that the driver rewrites
 * with its noise generator while they play. Samples here do not change once
 * made, so each header the sound effects ask for ($FD) gets one long noise
 * made once, from the same generator and the same masks, at the original's
 * four steps a block. A header without the loop flag keeps the 28 blocks, so
 * that the voice stops when the original's would. One period of the
 * generator (32,767 steps, about 8,192 blocks).
 */
#define NOISE_LONG_BLOCKS 8192
#define NOISE_SHORT_BLOCKS 28
static uint8_t noise_data[NOISE_LONG_BLOCKS * 9];

static nshvc_sample noise_make(uint8_t header)
{
    uint32_t blocks = (header & 2) ? NOISE_LONG_BLOCKS : NOISE_SHORT_BLOCKS;
    uint32_t b, at = 0;
    uint16_t lfsr = (uint16_t)(header << 8 | 1);
    int step;

    for (b = 0; b < blocks; b++) {
        noise_data[at++] = b + 1 == blocks ? (uint8_t)(header | 1) : header;
        for (step = 0; step < 4; step++) {
            uint8_t hi = (uint8_t)(lfsr >> 8), lo = (uint8_t)lfsr;
            uint8_t in = (uint8_t)(((hi >> 6) ^ (hi >> 5)) & 1);

            lfsr = (uint16_t)((uint16_t)(hi << 1 | lo >> 7) << 8 | (uint8_t)(lo << 1 | in));
            noise_data[at++] = (uint8_t)(((lfsr >> 8) & 0xD0) | 0x11);
            noise_data[at++] = (uint8_t)((lfsr & 0xFF) | 0x11);
        }
    }
    return sample_of(noise_data, at, 0);
}

/* The headers the bank's sound effects use: each script sets sample $23 and
   then its header. */
static void noise_samples_make(SoundBank *b, const uint8_t *image)
{
    uint32_t i;

    memset(b->noise, 0, sizeof b->noise);
    for (i = 0; i + 3 < 0x10000; i++) {
        if (image[i] == 0xE3 && image[i + 1] == 0x23 && image[i + 2] == 0xFD) {
            uint8_t header = (uint8_t)(image[i + 3] & 0xFE);

            if (!b->noise[header >> 1].id)
                b->noise[header >> 1] = noise_make(header);
        }
    }
}

/* A sound memory image and its samples as a bank the driver can use. */
void music_bank_fill(SoundBank *b, const uint8_t image[0x10000])
{
    int e;

    memcpy(b->bytes, image, 0x10000);
    noise_samples_make(b, image);
    for (e = 0; e < DRIVER_SAMPLES; e++) {
        uint16_t start = (uint16_t)(image[BANK_SAMPLE_DIRECTORY + e * 4] | image[BANK_SAMPLE_DIRECTORY + e * 4 + 1] << 8);
        uint16_t loop = (uint16_t)(image[BANK_SAMPLE_DIRECTORY + e * 4 + 2] | image[BANK_SAMPLE_DIRECTORY + e * 4 + 3] << 8);
        uint32_t size = 0;

        memset(&b->samples[e], 0, sizeof b->samples[e]);
        if (start < 0x200)
            continue;
        while ((uint32_t)start + size + 9 <= 0x10000 && size < 0x10000) {
            uint8_t header = image[(uint32_t)start + size];

            size += 9;
            if (header & 1)
                break;
        }
        if (size)
            b->samples[e] = sample_of(image + start, size, loop >= start ? (uint32_t)(loop - start) : 0);
    }
}

void music_bank_set(const uint8_t image[0x10000], int battle)
{
    int next = bank_in_use == 0 ? 1 : 0;
    SoundBank *b = &banks[next];
    Message m;

    music_bank_fill(b, image);
    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_BANK;
    m.battle = (uint8_t)battle;
    m.bank = b;
    send(&m);
    bank_in_use = next;
}
