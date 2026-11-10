/*
 * fx_physics.c: the Super FX code of asm/stuntracefx/object/fx_physics.asm, written from it by
 * the generator.
 *
 * fx_physics.asm: the Super FX physics program (04:8FCA), the record
 * handlers, collisions and contacts, a car's parts and its course.
 * C: src/stuntracefx/object/fx_physics.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:8FCA FxPhysicsProgram
 * WHAT IT DOES: A program the S-CPU starts once a race frame: steps
 * the race timers ($2472 down to 0, $2470 up), notes every touch
 * between records (FxAllTouches, unless $2472 is still running), then
 * runs each record of the list at $2332: one with a handler (+$0A) has
 * it run (twice when +$1C bit 3 is set, $263C 0 then 1); one without
 * is freed when its +$1C bit 2 is clear (FxObjectFree with its +$04),
 * else that bit is cleared. Then the cars are ranked (FxRankCars) and
 * the nearest found (FxNearestCar), and STOP. Nothing runs when $246C
 * is 0.
 * HOW: R10 = $0240; ROMB and RAMB 0; $246E = $FFFF while it runs; the
 * list walked by +$04 (R8 = the record); a handler is reached by LINK
 * #2; JMP R12, so it returns to 04:9030 or 04:903D.
 * Entry: started by the S-CPU.
 * Exit: (at the STOP) $246E = 0; the rest as the last record's handler
 * and FxNearestCar leave them.
 */
FxPhysicsProgramResult FxPhysicsProgram(uint16_t entry_scratch_a, uint16_t pose_sin, uint16_t entry_scratch_b, uint16_t entry_scratch_c, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t stack_top;
    uint16_t *stack = &stack_top;
    uint16_t count;
    uint16_t flags1c;
    uint16_t records;
    uint16_t all_next;
    uint16_t fxcar_0a;
    uint16_t flags1c_2;
    uint16_t init_2470;
    uint16_t init_2472;
    uint16_t view_246c;
    uint16_t record_out;
    uint16_t init_2472_2;
    uint16_t current_record;
    uint16_t candidate_car_out;

    *stack = 0x0240;
    fx_romb(0);
    fx_ramb(0);
    view_246c = fx_ldw(GSU_VIEW_246C);
    if (view_246c != 0) {
        init_2472 = fx_ldw(FX_INIT_2472);
        if (init_2472 == 0) {
        } else {
            fx_sbk((uint16_t)(init_2472 - 1));
        }
        init_2470 = fx_ldw(FX_INIT_2470);
        fx_sbk((uint16_t)(init_2470 + 1));
        fx_stw(FX_PHYSICS_BUSY, 0xFFFF);
        init_2472_2 = fx_ldw(FX_INIT_2472);
        if (init_2472_2 != 0) {
        } else {
            { FxAllTouchesResult r_ = FxAllTouches(entry_scratch_a, pose_sin, entry_scratch_b, entry_scratch_c, stack); entry_scratch_a = r_.touch_pos_result; pose_sin = r_.record_z_out; entry_scratch_b = r_.last_contact_ptr_out; entry_scratch_c = r_.last_contact_flags_out; };
        }
        records = fx_ldw(FX_RECORDS);
        for (;;) {
            current_record = records;
            record_out = current_record;
            if (current_record == 0) {
                break;
            } else {
                fxcar_0a = fx_ldw((uint16_t)(record_out + FXREC_HANDLER));
                if (fxcar_0a == 0) {
                } else {
                    count = fxcar_0a;
                    flags1c = fx_ldw((uint16_t)(record_out + FXREC_FLAGS1C));
                    if ((flags1c & 8) == 0) {
                    } else {
                        fx_stw((*stack), count);
                        *stack = (uint16_t)((*stack) + 2);
                        fx_stw(FX_HIT_ON, 0);
                        { FxRecordHandlerResult r_ = FxRecordHandler(entry_scratch_a, pose_sin, entry_scratch_b, record_out, entry_scratch_c, stack, count, rom_at); up_ = r_.up; entry_scratch_a = r_.pose_cos_out; pose_sin = r_.pose_sin_out; entry_scratch_b = r_.pose_aux_out; record_out = r_.record_out; entry_scratch_c = r_.pose_matrix_ptr_out; rom_at = r_.rom_at_out; };
                        if (up_) {                       /* it returned past us */
                            return (FxPhysicsProgramResult){up_ == 1 ? 0 : up_ - 1};
                        }
                        *stack = (uint16_t)((*stack) - 2);
                        count = fx_ldw((*stack));
                    }
                    fx_stw(FX_HIT_ON, 1);
                    { FxRecordHandlerResult r_ = FxRecordHandler(entry_scratch_a, pose_sin, entry_scratch_b, record_out, entry_scratch_c, stack, count, rom_at); up_ = r_.up; entry_scratch_a = r_.pose_cos_out; pose_sin = r_.pose_sin_out; entry_scratch_b = r_.pose_aux_out; record_out = r_.record_out; entry_scratch_c = r_.pose_matrix_ptr_out; rom_at = r_.rom_at_out; };
                    if (up_) {                       /* it returned past us */
                        return (FxPhysicsProgramResult){up_ == 0 ? 0 : up_ - 0};
                    }
                }
                flags1c_2 = fx_ldw((uint16_t)(record_out + FXREC_FLAGS1C));
                if ((flags1c_2 & 4) != 0) {
                    fx_sbk((uint16_t)(flags1c_2 & ~4));
                    records = fx_ldw((uint16_t)(record_out + FXREC_ALL_NEXT));
                    continue;
                } else {
                    all_next = fx_ldw((uint16_t)(record_out + FXREC_ALL_NEXT));
                    fx_stw((*stack), all_next);
                    *stack = (uint16_t)((*stack) + 2);
                    { FxObjectFreeResult r_ = FxObjectFree(record_out, entry_scratch_a, entry_scratch_b, entry_scratch_c, stack); entry_scratch_a = r_.children_result; entry_scratch_b = r_.sweep_result_7; entry_scratch_c = r_.sweep_result_9; };
                    *stack = (uint16_t)((*stack) - 2);
                    records = fx_ldw((*stack));
                    continue;
                }
            }
        }
        candidate_car_out = FxRankCars(entry_scratch_b, stack);
        FxNearestCar(candidate_car_out, stack);
        fx_stw(FX_PHYSICS_BUSY, 0);
    } else {
    }
    return (FxPhysicsProgramResult){0};
}


/*
 * @routine 04:907A FxObjectBounce
 * WHAT IT DOES: A record handler: an object that falls and bounces:
 * its y speed +$30 gains $78, it moves (FxRecordMove, R1 = 3) and
 * collides (FxRecordCollide); hit from below ($239C positive) its y
 * speed loses the hit, and on any hit its x and z speeds (+$2E, +$32)
 * lose an eighth. Not reached in the traces so far.
 * HOW: Pushes R11 and R8 on the R10 stack; R9 = R8; returns by pulling
 * R15.
 * Entry: R8 = the record; R10 = stack; R11 = return address.
 * Exit: R8 back; the rest as FxRecordCollide and the damping leave
 * them.
 */
FxObjectBounceResult FxObjectBounce(uint16_t place_x, uint16_t pose_aux, uint16_t record, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t hit_depth;
    uint16_t collided_x;
    uint16_t collided_z;
    uint16_t record_out;
    uint16_t rom_at_out_2;
    uint16_t record_restored;
    uint16_t record_z_before;
    uint16_t collided_y_speed;
    uint16_t world_sum_x_out_2;
    uint16_t world_sum_y_out_2;
    uint16_t next_entry_end_ptr;
    uint16_t caller_scratch_back;
    uint16_t push_flags_high_out_2;

    fx_spill(stack);
    fx_push(stack, record);
    record_z_before = fx_ldw((uint16_t)(record + 0x0030));
    fx_stw((uint16_t)(record + 0x0030), (uint16_t)(record_z_before + 0x0078));
    next_entry_end_ptr = FxRecordMove(3, record);
    { FxRecordCollideResult r_ = FxRecordCollide(place_x, next_entry_end_ptr, pose_aux, record, record, stack, rom_at); up_ = r_.up; world_sum_x_out_2 = r_.world_sum_x_out; world_sum_y_out_2 = r_.world_sum_y_out; push_flags_high_out_2 = r_.push_flags_high_out; caller_scratch_back = r_.caller_scratch_back; record_out = r_.record_out; rom_at_out_2 = r_.rom_at_out; };
    if (up_) {                       /* it returned past us */
        return (FxObjectBounceResult){world_sum_x_out_2, world_sum_y_out_2, push_flags_high_out_2, caller_scratch_back, record_out, rom_at_out_2, up_ == 2 ? 0 : up_ - 2};
    }
    hit_depth = fx_ldw(FX_HIT_DEPTH);
    if (hit_depth == 0) {
    } else {
        if (hit_depth & 0x8000) {
        } else {
            collided_z = fx_ldw((uint16_t)(record_out + 0x0030));
            fx_stw((uint16_t)(record_out + 0x0030), (uint16_t)(collided_z - hit_depth));
        }
        collided_x = fx_ldw((uint16_t)(record_out + 0x002E));
        fx_stw((uint16_t)(record_out + 0x002E), (uint16_t)(collided_x - fx_div2(fx_div2(fx_div2(collided_x)))));
        collided_y_speed = fx_ldw((uint16_t)(record_out + 0x0032));
        fx_stw((uint16_t)(record_out + 0x0032), (uint16_t)(collided_y_speed - fx_div2(fx_div2(fx_div2(collided_y_speed)))));
    }
    record_restored = fx_pull(stack);
    fx_unspill(stack);
    return (FxObjectBounceResult){world_sum_x_out_2, world_sum_y_out_2, push_flags_high_out_2, record_restored, record_out, rom_at_out_2, 0};
}


/*
 * @routine 04:90E8 FxObjectMarkTouched
 * WHAT IT DOES: A record handler: marks every record in this one's
 * touch list (+$18, linked by +$0E) touched (bit 1 of +$10).
 * HOW: A loop of LDW, OR, SBK; returns by JMP R11 (MOVE R15, R11).
 * Entry: R8 = the record; R11 = return address.
 * Exit: R0 = 0 at the list's end; R1 = 0; R2 = 2.
 */
FxObjectMarkTouchedResult FxObjectMarkTouched(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t record, uint16_t pose_matrix_ptr, uint16_t rom_at)
{
    uint16_t list;
    uint16_t next;
    uint16_t flags10;
    uint16_t touching_rec_ptr;

    list = fx_ldw((uint16_t)(record + FXREC_LIST));
    touching_rec_ptr = list;
    if (list == 0) {
    } else {
        for (;;) {
            flags10 = fx_ldw((uint16_t)(touching_rec_ptr + FXREC_FLAGS10));
            fx_sbk((flags10 | 2));
            next = fx_ldw((uint16_t)(touching_rec_ptr + FXREC_NEXT));
            touching_rec_ptr = next;
            if (next != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    return (FxObjectMarkTouchedResult){pose_cos, pose_sin, pose_aux, record, pose_matrix_ptr, rom_at};
}


/*
 * @routine 04:910A FxObjectFall
 * WHAT IT DOES: A record handler: an object that falls while it
 * touches something: with a touch list (+$18) its y speed +$30 gains
 * $78; then it moves (FxRecordMove, R1 = 3) and collides
 * (FxRecordCollide); on a hit its x and z speeds lose a quarter.
 * HOW: Pushes R11 and R8 on the R10 stack; R9 = R8; returns by pulling
 * R15.
 * Entry: R8 = the record; R10 = stack; R11 = return address.
 * Exit: R8 back; the rest as FxRecordCollide and the damping leave
 * them.
 */
FxObjectFallResult FxObjectFall(uint16_t place_x, uint16_t pose_aux, uint16_t record, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t list;
    uint16_t speed_x;
    uint16_t speed_z;
    uint16_t hit_depth;
    uint16_t record_out;
    uint16_t fall_speed_y;
    uint16_t rom_at_out_2;
    uint16_t record_restored;
    uint16_t world_sum_x_out_2;
    uint16_t world_sum_y_out_2;
    uint16_t next_entry_end_ptr;
    uint16_t caller_scratch_back;
    uint16_t push_flags_high_out_2;

    fx_spill(stack);
    fx_push(stack, record);
    list = fx_ldw((uint16_t)(record + FXREC_LIST));
    if (list == 0) {
    } else {
        fall_speed_y = fx_ldw((uint16_t)(record + 0x0030));
        fx_stw((uint16_t)(record + 0x0030), (uint16_t)(fall_speed_y + 0x0078));
    }
    next_entry_end_ptr = FxRecordMove(3, record);
    { FxRecordCollideResult r_ = FxRecordCollide(place_x, next_entry_end_ptr, pose_aux, record, record, stack, rom_at); up_ = r_.up; world_sum_x_out_2 = r_.world_sum_x_out; world_sum_y_out_2 = r_.world_sum_y_out; push_flags_high_out_2 = r_.push_flags_high_out; caller_scratch_back = r_.caller_scratch_back; record_out = r_.record_out; rom_at_out_2 = r_.rom_at_out; };
    if (up_) {                       /* it returned past us */
        return (FxObjectFallResult){world_sum_x_out_2, world_sum_y_out_2, push_flags_high_out_2, caller_scratch_back, record_out, rom_at_out_2, up_ == 2 ? 0 : up_ - 2};
    }
    hit_depth = fx_ldw(FX_HIT_DEPTH);
    if (hit_depth == 0) {
    } else {
        speed_x = fx_ldw((uint16_t)(record_out + 0x002E));
        fx_stw((uint16_t)(record_out + 0x002E), (uint16_t)(speed_x - fx_div2(fx_div2(speed_x))));
        speed_z = fx_ldw((uint16_t)(record_out + 0x0032));
        fx_stw((uint16_t)(record_out + 0x0032), (uint16_t)(speed_z - fx_div2(fx_div2(speed_z))));
    }
    record_restored = fx_pull(stack);
    fx_unspill(stack);
    return (FxObjectFallResult){world_sum_x_out_2, world_sum_y_out_2, push_flags_high_out_2, record_restored, record_out, rom_at_out_2, 0};
}


/*
 * @routine 04:916D FxObjectCollideOnly
 * WHAT IT DOES: A record handler: only collides the record with what
 * it touches (FxRecordCollide), without moving it. Not reached in the
 * traces so far.
 * HOW: Pushes R11 and R8 on the R10 stack; R9 = R8; returns by pulling
 * R15.
 * Entry: R8 = the record; R10 = stack; R11 = return address.
 * Exit: R8 back; the rest as FxRecordCollide leaves them.
 */
FxObjectCollideOnlyResult FxObjectCollideOnly(uint16_t place_x, uint16_t place_y, uint16_t last_other_ptr, uint16_t record, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t record_out;
    uint16_t rom_at_out_2;
    uint16_t pulled_record;
    uint16_t world_sum_x_out;
    uint16_t world_sum_y_out;
    uint16_t caller_scratch_back;
    uint16_t push_flags_high_out;

    fx_spill(stack);
    fx_push(stack, record);
    { FxRecordCollideResult r_ = FxRecordCollide(place_x, place_y, last_other_ptr, record, record, stack, rom_at); up_ = r_.up; world_sum_x_out = r_.world_sum_x_out; world_sum_y_out = r_.world_sum_y_out; push_flags_high_out = r_.push_flags_high_out; caller_scratch_back = r_.caller_scratch_back; record_out = r_.record_out; rom_at_out_2 = r_.rom_at_out; };
    if (up_) {                       /* it returned past us */
        return (FxObjectCollideOnlyResult){world_sum_x_out, world_sum_y_out, push_flags_high_out, caller_scratch_back, record_out, rom_at_out_2, up_ == 2 ? 0 : up_ - 2};
    }
    pulled_record = fx_pull(stack);
    fx_unspill(stack);
    return (FxObjectCollideOnlyResult){world_sum_x_out, world_sum_y_out, push_flags_high_out, pulled_record, record_out, rom_at_out_2, 0};
}


/*
 * @routine 04:9185 FxObjectDrop
 * WHAT IT DOES: A record handler: an object dropped straight down: its
 * y speed gains $3C, its x and z speeds are 0, it moves (FxRecordMove,
 * R1 = 3) and collides keeping x and z (+$1E, +$22) from before; on a
 * hit its handler (+$0A) is cleared, so it stops. Not reached in the
 * traces so far.
 * HOW: Pushes R11, R8 and the x and z on the R10 stack; returns by
 * pulling R15.
 * Entry: R8 = the record; R10 = stack; R11 = return address.
 * Exit: R8 back; the rest as FxRecordCollide leaves them.
 */
FxObjectDropResult FxObjectDrop(uint16_t place_x, uint16_t last_other_ptr, uint16_t record, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t half_x;
    uint16_t half_x_2;
    uint16_t hit_depth;
    uint16_t drop_speed;
    uint16_t record_out;
    uint16_t rom_at_out_2;
    uint16_t record_angle_22;
    uint16_t world_sum_x_out;
    uint16_t world_sum_y_out;
    uint16_t restored_angle_22;
    uint16_t next_entry_end_ptr;
    uint16_t pulled_record_kept;
    uint16_t caller_scratch_back;
    uint16_t push_flags_high_out;
    uint16_t pulled_record_stopped;

    fx_spill(stack);
    fx_push(stack, record);
    drop_speed = fx_ldw((uint16_t)(record + 0x0030));
    fx_stw((uint16_t)(record + 0x0030), (uint16_t)(drop_speed + 0x003C));
    fx_stw((uint16_t)(record + 0x0032), 0);
    fx_stw((uint16_t)(record + 0x0032), 0);
    next_entry_end_ptr = FxRecordMove(3, record);
    half_x = fx_ldw((uint16_t)(record + FXREC_HALF_X));
    record_angle_22 = fx_ldw((uint16_t)(record + 0x0022));
    fx_push(stack, half_x);
    fx_push(stack, record_angle_22);
    { FxRecordCollideResult r_ = FxRecordCollide(place_x, next_entry_end_ptr, last_other_ptr, record, record, stack, rom_at); up_ = r_.up; world_sum_x_out = r_.world_sum_x_out; world_sum_y_out = r_.world_sum_y_out; push_flags_high_out = r_.push_flags_high_out; caller_scratch_back = r_.caller_scratch_back; record_out = r_.record_out; rom_at_out_2 = r_.rom_at_out; };
    if (up_) {                       /* it returned past us */
        return (FxObjectDropResult){world_sum_x_out, world_sum_y_out, push_flags_high_out, caller_scratch_back, record_out, rom_at_out_2, up_ == 4 ? 0 : up_ - 4};
    }
    restored_angle_22 = fx_pull(stack);
    half_x_2 = fx_pull(stack);
    hit_depth = fx_ldw(FX_HIT_DEPTH);
    if (hit_depth != 0) {
        fx_stw((uint16_t)(record_out + FXREC_HANDLER), 0);
        pulled_record_stopped = fx_pull(stack);
        fx_unspill(stack);
        return (FxObjectDropResult){world_sum_x_out, world_sum_y_out, push_flags_high_out, pulled_record_stopped, record_out, rom_at_out_2, 0};
    } else {
        fx_stw((uint16_t)(record_out + FXREC_HALF_X), half_x_2);
        fx_stw((uint16_t)(record_out + 0x0022), restored_angle_22);
        pulled_record_kept = fx_pull(stack);
        fx_unspill(stack);
        return (FxObjectDropResult){world_sum_x_out, world_sum_y_out, push_flags_high_out, pulled_record_kept, record_out, rom_at_out_2, 0};
    }
}


/*
 * @routine 04:91FF FxRecordCollide
 * WHAT IT DOES: Collides record R9 with each record in its touch list
 * (+$18): with the other's model (FxCollideModel) unless the other's
 * flags (+$16) have bit 4, else as a box (FxCollidePush); the pushes
 * found are applied (FxPushesApply, FxPushApply), and $239C keeps the
 * first hit's depth. Clears bit 0 of R9's +$1C first and bit 1 of each
 * other's +$10.
 * HOW: Pushes R11 and each list entry on the R10 stack; $247E and
 * $239C cleared and the sums cleared (FxSumsClear) first; the next
 * entry by +$0E; returns by pulling R15.
 * Entry: R9 = the record; R10 = stack; R11 = return address.
 * Exit: $239C = the hit depth (0: none); the rest as the last
 * collision leaves them.
 */
FxRecordCollideResult FxRecordCollide(uint16_t place_x, uint16_t place_y, uint16_t pose_aux, uint16_t other_record, uint16_t record, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t y;
    uint16_t list;
    uint16_t next;
    uint16_t flags10;
    uint16_t flags16;
    uint16_t flags1c;
    uint16_t flags10_2;
    uint16_t flags1c_2;
    uint16_t hit_depth;
    uint16_t list_face;
    uint16_t place_y_2;
    uint16_t push_flags;
    uint16_t hit_depth_2;
    uint16_t push_amount;
    uint16_t world_sum_x;
    uint16_t world_sum_y;
    uint16_t rom_at_out_2;
    uint16_t share_before;
    uint16_t collide_model;
    uint16_t contact_words;
    uint16_t push_amount_2;
    uint16_t push_amount_3;
    uint16_t applied_skipped;
    uint16_t pulled_list_record;
    uint16_t record_contact_flags;
    uint16_t push_flags_high_out_2;

    fx_spill(stack);
    fx_stw(FX_PUSH_TURN, 0);
    fx_stw(FX_HIT_DEPTH, 0);
    FxSumsClear();
    flags1c = fx_ldw((uint16_t)(record + FXREC_FLAGS1C));
    fx_sbk((uint16_t)(flags1c & ~1));
    list = fx_ldw((uint16_t)(record + FXREC_LIST));
    contact_words = list;
    if (list != 0) {
        for (;;) {
            fx_push(stack, contact_words);
            collide_model = fx_ldw((uint16_t)(contact_words + 0x000C));
            fx_stw(FX_COLLIDE_MODEL, collide_model);
            flags10 = fx_ldw((uint16_t)(contact_words + FXREC_FLAGS10));
            fx_sbk((uint16_t)(flags10 & ~2));
            fx_stw(FX_PUSH_AMOUNT, 0);
            flags16 = fx_ldw((uint16_t)(collide_model + FXREC_FLAGS16));
            if ((flags16 & 0x0010) != 0) {
                { FxCollidePushResult r_ = FxCollidePush(contact_words, collide_model, record, stack); share_before = r_.share_before; place_y_2 = r_.place_y; };
                { FxPushApplyResult r_ = FxPushApply(0x23A4, share_before, place_y_2, collide_model, stack, rom_at); place_x = r_.place_x; place_y = r_.place_y; pose_aux = r_.push_flags_high_out; rom_at = r_.rom_at_out; };
                push_amount = fx_ldw(FX_PUSH_AMOUNT);
                hit_depth = fx_ldw(FX_HIT_DEPTH);
                if (hit_depth != 0) {
                } else {
                    fx_stw(FX_HIT_DEPTH, push_amount);
                }
                if (push_amount == 0) {
                    pulled_list_record = fx_pull(stack);
                } else {
                    pulled_list_record = fx_pull(stack);
                    push_flags = fx_ldw(FX_PUSH_FLAGS);
                    if ((push_flags & 0x0020) == 0) {
                    } else {
                        record_contact_flags = fx_ldw((uint16_t)(record + 0x002C));
                        fx_sbk((0x0020 | record_contact_flags));
                    }
                    flags10_2 = fx_ldw((uint16_t)(pulled_list_record + FXREC_FLAGS10));
                    fx_sbk((2 | flags10_2));
                }
            } else {
                { FxCollideModelResult r_ = FxCollideModel(place_x, place_y, collide_model, other_record, record, stack, rom_at); up_ = r_.up; list_face = r_.list_face; place_x = r_.quotient; place_y = r_.place_y_out; pose_aux = r_.model_record_out; other_record = r_.caller_scratch_back; record = r_.record_back; rom_at = r_.rom_at_out; };
                if (up_) {                       /* it returned past us */
                    return (FxRecordCollideResult){place_x, place_y, pose_aux, other_record, record, rom_at, up_ == 2 ? 0 : up_ - 2};
                }
                if (list_face == 0) {
                    pulled_list_record = fx_pull(stack);
                } else {
                    { FxPushesApplyResult r_ = FxPushesApply(place_x, place_y, pose_aux, stack, rom_at); place_x = r_.place_x; place_y = r_.place_y; pose_aux = r_.push_amount_out; rom_at = r_.rom_at_out; };
                    push_amount_2 = fx_ldw(FX_PUSH_AMOUNT);
                    push_amount = (uint16_t)((uint16_t)~push_amount_2 + 1);
                    hit_depth = fx_ldw(FX_HIT_DEPTH);
                    if (hit_depth != 0) {
                    } else {
                        fx_stw(FX_HIT_DEPTH, push_amount);
                    }
                    if (push_amount == 0) {
                        pulled_list_record = fx_pull(stack);
                    } else {
                        pulled_list_record = fx_pull(stack);
                        push_flags = fx_ldw(FX_PUSH_FLAGS);
                        if ((push_flags & 0x0020) == 0) {
                        } else {
                            record_contact_flags = fx_ldw((uint16_t)(record + 0x002C));
                            fx_sbk((0x0020 | record_contact_flags));
                        }
                        flags10_2 = fx_ldw((uint16_t)(pulled_list_record + FXREC_FLAGS10));
                        fx_sbk((2 | flags10_2));
                    }
                }
            }
            next = fx_ldw((uint16_t)(pulled_list_record + FXREC_NEXT));
            contact_words = next;
            if (next == 0) {
                break;
            } else {
                continue;
            }
        }
    } else {
    }
    y = fx_ldw((uint16_t)(record + FXREC_Y));
    FxPushSet(y);
    { FxPushApplyResult r_ = FxPushApply(0x23A4, place_x, place_y, pose_aux, stack, rom_at); applied_skipped = r_.applied_skipped; push_flags_high_out_2 = r_.push_flags_high_out; rom_at_out_2 = r_.rom_at_out; };
    if (applied_skipped == 0) {
    } else {
        flags1c_2 = fx_ldw((uint16_t)(record + FXREC_FLAGS1C));
        fx_sbk((flags1c_2 | 1));
        hit_depth_2 = fx_ldw(FX_HIT_DEPTH);
        if (hit_depth_2 != 0) {
        } else {
            push_amount_3 = fx_ldw(FX_PUSH_AMOUNT);
            fx_stw(FX_HIT_DEPTH, push_amount_3);
        }
    }
    { FxSumsApplyResult r_ = FxSumsApply(record); world_sum_x = r_.world_sum_x; world_sum_y = r_.world_sum_y; };
    fx_unspill(stack);
    return (FxRecordCollideResult){world_sum_x, world_sum_y, push_flags_high_out_2, other_record, record, rom_at_out_2, 0};
}


/*
 * @routine 04:957C FxContactsAnyTouch
 * WHAT IT DOES: Whether any of the twenty contact slots at $23AE holds
 * a touch: R0 = 1 at the first slot in use whose depth word is neither
 * $7D00 nor negative, else 0. Not reached in the traces so far.
 * HOW: CACHE; R13 counts the slots to $14, R3 walks them (8 past an
 * empty one, 6 past a counted one); returns by JMP R11 (MOVE R15,
 * R11).
 * Entry: R11 = return address.
 * Exit: R0 = 1 or 0; R3 at the slot reached; R4 its depth; R13 the
 * slot count.
 */
FxContactsAnyTouchResult FxContactsAnyTouch(uint16_t place_x)
{
    uint16_t r_t_6;
    uint16_t slot_ptr;
    uint16_t slot_flags;
    uint16_t slots_checked;
    uint16_t slot_depth_ptr;
    uint16_t slot_after_depth_ptr;

    slot_ptr = 0x23AE;
    slots_checked = 0;
    for (;;) {
        slots_checked = (uint16_t)(slots_checked + 1);
        if ((uint16_t)((uint16_t)(0 - slots_checked) + 0x0014) == 0) {
            return (FxContactsAnyTouchResult){0, place_x};
        } else {
            slot_flags = fx_ldw(slot_ptr);
            slot_depth_ptr = (uint16_t)(slot_ptr + 2);
            if (slot_flags == 0) {
                slot_ptr = (uint16_t)(slot_depth_ptr + 8);
                continue;
            } else {
                place_x = fx_ldw(slot_depth_ptr);
                slot_after_depth_ptr = (uint16_t)(slot_depth_ptr + 2);
                if ((uint16_t)((uint16_t)(0 - place_x) + 0x7D00) == 0) {
                    slot_ptr = (uint16_t)(slot_after_depth_ptr + 6);
                    continue;
                } else {
                    r_t_6 = place_x;
                    place_x = r_t_6;
                    if (!(r_t_6 & 0x8000)) {
                        break;
                    } else {
                        slot_ptr = (uint16_t)(slot_after_depth_ptr + 6);
                        continue;
                    }
                }
            }
        }
    }
    return (FxContactsAnyTouchResult){1, place_x};
}


/*
 * @routine 04:9C3D FxCollideModel
 * WHAT IT DOES: Collides record R9 with the model of record R7 (its
 * shape's +$14 in ROM): R9's place relative to R7's is turned into the
 * model's axes (by quarter turns, or FxTurn for other headings), then
 * each face of the model's list is tested against it: a face the place
 * lies behind, by less than the face's best so far, is kept in one of
 * the twenty contact slots at $23AE (its flags, depth and normal). R0
 * = 0 when the model has no collision list.
 * HOW: Pushes R11, R8 and R9; the slots' depth words set to $7D00
 * first; the face corners from the point table at $2488 (FxModelPoints
 * when the model or its scale changed), the normals by FxNormalize and
 * LMULT cross products, the planes by FMULT; returns by pulling R15.
 * Entry: R7 = the model's record, R9 = the record; R10 = stack; R11 =
 * return address.
 * Exit: R0 = 0 (no list) or as the face walk leaves it; R8, R9 back;
 * the slots filled.
 */
FxCollideModelResult FxCollideModel(uint16_t place_x, uint16_t place_y, uint16_t model_record, uint16_t caller_scratch, uint16_t record, uint16_t *stack, uint16_t rom_at)
{
    int again_l9E92 = 0;
    uint16_t k;
    uint16_t y;
    uint16_t a2;
    uint16_t a4;
    uint16_t y_2;
    uint16_t a2_2;
    uint16_t a2_3;
    uint16_t a2_4;
    uint16_t a2_5;
    uint16_t a4_2;
    uint16_t a4_3;
    uint16_t a4_4;
    uint16_t a4_5;
    uint16_t count;
    uint16_t new_z;
    uint16_t pitch;
    uint16_t half_x;
    uint16_t r_t_27;
    uint16_t r_t_28;
    uint16_t script;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t count_4;
    uint16_t delta_y;
    uint16_t delta_z;
    uint16_t edge1_x;
    uint16_t edge1_y;
    uint16_t edge1_z;
    uint16_t model_z;
    uint16_t new_x_x;
    uint16_t half_x_2;
    uint16_t normal_x;
    uint16_t record_z;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t rom_at_4;
    uint16_t rom_at_5;
    uint16_t rom_at_6;
    uint16_t rom_at_7;
    uint16_t rom_at_8;
    uint16_t vector_x;
    uint16_t vector_y;
    uint16_t vector_z;
    uint16_t fxpart_12;
    uint16_t init_2380;
    uint16_t init_2382;
    uint16_t list_size;
    uint16_t corner_a_x;
    uint16_t corner_a_y;
    uint16_t corner_a_z;
    uint16_t corner_b_x;
    uint16_t corner_c_x;
    uint16_t corner_c_y;
    uint16_t corner_c_z;
    uint16_t face_flags;
    uint16_t face_plane;
    uint16_t face_scale;
    uint16_t model_turn;
    uint32_t prod_x2_y1;
    uint32_t prod_x2_z1;
    uint32_t prod_y2_x1;
    uint32_t prod_y2_z1;
    uint32_t prod_z2_x1;
    uint32_t prod_z2_y1;
    uint16_t stored_a_x;
    uint16_t stored_a_y;
    uint16_t stored_a_z;
    uint16_t stored_b_x;
    uint16_t vector_x_2;
    uint16_t face_normal;
    uint16_t init_2380_2;
    uint16_t init_2382_2;
    uint16_t points_bank;
    uint16_t turned_x_x2;
    uint16_t turned_z_x2;
    uint16_t cross_x_diff;
    uint16_t cross_y_diff;
    uint16_t cross_z_diff;
    uint16_t delta_half_x;
    uint16_t face_flags_2;
    uint16_t face_flags_3;
    uint16_t face_flags_4;
    uint16_t list_size_lo;
    uint16_t corner_a_addr;
    uint16_t corner_b_addr;
    uint16_t corner_c_addr;
    uint16_t edge1_x_again;
    uint16_t edge1_y_again;
    uint16_t edge1_z_again;
    uint16_t face_body_len;
    uint16_t face_list_ptr;
    uint16_t face_normal_x;
    uint16_t face_normal_y;
    uint16_t face_normal_z;
    uint16_t high_words_hi;
    uint16_t next_face_tag;
    uint16_t prod_x2_y1_lo;
    uint16_t prod_x2_z1_lo;
    uint16_t prod_y2_x1_lo;
    uint16_t prod_y2_z1_lo;
    uint16_t prod_z2_x1_lo;
    uint16_t prod_z2_y1_lo;
    uint16_t corner_a_index;
    uint16_t corner_b_index;
    uint16_t corner_c_index;
    uint16_t face_flag_byte;
    uint16_t first_face_ptr;
    uint16_t first_face_tag;
    uint16_t plane_distance;
    uint16_t slot_scan_addr;
    uint16_t slot_scan_word;
    uint16_t corner_a_y_addr;
    uint16_t corner_b_y_addr;
    uint16_t corner_c_y_addr;
    uint16_t face_list_count;
    uint16_t face_ptr_skip_1;
    uint16_t face_ptr_skip_2;
    uint16_t list_header_ptr;
    uint16_t model_part_word;
    uint16_t point_table_ptr;
    uint16_t points_ptr_copy;
    uint16_t record_back_hit;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_y;
    uint16_t scaled_vector_z;
    uint16_t slot_depth_word;
    uint16_t slot_flags_word;
    uint16_t slot_hit_result;
    uint16_t slot_scan_model;
    uint16_t corner_index_ptr;
    uint16_t face_list_ptr_lo;
    uint16_t face_normal_addr;
    uint16_t face_scale_shift;
    uint16_t points_script_lo;
    uint16_t record_back_none;
    uint16_t stored_b_x_again;
    uint16_t contact_slot_addr;
    uint16_t face_list_hdr_ptr;
    uint16_t face_normal_index;
    uint16_t scaled_vector_x_2;
    uint16_t scaled_vector_y_2;
    uint16_t scaled_vector_z_2;
    uint16_t slot_flags_word_2;
    uint16_t slot_flags_word_3;
    uint16_t caller_scratch_hit;
    uint16_t collision_list_ptr;
    uint16_t corner_index_ptr_b;
    uint16_t corner_index_ptr_c;
    uint16_t face_body_len_kept;
    uint16_t face_normal_y_addr;
    uint16_t face_ptr_after_len;
    uint16_t fixed_normal_y_ptr;
    uint16_t fixed_normal_z_ptr;
    uint16_t caller_scratch_none;
    uint16_t face_list_count_ptr;
    uint16_t face_list_pairs_ptr;
    uint16_t face_ptr_after_body;
    uint16_t fixed_normal_x_byte;
    uint16_t fixed_normal_y_byte;
    uint16_t fixed_normal_z_byte;
    uint16_t face_normal_y_scaled;
    uint16_t face_normal_z_scaled;
    uint16_t face_ptr_after_flags;

    fx_spill(stack);
    fx_push(stack, caller_scratch);
    fx_push(stack, record);
    fxpart_12 = fx_ldw((uint16_t)(model_record + FXPART_CHILD));
    fx_stw(0x0044, fxpart_12);
    fx_stw(FX_PUSHES, 0);
    fx_stw(0x23B0, 0x7D00);
    fx_stw(0x23B8, 0);
    fx_stw(0x23BA, 0x7D00);
    fx_stw(0x23C2, 0);
    fx_stw(0x23C4, 0x7D00);
    fx_stw(0x23CC, 0);
    fx_stw(0x23CE, 0x7D00);
    fx_stw(0x23D6, 0);
    fx_stw(0x23D8, 0x7D00);
    fx_stw(0x23E0, 0);
    fx_stw(0x23E2, 0x7D00);
    fx_stw(0x23EA, 0);
    fx_stw(0x23EC, 0x7D00);
    fx_stw(0x23F4, 0);
    fx_stw(0x23F6, 0x7D00);
    fx_stw(0x23FE, 0);
    fx_stw(0x2400, 0x7D00);
    fx_stw(0x2408, 0);
    fx_stw(0x240A, 0x7D00);
    fx_stw(0x2412, 0);
    fx_stw(0x2414, 0x7D00);
    fx_stw(0x241C, 0);
    fx_stw(0x241E, 0x7D00);
    fx_stw(0x2426, 0);
    fx_stw(0x2428, 0x7D00);
    fx_stw(0x2430, 0);
    fx_stw(0x2432, 0x7D00);
    fx_stw(0x243A, 0);
    fx_stw(0x243C, 0x7D00);
    fx_stw(0x2444, 0);
    fx_stw(0x2446, 0x7D00);
    fx_stw(0x244E, 0);
    fx_stw(0x2450, 0x7D00);
    fx_stw(0x2458, 0);
    fx_stw(0x245A, 0x7D00);
    fx_stw(0x2462, 0);
    fx_stw(0x2464, 0x7D00);
    fx_romb(0);
    fx_table_bank = 0x00;
    collision_list_ptr = fx_ldw((uint16_t)(model_record + 0x0014));
    init_2380 = collision_list_ptr;
    if (collision_list_ptr != 0) {
        rom_at_2 = init_2380;
        fx_r14(rom_at_2);
        list_header_ptr = (uint16_t)(rom_at_2 + 0x000C);
        fx_r14(list_header_ptr);
        list_size_lo = fx_getb();
        rom_at = (uint16_t)(list_header_ptr + 1);
        fx_r14(rom_at);
        list_size = (uint16_t)((list_size_lo & 0xFF) | fx_getb() << 8);
        if (list_size != 0) {
            rom_at_3 = init_2380;
            fx_r14(rom_at_3);
            half_x = fx_ldw((uint16_t)(record + FXREC_HALF_X));
            half_x_2 = fx_ldw((uint16_t)(model_record + FXREC_HALF_X));
            delta_half_x = (uint16_t)(half_x - half_x_2);
            fx_stw(0x0020, delta_half_x);
            y = fx_ldw((uint16_t)(record + FXREC_Y));
            y_2 = fx_ldw((uint16_t)(model_record + FXREC_Y));
            delta_y = (uint16_t)(y - y_2);
            fx_stw(0x0022, delta_y);
            record_z = fx_ldw((uint16_t)(record + 0x0022));
            model_z = fx_ldw((uint16_t)(model_record + 0x0022));
            delta_z = (uint16_t)(record_z - model_z);
            fx_stw(0x0024, delta_z);
            fx_stw(FX_PUSH_TURN, 0);
            pitch = fx_ldw((uint16_t)(model_record + FXREC_HEADING));
            if (pitch != 0) {
                model_turn = (uint16_t)(pitch >> 8);
                fx_stw(FX_PUSH_TURN, model_turn);
                model_record = 0x00C0;
                if (model_turn != model_record) {
                    model_record = 0x0040;
                    if (model_turn != model_record) {
                        model_record = 0x0080;
                        if (model_turn != model_record) {
                            fx_push(stack, rom_at_3);
                            fx_push(stack, init_2380);
                            fx_spill(stack);
                            fx_spill(stack);
                            fx_push(stack, delta_y);
                            fx_push(stack, model_record);
                            fx_push(stack, record);
                            fx_romb(0);
                            { FxTurned t = FxTurn((int16_t)delta_half_x, (uint8_t)((uint16_t)~model_turn + 1), (int16_t)delta_z, 0x00); new_x_x = (uint16_t)t.x; new_z = (uint16_t)t.z; };
                            turned_x_x2 = (uint16_t)(new_x_x + new_x_x);
                            turned_z_x2 = (uint16_t)(new_z + new_z);
                            delta_half_x = (uint16_t)(turned_z_x2 + turned_z_x2);
                            delta_z = (uint16_t)(turned_x_x2 + turned_x_x2);
                            fx_unspill(stack);
                            model_record = fx_pull(stack);
                            delta_y = fx_pull(stack);
                            fx_unspill(stack);
                            fx_unspill(stack);
                            init_2380 = fx_pull(stack);
                            rom_at_3 = fx_pull(stack);
                            fx_r14(rom_at_3);
                            fx_romb(0);
                        } else {
                            delta_half_x = (uint16_t)((uint16_t)~delta_half_x + 1);
                            delta_z = (uint16_t)((uint16_t)~delta_z + 1);
                        }
                    } else {
                        r_t_27 = (uint16_t)((uint16_t)~delta_half_x + 1);
                        delta_half_x = delta_z;
                        delta_z = r_t_27;
                    }
                } else {
                    r_t_28 = (uint16_t)((uint16_t)~delta_z + 1);
                    delta_z = delta_half_x;
                    delta_half_x = r_t_28;
                }
            } else {
            }
            face_list_hdr_ptr = (uint16_t)(rom_at_3 + 3);
            fx_r14(face_list_hdr_ptr);
            face_list_ptr_lo = fx_getb();
            fx_r14((uint16_t)(face_list_hdr_ptr + 1));
            face_list_ptr = (uint16_t)((face_list_ptr_lo & 0xFF) | fx_getb() << 8);
            rom_at_4 = init_2380;
            fx_r14(rom_at_4);
            points_ptr_copy = rom_at_4;
            fx_r14(points_ptr_copy);
            points_script_lo = fx_getb();
            fx_r14((uint16_t)(points_ptr_copy + 1));
            script = (uint16_t)((points_script_lo & 0xFF) | fx_getb() << 8);
            rom_at_5 = init_2380;
            fx_r14(rom_at_5);
            fx_r14((uint16_t)(rom_at_5 + 7));
            face_scale_shift = (uint16_t)(int16_t)(int8_t)fx_getb();
            count = face_scale_shift;
            face_scale = 1;
            for (;;) {
                face_scale = (uint16_t)(face_scale + face_scale);
                count = (uint16_t)(count - 1);
                if (count != 0) {
                    continue;
                } else {
                    break;
                }
            }
            fx_stw(FX_FACE_SCALE, face_scale);
            rom_at_6 = init_2380;
            fx_r14(rom_at_6);
            fx_r14((uint16_t)(rom_at_6 + 2));
            points_bank = fx_getb();
            fx_romb(points_bank);
            fx_table_bank = (uint8_t)(points_bank & 0x7F);
            init_2382 = fx_ldw(0x0044);
            init_2380_2 = fx_ldw(FX_INIT_2380);
            if (init_2380_2 != init_2380) {
                fx_stw(FX_INIT_2380, init_2380);
                fx_stw(FX_INIT_2382, init_2382);
                FxModelPoints(script, stack);
            } else {
                init_2382_2 = fx_ldw(FX_INIT_2382);
                if (init_2382_2 == init_2382) {
                } else {
                    fx_stw(FX_INIT_2380, init_2380);
                    fx_stw(FX_INIT_2382, init_2382);
                    FxModelPoints(script, stack);
                }
            }
            point_table_ptr = 0x2488;
            fx_r14(face_list_ptr);
            face_list_count_ptr = (uint16_t)(face_list_ptr + 1);
            fx_r14(face_list_count_ptr);
            face_list_count = fx_getb();
            face_list_pairs_ptr = (uint16_t)(face_list_count_ptr + 1);
            fx_r14(face_list_pairs_ptr);
            first_face_ptr = (uint16_t)(face_list_pairs_ptr + (uint16_t)((uint8_t)face_list_count * (uint8_t)3));
            fx_r14(first_face_ptr);
            first_face_tag = (uint16_t)(int16_t)(int8_t)fx_getb();
            rom_at_7 = (uint16_t)(first_face_ptr + 1);
            fx_r14(rom_at_7);
            for (;;) {                 /* entered again at the first way */
                if (again_l9E92 || (first_face_tag != 0x003C)) {
                    for (;;) {
                        face_body_len = (uint16_t)(int16_t)(int8_t)fx_getb();
                        face_body_len_kept = face_body_len;
                        face_ptr_after_len = (uint16_t)(rom_at_7 + 1);
                        fx_r14(face_ptr_after_len);
                        if (face_body_len & 0x8000) {
                            break;
                        } else {
                            face_flag_byte = (uint16_t)(int16_t)(int8_t)fx_getb();
                            face_ptr_after_flags = (uint16_t)(face_ptr_after_len + 1);
                            fx_r14(face_ptr_after_flags);
                            if (face_flag_byte != 0) {
                                fx_stw(FX_FACE_FLAGS, face_flag_byte);
                                contact_slot_addr = (uint16_t)((uint16_t)((int8_t)0x000A * (int8_t)(uint16_t)(face_flag_byte & ~0x00E0)) + 0x23A4);
                                slot_flags_word = fx_ldw(contact_slot_addr);
                                if ((uint16_t)(slot_flags_word >> 8) == 0) {
                                    face_ptr_skip_1 = (uint16_t)(face_ptr_after_flags + 1);
                                    fx_r14(face_ptr_skip_1);
                                    face_ptr_skip_2 = (uint16_t)(face_ptr_skip_1 + 1);
                                    fx_r14(face_ptr_skip_2);
                                    model_part_word = fx_ldw(0x0044);
                                    if (model_part_word != 0) {
                                        face_flags = fx_ldw(FX_FACE_FLAGS);
                                        if ((0x0040 & face_flags) != 0) {
                                            corner_index_ptr = (uint16_t)(face_ptr_skip_2 + 3);
                                            fx_r14(corner_index_ptr);
                                            fx_push(stack, corner_index_ptr);
                                            count_2 = point_table_ptr;
                                            corner_a_index = fx_getb();
                                            corner_index_ptr_b = (uint16_t)(corner_index_ptr + 1);
                                            fx_r14(corner_index_ptr_b);
                                            corner_a_addr = (uint16_t)((uint16_t)((uint8_t)corner_a_index * (uint8_t)6) + count_2);
                                            corner_a_x = fx_ldw(corner_a_addr);
                                            corner_a_y_addr = (uint16_t)(corner_a_addr + 2);
                                            fx_stw(0x0068, corner_a_x);
                                            corner_a_y = fx_ldw(corner_a_y_addr);
                                            fx_stw(0x002C, corner_a_y);
                                            corner_a_z = fx_ldw((uint16_t)(corner_a_y_addr + 2));
                                            fx_stw(0x002E, corner_a_z);
                                            corner_b_index = fx_getb();
                                            corner_index_ptr_c = (uint16_t)(corner_index_ptr_b + 1);
                                            fx_r14(corner_index_ptr_c);
                                            corner_b_addr = (uint16_t)((uint16_t)((uint8_t)corner_b_index * (uint8_t)6) + count_2);
                                            corner_b_x = fx_ldw(corner_b_addr);
                                            corner_b_y_addr = (uint16_t)(corner_b_addr + 2);
                                            fx_stw(0x006A, corner_b_x);
                                            a2 = fx_ldw(corner_b_y_addr);
                                            fx_stw(GSU_IN_A2, a2);
                                            a4 = fx_ldw((uint16_t)(corner_b_y_addr + 2));
                                            fx_stw(GSU_IN_A4, a4);
                                            corner_c_index = fx_getb();
                                            fx_r14((uint16_t)(corner_index_ptr_c + 1));
                                            corner_c_addr = (uint16_t)((uint16_t)((uint8_t)corner_c_index * (uint8_t)6) + count_2);
                                            corner_c_x = fx_ldw(corner_c_addr);
                                            corner_c_y_addr = (uint16_t)(corner_c_addr + 2);
                                            corner_c_y = fx_ldw(corner_c_y_addr);
                                            corner_c_z = fx_ldw((uint16_t)(corner_c_y_addr + 2));
                                            stored_a_x = fx_ldw(0x0068);
                                            stored_b_x = fx_ldw(0x006A);
                                            fx_stw(0x0068, (uint16_t)(stored_a_x - stored_b_x));
                                            stored_a_y = fx_ldw(0x002C);
                                            a2_2 = fx_ldw(GSU_IN_A2);
                                            fx_stw(0x002C, (uint16_t)(stored_a_y - a2_2));
                                            stored_a_z = fx_ldw(0x002E);
                                            a4_2 = fx_ldw(GSU_IN_A4);
                                            fx_stw(0x002E, (uint16_t)(stored_a_z - a4_2));
                                            stored_b_x_again = fx_ldw(0x006A);
                                            fx_stw(0x006A, (uint16_t)(stored_b_x_again - corner_c_x));
                                            a2_3 = fx_ldw(GSU_IN_A2);
                                            fx_stw(GSU_IN_A2, (uint16_t)(a2_3 - corner_c_y));
                                            a4_3 = fx_ldw(GSU_IN_A4);
                                            fx_stw(GSU_IN_A4, (uint16_t)(a4_3 - corner_c_z));
                                            fx_push(stack, delta_half_x);
                                            fx_push(stack, delta_y);
                                            fx_push(stack, delta_z);
                                            fx_push(stack, count_2);
                                            fx_push(stack, face_body_len_kept);
                                            fx_push(stack, contact_slot_addr);
                                            vector_x = fx_ldw(0x0068);
                                            vector_y = fx_ldw(0x002C);
                                            vector_z = fx_ldw(0x002E);
                                            { FxNormalized n_; if (FxNormalize((FxVec3){(int16_t)vector_x, (int16_t)vector_y, (int16_t)vector_z}, &n_)) { scaled_vector_x = (uint16_t)n_.unit.x; scaled_vector_y = (uint16_t)n_.unit.y; scaled_vector_z = (uint16_t)n_.unit.z; high_words_hi = (uint16_t)n_.x_high; k = (uint16_t)n_.shift; } else { scaled_vector_x = 0; scaled_vector_y = 0; scaled_vector_z = 0; high_words_hi = face_flags; k = a4_2; } };
                                            fx_stw(0x0068, scaled_vector_x);
                                            fx_stw(0x002C, scaled_vector_y);
                                            fx_stw(0x002E, scaled_vector_z);
                                            vector_x_2 = fx_ldw(0x006A);
                                            a2_4 = fx_ldw(GSU_IN_A2);
                                            a4_4 = fx_ldw(GSU_IN_A4);
                                            { FxNormalized n_; if (FxNormalize((FxVec3){(int16_t)vector_x_2, (int16_t)a2_4, (int16_t)a4_4}, &n_)) { scaled_vector_x_2 = (uint16_t)n_.unit.x; scaled_vector_y_2 = (uint16_t)n_.unit.y; scaled_vector_z_2 = (uint16_t)n_.unit.z; } else { scaled_vector_x_2 = 0; scaled_vector_y_2 = 0; scaled_vector_z_2 = 0; } };
                                            edge1_y = fx_ldw(0x002C);
                                            prod_z2_y1 = (uint32_t)((int32_t)(int16_t)scaled_vector_z_2 * (int16_t)edge1_y);
                                            prod_z2_y1_lo = (uint16_t)prod_z2_y1;
                                            edge1_z = fx_ldw(0x002E);
                                            prod_y2_z1 = (uint32_t)((int32_t)(int16_t)scaled_vector_y_2 * (int16_t)edge1_z);
                                            prod_y2_z1_lo = (uint16_t)prod_y2_z1;
                                            cross_x_diff = (uint16_t)(prod_z2_y1_lo - prod_y2_z1_lo);
                                            fx_stw(0x006A, (uint16_t)((uint16_t)((uint16_t)(prod_z2_y1 >> 16) - (uint16_t)(prod_y2_z1 >> 16) - (prod_z2_y1_lo < prod_y2_z1_lo)) << 1 | ((uint32_t)cross_x_diff + cross_x_diff > 0xFFFF)));
                                            edge1_z_again = fx_ldw(0x002E);
                                            prod_x2_z1 = (uint32_t)((int32_t)(int16_t)scaled_vector_x_2 * (int16_t)edge1_z_again);
                                            prod_x2_z1_lo = (uint16_t)prod_x2_z1;
                                            edge1_x = fx_ldw(0x0068);
                                            prod_z2_x1 = (uint32_t)((int32_t)(int16_t)scaled_vector_z_2 * (int16_t)edge1_x);
                                            prod_z2_x1_lo = (uint16_t)prod_z2_x1;
                                            cross_y_diff = (uint16_t)(prod_x2_z1_lo - prod_z2_x1_lo);
                                            fx_stw(GSU_IN_A2, (uint16_t)((uint16_t)((uint16_t)(prod_x2_z1 >> 16) - (uint16_t)(prod_z2_x1 >> 16) - (prod_x2_z1_lo < prod_z2_x1_lo)) << 1 | ((uint32_t)cross_y_diff + cross_y_diff > 0xFFFF)));
                                            edge1_x_again = fx_ldw(0x0068);
                                            prod_y2_x1 = (uint32_t)((int32_t)(int16_t)scaled_vector_y_2 * (int16_t)edge1_x_again);
                                            prod_y2_x1_lo = (uint16_t)prod_y2_x1;
                                            edge1_y_again = fx_ldw(0x002C);
                                            prod_x2_y1 = (uint32_t)((int32_t)(int16_t)scaled_vector_x_2 * (int16_t)edge1_y_again);
                                            prod_x2_y1_lo = (uint16_t)prod_x2_y1;
                                            cross_z_diff = (uint16_t)(prod_y2_x1_lo - prod_x2_y1_lo);
                                            fx_stw(GSU_IN_A4, (uint16_t)((uint16_t)((uint16_t)(prod_y2_x1 >> 16) - (uint16_t)(prod_x2_y1 >> 16) - (prod_y2_x1_lo < prod_x2_y1_lo)) << 1 | ((uint32_t)cross_z_diff + cross_z_diff > 0xFFFF)));
                                            normal_x = fx_ldw(0x006A);
                                            a2_5 = fx_ldw(GSU_IN_A2);
                                            a4_5 = fx_ldw(GSU_IN_A4);
                                            contact_slot_addr = fx_pull(stack);
                                            face_body_len_kept = fx_pull(stack);
                                            count_3 = fx_pull(stack);
                                            delta_z = fx_pull(stack);
                                            delta_y = fx_pull(stack);
                                            delta_half_x = fx_pull(stack);
                                            point_table_ptr = count_3;
                                            rom_at_8 = fx_pull(stack);
                                            fx_r14(rom_at_8);
                                        } else {
                                            fixed_normal_x_byte = fx_getb();
                                            fixed_normal_y_ptr = (uint16_t)(face_ptr_skip_2 + 1);
                                            fx_r14(fixed_normal_y_ptr);
                                            normal_x = (uint16_t)((uint16_t)~(uint16_t)(fixed_normal_x_byte << 8 | fixed_normal_x_byte >> 8) + 1);
                                            fixed_normal_y_byte = fx_getb();
                                            fixed_normal_z_ptr = (uint16_t)(fixed_normal_y_ptr + 1);
                                            fx_r14(fixed_normal_z_ptr);
                                            a2_5 = (uint16_t)((uint16_t)~(uint16_t)(fixed_normal_y_byte << 8 | fixed_normal_y_byte >> 8) + 1);
                                            fixed_normal_z_byte = fx_getb();
                                            rom_at_8 = (uint16_t)(fixed_normal_z_ptr + 1);
                                            fx_r14(rom_at_8);
                                            a4_5 = (uint16_t)((uint16_t)~(uint16_t)(fixed_normal_z_byte << 8 | fixed_normal_z_byte >> 8) + 1);
                                        }
                                    } else {
                                        fixed_normal_x_byte = fx_getb();
                                        fixed_normal_y_ptr = (uint16_t)(face_ptr_skip_2 + 1);
                                        fx_r14(fixed_normal_y_ptr);
                                        normal_x = (uint16_t)((uint16_t)~(uint16_t)(fixed_normal_x_byte << 8 | fixed_normal_x_byte >> 8) + 1);
                                        fixed_normal_y_byte = fx_getb();
                                        fixed_normal_z_ptr = (uint16_t)(fixed_normal_y_ptr + 1);
                                        fx_r14(fixed_normal_z_ptr);
                                        a2_5 = (uint16_t)((uint16_t)~(uint16_t)(fixed_normal_y_byte << 8 | fixed_normal_y_byte >> 8) + 1);
                                        fixed_normal_z_byte = fx_getb();
                                        rom_at_8 = (uint16_t)(fixed_normal_z_ptr + 1);
                                        fx_r14(rom_at_8);
                                        a4_5 = (uint16_t)((uint16_t)~(uint16_t)(fixed_normal_z_byte << 8 | fixed_normal_z_byte >> 8) + 1);
                                    }
                                    model_record = fx_ldw(FX_FACE_SCALE);
                                    face_normal_index = (uint16_t)(int16_t)(int8_t)fx_getb();
                                    rom_at_7 = (uint16_t)(rom_at_8 + face_body_len_kept);
                                    fx_r14(rom_at_7);
                                    face_normal_addr = (uint16_t)(point_table_ptr + (uint16_t)((uint8_t)face_normal_index * (uint8_t)6));
                                    face_normal_x = fx_ldw(face_normal_addr);
                                    face_normal_y_addr = (uint16_t)(face_normal_addr + 2);
                                    fx_stw(FX_FACE_NORMAL, (uint16_t)((int8_t)face_normal_x * (int8_t)model_record));
                                    face_normal_y = fx_ldw(face_normal_y_addr);
                                    fx_stw(0x2374, (uint16_t)((int8_t)face_normal_y * (int8_t)model_record));
                                    face_normal_z = fx_ldw((uint16_t)(face_normal_y_addr + 2));
                                    fx_stw(0x2376, (uint16_t)((int8_t)face_normal_z * (int8_t)model_record));
                                    slot_depth_word = fx_ldw(contact_slot_addr);
                                    if ((uint16_t)(slot_depth_word >> 8) == 0) {
                                        fx_stw(FX_FACE_PLANE, (uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)a4_5 * (int16_t)delta_z) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_x * (int16_t)delta_half_x) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)a2_5 * (int16_t)delta_y) >> 16)));
                                        face_normal = fx_ldw(FX_FACE_NORMAL);
                                        face_normal_y_scaled = fx_ldw(0x2374);
                                        face_normal_z_scaled = fx_ldw(0x2376);
                                        face_plane = fx_ldw(FX_FACE_PLANE);
                                        plane_distance = (uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)a4_5 * (int16_t)face_normal_z_scaled) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_x * (int16_t)face_normal) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)a2_5 * (int16_t)face_normal_y_scaled) >> 16)) - face_plane);
                                        if (!(plane_distance & 0x8000)) {
                                            face_flags_2 = fx_ldw(FX_FACE_FLAGS);
                                            if ((0x0080 & face_flags_2) == 0) {
                                                { FxCollideModelDepthCompareResult tr_ = FxCollideModelDepthCompare(plane_distance, normal_x, a2_5, a4_5, delta_half_x, delta_y, delta_z, point_table_ptr, stack, contact_slot_addr, rom_at_7); return (FxCollideModelResult){tr_.contact_found, tr_.quotient, tr_.place_y_out, tr_.face_scale_out, tr_.exit_aux_record, tr_.exit_record, tr_.rom_at_out, tr_.up ? tr_.up == 3 ? 0 : tr_.up - 3 : 0}; }
                                            }
                                            face_flags_3 = fx_ldw(FX_FACE_FLAGS);
                                            model_record = (face_flags_3 & 0x00FF);
                                            slot_flags_word_3 = fx_ldw(contact_slot_addr);
                                            fx_sbk((slot_flags_word_3 | model_record));
                                            continue;
                                        } else {
                                            face_flags_4 = fx_ldw(FX_FACE_FLAGS);
                                            slot_flags_word_2 = fx_ldw(contact_slot_addr);
                                            model_record = 0xFF00;
                                            fx_sbk((((uint16_t)(face_flags_4 << 8 | face_flags_4 >> 8) | slot_flags_word_2) & model_record));
                                            continue;
                                        }
                                    } else {
                                        continue;
                                    }
                                } else {
                                    face_ptr_after_body = (uint16_t)(face_ptr_after_flags + face_body_len_kept);
                                    fx_r14(face_ptr_after_body);
                                    rom_at_7 = (uint16_t)(face_ptr_after_body + 5);
                                    fx_r14(rom_at_7);
                                    continue;
                                }
                            } else {
                                face_ptr_after_body = (uint16_t)(face_ptr_after_flags + face_body_len_kept);
                                fx_r14(face_ptr_after_body);
                                rom_at_7 = (uint16_t)(face_ptr_after_body + 5);
                                fx_r14(rom_at_7);
                                continue;
                            }
                        }
                    }
                    next_face_tag = (uint16_t)(int16_t)(int8_t)fx_getb();
                    rom_at_7 = (uint16_t)(face_ptr_after_len + 1);
                    fx_r14(rom_at_7);
                } else {
                    model_record = 5;
                    for (;;) {
                        next_face_tag = (uint16_t)(int16_t)(int8_t)fx_getb();
                        if (next_face_tag != 0x0040) {
                            if (next_face_tag != 0x0028) {
                                if (next_face_tag != 0x0044) {
                                    break;
                                } else {
                                    rom_at_7 = (uint16_t)(rom_at_7 + 3);
                                    fx_r14(rom_at_7);
                                    continue;
                                }
                            } else {
                                rom_at_7 = (uint16_t)(rom_at_7 + model_record);
                                fx_r14(rom_at_7);
                                continue;
                            }
                        } else {
                            rom_at_7 = (uint16_t)(rom_at_7 + 1);
                            fx_r14(rom_at_7);
                            continue;
                        }
                    }
                    rom_at_7 = (uint16_t)(rom_at_7 + 1);
                    fx_r14(rom_at_7);
                }
                if (next_face_tag != 0) {
                    again_l9E92 = 1;
                    continue;
                }
                break;
            }
            slot_scan_addr = 0x23A4;
            count_4 = 0x0013;
            for (;;) {
                slot_scan_addr = (uint16_t)(slot_scan_addr + 0x000A);
                slot_scan_word = fx_ldw(slot_scan_addr);
                if (slot_scan_word == 0) {
                    count_4 = (uint16_t)(count_4 - 1);
                    if (count_4 != 0) {
                        continue;
                    } else {
                        slot_hit_result = 0;
                        break;
                    }
                } else {
                    slot_scan_model = (uint16_t)(slot_scan_word >> 8);
                    model_record = slot_scan_model;
                    if (slot_scan_model == 0) {
                        slot_hit_result = 1;
                        break;
                    } else {
                        count_4 = (uint16_t)(count_4 - 1);
                        if (count_4 != 0) {
                            continue;
                        } else {
                            slot_hit_result = 0;
                            break;
                        }
                    }
                }
            }
            record_back_hit = fx_pull(stack);
            caller_scratch_hit = fx_pull(stack);
            fx_unspill(stack);
            return (FxCollideModelResult){slot_hit_result, delta_half_x, delta_y, model_record, caller_scratch_hit, record_back_hit, rom_at_7, 0};
        } else {
            record_back_none = fx_pull(stack);
            caller_scratch_none = fx_pull(stack);
            fx_unspill(stack);
            return (FxCollideModelResult){0, place_x, place_y, model_record, caller_scratch_none, record_back_none, rom_at, 0};
        }
    } else {
        record_back_none = fx_pull(stack);
        caller_scratch_none = fx_pull(stack);
        fx_unspill(stack);
        return (FxCollideModelResult){0, place_x, place_y, model_record, caller_scratch_none, record_back_none, rom_at, 0};
    }
}


