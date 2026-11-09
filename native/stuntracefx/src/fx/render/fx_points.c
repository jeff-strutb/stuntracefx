/*
 * fx_points.c: the Super FX code of asm/stuntracefx/render/fx_points.asm, written from it by
 * the generator.
 *
 * fx_points.asm: the Super FX unpacks a model's points from a byte script in
 * ROM into the list at $70:2488, three words (x, y, z) a point.
 *
 * The script is read through the ROM buffer. Commands: 4, a count, then
 * that many points as three signed bytes; $38, a count, then that many
 * points each stored twice, as (x, y, z) and (-x, y, z); $20, a word
 * added to R14 (a jump, counted from the word's second byte); $1C, a skipped
 * byte, then a jump through a table of such words indexed by the word at
 * $0044. Anything else ends the script.
 * C: src/stuntracefx/render/fx_points.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:A154 FxModelPoints
 * WHAT IT DOES: Unpacks the point script at R9 (in ROMBR's bank) into the
 * list at $2488, as described above.
 * HOW: R14 takes the script and R9 the list; each command byte is GETBS
 * and compared with 4, $38, $1C and $20 in turn. Points go out with STW
 * under LOOP (the last INC R9 in its delay slot); the jumps add to R14.
 * R3, R1 and R2 are kept on the R10 stack. The routine runs under CACHE
 * (the code cache only; nothing the C has to do).
 * Entry: R9 = the script; R10 = stack; R11 = return address. Exit: R9 =
 * just past the last point; R14 = just past the ending byte; R0 = that
 * byte (sign-extended); R1, R2, R3 as on entry; R12, R13 from the last
 * count; S, Z from R10, OV, CY from the compare with $20; the last RAM
 * address R10.
 */
void FxModelPoints(uint16_t script, uint16_t *stack)
{
    uint16_t count;
    uint16_t rom_at;
    uint16_t count_2;
    uint16_t rom_at_2;
    uint16_t point_a_x;
    uint16_t point_a_y;
    uint16_t point_a_z;
    uint16_t point_b_x;
    uint16_t point_b_y;
    uint16_t point_b_z;
    uint16_t out_ptr_a1;
    uint16_t out_ptr_a2;
    uint16_t out_ptr_b1;
    uint16_t out_ptr_b2;
    uint16_t out_ptr_b3;
    uint16_t out_ptr_b4;
    uint16_t out_ptr_b5;
    uint16_t jump_offset;
    uint16_t command_byte;
    uint16_t point_out_ptr;
    uint16_t script_ptr_a1;
    uint16_t script_ptr_a2;
    uint16_t script_ptr_b1;
    uint16_t script_ptr_b2;
    uint16_t variant_index;
    uint16_t jump_offset_lo;
    uint16_t jump_table_ptr;
    uint16_t jump_offset_hi_ptr;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    rom_at = script;
    fx_r14(rom_at);
    point_out_ptr = FX_POINTS_W;
    for (;;) {
        command_byte = (uint16_t)(int16_t)(int8_t)fx_getb();
        rom_at_2 = (uint16_t)(rom_at + 1);
        fx_r14(rom_at_2);
        if (command_byte == 4) {
            count = fx_getb();
            rom_at = (uint16_t)(rom_at_2 + 1);
            fx_r14(rom_at);
            for (;;) {
                point_a_x = (uint16_t)(int16_t)(int8_t)fx_getb();
                script_ptr_a1 = (uint16_t)(rom_at + 1);
                fx_r14(script_ptr_a1);
                fx_stw(point_out_ptr, point_a_x);
                out_ptr_a1 = (uint16_t)(point_out_ptr + 2);
                point_a_y = (uint16_t)(int16_t)(int8_t)fx_getb();
                script_ptr_a2 = (uint16_t)(script_ptr_a1 + 1);
                fx_r14(script_ptr_a2);
                fx_stw(out_ptr_a1, point_a_y);
                out_ptr_a2 = (uint16_t)(out_ptr_a1 + 2);
                point_a_z = (uint16_t)(int16_t)(int8_t)fx_getb();
                rom_at = (uint16_t)(script_ptr_a2 + 1);
                fx_r14(rom_at);
                fx_stw(out_ptr_a2, point_a_z);
                count = (uint16_t)(count - 1);
                point_out_ptr = (uint16_t)(out_ptr_a2 + 2);
                if (count != 0) {
                    continue;
                } else {
                    break;
                }
            }
            continue;
        } else {
            if (command_byte == 0x0038) {
                count_2 = fx_getb();
                rom_at = (uint16_t)(rom_at_2 + 1);
                fx_r14(rom_at);
                for (;;) {
                    point_b_x = (uint16_t)(int16_t)(int8_t)fx_getb();
                    script_ptr_b1 = (uint16_t)(rom_at + 1);
                    fx_r14(script_ptr_b1);
                    fx_stw(point_out_ptr, point_b_x);
                    out_ptr_b1 = (uint16_t)(point_out_ptr + 2);
                    point_b_y = (uint16_t)(int16_t)(int8_t)fx_getb();
                    script_ptr_b2 = (uint16_t)(script_ptr_b1 + 1);
                    fx_r14(script_ptr_b2);
                    fx_stw(out_ptr_b1, point_b_y);
                    out_ptr_b2 = (uint16_t)(out_ptr_b1 + 2);
                    point_b_z = (uint16_t)(int16_t)(int8_t)fx_getb();
                    rom_at = (uint16_t)(script_ptr_b2 + 1);
                    fx_r14(rom_at);
                    fx_stw(out_ptr_b2, point_b_z);
                    out_ptr_b3 = (uint16_t)(out_ptr_b2 + 2);
                    fx_stw(out_ptr_b3, (uint16_t)((uint16_t)~point_b_x + 1));
                    out_ptr_b4 = (uint16_t)(out_ptr_b3 + 2);
                    fx_stw(out_ptr_b4, point_b_y);
                    out_ptr_b5 = (uint16_t)(out_ptr_b4 + 2);
                    fx_stw(out_ptr_b5, point_b_z);
                    count_2 = (uint16_t)(count_2 - 1);
                    point_out_ptr = (uint16_t)(out_ptr_b5 + 2);
                    if (count_2 != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                continue;
            } else {
                if (command_byte == 0x001C) {
                    jump_table_ptr = (uint16_t)(rom_at_2 + 1);
                    fx_r14(jump_table_ptr);
                    variant_index = fx_ldw(0x0044);
                    rom_at_2 = (uint16_t)((uint16_t)(jump_table_ptr + variant_index) + variant_index);
                    fx_r14(rom_at_2);
                    jump_offset_lo = fx_getb();
                    jump_offset_hi_ptr = (uint16_t)(rom_at_2 + 1);
                    fx_r14(jump_offset_hi_ptr);
                    jump_offset = (uint16_t)((jump_offset_lo & 0xFF) | fx_getb() << 8);
                    rom_at = (uint16_t)(jump_offset_hi_ptr + jump_offset);
                    fx_r14(rom_at);
                    continue;
                } else {
                    if (command_byte == 0x0020) {
                        jump_offset_lo = fx_getb();
                        jump_offset_hi_ptr = (uint16_t)(rom_at_2 + 1);
                        fx_r14(jump_offset_hi_ptr);
                        jump_offset = (uint16_t)((jump_offset_lo & 0xFF) | fx_getb() << 8);
                        rom_at = (uint16_t)(jump_offset_hi_ptr + jump_offset);
                        fx_r14(rom_at);
                        continue;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return;
}

