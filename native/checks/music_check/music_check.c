/*
 * music_check.c: drives the converted sound driver with the port values the
 * original's SPC700 read in a recording (sfxbox --native), and compares its
 * voice events with the DSP writes the original made. A check, not the game.
 *
 *   music_check RECORD [--driver race|battle] [--passes N] [--from PASS]
 *
 * Time is in driver passes of 64 DSP samples. The original's DSP writes are
 * read from apu.bin, its port reads from spc.bin, the sound memory from
 * state.bin at the frame after each driver start.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

#include "audio/driver.h"
#include "audio/driver_sfx.h"
#include "audio/music.h"

#define APU_AT (0x20000 + 0x10000 + 0x10000 + 544 + 512)
#define STATE_SIZE (APU_AT + 0x10000)

/* ---- the events the driver makes ---- */

enum { EV_KEY_ON, EV_KEY_OFF };
typedef struct { uint32_t pass; uint8_t kind, voice, source; uint16_t pitch; int8_t left, right; } Event;
static Event *events;
static size_t nevents, cap_events;
static uint16_t voice_pitch[8];
static int8_t voice_left[8], voice_right[8];
static uint32_t pass_now;

/* every pitch and volume told to a voice, the original's and the native's */
enum { SET_PITCH, SET_LEFT, SET_RIGHT, SET_KINDS };
#define SET_UNKNOWN 0x7FFF
typedef struct { uint32_t t; uint8_t voice, kind; int16_t value; } Set;
static Set *native_sets, *original_sets;
static size_t n_native, n_original;
static uint32_t time_now;
#define SETS_MAX 40000000u

static void set_add(Set *list, size_t *n, uint32_t v, int kind, int value)
{
    if (*n < SETS_MAX) {
        list[*n].t = time_now;
        list[*n].voice = (uint8_t)v;
        list[*n].kind = (uint8_t)kind;
        list[*n].value = (int16_t)value;
        (*n)++;
    }
}

static void add_event(int kind, uint32_t v, uint8_t source)
{
    Event *e;

    if (nevents == cap_events) {
        cap_events = cap_events ? cap_events * 2 : 65536;
        events = (Event *)realloc(events, cap_events * sizeof *events);
    }
    e = &events[nevents++];
    e->pass = pass_now;
    e->kind = (uint8_t)kind;
    e->voice = (uint8_t)v;
    e->source = source;
    e->pitch = voice_pitch[v];
    e->left = voice_left[v];
    e->right = voice_right[v];
}

void nshvc_voice_key_on(uint32_t v, nshvc_sample s) { add_event(EV_KEY_ON, v, (uint8_t)(s.id - 1)); }
void nshvc_voice_key_off(uint32_t v) { add_event(EV_KEY_OFF, v, 0); }
void nshvc_voice_pitch(uint32_t v, uint16_t p) { voice_pitch[v] = p; set_add(native_sets, &n_native, v, SET_PITCH, p); }
void nshvc_voice_volume(uint32_t v, int8_t l, int8_t r)
{
    voice_left[v] = l;
    voice_right[v] = r;
    set_add(native_sets, &n_native, v, SET_LEFT, l);
    set_add(native_sets, &n_native, v, SET_RIGHT, r);
}
void nshvc_voice_adsr(uint32_t v, uint8_t a, uint8_t b, uint8_t c, uint8_t e) { (void)v; (void)a; (void)b; (void)c; (void)e; }
void nshvc_voice_gain(uint32_t v, nshvc_gain_mode m, uint8_t x) { (void)v; (void)m; (void)x; }
void nshvc_voice_options(uint32_t v, uint8_t a, uint8_t b, uint8_t c) { (void)v; (void)a; (void)b; (void)c; }
nshvc_voice_state nshvc_voice_read(uint32_t v) { nshvc_voice_state s; (void)v; memset(&s, 0, sizeof s); return s; }
void nshvc_sound_volume(int8_t l, int8_t r) { (void)l; (void)r; }
void nshvc_sound_noise_rate(uint8_t r) { (void)r; }
void nshvc_echo(uint8_t a, int8_t b, int8_t c, int8_t e, const int8_t f[8]) { (void)a; (void)b; (void)c; (void)e; (void)f; }
void nshvc_sound_mute(int on) { (void)on; }
void nshvc_echo_writes(int on) { (void)on; }

