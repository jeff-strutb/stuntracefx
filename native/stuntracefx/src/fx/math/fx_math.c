/*
 * fx_math.c: the Super FX logic's 32-bit divide, square root and vector scaling (fx_math.h), as the original computes
 * them, so the physics gets the same numbers. The original: asm/stuntracefx/math/fx_math.asm, its C
 * src/stuntracefx/math/fx_math.c.
 */
#include "fx/math/fx_math.h"

/*
 * @routine 04:C087 FxNormalize
 * WHAT IT DOES: Scales a vector to length $7FFF (1.0 in 1.15 fixed point). The zero vector is left alone (0 returned).
 * HOW: the squared length as 32 bits, its square root, the shift k = the root's top bit number; the scale is $7FFF << k
 * divided by the root; each component times the scale (as signed 16 bits), the 32-bit product shifted right by k.
 */
int FxNormalize(FxVec3 v, FxNormalized *out)
{
    uint32_t length_squared = (uint32_t)((int32_t)v.x * v.x) + (uint32_t)((int32_t)v.y * v.y) + (uint32_t)((int32_t)v.z * v.z);
    uint16_t root, shift = 0, r;
    int16_t scale;
    int32_t x, y, z;

    if (length_squared == 0)
        return 0;
    root = FxSquareRoot(length_squared);
    for (r = (uint16_t)(root >> 1); r; r >>= 1)
        shift++;
    scale = (int16_t)FxDivideHalvedIfLarge((uint32_t)0x7FFF << shift, root);
    x = (int32_t)v.x * scale;
    y = (int32_t)v.y * scale;
    z = (int32_t)v.z * scale;
    out->unit.x = (int16_t)(x >> shift);
    out->unit.y = (int16_t)(y >> shift);
    out->unit.z = (int16_t)(z >> shift);
    out->shift = shift;
    out->x_high = (uint16_t)((x >> shift) >> 16);
    out->z_low = (uint16_t)z;
    return 1;
}

/*
 * @routine 04:C10F FxSquareRoot
 * WHAT IT DOES: The integer square root of a 32-bit value.
 * HOW: Digit by digit: each of sixteen rounds takes the value's next two bits into a 16-bit remainder and doubles the
 * root; when the remainder exceeds twice the root, twice the root plus one comes off it and the root's new bit is set.
 */
uint16_t FxSquareRoot(uint32_t value)
{
    uint16_t root = 0, remainder = 0;
    int round;

    for (round = 0; round < 16; round++) {
        remainder = (uint16_t)(remainder << 2 | value >> 30);
        value <<= 2;
        root = (uint16_t)(root << 1);
        if ((uint16_t)(root << 1) < remainder) {
            remainder = (uint16_t)(remainder - (uint16_t)(root << 1) - 1);
            root++;
        }
    }
    return root;
}

/*
 * @routine 04:C135 FxDivideHalvedIfLarge
 * WHAT IT DOES: FxDivide, except that when the dividend's top bit or the divisor's is set, both are halved first, so
 * the quotient stays in range. (The halving is decoded from the ROM and not reached in the recordings so far.)
 */
uint16_t FxDivideHalvedIfLarge(uint32_t dividend, uint16_t divisor)
{
    if ((dividend & 0x80000000u) || (divisor & 0x8000))
        return FxDivide(dividend >> 1, (uint16_t)(divisor >> 1));
    return FxDivide(dividend, divisor);
}

/*
 * @routine 04:C144 FxDivide
 * WHAT IT DOES: The 32-bit dividend divided by the divisor, the quotient fitting in 16 bits.
 * HOW: Restoring division, one quotient bit a round, the remainder kept in 16 bits: it is shifted one bit ahead of the
 * quotient, so a remainder's bit 15 is lost when it goes above $7FFF, as on the chip.
 */
uint16_t FxDivide(uint32_t dividend, uint16_t divisor)
{
    uint16_t quotient = (uint16_t)(dividend << 1), remainder = (uint16_t)(dividend >> 15);
    int round;

    for (round = 0; round < 16; round++) {
        int fits = remainder >= divisor;
        uint16_t top = (uint16_t)(quotient >> 15);

        if (fits)
            remainder = (uint16_t)(remainder - divisor);
        quotient = (uint16_t)(quotient << 1 | fits);
        remainder = (uint16_t)(remainder << 1 | top);
    }
    return quotient;
}
