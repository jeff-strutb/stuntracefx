/*
 * approach.c: moving a value a fraction of the way toward a target.
 *
 * The step is the difference raised to a minimum size either way, then
 * halved some number of times; each halving is an arithmetic shift that
 * rounds a negative step toward zero, so a step never overshoots. The
 * original has one routine per fraction, for words and for bytes.
 */
#include "fixmath.h"

static unsigned approach(unsigned value, unsigned target, unsigned min_step, int halvings, unsigned mask,
                         bool *moved)
{
    unsigned sign = (mask >> 1) + 1, d;
    int i;

    value &= mask;
    target &= mask;
    *moved = value != target;
    if (!*moved)
        return value;
    d = (target - value) & mask;
    if (!(d & sign)) {
        if (d < min_step)
            d = min_step;                       /* forward, at least min_step */
    } else if (!((d + min_step) & sign)) {
        d = -min_step & mask;                   /* back, at least min_step */
    }
    for (i = 0; i < halvings; i++) {
        unsigned h = d >> 1 | (d & sign);

        if (h & sign)
            h += d & 1;                         /* toward zero */
        d = h & mask;
    }
    return (value + d) & mask;
}

int16_t approach16(int16_t value, int16_t target, uint16_t min_step, int halvings, bool *moved)
{
    return (int16_t)(uint16_t)approach((uint16_t)value, (uint16_t)target, min_step, halvings, 0xFFFF, moved);
}

uint8_t approach8(uint8_t value, uint8_t target, uint8_t min_step, int halvings, bool *moved)
{
    return (uint8_t)approach(value, target, min_step, halvings, 0xFF, moved);
}
