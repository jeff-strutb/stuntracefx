/*
 * fx_hud.c: the Super FX programs that draw the 2D pieces of the screen:
 * glyphs and digits (rolled into their cells), the rising picture, scaled
 * sprites, the noise fill, and a tile conversion. The S-CPU sets their
 * inputs in the game pak RAM and starts each one.
 *
 * Bitmaps are two bits a pixel in ROM bank $15, rows 32 bytes apart, the
 * low two bits of a byte its leftmost pixel; a pixel's bits pick one of
 * the four colour bytes at FX_HUD_COLOURS, and colour 0 leaves the screen
 * as it was. Most programs read the screen back (RPIX) after each byte
 * or row, which writes the pixel caches out; one that stops without
 * doing so leaves its last pixels for the next program to write.
 */
#include "game.h"
#include "render.h"
#include "draw_list.h"

#define FX_HUD_COLOURS      0x0200u
#define hud_x               FXU(0x020A)
#define hud_y               FXU(0x020C)
#define hud_advance         FXU(0x020E)     /* also the size across */
#define hud_rows            FXU(0x0210)     /* also the size down */
#define hud_step_x          FXU(0x0212)     /* 8.8 */
#define hud_step_y          FXU(0x0214)
#define hud_bitmap          FXU(0x0216)
#define hud_noise_seed      FXU(0x021C)

#define FX_HUD_BANK         0x15
#define FX_PICTURE          0x3D00u         /* the rising picture */
#define FX_NOISE_RAMP       0x3C00u         /* eight colour words */

static uint8_t rb(uint8_t bank, uint16_t at)
{
    return rom_at((uint32_t)bank << 16 | at)[0];
}

/* A pixel of colour byte `bits` of the table, at (*x, y); x moves on. */
static void hud_pixel(unsigned bits, uint16_t *x, uint16_t y, int twice)
{
    fx_set_colour(pakram[(uint16_t)(FX_HUD_COLOURS + bits)]);
    fx_plot((uint8_t)(*x)++, (uint8_t)y);
    if (twice)
        fx_plot((uint8_t)(*x)++, (uint8_t)y);
}

/* A bitmap byte's four pixels, from its low bits up. */
static void hud_byte(uint8_t bank, uint16_t *at, uint16_t *x, uint16_t y, int twice)
{
    uint8_t b = rb(bank, (*at)++);
    int k;

    for (k = 0; k < 4; k++)
        hud_pixel(b >> 2 * k & 3, x, y, twice);
}

/* `rows` rows (at least one; 0 is 65536) of `bytes` bytes from `at`,
   from (x, y) down; with `each`, the caches written out after each byte. */
static void hud_rows_draw(uint8_t bank, uint16_t at, uint16_t rows, unsigned bytes, uint16_t x, uint16_t y, int each)
{
    do {
        uint16_t px = x, p = at;
        unsigned b;

        for (b = 0; b < bytes; b++) {
            hud_byte(bank, &p, &px, y, 0);
            if (each)
                fx_pixels_flush();
        }
        at = (uint16_t)(at + 32);
        y++;
    } while (--rows);
}

/* x from hud_x, which moves on by the advance */
static uint16_t glyph_x(uint16_t advance)
{
    uint16_t x = hud_x;

    hud_x = (uint16_t)(x + advance);
    return x;
}

/* ---- glyphs ---- */

/* The bottom hud_rows rows of a 16 x 32 glyph at (hud_x, hud_y), x on by
   hud_advance. */
void fx_glyph_rows_from_bottom(void)
{
    uint16_t x = glyph_x(hud_advance);

    fx_plot_options = 0;
    if (!hud_bitmap)
        return;
    hud_rows_draw(FX_HUD_BANK, (uint16_t)(hud_bitmap + (uint16_t)(0x20 - hud_rows) * 32), hud_rows, 4, x, hud_y, 1);
}

/* The top hud_rows rows of a 16 x 32 glyph, at the bottom of the 32-line
   cell at (hud_x, hud_y); with 32 rows the whole glyph. */
void fx_glyph_rows_from_top(void)
{
    uint16_t y = (uint16_t)(0x20 + hud_y - hud_rows), x;

    fx_plot_options = 0;
    x = glyph_x(hud_advance);
    if (!hud_bitmap)
        return;
    hud_rows_draw(FX_HUD_BANK, hud_bitmap, hud_rows, 4, x, y, 1);
}

void fx_glyph_8x16(void)
{
    uint16_t x = glyph_x(8);

    fx_plot_options = 0;
    if (hud_bitmap)
        hud_rows_draw(FX_HUD_BANK, hud_bitmap, 16, 2, x, hud_y, 1);
}

