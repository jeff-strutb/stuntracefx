/*
 * bcd.c: decimal arithmetic as the S-CPU's decimal mode does it: the race
 * times and counters are BCD bytes, and a digit that is not decimal is
 * carried as the CPU carries it, not corrected.
 */
#include "fixmath.h"

uint8_t bcd_add8(uint8_t a, uint8_t m, bool *carry)
{
    int c = *carry, r = (a & 0x0F) + (m & 0x0F) + c;

    if (r > 0x09)
        r += 0x06;
    c = r > 0x0F;
    r = (a & 0xF0) + (m & 0xF0) + (c << 4) + (r & 0x0F);
    if (r > 0x9F)
        r += 0x60;
    *carry = r > 0xFF;
    return (uint8_t)r;
}

/* a - m - (no carry), carry out set when there was no borrow. */
uint8_t bcd_sub8(uint8_t a, uint8_t m, bool *carry)
{
    int c = *carry, r;

    m = (uint8_t)~m;
    r = (a & 0x0F) + (m & 0x0F) + c;
    if (r <= 0x0F)
        r -= 0x06;
    c = r > 0x0F;
    r = (a & 0xF0) + (m & 0xF0) + (c << 4) + (r & 0x0F);
    if (r <= 0xFF)
        r -= 0x60;
    *carry = r > 0xFF;
    return (uint8_t)r;
}

uint16_t bcd_add16(uint16_t a, uint16_t m, bool *carry)
{
    uint8_t lo = bcd_add8((uint8_t)a, (uint8_t)m, carry);

    return (uint16_t)(bcd_add8((uint8_t)(a >> 8), (uint8_t)(m >> 8), carry) << 8 | lo);
}

uint16_t bcd_sub16(uint16_t a, uint16_t m, bool *carry)
{
    uint8_t lo = bcd_sub8((uint8_t)a, (uint8_t)m, carry);

    return (uint16_t)(bcd_sub8((uint8_t)(a >> 8), (uint8_t)(m >> 8), carry) << 8 | lo);
}

uint16_t binary_to_bcd(uint16_t v)
{
    return (uint16_t)(v / 1000 * 0x1000 + v % 1000 / 100 * 0x100 + v % 100 / 10 * 0x10 + v % 10);
}

uint16_t bcd_to_binary(uint16_t v)
{
    return (uint16_t)((v >> 12) * 1000 + (v >> 8 & 0xF) * 100 + (v >> 4 & 0xF) * 10 + (v & 0xF));
}
