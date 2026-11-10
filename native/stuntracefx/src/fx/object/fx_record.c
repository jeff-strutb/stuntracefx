/*
 * fx_record.c: the Super FX code of asm/stuntracefx/object/fx_record.asm, written from it by
 * the generator.
 *
 * fx_record.asm: small Super FX routines on object records in Super FX RAM.
 *
 * R8 or R9 is the record; records in a list are linked through +$0E (0 ends
 * the list). The push at $70:23A4 is five words: on (1) or off (0), an
 * amount, and a vector.
 * C: src/stuntracefx/object/fx_record.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:9683 FxPushSet
 * WHAT IT DOES: Sets the push to amount R2, on when R2 is not negative, with
 * the vector (0, $8100, 0).
 * HOW: SUB R0 and MOVES R2 for the sign, then five SMs in field order with
 * R2 reused for the zeros and $8100.
 * Entry: R2 = amount; R11 = return address. Exit: R0 = 1 or 0; R2 = $8100;
 * OV, S, Z from R2's sign test, CY from the SUB (set); the last RAM
 * address $23AA.
 */
uint16_t FxPushSet(uint16_t amount)
{
    uint16_t push;

    push = 0;
    if (amount & 0x8000) {
    } else {
        push = 1;
    }
    fx_stw(FX_PUSH, push);
    fx_stw(0x23A6, amount);
    fx_stw(0x23A8, 0);
    fx_stw(0x23AC, 0);
    fx_stw(0x23AA, 0x8100);
    return push;
}


/*
 * @routine 04:A441 FxListClearBit0
 * WHAT IT DOES: Clears bit 0 of +$10 in every record of the list whose head
 * is at +$18 of record R6.
 * HOW: LDW, BIC R12 (= 1) and SBK per record, then the link at +$0E with
 * MOVES until it is 0.
 * Entry: R6 = the record holding the list; R11 = return address. Exit: R8 =
 * 0; with records: R12 = 1, R0 = 0; OV, S clear, Z set; the last RAM
 * address the last record's +$0E (else R6 + $18).
 */