void fx_glyph_8x8(void)
{
    uint16_t x = glyph_x(8);

    fx_plot_options = 0;
    if (hud_bitmap)
        hud_rows_draw(FX_HUD_BANK, hud_bitmap, 8, 2, x, hud_y, 1);
}

/* 24 x 16, though x moves on by 16 */
void fx_glyph_24x16(void)
{
    uint16_t x = glyph_x(0x10);

    fx_plot_options = 0;
    hud_rows_draw(FX_HUD_BANK, hud_bitmap, 16, 6, x, hud_y, 1);
    fx_pixels_flush();
}

void fx_glyph_16x16(void)
{
    uint16_t x = glyph_x(0x10);

    fx_plot_options = 0;
    hud_rows_draw(FX_HUD_BANK, hud_bitmap, 16, 4, x, hud_y, 1);
    fx_pixels_flush();
}

void fx_glyph_16x32(void)
{
    uint16_t x = glyph_x(0x10);

    fx_plot_options = 0;
    if (!hud_bitmap)
        return;
    hud_rows_draw(FX_HUD_BANK, hud_bitmap, 32, 4, x, hud_y, 0);
    fx_pixels_flush();
}

/* 24 x 32 from ROM bank 4, x on by 16 */
void fx_glyph_24x32_bank04(void)
{
    uint16_t x = glyph_x(0x10);

    fx_plot_options = 0;
    hud_rows_draw(0x04, hud_bitmap, 32, 6, x, hud_y, 0);
    fx_pixels_flush();
}

/* A 16 x 32 glyph at twice its size, x on by 32. */
void fx_glyph_16x32_double(void)
{
    uint16_t x = glyph_x(0x20), y = hud_y, at = hud_bitmap, rows = 32;

    fx_plot_options = 0;
    if (!at)
        return;
    do {
        int line, b;

        for (line = 0; line < 2; line++, y++) {
            uint16_t px = x, p = at;

            for (b = 0; b < 4; b++) {
                hud_byte(FX_HUD_BANK, &p, &px, y, 1);
                fx_pixels_flush();
            }
        }
        at = (uint16_t)(at + 32);
    } while (--rows);
}

/* ---- the rising picture ---- */

/* The top hud_y rows of the picture (from byte `src` of each row, `bytes`
   bytes) at x, its last row just above line `base`. */
static void picture(uint16_t x, uint16_t base, uint16_t src, unsigned bytes)
{
    fx_plot_options = 0;
    hud_rows_draw(FX_HUD_BANK, (uint16_t)(FX_PICTURE + src), hud_y, bytes, x, (uint16_t)(base - hud_y), 0);
    fx_pixels_flush();
}

void fx_picture_rise_split_40(void)
{
    picture(0x1C, 0x40, 0x0E, 14);
    picture(0x1C + 0x38, 0x40, 0x00, 10);
}

void fx_picture_rise_split_60(void)
{
    picture(0x34, 0x60, 0x0E, 14);
    picture(0x34 + 0x38, 0x60, 0x00, 10);
}

void fx_picture_rise_40(void)
{
    picture(0x28, 0x40, 0x00, 14);
}

void fx_picture_rise_60(void)
{
    picture(0x4C, 0x60, 0x00, 14);
}

/* ---- scaled sprites ---- */

/* A sprite w x h at (x, y) from the bitmap at hud_bitmap, the bitmap
   stepped by hud_step_x across and hud_step_y down (8.8, by the 16-bit
   products the Super FX's FMULT gives), clipped to the screen's width and
   height; the caches written out after each row and at the end. */
