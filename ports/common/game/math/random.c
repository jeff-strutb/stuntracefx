/*
 * random.c: the game's two pseudo-random generators.
 */
#include "fixmath.h"

uint8_t random_step(uint8_t s[4])
{
    unsigned a = s[0], borrow = 1, r;
    int i;

    for (i = 1; i < 4; i++) {
        r = a - s[i] - borrow;
        borrow = r > 0xFF;
        a = s[i] = (uint8_t)r;
    }
    s[0] = (uint8_t)(a - s[0] - borrow);
    return s[0];
}

uint16_t random_word_step(uint16_t *state)
{
    uint16_t x = *state, v;

    x = (uint16_t)(((x & 0xFF) << 8) ^ x);
    x = (uint16_t)(x >> 8 | x << 8);
    v = (uint16_t)((x >> 8) << 1 ^ x);
    if (v & 1) {
        v = (uint16_t)(v >> 1 ^ 0xFF80 ^ 0x8180);
        *state = v;
    } else {
        v = (uint16_t)(v >> 1 ^ 0xFF80);
        *state = v == 0xAA55 ? 0 : v;
    }
    return v;
}
