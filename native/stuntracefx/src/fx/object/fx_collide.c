/*
 * fx_collide.c: the Super FX code of asm/stuntracefx/object/fx_collide.asm, written from it by
 * the generator.
 *
 * fx_collide.asm: the Super FX's push between two touching objects.
 *
 * R9 is the record being pushed and R7 the other's record; R1 points at
 * the other's contact words: its velocity x, z at +$04, +$06 and position
 * x, z at +$08, +$0A. A record's +$2A is its mass. The result goes to the
 * push at $70:23A4 (object/fx_record.asm) as an amount along the unit vector
 * from the other's position to this one's.
 * C: src/stuntracefx/object/fx_collide.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:9301 FxCollidePush
 * WHAT IT DOES: Sets the push on record R9 from the object at R5 (record
 * R7): along the unit vector from the other's position to R9's (on the
 * ground plane), with amount = the closing speed along it (the velocity
 * difference, doubled, dotted with the unit vector; raised to $04B0 when it
 * is from 0 to $04B0) times the other's share of the two masses, halved.
 * HOW: The difference goes to $0020/$0022/$0024 and FxNormalize makes the
 * unit vector, kept at $0068/$002C/$002E; the closing speed is two FMULTs
 * and is kept at $26F6; FxDivideHalvedIfLarge gives the mass share as
 * other * $10000 / (both), halved with LSR; the amount is FMULT of the
 * doubled speed by it. The push is on when the amount is not negative.
 * R11, R1, R8 and the registers FxNormalize and the divide would lose are
 * kept on the R10 stack.
 * Entry: R1 = the contact words; R7 = the other's record; R9 = the
 * record; R10 = stack; R11 = return address. Exit: R0 = 1 (on) or 0; R1,
 * R7, R8, R9 as on entry; R5 = R1; R2 = 0; R3 = the unit z; R4 = the share
 * before halving; R6 = the share; R11 = $93B3; R12 = 0; R13 = $C14B; S, Z
 * from R10, OV, CY from the SUB setting R0; the last RAM address R10.
 */
