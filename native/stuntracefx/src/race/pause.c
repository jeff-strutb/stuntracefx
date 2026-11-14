/*
 * pause.c: the race's pause screens, one stage an update. A pause by pad 1 or
 * pad 2 in the split screen (07:BB3C, 07:BBA2 tables), the one-player pause
 * with its turning picture (07:BC24 table). The race's step in progress is
 * held until the pause ends and then goes on from where the pause started it.
 *
 * Not converted, nothing reaches them: PauseStageLeaveRts (07:BD08),
 * PauseQuitMenuCursor0 (07:C021), PauseQuitMenuBack (07:C03E).
 */
#include "audio/sound.h"
#include "hud/hud.h"
#include "menu/menu.h"
#include "pause/pause.h"
#include "race/race.h"
#include "system/game.h"
#include "system/state.h"
#include "video/video.h"

typedef enum { PAUSE_ONE_PLAYER, PAUSE_PAD_1, PAUSE_PAD_2 } PauseKind;

static PauseKind pause_kind;

/* The options menu's QUIT RACE (pause/pause.c): the game's own pause begins
   at the race's next step as Start would begin it, and takes its quit as the
   player's choice would, down the same stages to leaving the race. A request
   not taken within a second of the game's updates lapses. */
static int quit_wanted;
static uint64_t quit_at;
static int quit_driving;                /* the pause that began took the quit: to be carried through */

void race_pause_quit(void)
{
    quit_wanted = 1;
    quit_at = pause_game_updates();
}

/* the quit asked for and still wanted; taken (cleared) as the pause begins */
static int quit_take(void)
{
    if (!quit_wanted)
        return 0;
    quit_wanted = 0;
    return pause_game_updates() - quit_at <= 60;
}

/* 07:BC88: the race goes on: sound $16, the flags that hold it cleared. */
static void pause_resume(void)
{
    SoundPort1QueuePush(0x16);
    g.pause_stage_b = 0;
    g.finish_mark--;
    g.palette_animate--;
}

/* The timer counted down; 1 at its end. */
static int timer_out(void)
{
    return --g.panel_delay == 0;
}

/* @routine 07:C53A Gauge6BPlace, 07:C546 Gauge6APlace: the six-sprite gauge's
   Y, and the X high bits of the twelve sprites from 27 (x 0) or 91 (x $10)
   cleared. */
static void gauge6_place(uint8_t *at, uint8_t y, unsigned x)
{
    static const uint8_t keep[4] = {0xBF, 0xAA, 0xAA, 0xEA};
    unsigned i;

    *at = y;
    for (i = 0; i < 4; i++)
        sprite_high_set((int)((6 + i + x) * 4), (uint8_t)(sprite_high_get((int)((6 + i + x) * 4)) & keep[i]));
}

/* @routine 07:BC32 PauseStageStart */
static void stage_start(void)
{
    g.panel_delay = 0x0A;
    g.pause_stage = 0x02;
}

/* @routine 07:BC3F PauseStageWait */
static void stage_wait(void)
{
    if (timer_out())
        g.pause_stage = 0x04;
}

/* @routine 07:BC4C PauseStageResumeB */
static void stage_resume_b(void)
{
    if (!timer_out())
        return;
    gauge6_place(&g.gauge_place[6], 0x5A, 0x00);
    pause_resume();
}

/* @routine 07:BC58 PauseStageResumeA */
static void stage_resume_a(void)
{
    if (!timer_out())
        return;
    gauge6_place(&g.gauge_place[7], 0xC2, 0x10);
    pause_resume();
}

/* @routine 07:BC64 PauseStageReplayEnd */
static void stage_replay_end(void)
{
    if (!timer_out()) {
        if (g.panel_delay == 0x01)
            HudSpritesPlace();
        return;
    }
    g.gauge_place[6] = 0x93;            /* $0E14 */
    g.hud_map_y_b = 0x7C;               /* $0E0C */
    g.split_stage_on = 0;
    pause_resume();
}

static void slide_up(uint8_t kind)
{
    WindowSlideUpSound();
    windows.bands_kind = kind;
}