void FxListClearBit0(uint16_t record_holding)
{
    uint16_t list;
    uint16_t next;
    uint16_t flags10;
    uint16_t list_item_ptr;

    list = fx_ldw((uint16_t)(FXREC_LIST + record_holding));
    list_item_ptr = list;
    if (list == 0) {
    } else {
        for (;;) {
            flags10 = fx_ldw((uint16_t)(FXREC_FLAGS10 + list_item_ptr));
            fx_sbk((uint16_t)(flags10 & ~1));
            next = fx_ldw((uint16_t)(FXREC_NEXT + list_item_ptr));
            list_item_ptr = next;
            if (next != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    return;
}


/*
 * @routine 04:BC7B FxRecordHoldY
 * WHAT IT DOES: Keeps record R8's +$60 as its height +$20 while bit 8 of
 * +$40 is clear; while it is set, +$60 holds, and becomes 0 once it is
 * below +$20.
 * HOW: AND #$0100 of +$40; the copy is an LDW and STW; "below" is the sign
 * of +$60 - +$20 (CMP); SUB R0 and SBK clear it.
 * Entry: R8 = the record; R11 = return address. Exit: R1 = +$20; R0 = +$60
 * (0 when cleared); OV, S, CY, Z from the CMP or the SUB; the last RAM
 * address R8 + $60.
 */
void FxRecordHoldY(uint16_t record)
{
    uint16_t y;
    uint16_t y_2;
    uint16_t y_held;
    uint16_t flags40;

    flags40 = fx_ldw((uint16_t)(FXREC_FLAGS40 + record));
    if ((flags40 & 0x0100) != 0) {
    } else {
        y = fx_ldw((uint16_t)(FXREC_Y + record));
        fx_stw((uint16_t)(FXREC_Y_HELD + record), y);
    }
    y_2 = fx_ldw((uint16_t)(FXREC_Y + record));
    y_held = fx_ldw((uint16_t)(FXREC_Y_HELD + record));
    if (!((uint16_t)(y_held - y_2) & 0x8000)) {
    } else {
        fx_sbk(0);
    }
    return;
}


/*
 * @routine 04:CBE8 FxBearingTo
 * WHAT IT DOES: The angle from record R8's position (+$1E, +$22) to the
 * point (R1, R3), negated: FxAngleOf of the difference, then NOT and INC.
 * HOW: The differences replace R1 and R3 (SUB under WITH), R2 = the z
 * difference for FxAngleOf; the return address is kept on the R10 stack.
 * Entry: R1 = x, R3 = z; R8 = the record; R10 = stack; R11 = return
 * address. Exit: R0 = the angle; R1 = the x difference; R2 = R3 = the z
 * difference; R4-R6, R11-R14 as FxAngleOf leaves them (R11 = $CBFF when it
 * did not divide); S, Z from R10; the last RAM address R10.
 */
FxBearingToResult FxBearingTo(uint16_t x, uint16_t z, uint16_t swap_mark, uint16_t record, uint16_t *stack, uint16_t rom_at)
{
    uint16_t half_x;
    uint16_t angle_2;
    uint16_t record_z;
    uint16_t xor_negative;
    uint16_t swap_marker_out;

    fx_spill(stack);
    half_x = fx_ldw((uint16_t)(FXREC_HALF_X + record));
    record_z = fx_ldw((uint16_t)(record + 0x0022));
    { FxAngleOfResult r_ = FxAngleOf((uint16_t)(x - half_x), (uint16_t)(z - record_z), swap_mark, stack, rom_at); angle_2 = r_.angle; xor_negative = r_.xor_negative; swap_marker_out = r_.swap_marker_out; };
    fx_unspill(stack);
    return (FxBearingToResult){(uint16_t)((uint16_t)~angle_2 + 1), xor_negative, swap_marker_out};
}


/*
 * @routine 04:BCBA FxRecordAlignment
 * WHAT IT DOES: Sets record R8's +$6C to how closely its velocity points
 * along the third axis of the matrix at $00E4 (the column at $00F0): the
 * dot product of the unit velocity with it, $7FFF for straight along; or
 * $7FFF when its speed +$4A is below 10 either way.
 * HOW: FxNormalize of the velocity (+$2E, +$30, +$32), three LMULTs summed
 * as 32 bits and the result doubled (ADD, ROL) to its high word; R8 and the
 * return address are kept on the R10 stack.
 * Entry: R8 = the record; R10 = stack; R11 = return address. Exit: R1 =
 * the value stored; R0 = R8 + $6C; R2 = 10; R3, R9, R12, R13 as
 * FxNormalize leaves them; R4, R5 = the last product's low and high
 * words; R6 = the word at $00F4; R7 = the low word of the sum, doubled; R11
 * = $C0E3 ($BCD9 for a zero velocity); S, Z from R10, OV, CY from the add
 * for +$6C; the last RAM address R10.
 */
FxRecordAlignmentResult FxRecordAlignment(uint16_t pose_aux, uint16_t record, uint16_t normalize_work_reg, uint16_t *stack)
{
    uint16_t k;
    uint16_t align;
    uint16_t speed;
    uint16_t vector_y;
    uint16_t vector_z;
    uint16_t velocity;
    uint16_t abs_speed;
    uint16_t dot_x_low;
    uint16_t dot_y_low;
    uint32_t dot_x_prod;
    uint32_t dot_y_prod;
    uint16_t dot_z_high;
    uint32_t dot_z_prod;
    uint16_t dot_low_sum;
    uint16_t dot_xy_low_sum;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_y;
    uint16_t scaled_vector_z;
    uint16_t last_product_hi_2;
    uint16_t dot_low_sum_doubled;
    uint16_t restored_record_ptr;

    fx_spill(stack);
    fx_push(stack, record);
    velocity = fx_ldw((uint16_t)(FXREC_VELOCITY + record));
    vector_y = fx_ldw((uint16_t)(record + 0x0030));
    vector_z = fx_ldw((uint16_t)(record + 0x0032));
    { FxNormalizeResult r_ = FxNormalize(velocity, vector_y, vector_z, pose_aux, normalize_work_reg, stack); scaled_vector_x = r_.scaled_vector_x; scaled_vector_y = r_.scaled_vector_y; scaled_vector_z = r_.scaled_vector_z; k = r_.k; };
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    dot_x_prod = (uint32_t)((int32_t)(int16_t)scaled_vector_x * (int16_t)object_matrix_6);
    dot_x_low = (uint16_t)dot_x_prod;
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    dot_y_prod = (uint32_t)((int32_t)(int16_t)scaled_vector_y * (int16_t)object_matrix_7);
    dot_y_low = (uint16_t)dot_y_prod;
    dot_xy_low_sum = (uint16_t)(dot_x_low + dot_y_low);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    dot_z_prod = (uint32_t)((int32_t)(int16_t)scaled_vector_z * (int16_t)object_matrix_8);
    dot_z_high = (uint16_t)(dot_z_prod >> 16);
    last_product_hi_2 = (uint16_t)dot_z_prod;
    dot_low_sum = (uint16_t)(dot_xy_low_sum + last_product_hi_2);
    dot_low_sum_doubled = (uint16_t)(dot_low_sum + dot_low_sum);
    align = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(dot_x_prod >> 16) + (uint16_t)(dot_y_prod >> 16) + ((uint32_t)dot_x_low + dot_y_low > 0xFFFF)) + dot_z_high + ((uint32_t)dot_xy_low_sum + last_product_hi_2 > 0xFFFF)) << 1 | ((uint32_t)dot_low_sum + dot_low_sum > 0xFFFF));
    restored_record_ptr = fx_pull(stack);
    speed = fx_ldw((uint16_t)(FXREC_SPEED + restored_record_ptr));
    abs_speed = speed;
    if (!(speed & 0x8000)) {
    } else {
        abs_speed = (uint16_t)((uint16_t)~abs_speed + 1);
    }
    if (!((uint16_t)(abs_speed - 0x000A) & 0x8000)) {
    } else {
        align = 0x7FFF;
    }
    fx_stw((uint16_t)(FXREC_ALIGN + restored_record_ptr), align);
    fx_unspill(stack);
    return (FxRecordAlignmentResult){last_product_hi_2, dot_z_high, dot_low_sum_doubled, k};
}