/* ---- the recording ---- */

static uint8_t *load(const char *dir, const char *name, size_t *n)
{
    char p[4096];
    FILE *f;
    uint8_t *d;
    long size;

    snprintf(p, sizeof p, "%s/%s", dir, name);
    if (!(f = fopen(p, "rb"))) { fprintf(stderr, "no %s\n", p); exit(1); }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = (uint8_t *)malloc((size_t)size);
    fread(d, 1, (size_t)size, f);
    fclose(f);
    *n = (size_t)size;
    return d;
}

typedef struct { uint32_t sample; uint8_t reg, value; } DspWrite;
typedef struct { uint32_t pass; uint8_t voice, source; uint16_t pitch; int8_t left, right; } KeyOn;

static uint8_t machine[STATE_SIZE];
static FILE *state_f;
static uint32_t state_frame;

static void state_to_frame(uint32_t want)
{
    uint32_t head[3], o;

    while (state_frame < want && fread(head, 4, 3, state_f) == 3) {
        while (fread(&o, 4, 1, state_f) == 1 && o != 0xFFFFFFFFu) {
            uint32_t n = STATE_SIZE - o < 64 ? STATE_SIZE - o : 64;
            fread(machine + o, 1, n, state_f);
        }
        if (head[1] == 0)
            state_frame = head[0] + 1;
    }
}

static SoundBank *bank;

static void bank_from_machine(void)
{
    int i;

    memcpy(bank->bytes, machine + APU_AT, 0x10000);
    for (i = 0; i < DRIVER_SAMPLES; i++)
        bank->samples[i].id = (uint32_t)i + 1;
    for (i = 0; i < 128; i++)
        bank->noise[i].id = 0x23 + 1;   /* any noise variant is source $23 here */
    d.bank = bank;
}

static uint8_t port_wire[4];
static uint32_t trace_frame;
#define PASSES_MAX 4000000u
static uint32_t *pass_ends;
static size_t n_passes;
static FILE *trace_f;

void nshvc_music_send(const void *m, uint32_t size) { (void)m; (void)size; }
uint32_t nshvc_music_receive(void *b, uint32_t max) { (void)b; (void)max; return 0; }
nshvc_sample nshvc_sample_load(const void *d_, uint32_t size, nshvc_sample_format f, uint32_t loop) { nshvc_sample r; (void)d_; (void)size; (void)f; (void)loop; r.id = 0; return r; }

/* A pass in its two halves, with the original's pacing: the tick's work with
   the timer periods it covers, and the DSP writes the next loop opens with. The
   accumulators are 8-bit adds of the product's low byte, so a pass of many
   periods can lose a carry, as the original's does. */
static int output_due, latch_due;

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
        latch_due = 1;          /* the original reads its ports as the tick ends */
    } else {
        driver_between_ticks();
    }
    output_due = 1;
}

static int pass_output(void)
{
    if (!output_due)
        return 0;
    output_due = 0;
    if (latch_due) {
        memcpy(s.port_latch, port_wire, 4);
        latch_due = 0;
    }
    driver_output();
    return 1;
}

static void on_alarm(int sig)
{
    void *bt[64];
    int n = backtrace(bt, 64);

    (void)sig;
    fprintf(stderr, "stuck at pass %u, voice %u\n", pass_now, d.current);
    backtrace_symbols_fd(bt, n, 2);
    {
        int i, k;
        for (i = 0; i < 8; i++)
            if (s.ch[i].active) {
                fprintf(stderr, "ch %d cmd %02X state %u age %u ptr %04X read_pos %u number %02X:", i, s.ch[i].command, s.ch[i].state, s.ch[i].age, s.ch[i].ptr, s.ch[i].read_pos, s.ch[i].number);
                for (k = 0; k < 24; k++) fprintf(stderr, " %02X", bank_byte((uint16_t)(s.ch[i].ptr + k)));
                fprintf(stderr, "\n");
            }
    }
    _exit(3);
}

