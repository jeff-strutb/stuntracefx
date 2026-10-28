/*
 * picture.c: Enhanced mode's picture (remaster.h): the Super FX's finished
 * screen as the characters VRAM would hold, each a block of colour
 * indices, for the console's character canvas (nativeshvc's Enhanced
 * extension) in place of the upload stages' DMAs.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "console_ext.h"
#include "remaster.h"

/* The colour index of pixel (x, y) of a character's planar data at bpp
   bits a pixel (2, 4, 8), as the PPU reads it: bitplane pairs of 16 bytes,
   a row's two planes in consecutive bytes, the leftmost pixel bit 7. */
static uint8_t char_pixel(const uint8_t *c, unsigned bpp, unsigned x, unsigned y)
{
    uint8_t v = 0;
    unsigned n;
    for (n = 0; n < bpp; n++)
        v |= (uint8_t)(((c[(n >> 1) * 16 + y * 2 + (n & 1)] >> (7 - x)) & 1) << n);
    return v;
}

void rm_picture_chars(const uint8_t *data, unsigned count, unsigned bpp, unsigned scale, uint8_t *out)
{
    unsigned k, x, y, i, j, side = 8 * scale;
    for (k = 0; k < count; k++) {
        const uint8_t *c = data + (size_t)k * bpp * 8;
        uint8_t *o = out + (size_t)k * side * side;
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++) {
                uint8_t v = char_pixel(c, bpp, x, y);
                for (j = 0; j < scale; j++)
                    for (i = 0; i < scale; i++)
                        o[(y * scale + j) * side + x * scale + i] = v;
            }
    }
}

/* The canvas as handed: its source characters kept (the Super FX draws
   over its RAM), so a new k (the window resized) hands it again */
static uint8_t canvas_src[0x8000];
static uint8_t *texels;
static size_t texels_size;
static unsigned canvas_word, canvas_count, canvas_bpp = 8;
/* VRAM as the uploads the canvas stands in for would have left it
   (rm_canvas_copy) */
static uint8_t vram_shadow[0x10000];
static int canvas_k, canvas_on, canvas_seen, canvas_owner, owner_now;

int rm_canvas_ready(void)
{
    return rm_enhanced() && nshvc_ext_canvas_scale() > 0;
}

static void canvas_hand(void)
{
    size_t need = (size_t)canvas_count * 64 * canvas_k * canvas_k;

    if (need > texels_size) {
        free(texels);
        texels = malloc(need);
        texels_size = texels ? need : 0;
        if (!texels) {
            rm_canvas_off();
            return;
        }
    }
    rm_picture_chars(canvas_src, canvas_count, canvas_bpp, (unsigned)canvas_k, texels);
    nshvc_ext_canvas_chars(0, canvas_word, (int)canvas_count, texels, canvas_k);
    canvas_on = 1;
}

void rm_canvas_show(unsigned word, uint16_t src, unsigned count)
{
    canvas_seen = 1;
    if (count * 64 > sizeof canvas_src || src + count * 64 > 0x10000)
        return;
    memcpy(canvas_src, nshvc_sram() + src, count * 64);
    canvas_owner = owner_now;
    canvas_word = word;
    canvas_count = count;
    canvas_bpp = 8;
    canvas_k = nshvc_ext_canvas_scale();
    canvas_hand();
}

void rm_canvas_copy(unsigned word, uint16_t src, unsigned bytes, unsigned stride, unsigned rows)
{
    const uint8_t *ram = nshvc_sram();
    unsigned r;

    for (r = 0; r < rows; r++, src = (uint16_t)(src + stride), word += bytes / 2)
        if (word * 2 + bytes <= sizeof vram_shadow && src + bytes <= 0x10000)
            memcpy(vram_shadow + word * 2, ram + src, bytes);
}

void rm_canvas_show_copied(unsigned word, unsigned count, unsigned bpp)
{
    size_t n = (size_t)count * bpp * 8;

    canvas_seen = 1;
    if (n > sizeof canvas_src || word * 2 + n > sizeof vram_shadow)
        return;
    memcpy(canvas_src, vram_shadow + word * 2, n);
    canvas_owner = owner_now;
    canvas_word = word;
    canvas_count = count;
    canvas_bpp = bpp;
    canvas_k = nshvc_ext_canvas_scale();
    canvas_hand();
}

void rm_canvas_narrow(unsigned word, unsigned count)
{
    unsigned w = canvas_bpp * 4;       /* words a character */

    if (!canvas_on || word < canvas_word || word + count * w > canvas_word + canvas_count * w)
        return;
    memmove(canvas_src, canvas_src + (word - canvas_word) * 2, (size_t)count * canvas_bpp * 8);
    canvas_word = word;
    canvas_count = count;
    canvas_hand();
}

void rm_canvas_off(void)
{
    if (canvas_on)
        nshvc_ext_canvas_chars(0, 0, 0, NULL, 1);
    canvas_on = 0;
}

void rm_canvas_kept(int owner)
{
    owner_now = owner;
    if (owner == canvas_owner)
        canvas_seen = 1;
}

void rm_canvas_frame_end(void)
{
    int k;

    if (canvas_on && !canvas_seen)       /* the screen that handed it is gone */
        rm_canvas_off();
    canvas_seen = 0;
    if (!canvas_on)
        return;
    k = nshvc_ext_canvas_scale();
    if (k > 0 && k != canvas_k) {
        canvas_k = k;
        canvas_hand();
    }
}
