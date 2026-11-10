/*
 * fx_drive.c: the Super FX code of asm/stuntracefx/race/fx_drive.asm, written from it by
 * the generator.
 *
 * fx_drive.asm: the Super FX drives a computer car.
 *
 * The decision goes to the car's +$42 as bits: 0 steer one way, 1 the
 * other, 2 and 3 their harder forms, 4 accelerate, 5 a reversing turn, 6
 * brake, 10 a boost. +$7C counts down a period of not accelerating (set
 * when cars are pushed apart, race/fx_traffic.asm); +$7E counts how long the
 * car has been stuck.
 * C: src/stuntracefx/race/fx_drive.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:C7D5 FxDriveDecide
 * WHAT IT DOES: Decides computer car R8's controls toward the point (R1,
 * R3) at speed R4 into +$42 (bits above). The heading error is the bearing
 * (FxBearingTo) less +$24, less +$3A: from -$38E to $38E nothing, beyond
 * it steer (bit 1 to the right, bit 0 to the left), and beyond $FA4 also
 * no accelerating; with bit 14 of +$40 the harder form too (decoded from
 * the ROM bytes and not reached in the traces so far). Bit 6 when at or
 * above R4, else bit 10 when R4 is $227 or more, else bit 4; bit 4 also
 * below speed 200. A running +$7C counts down and clears bits 4 and 10.
 * +$7E counts up from -40 to -20 for a reversing turn (bits 0, 1 swapped,
 * bit 5) once it has reached 20 while slower than 10, and down otherwise.
 * Bits 4 and 10 are cleared while +$44 is not 0. Bit 8 of +$40 takes a
 * quarter off +$26; bit 5 of +$70 makes R4 three quarters (decoded from
 * the ROM bytes and not reached in the traces so far).
 * HOW: R9 collects the bits; limits are CMP and the sign. R4 and the
 * return address are kept on the R10 stack.
 * Entry: R1, R3 = the point; R4 = the speed wanted; R8 = the car's record;
 * R10 = stack; R11 = return address. Exit: R9 = the bits; R4 as used; R1
 * the heading error or the last limit; the others from the tests and
 * FxBearingTo; S, Z from R10; the last RAM address R10.
 */
