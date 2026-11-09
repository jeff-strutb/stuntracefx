/*
 * project.c: the Super FX code of asm/stuntracefx/render/project.asm, written from it by
 * the generator.
 *
 * project.asm: the Super FX projecting a point onto the screen: X and Y over
 * the depth (FxRatioClamp, clamped to $3FFF), moved to the screen centre,
 * with an outcode of which screen edges the point is beyond.
 * C: src/stuntracefx/render/project.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:947A FxProjectPoint
 * WHAT IT DOES: Projects the point (R1, R2) at depth R3 to the screen,
 * moved to the centre at $0034, and returns its outcode: bits 0-1 past
 * the X edges, bits 2-3 past the Y edges, bit 4 behind the view.
 * HOW: Pushes R11 on the R10 stack. A negative depth is negated and sets
 * R5 = 4 (else R5 = 0; a zero depth counts as 1). FxRatioClamp divides the
 * larger of |R1|, |R2| first (the pair swapped around the call when |R1|
 * is the smaller), then the signs go back, each one reversed when the
 * depth was negative: sixteen copies of the code, one per case. Then
 * CACHE, the centre is added, and against the edges at $0038-$003E
 * (SUB, BGE) R5 counts 1 for past the low edge and 2 for past the high
 * one, its two bits swapped when behind, X then (after MULT #4) Y. R0 =
 * the outcode's complement in its low 5 bits << 8, OR the outcode.
 * Returns by pulling R11's value into R15.
 * Entry: R1, R2 = the point, R3 = its depth; R10 = the stack; R11 =
 * return address. Exit: R1, R2 = the screen position; R5 = the outcode;
 * R0 = ((R5 XOR $1F) << 8) | R5; R3 = |depth| (1 for 0); R4, R6, R11,
 * R12, R13 as FxRatioClamp leaves them; R10 as at entry; the last RAM
 * address R10; S and Z from R10; OV and CY from the last SUB R2.
 */