FxCollidePushResult FxCollidePush(uint16_t contact_words, uint16_t other_record, uint16_t record, uint16_t *stack)
{
    uint16_t mass;
    uint16_t push;
    uint16_t half_x;
    uint16_t mass_2;
    uint16_t fxhit_x;
    uint16_t all_prev;
    uint16_t fxcar_0a;
    uint16_t normal_x;
    uint16_t normal_z;
    uint16_t offset_x;
    uint16_t offset_z;
    uint16_t quotient;
    uint16_t record_z;
    uint16_t velocity;
    uint16_t other_ptr;
    uint16_t record_ptr;
    uint16_t contact_ptr;
    uint16_t push_amount;
    uint16_t closing_speed;
    uint16_t record_prev_z;
    uint16_t fxhit_velocity;
    uint16_t normal_x_saved;
    uint16_t normal_z_saved;
    uint16_t rel_velocity_x;
    uint16_t rel_velocity_z;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_y;
    uint16_t scaled_vector_z;
    uint16_t closing_speed_pulled;
    uint16_t closing_speed_clamped;

    fx_spill(stack);
    fx_push(stack, contact_words);
    fx_spill(stack);
    half_x = fx_ldw((uint16_t)(FXREC_HALF_X + record));
    fxhit_x = fx_ldw((uint16_t)(FXHIT_X + contact_words));
    offset_x = (uint16_t)(half_x - fxhit_x);
    fx_stw(0x0020, offset_x);
    fx_stw(0x0022, 0);
    record_z = fx_ldw((uint16_t)(record + 0x0022));
    fxcar_0a = fx_ldw((uint16_t)(contact_words + FXREC_HANDLER));
    offset_z = (uint16_t)(record_z - fxcar_0a);
    fx_stw(0x0024, offset_z);
    fx_push(stack, other_record);
    fx_push(stack, record);
    fx_push(stack, contact_words);
    { FxNormalizeResult r_ = FxNormalize(offset_x, 0, offset_z, other_record, record, stack); scaled_vector_x = r_.scaled_vector_x; scaled_vector_y = r_.scaled_vector_y; scaled_vector_z = r_.scaled_vector_z; };
    fx_stw(0x0068, scaled_vector_x);
    fx_stw(0x002C, scaled_vector_y);
    fx_stw(0x002E, scaled_vector_z);
    contact_ptr = fx_pull(stack);
    record_ptr = fx_pull(stack);
    other_ptr = fx_pull(stack);
    fxhit_velocity = fx_ldw((uint16_t)(FXHIT_VELOCITY + contact_ptr));
    velocity = fx_ldw((uint16_t)(FXREC_VELOCITY + record_ptr));
    rel_velocity_x = (uint16_t)(fxhit_velocity - velocity);
    normal_x = fx_ldw(0x0068);
    all_prev = fx_ldw((uint16_t)(contact_ptr + FXREC_ALL_PREV));
    record_prev_z = fx_ldw((uint16_t)(record_ptr + 0x0032));
    rel_velocity_z = (uint16_t)(all_prev - record_prev_z);
    normal_z = fx_ldw(0x002E);
    closing_speed = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(rel_velocity_x + rel_velocity_x) * (int16_t)normal_x) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(rel_velocity_z + rel_velocity_z) * (int16_t)normal_z) >> 16));
    closing_speed_clamped = closing_speed;
    if (closing_speed & 0x8000) {
    } else {
        if ((uint16_t)((uint16_t)(0 - closing_speed_clamped) + 0x04B0) & 0x8000) {
        } else {
            closing_speed_clamped = 0x04B0;
        }
    }
    fx_stw(0x26F6, closing_speed_clamped);
    fx_push(stack, other_ptr);
    fx_push(stack, record_ptr);
    fx_push(stack, closing_speed_clamped);
    mass = fx_ldw((uint16_t)(FXREC_MASS + other_ptr));
    mass_2 = fx_ldw((uint16_t)(FXREC_MASS + record_ptr));
    quotient = FxDivideHalvedIfLarge(mass, 0, (uint16_t)(mass_2 + mass));
    closing_speed_pulled = fx_pull(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    push_amount = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(closing_speed_pulled + closing_speed_pulled) * (int16_t)(uint16_t)(quotient >> 1)) >> 16);
    normal_x_saved = fx_ldw(0x0068);
    normal_z_saved = fx_ldw(0x002E);
    push = 0;
    if (push_amount & 0x8000) {
    } else {
        push = 1;
    }
    fx_stw(FX_PUSH, push);
    fx_stw(0x23A6, push_amount);
    fx_stw(0x23A8, normal_x_saved);
    fx_stw(0x23AA, 0);
    fx_stw(0x23AC, normal_z_saved);
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxCollidePushResult){quotient, contact_ptr};
}


/*
 * @routine 04:CA99 FxRecordsApart
 * WHAT IT DOES: Whether records R8 and R7 stay apart this step: R0 = 1 when
 * their distance on the ground plane is more than their sizes (+$0C each)
 * less $294 (no less than 0, decoded from the ROM bytes and not reached in
 * the traces so far) plus how fast they close (half the velocity
 * difference along the line between them), else R0 = 0.
 * HOW: The difference (R7 - R8) goes to $0068/$002E and FxNormalize makes
 * the unit vector; the distance is the difference, doubled, dotted with
 * it (FMULT) and kept at $0068; the closing speed is the velocity
 * difference halved (DIV2) dotted with it, negated, at $006A. R1-R5 and
 * the return address are kept on the R10 stack, R8 and R7 across the call.
 * Entry: R7, R8 = the records; R10 = stack; R11 = return address. Exit: R0
 * = 1 or 0; R1-R5, R7, R8 as on entry; R6 = the unit z; R9, R11-R13 as
 * FxNormalize leaves them; S, Z from R10; OV, CY from the CMP (R0 = 1) or
 * SUB R0 (R0 = 0); the last RAM address R10.
 */
