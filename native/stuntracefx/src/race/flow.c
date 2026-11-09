/*
 * flow.c: the race from the menus' hand-over to its end: the session's
 * states, the work the original's raster interrupt did each video frame, the
 * race's pacing, the waits the code did inside a step and the attract demos.
 *
 * Each update is one video frame of the original. A step of the race runs
 * every `race_pace` updates; between steps only the raster's work runs.
 */
#include <stdio.h>
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "fx/fx_draw.h"
#include "hud/hud.h"
#include "menu/menu.h"
#include "object/object.h"
#include "race/race.h"
#include "save/save.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

/* Video frames a race step takes; section 6 of the plan. The original's step
   took 4 to 9 frames, most often 5 and 6; a one-player race uses 6, the
   approved whole-step design's value, until the recorded medians say. */
enum { PACE_ONE_PLAYER = 6, PACE_TWO_PLAYERS = 6 };

/* What the race is doing while no step runs. */
typedef enum {
    WAIT_NONE, WAIT_OVERLAY, WAIT_CHAMPION, WAIT_END_FADE, WAIT_END_COUNT,
    WAIT_SOFT_RESET, WAIT_PAUSE, WAIT_PAUSE_LOOP
} RaceWait;

typedef enum {
    FLOW_OFF, FLOW_RACE_SETUP, FLOW_RACE_LOAD, FLOW_INTRO, FLOW_VIDEO_INIT, FLOW_ATTRACT_SETUP,
    FLOW_RACE_RUN, FLOW_DEMO
} FlowState;

/* The demo's mode: its commands, or running to the fade's end. */
enum { DEMO_COMMANDS, DEMO_TO_FADE, DEMO_WAIT_FADE };

static struct {
    FlowState state;
    RaceWait wait;
    void (*after)(void);
    int stopped;                        /* the step in progress was left */
    int finished;                       /* the session is over; back to the menus */
    int resuming;                       /* a step is going on after a pause */
    uint8_t pace_count;
    /* the demo's script */
    uint16_t demo_at, demo_frame, demo_flags;
    int demo_mode;
    int demo_ended;                     /* the step a pause held ended the demo */
} flow;

int race_flow_stopped(void) { return flow.stopped; }

void race_wait_overlay(void (*after)(void))
{
    flow.wait = WAIT_OVERLAY;
    flow.after = after;
    flow.stopped = 1;
}

void race_wait_champion(void (*after)(void))
{
    flow.wait = WAIT_CHAMPION;
    flow.after = after;
    flow.stopped = 1;
}

/* @routine 07:DCAE RaceEndCountdownFadeOn, 07:DCDF RaceEndCountdownOn */
void race_go_on(void)
{
    if (g.race_again)
        RaceRestart();
    else
        RaceEndVideoReset();
}

/* @routine 07:DC88 RaceEndCountdownFade */
void RaceEndCountdownFade(void)
{
    flow.wait = WAIT_END_FADE;
    flow.stopped = 1;
}

/* @routine 07:DCCA RaceEndCountdown */
void RaceEndCountdown(void)
{
    flow.wait = WAIT_END_COUNT;
    flow.stopped = 1;
}

/* The race's own video-frame work, one player: the chain $06, $04, $14 in
   the order the frame sees it, the blank lines before the picture first. */
static void raster_one_player(void)
{
    race_frame_upload();
    RaceVideoUpload1P();
    pic.brightness = g.fade_level & 0x0F;
    pic.blank = (g.fade_level & 0x80) != 0;
    g.frame_count++;
    g.frames_since_step++;
    FadeStep();
    JoypadAccumulate();
    sound_frame();
    RaceClockStep();
    g.time_sprite_base = 0x01D8;
    HudDrawTime();
    HudPanelsStep();
    PalettesStep();
    RaceVideoWindows1P();
}

/* Two players: $0E, $08, $0A, $0C. */
static void raster_two_players(void)
{
    race_frame_upload();
    JoypadAccumulate();
    RaceVideoUpload2P();
    pic.brightness = g.fade_level & 0x0F;
    pic.blank = (g.fade_level & 0x80) != 0;
    g.frame_count++;
    g.frames_since_step++;
    FadeStep();
    RaceClockStep();
    g.time_sprite_base = 0x0100;
    HudDrawTime();
    HudPanelASlide();
    HudPanelBSlide();
    sound_frame();
    PaletteKeptColoursStep(1, 2);
    RaceVideoSplit2P();
    g.time_sprite_base = 0x0000;
    HudDrawTime();
    PaletteKeptColoursStep(0, 0);
    WindowSplitAnimate();
}

