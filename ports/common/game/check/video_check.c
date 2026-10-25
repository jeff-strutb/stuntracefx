/* old: video/dma.c
 *
 * video_check.c: the port's video (video/) against the decomp's, on
 * random states: the decomp's writes to the video hardware go to the
 * checks' model of it (ppu.c), the port's to its video state, and the two
 * are compared, with the work RAM and the game pak RAM.
 *
 *   check/check.sh video N
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
#undef gsu

#include "game.h"
#include "video.h"
#include "model.h"
#include "ppu.h"

#define OLD(name) void old_abi_##name(void);
OLD(VramDmaFromBank7F) OLD(VramDmaFromGsuRam) OLD(VramDmaRowsFromGsuRam) OLD(VramDmaWideRowsFromGsuRam)
OLD(VramWriteRowFromGsuRam) OLD(CoursePictureToVram) OLD(VramUploadGsuBuffer) OLD(OamDma) OLD(OamDmaAll)
OLD(CgramDma) OLD(CgramDmaAll)

static uint64_t rng = 0xBB67AE8584CAA73Bull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static Ppu old_ppu;
static long cases, failed, iteration;
static int scratch_fb;                  /* the case's routine keeps its source in direct page $FB-$FC */

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

static void fresh(void)
{
    size_t i;

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
    ppu.vmain = 0x80;                   /* the game's standing setting: words, a step of 1 */
    ppu_to_port();
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
    old_ppu = ppu;
}

static void same(void)
{
    size_t i;
    char what[160];

    cases++;
    for (i = 0; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && !(scratch_fb && (i == 0xFB || i == 0xFC))) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    if (ppu_differs(what, sizeof what) || ppu_screen_differs(what, sizeof what))
        fail("%s", what);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: video_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("VramDmaFromBank7F / VramDmaFromGsuRam") {
        int g = (int)(rnd() & 1);
        uint16_t size = (uint16_t)((rnd() & 7) ? rnd() % 0x1000 : rnd()), at = (uint16_t)rnd(), word = (uint16_t)rnd();

        fresh();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_cpu.a = size, old_cpu.x = at, old_cpu.y = word;
        old_run(g ? old_abi_VramDmaFromGsuRam : old_abi_VramDmaFromBank7F);
        if (g)
            video_from_gsu_ram(size, at, word);
        else
            video_from_bank_7f(size, at, word);
        same();
    }
    CASE("VramDmaRowsFromGsuRam / VramDmaWideRowsFromGsuRam") {
        int wide = (int)(rnd() & 1);
        uint16_t at = (uint16_t)rnd(), word = (uint16_t)rnd();
        uint8_t rows = (uint8_t)(1 + rnd() % 30);

        fresh();
        scratch_fb = wide;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        if (wide) {
            old_cpu.a = (uint16_t)((rnd() & 0xFF00) | (uint8_t)(rows - 1));
            old_cpu.x = at, old_cpu.y = word;
            old_run(old_abi_VramDmaWideRowsFromGsuRam);
            video_wide_rows_from_gsu_ram((uint8_t)(rows - 1), at, word);
        } else {
            old_cpu.a = at, old_cpu.x = word, old_cpu.y = rows;
            old_run(old_abi_VramDmaRowsFromGsuRam);
            video_rows_from_gsu_ram(at, word, rows);
        }
        same();
        scratch_fb = 0;
    }
    CASE("VramWriteRowFromGsuRam / CoursePictureToVram / VramUploadGsuBuffer") {
        int w = (int)(rnd() % 3);
        uint16_t word = (uint16_t)rnd();

        fresh();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        W16(0x0002) = (uint16_t)(rnd() % 0xCF80);    /* rows inside the Super FX's RAM */
        if (w == 1) {                       /* the course's place: four rows inside the Super FX's RAM */
            uint16_t e, t;

            W8(0x0E3D) = (uint8_t)(rnd() % 16);
            do {
                e = (uint16_t)rnd();
                t = (uint16_t)(e & 0xFFFC);
                t = (uint16_t)(((uint16_t)(t >> 8 | t << 8) << 1) + ((e & 3) << 7));
            } while (t >= 0xC780);
            W16(0x10B96 + 2 * W8(0x0E3D)) = e;
        }
        if (w == 2 && (rnd() & 1))
            W8(0x0D32) = 0;
        old_cpu.a = word;
        old_run(w == 0 ? old_abi_VramWriteRowFromGsuRam : w == 1 ? old_abi_CoursePictureToVram : old_abi_VramUploadGsuBuffer);
        if (w == 0)
            video_row_from_gsu_ram(word);
        else if (w == 1)
            video_course_picture();
        else
            video_gsu_picture();
        same();
    }
    CASE("OamDma / OamDmaAll / CgramDma / CgramDmaAll") {
        int w = (int)(rnd() % 4);
        uint16_t at = (uint16_t)(rnd() % 0x1C00), size = (uint16_t)(2 * (1 + rnd() % 0x11F)), first = (uint16_t)(rnd() & 0xFF);

        fresh();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        switch (w) {
        case 0: at = (uint16_t)(rnd() % (0x2000 - size + 1)); old_cpu.x = at, old_cpu.y = size; old_cpu.p |= P_M; old_run(old_abi_OamDma); video_sprites_from(at, size); break;
        case 1: old_run(old_abi_OamDmaAll); video_sprites_all(); break;
        case 2: at = (uint16_t)rnd(); old_cpu.a = at, old_cpu.x = first, old_cpu.y = size; old_run(old_abi_CgramDma); video_colours_from(at, (uint8_t)first, size); break;
        case 3: old_run(old_abi_CgramDmaAll); video_colours_all(); break;
        }
        same();
    }

    printf("video_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
