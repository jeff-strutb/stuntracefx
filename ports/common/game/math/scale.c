/*
 * scale.c: fixed fractions of signed words, a grid key from a position,
 * and the stepped blend of four words.
 *
 * The fractions are sums of halvings, each an arithmetic shift (rounding
 * toward minus infinity), so v/2 + v/4 is not always 3v/4 to the unit; the
 * sums wrap at 16 bits.
 */
#include "fixmath.h"

int16_t scale_1_4(int16_t v)
{
    return asr16(v, 2);
}

int16_t scale_3_4(int16_t v)
{
    return wrap16(asr16(v, 1) + asr16(v, 2));
}

int16_t scale_9_16(int16_t v)
{
    return wrap16(asr16(v, 1) + asr16(v, 4));
}

int16_t scale_5_8(int16_t v)
{
    return wrap16(asr16(v, 1) + asr16(v, 3));
}

int16_t scale_7_8(int16_t v)
{
    return wrap16(asr16(v, 3) + asr16(v, 1) + asr16(v, 2));
}

int16_t scale_15_16(int16_t v)
{
    return wrap16(asr16(v, 4) + asr16(v, 3) + asr16(v, 1) + asr16(v, 2));
}

int16_t scale_5_32(int16_t v)
{
    return wrap16(asr16(v, 5) + asr16(v, 3));
}

uint16_t xz_grid_key(int16_t *x, int16_t *z)
{
    uint16_t kx = (uint16_t)((asr16(*x, 10) + 0x20) & 0xFFFE);
    uint16_t kz = (uint16_t)(asr16((int16_t)(*z & 0xF800), 5) + 0x400);

    *x = (int16_t)kx;
    *z = (int16_t)kz;
    return (uint16_t)(kx + kz);
}

bool blend_step(uint16_t *factor, uint16_t limit, bool up, const uint16_t base[4], const uint16_t target[4],
                uint16_t out[4])
{
    int i;

    if (*factor == (up ? limit : 0))
        return false;
    *factor = (uint16_t)(up ? *factor + 1 : *factor - 1);
    for (i = 0; i < 4; i++) {
        uint16_t d = (uint16_t)(target[i] - base[i]);

        out[i] = (uint16_t)(base[i] + mode7_scale_product(d, target[i] >= base[i], (uint8_t)limit,
                                                          (int8_t)(uint8_t)*factor));
    }
    return true;
}