static void scaled_sprite(uint16_t w, uint16_t h, uint16_t x, uint16_t y)
{
    uint16_t px0, py, width = FXU(0x003A), height = FXU(0x003E), sx, sy;

    fx_plot_options = 0;
    if (!hud_bitmap)
        return;
    if ((int16_t)x < 0) {
        w = (uint16_t)(x + w);
        if ((int16_t)w < 0)
            return;
        px0 = 0;
    } else {
        if (x >= width)
            return;
        if ((uint16_t)(x + w) >= width)
            w = (uint16_t)(width - x);
        px0 = x;
    }
    if ((int16_t)y < 0) {
        h = (uint16_t)(y + h);
        if ((int16_t)h < 0) {
            fx_pixels_flush();
            return;
        }
        py = 0;
    } else {
        if (y >= height)
            return;
        if ((uint16_t)(y + h) >= height)
            h = (uint16_t)(height - y);
        py = y;
    }
    sx = hud_step_x;
    sy = hud_step_y;
    do {
        uint16_t px = px0, n = w;

        do {
            uint16_t at = hud_bitmap, dy = (uint16_t)(py - y), dx = (uint16_t)(px - x);
            int16_t c;
            uint8_t b;

            if (dy)
                at = (uint16_t)(at + (uint16_t)fmult((int16_t)(uint16_t)(dy << 8 | dy >> 8), (int16_t)sy) * 32);
            c = fmult((int16_t)(uint16_t)(dx << 8 | dx >> 8), (int16_t)sx);
            b = rb(FX_HUD_BANK, (uint16_t)(at + asr16(c, 2)));
            hud_pixel((unsigned)(b >> 2 * (c & 3)) & 3, &px, py, 0);
        } while (--n);
        fx_pixels_flush();
        py++;
    } while (--h);
    fx_pixels_flush();
}

/* (size across / 2) times `k` / 16 wide (MULT: the low byte, signed),
   (size down) / 16 high */
static void scaled_k(int k)
{
    int16_t m = (int16_t)((int8_t)div2((int16_t)hud_advance) * k);
    uint16_t w = (uint16_t)(m * 16) >> 8;

    scaled_sprite(w, (uint16_t)(hud_rows * 16) >> 8, hud_x, hud_y);
}

void fx_scaled_sprite_13(void)
{
    scaled_k(13);
}

void fx_scaled_sprite_12(void)
{
    scaled_k(12);
}

/* (size across) / 32 wide, (size down) / 32 high */
void fx_scaled_sprite_eighth(void)
{
    scaled_sprite((uint16_t)(hud_advance * 8) >> 8, (uint16_t)(hud_rows * 8) >> 8, hud_x, hud_y);
}

/* (size across) / 16 wide, (size down) / 8 high, x on by the width */
void fx_scaled_sprite_advance(void)
{
    uint16_t w = (uint16_t)(hud_advance * 16) >> 8, x = glyph_x(w);

    scaled_sprite(w, (uint16_t)(hud_rows * 32) >> 8, x, hud_y);
}

void fx_sprite_24x40(void)
{
    uint16_t x = glyph_x(0x18);

    scaled_sprite(0x18, 0x28, x, hud_y);
}

/* 16 x 32 unscaled, x on by 16; at x from $D1 to $FFF0 as
   fx_scaled_sprite_advance instead. */
void fx_sprite_16x32(void)
{
    uint16_t x;

    hud_step_x = 0x0100;
    hud_step_y = 0x0100;
    if (hud_x <= 0xFFF0 && hud_x > 0xD0) {
        fx_scaled_sprite_advance();
        return;
    }
    x = glyph_x(0x10);
    scaled_sprite(0x10, 0x20, x, hud_y);
}

/* ---- the noise fill ---- */

/* w pixels across and h lines down from the top left, each one of the
   eight colour words at FX_NOISE_RAMP picked by the noise generator from
   hud_noise_seed (kept back there). */
static void noise(uint16_t w, uint16_t h)
{
    uint16_t seed = hud_noise_seed, y = 0;

    fx_plot_options = 0;
    do {
        uint16_t x = 0, n = w;

        do {
            seed = fx_noise_step(seed);
            fx_set_colour(pakram[(uint16_t)(FX_NOISE_RAMP + 2 * (seed & 7))]);
            fx_plot((uint8_t)x++, (uint8_t)y);
        } while (--n);
        y++;
    } while (--h);
    fx_pixels_flush();
    hud_noise_seed = seed;
}

void fx_noise_fill(void)
{
    noise(0x10, 0x80);
}

void fx_noise_fill_screen(void)
{
    noise(FXU(0x003A), (uint16_t)(FXU(0x003E) + 1));
}

/* ---- tiles ---- */

/* 96 tiles of eight bytes from `from` as tiles of 32 bytes at `to`: each
   byte followed by a 0, then sixteen zeros. */
static void tiles(uint16_t from, uint16_t to)
{
    int t, k;

    for (t = 0; t < 96; t++) {
        for (k = 0; k < 8; k++) {
            pakram[to++] = pakram[from++];
            pakram[to++] = 0;
        }
        for (k = 0; k < 16; k++)
            pakram[to++] = 0;
    }
}

void fx_tiles_2bpp_to_4bpp_high(void)
{
    tiles(0xC000, 0xD000);
}

void fx_tiles_2bpp_to_4bpp_low(void)
{
    tiles(0x3000, 0x4000);
}
