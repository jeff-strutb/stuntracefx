/* old: video/screen_setup.c video/dma.c
 *
 * screen_setup_check.c: the port's screen set-up (video/screen_setup.c)
 * against the decomp's, on random states: the decomp's writes to the video
 * hardware go to the checks' model of it (ppu.c), the port's to its video
 * state, and the two are compared: the work RAM and the game pak RAM, the
 * Super FX's screen base, the HDMA channels' tables, and the picture (the
 * video RAM, the palette, the sprites and the screen's settings).
 *
 *   check/check.sh screen_setup N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define gsu old_gsu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#include "machine.h"
#include "cpu_ops.h"
#include "gsu.h"
#undef cpu

#include "game.h"
#include "render.h"
#include "screen_setup.h"
#include "video.h"
#include "model.h"
#include "ppu.h"

#define OLD(name) void old_abi_##name(void);
OLD(VideoRegsReset) OLD(RaceScreenSetup) OLD(VideoStartUploads)

static uint64_t rng = 0x2545F4914F6CDD1Dull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration;

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 20)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld: ", model_current, iteration);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* A random picture and random registers, the port's state set from them;
   the HDMA channels' tables and the Super FX's screen base random too. */
static void fresh(void)
{
    size_t i;
    int c;

    for (i = 0; i < sizeof wram; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    ppu_reset();
    for (i = 0; i < sizeof ppu.vram; i++)
        ppu.vram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof ppu.cgram; i++)
        ppu.cgram[i] = (uint8_t)(i & 1 ? rnd() & 0x7F : rnd());
    for (i = 0; i < sizeof ppu.oam; i++)
        ppu.oam[i] = (uint8_t)rnd();
    for (i = 0; i < 0x34; i++)
        ppu.reg[i] = (uint8_t)rnd();
    for (i = 0; i < 4; i++)
        ppu.hofs[i] = (uint16_t)(rnd() & 0x3FF), ppu.vofs[i] = (uint16_t)(rnd() & 0x3FF);
    for (i = 0; i < 3; i++)
        ppu.fixed[i] = (uint8_t)(rnd() & 0x1F);
    ppu.hdmaen = (uint8_t)rnd();
    for (i = 0; i < sizeof ppu.dma; i++)
        ppu.dma[i] = (uint8_t)rnd();
    ppu.vmain = 0x80;
    old_gsu.scbr = fx_screen.base = (uint8_t)rnd();
    video_frame_begin();
    ppu_to_port();
    video_registers_reset();
    /* the port's HDMA tables as the decomp's channel registers hold them */
    for (c = 0; c < 8; c++) {
        const uint8_t *d = &ppu.dma[c * 0x10];
        VideoLineTable t = {
            .mode = d[0] & 7, .indirect = (uint8_t)(d[0] >> 6 & 1), .target = d[1],
            .bank = d[4], .indirect_bank = d[7], .at = (uint16_t)(d[2] | d[3] << 8)
        };

        video_line_table(c, &t);
    }
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = rnd() & 1 ? 0x00 : 0x03;   /* the I/O bank the callers keep: either mirror */
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same(void)
{
    size_t i;
    int c;
    char what[160];

    cases++;
    for (i = 0; i < sizeof wram; i++)
        if (wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    if (fx_screen.base != old_gsu.scbr) {
        fail("Super FX screen base %02X, the decomp's %02X", fx_screen.base, old_gsu.scbr);
        return;
    }
    for (c = 0; c < 8; c++) {
        const uint8_t *d = &ppu.dma[c * 0x10];
        const VideoLineTable *t = video_line_table_get(c);

        if (t->mode != (d[0] & 7) || t->indirect != (d[0] >> 6 & 1) || t->target != d[1] || t->bank != d[4]
            || t->at != (uint16_t)(d[2] | d[3] << 8)) {
            fail("HDMA channel %d: mode %u target %02X bank %02X at %04X, the decomp's %02X %02X %02X %04X",
                 c, t->mode, t->target, t->bank, t->at, d[0] & 7, d[1], d[4], (unsigned)(d[2] | d[3] << 8));
            return;
        }
    }
    if (ppu_differs(what, sizeof what) || ppu_screen_differs(what, sizeof what))
        fail("%s", what);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: screen_setup_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("VideoRegsReset") {
        uint16_t x;

        fresh();
        x = old_cpu.x;
        old_run(old_abi_VideoRegsReset);
        screen_ppu_reset(x);
        same();
    }
    CASE("RaceScreenSetup") {
        fresh();
        old_run(old_abi_RaceScreenSetup);
        race_screen_setup();
        same();
    }
    CASE("VideoStartUploads") {
        fresh();
        old_cpu.db = 0x03;  /* its caller's bank, where the two transfers are described */
        old_run(old_abi_VideoStartUploads);
        video_start_uploads();
        /* direct page $FB-$FC: the transfers' descriptor pointer, scratch
           that each user of it sets first */
        wram[0xFB] = old_w[0xFB];
        wram[0xFC] = old_w[0xFC];
        /* channel 0's registers as the decomp's transfers leave them: the
           game never runs a per-line table on channel 0, so they are not
           the picture's */
        {
            const uint8_t *d = &ppu.dma[0];
            VideoLineTable t = {
                .mode = d[0] & 7, .indirect = (uint8_t)(d[0] >> 6 & 1), .target = d[1],
                .bank = d[4], .indirect_bank = d[7], .at = (uint16_t)(d[2] | d[3] << 8)
            };

            video_line_table(0, &t);
        }
        same();
    }
    printf("screen_setup_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