/* One video frame of the raster's work. */
static void raster_frame(void)
{
    switch (g.raster_chain) {
    case RASTER_ONE_PLAYER:
        raster_one_player();
        break;
    case RASTER_TWO_PLAYERS:
        raster_two_players();
        break;
    default:
        break;
    }
}

/* forward */
static void soft_reset_finish(void);

/* The upload of the finished picture, three stages a picture: the race's
   part of the vertical blank's frame upload. */
void race_frame_upload(void)
{
    int shown;

    switch (g.fx_upload_stage) {
    case 0x00:
        if (!g.fx_buffer) {
            NmiFrameUploadSprites();
            return;
        }
        g.fx_upload_stage = 0x02;
        return;
    case 0x02:
        g.fx_upload_stage = 0x04;
        g.picture_uploaded = 1;
        return;
    case 0x04:
        g.fx_upload_stage = 0x06;
        shown = 0;
        break;
    case 0x06:
        if (g.fx_buffer) {
            NmiFrameUploadSprites();
            return;
        }
        g.fx_upload_stage = 0x08;
        return;
    case 0x08:
        g.fx_upload_stage = 0x0A;
        g.picture_uploaded = 1;
        return;
    case 0x0A:
        g.fx_upload_stage = 0x00;
        shown = 1;
        break;
    default:
        return;
    }
    race_picture_shown(shown);
    g.picture_flip_0d81 = 0xFFFF;
    g.frame_stage_00 = 0;
    g.oam_upload_wanted--;
}

/* One update of a wait; the race's steps resume when it is over. */
static void wait_step(void)
{
    uint8_t v;

    switch (flow.wait) {
    case WAIT_END_FADE:
        if (!g.finish_timer) {
            if (g.fade_level & 0x80) {
                flow.wait = WAIT_NONE;
                RaceEndLeave();
            }
            return;
        }
        v = --g.finish_timer;
        if (v)
            return;
        if (g.hud_mode) {
            g.overlay_d59 = 0;
            SoundPort0QueuePushRaw(0x30);
            race_wait_champion(race_go_on);
            return;
        }
        g.fade_out = 0x01;
        return;
    case WAIT_END_COUNT:
        if (--g.time_up_timer)
            return;
        g.overlay_d59 = 0;
        race_wait_champion(race_go_on);
        return;
    case WAIT_OVERLAY:
        if (g.fade_level & 0x80) {
            flow.wait = WAIT_NONE;
            if (flow.after)
                flow.after();
            return;
        }
        OverlayFrameStep();
        frame_state.pressed_copy[0] = frame_state.pressed_copy[1] = 0;
        frame_state.held[0] = frame_state.held[1] = 0;
        return;
    case WAIT_CHAMPION:
        if (ChampionshipScreenFrame()) {
            flow.wait = WAIT_NONE;
            if (flow.after)
                flow.after();
        }
        return;
    case WAIT_SOFT_RESET:
        if (--g.panel_delay)
            return;
        soft_reset_finish();
        return;
    case WAIT_PAUSE:
    case WAIT_PAUSE_LOOP:
        PauseFrameStep();
        return;
    default:
        return;
    }
}

/* What follows the frame in the race's loop. */
static void race_step_end(void)
{
    if (!flow.stopped && g.object_frames >= 0x1E)
        g.byte_0713 = 0;
}

/* One step of the race. */
static void race_step(void)
{
    flow.stopped = 0;
    SpriteSlot1Countdown();
    if (g.split_screen)
        RaceFrameRunTwoPlayers();
    else
        RaceFrameRun();
    race_step_end();
}

/* forward */
static int demo_after_step(void);

void race_pause_begin(int leaves)
{
    flow.wait = leaves ? WAIT_PAUSE_LOOP : WAIT_PAUSE;
    flow.stopped = 1;
}

int race_resuming(void) { return flow.resuming; }
void race_resume_done(void) { flow.resuming = 0; }

