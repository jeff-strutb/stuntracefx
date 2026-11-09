/*
 * unpack.c: the game's packed art and data. A pack is read backward from its
 * end: its size, then a stream of bits; output is written from its end down
 * as runs of literal bytes and copies from higher up.
 */
#include <stdint.h>

#include "data/cart.h"
#include "data/unpack.h"

typedef struct {
    uint32_t at;                /* the next byte to read, moving down */
    uint32_t buf;
    uint8_t bank;
} Bits;

static uint8_t back_byte(Bits *s)
{
    s->at--;
    return cart[s->at & (CART_SIZE - 1)];
}

static uint16_t back_word(Bits *s)
{
    uint8_t lo = back_byte(s), hi = back_byte(s);

    return (uint16_t)(lo | hi << 8);
}

static unsigned take_bit(Bits *s)
{
    unsigned b = s->buf & 1;

    s->buf >>= 1;
    if (!s->buf) {              /* that was the marker: refill */
        uint32_t w3 = back_word(s), w2 = back_word(s);
        uint32_t next = w2 << 16 | w3;

        b = next & 1;
        s->buf = 0x80000000u | next >> 1;
    }
    return b;
}

static uint32_t take(Bits *s, unsigned n)
{
    uint32_t v = 0;
    unsigned i;

    for (i = 0; i < n; i++)
        v = v << 1 | take_bit(s);
    return v;
}

/*
 * @routine 01:CF42 FxUnpackProgram
 * WHAT IT DOES: Unpacks the data whose pack ends just below cart position
 * `end` into `out`, and returns its size.
 */
uint32_t unpack(uint32_t end, uint8_t *out, uint32_t room)
{
    Bits s;
    uint32_t size, left;

    s.at = cart_offset((uint8_t)(end >> 16), (uint16_t)end);
    size = back_word(&s);
    left = size;
    (void)back_word(&s);
    {
        uint32_t w3 = back_word(&s), w2 = back_word(&s);

        s.buf = w2 << 16 | w3;
    }
    if (size > room)
        return 0;
    for (;;) {
        uint32_t n = take(&s, 3), i;

        if (n == 7) {
            if (take_bit(&s)) {
                n = take(&s, 10);
                if (!n)
                    n = take(&s, 18);
            } else {
                n = take(&s, 4) + 7;
            }
        }
        for (i = 0; i < n; i++)
            out[--left] = (uint8_t)take(&s, 8);
        if (!left)
            break;
        {
            uint32_t c = take(&s, 2), length, width, offset;

            if (c == 0) {
                length = 2;
                width = 8;
            } else if (c == 1) {
                length = 3;
                width = take_bit(&s) ? 8 : 14;
            } else {
                if (c == 2) {
                    length = 4;
                } else {
                    uint32_t c2 = take(&s, 2);

                    length = c2 == 3 ? take(&s, 8) : c2 == 2 ? take(&s, 2) + 7 : c2 + 5;
                }
                width = !take_bit(&s) ? 16 : take_bit(&s) ? 8 : 12;
            }
            offset = take(&s, width);
            for (i = 0; i < length; i++) {
                left--;
                out[left] = out[left + offset];
            }
        }
    }
    return size;
}