/*
 * @routine 04:BF51 FxRecordRaise6A
 * WHAT IT DOES: A program: raises +$6A of the record whose address the
 * S-CPU left at $238E by the amount at $2390 times $300 / 65536, to at most
 * $0180.
 * HOW: ROMB, RAMB 0; FMULT of $0300 (R2 doubled) by the amount, ADD, and
 * the cap by CMP and the sign; SBK stores to the field's address, the last
 * loaded. The S-CPU side is GsuRecordBoostRaise (object/fx_record.asm).
 * Entry: the S-CPU starts it. Exit: (at its STOP) R0 = the new +$6A; R1 =
 * the old; R2 = $0180; R6 = the amount; R8 = the record; ROMBR = RAMBR = 0;
 * OV, S, CY, Z from the CMP; the last RAM address R8 + $6A.
 */
void FxRecordRaise6A(void)
{
    uint16_t amount;
    uint16_t fxrec_6a;
    uint16_t record_arg;
    uint16_t raised_6a_value;

    fx_romb(0);
    fx_ramb(0);
    record_arg = fx_ldw(GSU_RECORD_ARG);
    amount = fx_ldw(GSU_IN_AMOUNT);
    fxrec_6a = fx_ldw((uint16_t)(FXREC_6A + record_arg));
    raised_6a_value = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(0 + 0x0300) * (int16_t)amount) >> 16) + fxrec_6a);
    if ((uint16_t)(raised_6a_value - 0x0180) & 0x8000) {
    } else {
        raised_6a_value = 0x0180;
    }
    fx_sbk(raised_6a_value);
    return;
}