/*
 * @routine 04:A053 FxCollideModelSkipFace
 * WHAT IT DOES: FxCollideModel's skip of a face whose slot word has its
 * high byte set: on to the next face of the list (04:A0F8 at its end).
 * The game never runs it: the same slot word was tested just before
 * with nothing written between, so the BEQ at 04:A050 always branches
 * past it. A unit start runs it so the C is judged against the
 * original.
 * HOW: IWT R15 to 04:9E92 (NOP in the delay slot); then as
 * FxCollideModel's face walk.
 * Entry: R14 = the face list (ROM, the bank set); R10 = stack with the
 * return address, R8 and R9 pushed, as FxCollideModel leaves it.
 * Exit: as FxCollideModel's.
 */
FxCollideModelSkipFaceResult FxCollideModelSkipFace(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t vertex_base, uint16_t *stack, uint16_t rom_at)
{
    int escape = 0;                    /* a way out of loops within loops */
    uint16_t k;
    uint16_t a2;
    uint16_t a4;
    uint16_t a2_2;
    uint16_t a2_3;
    uint16_t a2_4;
    uint16_t a2_5;
    uint16_t a4_2;
    uint16_t a4_3;
    uint16_t a4_4;
    uint16_t a4_5;
    uint16_t count;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t edge_a_x;
    uint16_t edge_a_y;
    uint16_t edge_a_z;
    uint16_t edge_b_x;
    uint16_t normal_x;
    uint16_t rom_at_2;
    uint16_t vector_x;
    uint16_t vector_y;
    uint16_t vector_z;
    uint16_t face_flags;
    uint16_t face_plane;
    uint16_t face_scale;
    uint16_t plane_side;
    uint16_t vector_x_2;
    uint16_t vertex_a_x;
    uint16_t vertex_a_y;
    uint16_t vertex_a_z;
    uint16_t vertex_b_x;
    uint16_t vertex_c_x;
    uint16_t vertex_c_y;
    uint16_t vertex_c_z;
    uint16_t face_normal;
    uint16_t normal_addr;
    uint16_t face_flags_2;
    uint16_t face_flags_3;
    uint16_t face_flags_4;
    uint16_t normal_index;
    uint16_t skip_rom_ptr;
    uint16_t cross_x_low_1;
    uint16_t cross_x_low_2;
    uint16_t cross_y_low_1;
    uint16_t cross_y_low_2;
    uint16_t cross_z_low_1;
    uint16_t cross_z_low_2;
    uint16_t edge_a_x_term;
    uint16_t edge_a_y_term;
    uint16_t edge_a_z_term;
    uint16_t high_words_hi;
    uint16_t normal_x_comp;
    uint16_t normal_y_addr;
    uint16_t normal_y_comp;
    uint16_t normal_z_comp;
    uint16_t rom_ptr_moved;
    uint16_t vertex_a_addr;
    uint16_t vertex_b_addr;
    uint16_t vertex_c_addr;
    uint32_t cross_x_prod_1;
    uint32_t cross_x_prod_2;
    uint32_t cross_y_prod_1;
    uint32_t cross_y_prod_2;
    uint32_t cross_z_prod_1;
    uint32_t cross_z_prod_2;
    uint16_t face_slot_addr;
    uint16_t face_slot_word;
    uint16_t scan_slot_addr;
    uint16_t scan_slot_word;
    uint16_t vertex_index_a;
    uint16_t vertex_index_b;
    uint16_t vertex_index_c;
    uint16_t edge_a_x_term_z;
    uint16_t edge_a_y_term_z;
    uint16_t edge_a_z_term_y;
    uint16_t edge_b_x_reload;
    uint16_t extra_skip_byte;
    uint16_t face_slot_index;
    uint16_t normal_y_loaded;
    uint16_t normal_z_loaded;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_y;
    uint16_t scaled_vector_z;
    uint16_t vertex_a_y_addr;
    uint16_t vertex_b_y_addr;
    uint16_t vertex_c_y_addr;
    uint16_t cross_x_low_diff;
    uint16_t cross_y_low_diff;
    uint16_t cross_z_low_diff;
    uint16_t face_header_byte;
    uint16_t face_skip_length;
    uint16_t vertex_index_ptr;
    uint16_t scaled_vector_x_2;
    uint16_t scaled_vector_y_2;
    uint16_t scaled_vector_z_2;
    uint16_t slot_word_recheck;
    uint16_t rom_ptr_after_slot;
    uint16_t rom_ptr_after_flags;
    uint16_t stored_normal_x_raw;
    uint16_t stored_normal_y_raw;
    uint16_t stored_normal_z_raw;
    uint16_t rom_ptr_after_header;
    uint16_t rom_ptr_after_flags_2;
    uint16_t rom_ptr_after_index_a;
    uint16_t rom_ptr_after_index_b;
    uint16_t slot_word_back_facing;
    uint16_t rom_ptr_after_normal_x;
    uint16_t rom_ptr_after_normal_y;
    uint16_t slot_word_front_facing;

    for (;;) {
        for (;;) {
            face_header_byte = (uint16_t)(int16_t)(int8_t)fx_getb();
            face_skip_length = face_header_byte;
            rom_ptr_after_header = (uint16_t)(rom_at + 1);
            fx_r14(rom_ptr_after_header);
            if (face_header_byte & 0x8000) {
                extra_skip_byte = (uint16_t)(int16_t)(int8_t)fx_getb();
                rom_at = (uint16_t)(rom_ptr_after_header + 1);
                fx_r14(rom_at);
                if (extra_skip_byte != 0) {
                    continue;
                } else {
                    escape = 1;          /* break lA053 */
                    break;
                }
            } else {
                face_slot_index = (uint16_t)(int16_t)(int8_t)fx_getb();
                rom_ptr_after_slot = (uint16_t)(rom_ptr_after_header + 1);
                fx_r14(rom_ptr_after_slot);
                if (face_slot_index != 0) {
                    fx_stw(FX_FACE_FLAGS, face_slot_index);
                    face_slot_addr = (uint16_t)((uint16_t)((int8_t)0x000A * (int8_t)(uint16_t)(face_slot_index & ~0x00E0)) + 0x23A4);
                    face_slot_word = fx_ldw(face_slot_addr);
                    if ((uint16_t)(face_slot_word >> 8) == 0) {
                        rom_ptr_after_flags = (uint16_t)(rom_ptr_after_slot + 1);
                        fx_r14(rom_ptr_after_flags);
                        rom_ptr_after_flags_2 = (uint16_t)(rom_ptr_after_flags + 1);
                        fx_r14(rom_ptr_after_flags_2);
                        rom_ptr_moved = fx_ldw(0x0044);
                        if (rom_ptr_moved != 0) {
                            face_flags = fx_ldw(FX_FACE_FLAGS);
                            if ((0x0040 & face_flags) != 0) {
                                vertex_index_ptr = (uint16_t)(rom_ptr_after_flags_2 + 3);
                                fx_r14(vertex_index_ptr);
                                fx_push(stack, vertex_index_ptr);
                                count = vertex_base;
                                vertex_index_a = fx_getb();
                                rom_ptr_after_index_a = (uint16_t)(vertex_index_ptr + 1);
                                fx_r14(rom_ptr_after_index_a);
                                vertex_a_addr = (uint16_t)((uint16_t)((uint8_t)vertex_index_a * (uint8_t)6) + count);
                                vertex_a_x = fx_ldw(vertex_a_addr);
                                vertex_a_y_addr = (uint16_t)(vertex_a_addr + 2);
                                fx_stw(0x0068, vertex_a_x);
                                vertex_a_y = fx_ldw(vertex_a_y_addr);
                                fx_stw(0x002C, vertex_a_y);
                                vertex_a_z = fx_ldw((uint16_t)(vertex_a_y_addr + 2));
                                fx_stw(0x002E, vertex_a_z);
                                vertex_index_b = fx_getb();
                                rom_ptr_after_index_b = (uint16_t)(rom_ptr_after_index_a + 1);
                                fx_r14(rom_ptr_after_index_b);
                                vertex_b_addr = (uint16_t)((uint16_t)((uint8_t)vertex_index_b * (uint8_t)6) + count);
                                vertex_b_x = fx_ldw(vertex_b_addr);
                                vertex_b_y_addr = (uint16_t)(vertex_b_addr + 2);
                                fx_stw(0x006A, vertex_b_x);
                                a2 = fx_ldw(vertex_b_y_addr);
                                fx_stw(GSU_IN_A2, a2);
                                a4 = fx_ldw((uint16_t)(vertex_b_y_addr + 2));
                                fx_stw(GSU_IN_A4, a4);
                                vertex_index_c = fx_getb();
                                fx_r14((uint16_t)(rom_ptr_after_index_b + 1));
                                vertex_c_addr = (uint16_t)((uint16_t)((uint8_t)vertex_index_c * (uint8_t)6) + count);
                                vertex_c_x = fx_ldw(vertex_c_addr);
                                vertex_c_y_addr = (uint16_t)(vertex_c_addr + 2);
                                vertex_c_y = fx_ldw(vertex_c_y_addr);
                                vertex_c_z = fx_ldw((uint16_t)(vertex_c_y_addr + 2));
                                edge_a_x = fx_ldw(0x0068);
                                edge_b_x = fx_ldw(0x006A);
                                fx_stw(0x0068, (uint16_t)(edge_a_x - edge_b_x));
                                edge_a_y = fx_ldw(0x002C);
                                a2_2 = fx_ldw(GSU_IN_A2);
                                fx_stw(0x002C, (uint16_t)(edge_a_y - a2_2));
                                edge_a_z = fx_ldw(0x002E);
                                a4_2 = fx_ldw(GSU_IN_A4);
                                fx_stw(0x002E, (uint16_t)(edge_a_z - a4_2));
                                edge_b_x_reload = fx_ldw(0x006A);
                                fx_stw(0x006A, (uint16_t)(edge_b_x_reload - vertex_c_x));
                                a2_3 = fx_ldw(GSU_IN_A2);
                                fx_stw(GSU_IN_A2, (uint16_t)(a2_3 - vertex_c_y));
                                a4_3 = fx_ldw(GSU_IN_A4);
                                fx_stw(GSU_IN_A4, (uint16_t)(a4_3 - vertex_c_z));
                                fx_push(stack, point_x);
                                fx_push(stack, point_y);
                                fx_push(stack, point_z);
                                fx_push(stack, count);
                                fx_push(stack, face_skip_length);
                                fx_push(stack, face_slot_addr);
                                vector_x = fx_ldw(0x0068);
                                vector_y = fx_ldw(0x002C);
                                vector_z = fx_ldw(0x002E);
                                { FxNormalized n_; if (FxNormalize((FxVec3){(int16_t)vector_x, (int16_t)vector_y, (int16_t)vector_z}, &n_)) { scaled_vector_x = (uint16_t)n_.unit.x; scaled_vector_y = (uint16_t)n_.unit.y; scaled_vector_z = (uint16_t)n_.unit.z; high_words_hi = (uint16_t)n_.x_high; k = (uint16_t)n_.shift; } else { scaled_vector_x = 0; scaled_vector_y = 0; scaled_vector_z = 0; high_words_hi = face_flags; k = a4_2; } };
                                fx_stw(0x0068, scaled_vector_x);
                                fx_stw(0x002C, scaled_vector_y);
                                fx_stw(0x002E, scaled_vector_z);
                                vector_x_2 = fx_ldw(0x006A);
                                a2_4 = fx_ldw(GSU_IN_A2);
                                a4_4 = fx_ldw(GSU_IN_A4);
                                { FxNormalized n_; if (FxNormalize((FxVec3){(int16_t)vector_x_2, (int16_t)a2_4, (int16_t)a4_4}, &n_)) { scaled_vector_x_2 = (uint16_t)n_.unit.x; scaled_vector_y_2 = (uint16_t)n_.unit.y; scaled_vector_z_2 = (uint16_t)n_.unit.z; } else { scaled_vector_x_2 = 0; scaled_vector_y_2 = 0; scaled_vector_z_2 = 0; } };
                                edge_a_y_term = fx_ldw(0x002C);
                                cross_x_prod_1 = (uint32_t)((int32_t)(int16_t)scaled_vector_z_2 * (int16_t)edge_a_y_term);
                                cross_x_low_1 = (uint16_t)cross_x_prod_1;
                                edge_a_z_term = fx_ldw(0x002E);
                                cross_x_prod_2 = (uint32_t)((int32_t)(int16_t)scaled_vector_y_2 * (int16_t)edge_a_z_term);
                                cross_x_low_2 = (uint16_t)cross_x_prod_2;
                                cross_x_low_diff = (uint16_t)(cross_x_low_1 - cross_x_low_2);
                                fx_stw(0x006A, (uint16_t)((uint16_t)((uint16_t)(cross_x_prod_1 >> 16) - (uint16_t)(cross_x_prod_2 >> 16) - (cross_x_low_1 < cross_x_low_2)) << 1 | ((uint32_t)cross_x_low_diff + cross_x_low_diff > 0xFFFF)));
                                edge_a_z_term_y = fx_ldw(0x002E);
                                cross_y_prod_1 = (uint32_t)((int32_t)(int16_t)scaled_vector_x_2 * (int16_t)edge_a_z_term_y);
                                cross_y_low_1 = (uint16_t)cross_y_prod_1;
                                edge_a_x_term = fx_ldw(0x0068);
                                cross_y_prod_2 = (uint32_t)((int32_t)(int16_t)scaled_vector_z_2 * (int16_t)edge_a_x_term);
                                cross_y_low_2 = (uint16_t)cross_y_prod_2;
                                cross_y_low_diff = (uint16_t)(cross_y_low_1 - cross_y_low_2);
                                fx_stw(GSU_IN_A2, (uint16_t)((uint16_t)((uint16_t)(cross_y_prod_1 >> 16) - (uint16_t)(cross_y_prod_2 >> 16) - (cross_y_low_1 < cross_y_low_2)) << 1 | ((uint32_t)cross_y_low_diff + cross_y_low_diff > 0xFFFF)));
                                edge_a_x_term_z = fx_ldw(0x0068);
                                cross_z_prod_1 = (uint32_t)((int32_t)(int16_t)scaled_vector_y_2 * (int16_t)edge_a_x_term_z);
                                cross_z_low_1 = (uint16_t)cross_z_prod_1;
                                edge_a_y_term_z = fx_ldw(0x002C);
                                cross_z_prod_2 = (uint32_t)((int32_t)(int16_t)scaled_vector_x_2 * (int16_t)edge_a_y_term_z);
                                cross_z_low_2 = (uint16_t)cross_z_prod_2;
                                cross_z_low_diff = (uint16_t)(cross_z_low_1 - cross_z_low_2);
                                fx_stw(GSU_IN_A4, (uint16_t)((uint16_t)((uint16_t)(cross_z_prod_1 >> 16) - (uint16_t)(cross_z_prod_2 >> 16) - (cross_z_low_1 < cross_z_low_2)) << 1 | ((uint32_t)cross_z_low_diff + cross_z_low_diff > 0xFFFF)));
                                normal_x = fx_ldw(0x006A);
                                a2_5 = fx_ldw(GSU_IN_A2);
                                a4_5 = fx_ldw(GSU_IN_A4);
                                face_slot_addr = fx_pull(stack);
                                face_skip_length = fx_pull(stack);
                                count_2 = fx_pull(stack);
                                point_z = fx_pull(stack);
                                point_y = fx_pull(stack);
                                point_x = fx_pull(stack);
                                vertex_base = count_2;
                                rom_at_2 = fx_pull(stack);
                                fx_r14(rom_at_2);
                            } else {
                                stored_normal_x_raw = fx_getb();
                                rom_ptr_after_normal_x = (uint16_t)(rom_ptr_after_flags_2 + 1);
                                fx_r14(rom_ptr_after_normal_x);
                                normal_x = (uint16_t)((uint16_t)~(uint16_t)(stored_normal_x_raw << 8 | stored_normal_x_raw >> 8) + 1);
                                stored_normal_y_raw = fx_getb();
                                rom_ptr_after_normal_y = (uint16_t)(rom_ptr_after_normal_x + 1);
                                fx_r14(rom_ptr_after_normal_y);
                                a2_5 = (uint16_t)((uint16_t)~(uint16_t)(stored_normal_y_raw << 8 | stored_normal_y_raw >> 8) + 1);
                                stored_normal_z_raw = fx_getb();
                                rom_at_2 = (uint16_t)(rom_ptr_after_normal_y + 1);
                                fx_r14(rom_at_2);
                                a4_5 = (uint16_t)((uint16_t)~(uint16_t)(stored_normal_z_raw << 8 | stored_normal_z_raw >> 8) + 1);
                            }
                        } else {
                            stored_normal_x_raw = fx_getb();
                            rom_ptr_after_normal_x = (uint16_t)(rom_ptr_after_flags_2 + 1);
                            fx_r14(rom_ptr_after_normal_x);
                            normal_x = (uint16_t)((uint16_t)~(uint16_t)(stored_normal_x_raw << 8 | stored_normal_x_raw >> 8) + 1);
                            stored_normal_y_raw = fx_getb();
                            rom_ptr_after_normal_y = (uint16_t)(rom_ptr_after_normal_x + 1);
                            fx_r14(rom_ptr_after_normal_y);
                            a2_5 = (uint16_t)((uint16_t)~(uint16_t)(stored_normal_y_raw << 8 | stored_normal_y_raw >> 8) + 1);
                            stored_normal_z_raw = fx_getb();
                            rom_at_2 = (uint16_t)(rom_ptr_after_normal_y + 1);
                            fx_r14(rom_at_2);
                            a4_5 = (uint16_t)((uint16_t)~(uint16_t)(stored_normal_z_raw << 8 | stored_normal_z_raw >> 8) + 1);
                        }
                        face_scale = fx_ldw(FX_FACE_SCALE);
                        normal_index = (uint16_t)(int16_t)(int8_t)fx_getb();
                        rom_at = (uint16_t)(rom_at_2 + face_skip_length);
                        fx_r14(rom_at);
                        normal_addr = (uint16_t)(vertex_base + (uint16_t)((uint8_t)normal_index * (uint8_t)6));
                        normal_x_comp = fx_ldw(normal_addr);
                        normal_y_addr = (uint16_t)(normal_addr + 2);
                        fx_stw(FX_FACE_NORMAL, (uint16_t)((int8_t)normal_x_comp * (int8_t)face_scale));
                        normal_y_comp = fx_ldw(normal_y_addr);
                        fx_stw(0x2374, (uint16_t)((int8_t)normal_y_comp * (int8_t)face_scale));
                        normal_z_comp = fx_ldw((uint16_t)(normal_y_addr + 2));
                        fx_stw(0x2376, (uint16_t)((int8_t)normal_z_comp * (int8_t)face_scale));
                        slot_word_recheck = fx_ldw(face_slot_addr);
                        if ((uint16_t)(slot_word_recheck >> 8) == 0) {
                            fx_stw(FX_FACE_PLANE, (uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)a4_5 * (int16_t)point_z) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_x * (int16_t)point_x) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)a2_5 * (int16_t)point_y) >> 16)));
                            face_normal = fx_ldw(FX_FACE_NORMAL);
                            normal_y_loaded = fx_ldw(0x2374);
                            normal_z_loaded = fx_ldw(0x2376);
                            face_plane = fx_ldw(FX_FACE_PLANE);
                            plane_side = (uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)a4_5 * (int16_t)normal_z_loaded) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_x * (int16_t)face_normal) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)a2_5 * (int16_t)normal_y_loaded) >> 16)) - face_plane);
                            if (!(plane_side & 0x8000)) {
                                face_flags_2 = fx_ldw(FX_FACE_FLAGS);
                                if ((0x0080 & face_flags_2) == 0) {
                                    { FxCollideModelDepthCompareResult tr_ = FxCollideModelDepthCompare(plane_side, normal_x, a2_5, a4_5, point_x, point_y, point_z, vertex_base, stack, face_slot_addr, rom_at); return (FxCollideModelSkipFaceResult){tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
                                }
                                face_flags_3 = fx_ldw(FX_FACE_FLAGS);
                                slot_word_front_facing = fx_ldw(face_slot_addr);
                                fx_sbk((slot_word_front_facing | (face_flags_3 & 0x00FF)));
                                continue;
                            } else {
                                face_flags_4 = fx_ldw(FX_FACE_FLAGS);
                                slot_word_back_facing = fx_ldw(face_slot_addr);
                                fx_sbk((((uint16_t)(face_flags_4 << 8 | face_flags_4 >> 8) | slot_word_back_facing) & 0xFF00));
                                continue;
                            }
                        } else {
                            break;
                        }
                    } else {
                        skip_rom_ptr = (uint16_t)(rom_ptr_after_slot + face_skip_length);
                        fx_r14(skip_rom_ptr);
                        rom_at = (uint16_t)(skip_rom_ptr + 5);
                        fx_r14(rom_at);
                        continue;
                    }
                } else {
                    skip_rom_ptr = (uint16_t)(rom_ptr_after_slot + face_skip_length);
                    fx_r14(skip_rom_ptr);
                    rom_at = (uint16_t)(skip_rom_ptr + 5);
                    fx_r14(rom_at);
                    continue;
                }
            }
        }
        if (escape == 1) {
            escape = 0;
            break;
        }
        continue;
    }
    scan_slot_addr = 0x23A4;
    count_3 = 0x0013;
    for (;;) {
        scan_slot_addr = (uint16_t)(scan_slot_addr + 0x000A);
        scan_slot_word = fx_ldw(scan_slot_addr);
        if (scan_slot_word == 0) {
            count_3 = (uint16_t)(count_3 - 1);
            if (count_3 != 0) {
                continue;
            } else {
                break;
            }
        } else {
            if ((uint16_t)(scan_slot_word >> 8) == 0) {
                break;
            } else {
                count_3 = (uint16_t)(count_3 - 1);
                if (count_3 != 0) {
                    continue;
                } else {
                    break;
                }
            }
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxCollideModelSkipFaceResult){3};
}


