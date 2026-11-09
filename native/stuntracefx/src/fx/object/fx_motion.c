/*
 * fx_motion.c: the Super FX code of asm/stuntracefx/object/fx_motion.asm, written from it by
 * the generator.
 *
 * fx_motion.asm: the Super FX moves an object's record by its velocity.
 *
 * A record (R8) holds a velocity at +$2E/+$30/+$32 and the position it
 * moves at +$1E/+$20/+$22; an object of kind 6 (R1) also has a second pair,
 * a rate at +$34/+$36/+$38 for the three words at +$24/+$26/+$28. Bit 3 of
 * +$1C halves every step.
 * C: src/stuntracefx/object/fx_motion.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:95BE FxRecordMove
 * WHAT IT DOES: Adds record R8's velocity / 8 to its position (/ 16 when
 * bit 3 of +$1C is set); for kind 6 (R1) also adds the rate at +$34 to the
 * words at +$24 (halved when the bit is set).
 * HOW: R5 walks the rates and R2 the words; each word is LDW, DIV2s, LDW,
 * ADD, STW in that order. The halvings are DIV2 (rounding toward minus
 * infinity, -1 to 0) applied once per halving.
 * Entry: R1 = the object's kind; R8 = the record; R11 = return address.
 * Exit: R2, R5 just past the last word and rate; R3 = the last word before
 * the add; kind 6: R0 = the last new word, S, Z from R2, OV, CY from the
 * add; otherwise R0 = kind - 6 and the flags from that SUB. The last RAM
 * address the last word.
 */
