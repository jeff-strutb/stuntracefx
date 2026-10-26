/* old: video/gradient.c
 *
 * gradient_check.c: the port's sky gradient (video/gradient.c) against the
 * decomp's, on random states: the colour lists expanded from the course
 * blocks and run tables, the course's lists built, the CGADD fill, and the
 * HDMA set-up. The decomp's writes to the video hardware go to the checks'
 * model of it (ppu.c), the port's to its video state, and the two are
 * compared: work RAM and game pak RAM (direct page $F1-$F2 is the decomp's
 * run counter, not kept by the port), the HDMA channels' tables, the return
 * values, and the picture.
 *
 * The inputs stay as the game gives them: the colour blocks and banks come
 * from the course tables in ROM (courses 0-27, entries of 3 bytes at
 * $03:F6F1; the table's later entries are not courses) or from bank $1F at
 * its colour addresses ($8000-$BFFF); the write offset $F3 is 0 or $0400;
 * the fill's count is 1-96 entries and its offset an even number under
 * $0200; the set-up's data bank is $00 or $03 (the I/O mirror). Colour
 * blocks in work RAM are not checked: the original's run counter at $F1
 * is read as colour bytes there.
 *
 *   check/check.sh gradient N
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
#include "gradient.h"
#include "video.h"
#include "model.h"
#include "ppu.h"

#define OLD(name) void old_abi_##name(void);
OLD(GradientExpandFirst) OLD(GradientExpandSecond) OLD(GradientBuildColours)
OLD(GradientFillCgadd) OLD(GradientFillCgaddB) OLD(GradientSetup)

static uint64_t rng = 0x6A09E667F3BCC909ull;

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

/* Random work RAM, pak RAM and picture; the port's HDMA tables as the
   decomp's channel registers hold them. */
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
    video_frame_begin();
    ppu_to_port();
    video_registers_reset();
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
    old_cpu.db = 0x00;
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
        if (wram[i] != old_w[i] && i != 0xF1 && i != 0xF2) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    for (c = 0; c < 8; c++) {
        const uint8_t *d = &ppu.dma[c * 0x10];
        const VideoLineTable *t = video_line_table_get(c);

        if (t->mode != (d[0] & 7) || t->indirect != (d[0] >> 6 & 1) || t->target != d[1] || t->bank != d[4]
            || t->at != (uint16_t)(d[2] | d[3] << 8) || (t->indirect && t->indirect_bank != d[7])) {
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
        fprintf(stderr, "usage: gradient_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("GradientExpandFirst / GradientExpandSecond") {
        int second = (int)(rnd() & 1);
        uint8_t bank = 0x1F;                /* the colour blocks are in ROM bank $1F */
        uint16_t y = (uint16_t)(0x8000 + rnd() % 0x4000), y_port;
        uint16_t out = rnd() & 1 ? 0x0000 : 0x0400;

        fresh();
        W16(0x00F3) = out;
        old_cpu.db = bank;
        old_cpu.y = y;
        old_run(second ? old_abi_GradientExpandSecond : old_abi_GradientExpandFirst);
        y_port = y;
        if (second)
            gradient_expand_second(bank, &y_port);
        else
            gradient_expand_first(bank, &y_port);
        if (y_port != old_cpu.y) {
            fail("Y %04X, the decomp's %04X", y_port, old_cpu.y);
            continue;
        }
        same();
    }
    CASE("GradientBuildColours") {
        uint16_t course = (uint16_t)(rnd() % 28), y_port;   /* the game's 28 courses */

        fresh();
        old_cpu.x = (uint16_t)(course * 3);
        old_run(old_abi_GradientBuildColours);
        y_port = gradient_build_colours((uint16_t)(course * 3));
        if (y_port != old_cpu.y) {
            fail("Y %04X, the decomp's %04X (course %u)", y_port, old_cpu.y, (unsigned)course);
            continue;
        }
        same();
    }
    CASE("GradientFillCgadd / GradientFillCgaddB") {
        int bee = (int)(rnd() & 1);
        uint16_t value = (uint16_t)rnd(), x = (uint16_t)((rnd() & 0xFF) * 2), count = (uint16_t)(1 + rnd() % 96), x_port;

        fresh();
        old_cpu.a = value;
        old_cpu.x = x;
        old_cpu.y = count;
        old_run(bee ? old_abi_GradientFillCgaddB : old_abi_GradientFillCgadd);
        x_port = bee ? gradient_fill_cgadd_b(value, x, count) : gradient_fill_cgadd(value, x, count);
        if (x_port != old_cpu.x) {
            fail("X %04X, the decomp's %04X", x_port, old_cpu.x);
            continue;
        }
        same();
    }
    CASE("GradientSetup") {
        uint8_t bank = (uint8_t)(rnd() & 1 ? 0x00 : 0x03);
        uint16_t a_port;

        fresh();
        W8(0x0E3D) = (uint8_t)(rnd() % 28);     /* the course byte: a game course */
        old_cpu.db = bank;
        old_run(old_abi_GradientSetup);
        a_port = gradient_setup(bank);
        if (a_port != old_cpu.a) {
            fail("A %04X, the decomp's %04X", a_port, old_cpu.a);
            continue;
        }
        same();
    }
    printf("gradient_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
