/*
 * rotate.c: rotating a 2D vector by an 8-bit angle, in bytes (mul8) or in
 * words (mul_word_byte), with the S-CPU's sine tables.
 */
#include "fixmath.h"

void rotate8(int8_t x, int8_t v, uint8_t angle, int16_t *x_out, int16_t *v_out)
{
    int8_t c = cos8(angle), s = sin8(angle);

    *x_out = (int8_t)(uint8_t)(mul8(x, c) + mul8(v, s));
    *v_out = (int8_t)(uint8_t)(mul8(v, c) - mul8(x, s));
}

void rotate16(int16_t x, int16_t z, uint8_t angle, int16_t *x_out, int16_t *z_out)
{
    int8_t c = cos8(angle), s = sin8(angle);

    *z_out = wrap16(mul_word_byte(x, s) + mul_word_byte(z, c));
    *x_out = wrap16(mul_word_byte(x, c) - mul_word_byte(z, s));
}
