/*
 * fx_rotate.c: the Super FX code of asm/stuntracefx/math/fx_rotate.asm, written from it by
 * the generator.
 *
 * fx_rotate.asm: the Super FX's rotations: a vector from an angle, a turn
 * about the vertical, and a 3x3 matrix transposed.
 *
 * Angles are 0-255 for a full turn; cosine and sine are signed bytes
 * (64 = 1/2) in the tables at $00:973E and $00:96FE, the cosine table being
 * the sine table 64 entries on. The Super FX reads them through the ROM
 * buffer with ROMBR = 0. Each byte is SWAPped into the high byte, so a
 * product's high word is the value times the byte / 256.
 * C: src/stuntracefx/math/fx_rotate.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:C15C FxTurn
 * WHAT IT DOES: Turns the vector (R1, R3) by the angle R2 (0-255): R2 = R1 =
 * (sin * x + cos * z) / 2^17 and R3 = (cos * x - sin * z) / 2^17 with the
 * table bytes as signed values times 256.
 * HOW: The return address is pushed on the R10 stack and pulled into R15 at
 * the end. Each component is two LMULTs summed as 32-bit (ADD then ADC of
 * the low and high words) and the high word halved with ASR. The second
 * component's low words are subtracted the other way round (sin * z minus
 * cos * x) from its high words (cos * x minus sin * z); only the borrow of
 * that low subtraction reaches the result.
 * Entry: R1 = x, R2 = angle, R3 = z; R10 = stack; R11 = return address.
 * Exit: R1 = R2 = the new x; R3 = the new z; R0 = the second high word; R4 =
 * low word of cos * x; R5 = cos * 256; R6 = the old x; R7 = the second low
 * word; R8 = high word of sin * z; R9 = sin * 256; R14 = $96FE + angle; the
 * last RAM address R10; S, Z from R10; CY from the last ASR; OV from SBC.
 */
FxTurnResult FxTurn(uint16_t x, uint16_t angle, uint16_t z, uint16_t *stack)
{
    uint16_t cos_byte;
    uint16_t sin_byte;
    uint16_t cos_scaled;
    uint32_t cos_x_prod;
    uint32_t cos_z_prod;
    uint16_t sin_scaled;
    uint32_t sin_x_prod;
    uint32_t sin_z_prod;
    uint16_t sin_table_addr;

    fx_spill(stack);
    fx_r14((uint16_t)(ROT_COS_TABLE + angle));
    cos_byte = fx_getb();
    cos_scaled = (uint16_t)(cos_byte << 8 | cos_byte >> 8);
    sin_table_addr = (uint16_t)(ROT_SIN_TABLE + angle);
    fx_r14(sin_table_addr);
    sin_byte = fx_getb();
    sin_scaled = (uint16_t)(sin_byte << 8 | sin_byte >> 8);
    sin_x_prod = (uint32_t)((int32_t)(int16_t)sin_scaled * (int16_t)x);
    cos_z_prod = (uint32_t)((int32_t)(int16_t)cos_scaled * (int16_t)z);
    sin_z_prod = (uint32_t)((int32_t)(int16_t)sin_scaled * (int16_t)z);
    cos_x_prod = (uint32_t)((int32_t)(int16_t)cos_scaled * (int16_t)x);
    fx_unspill(stack);
    return (FxTurnResult){(uint16_t)((int16_t)(uint16_t)((uint16_t)(cos_z_prod >> 16) + (uint16_t)(sin_x_prod >> 16) + ((uint32_t)(uint16_t)sin_x_prod + (uint16_t)cos_z_prod > 0xFFFF)) >> 1), (uint16_t)((int16_t)(uint16_t)((uint16_t)(cos_x_prod >> 16) - (uint16_t)(sin_z_prod >> 16) - !((uint16_t)sin_z_prod >= (uint16_t)cos_x_prod)) >> 1), sin_table_addr};
}


