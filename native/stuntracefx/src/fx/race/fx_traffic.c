/*
 * fx_traffic.c: the Super FX code of asm/stuntracefx/race/fx_traffic.asm, written from it by
 * the generator.
 *
 * fx_traffic.asm: the Super FX looks for cars about to be caught up with.
 *
 * For car R8, every other car on the cars' list ($70:262E, linked through
 * +$74) that is not flagged out (bit 10 of +$16) and not faster (+$4A) is
 * tested: it counts when the closing speed along the line between them,
 * plus 20, is at least their distance (|dx| + |dz|) / 32, and it is less
 * than $200 to the side (along the first column of the matrix at $00E4).
 * Bits 0 and 1 of R8's +$42 then say on which side it is.
 * C: src/stuntracefx/race/fx_traffic.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:C933 FxTrafficSides
 * WHAT IT DOES: As described above: for each car that counts, bit 1 of R8's
 * +$42 is set when it is to the left (its offset negative) and bit 0
 * cleared if it was set when the routine started; to the right, bit 0 set
 * and bit 1 cleared likewise. $2638 = -1 for a car $200 or more to the
 * side.
 * HOW: R8's position and velocity are copied to $26F6, $26FA, $26FC,
 * $2700; per car, the difference goes to $0068/$002C/$002E, the distance
 * / 32 (DIV2 five times) to $263A, the closing speed (FMULT, ROL) plus 20
 * to $2638, then the side offset to $263A. R9 holds +$42 as it was at the
 * start; the bits are changed in R6, read again per car. The return
 * address is kept on the R10 stack.
 * Entry: R8 = the car's record; R10 = stack; R11 = return address. Exit:
 * R7 = 0; R9 = the old +$42; the other registers from the last car tested;
 * S, Z from R10; the last RAM address R10.
 */