/*
 * @routine 04:A0B8 FxCollideModelDepthCompare
 * WHAT IT DOES: FxCollideModel's keep test for a face the place lies
 * behind: the absolute depth kept in the slot (R11 + 2) against the
 * absolute depth R0; a deeper one replaces the slot's flags, depth and
 * normal (R1-R3); then the next face. The game reaches it only with
 * both depths 0 or more (a slot's depth starts at $7D00 and only gets R0,
 * which the BPL before it keeps positive), so their negations at
 * 04:A0C1 and 04:A0C9 never run there; a unit start with both negative
 * runs them so the C is judged against the original.
 * HOW: LDW, MOVES and BPL with NOT, INC for each absolute value; CMP;
 * STW of R0, the flags at $237C and R1-R3 into the slot; IWT R15 to
 * 04:9E92.
 * Entry: R0 = the depth; R1-R3 = the normal; R11 = the slot; R14 = the
 * face list; R10 = stack with the return address, R8 and R9 pushed, as
 * FxCollideModel leaves it.
 * Exit: as FxCollideModel's.
 */
FxCollideModelDepthCompareResult FxCollideModelDepthCompare(uint16_t depth, uint16_t normal_x, uint16_t normal_y, uint16_t normal_z, uint16_t place_x, uint16_t place_y, uint16_t place_z, uint16_t point_table, uint16_t *stack, uint16_t entry_slot_ptr, uint16_t rom_at)
{
    int escape = 0;                    /* a way out of loops within loops */
    uint16_t k;
    uint16_t a2;
    uint16_t a4;
    uint16_t a2_2;
    uint16_t a2_3;
    uint16_t a2_4;
    uint16_t a4_2;
    uint16_t a4_3;
    uint16_t a4_4;
    uint16_t count;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t count_4;
    uint16_t count_5;
    uint16_t edge_x_1;
    uint16_t edge_x_2;
    uint16_t edge_y_1;
    uint16_t edge_y_2;
    uint16_t edge_z_1;
    uint16_t edge_z_2;
    uint16_t rom_at_2;
    uint16_t vector_x;
    uint16_t vector_y;
    uint16_t vector_z;
    uint16_t corner_a_x;
    uint16_t corner_a_y;
    uint16_t corner_a_z;
    uint16_t corner_b_x;
    uint16_t corner_c_x;
    uint16_t corner_c_y;
    uint16_t corner_c_z;
    uint16_t depth_copy;
    uint16_t end_marker;
    uint16_t face_flags;
    uint16_t face_plane;
    uint16_t face_scale;
    uint16_t list_ptr_1;
    uint16_t list_ptr_2;
    uint16_t list_ptr_3;
    uint16_t list_ptr_4;
    uint16_t list_ptr_5;
    uint16_t list_ptr_6;
    uint16_t list_ptr_7;
    uint16_t list_ptr_8;
    uint16_t list_ptr_9;
    uint32_t prod_x2_y1;
    uint32_t prod_x2_z1;
    uint32_t prod_y2_x1;
    uint32_t prod_y2_z1;
    uint32_t prod_z2_x1;
    uint32_t prod_z2_y1;
    uint16_t vector_x_2;
    uint16_t face_normal;
    uint16_t slot_word_1;
    uint16_t slot_word_2;
    uint16_t slot_word_3;
    uint16_t slot_word_4;
    uint16_t face_flags_2;
    uint16_t face_flags_3;
    uint16_t face_flags_4;
    uint16_t face_flags_5;
    uint16_t raw_normal_x;
    uint16_t raw_normal_y;
    uint16_t raw_normal_z;
    uint16_t corner_a_addr;
    uint16_t corner_b_addr;
    uint16_t corner_c_addr;
    uint16_t face_normal_y;
    uint16_t face_normal_z;
    uint16_t face_skip_len;
    uint16_t high_words_hi;
    uint16_t plane_point_x;
    uint16_t plane_point_y;
    uint16_t plane_point_z;
    uint16_t prod_x2_y1_lo;
    uint16_t prod_x2_z1_lo;
    uint16_t prod_y2_x1_lo;
    uint16_t prod_y2_z1_lo;
    uint16_t prod_z2_x1_lo;
    uint16_t prod_z2_y1_lo;
    uint16_t pulled_record;
    uint16_t slot_scan_ptr;
    uint16_t corner_a_index;
    uint16_t corner_b_index;
    uint16_t corner_c_index;
    uint16_t depth_slot_ptr;
    uint16_t flags_slot_ptr;
    uint16_t corner_a_y_addr;
    uint16_t corner_b_y_addr;
    uint16_t corner_c_y_addr;
    uint16_t cross_x_lo_diff;
    uint16_t cross_y_lo_diff;
    uint16_t cross_z_lo_diff;
    uint16_t face_flags_byte;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_y;
    uint16_t scaled_vector_z;
    uint16_t slot_hit_result;
    uint16_t slot_scale_byte;
    uint16_t plane_point_addr;
    uint16_t plane_side_depth;
    uint16_t skipped_face_ptr;
    uint16_t corner_a_x_reload;
    uint16_t corner_a_y_reload;
    uint16_t corner_a_z_reload;
    uint16_t corner_b_x_reload;
    uint16_t normal_x_slot_ptr;
    uint16_t normal_y_slot_ptr;
    uint16_t plane_point_index;
    uint16_t pulled_aux_record;
    uint16_t scaled_vector_x_2;
    uint16_t scaled_vector_y_2;
    uint16_t scaled_vector_z_2;
    uint16_t scanned_slot_word;
    uint16_t turned_model_flag;
    uint16_t face_skip_len_copy;
    uint16_t plane_point_y_addr;
    uint16_t corner_b_x_reload_2;

    for (;;) {
        depth_slot_ptr = (uint16_t)(entry_slot_ptr + 2);
        count = fx_ldw(depth_slot_ptr);
        face_scale = count;
        if (!(count & 0x8000)) {
        } else {
            face_scale = (uint16_t)((uint16_t)~face_scale + 1);
        }
        depth_copy = depth;
        count_2 = depth_copy;
        if (!(depth_copy & 0x8000)) {
        } else {
            count_2 = (uint16_t)((uint16_t)~count_2 + 1);
        }
        if (!((uint16_t)(face_scale - count_2) & 0x8000)) {
            fx_stw(depth_slot_ptr, depth);
            flags_slot_ptr = (uint16_t)(depth_slot_ptr - 2);
            face_flags = fx_ldw(FX_FACE_FLAGS);
            fx_stw(flags_slot_ptr, face_flags);
            normal_x_slot_ptr = (uint16_t)(flags_slot_ptr + 4);
            fx_stw(normal_x_slot_ptr, normal_x);
            normal_y_slot_ptr = (uint16_t)(normal_x_slot_ptr + 2);
            fx_stw(normal_y_slot_ptr, normal_y);
            fx_stw((uint16_t)(normal_y_slot_ptr + 2), normal_z);
        } else {
        }
        for (;;) {
            face_skip_len = (uint16_t)(int16_t)(int8_t)fx_getb();
            face_skip_len_copy = face_skip_len;
            list_ptr_1 = (uint16_t)(rom_at + 1);
            fx_r14(list_ptr_1);
            if (face_skip_len & 0x8000) {
                end_marker = (uint16_t)(int16_t)(int8_t)fx_getb();
                rom_at = (uint16_t)(list_ptr_1 + 1);
                fx_r14(rom_at);
                if (end_marker != 0) {
                    continue;
                } else {
                    escape = 1;          /* break lA0B8 */
                    break;
                }
            } else {
                face_flags_byte = (uint16_t)(int16_t)(int8_t)fx_getb();
                list_ptr_2 = (uint16_t)(list_ptr_1 + 1);
                fx_r14(list_ptr_2);
                if (face_flags_byte != 0) {
                    fx_stw(FX_FACE_FLAGS, face_flags_byte);
                    entry_slot_ptr = (uint16_t)((uint16_t)((int8_t)0x000A * (int8_t)(uint16_t)(face_flags_byte & ~0x00E0)) + 0x23A4);
                    slot_word_1 = fx_ldw(entry_slot_ptr);
                    if ((uint16_t)(slot_word_1 >> 8) == 0) {
                        list_ptr_3 = (uint16_t)(list_ptr_2 + 1);
                        fx_r14(list_ptr_3);
                        list_ptr_4 = (uint16_t)(list_ptr_3 + 1);
                        fx_r14(list_ptr_4);
                        turned_model_flag = fx_ldw(0x0044);
                        if (turned_model_flag != 0) {
                            face_flags_2 = fx_ldw(FX_FACE_FLAGS);
                            if ((0x0040 & face_flags_2) != 0) {
                                list_ptr_5 = (uint16_t)(list_ptr_4 + 3);
                                fx_r14(list_ptr_5);
                                fx_push(stack, list_ptr_5);
                                count_3 = point_table;
                                corner_a_index = fx_getb();
                                list_ptr_6 = (uint16_t)(list_ptr_5 + 1);
                                fx_r14(list_ptr_6);
                                corner_a_addr = (uint16_t)((uint16_t)((uint8_t)corner_a_index * (uint8_t)6) + count_3);
                                corner_a_x = fx_ldw(corner_a_addr);
                                corner_a_y_addr = (uint16_t)(corner_a_addr + 2);
                                fx_stw(0x0068, corner_a_x);
                                corner_a_y = fx_ldw(corner_a_y_addr);
                                fx_stw(0x002C, corner_a_y);
                                corner_a_z = fx_ldw((uint16_t)(corner_a_y_addr + 2));
                                fx_stw(0x002E, corner_a_z);
                                corner_b_index = fx_getb();
                                list_ptr_7 = (uint16_t)(list_ptr_6 + 1);
                                fx_r14(list_ptr_7);
                                corner_b_addr = (uint16_t)((uint16_t)((uint8_t)corner_b_index * (uint8_t)6) + count_3);
                                corner_b_x = fx_ldw(corner_b_addr);
                                corner_b_y_addr = (uint16_t)(corner_b_addr + 2);
                                fx_stw(0x006A, corner_b_x);
                                a2 = fx_ldw(corner_b_y_addr);
                                fx_stw(GSU_IN_A2, a2);
                                a4 = fx_ldw((uint16_t)(corner_b_y_addr + 2));
                                fx_stw(GSU_IN_A4, a4);
                                corner_c_index = fx_getb();
                                fx_r14((uint16_t)(list_ptr_7 + 1));
                                corner_c_addr = (uint16_t)((uint16_t)((uint8_t)corner_c_index * (uint8_t)6) + count_3);
                                corner_c_x = fx_ldw(corner_c_addr);
                                corner_c_y_addr = (uint16_t)(corner_c_addr + 2);
                                corner_c_y = fx_ldw(corner_c_y_addr);
                                corner_c_z = fx_ldw((uint16_t)(corner_c_y_addr + 2));
                                corner_a_x_reload = fx_ldw(0x0068);
                                corner_b_x_reload = fx_ldw(0x006A);
                                fx_stw(0x0068, (uint16_t)(corner_a_x_reload - corner_b_x_reload));
                                corner_a_y_reload = fx_ldw(0x002C);
                                a2_2 = fx_ldw(GSU_IN_A2);
                                fx_stw(0x002C, (uint16_t)(corner_a_y_reload - a2_2));
                                corner_a_z_reload = fx_ldw(0x002E);
                                a4_2 = fx_ldw(GSU_IN_A4);
                                fx_stw(0x002E, (uint16_t)(corner_a_z_reload - a4_2));
                                corner_b_x_reload_2 = fx_ldw(0x006A);
                                fx_stw(0x006A, (uint16_t)(corner_b_x_reload_2 - corner_c_x));
                                a2_3 = fx_ldw(GSU_IN_A2);
                                fx_stw(GSU_IN_A2, (uint16_t)(a2_3 - corner_c_y));
                                a4_3 = fx_ldw(GSU_IN_A4);
                                fx_stw(GSU_IN_A4, (uint16_t)(a4_3 - corner_c_z));
                                fx_push(stack, place_x);
                                fx_push(stack, place_y);
                                fx_push(stack, place_z);
                                fx_push(stack, count_3);
                                fx_push(stack, face_skip_len_copy);
                                fx_push(stack, entry_slot_ptr);
                                vector_x = fx_ldw(0x0068);
                                vector_y = fx_ldw(0x002C);
                                vector_z = fx_ldw(0x002E);
                                { FxNormalized n_; if (FxNormalize((FxVec3){(int16_t)vector_x, (int16_t)vector_y, (int16_t)vector_z}, &n_)) { scaled_vector_x = (uint16_t)n_.unit.x; scaled_vector_y = (uint16_t)n_.unit.y; scaled_vector_z = (uint16_t)n_.unit.z; high_words_hi = (uint16_t)n_.x_high; k = (uint16_t)n_.shift; } else { scaled_vector_x = 0; scaled_vector_y = 0; scaled_vector_z = 0; high_words_hi = face_flags_2; k = a4_2; } };
                                fx_stw(0x0068, scaled_vector_x);
                                fx_stw(0x002C, scaled_vector_y);
                                fx_stw(0x002E, scaled_vector_z);
                                vector_x_2 = fx_ldw(0x006A);
                                a2_4 = fx_ldw(GSU_IN_A2);
                                a4_4 = fx_ldw(GSU_IN_A4);
                                { FxNormalized n_; if (FxNormalize((FxVec3){(int16_t)vector_x_2, (int16_t)a2_4, (int16_t)a4_4}, &n_)) { scaled_vector_x_2 = (uint16_t)n_.unit.x; scaled_vector_y_2 = (uint16_t)n_.unit.y; scaled_vector_z_2 = (uint16_t)n_.unit.z; } else { scaled_vector_x_2 = 0; scaled_vector_y_2 = 0; scaled_vector_z_2 = 0; } };
                                edge_y_1 = fx_ldw(0x002C);
                                prod_z2_y1 = (uint32_t)((int32_t)(int16_t)scaled_vector_z_2 * (int16_t)edge_y_1);
                                prod_z2_y1_lo = (uint16_t)prod_z2_y1;
                                edge_z_1 = fx_ldw(0x002E);
                                prod_y2_z1 = (uint32_t)((int32_t)(int16_t)scaled_vector_y_2 * (int16_t)edge_z_1);
                                prod_y2_z1_lo = (uint16_t)prod_y2_z1;
                                cross_x_lo_diff = (uint16_t)(prod_z2_y1_lo - prod_y2_z1_lo);
                                fx_stw(0x006A, (uint16_t)((uint16_t)((uint16_t)(prod_z2_y1 >> 16) - (uint16_t)(prod_y2_z1 >> 16) - (prod_z2_y1_lo < prod_y2_z1_lo)) << 1 | ((uint32_t)cross_x_lo_diff + cross_x_lo_diff > 0xFFFF)));
                                edge_z_2 = fx_ldw(0x002E);
                                prod_x2_z1 = (uint32_t)((int32_t)(int16_t)scaled_vector_x_2 * (int16_t)edge_z_2);
                                prod_x2_z1_lo = (uint16_t)prod_x2_z1;
                                edge_x_1 = fx_ldw(0x0068);
                                prod_z2_x1 = (uint32_t)((int32_t)(int16_t)scaled_vector_z_2 * (int16_t)edge_x_1);
                                prod_z2_x1_lo = (uint16_t)prod_z2_x1;
                                cross_y_lo_diff = (uint16_t)(prod_x2_z1_lo - prod_z2_x1_lo);
                                fx_stw(GSU_IN_A2, (uint16_t)((uint16_t)((uint16_t)(prod_x2_z1 >> 16) - (uint16_t)(prod_z2_x1 >> 16) - (prod_x2_z1_lo < prod_z2_x1_lo)) << 1 | ((uint32_t)cross_y_lo_diff + cross_y_lo_diff > 0xFFFF)));
                                edge_x_2 = fx_ldw(0x0068);
                                prod_y2_x1 = (uint32_t)((int32_t)(int16_t)scaled_vector_y_2 * (int16_t)edge_x_2);
                                prod_y2_x1_lo = (uint16_t)prod_y2_x1;
                                edge_y_2 = fx_ldw(0x002C);
                                prod_x2_y1 = (uint32_t)((int32_t)(int16_t)scaled_vector_x_2 * (int16_t)edge_y_2);
                                prod_x2_y1_lo = (uint16_t)prod_x2_y1;
                                cross_z_lo_diff = (uint16_t)(prod_y2_x1_lo - prod_x2_y1_lo);
                                fx_stw(GSU_IN_A4, (uint16_t)((uint16_t)((uint16_t)(prod_y2_x1 >> 16) - (uint16_t)(prod_x2_y1 >> 16) - (prod_y2_x1_lo < prod_x2_y1_lo)) << 1 | ((uint32_t)cross_z_lo_diff + cross_z_lo_diff > 0xFFFF)));
                                normal_x = fx_ldw(0x006A);
                                normal_y = fx_ldw(GSU_IN_A2);
                                normal_z = fx_ldw(GSU_IN_A4);
                                entry_slot_ptr = fx_pull(stack);
                                face_skip_len_copy = fx_pull(stack);
                                count_4 = fx_pull(stack);
                                place_z = fx_pull(stack);
                                place_y = fx_pull(stack);
                                place_x = fx_pull(stack);
                                point_table = count_4;
                                rom_at_2 = fx_pull(stack);
                                fx_r14(rom_at_2);
                            } else {
                                raw_normal_x = fx_getb();
                                list_ptr_8 = (uint16_t)(list_ptr_4 + 1);
                                fx_r14(list_ptr_8);
                                normal_x = (uint16_t)((uint16_t)~(uint16_t)(raw_normal_x << 8 | raw_normal_x >> 8) + 1);
                                raw_normal_y = fx_getb();
                                list_ptr_9 = (uint16_t)(list_ptr_8 + 1);
                                fx_r14(list_ptr_9);
                                normal_y = (uint16_t)((uint16_t)~(uint16_t)(raw_normal_y << 8 | raw_normal_y >> 8) + 1);
                                raw_normal_z = fx_getb();
                                rom_at_2 = (uint16_t)(list_ptr_9 + 1);
                                fx_r14(rom_at_2);
                                normal_z = (uint16_t)((uint16_t)~(uint16_t)(raw_normal_z << 8 | raw_normal_z >> 8) + 1);
                            }
                        } else {
                            raw_normal_x = fx_getb();
                            list_ptr_8 = (uint16_t)(list_ptr_4 + 1);
                            fx_r14(list_ptr_8);
                            normal_x = (uint16_t)((uint16_t)~(uint16_t)(raw_normal_x << 8 | raw_normal_x >> 8) + 1);
                            raw_normal_y = fx_getb();
                            list_ptr_9 = (uint16_t)(list_ptr_8 + 1);
                            fx_r14(list_ptr_9);
                            normal_y = (uint16_t)((uint16_t)~(uint16_t)(raw_normal_y << 8 | raw_normal_y >> 8) + 1);
                            raw_normal_z = fx_getb();
                            rom_at_2 = (uint16_t)(list_ptr_9 + 1);
                            fx_r14(rom_at_2);
                            normal_z = (uint16_t)((uint16_t)~(uint16_t)(raw_normal_z << 8 | raw_normal_z >> 8) + 1);
                        }
                        face_scale = fx_ldw(FX_FACE_SCALE);
                        plane_point_index = (uint16_t)(int16_t)(int8_t)fx_getb();
                        rom_at = (uint16_t)(rom_at_2 + face_skip_len_copy);
                        fx_r14(rom_at);
                        plane_point_addr = (uint16_t)(point_table + (uint16_t)((uint8_t)plane_point_index * (uint8_t)6));
                        plane_point_x = fx_ldw(plane_point_addr);
                        plane_point_y_addr = (uint16_t)(plane_point_addr + 2);
                        fx_stw(FX_FACE_NORMAL, (uint16_t)((int8_t)plane_point_x * (int8_t)face_scale));
                        plane_point_y = fx_ldw(plane_point_y_addr);
                        fx_stw(0x2374, (uint16_t)((int8_t)plane_point_y * (int8_t)face_scale));
                        plane_point_z = fx_ldw((uint16_t)(plane_point_y_addr + 2));
                        fx_stw(0x2376, (uint16_t)((int8_t)plane_point_z * (int8_t)face_scale));
                        slot_word_2 = fx_ldw(entry_slot_ptr);
                        if ((uint16_t)(slot_word_2 >> 8) == 0) {
                            fx_stw(FX_FACE_PLANE, (uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)normal_z * (int16_t)place_z) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_x * (int16_t)place_x) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_y * (int16_t)place_y) >> 16)));
                            face_normal = fx_ldw(FX_FACE_NORMAL);
                            face_normal_y = fx_ldw(0x2374);
                            face_normal_z = fx_ldw(0x2376);
                            face_plane = fx_ldw(FX_FACE_PLANE);
                            plane_side_depth = (uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)normal_z * (int16_t)face_normal_z) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_x * (int16_t)face_normal) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)normal_y * (int16_t)face_normal_y) >> 16)) - face_plane);
                            depth = plane_side_depth;
                            if (!(plane_side_depth & 0x8000)) {
                                face_flags_3 = fx_ldw(FX_FACE_FLAGS);
                                if ((0x0080 & face_flags_3) == 0) {
                                    break;
                                } else {
                                    face_flags_4 = fx_ldw(FX_FACE_FLAGS);
                                    face_scale = (face_flags_4 & 0x00FF);
                                    slot_word_3 = fx_ldw(entry_slot_ptr);
                                    fx_sbk((slot_word_3 | face_scale));
                                    continue;
                                }
                            } else {
                                face_flags_5 = fx_ldw(FX_FACE_FLAGS);
                                slot_word_4 = fx_ldw(entry_slot_ptr);
                                face_scale = 0xFF00;
                                fx_sbk((((uint16_t)(face_flags_5 << 8 | face_flags_5 >> 8) | slot_word_4) & face_scale));
                                continue;
                            }
                        } else {
                            continue;
                        }
                    } else {
                        skipped_face_ptr = (uint16_t)(list_ptr_2 + face_skip_len_copy);
                        fx_r14(skipped_face_ptr);
                        rom_at = (uint16_t)(skipped_face_ptr + 5);
                        fx_r14(rom_at);
                        continue;
                    }
                } else {
                    skipped_face_ptr = (uint16_t)(list_ptr_2 + face_skip_len_copy);
                    fx_r14(skipped_face_ptr);
                    rom_at = (uint16_t)(skipped_face_ptr + 5);
                    fx_r14(rom_at);
                    continue;
                }
            }
        }
        if (escape == 1) {
            escape = 0;
            break;
        }
        continue;
    }
    slot_scan_ptr = 0x23A4;
    count_5 = 0x0013;
    for (;;) {
        slot_scan_ptr = (uint16_t)(slot_scan_ptr + 0x000A);
        scanned_slot_word = fx_ldw(slot_scan_ptr);
        if (scanned_slot_word == 0) {
            count_5 = (uint16_t)(count_5 - 1);
            if (count_5 != 0) {
                continue;
            } else {
                slot_hit_result = 0;
                break;
            }
        } else {
            slot_scale_byte = (uint16_t)(scanned_slot_word >> 8);
            face_scale = slot_scale_byte;
            if (slot_scale_byte == 0) {
                slot_hit_result = 1;
                break;
            } else {
                count_5 = (uint16_t)(count_5 - 1);
                if (count_5 != 0) {
                    continue;
                } else {
                    slot_hit_result = 0;
                    break;
                }
            }
        }
    }
    pulled_record = fx_pull(stack);
    pulled_aux_record = fx_pull(stack);
    fx_unspill(stack);
    return (FxCollideModelDepthCompareResult){slot_hit_result, place_x, place_y, face_scale, pulled_aux_record, pulled_record, rom_at, 3};
}


/*
 * @routine 04:A124 FxCollideModelUnused
 * WHAT IT DOES: Code no jump or branch reaches, after FxCollideModel's
 * return: a branch back to its slot scan (04:A0F8), which then ends as
 * FxCollideModel does (R0 = 1 when a slot holds a touch, else 0; R9, R8
 * and R15 pulled). The store after it is FxCollideModelUnusedStore
 * (04:A127). A unit start runs it so the C is judged against the
 * original.
 * HOW: BRA to 04:A0F8 (NOP in the delay slot).
 * Entry: R10 = stack with the return address, R8 and R9 pushed, as
 * FxCollideModel leaves it.
 * Exit: as FxCollideModel's.
 */
