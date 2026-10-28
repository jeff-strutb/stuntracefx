/*
 * picture.c: Enhanced mode's picture (remaster.h): the Super FX's finished
 * screen as the characters VRAM would hold, each a block of colour
 * indices, for the console's character canvas (nativeshvc's Enhanced
 * extension) in place of the upload stages' DMAs.
 */
#include <stddef.h>
#include <stdint.h>

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