/*
 * @routine 04:C1A7 FxMatrixFromAngles
 * WHAT IT DOES: Builds the 3x3 rotation matrix of the three angles at $0020
 * ($0022, $0024) into the nine words at R9: with s, c the sines and
 * cosines of angles 0, 1, 2, the words are s0s1s2 + c1c2, s0s1c2 - c1s2,
 * c0s1, c0s2, c0c2, -s0, s0c1s2 - s1c2, s0c1c2 + s1s2, c0c1.
 * HOW: RAMB and ROMB 0. Each sine is read from the word table at $00:983E
 * with linear interpolation (sine16); cosines are the sine a quarter turn
 * on. Products are FMULT then ROL (1.15 fixed point), the shared pairs
 * worked out first; the words are stored with STW in order.
 * Entry: R9 = where the matrix goes; R11 = return address. Exit: R9 =
 * matrix + 16; R0 = the last word; R1 = s1c2, R2 = c1c2, R3 = c1, R4 = s1,
 * R5 = c2, R6 = R8 = c0, R7 = s2, R12 = s0, R13 = s1s2, R14 = c1s2; ROMBR =
 * RAMBR = 0; S, CY, Z from the last ROL, OV from the add of the eighth
 * word; the last RAM address matrix + 16.
 */
FxMatrixFromAnglesResult FxMatrixFromAngles(uint16_t where_matrix)
{
    uint16_t c1c2;
    uint16_t c1s2;
    uint16_t cos0;
    uint16_t cos1;
    uint16_t cos2;
    uint16_t s1c2;
    uint16_t s1s2;
    uint16_t sin0;
    uint16_t sin1;
    uint16_t sin2;
    uint16_t angle0;
    uint16_t angle1;
    uint16_t angle2;
    uint16_t cos0_entry;
    uint16_t cos0_index;
    uint16_t cos1_entry;
    uint16_t cos1_index;
    uint16_t cos2_entry;
    uint16_t cos2_index;
    uint32_t prod_c1_c0;
    uint32_t prod_c1_c2;
    uint32_t prod_c1_s2;
    uint32_t prod_c2_c0;
    uint32_t prod_s1_c0;
    uint32_t prod_s1_c2;
    uint32_t prod_s1_s2;
    uint32_t prod_s2_c0;
    uint16_t sin0_entry;
    uint16_t sin0_index;
    uint16_t sin1_entry;
    uint16_t sin1_index;
    uint16_t sin2_entry;
    uint16_t sin2_index;
    uint16_t cos0_next_lo;
    uint16_t cos1_next_lo;
    uint16_t cos2_next_lo;
    uint16_t matrix_ptr_1;
    uint16_t matrix_ptr_2;
    uint16_t matrix_ptr_3;
    uint16_t matrix_ptr_4;
    uint16_t matrix_ptr_5;
    uint16_t matrix_ptr_6;
    uint16_t matrix_ptr_7;
    uint32_t prod_c1c2_s0;
    uint32_t prod_c1s2_s0;
    uint32_t prod_s1c2_s0;
    uint32_t prod_s1s2_s0;
    uint16_t sin0_next_lo;
    uint16_t sin1_next_lo;
    uint16_t sin2_next_lo;
    uint16_t cos0_entry_lo;
    uint16_t cos0_next_ptr;
    uint16_t cos1_entry_lo;
    uint16_t cos1_next_ptr;
    uint16_t cos2_entry_lo;
    uint16_t cos2_next_ptr;
    uint16_t sin0_entry_lo;
    uint16_t sin0_next_ptr;
    uint16_t sin1_entry_lo;
    uint16_t sin1_next_ptr;
    uint16_t sin2_entry_lo;
    uint16_t sin2_next_ptr;
    uint16_t angle0_quarter;
    uint16_t angle1_quarter;
    uint16_t angle2_quarter;
    uint16_t cos0_entry_ptr;
    uint16_t cos1_entry_ptr;
    uint16_t cos2_entry_ptr;
    uint16_t sin0_entry_ptr;
    uint16_t sin1_entry_ptr;
    uint16_t sin2_entry_ptr;
    uint16_t cos0_next_entry;
    uint16_t cos0_table_addr;
    uint16_t cos1_next_entry;
    uint16_t cos1_table_addr;
    uint16_t cos2_next_entry;
    uint16_t cos2_table_addr;
    uint16_t sin0_next_entry;
    uint16_t sin0_table_addr;
    uint16_t sin1_next_entry;
    uint16_t sin1_table_addr;
    uint16_t sin2_next_entry;
    uint16_t sin2_table_addr;
    uint32_t cos0_interp_prod;
    uint32_t cos1_interp_prod;
    uint32_t cos2_interp_prod;
    uint32_t sin0_interp_prod;
    uint32_t sin1_interp_prod;
    uint32_t sin2_interp_prod;

    fx_ramb(0);
    fx_romb(0);
    angle0 = fx_ldw(0x0020);
    sin0_index = (uint16_t)(angle0 >> 8);
    sin0_table_addr = (uint16_t)((uint16_t)(sin0_index + sin0_index) - 0x67C2);
    fx_r14(sin0_table_addr);
    sin0_entry_lo = fx_getb();
    sin0_entry_ptr = (uint16_t)(sin0_table_addr + 1);
    fx_r14(sin0_entry_ptr);
    sin0_entry = (uint16_t)((sin0_entry_lo & 0xFF) | fx_getb() << 8);
    sin0_next_ptr = (uint16_t)(sin0_entry_ptr + 1);
    fx_r14(sin0_next_ptr);
    sin0_next_lo = fx_getb();
    fx_r14((uint16_t)(sin0_next_ptr + 1));
    sin0_next_entry = (uint16_t)((sin0_next_lo & 0xFF) | fx_getb() << 8);
    sin0_interp_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(angle0 & 0xFF) << 8 | (uint16_t)(angle0 & 0xFF) >> 8) >> 1) * (int16_t)(uint16_t)(sin0_next_entry - sin0_entry));
    sin0 = (uint16_t)((uint16_t)((uint16_t)(sin0_interp_prod >> 16) << 1 | (int)(sin0_interp_prod >> 15 & 1)) + sin0_entry);
    fx_r14(0x4000);
    angle0_quarter = (uint16_t)(angle0 + 0x4000);
    cos0_index = (uint16_t)(angle0_quarter >> 8);
    cos0_table_addr = (uint16_t)((uint16_t)(cos0_index + cos0_index) - 0x67C2);
    fx_r14(cos0_table_addr);
    cos0_entry_lo = fx_getb();
    cos0_entry_ptr = (uint16_t)(cos0_table_addr + 1);
    fx_r14(cos0_entry_ptr);
    cos0_entry = (uint16_t)((cos0_entry_lo & 0xFF) | fx_getb() << 8);
    cos0_next_ptr = (uint16_t)(cos0_entry_ptr + 1);
    fx_r14(cos0_next_ptr);
    cos0_next_lo = fx_getb();
    fx_r14((uint16_t)(cos0_next_ptr + 1));
    cos0_next_entry = (uint16_t)((cos0_next_lo & 0xFF) | fx_getb() << 8);
    cos0_interp_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(angle0_quarter & 0xFF) << 8 | (uint16_t)(angle0_quarter & 0xFF) >> 8) >> 1) * (int16_t)(uint16_t)(cos0_next_entry - cos0_entry));
    cos0 = (uint16_t)((uint16_t)((uint16_t)(cos0_interp_prod >> 16) << 1 | (int)(cos0_interp_prod >> 15 & 1)) + cos0_entry);
    angle1 = fx_ldw(0x0022);
    sin1_index = (uint16_t)(angle1 >> 8);
    sin1_table_addr = (uint16_t)((uint16_t)(sin1_index + sin1_index) - 0x67C2);
    fx_r14(sin1_table_addr);
    sin1_entry_lo = fx_getb();
    sin1_entry_ptr = (uint16_t)(sin1_table_addr + 1);
    fx_r14(sin1_entry_ptr);
    sin1_entry = (uint16_t)((sin1_entry_lo & 0xFF) | fx_getb() << 8);
    sin1_next_ptr = (uint16_t)(sin1_entry_ptr + 1);
    fx_r14(sin1_next_ptr);
    sin1_next_lo = fx_getb();
    fx_r14((uint16_t)(sin1_next_ptr + 1));
    sin1_next_entry = (uint16_t)((sin1_next_lo & 0xFF) | fx_getb() << 8);
    sin1_interp_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(angle1 & 0xFF) << 8 | (uint16_t)(angle1 & 0xFF) >> 8) >> 1) * (int16_t)(uint16_t)(sin1_next_entry - sin1_entry));
    sin1 = (uint16_t)((uint16_t)((uint16_t)(sin1_interp_prod >> 16) << 1 | (int)(sin1_interp_prod >> 15 & 1)) + sin1_entry);
    fx_r14(0x4000);
    angle1_quarter = (uint16_t)(angle1 + 0x4000);
    cos1_index = (uint16_t)(angle1_quarter >> 8);
    cos1_table_addr = (uint16_t)((uint16_t)(cos1_index + cos1_index) - 0x67C2);
    fx_r14(cos1_table_addr);
    cos1_entry_lo = fx_getb();
    cos1_entry_ptr = (uint16_t)(cos1_table_addr + 1);
    fx_r14(cos1_entry_ptr);
    cos1_entry = (uint16_t)((cos1_entry_lo & 0xFF) | fx_getb() << 8);
    cos1_next_ptr = (uint16_t)(cos1_entry_ptr + 1);
    fx_r14(cos1_next_ptr);
    cos1_next_lo = fx_getb();
    fx_r14((uint16_t)(cos1_next_ptr + 1));
    cos1_next_entry = (uint16_t)((cos1_next_lo & 0xFF) | fx_getb() << 8);
    cos1_interp_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(angle1_quarter & 0xFF) << 8 | (uint16_t)(angle1_quarter & 0xFF) >> 8) >> 1) * (int16_t)(uint16_t)(cos1_next_entry - cos1_entry));
    cos1 = (uint16_t)((uint16_t)((uint16_t)(cos1_interp_prod >> 16) << 1 | (int)(cos1_interp_prod >> 15 & 1)) + cos1_entry);
    angle2 = fx_ldw(0x0024);
    sin2_index = (uint16_t)(angle2 >> 8);
    sin2_table_addr = (uint16_t)((uint16_t)(sin2_index + sin2_index) - 0x67C2);
    fx_r14(sin2_table_addr);
    sin2_entry_lo = fx_getb();
    sin2_entry_ptr = (uint16_t)(sin2_table_addr + 1);
    fx_r14(sin2_entry_ptr);
    sin2_entry = (uint16_t)((sin2_entry_lo & 0xFF) | fx_getb() << 8);
    sin2_next_ptr = (uint16_t)(sin2_entry_ptr + 1);
    fx_r14(sin2_next_ptr);
    sin2_next_lo = fx_getb();
    fx_r14((uint16_t)(sin2_next_ptr + 1));
    sin2_next_entry = (uint16_t)((sin2_next_lo & 0xFF) | fx_getb() << 8);
    sin2_interp_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(angle2 & 0xFF) << 8 | (uint16_t)(angle2 & 0xFF) >> 8) >> 1) * (int16_t)(uint16_t)(sin2_next_entry - sin2_entry));
    sin2 = (uint16_t)((uint16_t)((uint16_t)(sin2_interp_prod >> 16) << 1 | (int)(sin2_interp_prod >> 15 & 1)) + sin2_entry);
    fx_r14(0x4000);
    angle2_quarter = (uint16_t)(angle2 + 0x4000);
    cos2_index = (uint16_t)(angle2_quarter >> 8);
    cos2_table_addr = (uint16_t)((uint16_t)(cos2_index + cos2_index) - 0x67C2);
    fx_r14(cos2_table_addr);
    cos2_entry_lo = fx_getb();
    cos2_entry_ptr = (uint16_t)(cos2_table_addr + 1);
    fx_r14(cos2_entry_ptr);
    cos2_entry = (uint16_t)((cos2_entry_lo & 0xFF) | fx_getb() << 8);
    cos2_next_ptr = (uint16_t)(cos2_entry_ptr + 1);
    fx_r14(cos2_next_ptr);
    cos2_next_lo = fx_getb();
    fx_r14((uint16_t)(cos2_next_ptr + 1));
    cos2_next_entry = (uint16_t)((cos2_next_lo & 0xFF) | fx_getb() << 8);
    cos2_interp_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)((uint16_t)(angle2_quarter & 0xFF) << 8 | (uint16_t)(angle2_quarter & 0xFF) >> 8) >> 1) * (int16_t)(uint16_t)(cos2_next_entry - cos2_entry));
    cos2 = (uint16_t)((uint16_t)((uint16_t)(cos2_interp_prod >> 16) << 1 | (int)(cos2_interp_prod >> 15 & 1)) + cos2_entry);
    prod_s1_c2 = (uint32_t)((int32_t)(int16_t)sin1 * (int16_t)cos2);
    s1c2 = (uint16_t)((uint16_t)(prod_s1_c2 >> 16) << 1 | (int)(prod_s1_c2 >> 15 & 1));
    prod_c1_c2 = (uint32_t)((int32_t)(int16_t)cos1 * (int16_t)cos2);
    c1c2 = (uint16_t)((uint16_t)(prod_c1_c2 >> 16) << 1 | (int)(prod_c1_c2 >> 15 & 1));
    prod_s1_s2 = (uint32_t)((int32_t)(int16_t)sin1 * (int16_t)sin2);
    s1s2 = (uint16_t)((uint16_t)(prod_s1_s2 >> 16) << 1 | (int)(prod_s1_s2 >> 15 & 1));
    prod_c1_s2 = (uint32_t)((int32_t)(int16_t)cos1 * (int16_t)sin2);
    c1s2 = (uint16_t)((uint16_t)(prod_c1_s2 >> 16) << 1 | (int)(prod_c1_s2 >> 15 & 1));
    fx_r14(c1s2);
    prod_s1s2_s0 = (uint32_t)((int32_t)(int16_t)s1s2 * (int16_t)sin0);
    fx_stw(where_matrix, (uint16_t)((uint16_t)((uint16_t)(prod_s1s2_s0 >> 16) << 1 | (int)(prod_s1s2_s0 >> 15 & 1)) + c1c2));
    matrix_ptr_1 = (uint16_t)(where_matrix + 2);
    prod_s1c2_s0 = (uint32_t)((int32_t)(int16_t)s1c2 * (int16_t)sin0);
    fx_stw(matrix_ptr_1, (uint16_t)((uint16_t)((uint16_t)(prod_s1c2_s0 >> 16) << 1 | (int)(prod_s1c2_s0 >> 15 & 1)) - c1s2));
    matrix_ptr_2 = (uint16_t)(matrix_ptr_1 + 2);
    prod_s1_c0 = (uint32_t)((int32_t)(int16_t)sin1 * (int16_t)cos0);
    fx_stw(matrix_ptr_2, (uint16_t)((uint16_t)(prod_s1_c0 >> 16) << 1 | (int)(prod_s1_c0 >> 15 & 1)));
    matrix_ptr_3 = (uint16_t)(matrix_ptr_2 + 2);
    prod_s2_c0 = (uint32_t)((int32_t)(int16_t)sin2 * (int16_t)cos0);
    fx_stw(matrix_ptr_3, (uint16_t)((uint16_t)(prod_s2_c0 >> 16) << 1 | (int)(prod_s2_c0 >> 15 & 1)));
    matrix_ptr_4 = (uint16_t)(matrix_ptr_3 + 2);
    prod_c2_c0 = (uint32_t)((int32_t)(int16_t)cos2 * (int16_t)cos0);
    fx_stw(matrix_ptr_4, (uint16_t)((uint16_t)(prod_c2_c0 >> 16) << 1 | (int)(prod_c2_c0 >> 15 & 1)));
    matrix_ptr_5 = (uint16_t)(matrix_ptr_4 + 2);
    fx_stw(matrix_ptr_5, (uint16_t)((uint16_t)~sin0 + 1));
    matrix_ptr_6 = (uint16_t)(matrix_ptr_5 + 2);
    prod_c1s2_s0 = (uint32_t)((int32_t)(int16_t)c1s2 * (int16_t)sin0);
    fx_stw(matrix_ptr_6, (uint16_t)((uint16_t)((uint16_t)(prod_c1s2_s0 >> 16) << 1 | (int)(prod_c1s2_s0 >> 15 & 1)) - s1c2));
    matrix_ptr_7 = (uint16_t)(matrix_ptr_6 + 2);
    prod_c1c2_s0 = (uint32_t)((int32_t)(int16_t)c1c2 * (int16_t)sin0);
    fx_stw(matrix_ptr_7, (uint16_t)((uint16_t)((uint16_t)(prod_c1c2_s0 >> 16) << 1 | (int)(prod_c1c2_s0 >> 15 & 1)) + s1s2));
    prod_c1_c0 = (uint32_t)((int32_t)(int16_t)cos1 * (int16_t)cos0);
    fx_stw((uint16_t)(matrix_ptr_7 + 2), (uint16_t)((uint16_t)(prod_c1_c0 >> 16) << 1 | (int)(prod_c1_c0 >> 15 & 1)));
    return (FxMatrixFromAnglesResult){sin1, cos2, sin2, c1s2};
}


