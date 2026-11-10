/*
 * fx_sums.c: the Super FX code of asm/stuntracefx/object/fx_sums.asm, written from it by
 * the generator.
 *
 * fx_sums.asm: the Super FX's two vector sums for the object it is moving.
 *
 * $70:2340/2342/2344 is a sum in world axes (x, y, z) and $70:2346/2348/234A
 * a sum in the object's own axes. Pushes are added into them during a step;
 * the own-axes sum is turned into world axes through the 3x3 matrix at
 * $70:00E4 (columns of three words at $00E4, $00EA and $00F0, 1.15 fixed
 * point) and added into the world sum; the world sum is then added to the
 * object's Super FX record (R9).
 * C: src/stuntracefx/object/fx_sums.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:941C FxPushesApply
 * WHAT IT DOES: Applies the 19 pushes at $23AE (FxPushApply each) with the
 * push flags at $2480 cleared first, and returns the flags they set.
 * HOW: The return address is kept on the R10 stack; the ROM bank is set to 0;
 * R13 counts the pushes from 1, stopping at $14.
 * Entry: R10 = stack; R11 = return address. Exit: R0 = the flags at $2480;
 * R3 past the last push; R13 = $14; R11 = $943F; CY, OV from the last count
 * compare; S, Z from R10; the last RAM address R10.
 */
FxPushesApplyResult FxPushesApply(uint16_t push_x, uint16_t push_y, uint16_t push_amount, uint16_t *stack, uint16_t rom_at)
{
    uint16_t push_out;
    uint16_t slot_counter;

    fx_spill(stack);
    fx_romb(0);
    fx_stw(FX_PUSH_FLAGS, 0);
    push_out = 0x23AE;
    slot_counter = 0;
    for (;;) {
        slot_counter = (uint16_t)(slot_counter + 1);
        if ((uint16_t)((uint16_t)(0 - slot_counter) + 0x0014) != 0) {
            { FxPushApplyResult r_ = FxPushApply(push_out, push_x, push_y, push_amount, stack, rom_at); push_out = r_.push_out; push_x = r_.place_x; push_y = r_.place_y; push_amount = r_.push_flags_high_out; rom_at = r_.rom_at_out; };
            continue;
        } else {
            break;
        }
    }
    fx_ldw(FX_PUSH_FLAGS);
    fx_unspill(stack);
    return (FxPushesApplyResult){push_x, push_y, push_amount, rom_at};
}


/*
 * @routine 04:944C FxPushApply
 * WHAT IT DOES: Applies the push whose five words are at R3 (flags, amount,
 * then a direction x, y, z) and steps R3 past them: a push with no flags,
 * flags above the low byte, a negative amount or an amount of $7D00 is
 * skipped (the last two decoded from the ROM bytes and not reached in the
 * traces so far). Otherwise its flag byte is ORed into $2480, twice the
 * amount goes to $2378, the direction is turned about the vertical by the
 * angle at $247E (a quarter, half or three quarters turn by swapping and
 * negating; any other angle by FxTurn, decoded from the ROM bytes and not
 * reached in the traces so far) and kept at $236C/$236E/$2370, and twice
 * the amount times it (FMULT) is added to the world sum (FxSumsPushWorld).
 * HOW: R0 = 1 for an applied push. The registers the general turn needs
 * and the return address are kept on the R10 stack.
 * Entry: R3 = the push; R10 = stack; R11 = return address. Exit: R3 = the
 * push + 10; R0 = 1 (applied), 8 or 6 (skipped); applied: R4-R6 as
 * FxSumsPushWorld leaves them, R11 = $955E; S, Z from R10; the last RAM
 * address R10 (applied) or the push's last word read.
 */
FxPushApplyResult FxPushApply(uint16_t push, uint16_t push_x, uint16_t push_y, uint16_t prior_flags_high, uint16_t *stack, uint16_t rom_at)
{
    uint16_t x;
    uint16_t z;
    uint16_t new_z;
    uint16_t sizes;
    uint16_t r_t_22;
    uint16_t r_t_23;
    uint16_t new_x_x;
    uint16_t sizes_2;
    uint16_t normal_y;
    uint16_t normal_z;
    uint16_t push_turn;
    uint16_t push_flags;
    uint16_t push_out_2;
    uint16_t push_x_out;
    uint16_t push_y_out;
    uint16_t push_amount;
    uint16_t turned_x_x2;
    uint16_t turned_z_x2;
    uint16_t normal_y_ptr;
    uint16_t normal_y_ram;
    uint16_t normal_z_ptr;
    uint16_t normal_z_ram;
    uint16_t push_amount_ptr;
    uint16_t push_amount_raw;
    uint16_t push_flags_high;
    uint16_t push_flags_word;
    uint16_t push_normal_ptr;

    push_flags_word = fx_ldw(push);
    push_amount_ptr = (uint16_t)(push + 2);
    if (push_flags_word != 0) {
        push_flags_high = (uint16_t)(push_flags_word >> 8);
        if (push_flags_high == 0) {
            fx_spill(stack);
            push_amount_raw = fx_ldw(push_amount_ptr);
            push_normal_ptr = (uint16_t)(push_amount_ptr + 2);
            if (!(push_amount_raw & 0x8000)) {
                if ((uint16_t)((uint16_t)(0 - push_amount_raw) + 0x7D00) != 0) {
                    push_flags = fx_ldw(FX_PUSH_FLAGS);
                    fx_sbk((push_flags | push_flags_word));
                    push_amount = (uint16_t)(push_amount_raw + push_amount_raw);
                    fx_stw(FX_PUSH_AMOUNT, push_amount);
                    sizes = fx_ldw(push_normal_ptr);
                    normal_y_ptr = (uint16_t)(push_normal_ptr + 2);
                    normal_y = fx_ldw(normal_y_ptr);
                    normal_z_ptr = (uint16_t)(normal_y_ptr + 2);
                    normal_z = fx_ldw(normal_z_ptr);
                    push_out_2 = (uint16_t)(normal_z_ptr + 2);
                    push_turn = fx_ldw(FX_PUSH_TURN);
                    if (push_turn != 0) {
                        if (push_turn != 0x0040) {
                            if (push_turn != 0x00C0) {
                                if (push_turn != 0x0080) {
                                    fx_spill(stack);
                                    fx_push(stack, push_out_2);
                                    fx_push(stack, normal_y);
                                    fx_push(stack, push_amount);
                                    fx_spill(stack);
                                    fx_spill(stack);
                                    fx_spill(stack);
                                    x = sizes;
                                    z = normal_z;
                                    if ((x | z) == 0) {
                                    } else {
                                        { FxTurnResult r_ = FxTurn(x, push_turn, z, stack); new_x_x = r_.new_x_x; new_z = r_.new_z; rom_at = r_.rom_at; };
                                        turned_x_x2 = (uint16_t)(new_x_x + new_x_x);
                                        x = (uint16_t)(turned_x_x2 + turned_x_x2);
                                        turned_z_x2 = (uint16_t)(new_z + new_z);
                                        z = (uint16_t)(turned_z_x2 + turned_z_x2);
                                    }
                                    fx_unspill(stack);
                                    sizes = z;
                                    normal_z = x;
                                    fx_unspill(stack);
                                    fx_unspill(stack);
                                    push_amount = fx_pull(stack);
                                    normal_y = fx_pull(stack);
                                    push_out_2 = fx_pull(stack);
                                    fx_unspill(stack);
                                } else {
                                    sizes = (uint16_t)((uint16_t)~sizes + 1);
                                    normal_z = (uint16_t)((uint16_t)~normal_z + 1);
                                }
                            } else {
                                r_t_22 = (uint16_t)((uint16_t)~sizes + 1);
                                sizes = normal_z;
                                normal_z = r_t_22;
                            }
                        } else {
                            r_t_23 = (uint16_t)((uint16_t)~normal_z + 1);
                            normal_z = sizes;
                            sizes = r_t_23;
                        }
                    } else {
                    }
                    fx_stw(FX_SIZES, sizes);
                    fx_stw(0x236E, normal_y);
                    fx_stw(0x2370, normal_z);
                    sizes_2 = fx_ldw(FX_SIZES);
                    normal_y_ram = fx_ldw(0x236E);
                    normal_z_ram = fx_ldw(0x2370);
                    { FxSumsPushWorldResult r_ = FxSumsPushWorld((uint16_t)((uint32_t)((int32_t)(int16_t)push_amount * (int16_t)sizes_2) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)push_amount * (int16_t)normal_y_ram) >> 16), (uint16_t)((uint32_t)((int32_t)(int16_t)push_amount * (int16_t)normal_z_ram) >> 16), stack); push_x_out = r_.push_x_out; push_y_out = r_.push_y_out; };
                    fx_unspill(stack);
                    return (FxPushApplyResult){1, push_out_2, push_x_out, push_y_out, push_amount, rom_at};
                } else {
                    fx_unspill(stack);
                    return (FxPushApplyResult){6, (uint16_t)(push_normal_ptr + 6), push_x, push_y, push_amount_raw, rom_at};
                }
            } else {
                fx_unspill(stack);
                return (FxPushApplyResult){6, (uint16_t)(push_normal_ptr + 6), push_x, push_y, push_amount_raw, rom_at};
            }
        } else {
            return (FxPushApplyResult){8, (uint16_t)(push_amount_ptr + 8), push_x, push_y, push_flags_high, rom_at};
        }
    } else {
        return (FxPushApplyResult){8, (uint16_t)(push_amount_ptr + 8), push_x, push_y, prior_flags_high, rom_at};
    }
}


/*
 * @routine 04:96A7 FxSumsClear
 * WHAT IT DOES: Clears both vector sums.
 * HOW: SUB R0 from itself, then six SMs of the zero, world x first.
 * Entry: R11 = return address. Exit: R0 = 0; Z, CY set, S, OV clear; the
 * last RAM address $234A.
 */
void FxSumsClear(void)
{

    fx_stw(FX_WORLD_SUM, 0);
    fx_stw(0x2342, 0);
    fx_stw(0x2344, 0);
    fx_stw(FX_LOCAL_SUM, 0);
    fx_stw(0x2348, 0);
    fx_stw(0x234A, 0);
    return;
}


/*
 * @routine 04:96C3 FxSumsPushWorld
 * WHAT IT DOES: Adds the push (R4, R5, R6), each held to -5000..5000, to
 * the world sum.
 * HOW: The clamp (clamp) per component with R1 = -5000 and R2 = 5000,
 * then LM, ADD, SM per word. R1 and R2 are kept on the R10 stack.
 * Entry: R4, R5, R6 = the push; R10 = stack; R11 = return address. Exit:
 * R4-R6 clamped; R0 = the new world z; R1, R2 as on entry; OV, S, CY, Z
 * from the last ADD; the last RAM address $2344.
 */