/* The pause is over: the step it held goes on from there. */
void race_pause_over(int second_half)
{
    if (flow.wait != WAIT_PAUSE)
        return;
    flow.wait = WAIT_NONE;
    flow.stopped = 0;
    flow.resuming = 1;
    if (g.split_screen)
        RaceFrameRunTwoPlayersResume(second_half);
    else
        RaceFrameRunResume();
    flow.resuming = 0;
    if (flow.state == FLOW_DEMO) {
        if (!flow.stopped && demo_after_step())
            flow.demo_ended = 1;
    } else {
        race_step_end();
    }
}

static uint8_t race_pace(void)
{
    return g.split_screen ? PACE_TWO_PLAYERS : PACE_ONE_PLAYER;
}

/* @routine 03:EC24 CourseDataPointer: the course script's start. */
static void course_script_start(void)
{
    uint16_t i = (uint16_t)((g.course * 3) & 0x00FF);

    CourseScriptRun(cart8(0x03, (uint16_t)(0xF4A7 + i)),
                    (uint16_t)(cart16(0x03, (uint16_t)(0xF4A5 + i)) - 0x8000));
}

/* @routine 03:8BF8 BootWarmAttract's start and BootWarm's end: the race's setup. */
static void attract_setup(void)
{
    WindowsInit();
    AttractCourseSetup();
    course_script_start();
    ObjectPlaceAll();
    fx8_set(0x023A, 0);
    g.byte_1a32 = 0;
    g.object_frames = 0;
    g.byte_0713 = 1;
    FxInit();
    g.picture_uploaded = 1;
}

/* @routine 02:E289 RaceLoop's setup: the colours packed, the split-screen
   setting, the sounds. The course screen it goes on to show is not converted
   yet: the race starts directly. */
static void race_load(void)
{
    static int said;

    sound_muted = 0;                    /* $0E4E, as BootWarm clears it */
    g.player_name = (uint16_t)((g.car_settings[0] & 0x1F)
                               | ((uint16_t)(g.car_settings[1] & 0x1F) << 5)
                               | ((uint16_t)(g.car_settings[2] & 0x1F) << 10));
    g.split_screen = cart8(0x02, (uint16_t)(0xE338 + g.hud_mode));
    RaceLoadSound();
    SoundReset();
    sound_set_port0(0x01);
    g.fx_screen_mode = 0x03;
    if (!said) {
        said = 1;
        fprintf(stderr, "not converted yet: the course screen before a race (RaceLoad, RaceLoop)\n");
    }
}

void RaceRestart(void)
{
    pic.blank = 1;
    flow.wait = WAIT_NONE;
    flow.stopped = 1;
    flow.state = FLOW_RACE_LOAD;
}

void RaceEndToAttract(void)
{
    pic.blank = 1;
    flow.wait = WAIT_NONE;
    flow.stopped = 1;
    flow.state = FLOW_ATTRACT_SETUP;
}

void RaceEndVideoReset(void)
{
    pic.blank = 1;
    flow.wait = WAIT_NONE;
    flow.stopped = 1;
    flow.state = FLOW_VIDEO_INIT;
}

/* @routine 07:EBE9 SoftResetToBootWarm: the sound data reloaded, back to the course menu. */
void SoftResetToBootWarm(void)
{
    pic.blank = 1;
    SoftResetToBootWarmSound();
    flow.wait = WAIT_NONE;
    flow.stopped = 1;
    flow.finished = 1;
    session_race_ended(1);
}

void race_leave_to_champion(void)
{
    flow.wait = WAIT_NONE;
    flow.stopped = 1;
    flow.finished = 1;
    session_race_ended(1);
}

/* @routine 07:AADC SoftResetCheck's finish: the game restarted from the start. */
static void soft_reset_finish(void)
{
    pic.blank = 1;
    g.reset_state = 0;
    SoftResetApu();
    memset(&g, 0, sizeof g);
    memset(&objects, 0, sizeof objects);
    memset(fx_ram, 0, 0x2800);
    flow.wait = WAIT_NONE;
    flow.stopped = 1;
    flow.finished = 1;
    session_race_ended(0);
}

/*
 * @routine 07:AADC SoftResetCheck
 * WHAT IT DOES: The soft reset: player 1 holding L, R, START and SELECT and
 * nothing else starts the fade out and a wait of $70 frames, then restarts
 * the game.
 */