FxTrafficSidesResult FxTrafficSides(uint16_t entry_scratch_b, uint16_t car_record, uint16_t *stack)
{
    uint16_t cars;
    uint16_t me_x;
    uint16_t me_z;
    uint16_t me_vz;
    uint16_t sides;
    uint16_t speed;
    uint16_t abs_dx;
    uint16_t abs_dz;
    uint16_t half_x;
    uint16_t me_z_2;
    uint16_t flags16;
    uint16_t me_vz_2;
    uint16_t other_z;
    uint16_t sides_2;
    uint16_t speed_2;
    uint16_t half_x_2;
    uint16_t other_vz;
    uint16_t velocity;
    uint16_t dx_to_car;
    uint16_t dz_to_car;
    uint16_t other_car;
    uint16_t stored_dx;
    uint16_t stored_dy;
    uint16_t stored_dz;
    uint16_t velocity_2;
    uint16_t closing_dot;
    uint16_t me_matrix_0;
    uint16_t me_velocity;
    uint16_t side_offset;
    uint32_t side_x_prod;
    uint32_t side_y_prod;
    uint32_t side_z_prod;
    uint16_t traffic_dist;
    uint16_t traffic_close;
    uint32_t closing_x_prod;
    uint32_t closing_y_prod;
    uint32_t closing_z_prod;
    uint16_t abs_side_offset;
    uint16_t object_matrix_1;
    uint16_t object_matrix_2;
    uint16_t stored_dx_again;
    uint16_t stored_dy_again;
    uint16_t stored_dz_again;

    fx_spill(stack);
    sides = fx_ldw((uint16_t)(FXREC_SIDES + car_record));
    half_x = fx_ldw((uint16_t)(FXREC_HALF_X + car_record));
    fx_stw(0x26F6, half_x);
    me_z = fx_ldw((uint16_t)(car_record + 0x0022));
    fx_stw(FX_ME_Z, me_z);
    velocity = fx_ldw((uint16_t)(FXREC_VELOCITY + car_record));
    fx_stw(0x26FC, velocity);
    me_vz = fx_ldw((uint16_t)(car_record + 0x0032));
    fx_stw(FX_ME_VZ, me_vz);
    cars = fx_ldw(FX_CARS);
    for (;;) {
        other_car = cars;
        if (other_car != 0) {
            if (cars != car_record) {
                flags16 = fx_ldw((uint16_t)(FXREC_FLAGS16 + other_car));
                if ((0x0400 & flags16) == 0) {
                    speed = fx_ldw((uint16_t)(FXREC_SPEED + other_car));
                    speed_2 = fx_ldw((uint16_t)(FXREC_SPEED + car_record));
                    if (!((uint16_t)(speed_2 - speed) & 0x8000)) {
                        half_x_2 = fx_ldw((uint16_t)(FXREC_HALF_X + other_car));
                        me_x = fx_ldw(0x26F6);
                        dx_to_car = (uint16_t)(half_x_2 - me_x);
                        fx_stw(0x0068, dx_to_car);
                        abs_dx = dx_to_car;
                        if (!(dx_to_car & 0x8000)) {
                        } else {
                            abs_dx = (uint16_t)((uint16_t)~abs_dx + 1);
                        }
                        other_z = fx_ldw((uint16_t)(other_car + 0x0022));
                        me_z_2 = fx_ldw(FX_ME_Z);
                        dz_to_car = (uint16_t)(other_z - me_z_2);
                        fx_stw(0x002E, dz_to_car);
                        abs_dz = dz_to_car;
                        if (!(dz_to_car & 0x8000)) {
                        } else {
                            abs_dz = (uint16_t)((uint16_t)~abs_dz + 1);
                        }
                        fx_stw(0x002C, 0);
                        fx_stw(FX_TRAFFIC_DIST, fx_div2(fx_div2(fx_div2(fx_div2(fx_div2((uint16_t)(abs_dx + abs_dz)))))));
                        velocity_2 = fx_ldw((uint16_t)(FXREC_VELOCITY + other_car));
                        me_velocity = fx_ldw(0x26FC);
                        other_vz = fx_ldw((uint16_t)(other_car + 0x0032));
                        me_vz_2 = fx_ldw(FX_ME_VZ);
                        stored_dx = fx_ldw(0x0068);
                        closing_x_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(me_velocity - velocity_2) * (int16_t)stored_dx);
                        stored_dy = fx_ldw(0x002C);
                        closing_y_prod = (uint32_t)((int32_t)(int16_t)0 * (int16_t)stored_dy);
                        entry_scratch_b = (uint16_t)((uint16_t)((uint16_t)(closing_x_prod >> 16) << 1 | (int)(closing_x_prod >> 15 & 1)) + (uint16_t)((uint16_t)(closing_y_prod >> 16) << 1 | (int)(closing_y_prod >> 15 & 1)));
                        stored_dz = fx_ldw(0x002E);
                        closing_z_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(me_vz_2 - other_vz) * (int16_t)stored_dz);
                        closing_dot = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(closing_z_prod >> 16) << 1 | (int)(closing_z_prod >> 15 & 1)) + entry_scratch_b) + 0x0014);
                        fx_stw(FX_TRAFFIC_CLOSE, closing_dot);
                        traffic_dist = fx_ldw(FX_TRAFFIC_DIST);
                        if (!((uint16_t)(closing_dot - traffic_dist) & 0x8000)) {
                            me_matrix_0 = fx_ldw(0x00E4);
                            object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
                            object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
                            stored_dx_again = fx_ldw(0x0068);
                            side_x_prod = (uint32_t)((int32_t)(int16_t)me_matrix_0 * (int16_t)stored_dx_again);
                            stored_dy_again = fx_ldw(0x002C);
                            side_y_prod = (uint32_t)((int32_t)(int16_t)object_matrix_1 * (int16_t)stored_dy_again);
                            entry_scratch_b = (uint16_t)((uint16_t)((uint16_t)(side_x_prod >> 16) << 1 | (int)(side_x_prod >> 15 & 1)) + (uint16_t)((uint16_t)(side_y_prod >> 16) << 1 | (int)(side_y_prod >> 15 & 1)));
                            stored_dz_again = fx_ldw(0x002E);
                            side_z_prod = (uint32_t)((int32_t)(int16_t)object_matrix_2 * (int16_t)stored_dz_again);
                            side_offset = (uint16_t)((uint16_t)((uint16_t)(side_z_prod >> 16) << 1 | (int)(side_z_prod >> 15 & 1)) + entry_scratch_b);
                            fx_stw(FX_TRAFFIC_DIST, side_offset);
                            abs_side_offset = side_offset;
                            if (!(side_offset & 0x8000)) {
                            } else {
                                abs_side_offset = (uint16_t)((uint16_t)~abs_side_offset + 1);
                            }
                            if ((uint16_t)(abs_side_offset - 0x0200) & 0x8000) {
                            } else {
                                fx_stw(FX_TRAFFIC_CLOSE, 0xFFFF);
                            }
                            sides_2 = fx_ldw((uint16_t)(FXREC_SIDES + car_record));
                            traffic_close = fx_ldw(FX_TRAFFIC_CLOSE);
                            if (traffic_close & 0x8000) {
                            } else {
                                if (!(side_offset & 0x8000)) {
                                    if ((2 & sides) == 0) {
                                    } else {
                                        sides_2 = (0xFFFD & sides_2);
                                    }
                                    sides_2 = (1 | sides_2);
                                } else {
                                    if ((1 & sides) == 0) {
                                    } else {
                                        sides_2 = (0xFFFE & sides_2);
                                    }
                                    sides_2 = (2 | sides_2);
                                }
                            }
                            fx_stw((uint16_t)(FXREC_SIDES + car_record), sides_2);
                        } else {
                        }
                    } else {
                    }
                } else {
                }
            } else {
            }
            cars = fx_ldw((uint16_t)(FXREC_NEXT_CAR + other_car));
            continue;
        } else {
            break;
        }
    }
    fx_unspill(stack);
    return (FxTrafficSidesResult){entry_scratch_b, other_car, sides};
}


