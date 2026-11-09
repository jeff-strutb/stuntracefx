/*
 * fx_math.c: the Super FX code of asm/stuntracefx/math/fx_math.asm, written from it by
 * the generator.
 *
 * fx_math.asm: the Super FX's 32-bit divide and square root.
 *
 * Both run sixteen rounds under LOOP (R12 the count, R13 the round's start)
 * and keep a 32-bit value in two registers, high word first.
 * C: src/stuntracefx/math/fx_math.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:C087 FxNormalize
 * WHAT IT DOES: Scales the vector (R1, R2, R3) to length $7FFF (1.0 in 1.15
 * fixed point); a zero vector stays zero.
 * HOW: The squared length as 32 bits (three LMULTs summed with ADD and
 * ADC), FxSquareRoot of it, then k = the root's top bit number (LSR until
 * zero); FxDivideHalvedIfLarge divides $7FFF << k by the root, each
 * component is LMULTed by the quotient and the 32-bit products shifted
 * right by k (ASR, ROR under LOOP). The return address is kept on the R10
 * stack around the two calls.
 * Entry: R1, R2, R3 = the vector; R10 = stack; R11 = return address.
 * Exit: R1, R2, R3 = the scaled vector (low words of the products); R7, R8,
 * R0 = their high words; R6 = the quotient; R9 = k; R4 = R3; R5 = 0; R11 =
 * $C0E3; R12 = 0; R13 = $C0FD ($C14B when k = 0); the last RAM address R10;
 * the flags from the last LOOP and ROR. A zero vector: R0-R6 = 0; R11
 * unchanged; Z set, S, OV clear.
 */
