/*
 * fx.h: the Super FX logic's routines and names.
 */
#ifndef STUNTRACEFX_FX_H
#define STUNTRACEFX_FX_H

#include <stdint.h>
#include "fx/fx_rt.h"

/* DIV2: half, rounded down, except that -1 halves to 0. */
static inline uint16_t fx_div2(uint16_t a)
{
    return a == 0xFFFF ? 0 : (uint16_t)((int16_t)a >> 1);
}

/* a dispatch reached with a place it does not know: fatal */
_Noreturn void fx_bad_dispatch(uint16_t at);

/* the port's renderer calls (render/fx_ops.c, fxhooks.txt) */

#define FXCAR_0A 0x000A
#define FXCAR_44 0x0044
#define FXCAR_64 0x0064
#define FXCAR_8C 0x008C
#define FXCAR_ENTRY 0x006E
#define FXCAR_IMPACT 0x0062
#define FXCAR_PARTS 0x003E
#define FXCAR_PITCH 0x0024
#define FXCAR_ROLL 0x0028
#define FXCAR_SCRIPT 0x0066
#define FXCAR_STEER_BIAS 0x003A
#define FXCAR_STUCK 0x007E
#define FXCAR_YAW 0x0026
#define FXCON_OTHER 0x000C
#define FXHIT_VELOCITY 0x0004
#define FXHIT_X 0x0008
#define FXPART_12 0x0012
#define FXPART_3C 0x003C
#define FXPART_46 0x0046
#define FXPART_50 0x0050
#define FXPART_NEXT 0x0034
#define FXPART_WORLD 0x0036
#define FXREC_6A 0x006A
#define FXREC_82 0x0082
#define FXREC_ALIGN 0x006C
#define FXREC_ALL_NEXT 0x0004
#define FXREC_ALL_PREV 0x0006
#define FXREC_COUNTED 0x0086
#define FXREC_DIRECTION 0x0070
#define FXREC_FLAGS10 0x0010
#define FXREC_FLAGS16 0x0016
#define FXREC_FLAGS1C 0x001C
#define FXREC_FLAGS40 0x0040
#define FXREC_HALF_X 0x001E
#define FXREC_LAPS 0x007A
#define FXREC_LINK_422 0x0026
#define FXREC_LIST 0x0018
#define FXREC_MASS 0x002A
#define FXREC_MODE 0x001A
#define FXREC_NEXT 0x000E
#define FXREC_NEXT_CAR 0x0074
#define FXREC_POOL 0x0008
#define FXREC_RANK 0x0088
#define FXREC_SCALED_X 0x002E
#define FXREC_SEGMENT 0x0058
#define FXREC_SIDES 0x0042
#define FXREC_SIZE 0x000C
#define FXREC_SPEED 0x004A
#define FXREC_SPREAD 0x007C
#define FXREC_TO_GO 0x0084
#define FXREC_VELOCITY 0x002E
#define FXREC_Y 0x0020
#define FXREC_YAW_RATE 0x0034
#define FXREC_Y_HELD 0x0060
#define FX_ATAN_TABLE 0x9E40
#define FX_CARS 0x262E
#define FX_CAR_FRAME 0x2394
#define FX_CAR_LAST 0x2474
#define FX_CAR_POS 0x2354
#define FX_CLIP_END1_ATTR2 0x008E
#define FX_CLIP_END2_ATTR1 0x008C
#define FX_CLIP_KEPT_X 0x009A
#define FX_CLIP_KEPT_Y 0x009C
#define FX_CLIP_KEPT_Z 0x009E
#define FX_CLIP_STACK 0x00A0
#define FX_COLLIDE_MODEL 0x2486
#define FX_CONTACT_BYTES 0x0012
#define FX_CONTACT_DEPTH 0x237E
#define FX_CONTACT_FORCE 0x2386
#define FX_CONTACT_FORCE_SIDE 0x239E
#define FX_CONTACT_FORCE_UP 0x23A0
#define FX_CONTACT_SUM 0x2388
#define FX_DRIVE_LEVEL 0x26F4
#define FX_EFFECT_MAX 0x26EE
#define FX_FACE_FLAGS 0x237C
#define FX_FACE_NORMAL 0x2372
#define FX_FACE_PLANE 0x237A
#define FX_FACE_SCALE 0x2384
#define FX_GAUGE_LEFT 0x01CA
#define FX_GAUGE_RIGHT 0x01CE
#define FX_GAUGE_TOP 0x01AC
#define FX_GAUGE_TOP_INNER 0x01AE
#define FX_HAZE_X8 0x26F8
#define FX_HIT 0x2334
#define FX_HIT_DEPTH 0x239C
#define FX_HIT_ON 0x263C
#define FX_HIT_SCALE 0x2678
#define FX_HIT_SUM 0x263E
#define FX_INIT_2380 0x2380
#define FX_INIT_2382 0x2382
#define FX_INIT_2470 0x2470
#define FX_INIT_2472 0x2472
#define FX_INIT_2636 0x2636
#define FX_INIT_2684 0x2684
#define FX_INIT_2686 0x2686
#define FX_INIT_2B92 0x2B92
#define FX_INIT_2B94 0x2B94
#define FX_LEFT_DU 0x0078
#define FX_LEFT_DV 0x0084
#define FX_LEFT_U_PREV 0x0074
#define FX_LEFT_V 0x007C
#define FX_LEFT_V_PREV 0x0080
#define FX_LIGHT_LOCAL_X 0x0106
#define FX_LIGHT_LOCAL_Y 0x0108
#define FX_LIGHT_LOCAL_Z 0x010A
#define FX_LIGHT_X 0x0100
#define FX_LIGHT_Y 0x0102
#define FX_LIGHT_Z 0x0104
#define FX_LIST_422 0x2398
#define FX_LIST_END 0x005C
#define FX_LOCAL_SUM 0x2346
#define FX_MATERIAL_MASK 0x0046
#define FX_MATRIX_PACKED_0 0x0128
#define FX_MATRIX_PACKED_1 0x012A
#define FX_MATRIX_PACKED_2 0x012C
#define FX_MATRIX_PACKED_3 0x012E
#define FX_MATRIX_PACKED_4 0x0130
#define FX_MATRIX_W 0x00E4
#define FX_ME_VZ 0x2700
#define FX_ME_Z 0x26FA
#define FX_MODEL_118 0x0118
#define FX_MODEL_11E 0x011E
#define FX_MODEL_124 0x0124
#define FX_MODEL_148 0x0148
#define FX_MODEL_14E 0x014E
#define FX_MODEL_150 0x0150
#define FX_MODEL_1B4 0x01B4
#define FX_MODEL_1B6 0x01B6
#define FX_MODEL_96 0x0096
#define FX_MODEL_BA 0x00BA
#define FX_MODEL_BC 0x00BC
#define FX_MODEL_BE 0x00BE
#define FX_MODEL_C8 0x00C8
#define FX_MODEL_CE 0x00CE
#define FX_MODEL_D2 0x00D2
#define FX_MODEL_FLAGS 0x0054
#define FX_MODEL_RETURN_AT 0x0056
#define FX_NEAR_BAND 0x2646
#define FX_NEAR_CAR 0x2648
#define FX_NEAR_SIDE 0x2644
#define FX_NEAR_X 0x264C
#define FX_NEAR_Z 0x264A
#define FX_NOISE_SEED 0x021C
#define FX_OBJECT_KIND 0x0048
#define FX_OBJECT_MATRIX_1 0x00E6
#define FX_OBJECT_MATRIX_2 0x00E8
#define FX_OBJECT_MATRIX_3 0x00EA
#define FX_OBJECT_MATRIX_4 0x00EC
#define FX_OBJECT_MATRIX_5 0x00EE
#define FX_OBJECT_MATRIX_6 0x00F0
#define FX_OBJECT_MATRIX_7 0x00F2
#define FX_OBJECT_MATRIX_8 0x00F4
#define FX_OUTCODES 0x014A
#define FX_PARAM_BOUNCE 0x2656
#define FX_PARAM_CURVE_A 0x267A
#define FX_PARAM_CURVE_B 0x267C
#define FX_PARAM_GRIP_AHEAD 0x2676
#define FX_PARAM_GRIP_FREE 0x266C
#define FX_PARAM_GRIP_SIDE 0x2674
#define FX_PARAM_GRIP_SLIDE 0x266E
#define FX_PARAM_HIT 0x2652
#define FX_PARAM_JUMP 0x2682
#define FX_PARAM_LIFT 0x2672
#define FX_PARAM_PUSH 0x2654
#define FX_PARAM_REVERSE 0x265C
#define FX_PARAM_SLIDE_LIMIT 0x2680
#define FX_PARAM_SPEED_CAP 0x267E
#define FX_PARAM_SPIN_DAMP 0x2670
#define FX_PARAM_THRUST 0x265A
#define FX_PARAM_THRUST_BOOST 0x2658
#define FX_PARAM_TURN_GRIP 0x2660
#define FX_PARAM_WHEEL_48 0x265E
#define FX_PART_BYTES 0x0052
#define FX_PART_Z_SPEED 0x235E
#define FX_PHYSICS_BUSY 0x246E
#define FX_POINTS_TOTAL 0x0144
#define FX_POINTS_W 0x2488
#define FX_POINT_COUNT 0x0146
#define FX_POOL_FIRST 0x1098
#define FX_POOL_SECOND 0x1D70
#define FX_PUSH 0x23A4
#define FX_PUSHES 0x23AE
#define FX_PUSH_AMOUNT 0x2378
#define FX_PUSH_FLAGS 0x2480
#define FX_PUSH_TURN 0x247E
#define FX_RAM_TEST 0x235C
#define FX_RAM_TEST_W 0x235C
#define FX_RECORDS 0x2332
#define FX_RIGHT_DV 0x0086
#define FX_RIGHT_SLOPE 0x006E
#define FX_RIGHT_U 0x0072
#define FX_RIGHT_U_PREV 0x0076
#define FX_RIGHT_V 0x007E
#define FX_SHADE_AT 0x0052
#define FX_SHADE_TABLE 0x004C
#define FX_SINE_TABLE 0x983E
#define FX_SIZES 0x236C
#define FX_SPIN 0x233A
#define FX_SPRITE_CLIP_W 0x00B4
#define FX_SPRITE_CLIP_X 0x00B2
#define FX_SPRITE_COLOUR 0x00AC
#define FX_SPRITE_MATERIAL 0x00D6
#define FX_SPRITE_SIZE 0x00AA
#define FX_SPRITE_X 0x01D2
#define FX_SPRITE_Y 0x01D4
#define FX_STACK 0x0240
#define FX_STEER_BACK 0x266A
#define FX_STEER_MAX 0x2666
#define FX_STEER_MAX2 0x2668
#define FX_STEER_RATE 0x2662
#define FX_STEER_RATE2 0x2664
#define FX_TEXT_BANK 0x2734
#define FX_TEXT_X 0x2738
#define FX_TEXT_Y 0x273A
#define FX_TEX_BANK 0x00A6
#define FX_TEX_BASE 0x00A8
#define FX_TEX_MASK 0x00D4
#define FX_TEX_U_OFF 0x00C0
#define FX_TEX_V_OFF 0x00C2
#define FX_TRAFFIC_CLOSE 0x2638
#define FX_TRAFFIC_DIST 0x263A
#define FX_VERTEX_COUNT_SAVE 0x0098
#define FX_VIEW_MATRIX_0 0x0132
#define FX_VIEW_MATRIX_1 0x0134
#define FX_VIEW_MATRIX_2 0x0136
#define FX_VIEW_MATRIX_3 0x0138
#define FX_VIEW_MATRIX_4 0x013A
#define FX_VIEW_MATRIX_5 0x013C
#define FX_VIEW_MATRIX_6 0x013E
#define FX_VIEW_MATRIX_7 0x0140
#define FX_VIEW_MATRIX_8 0x0142
#define FX_WEATHER_COLOUR 0x26FE
#define FX_WEATHER_DRIFT 0x2B90
#define FX_WEATHER_TURN 0x2B8E
#define FX_WHEEL_AHEAD 0x236A
#define FX_WHEEL_CONTACT 0x2640
#define FX_WHEEL_FLAGS 0x2484
#define FX_WHEEL_LOAD 0x2364
#define FX_WHEEL_SIDE 0x2366
#define FX_WORLD_SUM 0x2340
#define GSU_DRAW_LIST 0x023C
#define GSU_IN_A2 0x00A2
#define GSU_IN_A4 0x00A4
#define GSU_IN_AMOUNT 0x2390
#define GSU_MATCH_2478 0x2478
#define GSU_PARAM_00D8 0x00D8
#define GSU_PARAM_00DC 0x00DC
#define GSU_PARAM_00E0 0x00E0
#define GSU_PARAM_108A 0x108A
#define GSU_RECORDS_N 0x01C8
#define GSU_RECORD_ARG 0x238E
#define GSU_SCREEN_START 0x021A
#define GSU_VECTORS 0x0116
#define GSU_VIEW_246C 0x246C
#define GSU_WORD_1C4 0x01C4
#define ROT_COS_TABLE 0x973E
#define ROT_SIN_TABLE 0x96FE