FxSumsPushWorldResult FxSumsPushWorld(uint16_t push_x, uint16_t push_y, uint16_t push_z, uint16_t *stack)
{
    uint16_t world_sum;
    uint16_t world_sum_y;
    uint16_t world_sum_z;
    uint16_t clamped_push_x;
    uint16_t clamped_push_y;
    uint16_t clamped_push_z;

    fx_spill(stack);
    fx_spill(stack);
    clamped_push_x = push_x;
    if (push_x & 0x8000) {
        if (!((uint16_t)(clamped_push_x + 0x1388) & 0x8000)) {
        } else {
            clamped_push_x = 0xEC78;
        }
    } else {
        if ((uint16_t)(clamped_push_x - 0x1388) & 0x8000) {
        } else {
            clamped_push_x = 0x1388;
        }
    }
    clamped_push_y = push_y;
    if (push_y & 0x8000) {
        if (!((uint16_t)(clamped_push_y + 0x1388) & 0x8000)) {
        } else {
            clamped_push_y = 0xEC78;
        }
    } else {
        if ((uint16_t)(clamped_push_y - 0x1388) & 0x8000) {
        } else {
            clamped_push_y = 0x1388;
        }
    }
    clamped_push_z = push_z;
    if (push_z & 0x8000) {
        if (!((uint16_t)(clamped_push_z + 0x1388) & 0x8000)) {
        } else {
            clamped_push_z = 0xEC78;
        }
    } else {
        if ((uint16_t)(clamped_push_z - 0x1388) & 0x8000) {
        } else {
            clamped_push_z = 0x1388;
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    world_sum = fx_ldw(FX_WORLD_SUM);
    fx_stw(FX_WORLD_SUM, (uint16_t)(world_sum + clamped_push_x));
    world_sum_y = fx_ldw(0x2342);
    fx_stw(0x2342, (uint16_t)(world_sum_y + clamped_push_y));
    world_sum_z = fx_ldw(0x2344);
    fx_stw(0x2344, (uint16_t)(world_sum_z + clamped_push_z));
    return (FxSumsPushWorldResult){clamped_push_x, clamped_push_y};
}


/*
 * @routine 04:973F FxSumsPushLocal
 * WHAT IT DOES: As FxSumsPushWorld, into the own-axes sum.
 * HOW: As FxSumsPushWorld.
 * Entry: as FxSumsPushWorld. Exit: as FxSumsPushWorld; R0 = the new
 * own-axes z; the last RAM address $234A.
 */
void FxSumsPushLocal(uint16_t push_x, uint16_t push_y, uint16_t push_z, uint16_t *stack)
{
    uint16_t local_sum;
    uint16_t local_sum_y;
    uint16_t local_sum_z;
    uint16_t clamped_push_x;
    uint16_t clamped_push_y;
    uint16_t clamped_push_z;

    fx_spill(stack);
    fx_spill(stack);
    clamped_push_x = push_x;
    if (push_x & 0x8000) {
        if (!((uint16_t)(clamped_push_x + 0x1388) & 0x8000)) {
        } else {
            clamped_push_x = 0xEC78;
        }
    } else {
        if ((uint16_t)(clamped_push_x - 0x1388) & 0x8000) {
        } else {
            clamped_push_x = 0x1388;
        }
    }
    clamped_push_y = push_y;
    if (push_y & 0x8000) {
        if (!((uint16_t)(clamped_push_y + 0x1388) & 0x8000)) {
        } else {
            clamped_push_y = 0xEC78;
        }
    } else {
        if ((uint16_t)(clamped_push_y - 0x1388) & 0x8000) {
        } else {
            clamped_push_y = 0x1388;
        }
    }
    clamped_push_z = push_z;
    if (push_z & 0x8000) {
        if (!((uint16_t)(clamped_push_z + 0x1388) & 0x8000)) {
        } else {
            clamped_push_z = 0xEC78;
        }
    } else {
        if ((uint16_t)(clamped_push_z - 0x1388) & 0x8000) {
        } else {
            clamped_push_z = 0x1388;
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    local_sum = fx_ldw(FX_LOCAL_SUM);
    fx_stw(FX_LOCAL_SUM, (uint16_t)(local_sum + clamped_push_x));
    local_sum_y = fx_ldw(0x2348);
    fx_stw(0x2348, (uint16_t)(local_sum_y + clamped_push_y));
    local_sum_z = fx_ldw(0x234A);
    fx_stw(0x234A, (uint16_t)(local_sum_z + clamped_push_z));
    return;
}


/*
 * @routine 04:97BB FxSumsRotateLocal
 * WHAT IT DOES: Adds the own-axes sum, turned into world axes by the matrix
 * at $00E4, into the world sum.
 * HOW: For world x, z, then y (in that order), the three products of the
 * own-axes sum with the matrix row, each an FMULT whose top bit is rolled
 * in, summed and added to the world sum's word with SBK.
 * Entry: R11 = return address. Exit: R0 = the new world y; R2 = the own-axes
 * sum's world y; R1, R3 = own-axes x, z; R4 = the old world y; R5 = the y
 * row's first two products summed; R6 = the matrix word at $00F2; the last
 * RAM address $2342; OV, S, CY, Z from the final add.
 */
FxSumsRotateLocalResult FxSumsRotateLocal(void)
{
    uint16_t local_y;
    uint16_t local_z;
    uint16_t local_sum;
    uint16_t world_sum;
    uint32_t x_m0_prod;
    uint32_t x_m1_prod;
    uint32_t x_m2_prod;
    uint32_t y_m3_prod;
    uint32_t y_m4_prod;
    uint32_t y_m5_prod;
    uint32_t z_m6_prod;
    uint32_t z_m7_prod;
    uint32_t z_m8_prod;
    uint16_t old_world_2;
    uint16_t old_world_2344;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t rotated_third_partial;

    local_sum = fx_ldw(FX_LOCAL_SUM);
    local_y = fx_ldw(0x2348);
    local_z = fx_ldw(0x234A);
    object_matrix_0 = fx_ldw(0x00E4);
    x_m0_prod = (uint32_t)((int32_t)(int16_t)local_sum * (int16_t)object_matrix_0);
    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
    y_m3_prod = (uint32_t)((int32_t)(int16_t)local_y * (int16_t)object_matrix_3);
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    z_m6_prod = (uint32_t)((int32_t)(int16_t)local_z * (int16_t)object_matrix_6);
    world_sum = fx_ldw(FX_WORLD_SUM);
    fx_sbk((uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_m6_prod >> 16) << 1 | (int)(z_m6_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_m0_prod >> 16) << 1 | (int)(x_m0_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_m3_prod >> 16) << 1 | (int)(y_m3_prod >> 15 & 1))) + world_sum));
    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
    x_m2_prod = (uint32_t)((int32_t)(int16_t)local_sum * (int16_t)object_matrix_2);
    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
    y_m5_prod = (uint32_t)((int32_t)(int16_t)local_y * (int16_t)object_matrix_5);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    z_m8_prod = (uint32_t)((int32_t)(int16_t)local_z * (int16_t)object_matrix_8);
    old_world_2344 = fx_ldw(0x2344);
    fx_sbk((uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_m8_prod >> 16) << 1 | (int)(z_m8_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_m2_prod >> 16) << 1 | (int)(x_m2_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_m5_prod >> 16) << 1 | (int)(y_m5_prod >> 15 & 1))) + old_world_2344));
    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
    x_m1_prod = (uint32_t)((int32_t)(int16_t)local_sum * (int16_t)object_matrix_1);
    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
    y_m4_prod = (uint32_t)((int32_t)(int16_t)local_y * (int16_t)object_matrix_4);
    rotated_third_partial = (uint16_t)((uint16_t)((uint16_t)(x_m1_prod >> 16) << 1 | (int)(x_m1_prod >> 15 & 1)) + (uint16_t)((uint16_t)(y_m4_prod >> 16) << 1 | (int)(y_m4_prod >> 15 & 1)));
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    z_m7_prod = (uint32_t)((int32_t)(int16_t)local_z * (int16_t)object_matrix_7);
    old_world_2 = fx_ldw(0x2342);
    fx_sbk((uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_m7_prod >> 16) << 1 | (int)(z_m7_prod >> 15 & 1)) + rotated_third_partial) + old_world_2));
    return (FxSumsRotateLocalResult){old_world_2, rotated_third_partial};
}


/*
 * @routine 04:9BD2 FxSumsApply
 * WHAT IT DOES: Adds the world sum to the Super FX record R9: half of it to
 * +$1E/+$20/+$22, and twice x, eight times y and twice z to +$2E/+$30/+$32.
 * Nothing is changed when the sum is zero.
 * HOW: The three words are ORed to test for zero; each field is read with
 * LDW, the scaled sum added (DIV2 for the halves) and stored back with SBK,
 * in record order.
 * Entry: R9 = the record; R11 = return address. Exit: R4, R5, R6 = the world
 * sum. A zero sum: R0 = 0; S clear, Z set; the last RAM address $2344.
 * Otherwise: R0 = the new +$32; R1 = the old +$32; the last RAM address
 * R9 + $32; OV, S, CY, Z from the final add.
 */
FxSumsApplyResult FxSumsApply(uint16_t record)
{
    uint16_t y;
    uint16_t half_x;
    uint16_t record_z;
    uint16_t scaled_x;
    uint16_t sum_y_x2;
    uint16_t sum_y_x4;
    uint16_t world_sum;
    uint16_t world_sum_z;
    uint16_t world_sum_y_2;
    uint16_t record_speed_y;
    uint16_t record_speed_z;

    world_sum = fx_ldw(FX_WORLD_SUM);
    world_sum_y_2 = fx_ldw(0x2342);
    world_sum_z = fx_ldw(0x2344);
    if (((world_sum | world_sum_y_2) | world_sum_z) == 0) {
    } else {
        half_x = fx_ldw((uint16_t)(FXREC_HALF_X + record));
        fx_sbk((uint16_t)(fx_div2(world_sum) + half_x));
        y = fx_ldw((uint16_t)(record + FXREC_Y));
        fx_sbk((uint16_t)(fx_div2(world_sum_y_2) + y));
        record_z = fx_ldw((uint16_t)(record + 0x0022));
        fx_sbk((uint16_t)(fx_div2(world_sum_z) + record_z));
        scaled_x = fx_ldw((uint16_t)(FXREC_SCALED_X + record));
        fx_sbk((uint16_t)((uint16_t)(world_sum + world_sum) + scaled_x));
        record_speed_y = fx_ldw((uint16_t)(record + 0x0030));
        sum_y_x2 = (uint16_t)(world_sum_y_2 + world_sum_y_2);
        sum_y_x4 = (uint16_t)(sum_y_x2 + sum_y_x2);
        fx_sbk((uint16_t)((uint16_t)(sum_y_x4 + sum_y_x4) + record_speed_y));
        record_speed_z = fx_ldw((uint16_t)(record + 0x0032));
        fx_sbk((uint16_t)((uint16_t)(world_sum_z + world_sum_z) + record_speed_z));
    }
    return (FxSumsApplyResult){world_sum, world_sum_y_2};
}


