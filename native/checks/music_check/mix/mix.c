/*
 * mix.c: the converted sound driver through the engine's own mixer, driven
 * by the port values the original's SPC700 read in a recording (the check's
 * counterpart to music_check, which compares voice events with the mixer
 * left out). The driver runs in this program's own music code, one pass every
 * 64 DSP samples, with the banks made from the recording's sound memory.
 *
 *   srfx_music_mix RECORD --headless --sound OUT.raw --updates N
 *
 * RECORD is sfxbox's --native directory; OUT.raw is 16-bit stereo at 32,040 Hz
 * to compare with the recording's sound.raw.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/driver.h"
#include "audio/driver_sfx.h"
#include "audio/music.h"
#include "nshvc/nshvc.h"

#define APU_AT (0x20000 + 0x10000 + 0x10000 + 544 + 512)
#define STATE_SIZE (APU_AT + 0x10000)
#define MAX_STARTS 256

static uint8_t machine[STATE_SIZE];
static FILE *state_f;
static uint32_t state_frame;

static void state_to_frame(uint32_t want)
{
    uint32_t head[3], o;

    while (state_frame < want && fread(head, 4, 3, state_f) == 3) {
        while (fread(&o, 4, 1, state_f) == 1 && o != 0xFFFFFFFFu) {
            uint32_t n = STATE_SIZE - o < 64 ? STATE_SIZE - o : 64;

            if (fread(machine + o, 1, n, state_f) != n)
                return;
        }
        if (head[1] == 0)
            state_frame = head[0] + 1;
    }
}

static uint8_t *load(const char *dir, const char *name, size_t *n)
{
    char p[4096];
    FILE *f;
    uint8_t *data;
    long size;

    snprintf(p, sizeof p, "%s/%s", dir, name);
    if (!(f = fopen(p, "rb"))) {
        fprintf(stderr, "no %s\n", p);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (uint8_t *)malloc((size_t)size);
    if (fread(data, 1, (size_t)size, f) != (size_t)size)
        exit(1);
    fclose(f);
    *n = (size_t)size;
    return data;
}

static uint8_t *spc;
static size_t nspc, spc_at;
static uint32_t reset_sample[MAX_STARTS], stop_sample[MAX_STARTS];
static int nresets, current = -1, running;
static SoundBank *banks;
static uint8_t battle_flag[MAX_STARTS];
static uint8_t wire[4];
static uint64_t ticks;

/* the driver's memory at a start: the uploads have settled four frames after
   the last start of a burst; six frames is before any later change and the
   driver has barely started */
static void banks_make(void)
{
    int i;

    banks = (SoundBank *)calloc((size_t)nresets, sizeof *banks);
    for (i = 0; i < nresets; i++) {
        state_to_frame(reset_sample[i] / 534 + 6);
        music_bank_fill(&banks[i], machine + APU_AT);
        battle_flag[i] = banks[i].bytes[0x10E3] != 0xC4;
    }
}

static void init(void)
{
    banks_make();
}

static void update(const nshvc_input *input)
{
    (void)input;
}

static void draw(float between)
{
    (void)between;
}

static void silence(void)
{
    int v;

    for (v = 0; v < DRIVER_VOICES; v++)
        nshvc_voice_key_off((uint32_t)v);
}

/* music_pass, for the key-on log (MIX_KEYLOG=FILE): each key-on with its pass's time */
static FILE *keylog, *tempolog;

static void pass(void)
{
    uint8_t tempo;
    int v;

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
        memcpy(s.port_latch, wire, 4);
    } else {
        driver_between_ticks();
    }
    if (keylog)
        for (v = 0; v < DRIVER_VOICES; v++)
            if (d.key_on & (1u << v))
                fprintf(keylog, "%llu %d %d\n", (unsigned long long)ticks, v, d.voice[v].source);
    driver_output();
}

/* one pass of the driver with the recorded ports, every 64 samples from the
   driver's start, as its timer runs */
static void tick(void)
{
    uint32_t now = (uint32_t)ticks++;

    while (current + 1 < nresets && now >= reset_sample[current + 1]) {
        current++;
        d.bank = &banks[current];
        driver_reset();
        s.battle = battle_flag[current];
        running = 1;
    }
    if (current < 0 || ((now - reset_sample[current]) & 63))
        return;
    if (now >= stop_sample[current]) {
        if (running)
            silence();
        running = 0;
        return;
    }
    while (spc_at + 8 <= nspc) {
        uint32_t t = (uint32_t)spc[spc_at] | (uint32_t)spc[spc_at + 1] << 8 | (uint32_t)spc[spc_at + 2] << 16 | (uint32_t)spc[spc_at + 3] << 24;
        uint16_t addr = (uint16_t)(spc[spc_at + 5] | spc[spc_at + 6] << 8);

        if (t / 64 > now)         /* the log's times are the SPC700's clocks, 64 to a sample */
            break;
        if (!spc[spc_at + 4] && addr >= 0xF4 && addr <= 0xF7)
            wire[addr - 0xF4] = spc[spc_at + 7];
        spc_at += 8;
    }
    if (running)
        pass();
    if (tempolog && (now % 534) < 64 && running)
        fprintf(tempolog, "%u %u %u %u %u %u %u\n", now, d.tempo, d.music_acc, d.sfx_acc, d.song, d.flags, d.pattern);
}

int main(int argc, char **argv)
{
    static nshvc_game game;
    uint8_t *apu;
    size_t napu, i;
    char p[4096];

    if (argc < 2) {
        fprintf(stderr, "usage: srfx_music_mix RECORD --headless --sound OUT --updates N\n");
        return 2;
    }
    apu = load(argv[1], "apu.bin", &napu);
    spc = load(argv[1], "spc.bin", &nspc);
    for (i = 65536; i + 8 <= napu; i += 8) {
        uint32_t sample = (uint32_t)apu[i] | (uint32_t)apu[i + 1] << 8 | (uint32_t)apu[i + 2] << 16 | (uint32_t)apu[i + 3] << 24;
        uint16_t addr = (uint16_t)(apu[i + 5] | apu[i + 6] << 8);
        uint8_t v = apu[i + 7];

        if (addr == 0x00F1 && v == 0x30 && nresets < MAX_STARTS) {
            reset_sample[nresets] = sample;
            stop_sample[nresets] = 0xFFFFFFFFu;
            nresets++;
        } else if (addr == 0x00F4 && v == 0xAA && nresets && stop_sample[nresets - 1] == 0xFFFFFFFFu) {
            stop_sample[nresets - 1] = sample;
        }
    }
    snprintf(p, sizeof p, "%s/state.bin", argv[1]);
    if (getenv("MIX_TEMPOLOG"))
        tempolog = fopen(getenv("MIX_TEMPOLOG"), "w");
    if (getenv("MIX_KEYLOG"))
        keylog = fopen(getenv("MIX_KEYLOG"), "w");
    state_f = fopen(p, "rb");
    if (!state_f) {
        fprintf(stderr, "no %s\n", p);
        return 1;
    }
    game.title = "SRFX music mix";
    game.id = "srfx-music-mix";
    game.update_hz = 60;
    game.init = init;
    game.update = update;
    game.draw = draw;
    game.music.hz = 32040;
    game.music.tick = tick;
    return nshvc_run(&game, argc, argv);
}