/*
 * @routine 04:BF98 FxSpreadCars
 * WHAT IT DOES: Pushes the other cars near car R8 away from it: for each
 * car on the cars' list (not R8) whose distance (|dx| + |dy| + |dz|) / 2
 * is below 2000, its velocity x and z gain twice (2000 - that) along the
 * unit vector from R8 to it, its +$34 gains or loses a share of it (by the
 * sign of +$34: times $C180 or $3E80, scaled), and its +$7C = 30. A
 * negative half z distance is made positive by code decoded from the ROM
 * bytes and not reached in the traces so far.
 * HOW: R8's position in R6, R5, R7 and R2 = 2000; per car, the differences
 * (x into R1, z into R3) halved with DIV2; for a near car R8, R6, R5, R7,
 * R2, the push and the car are kept on the R10 stack around FxNormalize of
 * (dx, 0, dz), and each push is FMULT then ADD and SBK. R8 and the return
 * address are kept on the R10 stack.
 * Entry: R8 = the car's record; R10 = stack; R11 = return address. Exit:
 * R9 = 0; R8, R6, R5, R7, R2 as set up; the others from the last car; S,
 * Z from R10; the last RAM address R10.
 */
FxSpreadCarsResult FxSpreadCars(uint16_t entry_distance, uint16_t car_record, uint16_t *stack)
{
    uint16_t y;
    uint16_t y_2;
    uint16_t cars;
    uint16_t car_z;
    uint16_t half_x;
    uint16_t half_dx;
    uint16_t half_dy;
    uint16_t half_dz;
    uint16_t half_x_2;
    uint16_t velocity;
    uint16_t yaw_rate;
    uint16_t dx_double;
    uint16_t dz_double;
    uint16_t other_car;
    uint16_t spin_gain;
    uint16_t abs_half_dx;
    uint16_t abs_half_dy;
    uint16_t abs_half_dz;
    uint16_t other_car_z;
    uint16_t overlap_abs;
    uint16_t push_strength;
    uint16_t spread_radius;
    uint16_t other_car_copy;
    uint16_t scaled_vector_x;
    uint16_t scaled_vector_z;
    uint16_t abs_half_dx_copy;
    uint16_t other_velocity_z;
    uint16_t push_strength_x2;
    uint16_t distance_minus_radius;

    fx_spill(stack);
    fx_push(stack, car_record);
    half_x = fx_ldw((uint16_t)(FXREC_HALF_X + car_record));
    y = fx_ldw((uint16_t)(car_record + FXREC_Y));
    car_z = fx_ldw((uint16_t)(car_record + 0x0022));
    spread_radius = 0x07D0;
    cars = fx_ldw(FX_CARS);
    for (;;) {
        other_car_copy = cars;
        other_car = other_car_copy;
        if (other_car_copy != 0) {
            if (cars != car_record) {
                half_x_2 = fx_ldw((uint16_t)(FXREC_HALF_X + other_car));
                dx_double = (uint16_t)(half_x_2 - half_x);
                half_dx = fx_div2(dx_double);
                abs_half_dx = half_dx;
                if (!(half_dx & 0x8000)) {
                } else {
                    abs_half_dx = (uint16_t)((uint16_t)~abs_half_dx + 1);
                }
                abs_half_dx_copy = abs_half_dx;
                y_2 = fx_ldw((uint16_t)(other_car + FXREC_Y));
                half_dy = fx_div2((uint16_t)(y_2 - y));
                abs_half_dy = half_dy;
                if (!(half_dy & 0x8000)) {
                } else {
                    abs_half_dy = (uint16_t)((uint16_t)~abs_half_dy + 1);
                }
                entry_distance = (uint16_t)(abs_half_dx_copy + abs_half_dy);
                other_car_z = fx_ldw((uint16_t)(other_car + 0x0022));
                dz_double = (uint16_t)(other_car_z - car_z);
                half_dz = fx_div2(dz_double);
                abs_half_dz = half_dz;
                if (!(half_dz & 0x8000)) {
                } else {
                    abs_half_dz = (uint16_t)((uint16_t)~abs_half_dz + 1);
                }
                distance_minus_radius = (uint16_t)((uint16_t)(abs_half_dz + entry_distance) - spread_radius);
                if (!(distance_minus_radius & 0x8000)) {
                } else {
                    fx_push(stack, car_record);
                    fx_push(stack, half_x);
                    fx_push(stack, y);
                    fx_push(stack, car_z);
                    fx_push(stack, spread_radius);
                    overlap_abs = distance_minus_radius;
                    if (!(distance_minus_radius & 0x8000)) {
                    } else {
                        overlap_abs = (uint16_t)((uint16_t)~overlap_abs + 1);
                    }
                    fx_push(stack, (uint16_t)(overlap_abs + overlap_abs));
                    fx_push(stack, other_car);
                    { FxNormalizeResult r_ = FxNormalize(dx_double, 0, dz_double, car_z, other_car, stack); scaled_vector_x = r_.scaled_vector_x; scaled_vector_z = r_.scaled_vector_z; };
                    other_car = fx_pull(stack);
                    push_strength = fx_pull(stack);
                    velocity = fx_ldw((uint16_t)(FXREC_VELOCITY + other_car));
                    fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)scaled_vector_x * (int16_t)push_strength) >> 16) + velocity));
                    other_velocity_z = fx_ldw((uint16_t)(other_car + 0x0032));
                    fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)scaled_vector_z * (int16_t)push_strength) >> 16) + other_velocity_z));
                    yaw_rate = fx_ldw((uint16_t)(FXREC_HEADING_RATE + other_car));
                    spin_gain = 0xC180;
                    if (yaw_rate & 0x8000) {
                    } else {
                        spin_gain = 0x3E80;
                    }
                    push_strength_x2 = (uint16_t)(push_strength + push_strength);
                    fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)spin_gain * (int16_t)(uint16_t)(push_strength_x2 + push_strength_x2)) >> 16) + yaw_rate));
                    entry_distance = 0x001E;
                    fx_stw((uint16_t)(FXREC_SPREAD + other_car), entry_distance);
                    spread_radius = fx_pull(stack);
                    car_z = fx_pull(stack);
                    y = fx_pull(stack);
                    half_x = fx_pull(stack);
                    car_record = fx_pull(stack);
                }
            } else {
            }
            cars = fx_ldw((uint16_t)(FXREC_NEXT_CAR + other_car));
            continue;
        } else {
            break;
        }
    }
    fx_unspill(stack);
    fx_unspill(stack);
    return (FxSpreadCarsResult){entry_distance, y, car_z, other_car};
}


