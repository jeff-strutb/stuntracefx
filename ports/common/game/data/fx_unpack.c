/*
 * fx_unpack.c: the Super FX's unpacker, a program the S-CPU starts.
 *
 * Packed data is read backward from its end in ROM: the unpacked size (a
 * word, high byte below), two bytes skipped, then a 32-bit bit buffer, and
 * from there four bytes at a time as the buffer runs out (a marker bit
 * above them shows when it has). The output is written from its end down:
 * runs of literal bytes, each run followed by a copy of earlier output.
 */
#include "game.h"

#define fx_unpack_dest      FXU(0x002C)     /* the output's start */
#define fx_unpack_end       FXU(0x0060)     /* and its end */
#define fx_unpack_src       FXU(0x0068)     /* the packed data's end, then where reading stopped */
#define fx_unpack_bank      pakram[0x006A]
#define fx_unpack_add       FXU(0x00A2)     /* added to the output's words when not 0 */

typedef struct {
    uint8_t bank;
    uint16_t at;        /* the last byte read (R14) */
    uint32_t buf;       /* the bits left, the marker above them */
} Bits;

static uint8_t below(Bits *b)
{
    b->at--;
    return rom_at((uint32_t)(b->bank & 0x7F) << 16 | b->at)[0];
}

/* a word from the two bytes below, its low byte the higher one */
static uint16_t word_below(Bits *b)
{
    uint8_t lo = below(b);

    return (uint16_t)(lo | below(b) << 8);
}

static unsigned bit(Bits *b)
{
    unsigned v = b->buf & 1;

    b->buf >>= 1;
    if (!b->buf) {              /* that was the marker: four more bytes */
        uint16_t w3 = word_below(b), w2 = word_below(b);
        uint32_t w = (uint32_t)w2 << 16 | w3;

        v = w & 1;
        b->buf = 0x80000000u | w >> 1;
    }
    return v;
}

/* n bits, the first read the highest, into 16 bits (0 reads 65536) */
static uint16_t bits(Bits *b, uint16_t n)
{
    uint16_t v = 0;

    do
        v = (uint16_t)(v << 1 | bit(b));
    while (--n);
    return v;
}

/* the length of a copy and the width of its offset */
static uint16_t copy_code(Bits *b, uint16_t *width)
{
    uint16_t c = bits(b, 2), len;

    *width = 8;
    if (c == 0)
        return 2;
    if (c == 1) {
        if (!bit(b))
            *width = 14;
        return 3;
    }
    len = 4;
    if (c == 3) {
        c = bits(b, 2);
        len = c == 3 ? bits(b, 8) : c == 2 ? (uint16_t)(bits(b, 2) + 7) : (uint16_t)(c + 5);
    }
    *width = !bit(b) ? 16 : bit(b) ? 8 : 12;
    return len;
}

/* The data ending at fx_unpack_src (bank fx_unpack_bank) unpacked to
   fx_unpack_dest up; then, when fx_unpack_add is not 0, it is added to
   the words from there, one for each byte of the size. */
void fx_unpack_program(void)
{
    Bits b;
    uint16_t out, start = fx_unpack_dest, n, add;

    b.bank = fx_unpack_bank;
    b.at = fx_unpack_src;
    out = word_below(&b);
    b.at = (uint16_t)(b.at - 2);
    b.buf = word_below(&b);
    b.buf |= (uint32_t)word_below(&b) << 16;
    out = (uint16_t)(out + start);
    fx_unpack_end = out;
    for (;;) {
        uint16_t len, width, off;

        n = bits(&b, 3);
        if (n) {
            if (n == 7) {
                if (bit(&b)) {
                    n = bits(&b, 10);
                    if (!n)
                        n = bits(&b, 18);
                } else {
                    n = (uint16_t)(bits(&b, 4) + 7);
                }
            }
            do
                pakram[--out] = (uint8_t)bits(&b, 8);
            while (--n);
        }
        if (out == start)
            break;
        len = copy_code(&b, &width);
        off = bits(&b, width);
        n = (uint16_t)(len - 1);
        do {
            uint8_t v = pakram[(uint16_t)(out + off - 1)];

            pakram[--out] = v;
        } while ((int16_t)--n >= 0);
    }
    fx_unpack_src = b.at;
    add = fx_unpack_add;
    if (add) {
        uint16_t at = fx_unpack_dest;     /* read again: the output may have reached it */

        n = (uint16_t)(fx_unpack_end - at);
        do {
            /* a Super FX word: its high byte at the address XOR 1 */
            uint16_t w = (uint16_t)((pakram[at] | pakram[at ^ 1] << 8) + add);

            pakram[at] = (uint8_t)w;
            pakram[at ^ 1] = (uint8_t)(w >> 8);
            at = (uint16_t)(at + 2);
        } while (--n);
    }
}