FxRecordsApartResult FxRecordsApart(uint16_t records_1, uint16_t records_2, uint16_t normalize_aux_in, uint16_t *stack)
{
    uint16_t k;
    uint16_t size;
    uint16_t half_x;
    uint16_t size_2;
    uint16_t delta_x;
    uint16_t delta_z;
    uint16_t half_x_2;
    uint16_t velocity;
    uint16_t record1_z;
    uint16_t record2_z;
    uint16_t separation;
    uint16_t velocity_2;
    uint16_t record1_ptr;
    uint16_t record2_ptr;
    uint16_t closing_speed;
    uint16_t delta_x_saved;
    uint16_t delta_z_saved;
    uint16_t touch_distance;
    uint16_t record1_word_32;
    uint16_t record2_word_32;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_z;
    uint16_t touch_distance_raw;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    half_x = fx_ldw((uint16_t)(FXREC_HALF_X + records_1));
    half_x_2 = fx_ldw((uint16_t)(FXREC_HALF_X + records_2));
    delta_x = (uint16_t)(half_x - half_x_2);
    fx_stw(0x0068, delta_x);
    record1_z = fx_ldw((uint16_t)(records_1 + 0x0022));
    record2_z = fx_ldw((uint16_t)(records_2 + 0x0022));
    delta_z = (uint16_t)(record1_z - record2_z);
    fx_stw(0x002E, delta_z);
    fx_push(stack, records_2);
    fx_push(stack, records_1);
    { FxNormalizeResult r_ = FxNormalize(delta_x, 0, delta_z, records_1, normalize_aux_in, stack); scaled_vector_x = r_.scaled_vector_x; scaled_vector_z = r_.scaled_vector_z; k = r_.k; };
    record1_ptr = fx_pull(stack);
    record2_ptr = fx_pull(stack);
    delta_x_saved = fx_ldw(0x0068);
    delta_z_saved = fx_ldw(0x002E);
    fx_stw(0x0068, (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(delta_z_saved + delta_z_saved) * (int16_t)scaled_vector_z) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(delta_x_saved + delta_x_saved) * (int16_t)scaled_vector_x) >> 16)));
    velocity = fx_ldw((uint16_t)(FXREC_VELOCITY + record1_ptr));
    velocity_2 = fx_ldw((uint16_t)(FXREC_VELOCITY + record2_ptr));
    record1_word_32 = fx_ldw((uint16_t)(record1_ptr + 0x0032));
    record2_word_32 = fx_ldw((uint16_t)(record2_ptr + 0x0032));
    fx_stw(0x006A, (uint16_t)((uint16_t)~(uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)fx_div2((uint16_t)(record1_word_32 - record2_word_32)) * (int16_t)scaled_vector_z) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)fx_div2((uint16_t)(velocity - velocity_2)) * (int16_t)scaled_vector_x) >> 16)) + 1));
    size = fx_ldw((uint16_t)(FXREC_SIZE + record1_ptr));
    size_2 = fx_ldw((uint16_t)(FXREC_SIZE + record2_ptr));
    touch_distance_raw = (uint16_t)((uint16_t)(size + size_2) - 0x0294);
    touch_distance = touch_distance_raw;
    if (!(touch_distance_raw & 0x8000)) {
    } else {
        touch_distance = 0;
    }
    closing_speed = fx_ldw(0x006A);
    separation = fx_ldw(0x0068);
    if ((uint16_t)((uint16_t)(touch_distance + closing_speed) - separation) & 0x8000) {
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        return (FxRecordsApartResult){1, k};
    } else {
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        fx_unspill(stack);
        return (FxRecordsApartResult){0, k};
    }
}


/*
 * @routine 04:A345 FxContactNote
 * WHAT IT DOES: Notes that record R6 touches record R8: finds R8's contact
 * on R6's list or makes one (a $12-byte block from the first pool, on the
 * front of the list, +$0C = R8, +$10 = 0), copies R8's velocity x, z and
 * position x, z into it unless bit 2 of its +$10 is set, and sets bit 0 of
 * +$10. Nothing is done when bit 7 of R6's +$16 is set. With no room in
 * the pool nothing is noted (decoded from the ROM bytes, as is the
 * whole-block case of the allocation below, and not reached in the traces
 * so far).
 * HOW: The allocation is FxPoolTake's done again inline, with R7 the size,
 * R9 the walk and R8 (kept on the R10 stack) the link: an exact fit stays
 * on the chain with its size negated, else the block is cut from the end
 * of a bigger one. The routine runs under CACHE (the code cache only;
 * nothing the C has to do).
 * Entry: R6 = the record; R8 = the other; R10 = stack; R11 = return
 * address. Exit: R7 = the contact; R0 = its new +$10; R9 = 1; S, Z from
 * the OR; the last RAM address the contact + $10. Bit 7 set: R0 = R6's
 * +$16 and $80; R9 = $80.
 */