FxDriveDecideResult FxDriveDecide(uint16_t point_x, uint16_t point_z, uint16_t speed_wanted, uint16_t car_record, uint16_t *stack, uint16_t rom_at)
{
    uint16_t yaw;
    uint16_t angle;
    uint16_t pitch;
    uint16_t sides;
    uint16_t speed;
    uint16_t stuck;
    uint16_t spread;
    uint16_t flags40;
    uint16_t speed_2;
    uint16_t speed_3;
    uint16_t stuck_2;
    uint16_t stuck_3;
    uint16_t fxcar_44;
    uint16_t direction;
    uint16_t flags40_2;
    uint16_t flags40_3;
    uint16_t steer_bias;
    uint16_t steer_error;
    uint16_t bearing_error;
    uint16_t stuck_counter;
    uint16_t swap_mark_out;
    uint16_t wanted_speed_half;
    uint16_t next_stuck_counter;
    uint16_t pulled_wanted_speed;

    fx_spill(stack);
    direction = fx_ldw((uint16_t)(FXREC_DIRECTION + car_record));
    if ((direction & 0x0020) == 0) {
    } else {
        wanted_speed_half = fx_div2(speed_wanted);
        speed_wanted = (uint16_t)(wanted_speed_half + fx_div2(wanted_speed_half));
    }
    fx_push(stack, speed_wanted);
    { FxBearingToResult r_ = FxBearingTo(point_x, point_z, 0x0020, car_record, stack, rom_at); angle = r_.angle; swap_mark_out = r_.swap_mark_out; };
    pitch = fx_ldw((uint16_t)(FXREC_HEADING + car_record));
    bearing_error = (uint16_t)(angle - pitch);
    flags40 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
    if ((flags40 & 0x0100) == 0) {
    } else {
        yaw = fx_ldw((uint16_t)(FXREC_PITCH + car_record));
        fx_sbk((uint16_t)(yaw - fx_div2(fx_div2(yaw))));
    }
    pulled_wanted_speed = fx_pull(stack);
    speed = fx_ldw((uint16_t)(FXREC_SPEED + car_record));
    if (!((uint16_t)(speed - pulled_wanted_speed) & 0x8000)) {
        sides = 0x0040;
    } else {
        if ((uint16_t)(pulled_wanted_speed - 0x0227) & 0x8000) {
            sides = 0x0010;
        } else {
            sides = 0x0400;
        }
    }
    steer_bias = fx_ldw((uint16_t)(FXCAR_STEER_BIAS + car_record));
    steer_error = (uint16_t)(bearing_error - steer_bias);
    if (!(steer_error & 0x8000)) {
        if ((uint16_t)(steer_error - 0x038E) & 0x8000) {
        } else {
            if ((uint16_t)(steer_error - 0x0FA4) & 0x8000) {
            } else {
                sides = (0xFFEF & sides);
            }
            sides = (1 | sides);
            flags40_2 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
            if ((flags40_2 & 0x4000) == 0) {
            } else {
                sides = (4 | sides);
            }
        }
    } else {
        if (!((uint16_t)(steer_error + 0x038E) & 0x8000)) {
        } else {
            if (!((uint16_t)(steer_error + 0x0FA4) & 0x8000)) {
            } else {
                sides = (0xFFEF & sides);
            }
            sides = (2 | sides);
            flags40_3 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
            if ((flags40_3 & 0x4000) == 0) {
            } else {
                sides = (8 | sides);
            }
        }
    }
    speed_2 = fx_ldw((uint16_t)(FXREC_SPEED + car_record));
    if (!((uint16_t)(speed_2 - 0x00C8) & 0x8000)) {
    } else {
        sides = (0x0010 | sides);
    }
    spread = fx_ldw((uint16_t)(FXREC_SPREAD + car_record));
    if (spread == 0) {
    } else {
        fx_sbk((uint16_t)(spread - 1));
        sides = (0xFBEF & sides);
    }
    stuck = fx_ldw((uint16_t)(FXCAR_STUCK + car_record));
    stuck_counter = stuck;
    if (stuck & 0x8000) {
        next_stuck_counter = (uint16_t)(stuck_counter + 1);
        fx_sbk(next_stuck_counter);
        if (!((uint16_t)(next_stuck_counter + 0x0014) & 0x8000)) {
        } else {
            sides = (3 ^ (0x0020 | (0xFBAF & sides)));
        }
    } else {
        if ((uint16_t)(stuck_counter - 0x0014) & 0x8000) {
            speed_3 = fx_ldw((uint16_t)(FXREC_SPEED + car_record));
            if (!((uint16_t)(speed_3 - 0x000A) & 0x8000)) {
                stuck_2 = fx_ldw((uint16_t)(FXCAR_STUCK + car_record));
                if (stuck_2 == 0) {
                } else {
                    fx_sbk((uint16_t)(stuck_2 - 1));
                }
            } else {
                stuck_3 = fx_ldw((uint16_t)(FXCAR_STUCK + car_record));
                fx_sbk((uint16_t)(stuck_3 + 1));
            }
        } else {
            stuck_counter = 0xFFD8;
            next_stuck_counter = (uint16_t)(stuck_counter + 1);
            fx_sbk(next_stuck_counter);
            if (!((uint16_t)(next_stuck_counter + 0x0014) & 0x8000)) {
            } else {
                sides = (3 ^ (0x0020 | (0xFBAF & sides)));
            }
        }
    }
    fxcar_44 = fx_ldw((uint16_t)(FXCAR_44 + car_record));
    if (fxcar_44 == 0) {
    } else {
        sides = (0xFBEF & sides);
    }
    fx_stw((uint16_t)(FXREC_SIDES + car_record), sides);
    fx_unspill(stack);
    return (FxDriveDecideResult){pulled_wanted_speed, swap_mark_out};
}


/*
 * @routine 04:BA05 FxSteerFromInput
 * WHAT IT DOES: The car R8's new steering value R3 from its old one
 * (+$3A) and the control bits R2: bit 0 steers one way and bit 1 the
 * other by $2662 a step (by $80 when already turned, and from the
 * opposite side straight back to 0), up to $2666 either way; bits 2 and 3
 * steer harder by $2664, the limit raised by $2668. With no bits it
 * returns toward 0 by $266A a step. With bit 10 of +$40 set the step is
 * scaled by the speed (twice $2662 times |+$4A|, $7F at most, / 256) and
 * a value is kept as it is when no bit is held (decoded from the ROM
 * bytes and not reached in the traces so far, as are the returns where
 * the value has not yet reached 0).
 * HOW: R5 = the step and R4 = its negative; R7 = 1 for the speed-scaled
 * case; limits by CMP and the sign. Three adds are WITH R3 before a BRA,
 * done in its delay slot.
 * Entry: R2 = the control bits; R8 = the car's record; R11 = return
 * address. Exit: R3 = the new value; R4, R5, R6 from the limits; R0 and
 * the flags from the last test; the last RAM address the last value read.
 */
