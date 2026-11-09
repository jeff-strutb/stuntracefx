/*
 * arith.c: the Super FX code of asm/stuntracefx/render/arith.asm, written from it by
 * the generator.
 *
 * arith.asm: the Super FX's long division and square root, done a bit at a
 * time in LOOPs (the GSU-1 has no divide).
 *
 * The three dividers share one loop, each with its own copy of it: the
 * dividend is shifted left one bit first (its low word into the high
 * word), then 16 passes of restoring division: subtract the divisor from
 * the high word, and if that borrows add it back and shift a 0 into the
 * low word, else shift a 1 in; the low word's top bit shifts into the high
 * word. The low word ends as the quotient and the high word as the
 * remainder shifted left one, with the bit shifted out of it dropped each
 * pass, so the remainder is exact only while it fits in 15 bits.
 * C: src/stuntracefx/render/arith.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:8096 FxSqrt32
 * WHAT IT DOES: The integer square root of the 32-bit value R4:R5 (R4 the
 * high word): R6 = the root, R8 = the remainder.
 * HOW: 16 passes, each shifting the next two bits of R4:R5 into R8 and
 * doubling R6; when R8 exceeds twice R6 (R7), R8 loses twice R6 plus 1 and
 * R6 gains 1. The pass loop starts at $80A0, the ADD R5 after the first
 * WITH R5: LOOP's delay slot is the WITH R5 for the next pass, and the
 * JMP R11 after the last pass runs with it pending.
 * Entry: R4:R5 = value; R11 = return address. Exit: R6 = root, R8 =
 * remainder; R4 = R5 = 0; R7 = twice the root before its last bit; R0 =
 * R7 - R8 as the last pass compared them; R12 = 0; R13 = $80A0; Z set, S
 * clear; CY and OV from the last pass's SUB R8, or from its SBC R7 when the
 * root gained a bit.
 */
uint16_t FxSqrt32(uint16_t radicand_lo, uint16_t radicand_hi)
{
    uint16_t count;
    uint16_t r_t_8;
    uint16_t root_2;
    uint16_t remainder;
    int pending_here;
    uint16_t trial_divisor;
    uint16_t pending_here_2;
    uint16_t radicand_hi_x2;
    uint16_t radicand_lo_shifted;
    int trial_exceeds_remainder;

    root_2 = 0;
    remainder = 0;
    count = 0x0010;
    pending_here = ((uint32_t)radicand_hi + radicand_hi > 0xFFFF);
    pending_here_2 = (uint16_t)(radicand_hi + radicand_hi);
    for (;;) {
        radicand_lo_shifted = (uint16_t)(radicand_lo << 1 | pending_here);
        r_t_8 = (uint16_t)(remainder << 1 | (radicand_lo >> 15));
        radicand_hi_x2 = (uint16_t)(pending_here_2 + pending_here_2);
        radicand_lo = (uint16_t)(radicand_lo_shifted << 1 | ((uint32_t)pending_here_2 + pending_here_2 > 0xFFFF));
        remainder = (uint16_t)(r_t_8 << 1 | (radicand_lo_shifted >> 15));
        root_2 = (uint16_t)(root_2 + root_2);
        trial_divisor = (uint16_t)(root_2 + root_2);
        trial_exceeds_remainder = (trial_divisor >= remainder);
        if (trial_exceeds_remainder) {
            count = (uint16_t)(count - 1);
            if (count != 0) {
                pending_here = ((uint32_t)radicand_hi_x2 + radicand_hi_x2 > 0xFFFF);
                pending_here_2 = (uint16_t)(radicand_hi_x2 + radicand_hi_x2);
                continue;
            } else {
                break;
            }
        } else {
            remainder = (uint16_t)(remainder - trial_divisor - !trial_exceeds_remainder);
            root_2 = (uint16_t)(root_2 + 1);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                pending_here = ((uint32_t)radicand_hi_x2 + radicand_hi_x2 > 0xFFFF);
                pending_here_2 = (uint16_t)(radicand_hi_x2 + radicand_hi_x2);
                continue;
            } else {
                break;
            }
        }
    }
    return root_2;
}


