/*
 * table_2702.c: the Super FX code of asm/stuntracefx/render/table_2702.asm, written from it by
 * the generator.
 *
 * table_2702.asm: a Super FX program that writes a fixed table of twelve
 * four-byte entries at $70:2702: a zero, then three bytes.
 * C: src/stuntracefx/render/table_2702.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:D1EB FxTable2702Program
 * WHAT IT DOES: A program the S-CPU starts: writes the fixed 48-byte table
 * at $70:2702.
 * HOW: Sets the RAM bank to 0, then stores each byte with STB through R1,
 * loading it with IBT (zero) or IWT (the rest), INC R1 after each. Then
 * STOP.
 * Entry: started by the S-CPU. Exit: (at the STOP) R1 = $2732; R0 = the
 * last byte; the RAM bank 0; the last RAM address $2731; CY set and OV
 * clear (SUB R0); S and Z from R1.
 */
FxTable2702ProgramResult FxTable2702Program(void)
{
    uint16_t r_t_46;
    uint16_t table_byte_ptr_1;
    uint16_t table_byte_ptr_2;
    uint16_t table_byte_ptr_3;
    uint16_t table_byte_ptr_4;
    uint16_t table_byte_ptr_5;
    uint16_t table_byte_ptr_6;
    uint16_t table_byte_ptr_7;
    uint16_t table_byte_ptr_8;
    uint16_t table_byte_ptr_9;
    uint16_t table_byte_ptr_10;
    uint16_t table_byte_ptr_11;
    uint16_t table_byte_ptr_12;
    uint16_t table_byte_ptr_13;
    uint16_t table_byte_ptr_14;
    uint16_t table_byte_ptr_15;
    uint16_t table_byte_ptr_16;
    uint16_t table_byte_ptr_17;
    uint16_t table_byte_ptr_18;
    uint16_t table_byte_ptr_19;
    uint16_t table_byte_ptr_20;
    uint16_t table_byte_ptr_21;
    uint16_t table_byte_ptr_22;
    uint16_t table_byte_ptr_23;
    uint16_t table_byte_ptr_24;
    uint16_t table_byte_ptr_25;
    uint16_t table_byte_ptr_26;
    uint16_t table_byte_ptr_27;
    uint16_t table_byte_ptr_28;
    uint16_t table_byte_ptr_29;
    uint16_t table_byte_ptr_30;
    uint16_t table_byte_ptr_31;
    uint16_t table_byte_ptr_32;
    uint16_t table_byte_ptr_33;
    uint16_t table_byte_ptr_34;
    uint16_t table_byte_ptr_35;
    uint16_t table_byte_ptr_36;
    uint16_t table_byte_ptr_37;
    uint16_t table_byte_ptr_38;
    uint16_t table_byte_ptr_39;
    uint16_t table_byte_ptr_40;
    uint16_t table_byte_ptr_41;
    uint16_t table_byte_ptr_42;
    uint16_t table_byte_ptr_43;
    uint16_t table_byte_ptr_44;
    uint16_t table_byte_ptr_45;
    uint16_t table_byte_ptr_46;

    fx_ramb(0);
    fx_stb(0x2702, (uint8_t)0);
    table_byte_ptr_1 = (uint16_t)(0 + 0x2703);
    fx_stb(table_byte_ptr_1, (uint8_t)0x001F);
    table_byte_ptr_2 = (uint16_t)(table_byte_ptr_1 + 1);
    fx_stb(table_byte_ptr_2, (uint8_t)0x001B);
    table_byte_ptr_3 = (uint16_t)(table_byte_ptr_2 + 1);
    fx_stb(table_byte_ptr_3, (uint8_t)0x001E);
    table_byte_ptr_4 = (uint16_t)(table_byte_ptr_3 + 1);
    fx_stb(table_byte_ptr_4, (uint8_t)0);
    table_byte_ptr_5 = (uint16_t)(table_byte_ptr_4 + 1);
    fx_stb(table_byte_ptr_5, (uint8_t)0x00BF);
    table_byte_ptr_6 = (uint16_t)(table_byte_ptr_5 + 1);
    fx_stb(table_byte_ptr_6, (uint8_t)0x00B7);
    table_byte_ptr_7 = (uint16_t)(table_byte_ptr_6 + 1);
    fx_stb(table_byte_ptr_7, (uint8_t)0x00B6);
    table_byte_ptr_8 = (uint16_t)(table_byte_ptr_7 + 1);
    fx_stb(table_byte_ptr_8, (uint8_t)0);
    table_byte_ptr_9 = (uint16_t)(table_byte_ptr_8 + 1);
    fx_stb(table_byte_ptr_9, (uint8_t)0x00BF);
    table_byte_ptr_10 = (uint16_t)(table_byte_ptr_9 + 1);
    fx_stb(table_byte_ptr_10, (uint8_t)0x00BC);
    table_byte_ptr_11 = (uint16_t)(table_byte_ptr_10 + 1);
    fx_stb(table_byte_ptr_11, (uint8_t)0x00BD);
    table_byte_ptr_12 = (uint16_t)(table_byte_ptr_11 + 1);
    fx_stb(table_byte_ptr_12, (uint8_t)0);
    table_byte_ptr_13 = (uint16_t)(table_byte_ptr_12 + 1);
    fx_stb(table_byte_ptr_13, (uint8_t)0x00BF);
    table_byte_ptr_14 = (uint16_t)(table_byte_ptr_13 + 1);
    fx_stb(table_byte_ptr_14, (uint8_t)0x00B5);
    table_byte_ptr_15 = (uint16_t)(table_byte_ptr_14 + 1);
    fx_stb(table_byte_ptr_15, (uint8_t)0x00BB);
    table_byte_ptr_16 = (uint16_t)(table_byte_ptr_15 + 1);
    fx_stb(table_byte_ptr_16, (uint8_t)0);
    table_byte_ptr_17 = (uint16_t)(table_byte_ptr_16 + 1);
    fx_stb(table_byte_ptr_17, (uint8_t)0x001F);
    table_byte_ptr_18 = (uint16_t)(table_byte_ptr_17 + 1);
    fx_stb(table_byte_ptr_18, (uint8_t)0x005B);
    table_byte_ptr_19 = (uint16_t)(table_byte_ptr_18 + 1);
    fx_stb(table_byte_ptr_19, (uint8_t)0x0058);
    table_byte_ptr_20 = (uint16_t)(table_byte_ptr_19 + 1);
    fx_stb(table_byte_ptr_20, (uint8_t)0);
    table_byte_ptr_21 = (uint16_t)(table_byte_ptr_20 + 1);
    fx_stb(table_byte_ptr_21, (uint8_t)0x00BF);
    table_byte_ptr_22 = (uint16_t)(table_byte_ptr_21 + 1);
    fx_stb(table_byte_ptr_22, (uint8_t)0x00B2);
    table_byte_ptr_23 = (uint16_t)(table_byte_ptr_22 + 1);
    fx_stb(table_byte_ptr_23, (uint8_t)0x00B3);
    table_byte_ptr_24 = (uint16_t)(table_byte_ptr_23 + 1);
    fx_stb(table_byte_ptr_24, (uint8_t)0);
    table_byte_ptr_25 = (uint16_t)(table_byte_ptr_24 + 1);
    fx_stb(table_byte_ptr_25, (uint8_t)0x001E);
    table_byte_ptr_26 = (uint16_t)(table_byte_ptr_25 + 1);
    fx_stb(table_byte_ptr_26, (uint8_t)0x0015);
    table_byte_ptr_27 = (uint16_t)(table_byte_ptr_26 + 1);
    fx_stb(table_byte_ptr_27, (uint8_t)0x0014);
    table_byte_ptr_28 = (uint16_t)(table_byte_ptr_27 + 1);
    fx_stb(table_byte_ptr_28, (uint8_t)0);
    table_byte_ptr_29 = (uint16_t)(table_byte_ptr_28 + 1);
    fx_stb(table_byte_ptr_29, (uint8_t)0x0017);
    table_byte_ptr_30 = (uint16_t)(table_byte_ptr_29 + 1);
    fx_stb(table_byte_ptr_30, (uint8_t)0x0019);
    table_byte_ptr_31 = (uint16_t)(table_byte_ptr_30 + 1);
    fx_stb(table_byte_ptr_31, (uint8_t)0x001A);
    table_byte_ptr_32 = (uint16_t)(table_byte_ptr_31 + 1);
    fx_stb(table_byte_ptr_32, (uint8_t)0);
    table_byte_ptr_33 = (uint16_t)(table_byte_ptr_32 + 1);
    fx_stb(table_byte_ptr_33, (uint8_t)0x0017);
    table_byte_ptr_34 = (uint16_t)(table_byte_ptr_33 + 1);
    fx_stb(table_byte_ptr_34, (uint8_t)0x0018);
    table_byte_ptr_35 = (uint16_t)(table_byte_ptr_34 + 1);
    fx_stb(table_byte_ptr_35, (uint8_t)0x001E);
    table_byte_ptr_36 = (uint16_t)(table_byte_ptr_35 + 1);
    fx_stb(table_byte_ptr_36, (uint8_t)0);
    table_byte_ptr_37 = (uint16_t)(table_byte_ptr_36 + 1);
    fx_stb(table_byte_ptr_37, (uint8_t)0x0017);
    table_byte_ptr_38 = (uint16_t)(table_byte_ptr_37 + 1);
    fx_stb(table_byte_ptr_38, (uint8_t)0x0019);
    table_byte_ptr_39 = (uint16_t)(table_byte_ptr_38 + 1);
    fx_stb(table_byte_ptr_39, (uint8_t)0x001A);
    table_byte_ptr_40 = (uint16_t)(table_byte_ptr_39 + 1);
    fx_stb(table_byte_ptr_40, (uint8_t)0);
    table_byte_ptr_41 = (uint16_t)(table_byte_ptr_40 + 1);
    fx_stb(table_byte_ptr_41, (uint8_t)0x0017);
    table_byte_ptr_42 = (uint16_t)(table_byte_ptr_41 + 1);
    fx_stb(table_byte_ptr_42, (uint8_t)0x0018);
    table_byte_ptr_43 = (uint16_t)(table_byte_ptr_42 + 1);
    fx_stb(table_byte_ptr_43, (uint8_t)0x0015);
    table_byte_ptr_44 = (uint16_t)(table_byte_ptr_43 + 1);
    fx_stb(table_byte_ptr_44, (uint8_t)0);
    table_byte_ptr_45 = (uint16_t)(table_byte_ptr_44 + 1);
    fx_stb(table_byte_ptr_45, (uint8_t)0x0014);
    table_byte_ptr_46 = (uint16_t)(table_byte_ptr_45 + 1);
    fx_stb(table_byte_ptr_46, (uint8_t)0x0015);
    r_t_46 = (uint16_t)(table_byte_ptr_46 + 1);
    fx_stb(r_t_46, (uint8_t)0x001E);
    return (FxTable2702ProgramResult){0x001E, (uint16_t)(r_t_46 + 1), 0, 1};
}