void SoftResetCheck(void)
{
    if (g.reset_state)
        return;
    if ((frame_state.held[0] & 0x3030) != 0x3030 || (frame_state.held[0] & 0xCFCF))
        return;
    g.reset_state = 0xFF;
    SoundPort1QueuePush(0x04);
    g.fade_out = 0x01;
    g.panel_delay = 0x70;
    flow.wait = WAIT_SOFT_RESET;
    flow.stopped = 1;
}

void race_session_begin(void)
{
    flow.state = FLOW_RACE_SETUP;
    flow.wait = WAIT_NONE;
    flow.stopped = 0;
    flow.finished = 0;
    flow.pace_count = 0;
}

/* One update of the race's session; nonzero when it hands back. */
int race_session_update(void)
{
    switch (flow.state) {
    case FLOW_RACE_SETUP:
        RaceModeSetup();
        Set7EFFA6Sequence();
        flow.state = FLOW_RACE_LOAD;
        /* fall through */
    case FLOW_RACE_LOAD:
        race_load();
        flow.state = FLOW_VIDEO_INIT;
        break;
    case FLOW_VIDEO_INIT:
        RaceVideoInit();
        flow.state = FLOW_ATTRACT_SETUP;
        break;
    case FLOW_ATTRACT_SETUP:
        attract_setup();
        flow.state = FLOW_RACE_RUN;
        flow.pace_count = 0;
        break;
    case FLOW_RACE_RUN:
        raster_frame();
        if (flow.wait != WAIT_NONE) {
            wait_step();
        } else if (++flow.pace_count >= race_pace()) {
            flow.pace_count = 0;
            race_step();
        }
        break;
    default:
        break;
    }
    if (flow.finished)
        g.raster_chain = RASTER_NONE;
    return flow.finished;
}

/* ---- the attract demos ---- */

/*
 * @routine 03:8683 DemoRun
 * WHAT IT DOES: One attract demo: the setup, then the script's commands
 * between race steps until it ends.
 */
static void demo_setup(uint16_t offset)
{
    uint16_t at = offset;

    g.course = (uint8_t)cart16(0x03, (uint16_t)(0x8004 + 2 + at));
    flow.demo_flags = 0x0020;
    g.selection_handler[0] = cart16(0x03, (uint16_t)(0x8004 + at));
    g.hud_mode = 0;
    Set7EFFA6Sequence();
    RaceVideoInit();
    WindowsInit();
    AttractCourseSetup();
    g.hud_mode = 3;
    course_script_start();
    ObjectPlaceAll();
    FxInit();
    g.picture_uploaded = 1;
    g.object_frames = 0;
    flow.demo_frame = 0;
    ObjectsRunHandlers();
    {
        uint16_t rec = fx16(FXIN_PLAYER1_RECORD);

        rec_or(rec, FXREC_FLAGS64, 0x0008);
    }
    fx16_set(FXIN_TEXT_NOTICE, 0);
    flow.demo_at = (uint16_t)(at + 3);
}

static int demo_fade_done(void) { return (g.fade_level & 0x80) != 0; }

static void demo_fade_out(void) { g.fade_out = 0x01; }

/* The camera moved: CameraFollowStep and ObjectFaceTied. */
static void demo_camera(void)
{
    CameraFollowStep(&cameras[0]);
    ObjectFaceTied(&cameras[0]);
}