/*
 * @routine 04:CF08 FxNearestCar
 * WHAT IT DOES: Finds the car nearest the player's car ($2476; from the
 * view's position $00D8/$00DC instead when bit 3 of its +$64 is set) by
 * |dx| + |dz|, keeping each car's distance in its +$8C, and for the
 * nearest: $2648 = the car; $2642 = a smoothed level from how fast it
 * closed ((old distance - new) / 2 + $80, 0 at least, 255 at most, a
 * quarter of the way from the old level each step); $2646 = its distance
 * / 512, 7 at most; $264A/$264C = its offset turned into the view's axes
 * (FxTurn by the high byte of $00E0), x negated; $2644 = 7 + that z / 32,
 * 0 to 15. With no player car, no other car or no old distance: $2642 =
 * -1, $2648 = 0. The case of an empty cars' list is decoded from the ROM
 * bytes and not reached in the traces so far.
 * HOW: R5 = the best distance (from $1000), R7 its car, R6 its old +$8C;
 * the sums are checked for overflow (BVS). The return address is kept on
 * the R10 stack. The walk runs under CACHE (the code cache only; nothing
 * the C has to do).
 * Entry: R10 = stack; R11 = return address. Exit: R0 = the side value or
 * 0; R7 = the nearest car; R11 = $D002 when it got that far; S, Z from
 * R10; the last RAM address R10.
 */