/*
 * @routine 04:C2C8 FxMatrixTranspose04
 * WHAT IT DOES: Transposes the 3x3 matrix of words at R9 in place.
 * HOW: Three swaps of the off-diagonal pairs (words 1 and 3, 2 and 6, 5 and
 * 7), walking R9 with INC and ADD/SUB of small constants: loads and stores
 * in the order 1, 2, 3, store 3, store 1, load 5, 6, store 6, load 7, store
 * 7, store 5, store 2.
 * Entry: R9 = the matrix; R11 = return address. Exit: R9 = matrix + 4; R1 =
 * the old word 5, R2 = the old word 7, R3 = the old word 6; the last RAM
 * address matrix + 4; OV, S, CY, Z from the SUB that leaves R9.
 */
uint16_t FxMatrixTranspose04(uint16_t matrix)
{
    uint16_t elem1;
    uint16_t elem2;
    uint16_t elem3;
    uint16_t elem5;
    uint16_t elem6;
    uint16_t elem7;
    uint16_t elem1_ptr;
    uint16_t elem2_ptr;
    uint16_t elem3_ptr;
    uint16_t elem5_ptr;
    uint16_t elem6_ptr;
    uint16_t elem7_ptr;
    uint16_t elem1_store_ptr;
    uint16_t elem2_store_ptr;
    uint16_t elem5_store_ptr;

    elem1_ptr = (uint16_t)(matrix + 2);
    elem1 = fx_ldw(elem1_ptr);
    elem2_ptr = (uint16_t)(elem1_ptr + 2);
    elem2 = fx_ldw(elem2_ptr);
    elem3_ptr = (uint16_t)(elem2_ptr + 2);
    elem3 = fx_ldw(elem3_ptr);
    fx_stw(elem3_ptr, elem1);
    elem1_store_ptr = (uint16_t)(elem3_ptr - 4);
    fx_stw(elem1_store_ptr, elem3);
    elem5_ptr = (uint16_t)(elem1_store_ptr + 8);
    elem5 = fx_ldw(elem5_ptr);
    elem6_ptr = (uint16_t)(elem5_ptr + 2);
    elem6 = fx_ldw(elem6_ptr);
    fx_stw(elem6_ptr, elem2);
    elem7_ptr = (uint16_t)(elem6_ptr + 2);
    elem7 = fx_ldw(elem7_ptr);
    fx_stw(elem7_ptr, elem5);
    elem5_store_ptr = (uint16_t)(elem7_ptr - 4);
    fx_stw(elem5_store_ptr, elem7);
    elem2_store_ptr = (uint16_t)(elem5_store_ptr - 6);
    fx_stw(elem2_store_ptr, elem6);
    return elem2_store_ptr;
}