/* @routine 07:BCA0 PauseStageQuit */
static void stage_quit(void)
{
    if (!timer_out())
        return;
    if (!g.hud_mode) {
        g.pause_stage = 0x0A;
        slide_up(0x01);
        g.panel_delay = 0x70;
        return;
    }
    g.pause_stage = 0x0C;
    slide_up(0x00);
}

/* @routine 07:BCE0 PauseStageLeave */
static void stage_leave(void)
{
    if (g.race_skipped) {
        g.fade_out = 0x01;
        if (g.fade_level & 0x80)
            SoftResetToBootWarm();
        return;
    }
    g.results_step = 0;
    race_wait_champion(race_go_on);
}

/* @routine 07:BCD0 PauseStageQuitWait */
static void stage_quit_wait(void)
{
    if (!timer_out())
        return;
    if (g.every_course_open)
        RaceRestart();
    else
        stage_leave();
}

/* B or START: the choice taken; the quit in mode 0 of the one-player pause
   also counts one down and plays a sound. */
static void choice_made(int quit_extra)
{
    windows.layout_request = 0xFF;
    g.panel_delay = 0x08;
    if (!g.pause_choice) {
        g.pause_stage = 0x06;
        return;
    }
    g.pause_stage = 0x08;
    if (quit_extra && !g.hud_mode && !g.time_override) {
        TryUsed();
        SoundPort1QueuePush(0x22);
    }
}

/* Left on continue, right on quit. */
static void choice_move(uint16_t pressed, void (*left)(void), void (*right)(void))
{
    if (!g.pause_choice) {
        if (!(pressed & 0x0200))
            return;
        SoundPort0QueuePush(0x45);
        g.pause_choice--;
        left();
        return;
    }
    if (!(pressed & 0x0100))
        return;
    SoundPort0QueuePush(0x45);
    g.pause_choice = 0;
    right();
}

/* @routine 07:BD09 PauseStageChoice */
static void stage_choice(void)
{
    uint16_t pressed = frame_state.pressed_copy[0];

    if (quit_driving) {
        quit_driving = 0;
        choice_move(0x0200, PauseChoiceLeft, PauseChoiceRight);  /* the cursor to quit, as its button moves it */
        choice_made(1);
        return;
    }
    if (pressed & 0x9000)
        choice_made(1);
    else
        choice_move(pressed, PauseChoiceLeft, PauseChoiceRight);
}

/* @routine 07:BD8D PauseStageChoiceP1 */
static void stage_choice_p1(void)
{
    uint16_t pressed = frame_state.pressed_copy[0];

    if (quit_driving) {
        choice_move(0x0200, PauseChoiceLeftPad1, PauseChoiceRightPad1);
        choice_made(0);
        return;                         /* quit_driving stays until the quit menu takes its first row */
    }
    if (pressed & 0x9000)
        choice_made(0);
    else
        choice_move(pressed, PauseChoiceLeftPad1, PauseChoiceRightPad1);
}

/* @routine 07:BDFB PauseStageChoiceP2 */
static void stage_choice_p2(void)
{
    uint16_t pressed = frame_state.pressed_copy[1];

    if (pressed & 0x9000)
        choice_made(0);
    else
        choice_move(pressed, PauseChoiceLeftPad2, PauseChoiceRightPad2);
}

static void continue_wait(uint8_t owner)
{
    if (g.panel_delay) {
        g.panel_delay--;
        return;
    }
    g.pause_player = owner;
    windows.bands_kind = 0;
    WindowSlideUpSound();
    g.pause_stage = 0x0A;
}

/* @routine 07:BE40 PauseStageContinueP2 */
static void stage_continue_p2(void) { continue_wait(0xFF); }
/* @routine 07:BE52 PauseStageContinue */
static void stage_continue(void) { continue_wait(0x00); }

/* 07:BEAB: the quit menu opened. */
static void quit_menu_open(void)
{
    windows.bands_kind = 0;
    windows.bands_state = 0x02;
    g.records_cursor = 0;               /* $0D50: here the quit menu's cursor */
    g.pause_stage = 0x0C;
    g.split_stage_on = 0x0C;
}

