/* old: render/hud.c
 *
 * fxhud_check.c: the port's HUD programs (render/fx_hud.c) against the
 * decomp's (render/hud.c) on random states: glyphs, the rising picture,
 * the scaled sprites, the noise fill and the tile conversion, from random
 * inputs at $0200-$021D and random screens. The RAM is compared once
 * both sides' pixel caches are written out.
 *
 *   check/check.sh fxhud N
 */
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define gsu old_gsu
#define gsu_rd8 old_gsu_rd8
#define gsu_wr8 old_gsu_wr8
#define gsu_plot old_gsu_plot
#define gsu_rpix old_gsu_rpix
#define gsu_code old_gsu_code
#define gsu_cache old_gsu_cache
#define gsu_cyc old_gsu_cyc
#define gsu_time old_gsu_time
#define gsu_buffer old_gsu_buffer
#include "gsu.h"
#undef gsu

#include "game.h"
#include "render.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(FxTiles2bppTo4bppHigh) OLD(FxTiles2bppTo4bppLow) OLD(FxNoiseFill) OLD(FxPictureRiseSplit40) OLD(FxPictureRiseSplit60)
OLD(FxPictureRise40) OLD(FxPictureRise60) OLD(FxGlyph16RowsFromBottom) OLD(FxGlyph16RowsFromTop) OLD(FxGlyph8x16)
OLD(FxGlyph24x16) OLD(FxGlyph16x32) OLD(FxScaledSprite13) OLD(FxScaledSpriteEighth) OLD(FxSprite24x40)
OLD(FxScaledSpriteAdvance) OLD(FxNoiseFillScreen) OLD(FxGlyph8x8) OLD(FxGlyph24x32Bank04) OLD(FxGlyph16x16)
OLD(FxGlyph16x32Double) OLD(FxScaledSprite12) OLD(FxSprite16x32)

static uint64_t rng = 0x5D2B7C94618E1F3Aull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static int16_t within(int lo, int hi)
{
    return (int16_t)(lo + (int)(rnd() % (unsigned)(hi - lo + 1)));
}

static uint8_t rom_copy[0x100000];
static uint8_t start_p[sizeof pakram], old_p[sizeof pakram];
static long cases, failed, iteration;
static volatile const char *side = "";

/* a case the decomp runs past 5 s on (a row or a height of 0: 65536 of
   them) is skipped and counted */
static sigjmp_buf escape;
static long skipped;

static void runaway(int sig)
{
    (void)sig;
    siglongjmp(escape, 1);
}

/* random RAM and registers, the game's screen, the view's edges */
static void fresh(void)
{
    static const uint8_t bases[2] = {0x0B, 0x25};
    static const uint8_t zero[8];
    size_t i;

    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    memset(&old_gsu, 0, sizeof old_gsu);
    for (i = 0; i < 16; i++)
        old_gsu.r[i] = (uint16_t)rnd();
    old_gsu.r[10] = 0x0240;
    old_gsu.r[11] = 0xFFF0;
    old_gsu.scmr = (rnd() & 1) ? 1 : 3;
    old_gsu.scbr = bases[rnd() & 1];
    old_gsu.colr = (uint8_t)rnd();
    old_gsu.por = (uint8_t)(rnd() & 0x0F);
    fx_screen.mode = old_gsu.scmr & 3;
    fx_screen.height = 0;
    fx_screen.base = old_gsu.scbr;
    fx_colour = old_gsu.colr;
    fx_plot_options = old_gsu.por;
    fx_pixels_set(0, 0xFFFF, 0, zero);
    fx_pixels_set(1, 0xFFFF, 0, zero);
    FXU(0x001C) = (uint16_t)(rnd() % 0x20);
    if (rnd() & 3) {
        fx_screen_centre_x = 108, fx_screen_centre_y = 64;
        fx_edges[0] = 0, fx_edges[1] = 216, fx_edges[2] = 0, fx_edges[3] = 128;
    } else {
        fx_screen_centre_x = within(40, 160), fx_screen_centre_y = within(30, 100);
        fx_edges[0] = within(0, 50), fx_edges[1] = within(150, 216);
        fx_edges[2] = within(0, 50), fx_edges[3] = within(80, 128);
    }
}

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 25)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld: ", model_current, iteration);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* the decomp's routine, its pixels written out; then the RAM put back for
   the port's */
static void old_run(void (*f)(void))
{
    memcpy(start_p, pakram, sizeof pakram);
    model_pixels_empty();
    side = "the decomp's routine";
    alarm(5);
    f();
    model_pixels_flush();
    side = "the port's";
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same_ram(void)
{
    size_t i;

    fx_pixels_flush();
    alarm(0);
    cases++;
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0)) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            if (getenv("ALLDIFF"))
                for (i++; i < sizeof pakram; i++)
                    if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0))
                        fprintf(stderr, "  $%04zX = %02X, the decomp's %02X (was %02X)\n", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
}