FxSteerFromInputResult FxSteerFromInput(uint16_t control_bits, uint16_t car_record)
{
    uint16_t speed;
    uint16_t r_t_12;
    uint16_t r_t_16;
    uint16_t r_t_23;
    uint16_t flags40;
    uint16_t abs_speed;
    uint16_t steer_max;
    uint16_t steer_back;
    uint16_t steer_bias;
    uint16_t steer_max2;
    uint16_t steer_rate;
    uint16_t steer_rate2;
    uint16_t bias_to_clamp;
    uint16_t neg_steer_max;
    uint16_t left_base_bias;
    uint16_t neg_steer_rate;
    uint16_t bias_decayed_up;
    uint16_t neg_steer_rate2;
    uint16_t right_base_bias;
    int bias_is_negative;
    uint16_t left_bias_chosen;
    uint16_t bias_before_decay;
    uint16_t bias_decayed_down;
    uint16_t right_bias_chosen;
    uint16_t left_bias_snapshot;
    uint16_t speed_scaled_steer;
    uint16_t right_bias_snapshot;

    steer_rate = fx_ldw(FX_STEER_RATE);
    speed_scaled_steer = 0;
    flags40 = fx_ldw((uint16_t)(FXREC_FLAGS40 + car_record));
    if ((0x0400 & flags40) == 0) {
    } else {
        speed_scaled_steer = 1;
        speed = fx_ldw((uint16_t)(FXREC_SPEED + car_record));
        abs_speed = speed;
        if (!(speed & 0x8000)) {
        } else {
            abs_speed = (uint16_t)((uint16_t)~abs_speed + 1);
        }
        if ((uint16_t)(abs_speed - 0x007F) & 0x8000) {
        } else {
            abs_speed = 0x007F;
        }
        steer_rate = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(steer_rate + steer_rate) * (int16_t)(uint16_t)(abs_speed << 8 | abs_speed >> 8)) >> 16);
    }
    steer_bias = fx_ldw((uint16_t)(FXCAR_STEER_BIAS + car_record));
    neg_steer_rate = (uint16_t)((uint16_t)~steer_rate + 1);
    if ((0x000F & control_bits) != 0) {
        steer_max = 0;
        if ((3 & control_bits) == 0) {
        } else {
            steer_max = fx_ldw(FX_STEER_MAX);
            if ((1 & control_bits) == 0) {
                if ((2 & control_bits) == 0) {
                } else {
                    left_bias_snapshot = steer_bias;
                    left_base_bias = left_bias_snapshot;
                    if (left_bias_snapshot & 0x8000) {
                    } else {
                        r_t_12 = speed_scaled_steer;
                        speed_scaled_steer = r_t_12;
                        if (r_t_12 != 0) {
                        } else {
                            left_base_bias = 0;
                        }
                    }
                    left_bias_chosen = left_base_bias;
                    if (left_bias_chosen == 0) {
                        steer_bias = (uint16_t)(left_bias_chosen + neg_steer_rate);
                    } else {
                        steer_bias = (uint16_t)(left_bias_chosen - 0x0080);
                    }
                }
            } else {
                right_bias_snapshot = steer_bias;
                right_base_bias = right_bias_snapshot;
                if (!(right_bias_snapshot & 0x8000)) {
                } else {
                    r_t_16 = speed_scaled_steer;
                    speed_scaled_steer = r_t_16;
                    if (r_t_16 != 0) {
                    } else {
                        right_base_bias = 0;
                    }
                }
                right_bias_chosen = right_base_bias;
                if (right_bias_chosen == 0) {
                    steer_bias = (uint16_t)(right_bias_chosen + steer_rate);
                } else {
                    steer_bias = (uint16_t)(right_bias_chosen + 0x0080);
                }
            }
        }
        if ((0x000C & control_bits) == 0) {
        } else {
            steer_max2 = fx_ldw(FX_STEER_MAX2);
            steer_max = (uint16_t)(steer_max + steer_max2);
            steer_rate2 = fx_ldw(FX_STEER_RATE2);
            neg_steer_rate2 = (uint16_t)((uint16_t)~steer_rate2 + 1);
            if ((4 & control_bits) == 0) {
            } else {
                steer_bias = (uint16_t)(steer_bias + steer_rate2);
            }
            if ((8 & control_bits) == 0) {
            } else {
                steer_bias = (uint16_t)(steer_bias + neg_steer_rate2);
            }
        }
        neg_steer_max = (uint16_t)((uint16_t)~steer_max + 1);
        bias_to_clamp = steer_bias;
        steer_bias = bias_to_clamp;
        if (bias_to_clamp & 0x8000) {
            if (!((uint16_t)(steer_bias - neg_steer_max) & 0x8000)) {
            } else {
                steer_bias = neg_steer_max;
            }
        } else {
            if ((uint16_t)(steer_bias - steer_max) & 0x8000) {
            } else {
                steer_bias = steer_max;
            }
        }
    } else {
        r_t_23 = speed_scaled_steer;
        speed_scaled_steer = r_t_23;
        if (r_t_23 == 0) {
            steer_back = fx_ldw(FX_STEER_BACK);
            bias_before_decay = steer_bias;
            bias_is_negative = ((bias_before_decay & 0x8000) != 0);
            steer_bias = bias_before_decay;
            if (bias_before_decay != 0) {
                if (!bias_is_negative) {
                    bias_decayed_down = (uint16_t)(steer_bias - steer_back);
                    steer_bias = bias_decayed_down;
                    if (bias_decayed_down & 0x8000) {
                        steer_bias = 0;
                    } else {
                    }
                } else {
                    bias_decayed_up = (uint16_t)(steer_bias + steer_back);
                    steer_bias = bias_decayed_up;
                    if (!(bias_decayed_up & 0x8000)) {
                        steer_bias = 0;
                    } else {
                    }
                }
            } else {
            }
        } else {
        }
    }
    return (FxSteerFromInputResult){steer_bias, speed_scaled_steer};
}