/* @routine 07:BE71 PauseStageQuitMenuP2 */
static void stage_quit_menu_p2(void)
{
    if (windows.bands_state != 0x05)
        return;
    g.pause_player = 0xFF;
    g.panel_bottom = 0x01;
    g.split_stage = 0x0E;
    BottomHudSpritesOff();
    quit_menu_open();
}

/* @routine 07:BE8F PauseStageQuitMenu */
static void stage_quit_menu(void)
{
    if (windows.bands_state != 0x05)
        return;
    g.pause_player = 0x00;
    g.panel_top = 0x01;
    g.split_stage = 0x0C;
    TopHudSpritesOff();
    quit_menu_open();
}

/* @routine 07:BEC6 PauseStageQuitTaken */
static void stage_quit_taken(void)
{
    if (windows.bands_state != 0x05)
        return;
    if (g.records_cursor)
        RaceRestart();
    else
        RaceEndToAttract();
}

/* B or START in the quit menu. */
static void quit_menu_take(uint8_t *panel, uint8_t stage, void (*park)(void))
{
    uint8_t cursor = g.records_cursor;

    SoundPort0QueuePush(0x46);
    if (cursor == 0x04) {
        if (!g.quit_cars_shown) {
            g.quit_cars_shown = 0xFF;
            *panel = 0xFF;
            g.split_stage = stage;
            park();
        }
        g.records_cursor = 0x06;
    } else if (cursor > 0x04) {
        g.records_cursor = 0;
    } else {
        g.pause_stage = 0x0E;
        windows.bands_kind = 0x01;
        windows.bands_state = 0x01;
    }
}

/* Down and up move the cursor; each row shows it. */
static void quit_menu(int pad, uint8_t *panel, uint8_t stage, void (*park)(void), void (*cars)(void))
{
    uint16_t pressed = frame_state.pressed_copy[pad];
    uint8_t cursor = g.records_cursor;

    if (quit_driving && !cursor) {
        quit_driving = 0;
        quit_menu_take(panel, stage, park);     /* the first row, the one that leaves the race */
        return;
    }
    if (pressed & 0x9000) {
        quit_menu_take(panel, stage, park);
        return;
    }
    switch (cursor) {
    case 0x00:
        if (pressed & 0x0400) {
            SoundPort0QueuePush(0x45);
            g.records_cursor = 0x02;
        }
        QuitMenuRow1Cycle();
        QuitMenuRow2Plain();
        QuitMenuRow3Plain();
        return;
    case 0x02:
        if (pressed & 0x0400) {
            SoundPort0QueuePush(0x45);
            g.records_cursor = 0x04;
        } else if (pressed & 0x0800) {
            SoundPort0QueuePush(0x45);
            g.records_cursor = 0x00;
        }
        QuitMenuRow1Plain();
        QuitMenuRow2Cycle();
        QuitMenuRow3Plain();
        return;
    case 0x04:
        if (pressed & 0x0800) {
            SoundPort0QueuePush(0x45);
            g.records_cursor = 0x02;
        }
        break;
    case 0x06:
        CarSelectTwoPlayers();
        cars();
        break;
    default:
        return;
    }
    QuitMenuRow3Cycle();
    QuitMenuRow2Plain();
    QuitMenuRow1Plain();
}

/* @routine 07:BFC9 PauseQuitMenu */
static void stage_pause_quit_menu(void)
{
    quit_menu(0, &g.panel_bottom, 0x0E, BottomHudSpritesOff, SpritePiecesTwoCars);
}

/* @routine 07:BEDB PauseQuitMenuP2 */
static void stage_pause_quit_menu_p2(void)
{
    quit_menu(1, &g.panel_top, 0x0C, TopHudSpritesOff, SpritePiecesTwoCarsTop);
}

/* @routine 07:BC1C PauseReplayStages: the table at 07:BC24. */
static void stages_one_player(void)
{
    switch (g.pause_stage) {
    case 0x00: stage_start(); break;
    case 0x02: stage_wait(); break;
    case 0x04: stage_choice(); break;
    case 0x06: stage_replay_end(); break;
    case 0x08: stage_quit(); break;
    case 0x0A: stage_quit_wait(); break;
    case 0x0C: stage_leave(); break;
    default: break;
    }
}