FxNormalizeResult FxNormalize(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t model_record, uint16_t fallback_shift, uint16_t *stack)
{
    uint16_t k_2;
    uint16_t root;
    uint32_t x_sq;
    uint32_t y_sq;
    uint32_t z_sq;
    uint16_t count;
    uint16_t count_2;
    uint16_t x_sq_lo;
    uint16_t y_sq_lo;
    uint16_t z_sq_lo;
    uint16_t scale_hi;
    uint16_t scale_lo;
    uint16_t place_y_2;
    uint16_t sum_sq_hi;
    uint16_t quotient_2;
    uint16_t quotient_3;
    uint16_t scaled_x_hi;
    uint16_t scaled_y_hi;
    uint16_t scaled_z_hi;
    uint16_t low_word_out;
    uint16_t root_shifted;
    uint32_t x_scale_prod;
    uint16_t xy_sq_lo_sum;
    uint32_t y_scale_prod;
    uint32_t z_scale_prod;
    int x_shift_carry;
    int y_shift_carry;
    int z_shift_carry;
    uint16_t root_remaining;
    int scale_lo_carry;
    uint16_t sum_sq_lo_copy;
    uint16_t shift_count_copy;
    uint16_t scaled_vector_x_2;
    uint16_t scaled_vector_y_2;
    uint16_t scaled_vector_z_2;
    uint16_t shift_count_copy_2;

    fx_spill(stack);
    x_sq = (uint32_t)((int32_t)(int16_t)vector_x * (int16_t)vector_x);
    x_sq_lo = (uint16_t)x_sq;
    y_sq = (uint32_t)((int32_t)(int16_t)vector_y * (int16_t)vector_y);
    y_sq_lo = (uint16_t)y_sq;
    xy_sq_lo_sum = (uint16_t)(x_sq_lo + y_sq_lo);
    z_sq = (uint32_t)((int32_t)(int16_t)vector_z * (int16_t)vector_z);
    z_sq_lo = (uint16_t)z_sq;
    place_y_2 = (uint16_t)(xy_sq_lo_sum + z_sq_lo);
    sum_sq_hi = (uint16_t)((uint16_t)((uint16_t)(x_sq >> 16) + (uint16_t)(y_sq >> 16) + ((uint32_t)x_sq_lo + y_sq_lo > 0xFFFF)) + (uint16_t)(z_sq >> 16) + ((uint32_t)xy_sq_lo_sum + z_sq_lo > 0xFFFF));
    if (sum_sq_hi != 0) {
        { FxSquareRootResult r_ = FxSquareRoot(sum_sq_hi, place_y_2); low_word_out = r_.low_word_out; root = r_.root; };
        root_remaining = root;
        k_2 = 0;
        for (;;) {
            root_shifted = (uint16_t)(root_remaining >> 1);
            root_remaining = root_shifted;
            if (root_shifted == 0) {
                break;
            } else {
                k_2 = (uint16_t)(k_2 + 1);
                continue;
            }
        }
        scale_hi = 0;
        scale_lo = 0x7FFF;
        shift_count_copy = k_2;
        count = shift_count_copy;
        if (shift_count_copy == 0) {
        } else {
            for (;;) {
                scale_lo_carry = ((uint32_t)scale_lo + scale_lo > 0xFFFF);
                scale_lo = (uint16_t)(scale_lo + scale_lo);
                count = (uint16_t)(count - 1);
                scale_hi = (uint16_t)(scale_hi << 1 | scale_lo_carry);
                if (count != 0) {
                    continue;
                } else {
                    break;
                }
            }
        }
        quotient_2 = FxDivideHalvedIfLarge(scale_hi, scale_lo, root);
        x_scale_prod = (uint32_t)((int32_t)(int16_t)vector_x * (int16_t)quotient_2);
        scaled_x_hi = (uint16_t)(x_scale_prod >> 16);
        scaled_vector_x_2 = (uint16_t)x_scale_prod;
        y_scale_prod = (uint32_t)((int32_t)(int16_t)vector_y * (int16_t)quotient_2);
        scaled_y_hi = (uint16_t)(y_scale_prod >> 16);
        scaled_vector_y_2 = (uint16_t)y_scale_prod;
        z_scale_prod = (uint32_t)((int32_t)(int16_t)vector_z * (int16_t)quotient_2);
        quotient_3 = (uint16_t)z_scale_prod;
        scaled_z_hi = (uint16_t)(z_scale_prod >> 16);
        scaled_vector_z_2 = quotient_3;
        shift_count_copy_2 = k_2;
        count_2 = shift_count_copy_2;
        if (shift_count_copy_2 == 0) {
        } else {
            for (;;) {
                x_shift_carry = (scaled_x_hi & 1);
                scaled_x_hi = (uint16_t)((int16_t)scaled_x_hi >> 1);
                scaled_vector_x_2 = (uint16_t)(scaled_vector_x_2 >> 1 | x_shift_carry << 15);
                y_shift_carry = (scaled_y_hi & 1);
                scaled_y_hi = (uint16_t)((int16_t)scaled_y_hi >> 1);
                scaled_vector_y_2 = (uint16_t)(scaled_vector_y_2 >> 1 | y_shift_carry << 15);
                z_shift_carry = (scaled_z_hi & 1);
                scaled_z_hi = (uint16_t)((int16_t)scaled_z_hi >> 1);
                scaled_vector_z_2 = (uint16_t)(scaled_vector_z_2 >> 1 | z_shift_carry << 15);
                count_2 = (uint16_t)(count_2 - 1);
                if (count_2 != 0) {
                    continue;
                } else {
                    break;
                }
            }
        }
        fx_unspill(stack);
        return (FxNormalizeResult){scaled_vector_x_2, scaled_vector_y_2, scaled_vector_z_2, quotient_3, low_word_out, scaled_x_hi, k_2};
    } else {
        sum_sq_lo_copy = place_y_2;
        place_y_2 = sum_sq_lo_copy;
        if (sum_sq_lo_copy != 0) {
            { FxSquareRootResult r_ = FxSquareRoot(sum_sq_hi, place_y_2); low_word_out = r_.low_word_out; root = r_.root; };
            root_remaining = root;
            k_2 = 0;
            for (;;) {
                root_shifted = (uint16_t)(root_remaining >> 1);
                root_remaining = root_shifted;
                if (root_shifted == 0) {
                    break;
                } else {
                    k_2 = (uint16_t)(k_2 + 1);
                    continue;
                }
            }
            scale_hi = 0;
            scale_lo = 0x7FFF;
            shift_count_copy = k_2;
            count = shift_count_copy;
            if (shift_count_copy == 0) {
            } else {
                for (;;) {
                    scale_lo_carry = ((uint32_t)scale_lo + scale_lo > 0xFFFF);
                    scale_lo = (uint16_t)(scale_lo + scale_lo);
                    count = (uint16_t)(count - 1);
                    scale_hi = (uint16_t)(scale_hi << 1 | scale_lo_carry);
                    if (count != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
            }
            quotient_2 = FxDivideHalvedIfLarge(scale_hi, scale_lo, root);
            x_scale_prod = (uint32_t)((int32_t)(int16_t)vector_x * (int16_t)quotient_2);
            scaled_x_hi = (uint16_t)(x_scale_prod >> 16);
            scaled_vector_x_2 = (uint16_t)x_scale_prod;
            y_scale_prod = (uint32_t)((int32_t)(int16_t)vector_y * (int16_t)quotient_2);
            scaled_y_hi = (uint16_t)(y_scale_prod >> 16);
            scaled_vector_y_2 = (uint16_t)y_scale_prod;
            z_scale_prod = (uint32_t)((int32_t)(int16_t)vector_z * (int16_t)quotient_2);
            quotient_3 = (uint16_t)z_scale_prod;
            scaled_z_hi = (uint16_t)(z_scale_prod >> 16);
            scaled_vector_z_2 = quotient_3;
            shift_count_copy_2 = k_2;
            count_2 = shift_count_copy_2;
            if (shift_count_copy_2 == 0) {
            } else {
                for (;;) {
                    x_shift_carry = (scaled_x_hi & 1);
                    scaled_x_hi = (uint16_t)((int16_t)scaled_x_hi >> 1);
                    scaled_vector_x_2 = (uint16_t)(scaled_vector_x_2 >> 1 | x_shift_carry << 15);
                    y_shift_carry = (scaled_y_hi & 1);
                    scaled_y_hi = (uint16_t)((int16_t)scaled_y_hi >> 1);
                    scaled_vector_y_2 = (uint16_t)(scaled_vector_y_2 >> 1 | y_shift_carry << 15);
                    z_shift_carry = (scaled_z_hi & 1);
                    scaled_z_hi = (uint16_t)((int16_t)scaled_z_hi >> 1);
                    scaled_vector_z_2 = (uint16_t)(scaled_vector_z_2 >> 1 | z_shift_carry << 15);
                    count_2 = (uint16_t)(count_2 - 1);
                    if (count_2 != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
            }
            fx_unspill(stack);
            return (FxNormalizeResult){scaled_vector_x_2, scaled_vector_y_2, scaled_vector_z_2, quotient_3, low_word_out, scaled_x_hi, k_2};
        } else {
            fx_unspill(stack);
            return (FxNormalizeResult){0, 0, 0, z_sq_lo, place_y_2, model_record, fallback_shift};
        }
    }
}


/*
 * @routine 04:C10F FxSquareRoot
 * WHAT IT DOES: The integer square root of the 32-bit R4:R5 to R6, the
 * remainder (value - root squared) to R8.
 * HOW: Digit by digit: each round shifts two bits of R4:R5 into R8 (ADD R5
 * to itself, ROL R4, ROL R8, twice), doubles the root, and when R8 exceeds
 * twice the root, subtracts twice the root plus one (SBC with the borrow
 * the compare left) and sets the root's new low bit. LOOP's delay slot
 * holds the WITH R5 for the next round's first ADD.
 * Entry: R4 = high word, R5 = low word; R11 = return address. Exit: R6 =
 * root; R8 = remainder; R4 = R5 = 0; R7 = the last round's twice the root;
 * R0 = R7 - R8 from the last round's compare; R12 = 0; R13 = $C119; Z set,
 * S clear; CY, OV from the last compare or subtract.
 */
FxSquareRootResult FxSquareRoot(uint16_t high_word, uint16_t low_word)
{
    uint16_t count;
    uint16_t r_t_8;
    uint16_t root_2;
    uint16_t remainder;
    int trial_fits;
    int low_top_bit;
    uint16_t trial_divisor;
    uint16_t high_shifted_in;
    uint16_t low_word_shifted;
    uint16_t low_word_shifted_twice;

    root_2 = 0;
    remainder = 0;
    count = 0x0010;
    low_top_bit = ((uint32_t)low_word + low_word > 0xFFFF);
    low_word_shifted = (uint16_t)(low_word + low_word);
    for (;;) {
        high_shifted_in = (uint16_t)(high_word << 1 | low_top_bit);
        r_t_8 = (uint16_t)(remainder << 1 | (high_word >> 15));
        low_word_shifted_twice = (uint16_t)(low_word_shifted + low_word_shifted);
        high_word = (uint16_t)(high_shifted_in << 1 | ((uint32_t)low_word_shifted + low_word_shifted > 0xFFFF));
        remainder = (uint16_t)(r_t_8 << 1 | (high_shifted_in >> 15));
        root_2 = (uint16_t)(root_2 + root_2);
        trial_divisor = (uint16_t)(root_2 + root_2);
        trial_fits = (trial_divisor >= remainder);
        if (trial_fits) {
            count = (uint16_t)(count - 1);
            if (count != 0) {
                low_top_bit = ((uint32_t)low_word_shifted_twice + low_word_shifted_twice > 0xFFFF);
                low_word_shifted = (uint16_t)(low_word_shifted_twice + low_word_shifted_twice);
                continue;
            } else {
                break;
            }
        } else {
            remainder = (uint16_t)(remainder - trial_divisor - !trial_fits);
            root_2 = (uint16_t)(root_2 + 1);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                low_top_bit = ((uint32_t)low_word_shifted_twice + low_word_shifted_twice > 0xFFFF);
                low_word_shifted = (uint16_t)(low_word_shifted_twice + low_word_shifted_twice);
                continue;
            } else {
                break;
            }
        }
    }
    return (FxSquareRootResult){low_word_shifted_twice, root_2};
}


/*
 * @routine 04:C135 FxDivideHalvedIfLarge
 * WHAT IT DOES: FxDivide, except that when R0 or R6 has bit 15 set the
 * divisor and the 32-bit dividend are first halved, so the quotient is
 * kept in range.
 * HOW: MOVES R0 and R6 for their signs; the halving is LSR R6, LSR R0 and
 * ROR R4, then the code runs on into FxDivide. The halving path is decoded
 * from the ROM bytes and not reached in the traces so far.
 * Entry: as FxDivide. Exit: as FxDivide; R6 = the divisor used.
 */
uint16_t FxDivideHalvedIfLarge(uint16_t dividend_high, uint16_t dividend_low, uint16_t divisor)
{
    uint16_t divisor_test;
    uint16_t halved_dividend_low;

    if (dividend_high & 0x8000) {
    } else {
        divisor_test = divisor;
        divisor = divisor_test;
        if (!(divisor_test & 0x8000)) {
            return FxDivide(dividend_high, dividend_low, divisor);
        } else {
        }
    }
    halved_dividend_low = (uint16_t)(dividend_low >> 1 | (dividend_high & 1) << 15);
    return FxDivide((uint16_t)(dividend_high >> 1), halved_dividend_low, (uint16_t)(divisor >> 1));
}


/*
 * @routine 04:C144 FxDivide
 * WHAT IT DOES: Divides the 32-bit R0:R4 by R6: the quotient to R4, the
 * remainder to R0. The quotient must fit in 16 bits (R0 < R6).
 * HOW: Restoring division, one quotient bit a round: the pair is shifted
 * left (ADD R4 to itself, ROL R0), R6 is subtracted from R0 and, on a
 * borrow, added back. The shift into R0 runs one bit ahead (the ROL in
 * LOOP's delay slot) and the closing LSR takes it back, so bit 15 of a
 * remainder above $7FFF is lost.
 * Entry: R0 = high word, R4 = low word, R6 = divisor; R11 = return address.
 * Exit: R4 = quotient; R0 = remainder; R12 = 0; R13 = $C14B; S clear, Z
 * from R0, CY = the bit shifted out; OV from the last round's SUB or ADD.
 */
uint16_t FxDivide(uint16_t high_word, uint16_t low_word, uint16_t divisor)
{
    uint16_t count;
    uint16_t quotient_2;
    int divisor_fits;
    uint16_t remainder_start;
    int quotient_top_bit;
    int quotient_carry_out;
    uint16_t remainder_less_divisor;

    count = 0x0010;
    quotient_2 = (uint16_t)(low_word + low_word);
    remainder_start = (uint16_t)(high_word << 1 | ((uint32_t)low_word + low_word > 0xFFFF));
    for (;;) {
        remainder_less_divisor = (uint16_t)(remainder_start - divisor);
        divisor_fits = (remainder_start >= divisor);
        if (!divisor_fits) {
            quotient_carry_out = ((uint32_t)quotient_2 + quotient_2 > 0xFFFF);
            quotient_2 = (uint16_t)(quotient_2 + quotient_2);
            count = (uint16_t)(count - 1);
            remainder_start = (uint16_t)((uint16_t)(remainder_less_divisor + divisor) << 1 | quotient_carry_out);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            quotient_top_bit = (quotient_2 >> 15);
            quotient_2 = (uint16_t)(quotient_2 << 1 | divisor_fits);
            count = (uint16_t)(count - 1);
            remainder_start = (uint16_t)(remainder_less_divisor << 1 | quotient_top_bit);
            if (count != 0) {
                continue;
            } else {
                return quotient_2;
            }
        }
    }
    return quotient_2;
}