FxProjectPointResult FxProjectPoint(uint16_t point_x, uint16_t point_y, uint16_t depth, uint16_t *stack)
{
    uint16_t divisor;
    uint16_t clip_top;
    uint16_t screen_x;
    uint16_t screen_y;
    uint16_t abs_depth;
    uint16_t clip_left;
    uint16_t outcode_x;
    uint16_t results_x;
    uint16_t results_y;
    uint16_t clip_right;
    uint16_t outcode_xy;
    uint16_t abs_point_x;
    uint16_t abs_point_y;
    uint16_t clip_bottom;
    uint16_t results_x_2;
    uint16_t results_x_3;
    uint16_t results_x_4;
    uint16_t results_x_5;
    uint16_t results_x_6;
    uint16_t results_x_7;
    uint16_t results_x_8;
    uint16_t results_x_9;
    uint16_t results_y_2;
    uint16_t results_y_3;
    uint16_t results_y_4;
    uint16_t results_y_5;
    uint16_t results_y_6;
    uint16_t results_y_7;
    uint16_t results_y_8;
    uint16_t results_y_9;
    uint16_t results_x_10;
    uint16_t results_x_11;
    uint16_t results_x_12;
    uint16_t results_x_13;
    uint16_t results_y_10;
    uint16_t results_y_11;
    uint16_t results_y_12;
    uint16_t results_y_13;
    uint16_t abs_point_x_2;
    uint16_t abs_point_y_2;
    uint16_t abs_point_y_3;
    uint16_t abs_point_y_4;
    int depth_is_zero;
    uint16_t screen_center_x;
    uint16_t screen_center_y;

    fx_spill(stack);
    depth_is_zero = (depth == 0);
    divisor = depth;
    if (!(depth & 0x8000)) {
        if (!depth_is_zero) {
        } else {
            divisor = (uint16_t)(divisor + 1);
        }
        outcode_x = 0;
        if (point_x & 0x8000) {
            abs_point_x = (uint16_t)((uint16_t)~point_x + 1);
            if (point_y & 0x8000) {
                abs_point_y_2 = (uint16_t)((uint16_t)~point_y + 1);
                if (abs_point_x < abs_point_y_2) {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_y_2, abs_point_x, divisor, stack); results_x = r_.results_x; results_y = r_.results_y; };
                    results_y_2 = (uint16_t)((uint16_t)~results_x + 1);
                    results_x_2 = (uint16_t)((uint16_t)~results_y + 1);
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_x, abs_point_y_2, divisor, stack); results_x_3 = r_.results_x; results_y_3 = r_.results_y; };
                    results_y_2 = (uint16_t)((uint16_t)~results_y_3 + 1);
                    results_x_2 = (uint16_t)((uint16_t)~results_x_3 + 1);
                }
            } else {
                if (abs_point_x < point_y) {
                    { FxRatioClampResult r_ = FxRatioClamp(point_y, abs_point_x, divisor, stack); results_x_4 = r_.results_x; results_y_4 = r_.results_y; };
                    results_y_2 = results_x_4;
                    results_x_2 = (uint16_t)((uint16_t)~results_y_4 + 1);
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_x, point_y, divisor, stack); results_x_5 = r_.results_x; results_y_2 = r_.results_y; };
                    results_x_2 = (uint16_t)((uint16_t)~results_x_5 + 1);
                }
            }
        } else {
            if (point_y & 0x8000) {
                abs_point_y = (uint16_t)((uint16_t)~point_y + 1);
                if (point_x < abs_point_y) {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_y, point_x, divisor, stack); results_x_6 = r_.results_x; results_y_5 = r_.results_y; };
                    results_x_2 = results_y_5;
                    results_y_2 = (uint16_t)((uint16_t)~results_x_6 + 1);
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(point_x, abs_point_y, divisor, stack); results_x_2 = r_.results_x; results_y_6 = r_.results_y; };
                    results_y_2 = (uint16_t)((uint16_t)~results_y_6 + 1);
                }
            } else {
                if (point_x < point_y) {
                    { FxRatioClampResult r_ = FxRatioClamp(point_y, point_x, divisor, stack); results_x_7 = r_.results_x; results_y_7 = r_.results_y; };
                    results_x_2 = results_y_7;
                    results_y_2 = results_x_7;
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(point_x, point_y, divisor, stack); results_x_2 = r_.results_x; results_y_2 = r_.results_y; };
                }
            }
        }
    } else {
        abs_depth = (uint16_t)((uint16_t)~divisor + 1);
        outcode_x = 4;
        if (point_x & 0x8000) {
            abs_point_x_2 = (uint16_t)((uint16_t)~point_x + 1);
            if (point_y & 0x8000) {
                abs_point_y_4 = (uint16_t)((uint16_t)~point_y + 1);
                if (abs_point_x_2 < abs_point_y_4) {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_y_4, abs_point_x_2, abs_depth, stack); results_x_8 = r_.results_x; results_y_8 = r_.results_y; };
                    results_x_2 = results_y_8;
                    results_y_2 = results_x_8;
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_x_2, abs_point_y_4, abs_depth, stack); results_x_2 = r_.results_x; results_y_2 = r_.results_y; };
                }
            } else {
                if (abs_point_x_2 < point_y) {
                    { FxRatioClampResult r_ = FxRatioClamp(point_y, abs_point_x_2, abs_depth, stack); results_x_9 = r_.results_x; results_y_9 = r_.results_y; };
                    results_x_2 = results_y_9;
                    results_y_2 = (uint16_t)((uint16_t)~results_x_9 + 1);
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_x_2, point_y, abs_depth, stack); results_x_2 = r_.results_x; results_y_10 = r_.results_y; };
                    results_y_2 = (uint16_t)((uint16_t)~results_y_10 + 1);
                }
            }
        } else {
            if (point_y & 0x8000) {
                abs_point_y_3 = (uint16_t)((uint16_t)~point_y + 1);
                if (point_x < abs_point_y_3) {
                    { FxRatioClampResult r_ = FxRatioClamp(abs_point_y_3, point_x, abs_depth, stack); results_x_10 = r_.results_x; results_y_11 = r_.results_y; };
                    results_y_2 = results_x_10;
                    results_x_2 = (uint16_t)((uint16_t)~results_y_11 + 1);
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(point_x, abs_point_y_3, abs_depth, stack); results_x_11 = r_.results_x; results_y_2 = r_.results_y; };
                    results_x_2 = (uint16_t)((uint16_t)~results_x_11 + 1);
                }
            } else {
                if (point_x < point_y) {
                    { FxRatioClampResult r_ = FxRatioClamp(point_y, point_x, abs_depth, stack); results_x_12 = r_.results_x; results_y_12 = r_.results_y; };
                    results_x_2 = (uint16_t)((uint16_t)~results_y_12 + 1);
                    results_y_2 = (uint16_t)((uint16_t)~results_x_12 + 1);
                } else {
                    { FxRatioClampResult r_ = FxRatioClamp(point_x, point_y, abs_depth, stack); results_x_13 = r_.results_x; results_y_13 = r_.results_y; };
                    results_x_2 = (uint16_t)((uint16_t)~results_x_13 + 1);
                    results_y_2 = (uint16_t)((uint16_t)~results_y_13 + 1);
                }
            }
        }
    }
    screen_center_x = fx_ldw(0x0034);
    screen_x = (uint16_t)(results_x_2 + screen_center_x);
    screen_center_y = fx_ldw(0x0036);
    screen_y = (uint16_t)(results_y_2 + screen_center_y);
    clip_left = fx_ldw(0x0038);
    if (((((uint16_t)(clip_left - screen_x) & 0x8000) != 0) == (((clip_left ^ screen_x) & (clip_left ^ (uint16_t)(clip_left - screen_x - 0)) & 0x8000) != 0))) {
        outcode_x = (uint16_t)(outcode_x + 1);
        if ((4 & outcode_x) == 0) {
        } else {
            outcode_x = (outcode_x ^ 3);
        }
    } else {
        clip_right = fx_ldw(0x003A);
        if (((((uint16_t)(clip_right - screen_x) & 0x8000) != 0) == (((clip_right ^ screen_x) & (clip_right ^ (uint16_t)(clip_right - screen_x - 0)) & 0x8000) != 0))) {
        } else {
            outcode_x = (uint16_t)(outcode_x + 1);
            outcode_x = (uint16_t)(outcode_x + 1);
            if ((4 & outcode_x) == 0) {
            } else {
                outcode_x = (outcode_x ^ 3);
            }
        }
    }
    outcode_xy = (uint16_t)((int8_t)outcode_x * (int8_t)4);
    clip_top = fx_ldw(0x003C);
    if (((((uint16_t)(clip_top - screen_y) & 0x8000) != 0) == (((clip_top ^ screen_y) & (clip_top ^ (uint16_t)(clip_top - screen_y - 0)) & 0x8000) != 0))) {
        outcode_xy = (uint16_t)(outcode_xy + 1);
        if ((0x0010 & outcode_xy) == 0) {
        } else {
            outcode_xy = (outcode_xy ^ 3);
        }
    } else {
        clip_bottom = fx_ldw(0x003E);
        if (((((uint16_t)(clip_bottom - screen_y) & 0x8000) != 0) == (((clip_bottom ^ screen_y) & (clip_bottom ^ (uint16_t)(clip_bottom - screen_y - 0)) & 0x8000) != 0))) {
        } else {
            outcode_xy = (uint16_t)(outcode_xy + 1);
            outcode_xy = (uint16_t)(outcode_xy + 1);
            if ((0x0010 & outcode_xy) == 0) {
            } else {
                outcode_xy = (outcode_xy ^ 3);
            }
        }
    }
    fx_unspill(stack);
    return (FxProjectPointResult){((uint16_t)((0x001F ^ outcode_xy) << 8 | (0x001F ^ outcode_xy) >> 8) | outcode_xy), screen_x, screen_y};
}


