/*
 * chip.c: running the Super FX's programs from the game's code: what the
 * main CPU put in the chip's RAM before a program ran, the call of its
 * function, what it read back after it stopped.
 *
 * The chip's general registers kept their values between programs on the
 * hardware. A program's scratch that the generated functions take as
 * parameters is passed as 0 here: it is what they read before writing on
 * the paths the game reaches, and the recorded state check is the proof.
 */
#include <string.h>

#include "fx/fx.h"
#include "object/object.h"
#include "system/chip.h"
#include "system/state.h"

/*
 * @routine 7E:E2C9 GsuParamsUpdate
 * WHAT IT DOES: Copies what the renderer needs from the game into its
 * parameter block: the view's position and angles, the camera's flags and
 * $0D12.
 */
void GsuParamsUpdate(void)
{
    fx16_set(GSU_PARAM_108A, 0);        /* the original's $0D12, which nothing writes */
    for (int i = 0; i < 3; i++) {
        fx16_set((uint16_t)(FXIN_VIEW_POS + 2 * i), g.view_pos[i]);
        fx16_set((uint16_t)(FXIN_VIEW_ANGLES + 2 * i), g.view_angles[i]);
    }
    fx16_set(FXIN_CAMERA_FLAGS, g.camera_flags);
}

/*
 * @routine 03:AEB6 GsuParamsFromDp
 * WHAT IT DOES: Marks the view changed and copies its window and projection
 * into the renderer's inputs.
 */
void GsuParamsFromDp(void)
{
    g.view_ef = 0xFF;
    fx16_set(FXIN_VIEW_CENTRE_X, g.view_centre_x);
    fx16_set(FXIN_VIEW_CENTRE_Y, g.view_centre_y);
    fx16_set(FXIN_VIEW_LEFT, g.view_left);
    fx16_set(FXIN_VIEW_RIGHT, g.view_right);
    fx16_set(FXIN_VIEW_TOP, g.view_top);
    fx16_set(FXIN_VIEW_BOTTOM, g.view_bottom);
    fx16_set(FXIN_VIEW_SCALE_A, g.view_scale_32);
    fx16_set(FXIN_VIEW_SCALE_B, g.view_scale_34);
}

/*
 * @routine 02:DA34 GsuRecordsUpload
 * WHAT IT DOES: Copies the frame's records ($0CEE of them, 30 bytes each)
 * to the chip's RAM at $0AAA and stores the count.
 */
void GsuRecordsUpload(void)
{
    fx16_set(GSU_RECORDS_N, g.frame_record_count);
    memcpy(&fx_ram[FXIN_RECORDS], g.frame_records, (size_t)g.frame_record_count * 30u);
}

/*
 * @routine 03:C9EE ObjectGsu80BC
 * WHAT IT DOES: Runs program $01:80BC on an object's offset from a point
 * and returns its result.
 */
uint16_t VectorLength(uint16_t x, uint16_t z)
{
    uint16_t stack = 0x0240;

    fx16_set(FXIN_VECTOR_X, x);
    fx16_set(FXIN_VECTOR_Z, z);
    (void)FxLengthProgram(&stack);
    return fx16(FXIN_LENGTH_RESULT);
}

/*
 * @routine 02:E01C Gsu81E3
 * WHAT IT DOES: Runs program $01:81E3 on (x, y) and returns the word it
 * leaves at $0040.
 */
uint16_t VectorAngle(uint16_t x, uint16_t y)
{
    fx16_set(FXIN_VECTOR_X, x);
    fx16_set(FXIN_VECTOR_Y, y);
    FxAngleProgram(0);
    return fx16(FXIN_ANGLE_RESULT);
}

/*
 * @routine 03:AFD7 Gsu82CE
 * WHAT IT DOES: Runs program $01:82CE on the vector and leaves its three
 * results in g.view_rotated.
 */
void ViewRotateVector(uint16_t x, uint16_t y, uint16_t z)
{
    fx16_set(FXIN_VECTOR_X, x);
    fx16_set(FXIN_VECTOR_Y, y);
    fx16_set(FXIN_VECTOR_Z, z);
    (void)FxMatrixRotateProgram();
    for (int i = 0; i < 3; i++)
        g.view_rotated[i] = fx16((uint16_t)(FXIN_ROTATED + 2 * i));
}

/*
 * @routine 03:B011 Gsu8325
 * WHAT IT DOES: Runs program $01:8325 on the three view angles and copies
 * the nine matrix words it leaves into g.view_matrix.
 */
void ViewMatrixMake(void)
{
    for (int i = 0; i < 3; i++)
        fx16_set((uint16_t)(FXIN_ANGLES + 2 * i), g.view_angles_kept[i]);
    (void)FxMatrixFromAnglesProgram();
    for (int i = 0; i < 9; i++)
        g.view_matrix[i] = (int16_t)fx16((uint16_t)(FX_MATRIX_W + 2 * i));
}

/*
 * @routine 03:FE8D GsuEE88Ramp
 * WHAT IT DOES: Writes the ramp $80, $90, ... $F0 to the chip's RAM and
 * runs program $07:EE88. The program no longer reads the ramp (the GPU's
 * noise image has its own); the writes stay, because $3C00 lies in the
 * unpack area and picture 0, until the chip's RAM images are settled.
 */