FxCollideModelUnusedResult FxCollideModelUnused(uint16_t *stack)
{
    uint16_t count;
    uint16_t scan_slot_addr;
    uint16_t scan_slot_word;

    scan_slot_addr = 0x23A4;
    count = 0x0013;
    for (;;) {
        scan_slot_addr = (uint16_t)(scan_slot_addr + 0x000A);
        scan_slot_word = fx_ldw(scan_slot_addr);
        if (scan_slot_word == 0) {
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            if ((uint16_t)(scan_slot_word >> 8) == 0) {
                break;
            } else {
                count = (uint16_t)(count - 1);
                if (count != 0) {
                    continue;
                } else {
                    break;
                }
            }
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxCollideModelUnusedResult){3};
}


/*
 * @routine 04:A127 FxCollideModelUnusedStore
 * WHAT IT DOES: Code no jump or branch reaches, after FxCollideModel's
 * return: stores record R9's y (+$20) minus the low seven bits of its x
 * plus z (+$1E + +$22) in the slot word at $23A4, then the slot scan
 * at 04:A0F8 as FxCollideModelUnused. A unit start runs it so the C is
 * judged against the original.
 * HOW: R11 = $23A4; ADD, AND #$7F, NOT, INC, ADD, STW (R11); BRA to
 * 04:A0F8.
 * Entry: R9 = the record; R10 = stack with the return address, R8 and
 * R9 pushed, as FxCollideModel leaves it.
 * Exit: as FxCollideModel's.
 */
FxCollideModelUnusedStoreResult FxCollideModelUnusedStore(uint16_t record, uint16_t *stack)
{
    uint16_t y;
    uint16_t count;
    uint16_t half_x;
    uint16_t slot_ptr;
    uint16_t slot_flags;
    uint16_t record_field_22;

    half_x = fx_ldw((uint16_t)(record + FXREC_HALF_X));
    record_field_22 = fx_ldw((uint16_t)(record + 0x0022));
    y = fx_ldw((uint16_t)(record + FXREC_Y));
    fx_stw(FX_PUSH, (uint16_t)((uint16_t)((uint16_t)~((uint16_t)(half_x + record_field_22) & 0x007F) + y) + 1));
    slot_ptr = 0x23A4;
    count = 0x0013;
    for (;;) {
        slot_ptr = (uint16_t)(slot_ptr + 0x000A);
        slot_flags = fx_ldw(slot_ptr);
        if (slot_flags == 0) {
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            if ((uint16_t)(slot_flags >> 8) == 0) {
                break;
            } else {
                count = (uint16_t)(count - 1);
                if (count != 0) {
                    continue;
                } else {
                    break;
                }
            }
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxCollideModelUnusedStoreResult){3};
}


/*
 * @routine 04:A607 FxPartsUpdate
 * WHAT IT DOES: Updates each part of car R8 (its list at +$3E, linked
 * by +$34): a part with bit 11 of its flags (+$46) is pulled toward
 * where it belongs (FxPartPull), any other is a wheel (FxWheelUpdate);
 * each part's contact flags (+$2C) are gathered into the car's +$92.
 * Clears the car's +$44, +$46 and +$56 first.
 * HOW: Pushes R11; R9 = the part; returns by pulling R15.
 * Entry: R8 = the car; R10 = stack; R11 = return address.
 * Exit: R0 = 0 at the list's end; R9 the last part; the rest as the
 * last part's update leaves them.
 */
FxPartsUpdateResult FxPartsUpdate(uint16_t entry_product_lo, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t parts;
    uint16_t fxpart_46;
    uint16_t wheel_out;
    uint16_t part_depth;
    uint16_t car_depth_flags;
    uint16_t current_part_ptr;

    fx_spill(stack);
    fx_stw((uint16_t)(car + 0x0044), 0);
    fx_stw((uint16_t)(car + FXPART_FLAGS), 0);
    fx_stw((uint16_t)(car + 0x0056), 0);
    parts = fx_ldw((uint16_t)(car + FXCAR_PARTS));
    for (;;) {
        current_part_ptr = parts;
        wheel_out = current_part_ptr;
        if (current_part_ptr == 0) {
            break;
        } else {
            fxpart_46 = fx_ldw((uint16_t)(wheel_out + FXPART_FLAGS));
            if ((fxpart_46 & 0x0800) == 0) {
                { FxWheelUpdateResult r_ = FxWheelUpdate(entry_product_lo, push_y, model_record, car, wheel_out, stack, rom_at); up_ = r_.up; entry_product_lo = r_.last_low; push_y = r_.y_row; model_record = r_.model_record_out; car = r_.car_out; wheel_out = r_.wheel_out; rom_at = r_.rom_at_out; };
                if (up_) {                       /* it returned past us */
                    return (FxPartsUpdateResult){entry_product_lo, push_y, model_record, car, wheel_out, rom_at, up_ == 1 ? 0 : up_ - 1};
                }
            } else {
                { FxPartPullResult r_ = FxPartPull(push_y, model_record, car, wheel_out, stack, rom_at); entry_product_lo = r_.quotient; push_y = r_.push_y_out; model_record = r_.high_words_hi; rom_at = r_.rom_at_out; };
            }
            part_depth = fx_ldw((uint16_t)(wheel_out + 0x002C));
            car_depth_flags = fx_ldw((uint16_t)(car + 0x0092));
            fx_sbk((car_depth_flags | part_depth));
            parts = fx_ldw((uint16_t)(wheel_out + 0x0034));
            continue;
        }
    }
    fx_unspill(stack);
    return (FxPartsUpdateResult){entry_product_lo, push_y, model_record, car, wheel_out, rom_at, 0};
}


/*
 * @routine 04:A656 FxPartPull
 * WHAT IT DOES: Pulls part R9 of car R8 toward its place on the car:
 * unless its flag bit 2 was clear (then it is only set), the part's
 * offset from the first wheel (by +$8E's list) is taken, each axis
 * under $C8 dropped, scaled by four and added to that wheel's speed,
 * the part's own speed subtracted where the signs differ; the result
 * is pushed on the car (FxSumsPushWorld, FxHitAddUnit) at the part's
 * place through the FxPushAt variant $2654 picks. With bit 0 of the
 * car's +$64 set it takes the car's speed ($2334-$2338) instead
 * (04:A7B4). Not reached in the traces so far.
 * HOW: Pushes R11; the push variant reached by LINK #2; JMP R12
 * through $04:9BC6; returns by pulling R15.
 * Entry: R8 = the car, R9 = the part; R10 = stack; R11 = return
 * address.
 * Exit: as the push leaves them.
 */
FxPartPullResult FxPartPull(uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t part, uint16_t *stack, uint16_t rom_at)
{
    uint16_t y;
    uint16_t hit;
    uint16_t y_2;
    uint16_t y_3;
    uint16_t y_4;
    uint16_t roll;
    uint16_t parts;
    uint16_t half_x;
    uint16_t part_z;
    uint16_t pull_x;
    uint16_t pull_y;
    uint16_t pull_z;
    uint16_t r_t_41;
    uint16_t flags40;
    uint16_t parts_2;
    uint16_t parts_3;
    uint16_t parts_4;
    uint16_t anchor_z;
    uint16_t fxcar_64;
    uint16_t half_x_2;
    uint16_t half_x_3;
    uint16_t half_x_4;
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t ram_test;
    uint16_t flags40_2;
    uint16_t fxpart_3c;
    uint16_t fxpart_46;
    uint16_t param_push;
    uint16_t part_z_car;
    uint16_t push_x_out;
    uint16_t quotient_2;
    uint16_t quotient_3;
    uint16_t anchor_part;
    uint16_t car_speed_y;
    uint16_t car_speed_z;
    uint16_t fxpart_3c_2;
    uint16_t fxpart_46_2;
    uint16_t fxpart_46_3;
    uint16_t fxpart_46_4;
    uint16_t offset_x_x2;
    uint16_t offset_y_x2;
    uint16_t offset_z_x2;
    uint16_t place_y_out;
    uint16_t abs_offset_x;
    uint16_t abs_offset_y;
    uint16_t abs_offset_z;
    uint16_t anchor_z_car;
    uint16_t offset_x_car;
    uint16_t offset_y_car;
    uint16_t offset_z_car;
    uint16_t param_push_2;
    uint16_t part_speed_x;
    uint16_t part_speed_y;
    uint16_t part_z_speed;
    uint16_t push_variant;
    uint16_t push_x_out_2;
    uint16_t push_x_out_3;
    uint16_t push_x_out_4;
    uint16_t push_y_out_2;
    uint16_t push_y_out_3;
    uint16_t push_y_out_4;
    uint16_t push_y_out_5;
    uint16_t next_part_ptr;
    uint16_t offset_x_copy;
    uint16_t offset_y_copy;
    uint16_t offset_z_copy;
    uint16_t place_y_out_2;
    uint16_t anchor_speed_x;
    uint16_t anchor_speed_z;
    uint16_t anchor_part_car;
    uint16_t high_words_hi_2;
    uint16_t high_words_hi_3;
    uint16_t offset_x_car_x2;
    uint16_t offset_y_car_x2;
    uint16_t offset_z_car_x2;
    uint16_t push_variant_lo;
    uint16_t model_record_out;
    uint16_t push_variant_car;
    uint16_t anchor_speed_x_x2;
    uint16_t anchor_speed_x_x4;
    uint16_t anchor_speed_y_x2;
    uint16_t anchor_speed_y_x4;
    uint16_t anchor_speed_z_x2;
    uint16_t anchor_speed_z_x4;
    uint16_t next_part_ptr_car;
    uint16_t push_table_offset;
    uint16_t model_record_out_2;
    uint16_t push_variant_hi_ptr;
    uint16_t push_variant_lo_car;
    uint16_t push_table_offset_car;
    uint16_t push_variant_hi_ptr_car;

    fxcar_64 = fx_ldw((uint16_t)(car + FXCAR_64));
    if ((fxcar_64 & 1) == 0) {
        fx_spill(stack);
        fxpart_46 = fx_ldw((uint16_t)(part + FXPART_FLAGS));
        if ((4 & fxpart_46) != 0) {
            anchor_part = fx_ldw((uint16_t)(car + 0x008E));
            roll = fx_ldw((uint16_t)(anchor_part + FXREC_ROLL));
            fx_stw((uint16_t)(car + FXREC_ROLL), roll);
            parts = fx_ldw((uint16_t)(anchor_part + FXCAR_PARTS));
            for (;;) {
                next_part_ptr = parts;
                if (next_part_ptr == 0) {
                    break;
                } else {
                    anchor_part = next_part_ptr;
                    fxpart_46_2 = fx_ldw((uint16_t)(anchor_part + FXPART_FLAGS));
                    if ((fxpart_46_2 & 0x0800) != 0) {
                        break;
                    } else {
                        parts = fx_ldw((uint16_t)(anchor_part + 0x0034));
                        continue;
                    }
                }
            }
            half_x = fx_ldw((uint16_t)(anchor_part + FXREC_HALF_X));
            y = fx_ldw((uint16_t)(anchor_part + FXREC_Y));
            anchor_z = fx_ldw((uint16_t)(anchor_part + 0x0022));
            half_x_2 = fx_ldw((uint16_t)(part + FXREC_HALF_X));
            offset_x = (uint16_t)(half_x - half_x_2);
            y_2 = fx_ldw((uint16_t)(part + FXREC_Y));
            offset_y = (uint16_t)(y - y_2);
            part_z = fx_ldw((uint16_t)(part + 0x0022));
            ram_test = (uint16_t)(anchor_z - part_z);
            fx_stw(FX_RAM_TEST, ram_test);
            offset_x_copy = offset_x;
            abs_offset_x = offset_x_copy;
            if (!(offset_x_copy & 0x8000)) {
            } else {
                abs_offset_x = (uint16_t)((uint16_t)~abs_offset_x + 1);
            }
            if (!((uint16_t)(abs_offset_x - 0x00C8) & 0x8000)) {
            } else {
                offset_x = 0;
            }
            offset_y_copy = offset_y;
            abs_offset_y = offset_y_copy;
            if (!(offset_y_copy & 0x8000)) {
            } else {
                abs_offset_y = (uint16_t)((uint16_t)~abs_offset_y + 1);
            }
            if (!((uint16_t)(abs_offset_y - 0x00C8) & 0x8000)) {
            } else {
                offset_y = 0;
            }
            offset_z_copy = ram_test;
            abs_offset_z = offset_z_copy;
            if (!(offset_z_copy & 0x8000)) {
            } else {
                abs_offset_z = (uint16_t)((uint16_t)~abs_offset_z + 1);
            }
            if (!((uint16_t)(abs_offset_z - 0x00C8) & 0x8000)) {
            } else {
                ram_test = 0;
            }
            offset_x_x2 = (uint16_t)(offset_x + offset_x);
            offset_y_x2 = (uint16_t)(offset_y + offset_y);
            offset_z_x2 = (uint16_t)(ram_test + ram_test);
            pull_x = (uint16_t)(offset_x_x2 + offset_x_x2);
            pull_y = (uint16_t)(offset_y_x2 + offset_y_x2);
            pull_z = (uint16_t)(offset_z_x2 + offset_z_x2);
            anchor_speed_x = fx_ldw((uint16_t)(anchor_part + 0x002E));
            fx_ldw((uint16_t)(anchor_part + 0x0030));
            anchor_speed_z = fx_ldw((uint16_t)(anchor_part + 0x0032));
            anchor_speed_x_x2 = (uint16_t)(anchor_speed_x + anchor_speed_x);
            anchor_speed_y_x2 = 0;
            anchor_speed_z_x2 = (uint16_t)(anchor_speed_z + anchor_speed_z);
            anchor_speed_x_x4 = (uint16_t)(anchor_speed_x_x2 + anchor_speed_x_x2);
            anchor_speed_y_x4 = (uint16_t)(anchor_speed_y_x2 + anchor_speed_y_x2);
            anchor_speed_z_x4 = (uint16_t)(anchor_speed_z_x2 + anchor_speed_z_x2);
            part_speed_x = fx_ldw((uint16_t)(part + 0x002E));
            if (!((part_speed_x ^ pull_x) & 0x8000)) {
            } else {
                pull_x = (uint16_t)(pull_x - part_speed_x);
            }
            part_speed_y = fx_ldw((uint16_t)(part + 0x0030));
            if (!((part_speed_y ^ pull_y) & 0x8000)) {
            } else {
                pull_y = (uint16_t)(pull_y - part_speed_y);
            }
            part_z_speed = fx_ldw((uint16_t)(part + 0x0032));
            fx_stw(FX_PART_Z_SPEED, part_z_speed);
            r_t_41 = (part_z_speed ^ pull_z);
            if (!(r_t_41 & 0x8000)) {
            } else {
                pull_y = (uint16_t)(pull_y - part_z_speed);
            }
            { FxSumsPushWorldResult r_ = FxSumsPushWorld((uint16_t)(pull_x + anchor_speed_x_x4), (uint16_t)(pull_y + anchor_speed_y_x4), (uint16_t)(pull_z + anchor_speed_z_x4), stack); push_x_out = r_.push_x_out; push_y_out_2 = r_.push_y_out; };
            fx_ldw((uint16_t)(part + FXPART_WORLD));
            fx_ldw((uint16_t)(part + 0x0038));
            fx_ldw((uint16_t)(part + FXCAR_STEER_BIAS));
            { FxHitAddUnitResult r_ = FxHitAddUnit(0, 0, 0, push_x_out, push_y_out_2, r_t_41, part, stack); quotient_2 = r_.quotient; place_y_out = r_.place_y_out; high_words_hi_2 = r_.high_words_hi; };
            fxpart_3c = fx_ldw((uint16_t)(part + FXPART_3C));
            parts_2 = fx_ldw((uint16_t)(part + FXCAR_PARTS));
            flags40 = fx_ldw((uint16_t)(part + FXREC_FLAGS40));
            fx_romb(4);
            fx_table_bank = 0x04;
            param_push = fx_ldw(FX_PARAM_PUSH);
            fx_r14(0x9BC6);
            push_table_offset = (uint16_t)(param_push - 0x643A);
            fx_r14(push_table_offset);
            push_variant_lo = fx_getb();
            push_variant_hi_ptr = (uint16_t)(push_table_offset + 1);
            fx_r14(push_variant_hi_ptr);
            push_variant = (uint16_t)((push_variant_lo & 0xFF) | fx_getb() << 8);
            { FxPushAtVariantResult r_ = FxPushAtVariant(fxpart_3c, parts_2, flags40, quotient_2, place_y_out, high_words_hi_2, stack, push_variant); push_x_out_2 = r_.push_x_out; push_y_out_3 = r_.push_y_out; model_record_out = r_.model_record_out; };
            fx_unspill(stack);
            return (FxPartPullResult){push_x_out_2, push_y_out_3, model_record_out, push_variant_hi_ptr};
        } else {
            fx_sbk((4 | fxpart_46));
            fx_unspill(stack);
            return (FxPartPullResult){1, push_y, model_record, rom_at};
        }
    } else {
        hit = fx_ldw(FX_HIT);
        fx_stw((uint16_t)(part + 0x002E), hit);
        car_speed_y = fx_ldw(0x2336);
        fx_stw((uint16_t)(part + 0x0030), car_speed_y);
        car_speed_z = fx_ldw(0x2338);
        fx_stw((uint16_t)(part + 0x0032), car_speed_z);
        fx_spill(stack);
        fxpart_46_3 = fx_ldw((uint16_t)(part + FXPART_FLAGS));
        if ((4 & fxpart_46_3) != 0) {
            anchor_part_car = fx_ldw((uint16_t)(car + 0x008E));
            parts_3 = fx_ldw((uint16_t)(anchor_part_car + FXCAR_PARTS));
            for (;;) {
                next_part_ptr_car = parts_3;
                if (next_part_ptr_car == 0) {
                    break;
                } else {
                    anchor_part_car = next_part_ptr_car;
                    fxpart_46_4 = fx_ldw((uint16_t)(anchor_part_car + FXPART_FLAGS));
                    if ((fxpart_46_4 & 0x0800) != 0) {
                        break;
                    } else {
                        parts_3 = fx_ldw((uint16_t)(anchor_part_car + 0x0034));
                        continue;
                    }
                }
            }
            half_x_3 = fx_ldw((uint16_t)(anchor_part_car + FXREC_HALF_X));
            y_3 = fx_ldw((uint16_t)(anchor_part_car + FXREC_Y));
            anchor_z_car = fx_ldw((uint16_t)(anchor_part_car + 0x0022));
            half_x_4 = fx_ldw((uint16_t)(part + FXREC_HALF_X));
            offset_x_car = (uint16_t)(half_x_3 - half_x_4);
            y_4 = fx_ldw((uint16_t)(part + FXREC_Y));
            offset_y_car = (uint16_t)(y_3 - y_4);
            part_z_car = fx_ldw((uint16_t)(part + 0x0022));
            offset_z_car = (uint16_t)(anchor_z_car - part_z_car);
            offset_x_car_x2 = (uint16_t)(offset_x_car + offset_x_car);
            offset_y_car_x2 = (uint16_t)(offset_y_car + offset_y_car);
            offset_z_car_x2 = (uint16_t)(offset_z_car + offset_z_car);
            { FxSumsPushWorldResult r_ = FxSumsPushWorld((uint16_t)(offset_x_car_x2 + offset_x_car_x2), (uint16_t)(offset_y_car_x2 + offset_y_car_x2), (uint16_t)(offset_z_car_x2 + offset_z_car_x2), stack); push_x_out_3 = r_.push_x_out; push_y_out_4 = r_.push_y_out; };
            fx_ldw((uint16_t)(part + FXPART_WORLD));
            fx_ldw((uint16_t)(part + 0x0038));
            fx_ldw((uint16_t)(part + FXCAR_STEER_BIAS));
            { FxHitAddUnitResult r_ = FxHitAddUnit(0, 0, 0, push_x_out_3, push_y_out_4, anchor_part_car, part, stack); quotient_3 = r_.quotient; place_y_out_2 = r_.place_y_out; high_words_hi_3 = r_.high_words_hi; };
            fxpart_3c_2 = fx_ldw((uint16_t)(part + FXPART_3C));
            parts_4 = fx_ldw((uint16_t)(part + FXCAR_PARTS));
            flags40_2 = fx_ldw((uint16_t)(part + FXREC_FLAGS40));
            fx_romb(4);
            fx_table_bank = 0x04;
            param_push_2 = fx_ldw(FX_PARAM_PUSH);
            fx_r14(0x9BC6);
            push_table_offset_car = (uint16_t)(param_push_2 - 0x643A);
            fx_r14(push_table_offset_car);
            push_variant_lo_car = fx_getb();
            push_variant_hi_ptr_car = (uint16_t)(push_table_offset_car + 1);
            fx_r14(push_variant_hi_ptr_car);
            push_variant_car = (uint16_t)((push_variant_lo_car & 0xFF) | fx_getb() << 8);
            { FxPushAtVariantResult r_ = FxPushAtVariant(fxpart_3c_2, parts_4, flags40_2, quotient_3, place_y_out_2, high_words_hi_3, stack, push_variant_car); push_x_out_4 = r_.push_x_out; push_y_out_5 = r_.push_y_out; model_record_out_2 = r_.model_record_out; };
            fx_unspill(stack);
            return (FxPartPullResult){push_x_out_4, push_y_out_5, model_record_out_2, push_variant_hi_ptr_car};
        } else {
            fx_sbk((4 | fxpart_46_3));
            fx_unspill(stack);
            return (FxPartPullResult){1, push_y, model_record, rom_at};
        }
    }
}


/*
 * @routine 04:A893 FxWheelUpdate
 * WHAT IT DOES: Updates wheel R9 of car R8: copies the car's heading
 * and height, finds the ground under it (FxCollideModel with the
 * touched record, FxContactsResolve), takes the surface kind from the
 * model's flags into its contact flags (+$2C), and from the contact
 * depth and force works the suspension load, the grip along and across
 * the car (by the handling words $2652-$2682) and pushes the car with
 * them (FxSumsPushWorld, FxSumsPushLocal, FxSumsRotateLocal,
 * FxHitAddScaled). A wheel off the ground ($237E = $7D00) is handled
 * at 04:AEE5.
 * HOW: Pushes R11; $2484 = the wheel's flags while it runs; the
 * contact slots cleared and the sums cleared (FxSumsClear); returns by
 * pulling R15.
 * Entry: R8 = the car, R9 = the wheel; R10 = stack; R11 = return
 * address.
 * Exit: $2484 = the wheel's flags; the rest as the pushes leave them.
 */
FxWheelUpdateResult FxWheelUpdate(uint16_t entry_product_lo, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t wheel, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    int run_lA8CC = 0;
    int run_lAA0D = 0;
    uint16_t y;
    uint16_t list;
    uint16_t roll;
    uint16_t parts;
    uint16_t pitch;
    uint16_t sides;
    uint16_t amount;
    uint16_t half_x;
    uint16_t flags16;
    uint16_t flags40;
    uint16_t pitch_2;
    uint16_t pitch_3;
    uint16_t pitch_4;
    uint16_t pitch_5;
    uint16_t vector_y;
    uint16_t flags16_2;
    uint16_t flags40_2;
    uint16_t flags40_3;
    uint16_t fxpart_3c;
    uint16_t fxpart_46;
    uint16_t fxpart_50;
    uint16_t list_face;
    uint16_t old_world;
    uint16_t param_hit;
    uint16_t push_flag;
    uint16_t wheel_yaw;
    uint16_t world_sum;
    uint16_t grip_limit;
    uint16_t last_low_2;
    uint16_t param_push;
    uint16_t steer_bias;
    uint16_t wheel_side;
    uint16_t ahead_speed;
    uint16_t car_speed_z;
    uint16_t contact_sum;
    uint16_t fxpart_3c_2;
    uint16_t fxpart_3c_3;
    uint16_t fxpart_46_2;
    uint16_t fxpart_46_3;
    uint16_t fxpart_50_2;
    uint16_t grip_demand;
    uint16_t grip_excess;
    uint16_t neg_speed_x;
    uint16_t neg_speed_y;
    uint16_t neg_speed_z;
    uint32_t side_prod_x;
    uint32_t side_prod_y;
    uint32_t side_prod_z;
    uint16_t wheel_ahead;
    uint16_t wheel_flags;
    uint16_t world_sum_y;
    uint16_t world_sum_z;
    uint32_t ahead_prod_x;
    uint32_t ahead_prod_y;
    uint32_t ahead_prod_z;
    uint16_t car_yaw_rate;
    uint16_t fxpart_world;
    uint16_t param_bounce;
    uint16_t steer_bias_2;
    uint16_t steer_bias_3;
    uint16_t steer_bias_4;
    uint16_t collide_model;
    uint16_t contact_depth;
    uint16_t contact_force;
    uint16_t contact_sum_y;
    uint16_t contact_sum_z;
    uint16_t drag_push_raw;
    uint16_t view_matrix_2;
    uint16_t view_matrix_8;
    uint16_t wheel_ahead_2;
    uint16_t wheel_flags_2;
    uint16_t wheel_flags_3;
    uint16_t wheel_flags_4;
    uint16_t wheel_flags_5;
    uint16_t wheel_flags_6;
    uint16_t wheel_flags_7;
    uint16_t wheel_flags_8;
    uint16_t wheel_flags_9;
    uint16_t wheel_speed_x;
    uint16_t wheel_speed_y;
    uint16_t wheel_speed_z;
    uint16_t car_counter_44;
    uint16_t grip_demand_x2;
    uint16_t grip_demand_x4;
    uint16_t part_load_copy;
    uint16_t push_flags_out;
    uint16_t wheel_field_22;
    uint16_t wheel_flags_10;
    uint16_t wheel_flags_11;
    uint16_t wheel_load_cap;
    uint16_t car_drag_factor;
    uint16_t car_ground_kind;
    uint16_t collide_model_2;
    uint16_t collide_model_3;
    uint16_t contact_force_2;
    uint16_t contact_force_3;
    uint16_t contact_speed_x;
    uint16_t contact_speed_y;
    uint16_t contact_speed_z;
    uint16_t grip_demand_abs;
    uint16_t grip_excess_abs;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t param_grip_free;
    uint16_t param_grip_side;
    uint16_t push_routine_lo;
    uint16_t view_matrix_2_2;
    uint16_t view_matrix_8_2;
    uint16_t ahead_factor_raw;
    uint16_t contact_force_up;
    uint16_t cross_factor_raw;
    uint16_t grip_demand_copy;
    uint16_t grip_demand_sign;
    uint16_t neg_bounce_limit;
    uint16_t param_grip_ahead;
    uint16_t param_grip_slide;
    uint16_t pitch_diff_index;
    uint16_t push_table_index;
    uint16_t car_ground_kind_b;
    uint16_t car_slide_total_a;
    uint16_t car_slide_total_b;
    uint16_t curve_ahead_entry;
    uint16_t curve_cross_entry;
    uint16_t drag_push_clamped;
    uint16_t flags_without_400;
    uint16_t object_matrix_6_2;
    uint16_t object_matrix_7_2;
    uint16_t object_matrix_8_2;
    uint16_t param_slide_limit;
    uint16_t push_routine_addr;
    uint16_t world_sum_y_check;
    uint16_t contact_force_side;
    uint16_t model_surface_bits;
    uint16_t wheel_load_doubled;
    uint16_t car_contact_flags_a;
    uint16_t car_contact_flags_b;
    uint16_t push_point_y_offset;
    uint16_t push_point_z_offset;
    uint16_t contact_sum_x_eighth;
    uint16_t contact_sum_y_eighth;
    uint16_t contact_sum_z_eighth;
    uint16_t ground_contact_flags;
    uint16_t surface_contact_flags;
    uint16_t wheel_contact_flags_a;
    uint16_t wheel_contact_flags_b;
    uint16_t wheel_contact_flags_c;
    uint16_t wheel_contact_flags_d;
    uint16_t wheel_contact_flags_e;
    uint16_t wheel_contact_flags_surface;

    fx_spill(stack);
    fxpart_46 = fx_ldw((uint16_t)(wheel + FXPART_FLAGS));
    fx_stw(FX_WHEEL_FLAGS, fxpart_46);
    fx_stw((uint16_t)(wheel + 0x002C), 0);
    flags40 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
    wheel_flags = fx_ldw(FX_WHEEL_FLAGS);
    flags_without_400 = (uint16_t)(wheel_flags & ~0x0400);
    if ((0x0200 & flags_without_400) == 0) {
        run_lA8CC = 1;
    } else {
        if ((0 & flags40) != 0) {
            run_lA8CC = 1;
        } else {
            fx_sbk((flags_without_400 | 0x0400));
        }
    }
    if (run_lA8CC) {
        run_lA8CC = 0;
        fx_sbk(flags_without_400);
        pitch = fx_ldw((uint16_t)(car + FXREC_HEADING));
        fx_stw((uint16_t)(wheel + FXREC_HEADING), pitch);
        roll = fx_ldw((uint16_t)(car + FXREC_ROLL));
        fx_stw((uint16_t)(wheel + FXREC_ROLL), roll);
        wheel_flags_2 = fx_ldw(FX_WHEEL_FLAGS);
        if ((2 & wheel_flags_2) == 0) {
        } else {
            steer_bias = fx_ldw((uint16_t)(car + FXCAR_STEER_BIAS));
            pitch_2 = fx_ldw((uint16_t)(wheel + FXREC_HEADING));
            fx_sbk((uint16_t)(steer_bias + pitch_2));
        }
        wheel_flags_3 = fx_ldw(FX_WHEEL_FLAGS);
        if ((wheel_flags_3 & 0x0080) == 0) {
            FxSumsClear();
            fx_stw(FX_PUSHES, 0);
            fx_stw(0x23B0, 0x7D00);
            fx_stw(0x23B8, 0);
            fx_stw(0x23BA, 0x7D00);
            fx_stw(0x23C2, 0);
            fx_stw(0x23C4, 0x7D00);
            fx_stw(0x23CC, 0);
            fx_stw(0x23CE, 0x7D00);
            fx_stw(0x23D6, 0);
            fx_stw(0x23D8, 0x7D00);
            fx_stw(0x23E0, 0);
            fx_stw(0x23E2, 0x7D00);
            fx_stw(0x23EA, 0);
            fx_stw(0x23EC, 0x7D00);
            fx_stw(0x23F4, 0);
            fx_stw(0x23F6, 0x7D00);
            fx_stw(0x23FE, 0);
            fx_stw(0x2400, 0x7D00);
            fx_stw(0x2408, 0);
            fx_stw(0x240A, 0x7D00);
            fx_stw(0x2412, 0);
            fx_stw(0x2414, 0x7D00);
            fx_stw(0x241C, 0);
            fx_stw(0x241E, 0x7D00);
            fx_stw(0x2426, 0);
            fx_stw(0x2428, 0x7D00);
            fx_stw(0x2430, 0);
            fx_stw(0x2432, 0x7D00);
            fx_stw(0x243A, 0);
            fx_stw(0x243C, 0x7D00);
            fx_stw(0x2444, 0);
            fx_stw(0x2446, 0x7D00);
            fx_stw(0x244E, 0);
            fx_stw(0x2450, 0x7D00);
            fx_stw(0x2458, 0);
            fx_stw(0x245A, 0x7D00);
            fx_stw(0x2462, 0);
            fx_stw(0x2464, 0x7D00);
            fx_stw(FX_CONTACT_SUM, 0);
            fx_stw(0x238A, 0);
            fx_stw(0x238C, 0);
            fx_stw(FX_CONTACT_DEPTH, 0x7D00);
            fx_stw(FX_WHEEL_CONTACT, 0);
            wheel_flags_4 = fx_ldw(FX_WHEEL_FLAGS);
            fx_sbk((uint16_t)(wheel_flags_4 & ~0x0030));
            list = fx_ldw((uint16_t)(wheel + FXREC_LIST));
            if (list != 0) {
                collide_model = fx_ldw((uint16_t)(list + 0x000C));
                fx_stw(FX_COLLIDE_MODEL, collide_model);
                { FxCollideModelResult r_ = FxCollideModel(entry_product_lo, push_y, collide_model, car, wheel, stack, rom_at); up_ = r_.up; list_face = r_.list_face; entry_product_lo = r_.quotient; push_y = r_.place_y_out; model_record = r_.model_record_out; car = r_.caller_scratch_back; wheel = r_.record_back; rom_at = r_.rom_at_out; };
                if (up_) {                       /* it returned past us */
                    return (FxWheelUpdateResult){entry_product_lo, push_y, model_record, car, wheel, rom_at, up_ == 1 ? 0 : up_ - 1};
                }
                if (list_face == 0) {
                    run_lAA0D = 1;
                } else {
                    { FxContactsResolveResult r_ = FxContactsResolve(entry_product_lo, push_y, model_record, car, wheel, stack, rom_at); push_flags_out = r_.push_flags_out; entry_product_lo = r_.entry_product_lo; push_y = r_.push_y_out; model_record = r_.model_record_out; rom_at = r_.rom_at_out; };
                    if (push_flags_out == 0) {
                        run_lAA0D = 1;
                    } else {
                        if ((0x001F & push_flags_out) != 0) {
                            if ((0x0020 & push_flags_out) == 0) {
                                collide_model_2 = fx_ldw(FX_COLLIDE_MODEL);
                                flags16 = fx_ldw((uint16_t)(collide_model_2 + FXREC_FLAGS16));
                                if ((0x4000 & flags16) != 0) {
                                } else {
                                    car_ground_kind = fx_ldw((uint16_t)(car + 0x008A));
                                    if ((0x0040 & car_ground_kind) == 0) {
                                    } else {
                                        wheel_contact_flags_a = fx_ldw((uint16_t)(wheel + 0x002C));
                                        fx_sbk((wheel_contact_flags_a | 0x1000));
                                    }
                                }
                                car_contact_flags_a = fx_ldw((uint16_t)(car + 0x002C));
                                fx_sbk((uint16_t)(car_contact_flags_a & ~0x0040));
                            } else {
                                run_lAA0D = 1;
                            }
                        } else {
                            car_ground_kind_b = fx_ldw((uint16_t)(car + 0x008A));
                            ground_contact_flags = 0x0040;
                            if ((4 & car_ground_kind_b) != 0) {
                            } else {
                                ground_contact_flags = 0x00C0;
                                if ((0x0020 & car_ground_kind_b) != 0) {
                                } else {
                                    ground_contact_flags = 0x1040;
                                    if ((0x0040 & car_ground_kind_b) != 0) {
                                    } else {
                                        ground_contact_flags = 0x0840;
                                        if ((2 & car_ground_kind_b) != 0) {
                                        } else {
                                            ground_contact_flags = 0x0060;
                                        }
                                    }
                                }
                            }
                            wheel_contact_flags_b = fx_ldw((uint16_t)(wheel + 0x002C));
                            fx_sbk((wheel_contact_flags_b | ground_contact_flags));
                        }
                    }
                }
                if (run_lAA0D) {
                    run_lAA0D = 0;
                    collide_model_3 = fx_ldw(FX_COLLIDE_MODEL);
                    flags16_2 = fx_ldw((uint16_t)(collide_model_3 + FXREC_FLAGS16));
                    if ((0x0040 & flags16_2) != 0) {
                        collide_model_2 = fx_ldw(FX_COLLIDE_MODEL);
                        flags16 = fx_ldw((uint16_t)(collide_model_2 + FXREC_FLAGS16));
                        if ((0x4000 & flags16) != 0) {
                        } else {
                            car_ground_kind = fx_ldw((uint16_t)(car + 0x008A));
                            if ((0x0040 & car_ground_kind) == 0) {
                            } else {
                                wheel_contact_flags_a = fx_ldw((uint16_t)(wheel + 0x002C));
                                fx_sbk((wheel_contact_flags_a | 0x1000));
                            }
                        }
                        car_contact_flags_a = fx_ldw((uint16_t)(car + 0x002C));
                        fx_sbk((uint16_t)(car_contact_flags_a & ~0x0040));
                    } else {
                        model_surface_bits = (0x3800 & flags16_2);
                        if (model_surface_bits == 0) {
                            car_contact_flags_b = fx_ldw((uint16_t)(car + 0x002C));
                            fx_sbk((uint16_t)(car_contact_flags_b & ~0x0040));
                            car_ground_kind_b = fx_ldw((uint16_t)(car + 0x008A));
                            ground_contact_flags = 0x0040;
                            if ((4 & car_ground_kind_b) != 0) {
                            } else {
                                ground_contact_flags = 0x00C0;
                                if ((0x0020 & car_ground_kind_b) != 0) {
                                } else {
                                    ground_contact_flags = 0x1040;
                                    if ((0x0040 & car_ground_kind_b) != 0) {
                                    } else {
                                        ground_contact_flags = 0x0840;
                                        if ((2 & car_ground_kind_b) != 0) {
                                        } else {
                                            ground_contact_flags = 0x0060;
                                        }
                                    }
                                }
                            }
                            wheel_contact_flags_b = fx_ldw((uint16_t)(wheel + 0x002C));
                            fx_sbk((wheel_contact_flags_b | ground_contact_flags));
                        } else {
                            surface_contact_flags = 0x00A0;
                            if ((uint16_t)((uint16_t)(0 - model_surface_bits) + 0x0800) == 0) {
                                wheel_contact_flags_surface = fx_ldw((uint16_t)(wheel + 0x002C));
                                fx_sbk((wheel_contact_flags_surface | surface_contact_flags));
                            } else {
                                surface_contact_flags = 0x2020;
                                if ((uint16_t)((uint16_t)(0 - model_surface_bits) + 0x2000) == 0) {
                                    wheel_contact_flags_surface = fx_ldw((uint16_t)(wheel + 0x002C));
                                    fx_sbk((wheel_contact_flags_surface | surface_contact_flags));
                                } else {
                                    surface_contact_flags = 0x1020;
                                    if ((uint16_t)((uint16_t)(0 - model_surface_bits) + 0x1000) == 0) {
                                        wheel_contact_flags_surface = fx_ldw((uint16_t)(wheel + 0x002C));
                                        fx_sbk((wheel_contact_flags_surface | surface_contact_flags));
                                    } else {
                                        surface_contact_flags = 0x0820;
                                        if ((uint16_t)((uint16_t)(0 - model_surface_bits) + 0x1800) != 0) {
                                            collide_model_2 = fx_ldw(FX_COLLIDE_MODEL);
                                            flags16 = fx_ldw((uint16_t)(collide_model_2 + FXREC_FLAGS16));
                                            if ((0x4000 & flags16) != 0) {
                                            } else {
                                                car_ground_kind = fx_ldw((uint16_t)(car + 0x008A));
                                                if ((0x0040 & car_ground_kind) == 0) {
                                                } else {
                                                    wheel_contact_flags_a = fx_ldw((uint16_t)(wheel + 0x002C));
                                                    fx_sbk((wheel_contact_flags_a | 0x1000));
                                                }
                                            }
                                        } else {
                                            wheel_contact_flags_surface = fx_ldw((uint16_t)(wheel + 0x002C));
                                            fx_sbk((wheel_contact_flags_surface | surface_contact_flags));
                                        }
                                    }
                                }
                            }
                            car_contact_flags_a = fx_ldw((uint16_t)(car + 0x002C));
                            fx_sbk((uint16_t)(car_contact_flags_a & ~0x0040));
                        }
                    }
                }
            } else {
                car_ground_kind_b = fx_ldw((uint16_t)(car + 0x008A));
                ground_contact_flags = 0x0040;
                if ((4 & car_ground_kind_b) != 0) {
                } else {
                    ground_contact_flags = 0x00C0;
                    if ((0x0020 & car_ground_kind_b) != 0) {
                    } else {
                        ground_contact_flags = 0x1040;
                        if ((0x0040 & car_ground_kind_b) != 0) {
                        } else {
                            ground_contact_flags = 0x0840;
                            if ((2 & car_ground_kind_b) != 0) {
                            } else {
                                ground_contact_flags = 0x0060;
                            }
                        }
                    }
                }
                wheel_contact_flags_b = fx_ldw((uint16_t)(wheel + 0x002C));
                fx_sbk((wheel_contact_flags_b | ground_contact_flags));
            }
            y = fx_ldw((uint16_t)(wheel + FXREC_Y));
            amount = y;
            if (y & 0x8000) {
            } else {
                wheel_contact_flags_c = fx_ldw((uint16_t)(wheel + 0x002C));
                if ((wheel_contact_flags_c & 0x0020) == 0) {
                } else {
                    half_x = fx_ldw((uint16_t)(wheel + FXREC_HALF_X));
                    wheel_field_22 = fx_ldw((uint16_t)(wheel + 0x0022));
                    amount = (uint16_t)((uint16_t)(amount + (fx_div2(fx_div2(fx_div2((uint16_t)(half_x + wheel_field_22)))) & 0x000F)) + 0x000A);
                }
                push_flag = FxPushSet(amount);
                if (push_flag == 0) {
                } else {
                    { FxContactResolveResult r_ = FxContactResolve(0x23A4, entry_product_lo, push_y, model_record, wheel, stack, rom_at); entry_product_lo = r_.push_x_out; push_y = r_.push_y_out; model_record = r_.model_record_out; rom_at = r_.rom_at_out; };
                }
            }
            contact_force_up = fx_ldw(FX_CONTACT_FORCE_UP);
            fx_stw(FX_CONTACT_FORCE, contact_force_up);
            wheel_contact_flags_d = fx_ldw((uint16_t)(wheel + 0x002C));
            if ((0x0020 & wheel_contact_flags_d) == 0) {
            } else {
                contact_force_side = fx_ldw(FX_CONTACT_FORCE_SIDE);
                fx_stw(FX_CONTACT_FORCE, contact_force_side);
                fxpart_46_2 = fx_ldw((uint16_t)(car + FXPART_FLAGS));
                fx_sbk((uint16_t)(fxpart_46_2 + 1));
                world_sum_y_check = fx_ldw(0x2342);
                if (world_sum_y_check != 0) {
                } else {
                    wheel_contact_flags_e = fx_ldw((uint16_t)(wheel + 0x002C));
                    fx_sbk((uint16_t)(wheel_contact_flags_e & ~0x0020));
                }
            }
            wheel_flags_5 = fx_ldw(FX_WHEEL_FLAGS);
            contact_depth = fx_ldw(FX_CONTACT_DEPTH);
            if (contact_depth != 0x7D00) {
                flags40_2 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                fx_sbk((uint16_t)(flags40_2 & ~0x0100));
                fx_stw(FX_WHEEL_FLAGS, (0x0010 | wheel_flags_5));
                wheel_flags_6 = fx_ldw(FX_WHEEL_FLAGS);
                if ((8 & wheel_flags_6) == 0) {
                } else {
                    fx_stw(FX_WHEEL_LOAD, 0);
                    fxpart_50 = fx_ldw((uint16_t)(wheel + FXPART_50));
                    contact_force = fx_ldw(FX_CONTACT_FORCE);
                    wheel_load_cap = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50 + fxpart_50) * (int16_t)contact_force) >> 16);
                    fxpart_3c = fx_ldw((uint16_t)(car + FXPART_3C));
                    car_speed_z = fx_ldw((uint16_t)(car + 0x004E));
                    if (!((uint16_t)(car_speed_z - 2) & 0x8000)) {
                        wheel_load_doubled = (uint16_t)(fxpart_3c + fxpart_3c);
                        fx_stw(FX_WHEEL_LOAD, wheel_load_doubled);
                        object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                        object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                        object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                        FxSumsPushWorld((uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_6 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_7 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_8 * (int16_t)wheel_load_doubled) >> 16), stack);
                    } else {
                        part_load_copy = fxpart_3c;
                        fxpart_3c = part_load_copy;
                        if (part_load_copy == 0) {
                        } else {
                            if (part_load_copy & 0x8000) {
                            } else {
                                if (!((uint16_t)(wheel_load_cap - fxpart_3c) & 0x8000)) {
                                } else {
                                    fxpart_3c = wheel_load_cap;
                                    fx_stw(FX_WHEEL_FLAGS, (0x0020 | wheel_flags_6));
                                    car_slide_total_a = fx_ldw((uint16_t)(car + 0x0056));
                                    fx_sbk((uint16_t)(car_slide_total_a + 0x1F40));
                                    contact_force_2 = fx_ldw(FX_CONTACT_FORCE);
                                    fx_sbk(fx_div2(contact_force_2));
                                }
                            }
                            wheel_load_doubled = (uint16_t)(fxpart_3c + fxpart_3c);
                            fx_stw(FX_WHEEL_LOAD, wheel_load_doubled);
                            object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                            object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                            object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                            FxSumsPushWorld((uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_6 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_7 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_8 * (int16_t)wheel_load_doubled) >> 16), stack);
                        }
                    }
                }
                fx_romb(0);
                fx_table_bank = 0x00;
                pitch_3 = fx_ldw((uint16_t)(wheel + FXREC_HEADING));
                pitch_4 = fx_ldw((uint16_t)(car + FXREC_HEADING));
                pitch_diff_index = (uint16_t)((uint16_t)(pitch_3 - pitch_4) >> 8);
                fx_r14(0x96FE);
                fx_r14((uint16_t)((uint16_t)(pitch_diff_index & 0xFF) - 0x6902));
                curve_cross_entry = fx_getb();
                cross_factor_raw = fx_div2(fx_div2((uint16_t)(curve_cross_entry << 8 | curve_cross_entry >> 8)));
                view_matrix_2 = cross_factor_raw;
                if (cross_factor_raw & 0x8000) {
                    if (!((uint16_t)(view_matrix_2 + 0x0FA0) & 0x8000)) {
                    } else {
                        view_matrix_2 = 0xF060;
                    }
                } else {
                    if ((uint16_t)(view_matrix_2 - 0x0FA0) & 0x8000) {
                    } else {
                        view_matrix_2 = 0x0FA0;
                    }
                }
                fx_stw(FX_VIEW_MATRIX_2, view_matrix_2);
                fx_r14(0x973E);
                rom_at = (uint16_t)((uint16_t)(pitch_diff_index & 0xFF) - 0x68C2);
                fx_r14(rom_at);
                curve_ahead_entry = fx_getb();
                ahead_factor_raw = (uint16_t)(curve_ahead_entry << 8 | curve_ahead_entry >> 8);
                view_matrix_8 = ahead_factor_raw;
                if (!(ahead_factor_raw & 0x8000)) {
                } else {
                    view_matrix_8 = (uint16_t)((uint16_t)~view_matrix_8 + 1);
                }
                fx_stw(FX_VIEW_MATRIX_8, view_matrix_8);
                wheel_speed_x = fx_ldw((uint16_t)(wheel + 0x002E));
                neg_speed_x = (uint16_t)((uint16_t)~wheel_speed_x + 1);
                wheel_speed_y = fx_ldw((uint16_t)(wheel + 0x0030));
                neg_speed_y = (uint16_t)((uint16_t)~wheel_speed_y + 1);
                wheel_speed_z = fx_ldw((uint16_t)(wheel + 0x0032));
                neg_speed_z = (uint16_t)((uint16_t)~wheel_speed_z + 1);
                object_matrix_0 = fx_ldw(0x00E4);
                side_prod_x = (uint32_t)((int32_t)(int16_t)neg_speed_x * (int16_t)object_matrix_0);
                object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
                side_prod_y = (uint32_t)((int32_t)(int16_t)neg_speed_y * (int16_t)object_matrix_1);
                object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
                side_prod_z = (uint32_t)((int32_t)(int16_t)neg_speed_z * (int16_t)object_matrix_2);
                fx_stw(FX_WHEEL_SIDE, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(side_prod_z >> 16) << 1 | (int)(side_prod_z >> 15 & 1)) + (uint16_t)((uint16_t)(side_prod_x >> 16) << 1 | (int)(side_prod_x >> 15 & 1))) + (uint16_t)((uint16_t)(side_prod_y >> 16) << 1 | (int)(side_prod_y >> 15 & 1))));
                object_matrix_6_2 = fx_ldw(FX_OBJECT_MATRIX_6);
                ahead_prod_x = (uint32_t)((int32_t)(int16_t)neg_speed_x * (int16_t)object_matrix_6_2);
                object_matrix_7_2 = fx_ldw(FX_OBJECT_MATRIX_7);
                ahead_prod_y = (uint32_t)((int32_t)(int16_t)neg_speed_y * (int16_t)object_matrix_7_2);
                object_matrix_8_2 = fx_ldw(FX_OBJECT_MATRIX_8);
                ahead_prod_z = (uint32_t)((int32_t)(int16_t)neg_speed_z * (int16_t)object_matrix_8_2);
                ahead_speed = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(ahead_prod_z >> 16) << 1 | (int)(ahead_prod_z >> 15 & 1)) + (uint16_t)((uint16_t)(ahead_prod_x >> 16) << 1 | (int)(ahead_prod_x >> 15 & 1))) + (uint16_t)((uint16_t)(ahead_prod_y >> 16) << 1 | (int)(ahead_prod_y >> 15 & 1)));
                fx_stw(FX_WHEEL_AHEAD, ahead_speed);
                wheel_yaw = fx_ldw((uint16_t)(wheel + 0x0026));
                fx_stw((uint16_t)(wheel + 0x0026), (uint16_t)(wheel_yaw + ahead_speed));
                wheel_side = fx_ldw(FX_WHEEL_SIDE);
                wheel_ahead = fx_ldw(FX_WHEEL_AHEAD);
                view_matrix_8_2 = fx_ldw(FX_VIEW_MATRIX_8);
                view_matrix_2_2 = fx_ldw(FX_VIEW_MATRIX_2);
                grip_demand = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(wheel_ahead + wheel_ahead) * (int16_t)view_matrix_2_2) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)wheel_side * (int16_t)view_matrix_8_2) >> 16));
                grip_demand_x2 = (uint16_t)(grip_demand + grip_demand);
                grip_demand_x4 = (uint16_t)(grip_demand_x2 + grip_demand_x2);
                fxpart_50_2 = fx_ldw((uint16_t)(wheel + FXPART_50));
                fxpart_3c_2 = fx_ldw((uint16_t)(car + FXPART_3C));
                if (fxpart_3c_2 != 0) {
                } else {
                    param_grip_free = fx_ldw(FX_PARAM_GRIP_FREE);
                    fxpart_50_2 = (uint16_t)(fxpart_50_2 + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_free) >> 16));
                }
                steer_bias_2 = fx_ldw((uint16_t)(car + FXCAR_STEER_BIAS));
                if (steer_bias_2 == 0) {
                    wheel_flags_7 = fx_ldw(FX_WHEEL_FLAGS);
                    if ((2 & wheel_flags_7) == 0) {
                    } else {
                        param_grip_slide = fx_ldw(FX_PARAM_GRIP_SLIDE);
                        fxpart_50_2 = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_slide) >> 16);
                    }
                } else {
                    car_yaw_rate = fx_ldw((uint16_t)(car + 0x0034));
                    if (!(car_yaw_rate & 0x8000)) {
                        if (!(steer_bias_2 & 0x8000)) {
                        } else {
                            wheel_flags_7 = fx_ldw(FX_WHEEL_FLAGS);
                            if ((2 & wheel_flags_7) == 0) {
                            } else {
                                param_grip_slide = fx_ldw(FX_PARAM_GRIP_SLIDE);
                                fxpart_50_2 = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_slide) >> 16);
                            }
                        }
                    } else {
                        if (!(steer_bias_2 & 0x8000)) {
                            wheel_flags_7 = fx_ldw(FX_WHEEL_FLAGS);
                            if ((2 & wheel_flags_7) == 0) {
                            } else {
                                param_grip_slide = fx_ldw(FX_PARAM_GRIP_SLIDE);
                                fxpart_50_2 = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_slide) >> 16);
                            }
                        } else {
                        }
                    }
                }
                wheel_flags_8 = fx_ldw(FX_WHEEL_FLAGS);
                sides = fx_ldw((uint16_t)(car + FXREC_SIDES));
                if ((0x000C & sides) == 0) {
                } else {
                    if ((2 & wheel_flags_8) == 0) {
                        if ((1 & wheel_flags_8) == 0) {
                        } else {
                            param_grip_ahead = fx_ldw(FX_PARAM_GRIP_AHEAD);
                            fxpart_50_2 = (uint16_t)(fxpart_50_2 + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_ahead) >> 16));
                        }
                    } else {
                        param_grip_side = fx_ldw(FX_PARAM_GRIP_SIDE);
                        fxpart_50_2 = (uint16_t)(fxpart_50_2 + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_side) >> 16));
                    }
                }
                if ((2 & wheel_flags_8) == 0) {
                } else {
                    fx_stw(FX_CONTACT_FORCE, 0x4650);
                }
                contact_force_3 = fx_ldw(FX_CONTACT_FORCE);
                grip_limit = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)contact_force_3) >> 16);
                grip_demand_copy = grip_demand_x4;
                grip_demand_abs = grip_demand_copy;
                if (!(grip_demand_copy & 0x8000)) {
                } else {
                    grip_demand_abs = (uint16_t)((uint16_t)~grip_demand_abs + 1);
                }
                if ((uint16_t)(grip_demand_abs - grip_limit) & 0x8000) {
                } else {
                    grip_demand_sign = grip_demand_x4;
                    if (!(grip_demand_sign & 0x8000)) {
                    } else {
                        grip_limit = (uint16_t)((uint16_t)~grip_limit + 1);
                    }
                    grip_demand_x4 = grip_limit;
                    grip_excess = (uint16_t)(grip_demand_sign - grip_limit);
                    grip_excess_abs = grip_excess;
                    if (!(grip_excess & 0x8000)) {
                    } else {
                        grip_excess_abs = (uint16_t)((uint16_t)~grip_excess_abs + 1);
                    }
                    param_slide_limit = fx_ldw(FX_PARAM_SLIDE_LIMIT);
                    if (!((uint16_t)(param_slide_limit - grip_excess_abs) & 0x8000)) {
                    } else {
                        car_slide_total_b = fx_ldw((uint16_t)(car + 0x0056));
                        fx_sbk((uint16_t)(car_slide_total_b + grip_excess_abs));
                        wheel_flags_9 = fx_ldw(FX_WHEEL_FLAGS);
                        fx_sbk((0x0020 | wheel_flags_9));
                    }
                }
                FxSumsPushLocal(grip_demand_x4, 0, 0, stack);
                car_drag_factor = fx_ldw((uint16_t)(car + 0x0048));
                if (car_drag_factor == 0) {
                } else {
                    wheel_ahead_2 = fx_ldw(FX_WHEEL_AHEAD);
                    drag_push_raw = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(wheel_ahead_2 + wheel_ahead_2) * (int16_t)car_drag_factor) >> 16);
                    drag_push_clamped = drag_push_raw;
                    if (drag_push_raw & 0x8000) {
                        if (!((uint16_t)(drag_push_clamped + 0x00C8) & 0x8000)) {
                        } else {
                            drag_push_clamped = 0xFF38;
                        }
                    } else {
                        if ((uint16_t)(drag_push_clamped - 0x00C8) & 0x8000) {
                        } else {
                            drag_push_clamped = 0x00C8;
                        }
                    }
                    FxSumsPushLocal(0, 0, drag_push_clamped, stack);
                }
                { FxSumsRotateLocalResult r_ = FxSumsRotateLocal(); old_world = r_.old_world; push_y = r_.y_row; };
                world_sum = fx_ldw(FX_WORLD_SUM);
                world_sum_y = fx_ldw(0x2342);
                world_sum_z = fx_ldw(0x2344);
                if (((world_sum | world_sum_y) | world_sum_z) != 0) {
                    fxpart_world = fx_ldw((uint16_t)(wheel + FXPART_WORLD));
                    vector_y = fx_ldw((uint16_t)(wheel + 0x0038));
                    steer_bias_3 = fx_ldw((uint16_t)(wheel + FXCAR_STEER_BIAS));
                    param_hit = fx_ldw(FX_PARAM_HIT);
                    last_low_2 = FxHitAddScaled(fxpart_world, vector_y, steer_bias_3, old_world, param_hit, stack);
                    wheel_flags_10 = fx_ldw(FX_WHEEL_FLAGS);
                    if ((0x0200 & wheel_flags_10) == 0) {
                    } else {
                    }
                    fxpart_3c_3 = fx_ldw((uint16_t)(wheel + FXPART_3C));
                    parts = fx_ldw((uint16_t)(wheel + FXCAR_PARTS));
                    flags40_3 = fx_ldw((uint16_t)(wheel + FXREC_FLAGS40));
                    push_point_y_offset = fx_ldw((uint16_t)(car + 0x005A));
                    push_point_z_offset = fx_ldw((uint16_t)(car + 0x005C));
                    fx_romb(4);
                    fx_table_bank = 0x04;
                    param_push = fx_ldw(FX_PARAM_PUSH);
                    fx_r14(0x9BC6);
                    push_table_index = (uint16_t)(param_push - 0x643A);
                    fx_r14(push_table_index);
                    push_routine_lo = fx_getb();
                    rom_at = (uint16_t)(push_table_index + 1);
                    fx_r14(rom_at);
                    push_routine_addr = (uint16_t)((push_routine_lo & 0xFF) | fx_getb() << 8);
                    { FxPushAtVariantResult r_ = FxPushAtVariant(fxpart_3c_3, (uint16_t)(parts + push_point_y_offset), (uint16_t)(flags40_3 + push_point_z_offset), last_low_2, push_y, model_record, stack, push_routine_addr); push_y = r_.push_y_out; model_record = r_.model_record_out; };
                } else {
                }
                param_bounce = fx_ldw(FX_PARAM_BOUNCE);
                neg_bounce_limit = (uint16_t)((uint16_t)~param_bounce + 1);
                contact_sum = fx_ldw(FX_CONTACT_SUM);
                contact_sum_x_eighth = fx_div2(fx_div2(fx_div2(contact_sum)));
                contact_speed_x = contact_sum_x_eighth;
                if (contact_sum_x_eighth & 0x8000) {
                    if (!((uint16_t)(contact_speed_x - neg_bounce_limit) & 0x8000)) {
                    } else {
                        contact_speed_x = neg_bounce_limit;
                    }
                } else {
                    if ((uint16_t)(contact_speed_x - param_bounce) & 0x8000) {
                    } else {
                        contact_speed_x = param_bounce;
                    }
                }
                fx_stw((uint16_t)(wheel + FXREC_SPEED), contact_speed_x);
                contact_sum_z = fx_ldw(0x238C);
                contact_sum_z_eighth = fx_div2(fx_div2(fx_div2(contact_sum_z)));
                contact_speed_z = contact_sum_z_eighth;
                if (contact_sum_z_eighth & 0x8000) {
                    if (!((uint16_t)(contact_speed_z - neg_bounce_limit) & 0x8000)) {
                    } else {
                        contact_speed_z = neg_bounce_limit;
                    }
                } else {
                    if ((uint16_t)(contact_speed_z - param_bounce) & 0x8000) {
                    } else {
                        contact_speed_z = param_bounce;
                    }
                }
                fx_stw((uint16_t)(wheel + 0x004E), contact_speed_z);
                contact_sum_y = fx_ldw(0x238A);
                contact_sum_y_eighth = fx_div2(fx_div2(fx_div2(contact_sum_y)));
                entry_product_lo = contact_sum_y_eighth;
                contact_speed_y = contact_sum_y_eighth;
                if (contact_sum_y_eighth & 0x8000) {
                    if (!((uint16_t)(contact_speed_y - neg_bounce_limit) & 0x8000)) {
                    } else {
                        contact_speed_y = neg_bounce_limit;
                    }
                } else {
                    if ((uint16_t)(contact_speed_y - param_bounce) & 0x8000) {
                    } else {
                        contact_speed_y = param_bounce;
                    }
                }
                fx_stw((uint16_t)(wheel + 0x004C), contact_speed_y);
            } else {
                fx_stw((uint16_t)(wheel + FXREC_SPEED), 0);
                fx_stw((uint16_t)(wheel + 0x004C), 0);
                fx_stw((uint16_t)(wheel + 0x004E), 0);
                car_counter_44 = fx_ldw((uint16_t)(car + 0x0044));
                fx_sbk((uint16_t)(car_counter_44 + 1));
            }
        } else {
        }
        fxpart_46_3 = fx_ldw((uint16_t)(wheel + FXPART_FLAGS));
        if ((2 & fxpart_46_3) == 0) {
        } else {
            pitch_5 = fx_ldw((uint16_t)(wheel + FXREC_HEADING));
            steer_bias_4 = fx_ldw((uint16_t)(car + FXCAR_STEER_BIAS));
            fx_stw((uint16_t)(wheel + FXREC_HEADING), (uint16_t)(pitch_5 + steer_bias_4));
        }
    }
    wheel_flags_11 = fx_ldw(FX_WHEEL_FLAGS);
    fx_stw((uint16_t)(wheel + FXPART_FLAGS), wheel_flags_11);
    fx_unspill(stack);
    return (FxWheelUpdateResult){entry_product_lo, push_y, model_record, car, wheel, rom_at, 0};
}


