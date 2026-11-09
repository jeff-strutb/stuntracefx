/*
 * race_frame.c: a race step and the race's start.
 */
#include <string.h>

#include "fx/fx.h"
#include "object/object.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"

/*
 * @routine 03:B7C5 RaceFrameStep
 * WHAT IT DOES: One step of the race the course menu shows: the frames
 * since the last one go to frames_last_step; the objects run; the camera
 * moves on (+$37 += $400, CameraFollowStep) and the tied objects face it;
 * then the view, the physics program with the object work, and the view
 * update. With show = 0 it then draws the screen and flips it.
 * The original waited here for the vertical blank to take the last
 * picture; here the caller paces the steps.
 */
void RaceFrameStep(int show)
{
    Camera *cam = &cameras[0];

    g.frames_last_step = g.frames_since_step;
    g.frames_since_step = 0;
    g.draw_side = 0;
    fx16_set(GSU_VIEW_246C, 0xFFFF);
    ObjectsRunHandlers();
    cam->word37 = (uint16_t)(cam->word37 + 0x0400);
    CameraFollowStep(cam);
    ObjectFaceTied(cam);
    ViewFromRecord(cam);
    FrameRunGsu8FCA();
    ViewUpdateAndGsuAEF8();
    if (show == 0) {
        GsuAB13WithParams();
        GsuScreenFlip();
        g.picture_uploaded = 0;
    }
}

/*
 * @routine 03:B5DF RaceStart
 * WHAT IT DOES: Starts the race behind the course menu: the chip's screen
 * mode, not the two-player split, the object pools and the view defaults,
 * the screen start, the chip's setup, the course script at $05:8000, two
 * steps not shown, the camera's target, and the chip's RAM above the
 * screen cleared. The chip's clock, config and screen base (CLSR, CFGR,
 * SCBR) need nothing here.
 */
void RaceStart(void)
{
    g.fx_screen_mode = 3;
    g.split_screen = 0;
    fx16_set(0x004E, 0xA545);
    ObjectPoolInit();
    ViewDefaults();
    fx16_set(GSU_SCREEN_START, 0x2C00);
    g.screen_start = 0x2C00;
    fx16_set(GSU_WORD_1C4, 2);
    fx16_set(FXIN_NOT_MENU, 0);
    FxInit();
    g.word_116a = 0;
    CourseScriptRun(0x05, 0x0000);
    RaceFrameStep(1);
    RaceFrameStep(1);
    cameras[0].object = g.word_116c;
    g.picture_uploaded = 1;
    memset(&fx_ram[0x2C00], 0, 0x3400);
}
