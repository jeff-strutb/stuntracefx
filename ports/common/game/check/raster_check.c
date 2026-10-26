/* old: video/fade.c video/palette.c video/dma.c
 *
 * raster_check.c: the port's frame work (what the original does in its
 * raster interrupt: the fade, the palettes, the windows, the uploads) against
 * the decomp's, on random states: the decomp's writes to the video
 * hardware go to the checks' model of it (ppu.c), the port's to its video
 * state, and the two are compared, with the work RAM and the game pak RAM.
 *
 *   check/check.sh raster N
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
#include "rom.h"

#define OLD(name) void old_abi_##name(void);
OLD(FadeStepFixed) OLD(FadeStep) OLD(RasterColours) OLD(RasterPaletteUpload) OLD(RasterColoursCycling)
OLD(RasterColoursE4) OLD(RasterColoursUnless0D22) OLD(PaletteCycle) OLD(PaletteKeptColoursStep)
OLD(PlayerColoursFlash) OLD(StatusColourPick) OLD(PalettesStep)

static uint64_t rng = 0x3C6EF372FE94F82Bull;

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
static int scratch_f9;                  /* the case's routine keeps counts in direct page $F9-$FA */

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
    ppu.hdmaen = (uint8_t)rnd();
    memcpy(&wram[0xA2D9], rom_at(0x028000), 0x54A6);  /* the code and tables the game runs from work RAM */
    ppu.vmain = 0x80;
    scratch_f9 = 0;                   /* the game's standing setting: words, a step of 1 */
    ppu_to_port();
    video_registers_reset();
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
        if (wram[i] != old_w[i] && !(scratch_f9 && (i == 0xF9 || i == 0xFA))) {
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
        fprintf(stderr, "usage: raster_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("FadeStepFixed / FadeStep") {
        int fixed = (int)(rnd() & 1);

        fresh();
        fade_out = (uint8_t)(rnd() & 1 ? 0 : rnd());
        fade_level = (uint16_t)(rnd() & 1 ? rnd() & 0x0FFF : rnd());
        old_cpu.p |= P_M | P_X;
        old_run(fixed ? old_abi_FadeStepFixed : old_abi_FadeStep);
        if (fixed)
            fade_step_fixed();
        else
            fade_step();
        same();
    }
    CASE("RasterColours / RasterPaletteUpload / RasterColoursCycling / RasterColoursE4 / RasterColoursUnless0D22") {
        int k = (int)(rnd() % 5);
        uint16_t x = (uint16_t)(rnd() & 1 ? rnd() & 0x3F : rnd());

        fresh();
        palette_animate = (uint8_t)(rnd() & 1 ? 0 : rnd());
        W8(0x0D22) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        old_cpu.p |= P_M;
        if (k == 1)
            old_cpu.p |= P_X;
        else if (k == 2 || k == 3)
            old_cpu.p &= (uint8_t)~P_X;
        old_cpu.x = x;
        old_run(k == 0 ? old_abi_RasterColours : k == 1 ? old_abi_RasterPaletteUpload : k == 2 ? old_abi_RasterColoursCycling
                : k == 3 ? old_abi_RasterColoursE4 : old_abi_RasterColoursUnless0D22);
        switch (k) {
        case 0: raster_colours(); break;
        case 1: raster_palette_upload(); break;
        case 2: raster_colours_cycling(x); break;
        case 3: raster_colours_e4(x); break;
        default: raster_colours_unless_0d22();
        }
        same();
    }
    CASE("PaletteCycle") {
        fresh();
        W16(0xFD5B) = (uint16_t)(rnd() & 15), W16(0xFD5D) = (uint16_t)(rnd() & 15), W16(0xFD5F) = (uint16_t)(rnd() & 15);
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_run(old_abi_PaletteCycle);
        palette_cycle();
        same();
    }
    CASE("PaletteKeptColoursStep / PlayerColoursFlash / StatusColourPick / PalettesStep") {
        int k = (int)(rnd() % 4);
        uint16_t x = (uint16_t)(rnd() & 6), y = (uint16_t)(rnd() & 1);
        uint8_t level = (uint8_t)(rnd() & 1 ? rnd() & 0x0F : rnd());

        fresh();
        scratch_f9 = 1;
        palette_animate = (uint8_t)(rnd() & 3 ? rnd() | 1 : 0);
        if (rnd() & 1)
            W8(0x0D53) = 0, W8(0x0E05 + y) |= 1, W16(0x0D1E + x) |= 0x4000;
        if (rnd() & 1)
            W8(0x0DDA + y) = (uint8_t)(rnd() & 1 ? 0 : rnd() & 0x7F);
        W16(0x0D83) = (uint16_t)(rnd() & 0x1FE), W16(0x0D85) = (uint16_t)(rnd() & 0x1FE);
        W16(0xFD5B) = (uint16_t)(rnd() & 15), W16(0xFD5D) = (uint16_t)(rnd() & 15), W16(0xFD5F) = (uint16_t)(rnd() & 15);
        old_cpu.p |= P_M;
        old_cpu.p = k == 0 ? (uint8_t)(old_cpu.p | P_X) : (uint8_t)(old_cpu.p & ~P_X);
        old_cpu.x = x, old_cpu.y = y;
        old_cpu.a = (uint16_t)((rnd() & 0xFF00) | level);
        old_run(k == 0 ? old_abi_PaletteKeptColoursStep : k == 1 ? old_abi_PlayerColoursFlash
                : k == 2 ? old_abi_StatusColourPick : old_abi_PalettesStep);
        switch (k) {
        case 0: palette_kept_colours_step(x, y); break;
        case 1: player_colours_flash(level, y); break;
        case 2: status_colour_pick(y); break;
        default: palettes_step();
        }
        same();
    }
    printf("raster_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