/*
 * @routine 04:C2F8 FxAngleOf
 * WHAT IT DOES: The angle of the vector (R1, R2), 0-$FFFF for a full turn,
 * to R0: $4000 for R2 = 0 (before the signs below), $2000 when |R1| =
 * |R2|, otherwise from the arctangent table at $00:9E40 indexed by the
 * smaller over the larger (times $4000, / 64); the result is negated when
 * R1 and R2 differ in sign, and $8000 is added when R2 is negative.
 * HOW: |R1| to R6 and |R2| to R0 with NOT and INC; the smaller (BLT) is
 * shifted into R0:R4 as times $4000 (LSR, ROR twice) and FxDivide gives the
 * ratio; the table word (GETB, GETBH, ROMBR 0) is the angle when |R2| is
 * the larger, else $4000 minus it. R11 and R14 are kept on the R10 stack.
 * Entry: R1, R2 = the vector; R10 = stack; R11 = return address. Exit: R0 =
 * the angle; R4 = R1 xor R2, or $8000 when R2 is negative; after the
 * divide: R5 = -1 when |R2| is the larger, else 0; R6 = the larger; R11 =
 * $C33E; R12 = 0; R13 = $C14B; R14 kept; S, Z from R10; the last RAM
 * address R10.
 */
FxAngleOfResult FxAngleOf(uint16_t vector_x, uint16_t vector_y, uint16_t prior_swap_marker, uint16_t *stack, uint16_t rom_at)
{
    uint16_t abs_y;
    uint16_t neg_y;
    uint16_t angle_2;
    uint16_t divisor;
    uint16_t quotient;
    uint16_t rom_at_2;
    uint16_t sign_xor;
    int y_is_zero;
    uint16_t y_minus_x;
    int sub_overflow;
    uint16_t atan_low_byte;
    uint16_t saved_divisor;
    int y_less_than_x;
    uint16_t numerator_half;
    uint16_t xor_negative_2;
    uint16_t atan_table_addr;
    uint16_t swap_marker_copy;

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
        neg_y = (uint16_t)((uint16_t)~abs_y + 1);
        y_is_zero = (neg_y == 0);
        abs_y = neg_y;
    }
    if (!y_is_zero) {
        prior_swap_marker = 0;
        y_minus_x = (uint16_t)(abs_y - divisor);
        sub_overflow = (((abs_y ^ divisor) & (abs_y ^ (uint16_t)(abs_y - divisor - 0)) & 0x8000) != 0);
        y_less_than_x = ((y_minus_x & 0x8000) != 0);
        if (y_minus_x != 0) {
            if ((y_less_than_x != sub_overflow)) {
            } else {
                saved_divisor = divisor;
                divisor = abs_y;
                abs_y = saved_divisor;
                prior_swap_marker = (uint16_t)(prior_swap_marker - 1);
            }
            numerator_half = (uint16_t)(abs_y >> 1);
            quotient = FxDivide((uint32_t)(uint16_t)(numerator_half >> 1) << 16 | (uint16_t)((uint16_t)(0 >> 1 | (abs_y & 1) << 15) >> 1 | (numerator_half & 1) << 15), divisor);
            fx_romb(0);
            atan_table_addr = (uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)(quotient >> 1) >> 1) >> 1) >> 1) >> 1) & ~1) + FX_ATAN_TABLE);
            fx_r14(atan_table_addr);
            atan_low_byte = fx_getb();
            fx_r14((uint16_t)(atan_table_addr + 1));
            angle_2 = (uint16_t)((atan_low_byte & 0xFF) | fx_getb() << 8);
            swap_marker_copy = prior_swap_marker;
            prior_swap_marker = swap_marker_copy;
            if (swap_marker_copy & 0x8000) {
            } else {
                angle_2 = (uint16_t)((uint16_t)~angle_2 + 0x4001);
            }
        } else {
            angle_2 = 0x2000;
        }
    } else {
        angle_2 = 0x4000;
    }
    sign_xor = (vector_x ^ vector_y);
    xor_negative_2 = sign_xor;
    if (!(sign_xor & 0x8000)) {
    } else {
        angle_2 = (uint16_t)((uint16_t)~angle_2 + 1);
    }
    if (!(vector_y & 0x8000)) {
    } else {
        xor_negative_2 = 0x8000;
        angle_2 = (uint16_t)(angle_2 + xor_negative_2);
    }
    rom_at_2 = fx_pull(stack);
    fx_r14(rom_at_2);
    fx_unspill(stack);
    return (FxAngleOfResult){angle_2, xor_negative_2, prior_swap_marker};
}