int main(int argc, char **argv)
{
    const char *dir;
    size_t napu, nspc, i, n_writes = 0, n_ko = 0;
    uint8_t *apu, *spc;
    uint8_t dsp_reg = 0;
    DspWrite *w;
    KeyOn *ko;
    uint32_t max_pass = 200000, from = 0;
    uint32_t reset_sample[64], stop_sample[64];
    int nresets = 0, a;
    char p[4096];

    signal(SIGALRM, on_alarm);
    alarm(20);
    if (argc < 2) { fprintf(stderr, "usage\n"); return 2; }
    dir = argv[1];
    for (a = 2; a + 1 < argc; a += 2) {
        if (!strcmp(argv[a], "--passes")) max_pass = (uint32_t)atoi(argv[a + 1]);
        if (!strcmp(argv[a], "--from")) from = (uint32_t)atoi(argv[a + 1]);
    }
    if (getenv("TRACE")) trace_f = fopen(getenv("TRACE"), "w");
    apu = load(dir, "apu.bin", &napu);
    spc = load(dir, "spc.bin", &nspc);
    snprintf(p, sizeof p, "%s/state.bin", dir);
    state_f = fopen(p, "rb");
    bank = (SoundBank *)calloc(1, sizeof *bank);

    /* the original's DSP writes, and each time the driver started */
    native_sets = (Set *)malloc(SETS_MAX * sizeof *native_sets);
    original_sets = (Set *)malloc(SETS_MAX * sizeof *original_sets);
    pass_ends = (uint32_t *)malloc(PASSES_MAX * sizeof *pass_ends);
    w = (DspWrite *)malloc((napu - 65536) / 8 * sizeof *w + 1);
    ko = (KeyOn *)malloc((napu - 65536) / 8 * sizeof *ko + 1);
    {
        uint16_t pitch[8] = { 0 };
        int8_t left[8] = { 0 }, right[8] = { 0 };
        uint8_t srcn[8] = { 0 };

        for (i = 65536; i + 8 <= napu; i += 8) {
            uint32_t sample = (uint32_t)apu[i] | (uint32_t)apu[i + 1] << 8 | (uint32_t)apu[i + 2] << 16 | (uint32_t)apu[i + 3] << 24;
            uint16_t addr = (uint16_t)(apu[i + 5] | apu[i + 6] << 8);
            uint8_t v = apu[i + 7];

            if (addr == 0x00F2) {
                dsp_reg = v;
            } else if (addr == 0x00F1 && v == 0x30 && nresets < 64) {
                reset_sample[nresets] = sample;
                stop_sample[nresets] = 0xFFFFFFFFu;
                nresets++;
                time_now = sample;
                {
                    int vv, kk;

                    for (vv = 0; vv < 8; vv++)
                        for (kk = 0; kk < SET_KINDS; kk++)
                            set_add(original_sets, &n_original, (uint32_t)vv, kk, SET_UNKNOWN);
                }
            } else if (addr == 0x00F4 && v == 0xAA && nresets && stop_sample[nresets - 1] == 0xFFFFFFFFu) {
                stop_sample[nresets - 1] = sample;      /* back in the upload protocol */
            } else if (addr == 0x00F3) {
                uint8_t voice = dsp_reg >> 4 & 7, r = dsp_reg & 15;

                w[n_writes].sample = sample;
                w[n_writes].reg = dsp_reg;
                w[n_writes].value = v;
                n_writes++;
                if (dsp_reg < 0x80) {
                    time_now = sample;
                    if (r == 0) { left[voice] = (int8_t)v; set_add(original_sets, &n_original, voice, SET_LEFT, (int8_t)v); }
                    else if (r == 1) { right[voice] = (int8_t)v; set_add(original_sets, &n_original, voice, SET_RIGHT, (int8_t)v); }
                    else if (r == 2) { pitch[voice] = (uint16_t)((pitch[voice] & 0x3F00) | v); set_add(original_sets, &n_original, voice, SET_PITCH, pitch[voice]); }
                    else if (r == 3) { pitch[voice] = (uint16_t)((pitch[voice] & 0xFF) | (v & 0x3F) << 8); set_add(original_sets, &n_original, voice, SET_PITCH, pitch[voice]); }
                    else if (r == 4) srcn[voice] = v;
                }
                if (dsp_reg == 0x4C) {
                    int b;

                    for (b = 0; b < 8; b++)
                        if (v & (1 << b)) {
                            ko[n_ko].pass = sample / 64;
                            ko[n_ko].voice = (uint8_t)b;
                            ko[n_ko].source = srcn[b];
                            ko[n_ko].pitch = pitch[b];
                            ko[n_ko].left = left[b];
                            ko[n_ko].right = right[b];
                            n_ko++;
                        }
                }
            }
        }
    }
    printf("driver starts %d; DSP writes %zu; key-ons %zu\n", nresets, n_writes, n_ko);
    {
        int start = -1, running = 0;
        size_t at;
        uint32_t limit = reset_sample[0] + max_pass * 64;

        for (at = 0; at + 8 <= nspc; at += 8) {
            uint32_t now = ((uint32_t)spc[at] | (uint32_t)spc[at + 1] << 8 | (uint32_t)spc[at + 2] << 16 | (uint32_t)spc[at + 3] << 24) / 64;
            uint16_t addr = (uint16_t)(spc[at + 5] | spc[at + 6] << 8);
            uint8_t value = spc[at + 7];
            int v, k;

            if (now >= limit)
                break;
            time_now = now;
            pass_now = now / 64;
            while (start + 1 < nresets && now >= reset_sample[start + 1]) {
                start++;
                state_to_frame(reset_sample[start] / 534 + 6);
                bank_from_machine();
                driver_reset();
                s.battle = bank_byte(0x10E3) != 0xC4;
                running = 1;
                output_due = latch_due = 0;
                for (v = 0; v < 8; v++)
                    for (k = 0; k < SET_KINDS; k++)
                        set_add(native_sets, &n_native, (uint32_t)v, k, SET_UNKNOWN);
            }
            if (start < 0)
                continue;
            if (now >= stop_sample[start])
                running = 0;
            if (spc[at + 4])
                continue;
            if (addr >= 0xF4 && addr <= 0xF7)
                port_wire[addr - 0xF4] = value;
            if (addr == 0xFD && running) {
                if (pass_output() && n_passes < PASSES_MAX)
                    pass_ends[n_passes++] = now;
                if (trace_f && now / 534 != trace_frame) {
                    trace_frame = now / 534;
                    fprintf(trace_f, "%u", now);
                    for (v = 0; v < 8; v++)
                        fprintf(trace_f, " %d:%02X,%02X,%u,%u,a%u,f%u", v, s.ch[v].command, s.ch[v].number, s.ch[v].vol[0], s.ch[v].vol[1], s.ch[v].active, s.ch[v].fade_count[0]);
                    fprintf(trace_f, " vol");
                    for (v = 0; v < 8; v++)
                        fprintf(trace_f, " %d,%d", voice_left[v], voice_right[v]);
                    fprintf(trace_f, " sfxv %02X song %u\n", d.sfx_voices, d.song);
                }
                if (value)
                    pass_work(value);
            }
        }
    }
    /* each voice's pitch and volumes at the end of every pass (strictly before the
       timer read that ends it: the next pass's work carries that read's time) */
    {
        static const char *const names[SET_KINDS] = { "pitch", "left", "right" };
        int v, k;
        long total[SET_KINDS] = { 0 }, equal[SET_KINDS] = { 0 };

        for (v = 0; v < 8; v++)
            for (k = 0; k < SET_KINDS; k++) {
                size_t a_n = 0, a_o = 0, e;
                int nv = SET_UNKNOWN, ov = SET_UNKNOWN;
                long v_total = 0, v_equal = 0;
                int shown = 0;

                for (e = 0; e < n_passes; e++) {
                    uint32_t end = pass_ends[e];

                    for (; a_n < n_native && native_sets[a_n].t < end; a_n++)
                        if (native_sets[a_n].voice == v && native_sets[a_n].kind == k)
                            nv = native_sets[a_n].value;
                    for (; a_o < n_original && original_sets[a_o].t < end; a_o++)
                        if (original_sets[a_o].voice == v && original_sets[a_o].kind == k)
                            ov = original_sets[a_o].value;
                    if (nv == SET_UNKNOWN || ov == SET_UNKNOWN)
                        continue;
                    v_total++;
                    if (nv == ov)
                        v_equal++;
                    else if (shown++ < 5)
                        printf("  sample %u voice %d %s: native %d original %d\n", end, v, names[k], nv, ov);
                }
                total[k] += v_total;
                equal[k] += v_equal;
                if (v_equal != v_total)
                    printf("voice %d %s: %ld of %ld passes equal\n", v, names[k], v_equal, v_total);
            }
        for (k = 0; k < SET_KINDS; k++)
            printf("%s: %ld of %ld voice-passes equal\n", names[k], equal[k], total[k]);
    }
    /* compare the two lists of key-ons in order, allowing small slips: the
       comparison finds the longest runs; only fields that define the note
       (voice, sample, pitch) count, volumes separately */
    {
        size_t e = 0, k = 0;
        long same = 0, same_volume = 0, native_only = 0, original_only = 0, total_native = 0;
        int shown = 0;
        Event *on = (Event *)malloc(nevents * sizeof *on + 1);
        size_t non = 0, i2;

        for (i2 = 0; i2 < nevents; i2++)
            if (events[i2].kind == EV_KEY_ON && events[i2].pass >= from)
                on[non++] = events[i2];
        total_native = (long)non;
        while (k < n_ko && ko[k].pass < reset_sample[0] / 64) k++;
        while (k < n_ko && ko[k].pass + 1 < from) k++;
#define SAME_NOTE(E, K) ((E).voice == (K).voice && (E).source == (K).source && (E).pitch == (K).pitch)
        {
            int v;
            uint32_t last_pass = ko[n_ko - 1].pass;

            for (v = 0; v < 8; v++) {
                size_t ee = 0, kk = 0;
                long v_same = 0, v_nat = 0, v_orig = 0;

                while (kk < n_ko && ko[kk].pass < reset_sample[0] / 64) kk++;
                for (;;) {
                    while (ee < non && (on[ee].voice != v)) ee++;
                    while (kk < n_ko && ko[kk].voice != v) kk++;
                    if (ee >= non || kk >= n_ko || on[ee].pass > last_pass + 8)
                        break;
                    if (on[ee].source == ko[kk].source && on[ee].pitch == ko[kk].pitch) {
                        v_same++;
                        same++;
                        if (on[ee].left == ko[kk].left && on[ee].right == ko[kk].right)
                            same_volume++;
                        ee++;
                        kk++;
                        continue;
                    }
                    /* look a little ahead on this voice, either way */
                    {
                        size_t look, ex = ee + 1, kx = kk + 1;
                        int fixed = 0;

                        for (look = 1; look <= 6 && !fixed; look++) {
                            while (ex < non && on[ex].voice != v) ex++;
                            while (kx < n_ko && ko[kx].voice != v) kx++;
                            if (ex < non && on[ex].source == ko[kk].source && on[ex].pitch == ko[kk].pitch) {
                                v_nat += (long)look;
                                ee = ex;
                                fixed = 1;
                            } else if (kx < n_ko && on[ee].source == ko[kx].source && on[ee].pitch == ko[kx].pitch) {
                                v_orig += (long)look;
                                kk = kx;
                                fixed = 1;
                            }
                            ex++;
                            kx++;
                        }
                        if (!fixed) {
                            if (shown++ < 14)
                                printf("voice %d lost: native pass %u sample %02X pitch %04X | original pass %u sample %02X pitch %04X\n", v, on[ee].pass, on[ee].source, on[ee].pitch, ko[kk].pass, ko[kk].source, ko[kk].pitch);
                            ee++;
                            kk++;
                            v_nat++;
                            v_orig++;
                        }
                    }
                }
                native_only += v_nat;
                original_only += v_orig;
                printf("voice %d: same %ld native-only %ld original-only %ld\n", v, v_same, v_nat, v_orig);
            }
        }
        printf("key-ons: original %zu, native %ld; same note %ld (same volumes %ld); native only %ld; original only %ld\n",
               n_ko, total_native, same, same_volume, native_only, original_only);
    }
    return 0;
}
