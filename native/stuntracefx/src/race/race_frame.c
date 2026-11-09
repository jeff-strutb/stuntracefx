/*
 * race_frame.c: a race step and the race's start.
 */
#include <string.h>

#include "audio/sound.h"
#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "hud/hud.h"
#include "menu/menu.h"
#include "object/object.h"
#include "race/race.h"
#include "save/save.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"
#include "video/video.h"

/*
 * @routine 03:B7C5 RaceFrameStep
 * WHAT IT DOES: One step of the race the course menu shows: the frames
 * since the last one go to frames_last_step; the objects run; the camera
 * moves on (+$37 += $400, CameraFollowStep) and the tied objects face it;
 * then the view, the physics program with the object work, and the view
 * update. Unless step_only is set it then draws the screen and flips it.
 * The original waited here for the vertical blank to take the last
 * picture; here the caller paces the steps. This path's screen base never
 * changes, so its drawing always goes into picture 0.
 */
void RaceFrameStep(int step_only)
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
    if (!step_only) {
        fx_draw_begin(0);
        GsuAB13WithParams();
        fx_draw_flush();
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

/* The camera a car's links lead to: the object two links on names one. */
static Camera *view_camera(uint16_t car)
{
    const Object *first = obj_at(obj_at(car)->link);

    return camera_of(first->link);
}

/* The frame's first part: the physics and the timers, the side's flag set. */
static void frame_side0(void)
{
    g.draw_side = 0;
    fx16_set(GSU_VIEW_246C, 0xFFFF);
    FrameRunGsu8FCA();
    FxTimersProgram();
}

/* The step's clock: the frames since the last step taken, and what the
   original kept as time counters; the Super FX is told the frames too. */
static void frame_start(void)
{
    g.frames_last_step = race_step_frames();
    g.frames_since_step = 0;
    g.frame_stage_00 = 0x02;
}

static void frame_time(void)
{
    g.time_1f3e = g.frames_since_step;
    g.time_sum_06d5 = (uint8_t)(g.time_sum_06d5 + g.frames_last_step);
    g.time_count_06d6++;
    if (g.time_sum_06d5 >= 0x3C) {
        g.time_fps_06d7 = g.time_count_06d6;
        g.time_count_06d6 = 0;
        g.time_sum_06d5 = (uint8_t)(g.time_sum_06d5 - 0x3C);
    }
    fx16_set(FXIN_FRAMES_108E, g.frames_last_step);
}

/*
 * @routine 7E:E303 HudFrameWhileGsuRuns
 * WHAT IT DOES: The HUD's share of a frame while the chip draws: the slide
 * and the fade value; then the HUD digits and backdrop rows, or for two
 * players the backdrop's scroll and the player's value.
 */
static void hud_frame_while_gsu_runs(void)
{
    SlideStep();
    FadeValueStep();
    if (g.split_screen) {
        BackdropScrollFromView();
        if (g.fx_buffer == 1)
            HudDrawValueC();
        else
            HudDrawValueB();
        return;
    }
    if (g.hud_mode != 1 && g.hud_mode != 3)
        HudDigitSprites62();
    if (g.hud_mode != 3)
        HudDrawDigits19EC();
    BackdropRowsUpdate();
    HudDrawValueA();
    HudColourStep();
}

/*
 * @routine 7E:E258 FrameRunGsuAB13B
 * WHAT IT DOES: A frame for panel B: the parameters, the frame program, the
 * HUD's work, the gauges, on two players the objects, the map markers.
 */
static void frame_run_ab13_b(void)
{
    GsuParamsUpdate();
    (void)FxFrameProgram(0);
    hud_frame_while_gsu_runs();
    HudGaugesPanelB();
    if (g.split_screen) {
        BucketTriggersFromView();
        ObjectDrawListBuild();
        BucketTriggersFixedArea();
        ObjectGroupsActivate();
        GsuRecordsUpload();
    }
    MapMarkersPanelB();
}

/* @routine 7E:E298 FrameRunGsuAB13A: a frame for panel A. */
static void frame_run_ab13_a(void)
{
    GsuParamsUpdate();
    (void)FxFrameProgram(0);
    hud_frame_while_gsu_runs();
    HudGaugesPanelA();
    MapMarkersPanelA();
}

/* The picture is handed over at the step's end. */
static void one_player_finish(void)
{
    fx_draw_flush();
    if (race_flow_stopped())
        return;
    GsuScreenToggle();
    g.picture_uploaded = 0;
    frame_time();
}

/*
 * @routine 02:D7CD RaceFrameRun
 * WHAT IT DOES: One step of a race: the view, the controls, the physics and
 * timers, the objects, the view again and the sort, the backdrop, the frame
 * picture with the HUD, the speed gauge and the near car's sound, the text or
 * the race's phases and HUD; then the picture is handed over.
 */
void RaceFrameRun(void)
{
    frame_start();
    ViewFromRecord(view_camera(g.player_car));
    PlayerControlsFrame();
    frame_side0();
    ObjectsRunHandlers();
    ViewUpdateAndGsuAEF8();
    BackdropAnglesFromView();
    g.draw_side = 1;
    g.hud_1ae8 = 0;
    fx_draw_begin(g.fx_buffer);
    frame_run_ab13_b();
    SpeedGaugeUpdate(0x2143);
    NearCarSoundUpdate();
    g.hud_1aea = g.hud_1ae8;
    if (fx16(FXIN_TEXT_NOTICE))
        FxTextProgram(0);
    else
        RaceSoundsAndHud();
    one_player_finish();
}

/* The rest of a step a pause held. */
void RaceFrameRunResume(void)
{
    RaceSoundsAndHud();
    one_player_finish();
}

/* From player 2's sounds to the step's end. */
static void two_players_second_half(void)
{
    RaceSoundsPlayer2();
    fx_draw_flush();
    if (race_flow_stopped())
        return;
    GsuScreenFlipMenu();
    g.picture_uploaded = 0;
    frame_time();
}

/* From player 1's sounds through player 2's picture. */
static void two_players_first_half(void)
{
    RaceSoundsPlayer1();
    fx_draw_flush();
    if (race_flow_stopped())
        return;
    GsuScreenFlipMenu();
    g.picture_uploaded = 0;
    ViewUpdateAndGsuAEF8();
    BackdropAnglesFromView();
    g.view_heading_06cf = (uint16_t)(g.view_heading_06cf - 0x0140);
    fx_draw_begin(g.fx_buffer);
    frame_run_ab13_a();
    SpeedGaugeUpdate(0x2142);
    two_players_second_half();
}

/*
 * @routine 0B:FB26 RaceFrameRunTwoPlayers
 * WHAT IT DOES: One step of a two-player race: the objects, then each
 * player's view, controls and picture in turn.
 */
void RaceFrameRunTwoPlayers(void)
{
    frame_start();
    ObjectsRunHandlers();
    ViewFromRecord(view_camera(g.player_car));
    PlayerControlsFrame();
    frame_side0();
    ViewUpdateAndGsuAEF8();
    BackdropAnglesFromView();
    ViewFromRecord(view_camera(g.player2_car));
    g.draw_side = 1;
    g.hud_1ae8 = 0;
    fx_draw_begin(g.fx_buffer);
    frame_run_ab13_b();
    SpeedGaugeUpdate(0x2143);
    two_players_first_half();
}

/* The rest of a two-player step a pause held. */
void RaceFrameRunTwoPlayersResume(int second_half)
{
    if (second_half)
        two_players_second_half();
    else
        two_players_first_half();
}

/* RaceFinishPhasesTail: $0E58 = $30, the mark and palette work cleared, sound 2. */
static void finish_tail(void)
{
    g.end_timer = 0x30;
    g.finish_mark = 0;
    g.palette_animate = 0;
    SoundPort0QueuePushRaw(0x02);
}

/*
 * @routine 07:A9D7 RaceFinishPhases0E, 07:AA0B RaceFinishPhases10
 * WHAT IT DOES: The race phases after a finish on the split screen: sound
 * $37; a player 2 finish sets player 1 $12 and player 2 `phase`; player 1's
 * sets both $14 when bit 0 of the other's flags is set, else its own `phase`
 * and player 2 $12.
 */
void RaceFinishPhases(int player, uint8_t phase)
{
    SoundPort0QueuePush(0x37);
    if (player != 0) {
        g.player_phase[0] = 0x12;
        g.player_phase[1] = phase;
    } else if (g.car_flags_low[1] & 0x01) {
        CarFinishSound();
        g.player_phase[0] = 0x14;
        g.player_phase[1] = 0x14;
    } else {
        g.player_phase[1] = 0x12;
        g.player_phase[0] = phase;
    }
    finish_tail();
}

/*
 * @routine 07:A99A RaceFinishSplit
 * WHAT IT DOES: A finish on the split screen, each player's phases and
 * sound.
 */
void RaceFinishSplit(int player)
{
    int one = 0;

    if (player == 0) {
        one = 1;
        if (g.laps_done[1] == 0x03)
            one = g.player_mode[0] == 0x01;
    }
    if (one) {
        CarFinishSound();
        g.player_phase[0] = 0x12;
        g.player_phase[1] = 0x0A;
    } else {
        CarFinishSoundP2();
        g.player_phase[1] = 0x12;
        g.player_phase[0] = 0x0A;
    }
    SoundPort0QueuePush(0x08);
    finish_tail();
}

/*
 * @routine 03:E838 RaceVarsReset
 * WHAT IT DOES: Resets the race's work variables before a race. The IRQ
 * timers, NMITIMEN and BG3 scroll the original wrote here have no meaning on
 * the engine. Addresses with no reader converted yet are not stored: $0D9E,
 * $0DA0, $0DD8-$0DD9, $0E07, $0E08, $0E0B, $0E1A, $0E4F-$0E53, $0E54, $0E90,
 * $117E, $1180, the $7F:3462, $346A, $346E, $3466, $3476,
 * $345E halves.
 */
void RaceVarsReset(void)
{
    g.backdrop_row_adjust = g.course == 0x0A ? 0xFFF0 : 0x0000;
    g.fade_low = 0;
    g.fade_level = 0;
    g.clock_step = 0x0166;
    g.fx_buffer = 0;
    fx16_set(GSU_SCREEN_START, 0x2C00);
    g.screen_start = 0x2C00;
    g.picture_flip_0d81 = 0;
    g.clock_fraction = 0;
    g.clock_seconds = 0;
    g.clock_minutes = 0;
    g.overlay_state = 0;
    g.overlay_x = 0x009A;
    g.overlay_y = 0x006A;
    g.overlay_attr = 0;
    g.view_event[0] = 0;
    g.view_event[1] = 0;
    g.picture_effects = 0;
    g.player_palette_offset[1] = 0x37D7;
    palettes.cycle_phase[0] = palettes.cycle_phase[1] = palettes.cycle_phase[2] = 0;
    palettes.weather = 0;
    palettes.amount_shown = 0;
    palettes.weather_shown = 0;
    palettes.target_shown = 0xFFFF;
    palettes.gradient_was_off = 0xFFFF;
    palettes.blend_request = 0x7FFF;
    palettes.amount = 0x001F;
    g.speed_total[0] = g.speed_total[1] = 0;
    g.speed_count[0] = g.speed_count[1] = 0;
    g.hud_hold = 0;
    g.tally_state = 0;
    g.quit_cars_shown = 0;
    g.pause_choice = 0;
    g.start_light = 0;
    sound_port2 = 0;
    sound_port3 = 0;
    g.panel_top = 0;
    g.panel_bottom = 0;
    g.lap_mark[0] = g.lap_mark[1] = g.lap_mark[2] = 0;
    g.player_phase[0] = g.player_phase[1] = 0;
    g.status_level[0] = g.status_level[1] = 0;
    g.laps_done[0] = g.laps_done[1] = 0;
    g.stunt_seen = 0;
    g.laps_counted[0] = g.laps_counted[1] = 0;
    g.status_df6[0] = g.status_df6[1] = 0;
    g.gauge_one[0] = g.gauge_one[1] = 0;
    g.car_flags_low[0] = g.car_flags_low[1] = 0;
    g.car_flags_high[0] = g.car_flags_high[1] = 0;
    g.player_mode[0] = 0x01;
    g.player_mode[1] = 0x01;
    g.frame_stage_00 = 0;
    g.split_stage_on = 0;
    g.split_stage = 0;
    g.pause_stage = 0;
    g.pause_stage_b = 0;
    g.pause_player = 0;
    g.race_ended_d55 = 0;
    g.lap_shown = 0;
    g.sprites_counted = 0;
    g.flag_0e59 = 0;
    g.fade_out = 0;
    g.banner_timer[0] = g.banner_timer[1] = 0;
    g.banner[0] = g.banner[1] = 0;
    g.current_player = 0;
    g.end_ready = 0;
    g.kept_timer[0] = g.kept_timer[1] = 0;
    g.finish_mark = 0;
    g.finish_lines = 0;
    g.start_substate = 0;
    g.palette_animate = 0x17;
    g.freeze = 0xFF;
    g.hdma_enable = 0x1C;
}

/*
 * @routine 7E:F6E9 ViewParamsByPlayers
 * WHAT IT DOES: The view's window and projection for one player or the
 * split screen.
 */
static void view_params_by_players(void)
{
    if (g.split_screen) {
        g.view_left = 0;
        g.view_right = 0x008F;
        g.view_top = 0;
        g.view_bottom = 0x0050;
        g.view_centre_x = 0x0048;
        g.view_centre_y = 0x0028;
        g.view_scale_34 = 0x6666;
        g.view_scale_32 = 0x71C7;
    } else {
        g.view_left = 0;
        g.view_right = 0x00CF;
        g.view_top = 0;
        g.view_bottom = 0x0080;
        g.view_centre_x = 0x0068;
        g.view_centre_y = 0x0040;
        g.view_scale_34 = 0x4000;
        g.view_scale_32 = 0x4EC4;
    }
}

/* @routine 7E:F29F AttractViewStart: the race view's parameters, the setup tail. */
static void attract_view_start(void)
{
    fx16_set(FXIN_DEFAULT_1D0, 0x1234);
    fx16_set(GSU_WORD_1C4, 1);
    fx8_set(FXIN_NOT_MENU, g.split_screen);
    g.view_ef = 0xFF;
    g.frame_stage_00 = 0;
    view_params_by_players();
    ViewSetupTail();
}

/*
 * @routine 7E:F21E AttractCourseSetup
 * WHAT IT DOES: The race's course setup: the held buttons cleared, the view,
 * the sprites' first slot, the HUD loaded, the chip's table program.
 */
void AttractCourseSetup(void)
{
    frame_state.held[0] = 0;
    attract_view_start();
    (void)SpriteSlotFirst();
    RaceHudLoad();
    (void)FxTable2702Program();
}

/*
 * @routine 03:D8B3 RaceModeSetup
 * WHAT IT DOES: Sets the race up for its mode: in mode 3 with no class won
 * only marks the race as skipped and takes course $1B; otherwise clears the
 * race's flags, then mode 0 unpacks the course's records, the others set the
 * count, mode 2 loading the battle driver.
 */
void RaceModeSetup(void)
{
    if (g.hud_mode == 0x03) {
        if (!(save.progress & 0x07)) {
            g.race_skipped = 0xFF;
            g.course = 0x1B;
            g.course_high = 0;
            RaceModeSetupPickSound();
            return;
        }
        g.course_class = (save.progress & 0x01) ? 0x00 : 0x01;
    } else {
        g.course_class = 0;
    }
    g.race_skipped = 0;
    g.course_scroll_low = 0;
    RaceModeSetupPickSound();
    g.time_override = 0;
    g.hud_blank = 0;
    g.champ_stage = 0;
    g.course_high = 0;
    g.byte_0d63 = 0;
    g.byte_0d44 = 0;
    memset(g.race_time, 0, sizeof g.race_time);
    g.byte_0d24 = 0;
    g.byte_0d25 = 0;
    memset(g.level_columns, g.hud_mode ? 0x00 : 0x04, sizeof g.level_columns);
    g.time_shown = 0;
    g.time_shown_2 = 0;
    if (g.hud_mode == 0x00) {
        RecordsUnpackCourseRunning();
        g.countdown_df8 = 0x02;
        return;
    }
    if (g.hud_mode == 0x02)
        RaceModeSetupBattleSound();
    g.countdown_df8 = 0xFF;
}

/* @routine 08:B893 Set7EFFA6Sequence */
void Set7EFFA6Sequence(void)
{
    for (int i = 0; i < 4; i++)
        objects.place_entries[i] = (uint16_t)(i + 1);
}

/* @routine 07:EE11 Countdown0DF8: the count down; once zero, the counted sprites' count. */
void Countdown0DF8(void)
{
    if (g.countdown_df8) {
        g.countdown_df8--;
        return;
    }
    g.sprites_counted--;
    g.sprite_count = 0;
}
