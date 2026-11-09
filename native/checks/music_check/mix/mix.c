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

static uint8_t *spc, *apu;
static size_t nspc, spc_at, napu;
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

/* music_pass in its two halves, for the log and the recorded pacing: the tick's
   work with the timer periods it covers, and the DSP writes the next loop opens
   with. The accumulators are 8-bit adds of the product's low byte, so a pass of
   many periods can lose a carry, as the original's does. */
static FILE *keylog, *tempolog;
static int output_due, latch_due, recorded_passes;

static void pass_work(uint8_t periods)
{
    uint8_t add = (uint8_t)(periods * 0x20), tempo;

    d.sfx_acc = (uint8_t)(d.sfx_acc + add);
    if (d.sfx_acc < add) {
        if (s.battle)
            battle_sound_tick();
        else
            race_sound_tick();
        if (d.echo_delay_wait != d.echo_delay)
            d.echo_delay_wait++;
    }
    tempo = (uint8_t)((d.tempo >> 8) * periods);
    d.music_acc = (uint8_t)(d.music_acc + tempo);
    if (d.music_acc < tempo) {
        driver_music_tick();
        if (recorded_passes)
            latch_due = 1;      /* the original reads its ports as the tick ends */
        else
            memcpy(s.port_latch, wire, 4);
    } else {
        driver_between_ticks();
    }
    output_due = 1;
}

static void pass_output(void)
{
    int v;

    if (!output_due)
        return;
    output_due = 0;
    if (latch_due) {
        memcpy(s.port_latch, wire, 4);
        latch_due = 0;
    }
    if (keylog)
        for (v = 0; v < DRIVER_VOICES; v++)
            if (d.key_on & (1u << v))
                fprintf(keylog, "%llu %d %d\n", (unsigned long long)ticks, v, d.voice[v].source);
    driver_output();
}

/* MIX_RECORDED_PASSES: the passes where the original's were: its reads of the
   timer's counter give each pass's start and periods, and its next read comes
   just after that pass's DSP writes. Otherwise one pass every 64 samples from
   the driver's start, as the game runs it. */
static size_t fd_at;

static void ports_up_to(uint32_t now)
{
    while (spc_at + 8 <= nspc) {
        uint32_t t = (uint32_t)spc[spc_at] | (uint32_t)spc[spc_at + 1] << 8 | (uint32_t)spc[spc_at + 2] << 16 | (uint32_t)spc[spc_at + 3] << 24;
        uint16_t addr = (uint16_t)(spc[spc_at + 5] | spc[spc_at + 6] << 8);

        if (t / 64 > now)         /* the log's times are the SPC700's clocks, 64 to a sample */
            break;
        if (!spc[spc_at + 4] && addr >= 0xF4 && addr <= 0xF7)
            wire[addr - 0xF4] = spc[spc_at + 7];
        spc_at += 8;
    }
}

static void recorded_pass_events(uint32_t now)
{
    while (fd_at + 8 <= nspc) {
        uint32_t t = (uint32_t)spc[fd_at] | (uint32_t)spc[fd_at + 1] << 8 | (uint32_t)spc[fd_at + 2] << 16 | (uint32_t)spc[fd_at + 3] << 24;
        uint16_t addr = (uint16_t)(spc[fd_at + 5] | spc[fd_at + 6] << 8);
        uint8_t v = spc[fd_at + 7];

        if (t / 64 > now)
            break;
        fd_at += 8;
        if (spc[fd_at - 4] || addr != 0xFD || t / 64 < reset_sample[current])
            continue;
        if (running)
            pass_output();
        if (v && running)
            pass_work(v);
    }
}

/* MIX_DSP_REPLAY: no driver at all: the original's own DSP writes, each at its
   sample, through the engine's voice calls. What is left against sound.raw is
   the engine's mixer itself. */
static int dsp_replay;
static size_t apu_at = 65536;
static uint8_t dsp_reg, dsp[256];

static void options_send(void)
{
    int v;

    for (v = 0; v < DRIVER_VOICES; v++)
        nshvc_voice_options((uint32_t)v, (dsp[0x3D] >> v) & 1, (dsp[0x4D] >> v) & 1, (dsp[0x2D] >> v) & 1);
}

static void echo_send(void)
{
    int8_t fir[8];
    int i;

    for (i = 0; i < 8; i++)
        fir[i] = (int8_t)dsp[(i << 4) | 0x0F];
    nshvc_echo(dsp[0x7D] & 15, (int8_t)dsp[0x0D], (int8_t)dsp[0x2C], (int8_t)dsp[0x3C], fir);
}

