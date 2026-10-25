/*
 * fx_text.c: strings of text and pictures drawn into the Super FX's
 * screen, and the 2-bit bitmaps they are made of.
 *
 * A string is bytes in ROM (bank fx_text_bank, from fx_text_at): from '0'
 * up a glyph (16 x 32, or 8 x 16 in the small font), $0D a new line, 0
 * the end, and $F5-$FF commands: $FF the place (two bytes), $FE the
 * palette (a byte: four bytes at FX_PALETTES each), $FD the font (a byte:
 * 0 big), $FC a space, $F5 half a line down, $F6-$FB pictures from ROM
 * bank $15. Any other byte only moves along.
 */
#include "game.h"
#include "render.h"

#define fx_text_at          FXU(0x2732)
#define fx_text_bank        pakram[0x2734]
#define fx_text_x           FXU(0x2738)
#define fx_text_y           FXU(0x273A)

#define FX_PALETTES         0x2702u
#define FX_GLYPHS_BIG       0xF4D2u         /* bank 1: each glyph's place in bank $15 */
#define FX_GLYPHS_SMALL     0xF52Eu
#define FX_PICTURES_BANK    0x15
#define FX_LABELS           0xD1CBu         /* bank 1: each car's label string */

static uint8_t rb(uint8_t bank, uint16_t at)
{
    return rom_at((uint32_t)(bank & 0x7F) << 16 | at)[0];
}

/* A bitmap of `w` x `h` pixels from ROM (bank $15 at $8000 + at, rows 32
   bytes apart, four pixels a byte from bit 0 up) with its top left at
   (x, y), each pixel's colour the byte of the palette at `palette` it
   picks. Returns y past its last row. */
uint16_t fx_draw_bitmap(uint16_t at, int w, int h, uint16_t x, uint16_t y, uint16_t palette)
{
    uint16_t row = (uint16_t)(0x8000 + at);
    int i, j;

    for (i = 0; i < h; i++, row = (uint16_t)(row + 32), y++) {
        for (j = 0; j < w; j++) {
            uint8_t b = rb(FX_PICTURES_BANK, (uint16_t)(row + j / 4));

            fx_set_colour(pakram[(uint16_t)(palette + (b >> 2 * (j & 3) & 3))]);
            fx_plot((uint8_t)(x + j), (uint8_t)y);
        }
    }
    return y;
}

/* The string at fx_text_at from (fx_text_x, fx_text_y). */
void fx_text_draw(void)
{
    static const struct {
        uint16_t at;
        uint8_t w, h;
    } pictures[6] = {
        {0x3D1C, 16, 24},       /* $F6 */
        {0x3A00, 120, 24},      /* $F7 */
        {0x3700, 120, 24},      /* $F8 */
        {0x1810, 64, 16},       /* $F9 */
        {0x3B00, 120, 16},      /* $FA */
        {0x3700, 120, 32},      /* $FB */
    };
    uint16_t start = fx_text_x, x, y = fx_text_y, line = 0x20, palette = FX_PALETTES, at = fx_text_at;
    bool small = false;

    fx_plot_options = 0;
    if (!at) {
        fx_pixels_flush();
        return;
    }
    x = start;
    for (;;) {
        uint8_t bank = fx_text_bank, b = rb(bank, at++);

        if (!b) {
            fx_pixels_flush();
            return;
        }
        switch (b) {
        case 0xFF:
            x = start = rb(bank, at);
            y = rb(bank, (uint16_t)(at + 1));
            at = (uint16_t)(at + 2);
            continue;
        case 0xFE:
            palette = (uint16_t)(FX_PALETTES + 4 * rb(bank, at++));
            continue;
        case 0xFD:
            small = rb(bank, at++) != 0;
            line = small ? 0x10 : 0x20;
            continue;
        case 0xFC:
            x = (uint16_t)(x + (small ? 4 : 8));
            continue;
        case 0xF5:
            x = start;
            y = (uint16_t)(y + (line >> 1));
            continue;
        case 0xF6: case 0xF7: case 0xF8: case 0xF9: case 0xFA: case 0xFB:
            y = fx_draw_bitmap(pictures[b - 0xF6].at, pictures[b - 0xF6].w, pictures[b - 0xF6].h, x, y, palette);
            continue;
        case 0x0D:
            x = start;
            y = (uint16_t)(y + line);
            continue;
        }
        if (b >= 0x30 && b < 0x80) {
            uint16_t g = (uint16_t)((small ? FX_GLYPHS_SMALL : FX_GLYPHS_BIG) + (b - 0x30) * 2);

            fx_draw_bitmap((uint16_t)(rb(1, g) | rb(1, (uint16_t)(g + 1)) << 8), small ? 8 : 16, small ? 16 : 32, x, y,
                           palette);
        }
        x = (uint16_t)(x + (small ? 10 : 20));
    }
}

/* The program the S-CPU starts to draw a string. */
void fx_text_program(void)
{
    fx_text_draw();
}

/* The label of the nearest car ($2648; not the player's, $2476, nor
   $2478), when $273C has bit 0 and $264C is not negative: its string (by
   the car's +$96) at Y $68 and X half of $264A from the screen's centre,
   held to 0 .. right edge - $11. */
void fx_text_notice(void)
{
    uint16_t car = FXU(0x2648), g;
    int16_t x;

    if (!(FXU(0x273C) & 1) || !car || car == FXU(0x2476) || car == FXU(0x2478) || FXW(0x264C) < 0)
        return;
    x = wrap16(div2(FXW(0x264A)) + fx_screen_centre_x);
    if (x < 0)
        x = 0;
    if (wrap16(x - wrap16(fx_edges[1] - 0x11)) >= 0)
        x = wrap16(fx_edges[1] - 0x11);
    fx_text_x = (uint16_t)x;
    fx_text_y = 0x68;
    FXU(0x2734) = 1;
    g = (uint16_t)(FX_LABELS + FXU((uint16_t)(car + 0x96)) * 2);
    fx_text_at = (uint16_t)(rb(1, g) | rb(1, (uint16_t)(g + 1)) << 8);
    fx_text_draw();
    fx_text_at = 0;
}
