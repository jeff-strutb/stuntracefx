/*
 * music.c: the music code the engine runs on its audio thread, and the
 * handover from the game thread. Sound banks are built whole on the game
 * thread and passed by pointer, so the audio thread never waits.
 */
#include <string.h>

#include "driver.h"
#include "driver_sfx.h"
#include "music.h"

enum { MESSAGE_PORTS, MESSAGE_BANK, MESSAGE_STOP };

typedef struct {
    uint8_t kind, battle;
    uint8_t ports[4];
    const SoundBank *bank;
} Message;

static uint8_t wire[4];
static uint32_t samples;

void music_pass_with_wire(const uint8_t *w)
{
    memcpy(wire, w, 4);
    music_pass();
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
        if (d.echo_delay_wait != d.echo_delay)
            d.echo_delay_wait++;
    }
    tempo = (uint8_t)(d.tempo >> 8);
    d.music_acc = (uint8_t)(d.music_acc + tempo);
    if (d.music_acc < tempo) {
        driver_music_tick();
        memcpy(s.port_latch, wire, 4);          /* the four port exchanges after a music tick */
    } else {
        driver_between_ticks();
    }
    driver_output();
}

/* 32,040 times a second: the driver's timer is 64 samples */
void music_tick(void)
{
    Message m;

    if (++samples < 64)
        return;
    samples = 0;
    while (nshvc_music_receive(&m, sizeof m)) {
        if (m.kind == MESSAGE_PORTS) {
            memcpy(wire, m.ports, 4);
        } else if (m.kind == MESSAGE_BANK) {
            d.bank = m.bank;
            driver_reset();
            s.battle = m.battle;
        } else {
            driver_leave();
        }
    }
    if (d.bank)
        music_pass();
}

/* ---- the game thread ---- */

static void send(const Message *m) { nshvc_music_send(m, sizeof *m); }

void music_ports(const uint8_t ports[4])
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_PORTS;
    memcpy(m.ports, ports, 4);
    send(&m);
}

void music_stop(void)
{
    Message m;

    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_STOP;
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

void music_bank_set(const uint8_t image[0x10000], int battle)
{
    int next = bank_in_use == 0 ? 1 : 0, e;
    SoundBank *b = &banks[next];
    Message m;

    memcpy(b->bytes, image, 0x10000);
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
    memset(&m, 0, sizeof m);
    m.kind = MESSAGE_BANK;
    m.battle = (uint8_t)battle;
    m.bank = b;
    send(&m);
    bank_in_use = next;
}