/*
 * @routine 04:987D FxHitAddScaled
 * WHAT IT DOES: Adds to the hit point at $2334 the vector (R1, R2, R3)
 * scaled by R6 (the low words of LMULT), made positive, each times half
 * the world sum's word (FMULT); a zero vector counts as $7FFF on each
 * axis.
 * HOW: R11, R9, R8 pushed on the R10 stack; OR of the three tests for
 * zero; MOVES, NOT, INC for the magnitudes; DIV2, FMULT, ADD, SM per axis.
 * Returns by pulling R15.
 * Entry: R1-R3 = the vector, R6 = the scale; R10 = stack; R11 = return
 * address. Exit: R1-R3 the amounts added; R0 the new hit z; R6 half the
 * world z; R4 the last low product; R8, R9 restored; S, Z from R10; the
 * last RAM address R10.
 */
uint16_t FxHitAddScaled(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t entry_product_lo, uint16_t scale, uint16_t *stack)
{
    uint16_t hit;
    uint16_t hit_y;
    uint16_t hit_z;
    uint16_t scaled_x;
    uint16_t scaled_y;
    uint16_t world_sum;
    uint16_t world_sum_y;
    uint16_t world_sum_z;
    uint16_t abs_scaled_x;
    uint16_t abs_scaled_y;
    uint16_t abs_scaled_z;
    uint16_t scaled_z_copy;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    if (((vector_x | vector_y) | vector_z) != 0) {
        scaled_x = (uint16_t)(uint32_t)((int32_t)(int16_t)vector_x * (int16_t)scale);
        scaled_y = (uint16_t)(uint32_t)((int32_t)(int16_t)vector_y * (int16_t)scale);
        entry_product_lo = (uint16_t)(uint32_t)((int32_t)(int16_t)vector_z * (int16_t)scale);
        scaled_z_copy = entry_product_lo;
        abs_scaled_x = scaled_x;
        if (!(scaled_x & 0x8000)) {
        } else {
            abs_scaled_x = (uint16_t)((uint16_t)~abs_scaled_x + 1);
        }
        abs_scaled_y = scaled_y;
        if (!(scaled_y & 0x8000)) {
        } else {
            abs_scaled_y = (uint16_t)((uint16_t)~abs_scaled_y + 1);
        }
        abs_scaled_z = scaled_z_copy;
        if (!(scaled_z_copy & 0x8000)) {
        } else {
            abs_scaled_z = (uint16_t)((uint16_t)~abs_scaled_z + 1);
        }
    } else {
        abs_scaled_x = 0x7FFF;
        abs_scaled_y = abs_scaled_x;
        abs_scaled_z = abs_scaled_x;
    }
    world_sum = fx_ldw(FX_WORLD_SUM);
    hit = fx_ldw(FX_HIT);
    fx_stw(FX_HIT, (uint16_t)(hit + (uint16_t)((uint32_t)((int32_t)(int16_t)abs_scaled_x * (int16_t)fx_div2(world_sum)) >> 16)));
    world_sum_y = fx_ldw(0x2342);
    hit_y = fx_ldw(0x2336);
    fx_stw(0x2336, (uint16_t)(hit_y + (uint16_t)((uint32_t)((int32_t)(int16_t)abs_scaled_y * (int16_t)fx_div2(world_sum_y)) >> 16)));
    world_sum_z = fx_ldw(0x2344);
    hit_z = fx_ldw(0x2338);
    fx_stw(0x2338, (uint16_t)(hit_z + (uint16_t)((uint32_t)((int32_t)(int16_t)abs_scaled_z * (int16_t)fx_div2(world_sum_z)) >> 16)));
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return entry_product_lo;
}


/*
 * @routine 04:98AB FxHitAddUnit
 * WHAT IT DOES: As FxHitAddScaled with the vector made a unit vector
 * (FxNormalize) instead of scaled.
 * HOW: As FxHitAddScaled. Not reached in the traces so far.
 * Entry: R1-R3 = the vector; R10 = stack; R11 = return address. Exit: as
 * FxHitAddScaled, the rest as FxNormalize leaves them.
 */
FxHitAddUnitResult FxHitAddUnit(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t place_x, uint16_t place_y, uint16_t model_record, uint16_t owner_record, uint16_t *stack)
{
    uint16_t hit;
    uint16_t sum_y;
    uint16_t sum_z;
    uint16_t world_sum;
    uint16_t abs_unit_x;
    uint16_t abs_unit_y;
    uint16_t abs_unit_z;
    uint16_t hit_y_total;
    uint16_t hit_z_total;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_y;
    uint16_t scaled_vector_z;

    fx_spill(stack);
    fx_push(stack, owner_record);
    fx_spill(stack);
    if (((vector_x | vector_y) | vector_z) != 0) {
        { FxNormalized n_; if (FxNormalize((FxVec3){(int16_t)vector_x, (int16_t)vector_y, (int16_t)vector_z}, &n_)) { scaled_vector_x = (uint16_t)n_.unit.x; scaled_vector_y = (uint16_t)n_.unit.y; scaled_vector_z = (uint16_t)n_.unit.z; place_x = (uint16_t)n_.z_low; place_y = 0; model_record = (uint16_t)n_.x_high; } else { scaled_vector_x = 0; scaled_vector_y = 0; scaled_vector_z = 0; place_x = 0; place_y = 0; model_record = model_record; } };
        abs_unit_x = scaled_vector_x;
        if (!(scaled_vector_x & 0x8000)) {
        } else {
            abs_unit_x = (uint16_t)((uint16_t)~abs_unit_x + 1);
        }
        abs_unit_y = scaled_vector_y;
        if (!(scaled_vector_y & 0x8000)) {
        } else {
            abs_unit_y = (uint16_t)((uint16_t)~abs_unit_y + 1);
        }
        abs_unit_z = scaled_vector_z;
        if (!(scaled_vector_z & 0x8000)) {
        } else {
            abs_unit_z = (uint16_t)((uint16_t)~abs_unit_z + 1);
        }
    } else {
        abs_unit_x = 0x7FFF;
        abs_unit_y = abs_unit_x;
        abs_unit_z = abs_unit_x;
    }
    world_sum = fx_ldw(FX_WORLD_SUM);
    hit = fx_ldw(FX_HIT);
    fx_stw(FX_HIT, (uint16_t)(hit + (uint16_t)((uint32_t)((int32_t)(int16_t)abs_unit_x * (int16_t)fx_div2(world_sum)) >> 16)));
    sum_y = fx_ldw(0x2342);
    hit_y_total = fx_ldw(0x2336);
    fx_stw(0x2336, (uint16_t)(hit_y_total + (uint16_t)((uint32_t)((int32_t)(int16_t)abs_unit_y * (int16_t)fx_div2(sum_y)) >> 16)));
    sum_z = fx_ldw(0x2344);
    hit_z_total = fx_ldw(0x2338);
    fx_stw(0x2338, (uint16_t)(hit_z_total + (uint16_t)((uint32_t)((int32_t)(int16_t)abs_unit_z * (int16_t)fx_div2(sum_z)) >> 16)));
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxHitAddUnitResult){place_x, place_y, model_record};
}


/*
 * @routine 04:9927 FxPushAtTurned
 * WHAT IT DOES: As FxPushAt4 at the point (R1, R2, R3) turned by the
 * matrix at $00E4 first (left at $26F6/$26F8/$26FA).
 * HOW: The matrix rows (FMULT, ROL) as in the shared part, SM to
 * $26F6-$26FA, LM back into R1-R3, then a jump to FxPushAt4. Nothing
 * calls it.
 * Entry: as FxPushAt4. Exit: as FxPushAt4.
 */
void FxPushAtTurned(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack)
{
    uint16_t me_z;
    uint16_t haze_x8;
    uint16_t point_x_2;
    uint32_t x_m0_prod;
    uint32_t x_m1_prod;
    uint32_t x_m2_prod;
    uint32_t y_m3_prod;
    uint32_t y_m4_prod;
    uint32_t y_m5_prod;
    uint32_t z_m6_prod;
    uint32_t z_m7_prod;
    uint32_t z_m8_prod;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;

    object_matrix_0 = fx_ldw(0x00E4);
    x_m0_prod = (uint32_t)((int32_t)(int16_t)point_x * (int16_t)object_matrix_0);
    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
    y_m3_prod = (uint32_t)((int32_t)(int16_t)point_y * (int16_t)object_matrix_3);
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    z_m6_prod = (uint32_t)((int32_t)(int16_t)point_z * (int16_t)object_matrix_6);
    fx_stw(0x26F6, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_m6_prod >> 16) << 1 | (int)(z_m6_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_m0_prod >> 16) << 1 | (int)(x_m0_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_m3_prod >> 16) << 1 | (int)(y_m3_prod >> 15 & 1))));
    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
    x_m1_prod = (uint32_t)((int32_t)(int16_t)point_x * (int16_t)object_matrix_1);
    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
    y_m4_prod = (uint32_t)((int32_t)(int16_t)point_y * (int16_t)object_matrix_4);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    z_m7_prod = (uint32_t)((int32_t)(int16_t)point_z * (int16_t)object_matrix_7);
    fx_stw(FX_HAZE_X8, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_m7_prod >> 16) << 1 | (int)(z_m7_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_m1_prod >> 16) << 1 | (int)(x_m1_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_m4_prod >> 16) << 1 | (int)(y_m4_prod >> 15 & 1))));
    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
    x_m2_prod = (uint32_t)((int32_t)(int16_t)point_x * (int16_t)object_matrix_2);
    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
    y_m5_prod = (uint32_t)((int32_t)(int16_t)point_y * (int16_t)object_matrix_5);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    z_m8_prod = (uint32_t)((int32_t)(int16_t)point_z * (int16_t)object_matrix_8);
    fx_stw(FX_ME_Z, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_m8_prod >> 16) << 1 | (int)(z_m8_prod >> 15 & 1)) + (uint16_t)((uint16_t)(x_m2_prod >> 16) << 1 | (int)(x_m2_prod >> 15 & 1))) + (uint16_t)((uint16_t)(y_m5_prod >> 16) << 1 | (int)(y_m5_prod >> 15 & 1))));
    point_x_2 = fx_ldw(0x26F6);
    haze_x8 = fx_ldw(FX_HAZE_X8);
    me_z = fx_ldw(FX_ME_Z);
    FxPushAt4(point_x_2, haze_x8, me_z, stack); return;
}


/*
 * @routine 04:9985 FxPushAt4Kept
 * WHAT IT DOES: As FxPushAt4.
 * HOW: R11, R9 and R8 are pushed and pulled straight back (R11 into R11)
 * before the jump to FxPushAt4. Nothing calls it.
 * Entry: as FxPushAt4. Exit: as FxPushAt4.
 */