uint16_t FxRecordMove(uint16_t object_kind, uint16_t record)
{
    uint16_t r_t_22;
    uint16_t r_t_43;
    uint16_t flags1c;
    uint16_t eighth_pos_x;
    uint16_t eighth_pos_y;
    uint16_t eighth_pos_z;
    uint16_t eighth_vel_x;
    uint16_t eighth_vel_y;
    uint16_t eighth_vel_z;
    uint16_t position_ptr;
    uint16_t velocity_ptr;
    uint16_t eighth_spin_x;
    uint16_t eighth_spin_y;
    uint16_t eighth_spin_z;
    uint16_t sixteenth_pos_x;
    uint16_t sixteenth_pos_y;
    uint16_t sixteenth_pos_z;
    uint16_t sixteenth_vel_x;
    uint16_t sixteenth_vel_y;
    uint16_t sixteenth_vel_z;
    uint16_t eighth_pos_y_ptr;
    uint16_t eighth_pos_z_ptr;
    uint16_t eighth_vel_y_ptr;
    uint16_t eighth_vel_z_ptr;
    uint16_t sixteenth_spin_x;
    uint16_t sixteenth_spin_y;
    uint16_t sixteenth_spin_z;
    uint16_t eighth_spin_y_ptr;
    uint16_t eighth_spin_dest_x;
    uint16_t eighth_spin_dest_y;
    uint16_t eighth_spin_dest_z;
    uint16_t sixteenth_pos_y_ptr;
    uint16_t sixteenth_pos_z_ptr;
    uint16_t sixteenth_vel_y_ptr;
    uint16_t sixteenth_vel_z_ptr;
    uint16_t next_entry_end_ptr_2;
    uint16_t sixteenth_spin_y_ptr;
    uint16_t sixteenth_spin_dest_x;
    uint16_t sixteenth_spin_dest_y;
    uint16_t sixteenth_spin_dest_z;
    uint16_t eighth_spin_dest_x_ptr;
    uint16_t eighth_spin_dest_y_ptr;
    uint16_t eighth_spin_dest_z_ptr;
    uint16_t sixteenth_spin_dest_x_ptr;
    uint16_t sixteenth_spin_dest_y_ptr;
    uint16_t sixteenth_spin_dest_z_ptr;

    velocity_ptr = (uint16_t)(FXREC_VELOCITY + record);
    position_ptr = (uint16_t)(FXREC_HALF_X + record);
    flags1c = fx_ldw((uint16_t)(FXREC_FLAGS1C + record));
    if ((8 & flags1c) == 0) {
        eighth_vel_x = fx_ldw(velocity_ptr);
        eighth_vel_y_ptr = (uint16_t)(velocity_ptr + 2);
        eighth_pos_x = fx_ldw(position_ptr);
        fx_stw(position_ptr, (uint16_t)(fx_div2(fx_div2(fx_div2(eighth_vel_x))) + eighth_pos_x));
        eighth_pos_y_ptr = (uint16_t)(position_ptr + 2);
        eighth_vel_y = fx_ldw(eighth_vel_y_ptr);
        eighth_vel_z_ptr = (uint16_t)(eighth_vel_y_ptr + 2);
        eighth_pos_y = fx_ldw(eighth_pos_y_ptr);
        fx_stw(eighth_pos_y_ptr, (uint16_t)(fx_div2(fx_div2(fx_div2(eighth_vel_y))) + eighth_pos_y));
        eighth_pos_z_ptr = (uint16_t)(eighth_pos_y_ptr + 2);
        eighth_vel_z = fx_ldw(eighth_vel_z_ptr);
        next_entry_end_ptr_2 = (uint16_t)(eighth_vel_z_ptr + 2);
        eighth_pos_z = fx_ldw(eighth_pos_z_ptr);
        fx_stw(eighth_pos_z_ptr, (uint16_t)(fx_div2(fx_div2(fx_div2(eighth_vel_z))) + eighth_pos_z));
        eighth_spin_dest_x_ptr = (uint16_t)(eighth_pos_z_ptr + 2);
        if (object_kind != 6) {
        } else {
            eighth_spin_x = fx_ldw(next_entry_end_ptr_2);
            eighth_spin_y_ptr = (uint16_t)(next_entry_end_ptr_2 + 2);
            eighth_spin_dest_x = fx_ldw(eighth_spin_dest_x_ptr);
            fx_stw(eighth_spin_dest_x_ptr, (uint16_t)(eighth_spin_x + eighth_spin_dest_x));
            eighth_spin_dest_y_ptr = (uint16_t)(eighth_spin_dest_x_ptr + 2);
            eighth_spin_y = fx_ldw(eighth_spin_y_ptr);
            r_t_22 = (uint16_t)(eighth_spin_y_ptr + 2);
            eighth_spin_dest_y = fx_ldw(eighth_spin_dest_y_ptr);
            fx_stw(eighth_spin_dest_y_ptr, (uint16_t)(eighth_spin_y + eighth_spin_dest_y));
            eighth_spin_dest_z_ptr = (uint16_t)(eighth_spin_dest_y_ptr + 2);
            eighth_spin_z = fx_ldw(r_t_22);
            next_entry_end_ptr_2 = (uint16_t)(r_t_22 + 2);
            eighth_spin_dest_z = fx_ldw(eighth_spin_dest_z_ptr);
            fx_stw(eighth_spin_dest_z_ptr, (uint16_t)(eighth_spin_z + eighth_spin_dest_z));
        }
    } else {
        sixteenth_vel_x = fx_ldw(velocity_ptr);
        sixteenth_vel_y_ptr = (uint16_t)(velocity_ptr + 2);
        sixteenth_pos_x = fx_ldw(position_ptr);
        fx_stw(position_ptr, (uint16_t)(fx_div2(fx_div2(fx_div2(fx_div2(sixteenth_vel_x)))) + sixteenth_pos_x));
        sixteenth_pos_y_ptr = (uint16_t)(position_ptr + 2);
        sixteenth_vel_y = fx_ldw(sixteenth_vel_y_ptr);
        sixteenth_vel_z_ptr = (uint16_t)(sixteenth_vel_y_ptr + 2);
        sixteenth_pos_y = fx_ldw(sixteenth_pos_y_ptr);
        fx_stw(sixteenth_pos_y_ptr, (uint16_t)(fx_div2(fx_div2(fx_div2(fx_div2(sixteenth_vel_y)))) + sixteenth_pos_y));
        sixteenth_pos_z_ptr = (uint16_t)(sixteenth_pos_y_ptr + 2);
        sixteenth_vel_z = fx_ldw(sixteenth_vel_z_ptr);
        next_entry_end_ptr_2 = (uint16_t)(sixteenth_vel_z_ptr + 2);
        sixteenth_pos_z = fx_ldw(sixteenth_pos_z_ptr);
        fx_stw(sixteenth_pos_z_ptr, (uint16_t)(fx_div2(fx_div2(fx_div2(fx_div2(sixteenth_vel_z)))) + sixteenth_pos_z));
        sixteenth_spin_dest_x_ptr = (uint16_t)(sixteenth_pos_z_ptr + 2);
        if (object_kind != 6) {
        } else {
            sixteenth_spin_x = fx_ldw(next_entry_end_ptr_2);
            sixteenth_spin_y_ptr = (uint16_t)(next_entry_end_ptr_2 + 2);
            sixteenth_spin_dest_x = fx_ldw(sixteenth_spin_dest_x_ptr);
            fx_stw(sixteenth_spin_dest_x_ptr, (uint16_t)(fx_div2(sixteenth_spin_x) + sixteenth_spin_dest_x));
            sixteenth_spin_dest_y_ptr = (uint16_t)(sixteenth_spin_dest_x_ptr + 2);
            sixteenth_spin_y = fx_ldw(sixteenth_spin_y_ptr);
            r_t_43 = (uint16_t)(sixteenth_spin_y_ptr + 2);
            sixteenth_spin_dest_y = fx_ldw(sixteenth_spin_dest_y_ptr);
            fx_stw(sixteenth_spin_dest_y_ptr, (uint16_t)(fx_div2(sixteenth_spin_y) + sixteenth_spin_dest_y));
            sixteenth_spin_dest_z_ptr = (uint16_t)(sixteenth_spin_dest_y_ptr + 2);
            sixteenth_spin_z = fx_ldw(r_t_43);
            next_entry_end_ptr_2 = (uint16_t)(r_t_43 + 2);
            sixteenth_spin_dest_z = fx_ldw(sixteenth_spin_dest_z_ptr);
            fx_stw(sixteenth_spin_dest_z_ptr, (uint16_t)(fx_div2(sixteenth_spin_z) + sixteenth_spin_dest_z));
        }
    }
    return next_entry_end_ptr_2;
}