/*
 * @routine 04:BF75 FxRecordLower82
 * WHAT IT DOES: A program: lowers +$82 of the record whose address the
 * S-CPU left at $238E by the amount at $2390 times $400 / 65536, to no less
 * than 0.
 * HOW: ROMB, RAMB 0; FMULT of $0400 (R2 doubled) by the amount, SUB from
 * the field, and SUB R0 for 0 when negative; SBK stores to the field's
 * address, the last loaded. The S-CPU side is GsuRecordRepair
 * (object/fx_record.asm).
 * Entry: the S-CPU starts it. Exit: (at its STOP) R0 = the new +$82; R1 =
 * the old; R2 = $0200; R6 = the amount; R8 = the record; ROMBR = RAMBR = 0;
 * OV, S, CY, Z from the last SUB; the last RAM address R8 + $82.
 */
void FxRecordLower82(void)
{
    uint16_t amount;
    uint16_t fxrec_82;
    uint16_t record_arg;
    uint16_t new_value_82;
    uint16_t lowered_value_82;

    fx_romb(0);
    fx_ramb(0);
    record_arg = fx_ldw(GSU_RECORD_ARG);
    amount = fx_ldw(GSU_IN_AMOUNT);
    fxrec_82 = fx_ldw((uint16_t)(FXREC_82 + record_arg));
    lowered_value_82 = (uint16_t)(fxrec_82 - (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(0 + 0x0400) * (int16_t)amount) >> 16));
    new_value_82 = lowered_value_82;
    if (!(lowered_value_82 & 0x8000)) {
    } else {
        new_value_82 = 0;
    }
    fx_sbk(new_value_82);
    return;
}


/*
 * @routine 01:BBC7 FxTimersProgram
 * WHAT IT DOES: The Super FX program that steps the 20 timed records at
 * $273E (14 bytes each): a record whose timer byte (+0) is set counts it
 * down; while its flags (+1) have bit 4 and the timer is below $20, +2
 * takes entry (timer) of the byte table at $14:D0BC (ROM); then a record
 * with bit 0 of +1 has it cleared, any other ends: its timer is cleared
 * and its word +$0C set to 0 (also for a record whose timer was already
 * 0).
 * HOW: ROM bank $14, RAM bank 0; CACHE and LOOP over R12 = 20 records, R9
 * stepping by R8 = 14; WITH then TO is a MOVE; the BNE's delay slot
 * clears R0 either way.
 * Entry: started by the S-CPU. Exit: (at the STOP) R9 past the records;
 * R12 = 0; R10 = $0240; the other registers as the last record left them.
 */