/*
 * @routine 01:B2A4 FxProjectScaled
 * WHAT IT DOES: Projects a point and a size at a depth: X (R1), Y (R2) and
 * the size (R5) times the scale for the depth (R3), X and Y moved to the
 * screen centre.
 * HOW: ROM bank $14. While the depth is above $4400 (SUB, BPL), ASR halves
 * X, Y, the size and the depth; while $100 - the depth is not negative,
 * ADD doubles them, so the depth ends from $101 to $4400. The scale is
 * the word at $14:896C + the depth AND NOT 1 (GETB, GETBH); FMULT by it
 * gives each, X plus $34 and Y plus $36.
 * Entry: R1 = X, R2 = Y, R3 = the depth (positive), R5 = the size; R11 =
 * return address. Exit: R1, R2 = the screen position; R5 = R0 = the size;
 * R3 = the depth as scaled, bit 0 clear; R6 = the scale; R14 = the
 * table's address + 1; the ROM bank $14; flags from the last FMULT.
 * R14 and the ROM buffer move as on the Super FX.
 */
FxProjectScaledResult FxProjectScaled(uint16_t x, uint16_t y, uint16_t depth_positive, uint16_t size)
{
    uint16_t scale_factor;
    uint16_t scale_low_byte;
    uint16_t screen_center_x;
    uint16_t screen_center_y;
    uint16_t scale_table_addr;

    fx_romb(0x0014);
    for (;;) {
        if (!((uint16_t)((uint16_t)(0 - depth_positive) + 0x4400) & 0x8000)) {
            break;
        } else {
            x = (uint16_t)((int16_t)x >> 1);
            y = (uint16_t)((int16_t)y >> 1);
            size = (uint16_t)((int16_t)size >> 1);
            depth_positive = (uint16_t)((int16_t)depth_positive >> 1);
            continue;
        }
    }
    for (;;) {
        if ((uint16_t)((uint16_t)(0 - depth_positive) + 0x0100) & 0x8000) {
            break;
        } else {
            size = (uint16_t)(size + size);
            x = (uint16_t)(x + x);
            y = (uint16_t)(y + y);
            depth_positive = (uint16_t)(depth_positive + depth_positive);
            continue;
        }
    }
    scale_table_addr = (uint16_t)((uint16_t)(depth_positive & ~1) - 0x7694);
    fx_r14(scale_table_addr);
    scale_low_byte = fx_getb();
    fx_r14((uint16_t)(scale_table_addr + 1));
    scale_factor = (uint16_t)((scale_low_byte & 0xFF) | fx_getb() << 8);
    screen_center_x = fx_ldw(0x0034);
    screen_center_y = fx_ldw(0x0036);
    return (FxProjectScaledResult){(uint16_t)(screen_center_x + (uint16_t)((uint32_t)((int32_t)(int16_t)x * (int16_t)scale_factor) >> 16)), (uint16_t)(screen_center_y + (uint16_t)((uint32_t)((int32_t)(int16_t)y * (int16_t)scale_factor) >> 16)), (uint16_t)((uint32_t)((int32_t)(int16_t)size * (int16_t)scale_factor) >> 16), scale_factor};
}