/*
 * @routine 01:80BC FxLengthProgram
 * WHAT IT DOES: A program the S-CPU starts: the length of the vector
 * ($0068, $002E), stored at $006A.
 * HOW: Sets the R10 stack to $0240; squares each word with LMULT (signed,
 * R4 the low word), adds the two 32-bit squares (ADD, ADC) and takes
 * FxSqrt32 of the sum; SM stores the root. Then STOP.
 * Entry: started by the S-CPU. Exit: (at the STOP) R10 = $0240; R11 =
 * $80DF; the registers and flags as FxSqrt32 leaves them; the last RAM
 * address $006A.
 * Returns the sum of the squares; *root = the length.
 */
uint16_t FxLengthProgram(uint16_t *stack)
{
    uint16_t root;
    uint16_t vec_x;
    uint16_t vec_z;
    uint32_t vec_x_sq;
    uint32_t vec_z_sq;
    uint16_t vec_x_sq_lo;
    uint16_t vec_z_sq_lo;

    vec_x = fx_ldw(0x0068);
    vec_x_sq = (uint32_t)((int32_t)(int16_t)vec_x * (int16_t)vec_x);
    vec_x_sq_lo = (uint16_t)vec_x_sq;
    vec_z = fx_ldw(0x002E);
    vec_z_sq = (uint32_t)((int32_t)(int16_t)vec_z * (int16_t)vec_z);
    vec_z_sq_lo = (uint16_t)vec_z_sq;
    root = FxSqrt32((uint16_t)((uint16_t)(vec_z_sq >> 16) + (uint16_t)(vec_x_sq >> 16) + ((uint32_t)vec_z_sq_lo + vec_x_sq_lo > 0xFFFF)), (uint16_t)(vec_z_sq_lo + vec_x_sq_lo));
    fx_stw(0x006A, root);
    return 0;
}


/*
 * @routine 01:8186 FxDivideSigned
 * WHAT IT DOES: Divides the 32-bit value R5:R4 (R5 the high word) by R6,
 * signed: R4 = the quotient, R0 = the remainder of the magnitudes, shifted
 * left one.
 * HOW: CACHE; R7 = R5 XOR R6 keeps the quotient's sign; R6 and R5:R4 are
 * made positive (SUB from R0 = 0, the 32-bit one as SUB then SBC); the
 * shared loop; then R4 is negated (NOT, INC) when R7 is negative. There is
 * no closing LSR: the remainder is left shifted.
 * Entry: R5:R4 = dividend, R6 = divisor; R11 = return address. Exit: R4 =
 * quotient, R0 = remainder * 2 plus the last bit shifted in; R5 = the
 * dividend's high word made positive, R6 = the divisor made positive, R7 =
 * the sign word; R12 = 0; R13 = $81A3 (the loop); OV, S, Z from MOVES R7
 * (S and Z from the quotient when it was negated); CY from the loop.
 */
