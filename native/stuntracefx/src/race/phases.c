/*
 * phases.c: the race's phase handlers, run each step for each player from
 * the phase tables. They count the start, the race, the finish and the
 * results panels down.
 *
 * A handler that "does not return" in the original (the race restarted,
 * left or ended) here sets the race's next state and returns: nothing after
 * those calls in a handler runs anyway.
 */
#include "audio/sound.h"
#include "data/cart.h"
#include "hud/hud.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"
#include "save/save.h"
#include "system/game.h"
#include "video/picture.h"
#include "video/video.h"

/* @routine 07:E284 PhaseNothing: the phases with no work. */
void PhaseNothing(void) {}

static void phases_both(uint8_t phase)
{
    g.player_phase[0] = phase;
    g.player_phase[1] = phase;
}

/* 07:E299: both phases 2, the sub-state cleared; the time bonus, the hits
   and the digit pair cleared; sound 5. */
static void race_start(void)
{
    phases_both(0x02);
    g.start_substate = 0;
    g.time_bonus = 0;
    g.time_bonus_2 = 0;
    g.hits_bcd = 0;
    g.hits_bcd_2 = 0;
    g.digit_pair = 0;
    sound_port0_push_raw(0x05);
}

/* @routine 07:E285 RaceStartCountdown */
void RaceStartCountdown(void)
{
    g.glyphs_extended = 0;
    g.race_timer = 0x20;
    race_start();
}

/* @routine 07:E292 RaceStartCountdownStunt */
void RaceStartCountdownStunt(void)
{
    g.race_timer = 0x12;
    race_start();
}

static void race_ready(uint8_t t)
{
    sound_port0_push_raw(0x05);
    g.race_timer = t;
    phases_both(0x02);
    g.piece_step = 0;
}

/* @routine 07:E2BD RaceReadyShort */
void RaceReadyShort(void) { race_ready(0x1E); }
/* @routine 07:E2CA RaceReadyLong */
void RaceReadyLong(void) { race_ready(0x36); }

/* @routine 07:E323 RaceStartStepsDraw: the race time at the size the timer picks. */
static void start_draw(void)
{
    fx16_set(FXIN_PIECE_H, (uint8_t)cart16(0x07, (uint16_t)(0xE33A + g.race_timer)));
    GlyphColours(0xBF, 0xB5, 0xBB);
    GsuTimeDigits();
}

/* @routine 07:E344 RaceStartStepsHold */
static void start_hold(void)
{
    if (g.start_light && --g.race_timer)
        return;
    g.start_substate = (uint8_t)(g.start_substate + 2);
    g.start_light = 0x08;
    g.race_timer = 0x03;
}

/*
 * @routine 07:E2E9 RaceStartSteps
 * WHAT IT DOES: Steps the start's sub-state, the timer counting each: a
 * wait; the race time drawn at sizes from the table and at its end the time
 * full size; a hold; a last wait, then both phases 4.
 */
void RaceStartSteps(void)
{
    switch (g.start_substate) {
    case 0:
        if (--g.race_timer)
            return;
        g.start_substate = 2;
        g.race_timer = 0x07;
        return;
    case 2:
        if (--g.race_timer) {
            start_draw();
            return;
        }
        g.start_substate = 4;
        g.race_timer = 0x14;
        g.start_light = 0x04;
        g.finish_lines--;
        GsuTimeDigitsFull();
        return;
    case 4:
        start_hold();
        return;
    default:
        if (--g.race_timer)
            return;
        g.panel_state = 0;
        phases_both(0x04);
        return;
    }
}

/* @routine 07:E2E4 RaceStartGlyphsSteps */
void RaceStartGlyphsSteps(void)
{
    GlyphWordAt1C28();
    RaceStartSteps();
}

/* @routine 07:E376 RaceStartTimer */
void RaceStartTimer(void)
{
    if (!g.race_timer) {
        phases_both(0x04);
        g.panel_state = 0;
        return;
    }
    if (--g.race_timer == 0x1A)
        return;
    if (g.race_timer == 0x11)
        g.start_light = 0x02;
    if (g.race_timer == 0x06)
        g.start_light = 0x08;
}

