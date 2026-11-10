/*
 * table.c: the Super FX code of asm/stuntracefx/render/table.asm, written from it by
 * the generator.
 *
 * table.asm: the Super FX's side of the 8-byte entry table whose address is
 * the word at $70:1092 (the S-CPU's side is in gsu/params.asm).
 *
 * Entries are counted from 1; the words of an entry start 2 bytes into it.
 * C: src/stuntracefx/render/table.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:CD83 FxTableEntryWords
 * WHAT IT DOES: Returns the address of the words of entry R0 of the table
 * whose address is the word at $1092: the Super FX side of
 * TrackTableEntryPlace.
 * HOW: (entry - 1) * 8 with LMULT (R6 = 8; R4 takes the low word of the
 * product), then the table address + 2 + that.
 * Entry: R0 = entry; R11 = return address. Exit: R5 = the address; R0 and
 * R1 = the entry; R4 = (entry - 1) * 8; R6 = 8; the last RAM address $1092;
 * OV, S, CY, Z from the final add (table + 2, plus R4).
 */
FxTableEntryWordsResult FxTableEntryWords(uint16_t entry)
{
    uint16_t table_base;
    uint16_t entry_out_2;

    entry_out_2 = (uint16_t)(uint32_t)((int32_t)(int16_t)(uint16_t)(entry - 1) * (int16_t)8);
    table_base = fx_ldw(0x1092);
    return (FxTableEntryWordsResult){entry_out_2, (uint16_t)((uint16_t)(table_base + 2) + entry_out_2)};
}

