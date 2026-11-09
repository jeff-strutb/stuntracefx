/*
 * points.c: the Super FX code of asm/stuntracefx/render/points.asm, written from it by
 * the generator.
 *
 * points.asm: the Super FX's point records at $0826, ten bytes each: two
 * words at +0 and +2, then a point's X, Y, Z at +4, +6, +8. The points are
 * taken from the table at $0340 (three words each), picked by index bytes
 * in ROM and moved by the vector at $0026.
 * C: src/stuntracefx/render/points.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:C1BE FxPointsGather
 * WHAT IT DOES: Fills R9 point records: the halved word pairs of the
 * 8-byte entries at the word at $005A (less 4), and the points named by R9
 * index bytes at R14, each moved by the vector at $0026.
 * HOW: CACHE. A LOOP of R9 passes from the last entry back: two LDWs from
 * entry i, DIV2 of each, STW to record i at +0 and +2 (records 1 to R9).
 * Then a LOOP of R9 passes forward: GETB, INC R14, UMULT #6 picks a point
 * at $0340; its three words plus the vector go to record k at +4, +6, +8
 * (records 0 to R9 - 1). R9 = 0 runs each loop $10000 times.
 * Entry: R9 = the count; R14 = the first index byte, in the ROM bank;
 * R11 = return address. Exit: R14 past the last index byte; R2 = $082A
 * plus ten per record; R1 = the last point's Z address; R0 = the last Z
 * stored; R3 = record 1's first word as stored; R5, R6, R7 = the
 * vector; R12 = 0; R13 = $C1FD (the second loop); the last RAM address
 * the last Z's; OV and CY from the last ADD #6 to R2; Z set, S clear.
 * R14 and the ROM buffer move as on the Super FX.
 */
FxPointsGatherResult FxPointsGather(uint16_t count, uint16_t rom_at)
{
    uint16_t entry;
    uint16_t point;
    uint16_t record;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t point_x;
    uint16_t point_y;
    uint16_t point_z;
    uint16_t vector_x;
    uint16_t vector_y;
    uint16_t dst_y_ptr;
    uint16_t dst_z_ptr;
    uint16_t vector_3_2;
    uint16_t point_index;
    uint16_t point_y_ptr;
    uint16_t entry_word_a;
    uint16_t entry_word_b;
    uint16_t point_dst_ptr;
    uint16_t entry_table_ptr;
    uint16_t entry_word_b_ptr;
    uint16_t record_word_b_ptr;

    entry_table_ptr = fx_ldw(0x005A);
    entry = (uint16_t)((uint16_t)((uint16_t)((uint8_t)count * (uint8_t)8) + entry_table_ptr) - 4);
    record = (uint16_t)((uint16_t)((uint8_t)count * (uint8_t)0x000A) + 0x0826);
    count_2 = count;
    for (;;) {
        entry_word_a = fx_ldw(entry);
        entry_word_b_ptr = (uint16_t)(entry + 2);
        entry_word_b = fx_ldw(entry_word_b_ptr);
        fx_stw(record, fx_div2(entry_word_a));
        record_word_b_ptr = (uint16_t)(record + 2);
        fx_stw(record_word_b_ptr, fx_div2(entry_word_b));
        entry = (uint16_t)(entry_word_b_ptr - 0x000A);
        record = (uint16_t)(record_word_b_ptr - 0x000C);
        count_2 = (uint16_t)(count_2 - 1);
        if (count_2 != 0) {
            continue;
        } else {
            break;
        }
    }
    vector_x = fx_ldw(0x0026);
    vector_y = fx_ldw(0x0028);
    vector_3_2 = fx_ldw(0x002A);
    point_dst_ptr = 0x082A;
    count_3 = count;
    for (;;) {
        point_index = fx_getb();
        rom_at = (uint16_t)(rom_at + 1);
        fx_r14(rom_at);
        point = (uint16_t)((uint16_t)((uint8_t)point_index * (uint8_t)6) + 0x0340);
        point_x = fx_ldw(point);
        point_y_ptr = (uint16_t)(point + 2);
        fx_stw(point_dst_ptr, (uint16_t)(point_x + vector_x));
        dst_y_ptr = (uint16_t)(point_dst_ptr + 2);
        point_y = fx_ldw(point_y_ptr);
        fx_stw(dst_y_ptr, (uint16_t)(point_y + vector_y));
        dst_z_ptr = (uint16_t)(dst_y_ptr + 2);
        point_z = fx_ldw((uint16_t)(point_y_ptr + 2));
        fx_stw(dst_z_ptr, (uint16_t)(point_z + vector_3_2));
        point_dst_ptr = (uint16_t)(dst_z_ptr + 6);
        count_3 = (uint16_t)(count_3 - 1);
        if (count_3 != 0) {
            continue;
        } else {
            break;
        }
    }
    return (FxPointsGatherResult){vector_3_2, rom_at};
}