/* @routine 07:E3A3 RaceStartTimerSteps */
void RaceStartTimerSteps(void)
{
    uint8_t v = (uint8_t)(g.race_timer - 1);

    if (!v) {
        g.panel_state = 0;
        phases_both(0x04);
        return;
    }
    g.race_timer = v;
    switch (g.start_substate) {
    case 0:
        if (v < 0x35)
            g.start_substate = 2;
        return;
    case 2:
        if (v < 0x21) {
            g.flash_0d65 = (uint8_t)(g.flash_0d65 + 2);
            g.start_substate = 4;
        }
        return;
    case 4:
        if (v < 0x19) {
            g.start_light = 0x02;
            g.start_substate = 6;
        }
        return;
    case 6:
        if (v < 0x09)
            g.start_substate = 8;
        return;
    default:
        return;
    }
}

/* @routine 07:E557 PanelDelayCount; returns 1 when it has run out. */
static int panel_delay_out(void)
{
    uint8_t before = g.panel_delay;

    g.panel_delay = (uint8_t)(before - g.frames_last_step);
    return before < g.frames_last_step;
}

static void panel_next_delay(uint8_t d)
{
    g.panel_state = (uint8_t)(g.panel_state + 2);
    g.panel_delay = d;
}

/* A result line shown: its sprites brought on, the next state, sound $49. */
static void panel_line(uint16_t at)
{
    if (g.split_screen) {
        OamBufferByteSet((uint16_t)(0x0420 + at), 0x20);
        OamBufferByteSet((uint16_t)(0x0520 + at), 0x88);
    } else {
        OamBufferByteSet((uint16_t)(0x0400 + at), 0x28);
    }
    panel_next_delay(0x32);
    sound_port0_push(0x49);
}

/*
 * @routine 07:E404 ResultsPanelSteps
 * WHAT IT DOES: Steps the results panel, each step after its delay: a
 * start, a wait, three lines shown, the marks and their sound, the panel
 * sound, $0D54 = $A; once $0D54 is 0, both phases 6.
 */
void ResultsPanelSteps(void)
{
    switch (g.panel_state) {
    case 0x00:
        g.panel_delay = 0x3C;
        g.panel_state = 2;
        g.start_light = 0x08;
        return;
    case 0x02:
        if (panel_delay_out())
            panel_next_delay(0x32);
        return;
    case 0x04:
        if (panel_delay_out())
            panel_line(0x06);
        return;
    case 0x06:
        if (panel_delay_out())
            panel_line(0x0A);
        return;
    case 0x08:
        if (panel_delay_out())
            panel_line(0x0E);
        return;
    case 0x0A:
        if (!panel_delay_out())
            return;
        if (g.split_screen) {
            static const uint16_t marks_split[6] = {0x0427, 0x0527, 0x042B, 0x052B, 0x042F, 0x052F};

            for (int i = 0; i < 6; i++)
                OamBufferByteSet(marks_split[i], 0xA2);
        } else {
            static const uint16_t marks[3] = {0x0407, 0x040B, 0x040F};

            for (int i = 0; i < 3; i++)
                OamBufferByteSet(marks[i], 0xA2);
        }
        panel_next_delay(0x32);
        sound_port0_push(0x4A);
        g.finish_mark = 0x01;
        g.freeze = 0;
        if (g.hud_mode == 0x01) {
            g.banner[0] = 0x02;
            g.banner_timer[0] = 0x0F;
        }
        return;
    case 0x0C:
        if (!panel_delay_out())
            return;
        panel_next_delay(0x0F);
        sound_port0_push_raw(0x01);
        return;
    case 0x0E:
        if (!panel_delay_out())
            return;
        g.start_light = 0x0A;
        g.panel_state = 0x10;
        return;
    default:
        if (g.start_light)
            return;
        g.time_bonus = 0x0002;
        phases_both(0x06);
        return;
    }
}

/* @routine 07:E937 CarFinishSoundP2 sets the current player and uses car 2. */
static void finish_sound(uint8_t car)
{
    sound_port0_push(car < 0x04 ? 0x08 : 0x15);
}

/* @routine 07:E944 CarFinishSound */
void CarFinishSound(void) { finish_sound(g.car[0]); }

void CarFinishSoundP2(void)
{
    g.current_player = 0x01;
    finish_sound(g.car[1]);
}

/*
 * @routine 07:D672 RaceFinishSounds
 * WHAT IT DOES: The sounds of a race's finish, by the mode and how it went.
 */