/* a car following the course: moved from fx/dispatch.c, where the generator put the routine it could not tag */
FxCarFollowCourseResult FxCarFollowCourse(uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t segment_scratch_a, uint16_t car_record, uint16_t segment_scratch_b, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t y;
    uint16_t old;
    uint16_t list;
    uint16_t half_x;
    uint16_t flags16;
    uint16_t flags1c;
    uint16_t point_z;
    uint16_t words_1;
    uint16_t words_3;
    uint16_t words_4;
    uint16_t number_1;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t rom_at_4;
    uint16_t direction;
    uint16_t entry_out;
    uint16_t entry_ptr;
    uint16_t flags1c_2;
    uint16_t flags1c_3;
    uint16_t words_4_2;
    uint16_t direction_2;
    uint16_t car_flags_2c;
    uint16_t entry_words_z;
    uint16_t last_dist_sum;
    uint16_t segment_count;
    uint16_t last_other_car;
    uint16_t segment_number;
    uint16_t course_followed_2;
    uint16_t last_change_low_2;
    uint16_t last_change_high_2;
    uint16_t entry_scratch_b_out;
    uint16_t segment_scratch_a_out;
    uint16_t segment_scratch_b_out;

    fx_spill(stack);
    flags16 = fx_ldw((uint16_t)(car_record + FXREC_FLAGS16));
    if ((0x0400 & flags16) != 0) {
        list = fx_ldw((uint16_t)(car_record + FXREC_LIST));
        if (list == 0) {
        } else {
            flags1c = fx_ldw((uint16_t)(car_record + FXREC_FLAGS1C));
            fx_sbk((flags1c | 0x0040));
        }
        flags1c_2 = fx_ldw((uint16_t)(car_record + FXREC_FLAGS1C));
        if ((flags1c_2 & 0x0010) != 0) {
        } else {
            fx_sbk((flags1c_2 | 0x0010));
            fx_stw((uint16_t)(car_record + 0x0072), 0xFFFF);
            fx_stw((uint16_t)(car_record + 0x002E), 0);
            fx_stw((uint16_t)(car_record + 0x0030), 0);
            fx_stw((uint16_t)(car_record + 0x0032), 0);
        }
        direction = fx_ldw((uint16_t)(car_record + FXREC_DIRECTION));
        if ((4 & direction) == 0) {
            { FxCarCatchUpResult r_ = FxCarCatchUp(entry_scratch_a, entry_scratch_b, segment_scratch_a, car_record, segment_scratch_b, stack); words_4 = r_.words_4; entry_out = r_.entry_out; segment_scratch_a_out = r_.segment_scratch_a_out; segment_scratch_b_out = r_.segment_scratch_b_out; rom_at_2 = r_.rom_at; };
            fx_unspill(stack);
            return (FxCarFollowCourseResult){0, words_4, entry_out, segment_scratch_a_out, car_record, segment_scratch_b_out, rom_at_2, 0};
        } else {
            { FxRecordSegmentResult r_ = FxRecordSegment(entry_scratch_a, entry_scratch_b, segment_scratch_a, car_record, segment_scratch_b, stack); segment_number = r_.segment_number; segment_count = r_.segment_count; entry_ptr = r_.entry_ptr; last_dist_sum = r_.last_dist_sum; rom_at_3 = r_.rom_at; };
            fx_stw((uint16_t)(car_record + FXREC_SEGMENT), segment_number);
            fx_unspill(stack);
            return (FxCarFollowCourseResult){1, segment_count, entry_ptr, segment_number, car_record, last_dist_sum, rom_at_3, 0};
        }
    } else {
        flags1c_3 = fx_ldw((uint16_t)(car_record + FXREC_FLAGS1C));
        if ((flags1c_3 & 0x0010) == 0) {
        } else {
            fx_sbk((uint16_t)(flags1c_3 & ~0x0010));
            { FxCarPoseResult r_ = FxCarPose(car_record, stack); last_change_low_2 = r_.last_change_low; last_change_high_2 = r_.last_change_high; segment_scratch_a = r_.pose_aux; segment_scratch_b = r_.last_part_ptr; rom_at_4 = r_.rom_at; };
            { FxCarVelocityFromSegmentResult r_ = FxCarVelocityFromSegment(last_change_low_2, last_change_high_2, segment_scratch_a, car_record, segment_scratch_b, stack, rom_at_4); up_ = r_.up; course_followed_2 = r_.course_followed; entry_scratch_a = r_.result_speed; entry_scratch_b = r_.segment_entry_ptr; rom_at = r_.rom_at_out; };
            if (up_) {                       /* it returned past us */
                return (FxCarFollowCourseResult){course_followed_2, entry_scratch_a, entry_scratch_b, segment_scratch_a, car_record, segment_scratch_b, rom_at, up_ == 1 ? 0 : up_ - 1};
            }
        }
        direction_2 = fx_ldw((uint16_t)(car_record + FXREC_DIRECTION));
        if ((4 & direction_2) == 0) {
            car_flags_2c = fx_ldw((uint16_t)(car_record + 0x002C));
            if ((0x0040 & car_flags_2c) == 0) {
                half_x = fx_ldw((uint16_t)(car_record + FXREC_HALF_X));
                y = fx_ldw((uint16_t)(car_record + FXREC_Y));
                point_z = fx_ldw((uint16_t)(car_record + 0x0022));
                { FxCourseNearestResult r_ = FxCourseNearest(half_x, y, point_z, entry_scratch_a, entry_scratch_b, segment_scratch_a, segment_scratch_b); number_1 = r_.number_1; entry_words_z = r_.entry_words_z; entry_scratch_a = r_.count; entry_scratch_b = r_.entry; rom_at = r_.rom_at; };
                if (number_1 != 0) {
                    fx_stw((uint16_t)(car_record + FXREC_SEGMENT), number_1);
                    { FxCourseNextEntryResult r_ = FxCourseNextEntry(number_1, entry_words_z, entry_scratch_a, entry_scratch_b, car_record, rom_at); words_1 = r_.words_1; words_3 = r_.words_3; words_4_2 = r_.words_4; rom_at = r_.rom_at_out; };
                    { FxDriveDecideResult r_ = FxDriveDecide(words_1, words_3, words_4_2, car_record, stack, rom_at); entry_scratch_a = r_.angle_sign_xor; entry_scratch_b = r_.swap_mark; };
                } else {
                }
            } else {
            }
            { FxTrafficSidesResult r_ = FxTrafficSides(entry_scratch_b, car_record, stack); entry_scratch_b_out = r_.entry_scratch_b_out; last_other_car = r_.last_other_car; old = r_.old; };
            fx_unspill(stack);
            return (FxCarFollowCourseResult){1, entry_scratch_a, entry_scratch_b_out, last_other_car, car_record, old, rom_at, 0};
        } else {
            { FxRecordSegmentResult r_ = FxRecordSegment(entry_scratch_a, entry_scratch_b, segment_scratch_a, car_record, segment_scratch_b, stack); segment_number = r_.segment_number; segment_count = r_.segment_count; entry_ptr = r_.entry_ptr; last_dist_sum = r_.last_dist_sum; rom_at_3 = r_.rom_at; };
            fx_stw((uint16_t)(car_record + FXREC_SEGMENT), segment_number);
            fx_unspill(stack);
            return (FxCarFollowCourseResult){1, segment_count, entry_ptr, segment_number, car_record, last_dist_sum, rom_at_3, 0};
        }
    }
}