void FxTimersProgram(void)
{
    uint16_t count;
    uint16_t slot_ptr;
    uint16_t slot_life;
    uint16_t frame_byte;
    uint16_t slot_flags;
    uint16_t slot_life_byte;
    uint16_t slot_zero_word;
    uint16_t slot_life_after;
    uint16_t slot_flags_again;

    fx_romb(0x0014);
    fx_ramb(0);
    slot_ptr = 0x273E;
    count = 0x0014;
    for (;;) {
        slot_life_byte = fx_ldb(slot_ptr);
        slot_life = (uint16_t)(slot_life_byte & 0xFF);
        slot_zero_word = slot_life;
        if (slot_life == 0) {
            fx_stw((uint16_t)(slot_ptr + 0x000C), slot_zero_word);
            slot_ptr = (uint16_t)(slot_ptr + 0x000E);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            slot_life_after = (uint16_t)(slot_zero_word - 1);
            fx_stb(slot_ptr, (uint8_t)slot_life_after);
            slot_flags = fx_ldb((uint16_t)(slot_ptr + 1));
            if ((slot_flags & 0x0010) == 0) {
            } else {
                if (!((uint16_t)(slot_life_after - 0x0020) & 0x8000)) {
                } else {
                    fx_r14(0xD0BC);
                    fx_r14((uint16_t)(slot_life_after - 0x2F44));
                    frame_byte = fx_getb();
                    fx_stb((uint16_t)(slot_ptr + 2), (uint8_t)frame_byte);
                }
            }
            slot_flags_again = fx_ldb((uint16_t)(slot_ptr + 1));
            slot_zero_word = 0;
            if ((slot_flags_again & 1) != 0) {
                fx_stb((uint16_t)(slot_ptr + 1), (uint8_t)(uint16_t)(slot_flags_again & ~1));
                slot_ptr = (uint16_t)(slot_ptr + 0x000E);
                count = (uint16_t)(count - 1);
                if (count != 0) {
                    continue;
                } else {
                    break;
                }
            } else {
                fx_stb(slot_ptr, (uint8_t)slot_zero_word);
                fx_stw((uint16_t)(slot_ptr + 0x000C), slot_zero_word);
                slot_ptr = (uint16_t)(slot_ptr + 0x000E);
                count = (uint16_t)(count - 1);
                if (count != 0) {
                    continue;
                } else {
                    break;
                }
            }
        }
    }
    return;
}


/*
 * @routine 01:BA13 FxParticlesDraw
 * WHAT IT DOES: Draws the particles of key R2: for each of the 20 records
 * at $273E (14 bytes) whose word +$0C is R2, moves it by its signed deltas
 * (+3, +4, +5 into the words +6, +8, +$0A), sets bit 0 of its flags (+1)
 * and, with flag bit 1, steps +4; then, when its z (plus $2A) is from
 * $100 up, projects it (the scale for z from the table at $14:896C,
 * FMULT, plus the centre $34/$36), and inside the bounds ($3A, $3E) plots
 * it in colour +2: a 2 x 2 dot, or with flag bit 2 a line from where it
 * was (projected the same way).
 * HOW: ROM bank $14; the view offsets at $26/$28/$2A; R12 counts the
 * records and is kept at $40 while a line is drawn; plot options 0 for
 * the colour, then 8. The line is not reached in the traces so far.
 * Entry: R2 = the key; R10 = stack; R11 = return address. Exit: R7 = the
 * key; R12 = 0; R11 past the last record + 12; the plot registers as the
 * last particle left them; S, Z from R10; the last RAM address R10.
 */
