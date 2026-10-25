/*
 * fx_screen.c: the Super FX's screen in the game pak RAM, and the frame's
 * draw list drawn into it.
 *
 * The screen is characters of 8 x 8 pixels, a row of a character two
 * bitplane bytes at a time, as the S-CPU copies them to VRAM: 2, 4 or 8
 * bits a pixel (SCMR's mode), the characters ordered down columns for a
 * screen 128, 160 or 192 lines high (or as sprites, POR's OBJ), from the
 * base SCBR gives in kilobytes. A pixel written here is the pixel PLOT
 * writes once the Super FX's pixel caches are written out.
 */
#include "game.h"
#include "render.h"
#include "draw_list.h"

FxScreen fx_screen;

static unsigned bits_per_pixel(const FxScreen *s)
{
    return 2u << (s->mode - (s->mode >> 1));
}

/* the address of the row of pixel (x, y)'s character */
static uint16_t char_row(const FxScreen *s, uint8_t x, uint8_t y)
{
    unsigned c;

    switch (s->height) {
    case 0: c = ((x & 0xF8u) << 1) + ((y & 0xF8u) >> 3); break;
    case 1: c = ((x & 0xF8u) << 1) + ((x & 0xF8u) >> 1) + ((y & 0xF8u) >> 3); break;
    case 2: c = ((x & 0xF8u) << 1) + (x & 0xF8u) + ((y & 0xF8u) >> 3); break;
    default: c = ((y & 0x80u) << 2) + ((x & 0x80u) << 1) + ((y & 0x78u) << 1) + ((x & 0x78u) >> 3); break;
    }
    return (uint16_t)(c * (bits_per_pixel(s) << 3) + ((unsigned)s->base << 10) + (unsigned)(y & 7) * 2);
}

void fx_screen_put(const FxScreen *s, uint8_t x, uint8_t y, uint8_t colour)
{
    uint16_t row = char_row(s, x, y);
    unsigned n, bpp = bits_per_pixel(s);
    uint8_t bit = (uint8_t)(0x80 >> (x & 7));

    for (n = 0; n < bpp; n++) {
        uint8_t *b = &pakram[(uint16_t)(row + ((n >> 1) << 4) + (n & 1))];

        *b = (uint8_t)((colour >> n & 1) ? *b | bit : *b & ~bit);
    }
}

uint8_t fx_screen_get(const FxScreen *s, uint8_t x, uint8_t y)
{
    uint16_t row = char_row(s, x, y);
    unsigned n, bpp = bits_per_pixel(s);
    uint8_t c = 0;

    for (n = 0; n < bpp; n++)
        c |= (uint8_t)((pakram[(uint16_t)(row + ((n >> 1) << 4) + (n & 1))] >> (7 - (x & 7)) & 1) << n);
    return c;
}

static void put_pixel(void *ctx, uint8_t x, uint8_t y, uint8_t colour)
{
    fx_screen_put((const FxScreen *)ctx, x, y, colour);
}

static void put_span(void *ctx, const FxSpan *s)
{
    const FxDrawSource *src = (const FxDrawSource *)ctx;

    fx_fill_span_pixels(src, put_pixel, &fx_screen, s);
}

/* The frame's queued shapes (the list at $08AA up to fx_draw_list_end)
   drawn into the screen, the list then emptied. */
void fx_draw_list_program(void)
{
    FxDrawSource src;

    if (fx_draw_list_end == FX_DRAW_LIST)
        return;
    src.ram = pakram;
    src.rom = srfx_rom_image;
    src.rom_size = 0x100000;
    src.mode = fx_screen.mode;
    src.layout = fx_screen.height;
    fx_list_spans(&src, put_span, &src);
    fx_draw_list_end = FX_DRAW_LIST;
}