void RaceFinishSounds(void)
{
    if (g.hud_mode == 0) {
        g.player_mode_kept = g.player_mode[0];
        if (g.player_mode[0] == 0x01) {
            sound_port0_push_raw(0x02);
            CarFinishSound();
            sound_port0_push(0x48);
        } else {
            sound_port0_push_raw(0x03);
            CarFinishSound();
        }
        return;
    }
    if (g.hud_mode == 2) {
        PlayerSound35();
        CarFinishSound();
        return;
    }
    RecordTimeCompare(6);
    sound_port0_push_raw(g.lead == 0x01 ? 0x02 : 0x03);
    CarFinishSound();
}

/* Both phases 8, CarFinishSound, sound 4 on port 1. */
static void finish_both(void)
{
    g.player_phase[0] = 0x08;
    CarFinishSound();
    sound_port1_push(0x04);
}

/* A banner's size by its timer, then the timer counted down; at 0 the banner is cleared. */
static void lap_banner(int player, void (*draw)(uint16_t))
{
    uint16_t size = cart8(0x07, (uint16_t)(0xE95A + g.banner_timer[player]));

    draw(size);
    if (!--g.banner_timer[player])
        g.banner[player] = 0;
}

/* @routine 07:EA01 LapBannerWait */
static void lap_banner_wait(int player)
{
    if (--g.banner_timer[player])
        return;
    g.banner[player] = 0;
    g.lap_shown = 0xFF;
    sound_port0_push_raw(0x03);
}

/* 07:E608 / 07:E656: a finish without the split screen. */
static void finish_alone(int player, uint16_t colour, uint8_t phase, uint8_t t)
{
    PaletteDarkenFour(colour);
    g.game_e18 = 0xF0;
    g.finish_mark = 0;
    g.finish_lines = 0;
    Countdown0DF8();
    g.player_phase[player] = phase;
    g.finish_timer = t;
    g.end_ready = t;
    OamHide75Up();
    SoundReset();
    sound_port0_push(0x37);
}

/* 07:E6BD: the lap's time with the time override. */
static void lap_override(void)
{
    uint8_t frac = (uint8_t)(g.clock_fraction >> 8), secs = (uint8_t)g.clock_seconds, mins = (uint8_t)g.clock_minutes;
    uint16_t saved, packed, best;
    uint16_t entry;

    sound_port0_push(0x35);
    g.laps_counted[0]++;
    LapSplitTimes(0);
    g.lap_mark[0] = frac;
    g.lap_mark[1] = secs;
    g.lap_mark[2] = mins;
    entry = RecordOffsetAllCourses();
    saved = fx16((uint16_t)(0xFC5E + entry));
    g.packed_time_a = saved;
    packed = TimePackBcd(g.lap_splits[0], g.lap_splits[1], g.lap_splits[2]);
    g.packed_time_b = packed;
    best = fx16((uint16_t)(0xFC5E + entry));
    if (packed < best) {
        fx16_set((uint16_t)(0xFC5E + entry), packed);
        HudBestTimeDigits();
        fx16_set((uint16_t)(0xFC60 + entry), g.level_colour);
        fx8_set((uint16_t)(0xFC62 + entry), g.car[0]);
        SaveSumsRecordsB();
        g.lead = 0x01;
    } else if (packed != best) {
        g.lead = 0xFF;
    } else {
        g.lead = 0x00;
    }
    g.hud_hold = 0x012C;
}

