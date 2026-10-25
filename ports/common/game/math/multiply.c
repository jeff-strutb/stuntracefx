/*
 * multiply.c: the products the game makes with the console's multipliers,
 * as the hardware it used computes them.
 *
 * The S-CPU's multiplier is unsigned 8 x 8; the game multiplies magnitudes
 * on it and fixes the sign after, and doubles the first factor in a byte to
 * keep one more bit, so a first factor of -128 multiplies as 0. The PPU's
 * (the Mode 7 matrix's) is signed 16 x 8 with a 24-bit product. The divider
 * is unsigned 16 / 8 and gives $FFFF for a divisor of 0.
 */
#include "fixmath.h"

static unsigned mag8(int8_t v)
{
    return v < 0 ? (unsigned)-v : (unsigned)v;
}

int8_t mul8(int8_t a, int8_t b)
{
    unsigned h = ((mag8(a) * 2 & 0xFF) * mag8(b)) >> 8;

    return (int8_t)(uint8_t)((a < 0) != (b < 0) ? -h : h);
}

int16_t mul16x8(int16_t a, int8_t b)
{
    return wrap16(asr32((int32_t)a * b, 8));
}

int16_t mul_word_byte(int16_t v, int8_t s)
{
    uint32_t m = v < 0 ? (uint32_t)-(int32_t)v : (uint32_t)v;
    uint32_t p = ((mag8(s) * 2 & 0xFF) * m) >> 8;

    return wrap16((v < 0) != (s < 0) ? -(int32_t)p : (int32_t)p);
}

uint16_t mode7_scale_product(uint16_t value, bool positive, uint8_t divisor, int8_t factor)
{
    uint16_t mag = positive ? value : (uint16_t)-value;
    uint16_t q = divisor ? (uint16_t)(mag / divisor) : 0xFFFF;

    if (!positive)
        q = (uint16_t)-q;
    return (uint16_t)((int16_t)q * factor);
}