void NoiseFillWithRamp(void)
{
    for (int i = 0; i < 8; i++)
        fx16_set((uint16_t)(FXIN_RAMP + 2 * i), (uint16_t)(0x80 + 0x10 * i));
    FxNoiseFill();
}

/*
 * @routine 03:AF05 ViewReset
 * WHAT IT DOES: Puts the view at the origin looking straight ahead: its
 * angles cleared and kept, the matrix made and kept, the identity set.
 */
void ViewReset(void)
{
    memset(g.view_angles, 0, sizeof g.view_angles);
    memset(g.view_angles_kept, 0, sizeof g.view_angles_kept);
    ViewMatrixMake();
    memcpy(g.view_matrix_kept, g.view_matrix, sizeof g.view_matrix_kept);
    for (int i = 0; i < 9; i++)
        g.view_matrix_identity[i] = (i % 4) == 0 ? 0x7F00 : 0;
}

/* The setup's tail: the random numbers seeded, the view state cleared,
   ViewReset and the parameters to the renderer. */
void ViewSetupTail(void)
{
    g.random_state[0] = (uint8_t)cameras[0].x;     /* $1A6C */
    g.random_state[1] = 0xA7;
    g.random_state[2] = 0x55;
    g.random_state[3] = 0x7F;
    g.camera_flags &= 0xFF00;
    ViewReset();
    GsuParamsFromDp();
}

/*
 * @routine 03:B67B ViewDefaults
 * WHAT IT DOES: Sets the default view: the window, centre and projection a
 * race's course menu uses, then the setup tail.
 */
void ViewDefaults(void)
{
    fx16_set(FXIN_EFFECTS_SEED, 0x1234);
    fx16_set(GSU_WORD_1C4, 1);
    fx16_set(FXIN_NOT_MENU, 0);
    g.view_ef = 0xFF;
    g.view_left = 0x0000;
    g.view_right = 0x0067;
    g.view_top = 0x0000;
    g.view_bottom = 0x0058;
    g.view_centre_x = 0x0034;
    g.view_centre_y = 0x002C;
    g.view_scale_34 = 0x5D17;
    g.view_scale_32 = 0x9D89;
    ViewSetupTail();
}

/*
 * @routine 7E:E221 FrameRunGsu8FCA
 * WHAT IT DOES: Runs the chip's physics program, then does the frame's
 * object work: the trigger lists, the draw list, the object groups and the
 * records for the renderer (FrameObjectsWhileGsuRuns). The original ran
 * the object work while the chip ran; here the chip's program runs first.
 * Not yet shown equivalent: the recorded state comparison decides it.
 */
void FramePhysicsAndObjects(void)
{
    (void)FxPhysicsProgram(0, 0, 0, 0, 0);
    BucketTriggersFromView();
    ObjectDrawListBuild();
    BucketTriggersFixedArea();
    ObjectGroupsActivate();
    GsuRecordsUpload();
}

/*
 * @routine 02:DF24 ViewMatrixUpdateAndSave
 * WHAT IT DOES: Keeps the view's angles, makes the matrix and keeps it.
 */
void ViewMatrixUpdateAndSave(void)
{
    memcpy(g.view_angles_kept, g.view_angles, sizeof g.view_angles_kept);
    ViewMatrixMake();
    memcpy(g.view_matrix_kept, g.view_matrix, sizeof g.view_matrix_kept);
}

/*
 * @routine 02:DB59 ViewUpdateAndGsuAEF8
 * WHAT IT DOES: Updates the view matrix, then runs the sort program.
 */
void ViewUpdateAndSort(void)
{
    ViewMatrixUpdateAndSave();
    FxSortObjectsProgram();
}

/*
 * @routine 7F:0919 GsuAB13WithParams
 * WHAT IT DOES: Updates the renderer's parameters and runs the frame
 * program.
 */
void FramePicture(void)
{
    GsuParamsUpdate();
    (void)FxFrameProgram(0);
}

/* The chip's draw buffer: the two buffers are `$2C00` (screen base $0B) and
   `$9400` ($25); the frame program reads the start from its RAM. */
static void screen_start_set(uint16_t start)
{
    fx16_set(GSU_SCREEN_START, start);
    g.screen_start = start;
}

/*
 * @routine 03:B83C GsuScreenFlip
 * WHAT IT DOES: Swaps the Super FX's draw buffer and resets its screen
 * base, for the course menu's staged upload (stage 0 or 8).
 */
void GsuScreenFlip(void)
{
    if (g.fx_buffer) {
        g.fx_buffer = 0;
        g.fx_upload_stage = 0x0008;
    } else {
        g.fx_buffer = 1;
        g.fx_upload_stage = 0x0000;
    }
    screen_start_set(FX_PICTURE_0);
}

/*
 * @routine 02:D8CD GsuScreenToggle
 * WHAT IT DOES: Swaps the draw buffer and points the chip at the matching
 * screen.
 * @routine 0B:FC49 GsuScreenFlipMenu
 * WHAT IT DOES: The same, for the two-player screens: both are one routine
 * in the original's effect.
 */
void GsuScreenToggle(void)
{
    if (g.fx_buffer) {
        g.fx_buffer = 0;
        screen_start_set(FX_PICTURE_0);
    } else {
        g.fx_buffer = 1;
        screen_start_set(FX_PICTURE_1);
    }
}

void GsuScreenFlipMenu(void)
{
    GsuScreenToggle();
}