/*
 * @routine 04:AA02 FxWheelUpdateNoSlot
 * WHAT IT DOES: FxWheelUpdate's way on when the contact flags from
 * FxContactsResolve (R1) are set but their slot bits (bits 0-4) are
 * clear: straight to the surface kind from the car's +$8A (04:AA98)
 * and the rest of the update. No play or unit start found so far makes
 * those flags (every slot word that is applied carries its slot
 * number), so a unit start runs it so the C is judged against the
 * original.
 * HOW: IWT R15 to 04:AA98 (NOP in the delay slot); then as
 * FxWheelUpdate from there.
 * Entry: R1 = the contact flags; R8 = the car, R9 = the wheel; R10 =
 * stack, the return address the word below it.
 * Exit: as FxWheelUpdate's.
 */
FxWheelUpdateNoSlotResult FxWheelUpdateNoSlot(uint16_t entry_product_lo, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t wheel, uint16_t *stack, uint16_t rom_at)
{
    uint16_t y;
    uint16_t parts;
    uint16_t pitch;
    uint16_t sides;
    uint16_t y_row;
    uint16_t amount;
    uint16_t half_x;
    uint16_t flags40;
    uint16_t pitch_2;
    uint16_t pitch_3;
    uint16_t last_low;
    uint16_t vector_y;
    uint16_t flags40_2;
    uint16_t fxpart_3c;
    uint16_t fxpart_46;
    uint16_t fxpart_50;
    uint16_t old_world;
    uint16_t param_hit;
    uint16_t push_flag;
    uint16_t wheel_yaw;
    uint16_t world_sum;
    uint16_t grip_limit;
    uint16_t param_push;
    uint16_t steer_bias;
    uint16_t wheel_side;
    uint16_t ahead_speed;
    uint16_t car_speed_z;
    uint16_t contact_sum;
    uint16_t fxpart_3c_2;
    uint16_t fxpart_3c_3;
    uint16_t fxpart_46_2;
    uint16_t fxpart_50_2;
    uint16_t grip_demand;
    uint16_t grip_excess;
    uint16_t neg_speed_x;
    uint16_t neg_speed_y;
    uint16_t neg_speed_z;
    uint32_t side_prod_x;
    uint32_t side_prod_y;
    uint32_t side_prod_z;
    uint16_t wheel_ahead;
    uint16_t wheel_flags;
    uint16_t world_sum_y;
    uint16_t world_sum_z;
    uint32_t ahead_prod_x;
    uint32_t ahead_prod_y;
    uint32_t ahead_prod_z;
    uint16_t car_offset_y;
    uint16_t car_offset_z;
    uint16_t car_yaw_rate;
    uint16_t fxpart_world;
    uint16_t param_bounce;
    uint16_t steer_bias_2;
    uint16_t steer_bias_3;
    uint16_t contact_depth;
    uint16_t contact_force;
    uint16_t contact_sum_y;
    uint16_t contact_sum_z;
    uint16_t drag_push_raw;
    uint16_t view_matrix_2;
    uint16_t view_matrix_8;
    uint16_t wheel_ahead_2;
    uint16_t wheel_flags_2;
    uint16_t wheel_flags_3;
    uint16_t wheel_flags_4;
    uint16_t wheel_flags_5;
    uint16_t wheel_flags_6;
    uint16_t wheel_flags_7;
    uint16_t wheel_speed_x;
    uint16_t wheel_speed_y;
    uint16_t wheel_speed_z;
    uint16_t car_counter_44;
    uint16_t grip_demand_x2;
    uint16_t grip_demand_x4;
    uint16_t part_load_copy;
    uint16_t wheel_field_22;
    uint16_t wheel_load_cap;
    uint16_t car_drag_factor;
    uint16_t contact_force_2;
    uint16_t contact_force_3;
    uint16_t contact_speed_x;
    uint16_t contact_speed_y;
    uint16_t contact_speed_z;
    uint16_t grip_demand_abs;
    uint16_t grip_excess_abs;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t param_grip_free;
    uint16_t param_grip_side;
    uint16_t push_routine_lo;
    uint16_t view_matrix_2_2;
    uint16_t view_matrix_8_2;
    uint16_t ahead_factor_raw;
    uint16_t contact_force_up;
    uint16_t cross_factor_raw;
    uint16_t grip_demand_copy;
    uint16_t grip_demand_sign;
    uint16_t neg_bounce_limit;
    uint16_t param_grip_ahead;
    uint16_t param_grip_slide;
    uint16_t pitch_diff_index;
    uint16_t push_table_index;
    uint16_t car_ground_kind_b;
    uint16_t car_slide_total_a;
    uint16_t car_slide_total_b;
    uint16_t curve_ahead_entry;
    uint16_t curve_cross_entry;
    uint16_t drag_push_clamped;
    uint16_t object_matrix_6_2;
    uint16_t object_matrix_7_2;
    uint16_t object_matrix_8_2;
    uint16_t param_slide_limit;
    uint16_t push_routine_addr;
    uint16_t world_sum_y_check;
    uint16_t contact_force_side;
    uint16_t wheel_load_doubled;
    uint16_t contact_sum_x_eighth;
    uint16_t contact_sum_y_eighth;
    uint16_t contact_sum_z_eighth;
    uint16_t ground_contact_flags;
    uint16_t wheel_contact_flags_b;
    uint16_t wheel_contact_flags_c;
    uint16_t wheel_contact_flags_d;
    uint16_t wheel_contact_flags_e;

    car_ground_kind_b = fx_ldw((uint16_t)(car + 0x008A));
    ground_contact_flags = 0x0040;
    if ((4 & car_ground_kind_b) != 0) {
    } else {
        ground_contact_flags = 0x00C0;
        if ((0x0020 & car_ground_kind_b) != 0) {
        } else {
            ground_contact_flags = 0x1040;
            if ((0x0040 & car_ground_kind_b) != 0) {
            } else {
                ground_contact_flags = 0x0840;
                if ((2 & car_ground_kind_b) != 0) {
                } else {
                    ground_contact_flags = 0x0060;
                }
            }
        }
    }
    wheel_contact_flags_b = fx_ldw((uint16_t)(wheel + 0x002C));
    fx_sbk((wheel_contact_flags_b | ground_contact_flags));
    y = fx_ldw((uint16_t)(wheel + FXREC_Y));
    amount = y;
    if (y & 0x8000) {
    } else {
        wheel_contact_flags_c = fx_ldw((uint16_t)(wheel + 0x002C));
        if ((wheel_contact_flags_c & 0x0020) == 0) {
        } else {
            half_x = fx_ldw((uint16_t)(wheel + FXREC_HALF_X));
            wheel_field_22 = fx_ldw((uint16_t)(wheel + 0x0022));
            amount = (uint16_t)((uint16_t)(amount + (fx_div2(fx_div2(fx_div2((uint16_t)(half_x + wheel_field_22)))) & 0x000F)) + 0x000A);
        }
        push_flag = FxPushSet(amount);
        if (push_flag == 0) {
        } else {
            { FxContactResolveResult r_ = FxContactResolve(0x23A4, entry_product_lo, push_y, model_record, wheel, stack, rom_at); model_record = r_.model_record_out; };
        }
    }
    contact_force_up = fx_ldw(FX_CONTACT_FORCE_UP);
    fx_stw(FX_CONTACT_FORCE, contact_force_up);
    wheel_contact_flags_d = fx_ldw((uint16_t)(wheel + 0x002C));
    if ((0x0020 & wheel_contact_flags_d) == 0) {
    } else {
        contact_force_side = fx_ldw(FX_CONTACT_FORCE_SIDE);
        fx_stw(FX_CONTACT_FORCE, contact_force_side);
        fxpart_46 = fx_ldw((uint16_t)(car + FXPART_FLAGS));
        fx_sbk((uint16_t)(fxpart_46 + 1));
        world_sum_y_check = fx_ldw(0x2342);
        if (world_sum_y_check != 0) {
        } else {
            wheel_contact_flags_e = fx_ldw((uint16_t)(wheel + 0x002C));
            fx_sbk((uint16_t)(wheel_contact_flags_e & ~0x0020));
        }
    }
    wheel_flags = fx_ldw(FX_WHEEL_FLAGS);
    contact_depth = fx_ldw(FX_CONTACT_DEPTH);
    if (contact_depth != 0x7D00) {
        flags40 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
        fx_sbk((uint16_t)(flags40 & ~0x0100));
        fx_stw(FX_WHEEL_FLAGS, (0x0010 | wheel_flags));
        wheel_flags_2 = fx_ldw(FX_WHEEL_FLAGS);
        if ((8 & wheel_flags_2) == 0) {
        } else {
            fx_stw(FX_WHEEL_LOAD, 0);
            fxpart_50 = fx_ldw((uint16_t)(wheel + FXPART_50));
            contact_force = fx_ldw(FX_CONTACT_FORCE);
            wheel_load_cap = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50 + fxpart_50) * (int16_t)contact_force) >> 16);
            fxpart_3c = fx_ldw((uint16_t)(car + FXPART_3C));
            car_speed_z = fx_ldw((uint16_t)(car + 0x004E));
            if (!((uint16_t)(car_speed_z - 2) & 0x8000)) {
                wheel_load_doubled = (uint16_t)(fxpart_3c + fxpart_3c);
                fx_stw(FX_WHEEL_LOAD, wheel_load_doubled);
                object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                FxSumsPushWorld((uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_6 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_7 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_8 * (int16_t)wheel_load_doubled) >> 16), stack);
            } else {
                part_load_copy = fxpart_3c;
                fxpart_3c = part_load_copy;
                if (part_load_copy == 0) {
                } else {
                    if (part_load_copy & 0x8000) {
                    } else {
                        if (!((uint16_t)(wheel_load_cap - fxpart_3c) & 0x8000)) {
                        } else {
                            fxpart_3c = wheel_load_cap;
                            fx_stw(FX_WHEEL_FLAGS, (0x0020 | wheel_flags_2));
                            car_slide_total_a = fx_ldw((uint16_t)(car + 0x0056));
                            fx_sbk((uint16_t)(car_slide_total_a + 0x1F40));
                            contact_force_2 = fx_ldw(FX_CONTACT_FORCE);
                            fx_sbk(fx_div2(contact_force_2));
                        }
                    }
                    wheel_load_doubled = (uint16_t)(fxpart_3c + fxpart_3c);
                    fx_stw(FX_WHEEL_LOAD, wheel_load_doubled);
                    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                    FxSumsPushWorld((uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_6 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_7 * (int16_t)wheel_load_doubled) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)object_matrix_8 * (int16_t)wheel_load_doubled) >> 16), stack);
                }
            }
        }
        fx_romb(0);
        pitch = fx_ldw((uint16_t)(wheel + FXREC_HEADING));
        pitch_2 = fx_ldw((uint16_t)(car + FXREC_HEADING));
        pitch_diff_index = (uint16_t)((uint16_t)(pitch - pitch_2) >> 8);
        fx_r14(0x96FE);
        fx_r14((uint16_t)((uint16_t)(pitch_diff_index & 0xFF) - 0x6902));
        curve_cross_entry = fx_getb();
        cross_factor_raw = fx_div2(fx_div2((uint16_t)(curve_cross_entry << 8 | curve_cross_entry >> 8)));
        view_matrix_2 = cross_factor_raw;
        if (cross_factor_raw & 0x8000) {
            if (!((uint16_t)(view_matrix_2 + 0x0FA0) & 0x8000)) {
            } else {
                view_matrix_2 = 0xF060;
            }
        } else {
            if ((uint16_t)(view_matrix_2 - 0x0FA0) & 0x8000) {
            } else {
                view_matrix_2 = 0x0FA0;
            }
        }
        fx_stw(FX_VIEW_MATRIX_2, view_matrix_2);
        fx_r14(0x973E);
        fx_r14((uint16_t)((uint16_t)(pitch_diff_index & 0xFF) - 0x68C2));
        curve_ahead_entry = fx_getb();
        ahead_factor_raw = (uint16_t)(curve_ahead_entry << 8 | curve_ahead_entry >> 8);
        view_matrix_8 = ahead_factor_raw;
        if (!(ahead_factor_raw & 0x8000)) {
        } else {
            view_matrix_8 = (uint16_t)((uint16_t)~view_matrix_8 + 1);
        }
        fx_stw(FX_VIEW_MATRIX_8, view_matrix_8);
        wheel_speed_x = fx_ldw((uint16_t)(wheel + 0x002E));
        neg_speed_x = (uint16_t)((uint16_t)~wheel_speed_x + 1);
        wheel_speed_y = fx_ldw((uint16_t)(wheel + 0x0030));
        neg_speed_y = (uint16_t)((uint16_t)~wheel_speed_y + 1);
        wheel_speed_z = fx_ldw((uint16_t)(wheel + 0x0032));
        neg_speed_z = (uint16_t)((uint16_t)~wheel_speed_z + 1);
        object_matrix_0 = fx_ldw(0x00E4);
        side_prod_x = (uint32_t)((int32_t)(int16_t)neg_speed_x * (int16_t)object_matrix_0);
        object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
        side_prod_y = (uint32_t)((int32_t)(int16_t)neg_speed_y * (int16_t)object_matrix_1);
        object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
        side_prod_z = (uint32_t)((int32_t)(int16_t)neg_speed_z * (int16_t)object_matrix_2);
        fx_stw(FX_WHEEL_SIDE, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(side_prod_z >> 16) << 1 | (int)(side_prod_z >> 15 & 1)) + (uint16_t)((uint16_t)(side_prod_x >> 16) << 1 | (int)(side_prod_x >> 15 & 1))) + (uint16_t)((uint16_t)(side_prod_y >> 16) << 1 | (int)(side_prod_y >> 15 & 1))));
        object_matrix_6_2 = fx_ldw(FX_OBJECT_MATRIX_6);
        ahead_prod_x = (uint32_t)((int32_t)(int16_t)neg_speed_x * (int16_t)object_matrix_6_2);
        object_matrix_7_2 = fx_ldw(FX_OBJECT_MATRIX_7);
        ahead_prod_y = (uint32_t)((int32_t)(int16_t)neg_speed_y * (int16_t)object_matrix_7_2);
        object_matrix_8_2 = fx_ldw(FX_OBJECT_MATRIX_8);
        ahead_prod_z = (uint32_t)((int32_t)(int16_t)neg_speed_z * (int16_t)object_matrix_8_2);
        ahead_speed = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(ahead_prod_z >> 16) << 1 | (int)(ahead_prod_z >> 15 & 1)) + (uint16_t)((uint16_t)(ahead_prod_x >> 16) << 1 | (int)(ahead_prod_x >> 15 & 1))) + (uint16_t)((uint16_t)(ahead_prod_y >> 16) << 1 | (int)(ahead_prod_y >> 15 & 1)));
        fx_stw(FX_WHEEL_AHEAD, ahead_speed);
        wheel_yaw = fx_ldw((uint16_t)(wheel + 0x0026));
        fx_stw((uint16_t)(wheel + 0x0026), (uint16_t)(wheel_yaw + ahead_speed));
        wheel_side = fx_ldw(FX_WHEEL_SIDE);
        wheel_ahead = fx_ldw(FX_WHEEL_AHEAD);
        view_matrix_8_2 = fx_ldw(FX_VIEW_MATRIX_8);
        view_matrix_2_2 = fx_ldw(FX_VIEW_MATRIX_2);
        grip_demand = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(wheel_ahead + wheel_ahead) * (int16_t)view_matrix_2_2) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)wheel_side * (int16_t)view_matrix_8_2) >> 16));
        grip_demand_x2 = (uint16_t)(grip_demand + grip_demand);
        grip_demand_x4 = (uint16_t)(grip_demand_x2 + grip_demand_x2);
        fxpart_50_2 = fx_ldw((uint16_t)(wheel + FXPART_50));
        fxpart_3c_2 = fx_ldw((uint16_t)(car + FXPART_3C));
        if (fxpart_3c_2 != 0) {
        } else {
            param_grip_free = fx_ldw(FX_PARAM_GRIP_FREE);
            fxpart_50_2 = (uint16_t)(fxpart_50_2 + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_free) >> 16));
        }
        steer_bias = fx_ldw((uint16_t)(car + FXCAR_STEER_BIAS));
        if (steer_bias == 0) {
            wheel_flags_3 = fx_ldw(FX_WHEEL_FLAGS);
            if ((2 & wheel_flags_3) == 0) {
            } else {
                param_grip_slide = fx_ldw(FX_PARAM_GRIP_SLIDE);
                fxpart_50_2 = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_slide) >> 16);
            }
        } else {
            car_yaw_rate = fx_ldw((uint16_t)(car + 0x0034));
            if (!(car_yaw_rate & 0x8000)) {
                if (!(steer_bias & 0x8000)) {
                } else {
                    wheel_flags_3 = fx_ldw(FX_WHEEL_FLAGS);
                    if ((2 & wheel_flags_3) == 0) {
                    } else {
                        param_grip_slide = fx_ldw(FX_PARAM_GRIP_SLIDE);
                        fxpart_50_2 = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_slide) >> 16);
                    }
                }
            } else {
                if (!(steer_bias & 0x8000)) {
                    wheel_flags_3 = fx_ldw(FX_WHEEL_FLAGS);
                    if ((2 & wheel_flags_3) == 0) {
                    } else {
                        param_grip_slide = fx_ldw(FX_PARAM_GRIP_SLIDE);
                        fxpart_50_2 = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_slide) >> 16);
                    }
                } else {
                }
            }
        }
        wheel_flags_4 = fx_ldw(FX_WHEEL_FLAGS);
        sides = fx_ldw((uint16_t)(car + FXREC_SIDES));
        if ((0x000C & sides) == 0) {
        } else {
            if ((2 & wheel_flags_4) == 0) {
                if ((1 & wheel_flags_4) == 0) {
                } else {
                    param_grip_ahead = fx_ldw(FX_PARAM_GRIP_AHEAD);
                    fxpart_50_2 = (uint16_t)(fxpart_50_2 + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_ahead) >> 16));
                }
            } else {
                param_grip_side = fx_ldw(FX_PARAM_GRIP_SIDE);
                fxpart_50_2 = (uint16_t)(fxpart_50_2 + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)param_grip_side) >> 16));
            }
        }
        if ((2 & wheel_flags_4) == 0) {
        } else {
            fx_stw(FX_CONTACT_FORCE, 0x4650);
        }
        contact_force_3 = fx_ldw(FX_CONTACT_FORCE);
        grip_limit = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(fxpart_50_2 + fxpart_50_2) * (int16_t)contact_force_3) >> 16);
        grip_demand_copy = grip_demand_x4;
        grip_demand_abs = grip_demand_copy;
        if (!(grip_demand_copy & 0x8000)) {
        } else {
            grip_demand_abs = (uint16_t)((uint16_t)~grip_demand_abs + 1);
        }
        if ((uint16_t)(grip_demand_abs - grip_limit) & 0x8000) {
        } else {
            grip_demand_sign = grip_demand_x4;
            if (!(grip_demand_sign & 0x8000)) {
            } else {
                grip_limit = (uint16_t)((uint16_t)~grip_limit + 1);
            }
            grip_demand_x4 = grip_limit;
            grip_excess = (uint16_t)(grip_demand_sign - grip_limit);
            grip_excess_abs = grip_excess;
            if (!(grip_excess & 0x8000)) {
            } else {
                grip_excess_abs = (uint16_t)((uint16_t)~grip_excess_abs + 1);
            }
            param_slide_limit = fx_ldw(FX_PARAM_SLIDE_LIMIT);
            if (!((uint16_t)(param_slide_limit - grip_excess_abs) & 0x8000)) {
            } else {
                car_slide_total_b = fx_ldw((uint16_t)(car + 0x0056));
                fx_sbk((uint16_t)(car_slide_total_b + grip_excess_abs));
                wheel_flags_5 = fx_ldw(FX_WHEEL_FLAGS);
                fx_sbk((0x0020 | wheel_flags_5));
            }
        }
        FxSumsPushLocal(grip_demand_x4, 0, 0, stack);
        car_drag_factor = fx_ldw((uint16_t)(car + 0x0048));
        if (car_drag_factor == 0) {
        } else {
            wheel_ahead_2 = fx_ldw(FX_WHEEL_AHEAD);
            drag_push_raw = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(wheel_ahead_2 + wheel_ahead_2) * (int16_t)car_drag_factor) >> 16);
            drag_push_clamped = drag_push_raw;
            if (drag_push_raw & 0x8000) {
                if (!((uint16_t)(drag_push_clamped + 0x00C8) & 0x8000)) {
                } else {
                    drag_push_clamped = 0xFF38;
                }
            } else {
                if ((uint16_t)(drag_push_clamped - 0x00C8) & 0x8000) {
                } else {
                    drag_push_clamped = 0x00C8;
                }
            }
            FxSumsPushLocal(0, 0, drag_push_clamped, stack);
        }
        { FxSumsRotateLocalResult r_ = FxSumsRotateLocal(); old_world = r_.old_world; y_row = r_.y_row; };
        world_sum = fx_ldw(FX_WORLD_SUM);
        world_sum_y = fx_ldw(0x2342);
        world_sum_z = fx_ldw(0x2344);
        if (((world_sum | world_sum_y) | world_sum_z) != 0) {
            fxpart_world = fx_ldw((uint16_t)(wheel + FXPART_WORLD));
            vector_y = fx_ldw((uint16_t)(wheel + 0x0038));
            steer_bias_2 = fx_ldw((uint16_t)(wheel + FXCAR_STEER_BIAS));
            param_hit = fx_ldw(FX_PARAM_HIT);
            last_low = FxHitAddScaled(fxpart_world, vector_y, steer_bias_2, old_world, param_hit, stack);
            wheel_flags_6 = fx_ldw(FX_WHEEL_FLAGS);
            if ((0x0200 & wheel_flags_6) == 0) {
            } else {
            }
            fxpart_3c_3 = fx_ldw((uint16_t)(wheel + FXPART_3C));
            parts = fx_ldw((uint16_t)(wheel + FXCAR_PARTS));
            flags40_2 = fx_ldw((uint16_t)(wheel + FXREC_FLAGS40));
            car_offset_y = fx_ldw((uint16_t)(car + 0x005A));
            car_offset_z = fx_ldw((uint16_t)(car + 0x005C));
            fx_romb(4);
            param_push = fx_ldw(FX_PARAM_PUSH);
            fx_r14(0x9BC6);
            push_table_index = (uint16_t)(param_push - 0x643A);
            fx_r14(push_table_index);
            push_routine_lo = fx_getb();
            fx_r14((uint16_t)(push_table_index + 1));
            push_routine_addr = (uint16_t)((push_routine_lo & 0xFF) | fx_getb() << 8);
            FxPushAtVariant(fxpart_3c_3, (uint16_t)(parts + car_offset_y), (uint16_t)(flags40_2 + car_offset_z), last_low, y_row, model_record, stack, push_routine_addr);
        } else {
        }
        param_bounce = fx_ldw(FX_PARAM_BOUNCE);
        neg_bounce_limit = (uint16_t)((uint16_t)~param_bounce + 1);
        contact_sum = fx_ldw(FX_CONTACT_SUM);
        contact_sum_x_eighth = fx_div2(fx_div2(fx_div2(contact_sum)));
        contact_speed_x = contact_sum_x_eighth;
        if (contact_sum_x_eighth & 0x8000) {
            if (!((uint16_t)(contact_speed_x - neg_bounce_limit) & 0x8000)) {
            } else {
                contact_speed_x = neg_bounce_limit;
            }
        } else {
            if ((uint16_t)(contact_speed_x - param_bounce) & 0x8000) {
            } else {
                contact_speed_x = param_bounce;
            }
        }
        fx_stw((uint16_t)(wheel + FXREC_SPEED), contact_speed_x);
        contact_sum_z = fx_ldw(0x238C);
        contact_sum_z_eighth = fx_div2(fx_div2(fx_div2(contact_sum_z)));
        contact_speed_z = contact_sum_z_eighth;
        if (contact_sum_z_eighth & 0x8000) {
            if (!((uint16_t)(contact_speed_z - neg_bounce_limit) & 0x8000)) {
            } else {
                contact_speed_z = neg_bounce_limit;
            }
        } else {
            if ((uint16_t)(contact_speed_z - param_bounce) & 0x8000) {
            } else {
                contact_speed_z = param_bounce;
            }
        }
        fx_stw((uint16_t)(wheel + 0x004E), contact_speed_z);
        contact_sum_y = fx_ldw(0x238A);
        contact_sum_y_eighth = fx_div2(fx_div2(fx_div2(contact_sum_y)));
        contact_speed_y = contact_sum_y_eighth;
        if (contact_sum_y_eighth & 0x8000) {
            if (!((uint16_t)(contact_speed_y - neg_bounce_limit) & 0x8000)) {
            } else {
                contact_speed_y = neg_bounce_limit;
            }
        } else {
            if ((uint16_t)(contact_speed_y - param_bounce) & 0x8000) {
            } else {
                contact_speed_y = param_bounce;
            }
        }
        fx_stw((uint16_t)(wheel + 0x004C), contact_speed_y);
    } else {
        fx_stw((uint16_t)(wheel + FXREC_SPEED), 0);
        fx_stw((uint16_t)(wheel + 0x004C), 0);
        fx_stw((uint16_t)(wheel + 0x004E), 0);
        car_counter_44 = fx_ldw((uint16_t)(car + 0x0044));
        fx_sbk((uint16_t)(car_counter_44 + 1));
    }
    fxpart_46_2 = fx_ldw((uint16_t)(wheel + FXPART_FLAGS));
    if ((2 & fxpart_46_2) == 0) {
    } else {
        pitch_3 = fx_ldw((uint16_t)(wheel + FXREC_HEADING));
        steer_bias_3 = fx_ldw((uint16_t)(car + FXCAR_STEER_BIAS));
        fx_stw((uint16_t)(wheel + FXREC_HEADING), (uint16_t)(pitch_3 + steer_bias_3));
    }
    wheel_flags_7 = fx_ldw(FX_WHEEL_FLAGS);
    fx_stw((uint16_t)(wheel + FXPART_FLAGS), wheel_flags_7);
    fx_unspill(stack);
    return (FxWheelUpdateNoSlotResult){1};
}


