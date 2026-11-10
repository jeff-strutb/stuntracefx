/*
 * dispatch.c: the Super FX's jumps through a register to one of a known
 * set (fxentry.txt), written by the generator: the model commands'
 * (FxModelNext). The physics' two (FxRecordHandler, FxPushAtVariant) are in
 * fx/object/fx_physics.c, and FxCarFollowCourse in fx/race/fx_drive.c.
 */

#include "fx/fx.h"

#include <stdio.h>
#include <stdlib.h>

void fx_bad_dispatch(uint16_t at)
{
    fprintf(stderr, "Super FX: a jump to $%04X, none of its known places\n", at);
    abort();
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
