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
void nshvc_voice_pitch(uint32_t v, uint16_t p) { voice_pitch[v] = p; }
void nshvc_voice_volume(uint32_t v, int8_t l, int8_t r) { voice_left[v] = l; voice_right[v] = r; }
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
    d.bank = bank;
}

static uint8_t port_wire[4];
static FILE *trace_f;

void nshvc_music_send(const void *m, uint32_t size) { (void)m; (void)size; }
uint32_t nshvc_music_receive(void *b, uint32_t max) { (void)b; (void)max; return 0; }
nshvc_sample nshvc_sample_load(const void *d_, uint32_t size, nshvc_sample_format f, uint32_t loop) { nshvc_sample r; (void)d_; (void)size; (void)f; (void)loop; r.id = 0; return r; }

static void pass(void)
{
    memcpy(s.port_latch, port_wire, 0);
    music_pass_with_wire(port_wire);
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
            } else if (addr == 0x00F4 && v == 0xAA && nresets && stop_sample[nresets - 1] == 0xFFFFFFFFu) {
                stop_sample[nresets - 1] = sample;      /* back in the upload protocol */
            } else if (addr == 0x00F3) {
                uint8_t voice = dsp_reg >> 4 & 7, r = dsp_reg & 15;

                w[n_writes].sample = sample;
                w[n_writes].reg = dsp_reg;
                w[n_writes].value = v;
                n_writes++;
                if (dsp_reg < 0x80) {
                    if (r == 0) left[voice] = (int8_t)v;
                    else if (r == 1) right[voice] = (int8_t)v;
                    else if (r == 2) pitch[voice] = (uint16_t)((pitch[voice] & 0x3F00) | v);
                    else if (r == 3) pitch[voice] = (uint16_t)((pitch[voice] & 0xFF) | (v & 0x3F) << 8);
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
        size_t at_spc = 0;
        int start = 0;
        uint32_t now = reset_sample[0];
        uint32_t end = now + max_pass * 64;

        state_to_frame(now / 534 + 2);
        bank_from_machine();
        driver_reset();
        s.battle = bank_byte(0x10E3) != 0xC4;
        for (; now < end; now += 64) {
            /* the driver is out of its main loop during an upload */
            if (now >= stop_sample[start]) {
                if (start + 1 >= nresets)
                    break;
                start++;
                state_to_frame(reset_sample[start] / 534 + 2);
                bank_from_machine();
                driver_reset();
                s.battle = bank_byte(0x10E3) != 0xC4;
                now = reset_sample[start];
            }
            pass_now = now / 64;
            /* the ports as the SPC700 saw them up to now */
            while (at_spc + 8 <= nspc) {
                uint32_t t = (uint32_t)spc[at_spc] | (uint32_t)spc[at_spc + 1] << 8 | (uint32_t)spc[at_spc + 2] << 16 | (uint32_t)spc[at_spc + 3] << 24;
                uint16_t addr = (uint16_t)(spc[at_spc + 5] | spc[at_spc + 6] << 8);

                if (t / 64 > now)
                    break;
                if (!spc[at_spc + 4] && addr >= 0xF4 && addr <= 0xF7)
                    port_wire[addr - 0xF4] = spc[at_spc + 7];
                at_spc += 8;
            }
            pass();
            if (trace_f && (now - reset_sample[0]) % 534 < 64) {
                int c;
                fprintf(trace_f, "%u", now / 534);
                for (c = 0; c < 8; c++) fprintf(trace_f, " %02X:%02X:%02X:%02X", s.ch[c].command, s.ch[c].number, s.ch[c].active, s.ch[c].state);
                fprintf(trace_f, " sfx %02X flags %02X loop %02X game %02X\n", d.sfx_voices, d.flags, s.loop_flags, s.game_flags);
            }
        }
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
                    if (ee >= non || kk >= n_ko || on[ee].pass > last_pass)
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