/* 07:E7DC: the last lap done. */
static void lap_finish(int player)
{
    uint16_t w;

    g.finish_mark = 0;
    g.lap_shown--;
    g.end_ready--;
    if (g.split_screen) {
        g.freeze = 0xFF;
        RaceFinishSplit(player);
        return;
    }
    if (g.hud_mode)
        goto hold;
    if (g.player_mode[0] >= 0x04) {
        g.finish_lines = 0;
        g.end_timer = 0x23;
        g.player_phase[player] = 0x0A;
        OamHideForHudMode();
        Countdown0DF8();
        sound_port0_push(0x51);
        return;
    }
    if (g.byte_0d24 == 0x03) {
        finish_both();
        return;
    }
    if (g.player_mode[0] < 0x02)
        w = 0x8000;
    else if (g.player_mode[0] == 0x02)
        w = 0x8006;
    else
        w = 0x800C;
    g.glyph_sprite_glyph[0] = w;
    g.glyph_sprite_glyph[1] = (uint16_t)(w + 0x0400);
    g.glyph_sprite_glyph[2] = w == 0x8000 ? 0x8012 : 0x8018;
    g.glyph_sprite_step[0] = 0x0021;
    g.glyph_sprite_step[1] = 0x001D;
    g.glyph_sprite_step[2] = 0x0019;
    g.glyph_sprite_target[0] = 0x0008;
    g.glyph_sprite_target[1] = 0x001C;
    g.glyph_sprite_target[2] = 0x002F;
    g.glyph_sprite_x[0] = 0x00D7;
    g.glyph_sprite_x[1] = 0x00D3;
    g.glyph_sprite_x[2] = 0x00CF;
    if (g.champ_stage == 0x03) {
        GlyphSpritesInit();
        goto panel;
    }
hold:
    g.hud_hold = 0xFFFF;
    if (g.race_skipped) {
        finish_both();
        return;
    }
panel:
    g.banner[player] = 0x08;
    LapSplit(6);
    RaceFinishSounds();
    g.banner_timer[player] = 0x14;
    g.player_phase[player] = 0x08;
    g.race_ended_d55 = 0;
    g.end_timer = 0x70;
    OamHideForHudMode();
    RaceTimeKeep();
    g.time_shown = g.time_left >= 0x0099 ? 0x0099 : g.time_left;
    if (g.clock_minutes >= 0x0008)
        return;
    SpecialRecordInsert(TimePackBcd((uint8_t)(g.clock_fraction >> 8), (uint8_t)g.clock_seconds, (uint8_t)g.clock_minutes));
    RecordsLoadCourseLong();
}

/*
 * @routine 07:E6B5 RaceLapCrossed
 * WHAT IT DOES: A lap crossed by the player: its lap count, time and
 * banner, or the finish after the third.
 */
void RaceLapCrossed(int player)
{
    uint8_t v;

    if (g.time_override) {
        lap_override();
        return;
    }
    v = ++g.laps_counted[player];
    if (v == 0x02) {
        RaceTimeAddD815();
        g.hud_hold = 0x012C;
        sound_port0_push(0x48);
        g.banner[player] = 0x06;
        g.banner_timer[player] = 0x17;
        LapSplit(3);
        RecordCompareTime1();
        return;
    }
    if (v > 0x02) {
        lap_finish(player);
        return;
    }
    if (v != 0x01) {
        sound_port0_push(0x48);
        return;
    }
    RaceTimeAddD814();
    g.hud_hold = 0x012C;
    g.banner[player] = 0x04;
    g.banner_timer[player] = 0x17;
    sound_port0_push(0x48);
    LapSplit(0);
    RecordCompareTime0();
}

/* 07:E575: the race phase's work after the pause tests. */
static void race_phase(int player)
{
    uint16_t w;

    if (g.clock_minutes == 0x09 && g.clock_seconds == 0x59 && (uint8_t)(g.clock_fraction >> 8) == 0x99)
        goto time_up;
    if (g.hud_mode == 0x02 || g.hud_mode == 0x03)
        goto progress;
    if (g.time_left)
        goto progress;
time_up:
    g.time_up_timer = 0x20;
    if (!g.split_screen) {
        g.player_phase[player] = 0x0C;
    } else {
        g.end_timer = 0x20;
        phases_both(0x14);
        g.freeze = 0;
    }
    sound_port0_push(0x31);
    return;
progress:
    w = (uint16_t)(g.car_flags_high[player] << 8 | g.car_flags_low[player]);
    if (!(w & 0x0001)) {
        switch (g.banner[player == 0 ? 0 : 1]) {
        case 0x00:
        case 0x0A:
            if (g.laps_done[player] > g.laps_counted[player])
                RaceLapCrossed(player);
            return;
        case 0x04:
            lap_banner(player, GsuF0A5OrF0B4);
            return;
        case 0x06:
            lap_banner(player, GsuEEF5OrEFCD);
            return;
        case 0x08:
            lap_banner_wait(player);
            return;
        default:
            return;
        }
    }
    if (w & 0x1000) {
        if (!g.split_screen) {
            finish_alone(player, 0xFC00, 0x10, 0x0E);
            return;
        }
        g.player_phase[player] = 0x10;
        g.finish_timer = 0x0C;
        sound_port0_push(0x37);
        g.freeze = 0xFF;
        RaceFinishPhases(player, 0x10);
        return;
    }
    if (!g.split_screen) {
        finish_alone(player, 0x801F, 0x0E, 0x0C);
        return;
    }
    g.player_phase[player] = 0x0E;
    g.finish_timer = 0x0C;
    g.freeze = 0xFF;
    RaceFinishPhases(player, 0x0E);
}

