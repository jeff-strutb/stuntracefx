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

/* The plot colour and options (CMODE): bit 0 colour 0 written too, 1
   dither, 2 the colour's high nibble from the source's high nibble, 3 the
   high nibble kept, 4 the sprite layout. */
uint8_t fx_colour, fx_plot_options;

static FxScreen plot_screen(void)
{
    FxScreen s = fx_screen;

    if (fx_plot_options & 0x10)
        s.height = 3;
    return s;
}

/* COLOR and GETC: the plot colour from a byte, as the options shape it. */
void fx_set_colour(uint8_t v)
{
    if (fx_plot_options & 0x04)
        fx_colour = (uint8_t)((fx_colour & 0xF0) | v >> 4);
    else if (fx_plot_options & 0x08)
        fx_colour = (uint8_t)((fx_colour & 0xF0) | (v & 0x0F));
    else
        fx_colour = v;
}

/* The Super FX's two pixel caches: PLOT gathers the pixels of one row of
   eight (one character's row) and writes them out when it moves to
   another row, or when RPIX reads, through the screen as it is set up at
   that time; pixels left in them when a program stops are written by a
   later one. */
static struct {
    uint16_t row;           /* y * 32 + x / 8 */
    uint8_t set;            /* the pixels plotted, bit 7 the leftmost */
    uint8_t c[8];
} cache[2] = {{0xFFFF, 0, {0}}, {0xFFFF, 0, {0}}};

static void cache_write(int i)
{
    FxScreen s = plot_screen();
    uint8_t x0 = (uint8_t)(cache[i].row << 3), y = (uint8_t)(cache[i].row >> 5);
    int k;

    if (!cache[i].set)
        return;
    for (k = 0; k < 8; k++)
        if (cache[i].set & 0x80 >> k)
            fx_screen_put(&s, (uint8_t)(x0 + k), y, cache[i].c[k]);
    cache[i].set = 0;
}

static void cache_next(void)
{
    cache_write(1);
    cache[1] = cache[0];
    cache[0].set = 0;
}

void fx_pixels_flush(void)
{
    cache_write(1);
    cache_write(0);
}

/* A cache as the Super FX holds it: its row, the pixels pending (bit 0
   the rightmost) and their colours (the rightmost first). */
void fx_pixels_set(int i, uint16_t row, uint8_t pending, const uint8_t colours[8])
{
    int k;

    cache[i].row = row;
    cache[i].set = 0;
    for (k = 0; k < 8; k++) {
        cache[i].c[k] = colours[7 - k];
        if (pending & 1 << (7 - k))
            cache[i].set |= (uint8_t)(0x80 >> k);
    }
}

/* PLOT: colour 0 (its low nibble outside 8 bits, or with the high nibble
   kept) is not written unless option 0 is set; dither takes the high
   nibble on odd pixels. */
void fx_plot(uint8_t x, uint8_t y)
{
    uint8_t c = fx_colour, md = fx_screen.mode;

    if (!(fx_plot_options & 0x01)) {
        if (md == 3 ? ((fx_plot_options & 0x08) ? !(c & 0x0F) : !c) : !(c & 0x0F))
            return;
    }
    if ((fx_plot_options & 0x02) && md != 3) {
        if ((x ^ y) & 1)
            c >>= 4;
        c &= 0x0F;
    }
    fx_plot_put(x, y, c);
}

/* A pixel of a colour PLOT's rules have already shaped, into the caches. */
void fx_plot_put(uint8_t x, uint8_t y, uint8_t c)
{
    uint16_t row = (uint16_t)(y << 5 | x >> 3);

    if (row != cache[0].row) {
        cache_next();
        cache[0].row = row;
    }
    cache[0].c[x & 7] = c;
    cache[0].set |= (uint8_t)(0x80 >> (x & 7));
    if (cache[0].set == 0xFF)
        cache_next();
}

/* RPIX: the caches written out, then the colour of a pixel. */
uint8_t fx_read_pixel(uint8_t x, uint8_t y)
{
    FxScreen s = plot_screen();

    fx_pixels_flush();
    return fx_screen_get(&s, x, y);
}

static void put_pixel(void *ctx, uint8_t x, uint8_t y, uint8_t colour)
{
    (void)ctx;
    fx_plot_put(x, y, colour);
}

/* each row drawn with its shape's plot options set, as CMODE sets them */
static void put_span(void *ctx, const FxSpan *s)
{
    const FxDrawSource *src = (const FxDrawSource *)ctx;

    fx_plot_options = s->options;
    fx_fill_span_pixels(src, put_pixel, 0, s);
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