static void dsp_write(uint8_t reg, uint8_t value)
{
    int v = reg >> 4, i;

    dsp[reg] = value;
    if (reg < 0x80 && (reg & 15) <= 7) {
        switch (reg & 15) {
        case 0: case 1:
            nshvc_voice_volume((uint32_t)v, (int8_t)dsp[v << 4], (int8_t)dsp[v << 4 | 1]);
            break;
        case 2: case 3:
            nshvc_voice_pitch((uint32_t)v, (uint16_t)((dsp[v << 4 | 3] & 0x3F) << 8 | dsp[v << 4 | 2]));
            break;
        case 5: case 6: case 7: {
            uint8_t a1 = dsp[v << 4 | 5], a2 = dsp[v << 4 | 6], g = dsp[v << 4 | 7];

            if (a1 & 0x80)
                nshvc_voice_adsr((uint32_t)v, a1 & 15, (a1 >> 4) & 7, a2 >> 5, a2 & 31);
            else if (g & 0x80)
                nshvc_voice_gain((uint32_t)v, (nshvc_gain_mode)((g >> 5) & 7), g & 31);
            else
                nshvc_voice_gain((uint32_t)v, NSHVC_GAIN_DIRECT, g & 127);
            break;
        }
        default:
            break;
        }
        return;
    }
    switch (reg) {
    case 0x0C: case 0x1C:
        nshvc_sound_volume((int8_t)dsp[0x0C], (int8_t)dsp[0x1C]);
        break;
    case 0x2D: case 0x3D: case 0x4D:
        options_send();
        break;
    case 0x4C:
        for (i = 0; i < DRIVER_VOICES; i++)
            if ((value >> i) & 1)
                nshvc_voice_key_on((uint32_t)i, banks[current].samples[dsp[i << 4 | 4]]);
        break;
    case 0x5C:
        for (i = 0; i < DRIVER_VOICES; i++)
            if ((value >> i) & 1)
                nshvc_voice_key_off((uint32_t)i);
        break;
    case 0x6C:
        nshvc_sound_noise_rate(value & 31);
        nshvc_echo_writes(!(value & 0x20));
        nshvc_sound_mute((value & 0x40) != 0);
        break;
    case 0x0D: case 0x2C: case 0x3C: case 0x7D:
    case 0x0F: case 0x1F: case 0x2F: case 0x3F: case 0x4F: case 0x5F: case 0x6F: case 0x7F:
        echo_send();
        break;
    default:
        break;
    }
}

static void dsp_writes_up_to(uint32_t now)
{
    while (apu_at + 8 <= napu) {
        uint32_t t = (uint32_t)apu[apu_at] | (uint32_t)apu[apu_at + 1] << 8 | (uint32_t)apu[apu_at + 2] << 16 | (uint32_t)apu[apu_at + 3] << 24;
        uint16_t addr = (uint16_t)(apu[apu_at + 5] | apu[apu_at + 6] << 8);
        uint8_t v = apu[apu_at + 7];

        if (t > now)
            break;
        if (addr == 0xF2)
            dsp_reg = v;
        else if (addr == 0xF3 && current >= 0)
            dsp_write(dsp_reg, v);
        apu_at += 8;
    }
}

static void tick(void)
{
    uint32_t now = (uint32_t)ticks++;

    while (current + 1 < nresets && now >= reset_sample[current + 1]) {
        current++;
        d.bank = &banks[current];
        if (dsp_replay)
            continue;
        driver_reset();
        s.battle = battle_flag[current];
        running = 1;
        output_due = latch_due = 0;
    }
    if (current < 0)
        return;
    if (now >= stop_sample[current] && !dsp_replay) {
        if (running)
            silence();
        running = 0;
        return;
    }
    ports_up_to(now);
    if (dsp_replay) {
        dsp_writes_up_to(now);
        return;
    }
    if (recorded_passes) {
        recorded_pass_events(now);
    } else if (!((now - reset_sample[current]) & 63) && running) {
        pass_work(1);
        pass_output();
    }
    if (tempolog && (now % 534) < 64 && running)
        fprintf(tempolog, "%u %u %u %u %u %u %u\n", now, d.tempo, d.music_acc, d.sfx_acc, d.song, d.flags, d.pattern);
}

int main(int argc, char **argv)
{
    static nshvc_game game;
    size_t i;
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
    recorded_passes = getenv("MIX_RECORDED_PASSES") != NULL;
    dsp_replay = getenv("MIX_DSP_REPLAY") != NULL;
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