uint16_t FxParticlesDraw(uint16_t key, uint16_t *stack)
{
    int run_lBAB9 = 0;
    int run_lBB3D = 0;
    uint16_t pool;
    uint16_t count;
    uint16_t depth;
    uint16_t rel_x;
    uint16_t rel_y;
    uint16_t rel_z;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t line_dx;
    uint16_t line_dy;
    uint16_t all_next;
    uint16_t all_prev;
    uint16_t fxcar_0a;
    uint16_t recip_lo;
    uint16_t screen_x;
    uint16_t screen_y;
    uint16_t recip_ptr;
    uint16_t all_next_2;
    uint16_t dot_left_x;
    uint16_t dot_next_y;
    uint16_t particle_x;
    uint16_t particle_y;
    uint16_t particle_z;
    uint16_t slots_left;
    uint16_t tail_depth;
    uint16_t tail_rel_x;
    uint16_t tail_rel_y;
    uint16_t tail_scale;
    uint16_t dot_right_x;
    uint16_t dx_minus_dy;
    uint16_t line_abs_dx;
    uint16_t line_abs_dy;
    uint16_t line_x_step;
    uint16_t line_y_step;
    uint16_t delta_x_byte;
    uint16_t delta_z_byte;
    uint16_t particle_key;
    uint16_t particle_ptr;
    uint16_t tail_x_limit;
    uint16_t tail_y_limit;
    uint16_t tail_center_x;
    uint16_t tail_center_y;
    uint16_t tail_recip_lo;
    uint16_t tail_screen_x;
    uint16_t tail_screen_y;
    uint16_t view_offset_x;
    uint16_t view_offset_y;
    uint16_t view_offset_z;
    uint16_t x_major_error;
    uint16_t y_major_error;
    uint16_t particle_color;
    uint16_t particle_flags;
    uint16_t screen_x_limit;
    uint16_t screen_y_limit;
    uint16_t tail_recip_ptr;
    uint16_t x_major_next_x;
    uint16_t y_major_next_x;
    uint16_t depth_below_min;
    uint16_t screen_center_x;
    uint16_t screen_center_y;
    uint16_t particle_key_ptr;
    uint16_t x_major_half_gap;
    uint16_t y_major_half_gap;
    uint16_t perspective_scale;
    uint16_t tail_view_offset_x;
    uint16_t tail_view_offset_y;
    uint16_t tail_view_offset_z;
    uint16_t particle_draw_flags;
    uint16_t tail_depth_below_min;
    int x_major_error_settled;
    int y_major_error_settled;

    fx_spill(stack);
    fx_romb(0x0014);
    particle_key_ptr = 0x274A;
    count = 0x0014;
    for (;;) {
        particle_key = fx_ldw(particle_key_ptr);
        if (particle_key == key) {
            particle_ptr = (uint16_t)(particle_key_ptr - 0x000C);
            delta_x_byte = fx_ldb((uint16_t)(particle_ptr + 3));
            all_prev = fx_ldw((uint16_t)(particle_ptr + FXREC_ALL_PREV));
            particle_x = (uint16_t)((uint16_t)(int16_t)(int8_t)delta_x_byte + all_prev);
            fx_sbk(particle_x);
            view_offset_x = fx_ldw(0x0026);
            rel_x = (uint16_t)(particle_x + view_offset_x);
            all_next = fx_ldb((uint16_t)(particle_ptr + FXREC_ALL_NEXT));
            pool = fx_ldw((uint16_t)(particle_ptr + FXREC_POOL));
            particle_y = (uint16_t)((uint16_t)(int16_t)(int8_t)all_next + pool);
            fx_sbk(particle_y);
            view_offset_y = fx_ldw(0x0028);
            rel_y = (uint16_t)(particle_y + view_offset_y);
            delta_z_byte = fx_ldb((uint16_t)(particle_ptr + 5));
            fxcar_0a = fx_ldw((uint16_t)(particle_ptr + FXCAR_0A));
            particle_z = (uint16_t)((uint16_t)(int16_t)(int8_t)delta_z_byte + fxcar_0a);
            fx_sbk(particle_z);
            view_offset_z = fx_ldw(0x002A);
            rel_z = (uint16_t)(particle_z + view_offset_z);
            particle_flags = fx_ldb((uint16_t)(particle_ptr + 1));
            fx_stb((uint16_t)(particle_ptr + 1), (uint8_t)(particle_flags | 1));
            if ((particle_flags & 2) == 0) {
            } else {
                all_next_2 = fx_ldb((uint16_t)(particle_ptr + FXREC_ALL_NEXT));
                fx_stb((uint16_t)(particle_ptr + FXREC_ALL_NEXT), (uint8_t)(uint16_t)(all_next_2 + 1));
            }
            depth = rel_z;
            if (rel_z & 0x8000) {
            } else {
                if (!((uint16_t)((uint16_t)(0 - depth) + 0x4400) & 0x8000)) {
                } else {
                    depth = 0x43FF;
                }
                depth_below_min = (uint16_t)((uint16_t)(0 - depth) + 0x0100);
                if (depth_below_min & 0x8000) {
                    run_lBAB9 = 1;
                } else {
                    if (depth_below_min == 0) {
                        run_lBAB9 = 1;
                    } else {
                    }
                }
                if (run_lBAB9) {
                    run_lBAB9 = 0;
                    recip_ptr = (uint16_t)((uint16_t)(depth & ~1) - 0x7694);
                    fx_r14(recip_ptr);
                    recip_lo = fx_getb();
                    fx_r14((uint16_t)(recip_ptr + 1));
                    perspective_scale = (uint16_t)((recip_lo & 0xFF) | fx_getb() << 8);
                    screen_center_x = fx_ldw(0x0034);
                    screen_x = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)rel_x * (int16_t)perspective_scale) >> 16) + screen_center_x);
                    screen_x_limit = fx_ldw(0x003A);
                    if (screen_x < screen_x_limit) {
                        screen_center_y = fx_ldw(0x0036);
                        screen_y = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)rel_y * (int16_t)perspective_scale) >> 16) + screen_center_y);
                        screen_y_limit = fx_ldw(0x003E);
                        if (screen_y < screen_y_limit) {
                            fx_cmode(0);
                            particle_color = fx_ldb((uint16_t)(particle_ptr + 2));
                            fx_color((uint8_t)particle_color);
                            fx_cmode(8);
                            particle_draw_flags = fx_ldb((uint16_t)(particle_ptr + 1));
                            if ((particle_draw_flags & 4) != 0) {
                                tail_view_offset_x = fx_ldw(0x0026);
                                tail_rel_x = (uint16_t)(tail_view_offset_x + all_prev);
                                tail_view_offset_y = fx_ldw(0x0028);
                                tail_rel_y = (uint16_t)(tail_view_offset_y + pool);
                                tail_view_offset_z = fx_ldw(0x002A);
                                tail_depth = (uint16_t)(tail_view_offset_z + fxcar_0a);
                                if (!((uint16_t)((uint16_t)(0 - tail_depth) + 0x4400) & 0x8000)) {
                                } else {
                                    tail_depth = 0x43FF;
                                }
                                tail_depth_below_min = (uint16_t)((uint16_t)(0 - tail_depth) + 0x0100);
                                if (tail_depth_below_min & 0x8000) {
                                    run_lBB3D = 1;
                                } else {
                                    if (tail_depth_below_min == 0) {
                                        run_lBB3D = 1;
                                    } else {
                                    }
                                }
                                if (run_lBB3D) {
                                    run_lBB3D = 0;
                                    tail_recip_ptr = (uint16_t)((uint16_t)(tail_depth & ~1) - 0x7694);
                                    fx_r14(tail_recip_ptr);
                                    tail_recip_lo = fx_getb();
                                    fx_r14((uint16_t)(tail_recip_ptr + 1));
                                    tail_scale = (uint16_t)((tail_recip_lo & 0xFF) | fx_getb() << 8);
                                    tail_center_x = fx_ldw(0x0034);
                                    tail_screen_x = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)tail_rel_x * (int16_t)tail_scale) >> 16) + tail_center_x);
                                    tail_x_limit = fx_ldw(0x003A);
                                    if (tail_screen_x < tail_x_limit) {
                                        tail_center_y = fx_ldw(0x0036);
                                        tail_screen_y = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)tail_rel_y * (int16_t)tail_scale) >> 16) + tail_center_y);
                                        tail_y_limit = fx_ldw(0x003E);
                                        if (tail_screen_y < tail_y_limit) {
                                            fx_stw(0x0040, count);
                                            line_x_step = 1;
                                            line_dx = (uint16_t)(tail_screen_x - screen_x);
                                            line_abs_dx = line_dx;
                                            if (!(line_dx & 0x8000)) {
                                            } else {
                                                line_x_step = 0xFFFF;
                                                line_abs_dx = (uint16_t)((uint16_t)~line_abs_dx + 1);
                                            }
                                            line_y_step = 1;
                                            line_dy = (uint16_t)(tail_screen_y - screen_y);
                                            line_abs_dy = line_dy;
                                            if (!(line_dy & 0x8000)) {
                                            } else {
                                                line_y_step = 0xFFFF;
                                                line_abs_dy = (uint16_t)((uint16_t)~line_abs_dy + 1);
                                            }
                                            dx_minus_dy = (uint16_t)(line_abs_dx - line_abs_dy);
                                            if (dx_minus_dy & 0x8000) {
                                                count_2 = (uint16_t)(line_abs_dy + 1);
                                                y_major_half_gap = (uint16_t)((uint16_t)((uint16_t)~dx_minus_dy + 1) >> 1);
                                                y_major_error_settled = (y_major_half_gap >= line_abs_dx);
                                                y_major_error = (uint16_t)(y_major_half_gap - line_abs_dx);
                                                for (;;) {
                                                    fx_flat_row(screen_x, screen_y, 1);
                                                    y_major_next_x = (uint16_t)(screen_x + 1);
                                                    if (y_major_error_settled) {
                                                    } else {
                                                        y_major_error = (uint16_t)(y_major_error + line_abs_dy);
                                                        y_major_next_x = (uint16_t)(y_major_next_x + line_x_step);
                                                    }
                                                    screen_x = (uint16_t)(y_major_next_x - 1);
                                                    screen_y = (uint16_t)(screen_y + line_y_step);
                                                    count_2 = (uint16_t)(count_2 - 1);
                                                    y_major_error_settled = (y_major_error >= line_abs_dx);
                                                    y_major_error = (uint16_t)(y_major_error - line_abs_dx);
                                                    if (count_2 != 0) {
                                                        continue;
                                                    } else {
                                                        break;
                                                    }
                                                }
                                                count = fx_ldw(0x0040);
                                            } else {
                                                count_3 = (uint16_t)(line_abs_dx + 1);
                                                x_major_half_gap = (uint16_t)(dx_minus_dy >> 1);
                                                x_major_error_settled = (x_major_half_gap >= line_abs_dy);
                                                x_major_error = (uint16_t)(x_major_half_gap - line_abs_dy);
                                                for (;;) {
                                                    fx_flat_row(screen_x, screen_y, 1);
                                                    x_major_next_x = (uint16_t)(screen_x + 1);
                                                    if (x_major_error_settled) {
                                                    } else {
                                                        x_major_error = (uint16_t)(x_major_error + line_abs_dx);
                                                        screen_y = (uint16_t)(screen_y + line_y_step);
                                                    }
                                                    screen_x = (uint16_t)((uint16_t)(x_major_next_x + line_x_step) - 1);
                                                    count_3 = (uint16_t)(count_3 - 1);
                                                    x_major_error_settled = (x_major_error >= line_abs_dy);
                                                    x_major_error = (uint16_t)(x_major_error - line_abs_dy);
                                                    if (count_3 != 0) {
                                                        continue;
                                                    } else {
                                                        break;
                                                    }
                                                }
                                                count = fx_ldw(0x0040);
                                            }
                                        } else {
                                        }
                                    } else {
                                    }
                                }
                            } else {
                                fx_flat_row(screen_x, screen_y, 2); fx_flat_row(screen_x, (uint16_t)(screen_y + 1), 2);
                                dot_right_x = (uint16_t)(screen_x + 1);
                                dot_next_y = (uint16_t)(screen_y + 1);
                                dot_left_x = (uint16_t)(dot_right_x - 1);
                            }
                        } else {
                        }
                    } else {
                    }
                }
            }
            particle_key_ptr = (uint16_t)(particle_ptr + 0x000C);
        } else {
        }
        particle_key_ptr = (uint16_t)(particle_key_ptr + 0x000E);
        slots_left = (uint16_t)(count - 1);
        count = slots_left;
        if (slots_left != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_unspill(stack);
    return key;
}