void FxPushAt4Kept(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack)
{

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    FxPushAt4(point_x, point_y, point_z, stack); return;
}


/*
 * @routine 04:99A1 FxPushAt2
 * WHAT IT DOES: As FxPushAt4 with twice the world sum.
 * HOW: As FxPushAt4, one ADD per word. Not reached in the traces so far.
 * Entry: as FxPushAt4. Exit: as FxPushAt4.
 */
FxPushAt2Result FxPushAt2(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack)
{
    uint16_t me_z;
    uint16_t spin;
    uint16_t sum_y;
    uint16_t sum_z;
    uint16_t haze_x8;
    uint16_t torque_a;
    uint16_t torque_b;
    uint16_t torque_c;
    uint16_t x_m4_low;
    uint16_t x_m8_low;
    uint16_t y_m0_low;
    uint16_t y_m8_low;
    uint16_t z_m0_low;
    uint16_t z_m4_low;
    uint16_t world_sum;
    uint32_t x_m4_prod;
    uint32_t x_m8_prod;
    uint32_t y_m0_prod;
    uint32_t y_m8_prod;
    uint32_t z_m0_prod;
    uint32_t z_m4_prod;
    uint16_t point_x_2x;
    uint16_t point_x_4x;
    uint16_t point_x_8x;
    uint16_t point_y_2x;
    uint16_t point_y_4x;
    uint16_t point_z_2x;
    uint16_t point_z_4x;
    uint16_t turn_sum_a;
    uint16_t turn_sum_b;
    uint16_t torque_a_2x;
    uint16_t torque_b_2x;
    uint16_t torque_c_2x;
    uint16_t object_scale;
    uint32_t new_m0_prod_a;
    uint32_t new_m0_prod_b;
    uint32_t new_m0_prod_c;
    uint32_t new_m4_prod_a;
    uint32_t new_m4_prod_b;
    uint32_t new_m4_prod_c;
    uint32_t new_m8_prod_a;
    uint32_t new_m8_prod_b;
    uint32_t new_m8_prod_c;
    uint16_t torque_a_diff;
    uint16_t torque_b_diff;
    uint16_t torque_c_diff;
    uint16_t view_matrix_0;
    uint16_t view_matrix_4;
    uint16_t view_matrix_8;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t point_x_8x_half;
    uint16_t point_y_8x_half;
    uint16_t point_z_8x_half;
    uint16_t view_matrix_0_2;
    uint16_t view_matrix_0_3;
    uint16_t view_matrix_4_2;
    uint16_t view_matrix_4_3;
    uint16_t view_matrix_8_2;
    uint16_t view_matrix_8_3;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    world_sum = fx_ldw(FX_WORLD_SUM);
    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)(world_sum + world_sum));
    sum_y = fx_ldw(0x2342);
    fx_stw(FX_VIEW_MATRIX_4, (uint16_t)(sum_y + sum_y));
    sum_z = fx_ldw(0x2344);
    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)(sum_z + sum_z));
    point_x_2x = (uint16_t)(point_x + point_x);
    point_y_2x = (uint16_t)(point_y + point_y);
    point_z_2x = (uint16_t)(point_z + point_z);
    point_x_4x = (uint16_t)(point_x_2x + point_x_2x);
    point_y_4x = (uint16_t)(point_y_2x + point_y_2x);
    point_z_4x = (uint16_t)(point_z_2x + point_z_2x);
    point_x_8x_half = (uint16_t)(point_x_4x + point_x_4x);
    point_y_8x_half = (uint16_t)(point_y_4x + point_y_4x);
    point_z_8x_half = (uint16_t)(point_z_4x + point_z_4x);
    fx_stw(FX_VIEW_MATRIX_1, 0);
    fx_stw(0x26F6, (uint16_t)(point_x_8x_half + point_x_8x_half));
    fx_stw(FX_HAZE_X8, (uint16_t)(point_y_8x_half + point_y_8x_half));
    fx_stw(FX_ME_Z, (uint16_t)(point_z_8x_half + point_z_8x_half));
    view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
    view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
    view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
    object_scale = fx_ldw(0x00E4);
    new_m0_prod_a = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_scale);
    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
    new_m0_prod_b = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_1);
    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
    new_m0_prod_c = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_2);
    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(new_m0_prod_c >> 16) << 1 | (int)(new_m0_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(new_m0_prod_a >> 16) << 1 | (int)(new_m0_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(new_m0_prod_b >> 16) << 1 | (int)(new_m0_prod_b >> 15 & 1))));
    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
    new_m4_prod_a = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_3);
    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
    new_m4_prod_b = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_4);
    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
    new_m4_prod_c = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_5);
    fx_stw(FX_VIEW_MATRIX_4, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(new_m4_prod_c >> 16) << 1 | (int)(new_m4_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(new_m4_prod_a >> 16) << 1 | (int)(new_m4_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(new_m4_prod_b >> 16) << 1 | (int)(new_m4_prod_b >> 15 & 1))));
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    new_m8_prod_a = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_6);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    new_m8_prod_b = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_7);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    new_m8_prod_c = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_8);
    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(new_m8_prod_c >> 16) << 1 | (int)(new_m8_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(new_m8_prod_a >> 16) << 1 | (int)(new_m8_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(new_m8_prod_b >> 16) << 1 | (int)(new_m8_prod_b >> 15 & 1))));
    point_x_8x = fx_ldw(0x26F6);
    haze_x8 = fx_ldw(FX_HAZE_X8);
    me_z = fx_ldw(FX_ME_Z);
    view_matrix_4_2 = fx_ldw(FX_VIEW_MATRIX_4);
    z_m4_prod = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_4_2);
    z_m4_low = (uint16_t)z_m4_prod;
    view_matrix_8_2 = fx_ldw(FX_VIEW_MATRIX_8);
    y_m8_prod = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_8_2);
    y_m8_low = (uint16_t)y_m8_prod;
    torque_a_diff = (uint16_t)(z_m4_low - y_m8_low);
    fx_stw(0x0068, (uint16_t)((uint16_t)((uint16_t)(z_m4_prod >> 16) - (uint16_t)(y_m8_prod >> 16) - (z_m4_low < y_m8_low)) << 1 | ((uint32_t)torque_a_diff + torque_a_diff > 0xFFFF)));
    view_matrix_8_3 = fx_ldw(FX_VIEW_MATRIX_8);
    x_m8_prod = (uint32_t)((int32_t)(int16_t)point_x_8x * (int16_t)view_matrix_8_3);
    x_m8_low = (uint16_t)x_m8_prod;
    view_matrix_0_2 = fx_ldw(FX_VIEW_MATRIX_0);
    z_m0_prod = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_0_2);
    z_m0_low = (uint16_t)z_m0_prod;
    torque_b_diff = (uint16_t)(x_m8_low - z_m0_low);
    fx_stw(0x002C, (uint16_t)((uint16_t)((uint16_t)(x_m8_prod >> 16) - (uint16_t)(z_m0_prod >> 16) - (x_m8_low < z_m0_low)) << 1 | ((uint32_t)torque_b_diff + torque_b_diff > 0xFFFF)));
    view_matrix_0_3 = fx_ldw(FX_VIEW_MATRIX_0);
    y_m0_prod = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_0_3);
    y_m0_low = (uint16_t)y_m0_prod;
    view_matrix_4_3 = fx_ldw(FX_VIEW_MATRIX_4);
    x_m4_prod = (uint32_t)((int32_t)(int16_t)point_x_8x * (int16_t)view_matrix_4_3);
    x_m4_low = (uint16_t)x_m4_prod;
    torque_c_diff = (uint16_t)(y_m0_low - x_m4_low);
    fx_stw(0x002E, (uint16_t)((uint16_t)((uint16_t)(y_m0_prod >> 16) - (uint16_t)(x_m4_prod >> 16) - (y_m0_low < x_m4_low)) << 1 | ((uint32_t)torque_c_diff + torque_c_diff > 0xFFFF)));
    torque_a = fx_ldw(0x0068);
    turn_sum_a = fx_ldw(0x233C);
    torque_a_2x = (uint16_t)(torque_a + torque_a);
    fx_sbk((uint16_t)((uint16_t)(torque_a_2x + torque_a_2x) + turn_sum_a));
    torque_c = fx_ldw(0x002E);
    turn_sum_b = fx_ldw(0x233E);
    torque_c_2x = (uint16_t)(torque_c + torque_c);
    fx_sbk((uint16_t)((uint16_t)(torque_c_2x + torque_c_2x) + turn_sum_b));
    torque_b = fx_ldw(0x002C);
    spin = fx_ldw(FX_SPIN);
    torque_b_2x = (uint16_t)(torque_b + torque_b);
    fx_sbk((uint16_t)((uint16_t)(torque_b_2x + torque_b_2x) + spin));
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxPushAt2Result){turn_sum_a, turn_sum_b, (uint16_t)(torque_c_diff + torque_c_diff)};
}


/*
 * @routine 04:99C9 FxPushAt1
 * WHAT IT DOES: As FxPushAt4 with the world sum as it is.
 * HOW: As FxPushAt4, the words as loaded. Not reached in the traces so
 * far.
 * Entry: as FxPushAt4. Exit: as FxPushAt4.
 */