/* the programs' inputs: colours, place, sizes, steps, a bitmap */
static void inputs(void)
{
    int k;

    for (k = 0; k < 4; k++)
        pakram[0x0200 + k] = (rnd() & 3) ? (uint8_t)rnd() : 0;
    FXU(0x003A) = (rnd() & 3) ? 216 : (uint16_t)within(100, 216);
    FXU(0x003E) = (rnd() & 3) ? 128 : (uint16_t)within(60, 128);
    FXW(0x020A) = (rnd() & 7) ? within(-0x30, 0xF0) : (int16_t)within(0xD0, 0xFFF4);
    if (!(rnd() & 7)) {
        static const uint16_t edges[6] = {0x00D0, 0x00D1, 0xFFF0, 0xFFF1, 0x0000, 0xFFFF};

        FXU(0x020A) = edges[rnd() % 6];
    }
    FXW(0x020C) = (rnd() & 7) ? within(-0x30, 0x80) : within(-0x60, 0x7F);
    FXU(0x020E) = (uint16_t)within(0, 0x400);
    FXU(0x0210) = (rnd() & 1) ? (uint16_t)within(1, 0x20) : (uint16_t)within(0, 0x400);
    FXU(0x0212) = (uint16_t)within(0x40, 0x300);
    FXU(0x0214) = (uint16_t)within(0x40, 0x300);
    FXU(0x0216) = (rnd() & 15) ? (uint16_t)(0x8000 | rnd()) : 0;
    FXU(0x021C) = (uint16_t)rnd();
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

static long n;

static void program(const char *name, void (*old)(void), void (*port)(void))
{
    long i;

    CASE(name) {
        fresh();
        inputs();
        /* sizes the game uses: none that make a width or height of 0
           (65536 pixels) */
        if (old == old_abi_FxPictureRiseSplit40 || old == old_abi_FxPictureRiseSplit60 || old == old_abi_FxPictureRise40
            || old == old_abi_FxPictureRise60)
            FXU(0x020C) = (uint16_t)within(1, 0x40);
        if (old == old_abi_FxGlyph16RowsFromBottom || old == old_abi_FxGlyph16RowsFromTop)
            FXU(0x0210) = (uint16_t)within(1, 0x20);
        if (old == old_abi_FxScaledSprite13 || old == old_abi_FxScaledSprite12 || old == old_abi_FxScaledSpriteEighth
            || old == old_abi_FxScaledSpriteAdvance) {
            FXU(0x020E) = (uint16_t)within(0x20, (old == old_abi_FxScaledSprite13 || old == old_abi_FxScaledSprite12) ? 0x1FD : 0x7FF);
            while (!((uint16_t)((int8_t)div2((int16_t)FXU(0x020E)) * 12 * 16) >> 8))
                FXU(0x020E) = (uint16_t)within(0x20, 0x1FD);
            FXU(0x0210) = (uint16_t)within(0x20, 0x7FF);
        }
        if (sigsetjmp(escape, 1)) {
            skipped++;
            continue;
        }
        old_run(old);
        port();
        same_ram();
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: fxhud_check ROM [N]\n");
        return 2;
    }
    n = argc > 2 ? atol(argv[2]) : 20000;
    signal(SIGALRM, runaway);
    model_load_rom(argv[1]);
    memcpy(rom_copy, srfx_rom_image, sizeof rom_copy);
    srfx_rom_image = rom_copy;
    program("FxTiles2bppTo4bppHigh", old_abi_FxTiles2bppTo4bppHigh, fx_tiles_2bpp_to_4bpp_high);
    program("FxTiles2bppTo4bppLow", old_abi_FxTiles2bppTo4bppLow, fx_tiles_2bpp_to_4bpp_low);
    program("FxNoiseFill", old_abi_FxNoiseFill, fx_noise_fill);
    program("FxNoiseFillScreen", old_abi_FxNoiseFillScreen, fx_noise_fill_screen);
    program("FxPictureRiseSplit40", old_abi_FxPictureRiseSplit40, fx_picture_rise_split_40);
    program("FxPictureRiseSplit60", old_abi_FxPictureRiseSplit60, fx_picture_rise_split_60);
    program("FxPictureRise40", old_abi_FxPictureRise40, fx_picture_rise_40);
    program("FxPictureRise60", old_abi_FxPictureRise60, fx_picture_rise_60);
    program("FxGlyph16RowsFromBottom", old_abi_FxGlyph16RowsFromBottom, fx_glyph_rows_from_bottom);
    program("FxGlyph16RowsFromTop", old_abi_FxGlyph16RowsFromTop, fx_glyph_rows_from_top);
    program("FxGlyph8x16", old_abi_FxGlyph8x16, fx_glyph_8x16);
    program("FxGlyph8x8", old_abi_FxGlyph8x8, fx_glyph_8x8);
    program("FxGlyph24x16", old_abi_FxGlyph24x16, fx_glyph_24x16);
    program("FxGlyph16x16", old_abi_FxGlyph16x16, fx_glyph_16x16);
    program("FxGlyph16x32", old_abi_FxGlyph16x32, fx_glyph_16x32);
    program("FxGlyph24x32Bank04", old_abi_FxGlyph24x32Bank04, fx_glyph_24x32_bank04);
    program("FxGlyph16x32Double", old_abi_FxGlyph16x32Double, fx_glyph_16x32_double);
    program("FxScaledSprite13", old_abi_FxScaledSprite13, fx_scaled_sprite_13);
    program("FxScaledSprite12", old_abi_FxScaledSprite12, fx_scaled_sprite_12);
    program("FxScaledSpriteEighth", old_abi_FxScaledSpriteEighth, fx_scaled_sprite_eighth);
    program("FxScaledSpriteAdvance", old_abi_FxScaledSpriteAdvance, fx_scaled_sprite_advance);
    program("FxSprite24x40", old_abi_FxSprite24x40, fx_sprite_24x40);
    program("FxSprite16x32", old_abi_FxSprite16x32, fx_sprite_16x32);
    printf("fxhud_check: %ld cases, %ld differ; %ld skipped (the decomp ran past 5 s)\n", cases, failed, skipped);
    return failed != 0;
}