/*
 * @routine 01:8BFF FxProjectVertex
 * WHAT IT DOES: Projects the point (R1, R2) at depth R6 to the screen and
 * returns its outcode. A point farther than $100 is scaled by the table at
 * $14:896C (as FxProjectScaled) and moved to the centre at $0034; its
 * outcode has bit 2 for X below 0, bit 3 for X at or past $003A, bit 0 for
 * Y below 0, bit 1 for Y at or past $003E. A nearer point goes through
 * FxProjectPoint, which divides by the depth instead.
 * HOW: While $4400 - the depth is negative (SUB, BPL), ASR halves X, Y
 * and the depth (the last in the BRA's delay slot). For a far point the
 * scale is the word at $896C + the depth AND NOT 1 (GETB, GETBH), FMULT
 * by it gives X and Y, each plus the centre. R6 counts the outcode: X
 * first (MOVES, then SUB against the right edge; MULT #4 moves it up),
 * then Y. R0 = ((R6 XOR $1F) << 8) | R6. For a near point: R14 = R12,
 * R3 = the depth, R11 pushed on the R10 stack, FxProjectPoint, then R12 =
 * R14 and R3, R13, R9 reloaded from $0026-$002A; return by pulling R15.
 * Entry: R1 = X, R2 = Y, R6 = the depth; R12 = the next index byte's
 * address (near points only); R10 = stack; R11 = return address; the ROM
 * bank $14. Exit: (far) R1, R2 = the screen position; R6 = the outcode, R0
 * as above; R5 = the last edge read (when one was); R14 = the scale's
 * address + 1; S and Z from the OR; OV and CY from the last SUB or ADD;
 * the last RAM address the last word loaded. Exit (near): as FxProjectPoint
 * with R12 = R14, R3, R13, R9 from $0026, $0028, $002A; S, Z from R10;
 * the last RAM address R10. R14 and the ROM buffer move as on the Super
 * FX.
 */