/* @routine 07:E562 RacePhaseLapP1 */
void RacePhaseLapP1(void)
{
    GsuF441Player1();
    OamStagePause();
    race_phase(0);
}

/* @routine 07:E56A RacePhaseLapP2 */
void RacePhaseLapP2(void)
{
    GsuF441Player2();
    OamStagePause2();
    race_phase(1);
}

/* @routine 07:E572 RacePhaseLap: one player: the pause, then the race. */
void RacePhaseLap(void)
{
    PauseOnStart();
    race_phase(0);
}

/* @routine 07:E972 PhaseGlyphsFive */
void PhaseGlyphsFive(void) { GsuF15BFive(); }
/* @routine 07:E976 PhaseGlyphsSix */
void PhaseGlyphsSix(void) { HudSixGlyphs(); }
/* @routine 07:EB45 PhaseGlyphsFourB */
void PhaseGlyphsFourB(void) { GsuF15BFourB(); }
/* @routine 07:EBA0 PhaseGlyphsFourA */
void PhaseGlyphsFourA(void) { GsuF15BFourA(); }

/* Once the slide out has reached $1F: the race's end; before it, draw. */
static void race_end_wait(void (*draw)(void))
{
    if (palettes.amount < 0x1F) {
        draw();
        return;
    }
    g.palette_animate = 0;
    g.split_stage_on = palettes.amount;
    g.split_stage = 0x06;
    g.finish_timer = 0x78;
    g.race_ended_d55 = 0;
    RaceEndCountdownFade();
}

/* @routine 07:E97A RaceEndWaitRow6B */
void RaceEndWaitRow6B(void) { race_end_wait(BigSpritesRow6B); }
/* @routine 07:E99D RaceEndWaitBigSprites */
void RaceEndWaitBigSprites(void) { race_end_wait(BigSpritesRow5); }

static void window_up(void)
{
    g.split_stage_on = 0x01;
    g.split_stage = 0x00;
    WindowSlideUpSound();
}

/* Bits 15 and 12 of the presses: B or START. */
static int pressed_b_start(void) { return (frame_state.pressed_copy[0] & 0x9000) != 0; }

static int window_closed_overlay(void)
{
    if (g.split_window_state != 0x05)
        return 0;
    g.overlay_d59 = 0;
    OverlayFramesUntilFaded();
    return 1;
}

/* @routine 07:EA1B ResultsWindowWait */
void ResultsWindowWait(void)
{
    if (g.race_skipped) {
        if (g.fade_level & 0x80)
            SoftResetToBootWarm();
        return;
    }
    if (!g.end_timer) {
        if (!window_closed_overlay())
            return;
        if (g.race_again)
            RaceRestart();
        else
            RaceEndVideoReset();
        return;
    }
    if (g.end_timer == 0x10) {
        window_up();
    } else if (g.end_timer < 0x58 && pressed_b_start()) {
        if (g.split_window_state)
            return;
        g.end_timer = 0;
        window_up();
        return;
    }
    g.end_timer--;
}

/* @routine 07:EA85 RaceEndWindowSteps */
void RaceEndWindowSteps(void)
{
    uint8_t v;

    if (g.champ_stage == 0x03) {
        GlyphSpritesGrowAndTimes();
        return;
    }
    if (window_closed_overlay()) {
        if (g.byte_0d24 != 0x03) {
            if (g.champ_stage == 0x01) {
                g.time_override = 0xFF;
                g.course = (uint8_t)(g.course_class + 0x14);
                g.car[0] = 0x04;
            } else {
                g.champ_stage++;
            }
        }
        RaceRestart();
        return;
    }
    v = g.end_timer;
    if (v == 0x10) {
        g.split_window_count = 0;
        window_up();
    }
    if (v == 0x58) {
        g.flash_0d65 = 0x0A;
        GlyphSlideThree();
    } else if (v < 0x58) {
        GlyphSlideThree();
    }
    if (v == 0x2B && g.flash_0d65)
        g.flash_0d65 = 0x10;
    g.end_timer--;
}

static void pause_stage_ten(void)
{
    g.pause_stage = 0x0A;
    g.pause_stage_b = 0x0A;
}

/* @routine 07:EB10 PauseResultsP2 */
void PauseResultsP2(void)
{
    GsuF15BThree();
    if (g.end_timer) {
        if (--g.end_timer)
            return;
        g.pause_player = 0x01;
        WindowSlideUpSound();
        g.pause_stage = 0x08;
        g.panel_delay = 0x08;
        return;
    }
    if (g.panel_delay) {
        g.panel_delay--;
        return;
    }
    pause_stage_ten();
    OamStagePause2Loop();
}

