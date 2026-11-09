/*
 * dispatch.c: the Super FX's jumps through a register to one of a known
 * set (fxentry.txt), and code the asm leaves untagged that is called as a
 * routine; written by the generator.
 */

#include "fx/fx.h"

#include <stdio.h>
#include <stdlib.h>

void fx_bad_dispatch(uint16_t at)
{
    fprintf(stderr, "Super FX: a jump to $%04X, none of its known places\n", at);
    abort();
}

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

FxModelNextResult FxModelNext(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at, uint16_t command_code)
{

    switch (command_code) {
    case 0x0000:
        { FxModelEndResult tr_ = FxModelEnd(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.model_state_r6, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0004:
        { FxModelPoints01Result tr_ = FxModelPoints01(stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.last_matrix_word, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0008:
        { FxModelPointsFullResult tr_ = FxModelPointsFull(stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x000C:
        { FxModelProjectResult tr_ = FxModelProject(stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0010:
        { FxModelDepthSortResult tr_ = FxModelDepthSort(draw_list_start, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.deepest_depth_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0014:
        { FxModelFacesResult tr_ = FxModelFaces(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.list_write_ptr_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0018:
        { FxModelReturnResult tr_ = FxModelReturn(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x001C:
        { FxModelFramePickResult tr_ = FxModelFramePick(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0020:
        { FxModelJumpResult tr_ = FxModelJump(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0024:
        { FxModelReturnResult tr_ = FxModelReturn(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0028:
        { FxModelIfSkipResult tr_ = FxModelIfSkip(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x002C:
        { FxModelReturnResult tr_ = FxModelReturn(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0030:
        { FxModelFacingResult tr_ = FxModelFacing(stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.cross_term_high_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0034:
        { FxModelPointsScaledResult tr_ = FxModelPointsScaled(draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0038:
        { FxModelPointsMirroredResult tr_ = FxModelPointsMirrored(stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x003C:
        { FxModelListStart3CResult tr_ = FxModelListStart3C(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0040:
        { FxModelReturn40Result tr_ = FxModelReturn40(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0044:
        { FxModelPushReturnResult tr_ = FxModelPushReturn(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0048:
        { FxModelReturnResult tr_ = FxModelReturn(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x004C:
        { FxModelPointsFullResult tr_ = FxModelPointsFull(stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0050:
        { FxModelSpriteResult tr_ = FxModelSprite(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0054:
        { FxModelIfSpriteResult tr_ = FxModelIfSprite(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0058:
        { FxModelListStartResult tr_ = FxModelListStart(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x005C:
        { FxModelIfCallResult tr_ = FxModelIfCall(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0060:
        { FxModelCallResult tr_ = FxModelCall(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.prev_end_y, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    case 0x0064:
        { FxModelReturnPullResult tr_ = FxModelReturnPull(draw_list_start, draw_list_end, stack, rom_at); return (FxModelNextResult){tr_.draw_list_start_out, tr_.draw_list_end_out, tr_.rom_at_out, tr_.up ? tr_.up == 0 ? 0 : tr_.up - 0 : 0}; }
    default:
        fx_bad_dispatch(command_code);
    }
}

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