FxProjectVertexResult FxProjectVertex(uint16_t x, uint16_t y, uint16_t entry_aux_a, uint16_t depth, uint16_t entry_aux_c, uint16_t *stack, uint16_t count, uint16_t entry_aux_b)
{
    uint16_t xor;
    uint16_t center_x;
    uint16_t center_y;
    uint16_t scale_lo;
    uint16_t right_edge;
    uint16_t bottom_edge;
    uint16_t scale_factor;
    uint16_t screen_x_far;
    uint16_t screen_y_far;
    uint16_t outcode_start;
    uint16_t reloaded_aux_a;
    uint16_t reloaded_aux_b;
    uint16_t reloaded_aux_c;
    uint16_t scale_table_addr;
    uint16_t screen_position_x;
    uint16_t screen_position_y;

    for (;;) {
        if (!((uint16_t)((uint16_t)(0 - depth) + 0x4400) & 0x8000)) {
            break;
        } else {
            x = (uint16_t)((int16_t)x >> 1);
            y = (uint16_t)((int16_t)y >> 1);
            depth = (uint16_t)((int16_t)depth >> 1);
            continue;
        }
    }
    if (!((uint16_t)((uint16_t)(0 - depth) + 0x0100) & 0x8000)) {
        fx_r14(count);
        fx_spill(stack);
        { FxProjectPointResult r_ = FxProjectPoint(x, y, depth, stack); xor = r_.xor; screen_position_x = r_.screen_position_x; screen_position_y = r_.screen_position_y; };
        reloaded_aux_a = fx_ldw(0x0026);
        reloaded_aux_b = fx_ldw(0x0028);
        reloaded_aux_c = fx_ldw(0x002A);
        fx_unspill(stack);
        return (FxProjectVertexResult){xor, screen_position_x, screen_position_y, reloaded_aux_a, reloaded_aux_c, reloaded_aux_b};
    } else {
        scale_table_addr = (uint16_t)((uint16_t)(depth & ~1) - 0x7694);
        fx_r14(scale_table_addr);
        scale_lo = fx_getb();
        fx_r14((uint16_t)(scale_table_addr + 1));
        scale_factor = (uint16_t)((scale_lo & 0xFF) | fx_getb() << 8);
        center_x = fx_ldw(0x0034);
        screen_x_far = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)x * (int16_t)scale_factor) >> 16) + center_x);
        center_y = fx_ldw(0x0036);
        screen_y_far = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)y * (int16_t)scale_factor) >> 16) + center_y);
        outcode_start = 0;
        if (screen_x_far & 0x8000) {
            outcode_start = (uint16_t)((int8_t)(uint16_t)(outcode_start + 1) * (int8_t)4);
        } else {
            right_edge = fx_ldw(0x003A);
            if ((uint16_t)(screen_x_far - right_edge) & 0x8000) {
            } else {
                outcode_start = (uint16_t)(outcode_start + 1);
                outcode_start = (uint16_t)((int8_t)(uint16_t)(outcode_start + 1) * (int8_t)4);
            }
        }
        if (screen_y_far & 0x8000) {
            outcode_start = (uint16_t)(outcode_start + 1);
        } else {
            bottom_edge = fx_ldw(0x003E);
            if ((uint16_t)(screen_y_far - bottom_edge) & 0x8000) {
            } else {
                outcode_start = (uint16_t)(outcode_start + 1);
                outcode_start = (uint16_t)(outcode_start + 1);
            }
        }
        return (FxProjectVertexResult){((uint16_t)((0x001F ^ outcode_start) << 8 | (0x001F ^ outcode_start) >> 8) | outcode_start), screen_x_far, screen_y_far, entry_aux_a, entry_aux_c, entry_aux_b};
    }
}