/*
 * @routine 04:AEFF FxContactsResolve
 * WHAT IT DOES: Resolves the contacts in the twenty slots at $23AE:
 * each slot in use goes through FxContactResolve and its upward force
 * ($23A0) is kept in $2386. R0 = $2480, the contact flags gathered.
 * HOW: Pushes R11; ROMB 0; $2480 = 0 and $239C = the wheel's +$2C
 * first; CACHE; R13 counts the slots to $14; returns by pulling R15.
 * Entry: R8 = the car; R10 = stack; R11 = return address.
 * Exit: R0 = $2480; the wheel's +$2C = $239C.
 */
FxContactsResolveResult FxContactsResolve(uint16_t push_x, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t contact_record, uint16_t *stack, uint16_t rom_at)
{
    uint16_t hit_depth;
    uint16_t slot_word;
    uint16_t push_flags;
    uint16_t hit_depth_2;
    uint16_t slot_counter;
    uint16_t contact_force_up;
    uint16_t contact_slot_addr;

    fx_spill(stack);
    fx_romb(0);
    fx_table_bank = 0x00;
    fx_stw(FX_PUSH_FLAGS, 0);
    hit_depth = fx_ldw((uint16_t)(car + 0x002C));
    fx_stw(FX_HIT_DEPTH, hit_depth);
    contact_slot_addr = 0x23AE;
    slot_counter = 0;
    for (;;) {
        slot_counter = (uint16_t)(slot_counter + 1);
        if ((uint16_t)((uint16_t)(0 - slot_counter) + 0x0014) != 0) {
            slot_word = fx_ldw(contact_slot_addr);
            if (slot_word == 0) {
                contact_slot_addr = (uint16_t)(contact_slot_addr + 0x000A);
                continue;
            } else {
                { FxContactResolveResult r_ = FxContactResolve(contact_slot_addr, push_x, push_y, model_record, contact_record, stack, rom_at); contact_slot_addr = r_.next_slot_ptr; push_x = r_.push_x_out; push_y = r_.push_y_out; model_record = r_.model_record_out; rom_at = r_.rom_at_out; };
                contact_force_up = fx_ldw(FX_CONTACT_FORCE_UP);
                fx_stw(FX_CONTACT_FORCE, contact_force_up);
                continue;
            }
        } else {
            break;
        }
    }
    hit_depth_2 = fx_ldw(FX_HIT_DEPTH);
    fx_stw((uint16_t)(car + 0x002C), hit_depth_2);
    push_flags = fx_ldw(FX_PUSH_FLAGS);
    fx_unspill(stack);
    return (FxContactsResolveResult){push_flags, push_x, push_y, model_record, rom_at};
}


/*
 * @routine 04:AF54 FxContactResolve
 * WHAT IT DOES: Resolves one contact slot (R3): marks its flags in
 * $2480, notes the shallowest depth in $237E, and from the depth and
 * the face's normal works the force pushing the wheel out ($239E
 * across, $23A0 up). The code after it to 04:B9AB holds the car
 * handler (FxCarHandler, 04:B1A1) and its helpers. The bytes
 * 04:AF90-AF92 never run in the game (FxContactResolveNegate).
 * HOW: Pushes R11 for slots it resolves; R3 steps through the slot;
 * returns by pulling R15 (or JMP R11 for an empty or skipped slot,
 * 04:B185-B19A).
 * Entry: R3 = the slot; R8 = the car; R10 = stack; R11 = return
 * address.
 * Exit: R3 past the slot; the rest as the code leaves them.
 */
FxContactResolveResult FxContactResolve(uint16_t slot_ptr, uint16_t push_x, uint16_t push_y, uint16_t model_record, uint16_t contact_record, uint16_t *stack, uint16_t rom_at)
{
    int run_lAFD2 = 0;
    uint16_t x;
    uint16_t z;
    uint16_t new_z;
    uint16_t sizes;
    uint16_t r_t_29;
    uint16_t r_t_30;
    uint16_t new_x_x;
    uint16_t sizes_2;
    uint16_t sizes_3;
    uint16_t sizes_4;
    uint16_t depth_x2;
    uint16_t depth_x4;
    uint16_t depth_x8;
    uint16_t depth_x16;
    uint16_t fxpart_3c;
    uint16_t hit_depth;
    uint16_t push_turn;
    uint16_t lift_force;
    uint16_t push_flags;
    uint16_t push_force;
    uint16_t slot_depth;
    uint16_t slot_flags;
    uint16_t clamped_dot;
    uint16_t contact_sum;
    uint16_t hit_depth_2;
    uint16_t hit_depth_3;
    uint16_t turned_x_x2;
    uint16_t turned_z_x2;
    uint16_t wheel_flags;
    uint16_t push_force_y;
    uint16_t push_x_out_2;
    uint16_t record_dir_x;
    uint16_t record_dir_y;
    uint16_t record_dir_z;
    uint16_t record_scale;
    uint16_t slot_end_ptr;
    uint16_t contact_depth;
    uint16_t contact_sum_y;
    uint16_t contact_sum_z;
    uint16_t slot_normal_y;
    uint16_t slot_normal_z;
    uint16_t wheel_contact;
    uint16_t abs_lift_force;
    uint16_t clamped_dot_x2;
    uint16_t clamped_dot_x4;
    uint16_t slot_depth_ptr;
    uint16_t normal_y_reload;
    uint16_t normal_z_reload;
    uint16_t push_force_copy;
    uint32_t scaled_dot_prod;
    uint16_t normal_y_reload_2;
    uint16_t normal_y_reload_3;
    uint16_t normal_z_reload_2;
    uint16_t normal_z_reload_3;
    uint16_t normal_z_reload_4;
    uint16_t slot_normal_x_ptr;
    uint16_t slot_normal_y_ptr;
    uint16_t slot_normal_z_ptr;
    uint16_t dot_scaled_swapped;
    uint16_t normal_x_store_ptr;
    uint16_t normal_y_store_ptr;
    uint16_t normal_z_store_ptr;
    uint16_t negated_dir_dot_normal;

    slot_flags = fx_ldw(slot_ptr);
    slot_depth_ptr = (uint16_t)(slot_ptr + 2);
    if (slot_flags != 0) {
        push_flags = fx_ldw(FX_PUSH_FLAGS);
        fx_sbk((slot_flags | push_flags));
        if ((uint16_t)(slot_flags >> 8) == 0) {
            fx_spill(stack);
            slot_depth = fx_ldw(slot_depth_ptr);
            slot_normal_x_ptr = (uint16_t)(slot_depth_ptr + 2);
            if (!(slot_depth & 0x8000)) {
                contact_depth = fx_ldw(FX_CONTACT_DEPTH);
                push_y = contact_depth;
                if (!(contact_depth & 0x8000)) {
                } else {
                    push_y = (uint16_t)((uint16_t)~push_y + 1);
                }
                push_x = slot_depth;
                if (!(slot_depth & 0x8000)) {
                } else {
                    push_x = (uint16_t)((uint16_t)~push_x + 1);
                }
                if ((uint16_t)(push_y - push_x) & 0x8000) {
                } else {
                    fx_stw(FX_CONTACT_DEPTH, slot_depth);
                }
                if ((uint16_t)((uint16_t)(0 - slot_depth) + 0x7D00) != 0) {
                    depth_x2 = (uint16_t)(slot_depth + slot_depth);
                    depth_x4 = (uint16_t)(depth_x2 + depth_x2);
                    depth_x8 = (uint16_t)(depth_x4 + depth_x4);
                    depth_x16 = (uint16_t)(depth_x8 + depth_x8);
                    push_x_out_2 = fx_ldw(slot_normal_x_ptr);
                    slot_normal_y_ptr = (uint16_t)(slot_normal_x_ptr + 2);
                    slot_normal_y = fx_ldw(slot_normal_y_ptr);
                    slot_normal_z_ptr = (uint16_t)(slot_normal_y_ptr + 2);
                    slot_normal_z = fx_ldw(slot_normal_z_ptr);
                    slot_end_ptr = (uint16_t)(slot_normal_z_ptr + 2);
                    if ((push_x_out_2 | slot_normal_z) != 0) {
                        run_lAFD2 = 1;
                    } else {
                        wheel_contact = fx_ldw(FX_WHEEL_CONTACT);
                        if (wheel_contact == 0) {
                            fx_sbk(1);
                            run_lAFD2 = 1;
                        } else {
                        }
                    }
                    if (run_lAFD2) {
                        run_lAFD2 = 0;
                        push_turn = fx_ldw(FX_PUSH_TURN);
                        if (push_turn != 0) {
                            if (push_turn != 0x0040) {
                                if (push_turn != 0x00C0) {
                                    if (push_turn != 0x0080) {
                                        fx_push(stack, push_flags);
                                        fx_push(stack, slot_end_ptr);
                                        fx_push(stack, slot_normal_y);
                                        fx_push(stack, depth_x16);
                                        fx_spill(stack);
                                        fx_push(stack, contact_record);
                                        fx_spill(stack);
                                        x = push_x_out_2;
                                        z = slot_normal_z;
                                        if ((x | z) == 0) {
                                        } else {
                                            { FxTurned t = FxTurn((int16_t)x, (uint8_t)push_turn, (int16_t)z, fx_table_bank); new_x_x = (uint16_t)t.x; new_z = (uint16_t)t.z; };
                                            turned_x_x2 = (uint16_t)(new_x_x + new_x_x);
                                            x = (uint16_t)(turned_x_x2 + turned_x_x2);
                                            turned_z_x2 = (uint16_t)(new_z + new_z);
                                            z = (uint16_t)(turned_z_x2 + turned_z_x2);
                                        }
                                        fx_unspill(stack);
                                        push_x_out_2 = z;
                                        slot_normal_z = x;
                                        contact_record = fx_pull(stack);
                                        fx_unspill(stack);
                                        depth_x16 = fx_pull(stack);
                                        slot_normal_y = fx_pull(stack);
                                        slot_end_ptr = fx_pull(stack);
                                        fx_unspill(stack);
                                    } else {
                                        push_x_out_2 = (uint16_t)((uint16_t)~push_x_out_2 + 1);
                                        slot_normal_z = (uint16_t)((uint16_t)~slot_normal_z + 1);
                                    }
                                } else {
                                    r_t_29 = (uint16_t)((uint16_t)~push_x_out_2 + 1);
                                    push_x_out_2 = slot_normal_z;
                                    slot_normal_z = r_t_29;
                                }
                            } else {
                                r_t_30 = (uint16_t)((uint16_t)~slot_normal_z + 1);
                                slot_normal_z = push_x_out_2;
                                push_x_out_2 = r_t_30;
                            }
                        } else {
                        }
                        normal_x_store_ptr = (uint16_t)(slot_end_ptr - 6);
                        fx_stw(normal_x_store_ptr, push_x_out_2);
                        normal_y_store_ptr = (uint16_t)(normal_x_store_ptr + 2);
                        fx_stw(normal_y_store_ptr, slot_normal_y);
                        normal_z_store_ptr = (uint16_t)(normal_y_store_ptr + 2);
                        fx_stw(normal_z_store_ptr, slot_normal_z);
                        slot_end_ptr = (uint16_t)(normal_z_store_ptr + 2);
                        fx_stw(FX_SIZES, push_x_out_2);
                        fx_stw(0x236E, slot_normal_y);
                        fx_stw(0x2370, slot_normal_z);
                        record_dir_x = fx_ldw((uint16_t)(contact_record + 0x002E));
                        sizes = fx_ldw(FX_SIZES);
                        record_dir_y = fx_ldw((uint16_t)(contact_record + 0x0030));
                        normal_y_reload = fx_ldw(0x236E);
                        record_dir_z = fx_ldw((uint16_t)(contact_record + 0x0032));
                        normal_z_reload = fx_ldw(0x2370);
                        negated_dir_dot_normal = (uint16_t)((uint16_t)~(uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(record_dir_x + record_dir_x) * (int16_t)sizes) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(record_dir_y + record_dir_y) * (int16_t)normal_y_reload) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(record_dir_z + record_dir_z) * (int16_t)normal_z_reload) >> 16)) + 1);
                        fx_stw((uint16_t)(contact_record + FXREC_SIDES), negated_dir_dot_normal);
                        record_scale = fx_ldw((uint16_t)(contact_record + 0x0044));
                        scaled_dot_prod = (uint32_t)((int32_t)(int16_t)negated_dir_dot_normal * (int16_t)record_scale);
                        dot_scaled_swapped = ((uint16_t)((uint16_t)((uint16_t)(scaled_dot_prod >> 16) & 0xFF) << 8 | (uint16_t)((uint16_t)(scaled_dot_prod >> 16) & 0xFF) >> 8) | (uint16_t)((uint16_t)scaled_dot_prod >> 8));
                        push_force = dot_scaled_swapped;
                        if (!(dot_scaled_swapped & 0x8000)) {
                            if (!((uint16_t)(push_force - 0x07D0) & 0x8000)) {
                            } else {
                                push_force = (uint16_t)(push_force + depth_x16);
                            }
                        } else {
                            push_force = (uint16_t)(fx_div2(push_force) + depth_x16);
                            wheel_flags = fx_ldw(FX_WHEEL_FLAGS);
                            if ((0x0100 & wheel_flags) == 0) {
                            } else {
                                push_force = 0;
                            }
                        }
                        push_force_copy = push_force;
                        normal_y_reload_2 = fx_ldw(0x236E);
                        push_force_y = (uint16_t)((uint32_t)((int32_t)(int16_t)push_force_copy * (int16_t)normal_y_reload_2) >> 16);
                        if (normal_y_reload_2 != 0) {
                        } else {
                            clamped_dot = negated_dir_dot_normal;
                            if (!(negated_dir_dot_normal & 0x8000)) {
                            } else {
                                clamped_dot = 0;
                            }
                            clamped_dot_x2 = (uint16_t)(clamped_dot + clamped_dot);
                            clamped_dot_x4 = (uint16_t)((uint16_t)(clamped_dot_x2 + clamped_dot_x2) + depth_x16);
                            lift_force = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(clamped_dot_x4 + clamped_dot_x4) * (int16_t)0x1000) >> 16);
                            push_force_copy = (uint16_t)(clamped_dot_x4 + lift_force);
                            abs_lift_force = lift_force;
                            if (!(lift_force & 0x8000)) {
                            } else {
                                abs_lift_force = (uint16_t)((uint16_t)~abs_lift_force + 1);
                            }
                            if (!((uint16_t)((uint16_t)(0 - abs_lift_force) + 0x0064) & 0x8000)) {
                            } else {
                                fxpart_3c = fx_ldw((uint16_t)(contact_record + FXPART_3C));
                                if (!(fxpart_3c & 0x8000)) {
                                    hit_depth = fx_ldw(FX_HIT_DEPTH);
                                    fx_sbk((hit_depth | 0x0200));
                                } else {
                                    hit_depth_2 = fx_ldw(FX_HIT_DEPTH);
                                    fx_sbk((hit_depth_2 | 0x0100));
                                }
                            }
                        }
                        sizes_2 = fx_ldw(FX_SIZES);
                        normal_z_reload_2 = fx_ldw(0x2370);
                        if ((sizes_2 | normal_z_reload_2) == 0) {
                        } else {
                            hit_depth_3 = fx_ldw(FX_HIT_DEPTH);
                            fx_sbk((hit_depth_3 | 0x0400));
                        }
                        sizes_3 = fx_ldw(FX_SIZES);
                        normal_z_reload_3 = fx_ldw(0x2370);
                        { FxSumsPushWorldResult r_ = FxSumsPushWorld((uint16_t)((uint32_t)((int32_t)(int16_t)push_force_copy * (int16_t)sizes_3) >> 16), push_force_y, (uint16_t)((uint32_t)((int32_t)(int16_t)push_force_copy * (int16_t)normal_z_reload_3) >> 16), stack); push_x_out_2 = r_.push_x_out; slot_normal_y = r_.push_y_out; };
                        sizes_4 = fx_ldw(FX_SIZES);
                        contact_sum = fx_ldw(FX_CONTACT_SUM);
                        fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)depth_x16 * (int16_t)sizes_4) >> 16) + contact_sum));
                        normal_y_reload_3 = fx_ldw(0x236E);
                        contact_sum_y = fx_ldw(0x238A);
                        fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)depth_x16 * (int16_t)normal_y_reload_3) >> 16) + contact_sum_y));
                        normal_z_reload_4 = fx_ldw(0x2370);
                        contact_sum_z = fx_ldw(0x238C);
                        fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)depth_x16 * (int16_t)normal_z_reload_4) >> 16) + contact_sum_z));
                    }
                    fx_unspill(stack);
                    return (FxContactResolveResult){slot_end_ptr, push_x_out_2, slot_normal_y, depth_x16, rom_at};
                } else {
                    fx_unspill(stack);
                    return (FxContactResolveResult){(uint16_t)(slot_normal_x_ptr + 6), push_x, push_y, slot_depth, rom_at};
                }
            } else {
                fx_unspill(stack);
                return (FxContactResolveResult){(uint16_t)(slot_normal_x_ptr + 6), push_x, push_y, slot_depth, rom_at};
            }
        } else {
            return (FxContactResolveResult){(uint16_t)(slot_depth_ptr + 8), push_x, push_y, model_record, rom_at};
        }
    } else {
        return (FxContactResolveResult){(uint16_t)(slot_depth_ptr + 8), push_x, push_y, model_record, rom_at};
    }
}


/*
 * @routine 04:AF90 FxContactResolveNegate
 * WHAT IT DOES: FxContactResolve with the slot's depth (R7, copied to
 * R4) negative at its absolute value: R4 = -R4, then the rest of
 * FxContactResolve from its depth compare. The game never runs it: a
 * negative depth branches to 04:B191 before, so the BPL at 04:AF8D
 * always branches past it. A unit start runs it so the C is judged
 * against the original.
 * HOW: WITH R4; NOT; INC R4; then as FxContactResolve from 04:AF93.
 * Entry: R3 = the slot past its depth word; R4 = R7 = the depth; R5 =
 * the shallowest depth so far, made positive; R8 = the car; R10 =
 * stack, the return address the word below it.
 * Exit: as FxContactResolve's.
 */
FxContactResolveNegateResult FxContactResolveNegate(uint16_t slot_past, uint16_t depth_x, uint16_t shallowest_depth, uint16_t depth_z, uint16_t car_ptr, uint16_t *stack)
{
    int run_lAFD2 = 0;
    uint16_t x;
    uint16_t z;
    uint16_t new_z;
    uint16_t sizes;
    uint16_t r_t_23;
    uint16_t r_t_24;
    uint16_t new_x_x;
    uint16_t sizes_2;
    uint16_t sizes_3;
    uint16_t sizes_4;
    uint16_t sizes_5;
    uint16_t depth_x2;
    uint16_t depth_x4;
    uint16_t depth_x8;
    uint16_t normal_y;
    uint16_t normal_z;
    uint16_t fxpart_3c;
    uint16_t hit_depth;
    uint16_t impulse_y;
    uint16_t push_turn;
    uint16_t car_axis_x;
    uint16_t car_axis_y;
    uint16_t car_axis_z;
    uint16_t slot_y_ptr;
    uint16_t slot_z_ptr;
    uint16_t contact_sum;
    uint16_t hit_depth_2;
    uint16_t hit_depth_3;
    uint16_t turned_x_x2;
    uint16_t turned_z_x2;
    uint16_t wheel_flags;
    uint16_t car_field_44;
    uint16_t depth_scaled;
    uint16_t normal_y_ram;
    uint16_t normal_z_ram;
    uint16_t contact_sum_y;
    uint16_t contact_sum_z;
    uint16_t impulse_mid16;
    uint16_t impulse_scale;
    uint16_t slot_next_ptr;
    uint16_t wheel_contact;
    uint16_t impulse_adjust;
    uint16_t neg_normal_dot;
    uint16_t normal_y_ram_2;
    uint16_t normal_y_ram_3;
    uint16_t normal_z_ram_2;
    uint16_t normal_z_ram_3;
    uint16_t normal_z_ram_4;
    uint16_t boosted_impulse;
    uint16_t clamped_neg_dot;
    uint16_t contact_impulse;
    uint16_t normal_store_ptr;
    uint16_t abs_impulse_adjust;
    uint16_t clamped_neg_dot_x2;
    uint32_t dot_times_field_44;
    uint16_t normal_store_y_ptr;

    if ((uint16_t)((uint16_t)(shallowest_depth - (uint16_t)~depth_x) - 1) & 0x8000) {
    } else {
        fx_stw(FX_CONTACT_DEPTH, depth_z);
    }
    if ((uint16_t)((uint16_t)(0 - depth_z) + 0x7D00) != 0) {
        depth_x2 = (uint16_t)(depth_z + depth_z);
        depth_x4 = (uint16_t)(depth_x2 + depth_x2);
        depth_x8 = (uint16_t)(depth_x4 + depth_x4);
        depth_scaled = (uint16_t)(depth_x8 + depth_x8);
        sizes = fx_ldw(slot_past);
        slot_y_ptr = (uint16_t)(slot_past + 2);
        normal_y = fx_ldw(slot_y_ptr);
        slot_z_ptr = (uint16_t)(slot_y_ptr + 2);
        normal_z = fx_ldw(slot_z_ptr);
        slot_next_ptr = (uint16_t)(slot_z_ptr + 2);
        if ((sizes | normal_z) != 0) {
            run_lAFD2 = 1;
        } else {
            wheel_contact = fx_ldw(FX_WHEEL_CONTACT);
            if (wheel_contact == 0) {
                fx_sbk(1);
                run_lAFD2 = 1;
            } else {
            }
        }
        if (run_lAFD2) {
            run_lAFD2 = 0;
            push_turn = fx_ldw(FX_PUSH_TURN);
            if (push_turn != 0) {
                if (push_turn != 0x0040) {
                    if (push_turn != 0x00C0) {
                        if (push_turn != 0x0080) {
                            fx_spill(stack);
                            fx_push(stack, slot_next_ptr);
                            fx_push(stack, normal_y);
                            fx_push(stack, depth_scaled);
                            fx_spill(stack);
                            fx_push(stack, car_ptr);
                            fx_spill(stack);
                            x = sizes;
                            z = normal_z;
                            if ((x | z) == 0) {
                            } else {
                                { FxTurned t = FxTurn((int16_t)x, (uint8_t)push_turn, (int16_t)z, fx_table_bank); new_x_x = (uint16_t)t.x; new_z = (uint16_t)t.z; };
                                turned_x_x2 = (uint16_t)(new_x_x + new_x_x);
                                x = (uint16_t)(turned_x_x2 + turned_x_x2);
                                turned_z_x2 = (uint16_t)(new_z + new_z);
                                z = (uint16_t)(turned_z_x2 + turned_z_x2);
                            }
                            fx_unspill(stack);
                            sizes = z;
                            normal_z = x;
                            car_ptr = fx_pull(stack);
                            fx_unspill(stack);
                            depth_scaled = fx_pull(stack);
                            normal_y = fx_pull(stack);
                            slot_next_ptr = fx_pull(stack);
                            fx_unspill(stack);
                        } else {
                            sizes = (uint16_t)((uint16_t)~sizes + 1);
                            normal_z = (uint16_t)((uint16_t)~normal_z + 1);
                        }
                    } else {
                        r_t_23 = (uint16_t)((uint16_t)~sizes + 1);
                        sizes = normal_z;
                        normal_z = r_t_23;
                    }
                } else {
                    r_t_24 = (uint16_t)((uint16_t)~normal_z + 1);
                    normal_z = sizes;
                    sizes = r_t_24;
                }
            } else {
            }
            normal_store_ptr = (uint16_t)(slot_next_ptr - 6);
            fx_stw(normal_store_ptr, sizes);
            normal_store_y_ptr = (uint16_t)(normal_store_ptr + 2);
            fx_stw(normal_store_y_ptr, normal_y);
            fx_stw((uint16_t)(normal_store_y_ptr + 2), normal_z);
            fx_stw(FX_SIZES, sizes);
            fx_stw(0x236E, normal_y);
            fx_stw(0x2370, normal_z);
            car_axis_x = fx_ldw((uint16_t)(car_ptr + 0x002E));
            sizes_2 = fx_ldw(FX_SIZES);
            car_axis_y = fx_ldw((uint16_t)(car_ptr + 0x0030));
            normal_y_ram = fx_ldw(0x236E);
            car_axis_z = fx_ldw((uint16_t)(car_ptr + 0x0032));
            normal_z_ram = fx_ldw(0x2370);
            neg_normal_dot = (uint16_t)((uint16_t)~(uint16_t)((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(car_axis_x + car_axis_x) * (int16_t)sizes_2) >> 16) + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(car_axis_y + car_axis_y) * (int16_t)normal_y_ram) >> 16)) + (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(car_axis_z + car_axis_z) * (int16_t)normal_z_ram) >> 16)) + 1);
            fx_stw((uint16_t)(car_ptr + FXREC_SIDES), neg_normal_dot);
            car_field_44 = fx_ldw((uint16_t)(car_ptr + 0x0044));
            dot_times_field_44 = (uint32_t)((int32_t)(int16_t)neg_normal_dot * (int16_t)car_field_44);
            impulse_mid16 = ((uint16_t)((uint16_t)((uint16_t)(dot_times_field_44 >> 16) & 0xFF) << 8 | (uint16_t)((uint16_t)(dot_times_field_44 >> 16) & 0xFF) >> 8) | (uint16_t)((uint16_t)dot_times_field_44 >> 8));
            contact_impulse = impulse_mid16;
            if (!(impulse_mid16 & 0x8000)) {
                if (!((uint16_t)(contact_impulse - 0x07D0) & 0x8000)) {
                } else {
                    contact_impulse = (uint16_t)(contact_impulse + depth_scaled);
                }
            } else {
                contact_impulse = (uint16_t)(fx_div2(contact_impulse) + depth_scaled);
                wheel_flags = fx_ldw(FX_WHEEL_FLAGS);
                if ((0x0100 & wheel_flags) == 0) {
                } else {
                    contact_impulse = 0;
                }
            }
            impulse_scale = contact_impulse;
            normal_y_ram_2 = fx_ldw(0x236E);
            impulse_y = (uint16_t)((uint32_t)((int32_t)(int16_t)impulse_scale * (int16_t)normal_y_ram_2) >> 16);
            if (normal_y_ram_2 != 0) {
            } else {
                clamped_neg_dot = neg_normal_dot;
                if (!(neg_normal_dot & 0x8000)) {
                } else {
                    clamped_neg_dot = 0;
                }
                clamped_neg_dot_x2 = (uint16_t)(clamped_neg_dot + clamped_neg_dot);
                boosted_impulse = (uint16_t)((uint16_t)(clamped_neg_dot_x2 + clamped_neg_dot_x2) + depth_scaled);
                impulse_adjust = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(boosted_impulse + boosted_impulse) * (int16_t)0x1000) >> 16);
                impulse_scale = (uint16_t)(boosted_impulse + impulse_adjust);
                abs_impulse_adjust = impulse_adjust;
                if (!(impulse_adjust & 0x8000)) {
                } else {
                    abs_impulse_adjust = (uint16_t)((uint16_t)~abs_impulse_adjust + 1);
                }
                if (!((uint16_t)((uint16_t)(0 - abs_impulse_adjust) + 0x0064) & 0x8000)) {
                } else {
                    fxpart_3c = fx_ldw((uint16_t)(car_ptr + FXPART_3C));
                    if (!(fxpart_3c & 0x8000)) {
                        hit_depth = fx_ldw(FX_HIT_DEPTH);
                        fx_sbk((hit_depth | 0x0200));
                    } else {
                        hit_depth_2 = fx_ldw(FX_HIT_DEPTH);
                        fx_sbk((hit_depth_2 | 0x0100));
                    }
                }
            }
            sizes_3 = fx_ldw(FX_SIZES);
            normal_z_ram_2 = fx_ldw(0x2370);
            if ((sizes_3 | normal_z_ram_2) == 0) {
            } else {
                hit_depth_3 = fx_ldw(FX_HIT_DEPTH);
                fx_sbk((hit_depth_3 | 0x0400));
            }
            sizes_4 = fx_ldw(FX_SIZES);
            normal_z_ram_3 = fx_ldw(0x2370);
            FxSumsPushWorld((uint16_t)((uint32_t)((int32_t)(int16_t)impulse_scale * (int16_t)sizes_4) >> 16), impulse_y, (uint16_t)((uint32_t)((int32_t)(int16_t)impulse_scale * (int16_t)normal_z_ram_3) >> 16), stack);
            sizes_5 = fx_ldw(FX_SIZES);
            contact_sum = fx_ldw(FX_CONTACT_SUM);
            fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)depth_scaled * (int16_t)sizes_5) >> 16) + contact_sum));
            normal_y_ram_3 = fx_ldw(0x236E);
            contact_sum_y = fx_ldw(0x238A);
            fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)depth_scaled * (int16_t)normal_y_ram_3) >> 16) + contact_sum_y));
            normal_z_ram_4 = fx_ldw(0x2370);
            contact_sum_z = fx_ldw(0x238C);
            fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)depth_scaled * (int16_t)normal_z_ram_4) >> 16) + contact_sum_z));
        }
        fx_unspill(stack);
        return (FxContactResolveNegateResult){1};
    } else {
        fx_unspill(stack);
        return (FxContactResolveNegateResult){1};
    }
}


/*
 * @routine 04:B1A1 FxCarHandler
 * WHAT IT DOES: The record handler of a car: steering from the input
 * (FxSteerFromInput), its pose (FxCarPose), its parts (FxPartsUpdate),
 * the move (FxRecordMove) and its collisions (FxCarCollide, FxCarHit),
 * holding it on the ground (FxRecordHoldY, FxRecordAlignment), and its
 * place on the course (FxCarFollowCourse, FxLapSegment,
 * FxCourseNextDistance).
 * HOW: Pushes R11 and R8; returns by pulling R15.
 * Entry: R8 = the car; R10 = stack; R11 = return address.
 * Exit: R8 back; the rest as the last call leaves them.
 */