FxPushAt1Result FxPushAt1(uint16_t push_x, uint16_t push_y, uint16_t push_z, uint16_t *stack)
{
    uint16_t me_z;
    uint16_t spin;
    uint16_t spin_a;
    uint16_t spin_c;
    uint16_t haze_x8;
    uint16_t push_x_x2;
    uint16_t push_x_x4;
    uint16_t push_x_x8;
    uint16_t push_y_x2;
    uint16_t push_y_x4;
    uint16_t push_y_x8;
    uint16_t push_z_x2;
    uint16_t push_z_x4;
    uint16_t push_z_x8;
    uint16_t world_sum;
    uint16_t cross_a_x2;
    uint16_t cross_b_x2;
    uint16_t cross_c_x2;
    uint16_t push_x_x16;
    uint32_t row0_prod_a;
    uint32_t row0_prod_b;
    uint32_t row0_prod_c;
    uint32_t row1_prod_a;
    uint32_t row1_prod_b;
    uint32_t row1_prod_c;
    uint32_t row2_prod_a;
    uint32_t row2_prod_b;
    uint32_t row2_prod_c;
    uint32_t cross_a_prod1;
    uint32_t cross_a_prod2;
    uint32_t cross_b_prod1;
    uint32_t cross_b_prod2;
    uint32_t cross_c_prod1;
    uint32_t cross_c_prod2;
    uint16_t view_matrix_0;
    uint16_t view_matrix_4;
    uint16_t view_matrix_8;
    uint16_t cross_a_reload;
    uint16_t cross_b_reload;
    uint16_t cross_c_reload;
    uint16_t cross_a_lo_diff;
    uint16_t cross_b_lo_diff;
    uint16_t cross_c_lo_diff;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t view_matrix_0_2;
    uint16_t view_matrix_0_3;
    uint16_t view_matrix_4_2;
    uint16_t view_matrix_4_3;
    uint16_t view_matrix_4_4;
    uint16_t view_matrix_8_2;
    uint16_t view_matrix_8_3;
    uint16_t view_matrix_8_4;
    uint16_t cross_a_prod1_lo;
    uint16_t cross_a_prod2_lo;
    uint16_t cross_b_prod1_lo;
    uint16_t cross_b_prod2_lo;
    uint16_t cross_c_prod1_lo;
    uint16_t cross_c_prod2_lo;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    world_sum = fx_ldw(FX_WORLD_SUM);
    fx_stw(FX_VIEW_MATRIX_0, world_sum);
    view_matrix_4 = fx_ldw(0x2342);
    fx_stw(FX_VIEW_MATRIX_4, view_matrix_4);
    view_matrix_8 = fx_ldw(0x2344);
    fx_stw(FX_VIEW_MATRIX_8, view_matrix_8);
    push_x_x2 = (uint16_t)(push_x + push_x);
    push_y_x2 = (uint16_t)(push_y + push_y);
    push_z_x2 = (uint16_t)(push_z + push_z);
    push_x_x4 = (uint16_t)(push_x_x2 + push_x_x2);
    push_y_x4 = (uint16_t)(push_y_x2 + push_y_x2);
    push_z_x4 = (uint16_t)(push_z_x2 + push_z_x2);
    push_x_x8 = (uint16_t)(push_x_x4 + push_x_x4);
    push_y_x8 = (uint16_t)(push_y_x4 + push_y_x4);
    push_z_x8 = (uint16_t)(push_z_x4 + push_z_x4);
    fx_stw(FX_VIEW_MATRIX_1, 0);
    fx_stw(0x26F6, (uint16_t)(push_x_x8 + push_x_x8));
    fx_stw(FX_HAZE_X8, (uint16_t)(push_y_x8 + push_y_x8));
    fx_stw(FX_ME_Z, (uint16_t)(push_z_x8 + push_z_x8));
    view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
    view_matrix_4_2 = fx_ldw(FX_VIEW_MATRIX_4);
    view_matrix_8_2 = fx_ldw(FX_VIEW_MATRIX_8);
    object_matrix_0 = fx_ldw(0x00E4);
    row0_prod_a = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_0);
    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
    row0_prod_b = (uint32_t)((int32_t)(int16_t)view_matrix_4_2 * (int16_t)object_matrix_1);
    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
    row0_prod_c = (uint32_t)((int32_t)(int16_t)view_matrix_8_2 * (int16_t)object_matrix_2);
    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(row0_prod_c >> 16) << 1 | (int)(row0_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(row0_prod_a >> 16) << 1 | (int)(row0_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(row0_prod_b >> 16) << 1 | (int)(row0_prod_b >> 15 & 1))));
    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
    row1_prod_a = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_3);
    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
    row1_prod_b = (uint32_t)((int32_t)(int16_t)view_matrix_4_2 * (int16_t)object_matrix_4);
    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
    row1_prod_c = (uint32_t)((int32_t)(int16_t)view_matrix_8_2 * (int16_t)object_matrix_5);
    fx_stw(FX_VIEW_MATRIX_4, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(row1_prod_c >> 16) << 1 | (int)(row1_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(row1_prod_a >> 16) << 1 | (int)(row1_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(row1_prod_b >> 16) << 1 | (int)(row1_prod_b >> 15 & 1))));
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    row2_prod_a = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_6);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    row2_prod_b = (uint32_t)((int32_t)(int16_t)view_matrix_4_2 * (int16_t)object_matrix_7);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    row2_prod_c = (uint32_t)((int32_t)(int16_t)view_matrix_8_2 * (int16_t)object_matrix_8);
    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(row2_prod_c >> 16) << 1 | (int)(row2_prod_c >> 15 & 1)) + (uint16_t)((uint16_t)(row2_prod_a >> 16) << 1 | (int)(row2_prod_a >> 15 & 1))) + (uint16_t)((uint16_t)(row2_prod_b >> 16) << 1 | (int)(row2_prod_b >> 15 & 1))));
    push_x_x16 = fx_ldw(0x26F6);
    haze_x8 = fx_ldw(FX_HAZE_X8);
    me_z = fx_ldw(FX_ME_Z);
    view_matrix_4_3 = fx_ldw(FX_VIEW_MATRIX_4);
    cross_a_prod1 = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_4_3);
    cross_a_prod1_lo = (uint16_t)cross_a_prod1;
    view_matrix_8_3 = fx_ldw(FX_VIEW_MATRIX_8);
    cross_a_prod2 = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_8_3);
    cross_a_prod2_lo = (uint16_t)cross_a_prod2;
    cross_a_lo_diff = (uint16_t)(cross_a_prod1_lo - cross_a_prod2_lo);
    fx_stw(0x0068, (uint16_t)((uint16_t)((uint16_t)(cross_a_prod1 >> 16) - (uint16_t)(cross_a_prod2 >> 16) - (cross_a_prod1_lo < cross_a_prod2_lo)) << 1 | ((uint32_t)cross_a_lo_diff + cross_a_lo_diff > 0xFFFF)));
    view_matrix_8_4 = fx_ldw(FX_VIEW_MATRIX_8);
    cross_b_prod1 = (uint32_t)((int32_t)(int16_t)push_x_x16 * (int16_t)view_matrix_8_4);
    cross_b_prod1_lo = (uint16_t)cross_b_prod1;
    view_matrix_0_2 = fx_ldw(FX_VIEW_MATRIX_0);
    cross_b_prod2 = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_0_2);
    cross_b_prod2_lo = (uint16_t)cross_b_prod2;
    cross_b_lo_diff = (uint16_t)(cross_b_prod1_lo - cross_b_prod2_lo);
    fx_stw(0x002C, (uint16_t)((uint16_t)((uint16_t)(cross_b_prod1 >> 16) - (uint16_t)(cross_b_prod2 >> 16) - (cross_b_prod1_lo < cross_b_prod2_lo)) << 1 | ((uint32_t)cross_b_lo_diff + cross_b_lo_diff > 0xFFFF)));
    view_matrix_0_3 = fx_ldw(FX_VIEW_MATRIX_0);
    cross_c_prod1 = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_0_3);
    cross_c_prod1_lo = (uint16_t)cross_c_prod1;
    view_matrix_4_4 = fx_ldw(FX_VIEW_MATRIX_4);
    cross_c_prod2 = (uint32_t)((int32_t)(int16_t)push_x_x16 * (int16_t)view_matrix_4_4);
    cross_c_prod2_lo = (uint16_t)cross_c_prod2;
    cross_c_lo_diff = (uint16_t)(cross_c_prod1_lo - cross_c_prod2_lo);
    fx_stw(0x002E, (uint16_t)((uint16_t)((uint16_t)(cross_c_prod1 >> 16) - (uint16_t)(cross_c_prod2 >> 16) - (cross_c_prod1_lo < cross_c_prod2_lo)) << 1 | ((uint32_t)cross_c_lo_diff + cross_c_lo_diff > 0xFFFF)));
    cross_a_reload = fx_ldw(0x0068);
    spin_a = fx_ldw(0x233C);
    cross_a_x2 = (uint16_t)(cross_a_reload + cross_a_reload);
    fx_sbk((uint16_t)((uint16_t)(cross_a_x2 + cross_a_x2) + spin_a));
    cross_c_reload = fx_ldw(0x002E);
    spin_c = fx_ldw(0x233E);
    cross_c_x2 = (uint16_t)(cross_c_reload + cross_c_reload);
    fx_sbk((uint16_t)((uint16_t)(cross_c_x2 + cross_c_x2) + spin_c));
    cross_b_reload = fx_ldw(0x002C);
    spin = fx_ldw(FX_SPIN);
    cross_b_x2 = (uint16_t)(cross_b_reload + cross_b_reload);
    fx_sbk((uint16_t)((uint16_t)(cross_b_x2 + cross_b_x2) + spin));
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxPushAt1Result){spin_a, spin_c, (uint16_t)(cross_c_lo_diff + cross_c_lo_diff)};
}


/*
 * @routine 04:99EE FxPushAtHalf
 * WHAT IT DOES: As FxPushAt4 with half the world sum.
 * HOW: As FxPushAt4, one DIV2 per word.
 * Entry: as FxPushAt4. Exit: as FxPushAt4.
 */
