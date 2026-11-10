/*
 * model.c: the Super FX code of asm/stuntracefx/render/model.asm, written from it by
 * the generator.
 *
 * model.asm: the Super FX's 3D model interpreter (01:8561), its command
 * code (the IWT R15 table at 01:8000 picks it, through the bank's mirror
 * at 01:00xx), the material lookups and the textured screen clip it uses.
 * C: src/stuntracefx/render/model.c, which describes the commands.
 */

#include "fx/fx.h"
#include "fx/fx_far.h"

/*
 * @routine 01:8000 FxModelCommands
 * WHAT IT DOES: The model interpreter's command table: command byte cc
 * (0, 4, ... $64) runs the entry at 01:80cc, which jumps to that
 * command's code. The code reaches an entry through the bank's mirror
 * (TO R15; GETB jumps to 01:00cc).
 * HOW: 26 entries of IWT R15,#code; NOP (the NOP is the jump's delay
 * slot).
 * Entry: R15 = 01:00cc (a command's last GETB); R10 = stack; R14 at the
 * command's bytes.
 * Exit: as the command's code leaves the registers, flags and RAM.
 */
FxModelCommandsResult FxModelCommands(uint16_t model_state_r6, uint16_t model_state_r7, uint16_t *stack, uint16_t rom_at)
{

    { FxModelEndResult tr_ = FxModelEnd(model_state_r6, model_state_r7, stack, rom_at); return (FxModelCommandsResult){tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8561 FxModelDraw
 * WHAT IT DOES: Draws a 3D model: sets up the object's rotation, light
 * and point scale, then runs the model's command list from ROM ($0016,
 * bank $001C) until its end command, each command transforming points,
 * projecting them, or queueing and drawing faces.
 * HOW: Pushes R11 on the R10 stack; a non-zero $01B4 first (01:8568)
 * and $01B6 pick other set-ups. $005C = $08AA (the draw list empty).
 * The object's angles ($0020-$0024) become negated 8.8 words; with all
 * three zero the matrix at $00E4 is copied to $0132 (or negated in
 * parts when the y angle is $40 or $80), else FxMatrixFromAngles01,
 * FxMatrixTranspose and FxMatrixApplyVectors build it. The light $0000,
 * $393E, $727C is turned into $0106-$010A. Points start at $0340
 * ($001E); the end command (9243) is pushed on the stack, and the first
 * command byte is dispatched through the IWT R15 table at 01:8000, as
 * every command's last GETB is.
 * Entry: R10 = stack; R11 = return address; the object's set-up in RAM.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelDrawResult FxModelDraw(uint16_t draw_list_end, uint16_t *stack)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t model_1b4;
    uint16_t draw_list_end_out_2;

    model_1b4 = fx_ldw(FX_MODEL_1B4);
    if ((uint16_t)(model_1b4 & 0xFF) == 0) {
        { FxModelDrawFlags8000Result tr_ = FxModelDrawFlags8000(draw_list_end, stack); return (FxModelDrawResult){tr_.draw_list_end_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
    fx_spill(stack);
    { FxModelDrawFlaggedResult r_ = FxModelDrawFlagged(draw_list_end, stack); up_ = r_.up; draw_list_end_out_2 = r_.draw_list_end_out; };
    if (up_) {                       /* it returned past us */
        return (FxModelDrawResult){draw_list_end_out_2, up_ == 1 ? 0 : up_ - 1};
    }
    fx_unspill(stack);
    return (FxModelDrawResult){draw_list_end_out_2, 0};
}


/*
 * @routine 01:8576 FxModelDrawFlagged
 * WHAT IT DOES: FxModelDraw's set-up when $01B4 is set (01:8561 calls
 * it with R11 = $8571): the model flags $0054 become bit 12 of them
 * with bit 13 set, then the model is drawn as FxModelDraw draws it.
 * HOW: R2 = $1000, R1 = $2000: ($0054 AND R2) OR R1 into $0054
 * (01:858F), the rest as in FxModelDraw; its end command's pull returns
 * to R11.
 * Entry: R10 = stack; R11 = return address; the object's set-up in RAM.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelDrawFlaggedResult FxModelDrawFlagged(uint16_t draw_list_end, uint16_t *stack)
{
    uint16_t flags;

    flags = fx_ldw(FX_MODEL_FLAGS);
    { FxModelDrawWithFlagsResult tr_ = FxModelDrawWithFlags(((flags & 0x1000) | 0x2000), draw_list_end, stack); return (FxModelDrawFlaggedResult){tr_.draw_list_end_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8584 FxModelDrawFlags8000
 * WHAT IT DOES: An entry inside FxModelDrawFlagged's set-up: draws the
 * model with the model flags $0054 = $8000. Only the uncalled program
 * FxModelMirrorProgram (01:84DA) calls it.
 * HOW: Pushes R11 and pulls it straight back, R0 = $8000, then on as at
 * 01:858F.
 * Entry: as FxModelDrawFlagged. Exit: as FxModelDrawFlagged.
 */
FxModelDrawFlags8000Result FxModelDrawFlags8000(uint16_t draw_list_end, uint16_t *stack)
{

    fx_spill(stack);
    fx_unspill(stack);
    { FxModelDrawWithFlagsResult tr_ = FxModelDrawWithFlags(0x8000, draw_list_end, stack); return (FxModelDrawFlags8000Result){tr_.draw_list_end_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:858F FxModelDrawWithFlags
 * WHAT IT DOES: The draw both set-ups lead to: the model flags $0054 = R0,
 * then the model drawn as FxModelDraw draws it. Called directly only by
 * the uncalled program FxModelRecordProgram (01:8515).
 * HOW: SMS $0054, R0, then on in the model interpreter.
 * Entry: R0 = the model flags; otherwise as FxModelDrawFlagged. Exit: as
 * FxModelDrawFlagged.
 */
FxModelDrawWithFlagsResult FxModelDrawWithFlags(uint16_t model_flags, uint16_t draw_list_end, uint16_t *stack)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t count;
    uint16_t flags;
    uint16_t rom_at;
    uint16_t count_2;
    uint16_t flags_2;
    uint16_t flags_3;
    uint16_t light_x;
    uint16_t light_y;
    uint16_t light_z;
    uint16_t lod_bias;
    uint16_t rom_at_2;
    uint16_t model_1b6;
    uint16_t scale_bit;
    uint16_t prev_end_y;
    uint16_t rom_at_out;
    uint16_t shade_base;
    uint16_t angle_xz_or;
    uint16_t model_index;
    uint16_t shade_table;
    uint16_t detail_level;
    uint16_t lod_bias_far;
    uint16_t packed_low_0;
    uint16_t packed_low_1;
    uint16_t packed_low_2;
    uint16_t packed_low_3;
    uint16_t packed_src_1;
    uint16_t packed_src_2;
    uint16_t packed_src_3;
    uint16_t packed_src_4;
    uint16_t packed_src_5;
    uint16_t packed_src_6;
    uint16_t packed_src_7;
    uint16_t shade_offset;
    uint32_t light_x_by_m0;
    uint32_t light_x_by_m3;
    uint32_t light_x_by_m6;
    uint32_t light_y_by_m1;
    uint32_t light_y_by_m4;
    uint32_t light_y_by_m7;
    uint32_t light_z_by_m2;
    uint32_t light_z_by_m5;
    uint32_t light_z_by_m8;
    uint16_t lod_far_limit;
    uint16_t lod_mid_limit;
    uint16_t lod_rom_ptr_1;
    uint16_t lod_rom_ptr_2;
    uint16_t packed_high_0;
    uint16_t packed_high_1;
    uint16_t packed_high_2;
    uint16_t packed_high_3;
    uint16_t shade_table_2;
    uint16_t view_matrix_0;
    uint16_t view_matrix_1;
    uint16_t view_matrix_2;
    uint16_t view_matrix_3;
    uint16_t view_matrix_4;
    uint16_t view_matrix_5;
    uint16_t view_matrix_6;
    uint16_t view_matrix_7;
    uint16_t view_matrix_8;
    uint16_t lod_mode_flags;
    uint16_t lod_near_limit;
    uint16_t model_rom_bank;
    uint16_t object_angle_x;
    uint16_t object_angle_y;
    uint16_t object_angle_z;
    uint16_t shade_base_far;
    uint16_t matrix_packed_4;
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
    uint16_t next_command_ptr;
    uint16_t shade_offset_far;
    uint16_t lod_far_limit_far;
    uint16_t lod_mid_limit_far;
    uint16_t lod_rom_ptr_far_1;
    uint16_t lod_rom_ptr_far_2;
    uint16_t next_command_byte;
    uint16_t object_matrix_1_2;
    uint16_t object_matrix_2_2;
    uint16_t object_matrix_3_2;
    uint16_t object_matrix_4_2;
    uint16_t object_matrix_5_2;
    uint16_t object_matrix_6_2;
    uint16_t object_matrix_7_2;
    uint16_t object_matrix_8_2;
    uint16_t lod_near_limit_far;
    uint16_t draw_list_end_out_2;
    uint16_t caller_write_ptr_out;
    uint16_t model_offset_doubled;
    uint16_t angle_y_minus_quarter;
    uint16_t model_index_minus_one;

    fx_stw(FX_MODEL_FLAGS, model_flags);
    fx_spill(stack);
    fx_romb(3);
    fx_stw(FX_LIST_END, 0x08AA);
    fx_romb(3);
    rom_at = fx_ldw(0x0050);
    fx_r14(rom_at);
    model_1b6 = fx_ldw(FX_MODEL_1B6);
    model_index = (uint16_t)(model_1b6 & 0xFF);
    if (model_index == 0) {
    } else {
        model_index_minus_one = (uint16_t)(model_index - 1);
        model_offset_doubled = (uint16_t)(model_index_minus_one + model_index_minus_one);
        rom_at = (uint16_t)((uint16_t)(model_offset_doubled + model_offset_doubled) - 0x51BB);
        fx_r14(rom_at);
    }
    lod_mode_flags = fx_ldw(0x108C);
    if ((uint16_t)(lod_mode_flags & 0xFF) == 0) {
        lod_bias = fx_far_depth(fx_ldw(0x002A));     /* a far object's tier by its true depth (fx_far.c) */
        lod_near_limit = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
        lod_rom_ptr_1 = (uint16_t)(rom_at + 1);
        fx_r14(lod_rom_ptr_1);
        if (!((uint16_t)(lod_near_limit + lod_bias) & 0x8000)) {
            lod_mid_limit = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
            lod_rom_ptr_2 = (uint16_t)(lod_rom_ptr_1 + 1);
            fx_r14(lod_rom_ptr_2);
            if (!((uint16_t)(lod_mid_limit + lod_bias) & 0x8000)) {
                lod_far_limit = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
                fx_r14((uint16_t)(lod_rom_ptr_2 + 1));
                if (!((uint16_t)(lod_far_limit + lod_bias) & 0x8000)) {
                    shade_table = 0xA249;
                    shade_offset = 0x0198;
                } else {
                    shade_table = 0xA21D;
                    shade_offset = 0x0110;
                }
            } else {
                shade_table = 0xA1F1;
                shade_offset = 0x0088;
            }
        } else {
            shade_table = 0xA1C5;
            shade_offset = 0;
        }
        fx_stw(FX_SHADE_TABLE, shade_table);
        shade_base = fx_ldw(0x004E);
        fx_stw(FX_SHADE_AT, (uint16_t)(shade_base + shade_offset));
    } else {
        lod_bias_far = fx_far_depth(fx_ldw(0x002A));
        lod_near_limit_far = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
        lod_rom_ptr_far_1 = (uint16_t)(rom_at + 1);
        fx_r14(lod_rom_ptr_far_1);
        if (!((uint16_t)(lod_near_limit_far + lod_bias_far) & 0x8000)) {
            lod_mid_limit_far = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
            lod_rom_ptr_far_2 = (uint16_t)(lod_rom_ptr_far_1 + 1);
            fx_r14(lod_rom_ptr_far_2);
            if (!((uint16_t)(lod_mid_limit_far + lod_bias_far) & 0x8000)) {
                lod_far_limit_far = (uint16_t)((0 & 0xFF) | fx_getb() << 8);
                fx_r14((uint16_t)(lod_rom_ptr_far_2 + 1));
                if (!((uint16_t)(lod_far_limit_far + lod_bias_far) & 0x8000)) {
                    shade_table_2 = 0xA2F9;
                    shade_offset_far = 0x0198;
                } else {
                    shade_table_2 = 0xA2CD;
                    shade_offset_far = 0x0110;
                }
            } else {
                shade_table_2 = 0xA2A1;
                shade_offset_far = 0x0088;
            }
        } else {
            shade_table_2 = 0xA275;
            shade_offset_far = 0;
        }
        fx_stw(FX_SHADE_TABLE, shade_table_2);
        shade_base_far = fx_ldw(0x004E);
        fx_stw(FX_SHADE_AT, (uint16_t)(shade_base_far + shade_offset_far));
    }
    scale_bit = 1;
    count = fx_ldw(0x0032);
    count_2 = count;
    if (count == 0) {
    } else {
        for (;;) {
            count_2 = (uint16_t)(count_2 - 1);
            scale_bit = (uint16_t)(scale_bit + scale_bit);
            if (count_2 != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    fx_stw(0x0030, scale_bit);
    /* each angle byte made a word, negated; in a picture between steps the
       byte's 256ths come with it (fx_tween.c) */
    object_angle_x = fx_ldw(0x0020);
    fx_sbk((uint16_t)((uint16_t)~(uint16_t)((uint16_t)(object_angle_x & 0xFF) << 8 | fx_angle_fraction(0)) + 1));
    object_angle_y = fx_ldw(0x0022);
    fx_sbk((uint16_t)((uint16_t)~(uint16_t)((uint16_t)(object_angle_y & 0xFF) << 8 | fx_angle_fraction(1)) + 1));
    object_angle_z = fx_ldw(0x0024);
    fx_sbk((uint16_t)((uint16_t)~(uint16_t)((uint16_t)(object_angle_z & 0xFF) << 8 | fx_angle_fraction(2)) + 1));
    angle_xz_or = (object_angle_x | object_angle_z);
    if (angle_xz_or == 0 && !fx_angle_fraction(0) && !fx_angle_fraction(1) && !fx_angle_fraction(2)) {
        if ((angle_xz_or | object_angle_y) != 0) {
            angle_y_minus_quarter = (uint16_t)(object_angle_y - 0x0040);
            if (angle_y_minus_quarter != 0) {
                if (angle_y_minus_quarter != 0x0040) {
                    { FxMatrixFromAngles01Result r_ = FxMatrixFromAngles01(0x0116); draw_list_end = r_.y_z; };
                    FxMatrixTranspose(0x0116);
                    flags = fx_ldw(FX_MODEL_FLAGS);
                    if ((flags & 0x2000) == 0) {
                        FxMatrixApplyVectors(0x0132, 0x8778);
                    } else {
                        fx_stw(FX_MODEL_118, 0);
                        fx_stw(FX_MODEL_11E, 0);
                        fx_stw(FX_MODEL_124, 0);
                        FxMatrixApplyVectors(0x0132, 0x876C);
                    }
                } else {
                    object_matrix_0 = fx_ldw(0x00E4);
                    fx_stw(FX_VIEW_MATRIX_0, (uint16_t)((uint16_t)~object_matrix_0 + 1));
                    object_matrix_1 = fx_ldw(FX_OBJECT_MATRIX_1);
                    fx_stw(FX_VIEW_MATRIX_1, (uint16_t)((uint16_t)~object_matrix_1 + 1));
                    object_matrix_2 = fx_ldw(FX_OBJECT_MATRIX_2);
                    fx_stw(FX_VIEW_MATRIX_2, (uint16_t)((uint16_t)~object_matrix_2 + 1));
                    object_matrix_3 = fx_ldw(FX_OBJECT_MATRIX_3);
                    fx_stw(FX_VIEW_MATRIX_3, object_matrix_3);
                    object_matrix_4 = fx_ldw(FX_OBJECT_MATRIX_4);
                    fx_stw(FX_VIEW_MATRIX_4, object_matrix_4);
                    object_matrix_5 = fx_ldw(FX_OBJECT_MATRIX_5);
                    fx_stw(FX_VIEW_MATRIX_5, object_matrix_5);
                    object_matrix_6 = fx_ldw(FX_OBJECT_MATRIX_6);
                    fx_stw(FX_VIEW_MATRIX_6, (uint16_t)((uint16_t)~object_matrix_6 + 1));
                    object_matrix_7 = fx_ldw(FX_OBJECT_MATRIX_7);
                    fx_stw(FX_VIEW_MATRIX_7, (uint16_t)((uint16_t)~object_matrix_7 + 1));
                    object_matrix_8 = fx_ldw(FX_OBJECT_MATRIX_8);
                    fx_stw(FX_VIEW_MATRIX_8, (uint16_t)((uint16_t)~object_matrix_8 + 1));
                    flags_2 = fx_ldw(FX_MODEL_FLAGS);
                    if ((flags_2 & 0x2000) == 0) {
                    } else {
                        fx_stw(FX_VIEW_MATRIX_3, 0);
                        fx_stw(FX_VIEW_MATRIX_4, 0);
                        fx_stw(FX_VIEW_MATRIX_5, 0);
                    }
                }
            } else {
                { FxMatrixFromAngles01Result r_ = FxMatrixFromAngles01(0x0116); draw_list_end = r_.y_z; };
                FxMatrixTranspose(0x0116);
                flags = fx_ldw(FX_MODEL_FLAGS);
                if ((flags & 0x2000) == 0) {
                    FxMatrixApplyVectors(0x0132, 0x8778);
                } else {
                    fx_stw(FX_MODEL_118, 0);
                    fx_stw(FX_MODEL_11E, 0);
                    fx_stw(FX_MODEL_124, 0);
                    FxMatrixApplyVectors(0x0132, 0x876C);
                }
            }
        } else {
            view_matrix_0 = fx_ldw(0x00E4);
            fx_stw(FX_VIEW_MATRIX_0, view_matrix_0);
            object_matrix_1_2 = fx_ldw(FX_OBJECT_MATRIX_1);
            fx_stw(FX_VIEW_MATRIX_1, object_matrix_1_2);
            object_matrix_2_2 = fx_ldw(FX_OBJECT_MATRIX_2);
            fx_stw(FX_VIEW_MATRIX_2, object_matrix_2_2);
            object_matrix_3_2 = fx_ldw(FX_OBJECT_MATRIX_3);
            fx_stw(FX_VIEW_MATRIX_3, object_matrix_3_2);
            object_matrix_4_2 = fx_ldw(FX_OBJECT_MATRIX_4);
            fx_stw(FX_VIEW_MATRIX_4, object_matrix_4_2);
            object_matrix_5_2 = fx_ldw(FX_OBJECT_MATRIX_5);
            fx_stw(FX_VIEW_MATRIX_5, object_matrix_5_2);
            object_matrix_6_2 = fx_ldw(FX_OBJECT_MATRIX_6);
            fx_stw(FX_VIEW_MATRIX_6, object_matrix_6_2);
            object_matrix_7_2 = fx_ldw(FX_OBJECT_MATRIX_7);
            fx_stw(FX_VIEW_MATRIX_7, object_matrix_7_2);
            object_matrix_8_2 = fx_ldw(FX_OBJECT_MATRIX_8);
            fx_stw(FX_VIEW_MATRIX_8, object_matrix_8_2);
            flags_3 = fx_ldw(FX_MODEL_FLAGS);
            if ((flags_3 & 0x2000) == 0) {
            } else {
                fx_stw(FX_VIEW_MATRIX_3, 0);
                fx_stw(FX_VIEW_MATRIX_4, 0);
                fx_stw(FX_VIEW_MATRIX_5, 0);
            }
        }
    } else {
        { FxMatrixFromAngles01Result r_ = FxMatrixFromAngles01(0x0116); draw_list_end = r_.y_z; };
        FxMatrixTranspose(0x0116);
        flags = fx_ldw(FX_MODEL_FLAGS);
        if ((flags & 0x2000) == 0) {
            FxMatrixApplyVectors(0x0132, 0x8778);
        } else {
            fx_stw(FX_MODEL_118, 0);
            fx_stw(FX_MODEL_11E, 0);
            fx_stw(FX_MODEL_124, 0);
            FxMatrixApplyVectors(0x0132, 0x876C);
        }
    }
    fx_stw(FX_LIGHT_X, 0);
    fx_stw(FX_LIGHT_Y, 0x393E);
    fx_stw(FX_LIGHT_Z, 0x727C);
    light_x = fx_ldw(FX_LIGHT_X);
    light_y = fx_ldw(FX_LIGHT_Y);
    light_z = fx_ldw(FX_LIGHT_Z);
    view_matrix_0_2 = fx_ldw(FX_VIEW_MATRIX_0);
    light_x_by_m0 = (uint32_t)((int32_t)(int16_t)light_x * (int16_t)view_matrix_0_2);
    view_matrix_1 = fx_ldw(FX_VIEW_MATRIX_1);
    light_y_by_m1 = (uint32_t)((int32_t)(int16_t)light_y * (int16_t)view_matrix_1);
    view_matrix_2 = fx_ldw(FX_VIEW_MATRIX_2);
    light_z_by_m2 = (uint32_t)((int32_t)(int16_t)light_z * (int16_t)view_matrix_2);
    fx_stw(FX_LIGHT_LOCAL_X, (uint16_t)(int16_t)(int8_t)(uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)(light_z_by_m2 >> 16) << 1 | (int)(light_z_by_m2 >> 15 & 1)) + (uint16_t)((uint16_t)(light_x_by_m0 >> 16) << 1 | (int)(light_x_by_m0 >> 15 & 1))) + (uint16_t)((uint16_t)(light_y_by_m1 >> 16) << 1 | (int)(light_y_by_m1 >> 15 & 1))) >> 8));
    view_matrix_3 = fx_ldw(FX_VIEW_MATRIX_3);
    light_x_by_m3 = (uint32_t)((int32_t)(int16_t)light_x * (int16_t)view_matrix_3);
    view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
    light_y_by_m4 = (uint32_t)((int32_t)(int16_t)light_y * (int16_t)view_matrix_4);
    view_matrix_5 = fx_ldw(FX_VIEW_MATRIX_5);
    light_z_by_m5 = (uint32_t)((int32_t)(int16_t)light_z * (int16_t)view_matrix_5);
    fx_stw(FX_LIGHT_LOCAL_Y, (uint16_t)(int16_t)(int8_t)(uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)(light_z_by_m5 >> 16) << 1 | (int)(light_z_by_m5 >> 15 & 1)) + (uint16_t)((uint16_t)(light_x_by_m3 >> 16) << 1 | (int)(light_x_by_m3 >> 15 & 1))) + (uint16_t)((uint16_t)(light_y_by_m4 >> 16) << 1 | (int)(light_y_by_m4 >> 15 & 1))) >> 8));
    view_matrix_6 = fx_ldw(FX_VIEW_MATRIX_6);
    light_x_by_m6 = (uint32_t)((int32_t)(int16_t)light_x * (int16_t)view_matrix_6);
    view_matrix_7 = fx_ldw(FX_VIEW_MATRIX_7);
    light_y_by_m7 = (uint32_t)((int32_t)(int16_t)light_y * (int16_t)view_matrix_7);
    view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
    light_z_by_m8 = (uint32_t)((int32_t)(int16_t)light_z * (int16_t)view_matrix_8);
    fx_stw(FX_LIGHT_LOCAL_Z, (uint16_t)(int16_t)(int8_t)(uint16_t)((uint16_t)((uint16_t)((uint16_t)((uint16_t)(light_z_by_m8 >> 16) << 1 | (int)(light_z_by_m8 >> 15 & 1)) + (uint16_t)((uint16_t)(light_x_by_m6 >> 16) << 1 | (int)(light_x_by_m6 >> 15 & 1))) + (uint16_t)((uint16_t)(light_y_by_m7 >> 16) << 1 | (int)(light_y_by_m7 >> 15 & 1))) >> 8));
    if (fx_far_shift()) {               /* a far object drawn halved with its place (fx_far.c), its light as it is */
        uint16_t at;

        for (at = FX_VIEW_MATRIX_0; at <= FX_VIEW_MATRIX_8; at = (uint16_t)(at + 2))
            fx_stw(at, (uint16_t)((int16_t)fx_ldw(at) >> fx_far_shift()));
    }
    detail_level = fx_ldw(0x0032);
    if (!((uint16_t)(detail_level - 3) & 0x8000)) {
    } else {
        packed_low_0 = fx_ldb(0x0133);
        packed_src_1 = (uint16_t)(0 + 0x0135);
        packed_high_0 = fx_ldb(packed_src_1);
        fx_stw(FX_MATRIX_PACKED_0, ((uint16_t)(packed_high_0 << 8 | packed_high_0 >> 8) | packed_low_0));
        packed_src_2 = (uint16_t)(packed_src_1 + 2);
        packed_low_1 = fx_ldb(packed_src_2);
        packed_src_3 = (uint16_t)(packed_src_2 + 2);
        packed_high_1 = fx_ldb(packed_src_3);
        fx_stw(FX_MATRIX_PACKED_1, ((uint16_t)(packed_high_1 << 8 | packed_high_1 >> 8) | packed_low_1));
        packed_src_4 = (uint16_t)(packed_src_3 + 2);
        packed_low_2 = fx_ldb(packed_src_4);
        packed_src_5 = (uint16_t)(packed_src_4 + 2);
        packed_high_2 = fx_ldb(packed_src_5);
        fx_stw(FX_MATRIX_PACKED_2, ((uint16_t)(packed_high_2 << 8 | packed_high_2 >> 8) | packed_low_2));
        packed_src_6 = (uint16_t)(packed_src_5 + 2);
        packed_low_3 = fx_ldb(packed_src_6);
        packed_src_7 = (uint16_t)(packed_src_6 + 2);
        packed_high_3 = fx_ldb(packed_src_7);
        fx_stw(FX_MATRIX_PACKED_3, ((uint16_t)(packed_high_3 << 8 | packed_high_3 >> 8) | packed_low_3));
        matrix_packed_4 = fx_ldb((uint16_t)(packed_src_7 + 2));
        fx_stw(FX_MATRIX_PACKED_4, matrix_packed_4);
    }
    fx_stw(FX_POINTS_TOTAL, 0);
    fx_stw(0x001E, 0x0340);
    model_rom_bank = fx_ldw(0x001C);
    fx_romb(model_rom_bank);
    rom_at_2 = fx_ldw(0x0016);
    fx_r14(rom_at_2);
    fx_stw(0x001A, rom_at_2);
    fx_push(stack, 0x9243);
    next_command_byte = fx_getb();
    next_command_ptr = (uint16_t)(rom_at_2 + 1);
    fx_r14(next_command_ptr);
    { FxModelNextResult r_ = FxModelNext(view_matrix_8, draw_list_end, stack, next_command_ptr, next_command_byte); up_ = r_.up; prev_end_y = r_.prev_end_y; draw_list_end_out_2 = r_.draw_list_end_out; rom_at_out = r_.rom_at_out; };
    if (up_ == 1) {
        up_ = 0;
    } else if (up_) {                /* it returned past us */
        return (FxModelDrawWithFlagsResult){draw_list_end_out_2, up_ == 2 ? 0 : up_ - 2};
    }
    { FxDrawListResult r_ = FxDrawList(prev_end_y, draw_list_end_out_2, stack, rom_at_out); caller_write_ptr_out = r_.caller_write_ptr_out; };
    fx_unspill(stack);
    return (FxModelDrawWithFlagsResult){caller_write_ptr_out, 0};
}


/*
 * @routine 01:884A FxModelReturn
 * WHAT IT DOES: Commands $18, $24, $2C and $48: return, pulling the
 * next command address from the R10 stack.
 * HOW: DEC R10 twice, R15 from (R10).
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelReturnResult FxModelReturn(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{

    fx_unspill(stack);
    return (FxModelReturnResult){draw_list_start, draw_list_end, rom_at, 1};
}


/*
 * @routine 01:884F FxModelFramePick
 * WHAT IT DOES: Command $1C: of the n lists that follow (n the next
 * byte), runs the one the model frame $0044 picks ($0044 AND $3F, less
 * n until below it).
 * HOW: R14 skips to the picked list's word offset and the list's first
 * command is dispatched.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelFramePickResult FxModelFramePick(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t frame_count;
    uint16_t frame_phase;
    uint16_t frame_entry_ptr;
    uint16_t animation_counter;
    uint16_t rom_ptr_after_count;

    frame_count = fx_getb();
    rom_ptr_after_count = (uint16_t)(rom_at + 1);
    fx_r14(rom_ptr_after_count);
    animation_counter = fx_ldw(0x0044);
    frame_phase = (animation_counter & 0x003F);
    for (;;) {
        if ((uint16_t)(frame_phase - frame_count) & 0x8000) {
            break;
        } else {
            frame_phase = (uint16_t)(frame_phase - frame_count);
            continue;
        }
    }
    frame_entry_ptr = (uint16_t)((uint16_t)(rom_ptr_after_count + frame_phase) + frame_phase);
    fx_r14(frame_entry_ptr);
    { FxModelJumpResult tr_ = FxModelJump(draw_list_start, draw_list_end, stack, frame_entry_ptr); return (FxModelFramePickResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8863 FxModelJump
 * WHAT IT DOES: Command $20: jumps within the commands by the signed
 * word that follows.
 * HOW: GETB, GETBH, R14 += it, dispatch.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelJumpResult FxModelJump(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t offset_lo;
    uint16_t jump_offset;
    uint16_t jump_target;
    uint16_t next_rom_at;
    uint16_t next_command;
    uint16_t rom_ptr_offset_hi;

    offset_lo = fx_getb();
    rom_ptr_offset_hi = (uint16_t)(rom_at + 1);
    fx_r14(rom_ptr_offset_hi);
    jump_offset = (uint16_t)((offset_lo & 0xFF) | fx_getb() << 8);
    jump_target = (uint16_t)(rom_ptr_offset_hi + jump_offset);
    fx_r14(jump_target);
    next_command = fx_getb();
    next_rom_at = (uint16_t)(jump_target + 1);
    fx_r14(next_rom_at);
    { FxModelNextResult tr_ = FxModelNext(draw_list_start, draw_list_end, stack, next_rom_at, next_command); return (FxModelJumpResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:886C FxModelPointsFull
 * WHAT IT DOES: Commands $08 and $4C: n points (the next byte) turned
 * by the view matrix ($0132-$0142, held in R10, R11, R13, R4, R7, R8
 * and two RAM words) into x, y, z words at $001E (not reached in the
 * traces so far).
 * HOW: R10 kept at $0012 while it holds a matrix element; a count of
 * DEC R12; $0144 counts the points.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelPointsFullResult FxModelPointsFull(uint16_t *stack, uint16_t rom_at)
{
    uint16_t count;
    uint16_t model_x;
    uint16_t model_y;
    uint16_t model_z;
    uint16_t rom_at_2;
    uint16_t model_x_lo;
    uint16_t model_y_lo;
    uint16_t model_z_lo;
    uint16_t points_left;
    uint16_t points_total;
    uint32_t prod_x_view0;
    uint32_t prod_x_view1;
    uint32_t prod_x_view2;
    uint32_t prod_y_view3;
    uint32_t prod_y_view4;
    uint32_t prod_y_view5;
    uint32_t prod_z_view6;
    uint32_t prod_z_view7;
    uint32_t prod_z_view8;
    uint16_t rom_ptr_x_hi;
    uint16_t rom_ptr_y_hi;
    uint16_t rom_ptr_y_lo;
    uint16_t rom_ptr_z_hi;
    uint16_t rom_ptr_z_lo;
    uint16_t out_point_ptr;
    uint16_t view_matrix_0;
    uint16_t view_matrix_1;
    uint16_t view_matrix_2;
    uint16_t view_matrix_3;
    uint16_t view_matrix_4;
    uint16_t view_matrix_5;
    uint16_t view_matrix_6;
    uint16_t view_matrix_7;
    uint16_t view_matrix_8;
    uint16_t out_ptr_view_y;
    uint16_t out_ptr_view_z;
    uint16_t next_command_ptr;
    uint16_t next_command_byte;

    count = fx_getb();
    rom_at_2 = (uint16_t)(rom_at + 1);
    fx_r14(rom_at_2);
    points_total = fx_ldw(FX_POINTS_TOTAL);
    fx_sbk((uint16_t)(points_total + count));
    out_point_ptr = fx_ldw(0x001E);
    fx_stw(0x0012, (*stack));
    view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
    view_matrix_3 = fx_ldw(FX_VIEW_MATRIX_3);
    view_matrix_6 = fx_ldw(FX_VIEW_MATRIX_6);
    view_matrix_1 = fx_ldw(FX_VIEW_MATRIX_1);
    view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
    view_matrix_7 = fx_ldw(FX_VIEW_MATRIX_7);
    for (;;) {
        model_x_lo = fx_getb();
        rom_ptr_x_hi = (uint16_t)(rom_at_2 + 1);
        fx_r14(rom_ptr_x_hi);
        model_x = (uint16_t)((model_x_lo & 0xFF) | fx_getb() << 8);
        rom_ptr_y_lo = (uint16_t)(rom_ptr_x_hi + 1);
        fx_r14(rom_ptr_y_lo);
        model_y_lo = fx_getb();
        rom_ptr_y_hi = (uint16_t)(rom_ptr_y_lo + 1);
        fx_r14(rom_ptr_y_hi);
        model_y = (uint16_t)((model_y_lo & 0xFF) | fx_getb() << 8);
        rom_ptr_z_lo = (uint16_t)(rom_ptr_y_hi + 1);
        fx_r14(rom_ptr_z_lo);
        model_z_lo = fx_getb();
        rom_ptr_z_hi = (uint16_t)(rom_ptr_z_lo + 1);
        fx_r14(rom_ptr_z_hi);
        model_z = (uint16_t)((model_z_lo & 0xFF) | fx_getb() << 8);
        rom_at_2 = (uint16_t)(rom_ptr_z_hi + 1);
        fx_r14(rom_at_2);
        prod_x_view0 = (uint32_t)((int32_t)(int16_t)model_x * (int16_t)view_matrix_0);
        prod_y_view3 = (uint32_t)((int32_t)(int16_t)model_y * (int16_t)view_matrix_3);
        prod_z_view6 = (uint32_t)((int32_t)(int16_t)model_z * (int16_t)view_matrix_6);
        fx_stw(out_point_ptr, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_z_view6 >> 16) << 1 | (int)(prod_z_view6 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_x_view0 >> 16) << 1 | (int)(prod_x_view0 >> 15 & 1))) + (uint16_t)((uint16_t)(prod_y_view3 >> 16) << 1 | (int)(prod_y_view3 >> 15 & 1))));
        out_ptr_view_y = (uint16_t)(out_point_ptr + 2);
        prod_x_view1 = (uint32_t)((int32_t)(int16_t)model_x * (int16_t)view_matrix_1);
        prod_y_view4 = (uint32_t)((int32_t)(int16_t)model_y * (int16_t)view_matrix_4);
        prod_z_view7 = (uint32_t)((int32_t)(int16_t)model_z * (int16_t)view_matrix_7);
        fx_stw(out_ptr_view_y, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_z_view7 >> 16) << 1 | (int)(prod_z_view7 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_x_view1 >> 16) << 1 | (int)(prod_x_view1 >> 15 & 1))) + (uint16_t)((uint16_t)(prod_y_view4 >> 16) << 1 | (int)(prod_y_view4 >> 15 & 1))));
        out_ptr_view_z = (uint16_t)(out_ptr_view_y + 2);
        view_matrix_2 = fx_ldw(FX_VIEW_MATRIX_2);
        prod_x_view2 = (uint32_t)((int32_t)(int16_t)model_x * (int16_t)view_matrix_2);
        view_matrix_5 = fx_ldw(FX_VIEW_MATRIX_5);
        prod_y_view5 = (uint32_t)((int32_t)(int16_t)model_y * (int16_t)view_matrix_5);
        view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
        prod_z_view8 = (uint32_t)((int32_t)(int16_t)model_z * (int16_t)view_matrix_8);
        fx_stw(out_ptr_view_z, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_z_view8 >> 16) << 1 | (int)(prod_z_view8 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_x_view2 >> 16) << 1 | (int)(prod_x_view2 >> 15 & 1))) + (uint16_t)((uint16_t)(prod_y_view5 >> 16) << 1 | (int)(prod_y_view5 >> 15 & 1))));
        points_left = (uint16_t)(count - 1);
        count = points_left;
        out_point_ptr = (uint16_t)(out_ptr_view_z + 2);
        if (points_left != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_stw(0x001E, out_point_ptr);
    (*stack) = fx_ldw(0x0012);
    next_command_byte = fx_getb();
    next_command_ptr = (uint16_t)(rom_at_2 + 1);
    fx_r14(next_command_ptr);
    { FxModelNextResult tr_ = FxModelNext(view_matrix_8, view_matrix_4, stack, next_command_ptr, next_command_byte); return (FxModelPointsFullResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:88F3 FxModelPointsScaled
 * WHAT IT DOES: Command $34: n points of three words (GETB, GETBH),
 * each turned by the view matrix (FMULT, ROL) and written twice at
 * $001E: as given and with x negated (the x term subtracted twice);
 * $0144 counts 2n (not reached in the traces so far).
 * HOW: As FxModelPointsFull, with FMULT and ROL by the matrix
 * registers.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelPointsScaledResult FxModelPointsScaled(uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t count;
    uint16_t view_x;
    uint16_t view_y;
    uint16_t view_z;
    uint16_t out_ptr;
    uint16_t point_x;
    uint16_t point_y;
    uint16_t point_z;
    uint16_t rom_at_2;
    uint16_t out_y_ptr;
    uint16_t out_z_ptr;
    uint16_t next_rom_at;
    uint16_t point_x_low;
    uint16_t point_y_low;
    uint16_t point_z_low;
    uint16_t mirror_x_ptr;
    uint16_t mirror_y_ptr;
    uint16_t mirror_z_ptr;
    uint16_t next_command;
    uint16_t points_total;
    uint32_t x_by_m0_prod;
    uint16_t x_by_m0_term;
    uint32_t x_by_m1_prod;
    uint16_t x_by_m1_term;
    uint32_t x_by_m2_prod;
    uint16_t x_by_m2_term;
    uint32_t y_by_m3_prod;
    uint32_t y_by_m4_prod;
    uint32_t y_by_m5_prod;
    uint32_t z_by_m6_prod;
    uint32_t z_by_m7_prod;
    uint32_t z_by_m8_prod;
    uint16_t rom_ptr_y_low;
    uint16_t rom_ptr_z_low;
    uint16_t view_matrix_0;
    uint16_t view_matrix_1;
    uint16_t view_matrix_2;
    uint16_t view_matrix_3;
    uint16_t view_matrix_4;
    uint16_t view_matrix_5;
    uint16_t view_matrix_6;
    uint16_t view_matrix_7;
    uint16_t view_matrix_8;
    uint16_t rom_ptr_x_high;
    uint16_t rom_ptr_y_high;
    uint16_t rom_ptr_z_high;

    count = fx_getb();
    rom_at_2 = (uint16_t)(rom_at + 1);
    fx_r14(rom_at_2);
    points_total = fx_ldw(FX_POINTS_TOTAL);
    fx_sbk((uint16_t)((uint16_t)(points_total + count) + count));
    out_ptr = fx_ldw(0x001E);
    for (;;) {
        point_x_low = fx_getb();
        rom_ptr_x_high = (uint16_t)(rom_at_2 + 1);
        fx_r14(rom_ptr_x_high);
        point_x = (uint16_t)((point_x_low & 0xFF) | fx_getb() << 8);
        rom_ptr_y_low = (uint16_t)(rom_ptr_x_high + 1);
        fx_r14(rom_ptr_y_low);
        point_y_low = fx_getb();
        rom_ptr_y_high = (uint16_t)(rom_ptr_y_low + 1);
        fx_r14(rom_ptr_y_high);
        point_y = (uint16_t)((point_y_low & 0xFF) | fx_getb() << 8);
        rom_ptr_z_low = (uint16_t)(rom_ptr_y_high + 1);
        fx_r14(rom_ptr_z_low);
        point_z_low = fx_getb();
        rom_ptr_z_high = (uint16_t)(rom_ptr_z_low + 1);
        fx_r14(rom_ptr_z_high);
        point_z = (uint16_t)((point_z_low & 0xFF) | fx_getb() << 8);
        rom_at_2 = (uint16_t)(rom_ptr_z_high + 1);
        fx_r14(rom_at_2);
        view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
        x_by_m0_prod = (uint32_t)((int32_t)(int16_t)point_x * (int16_t)view_matrix_0);
        x_by_m0_term = (uint16_t)((uint16_t)(x_by_m0_prod >> 16) << 1 | (int)(x_by_m0_prod >> 15 & 1));
        view_matrix_3 = fx_ldw(FX_VIEW_MATRIX_3);
        y_by_m3_prod = (uint32_t)((int32_t)(int16_t)point_y * (int16_t)view_matrix_3);
        view_matrix_6 = fx_ldw(FX_VIEW_MATRIX_6);
        z_by_m6_prod = (uint32_t)((int32_t)(int16_t)point_z * (int16_t)view_matrix_6);
        view_x = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_by_m6_prod >> 16) << 1 | (int)(z_by_m6_prod >> 15 & 1)) + (uint16_t)((uint16_t)(y_by_m3_prod >> 16) << 1 | (int)(y_by_m3_prod >> 15 & 1))) + x_by_m0_term);
        fx_stw(out_ptr, view_x);
        mirror_x_ptr = (uint16_t)(out_ptr + 6);
        fx_stw(mirror_x_ptr, (uint16_t)((uint16_t)(view_x - x_by_m0_term) - x_by_m0_term));
        out_y_ptr = (uint16_t)(mirror_x_ptr - 4);
        view_matrix_1 = fx_ldw(FX_VIEW_MATRIX_1);
        x_by_m1_prod = (uint32_t)((int32_t)(int16_t)point_x * (int16_t)view_matrix_1);
        x_by_m1_term = (uint16_t)((uint16_t)(x_by_m1_prod >> 16) << 1 | (int)(x_by_m1_prod >> 15 & 1));
        view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
        y_by_m4_prod = (uint32_t)((int32_t)(int16_t)point_y * (int16_t)view_matrix_4);
        view_matrix_7 = fx_ldw(FX_VIEW_MATRIX_7);
        z_by_m7_prod = (uint32_t)((int32_t)(int16_t)point_z * (int16_t)view_matrix_7);
        view_y = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_by_m7_prod >> 16) << 1 | (int)(z_by_m7_prod >> 15 & 1)) + (uint16_t)((uint16_t)(y_by_m4_prod >> 16) << 1 | (int)(y_by_m4_prod >> 15 & 1))) + x_by_m1_term);
        fx_stw(out_y_ptr, view_y);
        mirror_y_ptr = (uint16_t)(out_y_ptr + 6);
        fx_stw(mirror_y_ptr, (uint16_t)((uint16_t)(view_y - x_by_m1_term) - x_by_m1_term));
        out_z_ptr = (uint16_t)(mirror_y_ptr - 4);
        view_matrix_2 = fx_ldw(FX_VIEW_MATRIX_2);
        x_by_m2_prod = (uint32_t)((int32_t)(int16_t)point_x * (int16_t)view_matrix_2);
        x_by_m2_term = (uint16_t)((uint16_t)(x_by_m2_prod >> 16) << 1 | (int)(x_by_m2_prod >> 15 & 1));
        view_matrix_5 = fx_ldw(FX_VIEW_MATRIX_5);
        y_by_m5_prod = (uint32_t)((int32_t)(int16_t)point_y * (int16_t)view_matrix_5);
        view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
        z_by_m8_prod = (uint32_t)((int32_t)(int16_t)point_z * (int16_t)view_matrix_8);
        view_z = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_by_m8_prod >> 16) << 1 | (int)(z_by_m8_prod >> 15 & 1)) + (uint16_t)((uint16_t)(y_by_m5_prod >> 16) << 1 | (int)(y_by_m5_prod >> 15 & 1))) + x_by_m2_term);
        fx_stw(out_z_ptr, view_z);
        mirror_z_ptr = (uint16_t)(out_z_ptr + 6);
        fx_stw(mirror_z_ptr, (uint16_t)((uint16_t)(view_z - x_by_m2_term) - x_by_m2_term));
        out_ptr = (uint16_t)(mirror_z_ptr + 2);
        count = (uint16_t)(count - 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_stw(0x001E, out_ptr);
    next_command = fx_getb();
    next_rom_at = (uint16_t)(rom_at_2 + 1);
    fx_r14(next_rom_at);
    { FxModelNextResult tr_ = FxModelNext(view_matrix_8, draw_list_end, stack, next_rom_at, next_command); return (FxModelPointsScaledResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8A97 FxModelPoints01
 * WHAT IT DOES: Command $04: n points of three signed bytes each,
 * scaled by $0030 and turned by the packed matrix ($0128-$0130),
 * written as x, y, z words at $001E.
 * HOW: A LOOP of R12 = n; MULT of each byte by the packed matrix bytes;
 * $0144 counts the points; models of fewer than 3 points per $0032 take
 * 01:8982.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelPoints01Result FxModelPoints01(uint16_t *stack, uint16_t rom_at)
{
    uint16_t count;
    uint16_t count_2;
    uint16_t out_ptr;
    uint16_t point_x;
    uint16_t point_y;
    uint16_t point_z;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t scaled_x;
    uint16_t scaled_y;
    uint16_t scaled_z;
    uint16_t out_y_ptr;
    uint16_t out_z_ptr;
    uint16_t rom_ptr_y;
    uint16_t rom_ptr_z;
    uint32_t x_times_m0;
    uint32_t x_times_m1;
    uint32_t x_times_m2;
    uint32_t y_times_m3;
    uint32_t y_times_m4;
    uint32_t y_times_m5;
    uint32_t z_times_m6;
    uint32_t z_times_m7;
    uint32_t z_times_m8;
    uint16_t model_scale;
    uint16_t points_left;
    uint16_t out_ptr_view;
    uint16_t points_total;
    uint16_t rotated_x_sum;
    uint16_t rotated_y_sum;
    uint16_t rotated_z_sum;
    uint16_t view_matrix_0;
    uint16_t view_matrix_1;
    uint16_t view_matrix_2;
    uint16_t view_matrix_3;
    uint16_t view_matrix_4;
    uint16_t view_matrix_5;
    uint16_t view_matrix_6;
    uint16_t view_matrix_7;
    uint16_t view_matrix_8;
    uint16_t out_y_ptr_view;
    uint16_t out_z_ptr_view;
    uint16_t points_total_2;
    uint16_t rom_ptr_y_view;
    uint16_t rom_ptr_z_view;
    uint16_t matrix_packed_0;
    uint16_t matrix_packed_1;
    uint16_t matrix_packed_2;
    uint16_t matrix_packed_3;
    uint16_t matrix_packed_4;
    uint16_t model_point_mode;
    uint16_t model_scale_view;
    uint16_t next_rom_at_view;
    uint16_t next_command_view;
    uint16_t point_byte_x_view;
    uint16_t point_byte_y_view;
    uint16_t point_byte_z_view;
    uint16_t next_rom_at_packed;
    uint16_t next_command_packed;
    uint16_t scale_with_matrix_byte;

    model_point_mode = fx_ldw(0x0032);
    if ((uint16_t)(model_point_mode - 3) & 0x8000) {
        out_ptr = fx_ldw(0x001E);
        matrix_packed_0 = fx_ldw(FX_MATRIX_PACKED_0);
        matrix_packed_1 = fx_ldw(FX_MATRIX_PACKED_1);
        matrix_packed_2 = fx_ldw(FX_MATRIX_PACKED_2);
        matrix_packed_3 = fx_ldw(FX_MATRIX_PACKED_3);
        matrix_packed_4 = fx_ldw(FX_MATRIX_PACKED_4);
        model_scale = fx_ldw(0x0030);
        scale_with_matrix_byte = ((uint16_t)((uint16_t)(matrix_packed_4 & 0xFF) << 8 | (uint16_t)(matrix_packed_4 & 0xFF) >> 8) | model_scale);
        count = fx_getb();
        rom_at_2 = (uint16_t)(rom_at + 1);
        fx_r14(rom_at_2);
        points_total = fx_ldw(FX_POINTS_TOTAL);
        fx_sbk((uint16_t)(points_total + count));
        for (;;) {
            point_x = fx_getb();
            rom_ptr_y = (uint16_t)(rom_at_2 + 1);
            fx_r14(rom_ptr_y);
            point_y = fx_getb();
            rom_ptr_z = (uint16_t)(rom_ptr_y + 1);
            fx_r14(rom_ptr_z);
            point_z = fx_getb();
            rom_at_2 = (uint16_t)(rom_ptr_z + 1);
            fx_r14(rom_at_2);
            rotated_x_sum = (uint16_t)((uint16_t)((uint16_t)((int8_t)matrix_packed_3 * (int8_t)point_z) + (uint16_t)((int8_t)(uint16_t)(matrix_packed_1 >> 8) * (int8_t)point_y)) + (uint16_t)((int8_t)matrix_packed_0 * (int8_t)point_x));
            fx_stw(out_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(rotated_x_sum + rotated_x_sum) >> 8) * (int8_t)scale_with_matrix_byte));
            out_y_ptr = (uint16_t)(out_ptr + 2);
            rotated_y_sum = (uint16_t)((uint16_t)((uint16_t)((int8_t)(uint16_t)(matrix_packed_3 >> 8) * (int8_t)point_z) + (uint16_t)((int8_t)matrix_packed_2 * (int8_t)point_y)) + (uint16_t)((int8_t)(uint16_t)(matrix_packed_0 >> 8) * (int8_t)point_x));
            fx_stw(out_y_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(rotated_y_sum + rotated_y_sum) >> 8) * (int8_t)scale_with_matrix_byte));
            out_z_ptr = (uint16_t)(out_y_ptr + 2);
            rotated_z_sum = (uint16_t)((uint16_t)((uint16_t)((int8_t)(uint16_t)(scale_with_matrix_byte >> 8) * (int8_t)point_z) + (uint16_t)((int8_t)(uint16_t)(matrix_packed_2 >> 8) * (int8_t)point_y)) + (uint16_t)((int8_t)matrix_packed_1 * (int8_t)point_x));
            fx_stw(out_z_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(rotated_z_sum + rotated_z_sum) >> 8) * (int8_t)scale_with_matrix_byte));
            count = (uint16_t)(count - 1);
            out_ptr = (uint16_t)(out_z_ptr + 2);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        fx_stw(0x001E, out_ptr);
        next_command_packed = fx_getb();
        next_rom_at_packed = (uint16_t)(rom_at_2 + 1);
        fx_r14(next_rom_at_packed);
        { FxModelNextResult tr_ = FxModelNext(matrix_packed_1, matrix_packed_2, stack, next_rom_at_packed, next_command_packed); return (FxModelPoints01Result){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    } else {
        out_ptr_view = fx_ldw(0x001E);
        fx_stw(0x0012, (*stack));
        view_matrix_3 = fx_ldw(FX_VIEW_MATRIX_3);
        view_matrix_6 = fx_ldw(FX_VIEW_MATRIX_6);
        view_matrix_1 = fx_ldw(FX_VIEW_MATRIX_1);
        view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
        view_matrix_7 = fx_ldw(FX_VIEW_MATRIX_7);
        view_matrix_2 = fx_ldw(FX_VIEW_MATRIX_2);
        count_2 = fx_getb();
        rom_at_3 = (uint16_t)(rom_at + 1);
        fx_r14(rom_at_3);
        points_total_2 = fx_ldw(FX_POINTS_TOTAL);
        fx_sbk((uint16_t)(points_total_2 + count_2));
        for (;;) {
            point_byte_x_view = fx_getb();
            rom_ptr_y_view = (uint16_t)(rom_at_3 + 1);
            fx_r14(rom_ptr_y_view);
            model_scale_view = fx_ldw(0x0030);
            scaled_x = (uint16_t)((int8_t)point_byte_x_view * (int8_t)model_scale_view);
            point_byte_y_view = fx_getb();
            rom_ptr_z_view = (uint16_t)(rom_ptr_y_view + 1);
            fx_r14(rom_ptr_z_view);
            view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
            scaled_y = (uint16_t)((int8_t)point_byte_y_view * (int8_t)model_scale_view);
            point_byte_z_view = fx_getb();
            rom_at_3 = (uint16_t)(rom_ptr_z_view + 1);
            fx_r14(rom_at_3);
            scaled_z = (uint16_t)((int8_t)point_byte_z_view * (int8_t)model_scale_view);
            x_times_m0 = (uint32_t)((int32_t)(int16_t)scaled_x * (int16_t)view_matrix_0);
            y_times_m3 = (uint32_t)((int32_t)(int16_t)scaled_y * (int16_t)view_matrix_3);
            z_times_m6 = (uint32_t)((int32_t)(int16_t)scaled_z * (int16_t)view_matrix_6);
            fx_stw(out_ptr_view, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_times_m6 >> 16) << 1 | (int)(z_times_m6 >> 15 & 1)) + (uint16_t)((uint16_t)(x_times_m0 >> 16) << 1 | (int)(x_times_m0 >> 15 & 1))) + (uint16_t)((uint16_t)(y_times_m3 >> 16) << 1 | (int)(y_times_m3 >> 15 & 1))));
            out_y_ptr_view = (uint16_t)(out_ptr_view + 2);
            x_times_m1 = (uint32_t)((int32_t)(int16_t)scaled_x * (int16_t)view_matrix_1);
            y_times_m4 = (uint32_t)((int32_t)(int16_t)scaled_y * (int16_t)view_matrix_4);
            z_times_m7 = (uint32_t)((int32_t)(int16_t)scaled_z * (int16_t)view_matrix_7);
            fx_stw(out_y_ptr_view, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_times_m7 >> 16) << 1 | (int)(z_times_m7 >> 15 & 1)) + (uint16_t)((uint16_t)(x_times_m1 >> 16) << 1 | (int)(x_times_m1 >> 15 & 1))) + (uint16_t)((uint16_t)(y_times_m4 >> 16) << 1 | (int)(y_times_m4 >> 15 & 1))));
            out_z_ptr_view = (uint16_t)(out_y_ptr_view + 2);
            x_times_m2 = (uint32_t)((int32_t)(int16_t)scaled_x * (int16_t)view_matrix_2);
            view_matrix_5 = fx_ldw(FX_VIEW_MATRIX_5);
            y_times_m5 = (uint32_t)((int32_t)(int16_t)scaled_y * (int16_t)view_matrix_5);
            view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
            z_times_m8 = (uint32_t)((int32_t)(int16_t)scaled_z * (int16_t)view_matrix_8);
            fx_stw(out_z_ptr_view, (uint16_t)((uint16_t)((uint16_t)((uint16_t)(z_times_m8 >> 16) << 1 | (int)(z_times_m8 >> 15 & 1)) + (uint16_t)((uint16_t)(x_times_m2 >> 16) << 1 | (int)(x_times_m2 >> 15 & 1))) + (uint16_t)((uint16_t)(y_times_m5 >> 16) << 1 | (int)(y_times_m5 >> 15 & 1))));
            points_left = (uint16_t)(count_2 - 1);
            count_2 = points_left;
            out_ptr_view = (uint16_t)(out_z_ptr_view + 2);
            if (points_left != 0) {
                continue;
            } else {
                break;
            }
        }
        (*stack) = fx_ldw(0x0012);
        fx_stw(0x001E, out_ptr_view);
        next_command_view = fx_getb();
        next_rom_at_view = (uint16_t)(rom_at_3 + 1);
        fx_r14(next_rom_at_view);
        { FxModelNextResult tr_ = FxModelNext(view_matrix_8, view_matrix_4, stack, next_rom_at_view, next_command_view); return (FxModelPoints01Result){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
}


/*
 * @routine 01:8B0E FxModelPointsMirrored
 * WHAT IT DOES: Command $38: as command $04, each point also written
 * mirrored in x (two points per entry).
 * HOW: The x term added and subtracted (ADD R8, SUB R8 twice) for the
 * pair.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelPointsMirroredResult FxModelPointsMirrored(uint16_t *stack, uint16_t rom_at)
{
    uint16_t count;
    uint16_t view_x;
    uint16_t view_y;
    uint16_t view_z;
    uint16_t count_2;
    uint16_t out_ptr;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t yz_sum_a;
    uint16_t yz_sum_b;
    uint16_t yz_sum_c;
    uint32_t prod_x_m0;
    uint32_t prod_x_m1;
    uint32_t prod_x_m2;
    uint32_t prod_y_m3;
    uint32_t prod_y_m4;
    uint32_t prod_y_m5;
    uint32_t prod_z_m6;
    uint32_t prod_z_m7;
    uint32_t prod_z_m8;
    uint16_t row_b_ptr;
    uint16_t row_c_ptr;
    uint16_t term_x_m0;
    uint16_t term_x_m1;
    uint16_t term_x_m2;
    uint16_t row_a_plus;
    uint16_t row_b_plus;
    uint16_t row_c_plus;
    uint16_t view_y_ptr;
    uint16_t view_z_ptr;
    uint16_t x_times_m0;
    uint16_t x_times_m1;
    uint16_t point_scale;
    uint16_t point_x_raw;
    uint16_t point_y_raw;
    uint16_t point_z_raw;
    uint16_t points_left;
    uint16_t row_a_minus;
    uint16_t row_b_minus;
    uint16_t row_c_minus;
    uint16_t mirror_a_ptr;
    uint16_t mirror_b_ptr;
    uint16_t mirror_c_ptr;
    uint16_t mirror_x_ptr;
    uint16_t mirror_y_ptr;
    uint16_t next_out_ptr;
    uint16_t point_x_byte;
    uint16_t point_y_byte;
    uint16_t point_z_byte;
    uint16_t points_total;
    uint16_t view_matrix_0;
    uint16_t view_matrix_1;
    uint16_t view_matrix_2;
    uint16_t view_matrix_3;
    uint16_t view_matrix_4;
    uint16_t view_matrix_5;
    uint16_t view_matrix_6;
    uint16_t view_matrix_7;
    uint16_t view_matrix_8;
    uint16_t x_times_m0_hi;
    uint16_t next_point_ptr;
    uint16_t out_ptr_packed;
    uint16_t point_x_scaled;
    uint16_t point_y_scaled;
    uint16_t point_z_scaled;
    uint16_t points_total_2;
    uint16_t matrix_packed_0;
    uint16_t matrix_packed_1;
    uint16_t matrix_packed_2;
    uint16_t matrix_packed_3;
    uint16_t matrix_packed_4;
    uint16_t next_point_ptr_2;
    uint16_t scale_limit_word;
    uint16_t view_command_ptr;
    uint16_t scale_matrix_word;
    uint16_t view_command_byte;
    uint16_t packed_command_ptr;
    uint16_t point_scale_packed;
    uint16_t points_left_packed;
    uint16_t packed_command_byte;
    uint16_t next_point_ptr_packed;
    uint16_t next_point_ptr_packed_2;

    scale_limit_word = fx_ldw(0x0032);
    if ((uint16_t)(scale_limit_word - 3) & 0x8000) {
        out_ptr_packed = fx_ldw(0x001E);
        matrix_packed_0 = fx_ldw(FX_MATRIX_PACKED_0);
        matrix_packed_1 = fx_ldw(FX_MATRIX_PACKED_1);
        matrix_packed_2 = fx_ldw(FX_MATRIX_PACKED_2);
        matrix_packed_3 = fx_ldw(FX_MATRIX_PACKED_3);
        matrix_packed_4 = fx_ldw(FX_MATRIX_PACKED_4);
        point_scale_packed = fx_ldw(0x0030);
        scale_matrix_word = ((uint16_t)((uint16_t)(matrix_packed_4 & 0xFF) << 8 | (uint16_t)(matrix_packed_4 & 0xFF) >> 8) | point_scale_packed);
        count = fx_getb();
        rom_at_2 = (uint16_t)(rom_at + 1);
        fx_r14(rom_at_2);
        points_total = fx_ldw(FX_POINTS_TOTAL);
        fx_sbk((uint16_t)((uint16_t)(points_total + count) + count));
        for (;;) {
            point_x_byte = fx_getb();
            next_point_ptr_packed = (uint16_t)(rom_at_2 + 1);
            fx_r14(next_point_ptr_packed);
            x_times_m0 = (uint16_t)((int8_t)matrix_packed_0 * (int8_t)point_x_byte);
            point_y_byte = fx_getb();
            next_point_ptr_packed_2 = (uint16_t)(next_point_ptr_packed + 1);
            fx_r14(next_point_ptr_packed_2);
            point_z_byte = fx_getb();
            rom_at_2 = (uint16_t)(next_point_ptr_packed_2 + 1);
            fx_r14(rom_at_2);
            yz_sum_a = (uint16_t)((uint16_t)((int8_t)matrix_packed_3 * (int8_t)point_z_byte) + (uint16_t)((int8_t)(uint16_t)(matrix_packed_1 >> 8) * (int8_t)point_y_byte));
            row_a_plus = (uint16_t)(yz_sum_a + x_times_m0);
            fx_stw(out_ptr_packed, (uint16_t)((int8_t)(uint16_t)((uint16_t)(row_a_plus + row_a_plus) >> 8) * (int8_t)scale_matrix_word));
            mirror_a_ptr = (uint16_t)(out_ptr_packed + 6);
            row_a_minus = (uint16_t)(yz_sum_a - x_times_m0);
            fx_stw(mirror_a_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(row_a_minus + row_a_minus) >> 8) * (int8_t)scale_matrix_word));
            row_b_ptr = (uint16_t)(mirror_a_ptr - 4);
            x_times_m0_hi = (uint16_t)((int8_t)(uint16_t)(matrix_packed_0 >> 8) * (int8_t)point_x_byte);
            yz_sum_b = (uint16_t)((uint16_t)((int8_t)(uint16_t)(matrix_packed_3 >> 8) * (int8_t)point_z_byte) + (uint16_t)((int8_t)matrix_packed_2 * (int8_t)point_y_byte));
            row_b_plus = (uint16_t)(yz_sum_b + x_times_m0_hi);
            fx_stw(row_b_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(row_b_plus + row_b_plus) >> 8) * (int8_t)scale_matrix_word));
            mirror_b_ptr = (uint16_t)(row_b_ptr + 6);
            row_b_minus = (uint16_t)(yz_sum_b - x_times_m0_hi);
            fx_stw(mirror_b_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(row_b_minus + row_b_minus) >> 8) * (int8_t)scale_matrix_word));
            row_c_ptr = (uint16_t)(mirror_b_ptr - 4);
            x_times_m1 = (uint16_t)((int8_t)matrix_packed_1 * (int8_t)point_x_byte);
            yz_sum_c = (uint16_t)((uint16_t)((int8_t)(uint16_t)(scale_matrix_word >> 8) * (int8_t)point_z_byte) + (uint16_t)((int8_t)(uint16_t)(matrix_packed_2 >> 8) * (int8_t)point_y_byte));
            row_c_plus = (uint16_t)(yz_sum_c + x_times_m1);
            fx_stw(row_c_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(row_c_plus + row_c_plus) >> 8) * (int8_t)scale_matrix_word));
            mirror_c_ptr = (uint16_t)(row_c_ptr + 6);
            row_c_minus = (uint16_t)(yz_sum_c - x_times_m1);
            fx_stw(mirror_c_ptr, (uint16_t)((int8_t)(uint16_t)((uint16_t)(row_c_minus + row_c_minus) >> 8) * (int8_t)scale_matrix_word));
            out_ptr_packed = (uint16_t)(mirror_c_ptr + 2);
            points_left_packed = (uint16_t)(count - 1);
            count = points_left_packed;
            if (points_left_packed != 0) {
                continue;
            } else {
                break;
            }
        }
        fx_stw(0x001E, out_ptr_packed);
        packed_command_byte = fx_getb();
        packed_command_ptr = (uint16_t)(rom_at_2 + 1);
        fx_r14(packed_command_ptr);
        { FxModelNextResult tr_ = FxModelNext(matrix_packed_1, matrix_packed_2, stack, packed_command_ptr, packed_command_byte); return (FxModelPointsMirroredResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    } else {
        out_ptr = fx_ldw(0x001E);
        fx_stw(0x0012, (*stack));
        view_matrix_3 = fx_ldw(FX_VIEW_MATRIX_3);
        view_matrix_6 = fx_ldw(FX_VIEW_MATRIX_6);
        view_matrix_1 = fx_ldw(FX_VIEW_MATRIX_1);
        view_matrix_4 = fx_ldw(FX_VIEW_MATRIX_4);
        view_matrix_2 = fx_ldw(FX_VIEW_MATRIX_2);
        count_2 = fx_getb();
        rom_at_3 = (uint16_t)(rom_at + 1);
        fx_r14(rom_at_3);
        points_total_2 = fx_ldw(FX_POINTS_TOTAL);
        fx_sbk((uint16_t)((uint16_t)(points_total_2 + count_2) + count_2));
        for (;;) {
            point_x_raw = fx_getb();
            next_point_ptr = (uint16_t)(rom_at_3 + 1);
            fx_r14(next_point_ptr);
            point_scale = fx_ldw(0x0030);
            point_x_scaled = (uint16_t)((int8_t)point_x_raw * (int8_t)point_scale);
            point_y_raw = fx_getb();
            next_point_ptr_2 = (uint16_t)(next_point_ptr + 1);
            fx_r14(next_point_ptr_2);
            view_matrix_0 = fx_ldw(FX_VIEW_MATRIX_0);
            point_y_scaled = (uint16_t)((int8_t)point_y_raw * (int8_t)point_scale);
            point_z_raw = fx_getb();
            rom_at_3 = (uint16_t)(next_point_ptr_2 + 1);
            fx_r14(rom_at_3);
            point_z_scaled = (uint16_t)((int8_t)point_z_raw * (int8_t)point_scale);
            prod_x_m0 = (uint32_t)((int32_t)(int16_t)point_x_scaled * (int16_t)view_matrix_0);
            term_x_m0 = (uint16_t)((uint16_t)(prod_x_m0 >> 16) << 1 | (int)(prod_x_m0 >> 15 & 1));
            prod_y_m3 = (uint32_t)((int32_t)(int16_t)point_y_scaled * (int16_t)view_matrix_3);
            prod_z_m6 = (uint32_t)((int32_t)(int16_t)point_z_scaled * (int16_t)view_matrix_6);
            view_x = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_z_m6 >> 16) << 1 | (int)(prod_z_m6 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_y_m3 >> 16) << 1 | (int)(prod_y_m3 >> 15 & 1))) + term_x_m0);
            fx_stw(out_ptr, view_x);
            mirror_x_ptr = (uint16_t)(out_ptr + 6);
            fx_stw(mirror_x_ptr, (uint16_t)((uint16_t)(view_x - term_x_m0) - term_x_m0));
            view_y_ptr = (uint16_t)(mirror_x_ptr - 4);
            prod_x_m1 = (uint32_t)((int32_t)(int16_t)point_x_scaled * (int16_t)view_matrix_1);
            term_x_m1 = (uint16_t)((uint16_t)(prod_x_m1 >> 16) << 1 | (int)(prod_x_m1 >> 15 & 1));
            prod_y_m4 = (uint32_t)((int32_t)(int16_t)point_y_scaled * (int16_t)view_matrix_4);
            view_matrix_7 = fx_ldw(FX_VIEW_MATRIX_7);
            prod_z_m7 = (uint32_t)((int32_t)(int16_t)point_z_scaled * (int16_t)view_matrix_7);
            view_y = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_z_m7 >> 16) << 1 | (int)(prod_z_m7 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_y_m4 >> 16) << 1 | (int)(prod_y_m4 >> 15 & 1))) + term_x_m1);
            fx_stw(view_y_ptr, view_y);
            mirror_y_ptr = (uint16_t)(view_y_ptr + 6);
            fx_stw(mirror_y_ptr, (uint16_t)((uint16_t)(view_y - term_x_m1) - term_x_m1));
            view_z_ptr = (uint16_t)(mirror_y_ptr - 4);
            prod_x_m2 = (uint32_t)((int32_t)(int16_t)point_x_scaled * (int16_t)view_matrix_2);
            term_x_m2 = (uint16_t)((uint16_t)(prod_x_m2 >> 16) << 1 | (int)(prod_x_m2 >> 15 & 1));
            view_matrix_5 = fx_ldw(FX_VIEW_MATRIX_5);
            prod_y_m5 = (uint32_t)((int32_t)(int16_t)point_y_scaled * (int16_t)view_matrix_5);
            view_matrix_8 = fx_ldw(FX_VIEW_MATRIX_8);
            prod_z_m8 = (uint32_t)((int32_t)(int16_t)point_z_scaled * (int16_t)view_matrix_8);
            view_z = (uint16_t)((uint16_t)((uint16_t)((uint16_t)(prod_z_m8 >> 16) << 1 | (int)(prod_z_m8 >> 15 & 1)) + (uint16_t)((uint16_t)(prod_y_m5 >> 16) << 1 | (int)(prod_y_m5 >> 15 & 1))) + term_x_m2);
            fx_stw(view_z_ptr, view_z);
            next_out_ptr = (uint16_t)(view_z_ptr + 6);
            fx_stw(next_out_ptr, (uint16_t)((uint16_t)(view_z - term_x_m2) - term_x_m2));
            points_left = (uint16_t)(count_2 - 1);
            count_2 = points_left;
            out_ptr = (uint16_t)(next_out_ptr + 2);
            if (points_left != 0) {
                continue;
            } else {
                break;
            }
        }
        (*stack) = fx_ldw(0x0012);
        fx_stw(0x001E, out_ptr);
        view_command_byte = fx_getb();
        view_command_ptr = (uint16_t)(rom_at_3 + 1);
        fx_r14(view_command_ptr);
        { FxModelNextResult tr_ = FxModelNext(view_matrix_8, view_matrix_4, stack, view_command_ptr, view_command_byte); return (FxModelPointsMirroredResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
}


/*
 * @routine 01:8BAA FxModelProject
 * WHAT IT DOES: Command $0C: projects the points written so far
 * (FxProjectVertex, or FxProjectPoint for near ones) for the faces that
 * follow.
 * HOW: Its rest runs at 01:8C79 (FxModelProjectRest).
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelProjectResult FxModelProject(uint16_t *stack, uint16_t rom_at)
{
    uint16_t flags;
    uint16_t view_y;
    uint16_t stack_2;
    uint16_t center_x;
    uint16_t center_y;
    uint16_t outcodes;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t rom_at_4;
    uint16_t rom_at_5;
    uint16_t screen_x;
    uint16_t screen_y;
    uint16_t vertex_x;
    uint16_t vertex_y;
    uint16_t vertex_z;
    uint16_t depth_addr;
    uint16_t outcodes_2;
    uint16_t outcodes_3;
    uint16_t outcodes_4;
    uint16_t screen_x_2;
    uint16_t screen_y_2;
    uint16_t vertex_ptr;
    uint16_t view_depth;
    uint16_t outcode_ptr;
    uint16_t points_left;
    uint16_t recip_scale;
    uint16_t cam_offset_x;
    uint16_t cam_offset_y;
    uint16_t cam_offset_z;
    uint16_t next_out_ptr;
    uint16_t outcode_word;
    uint16_t points_total;
    uint16_t screen_max_x;
    uint16_t screen_max_y;
    uint16_t screen_y_ptr;
    uint16_t vertex_x_ptr;
    uint16_t vertex_x_raw;
    uint16_t vertex_y_ptr;
    uint16_t vertex_y_raw;
    uint16_t vertex_z_raw;
    uint16_t clamped_depth;
    uint16_t screen_y_addr;
    uint16_t vertex_y_addr;
    uint16_t vertex_z_addr;
    uint16_t view_offset_x;
    uint16_t view_offset_y;
    uint16_t view_offset_z;
    uint16_t outcode_word_2;
    uint16_t recip_low_byte;
    uint16_t saved_rom_bank;
    uint16_t depth_minus_far;
    uint16_t next_vertex_ptr;
    uint16_t depth_minus_near;
    uint16_t next_command_ptr;
    uint16_t outcode_plus_one;
    uint16_t points_left_next;
    uint16_t recip_table_addr;
    uint16_t next_command_byte;
    uint16_t saved_rom_bank_end;

    screen_max_x = fx_ldw(0x003A);
    screen_max_y = fx_ldw(0x003E);
    center_x = fx_ldw(0x0034);
    center_y = fx_ldw(0x0036);
    cam_offset_x = fx_ldw(0x0026);
    cam_offset_y = fx_ldw(0x0028);
    cam_offset_z = fx_ldw(0x002A);
    points_total = fx_ldw(FX_POINTS_TOTAL);
    fx_stw(0x0012, (*stack));
    stack_2 = 0;
    fx_romb(0x0014);
    fx_stw(0x001A, rom_at);
    vertex_ptr = 0x0344;
    next_out_ptr = 0x04E4;
    for (;;) {
        vertex_z_raw = fx_ldw(vertex_ptr);
        depth_minus_near = (uint16_t)((uint16_t)(vertex_z_raw + cam_offset_z) - 0x0100);
        view_depth = (uint16_t)(depth_minus_near + 0x0100);
        if (!(depth_minus_near & 0x8000)) {
            depth_minus_far = (uint16_t)(view_depth - 0x43FE);
            clamped_depth = (uint16_t)(depth_minus_far + 0x43FE);
            if (depth_minus_far & 0x8000) {
            } else {
                clamped_depth = 0x43FE;
            }
            recip_table_addr = (uint16_t)((uint16_t)(clamped_depth & ~1) - 0x7694);
            fx_r14(recip_table_addr);
            vertex_y_ptr = (uint16_t)(vertex_ptr - 2);
            vertex_y_raw = fx_ldw(vertex_y_ptr);
            view_y = (uint16_t)(vertex_y_raw + cam_offset_y);
            recip_low_byte = fx_getb();
            fx_r14((uint16_t)(recip_table_addr + 1));
            vertex_x_ptr = (uint16_t)(vertex_y_ptr - 2);
            vertex_x_raw = fx_ldw(vertex_x_ptr);
            recip_scale = (uint16_t)((recip_low_byte & 0xFF) | fx_getb() << 8);
            screen_x = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(vertex_x_raw + cam_offset_x) * (int16_t)recip_scale) >> 16) + center_x);
            fx_face_point(next_out_ptr, (uint16_t)(vertex_x_raw + cam_offset_x), view_y, view_depth);
            fx_stw(next_out_ptr, screen_x);
            rom_at_2 = 0;
            fx_r14(rom_at_2);
            if (screen_x & 0x8000) {
                outcode_plus_one = (uint16_t)(rom_at_2 + 1);
                fx_r14(outcode_plus_one);
                rom_at_2 = (uint16_t)((int8_t)outcode_plus_one * (int8_t)4);
                fx_r14(rom_at_2);
            } else {
                if ((uint16_t)(screen_x - screen_max_x) & 0x8000) {
                } else {
                    rom_at_2 = (uint16_t)(rom_at_2 + 1);
                    fx_r14(rom_at_2);
                    outcode_plus_one = (uint16_t)(rom_at_2 + 1);
                    fx_r14(outcode_plus_one);
                    rom_at_2 = (uint16_t)((int8_t)outcode_plus_one * (int8_t)4);
                    fx_r14(rom_at_2);
                }
            }
            screen_y_ptr = (uint16_t)(next_out_ptr + 2);
            screen_y = (uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)view_y * (int16_t)recip_scale) >> 16) + center_y);
            fx_stw(screen_y_ptr, screen_y);
            if (screen_y & 0x8000) {
                rom_at_2 = (uint16_t)(rom_at_2 + 1);
                fx_r14(rom_at_2);
            } else {
                if ((uint16_t)(screen_y - screen_max_y) & 0x8000) {
                } else {
                    rom_at_2 = (uint16_t)(rom_at_2 + 1);
                    fx_r14(rom_at_2);
                    rom_at_2 = (uint16_t)(rom_at_2 + 1);
                    fx_r14(rom_at_2);
                }
            }
            outcode_ptr = (uint16_t)(screen_y_ptr + 2);
            outcode_word = ((uint16_t)((0x001F ^ rom_at_2) << 8 | (0x001F ^ rom_at_2) >> 8) | rom_at_2);
            fx_stw(outcode_ptr, outcode_word);
            stack_2 = (outcode_word | stack_2);
            vertex_ptr = (uint16_t)(vertex_x_ptr + 0x000A);
            points_left_next = (uint16_t)(points_total - 1);
            points_total = points_left_next;
            next_out_ptr = (uint16_t)(outcode_ptr + 2);
            if (points_left_next != 0) {
                continue;
            } else {
                break;
            }
        } else {
            next_vertex_ptr = (uint16_t)(vertex_ptr - 4);
            outcodes = stack_2;
            (*stack) = fx_ldw(0x0012);
            fx_stw(FX_OUTCODES, outcodes);
            view_offset_z = fx_ldw(0x002A);
            view_offset_x = fx_ldw(0x0026);
            view_offset_y = fx_ldw(0x0028);
            for (;;) {
                vertex_x = fx_ldw(next_vertex_ptr);
                vertex_y_addr = (uint16_t)(next_vertex_ptr + 2);
                vertex_y = fx_ldw(vertex_y_addr);
                vertex_z_addr = (uint16_t)(vertex_y_addr + 2);
                vertex_z = fx_ldw(vertex_z_addr);
                next_vertex_ptr = (uint16_t)(vertex_z_addr + 2);
                fx_face_point(next_out_ptr, (uint16_t)(vertex_x + view_offset_x), (uint16_t)(vertex_y + view_offset_y), (uint16_t)(vertex_z + view_offset_z));
                { FxProjectVertexResult r_ = FxProjectVertex((uint16_t)(vertex_x + view_offset_x), (uint16_t)(vertex_y + view_offset_y), view_offset_x, (uint16_t)(vertex_z + view_offset_z), view_offset_z, stack, points_total, view_offset_y); outcode_word_2 = r_.outcode_word; screen_x_2 = r_.screen_x; screen_y_2 = r_.screen_y; view_offset_x = r_.exit_aux_a; view_offset_z = r_.exit_aux_c; view_offset_y = r_.exit_aux_b; };
                fx_stw(next_out_ptr, screen_x_2);
                screen_y_addr = (uint16_t)(next_out_ptr + 2);
                fx_stw(screen_y_addr, screen_y_2);
                depth_addr = (uint16_t)(screen_y_addr + 2);
                fx_stw(depth_addr, outcode_word_2);
                next_out_ptr = (uint16_t)(depth_addr + 2);
                outcodes_2 = fx_ldw(FX_OUTCODES);
                points_left = (uint16_t)(points_total - 1);
                points_total = points_left;
                fx_sbk((outcode_word_2 | outcodes_2));
                if (points_left != 0) {
                    continue;
                } else {
                    break;
                }
            }
            saved_rom_bank = fx_ldw(0x001C);
            fx_romb(saved_rom_bank);
            rom_at_3 = fx_ldw(0x001A);
            fx_r14(rom_at_3);
            { FxModelProjectRestResult tr_ = FxModelProjectRest(outcodes_2, next_vertex_ptr, stack, rom_at_3); return (FxModelProjectResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
        }
    }
    fx_stw(FX_OUTCODES, stack_2);
    (*stack) = fx_ldw(0x0012);
    saved_rom_bank_end = fx_ldw(0x001C);
    fx_romb(saved_rom_bank_end);
    rom_at_4 = fx_ldw(0x001A);
    fx_r14(rom_at_4);
    outcodes_3 = fx_ldw(FX_OUTCODES);
    if ((outcodes_3 & 0x1F00) == 0x1F00) {
        outcodes_4 = fx_ldw(FX_OUTCODES);
        if ((0x001F & outcodes_4) != 0) {
        } else {
            flags = fx_ldw(FX_MODEL_FLAGS);
            fx_sbk((flags | 0x4000));
        }
        rom_at_5 = fx_ldw(0x0018);
        fx_r14(rom_at_5);
        fx_stw(0x001A, rom_at_5);
        next_command_byte = fx_getb();
        next_command_ptr = (uint16_t)(rom_at_5 + 1);
        fx_r14(next_command_ptr);
        { FxModelNextResult tr_ = FxModelNext(recip_scale, vertex_ptr, stack, next_command_ptr, next_command_byte); return (FxModelProjectResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    } else {
        fx_unspill(stack);
        return (FxModelProjectResult){recip_scale, vertex_ptr, rom_at_4, 1};
    }
}


/*
 * @routine 01:8BAE FxModelProjectEach
 * WHAT IT DOES: A projection of the points written so far that command
 * $0C does not use: each point ($0340 on, $0144 of them) moved by the
 * object's place and projected by FxProjectVertex into the table at
 * $04E4, the outcodes ORed into $014A; then on as FxModelProjectRest.
 * The game never runs it: FxModelProject jumps over it (to 01:8C89)
 * with its first instruction. A unit start runs it so the C is judged
 * against the original.
 * HOW: ROMB $14 for the loop, R14 kept in $001A; the ROM bank ($001C)
 * and R14 put back after it, then IWT R15 to 01:8C79.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes;
 * the ROM bank the model's.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelProjectEachResult FxModelProjectEach(uint16_t *stack, uint16_t rom_at)
{
    uint16_t origin_x;
    uint16_t origin_y;
    uint16_t origin_z;
    uint16_t outcodes;
    uint16_t rom_at_2;
    uint16_t screen_x;
    uint16_t screen_y;
    uint16_t vertex_x;
    uint16_t vertex_y;
    uint16_t vertex_z;
    uint16_t points_left;
    uint16_t outcode_word;
    uint16_t points_total;
    uint16_t vertex_y_ptr;
    uint16_t vertex_z_ptr;
    uint16_t saved_rom_bank;
    uint16_t vertex_src_ptr;
    uint16_t projected_y_ptr;
    uint16_t projected_z_ptr;
    uint16_t projected_out_ptr;

    points_total = fx_ldw(FX_POINTS_TOTAL);
    fx_romb(0x0014);
    vertex_src_ptr = 0x0340;
    fx_stw(0x001A, rom_at);
    projected_out_ptr = 0x04E4;
    fx_stw(FX_OUTCODES, 0);
    origin_z = fx_ldw(0x002A);
    origin_x = fx_ldw(0x0026);
    origin_y = fx_ldw(0x0028);
    for (;;) {
        vertex_x = fx_ldw(vertex_src_ptr);
        vertex_y_ptr = (uint16_t)(vertex_src_ptr + 2);
        vertex_y = fx_ldw(vertex_y_ptr);
        vertex_z_ptr = (uint16_t)(vertex_y_ptr + 2);
        vertex_z = fx_ldw(vertex_z_ptr);
        vertex_src_ptr = (uint16_t)(vertex_z_ptr + 2);
        fx_face_point(projected_out_ptr, (uint16_t)(vertex_x + origin_x), (uint16_t)(vertex_y + origin_y), (uint16_t)(vertex_z + origin_z));
        { FxProjectVertexResult r_ = FxProjectVertex((uint16_t)(vertex_x + origin_x), (uint16_t)(vertex_y + origin_y), origin_x, (uint16_t)(vertex_z + origin_z), origin_z, stack, points_total, origin_y); outcode_word = r_.outcode_word; screen_x = r_.screen_x; screen_y = r_.screen_y; origin_x = r_.exit_aux_a; origin_z = r_.exit_aux_c; origin_y = r_.exit_aux_b; };
        fx_stw(projected_out_ptr, screen_x);
        projected_y_ptr = (uint16_t)(projected_out_ptr + 2);
        fx_stw(projected_y_ptr, screen_y);
        projected_z_ptr = (uint16_t)(projected_y_ptr + 2);
        fx_stw(projected_z_ptr, outcode_word);
        projected_out_ptr = (uint16_t)(projected_z_ptr + 2);
        outcodes = fx_ldw(FX_OUTCODES);
        points_left = (uint16_t)(points_total - 1);
        points_total = points_left;
        fx_sbk((outcode_word | outcodes));
        if (points_left != 0) {
            continue;
        } else {
            break;
        }
    }
    saved_rom_bank = fx_ldw(0x001C);
    fx_romb(saved_rom_bank);
    rom_at_2 = fx_ldw(0x001A);
    fx_r14(rom_at_2);
    { FxModelProjectRestResult tr_ = FxModelProjectRest(outcodes, vertex_src_ptr, stack, rom_at_2); return (FxModelProjectEachResult){tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8C79 FxModelProjectRest
 * WHAT IT DOES: The rest of command $0C's projection: each point's
 * screen position and outcode, then the next command.
 * HOW: Reached by jump from 01:8BFE.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelProjectRestResult FxModelProjectRest(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t flags;
    uint16_t outcodes;
    uint16_t rom_at_2;
    uint16_t outcodes_2;
    uint16_t next_rom_at;
    uint16_t next_command;

    outcodes = fx_ldw(FX_OUTCODES);
    if ((outcodes & 0x1F00) == 0x1F00) {
        outcodes_2 = fx_ldw(FX_OUTCODES);
        if ((0x001F & outcodes_2) != 0) {
        } else {
            flags = fx_ldw(FX_MODEL_FLAGS);
            fx_sbk((flags | 0x4000));
        }
        rom_at_2 = fx_ldw(0x0018);
        fx_r14(rom_at_2);
        fx_stw(0x001A, rom_at_2);
        next_command = fx_getb();
        next_rom_at = (uint16_t)(rom_at_2 + 1);
        fx_r14(next_rom_at);
        { FxModelNextResult tr_ = FxModelNext(draw_list_start, draw_list_end, stack, next_rom_at, next_command); return (FxModelProjectRestResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    } else {
        fx_unspill(stack);
        return (FxModelProjectRestResult){draw_list_start, draw_list_end, rom_at, 1};
    }
}


/*
 * @routine 01:8D4D FxModelListStart
 * WHAT IT DOES: Command $58: starts the sub-list returns: $0056 = $0788
 * and 01:8E7D pushed on the R10 stack, then the next command.
 * HOW: CACHE; dispatch.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelListStartResult FxModelListStart(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t r_t_12;
    uint16_t rom_at_2;
    uint16_t return_at;
    uint16_t return_at_2;
    uint16_t command_code;
    uint16_t prev_end_y_2;
    uint16_t rom_at_out_2;
    uint16_t rom_at_out_3;
    uint16_t next_command_ptr;
    uint16_t next_command_byte;
    uint16_t draw_list_end_out_2;

    fx_stw(FX_MODEL_RETURN_AT, 0x0788);
    fx_push(stack, 0x8E7D);
    next_command_byte = fx_getb();
    next_command_ptr = (uint16_t)(rom_at + 1);
    fx_r14(next_command_ptr);
    { FxModelNextResult r_ = FxModelNext(draw_list_start, draw_list_end, stack, next_command_ptr, next_command_byte); up_ = r_.up; prev_end_y_2 = r_.prev_end_y; draw_list_end_out_2 = r_.draw_list_end_out; rom_at_out_2 = r_.rom_at_out; };
    if (up_ == 1) {
        up_ = 0;
    } else if (up_) {                /* it returned past us */
        return (FxModelListStartResult){prev_end_y_2, draw_list_end_out_2, rom_at_out_2, up_ == 1 ? 0 : up_ - 1};
    }
    return_at = fx_ldw(FX_MODEL_RETURN_AT);
    fx_stw(return_at, 0);
    fx_stw(FX_MODEL_RETURN_AT, 0x0788);
    for (;;) {
        return_at_2 = fx_ldw(FX_MODEL_RETURN_AT);
        rom_at_2 = fx_ldw(return_at_2);
        fx_r14(rom_at_2);
        fx_stw(FX_MODEL_RETURN_AT, (uint16_t)(return_at_2 + 2));
        fx_r14(rom_at_2);
        if (rom_at_2 == 0) {
            break;
        } else {
            fx_push(stack, (uint16_t)(0 - 0x7161));
            command_code = fx_getb();
            r_t_12 = (uint16_t)(rom_at_2 + 1);
            fx_r14(r_t_12);
            { FxModelNextResult r_ = FxModelNext(prev_end_y_2, draw_list_end_out_2, stack, r_t_12, command_code); up_ = r_.up; prev_end_y_2 = r_.prev_end_y; draw_list_end_out_2 = r_.draw_list_end_out; rom_at_out_3 = r_.rom_at_out; };
            if (up_ == 1) {
                up_ = 0;
            } else if (up_) {                /* it returned past us */
                return (FxModelListStartResult){prev_end_y_2, draw_list_end_out_2, rom_at_out_3, up_ == 1 ? 0 : up_ - 1};
            }
            fx_r14((uint16_t)(rom_at_out_3 + 1));
            continue;
        }
    }
    { FxModelEndResult tr_ = FxModelEnd(prev_end_y_2, draw_list_end_out_2, stack, rom_at_2); return (FxModelListStartResult){tr_.model_state_r6, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8D5D FxModelCall
 * WHAT IT DOES: Command $60: calls the commands at the signed word
 * offset that follows, the return (past the word) kept at the $0056
 * stack.
 * HOW: R14 refilled (DEC, INC); the return stored and $0056 stepped;
 * R14 += the offset; dispatch.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelCallResult FxModelCall(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t return_at;
    uint16_t callee_ptr;
    uint16_t call_offset;
    uint16_t call_op_ptr;
    uint16_t call_offset_low;
    uint16_t call_op_ptr_next;
    uint16_t next_command_ptr;
    uint16_t next_command_byte;
    uint16_t call_offset_end_ptr;

    call_op_ptr = (uint16_t)(rom_at - 1);
    fx_r14(call_op_ptr);
    call_op_ptr_next = (uint16_t)(call_op_ptr + 1);
    fx_r14(call_op_ptr_next);
    return_at = fx_ldw(FX_MODEL_RETURN_AT);
    fx_stw(return_at, (uint16_t)(call_op_ptr_next + 2));
    fx_stw(FX_MODEL_RETURN_AT, (uint16_t)(return_at + 2));
    call_offset_low = fx_getb();
    call_offset_end_ptr = (uint16_t)(call_op_ptr_next + 1);
    fx_r14(call_offset_end_ptr);
    call_offset = (uint16_t)((call_offset_low & 0xFF) | fx_getb() << 8);
    callee_ptr = (uint16_t)(call_offset + call_offset_end_ptr);
    fx_r14(callee_ptr);
    next_command_byte = fx_getb();
    next_command_ptr = (uint16_t)(callee_ptr + 1);
    fx_r14(next_command_ptr);
    { FxModelNextResult tr_ = FxModelNext(draw_list_start, draw_list_end, stack, next_command_ptr, next_command_byte); return (FxModelCallResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8D75 FxModelReturnPull
 * WHAT IT DOES: Command $64: return, pulling the next command address
 * from the R10 stack.
 * HOW: DEC R10 twice, R15 from (R10).
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelReturnPullResult FxModelReturnPull(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{

    fx_unspill(stack);
    return (FxModelReturnPullResult){draw_list_start, draw_list_end, rom_at, 1};
}


/*
 * @routine 01:8D7A FxModelIfCall
 * WHAT IT DOES: Command $5C: the flag byte at $07C6 plus the next byte
 * picks, by its bit 7, one of two lists at the word offsets that
 * follow; the one picked runs with R14 pushed on the R10 stack and
 * comes back past both.
 * HOW: R14 pushed on the R10 stack around the call; two forms (8D7A,
 * 8DBD).
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelIfCallResult FxModelIfCall(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t r0_20;
    uint16_t r0_22;
    uint16_t r0_48;
    uint16_t r0_50;
    uint16_t r_t_18;
    uint16_t r_t_19;
    uint16_t r_t_21;
    uint16_t r_t_23;
    uint16_t r_t_25;
    uint16_t r_t_30;
    uint16_t r_t_31;
    uint16_t r_t_32;
    uint16_t r_t_33;
    uint16_t r_t_35;
    uint16_t r_t_46;
    uint16_t r_t_47;
    uint16_t r_t_49;
    uint16_t r_t_51;
    uint16_t r_t_53;
    uint16_t r_t_58;
    uint16_t r_t_59;
    uint16_t r_t_61;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t rom_at_4;
    uint16_t rom_at_5;
    uint16_t command_code;
    uint16_t condition_id;
    uint16_t prev_end_y_2;
    uint16_t prev_end_y_3;
    uint16_t prev_end_y_4;
    uint16_t prev_end_y_5;
    uint16_t rom_at_out_2;
    uint16_t rom_at_out_3;
    uint16_t rom_at_out_4;
    uint16_t rom_at_out_5;
    uint16_t then_rom_ptr;
    uint16_t command_code_2;
    uint16_t command_code_3;
    uint16_t command_code_4;
    uint16_t else_rom_ptr_1;
    uint16_t rom_ptr_skip_1;
    uint16_t rom_ptr_skip_2;
    uint16_t condition_entry;
    uint16_t else_offset_low;
    uint16_t else_target_ptr;
    uint16_t then_offset_low;
    uint16_t then_target_ptr;
    uint16_t else_command_ptr;
    uint16_t else_jump_offset;
    uint16_t then_command_ptr;
    uint16_t then_jump_offset;
    uint16_t else_command_byte;
    uint16_t then_command_byte;
    uint16_t draw_list_end_out_2;
    uint16_t draw_list_end_out_3;
    uint16_t draw_list_end_out_4;
    uint16_t draw_list_end_out_5;
    uint16_t rom_ptr_after_condition;

    condition_id = fx_getb();
    rom_ptr_after_condition = (uint16_t)(rom_at + 1);
    fx_r14(rom_ptr_after_condition);
    condition_entry = fx_ldb((uint16_t)(condition_id + 0x07C6));
    if ((uint16_t)(condition_entry << 8 | condition_entry >> 8) & 0x8000) {
        fx_push(stack, rom_ptr_after_condition);
        rom_ptr_skip_1 = (uint16_t)(rom_ptr_after_condition + 1);
        fx_r14(rom_ptr_skip_1);
        rom_ptr_skip_2 = (uint16_t)(rom_ptr_skip_1 + 1);
        fx_r14(rom_ptr_skip_2);
        then_offset_low = fx_getb();
        then_rom_ptr = (uint16_t)(rom_ptr_skip_2 + 1);
        fx_r14(then_rom_ptr);
        then_jump_offset = (uint16_t)((then_offset_low & 0xFF) | fx_getb() << 8);
        then_target_ptr = (uint16_t)(then_jump_offset + then_rom_ptr);
        fx_r14(then_target_ptr);
        fx_push(stack, (uint16_t)(0 - 0x722F));
        then_command_byte = fx_getb();
        then_command_ptr = (uint16_t)(then_target_ptr + 1);
        fx_r14(then_command_ptr);
        { FxModelNextResult r_ = FxModelNext(draw_list_start, draw_list_end, stack, then_command_ptr, then_command_byte); up_ = r_.up; prev_end_y_2 = r_.prev_end_y; draw_list_end_out_2 = r_.draw_list_end_out; rom_at_out_2 = r_.rom_at_out; };
        if (up_ == 1) {
            up_ = 0;
        } else if (up_) {                /* it returned past us */
            return (FxModelIfCallResult){prev_end_y_2, draw_list_end_out_2, rom_at_out_2, up_ == 2 ? 0 : up_ - 2};
        }
        fx_r14((uint16_t)(rom_at_out_2 + 1));
        rom_at_2 = fx_pull(stack);
        fx_r14(rom_at_2);
        r_t_18 = (uint16_t)(rom_at_2 - 1);
        fx_r14(r_t_18);
        r_t_19 = (uint16_t)(r_t_18 + 1);
        fx_r14(r_t_19);
        fx_push(stack, r_t_19);
        r0_20 = fx_getb();
        r_t_21 = (uint16_t)(r_t_19 + 1);
        fx_r14(r_t_21);
        r0_22 = (uint16_t)((r0_20 & 0xFF) | fx_getb() << 8);
        r_t_23 = (uint16_t)(r0_22 + r_t_21);
        fx_r14(r_t_23);
        fx_push(stack, (uint16_t)(0 - 0x7216));
        command_code = fx_getb();
        r_t_25 = (uint16_t)(r_t_23 + 1);
        fx_r14(r_t_25);
        { FxModelNextResult r_ = FxModelNext(prev_end_y_2, draw_list_end_out_2, stack, r_t_25, command_code); up_ = r_.up; prev_end_y_3 = r_.prev_end_y; draw_list_end_out_3 = r_.draw_list_end_out; rom_at_out_3 = r_.rom_at_out; };
        if (up_ == 1) {
            up_ = 0;
        } else if (up_) {                /* it returned past us */
            return (FxModelIfCallResult){prev_end_y_3, draw_list_end_out_3, rom_at_out_3, up_ == 2 ? 0 : up_ - 2};
        }
        fx_r14((uint16_t)(rom_at_out_3 + 1));
        rom_at_3 = fx_pull(stack);
        fx_r14(rom_at_3);
        r_t_30 = (uint16_t)(rom_at_3 + 1);
        fx_r14(r_t_30);
        r_t_31 = (uint16_t)(r_t_30 + 1);
        fx_r14(r_t_31);
        r_t_32 = (uint16_t)(r_t_31 + 1);
        fx_r14(r_t_32);
        r_t_33 = (uint16_t)(r_t_32 + 1);
        fx_r14(r_t_33);
        command_code_2 = fx_getb();
        r_t_35 = (uint16_t)(r_t_33 + 1);
        fx_r14(r_t_35);
        { FxModelNextResult tr_ = FxModelNext(prev_end_y_3, draw_list_end_out_3, stack, r_t_35, command_code_2); return (FxModelIfCallResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    } else {
        fx_push(stack, rom_ptr_after_condition);
        else_offset_low = fx_getb();
        else_rom_ptr_1 = (uint16_t)(rom_ptr_after_condition + 1);
        fx_r14(else_rom_ptr_1);
        else_jump_offset = (uint16_t)((else_offset_low & 0xFF) | fx_getb() << 8);
        else_target_ptr = (uint16_t)(else_jump_offset + else_rom_ptr_1);
        fx_r14(else_target_ptr);
        fx_push(stack, (uint16_t)(0 - 0x7266));
        else_command_byte = fx_getb();
        else_command_ptr = (uint16_t)(else_target_ptr + 1);
        fx_r14(else_command_ptr);
        { FxModelNextResult r_ = FxModelNext(draw_list_start, draw_list_end, stack, else_command_ptr, else_command_byte); up_ = r_.up; prev_end_y_4 = r_.prev_end_y; draw_list_end_out_4 = r_.draw_list_end_out; rom_at_out_4 = r_.rom_at_out; };
        if (up_ == 1) {
            up_ = 0;
        } else if (up_) {                /* it returned past us */
            return (FxModelIfCallResult){prev_end_y_4, draw_list_end_out_4, rom_at_out_4, up_ == 2 ? 0 : up_ - 2};
        }
        fx_r14((uint16_t)(rom_at_out_4 + 1));
        rom_at_4 = fx_pull(stack);
        fx_r14(rom_at_4);
        r_t_46 = (uint16_t)(rom_at_4 + 1);
        fx_r14(r_t_46);
        r_t_47 = (uint16_t)(r_t_46 + 1);
        fx_r14(r_t_47);
        fx_push(stack, r_t_47);
        r0_48 = fx_getb();
        r_t_49 = (uint16_t)(r_t_47 + 1);
        fx_r14(r_t_49);
        r0_50 = (uint16_t)((r0_48 & 0xFF) | fx_getb() << 8);
        r_t_51 = (uint16_t)(r0_50 + r_t_49);
        fx_r14(r_t_51);
        fx_push(stack, (uint16_t)(0 - 0x724D));
        command_code_3 = fx_getb();
        r_t_53 = (uint16_t)(r_t_51 + 1);
        fx_r14(r_t_53);
        { FxModelNextResult r_ = FxModelNext(prev_end_y_4, draw_list_end_out_4, stack, r_t_53, command_code_3); up_ = r_.up; prev_end_y_5 = r_.prev_end_y; draw_list_end_out_5 = r_.draw_list_end_out; rom_at_out_5 = r_.rom_at_out; };
        if (up_ == 1) {
            up_ = 0;
        } else if (up_) {                /* it returned past us */
            return (FxModelIfCallResult){prev_end_y_5, draw_list_end_out_5, rom_at_out_5, up_ == 2 ? 0 : up_ - 2};
        }
        fx_r14((uint16_t)(rom_at_out_5 + 1));
        rom_at_5 = fx_pull(stack);
        fx_r14(rom_at_5);
        r_t_58 = (uint16_t)(rom_at_5 + 1);
        fx_r14(r_t_58);
        r_t_59 = (uint16_t)(r_t_58 + 1);
        fx_r14(r_t_59);
        command_code_4 = fx_getb();
        r_t_61 = (uint16_t)(r_t_59 + 1);
        fx_r14(r_t_61);
        { FxModelNextResult tr_ = FxModelNext(prev_end_y_5, draw_list_end_out_5, stack, r_t_61, command_code_4); return (FxModelIfCallResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
}


/*
 * @routine 01:8DF6 FxModelListStart3C
 * WHAT IT DOES: Command $3C: as command $58.
 * HOW: $0056 = $0788, 01:8E7D pushed, dispatch.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelListStart3CResult FxModelListStart3C(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t r_t_12;
    uint16_t rom_at_2;
    uint16_t return_at;
    uint16_t next_rom_at;
    uint16_t return_at_2;
    uint16_t command_code;
    uint16_t next_command;
    uint16_t prev_end_y_2;
    uint16_t rom_at_out_2;
    uint16_t rom_at_out_3;
    uint16_t draw_list_end_out_2;

    fx_stw(FX_MODEL_RETURN_AT, 0x0788);
    fx_push(stack, 0x8E7D);
    next_command = fx_getb();
    next_rom_at = (uint16_t)(rom_at + 1);
    fx_r14(next_rom_at);
    { FxModelNextResult r_ = FxModelNext(draw_list_start, draw_list_end, stack, next_rom_at, next_command); up_ = r_.up; prev_end_y_2 = r_.prev_end_y; draw_list_end_out_2 = r_.draw_list_end_out; rom_at_out_2 = r_.rom_at_out; };
    if (up_ == 1) {
        up_ = 0;
    } else if (up_) {                /* it returned past us */
        return (FxModelListStart3CResult){prev_end_y_2, draw_list_end_out_2, rom_at_out_2, up_ == 1 ? 0 : up_ - 1};
    }
    return_at = fx_ldw(FX_MODEL_RETURN_AT);
    fx_stw(return_at, 0);
    fx_stw(FX_MODEL_RETURN_AT, 0x0788);
    for (;;) {
        return_at_2 = fx_ldw(FX_MODEL_RETURN_AT);
        rom_at_2 = fx_ldw(return_at_2);
        fx_r14(rom_at_2);
        fx_stw(FX_MODEL_RETURN_AT, (uint16_t)(return_at_2 + 2));
        fx_r14(rom_at_2);
        if (rom_at_2 == 0) {
            break;
        } else {
            fx_push(stack, (uint16_t)(0 - 0x7161));
            command_code = fx_getb();
            r_t_12 = (uint16_t)(rom_at_2 + 1);
            fx_r14(r_t_12);
            { FxModelNextResult r_ = FxModelNext(prev_end_y_2, draw_list_end_out_2, stack, r_t_12, command_code); up_ = r_.up; prev_end_y_2 = r_.prev_end_y; draw_list_end_out_2 = r_.draw_list_end_out; rom_at_out_3 = r_.rom_at_out; };
            if (up_ == 1) {
                up_ = 0;
            } else if (up_) {                /* it returned past us */
                return (FxModelListStart3CResult){prev_end_y_2, draw_list_end_out_2, rom_at_out_3, up_ == 1 ? 0 : up_ - 1};
            }
            fx_r14((uint16_t)(rom_at_out_3 + 1));
            continue;
        }
    }
    { FxModelEndResult tr_ = FxModelEnd(prev_end_y_2, draw_list_end_out_2, stack, rom_at_2); return (FxModelListStart3CResult){tr_.model_state_r6, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8E06 FxModelPushReturn
 * WHAT IT DOES: Command $44: keeps the address at the word offset that
 * follows on the $0056 stack, then returns as command $40.
 * HOW: Falls into 01:8E14.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelPushReturnResult FxModelPushReturn(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t next_ptr;
    uint16_t offset_lo;
    uint16_t return_at;
    uint16_t return_offset;

    offset_lo = fx_getb();
    next_ptr = (uint16_t)(rom_at + 1);
    fx_r14(next_ptr);
    return_offset = (uint16_t)((offset_lo & 0xFF) | fx_getb() << 8);
    return_at = fx_ldw(FX_MODEL_RETURN_AT);
    fx_stw(return_at, (uint16_t)(return_offset + next_ptr));
    fx_stw(FX_MODEL_RETURN_AT, (uint16_t)(return_at + 2));
    { FxModelReturn40Result tr_ = FxModelReturn40(draw_list_start, draw_list_end, stack, next_ptr); return (FxModelPushReturnResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8E14 FxModelReturn40
 * WHAT IT DOES: Command $40: return, pulling the next command address
 * from the R10 stack.
 * HOW: DEC R10 twice, R15 from (R10).
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelReturn40Result FxModelReturn40(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{

    fx_unspill(stack);
    return (FxModelReturn40Result){draw_list_start, draw_list_end, rom_at, 1};
}


/*
 * @routine 01:8E19 FxModelIfSkip
 * WHAT IT DOES: Command $28: the flag byte at $07C6 plus the next byte
 * picks, by its bit 7, whether the list that follows runs (R14 pushed
 * on the R10 stack) or is passed by its length byte.
 * HOW: R14 pushed on the R10 stack around each list.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelIfSkipResult FxModelIfSkip(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t r0_16;
    uint16_t r0_19;
    uint16_t r_t_20;
    uint16_t r_t_21;
    uint16_t r_t_23;
    uint16_t body_ptr;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t skip_len;
    uint16_t flag_word;
    uint16_t return_at;
    uint16_t flag_index;
    uint16_t jump_offset;
    uint16_t command_code;
    uint16_t jump_lo_byte;
    uint16_t prev_end_y_2;
    uint16_t rom_at_out_2;
    uint16_t rom_at_out_3;
    uint16_t skip_len_ptr;
    uint16_t skip_len_byte;
    uint16_t ptr_after_flag;
    uint16_t ptr_after_jump;
    uint16_t return_ptr_next;
    uint16_t skip_target_ptr;
    uint16_t jump_command_ptr;
    uint16_t ptr_after_resume;
    uint16_t skip_command_ptr;
    uint16_t jump_command_byte;
    uint16_t ptr_after_jump_lo;
    uint16_t skip_command_byte;
    uint16_t draw_list_end_out_2;
    uint16_t empty_skip_command_ptr;
    uint16_t empty_skip_command_byte;

    flag_index = fx_getb();
    ptr_after_flag = (uint16_t)(rom_at + 1);
    fx_r14(ptr_after_flag);
    flag_word = fx_ldb((uint16_t)(flag_index + 0x07C6));
    if ((uint16_t)(flag_word << 8 | flag_word >> 8) & 0x8000) {
        jump_lo_byte = fx_getb();
        ptr_after_jump_lo = (uint16_t)(ptr_after_flag + 1);
        fx_r14(ptr_after_jump_lo);
        jump_offset = (uint16_t)((jump_lo_byte & 0xFF) | fx_getb() << 8);
        ptr_after_jump = (uint16_t)(ptr_after_jump_lo + 1);
        fx_r14(ptr_after_jump);
        fx_push(stack, ptr_after_jump);
        ptr_after_resume = (uint16_t)(ptr_after_jump + 1);
        fx_r14(ptr_after_resume);
        fx_push(stack, (uint16_t)(jump_offset + ptr_after_jump_lo));
        fx_push(stack, (uint16_t)(0 - 0x71A3));
        jump_command_byte = fx_getb();
        jump_command_ptr = (uint16_t)(ptr_after_resume + 1);
        fx_r14(jump_command_ptr);
        { FxModelNextResult r_ = FxModelNext(draw_list_start, draw_list_end, stack, jump_command_ptr, jump_command_byte); up_ = r_.up; prev_end_y_2 = r_.prev_end_y; draw_list_end_out_2 = r_.draw_list_end_out; rom_at_out_2 = r_.rom_at_out; };
        if (up_ == 1) {
            up_ = 0;
        } else if (up_) {                /* it returned past us */
            return (FxModelIfSkipResult){prev_end_y_2, draw_list_end_out_2, rom_at_out_2, up_ == 3 ? 0 : up_ - 3};
        }
        fx_r14((uint16_t)(rom_at_out_2 + 1));
        r0_16 = fx_pull(stack);
        return_at = fx_ldw(FX_MODEL_RETURN_AT);
        fx_stw(return_at, r0_16);
        fx_stw(FX_MODEL_RETURN_AT, (uint16_t)(return_at + 2));
        rom_at_2 = fx_pull(stack);
        fx_r14(rom_at_2);
        r0_19 = fx_getb();
        r_t_20 = (uint16_t)(r0_19 & 0xFF);
        if (r_t_20 == 0) {
            fx_unspill(stack);
            return (FxModelIfSkipResult){prev_end_y_2, draw_list_end_out_2, rom_at_2, 1};
        } else {
            r_t_21 = (uint16_t)(r_t_20 + rom_at_2);
            fx_r14(r_t_21);
            command_code = fx_getb();
            r_t_23 = (uint16_t)(r_t_21 + 1);
            fx_r14(r_t_23);
            { FxModelNextResult tr_ = FxModelNext(prev_end_y_2, draw_list_end_out_2, stack, r_t_23, command_code); return (FxModelIfSkipResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
        }
    } else {
        skip_len_ptr = (uint16_t)(ptr_after_flag + 1);
        fx_r14(skip_len_ptr);
        body_ptr = (uint16_t)(skip_len_ptr + 1);
        fx_r14(body_ptr);
        fx_push(stack, body_ptr);
        skip_len_byte = fx_getb();
        skip_len = (uint16_t)(skip_len_byte & 0xFF);
        if (skip_len == 0) {
        } else {
            skip_target_ptr = (uint16_t)(skip_len + body_ptr);
            fx_r14(skip_target_ptr);
            fx_push(stack, (uint16_t)(0 - 0x71C4));
            skip_command_byte = fx_getb();
            skip_command_ptr = (uint16_t)(skip_target_ptr + 1);
            fx_r14(skip_command_ptr);
            { FxModelNextResult r_ = FxModelNext(draw_list_start, draw_list_end, stack, skip_command_ptr, skip_command_byte); up_ = r_.up; draw_list_start = r_.prev_end_y; draw_list_end = r_.draw_list_end_out; rom_at_out_3 = r_.rom_at_out; };
            if (up_ == 1) {
                up_ = 0;
            } else if (up_) {                /* it returned past us */
                return (FxModelIfSkipResult){draw_list_start, draw_list_end, rom_at_out_3, up_ == 2 ? 0 : up_ - 2};
            }
            fx_r14((uint16_t)(rom_at_out_3 + 1));
        }
        rom_at_3 = fx_pull(stack);
        fx_r14(rom_at_3);
        return_ptr_next = (uint16_t)(rom_at_3 + 1);
        fx_r14(return_ptr_next);
        empty_skip_command_byte = fx_getb();
        empty_skip_command_ptr = (uint16_t)(return_ptr_next + 1);
        fx_r14(empty_skip_command_ptr);
        { FxModelNextResult tr_ = FxModelNext(draw_list_start, draw_list_end, stack, empty_skip_command_ptr, empty_skip_command_byte); return (FxModelIfSkipResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
}


/*
 * @routine 01:8EA7 FxModelDepthSort
 * WHAT IT DOES: Command $10: n sub-lists (n the next byte) run back to
 * front: the depth words of the n points the following bytes pick
 * ($0344 + 6index) copied to $07B0, then n times the deepest left
 * marked $8000 and its list (a word offset in the table after the
 * bytes) run, coming back to 01:8EF9 (not reached in the traces so
 * far).
 * HOW: $0148 = n, $014E counts down, $0150 = the offset table; the end
 * command when done.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelDepthSortResult FxModelDepthSort(uint16_t entry_r6_word, uint16_t *stack, uint16_t rom_at)
{
    int up_ = 0;                       /* levels a callee returned past us */
    uint16_t count;
    uint16_t r_t_19;
    uint16_t r_t_21;
    uint16_t rom_at_2;
    uint16_t model_148;
    uint16_t model_14e;
    uint16_t model_150;
    uint16_t slot_depth;
    uint16_t list_offset;
    uint16_t next_rom_at;
    uint16_t point_depth;
    uint16_t next_command;
    uint16_t rom_at_out_2;
    uint16_t deepest_depth;
    uint16_t scan_slot_ptr;
    uint16_t depth_slot_ptr;
    uint16_t list_offset_lo;
    uint16_t offset_entry_ptr;
    uint16_t point_index_byte;
    uint16_t draw_list_end_out;

    count = fx_getb();
    rom_at_2 = (uint16_t)(rom_at + 1);
    fx_r14(rom_at_2);
    fx_stw(FX_MODEL_150, (uint16_t)((uint16_t)(count + rom_at_2) - 0x07B0));
    fx_stw(FX_MODEL_14E, count);
    fx_stw(FX_MODEL_148, count);
    depth_slot_ptr = 0x07B0;
    for (;;) {
        point_index_byte = fx_getb();
        rom_at_2 = (uint16_t)(rom_at_2 + 1);
        fx_r14(rom_at_2);
        point_depth = fx_ldw((uint16_t)((uint16_t)((uint8_t)point_index_byte * (uint8_t)6) + 0x0344));
        fx_stw(depth_slot_ptr, point_depth);
        count = (uint16_t)(count - 1);
        depth_slot_ptr = (uint16_t)(depth_slot_ptr + 2);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    for (;;) {
        scan_slot_ptr = 0x07B0;
        deepest_depth = 0x8000;
        model_148 = fx_ldw(FX_MODEL_148);
        for (;;) {
            slot_depth = fx_ldw(scan_slot_ptr);
            if (((((uint16_t)(slot_depth - deepest_depth) & 0x8000) != 0) != (((slot_depth ^ deepest_depth) & (slot_depth ^ (uint16_t)(slot_depth - deepest_depth - 0)) & 0x8000) != 0))) {
            } else {
                deepest_depth = slot_depth;
                entry_r6_word = scan_slot_ptr;
            }
            model_148 = (uint16_t)(model_148 - 1);
            scan_slot_ptr = (uint16_t)(scan_slot_ptr + 2);
            if (model_148 != 0) {
                continue;
            } else {
                break;
            }
        }
        fx_stw(entry_r6_word, 0x8000);
        model_150 = fx_ldw(FX_MODEL_150);
        offset_entry_ptr = (uint16_t)(model_150 + entry_r6_word);
        fx_r14(offset_entry_ptr);
        list_offset_lo = fx_getb();
        fx_r14((uint16_t)(offset_entry_ptr + 1));
        list_offset = (uint16_t)((list_offset_lo & 0xFF) | fx_getb() << 8);
        fx_r14(list_offset);
        fx_push(stack, (uint16_t)(0 - 0x7107));
        next_command = fx_getb();
        next_rom_at = (uint16_t)(list_offset + 1);
        fx_r14(next_rom_at);
        { FxModelNextResult r_ = FxModelNext(entry_r6_word, deepest_depth, stack, next_rom_at, next_command); up_ = r_.up; entry_r6_word = r_.prev_end_y; draw_list_end_out = r_.draw_list_end_out; rom_at_out_2 = r_.rom_at_out; };
        if (up_ == 1) {
            up_ = 0;
        } else if (up_) {                /* it returned past us */
            return (FxModelDepthSortResult){entry_r6_word, draw_list_end_out, rom_at_out_2, up_ == 1 ? 0 : up_ - 1};
        }
        r_t_19 = (uint16_t)(rom_at_out_2 + 1);
        fx_r14(r_t_19);
        model_14e = fx_ldw(FX_MODEL_14E);
        r_t_21 = (uint16_t)(model_14e - 1);
        fx_sbk(r_t_21);
        if (r_t_21 != 0) {
            continue;
        } else {
            break;
        }
    }
    { FxModelEndResult tr_ = FxModelEnd(entry_r6_word, draw_list_end_out, stack, r_t_19); return (FxModelDepthSortResult){tr_.model_state_r6, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8F05 FxModelIfSprite
 * WHAT IT DOES: Command $54: the flag byte at $07C6 plus the next byte;
 * with bit 7 set, command $50 follows (01:8F18), else its bytes are
 * skipped (not reached in the traces so far).
 * HOW: SWAP, HIB, BMI.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelIfSpriteResult FxModelIfSprite(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t kind_flags;
    uint16_t skip_ptr_1;
    uint16_t skip_ptr_2;
    uint16_t skip_ptr_3;
    uint16_t object_kind;
    uint16_t model_ptr_next;
    uint16_t next_command_ptr;
    uint16_t next_command_byte;

    object_kind = fx_getb();
    model_ptr_next = (uint16_t)(rom_at + 1);
    fx_r14(model_ptr_next);
    kind_flags = fx_ldb((uint16_t)(object_kind + 0x07C6));
    if (((uint16_t)((uint16_t)(kind_flags << 8 | kind_flags >> 8) >> 8) >> 7 & 1)) {
        { FxModelSpriteResult tr_ = FxModelSprite(draw_list_start, draw_list_end, stack, model_ptr_next); return (FxModelIfSpriteResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
    skip_ptr_1 = (uint16_t)(model_ptr_next + 1);
    fx_r14(skip_ptr_1);
    skip_ptr_2 = (uint16_t)(skip_ptr_1 + 1);
    fx_r14(skip_ptr_2);
    skip_ptr_3 = (uint16_t)(skip_ptr_2 + 1);
    fx_r14(skip_ptr_3);
    next_command_byte = fx_getb();
    next_command_ptr = (uint16_t)(skip_ptr_3 + 1);
    fx_r14(next_command_ptr);
    { FxModelNextResult tr_ = FxModelNext(draw_list_start, draw_list_end, stack, next_command_ptr, next_command_byte); return (FxModelIfSpriteResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:8F18 FxModelSprite
 * WHAT IT DOES: Command $50: a shape at the projected point ($04E4 + 6
 * * the next byte) whose depth, against $002A, sets its size; its
 * colour from FxModelSpriteColour (01:9250) and a LINK to 01:AB11 (not
 * reached in the traces so far).
 * HOW: $00BA-$00D2 set for it; ROMB and R14 back from $001C and $001A.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelSpriteResult FxModelSprite(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t ba;
    uint16_t bc;
    uint16_t ce;
    uint16_t next_ptr;
    uint16_t rom_at_2;
    uint16_t tex_mask;
    uint16_t depth_bias;
    uint16_t model_bank;
    uint16_t skip_ptr_1;
    uint16_t skip_ptr_2;
    uint16_t rom_at_out_2;
    uint16_t sprite_depth;
    uint16_t sprite_index;
    uint16_t texture_size;
    uint16_t sprite_offset;
    uint16_t sprite_rec_ptr;
    uint16_t after_flags_ptr;
    uint16_t after_colour_ptr;
    uint16_t next_rom_at_drawn;
    uint16_t sprite_depth_base;
    uint16_t next_command_drawn;
    uint16_t next_rom_at_skipped;
    uint16_t next_command_skipped;
    uint16_t draw_list_start_out_2;

    sprite_index = fx_getb();
    next_ptr = (uint16_t)(rom_at + 1);
    fx_r14(next_ptr);
    sprite_offset = (uint16_t)((uint8_t)sprite_index * (uint8_t)6);
    sprite_rec_ptr = (uint16_t)(sprite_offset + 0x04E4);
    ba = fx_ldw(sprite_rec_ptr);
    fx_stw(FX_MODEL_BA, ba);
    bc = fx_ldw((uint16_t)(sprite_rec_ptr + 2));
    fx_stw(FX_MODEL_BC, bc);
    sprite_depth_base = fx_ldw((uint16_t)(sprite_offset + 0x0344));
    depth_bias = fx_ldw(0x002A);
    sprite_depth = (uint16_t)(sprite_depth_base + depth_bias);
    fx_stw(FX_MODEL_BE, sprite_depth);
    if ((uint16_t)(sprite_depth - 0x0020) & 0x8000) {
        skip_ptr_1 = (uint16_t)(next_ptr + 1);
        fx_r14(skip_ptr_1);
        skip_ptr_2 = (uint16_t)(skip_ptr_1 + 1);
        fx_r14(skip_ptr_2);
        next_command_skipped = fx_getb();
        next_rom_at_skipped = (uint16_t)(skip_ptr_2 + 1);
        fx_r14(next_rom_at_skipped);
        { FxModelNextResult tr_ = FxModelNext(draw_list_start, draw_list_end, stack, next_rom_at_skipped, next_command_skipped); return (FxModelSpriteResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    } else {
        { FxModelSpriteColourResult r_ = FxModelSpriteColour(draw_list_start, next_ptr); draw_list_start_out_2 = r_.draw_list_start_out; rom_at_out_2 = r_.rom_at_out; };
        after_colour_ptr = (uint16_t)(rom_at_out_2 + 1);
        fx_r14(after_colour_ptr);
        fx_getb();
        after_flags_ptr = (uint16_t)(after_colour_ptr + 1);
        fx_r14(after_flags_ptr);
        tex_mask = fx_ldw(FX_TEX_MASK);
        texture_size = (uint16_t)((uint16_t)(tex_mask & 0xFF) + 1);
        fx_stw(FX_MODEL_C8, texture_size);
        ce = 0x0100;
        if (!((uint16_t)((uint16_t)(texture_size + texture_size) << 8 | (uint16_t)(texture_size + texture_size) >> 8) & 0x8000)) {
        } else {
            ce = (uint16_t)(ce >> 1);
        }
        fx_stw(FX_MODEL_CE, ce);
        fx_stw(0x001A, after_flags_ptr);
        fx_stw(FX_MODEL_D2, 0);
        FxReturnR11();
        model_bank = fx_ldw(0x001C);
        fx_romb(model_bank);
        rom_at_2 = fx_ldw(0x001A);
        fx_r14(rom_at_2);
        next_command_drawn = fx_getb();
        next_rom_at_drawn = (uint16_t)(rom_at_2 + 1);
        fx_r14(next_rom_at_drawn);
        { FxModelNextResult tr_ = FxModelNext(draw_list_start_out_2, draw_list_end, stack, next_rom_at_drawn, next_command_drawn); return (FxModelSpriteResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
}


/*
 * @routine 01:8F7D FxModelMaterial
 * WHAT IT DOES: Sets the face colour (and for textured faces the
 * texture's bank, address and mask) from the face's material word in
 * ROM: a shade from the light ($0106-$010A) for shaded materials.
 * HOW: R2 keeps R14; ROMB $03; the material table at $004A, looked up
 * through $0046 for animated ones; the shade table at $004C; ROMB and
 * R14 back. Returns with JMP R11.
 * Entry: R14 at the face's material word; R11 = return address.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
uint16_t FxModelMaterial(uint16_t rom_at)
{
    uint16_t flags;
    uint16_t mask_ptr;
    uint16_t rom_at_2;
    uint16_t shade_at;
    uint16_t tex_bank;
    uint16_t tex_base;
    uint16_t tex_mask;
    uint16_t entry_slot;
    uint16_t mask_index;
    uint16_t jump_offset;
    uint16_t light_level;
    uint16_t shade_class;
    uint16_t shade_index;
    uint16_t shade_table;
    uint16_t mask_ptr_low;
    uint16_t normal_x_ptr;
    uint16_t normal_y_ptr;
    uint16_t repeat_count;
    uint16_t entry_slot_x2;
    uint16_t entry_slot_x4;
    uint16_t entry_slot_x8;
    uint16_t face_normal_x;
    uint16_t face_normal_y;
    uint16_t face_normal_z;
    uint16_t light_local_x;
    uint16_t light_local_y;
    uint16_t light_local_z;
    uint16_t mask_low_byte;
    uint16_t masked_repeat;
    uint16_t material_byte;
    uint16_t material_mask;
    uint16_t reject_status;
    uint16_t entry_low_byte;
    uint16_t mask_table_ptr;
    uint16_t material_entry;
    uint16_t material_index;
    uint16_t saved_rom_bank;
    uint16_t shade_row_base;
    int is_shaded_color;
    uint16_t shade_entry_low;
    uint16_t shade_entry_ptr;
    uint16_t texture_rec_ptr;
    uint16_t untextured_flag;
    uint16_t shade_class_next;
    uint16_t texture_base_low;
    uint16_t restored_rom_bank;
    uint16_t material_table_base;
    uint16_t untextured_flag_dec;
    uint16_t texture_rec_next_ptr;

    flags = fx_ldw(FX_MODEL_FLAGS);
    if ((flags & 0x2000) == 0) {
        material_index = fx_getb();
        fx_romb(3);
        material_table_base = fx_ldw(0x004A);
        rom_at_2 = (uint16_t)((uint16_t)(material_index + material_index) + material_table_base);
        fx_r14(rom_at_2);
        for (;;) {
            entry_low_byte = fx_getb();
            fx_r14((uint16_t)(rom_at_2 + 1));
            material_entry = (uint16_t)((entry_low_byte & 0xFF) | fx_getb() << 8);
            if (!(material_entry & 0x8000)) {
                fx_r14((uint16_t)((uint16_t)(material_entry & 0xFF) - 0x604F));
                material_byte = fx_getb();
                fx_stw(0x0058, (uint16_t)(material_byte + 0x0800));
                is_shaded_color = 1;
                untextured_flag = 0;
                if (!((uint16_t)(material_entry + material_entry) & 0x8000)) {
                } else {
                    untextured_flag_dec = (uint16_t)(untextured_flag - 1);
                    is_shaded_color = (untextured_flag_dec == 0);
                    untextured_flag = untextured_flag_dec;
                }
                fx_stw(0x0042, untextured_flag);
                if (is_shaded_color) {
                    shade_class = ((uint16_t)((uint16_t)(material_entry >> 8) + 1) & 0x003F);
                    if (shade_class == 0) {
                        reject_status = (uint16_t)(material_entry & 0xFF);
                        fx_color((uint8_t)reject_status);
                        break;
                    } else {
                        shade_class_next = ((uint16_t)(shade_class + 1) & 0x003F);
                        if (shade_class_next != 0) {
                            shade_index = ((uint16_t)(shade_class_next - 2) & 0x003F);
                            shade_table = fx_ldw(FX_SHADE_TABLE);
                            shade_entry_ptr = (uint16_t)((uint16_t)(shade_table + shade_index) + shade_index);
                            fx_r14(shade_entry_ptr);
                            shade_entry_low = fx_getb();
                            fx_r14((uint16_t)(shade_entry_ptr + 1));
                            shade_row_base = (uint16_t)((shade_entry_low & 0xFF) | fx_getb() << 8);
                            saved_rom_bank = fx_ldw(0x001C);
                            fx_romb(saved_rom_bank);
                            fx_r14(rom_at);
                            normal_x_ptr = (uint16_t)(rom_at + 1);
                            fx_r14(normal_x_ptr);
                            face_normal_x = fx_getb();
                            normal_y_ptr = (uint16_t)(normal_x_ptr + 1);
                            fx_r14(normal_y_ptr);
                            light_local_x = fx_ldw(FX_LIGHT_LOCAL_X);
                            face_normal_y = fx_getb();
                            fx_r14((uint16_t)(normal_y_ptr + 1));
                            light_local_y = fx_ldw(FX_LIGHT_LOCAL_Y);
                            face_normal_z = fx_getb();
                            light_local_z = fx_ldw(FX_LIGHT_LOCAL_Z);
                            light_level = (uint16_t)(int16_t)(int8_t)(uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)(uint16_t)((uint16_t)((uint16_t)((int8_t)face_normal_z * (int8_t)light_local_z) + (uint16_t)((int8_t)face_normal_y * (int8_t)light_local_y)) + (uint16_t)((int8_t)face_normal_x * (int8_t)light_local_x)) >> 1) >> 1) >> 8);
                            if (((((uint16_t)(light_level - 6) & 0x8000) != 0) == (((light_level ^ 6) & (light_level ^ (uint16_t)(light_level - 6 - 0)) & 0x8000) != 0))) {
                                if (((((uint16_t)(light_level - 0x0010) & 0x8000) != 0) != (((light_level ^ 0x0010) & (light_level ^ (uint16_t)(light_level - 0x0010 - 0)) & 0x8000) != 0))) {
                                } else {
                                    light_level = 0x000F;
                                }
                            } else {
                                light_level = 6;
                            }
                            fx_romb(3);
                            fx_r14((uint16_t)((uint16_t)(shade_row_base + light_level) - 6));
                            reject_status = fx_getb();
                            fx_color((uint8_t)reject_status);
                            break;
                        } else {
                            shade_at = fx_ldw(FX_SHADE_AT);
                            fx_r14((uint16_t)((uint16_t)(material_entry & 0xFF) + shade_at));
                            reject_status = fx_getb();
                            fx_color((uint8_t)reject_status);
                            break;
                        }
                    }
                } else {
                    if ((0x2000 & material_entry) == 0) {
                    } else {
                        fx_stw(0x0058, (uint16_t)(material_byte + 0x0C00));
                    }
                    texture_rec_ptr = (uint16_t)((uint16_t)((uint8_t)(uint16_t)(material_entry & 0xFF) * (uint8_t)3) - 0x62A7);
                    fx_r14(texture_rec_ptr);
                    texture_base_low = fx_getb();
                    texture_rec_next_ptr = (uint16_t)(texture_rec_ptr + 1);
                    fx_r14(texture_rec_next_ptr);
                    tex_base = (uint16_t)((texture_base_low & 0xFF) | fx_getb() << 8);
                    fx_r14((uint16_t)(texture_rec_next_ptr + 1));
                    fx_stw(FX_TEX_BASE, tex_base);
                    tex_bank = fx_getb();
                    fx_stw(FX_TEX_BANK, tex_bank);
                    mask_index = ((uint16_t)(material_entry >> 8) & 0x001F);
                    mask_table_ptr = (uint16_t)((uint16_t)(mask_index + mask_index) - 0x5F87);
                    fx_r14(mask_table_ptr);
                    mask_ptr_low = fx_getb();
                    fx_r14((uint16_t)(mask_table_ptr + 1));
                    mask_ptr = (uint16_t)((mask_ptr_low & 0xFF) | fx_getb() << 8);
                    fx_r14(mask_ptr);
                    mask_low_byte = fx_getb();
                    fx_r14((uint16_t)(mask_ptr + 1));
                    tex_mask = (uint16_t)((mask_low_byte & 0xFF) | fx_getb() << 8);
                    fx_stw(FX_TEX_MASK, tex_mask);
                    fx_stw(0x0088, (uint16_t)(mask_ptr + 2));
                    reject_status = 0xFFFF;
                    break;
                }
                restored_rom_bank = fx_ldw(0x001C);
                fx_romb(restored_rom_bank);
                fx_r14(rom_at);
                return reject_status;
            } else {
                if (!((uint16_t)(material_entry + material_entry) & 0x8000)) {
                    jump_offset = (0x1FFF & material_entry);
                    fx_r14(jump_offset);
                    material_mask = fx_ldw(FX_MATERIAL_MASK);
                    repeat_count = fx_getb();
                    masked_repeat = (material_mask & (uint16_t)(repeat_count - 1));
                    rom_at_2 = (uint16_t)((uint16_t)((uint16_t)(masked_repeat + masked_repeat) + jump_offset) + 1);
                    fx_r14(rom_at_2);
                    continue;
                } else {
                    fx_stw(0x0042, 0xFFFE);
                    entry_slot = ((uint16_t)(material_entry >> 8) & 0x000F);
                    entry_slot_x2 = (uint16_t)(entry_slot + entry_slot);
                    entry_slot_x4 = (uint16_t)(entry_slot_x2 + entry_slot_x2);
                    entry_slot_x8 = (uint16_t)(entry_slot_x4 + entry_slot_x4);
                    fx_stw(0x0058, (uint16_t)(((uint16_t)((uint16_t)(entry_slot_x8 + entry_slot_x8) << 8 | (uint16_t)(entry_slot_x8 + entry_slot_x8) >> 8) | 8) << 8 | ((uint16_t)((uint16_t)(entry_slot_x8 + entry_slot_x8) << 8 | (uint16_t)(entry_slot_x8 + entry_slot_x8) >> 8) | 8) >> 8));
                    reject_status = 0xFFFE;
                    break;
                }
            }
        }
    } else {
        fx_stw(0x0042, 0);
        fx_cmode(2);
        reject_status = 9;
    }
    restored_rom_bank = fx_ldw(0x001C);
    fx_romb(restored_rom_bank);
    fx_r14(rom_at);
    return reject_status;
}


/*
 * @routine 01:90CD FxModelFaces
 * WHAT IT DOES: Command $14: the faces that follow, each with its
 * material (FxModelMaterial), its points gathered and clipped
 * (FxPolygonGather or FxPolygonDraw, FxPolygonClipScreen) and queued in
 * the draw list, which is drawn (FxDrawList) when nearly full.
 * HOW: CACHE; a face count byte less than 0 ends it.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelFacesResult FxModelFaces(uint16_t divisor_made, uint16_t list_write_ptr, uint16_t *stack, uint16_t rom_at)
{
    int run_l90F0 = 0;
    int run_l920F = 0;
    uint16_t count;
    uint16_t point_x;
    uint16_t point_y;
    uint16_t face_cmd;
    uint16_t list_end;
    uint16_t outcodes;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t tex_bank;
    uint16_t tex_base;
    uint16_t tex_mask;
    uint16_t uv_u_byte;
    uint16_t uv_v_byte;
    uint16_t list_end_2;
    uint16_t next_rom_at;
    uint16_t point_count;
    uint16_t point_index;
    uint16_t next_command;
    uint16_t uv_rom_ptr_v;
    uint16_t clipped_count;
    uint16_t face_cmd_byte;
    uint16_t face_flags_uv;
    uint16_t material_word;
    uint16_t point_count_2;
    uint16_t point_count_3;
    uint16_t point_outcode;
    uint16_t point_rec_ptr;
    uint16_t face_list_base;
    uint16_t points_rom_ptr;
    uint16_t rom_bank_saved;
    uint16_t saved_rom_bank;
    uint16_t tex_extra_word;
    uint16_t face_flags_clip;
    uint16_t face_flags_word;
    uint16_t face_kind_flags;
    uint16_t face_kind_index;
    uint16_t material_result;
    uint16_t point_rec_ptr_y;
    uint16_t uv_rom_ptr_next;
    uint16_t clipped_count_x2;
    uint16_t clipped_count_x4;
    uint16_t list_ptr_after_u;
    uint16_t list_ptr_after_x;
    uint16_t rom_ptr_after_cmd;
    uint16_t clipped_count_copy;
    uint16_t final_vertex_count;
    uint16_t material_word_copy;
    uint16_t rom_ptr_after_kind;
    uint16_t rom_ptr_after_flags;
    uint16_t clipped_vertex_count;
    uint16_t face_flags_unclipped;
    uint16_t list_ptr_after_extra;
    uint16_t clipped_vertex_count_2;
    uint16_t list_ptr_after_tex_bank;

    for (;;) {
        face_cmd_byte = fx_getb();
        face_cmd = (uint16_t)(face_cmd_byte & 0xFF);
        rom_ptr_after_cmd = (uint16_t)(rom_at + 1);
        fx_r14(rom_ptr_after_cmd);
        if ((face_cmd >> 7 & 1)) {
            break;
        } else {
            rom_ptr_after_kind = (uint16_t)(rom_ptr_after_cmd + 1);
            fx_r14(rom_ptr_after_kind);
            face_kind_index = fx_getb();
            if (face_cmd == 2) {
                rom_at_2 = (uint16_t)(rom_ptr_after_kind + 1);
                fx_r14(rom_at_2);
                run_l90F0 = 1;
            } else {
                face_kind_flags = fx_ldb((uint16_t)(face_kind_index + 0x07C6));
                rom_ptr_after_flags = (uint16_t)(rom_ptr_after_kind + 1);
                rom_at_2 = rom_ptr_after_flags;
                fx_r14(rom_at_2);
                if (((uint16_t)(face_kind_flags & 0xFF) >> 7 & 1)) {
                    run_l90F0 = 1;
                } else {
                    if (rom_ptr_after_flags == 0) {
                        run_l90F0 = 1;
                    } else {
                        rom_at = (uint16_t)((uint16_t)(rom_at_2 + face_cmd) + 4);
                        fx_r14(rom_at);
                        continue;
                    }
                }
            }
            fx_stw(FX_POINT_COUNT, face_cmd);
            list_end = fx_ldw(FX_LIST_END);
            if (!((uint16_t)((uint16_t)(0 - list_end) + 0x09AA) & 0x8000)) {
            } else {
                fx_push(stack, rom_at_2);
                FxDrawList(list_end, list_write_ptr, stack, rom_at_2);
                rom_at_2 = fx_pull(stack);
                fx_r14(rom_at_2);
                fx_ldw(FX_LIST_END);
            }
            material_result = FxModelMaterial(rom_at_2);
            list_end_2 = fx_ldw(FX_LIST_END);
            fx_face_begin(list_end_2, (material_result & 0x8000) != 0);
            rom_at = (uint16_t)(rom_at_2 + 4);
            fx_r14(rom_at);
            point_count = fx_ldw(FX_POINT_COUNT);
            tex_mask = ((uint16_t)((uint16_t)(material_result & 0xFF) << 8 | (uint16_t)(material_result & 0xFF) >> 8) | point_count);
            material_word = material_result;
            if (!(material_result & 0x8000)) {
            } else {
                tex_bank = fx_ldw(FX_TEX_BANK);
                fx_stw(list_end_2, ((uint16_t)((uint16_t)(tex_bank & 0xFF) << 8 | (uint16_t)(tex_bank & 0xFF) >> 8) | (uint16_t)((uint16_t)~point_count & 0xFF)));
                list_ptr_after_tex_bank = (uint16_t)(list_end_2 + 2);
                tex_extra_word = fx_ldw(0x0058);
                fx_stw(list_ptr_after_tex_bank, tex_extra_word);
                list_ptr_after_extra = (uint16_t)(list_ptr_after_tex_bank + 2);
                tex_base = fx_ldw(FX_TEX_BASE);
                fx_stw(list_ptr_after_extra, tex_base);
                list_end_2 = (uint16_t)(list_ptr_after_extra + 2);
                tex_mask = fx_ldw(FX_TEX_MASK);
            }
            fx_stw(list_end_2, tex_mask);
            divisor_made = (uint16_t)(list_end_2 + 2);
            list_write_ptr = divisor_made;
            outcodes = 0;
            for (;;) {
                point_index = fx_getb();
                rom_at = (uint16_t)(rom_at + 1);
                fx_r14(rom_at);
                point_rec_ptr = (uint16_t)((uint16_t)((uint8_t)point_index * (uint8_t)6) + 0x04E4);
                point_x = fx_ldw(point_rec_ptr);
                fx_stw(divisor_made, point_x);
                list_ptr_after_x = (uint16_t)(divisor_made + 2);
                point_rec_ptr_y = (uint16_t)(point_rec_ptr + 2);
                point_y = fx_ldw(point_rec_ptr_y);
                fx_stw(list_ptr_after_x, point_y);
                divisor_made = (uint16_t)(list_ptr_after_x + 2);
                point_outcode = fx_ldw((uint16_t)(point_rec_ptr_y + 2));
                outcodes = (point_outcode | outcodes);
                material_word_copy = material_word;
                material_word = material_word_copy;
                if (!(material_word_copy & 0x8000)) {
                    fx_face_vertex((uint8_t)point_index, 0, 0);
                } else {
                    fx_stw(0x001A, rom_at);
                    fx_romb(3);
                    rom_at_3 = fx_ldw(0x0088);
                    fx_r14(rom_at_3);
                    uv_u_byte = fx_getb();
                    uv_rom_ptr_v = (uint16_t)(rom_at_3 + 1);
                    fx_r14(uv_rom_ptr_v);
                    fx_stw(divisor_made, (uint16_t)(uv_u_byte << 8 | uv_u_byte >> 8));
                    list_ptr_after_u = (uint16_t)(divisor_made + 2);
                    uv_v_byte = fx_getb();
                    uv_rom_ptr_next = (uint16_t)(uv_rom_ptr_v + 1);
                    fx_r14(uv_rom_ptr_next);
                    fx_stw(list_ptr_after_u, (uint16_t)(uv_v_byte << 8 | uv_v_byte >> 8));
                    fx_face_vertex((uint8_t)point_index, (uint8_t)uv_u_byte, (uint8_t)uv_v_byte);
                    divisor_made = (uint16_t)(list_ptr_after_u + 2);
                    rom_bank_saved = fx_ldw(0x001C);
                    fx_stw(0x0088, uv_rom_ptr_next);
                    fx_romb(rom_bank_saved);
                    rom_at = fx_ldw(0x001A);
                    fx_r14(rom_at);
                }
                point_count = (uint16_t)(point_count - 1);
                if (point_count != 0) {
                    continue;
                } else {
                    break;
                }
            }
            if ((uint16_t)(outcodes & 0xFF) != 0) {
                if ((0x1F00 & outcodes) == 0x1F00) {
                    fx_stw(FX_OUTCODES, outcodes);
                    fx_stw(0x001A, rom_at);
                    fx_stw(0x005A, list_write_ptr);
                    if ((uint16_t)(outcodes & 0xFF) == 0) {
                        if ((0x0010 & outcodes) == 0) {
                            face_flags_unclipped = fx_ldw(0x0042);
                            if ((uint16_t)(face_flags_unclipped >> 8) != 0) {
                                { FxModelFacesOnScreenUVResult tr_ = FxModelFacesOnScreenUV(divisor_made, list_write_ptr, stack); return (FxModelFacesResult){tr_.divisor_made_out, tr_.draw_list_end_out, tr_.rom_at, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
                            }
                            point_count_2 = fx_ldw(FX_POINT_COUNT);
                            { FxPolygonClipScreenResult r_ = FxPolygonClipScreen(point_count_2, divisor_made, list_write_ptr, stack); clipped_vertex_count = r_.clipped_vertex_count; divisor_made = r_.prev_end_y_out; list_write_ptr = r_.divide_remainder_out; };
                            clipped_count = clipped_vertex_count;
                            if (clipped_vertex_count == 0) {
                            } else {
                                run_l920F = 1;
                            }
                        } else {
                            point_count_3 = fx_ldw(FX_POINT_COUNT);
                            points_rom_ptr = (uint16_t)(rom_at - point_count_3);
                            fx_r14(points_rom_ptr);
                            { FxPolygonGatherResult r_ = FxPolygonGather(point_count_3, stack, points_rom_ptr); final_vertex_count = r_.final_vertex_count; divisor_made = r_.kept_z_out; list_write_ptr = r_.final_src_ptr; };
                            point_count_2 = final_vertex_count;
                            if (final_vertex_count == 0) {
                            } else {
                                face_flags_uv = fx_ldw(0x0042);
                                if ((uint16_t)(face_flags_uv >> 8) == 0) {
                                    { FxPolygonClipScreenResult r_ = FxPolygonClipScreen(point_count_2, divisor_made, list_write_ptr, stack); clipped_vertex_count = r_.clipped_vertex_count; divisor_made = r_.prev_end_y_out; list_write_ptr = r_.divide_remainder_out; };
                                    clipped_count = clipped_vertex_count;
                                    if (clipped_vertex_count == 0) {
                                    } else {
                                        run_l920F = 1;
                                    }
                                } else {
                                    { FxPolygonClipScreenUVResult r_ = FxPolygonClipScreenUV(point_count_2, divisor_made, list_write_ptr, stack); clipped_vertex_count_2 = r_.clipped_vertex_count; divisor_made = r_.divisor_made_out; list_write_ptr = r_.quotient_sign; };
                                    clipped_count = clipped_vertex_count_2;
                                    if (clipped_vertex_count_2 == 0) {
                                    } else {
                                        run_l920F = 1;
                                    }
                                }
                            }
                        }
                    } else {
                        face_flags_clip = fx_ldw(0x0042);
                        if ((uint16_t)(face_flags_clip >> 8) == 0) {
                            if ((0x0010 & outcodes) == 0) {
                                face_flags_unclipped = fx_ldw(0x0042);
                                if ((uint16_t)(face_flags_unclipped >> 8) != 0) {
                                    { FxModelFacesOnScreenUVResult tr_ = FxModelFacesOnScreenUV(divisor_made, list_write_ptr, stack); return (FxModelFacesResult){tr_.divisor_made_out, tr_.draw_list_end_out, tr_.rom_at, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
                                }
                                point_count_2 = fx_ldw(FX_POINT_COUNT);
                                { FxPolygonClipScreenResult r_ = FxPolygonClipScreen(point_count_2, divisor_made, list_write_ptr, stack); clipped_vertex_count = r_.clipped_vertex_count; divisor_made = r_.prev_end_y_out; list_write_ptr = r_.divide_remainder_out; };
                                clipped_count = clipped_vertex_count;
                                if (clipped_vertex_count == 0) {
                                } else {
                                    run_l920F = 1;
                                }
                            } else {
                                point_count_3 = fx_ldw(FX_POINT_COUNT);
                                points_rom_ptr = (uint16_t)(rom_at - point_count_3);
                                fx_r14(points_rom_ptr);
                                { FxPolygonGatherResult r_ = FxPolygonGather(point_count_3, stack, points_rom_ptr); final_vertex_count = r_.final_vertex_count; divisor_made = r_.kept_z_out; list_write_ptr = r_.final_src_ptr; };
                                point_count_2 = final_vertex_count;
                                if (final_vertex_count == 0) {
                                } else {
                                    face_flags_uv = fx_ldw(0x0042);
                                    if ((uint16_t)(face_flags_uv >> 8) == 0) {
                                        { FxPolygonClipScreenResult r_ = FxPolygonClipScreen(point_count_2, divisor_made, list_write_ptr, stack); clipped_vertex_count = r_.clipped_vertex_count; divisor_made = r_.prev_end_y_out; list_write_ptr = r_.divide_remainder_out; };
                                        clipped_count = clipped_vertex_count;
                                        if (clipped_vertex_count == 0) {
                                        } else {
                                            run_l920F = 1;
                                        }
                                    } else {
                                        { FxPolygonClipScreenUVResult r_ = FxPolygonClipScreenUV(point_count_2, divisor_made, list_write_ptr, stack); clipped_vertex_count_2 = r_.clipped_vertex_count; divisor_made = r_.divisor_made_out; list_write_ptr = r_.quotient_sign; };
                                        clipped_count = clipped_vertex_count_2;
                                        if (clipped_vertex_count_2 == 0) {
                                        } else {
                                            run_l920F = 1;
                                        }
                                    }
                                }
                            }
                        } else {
                            { FxPolygonDrawResult r_ = FxPolygonDraw(outcodes, stack, rom_at); count = r_.count; divisor_made = r_.last_clip_slope; list_write_ptr = r_.out_list_end; };
                            clipped_count = count;
                            if (count != 0) {
                                run_l920F = 1;
                            } else {
                            }
                        }
                    }
                    if (run_l920F) {
                        run_l920F = 0;
                        face_flags_word = fx_ldw(0x0042);
                        face_list_base = fx_ldw(0x005A);
                        clipped_count_copy = clipped_count;
                        clipped_count_x2 = (uint16_t)(clipped_count + clipped_count);
                        if (!(face_flags_word & 0x8000)) {
                            fx_stw(FX_LIST_END, (uint16_t)((uint16_t)(clipped_count_x2 + clipped_count_x2) + face_list_base));
                            divisor_made = (uint16_t)(face_list_base - 2);
                            fx_stb(divisor_made, (uint8_t)clipped_count_copy);
                        } else {
                            clipped_count_x4 = (uint16_t)(clipped_count_x2 + clipped_count_x2);
                            fx_stw(FX_LIST_END, (uint16_t)((uint16_t)(clipped_count_x4 + clipped_count_x4) + face_list_base));
                            divisor_made = (uint16_t)(face_list_base - 8);
                            fx_stb(divisor_made, (uint8_t)(uint16_t)~clipped_count_copy);
                        }
                        saved_rom_bank = fx_ldw(0x001C);
                        fx_romb(saved_rom_bank);
                    }
                    rom_at = fx_ldw(0x001A);
                    fx_r14(rom_at);
                } else {
                }
                continue;
            } else {
                fx_stw(FX_LIST_END, divisor_made);
                continue;
            }
        }
    }
    if ((uint16_t)((uint16_t)(face_cmd + 1) & 0xFF) == 0) {
        fx_unspill(stack);
        return (FxModelFacesResult){divisor_made, list_write_ptr, rom_ptr_after_cmd, 1};
    } else {
        next_command = fx_getb();
        next_rom_at = (uint16_t)(rom_ptr_after_cmd + 1);
        fx_r14(next_rom_at);
        { FxModelNextResult tr_ = FxModelNext(divisor_made, list_write_ptr, stack, next_rom_at, next_command); return (FxModelFacesResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
}


/*
 * @routine 01:91E7 FxModelFacesGatheredUV
 * WHAT IT DOES: FxModelFaces' way to the textured clip (01:9205,
 * FxPolygonClipScreenUV) for a face gathered by FxPolygonGather whose
 * kind ($0042) has its high byte set, then the face queued and the next
 * one. The game never runs it: a face with that high byte and points off
 * the screen was drawn by FxPolygonDraw before it instead, and the
 * gather does not change the kind, so the BEQ at 01:91E4 always
 * branches. A unit start runs it so the C is judged against the
 * original.
 * HOW: IWT R15 to 01:9205 (NOP in the delay slot); then as FxModelFaces.
 * Entry: R0 = the gathered point count; R10 = stack; R14 at the face
 * list; the ROM bank the model's.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelFacesGatheredUVResult FxModelFacesGatheredUV(uint16_t gathered_point, uint16_t divisor_made, uint16_t uv_list_ptr, uint16_t *stack)
{
    uint16_t rom_at;
    uint16_t list_end_ptr;
    uint16_t quotient_sign;
    uint16_t clip_result_x2;
    uint16_t clip_result_x4;
    uint16_t saved_rom_bank;
    uint16_t face_clip_flags;
    uint16_t divisor_made_out;
    uint16_t clipped_vertex_count;

    { FxPolygonClipScreenUVResult r_ = FxPolygonClipScreenUV(gathered_point, divisor_made, uv_list_ptr, stack); clipped_vertex_count = r_.clipped_vertex_count; divisor_made_out = r_.divisor_made_out; quotient_sign = r_.quotient_sign; };
    if (clipped_vertex_count == 0) {
    } else {
        face_clip_flags = fx_ldw(0x0042);
        list_end_ptr = fx_ldw(0x005A);
        clip_result_x2 = (uint16_t)(clipped_vertex_count + clipped_vertex_count);
        if (!(face_clip_flags & 0x8000)) {
            fx_stw(FX_LIST_END, (uint16_t)((uint16_t)(clip_result_x2 + clip_result_x2) + list_end_ptr));
            divisor_made_out = (uint16_t)(list_end_ptr - 2);
            fx_stb(divisor_made_out, (uint8_t)clipped_vertex_count);
        } else {
            clip_result_x4 = (uint16_t)(clip_result_x2 + clip_result_x2);
            fx_stw(FX_LIST_END, (uint16_t)((uint16_t)(clip_result_x4 + clip_result_x4) + list_end_ptr));
            divisor_made_out = (uint16_t)(list_end_ptr - 8);
            fx_stb(divisor_made_out, (uint8_t)(uint16_t)~clipped_vertex_count);
        }
        saved_rom_bank = fx_ldw(0x001C);
        fx_romb(saved_rom_bank);
    }
    rom_at = fx_ldw(0x001A);
    fx_r14(rom_at);
    { FxModelFacesResult tr_ = FxModelFaces(divisor_made_out, quotient_sign, stack, rom_at); return (FxModelFacesGatheredUVResult){tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:9202 FxModelFacesOnScreenUV
 * WHAT IT DOES: FxModelFaces' textured clip (FxPolygonClipScreenUV) of a
 * face whose kind ($0042) has its high byte set and whose points are
 * all on the screen (outcodes 0), then the face queued and the next one.
 * No play has reached it so far. A unit start runs it so the C is judged
 * against the original.
 * HOW: R0 = the point count ($0146); LINK to FxPolygonClipScreenUV; a
 * kept polygon is queued at 01:920F.
 * Entry: R10 = stack; R14 at the face list; the polygon's points in
 * place; the ROM bank the model's.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelFacesOnScreenUVResult FxModelFacesOnScreenUV(uint16_t divisor_made, uint16_t clip_record_ptr, uint16_t *stack)
{
    uint16_t rom_at_2;
    uint16_t list_start;
    uint16_t model_bank;
    uint16_t point_count;
    uint16_t quotient_sign;
    uint16_t face_sign_word;
    uint16_t vertex_bytes_x2;
    uint16_t vertex_bytes_x4;
    uint16_t divisor_made_out_2;
    uint16_t clipped_vertex_count;

    point_count = fx_ldw(FX_POINT_COUNT);
    { FxPolygonClipScreenUVResult r_ = FxPolygonClipScreenUV(point_count, divisor_made, clip_record_ptr, stack); clipped_vertex_count = r_.clipped_vertex_count; divisor_made_out_2 = r_.divisor_made_out; quotient_sign = r_.quotient_sign; };
    if (clipped_vertex_count == 0) {
    } else {
        face_sign_word = fx_ldw(0x0042);
        list_start = fx_ldw(0x005A);
        vertex_bytes_x2 = (uint16_t)(clipped_vertex_count + clipped_vertex_count);
        if (!(face_sign_word & 0x8000)) {
            fx_stw(FX_LIST_END, (uint16_t)((uint16_t)(vertex_bytes_x2 + vertex_bytes_x2) + list_start));
            divisor_made_out_2 = (uint16_t)(list_start - 2);
            fx_stb(divisor_made_out_2, (uint8_t)clipped_vertex_count);
        } else {
            vertex_bytes_x4 = (uint16_t)(vertex_bytes_x2 + vertex_bytes_x2);
            fx_stw(FX_LIST_END, (uint16_t)((uint16_t)(vertex_bytes_x4 + vertex_bytes_x4) + list_start));
            divisor_made_out_2 = (uint16_t)(list_start - 8);
            fx_stb(divisor_made_out_2, (uint8_t)(uint16_t)~clipped_vertex_count);
        }
        model_bank = fx_ldw(0x001C);
        fx_romb(model_bank);
    }
    rom_at_2 = fx_ldw(0x001A);
    fx_r14(rom_at_2);
    { FxModelFacesResult tr_ = FxModelFaces(divisor_made_out_2, quotient_sign, stack, rom_at_2); return (FxModelFacesOnScreenUVResult){tr_.prev_end_y, tr_.list_write_ptr_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
}


/*
 * @routine 01:9243 FxModelEnd
 * WHAT IT DOES: Command $00: draws the queued faces (FxDrawList) and
 * returns from the model, pulling R15.
 * HOW: LINK to FxDrawList, then DEC R10 twice and R15 from (R10).
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelEndResult FxModelEnd(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at)
{
    uint16_t rom_at_out_2;
    uint16_t caller_list_end_out;
    uint16_t caller_write_ptr_out;

    { FxDrawListResult r_ = FxDrawList(draw_list_start, draw_list_end, stack, rom_at); caller_list_end_out = r_.caller_list_end_out; caller_write_ptr_out = r_.caller_write_ptr_out; rom_at_out_2 = r_.rom_at_out; };
    fx_unspill(stack);
    return (FxModelEndResult){caller_list_end_out, caller_write_ptr_out, rom_at_out_2, 1};
}


/*
 * @routine 01:9250 FxModelSpriteColour
 * WHAT IT DOES: The sprite's colour: as FxModelMaterial for the flat
 * case (not reached in the traces so far).
 * HOW: Shares FxSpriteMaterial's ending (01:939E).
 * Entry: R14 at the sprite's bytes; R11 = return address.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelSpriteColourResult FxModelSpriteColour(uint16_t draw_list_start, uint16_t rom_at)
{
    uint16_t flags;
    uint16_t material_index;
    uint16_t saved_rom_bank;

    flags = fx_ldw(FX_MODEL_FLAGS);
    if ((flags & 0x2000) == 0) {
        material_index = fx_getb();
        { FxSpriteMaterialResult tr_ = FxSpriteMaterial(material_index, rom_at, draw_list_start); return (FxModelSpriteColourResult){tr_.shade_row_base_out, tr_.rom_at}; }
    } else {
        if ((flags & 0x1000) != 0) {
            material_index = fx_getb();
            { FxSpriteMaterialResult tr_ = FxSpriteMaterial(material_index, rom_at, draw_list_start); return (FxModelSpriteColourResult){tr_.shade_row_base_out, tr_.rom_at}; }
        } else {
            fx_cmode(0);
            fx_color((uint8_t)9);
            fx_stw(0x0058, 0);
            fx_stw(0x0042, 0);
            saved_rom_bank = fx_ldw(0x001C);
            fx_romb(saved_rom_bank);
            fx_r14(rom_at);
            return (FxModelSpriteColourResult){draw_list_start, rom_at};
        }
    }
}


/*
 * @routine 01:9276 FxSpriteMaterial
 * WHAT IT DOES: Sets the colour of a sprite from its material word: as
 * FxModelMaterial without the textured faces.
 * HOW: R2 keeps R14; ends at 939E: ROMB from $001C, R14 = R2, JMP R11.
 * Entry: R0 = the material index; R11 = return address.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxSpriteMaterialResult FxSpriteMaterial(uint16_t material_index, uint16_t model_stream_ptr, uint16_t sprite_scale_word)
{
    uint16_t flags;
    uint16_t mask_lo;
    uint16_t normal_x;
    uint16_t normal_y;
    uint16_t normal_z;
    uint16_t rom_at_2;
    uint16_t shade_at;
    uint16_t tex_bank;
    uint16_t tex_base;
    uint16_t tex_mask;
    uint16_t light_level;
    uint16_t shade_table;
    uint16_t color_nibble;
    uint16_t normal_y_ptr;
    uint16_t normal_z_ptr;
    uint16_t shade_row_lo;
    uint16_t indirect_pick;
    uint16_t light_local_x;
    uint16_t light_local_y;
    uint16_t light_local_z;
    uint16_t mask_table_lo;
    uint16_t material_mask;
    uint16_t material_word;
    uint16_t shade_counter;
    uint16_t shade_row_ptr;
    uint16_t indirect_count;
    uint16_t mask_table_ptr;
    uint16_t saved_rom_bank;
    uint16_t color_base_byte;
    uint16_t color_nibble_x2;
    uint16_t color_nibble_x4;
    uint16_t color_nibble_x8;
    uint16_t indirect_offset;
    uint16_t texture_base_lo;
    uint16_t material_lo_byte;
    uint16_t texture_bank_ptr;
    uint16_t texture_mask_ptr;
    uint16_t shade_counter_dec;
    uint16_t shade_field_plus1;
    uint16_t shade_field_plus2;
    uint16_t texture_entry_ptr;
    uint16_t texture_set_index;
    uint16_t material_word_read;
    uint16_t saved_rom_bank_lit;
    int shade_counter_zero;
    uint16_t material_table_base;

    fx_romb(3);
    material_table_base = fx_ldw(0x004A);
    rom_at_2 = (uint16_t)((uint16_t)(material_index + material_index) + material_table_base);
    fx_r14(rom_at_2);
    for (;;) {
        material_lo_byte = fx_getb();
        fx_r14((uint16_t)(rom_at_2 + 1));
        material_word_read = (uint16_t)((material_lo_byte & 0xFF) | fx_getb() << 8);
        material_word = material_word_read;
        if (!(material_word_read & 0x8000)) {
            fx_r14((uint16_t)((uint16_t)(material_word & 0xFF) - 0x604F));
            color_base_byte = fx_getb();
            fx_stw(0x0058, (uint16_t)(color_base_byte + 0x0800));
            shade_counter_zero = 1;
            shade_counter = 0;
            if (!((uint16_t)(material_word + material_word) & 0x8000)) {
            } else {
                shade_counter_dec = (uint16_t)(shade_counter - 1);
                shade_counter_zero = (shade_counter_dec == 0);
                shade_counter = shade_counter_dec;
            }
            fx_stw(0x0042, shade_counter);
            if (shade_counter_zero) {
                shade_field_plus1 = ((uint16_t)((uint16_t)(material_word >> 8) + 1) & 0x003F);
                if (shade_field_plus1 != 0) {
                    shade_field_plus2 = ((uint16_t)(shade_field_plus1 + 1) & 0x003F);
                    if (shade_field_plus2 == 0) {
                        shade_at = fx_ldw(FX_SHADE_AT);
                        fx_r14((uint16_t)((uint16_t)(material_word & 0xFF) + shade_at));
                        fx_color(fx_getb());
                        break;
                    } else {
                        material_word = ((uint16_t)(shade_field_plus2 - 2) & 0x003F);
                        flags = fx_ldw(FX_MODEL_FLAGS);
                        if (!((uint16_t)(flags >> 8) >> 7 & 1)) {
                            fx_color((uint8_t)(uint16_t)(material_word & 0xFF));
                            break;
                        } else {
                            shade_table = fx_ldw(FX_SHADE_TABLE);
                            shade_row_ptr = (uint16_t)((uint16_t)(shade_table + material_word) + material_word);
                            fx_r14(shade_row_ptr);
                            shade_row_lo = fx_getb();
                            fx_r14((uint16_t)(shade_row_ptr + 1));
                            sprite_scale_word = (uint16_t)((shade_row_lo & 0xFF) | fx_getb() << 8);
                            saved_rom_bank_lit = fx_ldw(0x001C);
                            fx_romb(saved_rom_bank_lit);
                            fx_r14(model_stream_ptr);
                            normal_y_ptr = (uint16_t)(model_stream_ptr + 1);
                            fx_r14(normal_y_ptr);
                            normal_x = fx_getb();
                            normal_z_ptr = (uint16_t)(normal_y_ptr + 1);
                            fx_r14(normal_z_ptr);
                            light_local_x = fx_ldw(FX_LIGHT_LOCAL_X);
                            normal_y = fx_getb();
                            fx_r14((uint16_t)(normal_z_ptr + 1));
                            light_local_y = fx_ldw(FX_LIGHT_LOCAL_Y);
                            normal_z = fx_getb();
                            light_local_z = fx_ldw(FX_LIGHT_LOCAL_Z);
                            light_level = (uint16_t)(int16_t)(int8_t)(uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)(uint16_t)((uint16_t)((uint16_t)((int8_t)normal_z * (int8_t)light_local_z) + (uint16_t)((int8_t)normal_y * (int8_t)light_local_y)) + (uint16_t)((int8_t)normal_x * (int8_t)light_local_x)) >> 1) >> 1) >> 8);
                            if (((((uint16_t)(light_level - 6) & 0x8000) != 0) == (((light_level ^ 6) & (light_level ^ (uint16_t)(light_level - 6 - 0)) & 0x8000) != 0))) {
                                if (((((uint16_t)(light_level - 0x0010) & 0x8000) != 0) != (((light_level ^ 0x0010) & (light_level ^ (uint16_t)(light_level - 0x0010 - 0)) & 0x8000) != 0))) {
                                } else {
                                    light_level = 0x000F;
                                }
                            } else {
                                light_level = 6;
                            }
                            fx_romb(3);
                            fx_r14((uint16_t)((uint16_t)(sprite_scale_word + light_level) - 6));
                            fx_color(fx_getb());
                            break;
                        }
                    }
                } else {
                    fx_color((uint8_t)(uint16_t)(material_word & 0xFF));
                    break;
                }
            } else {
                if ((0x2000 & material_word) == 0) {
                } else {
                    fx_stw(0x0058, (uint16_t)(color_base_byte + 0x0C00));
                }
                texture_entry_ptr = (uint16_t)((uint16_t)((uint8_t)(uint16_t)(material_word & 0xFF) * (uint8_t)3) - 0x62A7);
                fx_r14(texture_entry_ptr);
                texture_base_lo = fx_getb();
                texture_bank_ptr = (uint16_t)(texture_entry_ptr + 1);
                fx_r14(texture_bank_ptr);
                tex_base = (uint16_t)((texture_base_lo & 0xFF) | fx_getb() << 8);
                fx_r14((uint16_t)(texture_bank_ptr + 1));
                fx_stw(FX_TEX_BASE, tex_base);
                tex_bank = fx_getb();
                fx_stw(FX_TEX_BANK, tex_bank);
                texture_set_index = ((uint16_t)(material_word >> 8) & 0x001F);
                texture_mask_ptr = (uint16_t)((uint16_t)(texture_set_index + texture_set_index) - 0x5F87);
                fx_r14(texture_mask_ptr);
                mask_table_lo = fx_getb();
                fx_r14((uint16_t)(texture_mask_ptr + 1));
                mask_table_ptr = (uint16_t)((mask_table_lo & 0xFF) | fx_getb() << 8);
                fx_r14(mask_table_ptr);
                mask_lo = fx_getb();
                fx_r14((uint16_t)(mask_table_ptr + 1));
                tex_mask = (uint16_t)((mask_lo & 0xFF) | fx_getb() << 8);
                fx_stw(FX_TEX_MASK, tex_mask);
                fx_stw(0x0088, (uint16_t)(mask_table_ptr + 2));
                break;
            }
            saved_rom_bank = fx_ldw(0x001C);
            fx_romb(saved_rom_bank);
            fx_r14(model_stream_ptr);
            return (FxSpriteMaterialResult){sprite_scale_word, model_stream_ptr};
        } else {
            if (!((uint16_t)(material_word + material_word) & 0x8000)) {
                indirect_offset = (0x1FFF & material_word);
                fx_r14(indirect_offset);
                material_mask = fx_ldw(FX_MATERIAL_MASK);
                indirect_count = fx_getb();
                indirect_pick = (material_mask & (uint16_t)(indirect_count - 1));
                rom_at_2 = (uint16_t)((uint16_t)((uint16_t)(indirect_pick + indirect_pick) + indirect_offset) + 1);
                fx_r14(rom_at_2);
                continue;
            } else {
                fx_stw(0x0042, 0xFFFE);
                color_nibble = ((uint16_t)(material_word >> 8) & 0x000F);
                color_nibble_x2 = (uint16_t)(color_nibble + color_nibble);
                color_nibble_x4 = (uint16_t)(color_nibble_x2 + color_nibble_x2);
                color_nibble_x8 = (uint16_t)(color_nibble_x4 + color_nibble_x4);
                fx_stw(0x0058, (uint16_t)(((uint16_t)((uint16_t)(color_nibble_x8 + color_nibble_x8) << 8 | (uint16_t)(color_nibble_x8 + color_nibble_x8) >> 8) | 8) << 8 | ((uint16_t)((uint16_t)(color_nibble_x8 + color_nibble_x8) << 8 | (uint16_t)(color_nibble_x8 + color_nibble_x8) >> 8) | 8) >> 8));
                break;
            }
        }
    }
    saved_rom_bank = fx_ldw(0x001C);
    fx_romb(saved_rom_bank);
    fx_r14(model_stream_ptr);
    return (FxSpriteMaterialResult){sprite_scale_word, model_stream_ptr};
}


/*
 * @routine 01:93A7 FxModelFacing
 * WHAT IT DOES: Command $30: for n triangles (n the next byte) of three
 * projected points ($04E4 + 6index), the sign of their cross product
 * XORed with the points' attribute bytes, stored as a flag byte at
 * $07C6 on (skipped to 01:941B when bit 14 of $0054 is set).
 * HOW: A LOOP of R12 = n (R13 = $93C1); LMULT twice, SUB and SBC.
 * Entry: R10 = stack; R11 = return address; R14 at the command's bytes
 * (bank $001C).
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxModelFacingResult FxModelFacing(uint16_t *stack, uint16_t rom_at)
{
    uint16_t count;
    uint16_t flags;
    uint16_t count_2;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t point_a_x;
    uint16_t point_a_y;
    uint16_t point_b_x;
    uint16_t point_b_y;
    uint16_t point_c_x;
    uint16_t point_c_y;
    uint16_t rom_ptr_b;
    uint16_t rom_ptr_c;
    uint16_t edge_ab_dx;
    uint16_t facing_ptr;
    uint16_t next_rom_at;
    uint32_t cross_term_a;
    uint32_t cross_term_b;
    uint16_t next_command;
    uint16_t point_a_addr;
    uint16_t point_b_addr;
    uint16_t point_c_addr;
    uint16_t point_a_index;
    uint16_t point_b_index;
    uint16_t point_c_index;
    uint16_t depth_flags_x2;
    uint16_t depth_flags_x4;
    uint16_t point_a_x_byte;
    uint16_t point_a_y_addr;
    uint16_t point_a_y_byte;
    uint16_t point_b_x_byte;
    uint16_t point_b_y_addr;
    uint16_t point_b_y_byte;
    uint16_t point_c_x_byte;
    uint16_t point_c_y_addr;
    uint16_t point_c_y_byte;
    uint16_t rom_ptr_b_byte;
    uint16_t rom_ptr_c_byte;
    uint16_t depth_flags_xor;
    uint16_t edge_ab_dx_byte;
    uint16_t facing_ptr_byte;
    uint16_t next_rom_at_byte;
    uint32_t cross_term_a_byte;
    uint16_t cross_term_a_high;
    uint32_t cross_term_b_byte;
    uint16_t next_command_byte;
    uint16_t point_a_addr_byte;
    uint16_t point_b_addr_byte;
    uint16_t point_c_addr_byte;
    uint16_t point_a_index_byte;
    uint16_t point_b_index_byte;
    uint16_t point_c_index_byte;
    uint16_t point_a_depth_flags;
    uint16_t point_b_depth_flags;
    uint16_t point_c_depth_flags;
    uint16_t cross_term_a_high_byte;

    flags = fx_ldw(FX_MODEL_FLAGS);
    if ((flags & 0x4000) == 0) {
        count = fx_getb();
        rom_at_2 = (uint16_t)(rom_at + 1);
        fx_r14(rom_at_2);
        facing_ptr = 0x07C6;
        for (;;) {
            point_a_index = fx_getb();
            rom_ptr_b = (uint16_t)(rom_at_2 + 1);
            fx_r14(rom_ptr_b);
            point_a_addr = (uint16_t)((uint16_t)((int8_t)point_a_index * (int8_t)6) + 0x04E4);
            point_a_x = fx_ldw(point_a_addr);
            point_a_y_addr = (uint16_t)(point_a_addr + 2);
            point_a_y = fx_ldw(point_a_y_addr);
            point_a_depth_flags = fx_ldb((uint16_t)(point_a_y_addr + 2));
            point_b_index = fx_getb();
            rom_ptr_c = (uint16_t)(rom_ptr_b + 1);
            fx_r14(rom_ptr_c);
            point_b_addr = (uint16_t)((uint16_t)((int8_t)point_b_index * (int8_t)6) + 0x04E4);
            point_b_x = fx_ldw(point_b_addr);
            point_b_y_addr = (uint16_t)(point_b_addr + 2);
            edge_ab_dx = (uint16_t)(point_b_x - point_a_x);
            point_b_y = fx_ldw(point_b_y_addr);
            point_b_depth_flags = fx_ldb((uint16_t)(point_b_y_addr + 2));
            point_c_index = fx_getb();
            rom_at_2 = (uint16_t)(rom_ptr_c + 1);
            fx_r14(rom_at_2);
            point_c_addr = (uint16_t)((uint16_t)((int8_t)point_c_index * (int8_t)6) + 0x04E4);
            point_c_x = fx_ldw(point_c_addr);
            point_c_y_addr = (uint16_t)(point_c_addr + 2);
            point_c_y = fx_ldw(point_c_y_addr);
            point_c_depth_flags = fx_ldb((uint16_t)(point_c_y_addr + 2));
            depth_flags_xor = (point_c_depth_flags ^ (point_b_depth_flags ^ point_a_depth_flags));
            cross_term_a = (uint32_t)((int32_t)(int16_t)(uint16_t)(point_b_y - point_a_y) * (int16_t)(uint16_t)(point_c_x - point_a_x));
            cross_term_a_high = (uint16_t)(cross_term_a >> 16);
            cross_term_b = (uint32_t)((int32_t)(int16_t)(uint16_t)(point_c_y - point_a_y) * (int16_t)edge_ab_dx);
            depth_flags_x2 = (uint16_t)(depth_flags_xor + depth_flags_xor);
            depth_flags_x4 = (uint16_t)(depth_flags_x2 + depth_flags_x2);
            fx_stb(facing_ptr, fx_face_facing(point_a_addr, point_b_addr, point_c_addr,
                                              (uint8_t)(uint16_t)(((uint16_t)((uint16_t)(depth_flags_x4 + depth_flags_x4) << 8 | (uint16_t)(depth_flags_x4 + depth_flags_x4) >> 8) ^ (uint16_t)((uint16_t)(cross_term_b >> 16) - cross_term_a_high - ((uint16_t)cross_term_b < (uint16_t)cross_term_a))) >> 8)));
            count = (uint16_t)(count - 1);
            facing_ptr = (uint16_t)(facing_ptr + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        next_command = fx_getb();
        next_rom_at = (uint16_t)(rom_at_2 + 1);
        fx_r14(next_rom_at);
        { FxModelNextResult tr_ = FxModelNext(edge_ab_dx, cross_term_a_high, stack, next_rom_at, next_command); return (FxModelFacingResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    } else {
        count_2 = fx_getb();
        rom_at_3 = (uint16_t)(rom_at + 1);
        fx_r14(rom_at_3);
        facing_ptr_byte = 0x07C6;
        for (;;) {
            point_a_index_byte = fx_getb();
            rom_ptr_b_byte = (uint16_t)(rom_at_3 + 1);
            fx_r14(rom_ptr_b_byte);
            point_a_addr_byte = (uint16_t)((uint16_t)((uint8_t)point_a_index_byte * (uint8_t)6) + 0x04E4);
            point_a_x_byte = fx_ldb(point_a_addr_byte);
            point_a_y_byte = fx_ldb((uint16_t)(point_a_addr_byte + 2));
            point_b_index_byte = fx_getb();
            rom_ptr_c_byte = (uint16_t)(rom_ptr_b_byte + 1);
            fx_r14(rom_ptr_c_byte);
            point_b_addr_byte = (uint16_t)((uint16_t)((uint8_t)point_b_index_byte * (uint8_t)6) + 0x04E4);
            point_b_x_byte = fx_ldb(point_b_addr_byte);
            edge_ab_dx_byte = (uint16_t)(point_b_x_byte - point_a_x_byte);
            point_b_y_byte = fx_ldb((uint16_t)(point_b_addr_byte + 2));
            point_c_index_byte = fx_getb();
            rom_at_3 = (uint16_t)(rom_ptr_c_byte + 1);
            fx_r14(rom_at_3);
            point_c_addr_byte = (uint16_t)((uint16_t)((uint8_t)point_c_index_byte * (uint8_t)6) + 0x04E4);
            point_c_x_byte = fx_ldb(point_c_addr_byte);
            point_c_y_byte = fx_ldb((uint16_t)(point_c_addr_byte + 2));
            cross_term_a_byte = (uint32_t)((int32_t)(int16_t)(uint16_t)(point_b_y_byte - point_a_y_byte) * (int16_t)(uint16_t)(point_c_x_byte - point_a_x_byte));
            cross_term_a_high_byte = (uint16_t)(cross_term_a_byte >> 16);
            cross_term_b_byte = (uint32_t)((int32_t)(int16_t)(uint16_t)(point_c_y_byte - point_a_y_byte) * (int16_t)edge_ab_dx_byte);
            fx_stb(facing_ptr_byte, fx_face_facing(point_a_addr_byte, point_b_addr_byte, point_c_addr_byte,
                                                   (uint8_t)(uint16_t)((uint16_t)((uint16_t)(cross_term_b_byte >> 16) - cross_term_a_high_byte - ((uint16_t)cross_term_b_byte < (uint16_t)cross_term_a_byte)) >> 8)));
            count_2 = (uint16_t)(count_2 - 1);
            facing_ptr_byte = (uint16_t)(facing_ptr_byte + 1);
            if (count_2 != 0) {
                continue;
            } else {
                break;
            }
        }
        next_command_byte = fx_getb();
        next_rom_at_byte = (uint16_t)(rom_at_3 + 1);
        fx_r14(next_rom_at_byte);
        { FxModelNextResult tr_ = FxModelNext(edge_ab_dx_byte, cross_term_a_high_byte, stack, next_rom_at_byte, next_command_byte); return (FxModelFacingResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    }
}


/*
 * @routine 01:9A52 FxPolygonClipScreenUV
 * WHAT IT DOES: Clips a textured polygon (x, y, u, v words a vertex) at
 * $005A against the four screen edges, as FxPolygonClipScreen does for
 * flat ones (not reached in the traces so far).
 * HOW: Pushes R11; the crossings by FxDivideSigned; pulls R15.
 * Entry: R0 = the vertex count; R10 = stack; R11 = return address.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
FxPolygonClipScreenUVResult FxPolygonClipScreenUV(uint16_t vertex_count, uint16_t divisor_made, uint16_t quotient_sign_in, uint16_t *stack)
{
    int run_l9EFB = 0;
    uint16_t model_96;
    uint16_t quotient;
    uint16_t top_cur_u;
    uint16_t top_cur_v;
    uint16_t top_cur_x;
    uint16_t top_cur_y;
    uint16_t left_cur_u;
    uint16_t left_cur_v;
    uint16_t left_cur_x;
    uint16_t left_cur_y;
    uint16_t model_96_2;
    uint16_t model_96_3;
    uint16_t model_96_4;
    uint16_t quotient_2;
    uint16_t quotient_3;
    uint16_t quotient_4;
    uint16_t quotient_5;
    uint16_t quotient_6;
    uint16_t quotient_7;
    uint16_t quotient_8;
    uint16_t quotient_9;
    uint16_t top_next_u;
    uint16_t top_next_v;
    uint16_t top_next_x;
    uint32_t top_u_prod;
    uint32_t top_v_prod;
    uint32_t top_x_prod;
    uint16_t top_y_span;
    uint16_t left_next_u;
    uint16_t left_next_v;
    uint16_t left_next_x;
    uint32_t left_u_prod;
    uint32_t left_v_prod;
    uint16_t left_x_span;
    uint32_t left_y_prod;
    uint16_t quotient_10;
    uint16_t quotient_11;
    uint16_t quotient_12;
    uint16_t right_cur_u;
    uint16_t right_cur_v;
    uint16_t right_cur_x;
    uint16_t right_cur_y;
    uint16_t top_first_u;
    uint16_t top_first_v;
    uint16_t top_first_x;
    uint16_t top_first_y;
    uint16_t top_out_ptr;
    uint16_t bottom_cur_u;
    uint16_t bottom_cur_v;
    uint16_t bottom_cur_x;
    uint16_t bottom_cur_y;
    uint16_t left_first_u;
    uint16_t left_first_v;
    uint16_t left_first_x;
    uint16_t left_first_y;
    uint16_t left_out_ptr;
    uint16_t right_next_u;
    uint16_t right_next_v;
    uint16_t right_next_x;
    uint32_t right_u_prod;
    uint32_t right_v_prod;
    uint16_t right_x_span;
    uint32_t right_y_prod;
    uint16_t bottom_next_u;
    uint16_t bottom_next_v;
    uint16_t bottom_next_x;
    uint32_t bottom_u_prod;
    uint32_t bottom_v_prod;
    uint32_t bottom_x_prod;
    uint16_t bottom_y_span;
    uint16_t right_first_u;
    uint16_t right_first_v;
    uint16_t right_first_x;
    uint16_t right_first_y;
    uint16_t right_out_ptr;
    uint16_t bottom_first_u;
    uint16_t bottom_first_v;
    uint16_t bottom_first_x;
    uint16_t bottom_first_y;
    uint16_t bottom_out_ptr;
    uint16_t right_x_diff_a;
    uint16_t right_x_diff_b;
    uint16_t right_x_diff_c;
    uint16_t top_cur_u_addr;
    uint16_t top_cur_v_addr;
    uint16_t top_cur_y_addr;
    uint16_t top_edge_pos_a;
    uint16_t top_edge_pos_b;
    uint16_t top_edge_pos_c;
    uint16_t top_edge_pos_d;
    uint16_t top_emit_src_u;
    uint16_t top_emit_src_v;
    uint16_t top_keep_src_u;
    uint16_t top_keep_src_v;
    uint16_t bottom_y_diff_a;
    uint16_t bottom_y_diff_b;
    uint16_t bottom_y_diff_c;
    uint16_t left_cur_u_addr;
    uint16_t left_cur_v_addr;
    uint16_t left_cur_y_addr;
    uint16_t left_edge_pos_a;
    uint16_t left_edge_pos_b;
    uint16_t left_edge_pos_c;
    uint16_t left_edge_pos_d;
    uint16_t left_edge_pos_e;
    uint16_t left_edge_pos_f;
    uint16_t left_emit_src_u;
    uint16_t left_emit_src_v;
    uint16_t left_keep_src_u;
    uint16_t left_keep_src_v;
    uint16_t top_edge_offset;
    uint16_t top_emit_u_addr;
    uint16_t top_emit_v_addr;
    uint16_t top_emit_y_addr;
    uint16_t top_keep_u_addr;
    uint16_t top_keep_v_addr;
    uint16_t top_keep_y_addr;
    uint16_t top_next_u_addr;
    uint16_t top_next_v_addr;
    uint16_t top_next_x_addr;
    uint16_t top_next_y_addr;
    uint16_t top_poly_in_ptr;
    uint16_t left_emit_u_addr;
    uint16_t left_emit_v_addr;
    uint16_t left_emit_y_addr;
    uint16_t left_keep_u_addr;
    uint16_t left_keep_v_addr;
    uint16_t left_keep_y_addr;
    uint16_t left_next_u_addr;
    uint16_t left_next_v_addr;
    uint16_t left_next_x_addr;
    uint16_t left_next_y_addr;
    uint16_t left_poly_in_ptr;
    uint16_t right_cur_u_addr;
    uint16_t right_cur_v_addr;
    uint16_t right_cur_y_addr;
    uint16_t right_edge_pos_a;
    uint16_t right_edge_pos_b;
    uint16_t right_edge_pos_c;
    uint16_t right_edge_pos_d;
    uint16_t right_edge_pos_e;
    uint16_t right_edge_pos_f;
    uint16_t right_emit_src_u;
    uint16_t right_emit_src_v;
    uint16_t right_keep_src_u;
    uint16_t right_keep_src_v;
    uint16_t top_cross_u_addr;
    uint16_t top_cross_v_addr;
    uint16_t top_cross_y_addr;
    uint16_t top_edge_pos_out;
    uint16_t top_first_u_addr;
    uint16_t top_first_v_addr;
    uint16_t top_first_y_addr;
    uint16_t bottom_cur_u_addr;
    uint16_t bottom_cur_v_addr;
    uint16_t bottom_cur_y_addr;
    uint16_t bottom_edge_pos_a;
    uint16_t bottom_edge_pos_b;
    uint16_t bottom_edge_pos_c;
    uint16_t bottom_edge_pos_d;
    uint16_t bottom_emit_src_u;
    uint16_t bottom_emit_src_v;
    uint16_t bottom_keep_src_u;
    uint16_t bottom_keep_src_v;
    uint16_t left_cross_u_addr;
    uint16_t left_cross_v_addr;
    uint16_t left_cross_y_addr;
    uint16_t left_edge_pos_out;
    uint16_t left_first_u_addr;
    uint16_t left_first_v_addr;
    uint16_t left_first_y_addr;
    uint16_t right_emit_u_addr;
    uint16_t right_emit_v_addr;
    uint16_t right_emit_y_addr;
    uint16_t right_keep_u_addr;
    uint16_t right_keep_v_addr;
    uint16_t right_keep_y_addr;
    uint16_t right_next_u_addr;
    uint16_t right_next_v_addr;
    uint16_t right_next_x_addr;
    uint16_t right_next_y_addr;
    uint16_t vertex_count_save;
    uint16_t bottom_edge_offset;
    uint16_t bottom_emit_u_addr;
    uint16_t bottom_emit_v_addr;
    uint16_t bottom_emit_y_addr;
    uint16_t bottom_keep_u_addr;
    uint16_t bottom_keep_v_addr;
    uint16_t bottom_keep_y_addr;
    uint16_t bottom_next_u_addr;
    uint16_t bottom_next_v_addr;
    uint16_t bottom_next_x_addr;
    uint16_t bottom_next_y_addr;
    uint16_t right_cross_u_addr;
    uint16_t right_cross_v_addr;
    uint16_t right_cross_y_addr;
    uint16_t right_edge_pos_out;
    uint16_t right_first_u_addr;
    uint16_t right_first_v_addr;
    uint16_t right_first_y_addr;
    uint16_t top_closing_u_addr;
    uint16_t top_closing_x_addr;
    uint16_t top_closing_y_addr;
    uint16_t top_extra_vertices;
    uint16_t bottom_cross_u_addr;
    uint16_t bottom_cross_v_addr;
    uint16_t bottom_cross_y_addr;
    uint16_t bottom_edge_pos_out;
    uint16_t bottom_first_u_addr;
    uint16_t bottom_first_v_addr;
    uint16_t bottom_first_y_addr;
    uint16_t left_closing_u_addr;
    uint16_t left_closing_x_addr;
    uint16_t left_closing_y_addr;
    uint16_t left_extra_vertices;
    uint16_t top_emit_src_u_addr;
    uint16_t top_emit_src_v_addr;
    uint16_t top_keep_src_u_addr;
    uint16_t top_keep_src_v_addr;
    uint16_t top_vertex_ptr_init;
    uint16_t vertex_count_save_2;
    uint16_t vertex_count_save_3;
    uint16_t vertex_count_save_4;
    uint16_t vertex_count_save_5;
    uint16_t vertex_count_save_6;
    uint16_t vertex_count_save_7;
    uint16_t vertex_count_save_8;
    uint16_t vertex_count_save_9;
    uint16_t left_emit_src_u_addr;
    uint16_t left_emit_src_v_addr;
    uint16_t left_keep_src_u_addr;
    uint16_t left_keep_src_v_addr;
    uint16_t left_vertex_ptr_init;
    uint16_t right_closing_u_addr;
    uint16_t right_closing_x_addr;
    uint16_t right_closing_y_addr;
    uint16_t right_extra_vertices;
    uint16_t vertex_count_save_10;
    uint16_t vertex_count_save_11;
    uint16_t vertex_count_save_12;
    uint16_t bottom_closing_u_addr;
    uint16_t bottom_closing_x_addr;
    uint16_t bottom_closing_y_addr;
    uint16_t bottom_extra_vertices;
    uint16_t right_emit_src_u_addr;
    uint16_t right_emit_src_v_addr;
    uint16_t right_keep_src_u_addr;
    uint16_t right_keep_src_v_addr;
    uint16_t right_vertex_ptr_init;
    uint16_t top_vertex_count_load;
    uint16_t top_vertices_left_dec;
    uint16_t bottom_emit_src_u_addr;
    uint16_t bottom_emit_src_v_addr;
    uint16_t bottom_keep_src_u_addr;
    uint16_t bottom_keep_src_v_addr;
    uint16_t bottom_vertex_ptr_init;
    uint16_t left_vertex_count_load;
    uint16_t left_vertices_left_dec;
    uint16_t top_vertices_left_load;
    uint16_t left_vertices_left_load;
    uint16_t right_vertex_count_load;
    uint16_t right_vertices_left_dec;
    uint16_t bottom_vertex_count_load;
    uint16_t bottom_vertices_left_dec;
    uint16_t right_vertices_left_load;
    uint16_t bottom_vertices_left_load;

    fx_spill(stack);
    if (!((uint16_t)(vertex_count - 3) & 0x8000)) {
        fx_stw(FX_VERTEX_COUNT_SAVE, vertex_count);
        fx_stw(0x0040, vertex_count);
        left_poly_in_ptr = fx_ldw(0x005A);
        left_out_ptr = 0x0688;
        left_first_x = fx_ldw(left_poly_in_ptr);
        left_first_y_addr = (uint16_t)(left_poly_in_ptr + 2);
        left_first_y = fx_ldw(left_first_y_addr);
        left_first_u_addr = (uint16_t)(left_first_y_addr + 2);
        left_first_u = fx_ldw(left_first_u_addr);
        left_first_v_addr = (uint16_t)(left_first_u_addr + 2);
        left_first_v = fx_ldw(left_first_v_addr);
        left_vertex_ptr_init = (uint16_t)(left_first_v_addr - 6);
        left_vertex_count_load = fx_ldw(0x0040);
        left_closing_x_addr = (uint16_t)((uint16_t)((int8_t)left_vertex_count_load * (int8_t)8) + left_vertex_ptr_init);
        fx_stw(left_closing_x_addr, left_first_x);
        left_closing_y_addr = (uint16_t)(left_closing_x_addr + 2);
        fx_stw(left_closing_y_addr, left_first_y);
        left_closing_u_addr = (uint16_t)(left_closing_y_addr + 2);
        fx_stw(left_closing_u_addr, left_first_u);
        fx_stw((uint16_t)(left_closing_u_addr + 2), left_first_v);
        for (;;) {
            left_cur_x = fx_ldw(left_vertex_ptr_init);
            left_cur_y_addr = (uint16_t)(left_vertex_ptr_init + 2);
            left_cur_y = fx_ldw(left_cur_y_addr);
            left_next_x_addr = (uint16_t)(left_cur_y_addr + 6);
            left_next_x = fx_ldw(left_next_x_addr);
            left_next_y_addr = (uint16_t)(left_next_x_addr + 2);
            divisor_made = fx_ldw(left_next_y_addr);
            left_vertex_ptr_init = (uint16_t)(left_next_y_addr - 2);
            left_edge_pos_a = fx_ldw(0x0038);
            if (((((uint16_t)(left_cur_x - left_edge_pos_a) & 0x8000) != 0) != (((left_cur_x ^ left_edge_pos_a) & (left_cur_x ^ (uint16_t)(left_cur_x - left_edge_pos_a - 0)) & 0x8000) != 0))) {
                left_edge_pos_b = fx_ldw(0x0038);
                if (((((uint16_t)(left_next_x - left_edge_pos_b) & 0x8000) != 0) != (((left_next_x ^ left_edge_pos_b) & (left_next_x ^ (uint16_t)(left_next_x - left_edge_pos_b - 0)) & 0x8000) != 0))) {
                    vertex_count_save = fx_ldw(FX_VERTEX_COUNT_SAVE);
                    fx_sbk((uint16_t)(vertex_count_save - 1));
                } else {
                    left_x_span = (uint16_t)(left_next_x - left_cur_x);
                    left_edge_pos_d = fx_ldw(0x0038);
                    left_y_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_edge_pos_d - left_cur_x) * (int16_t)(uint16_t)(divisor_made - left_cur_y));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_y_prod, (uint16_t)(left_y_prod >> 16), left_x_span); quotient = r_.quotient; };
                    left_edge_pos_out = fx_ldw(0x0038);
                    fx_stw(left_out_ptr, left_edge_pos_out);
                    left_cross_y_addr = (uint16_t)(left_out_ptr + 2);
                    fx_stw(left_cross_y_addr, (uint16_t)(quotient + left_cur_y));
                    left_cross_u_addr = (uint16_t)(left_cross_y_addr + 2);
                    left_next_u_addr = (uint16_t)(left_vertex_ptr_init + 4);
                    left_next_u = fx_ldw(left_next_u_addr);
                    left_cur_u_addr = (uint16_t)(left_next_u_addr - 8);
                    left_cur_u = fx_ldw(left_cur_u_addr);
                    left_cur_v_addr = (uint16_t)(left_cur_u_addr + 2);
                    left_edge_pos_e = fx_ldw(0x0038);
                    left_u_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_edge_pos_e - left_cur_x) * (int16_t)(uint16_t)(left_next_u - left_cur_u));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_u_prod, (uint16_t)(left_u_prod >> 16), left_x_span); quotient_2 = r_.quotient; };
                    fx_stw(left_cross_u_addr, (uint16_t)(quotient_2 + left_cur_u));
                    left_cross_v_addr = (uint16_t)(left_cross_u_addr + 2);
                    left_cur_v = fx_ldw(left_cur_v_addr);
                    left_next_v_addr = (uint16_t)(left_cur_v_addr + 8);
                    left_next_v = fx_ldw(left_next_v_addr);
                    left_vertex_ptr_init = (uint16_t)(left_next_v_addr - 6);
                    left_edge_pos_f = fx_ldw(0x0038);
                    left_v_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_edge_pos_f - left_cur_x) * (int16_t)(uint16_t)(left_next_v - left_cur_v));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_v_prod, (uint16_t)(left_v_prod >> 16), left_x_span); quotient_3 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                    fx_stw(left_cross_v_addr, (uint16_t)(quotient_3 + left_cur_v));
                    left_out_ptr = (uint16_t)(left_cross_v_addr + 2);
                }
            } else {
                left_edge_pos_c = fx_ldw(0x0038);
                if (((((uint16_t)(left_next_x - left_edge_pos_c) & 0x8000) != 0) != (((left_next_x ^ left_edge_pos_c) & (left_next_x ^ (uint16_t)(left_next_x - left_edge_pos_c - 0)) & 0x8000) != 0))) {
                    vertex_count_save_2 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                    fx_sbk((uint16_t)(vertex_count_save_2 + 1));
                    fx_stw(left_out_ptr, left_cur_x);
                    left_emit_y_addr = (uint16_t)(left_out_ptr + 2);
                    fx_stw(left_emit_y_addr, left_cur_y);
                    left_emit_u_addr = (uint16_t)(left_emit_y_addr + 2);
                    left_emit_src_u_addr = (uint16_t)(left_vertex_ptr_init - 4);
                    left_emit_src_u = fx_ldw(left_emit_src_u_addr);
                    left_emit_src_v_addr = (uint16_t)(left_emit_src_u_addr + 2);
                    fx_stw(left_emit_u_addr, left_emit_src_u);
                    left_emit_v_addr = (uint16_t)(left_emit_u_addr + 2);
                    left_emit_src_v = fx_ldw(left_emit_src_v_addr);
                    left_vertex_ptr_init = (uint16_t)(left_emit_src_v_addr + 2);
                    fx_stw(left_emit_v_addr, left_emit_src_v);
                    left_out_ptr = (uint16_t)(left_emit_v_addr + 2);
                    left_x_span = (uint16_t)(left_next_x - left_cur_x);
                    left_edge_pos_d = fx_ldw(0x0038);
                    left_y_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_edge_pos_d - left_cur_x) * (int16_t)(uint16_t)(divisor_made - left_cur_y));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_y_prod, (uint16_t)(left_y_prod >> 16), left_x_span); quotient = r_.quotient; };
                    left_edge_pos_out = fx_ldw(0x0038);
                    fx_stw(left_out_ptr, left_edge_pos_out);
                    left_cross_y_addr = (uint16_t)(left_out_ptr + 2);
                    fx_stw(left_cross_y_addr, (uint16_t)(quotient + left_cur_y));
                    left_cross_u_addr = (uint16_t)(left_cross_y_addr + 2);
                    left_next_u_addr = (uint16_t)(left_vertex_ptr_init + 4);
                    left_next_u = fx_ldw(left_next_u_addr);
                    left_cur_u_addr = (uint16_t)(left_next_u_addr - 8);
                    left_cur_u = fx_ldw(left_cur_u_addr);
                    left_cur_v_addr = (uint16_t)(left_cur_u_addr + 2);
                    left_edge_pos_e = fx_ldw(0x0038);
                    left_u_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_edge_pos_e - left_cur_x) * (int16_t)(uint16_t)(left_next_u - left_cur_u));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_u_prod, (uint16_t)(left_u_prod >> 16), left_x_span); quotient_2 = r_.quotient; };
                    fx_stw(left_cross_u_addr, (uint16_t)(quotient_2 + left_cur_u));
                    left_cross_v_addr = (uint16_t)(left_cross_u_addr + 2);
                    left_cur_v = fx_ldw(left_cur_v_addr);
                    left_next_v_addr = (uint16_t)(left_cur_v_addr + 8);
                    left_next_v = fx_ldw(left_next_v_addr);
                    left_vertex_ptr_init = (uint16_t)(left_next_v_addr - 6);
                    left_edge_pos_f = fx_ldw(0x0038);
                    left_v_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(left_edge_pos_f - left_cur_x) * (int16_t)(uint16_t)(left_next_v - left_cur_v));
                    { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)left_v_prod, (uint16_t)(left_v_prod >> 16), left_x_span); quotient_3 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                    fx_stw(left_cross_v_addr, (uint16_t)(quotient_3 + left_cur_v));
                    left_out_ptr = (uint16_t)(left_cross_v_addr + 2);
                } else {
                    fx_stw(left_out_ptr, left_cur_x);
                    left_keep_y_addr = (uint16_t)(left_out_ptr + 2);
                    fx_stw(left_keep_y_addr, left_cur_y);
                    left_keep_u_addr = (uint16_t)(left_keep_y_addr + 2);
                    left_keep_src_u_addr = (uint16_t)(left_vertex_ptr_init - 4);
                    left_keep_src_u = fx_ldw(left_keep_src_u_addr);
                    left_keep_src_v_addr = (uint16_t)(left_keep_src_u_addr + 2);
                    fx_stw(left_keep_u_addr, left_keep_src_u);
                    left_keep_v_addr = (uint16_t)(left_keep_u_addr + 2);
                    left_keep_src_v = fx_ldw(left_keep_src_v_addr);
                    left_vertex_ptr_init = (uint16_t)(left_keep_src_v_addr + 2);
                    fx_stw(left_keep_v_addr, left_keep_src_v);
                    left_out_ptr = (uint16_t)(left_keep_v_addr + 2);
                }
            }
            left_vertices_left_load = fx_ldw(0x0040);
            left_vertices_left_dec = (uint16_t)(left_vertices_left_load - 1);
            fx_sbk(left_vertices_left_dec);
            if (left_vertices_left_dec == 0) {
                break;
            } else {
                continue;
            }
        }
        vertex_count_save_3 = fx_ldw(FX_VERTEX_COUNT_SAVE);
        left_extra_vertices = (uint16_t)(vertex_count_save_3 - 3);
        if (!(left_extra_vertices & 0x8000)) {
            fx_stw(0x0040, (uint16_t)(left_extra_vertices + 3));
            right_out_ptr = fx_ldw(0x005A);
            right_first_x = fx_ldw(0x0688);
            right_first_y_addr = (uint16_t)(0 + 0x068A);
            right_first_y = fx_ldw(right_first_y_addr);
            right_first_u_addr = (uint16_t)(right_first_y_addr + 2);
            right_first_u = fx_ldw(right_first_u_addr);
            right_first_v_addr = (uint16_t)(right_first_u_addr + 2);
            right_first_v = fx_ldw(right_first_v_addr);
            right_vertex_ptr_init = (uint16_t)(right_first_v_addr - 6);
            right_vertex_count_load = fx_ldw(0x0040);
            right_closing_x_addr = (uint16_t)((uint16_t)((int8_t)right_vertex_count_load * (int8_t)8) + right_vertex_ptr_init);
            fx_stw(right_closing_x_addr, right_first_x);
            right_closing_y_addr = (uint16_t)(right_closing_x_addr + 2);
            fx_stw(right_closing_y_addr, right_first_y);
            right_closing_u_addr = (uint16_t)(right_closing_y_addr + 2);
            fx_stw(right_closing_u_addr, right_first_u);
            fx_stw((uint16_t)(right_closing_u_addr + 2), right_first_v);
            for (;;) {
                right_cur_x = fx_ldw(right_vertex_ptr_init);
                right_cur_y_addr = (uint16_t)(right_vertex_ptr_init + 2);
                right_cur_y = fx_ldw(right_cur_y_addr);
                right_next_x_addr = (uint16_t)(right_cur_y_addr + 6);
                right_next_x = fx_ldw(right_next_x_addr);
                right_next_y_addr = (uint16_t)(right_next_x_addr + 2);
                divisor_made = fx_ldw(right_next_y_addr);
                right_vertex_ptr_init = (uint16_t)(right_next_y_addr - 2);
                right_edge_pos_a = fx_ldw(0x003A);
                right_x_diff_a = (uint16_t)(right_cur_x - right_edge_pos_a);
                if (((((uint16_t)(right_x_diff_a - 1) & 0x8000) != 0) == (((right_x_diff_a ^ 1) & (right_x_diff_a ^ (uint16_t)(right_x_diff_a - 1 - 0)) & 0x8000) != 0))) {
                    right_edge_pos_b = fx_ldw(0x003A);
                    right_x_diff_b = (uint16_t)(right_next_x - right_edge_pos_b);
                    if (((((uint16_t)(right_x_diff_b - 1) & 0x8000) != 0) == (((right_x_diff_b ^ 1) & (right_x_diff_b ^ (uint16_t)(right_x_diff_b - 1 - 0)) & 0x8000) != 0))) {
                        vertex_count_save_4 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        fx_sbk((uint16_t)(vertex_count_save_4 - 1));
                    } else {
                        right_x_span = (uint16_t)(right_next_x - right_cur_x);
                        right_edge_pos_d = fx_ldw(0x003A);
                        right_y_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_edge_pos_d - right_cur_x) * (int16_t)(uint16_t)(divisor_made - right_cur_y));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_y_prod, (uint16_t)(right_y_prod >> 16), right_x_span); quotient_4 = r_.quotient; };
                        right_edge_pos_out = fx_ldw(0x003A);
                        fx_stw(right_out_ptr, right_edge_pos_out);
                        right_cross_y_addr = (uint16_t)(right_out_ptr + 2);
                        fx_stw(right_cross_y_addr, (uint16_t)(quotient_4 + right_cur_y));
                        right_cross_u_addr = (uint16_t)(right_cross_y_addr + 2);
                        right_next_u_addr = (uint16_t)(right_vertex_ptr_init + 4);
                        right_next_u = fx_ldw(right_next_u_addr);
                        right_cur_u_addr = (uint16_t)(right_next_u_addr - 8);
                        right_cur_u = fx_ldw(right_cur_u_addr);
                        right_cur_v_addr = (uint16_t)(right_cur_u_addr + 2);
                        right_edge_pos_e = fx_ldw(0x003A);
                        right_u_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_edge_pos_e - right_cur_x) * (int16_t)(uint16_t)(right_next_u - right_cur_u));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_u_prod, (uint16_t)(right_u_prod >> 16), right_x_span); quotient_5 = r_.quotient; };
                        fx_stw(right_cross_u_addr, (uint16_t)(quotient_5 + right_cur_u));
                        right_cross_v_addr = (uint16_t)(right_cross_u_addr + 2);
                        right_cur_v = fx_ldw(right_cur_v_addr);
                        right_next_v_addr = (uint16_t)(right_cur_v_addr + 8);
                        right_next_v = fx_ldw(right_next_v_addr);
                        right_vertex_ptr_init = (uint16_t)(right_next_v_addr - 6);
                        right_edge_pos_f = fx_ldw(0x003A);
                        right_v_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_edge_pos_f - right_cur_x) * (int16_t)(uint16_t)(right_next_v - right_cur_v));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_v_prod, (uint16_t)(right_v_prod >> 16), right_x_span); quotient_6 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                        fx_stw(right_cross_v_addr, (uint16_t)(quotient_6 + right_cur_v));
                        right_out_ptr = (uint16_t)(right_cross_v_addr + 2);
                    }
                } else {
                    right_edge_pos_c = fx_ldw(0x003A);
                    right_x_diff_c = (uint16_t)(right_next_x - right_edge_pos_c);
                    if (((((uint16_t)(right_x_diff_c - 1) & 0x8000) != 0) == (((right_x_diff_c ^ 1) & (right_x_diff_c ^ (uint16_t)(right_x_diff_c - 1 - 0)) & 0x8000) != 0))) {
                        vertex_count_save_5 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                        fx_sbk((uint16_t)(vertex_count_save_5 + 1));
                        fx_stw(right_out_ptr, right_cur_x);
                        right_emit_y_addr = (uint16_t)(right_out_ptr + 2);
                        fx_stw(right_emit_y_addr, right_cur_y);
                        right_emit_u_addr = (uint16_t)(right_emit_y_addr + 2);
                        right_emit_src_u_addr = (uint16_t)(right_vertex_ptr_init - 4);
                        right_emit_src_u = fx_ldw(right_emit_src_u_addr);
                        right_emit_src_v_addr = (uint16_t)(right_emit_src_u_addr + 2);
                        fx_stw(right_emit_u_addr, right_emit_src_u);
                        right_emit_v_addr = (uint16_t)(right_emit_u_addr + 2);
                        right_emit_src_v = fx_ldw(right_emit_src_v_addr);
                        right_vertex_ptr_init = (uint16_t)(right_emit_src_v_addr + 2);
                        fx_stw(right_emit_v_addr, right_emit_src_v);
                        right_out_ptr = (uint16_t)(right_emit_v_addr + 2);
                        right_x_span = (uint16_t)(right_next_x - right_cur_x);
                        right_edge_pos_d = fx_ldw(0x003A);
                        right_y_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_edge_pos_d - right_cur_x) * (int16_t)(uint16_t)(divisor_made - right_cur_y));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_y_prod, (uint16_t)(right_y_prod >> 16), right_x_span); quotient_4 = r_.quotient; };
                        right_edge_pos_out = fx_ldw(0x003A);
                        fx_stw(right_out_ptr, right_edge_pos_out);
                        right_cross_y_addr = (uint16_t)(right_out_ptr + 2);
                        fx_stw(right_cross_y_addr, (uint16_t)(quotient_4 + right_cur_y));
                        right_cross_u_addr = (uint16_t)(right_cross_y_addr + 2);
                        right_next_u_addr = (uint16_t)(right_vertex_ptr_init + 4);
                        right_next_u = fx_ldw(right_next_u_addr);
                        right_cur_u_addr = (uint16_t)(right_next_u_addr - 8);
                        right_cur_u = fx_ldw(right_cur_u_addr);
                        right_cur_v_addr = (uint16_t)(right_cur_u_addr + 2);
                        right_edge_pos_e = fx_ldw(0x003A);
                        right_u_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_edge_pos_e - right_cur_x) * (int16_t)(uint16_t)(right_next_u - right_cur_u));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_u_prod, (uint16_t)(right_u_prod >> 16), right_x_span); quotient_5 = r_.quotient; };
                        fx_stw(right_cross_u_addr, (uint16_t)(quotient_5 + right_cur_u));
                        right_cross_v_addr = (uint16_t)(right_cross_u_addr + 2);
                        right_cur_v = fx_ldw(right_cur_v_addr);
                        right_next_v_addr = (uint16_t)(right_cur_v_addr + 8);
                        right_next_v = fx_ldw(right_next_v_addr);
                        right_vertex_ptr_init = (uint16_t)(right_next_v_addr - 6);
                        right_edge_pos_f = fx_ldw(0x003A);
                        right_v_prod = (uint32_t)((int32_t)(int16_t)(uint16_t)(right_edge_pos_f - right_cur_x) * (int16_t)(uint16_t)(right_next_v - right_cur_v));
                        { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)right_v_prod, (uint16_t)(right_v_prod >> 16), right_x_span); quotient_6 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                        fx_stw(right_cross_v_addr, (uint16_t)(quotient_6 + right_cur_v));
                        right_out_ptr = (uint16_t)(right_cross_v_addr + 2);
                    } else {
                        fx_stw(right_out_ptr, right_cur_x);
                        right_keep_y_addr = (uint16_t)(right_out_ptr + 2);
                        fx_stw(right_keep_y_addr, right_cur_y);
                        right_keep_u_addr = (uint16_t)(right_keep_y_addr + 2);
                        right_keep_src_u_addr = (uint16_t)(right_vertex_ptr_init - 4);
                        right_keep_src_u = fx_ldw(right_keep_src_u_addr);
                        right_keep_src_v_addr = (uint16_t)(right_keep_src_u_addr + 2);
                        fx_stw(right_keep_u_addr, right_keep_src_u);
                        right_keep_v_addr = (uint16_t)(right_keep_u_addr + 2);
                        right_keep_src_v = fx_ldw(right_keep_src_v_addr);
                        right_vertex_ptr_init = (uint16_t)(right_keep_src_v_addr + 2);
                        fx_stw(right_keep_v_addr, right_keep_src_v);
                        right_out_ptr = (uint16_t)(right_keep_v_addr + 2);
                    }
                }
                right_vertices_left_load = fx_ldw(0x0040);
                right_vertices_left_dec = (uint16_t)(right_vertices_left_load - 1);
                fx_sbk(right_vertices_left_dec);
                if (right_vertices_left_dec == 0) {
                    break;
                } else {
                    continue;
                }
            }
            vertex_count_save_6 = fx_ldw(FX_VERTEX_COUNT_SAVE);
            right_extra_vertices = (uint16_t)(vertex_count_save_6 - 3);
            if (!(right_extra_vertices & 0x8000)) {
                fx_stw(0x0040, (uint16_t)(right_extra_vertices + 3));
                top_poly_in_ptr = fx_ldw(0x005A);
                top_out_ptr = 0x0688;
                top_first_x = fx_ldw(top_poly_in_ptr);
                top_first_y_addr = (uint16_t)(top_poly_in_ptr + 2);
                top_first_y = fx_ldw(top_first_y_addr);
                top_first_u_addr = (uint16_t)(top_first_y_addr + 2);
                top_first_u = fx_ldw(top_first_u_addr);
                top_first_v_addr = (uint16_t)(top_first_u_addr + 2);
                top_first_v = fx_ldw(top_first_v_addr);
                top_vertex_ptr_init = (uint16_t)(top_first_v_addr - 6);
                top_vertex_count_load = fx_ldw(0x0040);
                top_closing_x_addr = (uint16_t)((uint16_t)((int8_t)top_vertex_count_load * (int8_t)8) + top_vertex_ptr_init);
                fx_stw(top_closing_x_addr, top_first_x);
                top_closing_y_addr = (uint16_t)(top_closing_x_addr + 2);
                fx_stw(top_closing_y_addr, top_first_y);
                top_closing_u_addr = (uint16_t)(top_closing_y_addr + 2);
                fx_stw(top_closing_u_addr, top_first_u);
                fx_stw((uint16_t)(top_closing_u_addr + 2), top_first_v);
                for (;;) {
                    top_cur_x = fx_ldw(top_vertex_ptr_init);
                    top_cur_y_addr = (uint16_t)(top_vertex_ptr_init + 2);
                    top_cur_y = fx_ldw(top_cur_y_addr);
                    top_next_x_addr = (uint16_t)(top_cur_y_addr + 6);
                    top_next_x = fx_ldw(top_next_x_addr);
                    top_next_y_addr = (uint16_t)(top_next_x_addr + 2);
                    divisor_made = fx_ldw(top_next_y_addr);
                    top_vertex_ptr_init = (uint16_t)(top_next_y_addr - 2);
                    top_edge_pos_a = fx_ldw(0x003C);
                    if (((((uint16_t)(top_cur_y - top_edge_pos_a) & 0x8000) != 0) != (((top_cur_y ^ top_edge_pos_a) & (top_cur_y ^ (uint16_t)(top_cur_y - top_edge_pos_a - 0)) & 0x8000) != 0))) {
                        top_edge_pos_b = fx_ldw(0x003C);
                        if (((((uint16_t)(divisor_made - top_edge_pos_b) & 0x8000) != 0) != (((divisor_made ^ top_edge_pos_b) & (divisor_made ^ (uint16_t)(divisor_made - top_edge_pos_b - 0)) & 0x8000) != 0))) {
                            vertex_count_save_7 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                            fx_sbk((uint16_t)(vertex_count_save_7 - 1));
                        } else {
                            top_y_span = (uint16_t)(divisor_made - top_cur_y);
                            top_edge_pos_d = fx_ldw(0x003C);
                            top_edge_offset = (uint16_t)(top_edge_pos_d - top_cur_y);
                            fx_stw(FX_MODEL_96, top_edge_offset);
                            top_x_prod = (uint32_t)((int32_t)(int16_t)top_edge_offset * (int16_t)(uint16_t)(top_next_x - top_cur_x));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_x_prod, (uint16_t)(top_x_prod >> 16), top_y_span); quotient_7 = r_.quotient; };
                            top_edge_pos_out = fx_ldw(0x003C);
                            fx_stw(top_out_ptr, (uint16_t)(quotient_7 + top_cur_x));
                            top_cross_y_addr = (uint16_t)(top_out_ptr + 2);
                            fx_stw(top_cross_y_addr, top_edge_pos_out);
                            top_cross_u_addr = (uint16_t)(top_cross_y_addr + 2);
                            top_next_u_addr = (uint16_t)(top_vertex_ptr_init + 4);
                            top_next_u = fx_ldw(top_next_u_addr);
                            top_cur_u_addr = (uint16_t)(top_next_u_addr - 8);
                            top_cur_u = fx_ldw(top_cur_u_addr);
                            top_cur_v_addr = (uint16_t)(top_cur_u_addr + 2);
                            model_96 = fx_ldw(FX_MODEL_96);
                            top_u_prod = (uint32_t)((int32_t)(int16_t)model_96 * (int16_t)(uint16_t)(top_next_u - top_cur_u));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_u_prod, (uint16_t)(top_u_prod >> 16), top_y_span); quotient_8 = r_.quotient; };
                            fx_stw(top_cross_u_addr, (uint16_t)(quotient_8 + top_cur_u));
                            top_cross_v_addr = (uint16_t)(top_cross_u_addr + 2);
                            top_cur_v = fx_ldw(top_cur_v_addr);
                            top_next_v_addr = (uint16_t)(top_cur_v_addr + 8);
                            top_next_v = fx_ldw(top_next_v_addr);
                            top_vertex_ptr_init = (uint16_t)(top_next_v_addr - 6);
                            model_96_2 = fx_ldw(FX_MODEL_96);
                            top_v_prod = (uint32_t)((int32_t)(int16_t)model_96_2 * (int16_t)(uint16_t)(top_next_v - top_cur_v));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_v_prod, (uint16_t)(top_v_prod >> 16), top_y_span); quotient_9 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                            fx_stw(top_cross_v_addr, (uint16_t)(quotient_9 + top_cur_v));
                            top_out_ptr = (uint16_t)(top_cross_v_addr + 2);
                        }
                    } else {
                        top_edge_pos_c = fx_ldw(0x003C);
                        if (((((uint16_t)(divisor_made - top_edge_pos_c) & 0x8000) != 0) != (((divisor_made ^ top_edge_pos_c) & (divisor_made ^ (uint16_t)(divisor_made - top_edge_pos_c - 0)) & 0x8000) != 0))) {
                            vertex_count_save_8 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                            fx_sbk((uint16_t)(vertex_count_save_8 + 1));
                            fx_stw(top_out_ptr, top_cur_x);
                            top_emit_y_addr = (uint16_t)(top_out_ptr + 2);
                            fx_stw(top_emit_y_addr, top_cur_y);
                            top_emit_u_addr = (uint16_t)(top_emit_y_addr + 2);
                            top_emit_src_u_addr = (uint16_t)(top_vertex_ptr_init - 4);
                            top_emit_src_u = fx_ldw(top_emit_src_u_addr);
                            top_emit_src_v_addr = (uint16_t)(top_emit_src_u_addr + 2);
                            fx_stw(top_emit_u_addr, top_emit_src_u);
                            top_emit_v_addr = (uint16_t)(top_emit_u_addr + 2);
                            top_emit_src_v = fx_ldw(top_emit_src_v_addr);
                            top_vertex_ptr_init = (uint16_t)(top_emit_src_v_addr + 2);
                            fx_stw(top_emit_v_addr, top_emit_src_v);
                            top_out_ptr = (uint16_t)(top_emit_v_addr + 2);
                            top_y_span = (uint16_t)(divisor_made - top_cur_y);
                            top_edge_pos_d = fx_ldw(0x003C);
                            top_edge_offset = (uint16_t)(top_edge_pos_d - top_cur_y);
                            fx_stw(FX_MODEL_96, top_edge_offset);
                            top_x_prod = (uint32_t)((int32_t)(int16_t)top_edge_offset * (int16_t)(uint16_t)(top_next_x - top_cur_x));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_x_prod, (uint16_t)(top_x_prod >> 16), top_y_span); quotient_7 = r_.quotient; };
                            top_edge_pos_out = fx_ldw(0x003C);
                            fx_stw(top_out_ptr, (uint16_t)(quotient_7 + top_cur_x));
                            top_cross_y_addr = (uint16_t)(top_out_ptr + 2);
                            fx_stw(top_cross_y_addr, top_edge_pos_out);
                            top_cross_u_addr = (uint16_t)(top_cross_y_addr + 2);
                            top_next_u_addr = (uint16_t)(top_vertex_ptr_init + 4);
                            top_next_u = fx_ldw(top_next_u_addr);
                            top_cur_u_addr = (uint16_t)(top_next_u_addr - 8);
                            top_cur_u = fx_ldw(top_cur_u_addr);
                            top_cur_v_addr = (uint16_t)(top_cur_u_addr + 2);
                            model_96 = fx_ldw(FX_MODEL_96);
                            top_u_prod = (uint32_t)((int32_t)(int16_t)model_96 * (int16_t)(uint16_t)(top_next_u - top_cur_u));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_u_prod, (uint16_t)(top_u_prod >> 16), top_y_span); quotient_8 = r_.quotient; };
                            fx_stw(top_cross_u_addr, (uint16_t)(quotient_8 + top_cur_u));
                            top_cross_v_addr = (uint16_t)(top_cross_u_addr + 2);
                            top_cur_v = fx_ldw(top_cur_v_addr);
                            top_next_v_addr = (uint16_t)(top_cur_v_addr + 8);
                            top_next_v = fx_ldw(top_next_v_addr);
                            top_vertex_ptr_init = (uint16_t)(top_next_v_addr - 6);
                            model_96_2 = fx_ldw(FX_MODEL_96);
                            top_v_prod = (uint32_t)((int32_t)(int16_t)model_96_2 * (int16_t)(uint16_t)(top_next_v - top_cur_v));
                            { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)top_v_prod, (uint16_t)(top_v_prod >> 16), top_y_span); quotient_9 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                            fx_stw(top_cross_v_addr, (uint16_t)(quotient_9 + top_cur_v));
                            top_out_ptr = (uint16_t)(top_cross_v_addr + 2);
                        } else {
                            fx_stw(top_out_ptr, top_cur_x);
                            top_keep_y_addr = (uint16_t)(top_out_ptr + 2);
                            fx_stw(top_keep_y_addr, top_cur_y);
                            top_keep_u_addr = (uint16_t)(top_keep_y_addr + 2);
                            top_keep_src_u_addr = (uint16_t)(top_vertex_ptr_init - 4);
                            top_keep_src_u = fx_ldw(top_keep_src_u_addr);
                            top_keep_src_v_addr = (uint16_t)(top_keep_src_u_addr + 2);
                            fx_stw(top_keep_u_addr, top_keep_src_u);
                            top_keep_v_addr = (uint16_t)(top_keep_u_addr + 2);
                            top_keep_src_v = fx_ldw(top_keep_src_v_addr);
                            top_vertex_ptr_init = (uint16_t)(top_keep_src_v_addr + 2);
                            fx_stw(top_keep_v_addr, top_keep_src_v);
                            top_out_ptr = (uint16_t)(top_keep_v_addr + 2);
                        }
                    }
                    top_vertices_left_load = fx_ldw(0x0040);
                    top_vertices_left_dec = (uint16_t)(top_vertices_left_load - 1);
                    fx_sbk(top_vertices_left_dec);
                    if (top_vertices_left_dec == 0) {
                        break;
                    } else {
                        continue;
                    }
                }
                vertex_count_save_9 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                top_extra_vertices = (uint16_t)(vertex_count_save_9 - 3);
                if (!(top_extra_vertices & 0x8000)) {
                    fx_stw(0x0040, (uint16_t)(top_extra_vertices + 3));
                    bottom_out_ptr = fx_ldw(0x005A);
                    bottom_first_x = fx_ldw(0x0688);
                    bottom_first_y_addr = (uint16_t)(0 + 0x068A);
                    bottom_first_y = fx_ldw(bottom_first_y_addr);
                    bottom_first_u_addr = (uint16_t)(bottom_first_y_addr + 2);
                    bottom_first_u = fx_ldw(bottom_first_u_addr);
                    bottom_first_v_addr = (uint16_t)(bottom_first_u_addr + 2);
                    bottom_first_v = fx_ldw(bottom_first_v_addr);
                    bottom_vertex_ptr_init = (uint16_t)(bottom_first_v_addr - 6);
                    bottom_vertex_count_load = fx_ldw(0x0040);
                    bottom_closing_x_addr = (uint16_t)((uint16_t)((int8_t)bottom_vertex_count_load * (int8_t)8) + bottom_vertex_ptr_init);
                    fx_stw(bottom_closing_x_addr, bottom_first_x);
                    bottom_closing_y_addr = (uint16_t)(bottom_closing_x_addr + 2);
                    fx_stw(bottom_closing_y_addr, bottom_first_y);
                    bottom_closing_u_addr = (uint16_t)(bottom_closing_y_addr + 2);
                    fx_stw(bottom_closing_u_addr, bottom_first_u);
                    fx_stw((uint16_t)(bottom_closing_u_addr + 2), bottom_first_v);
                    for (;;) {
                        bottom_cur_x = fx_ldw(bottom_vertex_ptr_init);
                        bottom_cur_y_addr = (uint16_t)(bottom_vertex_ptr_init + 2);
                        bottom_cur_y = fx_ldw(bottom_cur_y_addr);
                        bottom_next_x_addr = (uint16_t)(bottom_cur_y_addr + 6);
                        bottom_next_x = fx_ldw(bottom_next_x_addr);
                        bottom_next_y_addr = (uint16_t)(bottom_next_x_addr + 2);
                        divisor_made = fx_ldw(bottom_next_y_addr);
                        bottom_vertex_ptr_init = (uint16_t)(bottom_next_y_addr - 2);
                        bottom_edge_pos_a = fx_ldw(0x003E);
                        bottom_y_diff_a = (uint16_t)(bottom_cur_y - bottom_edge_pos_a);
                        if (((((uint16_t)(bottom_y_diff_a - 1) & 0x8000) != 0) == (((bottom_y_diff_a ^ 1) & (bottom_y_diff_a ^ (uint16_t)(bottom_y_diff_a - 1 - 0)) & 0x8000) != 0))) {
                            bottom_edge_pos_b = fx_ldw(0x003E);
                            bottom_y_diff_b = (uint16_t)(divisor_made - bottom_edge_pos_b);
                            if (((((uint16_t)(bottom_y_diff_b - 1) & 0x8000) != 0) == (((bottom_y_diff_b ^ 1) & (bottom_y_diff_b ^ (uint16_t)(bottom_y_diff_b - 1 - 0)) & 0x8000) != 0))) {
                                vertex_count_save_10 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                fx_sbk((uint16_t)(vertex_count_save_10 - 1));
                            } else {
                                bottom_y_span = (uint16_t)(divisor_made - bottom_cur_y);
                                bottom_edge_pos_d = fx_ldw(0x003E);
                                bottom_edge_offset = (uint16_t)(bottom_edge_pos_d - bottom_cur_y);
                                fx_stw(FX_MODEL_96, bottom_edge_offset);
                                bottom_x_prod = (uint32_t)((int32_t)(int16_t)bottom_edge_offset * (int16_t)(uint16_t)(bottom_next_x - bottom_cur_x));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_x_prod, (uint16_t)(bottom_x_prod >> 16), bottom_y_span); quotient_10 = r_.quotient; };
                                bottom_edge_pos_out = fx_ldw(0x003E);
                                fx_stw(bottom_out_ptr, (uint16_t)(quotient_10 + bottom_cur_x));
                                bottom_cross_y_addr = (uint16_t)(bottom_out_ptr + 2);
                                fx_stw(bottom_cross_y_addr, bottom_edge_pos_out);
                                bottom_cross_u_addr = (uint16_t)(bottom_cross_y_addr + 2);
                                bottom_next_u_addr = (uint16_t)(bottom_vertex_ptr_init + 4);
                                bottom_next_u = fx_ldw(bottom_next_u_addr);
                                bottom_cur_u_addr = (uint16_t)(bottom_next_u_addr - 8);
                                bottom_cur_u = fx_ldw(bottom_cur_u_addr);
                                bottom_cur_v_addr = (uint16_t)(bottom_cur_u_addr + 2);
                                model_96_3 = fx_ldw(FX_MODEL_96);
                                bottom_u_prod = (uint32_t)((int32_t)(int16_t)model_96_3 * (int16_t)(uint16_t)(bottom_next_u - bottom_cur_u));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_u_prod, (uint16_t)(bottom_u_prod >> 16), bottom_y_span); quotient_11 = r_.quotient; };
                                fx_stw(bottom_cross_u_addr, (uint16_t)(quotient_11 + bottom_cur_u));
                                bottom_cross_v_addr = (uint16_t)(bottom_cross_u_addr + 2);
                                bottom_cur_v = fx_ldw(bottom_cur_v_addr);
                                bottom_next_v_addr = (uint16_t)(bottom_cur_v_addr + 8);
                                bottom_next_v = fx_ldw(bottom_next_v_addr);
                                bottom_vertex_ptr_init = (uint16_t)(bottom_next_v_addr - 6);
                                model_96_4 = fx_ldw(FX_MODEL_96);
                                bottom_v_prod = (uint32_t)((int32_t)(int16_t)model_96_4 * (int16_t)(uint16_t)(bottom_next_v - bottom_cur_v));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_v_prod, (uint16_t)(bottom_v_prod >> 16), bottom_y_span); quotient_12 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                                fx_stw(bottom_cross_v_addr, (uint16_t)(quotient_12 + bottom_cur_v));
                                bottom_out_ptr = (uint16_t)(bottom_cross_v_addr + 2);
                            }
                        } else {
                            bottom_edge_pos_c = fx_ldw(0x003E);
                            bottom_y_diff_c = (uint16_t)(divisor_made - bottom_edge_pos_c);
                            if (((((uint16_t)(bottom_y_diff_c - 1) & 0x8000) != 0) == (((bottom_y_diff_c ^ 1) & (bottom_y_diff_c ^ (uint16_t)(bottom_y_diff_c - 1 - 0)) & 0x8000) != 0))) {
                                vertex_count_save_11 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                                fx_sbk((uint16_t)(vertex_count_save_11 + 1));
                                fx_stw(bottom_out_ptr, bottom_cur_x);
                                bottom_emit_y_addr = (uint16_t)(bottom_out_ptr + 2);
                                fx_stw(bottom_emit_y_addr, bottom_cur_y);
                                bottom_emit_u_addr = (uint16_t)(bottom_emit_y_addr + 2);
                                bottom_emit_src_u_addr = (uint16_t)(bottom_vertex_ptr_init - 4);
                                bottom_emit_src_u = fx_ldw(bottom_emit_src_u_addr);
                                bottom_emit_src_v_addr = (uint16_t)(bottom_emit_src_u_addr + 2);
                                fx_stw(bottom_emit_u_addr, bottom_emit_src_u);
                                bottom_emit_v_addr = (uint16_t)(bottom_emit_u_addr + 2);
                                bottom_emit_src_v = fx_ldw(bottom_emit_src_v_addr);
                                bottom_vertex_ptr_init = (uint16_t)(bottom_emit_src_v_addr + 2);
                                fx_stw(bottom_emit_v_addr, bottom_emit_src_v);
                                bottom_out_ptr = (uint16_t)(bottom_emit_v_addr + 2);
                                bottom_y_span = (uint16_t)(divisor_made - bottom_cur_y);
                                bottom_edge_pos_d = fx_ldw(0x003E);
                                bottom_edge_offset = (uint16_t)(bottom_edge_pos_d - bottom_cur_y);
                                fx_stw(FX_MODEL_96, bottom_edge_offset);
                                bottom_x_prod = (uint32_t)((int32_t)(int16_t)bottom_edge_offset * (int16_t)(uint16_t)(bottom_next_x - bottom_cur_x));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_x_prod, (uint16_t)(bottom_x_prod >> 16), bottom_y_span); quotient_10 = r_.quotient; };
                                bottom_edge_pos_out = fx_ldw(0x003E);
                                fx_stw(bottom_out_ptr, (uint16_t)(quotient_10 + bottom_cur_x));
                                bottom_cross_y_addr = (uint16_t)(bottom_out_ptr + 2);
                                fx_stw(bottom_cross_y_addr, bottom_edge_pos_out);
                                bottom_cross_u_addr = (uint16_t)(bottom_cross_y_addr + 2);
                                bottom_next_u_addr = (uint16_t)(bottom_vertex_ptr_init + 4);
                                bottom_next_u = fx_ldw(bottom_next_u_addr);
                                bottom_cur_u_addr = (uint16_t)(bottom_next_u_addr - 8);
                                bottom_cur_u = fx_ldw(bottom_cur_u_addr);
                                bottom_cur_v_addr = (uint16_t)(bottom_cur_u_addr + 2);
                                model_96_3 = fx_ldw(FX_MODEL_96);
                                bottom_u_prod = (uint32_t)((int32_t)(int16_t)model_96_3 * (int16_t)(uint16_t)(bottom_next_u - bottom_cur_u));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_u_prod, (uint16_t)(bottom_u_prod >> 16), bottom_y_span); quotient_11 = r_.quotient; };
                                fx_stw(bottom_cross_u_addr, (uint16_t)(quotient_11 + bottom_cur_u));
                                bottom_cross_v_addr = (uint16_t)(bottom_cross_u_addr + 2);
                                bottom_cur_v = fx_ldw(bottom_cur_v_addr);
                                bottom_next_v_addr = (uint16_t)(bottom_cur_v_addr + 8);
                                bottom_next_v = fx_ldw(bottom_next_v_addr);
                                bottom_vertex_ptr_init = (uint16_t)(bottom_next_v_addr - 6);
                                model_96_4 = fx_ldw(FX_MODEL_96);
                                bottom_v_prod = (uint32_t)((int32_t)(int16_t)model_96_4 * (int16_t)(uint16_t)(bottom_next_v - bottom_cur_v));
                                { FxDivideSignedResult r_ = FxDivideSigned((uint16_t)bottom_v_prod, (uint16_t)(bottom_v_prod >> 16), bottom_y_span); quotient_12 = r_.quotient; divisor_made = r_.divisor_made; quotient_sign_in = r_.sign_word; };
                                fx_stw(bottom_cross_v_addr, (uint16_t)(quotient_12 + bottom_cur_v));
                                bottom_out_ptr = (uint16_t)(bottom_cross_v_addr + 2);
                            } else {
                                fx_stw(bottom_out_ptr, bottom_cur_x);
                                bottom_keep_y_addr = (uint16_t)(bottom_out_ptr + 2);
                                fx_stw(bottom_keep_y_addr, bottom_cur_y);
                                bottom_keep_u_addr = (uint16_t)(bottom_keep_y_addr + 2);
                                bottom_keep_src_u_addr = (uint16_t)(bottom_vertex_ptr_init - 4);
                                bottom_keep_src_u = fx_ldw(bottom_keep_src_u_addr);
                                bottom_keep_src_v_addr = (uint16_t)(bottom_keep_src_u_addr + 2);
                                fx_stw(bottom_keep_u_addr, bottom_keep_src_u);
                                bottom_keep_v_addr = (uint16_t)(bottom_keep_u_addr + 2);
                                bottom_keep_src_v = fx_ldw(bottom_keep_src_v_addr);
                                bottom_vertex_ptr_init = (uint16_t)(bottom_keep_src_v_addr + 2);
                                fx_stw(bottom_keep_v_addr, bottom_keep_src_v);
                                bottom_out_ptr = (uint16_t)(bottom_keep_v_addr + 2);
                            }
                        }
                        bottom_vertices_left_load = fx_ldw(0x0040);
                        bottom_vertices_left_dec = (uint16_t)(bottom_vertices_left_load - 1);
                        fx_sbk(bottom_vertices_left_dec);
                        if (bottom_vertices_left_dec == 0) {
                            break;
                        } else {
                            continue;
                        }
                    }
                    vertex_count_save_12 = fx_ldw(FX_VERTEX_COUNT_SAVE);
                    bottom_extra_vertices = (uint16_t)(vertex_count_save_12 - 3);
                    if (!(bottom_extra_vertices & 0x8000)) {
                        fx_unspill(stack);
                        return (FxPolygonClipScreenUVResult){(uint16_t)(bottom_extra_vertices + 3), divisor_made, quotient_sign_in};
                    } else {
                        run_l9EFB = 1;
                    }
                } else {
                    run_l9EFB = 1;
                }
            } else {
                run_l9EFB = 1;
            }
        } else {
            run_l9EFB = 1;
        }
    } else {
        run_l9EFB = 1;
    }
    fx_unspill(stack);
    return (FxPolygonClipScreenUVResult){0, divisor_made, quotient_sign_in};
}


/*
 * @routine 01:AB11 FxReturnR11
 * WHAT IT DOES: A routine of one JMP R11: returns at once (not reached
 * in the traces so far).
 * HOW: JMP R11; NOP.
 * Entry: R11 = return address.
 * Exit: as the code leaves the registers, flags and RAM; R14 and the
 * ROM buffer move as on the Super FX.
 */
void FxReturnR11(void)
{

    return;
}

