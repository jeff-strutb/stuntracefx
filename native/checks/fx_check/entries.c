/* written by mkentries.py */
#include "fx/fx.h"
#include "fx_check.h"

static void entry_FxInit(void)
{

    FxInit();
}

static void entry_FxInitRamBad(void)
{

    FxInitRamBad();
}

static void entry_FxNormalize(void)
{
    uint16_t ret = gsu.r[11];
    FxNormalized n;

    if (FxNormalize((FxVec3){(int16_t)gsu.r[1], (int16_t)gsu.r[2], (int16_t)gsu.r[3]}, &n)) {
        gsu.r[1] = (uint16_t)n.unit.x;
        gsu.r[2] = (uint16_t)n.unit.y;
        gsu.r[3] = (uint16_t)n.unit.z;
        gsu.r[4] = n.z_low;
        gsu.r[7] = n.x_high;
        gsu.r[9] = n.shift;
    } else {
        gsu.r[1] = gsu.r[2] = gsu.r[3] = gsu.r[4] = 0;
    }
    gsu.r[5] = 0;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSquareRoot(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[6] = FxSquareRoot((uint32_t)gsu.r[4] << 16 | gsu.r[5]);
    gsu.r[5] = 0;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDivideHalvedIfLarge(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[4] = FxDivideHalvedIfLarge((uint32_t)gsu.r[0] << 16 | gsu.r[4], gsu.r[6]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDivide(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[4] = FxDivide((uint32_t)gsu.r[0] << 16 | gsu.r[4], gsu.r[6]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxTurn(void)
{
    uint16_t ret = gsu.r[11];

    FxTurned t = FxTurn((int16_t)gsu.r[1], (uint8_t)gsu.r[2], (int16_t)gsu.r[3], fxs.rombr);
    gsu.r[1] = (uint16_t)t.x;
    gsu.r[3] = (uint16_t)t.z;
    gsu.r[14] = (uint16_t)(0x96FE + gsu.r[2]);    /* the chip's cursor, left on the sine byte */
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxMatrixFromAngles(void)
{
    uint16_t ret = gsu.r[11];

    FxMatrixFromAnglesResult r_ = FxMatrixFromAngles(gsu.r[9]);
    gsu.r[4] = (uint16_t)r_.s1;
    gsu.r[5] = (uint16_t)r_.c2;
    gsu.r[7] = (uint16_t)r_.s2;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxMatrixTranspose04(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[9] = (uint16_t)FxMatrixTranspose04(gsu.r[9]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxAngleOf(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxAngleOfResult r_ = FxAngleOf(gsu.r[1], gsu.r[2], gsu.r[5], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.angle;
    gsu.r[4] = (uint16_t)r_.xor_negative;
    gsu.r[5] = (uint16_t)r_.swap_marker_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxAngleVector(void)
{
    uint16_t ret = gsu.r[11];

    FxVec3 v = FxAngleVector(gsu.r[0], (int16_t)gsu.r[4]);
    gsu.r[1] = (uint16_t)v.x;
    gsu.r[2] = (uint16_t)v.y;
    gsu.r[3] = (uint16_t)v.z;
    gsu.r[14] = (uint16_t)(0x973E + (gsu.r[0] >> 8));   /* the chip's cursor, left on the cosine byte */
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarAddPart(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[14] = (uint16_t)FxCarAddPart(gsu.r[1], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarFromScript(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarFromScript(gsu.r[1], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarMoveParts(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarMovePartsResult r_ = FxCarMoveParts(gsu.r[4], gsu.r[5], gsu.r[8], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_part_velocity_z;
    gsu.r[5] = (uint16_t)r_.world_z_partial_sum;
    gsu.r[9] = (uint16_t)r_.last_part_ptr;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarMovePartsAfterWalk(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarMovePartsAfterWalkResult r_ = FxCarMovePartsAfterWalk(&stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarMovePartsAfterStill(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarMovePartsAfterStillResult r_ = FxCarMovePartsAfterStill(&stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarPose(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarPoseResult r_ = FxCarPose(gsu.r[8], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_change_low;
    gsu.r[5] = (uint16_t)r_.last_change_high;
    gsu.r[7] = (uint16_t)r_.pose_aux;
    gsu.r[9] = (uint16_t)r_.last_part_ptr;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarNew(void)
{

    FxCarNew();
}

static void entry_FxCarHit(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarHitResult r_ = FxCarHit(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.spread_out;
    gsu.r[5] = (uint16_t)r_.car_y_out;
    gsu.r[7] = (uint16_t)r_.car_z_out;
    gsu.r[9] = (uint16_t)r_.other_car_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCollidePush(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCollidePushResult r_ = FxCollidePush(gsu.r[1], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.share_before;
    gsu.r[5] = (uint16_t)r_.place_y;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordsApart(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxRecordsApartResult r_ = FxRecordsApart(gsu.r[7], gsu.r[8], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.records_apart;
    gsu.r[9] = (uint16_t)r_.normalize_aux_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxContactNote(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxContactNoteResult r_ = FxContactNote(gsu.r[6], gsu.r[7], gsu.r[8], &stack);
    gsu.r[10] = stack;
    gsu.r[7] = (uint16_t)r_.contact;
    gsu.r[9] = (uint16_t)r_.contact_flag_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxAllTouches(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxAllTouchesResult r_ = FxAllTouches(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.touch_pos_result;
    gsu.r[5] = (uint16_t)r_.record_z_out;
    gsu.r[7] = (uint16_t)r_.last_contact_ptr_out;
    gsu.r[9] = (uint16_t)r_.last_contact_flags_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordTouches(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxRecordTouchesResult r_ = FxRecordTouches(gsu.r[1], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.record_y;
    gsu.r[5] = (uint16_t)r_.record_z_out;
    gsu.r[7] = (uint16_t)r_.last_contact_result;
    gsu.r[9] = (uint16_t)r_.last_contact_aux;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordMove(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[5] = (uint16_t)FxRecordMove(gsu.r[1], gsu.r[8]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPhysicsProgram(void)
{

    FxPhysicsProgramResult r_ = FxPhysicsProgram(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[9], gsu.r[14]);
}

static void entry_FxObjectBounce(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectBounceResult r_ = FxObjectBounce(gsu.r[4], gsu.r[7], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.world_sum_x_out;
    gsu.r[5] = (uint16_t)r_.world_sum_y_out;
    gsu.r[7] = (uint16_t)r_.push_flags_high_out;
    gsu.r[8] = (uint16_t)r_.record_restored_out;
    gsu.r[9] = (uint16_t)r_.working_record_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectMarkTouched(void)
{
    uint16_t ret = gsu.r[11];

    FxObjectMarkTouchedResult r_ = FxObjectMarkTouched(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], gsu.r[14]);
    gsu.r[4] = (uint16_t)r_.pose_cos_out;
    gsu.r[5] = (uint16_t)r_.pose_sin_out;
    gsu.r[7] = (uint16_t)r_.pose_aux_out;
    gsu.r[8] = (uint16_t)r_.record_out;
    gsu.r[9] = (uint16_t)r_.pose_matrix_ptr_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectFall(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectFallResult r_ = FxObjectFall(gsu.r[4], gsu.r[7], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.world_sum_x_out;
    gsu.r[5] = (uint16_t)r_.world_sum_y_out;
    gsu.r[7] = (uint16_t)r_.push_flags_high_out;
    gsu.r[8] = (uint16_t)r_.record_restored_out;
    gsu.r[9] = (uint16_t)r_.working_record_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectCollideOnly(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectCollideOnlyResult r_ = FxObjectCollideOnly(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_collision_result;
    gsu.r[5] = (uint16_t)r_.last_contact_ptr;
    gsu.r[7] = (uint16_t)r_.last_other_ptr_out;
    gsu.r[8] = (uint16_t)r_.record_restored;
    gsu.r[9] = (uint16_t)r_.collided_record;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectDrop(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectDropResult r_ = FxObjectDrop(gsu.r[4], gsu.r[7], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_collision_result;
    gsu.r[5] = (uint16_t)r_.last_contact_ptr;
    gsu.r[7] = (uint16_t)r_.last_other_ptr_out;
    gsu.r[8] = (uint16_t)r_.record_restored;
    gsu.r[9] = (uint16_t)r_.collided_record;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordCollide(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxRecordCollideResult r_ = FxRecordCollide(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.world_sum_x_out;
    gsu.r[5] = (uint16_t)r_.world_sum_y_out;
    gsu.r[7] = (uint16_t)r_.push_flags_high_out;
    gsu.r[8] = (uint16_t)r_.caller_scratch_back;
    gsu.r[9] = (uint16_t)r_.record_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxContactsAnyTouch(void)
{
    uint16_t ret = gsu.r[11];

    FxContactsAnyTouchResult r_ = FxContactsAnyTouch(gsu.r[4]);
    gsu.r[0] = (uint16_t)r_.any_touch;
    gsu.r[4] = (uint16_t)r_.depth;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCollideModel(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCollideModelResult r_ = FxCollideModel(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.list_face;
    gsu.r[4] = (uint16_t)r_.quotient;
    gsu.r[5] = (uint16_t)r_.place_y_out;
    gsu.r[7] = (uint16_t)r_.model_record_out;
    gsu.r[8] = (uint16_t)r_.caller_scratch_back;
    gsu.r[9] = (uint16_t)r_.record_back;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCollideModelSkipFace(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCollideModelSkipFaceResult r_ = FxCollideModelSkipFace(gsu.r[4], gsu.r[5], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCollideModelDepthCompare(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCollideModelDepthCompareResult r_ = FxCollideModelDepthCompare(gsu.r[0], gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[8], gsu.r[9], &stack, gsu.r[11], gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.contact_found;
    gsu.r[4] = (uint16_t)r_.quotient;
    gsu.r[5] = (uint16_t)r_.place_y_out;
    gsu.r[7] = (uint16_t)r_.face_scale_out;
    gsu.r[8] = (uint16_t)r_.exit_aux_record;
    gsu.r[9] = (uint16_t)r_.exit_record;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCollideModelUnused(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCollideModelUnusedResult r_ = FxCollideModelUnused(&stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCollideModelUnusedStore(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCollideModelUnusedStoreResult r_ = FxCollideModelUnusedStore(gsu.r[9], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPartsUpdate(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPartsUpdateResult r_ = FxPartsUpdate(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_low;
    gsu.r[5] = (uint16_t)r_.y_row;
    gsu.r[7] = (uint16_t)r_.model_record_out;
    gsu.r[8] = (uint16_t)r_.car_out;
    gsu.r[9] = (uint16_t)r_.last_part;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPartPull(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPartPullResult r_ = FxPartPull(gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.quotient;
    gsu.r[5] = (uint16_t)r_.push_y_out;
    gsu.r[7] = (uint16_t)r_.high_words_hi;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWheelUpdate(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxWheelUpdateResult r_ = FxWheelUpdate(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_low;
    gsu.r[5] = (uint16_t)r_.y_row;
    gsu.r[7] = (uint16_t)r_.model_record_out;
    gsu.r[8] = (uint16_t)r_.car_out;
    gsu.r[9] = (uint16_t)r_.wheel_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWheelUpdateNoSlot(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxWheelUpdateNoSlotResult r_ = FxWheelUpdateNoSlot(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxContactsResolve(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxContactsResolveResult r_ = FxContactsResolve(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.push_flags_out;
    gsu.r[4] = (uint16_t)r_.entry_product_lo;
    gsu.r[5] = (uint16_t)r_.push_y_out;
    gsu.r[7] = (uint16_t)r_.model_record_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxContactResolve(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxContactResolveResult r_ = FxContactResolve(gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[3] = (uint16_t)r_.next_slot_ptr;
    gsu.r[4] = (uint16_t)r_.push_x_out;
    gsu.r[5] = (uint16_t)r_.push_y_out;
    gsu.r[7] = (uint16_t)r_.model_record_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxContactResolveNegate(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxContactResolveNegateResult r_ = FxContactResolveNegate(gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarHandler(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarHandlerResult r_ = FxCarHandler(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.course_x_part;
    gsu.r[5] = (uint16_t)r_.next_entry_end_ptr;
    gsu.r[7] = (uint16_t)r_.pose_aux_out;
    gsu.r[8] = (uint16_t)r_.car_out;
    gsu.r[9] = (uint16_t)r_.pose_matrix_ptr_out;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarHandlerClamp(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarHandlerClampResult r_ = FxCarHandlerClamp(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.course_x_part;
    gsu.r[5] = (uint16_t)r_.next_entry_end_ptr;
    gsu.r[7] = (uint16_t)r_.pose_aux_out;
    gsu.r[8] = (uint16_t)r_.car_out;
    gsu.r[9] = (uint16_t)r_.pose_matrix_ptr_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarHandlerFlip(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarHandlerFlipResult r_ = FxCarHandlerFlip(gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.distance_x_part_out;
    gsu.r[5] = (uint16_t)r_.next_entry_ptr_out;
    gsu.r[7] = (uint16_t)r_.pose_aux_out;
    gsu.r[8] = (uint16_t)r_.car_out;
    gsu.r[9] = (uint16_t)r_.pose_matrix_ptr_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarCollide(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarCollideResult r_ = FxCarCollide(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.share_before;
    gsu.r[5] = (uint16_t)r_.next_entry_end_ptr;
    gsu.r[7] = (uint16_t)r_.model_record_out;
    gsu.r[8] = (uint16_t)r_.caller_scratch_back;
    gsu.r[9] = (uint16_t)r_.record_ptr;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPoolTake(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[1] = (uint16_t)FxPoolTake(gsu.r[3], gsu.r[4], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPoolsMergeProgram(void)
{

    FxPoolsMergeProgram();
}

static void entry_FxPoolsMerge(void)
{
    uint16_t ret = gsu.r[11];

    FxPoolsMerge();
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxListSweep(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxListSweepResult r_ = FxListSweep(gsu.r[6], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[7] = (uint16_t)r_.last_next_rec;
    gsu.r[9] = (uint16_t)r_.last_size_addr;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordNew426(void)
{

    FxRecordNew426();
}

static void entry_FxRecordNew422(void)
{

    FxRecordNew422();
}

static void entry_FxRecordNew424(void)
{

    FxRecordNew424();
}

static void entry_FxRecordNew426B(void)
{

    FxRecordNew426B();
}

static void entry_FxRecordNewCamera(void)
{

    FxRecordNewCamera();
}

static void entry_FxRecordNew42D(void)
{

    FxRecordNew42D();
}

static void entry_FxObjectFree(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectFreeResult r_ = FxObjectFree(gsu.r[1], gsu.r[4], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.children_result;
    gsu.r[7] = (uint16_t)r_.sweep_result_7;
    gsu.r[9] = (uint16_t)r_.sweep_result_9;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectFreeChildren(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectFreeChildrenResult r_ = FxObjectFreeChildren(gsu.r[1], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_child_ptr;
    gsu.r[7] = (uint16_t)r_.last_swept_record_out;
    gsu.r[9] = (uint16_t)r_.last_swept_value_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushSet(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[0] = (uint16_t)FxPushSet(gsu.r[2]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxListClearBit0(void)
{
    uint16_t ret = gsu.r[11];

    FxListClearBit0(gsu.r[6]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordHoldY(void)
{
    uint16_t ret = gsu.r[11];

    FxRecordHoldY(gsu.r[8]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxBearingTo(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxBearingToResult r_ = FxBearingTo(gsu.r[1], gsu.r[3], gsu.r[5], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.angle;
    gsu.r[4] = (uint16_t)r_.angle_sign_xor;
    gsu.r[5] = (uint16_t)r_.swap_mark_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordAlignment(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxRecordAlignmentResult r_ = FxRecordAlignment(gsu.r[7], gsu.r[8], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.last_product_hi;
    gsu.r[5] = (uint16_t)r_.last_product_lo;
    gsu.r[7] = (uint16_t)r_.low_word;
    gsu.r[9] = (uint16_t)r_.normalize_work_reg_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordRaise6A(void)
{

    FxRecordRaise6A();
}

static void entry_FxRecordLower82(void)
{

    FxRecordLower82();
}

static void entry_FxTimersProgram(void)
{

    FxTimersProgram();
}

static void entry_FxParticlesDraw(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[7] = (uint16_t)FxParticlesDraw(gsu.r[2], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushesApply(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushesApplyResult r_ = FxPushesApply(gsu.r[4], gsu.r[5], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.place_x;
    gsu.r[5] = (uint16_t)r_.place_y;
    gsu.r[7] = (uint16_t)r_.push_amount_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushApply(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushApplyResult r_ = FxPushApply(gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.applied_skipped;
    gsu.r[3] = (uint16_t)r_.push_out;
    gsu.r[4] = (uint16_t)r_.place_x;
    gsu.r[5] = (uint16_t)r_.place_y;
    gsu.r[7] = (uint16_t)r_.push_flags_high_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSumsClear(void)
{
    uint16_t ret = gsu.r[11];

    FxSumsClear();
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSumsPushWorld(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxSumsPushWorldResult r_ = FxSumsPushWorld(gsu.r[4], gsu.r[5], gsu.r[6], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.push_x_out;
    gsu.r[5] = (uint16_t)r_.push_y_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSumsPushLocal(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxSumsPushLocal(gsu.r[4], gsu.r[5], gsu.r[6], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSumsRotateLocal(void)
{
    uint16_t ret = gsu.r[11];

    FxSumsRotateLocalResult r_ = FxSumsRotateLocal();
    gsu.r[4] = (uint16_t)r_.old_world;
    gsu.r[5] = (uint16_t)r_.y_row;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSumsApply(void)
{
    uint16_t ret = gsu.r[11];

    FxSumsApplyResult r_ = FxSumsApply(gsu.r[9]);
    gsu.r[4] = (uint16_t)r_.world_sum_x;
    gsu.r[5] = (uint16_t)r_.world_sum_y;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxHitAddScaled(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[4] = (uint16_t)FxHitAddScaled(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[6], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxHitAddUnit(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxHitAddUnitResult r_ = FxHitAddUnit(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.quotient;
    gsu.r[5] = (uint16_t)r_.place_y_out;
    gsu.r[7] = (uint16_t)r_.high_words_hi;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAtTurned(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAtTurned(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAt4Kept(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAt4Kept(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAt2(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAt2Result r_ = FxPushAt2(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.push_x;
    gsu.r[5] = (uint16_t)r_.push_y;
    gsu.r[7] = (uint16_t)r_.model_record;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAt1(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAt1Result r_ = FxPushAt1(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.push_x_out;
    gsu.r[5] = (uint16_t)r_.push_y_out;
    gsu.r[7] = (uint16_t)r_.model_record;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAtHalf(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAtHalfResult r_ = FxPushAtHalf(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.push_x;
    gsu.r[5] = (uint16_t)r_.push_y;
    gsu.r[7] = (uint16_t)r_.model_record;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAtQuarter(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAtQuarterResult r_ = FxPushAtQuarter(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.push_x;
    gsu.r[5] = (uint16_t)r_.push_y;
    gsu.r[7] = (uint16_t)r_.model_record;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAt4Twist(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAt4TwistResult r_ = FxPushAt4Twist(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.push_x;
    gsu.r[5] = (uint16_t)r_.push_y;
    gsu.r[7] = (uint16_t)r_.model_record;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSumScale(void)
{
    uint16_t ret = gsu.r[11];

    FxSumScale(gsu.r[6]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPushAt4(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPushAt4Result r_ = FxPushAt4(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.place_x;
    gsu.r[5] = (uint16_t)r_.place_y;
    gsu.r[7] = (uint16_t)r_.model_record;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordHeadingAhead(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxRecordHeadingAheadResult r_ = FxRecordHeadingAhead(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.heading_angle;
    gsu.r[4] = (uint16_t)r_.angle_sign_xor;
    gsu.r[5] = (uint16_t)r_.swap_mark;
    gsu.r[7] = (uint16_t)r_.nearest_number_out;
    gsu.r[9] = (uint16_t)r_.nearest_dist_x_out;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRecordSegment(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxRecordSegmentResult r_ = FxRecordSegment(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.segment_number;
    gsu.r[3] = (uint16_t)r_.entry_word_z;
    gsu.r[4] = (uint16_t)r_.segment_count;
    gsu.r[5] = (uint16_t)r_.entry_ptr;
    gsu.r[9] = (uint16_t)r_.last_dist_sum;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCourseNearest(void)
{
    uint16_t ret = gsu.r[11];

    FxCourseNearestResult r_ = FxCourseNearest(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[9]);
    gsu.r[0] = (uint16_t)r_.number_1;
    gsu.r[3] = (uint16_t)r_.entry_words_z;
    gsu.r[4] = (uint16_t)r_.count;
    gsu.r[5] = (uint16_t)r_.entry;
    gsu.r[7] = (uint16_t)r_.number_2;
    gsu.r[9] = (uint16_t)r_.last_dist_sum;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCourseNextEntry(void)
{
    uint16_t ret = gsu.r[11];

    FxCourseNextEntryResult r_ = FxCourseNextEntry(gsu.r[0], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[8], gsu.r[14]);
    gsu.r[0] = (uint16_t)r_.number_1;
    gsu.r[1] = (uint16_t)r_.words_1;
    gsu.r[3] = (uint16_t)r_.words_3;
    gsu.r[4] = (uint16_t)r_.words_4;
    gsu.r[5] = (uint16_t)r_.entry_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCoursePrevEntry(void)
{
    uint16_t ret = gsu.r[11];

    FxCoursePrevEntryResult r_ = FxCoursePrevEntry(gsu.r[0], gsu.r[5]);
    gsu.r[0] = (uint16_t)r_.number_1;
    gsu.r[1] = (uint16_t)r_.words_1;
    gsu.r[3] = (uint16_t)r_.words_3;
    gsu.r[4] = (uint16_t)r_.words_4;
    gsu.r[5] = (uint16_t)r_.entry_out;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCourseEntryRead(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCourseEntryReadResult r_ = FxCourseEntryRead(gsu.r[5], &stack);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.words_1;
    gsu.r[2] = (uint16_t)r_.words_2;
    gsu.r[3] = (uint16_t)r_.words_3;
    gsu.r[4] = (uint16_t)r_.words_4;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxLapSegment(void)
{
    uint16_t ret = gsu.r[11];

    FxLapSegmentResult r_ = FxLapSegment(gsu.r[8], gsu.r[14]);
    gsu.r[3] = (uint16_t)r_.segment_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCourseNextDistance(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCourseNextDistanceResult r_ = FxCourseNextDistance(gsu.r[0], gsu.r[3], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.distance;
    gsu.r[4] = (uint16_t)r_.x_part;
    gsu.r[5] = (uint16_t)r_.next_entry_end_ptr;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarCatchUp(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarCatchUpResult r_ = FxCarCatchUp(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.words_4;
    gsu.r[5] = (uint16_t)r_.entry_out;
    gsu.r[7] = (uint16_t)r_.segment_scratch_a_out;
    gsu.r[9] = (uint16_t)r_.segment_scratch_b_out;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarPlaceOnSegment(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarPlaceOnSegmentResult r_ = FxCarPlaceOnSegment(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.angle_sign_xor;
    gsu.r[5] = (uint16_t)r_.swap_mark_out;
    gsu.r[7] = (uint16_t)r_.last_next_rec;
    gsu.r[9] = (uint16_t)r_.last_size_addr;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxCarVelocityFromSegment(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxCarVelocityFromSegmentResult r_ = FxCarVelocityFromSegment(gsu.r[4], gsu.r[5], gsu.r[7], gsu.r[8], gsu.r[9], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.course_followed;
    gsu.r[4] = (uint16_t)r_.result_speed;
    gsu.r[5] = (uint16_t)r_.segment_entry_ptr;
    gsu.r[7] = (uint16_t)r_.last_scan_value_out;
    gsu.r[8] = (uint16_t)r_.car_record_out;
    gsu.r[9] = (uint16_t)r_.last_scan_aux_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDriveDecide(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxDriveDecideResult r_ = FxDriveDecide(gsu.r[1], gsu.r[3], gsu.r[4], gsu.r[8], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.angle_sign_xor;
    gsu.r[5] = (uint16_t)r_.swap_mark;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSteerFromInput(void)
{
    uint16_t ret = gsu.r[11];

    FxSteerFromInputResult r_ = FxSteerFromInput(gsu.r[2], gsu.r[8]);
    gsu.r[3] = (uint16_t)r_.new_value;
    gsu.r[7] = (uint16_t)r_.pose_aux;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRankCars(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[7] = (uint16_t)FxRankCars(gsu.r[7], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRankNext(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[7] = (uint16_t)FxRankNext(gsu.r[7], gsu.r[9]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRankNextLapZero(void)
{
    uint16_t ret = gsu.r[11];

    FxRankNextLapZero(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[6], gsu.r[7], gsu.r[8], gsu.r[9]);
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxTrafficSides(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxTrafficSidesResult r_ = FxTrafficSides(gsu.r[5], gsu.r[8], &stack);
    gsu.r[10] = stack;
    gsu.r[5] = (uint16_t)r_.entry_scratch_b_out;
    gsu.r[7] = (uint16_t)r_.last_other_car;
    gsu.r[9] = (uint16_t)r_.old;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSpreadCars(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxSpreadCarsResult r_ = FxSpreadCars(gsu.r[4], gsu.r[8], &stack);
    gsu.r[10] = stack;
    gsu.r[4] = (uint16_t)r_.spread_value_out;
    gsu.r[5] = (uint16_t)r_.car_y_out;
    gsu.r[7] = (uint16_t)r_.car_z_out;
    gsu.r[9] = (uint16_t)r_.other_car_out;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxNearestCar(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxNearestCar(gsu.r[7], &stack);
    gsu.r[10] = stack;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxAngleProgram(void)
{

    FxAngleProgram(gsu.r[14]);
}

static void entry_FxAtan2(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[0] = (uint16_t)FxAtan2(gsu.r[1], gsu.r[2], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDivideSigned(void)
{
    uint16_t ret = gsu.r[11];

    FxDivideSignedResult r_ = FxDivideSigned(gsu.r[4], gsu.r[5], gsu.r[6]);
    gsu.r[4] = (uint16_t)r_.quotient;
    gsu.r[6] = (uint16_t)r_.divisor_made;
    gsu.r[7] = (uint16_t)r_.sign_word;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDivide32By16Halving(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[4] = (uint16_t)FxDivide32By16Halving(gsu.r[0], gsu.r[4], gsu.r[6]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDivide32By16(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[4] = (uint16_t)FxDivide32By16(gsu.r[0], gsu.r[4], gsu.r[6]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxRatioClamp(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxRatioClampResult r_ = FxRatioClamp(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.results_x;
    gsu.r[2] = (uint16_t)r_.results_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDivideHalving(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[4] = (uint16_t)FxDivideHalving(gsu.r[0], gsu.r[4], gsu.r[6]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap16x32(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap16x32Result r_ = FxDrawBitmap16x32(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    gsu.r[1] = (uint16_t)r_.end_x;
    gsu.r[2] = (uint16_t)r_.end_row_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap8x16(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap8x16Result r_ = FxDrawBitmap8x16(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    gsu.r[1] = (uint16_t)r_.right_x;
    gsu.r[2] = (uint16_t)r_.next_row_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap120x48(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap120x48(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap120x24(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap120x24Result r_ = FxDrawBitmap120x24(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    gsu.r[1] = (uint16_t)r_.end_x;
    gsu.r[2] = (uint16_t)r_.end_row_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap120x32(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap120x32Result r_ = FxDrawBitmap120x32(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    gsu.r[1] = (uint16_t)r_.right_x;
    gsu.r[2] = (uint16_t)r_.bottom_row_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap120x16(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap120x16Result r_ = FxDrawBitmap120x16(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    gsu.r[1] = (uint16_t)r_.right_x;
    gsu.r[2] = (uint16_t)r_.next_row_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap64x16(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap64x16Result r_ = FxDrawBitmap64x16(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    gsu.r[1] = (uint16_t)r_.right_x;
    gsu.r[2] = (uint16_t)r_.final_row_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawBitmap16x24(void)
{
    uint16_t ret = gsu.r[11];

    FxDrawBitmap16x24Result r_ = FxDrawBitmap16x24(gsu.r[2], gsu.r[4], gsu.r[9], gsu.r[14]);
    gsu.r[1] = (uint16_t)r_.end_x;
    gsu.r[2] = (uint16_t)r_.next_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsAge(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxEffectsAge(gsu.r[2]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsRun(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxEffectsRun(gsu.r[2], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxHazeSample(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxHazeSample(gsu.r[0], gsu.r[1], gsu.r[2], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxHazePlot(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxHazePlot(gsu.r[2]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsSpawn(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxEffectsSpawn(gsu.r[0], gsu.r[2], gsu.cy);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsDraw(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxEffectsDraw(gsu.r[2], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxFrameProgram(void)
{

    FxFrameProgramResult r_ = FxFrameProgram(gsu.r[7]);
}

static void entry_FxObjectsDraw(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectsDrawResult r_ = FxObjectsDraw(gsu.r[7], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectSprite(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectSpriteResult r_ = FxObjectSprite(gsu.r[5], gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[7] = (uint16_t)r_.source_row_accum_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectSpritePull(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectSpritePullResult r_ = FxObjectSpritePull(&stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectSpriteAt(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectSpriteAtResult r_ = FxObjectSpriteAt(gsu.r[1], gsu.r[2], &stack);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.last_plot_x;
    gsu.r[2] = (uint16_t)r_.final_row;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxObjectSpriteRows(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxObjectSpriteRowsResult r_ = FxObjectSpriteRows(gsu.r[1], gsu.r[6], &stack);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.plot_end_x;
    gsu.r[2] = (uint16_t)r_.plot_end_y;
    gsu.r[7] = (uint16_t)r_.row_error_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsAdd(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxEffectsAdd(gsu.r[0], gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsAddNext(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxEffectsAddNextResult r_ = FxEffectsAddNext(gsu.r[2], gsu.r[3], gsu.r[5], gsu.r[6], gsu.r[8], gsu.r[9], &stack, gsu.r[12]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsAddUnused(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxEffectsAddUnusedResult r_ = FxEffectsAddUnused(gsu.r[2], gsu.r[3], gsu.r[5], gsu.r[6], gsu.r[8], gsu.r[9], &stack, gsu.r[12]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxEffectsAddKinds(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxEffectsAddKindsResult r_ = FxEffectsAddKinds(gsu.r[2], gsu.r[3], gsu.r[5], gsu.r[6], gsu.r[8], gsu.r[9], &stack, gsu.r[12], gsu.cy);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWeatherReset(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxWeatherReset(gsu.r[0], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWeatherDraw(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    gsu.r[2] = (uint16_t)FxWeatherDraw(gsu.r[0], gsu.r[1], gsu.r[2], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWeatherUnused(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxWeatherUnusedResult r_ = FxWeatherUnused(gsu.r[7], gsu.r[8], &stack, gsu.r[11], gsu.r[12], gsu.cy);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWeatherUnusedLoop(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxWeatherUnusedLoopResult r_ = FxWeatherUnusedLoop(gsu.r[7], gsu.r[8], &stack, gsu.r[11], gsu.r[12], gsu.cy);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWeatherUnusedClear(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxWeatherUnusedClearResult r_ = FxWeatherUnusedClear(gsu.r[7], gsu.r[8], &stack, gsu.r[11], gsu.r[12]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWeatherUnusedFull(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxWeatherUnusedFullResult r_ = FxWeatherUnusedFull(gsu.r[7], gsu.r[8], &stack, gsu.r[11], gsu.r[12]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxWeatherDropNext(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxWeatherDropNextResult r_ = FxWeatherDropNext(gsu.r[7], gsu.r[8], &stack, gsu.r[11], gsu.r[12]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelPoints(void)
{
    uint16_t ret = gsu.r[11];

    FxModelPoints(cart_ref(fxs.rombr, gsu.r[9]), fx_ldw(0x0044));
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxTiles2bppTo4bppHigh(void)
{

    FxTiles2bppTo4bppHigh();
}

static void entry_FxTiles2bppTo4bppLow(void)
{

    FxTiles2bppTo4bppLow();
}

static void entry_FxMatrixTranspose(void)
{
    uint16_t ret = gsu.r[11];

    FxMatrixTranspose(gsu.r[9]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxMatrixApply(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[9] = (uint16_t)FxMatrixApply(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[9]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxMatrixApplyVectors(void)
{
    uint16_t ret = gsu.r[11];

    FxMatrixApplyVectors(gsu.r[9], gsu.r[11]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxMatrixRotateBackProgram(void)
{

    FxMatrixRotateBackProgram();
}

static void entry_FxMatrixRotateProgram(void)
{

    FxMatrixRotateProgramResult r_ = FxMatrixRotateProgram();
    gsu.r[0] = (uint16_t)r_.last_result;
    gsu.r[1] = (uint16_t)r_.out_r1;
    gsu.r[2] = (uint16_t)r_.out_r2;
    gsu.r[3] = (uint16_t)r_.out_r3;
    gsu.r[5] = (uint16_t)r_.first_two;
    gsu.r[6] = (uint16_t)r_.matrix_last;
}

static void entry_FxMatrixFromAnglesProgram(void)
{

    FxMatrixFromAnglesProgramResult r_ = FxMatrixFromAnglesProgram();
    gsu.r[9] = (uint16_t)r_.out_r9;
    gsu.r[11] = (uint16_t)r_.out_r11;
}

static void entry_FxMatrixFromAngles01(void)
{
    uint16_t ret = gsu.r[11];

    FxMatrixFromAngles01Result r_ = FxMatrixFromAngles01(gsu.r[9]);
    gsu.r[7] = (uint16_t)r_.y_z;
    gsu.r[9] = (uint16_t)r_.where_store_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSegmentCrossZ(void)
{
    uint16_t ret = gsu.r[11];

    FxSegmentCrossZResult r_ = FxSegmentCrossZ(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[6]);
    gsu.r[1] = (uint16_t)r_.cut_x;
    gsu.r[2] = (uint16_t)r_.cut_y;
    gsu.r[3] = (uint16_t)r_.cut_z;
    gsu.r[4] = (uint16_t)r_.kept_x;
    gsu.r[5] = (uint16_t)r_.kept_y;
    gsu.r[6] = (uint16_t)r_.kept_z;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxClipYPlane(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxClipYPlaneResult r_ = FxClipYPlane(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.clipped_a_x;
    gsu.r[2] = (uint16_t)r_.clipped_a_y;
    gsu.r[3] = (uint16_t)r_.clipped_a_z;
    gsu.r[4] = (uint16_t)r_.clipped_b_x;
    gsu.r[5] = (uint16_t)r_.clipped_b_y;
    gsu.r[7] = (uint16_t)r_.clipped_b_z;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxClipXPlane(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxClipXPlaneResult r_ = FxClipXPlane(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.cut_x;
    gsu.r[2] = (uint16_t)r_.cut_y;
    gsu.r[3] = (uint16_t)r_.cut_z;
    gsu.r[4] = (uint16_t)r_.kept_x;
    gsu.r[5] = (uint16_t)r_.kept_y;
    gsu.r[7] = (uint16_t)r_.kept_z;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonClipTop(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonClipTopResult r_ = FxPolygonClipTop(gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.new_vertex_count;
    gsu.r[6] = (uint16_t)r_.last_slope;
    gsu.r[7] = (uint16_t)r_.last_vertex_z;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonClipRight(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonClipRightResult r_ = FxPolygonClipRight(gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.out_vertex_count;
    gsu.r[6] = (uint16_t)r_.clip_slope_out;
    gsu.r[7] = (uint16_t)r_.end2_z_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonClipLeft(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonClipLeftResult r_ = FxPolygonClipLeft(gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.clipped_vertex_count;
    gsu.r[6] = (uint16_t)r_.slope_scale_out;
    gsu.r[7] = (uint16_t)r_.clipped_end_z;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonClipBottom(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonClipBottomResult r_ = FxPolygonClipBottom(gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.out_vertex_count;
    gsu.r[6] = (uint16_t)r_.slope_scale_out;
    gsu.r[7] = (uint16_t)r_.end2_z_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxClipNear(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxClipNearResult r_ = FxClipNear(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[4], gsu.r[5], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.clipped_end1_x;
    gsu.r[2] = (uint16_t)r_.clipped_end1_y;
    gsu.r[3] = (uint16_t)r_.clipped_end1_z;
    gsu.r[4] = (uint16_t)r_.clipped_end2_x;
    gsu.r[5] = (uint16_t)r_.clipped_end2_y;
    gsu.r[7] = (uint16_t)r_.clipped_end2_z;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonClipNear(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonClipNearResult r_ = FxPolygonClipNear(gsu.r[0], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.clipped_vertex_count;
    gsu.r[6] = (uint16_t)r_.last_in_vertex_ptr;
    gsu.r[7] = (uint16_t)r_.clipped_end_z;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelCommands(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelCommandsResult r_ = FxModelCommands(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelDraw(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelDrawResult r_ = FxModelDraw(gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelDrawFlagged(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelDrawFlaggedResult r_ = FxModelDrawFlagged(gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelDrawFlags8000(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelDrawFlags8000Result r_ = FxModelDrawFlags8000(gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelDrawWithFlags(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelDrawWithFlagsResult r_ = FxModelDrawWithFlags(gsu.r[0], gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelReturn(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelReturnResult r_ = FxModelReturn(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.draw_list_start_out;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelFramePick(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelFramePickResult r_ = FxModelFramePick(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelJump(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelJumpResult r_ = FxModelJump(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelPointsFull(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelPointsFullResult r_ = FxModelPointsFull(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelPointsScaled(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelPointsScaledResult r_ = FxModelPointsScaled(gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelPoints01(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelPoints01Result r_ = FxModelPoints01(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.last_matrix_word;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelPointsMirrored(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelPointsMirroredResult r_ = FxModelPointsMirrored(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelProject(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelProjectResult r_ = FxModelProject(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelProjectEach(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelProjectEachResult r_ = FxModelProjectEach(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelProjectRest(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelProjectRestResult r_ = FxModelProjectRest(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelListStart(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelListStartResult r_ = FxModelListStart(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelCall(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelCallResult r_ = FxModelCall(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelReturnPull(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelReturnPullResult r_ = FxModelReturnPull(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.draw_list_start_out;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelIfCall(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelIfCallResult r_ = FxModelIfCall(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelListStart3C(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelListStart3CResult r_ = FxModelListStart3C(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelPushReturn(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelPushReturnResult r_ = FxModelPushReturn(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.draw_list_start_out;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelReturn40(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelReturn40Result r_ = FxModelReturn40(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.draw_list_start_out;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelIfSkip(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelIfSkipResult r_ = FxModelIfSkip(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelDepthSort(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelDepthSortResult r_ = FxModelDepthSort(gsu.r[6], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.deepest_depth_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelIfSprite(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelIfSpriteResult r_ = FxModelIfSprite(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.draw_list_start_out;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelSprite(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelSpriteResult r_ = FxModelSprite(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.draw_list_start_out;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelMaterial(void)
{
    uint16_t ret = gsu.r[11];

    gsu.r[0] = (uint16_t)FxModelMaterial(gsu.r[14]);
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelFaces(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelFacesResult r_ = FxModelFaces(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.list_write_ptr_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelFacesGatheredUV(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelFacesGatheredUVResult r_ = FxModelFacesGatheredUV(gsu.r[0], gsu.r[6], gsu.r[7], &stack);
    gsu.r[10] = stack;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelFacesOnScreenUV(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelFacesOnScreenUVResult r_ = FxModelFacesOnScreenUV(gsu.r[6], gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.divisor_made_out;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelEnd(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelEndResult r_ = FxModelEnd(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.model_state_r6;
    gsu.r[7] = (uint16_t)r_.draw_list_end_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelSpriteColour(void)
{
    uint16_t ret = gsu.r[11];

    FxModelSpriteColourResult r_ = FxModelSpriteColour(gsu.r[6], gsu.r[14]);
    gsu.r[6] = (uint16_t)r_.draw_list_start_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSpriteMaterial(void)
{
    uint16_t ret = gsu.r[11];

    FxSpriteMaterialResult r_ = FxSpriteMaterial(gsu.r[0], gsu.r[2], gsu.r[6]);
    gsu.r[6] = (uint16_t)r_.shade_row_base_out;
    gsu.r[14] = (uint16_t)r_.rom_at;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxModelFacing(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxModelFacingResult r_ = FxModelFacing(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.prev_end_y;
    gsu.r[7] = (uint16_t)r_.cross_term_high_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonClipScreenUV(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonClipScreenUVResult r_ = FxPolygonClipScreenUV(gsu.r[0], gsu.r[6], gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.clipped_vertex_count;
    gsu.r[6] = (uint16_t)r_.divisor_made_out;
    gsu.r[7] = (uint16_t)r_.quotient_sign;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxReturnR11(void)
{
    uint16_t ret = gsu.r[11];

    FxReturnR11();
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPointsGather(void)
{
    uint16_t ret = gsu.r[11];

    FxPointsGatherResult r_ = FxPointsGather(gsu.r[9], gsu.r[14]);
    gsu.r[7] = (uint16_t)r_.vector_3;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonNearClip(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonNearClipResult r_ = FxPolygonNearClip(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.clipped_vertex_count;
    gsu.r[6] = (uint16_t)r_.last_in_vertex_ptr;
    gsu.r[7] = (uint16_t)r_.vertex_end_ptr;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonDraw(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonDrawResult r_ = FxPolygonDraw(gsu.r[5], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.count;
    gsu.r[6] = (uint16_t)r_.last_clip_slope;
    gsu.r[7] = (uint16_t)r_.out_list_end;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonProject(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonProjectResult r_ = FxPolygonProject(&stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.count;
    gsu.r[6] = (uint16_t)r_.read_last;
    gsu.r[7] = (uint16_t)r_.src_ptr_end;
    gsu.r[8] = (uint16_t)r_.dst_ptr_end;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxProjectPoint(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxProjectPointResult r_ = FxProjectPoint(gsu.r[1], gsu.r[2], gsu.r[3], &stack);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.xor;
    gsu.r[1] = (uint16_t)r_.screen_position_x;
    gsu.r[2] = (uint16_t)r_.screen_position_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxProjectScaled(void)
{
    uint16_t ret = gsu.r[11];

    FxProjectScaledResult r_ = FxProjectScaled(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[5]);
    gsu.r[1] = (uint16_t)r_.screen_position_x;
    gsu.r[2] = (uint16_t)r_.screen_position_y;
    gsu.r[5] = (uint16_t)r_.size_1;
    gsu.r[6] = (uint16_t)r_.scale;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxProjectVertex(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxProjectVertexResult r_ = FxProjectVertex(gsu.r[1], gsu.r[2], gsu.r[3], gsu.r[6], gsu.r[9], &stack, gsu.r[12], gsu.r[13]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.outcode_word;
    gsu.r[1] = (uint16_t)r_.screen_x;
    gsu.r[2] = (uint16_t)r_.screen_y;
    gsu.r[3] = (uint16_t)r_.exit_aux_a;
    gsu.r[9] = (uint16_t)r_.exit_aux_c;
    gsu.r[13] = (uint16_t)r_.exit_aux_b;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonGather(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonGatherResult r_ = FxPolygonGather(gsu.r[0], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.final_vertex_count;
    gsu.r[6] = (uint16_t)r_.kept_z_out;
    gsu.r[7] = (uint16_t)r_.final_src_ptr;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxPolygonClipScreen(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxPolygonClipScreenResult r_ = FxPolygonClipScreen(gsu.r[0], gsu.r[6], gsu.r[7], &stack);
    gsu.r[10] = stack;
    gsu.r[0] = (uint16_t)r_.clipped_vertex_count;
    gsu.r[6] = (uint16_t)r_.prev_end_y_out;
    gsu.r[7] = (uint16_t)r_.divide_remainder_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxDrawList(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxDrawListResult r_ = FxDrawList(gsu.r[6], gsu.r[7], &stack, gsu.r[14]);
    gsu.r[10] = stack;
    gsu.r[6] = (uint16_t)r_.caller_list_end_out;
    gsu.r[7] = (uint16_t)r_.caller_write_ptr_out;
    gsu.r[14] = (uint16_t)r_.rom_at_out;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxScreenClear(void)
{
    uint16_t ret = gsu.r[11];

    FxScreenClear();
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxSortObjectsProgram(void)
{

    FxSortObjectsProgram();
}

static void entry_FxTableEntryWords(void)
{
    uint16_t ret = gsu.r[11];

    FxTableEntryWordsResult r_ = FxTableEntryWords(gsu.r[0]);
    gsu.r[4] = (uint16_t)r_.entry_out;
    gsu.r[5] = (uint16_t)r_.address;
    fx_code(0x040000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxTable2702Program(void)
{

    FxTable2702ProgramResult r_ = FxTable2702Program();
    gsu.r[0] = (uint16_t)r_.last_byte;
    gsu.r[1] = (uint16_t)r_.out_r1;
    gsu.ov = (uint8_t)r_.out_rV;
    gsu.cy = (uint8_t)r_.out_rC;
}

static void entry_FxTextNotice(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxTextNoticeResult r_ = FxTextNotice(gsu.r[2], &stack);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.text_draw_result;
    gsu.r[2] = (uint16_t)r_.notice_car;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

static void entry_FxTextProgram(void)
{

    FxTextProgram(gsu.r[1]);
}

static void entry_FxTextDraw(void)
{
    uint16_t stack = gsu.r[10];
    uint16_t ret = gsu.r[11];

    FxTextDrawResult r_ = FxTextDraw(gsu.r[1], &stack);
    gsu.r[10] = stack;
    gsu.r[1] = (uint16_t)r_.end_x;
    gsu.r[2] = (uint16_t)r_.end_y;
    fx_code(0x010000 | ret, 1);          /* the byte the return goes to */
}

const FxEntry fx_entries[] = {
    {0x018000, entry_FxModelCommands, "FxModelCommands"},
    {0x018186, entry_FxDivideSigned, "FxDivideSigned"},
    {0x0181BC, entry_FxDivide32By16Halving, "FxDivide32By16Halving"},
    {0x0181CB, entry_FxDivide32By16, "FxDivide32By16"},
    {0x0181E3, entry_FxAngleProgram, "FxAngleProgram"},
    {0x0181F7, entry_FxAtan2, "FxAtan2"},
    {0x018277, entry_FxMatrixRotateBackProgram, "FxMatrixRotateBackProgram"},
    {0x0182CE, entry_FxMatrixRotateProgram, "FxMatrixRotateProgram"},
    {0x018325, entry_FxMatrixFromAnglesProgram, "FxMatrixFromAnglesProgram"},
    {0x01832F, entry_FxMatrixFromAngles01, "FxMatrixFromAngles01"},
    {0x018561, entry_FxModelDraw, "FxModelDraw"},
    {0x018576, entry_FxModelDrawFlagged, "FxModelDrawFlagged"},
    {0x018584, entry_FxModelDrawFlags8000, "FxModelDrawFlags8000"},
    {0x01858F, entry_FxModelDrawWithFlags, "FxModelDrawWithFlags"},
    {0x01884A, entry_FxModelReturn, "FxModelReturn"},
    {0x01884F, entry_FxModelFramePick, "FxModelFramePick"},
    {0x018863, entry_FxModelJump, "FxModelJump"},
    {0x01886C, entry_FxModelPointsFull, "FxModelPointsFull"},
    {0x0188F3, entry_FxModelPointsScaled, "FxModelPointsScaled"},
    {0x018A97, entry_FxModelPoints01, "FxModelPoints01"},
    {0x018B0E, entry_FxModelPointsMirrored, "FxModelPointsMirrored"},
    {0x018BAA, entry_FxModelProject, "FxModelProject"},
    {0x018BAE, entry_FxModelProjectEach, "FxModelProjectEach"},
    {0x018BFF, entry_FxProjectVertex, "FxProjectVertex"},
    {0x018C79, entry_FxModelProjectRest, "FxModelProjectRest"},
    {0x018D4D, entry_FxModelListStart, "FxModelListStart"},
    {0x018D5D, entry_FxModelCall, "FxModelCall"},
    {0x018D75, entry_FxModelReturnPull, "FxModelReturnPull"},
    {0x018D7A, entry_FxModelIfCall, "FxModelIfCall"},
    {0x018DF6, entry_FxModelListStart3C, "FxModelListStart3C"},
    {0x018E06, entry_FxModelPushReturn, "FxModelPushReturn"},
    {0x018E14, entry_FxModelReturn40, "FxModelReturn40"},
    {0x018E19, entry_FxModelIfSkip, "FxModelIfSkip"},
    {0x018EA7, entry_FxModelDepthSort, "FxModelDepthSort"},
    {0x018F05, entry_FxModelIfSprite, "FxModelIfSprite"},
    {0x018F18, entry_FxModelSprite, "FxModelSprite"},
    {0x018F7D, entry_FxModelMaterial, "FxModelMaterial"},
    {0x0190CD, entry_FxModelFaces, "FxModelFaces"},
    {0x0191E7, entry_FxModelFacesGatheredUV, "FxModelFacesGatheredUV"},
    {0x019202, entry_FxModelFacesOnScreenUV, "FxModelFacesOnScreenUV"},
    {0x019243, entry_FxModelEnd, "FxModelEnd"},
    {0x019250, entry_FxModelSpriteColour, "FxModelSpriteColour"},
    {0x019276, entry_FxSpriteMaterial, "FxSpriteMaterial"},
    {0x0193A7, entry_FxModelFacing, "FxModelFacing"},
    {0x01947A, entry_FxProjectPoint, "FxProjectPoint"},
    {0x019658, entry_FxRatioClamp, "FxRatioClamp"},
    {0x0196C0, entry_FxDivideHalving, "FxDivideHalving"},
    {0x01972B, entry_FxMatrixTranspose, "FxMatrixTranspose"},
    {0x01975B, entry_FxMatrixApplyVectors, "FxMatrixApplyVectors"},
    {0x01978B, entry_FxMatrixApply, "FxMatrixApply"},
    {0x01981A, entry_FxSegmentCrossZ, "FxSegmentCrossZ"},
    {0x0198D7, entry_FxPolygonGather, "FxPolygonGather"},
    {0x019A52, entry_FxPolygonClipScreenUV, "FxPolygonClipScreenUV"},
    {0x01A25F, entry_FxPolygonClipScreen, "FxPolygonClipScreen"},
    {0x01A931, entry_FxDrawList, "FxDrawList"},
    {0x01AB11, entry_FxReturnR11, "FxReturnR11"},
    {0x01AB13, entry_FxFrameProgram, "FxFrameProgram"},
    {0x01AB81, entry_FxObjectsDraw, "FxObjectsDraw"},
    {0x01AEB9, entry_FxScreenClear, "FxScreenClear"},
    {0x01AEF8, entry_FxSortObjectsProgram, "FxSortObjectsProgram"},
    {0x01B0D9, entry_FxObjectSprite, "FxObjectSprite"},
    {0x01B147, entry_FxObjectSpritePull, "FxObjectSpritePull"},
    {0x01B17D, entry_FxObjectSpriteAt, "FxObjectSpriteAt"},
    {0x01B2A4, entry_FxProjectScaled, "FxProjectScaled"},
    {0x01B2ED, entry_FxObjectSpriteRows, "FxObjectSpriteRows"},
    {0x01B6D5, entry_FxEffectsAdd, "FxEffectsAdd"},
    {0x01B763, entry_FxEffectsAddNext, "FxEffectsAddNext"},
    {0x01B7E6, entry_FxEffectsAddUnused, "FxEffectsAddUnused"},
    {0x01B856, entry_FxEffectsAddKinds, "FxEffectsAddKinds"},
    {0x01BA13, entry_FxParticlesDraw, "FxParticlesDraw"},
    {0x01BBC7, entry_FxTimersProgram, "FxTimersProgram"},
    {0x01BC35, entry_FxWeatherReset, "FxWeatherReset"},
    {0x01BC7D, entry_FxWeatherDraw, "FxWeatherDraw"},
    {0x01BF2F, entry_FxWeatherUnused, "FxWeatherUnused"},
    {0x01BF32, entry_FxWeatherUnusedLoop, "FxWeatherUnusedLoop"},
    {0x01BF35, entry_FxWeatherUnusedClear, "FxWeatherUnusedClear"},
    {0x01BF3B, entry_FxWeatherUnusedFull, "FxWeatherUnusedFull"},
    {0x01BF4D, entry_FxWeatherDropNext, "FxWeatherDropNext"},
    {0x01BF6D, entry_FxEffectsRun, "FxEffectsRun"},
    {0x01BFB7, entry_FxHazeSample, "FxHazeSample"},
    {0x01C039, entry_FxHazePlot, "FxHazePlot"},
    {0x01C060, entry_FxEffectsAge, "FxEffectsAge"},
    {0x01C130, entry_FxEffectsSpawn, "FxEffectsSpawn"},
    {0x01C175, entry_FxEffectsDraw, "FxEffectsDraw"},
    {0x01C1BE, entry_FxPointsGather, "FxPointsGather"},
    {0x01C21E, entry_FxPolygonNearClip, "FxPolygonNearClip"},
    {0x01C339, entry_FxPolygonDraw, "FxPolygonDraw"},
    {0x01C412, entry_FxPolygonProject, "FxPolygonProject"},
    {0x01C4AA, entry_FxClipYPlane, "FxClipYPlane"},
    {0x01C668, entry_FxPolygonClipTop, "FxPolygonClipTop"},
    {0x01C77A, entry_FxClipXPlane, "FxClipXPlane"},
    {0x01C933, entry_FxPolygonClipRight, "FxPolygonClipRight"},
    {0x01CA42, entry_FxPolygonClipLeft, "FxPolygonClipLeft"},
    {0x01CB63, entry_FxPolygonClipBottom, "FxPolygonClipBottom"},
    {0x01CC87, entry_FxClipNear, "FxClipNear"},
    {0x01CE34, entry_FxPolygonClipNear, "FxPolygonClipNear"},
    {0x01D12F, entry_FxTextNotice, "FxTextNotice"},
    {0x01D1EB, entry_FxTable2702Program, "FxTable2702Program"},
    {0x01D307, entry_FxTextProgram, "FxTextProgram"},
    {0x01D311, entry_FxTextDraw, "FxTextDraw"},
    {0x01D4E4, entry_FxDrawBitmap16x32, "FxDrawBitmap16x32"},
    {0x01D5D2, entry_FxDrawBitmap8x16, "FxDrawBitmap8x16"},
    {0x01D652, entry_FxDrawBitmap120x48, "FxDrawBitmap120x48"},
    {0x01DCD6, entry_FxDrawBitmap120x24, "FxDrawBitmap120x24"},
    {0x01E35A, entry_FxDrawBitmap120x32, "FxDrawBitmap120x32"},
    {0x01E9DE, entry_FxDrawBitmap120x16, "FxDrawBitmap120x16"},
    {0x01F062, entry_FxDrawBitmap64x16, "FxDrawBitmap64x16"},
    {0x01F3E4, entry_FxDrawBitmap16x24, "FxDrawBitmap16x24"},
    {0x048800, entry_FxInit, "FxInit"},
    {0x048822, entry_FxInitRamBad, "FxInitRamBad"},
    {0x04888E, entry_FxPoolsMergeProgram, "FxPoolsMergeProgram"},
    {0x048898, entry_FxPoolsMerge, "FxPoolsMerge"},
    {0x04893A, entry_FxPoolTake, "FxPoolTake"},
    {0x048A08, entry_FxObjectFree, "FxObjectFree"},
    {0x048AA2, entry_FxObjectFreeChildren, "FxObjectFreeChildren"},
    {0x048B33, entry_FxRecordNew426, "FxRecordNew426"},
    {0x048B64, entry_FxRecordNew422, "FxRecordNew422"},
    {0x048C0A, entry_FxRecordNew424, "FxRecordNew424"},
    {0x048C3B, entry_FxRecordNew426B, "FxRecordNew426B"},
    {0x048C6C, entry_FxRecordNewCamera, "FxRecordNewCamera"},
    {0x048C95, entry_FxRecordNew42D, "FxRecordNew42D"},
    {0x048CC6, entry_FxCarNew, "FxCarNew"},
    {0x048E84, entry_FxCarAddPart, "FxCarAddPart"},
    {0x048F48, entry_FxCarFromScript, "FxCarFromScript"},
    {0x048FCA, entry_FxPhysicsProgram, "FxPhysicsProgram"},
    {0x04907A, entry_FxObjectBounce, "FxObjectBounce"},
    {0x0490E8, entry_FxObjectMarkTouched, "FxObjectMarkTouched"},
    {0x04910A, entry_FxObjectFall, "FxObjectFall"},
    {0x04916D, entry_FxObjectCollideOnly, "FxObjectCollideOnly"},
    {0x049185, entry_FxObjectDrop, "FxObjectDrop"},
    {0x0491FF, entry_FxRecordCollide, "FxRecordCollide"},
    {0x049301, entry_FxCollidePush, "FxCollidePush"},
    {0x04941C, entry_FxPushesApply, "FxPushesApply"},
    {0x04944C, entry_FxPushApply, "FxPushApply"},
    {0x04957C, entry_FxContactsAnyTouch, "FxContactsAnyTouch"},
    {0x0495BE, entry_FxRecordMove, "FxRecordMove"},
    {0x049683, entry_FxPushSet, "FxPushSet"},
    {0x0496A7, entry_FxSumsClear, "FxSumsClear"},
    {0x0496C3, entry_FxSumsPushWorld, "FxSumsPushWorld"},
    {0x04973F, entry_FxSumsPushLocal, "FxSumsPushLocal"},
    {0x0497BB, entry_FxSumsRotateLocal, "FxSumsRotateLocal"},
    {0x04987D, entry_FxHitAddScaled, "FxHitAddScaled"},
    {0x0498AB, entry_FxHitAddUnit, "FxHitAddUnit"},
    {0x049927, entry_FxPushAtTurned, "FxPushAtTurned"},
    {0x049985, entry_FxPushAt4Kept, "FxPushAt4Kept"},
    {0x0499A1, entry_FxPushAt2, "FxPushAt2"},
    {0x0499C9, entry_FxPushAt1, "FxPushAt1"},
    {0x0499EE, entry_FxPushAtHalf, "FxPushAtHalf"},
    {0x049A19, entry_FxPushAtQuarter, "FxPushAtQuarter"},
    {0x049A4A, entry_FxPushAt4Twist, "FxPushAt4Twist"},
    {0x049A65, entry_FxSumScale, "FxSumScale"},
    {0x049A9B, entry_FxPushAt4, "FxPushAt4"},
    {0x049BD2, entry_FxSumsApply, "FxSumsApply"},
    {0x049C3D, entry_FxCollideModel, "FxCollideModel"},
    {0x04A053, entry_FxCollideModelSkipFace, "FxCollideModelSkipFace"},
    {0x04A0B8, entry_FxCollideModelDepthCompare, "FxCollideModelDepthCompare"},
    {0x04A124, entry_FxCollideModelUnused, "FxCollideModelUnused"},
    {0x04A127, entry_FxCollideModelUnusedStore, "FxCollideModelUnusedStore"},
    {0x04A154, entry_FxModelPoints, "FxModelPoints"},
    {0x04A1EC, entry_FxAllTouches, "FxAllTouches"},
    {0x04A20B, entry_FxRecordTouches, "FxRecordTouches"},
    {0x04A345, entry_FxContactNote, "FxContactNote"},
    {0x04A441, entry_FxListClearBit0, "FxListClearBit0"},
    {0x04A462, entry_FxListSweep, "FxListSweep"},
    {0x04A4ED, entry_FxCarMoveParts, "FxCarMoveParts"},
    {0x04A5E4, entry_FxCarMovePartsAfterWalk, "FxCarMovePartsAfterWalk"},
    {0x04A601, entry_FxCarMovePartsAfterStill, "FxCarMovePartsAfterStill"},
    {0x04A607, entry_FxPartsUpdate, "FxPartsUpdate"},
    {0x04A656, entry_FxPartPull, "FxPartPull"},
    {0x04A893, entry_FxWheelUpdate, "FxWheelUpdate"},
    {0x04AA02, entry_FxWheelUpdateNoSlot, "FxWheelUpdateNoSlot"},
    {0x04AEFF, entry_FxContactsResolve, "FxContactsResolve"},
    {0x04AF54, entry_FxContactResolve, "FxContactResolve"},
    {0x04AF90, entry_FxContactResolveNegate, "FxContactResolveNegate"},
    {0x04B1A1, entry_FxCarHandler, "FxCarHandler"},
    {0x04B6C1, entry_FxCarHandlerClamp, "FxCarHandlerClamp"},
    {0x04B799, entry_FxCarHandlerFlip, "FxCarHandlerFlip"},
    {0x04B9AC, entry_FxCarPose, "FxCarPose"},
    {0x04BA05, entry_FxSteerFromInput, "FxSteerFromInput"},
    {0x04BB17, entry_FxCarHit, "FxCarHit"},
    {0x04BC7B, entry_FxRecordHoldY, "FxRecordHoldY"},
    {0x04BCBA, entry_FxRecordAlignment, "FxRecordAlignment"},
    {0x04BD20, entry_FxCarCollide, "FxCarCollide"},
    {0x04BF51, entry_FxRecordRaise6A, "FxRecordRaise6A"},
    {0x04BF75, entry_FxRecordLower82, "FxRecordLower82"},
    {0x04BF98, entry_FxSpreadCars, "FxSpreadCars"},
    {0x04C087, entry_FxNormalize, "FxNormalize"},
    {0x04C10F, entry_FxSquareRoot, "FxSquareRoot"},
    {0x04C135, entry_FxDivideHalvedIfLarge, "FxDivideHalvedIfLarge"},
    {0x04C144, entry_FxDivide, "FxDivide"},
    {0x04C15C, entry_FxTurn, "FxTurn"},
    {0x04C1A7, entry_FxMatrixFromAngles, "FxMatrixFromAngles"},
    {0x04C2C8, entry_FxMatrixTranspose04, "FxMatrixTranspose04"},
    {0x04C2F8, entry_FxAngleOf, "FxAngleOf"},
    {0x04C378, entry_FxRecordHeadingAhead, "FxRecordHeadingAhead"},
    {0x04C3B5, entry_FxRecordSegment, "FxRecordSegment"},
    {0x04C595, entry_FxCarCatchUp, "FxCarCatchUp"},
    {0x04C65A, entry_FxCarPlaceOnSegment, "FxCarPlaceOnSegment"},
    {0x04C72F, entry_FxCarVelocityFromSegment, "FxCarVelocityFromSegment"},
    {0x04C7D5, entry_FxDriveDecide, "FxDriveDecide"},
    {0x04C933, entry_FxTrafficSides, "FxTrafficSides"},
    {0x04CA99, entry_FxRecordsApart, "FxRecordsApart"},
    {0x04CBE8, entry_FxBearingTo, "FxBearingTo"},
    {0x04CC06, entry_FxCourseNearest, "FxCourseNearest"},
    {0x04CCA4, entry_FxCourseNextEntry, "FxCourseNextEntry"},
    {0x04CD05, entry_FxCoursePrevEntry, "FxCoursePrevEntry"},
    {0x04CD56, entry_FxCourseEntryRead, "FxCourseEntryRead"},
    {0x04CD83, entry_FxTableEntryWords, "FxTableEntryWords"},
    {0x04CD98, entry_FxAngleVector, "FxAngleVector"},
    {0x04CDCA, entry_FxLapSegment, "FxLapSegment"},
    {0x04CE1D, entry_FxRankCars, "FxRankCars"},
    {0x04CE65, entry_FxRankNext, "FxRankNext"},
    {0x04CEA5, entry_FxRankNextLapZero, "FxRankNextLapZero"},
    {0x04CED6, entry_FxCourseNextDistance, "FxCourseNextDistance"},
    {0x04CF08, entry_FxNearestCar, "FxNearestCar"},
    {0x07EE50, entry_FxTiles2bppTo4bppHigh, "FxTiles2bppTo4bppHigh"},
    {0x07EE59, entry_FxTiles2bppTo4bppLow, "FxTiles2bppTo4bppLow"},
    {0, 0, 0}
};