FxDivideSignedResult FxDivideSigned(uint16_t dividend_lo, uint16_t dividend_hi, uint16_t divisor)
{
    uint16_t count;
    uint16_t quotient_2;
    uint16_t trial_diff;
    uint16_t abs_divisor;
    uint16_t quotient_sign;
    uint16_t remainder_acc;
    uint16_t abs_dividend_hi;
    uint16_t neg_dividend_lo;
    int trial_no_borrow;
    int dividend_negative;
    int quotient_top_bit_a;
    int quotient_top_bit_b;
    int dividend_lo_top_bit;

    quotient_sign = (dividend_hi ^ divisor);
    abs_divisor = divisor;
    if (!(divisor & 0x8000)) {
        dividend_negative = ((dividend_hi & 0x8000) != 0);
        abs_dividend_hi = dividend_hi;
    } else {
        abs_divisor = (uint16_t)(0 - abs_divisor);
        dividend_negative = ((dividend_hi & 0x8000) != 0);
        abs_dividend_hi = dividend_hi;
    }
    if (!dividend_negative) {
        dividend_lo_top_bit = ((uint32_t)dividend_lo + dividend_lo > 0xFFFF);
        quotient_2 = (uint16_t)(dividend_lo + dividend_lo);
    } else {
        neg_dividend_lo = (uint16_t)(0 - dividend_lo);
        abs_dividend_hi = (uint16_t)(0 - abs_dividend_hi - (0 < dividend_lo));
        dividend_lo_top_bit = ((uint32_t)neg_dividend_lo + neg_dividend_lo > 0xFFFF);
        quotient_2 = (uint16_t)(neg_dividend_lo + neg_dividend_lo);
    }
    remainder_acc = (uint16_t)(abs_dividend_hi << 1 | dividend_lo_top_bit);
    count = 0x0010;
    for (;;) {
        trial_diff = (uint16_t)(remainder_acc - abs_divisor);
        trial_no_borrow = (remainder_acc >= abs_divisor);
        if (!trial_no_borrow) {
            quotient_top_bit_b = ((uint32_t)quotient_2 + quotient_2 > 0xFFFF);
            quotient_2 = (uint16_t)(quotient_2 + quotient_2);
            count = (uint16_t)(count - 1);
            remainder_acc = (uint16_t)((uint16_t)(trial_diff + abs_divisor) << 1 | quotient_top_bit_b);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            quotient_top_bit_a = (quotient_2 >> 15);
            quotient_2 = (uint16_t)(quotient_2 << 1 | trial_no_borrow);
            count = (uint16_t)(count - 1);
            remainder_acc = (uint16_t)(trial_diff << 1 | quotient_top_bit_a);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    if (!(quotient_sign & 0x8000)) {
        return (FxDivideSignedResult){quotient_2, abs_divisor, quotient_sign};
    } else {
        return (FxDivideSignedResult){(uint16_t)((uint16_t)~quotient_2 + 1), abs_divisor, quotient_sign};
    }
}


/*
 * @routine 01:81BC FxDivide32By16Halving
 * WHAT IT DOES: Divides the 32-bit value R0:R4 (R0 the high word) by R6,
 * unsigned, halving both first when either has its top bit set: R4 = the
 * quotient, R0 = the remainder (of the halved values).
 * HOW: MOVES tests R0 and R6; when either is negative, LSR R6 and the
 * 32-bit LSR/ROR of R0:R4 halve them; then on into FxDivide32By16. The
 * halving ($81C6-$81CA) is not reached in the traces so far: every call
 * seen has both values below $8000.
 * Entry: R0:R4 = dividend, R6 = divisor; R11 = return address. Exit: as
 * FxDivide32By16; R6 = the divisor as used.
 */
uint16_t FxDivide32By16Halving(uint16_t dividend_high, uint16_t dividend_low, uint16_t divisor)
{
    uint16_t divisor_copy;
    uint16_t halved_dividend_low;

    if (dividend_high & 0x8000) {
    } else {
        divisor_copy = divisor;
        divisor = divisor_copy;
        if (!(divisor_copy & 0x8000)) {
            return FxDivide32By16(dividend_high, dividend_low, divisor);
        } else {
        }
    }
    halved_dividend_low = (uint16_t)(dividend_low >> 1 | (dividend_high & 1) << 15);
    return FxDivide32By16((uint16_t)(dividend_high >> 1), halved_dividend_low, (uint16_t)(divisor >> 1));
}


/*
 * @routine 01:81CB FxDivide32By16
 * WHAT IT DOES: Divides the 32-bit value R0:R4 (R0 the high word) by R6,
 * unsigned: R4 = the quotient, R0 = the remainder.
 * HOW: The shared loop (no CACHE); LSR in the return's delay slot undoes
 * the remainder's extra shift. 01:81BC branches here for dividends and
 * divisors below $8000.
 * Entry: R0:R4 = dividend, R6 = divisor; R11 = return address. Exit: R4 =
 * quotient, R0 = remainder; R12 = 0; R13 = $81D2 (the loop); S clear, Z
 * and CY from the LSR; OV from the last pass.
 */
uint16_t FxDivide32By16(uint16_t dividend_hi, uint16_t dividend_lo, uint16_t divisor)
{
    uint16_t count;
    uint16_t quotient_2;
    int divisor_fits;
    int quotient_carry;
    int quotient_top_bit;
    uint16_t remainder_reduced;
    uint16_t remainder_shifted;

    count = 0x0010;
    quotient_2 = (uint16_t)(dividend_lo + dividend_lo);
    remainder_shifted = (uint16_t)(dividend_hi << 1 | ((uint32_t)dividend_lo + dividend_lo > 0xFFFF));
    for (;;) {
        remainder_reduced = (uint16_t)(remainder_shifted - divisor);
        divisor_fits = (remainder_shifted >= divisor);
        if (!divisor_fits) {
            quotient_carry = ((uint32_t)quotient_2 + quotient_2 > 0xFFFF);
            quotient_2 = (uint16_t)(quotient_2 + quotient_2);
            count = (uint16_t)(count - 1);
            remainder_shifted = (uint16_t)((uint16_t)(remainder_reduced + divisor) << 1 | quotient_carry);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            quotient_top_bit = (quotient_2 >> 15);
            quotient_2 = (uint16_t)(quotient_2 << 1 | divisor_fits);
            count = (uint16_t)(count - 1);
            remainder_shifted = (uint16_t)(remainder_reduced << 1 | quotient_top_bit);
            if (count != 0) {
                continue;
            } else {
                return quotient_2;
            }
        }
    }
    return quotient_2;
}


/*
 * @routine 01:9658 FxRatioClamp
 * WHAT IT DOES: Turns the pair (R1, R2), each halved, into the ratios
 * R1 * 256 / R3 and R2 * 256 / R3; when the first would pass $3FFF it is
 * held at $3FFF and the second scaled to keep the pair's ratio.
 * HOW: Pushes R11 on the R10 stack; ASR R1, ASR R2. When R1's high byte
 * is below R3, FxDivideHalving divides R1 << 8 by R3; if that is at most
 * $3FFF, R2 << 8 is divided by R3 too and the routine returns. Otherwise
 * (R1's high byte at least R3, or a first quotient over $3FFF) R2 is made
 * R2 * $3FFF / R1 ((R2 << 14) - R2, then FxDivideHalving; R1 and R2 here
 * the halved values or the two quotients) and R1 = $3FFF. Returns by
 * pulling R11's value into R15.
 * Entry: R1, R2 = the pair, R3 = the divisor, R10 = the stack; R11 =
 * return address. Exit: R1, R2 = the results; R10 as at entry; R11 = the
 * return address of the last call; R0, R4, R6, R12, R13, OV and CY as the
 * last FxDivideHalving leaves them; S and Z from R10; the last RAM address
 * R10.
 */
FxRatioClampResult FxRatioClamp(uint16_t pair_x, uint16_t pair_y, uint16_t divisor, uint16_t *stack)
{
    uint16_t half_x;
    uint16_t half_y;
    uint16_t quotient;
    uint16_t quotient_2;
    uint16_t quotient_3;
    uint16_t quotient_4;
    uint16_t half_y_eighth;
    uint16_t half_x_hi_byte;
    uint16_t half_y_quarter;

    fx_spill(stack);
    half_x = (uint16_t)((int16_t)pair_x >> 1);
    half_y = (uint16_t)((int16_t)pair_y >> 1);
    half_x_hi_byte = (uint16_t)(half_x >> 8);
    if (half_x_hi_byte >= divisor) {
        half_y_quarter = (uint16_t)(half_y >> 1);
        half_y_eighth = (uint16_t)((uint16_t)(0 >> 1 | (half_y & 1) << 15) >> 1 | (half_y_quarter & 1) << 15);
        quotient = FxDivideHalving((uint16_t)((uint16_t)(half_y_quarter >> 1) - 0 - !(half_y_eighth >= half_y)), (uint16_t)(half_y_eighth - half_y), half_x);
        fx_unspill(stack);
        return (FxRatioClampResult){0x3FFF, quotient};
    } else {
        quotient_2 = FxDivideHalving(half_x_hi_byte, (uint16_t)((uint16_t)(half_x & 0xFF) << 8 | (uint16_t)(half_x & 0xFF) >> 8), divisor);
        if (0x3FFF < quotient_2) {
            half_x = quotient_2;
            quotient_3 = FxDivideHalving((uint16_t)(half_y >> 8), (uint16_t)((uint16_t)(half_y & 0xFF) << 8 | (uint16_t)(half_y & 0xFF) >> 8), divisor);
            half_y = quotient_3;
            half_y_quarter = (uint16_t)(half_y >> 1);
            half_y_eighth = (uint16_t)((uint16_t)(0 >> 1 | (half_y & 1) << 15) >> 1 | (half_y_quarter & 1) << 15);
            quotient = FxDivideHalving((uint16_t)((uint16_t)(half_y_quarter >> 1) - 0 - !(half_y_eighth >= half_y)), (uint16_t)(half_y_eighth - half_y), half_x);
            fx_unspill(stack);
            return (FxRatioClampResult){0x3FFF, quotient};
        } else {
            quotient_4 = FxDivideHalving((uint16_t)(half_y >> 8), (uint16_t)((uint16_t)(half_y & 0xFF) << 8 | (uint16_t)(half_y & 0xFF) >> 8), divisor);
            fx_unspill(stack);
            return (FxRatioClampResult){quotient_2, quotient_4};
        }
    }
}


/*
 * @routine 01:96C0 FxDivideHalving
 * WHAT IT DOES: Divides the 32-bit value R0:R4 (R0 the high word) by R6,
 * unsigned, halving both first when either has its top bit set: R4 = the
 * quotient, R0 = the remainder (of the halved values).
 * HOW: FxDivide32By16Halving's code again, with its own copy of the loop.
 * Entry: R0:R4 = dividend, R6 = divisor; R11 = return address. Exit: R4 =
 * quotient, R0 = remainder; R6 = the divisor as used; R12 = 0; R13 =
 * $96D6 (the loop); S clear, Z and CY from the LSR; OV from the last pass.
 */
uint16_t FxDivideHalving(uint16_t dividend_hi, uint16_t dividend_lo, uint16_t divisor)
{
    uint16_t count;
    uint16_t remainder;
    uint16_t quotient_2;
    uint16_t divisor_copy;
    int shifted_out_bit;
    uint16_t trial_remainder;
    uint16_t dividend_hi_work;
    int quotient_top_bit_ge;
    int quotient_top_bit_lt;
    int remainder_ge_divisor;

    dividend_hi_work = dividend_hi;
    if (dividend_hi & 0x8000) {
        divisor = (uint16_t)(divisor >> 1);
        shifted_out_bit = (dividend_hi_work & 1);
        dividend_hi_work = (uint16_t)(dividend_hi_work >> 1);
        dividend_lo = (uint16_t)(dividend_lo >> 1 | shifted_out_bit << 15);
    } else {
        divisor_copy = divisor;
        divisor = divisor_copy;
        if (!(divisor_copy & 0x8000)) {
        } else {
            divisor = (uint16_t)(divisor >> 1);
            shifted_out_bit = (dividend_hi_work & 1);
            dividend_hi_work = (uint16_t)(dividend_hi_work >> 1);
            dividend_lo = (uint16_t)(dividend_lo >> 1 | shifted_out_bit << 15);
        }
    }
    count = 0x0010;
    quotient_2 = (uint16_t)(dividend_lo + dividend_lo);
    remainder = (uint16_t)(dividend_hi_work << 1 | ((uint32_t)dividend_lo + dividend_lo > 0xFFFF));
    for (;;) {
        trial_remainder = (uint16_t)(remainder - divisor);
        remainder_ge_divisor = (remainder >= divisor);
        if (!remainder_ge_divisor) {
            quotient_top_bit_lt = ((uint32_t)quotient_2 + quotient_2 > 0xFFFF);
            quotient_2 = (uint16_t)(quotient_2 + quotient_2);
            count = (uint16_t)(count - 1);
            remainder = (uint16_t)((uint16_t)(trial_remainder + divisor) << 1 | quotient_top_bit_lt);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            quotient_top_bit_ge = (quotient_2 >> 15);
            quotient_2 = (uint16_t)(quotient_2 << 1 | remainder_ge_divisor);
            count = (uint16_t)(count - 1);
            remainder = (uint16_t)(trial_remainder << 1 | quotient_top_bit_ge);
            if (count != 0) {
                continue;
            } else {
                return quotient_2;
            }
        }
    }
    return quotient_2;
}