/*
 * @routine 04:CD98 FxAngleVector
 * WHAT IT DOES: The vector of length R4 at the angle in R0's high byte:
 * R1 = -sin * length, R3 = cos * length, both scaled by 16 / 128 (a byte
 * table value of 64 gives length * 8), and R2 = 0.
 * HOW: HIB for the angle, ROMB 0, then for each table the byte SWAPped up,
 * FMULT by R6 = length, (NOT and INC for the sine), and four ADDs to
 * itself.
 * Entry: R0 high byte = angle; R4 = length; R11 = return address. Exit: R1,
 * R3 as above, R0 = R3; R2 = 0; R6 = length; R14 = $973E + angle; ROMBR =
 * 0; OV, S, CY, Z from the last ADD.
 */
FxAngleVectorResult FxAngleVector(uint16_t heading_angle, uint16_t length)
{
    uint16_t cos_x2;
    uint16_t cos_x4;
    uint16_t cos_x8;
    uint16_t cos_value;
    uint16_t sin_value;
    uint16_t cos_scaled;
    uint16_t neg_sin_x2;
    uint16_t neg_sin_x4;
    uint16_t neg_sin_x8;
    uint16_t angle_index;
    uint16_t cos_table_addr;
    uint16_t neg_sin_scaled;

    angle_index = (uint16_t)(heading_angle >> 8);
    fx_romb(0);
    fx_r14(ROT_SIN_TABLE);
    fx_r14((uint16_t)((uint16_t)(angle_index & 0xFF) + ROT_SIN_TABLE));
    sin_value = fx_getb();
    neg_sin_scaled = (uint16_t)((uint16_t)~(uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(sin_value << 8 | sin_value >> 8) * (int16_t)length) >> 16) + 1);
    neg_sin_x2 = (uint16_t)(neg_sin_scaled + neg_sin_scaled);
    neg_sin_x4 = (uint16_t)(neg_sin_x2 + neg_sin_x2);
    neg_sin_x8 = (uint16_t)(neg_sin_x4 + neg_sin_x4);
    fx_r14(ROT_COS_TABLE);
    cos_table_addr = (uint16_t)((uint16_t)(angle_index & 0xFF) + ROT_COS_TABLE);
    fx_r14(cos_table_addr);
    cos_value = fx_getb();
    cos_scaled = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(cos_value << 8 | cos_value >> 8) * (int16_t)length) >> 16);
    cos_x2 = (uint16_t)(cos_scaled + cos_scaled);
    cos_x4 = (uint16_t)(cos_x2 + cos_x2);
    cos_x8 = (uint16_t)(cos_x4 + cos_x4);
    return (FxAngleVectorResult){(uint16_t)(neg_sin_x8 + neg_sin_x8), 0, (uint16_t)(cos_x8 + cos_x8), cos_table_addr};
}

