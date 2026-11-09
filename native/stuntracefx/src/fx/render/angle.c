/*
 * angle.c: the Super FX code of asm/stuntracefx/render/angle.asm, written from it by
 * the generator.
 *
 * angle.asm: the Super FX's angles: a full turn is $10000, so $4000 is a
 * quarter turn. The arctangent comes from a table of 256 words in ROM
 * bank $00 at $9E40, indexed by the ratio of the smaller side to the
 * larger.
 * C: src/stuntracefx/render/angle.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:81E3 FxAngleProgram
 * WHAT IT DOES: A program the S-CPU starts: the angle of the vector
 * ($002C, $0068) by FxAtan2, stored at $0040.
 * HOW: Sets the R10 stack to $0240; LMS loads $0068 into R1 and $002C into
 * R2; FxAtan2; SMS stores R0. Then STOP.
 * Entry: started by the S-CPU. Exit: (at the STOP) R10 = $0240; R11 =
 * $81F1; the registers and flags as FxAtan2 leaves them; the last RAM
 * address $0040.
 */
void FxAngleProgram(uint16_t rom_at)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t angle;
    uint16_t vector_x;
    uint16_t vector_y;

    *stack = 0x0240;
    vector_x = fx_ldw(0x0068);
    vector_y = fx_ldw(0x002C);
    angle = FxAtan2(vector_x, vector_y, stack, rom_at);
    fx_stw(0x0040, angle);
    return;
}


/*
 * @routine 01:81F7 FxAtan2
 * WHAT IT DOES: The angle of the vector (R2, R1): 0 along +R2, $4000
 * along +R1, as a word where a full turn is $10000.
 * HOW: Pushes R11 and R14 on the R10 stack. With R6 = |R1| and R0 = |R2|:
 * R2 = 0 gives $4000 and equal sides give $2000; otherwise the smaller
 * side << 14 is divided by the larger (FxDivide32By16), and the quotient
 * >> 5, made even, indexes the arctangent table (GETB/GETBH from ROM bank
 * 0). When |R1| is the larger side the angle is $4000 minus the table's.
 * Then it is negated when R1 and R2 differ in sign, and $8000 is added
 * when R2 is negative. Pulls R14 and returns by pulling R11's value into
 * R15.
 * Entry: R1, R2 = the vector; R10 = the stack; R11 = return address.
 * Exit: R0 = the angle; R4 = $8000 when R2 is negative, else R1 XOR R2;
 * R6 = the larger side (|R1| when R2 = 0 or the sides are equal); R5 =
 * $FFFF when |R2| is the larger side, 0 when |R1| is or they are equal,
 * kept when R2 = 0. When the table was used: R11 = $823D, R12 = 0, R13 =
 * $81D2, the ROM bank 0. R14 (written back, so the ROM buffer refilled
 * from it) and R10 as at entry; the last RAM address
 * R10; S and Z from R10; OV and CY from the ADD of $8000 when R2 is
 * negative, else OV = bit 7 of R2 and CY from the last ADD before (kept
 * when R2 = 0, set when the sides are equal).
 */
uint16_t FxAtan2(uint16_t vector_x, uint16_t vector_y, uint16_t *stack, uint16_t rom_at)
{
    uint16_t abs_y;
    uint16_t angle_2;
    uint16_t divisor;
    uint16_t quotient;
    uint16_t rom_at_2;
    uint16_t y_halved;
    uint16_t swap_temp;
    int y_below_x;
    int y_is_zero;
    uint16_t y_minus_x;
    uint16_t larger_side;
    uint16_t abs_y_negated;
    int diff_overflow;
    uint16_t table_low_byte;
    uint16_t atan_table_addr;

    fx_spill(stack);
    fx_push(stack, rom_at);
    divisor = vector_x;
    if (!(vector_x & 0x8000)) {
    } else {
        divisor = (uint16_t)((uint16_t)~divisor + 1);
    }
    y_is_zero = (vector_y == 0);
    abs_y = vector_y;
    if (!(vector_y & 0x8000)) {
    } else {
        abs_y_negated = (uint16_t)((uint16_t)~abs_y + 1);
        y_is_zero = (abs_y_negated == 0);
        abs_y = abs_y_negated;
    }
    if (!y_is_zero) {
        larger_side = 0;
        y_minus_x = (uint16_t)(abs_y - divisor);
        diff_overflow = (((abs_y ^ divisor) & (abs_y ^ (uint16_t)(abs_y - divisor - 0)) & 0x8000) != 0);
        y_below_x = ((y_minus_x & 0x8000) != 0);
        if (y_minus_x != 0) {
            if ((y_below_x != diff_overflow)) {
            } else {
                swap_temp = divisor;
                divisor = abs_y;
                abs_y = swap_temp;
                larger_side = (uint16_t)(larger_side - 1);
            }
            y_halved = (uint16_t)(abs_y >> 1);
            quotient = FxDivide32By16((uint16_t)(y_halved >> 1), (uint16_t)((uint16_t)(0 >> 1 | (abs_y & 1) << 15) >> 1 | (y_halved & 1) << 15), divisor);
            fx_romb(0);
            atan_table_addr = (uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)(quotient >> 1) >> 1) >> 1) >> 1) >> 1) & ~1) + FX_ATAN_TABLE);
            fx_r14(atan_table_addr);
            table_low_byte = fx_getb();
            fx_r14((uint16_t)(atan_table_addr + 1));
            angle_2 = (uint16_t)((table_low_byte & 0xFF) | fx_getb() << 8);
            if (larger_side & 0x8000) {
            } else {
                angle_2 = (uint16_t)((uint16_t)~angle_2 + 0x4001);
            }
        } else {
            angle_2 = 0x2000;
        }
    } else {
        angle_2 = 0x4000;
    }
    if (!((vector_x ^ vector_y) & 0x8000)) {
    } else {
        angle_2 = (uint16_t)((uint16_t)~angle_2 + 1);
    }
    if (!(vector_y & 0x8000)) {
    } else {
        angle_2 = (uint16_t)(angle_2 - 0x8000);
    }
    rom_at_2 = fx_pull(stack);
    fx_r14(rom_at_2);
    fx_unspill(stack);
    return angle_2;
}