void FxNearestCar(uint16_t prev_nearest_car, uint16_t *stack)
{
    uint16_t cars;
    uint16_t new_z;
    uint16_t dx_abs;
    uint16_t dz_abs;
    uint16_t half_x;
    uint16_t me_car;
    uint16_t dx_kept;
    uint16_t new_x_x;
    uint16_t fxcar_64;
    uint16_t fxcar_8c;
    uint16_t half_x_2;
    uint16_t half_x_3;
    uint16_t next_car;
    uint16_t side_raw;
    uint16_t dx_signed;
    uint16_t dz_signed;
    uint16_t near_band;
    uint16_t near_side;
    uint16_t me_car_raw;
    uint16_t param_00dc;
    uint16_t param_00e0;
    uint16_t candidate_z;
    uint16_t lead_gap_raw;
    uint16_t smoothed_gap;
    uint16_t best_distance;
    uint16_t candidate_car;
    uint16_t nearest_car_z;
    uint16_t lead_gap_clamped;
    uint16_t smoothed_gap_init;
    uint16_t manhattan_distance;
    uint16_t smoothed_gap_check;
    uint16_t smoothed_gap_stored;

    fx_spill(stack);
    me_car_raw = fx_ldw(0x2476);
    me_car = me_car_raw;
    if (me_car_raw != 0) {
        half_x = fx_ldw((uint16_t)(FXREC_HALF_X + me_car));
        param_00dc = fx_ldw((uint16_t)(me_car + 0x0022));
        fxcar_64 = fx_ldw((uint16_t)(FXCAR_64 + me_car));
        if ((8 & fxcar_64) == 0) {
        } else {
            me_car = 0;
            half_x = fx_ldw(GSU_PARAM_00D8);
            param_00dc = fx_ldw(GSU_PARAM_00DC);
        }
        best_distance = 0x1000;
        fxcar_8c = 0;
        cars = fx_ldw(FX_CARS);
        next_car = cars;
        if (cars != 0) {
            for (;;) {
                candidate_car = next_car;
                if (candidate_car == 0) {
                    break;
                } else {
                    if (next_car == me_car) {
                    } else {
                        half_x_2 = fx_ldw((uint16_t)(FXREC_HALF_X + candidate_car));
                        dx_signed = (uint16_t)(half_x_2 - half_x);
                        dx_abs = dx_signed;
                        if (!(dx_signed & 0x8000)) {
                        } else {
                            dx_abs = (uint16_t)((uint16_t)~dx_abs + 1);
                        }
                        dx_kept = dx_abs;
                        candidate_z = fx_ldw((uint16_t)(candidate_car + 0x0022));
                        dz_signed = (uint16_t)(candidate_z - param_00dc);
                        dz_abs = dz_signed;
                        if (!(dz_signed & 0x8000)) {
                        } else {
                            dz_abs = (uint16_t)((uint16_t)~dz_abs + 1);
                        }
                        manhattan_distance = (uint16_t)(dx_kept + dz_abs);
                        if (((~(dx_kept ^ dz_abs) & (dz_abs ^ (uint16_t)(dx_kept + dz_abs + 0)) & 0x8000) != 0)) {
                        } else {
                            if (!((uint16_t)(manhattan_distance - best_distance) & 0x8000)) {
                            } else {
                                fxcar_8c = fx_ldw((uint16_t)(FXCAR_8C + candidate_car));
                                best_distance = manhattan_distance;
                                prev_nearest_car = candidate_car;
                            }
                            fx_stw((uint16_t)(FXCAR_8C + candidate_car), manhattan_distance);
                        }
                    }
                    next_car = fx_ldw((uint16_t)(FXREC_NEXT_CAR + candidate_car));
                    continue;
                }
            }
            if (fxcar_8c != 0) {
                lead_gap_raw = (uint16_t)(fx_div2((uint16_t)(fxcar_8c - best_distance)) + 0x0080);
                lead_gap_clamped = lead_gap_raw;
                if (!(lead_gap_raw & 0x8000)) {
                } else {
                    lead_gap_clamped = 0;
                }
                smoothed_gap = fx_ldw(0x2642);
                smoothed_gap_init = smoothed_gap;
                if (!(smoothed_gap & 0x8000)) {
                } else {
                    smoothed_gap_init = lead_gap_clamped;
                    fx_stw(0x2642, smoothed_gap_init);
                }
                smoothed_gap_stored = fx_ldw(0x2642);
                fx_sbk((uint16_t)(smoothed_gap_stored - fx_div2(fx_div2((uint16_t)(smoothed_gap_init - lead_gap_clamped)))));
                near_band = (uint16_t)((uint32_t)((int32_t)(int16_t)best_distance * (int16_t)0x0080) >> 16);
                if ((uint16_t)(near_band - 7) & 0x8000) {
                } else {
                    near_band = 7;
                }
                fx_stw(FX_NEAR_BAND, near_band);
                smoothed_gap_check = fx_ldw(0x2642);
                if ((uint16_t)(smoothed_gap_check - 0x00FF) & 0x8000) {
                } else {
                    fx_sbk(0x00FF);
                }
                fx_stw(FX_NEAR_CAR, prev_nearest_car);
                half_x_3 = fx_ldw((uint16_t)(FXREC_HALF_X + prev_nearest_car));
                nearest_car_z = fx_ldw((uint16_t)(prev_nearest_car + 0x0022));
                param_00e0 = fx_ldw(GSU_PARAM_00E0);
                fx_romb(0);
                { FxTurnResult r_ = FxTurn((uint16_t)(half_x_3 - half_x), (uint16_t)(param_00e0 >> 8), (uint16_t)(nearest_car_z - param_00dc), stack); new_x_x = r_.new_x_x; new_z = r_.new_z; };
                fx_stw(FX_NEAR_Z, new_z);
                fx_stw(FX_NEAR_X, (uint16_t)((uint16_t)~new_x_x + 1));
                side_raw = (uint16_t)(fx_div2(fx_div2(fx_div2(fx_div2(fx_div2(new_z))))) + 7);
                near_side = side_raw;
                if (!(side_raw & 0x8000)) {
                } else {
                    near_side = 0;
                }
                if ((uint16_t)(near_side - 0x000F) & 0x8000) {
                } else {
                    near_side = 0x000F;
                }
                fx_stw(FX_NEAR_SIDE, near_side);
                fx_unspill(stack);
                return;
            } else {
                fx_stw(0x2642, 0xFFFF);
                fx_stw(FX_NEAR_CAR, 0);
                fx_unspill(stack);
                return;
            }
        } else {
            fx_stw(0x2642, 0xFFFF);
            fx_stw(FX_NEAR_CAR, 0);
            fx_unspill(stack);
            return;
        }
    } else {
        fx_stw(0x2642, 0xFFFF);
        fx_stw(FX_NEAR_CAR, 0);
        fx_unspill(stack);
        return;
    }
}