/* 07:EB51: the paused results, player 1. */
static void pause_results(void)
{
    if (g.end_timer) {
        if (--g.end_timer)
            return;
        sound_port0_push(0x51);
        g.pause_player = 0x00;
        WindowSlideUpSound();
        g.pause_stage = 0x08;
        g.panel_delay = 0x14;
        return;
    }
    if (g.panel_delay) {
        g.panel_delay--;
        return;
    }
    if ((uint8_t)g.clock_minutes == 0x09 && (uint8_t)g.clock_seconds == 0x59)
        sound_port0_push_raw(0x30);
    pause_stage_ten();
    OamStagePauseLoop();
}

/* @routine 07:EB49 PauseResultsFourB */
void PauseResultsFourB(void)
{
    GsuF15BFourB();
    pause_results();
}

/* @routine 07:EB4E PauseResults */
void PauseResults(void)
{
    GsuF15BThree();
    pause_results();
}

/* @routine 07:EBA4 RaceEndWindowNine */
void RaceEndWindowNine(void)
{
    uint8_t v;

    BigSpritesNine();
    v = g.end_timer;
    if (!v) {
        if (g.split_window_state == 0x05)
            RaceEndLeave();
        return;
    }
    if (v != 0x01 && (v >= 0x28 || !pressed_b_start())) {
        g.end_timer--;
        return;
    }
    g.end_timer = 0;
    g.split_window_count = 0x01;
    WindowSlideUpSound();
}

/* @routine 07:EBDD RaceEndLeave */
void RaceEndLeave(void)
{
    if (!g.sprites_counted) {
        RaceEndToAttract();
        return;
    }
    race_leave_to_champion();
}

static void time_up_end(uint8_t t)
{
    g.split_stage_on = 0x06;
    g.split_stage = 0x06;
    g.time_up_timer = t;
    g.race_ended_d55 = 0;
    sound_port0_push_raw(0x30);
    RaceEndCountdown();
}

/* @routine 07:ECA4 TimeUpBattle */
void TimeUpBattle(void)
{
    BigSpritesRow6();
    if (!g.flag_0e59) {
        if (!g.time_up_timer) {
            sound_port0_push(0x51);
            OamHide75Up();
            g.time_up_timer = 0x3C;
            g.flag_0e59 = 0x3C;
        }
        g.time_up_timer--;
        return;
    }
    if (--g.time_up_timer) {
        g.time_up_timer--;
        return;
    }
    g.save_bad_flag = 0x11;
    time_up_end(0x3C);
}

/* The first part's wait; returns 1 to go on, 2 when it set phase 6, 0 when it only counted. */
static int time_up_wait(void)
{
    if (g.status_level[0] < 0x10)
        return 1;
    if ((uint8_t)g.time_left) {
        g.player_phase[0] = 0x06;
        return 2;
    }
    return --g.time_up_timer == 0;
}

/* @routine 07:ECF4 TimeUpStunt */
void TimeUpStunt(void)
{
    BigSpritesRow6();
    if (!g.flag_0e59) {
        if (time_up_wait() == 1) {
            g.time_up_timer = 0x20;
            g.flag_0e59--;
            sound_port0_push(0x51);
            OamHide75Up();
        }
        return;
    }
    if (g.time_up_timer) {
        g.time_up_timer--;
        return;
    }
    time_up_end(0x78);
}

/* @routine 07:ED4C TimeUpRace */
void TimeUpRace(int player)
{
    uint8_t v;

    BigSpritesRow6();
    if (!g.flag_0e59) {
        if (g.laps_done[0] > g.laps_counted[0]) {
            g.player_phase[0] = 0x06;
            RaceLapCrossed(player);
            return;
        }
        if (time_up_wait() == 1) {
            g.time_up_timer = 0x24;
            g.flag_0e59--;
            sound_port0_push(0x51);
            Countdown0DF8();
        }
        return;
    }
    v = g.time_up_timer;
    if (!v) {
        if (g.split_window_state == 0x05)
            RaceEndLeave();
        return;
    }
    v--;
    g.time_up_timer = v;
    if (v == 0x01) {
        g.split_window_count = 0x01;
        WindowSlideUpSound();
    }
}