FxPushAtHalfResult FxPushAtHalf(uint16_t offset_x, uint16_t offset_y, uint16_t offset_z, uint16_t *stack)
{
    uint16_t me_z;
    uint16_t spin;
    uint16_t haze_x8;
    uint16_t world_sum;
    uint16_t cross_x_x2;
    uint16_t cross_y_x2;
    uint16_t cross_z_x2;
    uint16_t offset_x_x2;
    uint16_t offset_x_x4;
    uint16_t offset_x_x8;
    uint16_t offset_y_x2;
    uint16_t offset_y_x4;
    uint16_t offset_y_x8;
    uint16_t offset_z_x2;
    uint16_t offset_z_x4;
    uint16_t offset_z_x8;
    uint16_t world_sum_y;
    uint16_t world_sum_z;
    uint16_t cross_x_word;
    uint16_t cross_y_word;
    uint16_t cross_z_word;
    uint16_t offset_x_x16;
    uint32_t prod_vm0_om0;
    uint32_t prod_vm0_om3;
    uint32_t prod_vm0_om6;
    uint32_t prod_vm4_om1;
    uint32_t prod_vm4_om4;
    uint32_t prod_vm4_om7;
    uint32_t prod_vm8_om2;
    uint32_t prod_vm8_om5;
    uint32_t prod_vm8_om8;
    uint32_t prod_x16_vm4;
    uint32_t prod_x16_vm8;
    uint32_t prod_y16_vm0;
    uint32_t prod_y16_vm8;
    uint32_t prod_z16_vm0;
    uint32_t prod_z16_vm4;
    uint16_t twist_base_x;
    uint16_t twist_base_y;
    uint16_t view_matrix_0;
    uint16_t view_matrix_4;
    uint16_t view_matrix_8;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t prod_x16_vm4_lo;
    uint16_t prod_x16_vm8_lo;
    uint16_t prod_y16_vm0_lo;
    uint16_t prod_y16_vm8_lo;
    uint16_t prod_z16_vm0_lo;
    uint16_t prod_z16_vm4_lo;
    uint16_t view_matrix_0_2;
    uint16_t view_matrix_0_3;
    uint16_t view_matrix_4_2;
    uint16_t view_matrix_4_3;
    uint16_t view_matrix_8_2;
    uint16_t view_matrix_8_3;
    uint16_t cross_x_low_diff;
    uint16_t cross_y_low_diff;
    uint16_t cross_z_low_diff;
    uint16_t object_matrix_word_0;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    world_sum = fx_ldw(FX_WORLD_SUM);
    fx_stw(FX_VIEW_MATRIX_0, fx_div2(world_sum));
    world_sum_y = fx_ldw(0x2342);
    fx_stw(FX_VIEW_MATRIX_4, fx_div2(world_sum_y));
    world_sum_z = fx_ldw(0x2344);
    fx_stw(FX_VIEW_MATRIX_8, fx_div2(world_sum_z));
    offset_x_x2 = (uint16_t)(offset_x + offset_x);
    offset_y_x2 = (uint16_t)(offset_y + offset_y);
    offset_z_x2 = (uint16_t)(offset_z + offset_z);
    offset_x_x4 = (uint16_t)(offset_x_x2 + offset_x_x2);
    offset_y_x4 = (uint16_t)(offset_y_x2 + offset_y_x2);
    offset_z_x4 = (uint16_t)(offset_z_x2 + offset_z_x2);
    offset_x_x8 = (uint16_t)(offset_x_x4 + offset_x_x4);
    offset_y_x8 = (uint16_t)(offset_y_x4 + offset_y_x4);
    offset_z_x8 = (uint16_t)(offset_z_x4 + offset_z_x4);
    fx_stw(FX_VIEW_MATRIX_1, 0);
    fx_stw(0x26F6, (uint16_t)(offset_x_x8 + offset_x_x8));
    fx_stw(FX_HAZE_X8, (uint16_t)(offset_y_x8 + offset_y_x8));
    fx_stw(FX_ME_Z, (uint16_t)(offset_z_x8 + offset_z_x8));
    view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
    view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
    view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
    object_matrix_word_0 = fx_ldw(0x00E4);
    prod_vm0_om0 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_word_0);
    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
    prod_vm4_om1 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_1);
    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
    prod_vm8_om2 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_2);
    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_vm8_om2 >> 16) << 1 | (int)(prod_vm8_om2 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_vm0_om0 >> 16) << 1 | (int)(prod_vm0_om0 >> 15 & 1))) + (uint16_t)((uint16_t)(prod_vm4_om1 >> 16) << 1 | (int)(prod_vm4_om1 >> 15 & 1))));
    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
    prod_vm0_om3 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_3);
    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
    prod_vm4_om4 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_4);
    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
    prod_vm8_om5 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_5);
    fx_stw(FX_VIEW_MATRIX_4, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_vm8_om5 >> 16) << 1 | (int)(prod_vm8_om5 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_vm0_om3 >> 16) << 1 | (int)(prod_vm0_om3 >> 15 & 1))) + (uint16_t)((uint16_t)(prod_vm4_om4 >> 16) << 1 | (int)(prod_vm4_om4 >> 15 & 1))));
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    prod_vm0_om6 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_6);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    prod_vm4_om7 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_7);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    prod_vm8_om8 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_8);
    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_vm8_om8 >> 16) << 1 | (int)(prod_vm8_om8 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_vm0_om6 >> 16) << 1 | (int)(prod_vm0_om6 >> 15 & 1))) + (uint16_t)((uint16_t)(prod_vm4_om7 >> 16) << 1 | (int)(prod_vm4_om7 >> 15 & 1))));
    offset_x_x16 = fx_ldw(0x26F6);
    haze_x8 = fx_ldw(FX_HAZE_X8);
    me_z = fx_ldw(FX_ME_Z);
    view_matrix_4_2 = fx_ldw(FX_VIEW_MATRIX_4);
    prod_z16_vm4 = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_4_2);
    prod_z16_vm4_lo = (uint16_t)prod_z16_vm4;
    view_matrix_8_2 = fx_ldw(FX_VIEW_MATRIX_8);
    prod_y16_vm8 = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_8_2);
    prod_y16_vm8_lo = (uint16_t)prod_y16_vm8;
    cross_x_low_diff = (uint16_t)(prod_z16_vm4_lo - prod_y16_vm8_lo);
    fx_stw(0x0068, (uint16_t)((uint16_t)((uint16_t)(prod_z16_vm4 >> 16) - (uint16_t)(prod_y16_vm8 >> 16) - (prod_z16_vm4_lo < prod_y16_vm8_lo)) << 1 | ((uint32_t)cross_x_low_diff + cross_x_low_diff > 0xFFFF)));
    view_matrix_8_3 = fx_ldw(FX_VIEW_MATRIX_8);
    prod_x16_vm8 = (uint32_t)((int32_t)(int16_t)offset_x_x16 * (int16_t)view_matrix_8_3);
    prod_x16_vm8_lo = (uint16_t)prod_x16_vm8;
    view_matrix_0_2 = fx_ldw(FX_VIEW_MATRIX_0);
    prod_z16_vm0 = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_0_2);
    prod_z16_vm0_lo = (uint16_t)prod_z16_vm0;
    cross_y_low_diff = (uint16_t)(prod_x16_vm8_lo - prod_z16_vm0_lo);
    fx_stw(0x002C, (uint16_t)((uint16_t)((uint16_t)(prod_x16_vm8 >> 16) - (uint16_t)(prod_z16_vm0 >> 16) - (prod_x16_vm8_lo < prod_z16_vm0_lo)) << 1 | ((uint32_t)cross_y_low_diff + cross_y_low_diff > 0xFFFF)));
    view_matrix_0_3 = fx_ldw(FX_VIEW_MATRIX_0);
    prod_y16_vm0 = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_0_3);
    prod_y16_vm0_lo = (uint16_t)prod_y16_vm0;
    view_matrix_4_3 = fx_ldw(FX_VIEW_MATRIX_4);
    prod_x16_vm4 = (uint32_t)((int32_t)(int16_t)offset_x_x16 * (int16_t)view_matrix_4_3);
    prod_x16_vm4_lo = (uint16_t)prod_x16_vm4;
    cross_z_low_diff = (uint16_t)(prod_y16_vm0_lo - prod_x16_vm4_lo);
    fx_stw(0x002E, (uint16_t)((uint16_t)((uint16_t)(prod_y16_vm0 >> 16) - (uint16_t)(prod_x16_vm4 >> 16) - (prod_y16_vm0_lo < prod_x16_vm4_lo)) << 1 | ((uint32_t)cross_z_low_diff + cross_z_low_diff > 0xFFFF)));
    cross_x_word = fx_ldw(0x0068);
    twist_base_x = fx_ldw(0x233C);
    cross_x_x2 = (uint16_t)(cross_x_word + cross_x_word);
    fx_sbk((uint16_t)((uint16_t)(cross_x_x2 + cross_x_x2) + twist_base_x));
    cross_z_word = fx_ldw(0x002E);
    twist_base_y = fx_ldw(0x233E);
    cross_z_x2 = (uint16_t)(cross_z_word + cross_z_word);
    fx_sbk((uint16_t)((uint16_t)(cross_z_x2 + cross_z_x2) + twist_base_y));
    cross_y_word = fx_ldw(0x002C);
    spin = fx_ldw(FX_SPIN);
    cross_y_x2 = (uint16_t)(cross_y_word + cross_y_word);
    fx_sbk((uint16_t)((uint16_t)(cross_y_x2 + cross_y_x2) + spin));
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxPushAtHalfResult){twist_base_x, twist_base_y, (uint16_t)(cross_z_low_diff + cross_z_low_diff)};
}


/*
 * @routine 04:9A19 FxPushAtQuarter
 * WHAT IT DOES: As FxPushAt4 with a quarter of the world sum.
 * HOW: As FxPushAt4, two DIV2s per word. Not reached in the traces so far.
 * Entry: as FxPushAt4. Exit: as FxPushAt4.
 */