/* The commands before a step; returns 1 when the demo ended. */
static int demo_commands(void)
{
    for (;;) {
        uint16_t at = flow.demo_at;
        uint8_t c = cart8(0x03, (uint16_t)(0x8004 + at));

        switch (c) {
        case 0x01:
            ObjectLinkedSet1A(obj_at(g.player_car), cart16(0x03, (uint16_t)(0x8005 + at)));
            flow.demo_at = (uint16_t)(at + 3);
            continue;
        case 0x02:
            fx16_set(FXIN_TEXT_NOTICE, cart16(0x03, (uint16_t)(0x8005 + at)));
            fx8_set(FXIN_TEXT_NOTICE + 1, cart8(0x03, (uint16_t)(0x8006 + at)));
            flow.demo_at = (uint16_t)(at + 4);
            continue;
        case 0x03:
            if (cart16(0x03, (uint16_t)(0x8005 + at)) != flow.demo_frame)
                return 0;
            flow.demo_at = (uint16_t)(at + 3);
            continue;
        case 0x05:
            flow.demo_at = (uint16_t)(at + 1);
            demo_fade_out();
            continue;
        case 0x06:
            if (!demo_fade_done())
                return 0;
            flow.demo_at = (uint16_t)(at + 1);
            continue;
        case 0x07:
            flow.demo_at = (uint16_t)(at + 1);
            windows.bands_kind = 0;
            windows.bands_state = 0x01;
            continue;
        case 0x08:
            if (windows.bands_state != 0x05)
                return 0;
            flow.demo_at = (uint16_t)(at + 1);
            continue;
        case 0x09:
            cameras[0].x = cart16(0x03, (uint16_t)(0x8005 + at));
            cameras[0].z = cart16(0x03, (uint16_t)(0x8007 + at));
            demo_camera();
            flow.demo_at = (uint16_t)(at + 5);
            continue;
        case 0x0A:
            cameras[0].y = (uint16_t)(cameras[0].y + cart16(0x03, (uint16_t)(0x8005 + at)));
            demo_camera();
            flow.demo_at = (uint16_t)(at + 3);
            continue;
        case 0x0B:
            if (!(frame_state.held[0] >> 8))
                return 2;
            demo_fade_out();
            flow.demo_mode = DEMO_WAIT_FADE;
            return 0;
        case 0x0C:
            flow.demo_flags |= cart16(0x03, (uint16_t)(0x8005 + at));
            flow.demo_at = (uint16_t)(at + 3);
            continue;
        case 0x0D:
            flow.demo_flags &= cart16(0x03, (uint16_t)(0x8005 + at));
            flow.demo_at = (uint16_t)(at + 3);
            continue;
        default:
            return 1;
        }
    }
}

/* What follows a race step in the demo; returns 1 when it ended. */
static int demo_after_step(void)
{
    flow.demo_frame++;
    if (flow.demo_mode == DEMO_TO_FADE)
        return demo_fade_done();
    if (!(flow.demo_flags & 0x0001)) {
        if (flow.demo_flags & 0x0002)
            return 0;
        if (!(frame_state.held[0] >> 8))
            return 0;
        flow.demo_flags |= 0x0041;
        demo_fade_out();
    }
    if (demo_fade_done())
        return 1;
    flow.demo_mode = DEMO_TO_FADE;
    return 0;
}

/* One step of the demo; returns 1 when it ended. */
static int demo_step(void)
{
    int r;

    if (flow.demo_mode == DEMO_TO_FADE) {
        RaceFrameRun();
        return flow.stopped ? 0 : demo_after_step();
    }
    PlayerStatsAndPaletteAnimate();
    r = demo_commands();
    if (r == 1)
        return 1;
    if (r == 2) {
        /* $0B: waiting for a button held, no race step taken */
        flow.pace_count = (uint8_t)(PACE_ONE_PLAYER - 1);
        return 0;
    }
    if (flow.demo_mode == DEMO_WAIT_FADE)
        return 0;
    RaceFrameRun();
    return flow.stopped ? 0 : demo_after_step();
}

/* One update of attract demo `which`; 0 while it runs, 1 when a button
   ended it, -1 when it ran out. */
int DemoRun(int which)
{
    static const uint16_t offsets[3] = {0x0000, 0x006B, 0x00FC};

    if (flow.state != FLOW_DEMO) {
        g.fx_screen_mode = 1;
        demo_setup(offsets[which & 3]);
        flow.state = FLOW_DEMO;
        flow.pace_count = 0;
        flow.wait = WAIT_NONE;
        flow.demo_mode = DEMO_COMMANDS;
        flow.finished = 0;
        flow.demo_ended = 0;
        return 0;
    }
    raster_frame();
    if (flow.wait != WAIT_NONE) {
        wait_step();
        if (flow.finished) {
            flow.state = FLOW_OFF;
            g.raster_chain = RASTER_NONE;
            return -1;
        }
        if (!flow.demo_ended)
            return 0;
    } else if (flow.demo_mode == DEMO_WAIT_FADE) {
        if (!demo_fade_done())
            return 0;
    } else if (++flow.pace_count < PACE_ONE_PLAYER) {
        return 0;
    } else {
        flow.pace_count = 0;
        if (!demo_step())
            return 0;
    }
    {
        int button = (flow.demo_flags & 0x0040) != 0;

        pic.blank = 1;
        flow.state = FLOW_OFF;
        flow.demo_flags = 0;
        g.raster_chain = RASTER_NONE;
        return button ? 1 : -1;
    }
}