typedef struct { uint16_t touch_pos_result; uint16_t record_z_out; uint16_t last_contact_ptr_out; uint16_t last_contact_flags_out; } FxAllTouchesResult;
typedef struct { uint16_t angle; uint16_t xor_negative; uint16_t swap_marker_out; } FxAngleOfResult;
typedef struct { uint16_t vector_x; uint16_t vector_y; uint16_t vector_z; uint16_t rom_at; } FxAngleVectorResult;
typedef struct { uint16_t angle; uint16_t angle_sign_xor; uint16_t swap_mark_out; } FxBearingToResult;
typedef struct { uint16_t words_4; uint16_t entry_out; uint16_t segment_scratch_a_out; uint16_t segment_scratch_b_out; uint16_t rom_at; } FxCarCatchUpResult;
typedef struct { uint16_t share_before; uint16_t next_entry_end_ptr; uint16_t model_record_out; uint16_t caller_scratch_back; uint16_t record_ptr; uint16_t rom_at_out; int up; } FxCarCollideResult;
typedef struct { uint16_t course_followed; uint16_t last_change_low; uint16_t last_change_high; uint16_t last_scan_value; uint16_t car; uint16_t last_scan_aux; uint16_t rom_at_out; int up; } FxCarFollowCourseResult;
typedef struct { uint16_t course_x_part; uint16_t next_entry_end_ptr; uint16_t pose_aux_out; uint16_t car_out; uint16_t pose_matrix_ptr_out; uint16_t rom_at; int up; } FxCarHandlerResult;
typedef struct { uint16_t course_x_part; uint16_t next_entry_end_ptr; uint16_t pose_aux_out; uint16_t car_out; uint16_t pose_matrix_ptr_out; uint16_t rom_at_out; int up; } FxCarHandlerClampResult;
typedef struct { uint16_t distance_x_part_out; uint16_t next_entry_ptr_out; uint16_t pose_aux_out; uint16_t car_out; uint16_t pose_matrix_ptr_out; uint16_t rom_at_out; int up; } FxCarHandlerFlipResult;
typedef struct { uint16_t spread_out; uint16_t car_y_out; uint16_t car_z_out; uint16_t other_car_out; } FxCarHitResult;
typedef struct { uint16_t last_part_velocity_z; uint16_t world_z_partial_sum; uint16_t last_part_ptr; } FxCarMovePartsResult;
typedef struct { int up; } FxCarMovePartsAfterStillResult;
typedef struct { int up; } FxCarMovePartsAfterWalkResult;
typedef struct { uint16_t angle_sign_xor; uint16_t swap_mark_out; uint16_t last_next_rec; uint16_t last_size_addr; uint16_t rom_at_out; } FxCarPlaceOnSegmentResult;
typedef struct { uint16_t last_change_low; uint16_t last_change_high; uint16_t pose_aux; uint16_t last_part_ptr; uint16_t rom_at; } FxCarPoseResult;
typedef struct { uint16_t course_followed; uint16_t result_speed; uint16_t segment_entry_ptr; uint16_t last_scan_value_out; uint16_t car_record_out; uint16_t last_scan_aux_out; uint16_t rom_at_out; int up; } FxCarVelocityFromSegmentResult;
typedef struct { uint16_t clipped_end1_x; uint16_t clipped_end1_y; uint16_t clipped_end1_z; uint16_t clipped_end2_x; uint16_t clipped_end2_y; uint16_t clipped_end2_z; } FxClipNearResult;
typedef struct { uint16_t cut_x; uint16_t cut_y; uint16_t cut_z; uint16_t kept_x; uint16_t kept_y; uint16_t kept_z; } FxClipXPlaneResult;
typedef struct { uint16_t clipped_a_x; uint16_t clipped_a_y; uint16_t clipped_a_z; uint16_t clipped_b_x; uint16_t clipped_b_y; uint16_t clipped_b_z; } FxClipYPlaneResult;
typedef struct { uint16_t list_face; uint16_t quotient; uint16_t place_y_out; uint16_t model_record_out; uint16_t caller_scratch_back; uint16_t record_back; uint16_t rom_at_out; int up; } FxCollideModelResult;
typedef struct { uint16_t contact_found; uint16_t quotient; uint16_t place_y_out; uint16_t face_scale_out; uint16_t exit_aux_record; uint16_t exit_record; uint16_t rom_at_out; int up; } FxCollideModelDepthCompareResult;
typedef struct { int up; } FxCollideModelSkipFaceResult;
typedef struct { int up; } FxCollideModelUnusedResult;
typedef struct { int up; } FxCollideModelUnusedStoreResult;
typedef struct { uint16_t share_before; uint16_t place_y; } FxCollidePushResult;
typedef struct { uint16_t contact; uint16_t contact_flag_out; } FxContactNoteResult;
typedef struct { uint16_t next_slot_ptr; uint16_t push_x_out; uint16_t push_y_out; uint16_t model_record_out; uint16_t rom_at_out; } FxContactResolveResult;
typedef struct { int up; } FxContactResolveNegateResult;
typedef struct { uint16_t any_touch; uint16_t depth; } FxContactsAnyTouchResult;
typedef struct { uint16_t push_flags_out; uint16_t entry_product_lo; uint16_t push_y_out; uint16_t model_record_out; uint16_t rom_at_out; } FxContactsResolveResult;
typedef struct { uint16_t words_1; uint16_t words_2; uint16_t words_3; uint16_t words_4; uint16_t rom_at; } FxCourseEntryReadResult;
typedef struct { uint16_t number_1; uint16_t entry_words_z; uint16_t count; uint16_t entry; uint16_t number_2; uint16_t last_dist_sum; uint16_t rom_at; } FxCourseNearestResult;
typedef struct { uint16_t distance; uint16_t x_part; uint16_t next_entry_end_ptr; uint16_t rom_at_out; } FxCourseNextDistanceResult;
typedef struct { uint16_t number_1; uint16_t words_1; uint16_t words_3; uint16_t words_4; uint16_t entry_out; uint16_t rom_at_out; } FxCourseNextEntryResult;
typedef struct { uint16_t number_1; uint16_t words_1; uint16_t words_3; uint16_t words_4; uint16_t entry_out; uint16_t rom_at; } FxCoursePrevEntryResult;
typedef struct { uint16_t quotient; uint16_t divisor_made; uint16_t sign_word; } FxDivideSignedResult;
typedef struct { uint16_t right_x; uint16_t next_row_y; } FxDrawBitmap120x16Result;
typedef struct { uint16_t end_x; uint16_t end_row_y; } FxDrawBitmap120x24Result;
typedef struct { uint16_t right_x; uint16_t bottom_row_y; } FxDrawBitmap120x32Result;
typedef struct { uint16_t end_x; uint16_t next_y; } FxDrawBitmap16x24Result;
typedef struct { uint16_t end_x; uint16_t end_row_y; } FxDrawBitmap16x32Result;
typedef struct { uint16_t right_x; uint16_t final_row_y; } FxDrawBitmap64x16Result;
typedef struct { uint16_t right_x; uint16_t next_row_y; } FxDrawBitmap8x16Result;
typedef struct { uint16_t caller_list_end_out; uint16_t caller_write_ptr_out; uint16_t rom_at_out; } FxDrawListResult;
typedef struct { uint16_t angle_sign_xor; uint16_t swap_mark; } FxDriveDecideResult;
typedef struct { int up; } FxEffectsAddKindsResult;
typedef struct { int up; } FxEffectsAddNextResult;
typedef struct { int up; } FxEffectsAddUnusedResult;
typedef struct { int up; } FxFrameProgramResult;
typedef struct { uint16_t quotient; uint16_t place_y_out; uint16_t high_words_hi; } FxHitAddUnitResult;
typedef struct { uint16_t segment_out; uint16_t rom_at_out; } FxLapSegmentResult;
typedef struct { uint16_t last_next_rec; uint16_t last_size_addr; } FxListSweepResult;
typedef struct { uint16_t s1; uint16_t c2; uint16_t s2; uint16_t rom_at; } FxMatrixFromAnglesResult;
typedef struct { uint16_t y_z; uint16_t where_store_out; } FxMatrixFromAngles01Result;
typedef struct { uint16_t out_r9; uint16_t out_r11; } FxMatrixFromAnglesProgramResult;
typedef struct { uint16_t last_result; uint16_t out_r1; uint16_t out_r2; uint16_t out_r3; uint16_t first_two; uint16_t matrix_last; } FxMatrixRotateProgramResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelCallResult;
typedef struct { int up; } FxModelCommandsResult;
typedef struct { uint16_t prev_end_y; uint16_t deepest_depth_out; uint16_t rom_at_out; int up; } FxModelDepthSortResult;
typedef struct { uint16_t draw_list_end_out; int up; } FxModelDrawResult;
typedef struct { uint16_t draw_list_end_out; int up; } FxModelDrawFlaggedResult;
typedef struct { uint16_t draw_list_end_out; int up; } FxModelDrawFlags8000Result;
typedef struct { uint16_t draw_list_end_out; int up; } FxModelDrawWithFlagsResult;
typedef struct { uint16_t model_state_r6; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelEndResult;
typedef struct { uint16_t prev_end_y; uint16_t list_write_ptr_out; uint16_t rom_at_out; int up; } FxModelFacesResult;
typedef struct { int up; } FxModelFacesGatheredUVResult;
typedef struct { uint16_t divisor_made_out; uint16_t draw_list_end_out; uint16_t rom_at; int up; } FxModelFacesOnScreenUVResult;
typedef struct { uint16_t prev_end_y; uint16_t cross_term_high_out; uint16_t rom_at_out; int up; } FxModelFacingResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelFramePickResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelIfCallResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelIfSkipResult;
typedef struct { uint16_t draw_list_start_out; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelIfSpriteResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelJumpResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelListStartResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelListStart3CResult;
typedef struct { int up; } FxModelMirrorProgramResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelNextResult;
typedef struct { uint16_t prev_end_y; uint16_t last_matrix_word; uint16_t rom_at_out; int up; } FxModelPoints01Result;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelPointsFullResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelPointsMirroredResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelPointsScaledResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelProjectResult;
typedef struct { int up; } FxModelProjectEachResult;
typedef struct { uint16_t prev_end_y; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelProjectRestResult;
typedef struct { uint16_t draw_list_start_out; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelPushReturnResult;
typedef struct { int up; } FxModelRecordProgramResult;
typedef struct { uint16_t draw_list_start_out; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelReturnResult;
typedef struct { uint16_t draw_list_start_out; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelReturn40Result;
typedef struct { uint16_t draw_list_start_out; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelReturnPullResult;
typedef struct { uint16_t draw_list_start_out; uint16_t draw_list_end_out; uint16_t rom_at_out; int up; } FxModelSpriteResult;
typedef struct { uint16_t draw_list_start_out; uint16_t rom_at_out; } FxModelSpriteColourResult;
typedef struct { uint16_t scaled_vector_x; uint16_t scaled_vector_y; uint16_t scaled_vector_z; uint16_t quotient; uint16_t place_y; uint16_t high_words_hi; uint16_t k; } FxNormalizeResult;
typedef struct { uint16_t world_sum_x_out; uint16_t world_sum_y_out; uint16_t push_flags_high_out; uint16_t record_restored_out; uint16_t working_record_out; uint16_t rom_at_out; int up; } FxObjectBounceResult;
typedef struct { uint16_t last_collision_result; uint16_t last_contact_ptr; uint16_t last_other_ptr_out; uint16_t record_restored; uint16_t collided_record; uint16_t rom_at_out; int up; } FxObjectCollideOnlyResult;
typedef struct { uint16_t last_collision_result; uint16_t last_contact_ptr; uint16_t last_other_ptr_out; uint16_t record_restored; uint16_t collided_record; uint16_t rom_at_out; int up; } FxObjectDropResult;
typedef struct { uint16_t world_sum_x_out; uint16_t world_sum_y_out; uint16_t push_flags_high_out; uint16_t record_restored_out; uint16_t working_record_out; uint16_t rom_at_out; int up; } FxObjectFallResult;
typedef struct { uint16_t children_result; uint16_t sweep_result_7; uint16_t sweep_result_9; } FxObjectFreeResult;
typedef struct { uint16_t last_child_ptr; uint16_t last_swept_record_out; uint16_t last_swept_value_out; } FxObjectFreeChildrenResult;
typedef struct { uint16_t pose_cos_out; uint16_t pose_sin_out; uint16_t pose_aux_out; uint16_t record_out; uint16_t pose_matrix_ptr_out; uint16_t rom_at_out; } FxObjectMarkTouchedResult;
typedef struct { uint16_t source_row_accum_out; int up; } FxObjectSpriteResult;
typedef struct { uint16_t last_plot_x; uint16_t final_row; int up; } FxObjectSpriteAtResult;
typedef struct { int up; } FxObjectSpriteProgramResult;
typedef struct { int up; } FxObjectSpritePullResult;
typedef struct { uint16_t plot_end_x; uint16_t plot_end_y; uint16_t row_error_out; int up; } FxObjectSpriteRowsResult;
typedef struct { int up; } FxObjectsDrawResult;
typedef struct { uint16_t quotient; uint16_t push_y_out; uint16_t high_words_hi; uint16_t rom_at_out; } FxPartPullResult;
typedef struct { uint16_t last_low; uint16_t y_row; uint16_t model_record_out; uint16_t car_out; uint16_t last_part; uint16_t rom_at_out; int up; } FxPartsUpdateResult;
typedef struct { int up; } FxPhysicsProgramResult;
typedef struct { uint16_t vector_3; uint16_t rom_at_out; } FxPointsGatherResult;
typedef struct { uint16_t out_vertex_count; uint16_t slope_scale_out; uint16_t end2_z_out; } FxPolygonClipBottomResult;
typedef struct { uint16_t clipped_vertex_count; uint16_t slope_scale_out; uint16_t clipped_end_z; } FxPolygonClipLeftResult;
typedef struct { uint16_t clipped_vertex_count; uint16_t last_in_vertex_ptr; uint16_t clipped_end_z; } FxPolygonClipNearResult;
typedef struct { uint16_t out_vertex_count; uint16_t clip_slope_out; uint16_t end2_z_out; } FxPolygonClipRightResult;
typedef struct { uint16_t clipped_vertex_count; uint16_t prev_end_y_out; uint16_t divide_remainder_out; } FxPolygonClipScreenResult;
typedef struct { uint16_t clipped_vertex_count; uint16_t divisor_made_out; uint16_t quotient_sign; } FxPolygonClipScreenUVResult;
typedef struct { uint16_t new_vertex_count; uint16_t last_slope; uint16_t last_vertex_z; } FxPolygonClipTopResult;
typedef struct { uint16_t count; uint16_t last_clip_slope; uint16_t out_list_end; } FxPolygonDrawResult;
typedef struct { uint16_t final_vertex_count; uint16_t kept_z_out; uint16_t final_src_ptr; } FxPolygonGatherResult;
typedef struct { uint16_t clipped_vertex_count; uint16_t last_in_vertex_ptr; uint16_t vertex_end_ptr; uint16_t rom_at_out; } FxPolygonNearClipResult;
typedef struct { uint16_t count; uint16_t read_last; uint16_t src_ptr_end; uint16_t dst_ptr_end; uint16_t rom_at_out; } FxPolygonProjectResult;
typedef struct { int up; } FxPopReturnC4A5Result;
typedef struct { uint16_t xor; uint16_t screen_position_x; uint16_t screen_position_y; } FxProjectPointResult;
typedef struct { uint16_t screen_position_x; uint16_t screen_position_y; uint16_t size_1; uint16_t scale; } FxProjectScaledResult;
typedef struct { uint16_t outcode_word; uint16_t screen_x; uint16_t screen_y; uint16_t exit_aux_a; uint16_t exit_aux_c; uint16_t exit_aux_b; } FxProjectVertexResult;
typedef struct { uint16_t applied_skipped; uint16_t push_out; uint16_t place_x; uint16_t place_y; uint16_t push_flags_high_out; uint16_t rom_at_out; } FxPushApplyResult;
typedef struct { uint16_t push_x_out; uint16_t push_y_out; uint16_t model_record; } FxPushAt1Result;
typedef struct { uint16_t push_x; uint16_t push_y; uint16_t model_record; } FxPushAt2Result;
typedef struct { uint16_t place_x; uint16_t place_y; uint16_t model_record; } FxPushAt4Result;
typedef struct { uint16_t push_x; uint16_t push_y; uint16_t model_record; } FxPushAt4TwistResult;
typedef struct { uint16_t push_x; uint16_t push_y; uint16_t model_record; } FxPushAtHalfResult;
typedef struct { uint16_t push_x; uint16_t push_y; uint16_t model_record; } FxPushAtQuarterResult;
typedef struct { uint16_t push_x_out; uint16_t push_y_out; uint16_t model_record_out; } FxPushAtVariantResult;
typedef struct { uint16_t place_x; uint16_t place_y; uint16_t push_amount_out; uint16_t rom_at_out; } FxPushesApplyResult;
typedef struct { uint16_t results_x; uint16_t results_y; } FxRatioClampResult;
typedef struct { uint16_t last_product_hi; uint16_t last_product_lo; uint16_t low_word; uint16_t normalize_work_reg_out; } FxRecordAlignmentResult;
typedef struct { uint16_t world_sum_x_out; uint16_t world_sum_y_out; uint16_t push_flags_high_out; uint16_t caller_scratch_back; uint16_t record_out; uint16_t rom_at_out; int up; } FxRecordCollideResult;
typedef struct { uint16_t pose_cos_out; uint16_t pose_sin_out; uint16_t pose_aux_out; uint16_t record_out; uint16_t pose_matrix_ptr_out; uint16_t rom_at_out; int up; } FxRecordHandlerResult;
typedef struct { uint16_t heading_angle; uint16_t angle_sign_xor; uint16_t swap_mark; uint16_t nearest_number_out; uint16_t nearest_dist_x_out; uint16_t rom_at; } FxRecordHeadingAheadResult;
typedef struct { uint16_t segment_number; uint16_t entry_word_z; uint16_t segment_count; uint16_t entry_ptr; uint16_t last_dist_sum; uint16_t rom_at; } FxRecordSegmentResult;
typedef struct { uint16_t record_y; uint16_t record_z_out; uint16_t last_contact_result; uint16_t last_contact_aux; } FxRecordTouchesResult;
typedef struct { uint16_t records_apart; uint16_t normalize_aux_out; } FxRecordsApartResult;
typedef struct { int up; } FxReturnOneRestoreResult;
typedef struct { int up; } FxScaleFromTable14PullResult;
typedef struct { uint16_t cut_x; uint16_t cut_y; uint16_t cut_z; uint16_t kept_x; uint16_t kept_y; uint16_t kept_z; } FxSegmentCrossZResult;
typedef struct { uint16_t spread_value_out; uint16_t car_y_out; uint16_t car_z_out; uint16_t other_car_out; } FxSpreadCarsResult;
typedef struct { uint16_t shade_row_base_out; uint16_t rom_at; } FxSpriteMaterialResult;
typedef struct { uint16_t low_word_out; uint16_t root; } FxSquareRootResult;
typedef struct { uint16_t new_value; uint16_t pose_aux; } FxSteerFromInputResult;
typedef struct { uint16_t world_sum_x; uint16_t world_sum_y; } FxSumsApplyResult;
typedef struct { uint16_t push_x_out; uint16_t push_y_out; } FxSumsPushWorldResult;
typedef struct { uint16_t old_world; uint16_t y_row; } FxSumsRotateLocalResult;
typedef struct { uint16_t last_byte; uint16_t out_r1; int out_rV; int out_rC; } FxTable2702ProgramResult;
typedef struct { uint16_t entry_out; uint16_t address; } FxTableEntryWordsResult;
typedef struct { int up; } FxText3DLeftColumnResult;
typedef struct { int up; } FxText3DRowEndNopResult;
typedef struct { uint16_t end_x; uint16_t end_y; } FxTextDrawResult;
typedef struct { uint16_t text_draw_result; uint16_t notice_car; } FxTextNoticeResult;
typedef struct { uint16_t entry_scratch_b_out; uint16_t last_other_car; uint16_t old; } FxTrafficSidesResult;
typedef struct { uint16_t new_x_x; uint16_t new_z; uint16_t rom_at; } FxTurnResult;
typedef struct { uint16_t moved; uint16_t bit_buffer_lo_out; uint16_t bits; uint16_t rom_at_out; } FxUnpackBitsResult;
typedef struct { uint16_t buffer_hi; uint16_t buffer_lo; uint16_t rom_at_out; int out_rC; } FxUnpackRefillResult;
typedef struct { int up; } FxWeatherDropNextResult;
typedef struct { int up; } FxWeatherUnusedResult;
typedef struct { int up; } FxWeatherUnusedClearResult;
typedef struct { int up; } FxWeatherUnusedFullResult;
typedef struct { int up; } FxWeatherUnusedLoopResult;
typedef struct { uint16_t last_low; uint16_t y_row; uint16_t model_record_out; uint16_t car_out; uint16_t wheel_out; uint16_t rom_at_out; int up; } FxWheelUpdateResult;
typedef struct { int up; } FxWheelUpdateNoSlotResult;

void FxAbove100Scaled(uint16_t value);
FxAllTouchesResult FxAllTouches(uint16_t touch_pos_scratch, uint16_t prior_record_z, uint16_t last_contact_ptr, uint16_t last_contact_flags, uint16_t *stack);
FxAngleOfResult FxAngleOf(uint16_t vector_x, uint16_t vector_y, uint16_t prior_swap_marker, uint16_t *stack, uint16_t rom_at);
void FxAngleProgram(uint16_t rom_at);
FxAngleVectorResult FxAngleVector(uint16_t heading_angle, uint16_t length);
uint16_t FxAtan2(uint16_t vector_x, uint16_t vector_y, uint16_t *stack, uint16_t rom_at);
FxBearingToResult FxBearingTo(uint16_t x, uint16_t z, uint16_t swap_mark, uint16_t record, uint16_t *stack, uint16_t rom_at);
uint16_t FxCarAddPart(uint16_t car_record, uint16_t *stack, uint16_t rom_at);
FxCarCatchUpResult FxCarCatchUp(uint16_t prior_word4, uint16_t entry, uint16_t segment_scratch_a, uint16_t car_record, uint16_t segment_scratch_b, uint16_t *stack);
FxCarCollideResult FxCarCollide(uint16_t place_x, uint16_t place_y, uint16_t model_record, uint16_t car, uint16_t *stack, uint16_t rom_at);
FxCarFollowCourseResult FxCarFollowCourse(uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t segment_scratch_a, uint16_t car_record, uint16_t segment_scratch_b, uint16_t *stack, uint16_t rom_at);
void FxCarFromScript(uint16_t car_record, uint16_t *stack, uint16_t rom_at);
FxCarHandlerResult FxCarHandler(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t car, uint16_t pose_matrix_ptr, uint16_t *stack);
FxCarHandlerClampResult FxCarHandlerClamp(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t car, uint16_t pose_matrix_ptr, uint16_t *stack, uint16_t rom_at);
FxCarHandlerFlipResult FxCarHandlerFlip(uint16_t pose_aux, uint16_t car, uint16_t pose_matrix_ptr, uint16_t *stack, uint16_t rom_at);
FxCarHitResult FxCarHit(uint16_t spread_in, uint16_t car_y_in, uint16_t prior_car_z, uint16_t car_record, uint16_t prior_other_car, uint16_t *stack);
FxCarMovePartsResult FxCarMoveParts(uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t car_record, uint16_t *stack);
FxCarMovePartsAfterStillResult FxCarMovePartsAfterStill(uint16_t *stack);
FxCarMovePartsAfterWalkResult FxCarMovePartsAfterWalk(uint16_t *stack);
void FxCarNew(void);
FxCarPlaceOnSegmentResult FxCarPlaceOnSegment(uint16_t prior_word4, uint16_t swap_mark, uint16_t in_scratch_7, uint16_t car_record, uint16_t in_scratch_9, uint16_t *stack, uint16_t rom_at);
FxCarPoseResult FxCarPose(uint16_t car_record, uint16_t *stack);
FxCarVelocityFromSegmentResult FxCarVelocityFromSegment(uint16_t in_scratch_4, uint16_t in_scratch_5, uint16_t last_scan_value, uint16_t car_record, uint16_t last_scan_aux, uint16_t *stack, uint16_t rom_at);
void FxClear6000IfSet(void);
void FxClearRam200Program(void);
void FxClearScreenIfSet(void);
void FxClearScreenLong(void);
void FxClearScreenShort(void);
FxClipNearResult FxClipNear(uint16_t end1_x, uint16_t end1_y, uint16_t end1_z, uint16_t end2_x, uint16_t end2_y, uint16_t end2_z, uint16_t *stack, uint16_t rom_at);
FxClipXPlaneResult FxClipXPlane(uint16_t end1_x, uint16_t end1_y, uint16_t end1_z, uint16_t end2_x, uint16_t end2_y, uint16_t plane_slope, uint16_t end2_z, uint16_t *stack, uint16_t rom_at);
FxClipYPlaneResult FxClipYPlane(uint16_t a_x, uint16_t a_y, uint16_t a_z, uint16_t b_x, uint16_t b_y, uint16_t clip_slope, uint16_t b_z, uint16_t *stack, uint16_t rom_at);
FxCollideModelResult FxCollideModel(uint16_t place_x, uint16_t place_y, uint16_t model_record, uint16_t caller_scratch, uint16_t record, uint16_t *stack, uint16_t rom_at);
FxCollideModelDepthCompareResult FxCollideModelDepthCompare(uint16_t depth, uint16_t normal_x, uint16_t normal_y, uint16_t normal_z, uint16_t place_x, uint16_t place_y, uint16_t place_z, uint16_t point_table, uint16_t *stack, uint16_t entry_slot_ptr, uint16_t rom_at);
FxCollideModelSkipFaceResult FxCollideModelSkipFace(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t vertex_base, uint16_t *stack, uint16_t rom_at);
FxCollideModelUnusedResult FxCollideModelUnused(uint16_t *stack);
FxCollideModelUnusedStoreResult FxCollideModelUnusedStore(uint16_t record, uint16_t *stack);
FxCollidePushResult FxCollidePush(uint16_t contact_words, uint16_t other_record, uint16_t record, uint16_t *stack);
FxContactNoteResult FxContactNote(uint16_t record, uint16_t contact_ptr_in, uint16_t other, uint16_t *stack);
FxContactResolveResult FxContactResolve(uint16_t slot_ptr, uint16_t push_x, uint16_t push_y, uint16_t model_record, uint16_t contact_record, uint16_t *stack, uint16_t rom_at);
FxContactResolveNegateResult FxContactResolveNegate(uint16_t slot_past, uint16_t depth_x, uint16_t shallowest_depth, uint16_t depth_z, uint16_t car_ptr, uint16_t *stack);
FxContactsAnyTouchResult FxContactsAnyTouch(uint16_t place_x);
FxContactsResolveResult FxContactsResolve(uint16_t push_x, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t contact_record, uint16_t *stack, uint16_t rom_at);
FxCourseEntryReadResult FxCourseEntryRead(uint16_t entry_address, uint16_t *stack);
FxCourseNearestResult FxCourseNearest(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t prev_entry_count, uint16_t prev_entry_ptr, uint16_t prev_best_number, uint16_t prev_dist_sum);
FxCourseNextDistanceResult FxCourseNextDistance(uint16_t entry, uint16_t entry_words_z, uint16_t car_record, uint16_t *stack, uint16_t rom_at);
FxCourseNextEntryResult FxCourseNextEntry(uint16_t number, uint16_t prior_word3, uint16_t prior_word4, uint16_t entry, uint16_t car_record, uint16_t rom_at);
FxCoursePrevEntryResult FxCoursePrevEntry(uint16_t number, uint16_t entry);
uint16_t FxDivide(uint16_t high_word, uint16_t low_word, uint16_t divisor);
uint16_t FxDivide32By16(uint16_t dividend_hi, uint16_t dividend_lo, uint16_t divisor);
uint16_t FxDivide32By16Halving(uint16_t dividend_high, uint16_t dividend_low, uint16_t divisor);
uint16_t FxDivideHalvedIfLarge(uint16_t dividend_high, uint16_t dividend_low, uint16_t divisor);
uint16_t FxDivideHalving(uint16_t dividend_hi, uint16_t dividend_lo, uint16_t divisor);
FxDivideSignedResult FxDivideSigned(uint16_t dividend_lo, uint16_t dividend_hi, uint16_t divisor);
FxDrawBitmap120x16Result FxDrawBitmap120x16(uint16_t row_y, uint16_t left_x, uint16_t palette_ptr, uint16_t rom_at);
FxDrawBitmap120x24Result FxDrawBitmap120x24(uint16_t row_y, uint16_t left_x, uint16_t palette_ptr, uint16_t rom_at);
FxDrawBitmap120x32Result FxDrawBitmap120x32(uint16_t row_y, uint16_t left_x, uint16_t palette_ptr, uint16_t rom_at);
void FxDrawBitmap120x48(uint16_t row_y, uint16_t left_x, uint16_t palette_ptr, uint16_t rom_at);
FxDrawBitmap16x24Result FxDrawBitmap16x24(uint16_t top_left_y, uint16_t top_left_x, uint16_t palette, uint16_t rom_at);
FxDrawBitmap16x32Result FxDrawBitmap16x32(uint16_t row_y, uint16_t start_x, uint16_t palette_ptr, uint16_t rom_at);
FxDrawBitmap64x16Result FxDrawBitmap64x16(uint16_t row_y, uint16_t left_x, uint16_t palette_ptr, uint16_t rom_at);
FxDrawBitmap8x16Result FxDrawBitmap8x16(uint16_t row_y, uint16_t left_x, uint16_t palette_addr, uint16_t rom_at);
FxDrawListResult FxDrawList(uint16_t caller_list_end, uint16_t caller_write_ptr, uint16_t *stack, uint16_t rom_at);
FxDriveDecideResult FxDriveDecide(uint16_t point_x, uint16_t point_z, uint16_t speed_wanted, uint16_t car_record, uint16_t *stack, uint16_t rom_at);
void FxEffectsAdd(uint16_t kind, uint16_t place_x, uint16_t place, uint16_t count, uint16_t *stack);
FxEffectsAddKindsResult FxEffectsAddKinds(uint16_t seed, uint16_t particle_ptr, uint16_t place_y, uint16_t base_color, uint16_t effect_kind, uint16_t effect_kind_2, uint16_t *stack, uint16_t count, int rC);
FxEffectsAddNextResult FxEffectsAddNext(uint16_t effect_seed, uint16_t particle_slot_ptr, uint16_t place_y, uint16_t place_x, uint16_t effect_kind, uint16_t particles_left, uint16_t *stack, uint16_t count);
FxEffectsAddUnusedResult FxEffectsAddUnused(uint16_t seed, uint16_t slot_ptr, uint16_t place_y, uint16_t place_x, uint16_t effect_kind, uint16_t particles_left, uint16_t *stack, uint16_t count);
uint16_t FxEffectsAge(uint16_t entry_y);
uint16_t FxEffectsDraw(uint16_t entry_plot_result, uint16_t *stack);
uint16_t FxEffectsRun(uint16_t entry_plot_result, uint16_t *stack);
uint16_t FxEffectsSpawn(uint16_t number_records, uint16_t incoming_x, int rC);
FxFrameProgramResult FxFrameProgram(uint16_t draw_list_end);
void FxGaugeLeft(uint16_t *stack);
void FxGaugeRight(uint16_t *stack);
void FxGaugeTop(uint16_t *stack);
uint16_t FxHazePlot(uint16_t entry_plot_y);
uint16_t FxHazeSample(uint16_t time, uint16_t x, uint16_t y, uint16_t *stack);
void FxHitAddRotated(uint16_t push_x, uint16_t push_y, uint16_t push_z, uint16_t place_x, uint16_t model_record, uint16_t hit_work_reg, uint16_t *stack);
uint16_t FxHitAddScaled(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t entry_product_lo, uint16_t scale, uint16_t *stack);
FxHitAddUnitResult FxHitAddUnit(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t place_x, uint16_t place_y, uint16_t model_record, uint16_t owner_record, uint16_t *stack);
void FxInit(void);
void FxInitRamBad(void);
FxLapSegmentResult FxLapSegment(uint16_t car_record, uint16_t rom_at);
uint16_t FxLengthProgram(uint16_t *stack);
void FxListClearBit0(uint16_t record_holding);
FxListSweepResult FxListSweep(uint16_t record_holding, uint16_t prior_next_rec, uint16_t prior_keep_bit, uint16_t *stack);
uint16_t FxMatrixApply(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t where_store);
void FxMatrixApplyVectors(uint16_t where_store, uint16_t return_address);
FxMatrixFromAnglesResult FxMatrixFromAngles(uint16_t where_matrix);
FxMatrixFromAngles01Result FxMatrixFromAngles01(uint16_t where_store);
void FxMatrixFromAnglesBytes(uint16_t second_third_y, uint16_t second_third_z, uint16_t where_store);
FxMatrixFromAnglesProgramResult FxMatrixFromAnglesProgram(void);
void FxMatrixFromAnglesProgram04(void);
void FxMatrixRotateBackProgram(void);
FxMatrixRotateProgramResult FxMatrixRotateProgram(void);
void FxMatrixTranspose(uint16_t matrix);
uint16_t FxMatrixTranspose04(uint16_t matrix);
FxModelCallResult FxModelCall(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelCommandsResult FxModelCommands(uint16_t model_state_r6, uint16_t model_state_r7, uint16_t *stack, uint16_t rom_at);
FxModelDepthSortResult FxModelDepthSort(uint16_t entry_r6_word, uint16_t *stack, uint16_t rom_at);
FxModelDrawResult FxModelDraw(uint16_t draw_list_end, uint16_t *stack);
FxModelDrawFlaggedResult FxModelDrawFlagged(uint16_t draw_list_end, uint16_t *stack);
FxModelDrawFlags8000Result FxModelDrawFlags8000(uint16_t draw_list_end, uint16_t *stack);
FxModelDrawWithFlagsResult FxModelDrawWithFlags(uint16_t model_flags, uint16_t draw_list_end, uint16_t *stack);
FxModelEndResult FxModelEnd(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelFacesResult FxModelFaces(uint16_t divisor_made, uint16_t list_write_ptr, uint16_t *stack, uint16_t rom_at);
FxModelFacesGatheredUVResult FxModelFacesGatheredUV(uint16_t gathered_point, uint16_t divisor_made, uint16_t uv_list_ptr, uint16_t *stack);
FxModelFacesOnScreenUVResult FxModelFacesOnScreenUV(uint16_t divisor_made, uint16_t clip_record_ptr, uint16_t *stack);
FxModelFacingResult FxModelFacing(uint16_t *stack, uint16_t rom_at);
FxModelFramePickResult FxModelFramePick(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelIfCallResult FxModelIfCall(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelIfSkipResult FxModelIfSkip(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelIfSpriteResult FxModelIfSprite(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelJumpResult FxModelJump(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelListStartResult FxModelListStart(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelListStart3CResult FxModelListStart3C(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
uint16_t FxModelMaterial(uint16_t rom_at);
FxModelMirrorProgramResult FxModelMirrorProgram(uint16_t draw_list_end);
FxModelNextResult FxModelNext(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at, uint16_t command_code);
void FxModelPoints(uint16_t script, uint16_t *stack);
FxModelPoints01Result FxModelPoints01(uint16_t *stack, uint16_t rom_at);
FxModelPointsFullResult FxModelPointsFull(uint16_t *stack, uint16_t rom_at);
FxModelPointsMirroredResult FxModelPointsMirrored(uint16_t *stack, uint16_t rom_at);
FxModelPointsScaledResult FxModelPointsScaled(uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelProjectResult FxModelProject(uint16_t *stack, uint16_t rom_at);
FxModelProjectEachResult FxModelProjectEach(uint16_t *stack, uint16_t rom_at);
FxModelProjectRestResult FxModelProjectRest(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelPushReturnResult FxModelPushReturn(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelRecordProgramResult FxModelRecordProgram(uint16_t draw_list_end);
FxModelReturnResult FxModelReturn(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelReturn40Result FxModelReturn40(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelReturnPullResult FxModelReturnPull(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelSpriteResult FxModelSprite(uint16_t draw_list_start, uint16_t draw_list_end, uint16_t *stack, uint16_t rom_at);
FxModelSpriteColourResult FxModelSpriteColour(uint16_t draw_list_start, uint16_t rom_at);
void FxNearestCar(uint16_t prev_nearest_car, uint16_t *stack);
void FxNoiseFill(void);
void FxNoiseFillScreen(void);
void FxNopReturn(void);
void FxNopReturnCache(void);
FxNormalizeResult FxNormalize(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t model_record, uint16_t fallback_shift, uint16_t *stack);
FxObjectBounceResult FxObjectBounce(uint16_t place_x, uint16_t pose_aux, uint16_t record, uint16_t *stack, uint16_t rom_at);
FxObjectCollideOnlyResult FxObjectCollideOnly(uint16_t place_x, uint16_t place_y, uint16_t last_other_ptr, uint16_t record, uint16_t *stack, uint16_t rom_at);
FxObjectDropResult FxObjectDrop(uint16_t place_x, uint16_t last_other_ptr, uint16_t record, uint16_t *stack, uint16_t rom_at);
FxObjectFallResult FxObjectFall(uint16_t place_x, uint16_t pose_aux, uint16_t record, uint16_t *stack, uint16_t rom_at);
FxObjectFreeResult FxObjectFree(uint16_t object, uint16_t in_scratch_4, uint16_t in_scratch_7, uint16_t in_scratch_9, uint16_t *stack);
FxObjectFreeChildrenResult FxObjectFreeChildren(uint16_t object, uint16_t last_swept_record, uint16_t last_swept_value, uint16_t *stack);
void FxObjectFreeProgram(uint16_t pass_through_a, uint16_t pass_through_b, uint16_t pass_through_c);
FxObjectMarkTouchedResult FxObjectMarkTouched(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t record, uint16_t pose_matrix_ptr, uint16_t rom_at);
void FxObjectNewProgram(uint16_t segment_arg, uint16_t nearest_number, uint16_t nearest_dist_x);
void FxObjectSlideClamp(uint16_t limit, uint16_t objects_1, uint16_t objects_2, uint16_t *stack);
FxObjectSpriteResult FxObjectSprite(uint16_t sprite_scale, uint16_t draw_list_end, uint16_t *stack);
FxObjectSpriteAtResult FxObjectSpriteAt(uint16_t sprite_width, uint16_t incoming_row, uint16_t *stack);
FxObjectSpriteProgramResult FxObjectSpriteProgram(void);
FxObjectSpritePullResult FxObjectSpritePull(uint16_t *stack);
FxObjectSpriteRowsResult FxObjectSpriteRows(uint16_t sprite_size, uint16_t scale, uint16_t *stack);
FxObjectsDrawResult FxObjectsDraw(uint16_t draw_list_end, uint16_t *stack);
FxPartPullResult FxPartPull(uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t part, uint16_t *stack, uint16_t rom_at);
uint16_t FxParticlesDraw(uint16_t key, uint16_t *stack);
FxPartsUpdateResult FxPartsUpdate(uint16_t entry_product_lo, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t *stack, uint16_t rom_at);
FxPhysicsProgramResult FxPhysicsProgram(uint16_t entry_scratch_a, uint16_t pose_sin, uint16_t entry_scratch_b, uint16_t entry_scratch_c, uint16_t rom_at);
void FxPixelRun(uint16_t colour, uint16_t start_x, uint16_t start, uint16_t count);
void FxPlotTestProgram(void);
FxPointsGatherResult FxPointsGather(uint16_t count, uint16_t rom_at);
FxPolygonClipBottomResult FxPolygonClipBottom(uint16_t entry_r7_word, uint16_t *stack, uint16_t rom_at);
FxPolygonClipLeftResult FxPolygonClipLeft(uint16_t entry_z, uint16_t *stack, uint16_t rom_at);
FxPolygonClipNearResult FxPolygonClipNear(uint16_t near_z, uint16_t entry_z, uint16_t *stack, uint16_t rom_at);
FxPolygonClipRightResult FxPolygonClipRight(uint16_t entry_end2_z, uint16_t *stack, uint16_t rom_at);
FxPolygonClipScreenResult FxPolygonClipScreen(uint16_t vertex_count, uint16_t prev_end_y, uint16_t divide_remainder_in, uint16_t *stack);
FxPolygonClipScreenUVResult FxPolygonClipScreenUV(uint16_t vertex_count, uint16_t divisor_made, uint16_t quotient_sign_in, uint16_t *stack);
FxPolygonClipTopResult FxPolygonClipTop(uint16_t next_vertex_z, uint16_t *stack, uint16_t rom_at);
FxPolygonDrawResult FxPolygonDraw(uint16_t outcodes, uint16_t *stack, uint16_t rom_at);
FxPolygonGatherResult FxPolygonGather(uint16_t count, uint16_t *stack, uint16_t rom_at);
FxPolygonNearClipResult FxPolygonNearClip(uint16_t *stack, uint16_t rom_at);
FxPolygonProjectResult FxPolygonProject(uint16_t *stack, uint16_t rom_at);
uint16_t FxPoolChecksum(void);
void FxPoolChecksumProgram(void);
uint16_t FxPoolTake(uint16_t pool_chain, uint16_t bytes, uint16_t *stack);
void FxPoolsMerge(void);
void FxPoolsMergeProgram(void);
FxPopReturnC4A5Result FxPopReturnC4A5(uint16_t *stack);
FxProjectPointResult FxProjectPoint(uint16_t point_x, uint16_t point_y, uint16_t depth, uint16_t *stack);
void FxProjectPoints(uint16_t count_more, uint16_t points, uint16_t where_store, uint16_t *stack);
FxProjectScaledResult FxProjectScaled(uint16_t x, uint16_t y, uint16_t depth_positive, uint16_t size);
FxProjectVertexResult FxProjectVertex(uint16_t x, uint16_t y, uint16_t entry_aux_a, uint16_t depth, uint16_t entry_aux_c, uint16_t *stack, uint16_t count, uint16_t entry_aux_b);
FxPushApplyResult FxPushApply(uint16_t push, uint16_t push_x, uint16_t push_y, uint16_t prior_flags_high, uint16_t *stack, uint16_t rom_at);
FxPushAt1Result FxPushAt1(uint16_t push_x, uint16_t push_y, uint16_t push_z, uint16_t *stack);
FxPushAt2Result FxPushAt2(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack);
FxPushAt4Result FxPushAt4(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack);
void FxPushAt4Kept(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack);
FxPushAt4TwistResult FxPushAt4Twist(uint16_t offset_x, uint16_t offset_y, uint16_t offset_z, uint16_t *stack);
FxPushAtHalfResult FxPushAtHalf(uint16_t offset_x, uint16_t offset_y, uint16_t offset_z, uint16_t *stack);
FxPushAtQuarterResult FxPushAtQuarter(uint16_t arm_x, uint16_t arm_y, uint16_t arm_z, uint16_t *stack);
void FxPushAtTurned(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t *stack);
FxPushAtVariantResult FxPushAtVariant(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t push_x, uint16_t push_y, uint16_t model_record, uint16_t *stack, uint16_t count);
uint16_t FxPushSet(uint16_t amount);
FxPushesApplyResult FxPushesApply(uint16_t push_x, uint16_t push_y, uint16_t push_amount, uint16_t *stack, uint16_t rom_at);
uint16_t FxRankCars(uint16_t candidate_car_in, uint16_t *stack);
uint16_t FxRankNext(uint16_t in_scratch_7, uint16_t rank);
void FxRankNextLapZero(uint16_t best_keys_x, uint16_t best_keys_y, uint16_t best_keys_z, uint16_t car, uint16_t car_keys_1, uint16_t car_keys_2, uint16_t car_keys_3, uint16_t rival_record, uint16_t rank);
FxRatioClampResult FxRatioClamp(uint16_t pair_x, uint16_t pair_y, uint16_t divisor, uint16_t *stack);
void FxRatioClampSwapped(uint16_t pair_second, uint16_t pair_first, uint16_t divisor, uint16_t *stack);
void FxRecordAimFromLink(uint16_t record, uint16_t *stack);
FxRecordAlignmentResult FxRecordAlignment(uint16_t pose_aux, uint16_t record, uint16_t normalize_work_reg, uint16_t *stack);
FxRecordCollideResult FxRecordCollide(uint16_t place_x, uint16_t place_y, uint16_t pose_aux, uint16_t other_record, uint16_t record, uint16_t *stack, uint16_t rom_at);
FxRecordHandlerResult FxRecordHandler(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t record, uint16_t pose_matrix_ptr, uint16_t *stack, uint16_t count, uint16_t rom_at);
FxRecordHeadingAheadResult FxRecordHeadingAhead(uint16_t stale_entry_number, uint16_t stale_entry_ptr, uint16_t nearest_number, uint16_t record, uint16_t nearest_dist_x, uint16_t *stack);
void FxRecordHoldY(uint16_t record);
void FxRecordLower82(void);
uint16_t FxRecordMove(uint16_t object_kind, uint16_t record);
void FxRecordNew422(void);
void FxRecordNew424(void);
void FxRecordNew426(void);
void FxRecordNew426B(void);
void FxRecordNew42D(void);
void FxRecordNewCamera(void);
void FxRecordNewFlag1Program(uint16_t *stack);
void FxRecordNewMarkedProgram(void);
void FxRecordNoneProgram(void);
void FxRecordRaise6A(void);
FxRecordSegmentResult FxRecordSegment(uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t prev_best_number, uint16_t record, uint16_t prev_dist_sum, uint16_t *stack);
void FxRecordSegmentNext(uint16_t pose_cos, uint16_t pose_sin, uint16_t pose_aux, uint16_t car_record, uint16_t pose_matrix_ptr, uint16_t *stack);
FxRecordTouchesResult FxRecordTouches(uint16_t record, uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t *stack);
FxRecordsApartResult FxRecordsApart(uint16_t records_1, uint16_t records_2, uint16_t normalize_aux_in, uint16_t *stack);
void FxRectFill(uint16_t colour, uint16_t left_x, uint16_t top_left, uint16_t width, uint16_t height);
void FxRectFrame(uint16_t colour, uint16_t left_x, uint16_t top_left, uint16_t width, uint16_t height_least);
FxReturnOneRestoreResult FxReturnOneRestore(uint16_t *stack);
void FxReturnR11(void);
void FxRotateXZByte(uint16_t point_x, uint16_t angle, uint16_t point_z, uint16_t *stack);
void FxRowsCopyProgram(void);
void FxScaleFromTable14(void);
FxScaleFromTable14PullResult FxScaleFromTable14Pull(uint16_t *stack);
void FxScreenClear(void);
FxSegmentCrossZResult FxSegmentCrossZ(uint16_t ends_1, uint16_t ends_2, uint16_t ends_3, uint16_t ends_4, uint16_t ends_5, uint16_t ends_6);
void FxSortObjectsProgram(void);
FxSpreadCarsResult FxSpreadCars(uint16_t entry_distance, uint16_t car_record, uint16_t *stack);
void FxSprite32Program(void);
FxSpriteMaterialResult FxSpriteMaterial(uint16_t material_index, uint16_t model_stream_ptr, uint16_t sprite_scale_word);
void FxSqrt16(uint16_t value);
uint16_t FxSqrt32(uint16_t radicand_lo, uint16_t radicand_hi);
void FxSqrt32Test(void);
FxSquareRootResult FxSquareRoot(uint16_t high_word, uint16_t low_word);
FxSteerFromInputResult FxSteerFromInput(uint16_t control_bits, uint16_t car_record);
void FxSumScale(uint16_t scale);
FxSumsApplyResult FxSumsApply(uint16_t record);
void FxSumsClear(void);
void FxSumsPushLocal(uint16_t push_x, uint16_t push_y, uint16_t push_z, uint16_t *stack);
FxSumsPushWorldResult FxSumsPushWorld(uint16_t push_x, uint16_t push_y, uint16_t push_z, uint16_t *stack);
FxSumsRotateLocalResult FxSumsRotateLocal(void);
FxTable2702ProgramResult FxTable2702Program(void);
void FxTable7C6Byte(uint16_t rom_at);
FxTableEntryWordsResult FxTableEntryWords(uint16_t entry);
void FxText3D(uint16_t colour, uint16_t *stack);
FxText3DLeftColumnResult FxText3DLeftColumn(uint16_t font_bits_entry, uint16_t column_x, uint16_t row_y, uint16_t font_row, uint16_t string_ptr, uint16_t row_sum, uint16_t column_sum, uint16_t row_step, uint16_t column_step, uint16_t *stack, uint16_t count);
FxText3DRowEndNopResult FxText3DRowEndNop(uint16_t screen_row, uint16_t font_row, uint16_t row_accum, uint16_t row_step, uint16_t col_step, uint16_t *stack);
FxTextDrawResult FxTextDraw(uint16_t entry_x, uint16_t *stack);
FxTextNoticeResult FxTextNotice(uint16_t prev_text_arg, uint16_t *stack);
void FxTextProgram(uint16_t text_ptr);
void FxTiles2bppTo4bppHigh(void);
void FxTiles2bppTo4bppLow(void);
void FxTimersProgram(void);
FxTrafficSidesResult FxTrafficSides(uint16_t entry_scratch_b, uint16_t car_record, uint16_t *stack);
FxTurnResult FxTurn(uint16_t x, uint16_t angle, uint16_t z, uint16_t *stack);
FxUnpackBitsResult FxUnpackBits(uint16_t bit_buffer_hi, uint16_t bit_buffer_lo, uint16_t loop_addr, uint16_t count, uint16_t rom_at);
void FxUnpackProgram(void);
FxUnpackRefillResult FxUnpackRefill(uint16_t rom_at, int rC);
void FxVectorNormalize(uint16_t vector_x, uint16_t vector_y, uint16_t vector_z, uint16_t *stack);
void FxVectorNormalizeDelta(uint16_t from_x, uint16_t from_y, uint16_t from_z, uint16_t to_x, uint16_t to_y, uint16_t to_z, uint16_t *stack);
void FxVectorNormalizeTest(uint16_t *stack);
uint16_t FxWeatherDraw(uint16_t weather, uint16_t drops, uint16_t plot_y, uint16_t *stack);
FxWeatherDropNextResult FxWeatherDropNext(uint16_t seed, uint16_t drop_ptr, uint16_t *stack, uint16_t weather_kind, uint16_t count);
uint16_t FxWeatherReset(uint16_t weather, uint16_t *stack);
FxWeatherUnusedResult FxWeatherUnused(uint16_t seed, uint16_t drop_at, uint16_t *stack, uint16_t weather_kind, uint16_t count, int past_drop_end);
FxWeatherUnusedClearResult FxWeatherUnusedClear(uint16_t seed, uint16_t drop, uint16_t *stack, uint16_t weather_kind, uint16_t count);
FxWeatherUnusedFullResult FxWeatherUnusedFull(uint16_t seed, uint16_t drop, uint16_t *stack, uint16_t weather_kind, uint16_t count);
FxWeatherUnusedLoopResult FxWeatherUnusedLoop(uint16_t seed, uint16_t drop_at, uint16_t *stack, uint16_t weather_kind, uint16_t count, int past_drop_end);
FxWheelUpdateResult FxWheelUpdate(uint16_t entry_product_lo, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t wheel, uint16_t *stack, uint16_t rom_at);
FxWheelUpdateNoSlotResult FxWheelUpdateNoSlot(uint16_t entry_product_lo, uint16_t push_y, uint16_t model_record, uint16_t car, uint16_t wheel, uint16_t *stack, uint16_t rom_at);

#endif