FxPushAtQuarterResult FxPushAtQuarter(uint16_t arm_x, uint16_t arm_y, uint16_t arm_z, uint16_t *stack)
{
    uint16_t me_z;
    uint16_t spin;
    uint16_t haze_x8;
    uint16_t arm_x_x2;
    uint16_t arm_x_x4;
    uint16_t arm_x_x8;
    uint16_t arm_y_x2;
    uint16_t arm_y_x4;
    uint16_t arm_y_x8;
    uint16_t arm_z_x2;
    uint16_t arm_z_x4;
    uint16_t arm_z_x8;
    uint16_t torque_x;
    uint16_t torque_y;
    uint16_t torque_z;
    uint16_t world_sum;
    uint32_t force_x_m0;
    uint32_t force_x_m3;
    uint32_t force_x_m6;
    uint32_t force_y_m1;
    uint32_t force_y_m4;
    uint32_t force_y_m7;
    uint32_t force_z_m2;
    uint32_t force_z_m5;
    uint32_t force_z_m8;
    uint16_t torque_x_x2;
    uint16_t torque_y_x2;
    uint16_t torque_z_x2;
    uint16_t turn_x_base;
    uint16_t turn_z_base;
    uint16_t world_sum_y;
    uint16_t world_sum_z;
    uint16_t arm_x_scaled;
    uint16_t torque_x_diff;
    uint16_t torque_y_diff;
    uint16_t torque_z_diff;
    uint16_t view_matrix_0;
    uint16_t view_matrix_4;
    uint16_t view_matrix_8;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t view_matrix_0_2;
    uint16_t view_matrix_0_3;
    uint16_t view_matrix_4_2;
    uint16_t view_matrix_4_3;
    uint16_t view_matrix_8_2;
    uint16_t view_matrix_8_3;
    uint16_t arm_x_force_y_lo;
    uint16_t arm_x_force_z_lo;
    uint16_t arm_y_force_x_lo;
    uint16_t arm_y_force_z_lo;
    uint16_t arm_z_force_x_lo;
    uint16_t arm_z_force_y_lo;
    uint32_t arm_x_force_y_prod;
    uint32_t arm_x_force_z_prod;
    uint32_t arm_y_force_x_prod;
    uint32_t arm_y_force_z_prod;
    uint32_t arm_z_force_x_prod;
    uint32_t arm_z_force_y_prod;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    world_sum = fx_ldw(FX_WORLD_SUM);
    fx_stw(FX_VIEW_MATRIX_0, fx_div2(fx_div2(world_sum)));
    world_sum_y = fx_ldw(0x2342);
    fx_stw(FX_VIEW_MATRIX_4, fx_div2(fx_div2(world_sum_y)));
    world_sum_z = fx_ldw(0x2344);
    fx_stw(FX_VIEW_MATRIX_8, fx_div2(fx_div2(world_sum_z)));
    arm_x_x2 = (uint16_t)(arm_x + arm_x);
    arm_y_x2 = (uint16_t)(arm_y + arm_y);
    arm_z_x2 = (uint16_t)(arm_z + arm_z);
    arm_x_x4 = (uint16_t)(arm_x_x2 + arm_x_x2);
    arm_y_x4 = (uint16_t)(arm_y_x2 + arm_y_x2);
    arm_z_x4 = (uint16_t)(arm_z_x2 + arm_z_x2);
    arm_x_x8 = (uint16_t)(arm_x_x4 + arm_x_x4);
    arm_y_x8 = (uint16_t)(arm_y_x4 + arm_y_x4);
    arm_z_x8 = (uint16_t)(arm_z_x4 + arm_z_x4);
    fx_stw(FX_VIEW_MATRIX_1, 0);
    fx_stw(0x26F6, (uint16_t)(arm_x_x8 + arm_x_x8));
    fx_stw(FX_HAZE_X8, (uint16_t)(arm_y_x8 + arm_y_x8));
    fx_stw(FX_ME_Z, (uint16_t)(arm_z_x8 + arm_z_x8));
    view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
    view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
    view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
    object_matrix_0 = fx_ldw(0x00E4);
    force_x_m0 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_0);
    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
    force_y_m1 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_1);
    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
    force_z_m2 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_2);
    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(force_z_m2 >> 16) << 1 | (int)(force_z_m2 >> 15 & 1)) + (uint16_t)((uint16_t)(force_x_m0 >> 16) << 1 | (int)(force_x_m0 >> 15 & 1))) + (uint16_t)((uint16_t)(force_y_m1 >> 16) << 1 | (int)(force_y_m1 >> 15 & 1))));
    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
    force_x_m3 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_3);
    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
    force_y_m4 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_4);
    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
    force_z_m5 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_5);
    fx_stw(FX_VIEW_MATRIX_4, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(force_z_m5 >> 16) << 1 | (int)(force_z_m5 >> 15 & 1)) + (uint16_t)((uint16_t)(force_x_m3 >> 16) << 1 | (int)(force_x_m3 >> 15 & 1))) + (uint16_t)((uint16_t)(force_y_m4 >> 16) << 1 | (int)(force_y_m4 >> 15 & 1))));
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    force_x_m6 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_6);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    force_y_m7 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_7);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    force_z_m8 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_8);
    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(force_z_m8 >> 16) << 1 | (int)(force_z_m8 >> 15 & 1)) + (uint16_t)((uint16_t)(force_x_m6 >> 16) << 1 | (int)(force_x_m6 >> 15 & 1))) + (uint16_t)((uint16_t)(force_y_m7 >> 16) << 1 | (int)(force_y_m7 >> 15 & 1))));
    arm_x_scaled = fx_ldw(0x26F6);
    haze_x8 = fx_ldw(FX_HAZE_X8);
    me_z = fx_ldw(FX_ME_Z);
    view_matrix_4_2 = fx_ldw(FX_VIEW_MATRIX_4);
    arm_z_force_y_prod = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_4_2);
    arm_z_force_y_lo = (uint16_t)arm_z_force_y_prod;
    view_matrix_8_2 = fx_ldw(FX_VIEW_MATRIX_8);
    arm_y_force_z_prod = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_8_2);
    arm_y_force_z_lo = (uint16_t)arm_y_force_z_prod;
    torque_x_diff = (uint16_t)(arm_z_force_y_lo - arm_y_force_z_lo);
    fx_stw(0x0068, (uint16_t)((uint16_t)((uint16_t)(arm_z_force_y_prod >> 16) - (uint16_t)(arm_y_force_z_prod >> 16) - (arm_z_force_y_lo < arm_y_force_z_lo)) << 1 | ((uint32_t)torque_x_diff + torque_x_diff > 0xFFFF)));
    view_matrix_8_3 = fx_ldw(FX_VIEW_MATRIX_8);
    arm_x_force_z_prod = (uint32_t)((int32_t)(int16_t)arm_x_scaled * (int16_t)view_matrix_8_3);
    arm_x_force_z_lo = (uint16_t)arm_x_force_z_prod;
    view_matrix_0_2 = fx_ldw(FX_VIEW_MATRIX_0);
    arm_z_force_x_prod = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_0_2);
    arm_z_force_x_lo = (uint16_t)arm_z_force_x_prod;
    torque_y_diff = (uint16_t)(arm_x_force_z_lo - arm_z_force_x_lo);
    fx_stw(0x002C, (uint16_t)((uint16_t)((uint16_t)(arm_x_force_z_prod >> 16) - (uint16_t)(arm_z_force_x_prod >> 16) - (arm_x_force_z_lo < arm_z_force_x_lo)) << 1 | ((uint32_t)torque_y_diff + torque_y_diff > 0xFFFF)));
    view_matrix_0_3 = fx_ldw(FX_VIEW_MATRIX_0);
    arm_y_force_x_prod = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_0_3);
    arm_y_force_x_lo = (uint16_t)arm_y_force_x_prod;
    view_matrix_4_3 = fx_ldw(FX_VIEW_MATRIX_4);
    arm_x_force_y_prod = (uint32_t)((int32_t)(int16_t)arm_x_scaled * (int16_t)view_matrix_4_3);
    arm_x_force_y_lo = (uint16_t)arm_x_force_y_prod;
    torque_z_diff = (uint16_t)(arm_y_force_x_lo - arm_x_force_y_lo);
    fx_stw(0x002E, (uint16_t)((uint16_t)((uint16_t)(arm_y_force_x_prod >> 16) - (uint16_t)(arm_x_force_y_prod >> 16) - (arm_y_force_x_lo < arm_x_force_y_lo)) << 1 | ((uint32_t)torque_z_diff + torque_z_diff > 0xFFFF)));
    torque_x = fx_ldw(0x0068);
    turn_x_base = fx_ldw(0x233C);
    torque_x_x2 = (uint16_t)(torque_x + torque_x);
    fx_sbk((uint16_t)((uint16_t)(torque_x_x2 + torque_x_x2) + turn_x_base));
    torque_z = fx_ldw(0x002E);
    turn_z_base = fx_ldw(0x233E);
    torque_z_x2 = (uint16_t)(torque_z + torque_z);
    fx_sbk((uint16_t)((uint16_t)(torque_z_x2 + torque_z_x2) + turn_z_base));
    torque_y = fx_ldw(0x002C);
    spin = fx_ldw(FX_SPIN);
    torque_y_x2 = (uint16_t)(torque_y + torque_y);
    fx_sbk((uint16_t)((uint16_t)(torque_y_x2 + torque_y_x2) + spin));
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxPushAtQuarterResult){turn_x_base, turn_z_base, (uint16_t)(torque_z_diff + torque_z_diff)};
}


/*
 * @routine 04:9A4A FxPushAt4Twist
 * WHAT IT DOES: FxPushAt4, then the cross product's x ($0068) eight times
 * more into the spin sum at $233C.
 * HOW: R11 pushed on the R10 stack around the call; three ADDs and SBK.
 * Not reached in the traces so far.
 * Entry: as FxPushAt4. Exit: as FxPushAt4, R4 = the old spin sum, R11 =
 * $9A53; S, Z from R10.
 */
FxPushAt4TwistResult FxPushAt4Twist(uint16_t offset_x, uint16_t offset_y, uint16_t offset_z, uint16_t *stack)
{
    uint16_t place_y;
    uint16_t cross_x_x2;
    uint16_t cross_x_x4;
    uint16_t cross_x_word;
    uint16_t twist_base_x;
    uint16_t model_record_2;

    fx_spill(stack);
    { FxPushAt4Result r_ = FxPushAt4(offset_x, offset_y, offset_z, stack); place_y = r_.place_y; model_record_2 = r_.model_record; };
    cross_x_word = fx_ldw(0x0068);
    twist_base_x = fx_ldw(0x233C);
    cross_x_x2 = (uint16_t)(cross_x_word + cross_x_word);
    cross_x_x4 = (uint16_t)(cross_x_x2 + cross_x_x2);
    fx_sbk((uint16_t)((uint16_t)(cross_x_x4 + cross_x_x4) + twist_base_x));
    fx_unspill(stack);
    return (FxPushAt4TwistResult){twist_base_x, place_y, model_record_2};
}


/*
 * @routine 04:9A65 FxSumScale
 * WHAT IT DOES: Scales the world sum by R6 as an 8.8 fraction: each word
 * becomes bits 8-23 of its product with R6.
 * HOW: LMULT per word: the high word's low byte (LOB, SWAP) ORed with the
 * low word's high byte (HIB), then three SMs. Returns with MOVE R15,R11.
 * Not reached in the traces so far.
 * Entry: R6 = the scale; R11 = return address. Exit: R1-R3 the new sum;
 * R0 = the high byte part of z, R4 its low byte part; flags from the
 * last OR; the last RAM address $2344.
 */
void FxSumScale(uint16_t scale)
{
    uint16_t world_sum;
    uint16_t world_sum_y;
    uint16_t world_sum_z;
    uint32_t scaled_sum_x;
    uint32_t scaled_sum_y;
    uint32_t scaled_sum_z;

    world_sum = fx_ldw(FX_WORLD_SUM);
    world_sum_y = fx_ldw(0x2342);
    world_sum_z = fx_ldw(0x2344);
    scaled_sum_x = (uint32_t)((int32_t)(int16_t)world_sum * (int16_t)scale);
    scaled_sum_y = (uint32_t)((int32_t)(int16_t)world_sum_y * (int16_t)scale);
    scaled_sum_z = (uint32_t)((int32_t)(int16_t)world_sum_z * (int16_t)scale);
    fx_stw(FX_WORLD_SUM, ((uint16_t)((uint16_t)((uint16_t)(scaled_sum_x >> 16) & 0xFF) << 8 | (uint16_t)((uint16_t)(scaled_sum_x >> 16) & 0xFF) >> 8) | (uint16_t)((uint16_t)scaled_sum_x >> 8)));
    fx_stw(0x2342, ((uint16_t)((uint16_t)((uint16_t)(scaled_sum_y >> 16) & 0xFF) << 8 | (uint16_t)((uint16_t)(scaled_sum_y >> 16) & 0xFF) >> 8) | (uint16_t)((uint16_t)scaled_sum_y >> 8)));
    fx_stw(0x2344, ((uint16_t)((uint16_t)((uint16_t)(scaled_sum_z >> 16) & 0xFF) << 8 | (uint16_t)((uint16_t)(scaled_sum_z >> 16) & 0xFF) >> 8) | (uint16_t)((uint16_t)scaled_sum_z >> 8)));
    return;
}