FxContactNoteResult FxContactNote(uint16_t record, uint16_t contact_ptr_in, uint16_t other, uint16_t *stack)
{
    int run_lA375 = 0;
    uint16_t list;
    uint16_t next;
    uint16_t half_x;
    uint16_t list_2;
    uint16_t flags10;
    uint16_t flags16;
    uint16_t all_prev;
    uint16_t fxcar_0a;
    uint16_t velocity;
    uint16_t flags10_2;
    uint16_t fxcon_other;
    uint16_t contact_flag;
    uint16_t size_surplus;
    uint16_t contact_bytes;
    uint16_t leftover_size;
    uint16_t pool_block_ptr;
    uint16_t pool_size_addr;
    uint16_t pool_size_word;
    uint16_t free_block_size;
    uint16_t new_contact_ptr;
    uint16_t next_free_block;
    uint16_t next_pool_block;
    uint16_t contact_ptr_copy;
    uint16_t split_block_addr;
    uint16_t leftover_after_split;
    uint16_t surplus_minus_header;

    flags16 = fx_ldw((uint16_t)(FXREC_FLAGS16 + record));
    contact_flag = 0x0080;
    if ((flags16 & contact_flag) == 0) {
        list = fx_ldw((uint16_t)(FXREC_LIST + record));
        contact_ptr_in = list;
        if (list == 0) {
            run_lA375 = 1;
        } else {
            for (;;) {
                fxcon_other = fx_ldw((uint16_t)(FXCON_OTHER + contact_ptr_in));
                if (fxcon_other != other) {
                    next = fx_ldw((uint16_t)(FXREC_NEXT + contact_ptr_in));
                    contact_ptr_in = next;
                    if (next != 0) {
                        continue;
                    } else {
                        break;
                    }
                } else {
                    flags10 = fx_ldw((uint16_t)(FXREC_FLAGS10 + contact_ptr_in));
                    if ((4 & flags10) != 0) {
                    } else {
                        velocity = fx_ldw((uint16_t)(FXREC_VELOCITY + other));
                        fx_stw((uint16_t)(FXHIT_VELOCITY + contact_ptr_in), velocity);
                        all_prev = fx_ldw((uint16_t)(other + 0x0032));
                        fx_stw((uint16_t)(contact_ptr_in + FXREC_ALL_PREV), all_prev);
                        half_x = fx_ldw((uint16_t)(FXREC_HALF_X + other));
                        fx_stw((uint16_t)(FXHIT_X + contact_ptr_in), half_x);
                        fxcar_0a = fx_ldw((uint16_t)(other + 0x0022));
                        fx_stw((uint16_t)(contact_ptr_in + FXREC_HANDLER), fxcar_0a);
                    }
                    flags10_2 = fx_ldw((uint16_t)(FXREC_FLAGS10 + contact_ptr_in));
                    contact_flag = 1;
                    fx_sbk((flags10_2 | contact_flag));
                    return (FxContactNoteResult){contact_ptr_in, contact_flag};
                }
            }
            run_lA375 = 1;
        }
        fx_push(stack, other);
        pool_block_ptr = FX_POOL_FIRST;
        contact_bytes = FX_CONTACT_BYTES;
        fx_push(stack, pool_block_ptr);
        for (;;) {
            next_pool_block = fx_ldw(pool_block_ptr);
            pool_size_addr = (uint16_t)(pool_block_ptr + 2);
            pool_size_word = fx_ldw(pool_size_addr);
            if (pool_size_word == contact_bytes) {
                fx_unspill(stack);
                contact_bytes = (uint16_t)((uint16_t)~contact_bytes + 1);
                leftover_size = 0;
                split_block_addr = (uint16_t)(pool_size_addr + leftover_size);
                fx_stw(split_block_addr, contact_bytes);
                new_contact_ptr = (uint16_t)(split_block_addr - 2);
                break;
            } else {
                pool_block_ptr = next_pool_block;
                if (next_pool_block != 0) {
                    continue;
                } else {
                    pool_size_addr = fx_pull(stack);
                    for (;;) {
                        next_free_block = fx_ldw(pool_size_addr);
                        pool_size_addr = (uint16_t)(pool_size_addr + 2);
                        free_block_size = fx_ldw(pool_size_addr);
                        size_surplus = (uint16_t)(free_block_size - contact_bytes);
                        if (!(size_surplus & 0x8000)) {
                            surplus_minus_header = (uint16_t)(size_surplus - 4);
                            leftover_after_split = surplus_minus_header;
                            if (!(surplus_minus_header & 0x8000)) {
                            } else {
                                contact_bytes = (uint16_t)((uint16_t)~(uint16_t)((uint16_t)(contact_bytes + leftover_after_split) + 4) + 1);
                                leftover_after_split = 0xFFFC;
                            }
                            leftover_size = (uint16_t)(leftover_after_split + 4);
                            fx_stw(pool_size_addr, leftover_size);
                            split_block_addr = (uint16_t)(pool_size_addr + leftover_size);
                            fx_stw(split_block_addr, contact_bytes);
                            new_contact_ptr = (uint16_t)(split_block_addr - 2);
                            break;
                        } else {
                            pool_size_addr = next_free_block;
                            if (next_free_block != 0) {
                                continue;
                            } else {
                                new_contact_ptr = 0;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
        other = fx_pull(stack);
        contact_ptr_copy = new_contact_ptr;
        contact_ptr_in = contact_ptr_copy;
        if (contact_ptr_copy != 0) {
            fx_stw((uint16_t)(FXCON_OTHER + contact_ptr_in), other);
            list_2 = fx_ldw((uint16_t)(FXREC_LIST + record));
            fx_stw((uint16_t)(FXREC_NEXT + contact_ptr_in), list_2);
            fx_stw((uint16_t)(FXREC_LIST + record), contact_ptr_in);
            fx_stw((uint16_t)(FXREC_FLAGS10 + contact_ptr_in), 0);
            flags10 = fx_ldw((uint16_t)(FXREC_FLAGS10 + contact_ptr_in));
            if ((4 & flags10) != 0) {
            } else {
                velocity = fx_ldw((uint16_t)(FXREC_VELOCITY + other));
                fx_stw((uint16_t)(FXHIT_VELOCITY + contact_ptr_in), velocity);
                all_prev = fx_ldw((uint16_t)(other + 0x0032));
                fx_stw((uint16_t)(contact_ptr_in + FXREC_ALL_PREV), all_prev);
                half_x = fx_ldw((uint16_t)(FXREC_HALF_X + other));
                fx_stw((uint16_t)(FXHIT_X + contact_ptr_in), half_x);
                fxcar_0a = fx_ldw((uint16_t)(other + 0x0022));
                fx_stw((uint16_t)(contact_ptr_in + FXREC_HANDLER), fxcar_0a);
            }
            flags10_2 = fx_ldw((uint16_t)(FXREC_FLAGS10 + contact_ptr_in));
            contact_flag = 1;
            fx_sbk((flags10_2 | contact_flag));
            return (FxContactNoteResult){contact_ptr_in, contact_flag};
        } else {
            return (FxContactNoteResult){contact_ptr_in, pool_size_addr};
        }
    } else {
        return (FxContactNoteResult){contact_ptr_in, contact_flag};
    }
}


/*
 * @routine 04:A1EC FxAllTouches
 * WHAT IT DOES: FxRecordTouches for every record on the list of all
 * records ($2332), so each pair is tested once.
 * HOW: R1 walks the list through +$04; the return address is kept on the
 * R10 stack. The routine runs under CACHE (the code cache only; nothing
 * the C has to do).
 * Entry: R10 = stack; R11 = return address. Exit: R0 = R1 = 0; the others
 * from the last FxRecordTouches; S, Z from R10; the last RAM address R10.
 */
FxAllTouchesResult FxAllTouches(uint16_t touch_pos_scratch, uint16_t prior_record_z, uint16_t last_contact_ptr, uint16_t last_contact_flags, uint16_t *stack)
{
    uint16_t records;
    uint16_t current_record_ptr;

    fx_spill(stack);
    records = fx_ldw(FX_RECORDS);
    for (;;) {
        current_record_ptr = records;
        if (current_record_ptr == 0) {
            break;
        } else {
            { FxRecordTouchesResult r_ = FxRecordTouches(current_record_ptr, last_contact_ptr, last_contact_flags, stack); touch_pos_scratch = r_.record_y; prior_record_z = r_.record_z_out; last_contact_ptr = r_.last_contact_result; last_contact_flags = r_.last_contact_aux; };
            records = fx_ldw((uint16_t)(FXREC_ALL_NEXT + current_record_ptr));
            continue;
        }
    }
    fx_unspill(stack);
    return (FxAllTouchesResult){touch_pos_scratch, prior_record_z, last_contact_ptr, last_contact_flags};
}


/*
 * @routine 04:A20B FxRecordTouches
 * WHAT IT DOES: Tests record R1 against every record after it on the list
 * of all records ($2332, linked through +$04): their boxes overlap when on
 * each axis (z, then x, then y) the distance between the positions is
 * below the two sizes (+$10, +$0C, +$0E) added. For an overlapping pair:
 * when both have bit 1 of +$16 set and FxRecordsApart says they stay
 * apart, nothing; otherwise each notes the other as a contact
 * (FxContactNote) unless its +$1A shares a bit with the other's +$16.
 * Then, when R1 has contacts, FxListSweep drops those not seen this step.
 * HOW: R1's position goes to R3-R5 and its sizes to $236C/$236E/$2370;
 * R2 walks the list. The return address is kept on the R10 stack.
 * Entry: R1 = the record; R10 = stack; R11 = return address. Exit: R2 = 0;
 * R1, R3-R5 as above; the others from the last test or call; S, Z from
 * R10; the last RAM address R10.
 */
FxRecordTouchesResult FxRecordTouches(uint16_t record, uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t *stack)
{
    uint16_t y;
    uint16_t y_2;
    uint16_t list;
    uint16_t mode;
    uint16_t next;
    uint16_t size;
    uint16_t sizes;
    uint16_t half_x;
    uint16_t mode_2;
    uint16_t next_2;
    uint16_t x_dist;
    uint16_t y_dist;
    uint16_t z_dist;
    uint16_t flags10;
    uint16_t flags16;
    uint16_t other_z;
    uint16_t all_next;
    uint16_t half_x_2;
    uint16_t record_z;
    uint16_t flags10_2;
    uint16_t flags16_2;
    uint16_t flags16_3;
    uint16_t flags16_4;
    uint16_t x_dist_abs;
    uint16_t x_size_sum;
    uint16_t y_dist_abs;
    uint16_t y_size_sum;
    uint16_t z_dist_abs;
    uint16_t z_size_sum;
    uint16_t other_record;
    uint16_t other_x_size;
    uint16_t record_y_size;
    uint16_t record_z_size;
    uint16_t records_apart;

    fx_spill(stack);
    half_x = fx_ldw((uint16_t)(FXREC_HALF_X + record));
    y = fx_ldw((uint16_t)(record + FXREC_Y));
    record_z = fx_ldw((uint16_t)(record + 0x0022));
    size = fx_ldw((uint16_t)(FXREC_SIZE + record));
    fx_stw(FX_SIZES, size);
    next = fx_ldw((uint16_t)(record + FXREC_NEXT));
    fx_stw(0x236E, next);
    flags10 = fx_ldw((uint16_t)(record + FXREC_FLAGS10));
    fx_stw(0x2370, flags10);
    other_record = record;
    for (;;) {
        all_next = fx_ldw((uint16_t)(FXREC_ALL_NEXT + other_record));
        other_record = all_next;
        if (all_next != 0) {
            flags10_2 = fx_ldw((uint16_t)(other_record + FXREC_FLAGS10));
            record_z_size = fx_ldw(0x2370);
            z_size_sum = (uint16_t)(flags10_2 + record_z_size);
            other_z = fx_ldw((uint16_t)(other_record + 0x0022));
            z_dist = (uint16_t)(other_z - record_z);
            z_dist_abs = z_dist;
            if (!(z_dist & 0x8000)) {
            } else {
                z_dist_abs = (uint16_t)((uint16_t)~z_dist_abs + 1);
            }
            if (!((uint16_t)(z_dist_abs - z_size_sum) & 0x8000)) {
                continue;
            } else {
                other_x_size = fx_ldw((uint16_t)(other_record + 0x000C));
                sizes = fx_ldw(FX_SIZES);
                x_size_sum = (uint16_t)(other_x_size + sizes);
                half_x_2 = fx_ldw((uint16_t)(other_record + FXREC_HALF_X));
                x_dist = (uint16_t)(half_x_2 - half_x);
                x_dist_abs = x_dist;
                if (!(x_dist & 0x8000)) {
                } else {
                    x_dist_abs = (uint16_t)((uint16_t)~x_dist_abs + 1);
                }
                if (!((uint16_t)(x_dist_abs - x_size_sum) & 0x8000)) {
                    continue;
                } else {
                    next_2 = fx_ldw((uint16_t)(other_record + FXREC_NEXT));
                    record_y_size = fx_ldw(0x236E);
                    y_size_sum = (uint16_t)(next_2 + record_y_size);
                    y_2 = fx_ldw((uint16_t)(other_record + FXREC_Y));
                    y_dist = (uint16_t)(y_2 - y);
                    y_dist_abs = y_dist;
                    if (!(y_dist & 0x8000)) {
                    } else {
                        y_dist_abs = (uint16_t)((uint16_t)~y_dist_abs + 1);
                    }
                    if ((uint16_t)(y_dist_abs - y_size_sum) & 0x8000) {
                        flags16 = fx_ldw((uint16_t)(FXREC_FLAGS16 + record));
                        flags16_2 = fx_ldw((uint16_t)(FXREC_FLAGS16 + other_record));
                        if (((flags16 & flags16_2) & 2) == 0) {
                            mode = fx_ldw((uint16_t)(FXREC_MODE + record));
                            flags16_3 = fx_ldw((uint16_t)(FXREC_FLAGS16 + other_record));
                            if ((mode & flags16_3) != 0) {
                            } else {
                                { FxContactNoteResult r_ = FxContactNote(record, entry_scratch_a, other_record, stack); entry_scratch_a = r_.contact; entry_scratch_b = r_.contact_flag_out; };
                            }
                            mode_2 = fx_ldw((uint16_t)(FXREC_MODE + other_record));
                            flags16_4 = fx_ldw((uint16_t)(FXREC_FLAGS16 + record));
                            if ((mode_2 & flags16_4) != 0) {
                            } else {
                                { FxContactNoteResult r_ = FxContactNote(other_record, entry_scratch_a, record, stack); entry_scratch_a = r_.contact; entry_scratch_b = r_.contact_flag_out; };
                            }
                        } else {
                            entry_scratch_a = record;
                            { FxRecordsApartResult r_ = FxRecordsApart(entry_scratch_a, other_record, entry_scratch_b, stack); records_apart = r_.records_apart; entry_scratch_b = r_.normalize_aux_out; };
                            if (records_apart != 0) {
                            } else {
                                mode = fx_ldw((uint16_t)(FXREC_MODE + record));
                                flags16_3 = fx_ldw((uint16_t)(FXREC_FLAGS16 + other_record));
                                if ((mode & flags16_3) != 0) {
                                } else {
                                    { FxContactNoteResult r_ = FxContactNote(record, entry_scratch_a, other_record, stack); entry_scratch_a = r_.contact; entry_scratch_b = r_.contact_flag_out; };
                                }
                                mode_2 = fx_ldw((uint16_t)(FXREC_MODE + other_record));
                                flags16_4 = fx_ldw((uint16_t)(FXREC_FLAGS16 + record));
                                if ((mode_2 & flags16_4) != 0) {
                                } else {
                                    { FxContactNoteResult r_ = FxContactNote(other_record, entry_scratch_a, record, stack); entry_scratch_a = r_.contact; entry_scratch_b = r_.contact_flag_out; };
                                }
                            }
                        }
                        continue;
                    } else {
                        continue;
                    }
                }
            }
        } else {
            break;
        }
    }
    list = fx_ldw((uint16_t)(FXREC_LIST + record));
    if (list == 0) {
    } else {
        { FxListSweepResult r_ = FxListSweep(record, entry_scratch_a, entry_scratch_b, stack); entry_scratch_a = r_.last_next_rec; entry_scratch_b = r_.last_size_addr; };
    }
    fx_unspill(stack);
    return (FxRecordTouchesResult){y, record_z, entry_scratch_a, entry_scratch_b};
}