FxCarHandlerResult FxCarHandler(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t car, uint16_t pose_matrix_ptr, uint16_t *stack)
{
    int up_ = 0;                       /* levels a callee returned past us */
    int run_lB23B = 0;
    int run_lB29A = 0;
    uint16_t y;
    uint16_t c2;
    uint16_t s1;
    uint16_t s2;
    uint16_t roll;
    uint16_t count;
    uint16_t entry;
    uint16_t parts;
    uint16_t pitch;
    uint16_t sides;
    uint16_t speed;
    uint16_t y_row;
    uint16_t half_x;
    uint16_t spread;
    uint16_t x_part;
    uint16_t car_yaw;
    uint16_t counted;
    uint16_t flags1c;
    uint16_t flags40;
    uint16_t pitch_2;
    uint16_t segment;
    uint16_t sides_2;
    uint16_t speed_2;
    uint16_t speed_3;
    uint16_t speed_4;
    uint16_t speed_5;
    uint16_t speed_6;
    uint16_t y_row_2;
    uint16_t distance;
    uint16_t fxcar_64;
    uint16_t fxrec_6a;
    uint16_t last_low;
    uint16_t low_word;
    uint16_t part_ptr;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t abs_speed;
    uint16_t car_out_2;
    uint16_t car_y_out;
    uint16_t car_z_out;
    uint16_t direction;
    uint16_t flags1c_2;
    uint16_t flags1c_3;
    uint16_t flags1c_4;
    uint16_t flags1c_5;
    uint16_t flags40_2;
    uint16_t flags40_3;
    uint16_t flags40_4;
    uint16_t flags40_5;
    uint16_t flags40_6;
    uint16_t flags40_7;
    uint16_t flags40_8;
    uint16_t flags40_9;
    uint16_t fxpart_46;
    uint16_t fxpart_50;
    uint16_t init_2470;
    uint16_t init_2472;
    uint16_t last_part;
    uint16_t lift_base;
    uint16_t new_value;
    uint16_t old_world;
    uint16_t entry_word;
    uint16_t force_233c;
    uint16_t fxcar_64_2;
    uint16_t fxcar_64_3;
    uint16_t fxrec_6a_2;
    uint16_t gear_index;
    uint16_t grip_value;
    uint16_t last_low_2;
    uint16_t lift_value;
    uint16_t matrix_out;
    uint16_t param_jump;
    uint16_t param_lift;
    uint16_t pose_aux_2;
    uint16_t pose_aux_3;
    uint16_t record_ptr;
    uint16_t rom_at_out;
    uint16_t speed_span;
    uint16_t spread_out;
    uint16_t car_mode_44;
    uint16_t curve_index;
    uint16_t curve_scale;
    uint16_t direction_2;
    uint16_t direction_3;
    uint16_t segment_out;
    uint16_t car_angle_22;
    uint16_t car_timer_54;
    uint16_t param_thrust;
    uint16_t rom_at_out_2;
    uint16_t rom_at_out_3;
    uint16_t rom_at_out_4;
    uint16_t rom_at_out_5;
    uint16_t share_before;
    uint16_t thrust_scale;
    uint16_t curve_row_ptr;
    uint16_t heading_angle;
    uint16_t last_part_ptr;
    uint16_t other_car_out;
    uint16_t param_curve_a;
    uint16_t param_curve_b;
    uint16_t param_reverse;
    uint16_t speed_doubled;
    uint16_t spin_damp_abs;
    uint16_t curve_low_byte;
    uint16_t curve_ptr_next;
    uint16_t entry_word_low;
    uint16_t hit_flags_26f2;
    uint16_t param_wheel_48;
    uint16_t segment_number;
    uint16_t speed_octupled;
    uint16_t speed_over_cap;
    uint16_t spin_damp_term;
    uint16_t thrust_doubled;
    uint16_t car_state_flags;
    uint16_t course_followed;
    uint16_t entry_copy_dest;
    uint16_t last_change_low;
    uint16_t last_product_hi;
    uint16_t last_product_lo;
    uint16_t param_speed_cap;
    uint16_t param_spin_damp;
    uint16_t param_turn_grip;
    uint16_t record_angle_26;
    uint16_t speed_remaining;
    uint16_t speed_threshold;
    uint16_t car_countdown_5e;
    uint16_t contact_force_up;
    uint16_t last_change_high;
    uint16_t model_record_out;
    uint16_t prev_curve_speed;
    uint16_t speed_quadrupled;
    uint16_t state_flags_copy;
    uint16_t course_followed_2;
    uint16_t curve_entry_index;
    uint16_t curve_speed_limit;
    uint16_t last_change_low_2;
    uint16_t last_change_low_3;
    uint16_t contact_force_side;
    uint16_t entry_rom_ptr_next;
    uint16_t last_change_high_2;
    uint16_t last_change_high_3;
    uint16_t model_record_out_2;
    uint16_t record_curve_index;
    uint16_t record_state_flags;
    uint16_t state_flags_reload;
    uint16_t caller_scratch_back;
    uint16_t current_curve_index;
    uint16_t world_z_partial_sum;
    uint16_t last_part_velocity_z;
    uint16_t next_entry_end_ptr_2;
    uint16_t next_entry_end_ptr_3;
    uint16_t curve_ptr_after_scale;
    uint16_t signed_speed_octupled;
    uint16_t normalize_work_reg_out;

    fx_spill(stack);
    flags1c = fx_ldw((uint16_t)(car + FXREC_FLAGS1C));
    fx_sbk((flags1c | 4));
    parts = fx_ldw((uint16_t)(car + FXCAR_PARTS));
    for (;;) {
        part_ptr = parts;
        if (part_ptr == 0) {
            break;
        } else {
            flags1c_2 = fx_ldw((uint16_t)(part_ptr + FXREC_FLAGS1C));
            fx_sbk((flags1c_2 | 4));
            parts = fx_ldw((uint16_t)(part_ptr + 0x0034));
            continue;
        }
    }
    init_2472 = fx_ldw(FX_INIT_2472);
    if (init_2472 == 0) {
        flags1c_3 = fx_ldw((uint16_t)(car + FXREC_FLAGS1C));
        fx_sbk((uint16_t)(flags1c_3 & ~2));
        fx_stw((uint16_t)(car + 0x0092), 0);
        fx_romb(9);
        entry = fx_ldw((uint16_t)(car + FXCAR_ENTRY));
        rom_at_out = entry;
        fx_r14(rom_at_out);
        entry_copy_dest = 0x2652;
        count = 0x0019;
        for (;;) {
            entry_word_low = fx_getb();
            entry_rom_ptr_next = (uint16_t)(rom_at_out + 1);
            fx_r14(entry_rom_ptr_next);
            entry_word = (uint16_t)((entry_word_low & 0xFF) | fx_getb() << 8);
            rom_at_out = (uint16_t)(entry_rom_ptr_next + 1);
            fx_r14(rom_at_out);
            fx_stw(entry_copy_dest, entry_word);
            entry_copy_dest = (uint16_t)(entry_copy_dest + 2);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        flags1c_4 = fx_ldw((uint16_t)(car + FXREC_FLAGS1C));
        if ((0x0020 & flags1c_4) == 0) {
            if ((0x0010 & flags1c_4) == 0) {
                run_lB23B = 1;
            } else {
                fxcar_64 = fx_ldw((uint16_t)(car + FXCAR_64));
                if ((fxcar_64 & 8) != 0) {
                    { FxCarFollowCourseResult r_ = FxCarFollowCourse(pose_cos, pose_sin, pose_aux, car, pose_matrix_ptr, stack, rom_at_out); up_ = r_.up; course_followed = r_.course_followed; last_change_low = r_.last_change_low; last_change_high = r_.last_change_high; pose_aux = r_.last_scan_value; pose_matrix_ptr = r_.last_scan_aux; rom_at_out = r_.rom_at_out; };
                    if (up_) {                       /* it returned past us */
                        return (FxCarHandlerResult){last_change_low, last_change_high, pose_aux, car, pose_matrix_ptr, rom_at_out, up_ == 1 ? 0 : up_ - 1};
                    }
                    if (course_followed != 0) {
                        run_lB23B = 1;
                    } else {
                        { FxLapSegmentResult r_ = FxLapSegment(car, rom_at_out); segment_out = r_.segment_out; rom_at_out_2 = r_.rom_at_out; };
                        counted = fx_ldw((uint16_t)(car + FXREC_COUNTED));
                        { FxCourseNextDistanceResult r_ = FxCourseNextDistance(counted, segment_out, car, stack, rom_at_out_2); distance = r_.distance; x_part = r_.x_part; next_entry_end_ptr_2 = r_.next_entry_end_ptr; rom_at_out_3 = r_.rom_at_out; };
                        fx_stw((uint16_t)(car + FXREC_TO_GO), distance);
                        fx_unspill(stack);
                        return (FxCarHandlerResult){x_part, next_entry_end_ptr_2, pose_aux, car, pose_matrix_ptr, rom_at_out_3, 0};
                    }
                } else {
                    { FxLapSegmentResult r_ = FxLapSegment(car, rom_at_out); segment_out = r_.segment_out; rom_at_out_2 = r_.rom_at_out; };
                    counted = fx_ldw((uint16_t)(car + FXREC_COUNTED));
                    { FxCourseNextDistanceResult r_ = FxCourseNextDistance(counted, segment_out, car, stack, rom_at_out_2); distance = r_.distance; x_part = r_.x_part; next_entry_end_ptr_2 = r_.next_entry_end_ptr; rom_at_out_3 = r_.rom_at_out; };
                    fx_stw((uint16_t)(car + FXREC_TO_GO), distance);
                    fx_unspill(stack);
                    return (FxCarHandlerResult){x_part, next_entry_end_ptr_2, pose_aux, car, pose_matrix_ptr, rom_at_out_3, 0};
                }
            }
            FxRecordMove(6, car);
            fx_push(stack, car);
            car_yaw = fx_ldw((uint16_t)(car + 0x0026));
            fx_stw(0x0020, (uint16_t)((uint16_t)~car_yaw + 1));
            pitch = fx_ldw((uint16_t)(car + FXREC_HEADING));
            fx_stw(0x0022, (uint16_t)((uint16_t)~pitch + 1));
            roll = fx_ldw((uint16_t)(car + FXREC_ROLL));
            fx_stw(0x0024, (uint16_t)((uint16_t)~roll + 1));
            { FxMatrixFromAnglesResult r_ = FxMatrixFromAngles(0x00E4); s1 = r_.s1; c2 = r_.c2; s2 = r_.s2; rom_at_2 = r_.rom_at; };
            matrix_out = FxMatrixTranspose04(0x00E4);
            car = fx_pull(stack);
            fxcar_64_2 = fx_ldw((uint16_t)(car + FXCAR_64));
            if ((fxcar_64_2 & 8) == 0) {
                { FxRecordSegmentResult r_ = FxRecordSegment(s1, c2, s2, car, matrix_out, stack); segment_number = r_.segment_number; };
                fx_stw((uint16_t)(car + FXREC_SEGMENT), segment_number);
                run_lB29A = 1;
            } else {
                { FxCarFollowCourseResult r_ = FxCarFollowCourse(s1, c2, s2, car, matrix_out, stack, rom_at_2); up_ = r_.up; course_followed_2 = r_.course_followed; last_change_low_2 = r_.last_change_low; last_change_high_2 = r_.last_change_high; pose_aux = r_.last_scan_value; pose_matrix_ptr = r_.last_scan_aux; rom_at_out = r_.rom_at_out; };
                if (up_) {                       /* it returned past us */
                    return (FxCarHandlerResult){last_change_low_2, last_change_high_2, pose_aux, car, pose_matrix_ptr, rom_at_out, up_ == 1 ? 0 : up_ - 1};
                }
                if (course_followed_2 != 0) {
                    run_lB29A = 1;
                } else {
                    { FxLapSegmentResult r_ = FxLapSegment(car, rom_at_out); segment_out = r_.segment_out; rom_at_out_2 = r_.rom_at_out; };
                    counted = fx_ldw((uint16_t)(car + FXREC_COUNTED));
                    { FxCourseNextDistanceResult r_ = FxCourseNextDistance(counted, segment_out, car, stack, rom_at_out_2); distance = r_.distance; x_part = r_.x_part; next_entry_end_ptr_2 = r_.next_entry_end_ptr; rom_at_out_3 = r_.rom_at_out; };
                    fx_stw((uint16_t)(car + FXREC_TO_GO), distance);
                    fx_unspill(stack);
                    return (FxCarHandlerResult){x_part, next_entry_end_ptr_2, pose_aux, car, pose_matrix_ptr, rom_at_out_3, 0};
                }
            }
            fx_stw(FX_HIT, 0);
            fx_stw(0x2336, 0);
            fx_stw(0x2338, 0);
            fx_stw(0x233C, 0);
            fx_stw(FX_SPIN, 0);
            fx_stw(0x233E, 0);
            speed = fx_ldw((uint16_t)(car + FXREC_SPEED));
            param_spin_damp = fx_ldw(FX_PARAM_SPIN_DAMP);
            spin_damp_term = (uint16_t)((uint32_t)((int32_t)(int16_t)speed * (int16_t)param_spin_damp) >> 16);
            spin_damp_abs = spin_damp_term;
            if (!(spin_damp_term & 0x8000)) {
            } else {
                spin_damp_abs = (uint16_t)((uint16_t)~spin_damp_abs + 1);
            }
            lift_base = (uint16_t)(spin_damp_abs + 0x0078);
            flags1c_5 = fx_ldw((uint16_t)(car + FXREC_FLAGS1C));
            if ((flags1c_5 & 8) != 0) {
            } else {
                lift_base = (uint16_t)(lift_base + 0x003C);
            }
            fx_stw(0x2336, lift_base);
            half_x = fx_ldw((uint16_t)(car + FXREC_HALF_X));
            fx_stw(FX_CAR_POS, half_x);
            y = fx_ldw((uint16_t)(car + FXREC_Y));
            fx_stw(0x2356, y);
            car_angle_22 = fx_ldw((uint16_t)(car + 0x0022));
            fx_stw(0x2358, car_angle_22);
            car_state_flags = fx_ldw((uint16_t)(car + 0x008A));
            fx_stw(0x235A, car_state_flags);
            flags40 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
            fx_sbk(((uint16_t)(flags40 & ~0x086A) | 0x0100));
            sides = fx_ldw((uint16_t)(car + FXREC_SIDES));
            fxcar_64_3 = fx_ldw((uint16_t)(car + FXCAR_64));
            if ((8 & fxcar_64_3) == 0) {
                contact_force_side = 0x3E80;
                contact_force_up = 0x4650;
                flags40_2 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                if ((flags40_2 & 0x0010) == 0) {
                    state_flags_copy = fx_ldw(0x235A);
                    if ((4 & state_flags_copy) == 0) {
                        if ((1 & state_flags_copy) == 0) {
                            if ((0x0042 & state_flags_copy) == 0) {
                            } else {
                                contact_force_side = 0x4268;
                                contact_force_up = 0x3A98;
                            }
                        } else {
                            contact_force_side = 0x3E80;
                            contact_force_up = 0x4074;
                        }
                    } else {
                        contact_force_side = 0x445C;
                        contact_force_up = 0x4524;
                    }
                } else {
                }
            } else {
                spread = fx_ldw((uint16_t)(car + FXREC_SPREAD));
                if (spread == 0) {
                    contact_force_side = 0x7D00;
                    contact_force_up = 0x7D00;
                } else {
                    contact_force_side = 0x3E80;
                    contact_force_up = 0x3E80;
                }
            }
            fx_stw(FX_CONTACT_FORCE_SIDE, contact_force_side);
            fx_stw(FX_CONTACT_FORCE_UP, contact_force_up);
            speed_threshold = 0x00F0;
            if ((0x0400 & sides) == 0) {
            } else {
                speed_threshold = 0x014A;
            }
            flags40_3 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
            if ((0x2000 & flags40_3) != 0) {
            } else {
                state_flags_reload = fx_ldw(0x235A);
                if ((state_flags_reload & 0x0080) != 0) {
                    speed_2 = fx_ldw((uint16_t)(car + FXREC_SPEED));
                    abs_speed = speed_2;
                    if (!(speed_2 & 0x8000)) {
                    } else {
                        abs_speed = (uint16_t)((uint16_t)~abs_speed + 1);
                    }
                    if ((uint16_t)(abs_speed - speed_threshold) & 0x8000) {
                    } else {
                        sides = (uint16_t)((sides | 0x0040) & ~0x0430);
                    }
                } else {
                    if ((0x0010 & flags40_3) != 0) {
                    } else {
                        fxpart_46 = fx_ldw((uint16_t)(car + FXPART_FLAGS));
                        if (fxpart_46 == 0) {
                        } else {
                            speed_2 = fx_ldw((uint16_t)(car + FXREC_SPEED));
                            abs_speed = speed_2;
                            if (!(speed_2 & 0x8000)) {
                            } else {
                                abs_speed = (uint16_t)((uint16_t)~abs_speed + 1);
                            }
                            if ((uint16_t)(abs_speed - speed_threshold) & 0x8000) {
                            } else {
                                sides = (uint16_t)((sides | 0x0040) & ~0x0430);
                            }
                        }
                    }
                }
            }
            car_mode_44 = fx_ldw((uint16_t)(car + 0x0044));
            if ((uint16_t)(car_mode_44 - 3) & 0x8000) {
            } else {
                sides = (uint16_t)(sides & ~0x0430);
                flags40_4 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                fx_sbk((flags40_4 | 8));
            }
            car_timer_54 = fx_ldw((uint16_t)(car + 0x0054));
            if (car_timer_54 != 0) {
                sides = (uint16_t)(sides & ~0x043F);
            } else {
                flags40_5 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                if ((flags40_5 & 4) == 0) {
                } else {
                    sides = (uint16_t)(sides & ~0x043F);
                }
            }
            car_countdown_5e = fx_ldw((uint16_t)(car + 0x005E));
            if (car_countdown_5e == 0) {
                if ((0x0080 & sides) == 0) {
                } else {
                    param_lift = fx_ldw(FX_PARAM_LIFT);
                    if (param_lift == 0) {
                    } else {
                        lift_value = fx_ldw(0x2336);
                        fx_stw(0x2336, (uint16_t)(lift_value + param_lift));
                        fx_stw((uint16_t)(car + 0x005E), 0x0020);
                    }
                }
            } else {
                fx_sbk((uint16_t)(car_countdown_5e - 1));
            }
            { FxSteerFromInputResult r_ = FxSteerFromInput(sides, car); new_value = r_.new_value; pose_aux_2 = r_.pose_aux; };
            direction = fx_ldw((uint16_t)(car + FXREC_DIRECTION));
            if ((8 & direction) == 0) {
            } else {
                new_value = (uint16_t)(new_value + new_value);
            }
            fx_stw((uint16_t)(car + FXCAR_STEER_BIAS), new_value);
            param_thrust = 0;
            if ((0x0020 & sides) == 0) {
            } else {
                param_reverse = fx_ldw(FX_PARAM_REVERSE);
                param_thrust = (uint16_t)((uint16_t)~param_reverse + 1);
            }
            if ((0x0400 & sides) == 0) {
                if ((0x0010 & sides) == 0) {
                } else {
                    param_thrust = fx_ldw(FX_PARAM_THRUST_BOOST);
                }
            } else {
                fx_ldw(FX_PARAM_THRUST_BOOST);
                fxrec_6a = fx_ldw((uint16_t)(car + FXREC_BOOST));
                if (fxrec_6a == 0) {
                    param_thrust = fx_ldw(FX_PARAM_THRUST_BOOST);
                } else {
                    flags40_6 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                    if ((0x2000 & flags40_6) != 0) {
                    } else {
                        param_speed_cap = fx_ldw(FX_PARAM_SPEED_CAP);
                        speed_over_cap = (uint16_t)(fxrec_6a - param_speed_cap);
                        fxrec_6a_2 = speed_over_cap;
                        if (!(speed_over_cap & 0x8000)) {
                        } else {
                            fxrec_6a_2 = 0;
                        }
                        hit_flags_26f2 = fx_ldw(0x26F2);
                        if ((hit_flags_26f2 & 0x0200) != 0) {
                        } else {
                            fx_stw((uint16_t)(car + FXREC_BOOST), fxrec_6a_2);
                        }
                    }
                    param_thrust = fx_ldw(FX_PARAM_THRUST);
                }
            }
            flags40_7 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
            if ((0x2000 & flags40_7) == 0) {
            } else {
                thrust_doubled = (uint16_t)(param_thrust + param_thrust);
                param_thrust = (uint16_t)(thrust_doubled + thrust_doubled);
            }
            fx_stw((uint16_t)(car + 0x0078), param_thrust);
            fx_romb(9);
            fx_table_bank = 0x09;
            param_curve_a = fx_ldw(FX_PARAM_CURVE_A);
            fx_r14(param_curve_a);
            curve_index = 1;
            prev_curve_speed = 0;
            speed_3 = fx_ldw((uint16_t)(car + FXREC_SPEED));
            if (!(speed_3 & 0x8000)) {
                for (;;) {
                    curve_low_byte = fx_getb();
                    curve_ptr_next = (uint16_t)(param_curve_a + 1);
                    fx_r14(curve_ptr_next);
                    curve_speed_limit = (uint16_t)((curve_low_byte & 0xFF) | fx_getb() << 8);
                    param_curve_a = (uint16_t)(curve_ptr_next + 1);
                    fx_r14(param_curve_a);
                    if (curve_speed_limit == 0) {
                        curve_index = (uint16_t)(curve_index - 1);
                        break;
                    } else {
                        if (!((uint16_t)(curve_speed_limit - speed_3) & 0x8000)) {
                            break;
                        } else {
                            prev_curve_speed = curve_speed_limit;
                            curve_index = (uint16_t)(curve_index + 1);
                            continue;
                        }
                    }
                }
                init_2470 = fx_ldw(FX_INIT_2470);
                if ((init_2470 & 3) != 0) {
                } else {
                    current_curve_index = fx_ldw((uint16_t)(car + 0x004E));
                    if (current_curve_index == curve_index) {
                    } else {
                        fx_sbk(curve_index);
                        if (curve_index != 4) {
                        } else {
                            flags40_8 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                            fx_sbk((flags40_8 | 2));
                        }
                    }
                }
                param_curve_b = fx_ldw(FX_PARAM_CURVE_B);
                fx_r14(param_curve_b);
                gear_index = fx_ldw((uint16_t)(car + 0x004E));
                curve_row_ptr = (uint16_t)(param_curve_b + (uint16_t)((int8_t)(uint16_t)(gear_index - 1) * (int8_t)0x0011));
                fx_r14(curve_row_ptr);
                curve_scale = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
                curve_ptr_after_scale = (uint16_t)(curve_row_ptr + 1);
                fx_r14(curve_ptr_after_scale);
                speed_4 = fx_ldw((uint16_t)(car + FXREC_SPEED));
                speed_remaining = speed_4;
                if (!(speed_4 & 0x8000)) {
                } else {
                    speed_remaining = (uint16_t)((uint16_t)~speed_remaining + 1);
                }
                speed_span = (uint16_t)(speed_remaining - prev_curve_speed);
                curve_entry_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(speed_span + speed_span) * (int16_t)curve_scale) >> 16);
                if ((uint16_t)(curve_entry_index - 0x000F) & 0x8000) {
                } else {
                    curve_entry_index = 0x000F;
                }
                y_row = curve_entry_index;
                param_curve_a = (uint16_t)(curve_ptr_after_scale + curve_entry_index);
                fx_r14(param_curve_a);
                thrust_scale = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
                param_thrust = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(param_thrust + param_thrust) * (int16_t)thrust_scale) >> 16);
            } else {
                if ((0x0410 & sides) == 0) {
                    curve_index = 0;
                    init_2470 = fx_ldw(FX_INIT_2470);
                    if ((init_2470 & 3) != 0) {
                    } else {
                        current_curve_index = fx_ldw((uint16_t)(car + 0x004E));
                        if (current_curve_index == curve_index) {
                        } else {
                            fx_sbk(curve_index);
                            if (curve_index != 4) {
                            } else {
                                flags40_8 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                                fx_sbk((flags40_8 | 2));
                            }
                        }
                    }
                    param_curve_b = fx_ldw(FX_PARAM_CURVE_B);
                    fx_r14(param_curve_b);
                    gear_index = fx_ldw((uint16_t)(car + 0x004E));
                    curve_row_ptr = (uint16_t)(param_curve_b + (uint16_t)((int8_t)(uint16_t)(gear_index - 1) * (int8_t)0x0011));
                    fx_r14(curve_row_ptr);
                    curve_scale = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
                    curve_ptr_after_scale = (uint16_t)(curve_row_ptr + 1);
                    fx_r14(curve_ptr_after_scale);
                    speed_4 = fx_ldw((uint16_t)(car + FXREC_SPEED));
                    speed_remaining = speed_4;
                    if (!(speed_4 & 0x8000)) {
                    } else {
                        speed_remaining = (uint16_t)((uint16_t)~speed_remaining + 1);
                    }
                    speed_span = (uint16_t)(speed_remaining - prev_curve_speed);
                    curve_entry_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(speed_span + speed_span) * (int16_t)curve_scale) >> 16);
                    if ((uint16_t)(curve_entry_index - 0x000F) & 0x8000) {
                    } else {
                        curve_entry_index = 0x000F;
                    }
                    y_row = curve_entry_index;
                    param_curve_a = (uint16_t)(curve_ptr_after_scale + curve_entry_index);
                    fx_r14(param_curve_a);
                    thrust_scale = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
                    param_thrust = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(param_thrust + param_thrust) * (int16_t)thrust_scale) >> 16);
                } else {
                    y_row = 0x000F;
                }
            }
            fx_stw((uint16_t)(car + FXPART_3C), param_thrust);
            fxpart_50 = fx_ldw((uint16_t)(car + FXPART_50));
            last_low = (uint16_t)(uint32_t)((int32_t)(int16_t)y_row * (int16_t)0x0040);
            fx_sbk((uint16_t)(fx_div2(fx_div2((uint16_t)(last_low - fxpart_50))) + fxpart_50));
            if ((0x0800 & sides) == 0) {
            } else {
                fx_push(stack, sides);
                FxSumsPushLocal(0, 0, 0x0258, stack);
                { FxSumsRotateLocalResult r_ = FxSumsRotateLocal(); old_world = r_.old_world; y_row = r_.y_row; };
                last_low = FxHitAddScaled(0, 0, 0, old_world, 0x7FFF, stack);
                sides = fx_pull(stack);
            }
            param_wheel_48 = 0;
            if ((0x0410 & sides) != 0) {
            } else {
                speed_5 = fx_ldw((uint16_t)(car + FXREC_SPEED));
                speed_doubled = (uint16_t)(speed_5 + speed_5);
                speed_quadrupled = (uint16_t)(speed_doubled + speed_doubled);
                speed_octupled = (uint16_t)(speed_quadrupled + speed_quadrupled);
                signed_speed_octupled = speed_octupled;
                if (!(speed_octupled & 0x8000)) {
                } else {
                    signed_speed_octupled = (uint16_t)((uint16_t)~signed_speed_octupled + 1);
                }
                param_turn_grip = fx_ldw(FX_PARAM_TURN_GRIP);
                grip_value = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(signed_speed_octupled + signed_speed_octupled) * (int16_t)param_turn_grip) >> 16);
                if (!((uint16_t)(grip_value - 0x0800) & 0x8000)) {
                } else {
                    grip_value = 0x0800;
                }
                param_wheel_48 = grip_value;
            }
            if ((0x0040 & sides) == 0) {
            } else {
                speed_6 = fx_ldw((uint16_t)(car + FXREC_SPEED));
                if (speed_6 & 0x8000) {
                } else {
                    if (!((uint16_t)((uint16_t)(0 - speed_6) + 0x00C8) & 0x8000)) {
                    } else {
                        flags40_9 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
                        fx_sbk((0x0800 | flags40_9));
                    }
                    param_wheel_48 = fx_ldw(FX_PARAM_WHEEL_48);
                }
            }
            fx_stw((uint16_t)(car + 0x0048), param_wheel_48);
            fx_stw((uint16_t)(car + FXREC_SIDES), sides);
            fx_stw((uint16_t)(car + 0x002C), 0x0040);
            { FxCarMovePartsResult r_ = FxCarMoveParts(last_low, y_row, car, stack); last_part_velocity_z = r_.last_part_velocity_z; world_z_partial_sum = r_.world_z_partial_sum; };
            { FxPartsUpdateResult r_ = FxPartsUpdate(last_part_velocity_z, world_z_partial_sum, pose_aux_2, car, stack, param_curve_a); up_ = r_.up; last_low_2 = r_.last_low; y_row_2 = r_.y_row; model_record_out = r_.model_record_out; car_out_2 = r_.car_out; last_part = r_.last_part; rom_at_out_4 = r_.rom_at_out; };
            if (up_) {                       /* it returned past us */
                return (FxCarHandlerResult){last_low_2, y_row_2, model_record_out, car_out_2, last_part, rom_at_out_4, up_ == 1 ? 0 : up_ - 1};
            }
            FxRecordHoldY(car_out_2);
            sides_2 = fx_ldw((uint16_t)(car_out_2 + FXREC_SIDES));
            if ((0x0400 & sides_2) == 0) {
            } else {
                param_jump = fx_ldw(FX_PARAM_JUMP);
                if (param_jump == 0) {
                } else {
                    record_curve_index = fx_ldw((uint16_t)(car_out_2 + 0x004E));
                    if (!((uint16_t)(record_curve_index - 2) & 0x8000)) {
                    } else {
                        record_angle_26 = fx_ldw((uint16_t)(car_out_2 + 0x0026));
                        if ((uint16_t)(record_angle_26 + 0x1555) & 0x8000) {
                        } else {
                            force_233c = fx_ldw(0x233C);
                            fx_sbk((uint16_t)(force_233c - param_jump));
                        }
                    }
                }
            }
            { FxCarCollideResult r_ = FxCarCollide(last_low_2, y_row_2, model_record_out, car_out_2, stack, rom_at_out_4); up_ = r_.up; share_before = r_.share_before; next_entry_end_ptr_3 = r_.next_entry_end_ptr; model_record_out_2 = r_.model_record_out; caller_scratch_back = r_.caller_scratch_back; record_ptr = r_.record_ptr; rom_at_out_5 = r_.rom_at_out; };
            if (up_) {                       /* it returned past us */
                return (FxCarHandlerResult){share_before, next_entry_end_ptr_3, model_record_out_2, caller_scratch_back, record_ptr, rom_at_out_5, up_ == 1 ? 0 : up_ - 1};
            }
            { FxRecordAlignmentResult r_ = FxRecordAlignment(model_record_out_2, caller_scratch_back, record_ptr, stack); last_product_hi = r_.last_product_hi; last_product_lo = r_.last_product_lo; low_word = r_.low_word; normalize_work_reg_out = r_.normalize_work_reg_out; };
            { FxCarHitResult r_ = FxCarHit(last_product_hi, last_product_lo, low_word, caller_scratch_back, normalize_work_reg_out, stack); spread_out = r_.spread_out; car_y_out = r_.car_y_out; car_z_out = r_.car_z_out; other_car_out = r_.other_car_out; };
            direction_2 = fx_ldw((uint16_t)(caller_scratch_back + FXREC_DIRECTION));
            if ((0x0080 & direction_2) == 0) {
                { FxCarHandlerClampResult tr_ = FxCarHandlerClamp(spread_out, car_y_out, car_z_out, caller_scratch_back, other_car_out, stack, rom_at_out_5); return (FxCarHandlerResult){tr_.course_x_part, tr_.next_entry_end_ptr, tr_.pose_aux_out, tr_.car_out, tr_.pose_matrix_ptr_out, tr_.rom_at_out, tr_.up ? tr_.up == 1 ? 0 : tr_.up - 1 : 0}; }
            }
            segment = fx_ldw((uint16_t)(caller_scratch_back + FXREC_SEGMENT));
            if (segment != 0) {
                record_state_flags = fx_ldw((uint16_t)(caller_scratch_back + 0x008A));
                if ((0x0100 & record_state_flags) != 0) {
                    { FxRecordHeadingAheadResult r_ = FxRecordHeadingAhead(spread_out, car_y_out, car_z_out, caller_scratch_back, other_car_out, stack); heading_angle = r_.heading_angle; spread_out = r_.angle_sign_xor; car_y_out = r_.swap_mark; car_z_out = r_.nearest_number_out; other_car_out = r_.nearest_dist_x_out; rom_at_out_5 = r_.rom_at; };
                    pitch_2 = fx_ldw((uint16_t)(caller_scratch_back + FXREC_HEADING));
                    fx_sbk((uint16_t)(fx_div2(fx_div2((uint16_t)(heading_angle - pitch_2))) + pitch_2));
                    fx_stw(FX_SPIN, 0);
                    fx_stw((uint16_t)(caller_scratch_back + 0x0034), 0);
                    if (fx_div2((uint16_t)(int16_t)(int8_t)(uint16_t)((uint16_t)(pitch_2 - heading_angle) >> 8)) != 0) {
                        { FxCarHandlerClampResult tr_ = FxCarHandlerClamp(spread_out, car_y_out, car_z_out, caller_scratch_back, other_car_out, stack, rom_at_out_5); return (FxCarHandlerResult){tr_.course_x_part, tr_.next_entry_end_ptr, tr_.pose_aux_out, tr_.car_out, tr_.pose_matrix_ptr_out, tr_.rom_at_out, tr_.up ? tr_.up == 1 ? 0 : tr_.up - 1 : 0}; }
                    } else {
                    }
                } else {
                }
            } else {
            }
            direction_3 = fx_ldw((uint16_t)(caller_scratch_back + FXREC_DIRECTION));
            fx_sbk((uint16_t)(direction_3 & ~0x0080));
            { FxCarHandlerClampResult tr_ = FxCarHandlerClamp(spread_out, car_y_out, car_z_out, caller_scratch_back, other_car_out, stack, rom_at_out_5); return (FxCarHandlerResult){tr_.course_x_part, tr_.next_entry_end_ptr, tr_.pose_aux_out, tr_.car_out, tr_.pose_matrix_ptr_out, tr_.rom_at_out, tr_.up ? tr_.up == 1 ? 0 : tr_.up - 1 : 0}; }
        } else {
            { FxLapSegmentResult r_ = FxLapSegment(car, rom_at_out); segment_out = r_.segment_out; rom_at_out_2 = r_.rom_at_out; };
            counted = fx_ldw((uint16_t)(car + FXREC_COUNTED));
            { FxCourseNextDistanceResult r_ = FxCourseNextDistance(counted, segment_out, car, stack, rom_at_out_2); distance = r_.distance; x_part = r_.x_part; next_entry_end_ptr_2 = r_.next_entry_end_ptr; rom_at_out_3 = r_.rom_at_out; };
            fx_stw((uint16_t)(car + FXREC_TO_GO), distance);
            fx_unspill(stack);
            return (FxCarHandlerResult){x_part, next_entry_end_ptr_2, pose_aux, car, pose_matrix_ptr, rom_at_out_3, 0};
        }
    } else {
        { FxCarPoseResult r_ = FxCarPose(car, stack); last_change_low_3 = r_.last_change_low; last_change_high_3 = r_.last_change_high; pose_aux_3 = r_.pose_aux; last_part_ptr = r_.last_part_ptr; rom_at_3 = r_.rom_at; };
        fx_unspill(stack);
        return (FxCarHandlerResult){last_change_low_3, last_change_high_3, pose_aux_3, car, last_part_ptr, rom_at_3, 0};
    }
}


/*
 * @routine 04:B6C1 FxCarHandlerClamp
 * WHAT IT DOES: FxCarHandler's clamp of its three sums ($2336, $233E,
 * $233C) to -$0BB8..$0BB8, entered on its own, then the rest of the
 * handler (its spin and flip steps, FxCarFollowCourse, the lap and
 * course distance). The game reaches it only by the handler's branches;
 * a unit start with each sum past its limit runs the clamps no play has
 * reached so far, so the C is judged against the original.
 * HOW: R1 = $F448 and R2 = $0BB8; each sum's sign picks which limit it
 * is compared with (CMP, BMI or BPL) and SBK puts the limit back; then
 * as FxCarHandler after them (04:B718).
 * Entry: R8 = the car; R10 = stack, the return address the word below
 * it.
 * Exit: as FxCarHandler's.
 */