/*
 * @routine 04:9A9B FxPushAt4
 * WHAT IT DOES: Pushes the object at the point (R1, R2, R3) in its own
 * axes with four times the world sum: the sum, turned into the object's
 * axes, goes to $0132/$013A/$0142, and the point (times 16) crossed with
 * it is added, four times, to the spin sums at $233C, $233E and $233A.
 * HOW: R11, R9, R8 pushed on the R10 stack; two ADDs per word of the
 * world sum; then the part all the variants share (04:9AC2): sixteen
 * times the point to $26F6/$26F8/$26FA, $0134 cleared, the matrix at
 * $00E4 (FMULT, ROL), the cross product (LMULT, SUB, SBC, ADD, ROL) to
 * $0068/$002C/$002E and the sums by SBK. Returns by pulling R15.
 * Entry: R1, R2, R3 = the point; R10 = stack; R11 = return address.
 * Exit: R1-R3 = the point times 16; R0 the last spin sum; R4, R5, R6, R7
 * as the cross product and sums leave them; R8, R9 restored; S, Z from
 * R10; the last RAM address R10. Its variants are picked by $2654
 * through the table at $04:9BC6.
 */
FxPushAt4Result FxPushAt4(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack)
{
    uint16_t me_z;
    uint16_t spin;
    uint16_t sum_x;
    uint16_t sum_z;
    uint16_t push_x;
    uint16_t push_y;
    uint16_t push_z;
    uint16_t haze_x8;
    uint32_t v0_by_m0;
    uint32_t v0_by_m3;
    uint32_t v0_by_m6;
    uint32_t v4_by_m1;
    uint32_t v4_by_m4;
    uint32_t v4_by_m7;
    uint32_t v8_by_m2;
    uint32_t v8_by_m5;
    uint32_t v8_by_m8;
    uint16_t world_sum;
    uint16_t point_x_x4;
    uint16_t point_x_x8;
    uint16_t point_y_x4;
    uint16_t point_y_x8;
    uint16_t point_z_x4;
    uint16_t point_z_x8;
    uint16_t world_y_sum;
    uint16_t world_z_sum;
    uint16_t cross_x_low_1;
    uint16_t cross_x_low_2;
    uint16_t cross_y_low_1;
    uint16_t cross_y_low_2;
    uint16_t cross_z_low_1;
    uint16_t cross_z_low_2;
    uint16_t point_x_local;
    uint16_t view_matrix_0;
    uint16_t view_matrix_4;
    uint16_t view_matrix_8;
    uint32_t cross_x_prod_1;
    uint32_t cross_x_prod_2;
    uint32_t cross_y_prod_1;
    uint32_t cross_y_prod_2;
    uint32_t cross_z_prod_1;
    uint32_t cross_z_prod_2;
    uint16_t push_x_doubled;
    uint16_t push_y_doubled;
    uint16_t push_z_doubled;
    uint16_t object_matrix_0;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t object_matrix_3;
    uint16_t object_matrix_4;
    uint16_t object_matrix_5;
    uint16_t object_matrix_6;
    uint16_t object_matrix_7;
    uint16_t object_matrix_8;
    uint16_t point_x_doubled;
    uint16_t point_y_doubled;
    uint16_t point_z_doubled;
    uint16_t view_matrix_0_2;
    uint16_t view_matrix_0_3;
    uint16_t view_matrix_4_2;
    uint16_t view_matrix_4_3;
    uint16_t view_matrix_8_2;
    uint16_t view_matrix_8_3;
    uint16_t world_x_doubled;
    uint16_t world_y_doubled;
    uint16_t world_z_doubled;
    uint16_t cross_x_low_diff;
    uint16_t cross_y_low_diff;
    uint16_t cross_z_low_diff;

    fx_spill(stack);
    fx_spill(stack);
    fx_spill(stack);
    world_sum = fx_ldw(FX_WORLD_SUM);
    world_x_doubled = (uint16_t)(world_sum + world_sum);
    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)(world_x_doubled + world_x_doubled));
    world_y_sum = fx_ldw(0x2342);
    world_y_doubled = (uint16_t)(world_y_sum + world_y_sum);
    fx_stw(FX_VIEW_MATRIX_4, (uint16_t)(world_y_doubled + world_y_doubled));
    world_z_sum = fx_ldw(0x2344);
    world_z_doubled = (uint16_t)(world_z_sum + world_z_sum);
    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)(world_z_doubled + world_z_doubled));
    point_x_doubled = (uint16_t)(point_x + point_x);
    point_y_doubled = (uint16_t)(point_y + point_y);
    point_z_doubled = (uint16_t)(point_z + point_z);
    point_x_x4 = (uint16_t)(point_x_doubled + point_x_doubled);
    point_y_x4 = (uint16_t)(point_y_doubled + point_y_doubled);
    point_z_x4 = (uint16_t)(point_z_doubled + point_z_doubled);
    point_x_x8 = (uint16_t)(point_x_x4 + point_x_x4);
    point_y_x8 = (uint16_t)(point_y_x4 + point_y_x4);
    point_z_x8 = (uint16_t)(point_z_x4 + point_z_x4);
    fx_stw(FX_VIEW_MATRIX_1, 0);
    fx_stw(0x26F6, (uint16_t)(point_x_x8 + point_x_x8));
    fx_stw(FX_HAZE_X8, (uint16_t)(point_y_x8 + point_y_x8));
    fx_stw(FX_ME_Z, (uint16_t)(point_z_x8 + point_z_x8));
    view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
    view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
    view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
    object_matrix_0 = fx_ldw(0x00E4);
    v0_by_m0 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_0);
    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
    v4_by_m1 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_1);
    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
    v8_by_m2 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_2);
    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(v8_by_m2 >> 16) << 1 | (int)(v8_by_m2 >> 15 & 1)) + (uint16_t)((uint16_t)(v0_by_m0 >> 16) << 1 | (int)(v0_by_m0 >> 15 & 1))) + (uint16_t)((uint16_t)(v4_by_m1 >> 16) << 1 | (int)(v4_by_m1 >> 15 & 1))));
    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
    v0_by_m3 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_3);
    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
    v4_by_m4 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_4);
    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
    v8_by_m5 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_5);
    fx_stw(FX_VIEW_MATRIX_4, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(v8_by_m5 >> 16) << 1 | (int)(v8_by_m5 >> 15 & 1)) + (uint16_t)((uint16_t)(v0_by_m3 >> 16) << 1 | (int)(v0_by_m3 >> 15 & 1))) + (uint16_t)((uint16_t)(v4_by_m4 >> 16) << 1 | (int)(v4_by_m4 >> 15 & 1))));
    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
    v0_by_m6 = (uint32_t)((int32_t)(int16_t)view_matrix_0 * (int16_t)object_matrix_6);
    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
    v4_by_m7 = (uint32_t)((int32_t)(int16_t)view_matrix_4 * (int16_t)object_matrix_7);
    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
    v8_by_m8 = (uint32_t)((int32_t)(int16_t)view_matrix_8 * (int16_t)object_matrix_8);
    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(v8_by_m8 >> 16) << 1 | (int)(v8_by_m8 >> 15 & 1)) + (uint16_t)((uint16_t)(v0_by_m6 >> 16) << 1 | (int)(v0_by_m6 >> 15 & 1))) + (uint16_t)((uint16_t)(v4_by_m7 >> 16) << 1 | (int)(v4_by_m7 >> 15 & 1))));
    point_x_local = fx_ldw(0x26F6);
    haze_x8 = fx_ldw(FX_HAZE_X8);
    me_z = fx_ldw(FX_ME_Z);
    view_matrix_4_2 = fx_ldw(FX_VIEW_MATRIX_4);
    cross_x_prod_1 = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_4_2);
    cross_x_low_1 = (uint16_t)cross_x_prod_1;
    view_matrix_8_2 = fx_ldw(FX_VIEW_MATRIX_8);
    cross_x_prod_2 = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_8_2);
    cross_x_low_2 = (uint16_t)cross_x_prod_2;
    cross_x_low_diff = (uint16_t)(cross_x_low_1 - cross_x_low_2);
    fx_stw(0x0068, (uint16_t)((uint16_t)((uint16_t)(cross_x_prod_1 >> 16) - (uint16_t)(cross_x_prod_2 >> 16) - (cross_x_low_1 < cross_x_low_2)) << 1 | ((uint32_t)cross_x_low_diff + cross_x_low_diff > 0xFFFF)));
    view_matrix_8_3 = fx_ldw(FX_VIEW_MATRIX_8);
    cross_y_prod_1 = (uint32_t)((int32_t)(int16_t)point_x_local * (int16_t)view_matrix_8_3);
    cross_y_low_1 = (uint16_t)cross_y_prod_1;
    view_matrix_0_2 = fx_ldw(FX_VIEW_MATRIX_0);
    cross_y_prod_2 = (uint32_t)((int32_t)(int16_t)me_z * (int16_t)view_matrix_0_2);
    cross_y_low_2 = (uint16_t)cross_y_prod_2;
    cross_y_low_diff = (uint16_t)(cross_y_low_1 - cross_y_low_2);
    fx_stw(0x002C, (uint16_t)((uint16_t)((uint16_t)(cross_y_prod_1 >> 16) - (uint16_t)(cross_y_prod_2 >> 16) - (cross_y_low_1 < cross_y_low_2)) << 1 | ((uint32_t)cross_y_low_diff + cross_y_low_diff > 0xFFFF)));
    view_matrix_0_3 = fx_ldw(FX_VIEW_MATRIX_0);
    cross_z_prod_1 = (uint32_t)((int32_t)(int16_t)haze_x8 * (int16_t)view_matrix_0_3);
    cross_z_low_1 = (uint16_t)cross_z_prod_1;
    view_matrix_4_3 = fx_ldw(FX_VIEW_MATRIX_4);
    cross_z_prod_2 = (uint32_t)((int32_t)(int16_t)point_x_local * (int16_t)view_matrix_4_3);
    cross_z_low_2 = (uint16_t)cross_z_prod_2;
    cross_z_low_diff = (uint16_t)(cross_z_low_1 - cross_z_low_2);
    fx_stw(0x002E, (uint16_t)((uint16_t)((uint16_t)(cross_z_prod_1 >> 16) - (uint16_t)(cross_z_prod_2 >> 16) - (cross_z_low_1 < cross_z_low_2)) << 1 | ((uint32_t)cross_z_low_diff + cross_z_low_diff > 0xFFFF)));
    push_x = fx_ldw(0x0068);
    sum_x = fx_ldw(0x233C);
    push_x_doubled = (uint16_t)(push_x + push_x);
    fx_sbk((uint16_t)((uint16_t)(push_x_doubled + push_x_doubled) + sum_x));
    push_z = fx_ldw(0x002E);
    sum_z = fx_ldw(0x233E);
    push_z_doubled = (uint16_t)(push_z + push_z);
    fx_sbk((uint16_t)((uint16_t)(push_z_doubled + push_z_doubled) + sum_z));
    push_y = fx_ldw(0x002C);
    spin = fx_ldw(FX_SPIN);
    push_y_doubled = (uint16_t)(push_y + push_y);
    fx_sbk((uint16_t)((uint16_t)(push_y_doubled + push_y_doubled) + spin));
    fx_unspill(stack);
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxPushAt4Result){sum_x, sum_z, (uint16_t)(cross_z_low_diff + cross_z_low_diff)};
}