/* @routine 07:BB34 OamStageDispatchBB3C */
static void stages_pad_1(void)
{
    switch (g.pause_stage) {
    case 0x00: stage_start(); break;
    case 0x02: stage_wait(); break;
    case 0x04: stage_choice_p1(); break;
    case 0x06: stage_resume_b(); break;
    case 0x08: stage_continue(); break;
    case 0x0A: stage_quit_menu(); break;
    case 0x0C: stage_pause_quit_menu(); break;
    case 0x0E: stage_quit_taken(); break;
    default: break;
    }
}

/* @routine 07:BB9A OamStageDispatchBBA2 */
static void stages_pad_2(void)
{
    switch (g.pause_stage) {
    case 0x00: stage_start(); break;
    case 0x02: stage_wait(); break;
    case 0x04: stage_choice_p2(); break;
    case 0x06: stage_resume_a(); break;
    case 0x08: stage_continue_p2(); break;
    case 0x0A: stage_quit_menu_p2(); break;
    case 0x0C: stage_pause_quit_menu_p2(); break;
    case 0x0E: stage_quit_taken(); break;
    default: break;
    }
}

/* One update of the pause. */
void PauseFrameStep(void)
{
    switch (pause_kind) {
    case PAUSE_ONE_PLAYER:
        stages_one_player();
        PictureTurnStep();
        break;
    case PAUSE_PAD_1:
        stages_pad_1();
        break;
    case PAUSE_PAD_2:
        stages_pad_2();
        break;
    }
    frame_state.pressed_copy[0] = 0;
    frame_state.pressed_copy[1] = 0;
    if (!g.pause_stage_b)
        race_pause_over(pause_kind == PAUSE_PAD_2);
}

/*
 * @routine 07:BBB2 PauseOnStart
 * WHAT IT DOES: The race's pause on player 1's START: the screen laid out and
 * the race held; returns 1 when it paused and the step must stop here.
 */
int PauseOnStart(void)
{
    int start = frame_state.pressed_copy[0] & 0x1000;

    if (!start && !quit_take())
        return 0;
    quit_driving = !start;
    g.pause_player = 0;
    SoundPort1QueuePush(0x15);
    windows.layout_request = 0x01;
    HudSpritesHide();
    OamLargeColumns();
    g.pause_stage_b--;
    g.finish_mark = 0;
    g.palette_animate = 0;
    g.picture_turn_delay = 0;
    g.picture_turn_pending = 0;
    g.picture_turn_number = 0;
    g.split_stage_on--;
    g.pause_stage = 0;
    g.split_stage = 0x0A;
    pause_kind = PAUSE_ONE_PLAYER;
    race_pause_begin(0);
    return 1;
}

static void pause_pad_start(uint8_t owner, void (*sprites)(void), PauseKind kind)
{
    g.pause_player = owner;
    SoundPort1QueuePush(0x15);
    windows.layout_request = 0x01;
    sprites();
    g.pause_stage_b--;
    g.finish_mark = 0;
    g.palette_animate = 0;
    g.pause_stage = 0;
    pause_kind = kind;
    race_pause_begin(0);
}

/* @routine 07:BAE6 OamStagePause: split screen, pad 1's START. */
int OamStagePause(void)
{
    int start = frame_state.pressed_copy[0] & 0x1000;

    if (!start && !quit_take())
        return 0;
    quit_driving = !start;
    pause_pad_start(0x00, TopGaugeSpritesOff, PAUSE_PAD_1);
    return 1;
}

/* @routine 07:BB4C OamStagePause2: split screen, pad 2's START. */
int OamStagePause2(void)
{
    if (!(frame_state.pressed_copy[1] & 0x1000))
        return 0;
    pause_pad_start(0xFF, BottomGaugeSpritesOff, PAUSE_PAD_2);
    return 1;
}

/* @routine 07:BB16 OamStagePauseLoop: the pause's stages from the results;
   they end by leaving the race. */
void OamStagePauseLoop(void)
{
    pause_kind = PAUSE_PAD_1;
    race_pause_begin(1);
}

/* @routine 07:BB7C OamStagePause2Loop */
void OamStagePause2Loop(void)
{
    pause_kind = PAUSE_PAD_2;
    race_pause_begin(1);
}