FxCarHandlerClampResult FxCarHandlerClamp(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t car, uint16_t pose_matrix_ptr, uint16_t *stack, uint16_t rom_at)
{
    uint16_t roll;
    uint16_t spin;
    uint16_t count;
    uint16_t sides;
    uint16_t speed;
    uint16_t sum_a;
    uint16_t sum_b;
    uint16_t sum_c;
    uint16_t vel_x;
    uint16_t vel_y;
    uint16_t vel_z;
    uint16_t roll_2;
    uint16_t x_part;
    uint16_t counted;
    uint16_t flags40;
    uint16_t vel_ptr;
    uint16_t distance;
    uint16_t fxcar_64;
    uint16_t vel_word;
    uint16_t accel_ptr;
    uint16_t flags40_2;
    uint16_t fxpart_50;
    uint16_t speed_abs;
    uint16_t accel_word;
    uint16_t old_word_52;
    uint16_t segment_out;
    uint16_t turn_rate_a;
    uint16_t turn_rate_b;
    uint16_t turn_rate_c;
    uint16_t car_flags_2c;
    uint16_t clamped_spin;
    uint16_t rom_at_out_2;
    uint16_t rom_at_out_3;
    uint32_t speed_prod_x;
    uint32_t speed_prod_y;
    uint32_t speed_prod_z;
    uint16_t vel_x_sample;
    uint16_t vel_y_sample;
    uint16_t vel_z_sample;
    uint16_t angle_word_26;
    uint16_t clamped_sum_a;
    uint16_t clamped_sum_b;
    uint16_t clamped_sum_c;
    uint16_t count_word_44;
    uint16_t turn_rate_ptr;
    uint16_t vel_x_limited;
    uint16_t vel_y_limited;
    uint16_t vel_z_limited;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t turn_rate_ptr_b;
    uint16_t turn_rate_ptr_c;
    uint16_t vel_x_before_drag;
    uint16_t vel_z_before_drag;
    uint16_t turn_rate_a_limited;
    uint16_t turn_rate_b_limited;
    uint16_t turn_rate_c_limited;
    uint16_t next_entry_end_ptr_2;

    sum_a = fx_ldw(0x2336);
    clamped_sum_a = sum_a;
    if (sum_a & 0x8000) {
        if (!((uint16_t)(clamped_sum_a + 0x0BB8) & 0x8000)) {
        } else {
            clamped_sum_a = 0xF448;
        }
    } else {
        if ((uint16_t)(clamped_sum_a - 0x0BB8) & 0x8000) {
        } else {
            clamped_sum_a = 0x0BB8;
        }
    }
    fx_sbk(clamped_sum_a);
    sum_b = fx_ldw(0x233E);
    clamped_sum_b = sum_b;
    if (sum_b & 0x8000) {
        if (!((uint16_t)(clamped_sum_b + 0x0BB8) & 0x8000)) {
        } else {
            clamped_sum_b = 0xF448;
        }
    } else {
        if ((uint16_t)(clamped_sum_b - 0x0BB8) & 0x8000) {
        } else {
            clamped_sum_b = 0x0BB8;
        }
    }
    fx_sbk(clamped_sum_b);
    sum_c = fx_ldw(0x233C);
    clamped_sum_c = sum_c;
    if (sum_c & 0x8000) {
        if (!((uint16_t)(clamped_sum_c + 0x0BB8) & 0x8000)) {
        } else {
            clamped_sum_c = 0xF448;
        }
    } else {
        if ((uint16_t)(clamped_sum_c - 0x0BB8) & 0x8000) {
        } else {
            clamped_sum_c = 0x0BB8;
        }
    }
    fx_sbk(clamped_sum_c);
    spin = fx_ldw(FX_SPIN);
    clamped_spin = spin;
    if (spin & 0x8000) {
        if (!((uint16_t)(clamped_spin + 0x0BB8) & 0x8000)) {
        } else {
            clamped_spin = 0xF448;
        }
    } else {
        if ((uint16_t)(clamped_spin - 0x0BB8) & 0x8000) {
        } else {
            clamped_spin = 0x0BB8;
        }
    }
    fx_sbk(clamped_spin);
    roll = fx_ldw((uint16_t)(car + FXREC_ROLL));
    if (roll & 0x8000) {
        if (!((uint16_t)(roll + 0x2000) & 0x8000)) {
        } else {
            fx_sbk(0xE000);
            fx_stw((uint16_t)(car + 0x0038), 0);
        }
    } else {
        if ((uint16_t)(roll - 0x2000) & 0x8000) {
        } else {
            fx_sbk(0x2000);
            fx_stw((uint16_t)(car + 0x0038), 0);
        }
    }
    flags40 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
    if ((0 & flags40) != 0) {
        { FxCarHandlerFlipResult tr_ = FxCarHandlerFlip(pose_aux, car, pose_matrix_ptr, stack, rom_at); return (FxCarHandlerClampResult){tr_.distance_x_part_out, tr_.next_entry_ptr_out, tr_.pose_aux_out, tr_.car_out, tr_.pose_matrix_ptr_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
    angle_word_26 = fx_ldw((uint16_t)(car + 0x0026));
    if (angle_word_26 & 0x8000) {
        if (!((uint16_t)(angle_word_26 + 0x2000) & 0x8000)) {
        } else {
            fx_sbk(0xE000);
            fx_stw((uint16_t)(car + FXPART_WORLD), 0);
        }
    } else {
        if ((uint16_t)(angle_word_26 - 0x2000) & 0x8000) {
        } else {
            fx_sbk(0x2000);
            fx_stw((uint16_t)(car + FXPART_WORLD), 0);
        }
    }
    flags40_2 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
    fx_sbk((uint16_t)(flags40_2 & ~0));
    speed = fx_ldw((uint16_t)(car + FXREC_SPEED));
    speed_abs = speed;
    if (!(speed & 0x8000)) {
    } else {
        speed_abs = (uint16_t)((uint16_t)~speed_abs + 1);
    }
    if ((uint16_t)(speed_abs - 0x0258) & 0x8000) {
    } else {
        fx_stw(FX_HIT, 0);
        fx_stw(0x2338, 0);
        vel_x_before_drag = fx_ldw((uint16_t)(car + 0x002E));
        fx_sbk((uint16_t)(vel_x_before_drag - fx_div2(fx_div2(fx_div2(vel_x_before_drag)))));
        vel_z_before_drag = fx_ldw((uint16_t)(car + 0x0032));
        fx_sbk((uint16_t)(vel_z_before_drag - fx_div2(fx_div2(fx_div2(vel_z_before_drag)))));
    }
    accel_ptr = 0x2334;
    vel_ptr = (uint16_t)(car + 0x002E);
    count = 6;
    for (;;) {
        accel_word = fx_ldw(accel_ptr);
        accel_ptr = (uint16_t)(accel_ptr + 2);
        vel_word = fx_ldw(vel_ptr);
        fx_stw(vel_ptr, (uint16_t)(accel_word + vel_word));
        vel_ptr = (uint16_t)(vel_ptr + 2);
        count = (uint16_t)(count - 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fxcar_64 = fx_ldw((uint16_t)(car + FXCAR_64));
    if ((8 & fxcar_64) == 0) {
        sides = fx_ldw((uint16_t)(car + FXREC_SIDES));
        if ((0x0430 & sides) == 0) {
            car_flags_2c = fx_ldw((uint16_t)(car + 0x002C));
            if ((0x0400 & car_flags_2c) == 0) {
                vel_x_sample = fx_ldw((uint16_t)(car + 0x002E));
                vel_x_limited = vel_x_sample;
                if (vel_x_sample & 0x8000) {
                    if ((uint16_t)(vel_x_limited + 0x00C8) & 0x8000) {
                    } else {
                        vel_x_limited = 0;
                    }
                } else {
                    if (!((uint16_t)(vel_x_limited - 0x00C8) & 0x8000)) {
                    } else {
                        vel_x_limited = 0;
                    }
                }
                fx_sbk(vel_x_limited);
                vel_z_sample = fx_ldw((uint16_t)(car + 0x0032));
                vel_z_limited = vel_z_sample;
                if (vel_z_sample & 0x8000) {
                    if ((uint16_t)(vel_z_limited + 0x00C8) & 0x8000) {
                    } else {
                        vel_z_limited = 0;
                    }
                } else {
                    if (!((uint16_t)(vel_z_limited - 0x00C8) & 0x8000)) {
                    } else {
                        vel_z_limited = 0;
                    }
                }
                fx_sbk(vel_z_limited);
                vel_y_sample = fx_ldw((uint16_t)(car + 0x0030));
                vel_y_limited = vel_y_sample;
                if (vel_y_sample & 0x8000) {
                    if ((uint16_t)(vel_y_limited + 0x0032) & 0x8000) {
                    } else {
                        vel_y_limited = 0;
                    }
                } else {
                    if (!((uint16_t)(vel_y_limited - 0x0032) & 0x8000)) {
                    } else {
                        vel_y_limited = 0;
                    }
                }
                fx_sbk(vel_y_limited);
            } else {
            }
        } else {
        }
        turn_rate_ptr = (uint16_t)(car + 0x0034);
        turn_rate_a = fx_ldw(turn_rate_ptr);
        turn_rate_ptr_b = (uint16_t)(turn_rate_ptr + 2);
        turn_rate_a_limited = turn_rate_a;
        if (turn_rate_a & 0x8000) {
            if ((uint16_t)(turn_rate_a_limited + 0x0040) & 0x8000) {
            } else {
                turn_rate_a_limited = 0;
            }
        } else {
            if (!((uint16_t)(turn_rate_a_limited - 0x0040) & 0x8000)) {
            } else {
                turn_rate_a_limited = 0;
            }
        }
        fx_sbk(turn_rate_a_limited);
        turn_rate_b = fx_ldw(turn_rate_ptr_b);
        turn_rate_ptr_c = (uint16_t)(turn_rate_ptr_b + 2);
        turn_rate_b_limited = turn_rate_b;
        if (turn_rate_b & 0x8000) {
            if ((uint16_t)(turn_rate_b_limited + 0x0040) & 0x8000) {
            } else {
                turn_rate_b_limited = 0;
            }
        } else {
            if (!((uint16_t)(turn_rate_b_limited - 0x0040) & 0x8000)) {
            } else {
                turn_rate_b_limited = 0;
            }
        }
        fx_sbk(turn_rate_b_limited);
        turn_rate_c = fx_ldw(turn_rate_ptr_c);
        turn_rate_c_limited = turn_rate_c;
        if (turn_rate_c & 0x8000) {
            if ((uint16_t)(turn_rate_c_limited + 0x0040) & 0x8000) {
            } else {
                turn_rate_c_limited = 0;
            }
        } else {
            if (!((uint16_t)(turn_rate_c_limited - 0x0040) & 0x8000)) {
            } else {
                turn_rate_c_limited = 0;
            }
        }
        fx_sbk(turn_rate_c_limited);
    } else {
    }
    vel_x = fx_ldw((uint16_t)(car + 0x002E));
    vel_y = fx_ldw((uint16_t)(car + 0x0030));
    vel_z = fx_ldw((uint16_t)(car + 0x0032));
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    speed_prod_x = (uint32_t)((int32_t)(int16_t)vel_x * (int16_t)object_matrix_6);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    speed_prod_y = (uint32_t)((int32_t)(int16_t)vel_y * (int16_t)object_matrix_7);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    speed_prod_z = (uint32_t)((int32_t)(int16_t)vel_z * (int16_t)object_matrix_8);
    fx_stw((uint16_t)(car + FXREC_SPEED), fx_div2(fx_div2(fx_div2((uint16_t)((uint16_t)((uint16_t)((uint16_t)(speed_prod_z >> 16) << 1 | (int)(speed_prod_z >> 15 & 1)) + (uint16_t)((uint16_t)(speed_prod_x >> 16) << 1 | (int)(speed_prod_x >> 15 & 1))) + (uint16_t)((uint16_t)(speed_prod_y >> 16) << 1 | (int)(speed_prod_y >> 15 & 1)))))));
    fx_romb(4);
    fxpart_50 = fx_ldw((uint16_t)(car + FXPART_50));
    count_word_44 = fx_ldw((uint16_t)(car + 0x0044));
    if ((uint16_t)(count_word_44 - 4) & 0x8000) {
    } else {
        fxpart_50 = (uint16_t)(fxpart_50 + fxpart_50);
        roll_2 = fx_ldw((uint16_t)(car + FXREC_ROLL));
        fx_stw((uint16_t)(car + FXREC_ROLL), (uint16_t)(roll_2 - fx_div2(fx_div2(roll_2))));
    }
    old_word_52 = fx_ldw((uint16_t)(car + 0x0052));
    fx_stw((uint16_t)(car + 0x0052), (uint16_t)(fx_div2((uint16_t)(fxpart_50 - old_word_52)) + old_word_52));
    { FxLapSegmentResult r_ = FxLapSegment(car, rom_at); segment_out = r_.segment_out; rom_at_out_2 = r_.rom_at_out; };
    counted = fx_ldw((uint16_t)(car + FXREC_COUNTED));
    { FxCourseNextDistanceResult r_ = FxCourseNextDistance(counted, segment_out, car, stack, rom_at_out_2); distance = r_.distance; x_part = r_.x_part; next_entry_end_ptr_2 = r_.next_entry_end_ptr; rom_at_out_3 = r_.rom_at_out; };
    fx_stw((uint16_t)(car + FXREC_TO_GO), distance);
    fx_unspill(stack);
    return (FxCarHandlerClampResult){x_part, next_entry_end_ptr_2, pose_aux, car, pose_matrix_ptr, rom_at_out_3, 1};
}


/*
 * @routine 04:B799 FxCarHandlerFlip
 * WHAT IT DOES: FxCarHandler's flip step: when the car's angle +$26 is
 * $4000 or more either way, +$40 is ORed with 0, +$76 is set to $28
 * when it is 0, and the spin $233A is negated; then the rest of the
 * handler. The game never runs it: the test before it ANDs +$40 with 0
 * (IWT R0, #0), so the BNE at 04:B769 never branches here. A unit start
 * runs it so the C is judged against the original.
 * HOW: LDW, MOVES, BPL with NOT, INC for the absolute angle; CMP with
 * $4000; then as FxCarHandler from 04:B7D4.
 * Entry: R8 = the car; R10 = stack, the return address the word below
 * it.
 * Exit: as FxCarHandler's.
 */
FxCarHandlerFlipResult FxCarHandlerFlip(uint16_t pose_aux, uint16_t car, uint16_t pose_matrix_ptr, uint16_t *stack, uint16_t rom_at)
{
    uint16_t roll;
    uint16_t spin;
    uint16_t count;
    uint16_t sides;
    uint16_t speed;
    uint16_t vel_x;
    uint16_t vel_y;
    uint16_t vel_z;
    uint16_t x_part;
    uint16_t counted;
    uint16_t flags40;
    uint16_t vel_ptr;
    uint16_t distance;
    uint16_t fxcar_64;
    uint16_t vel_word;
    uint16_t flags40_2;
    uint16_t fxpart_50;
    uint16_t speed_abs;
    uint16_t vel_x_raw;
    uint16_t vel_y_raw;
    uint16_t vel_z_raw;
    uint16_t flip_timer;
    uint16_t spin_ptr_x;
    uint16_t spin_ptr_y;
    uint16_t spin_ptr_z;
    uint16_t spin_x_raw;
    uint16_t spin_y_raw;
    uint16_t spin_z_raw;
    uint16_t tilt_angle;
    uint16_t impulse_ptr;
    uint16_t segment_out;
    uint16_t car_state_2c;
    uint16_t impulse_word;
    uint16_t rom_at_out_2;
    uint16_t rom_at_out_3;
    uint32_t speed_prod_x;
    uint32_t speed_prod_y;
    uint32_t speed_prod_z;
    uint16_t contact_count;
    uint16_t vel_x_limited;
    uint16_t vel_y_limited;
    uint16_t vel_z_limited;
    uint16_t spin_x_limited;
    uint16_t spin_y_limited;
    uint16_t spin_z_limited;
    uint16_t tilt_angle_abs;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t vel_x_before_damp;
    uint16_t vel_z_before_damp;
    uint16_t next_entry_end_ptr;
    uint16_t old_smoothed_value;

    tilt_angle = fx_ldw((uint16_t)(car + 0x0026));
    tilt_angle_abs = tilt_angle;
    if (!(tilt_angle & 0x8000)) {
    } else {
        tilt_angle_abs = (uint16_t)((uint16_t)~tilt_angle_abs + 1);
    }
    if ((uint16_t)(tilt_angle_abs - 0x4000) & 0x8000) {
        flags40 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
        fx_sbk((uint16_t)(flags40 & ~0));
    } else {
        flags40_2 = fx_ldw((uint16_t)(car + FXREC_FLAGS40));
        fx_sbk(flags40_2);
        flip_timer = fx_ldw((uint16_t)(car + 0x0076));
        if (flip_timer != 0) {
        } else {
            fx_sbk(0x0028);
        }
        spin = fx_ldw(FX_SPIN);
        fx_sbk((uint16_t)((uint16_t)~spin + 1));
    }
    speed = fx_ldw((uint16_t)(car + FXREC_SPEED));
    speed_abs = speed;
    if (!(speed & 0x8000)) {
    } else {
        speed_abs = (uint16_t)((uint16_t)~speed_abs + 1);
    }
    if ((uint16_t)(speed_abs - 0x0258) & 0x8000) {
    } else {
        fx_stw(FX_HIT, 0);
        fx_stw(0x2338, 0);
        vel_x_before_damp = fx_ldw((uint16_t)(car + 0x002E));
        fx_sbk((uint16_t)(vel_x_before_damp - fx_div2(fx_div2(fx_div2(vel_x_before_damp)))));
        vel_z_before_damp = fx_ldw((uint16_t)(car + 0x0032));
        fx_sbk((uint16_t)(vel_z_before_damp - fx_div2(fx_div2(fx_div2(vel_z_before_damp)))));
    }
    impulse_ptr = 0x2334;
    vel_ptr = (uint16_t)(car + 0x002E);
    count = 6;
    for (;;) {
        impulse_word = fx_ldw(impulse_ptr);
        impulse_ptr = (uint16_t)(impulse_ptr + 2);
        vel_word = fx_ldw(vel_ptr);
        fx_stw(vel_ptr, (uint16_t)(impulse_word + vel_word));
        vel_ptr = (uint16_t)(vel_ptr + 2);
        count = (uint16_t)(count - 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fxcar_64 = fx_ldw((uint16_t)(car + FXCAR_64));
    if ((8 & fxcar_64) == 0) {
        sides = fx_ldw((uint16_t)(car + FXREC_SIDES));
        if ((0x0430 & sides) == 0) {
            car_state_2c = fx_ldw((uint16_t)(car + 0x002C));
            if ((0x0400 & car_state_2c) == 0) {
                vel_x_raw = fx_ldw((uint16_t)(car + 0x002E));
                vel_x_limited = vel_x_raw;
                if (vel_x_raw & 0x8000) {
                    if ((uint16_t)(vel_x_limited + 0x00C8) & 0x8000) {
                    } else {
                        vel_x_limited = 0;
                    }
                } else {
                    if (!((uint16_t)(vel_x_limited - 0x00C8) & 0x8000)) {
                    } else {
                        vel_x_limited = 0;
                    }
                }
                fx_sbk(vel_x_limited);
                vel_z_raw = fx_ldw((uint16_t)(car + 0x0032));
                vel_z_limited = vel_z_raw;
                if (vel_z_raw & 0x8000) {
                    if ((uint16_t)(vel_z_limited + 0x00C8) & 0x8000) {
                    } else {
                        vel_z_limited = 0;
                    }
                } else {
                    if (!((uint16_t)(vel_z_limited - 0x00C8) & 0x8000)) {
                    } else {
                        vel_z_limited = 0;
                    }
                }
                fx_sbk(vel_z_limited);
                vel_y_raw = fx_ldw((uint16_t)(car + 0x0030));
                vel_y_limited = vel_y_raw;
                if (vel_y_raw & 0x8000) {
                    if ((uint16_t)(vel_y_limited + 0x0032) & 0x8000) {
                    } else {
                        vel_y_limited = 0;
                    }
                } else {
                    if (!((uint16_t)(vel_y_limited - 0x0032) & 0x8000)) {
                    } else {
                        vel_y_limited = 0;
                    }
                }
                fx_sbk(vel_y_limited);
            } else {
            }
        } else {
        }
        spin_ptr_x = (uint16_t)(car + 0x0034);
        spin_x_raw = fx_ldw(spin_ptr_x);
        spin_ptr_y = (uint16_t)(spin_ptr_x + 2);
        spin_x_limited = spin_x_raw;
        if (spin_x_raw & 0x8000) {
            if ((uint16_t)(spin_x_limited + 0x0040) & 0x8000) {
            } else {
                spin_x_limited = 0;
            }
        } else {
            if (!((uint16_t)(spin_x_limited - 0x0040) & 0x8000)) {
            } else {
                spin_x_limited = 0;
            }
        }
        fx_sbk(spin_x_limited);
        spin_y_raw = fx_ldw(spin_ptr_y);
        spin_ptr_z = (uint16_t)(spin_ptr_y + 2);
        spin_y_limited = spin_y_raw;
        if (spin_y_raw & 0x8000) {
            if ((uint16_t)(spin_y_limited + 0x0040) & 0x8000) {
            } else {
                spin_y_limited = 0;
            }
        } else {
            if (!((uint16_t)(spin_y_limited - 0x0040) & 0x8000)) {
            } else {
                spin_y_limited = 0;
            }
        }
        fx_sbk(spin_y_limited);
        spin_z_raw = fx_ldw(spin_ptr_z);
        spin_z_limited = spin_z_raw;
        if (spin_z_raw & 0x8000) {
            if ((uint16_t)(spin_z_limited + 0x0040) & 0x8000) {
            } else {
                spin_z_limited = 0;
            }
        } else {
            if (!((uint16_t)(spin_z_limited - 0x0040) & 0x8000)) {
            } else {
                spin_z_limited = 0;
            }
        }
        fx_sbk(spin_z_limited);
    } else {
    }
    vel_x = fx_ldw((uint16_t)(car + 0x002E));
    vel_y = fx_ldw((uint16_t)(car + 0x0030));
    vel_z = fx_ldw((uint16_t)(car + 0x0032));
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    speed_prod_x = (uint32_t)((int32_t)(int16_t)vel_x * (int16_t)object_matrix_6);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    speed_prod_y = (uint32_t)((int32_t)(int16_t)vel_y * (int16_t)object_matrix_7);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    speed_prod_z = (uint32_t)((int32_t)(int16_t)vel_z * (int16_t)object_matrix_8);
    fx_stw((uint16_t)(car + FXREC_SPEED), fx_div2(fx_div2(fx_div2((uint16_t)((uint16_t)((uint16_t)((uint16_t)(speed_prod_z >> 16) << 1 | (int)(speed_prod_z >> 15 & 1)) + (uint16_t)((uint16_t)(speed_prod_x >> 16) << 1 | (int)(speed_prod_x >> 15 & 1))) + (uint16_t)((uint16_t)(speed_prod_y >> 16) << 1 | (int)(speed_prod_y >> 15 & 1)))))));
    fx_romb(4);
    fxpart_50 = fx_ldw((uint16_t)(car + FXPART_50));
    contact_count = fx_ldw((uint16_t)(car + 0x0044));
    if ((uint16_t)(contact_count - 4) & 0x8000) {
    } else {
        fxpart_50 = (uint16_t)(fxpart_50 + fxpart_50);
        roll = fx_ldw((uint16_t)(car + FXREC_ROLL));
        fx_stw((uint16_t)(car + FXREC_ROLL), (uint16_t)(roll - fx_div2(fx_div2(roll))));
    }
    old_smoothed_value = fx_ldw((uint16_t)(car + 0x0052));
    fx_stw((uint16_t)(car + 0x0052), (uint16_t)(fx_div2((uint16_t)(fxpart_50 - old_smoothed_value)) + old_smoothed_value));
    { FxLapSegmentResult r_ = FxLapSegment(car, rom_at); segment_out = r_.segment_out; rom_at_out_2 = r_.rom_at_out; };
    counted = fx_ldw((uint16_t)(car + FXREC_COUNTED));
    { FxCourseNextDistanceResult r_ = FxCourseNextDistance(counted, segment_out, car, stack, rom_at_out_2); distance = r_.distance; x_part = r_.x_part; next_entry_end_ptr = r_.next_entry_end_ptr; rom_at_out_3 = r_.rom_at_out; };
    fx_stw((uint16_t)(car + FXREC_TO_GO), distance);
    fx_unspill(stack);
    return (FxCarHandlerFlipResult){x_part, next_entry_end_ptr, pose_aux, car, pose_matrix_ptr, rom_at_out_3, 1};
}


/*
 * @routine 04:BD20 FxCarCollide
 * WHAT IT DOES: Collides car R8 with each record in its touch list
 * (+$18): another car's model (FxCollideModel with the roles swapped),
 * then if any slot holds a touch (FxContactsAnyTouch) the pushes are
 * applied (FxPushesApply), the world sum turned back and doubled, and
 * the car pushed at its place (FxHitAddUnit, FxPushAt4) and the other
 * record too (FxCollidePush, FxPushApply). Records whose flags rule
 * them out (+$1C bit 6, +$1A bit 1, +$16 bit 5) are skipped.
 * HOW: Pushes R11 and each entry; $247E, $239C, $2378 and $26F6
 * cleared first; returns by pulling R15.
 * Entry: R8 = the car; R10 = stack; R11 = return address.
 * Exit: as the last collision leaves them.
 */
FxCarCollideResult FxCarCollide(uint16_t place_x, uint16_t place_y, uint16_t model_record, uint16_t car, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    int run_lBDBD = 0;
    uint16_t hit;
    uint16_t list;
    uint16_t mode;
    uint16_t next;
    uint16_t hit_on;
    uint16_t spread;
    uint16_t flags16;
    uint16_t flags1c;
    uint16_t point_x;
    uint16_t point_y;
    uint16_t point_z;
    uint16_t fxcar_64;
    uint16_t any_touch;
    uint16_t flags16_2;
    uint16_t flags1c_2;
    uint16_t hit_scale;
    uint16_t list_face;
    uint16_t place_x_2;
    uint16_t place_y_2;
    uint16_t place_y_3;
    uint16_t world_sum;
    uint16_t fxcar_64_2;
    uint16_t push_sum_x;
    uint16_t push_sum_y;
    uint16_t spread_raw;
    uint16_t car_word_2c;
    uint16_t car_word_5e;
    uint16_t drive_level;
    uint16_t hit_point_x;
    uint16_t hit_point_z;
    uint16_t push_amount;
    uint16_t record_back;
    uint16_t world_sum_2;
    uint16_t world_sum_3;
    uint32_t out_x_y_prod;
    uint32_t out_x_z_prod;
    uint32_t out_y_x_prod;
    uint32_t out_y_y_prod;
    uint32_t out_y_z_prod;
    uint32_t out_z_x_prod;
    uint32_t out_z_y_prod;
    uint32_t out_z_z_prod;
    uint16_t record_ptr_2;
    uint16_t car_hit_total;
    uint16_t contact_words;
    uint16_t model_word_5e;
    uint16_t neg_world_sum;
    uint16_t neg_push_sum_y;
    uint16_t share_before_2;
    uint16_t applied_skipped;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t push_amount_out;
    uint32_t out_x_scale_prod;
    uint16_t push_sum_y_again;
    uint16_t push_sum_y_final;
    uint16_t model_record_out_2;
    uint16_t hit_point_x_quarter;
    uint16_t hit_point_z_quarter;
    uint16_t caller_scratch_back_2;

    fx_spill(stack);
    fx_stw(FX_PUSH_TURN, 0);
    fx_stw(FX_HIT_DEPTH, 0);
    fx_stw(FX_PUSH_AMOUNT, 0);
    fx_stw(0x26F6, 0);
    hit_on = fx_ldw(FX_HIT_ON);
    if (hit_on != 0) {
    } else {
        fx_stw((uint16_t)(car + 0x0094), 0);
    }
    record_ptr_2 = car;
    list = fx_ldw((uint16_t)(car + FXREC_LIST));
    contact_words = list;
    if (list != 0) {
        flags1c = fx_ldw((uint16_t)(car + FXREC_FLAGS1C));
        if ((flags1c & 0x0040) == 0) {
            for (;;) {
                model_record = fx_ldw((uint16_t)(contact_words + 0x000C));
                fx_stw(FX_COLLIDE_MODEL, model_record);
                mode = fx_ldw((uint16_t)(model_record + FXREC_MODE));
                if ((2 & mode) == 0) {
                    flags16 = fx_ldw((uint16_t)(model_record + FXREC_FLAGS16));
                    if ((0x0020 & flags16) == 0) {
                        if ((2 & flags16) == 0) {
                            run_lBDBD = 1;
                        } else {
                            drive_level = fx_ldw(FX_DRIVE_LEVEL);
                            spread_raw = (uint16_t)((uint16_t)((uint16_t)(0 - drive_level) - drive_level) + 0x0014);
                            spread = spread_raw;
                            if (!(spread_raw & 0x8000)) {
                            } else {
                                spread = 0;
                            }
                            fx_stw((uint16_t)(car + FXREC_SPREAD), spread);
                            car_word_5e = fx_ldw((uint16_t)(car + 0x005E));
                            model_word_5e = fx_ldw((uint16_t)(model_record + 0x005E));
                            if ((uint16_t)(car_word_5e + model_word_5e) == 0) {
                                run_lBDBD = 1;
                            } else {
                            }
                        }
                        if (run_lBDBD) {
                            run_lBDBD = 0;
                            fx_push(stack, contact_words);
                            fx_push(stack, car);
                            FxSumsClear();
                            if ((2 & flags16) == 0) {
                            } else {
                                fxcar_64 = fx_ldw((uint16_t)(car + FXCAR_64));
                                if ((fxcar_64 & 0x0010) == 0) {
                                    car_word_2c = fx_ldw((uint16_t)(car + 0x002C));
                                    fx_sbk((car_word_2c | 4));
                                } else {
                                    fxcar_64_2 = fx_ldw((uint16_t)(model_record + FXCAR_64));
                                    if ((fxcar_64_2 & 3) == 0) {
                                    } else {
                                        car_word_2c = fx_ldw((uint16_t)(car + 0x002C));
                                        fx_sbk((car_word_2c | 4));
                                    }
                                }
                            }
                            flags16_2 = fx_ldw((uint16_t)(car + FXREC_FLAGS16));
                            if ((flags16_2 & 0x0010) == 0) {
                                fx_push(stack, contact_words);
                                fx_push(stack, model_record);
                                fx_push(stack, record_ptr_2);
                                { FxCollideModelResult r_ = FxCollideModel(place_x, place_y, record_ptr_2, car, model_record, stack, rom_at); up_ = r_.up; list_face = r_.list_face; place_x = r_.quotient; place_y = r_.place_y_out; model_record_out_2 = r_.model_record_out; caller_scratch_back_2 = r_.caller_scratch_back; record_back = r_.record_back; rom_at = r_.rom_at_out; };
                                if (up_) {                       /* it returned past us */
                                    return (FxCarCollideResult){place_x, place_y, model_record_out_2, caller_scratch_back_2, record_back, rom_at, up_ == 6 ? 0 : up_ - 6};
                                }
                                record_ptr_2 = fx_pull(stack);
                                model_record = fx_pull(stack);
                                fx_unspill(stack);
                                if (list_face != 0) {
                                    { FxContactsAnyTouchResult r_ = FxContactsAnyTouch(place_x); any_touch = r_.any_touch; place_x = r_.depth; };
                                    if (any_touch != 0) {
                                        { FxPushesApplyResult r_ = FxPushesApply(place_x, place_y, model_record, stack, rom_at); place_x_2 = r_.place_x; place_y_2 = r_.place_y; push_amount_out = r_.push_amount_out; rom_at = r_.rom_at_out; };
                                        world_sum = fx_ldw(FX_WORLD_SUM);
                                        neg_world_sum = (uint16_t)((uint16_t)~world_sum + 1);
                                        fx_sbk((uint16_t)(neg_world_sum + neg_world_sum));
                                        fx_ldw(0x2342);
                                        fx_sbk(0);
                                        push_sum_y = fx_ldw(0x2344);
                                        neg_push_sum_y = (uint16_t)((uint16_t)~push_sum_y + 1);
                                        fx_sbk((uint16_t)(neg_push_sum_y + neg_push_sum_y));
                                        FxHitAddUnit(0, 0, 0, place_x_2, place_y_2, push_amount_out, record_ptr_2, stack);
                                        hit_point_x = fx_ldw(0x0020);
                                        hit_point_x_quarter = fx_div2(fx_div2(hit_point_x));
                                        hit_point_z = fx_ldw(0x0024);
                                        hit_point_z_quarter = fx_div2(fx_div2(hit_point_z));
                                        hit_scale = fx_ldw(0x00E4);
                                        out_x_scale_prod = (uint32_t)((int32_t)(int16_t)hit_point_x_quarter * (int16_t)hit_scale);
                                        object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
                                        out_x_y_prod = (uint32_t)((int32_t)(int16_t)0 * (int16_t)object_matrix_1);
                                        object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
                                        out_x_z_prod = (uint32_t)((int32_t)(int16_t)hit_point_z_quarter * (int16_t)object_matrix_2);
                                        fx_stw(0x0020, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(out_x_z_prod >> 16) << 1 | (int)(out_x_z_prod >> 15 & 1)) + (uint16_t)((uint16_t)(out_x_scale_prod >> 16) << 1 | (int)(out_x_scale_prod >> 15 & 1))) + (uint16_t)((uint16_t)(out_x_y_prod >> 16) << 1 | (int)(out_x_y_prod >> 15 & 1))));
                                        object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
                                        out_y_x_prod = (uint32_t)((int32_t)(int16_t)hit_point_x_quarter * (int16_t)object_matrix_3);
                                        object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
                                        out_y_y_prod = (uint32_t)((int32_t)(int16_t)0 * (int16_t)object_matrix_4);
                                        object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
                                        out_y_z_prod = (uint32_t)((int32_t)(int16_t)hit_point_z_quarter * (int16_t)object_matrix_5);
                                        fx_stw(0x0022, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(out_y_z_prod >> 16) << 1 | (int)(out_y_z_prod >> 15 & 1)) + (uint16_t)((uint16_t)(out_y_x_prod >> 16) << 1 | (int)(out_y_x_prod >> 15 & 1))) + (uint16_t)((uint16_t)(out_y_y_prod >> 16) << 1 | (int)(out_y_y_prod >> 15 & 1))));
                                        object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                                        out_z_x_prod = (uint32_t)((int32_t)(int16_t)hit_point_x_quarter * (int16_t)object_matrix_6);
                                        object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                                        out_z_y_prod = (uint32_t)((int32_t)(int16_t)0 * (int16_t)object_matrix_7);
                                        object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                                        out_z_z_prod = (uint32_t)((int32_t)(int16_t)hit_point_z_quarter * (int16_t)object_matrix_8);
                                        fx_stw(0x0024, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(out_z_z_prod >> 16) << 1 | (int)(out_z_z_prod >> 15 & 1)) + (uint16_t)((uint16_t)(out_z_x_prod >> 16) << 1 | (int)(out_z_x_prod >> 15 & 1))) + (uint16_t)((uint16_t)(out_z_y_prod >> 16) << 1 | (int)(out_z_y_prod >> 15 & 1))));
                                        point_x = fx_ldw(0x0020);
                                        point_y = fx_ldw(0x0022);
                                        point_z = fx_ldw(0x0024);
                                        { FxPushAt4Result r_ = FxPushAt4(point_x, point_y, point_z, stack); place_x = r_.place_x; place_y = r_.place_y; model_record = r_.model_record; };
                                    } else {
                                    }
                                } else {
                                }
                            } else {
                                { FxCollidePushResult r_ = FxCollidePush(contact_words, model_record, record_ptr_2, stack); share_before_2 = r_.share_before; place_y_3 = r_.place_y; };
                                fx_stw(FX_PUSH_TURN, 0);
                                fx_ldw(FX_COLLIDE_MODEL);
                                { FxPushApplyResult r_ = FxPushApply(0x23A4, share_before_2, place_y_3, model_record, stack, rom_at); applied_skipped = r_.applied_skipped; place_x = r_.place_x; place_y = r_.place_y; model_record = r_.push_flags_high_out; rom_at = r_.rom_at_out; };
                                if (applied_skipped == 0) {
                                } else {
                                    push_amount = fx_ldw(0x26F6);
                                    if (push_amount & 0x8000) {
                                    } else {
                                        car_hit_total = fx_ldw((uint16_t)(car + 0x0094));
                                        fx_sbk((uint16_t)(car_hit_total + push_amount));
                                    }
                                }
                                world_sum_2 = fx_ldw(FX_WORLD_SUM);
                                fx_sbk((uint16_t)(world_sum_2 + world_sum_2));
                                push_sum_y_again = fx_ldw(0x2344);
                                fx_sbk((uint16_t)(push_sum_y_again + push_sum_y_again));
                                world_sum_3 = fx_ldw(FX_WORLD_SUM);
                                hit = fx_ldw(FX_HIT);
                                fx_sbk((uint16_t)(hit + world_sum_3));
                                push_sum_y_final = fx_ldw(0x2344);
                                push_sum_x = fx_ldw(0x2338);
                                fx_sbk((uint16_t)(push_sum_x + push_sum_y_final));
                                fx_stw(0x2342, 0);
                                fx_stw(0x2342, 0);
                            }
                            car = fx_pull(stack);
                            contact_words = fx_pull(stack);
                        }
                    } else {
                    }
                } else {
                }
                next = fx_ldw((uint16_t)(contact_words + FXREC_NEXT));
                contact_words = next;
                if (next == 0) {
                    break;
                } else {
                    continue;
                }
            }
        } else {
        }
        fx_unspill(stack);
        return (FxCarCollideResult){place_x, place_y, model_record, car, record_ptr_2, rom_at, 0};
    } else {
        flags1c_2 = fx_ldw((uint16_t)(car + FXREC_FLAGS1C));
        fx_sbk((uint16_t)(flags1c_2 & ~0x0040));
        fx_unspill(stack);
        return (FxCarCollideResult){place_x, place_y, model_record, car, record_ptr_2, rom_at, 0};
    }
}


/* the push at a variant by its routine word: moved from fx/dispatch.c (the chip jumps through a register) */
FxPushAtVariantResult FxPushAtVariant(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t push_x, uint16_t push_y, uint16_t model_record, uint16_t *stack, uint16_t count)
{

    switch (count) {
    case 0x9A9B:
        { FxPushAt4Result tr_ = FxPushAt4(point_x, point_y, point_z, stack); return (FxPushAtVariantResult){tr_.place_x, tr_.place_y, tr_.model_record}; }
    case 0x99A1:
        { FxPushAt2Result tr_ = FxPushAt2(point_x, point_y, point_z, stack); return (FxPushAtVariantResult){tr_.push_x, tr_.push_y, tr_.model_record}; }
    case 0x99C9:
        { FxPushAt1Result tr_ = FxPushAt1(point_x, point_y, point_z, stack); return (FxPushAtVariantResult){tr_.push_x_out, tr_.push_y_out, tr_.model_record}; }
    case 0x99EE:
        { FxPushAtHalfResult tr_ = FxPushAtHalf(point_x, point_y, point_z, stack); return (FxPushAtVariantResult){tr_.push_x, tr_.push_y, tr_.model_record}; }
    case 0x9A19:
        { FxPushAtQuarterResult tr_ = FxPushAtQuarter(point_x, point_y, point_z, stack); return (FxPushAtVariantResult){tr_.push_x, tr_.push_y, tr_.model_record}; }
    case 0x9A4A:
        { FxPushAt4TwistResult tr_ = FxPushAt4Twist(point_x, point_y, point_z, stack); return (FxPushAtVariantResult){tr_.push_x, tr_.push_y, tr_.model_record}; }
    default:
        fx_bad_dispatch(count);
    }
}


/* a record's handler, by the word +$0A holds: moved from fx/dispatch.c (the chip jumps through a register) */
FxRecordHandlerResult FxRecordHandler(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t record, uint16_t pose_matrix_ptr, uint16_t *stack, uint16_t count, uint16_t rom_at)
{

    switch (count) {
    case 0x907A:
        { FxObjectBounceResult tr_ = FxObjectBounce(pose_cos, pose_aux, record, stack, rom_at); return (FxRecordHandlerResult){tr_.world_sum_x_out, tr_.world_sum_y_out, tr_.push_flags_high_out, tr_.record_restored_out, tr_.working_record_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x90E8:
        { FxObjectMarkTouchedResult tr_ = FxObjectMarkTouched(pose_cos, pose_sin, pose_aux, record, pose_matrix_ptr, rom_at); return (FxRecordHandlerResult){tr_.pose_cos_out, tr_.pose_sin_out, tr_.pose_aux_out, tr_.record_out, tr_.pose_matrix_ptr_out, tr_.rom_at_out, 0}; }
    case 0x910A:
        { FxObjectFallResult tr_ = FxObjectFall(pose_cos, pose_aux, record, stack, rom_at); return (FxRecordHandlerResult){tr_.world_sum_x_out, tr_.world_sum_y_out, tr_.push_flags_high_out, tr_.record_restored_out, tr_.working_record_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x916D:
        { FxObjectCollideOnlyResult tr_ = FxObjectCollideOnly(pose_cos, pose_sin, pose_aux, record, stack, rom_at); return (FxRecordHandlerResult){tr_.last_collision_result, tr_.last_contact_ptr, tr_.last_other_ptr_out, tr_.record_restored, tr_.collided_record, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x9185:
        { FxObjectDropResult tr_ = FxObjectDrop(pose_cos, pose_aux, record, stack, rom_at); return (FxRecordHandlerResult){tr_.last_collision_result, tr_.last_contact_ptr, tr_.last_other_ptr_out, tr_.record_restored, tr_.collided_record, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0xB1A1:
        { FxCarHandlerResult tr_ = FxCarHandler(pose_cos, pose_sin, pose_aux, record, pose_matrix_ptr, stack); return (FxRecordHandlerResult){tr_.course_x_part, tr_.next_entry_end_ptr, tr_.pose_aux_out, tr_.car_out, tr_.pose_matrix_ptr_out, tr_.rom_at, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    default:
        fx_bad_dispatch(count);
    }
}
