/*
 * player_stats.c: the figures the HUD and the results show, taken each step
 * from the player's car's Super FX record, and the dispatch of the race
 * phases that follow them.
 *
 * Not converted, nothing reaches them: RaceXPressPlayer1 and 2 (the test
 * before them loads 0 after its BIT), PlayerOnePhaseReturn.
 */
#include "audio/sound.h"
#include "data/cart.h"
#include "hud/hud.h"
#include "object/object.h"
#include "race/race.h"
#include "save/save.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"
#include "video/video.h"

/*
 * @routine 07:DDCE SpeedSampleAdd
 * WHAT IT DOES: A speed as a speedometer sample, |speed| * $3C / 256 kept
 * to $FF, stored as the player's and added to its running total and count.
 * A total that would overflow stays at its limit and the sample is not
 * counted; player 1's limit, as the original's, only reaches the low word.
 */
void SpeedSampleAdd(uint16_t speed, int player)
{
    uint32_t product;
    uint16_t m;
    uint8_t v;
    uint32_t total = g.speed_total[player], sum;

    if (speed & 0x8000)
        speed = (uint16_t)-speed;
    product = (uint32_t)((int32_t)(int16_t)speed * 0x3C);
    m = (uint16_t)(product >> 8);
    v = m < 0x00FF ? (uint8_t)m : 0xFF;
    g.status_level[player] = v;
    sum = total + v;
    if (sum < total) {
        if (player == 0)
            g.speed_total[0] = 0xFFFFFFFFu;
        else
            g.speed_total[1] = (total & 0xFFFF0000u) | 0xFFFFu;
        return;
    }
    g.speed_total[player] = sum;
    g.speed_count[player]++;
}

/*
 * @routine 07:DD60 PlayerStatsFromRecord
 * WHAT IT DOES: Unless the lap is being shown, copies the player's figures
 * from its car's record: the flags, the gauges, a speedometer sample, the
 * laps and the rank. Then GsuRecordBits60.
 */
void PlayerStatsFromRecord(uint16_t rec, int player)
{
    if (!g.lap_shown) {
        uint16_t f = rec_get(rec, FXREC_FLAGS40), w;
        uint8_t v;

        g.car_flags_low[player] = (uint8_t)f;
        g.car_flags_high[player] = (uint8_t)(f >> 8);
        g.status_flags[player] = rec_get(rec, FXREC_SIDES);
        w = rec_get(rec, FXREC_82) >> 3;
        v = (uint8_t)w;
        g.gauge_one[player] = v;
        if (v >= 0x3F)
            v = 0x3F;
        g.kept_rate[player] = (uint8_t)((v << 1) + 0x30);
        SpeedSampleAdd(rec_get(rec, FXREC_SPEED), player);
        g.kept_enable[player] = (uint8_t)(rec_get(rec, FXREC_6A) >> 3);
        g.laps_done[player] = (uint8_t)rec_get(rec, FXREC_LAPS);
        g.player_rank[player] = (uint8_t)(rec_get(rec, FXREC_RANK) & 0x0F);
    }
    GsuRecordBits60();
}

/* The figures are taken once a step: not again when a step goes on after a pause. */
static void player_one_stats(void)
{
    if (!race_resuming())
        PlayerStatsFromRecord(fx16(FXIN_PLAYER1_RECORD), 0);
}

/* @routine 07:DD47 PlayerStatsAndPaletteAnimate */
void PlayerStatsAndPaletteAnimate(void)
{
    PlayerStatsFromRecord(fx16(FXIN_PLAYER1_RECORD), 0);
    g.palette_animate = 0xFF;
}

/* The tally's tables in bank 7: each glyph sprite's glyph and its slide's
   end, and the stunt panel's count by its timer. */
enum { TALLY_BANK = 0x07, TALLY_GLYPHS = 0xDFA0, TALLY_X_ENDS = 0xDF70, STUNT_COUNT_STEPS = 0xE115 };

typedef void (*PhaseFn)(void);

static void phase_ready_long(void) { RaceReadyLong(); }

static void time_up_race_p0(void) { TimeUpRace(0); }

/* The race phases by the table at $07:E232. */
static void phases_one_player(void)
{
    static const PhaseFn t[] = {
        phase_ready_long, RaceStartTimerSteps, ResultsPanelSteps, RacePhaseLap,
        RaceEndWindowSteps, RaceEndWindowNine, time_up_race_p0, RaceEndWaitBigSprites,
        RaceEndWaitRow6B
    };
    unsigned i = (unsigned)g.player_phase[0] / 2;

    if (i < sizeof t / sizeof t[0])
        t[i]();
}

/* The race phases by the table at $07:E24F, two players, player 1's side. */
static void phases_two_players_p1(void)
{
    static const PhaseFn t[] = {
        phase_ready_long, RaceStartTimerSteps, PhaseNothing, RacePhaseLapP1,
        RaceEndWindowSteps, PhaseGlyphsFourA, PhaseNothing, PhaseGlyphsFive,
        PhaseGlyphsSix, PauseResultsP2, PhaseGlyphsFourB
    };
    unsigned i = (unsigned)g.player_phase[0] / 2;

    if (i < sizeof t / sizeof t[0])
        t[i]();
}

/* @routine 07:E265 PlayerTwoFrameWork: player 2's phase. */
static void player_two_frame_work(void)
{
    static const PhaseFn t[] = {
        PhaseNothing, PhaseNothing, ResultsPanelSteps, RacePhaseLapP2,
        RaceEndWindowSteps, PhaseGlyphsFourA, PhaseNothing, PhaseGlyphsFive,
        PhaseGlyphsSix, PauseResults, PauseResultsFourB
    };
    unsigned i = (unsigned)g.player_phase[1] / 2;

    if (i < sizeof t / sizeof t[0])
        t[i]();
}

/* @routine 07:DCEC PlayerStatsThenPhaseE232 */
static void stats_then_phases_one_player(void)
{
    player_one_stats();
    phases_one_player();
}

/*
 * @routine 07:DD03 PlayerStatsThenPhaseDD35
 * WHAT IT DOES: The player's figures; then in a mode with no race, at phase
 * 8 or more, the fade out and, once it is done, the restart; otherwise the
 * phase by the table at $07:DD35.
 */
static void stats_then_phases_battle_demo(void)
{
    static const PhaseFn t[] = {
        RaceReadyShort, RaceStartTimer, ResultsPanelSteps, RacePhaseLap,
        ResultsWindowWait, PhaseNothing, TimeUpBattle, RaceEndWaitBigSprites,
        RaceEndWaitRow6B
    };
    unsigned i;

    player_one_stats();
    if (g.race_skipped && g.player_phase[0] >= 0x08) {
        g.fade_out = 0x01;
        if (g.fade_level & 0x80)
            SoftResetToBootWarm();
        return;
    }
    i = (unsigned)g.player_phase[0] / 2;
    if (i < sizeof t / sizeof t[0])
        t[i]();
}

/* The two-player frame's stats and phases. */

/* @routine 07:DA02 RaceSoundsPlayer1: player 1's share of a two-player step. */
void RaceSoundsPlayer1(void)
{
    if (!race_resuming()) {
        SoftResetCheck();
        PlayerStatsFromRecord(fx16(FXIN_PLAYER1_RECORD), 0);
        PlayerStatsFromRecord(fx16(FXIN_PLAYER2_RECORD), 1);
        g.current_player = 0;
        EngineSoundsPlayer1(0, frame_state.pressed_copy[0], frame_state.held[0]);
    }
    phases_two_players_p1();
    race_resume_done();
    if (race_flow_stopped())
        return;
    frame_state.pressed_copy[0] = 0;
}

/* @routine 07:DA65 RaceSoundsPlayer2 */
void RaceSoundsPlayer2(void)
{
    if (!race_resuming()) {
        g.current_player = 1;
        EngineSoundsPlayer2(1, frame_state.pressed_copy[1], frame_state.held[1]);
    }
    player_two_frame_work();
    race_resume_done();
    if (race_flow_stopped())
        return;
    frame_state.pressed_copy[1] = 0;
}

/* @routine 07:DE6E PlayerOnePhaseRace: the championship's race: time left counts laps; at 0 its result is laid out. */
static void player_one_phase_race(void)
{
    static const struct { uint16_t n, y; } rows[3] = {{7, 0x0034}, {8, 0x004C}, {9, 0x0060}};
    int x = 1;

    if (PauseOnStart())
        return;
    if (g.time_left) {
        if (g.laps_done[0] == g.laps_counted[0])
            return;
        g.laps_counted[0] = g.laps_done[0];
        SoundPort0QueuePush(0x35);
        g.tries_left++;
        g.glyphs_extended++;
        return;
    }
    SoundPort0QueuePushRaw(0x02);
    SoundPort0QueuePush(0x15);
    OamHideForHudMode();
    g.player_phase[0] = 0x0A;
    g.freeze = 0x0A;
    g.finish_lines = 0;
    g.finish_mark = 0;
    g.glyph_sprite_x[0] = 0x0004;
    g.glyph_sprite_y[0] = 0x0008;
    for (int r = 0; r < 3; r++) {
        for (uint16_t n = rows[r].n; n; n--, x++) {
            g.glyph_sprite_y[x] = rows[r].y;
            g.glyph_sprite_x[x] = 0x0038;
            g.glyph_sprite_glyph[x] = cart16(TALLY_BANK, (uint16_t)(TALLY_GLYPHS + 2 * x));
        }
    }
    g.glyph_sprite_glyph[3] = (uint16_t)(0x1400 + ((g.digit_pair & 0x000F) << 1));
    g.glyph_sprite_glyph[2] = (uint16_t)(0x1400 + (((g.digit_pair >> 4) & 0x000F) << 1));
    if (!g.glyphs_extended) {
        g.glyph_sprite_glyph[9] = 0x960E;
        g.glyph_sprite_glyph[10] = 0x9610;
        g.glyph_sprite_glyph[11] = 0x0000;
        return;
    }
    if (g.glyphs_extended >= 0x0002)
        return;
    g.glyph_sprite_glyph[9] = 0x9610;
    g.glyph_sprite_glyph[10] = 0x960E;
    g.glyph_sprite_glyph[11] = 0x941C;
    g.glyph_sprite_glyph[15] = 0x0000;
    g.glyph_sprite_step[0] = 0x980E;
}

/* @routine 07:DFD2 PlayerOnePhaseTally: the result's sprites slide right, then the time. */
static void player_one_phase_tally(void)
{
    uint16_t a;

    for (int x = 1; x < 25; x++) {
        uint16_t limit = cart16(TALLY_BANK, (uint16_t)(TALLY_X_ENDS + 2 * x));

        a = (uint16_t)(g.glyph_sprite_x[x] + 0x000B);
        if (a >= limit)
            a = limit;
        g.glyph_sprite_x[x] = a;
    }
    a = (uint16_t)(g.glyph_sprite_x[0] + 0x000B);
    if (a >= 0x00AC) {
        g.time_left = (uint16_t)((g.time_left & 0xFF00) | (uint8_t)g.digit_pair);
        g.end_timer = 0x40;
        g.player_phase[0] = 0x0E;
        g.tally_state = 0x0C;
        g.hud_point_y = 0x0006;
        g.hud_point_index = 0xFFFE;
        g.digit_pair = g.time_shown;
        a = 0x00AC;
    }
    g.glyph_sprite_x[0] = a;
    fx16_set(FXIN_PIECE_H, 0x0020);
    GlyphColours(0xBF, 0xBA, 0xB9);
    BonusDigitsDrawAt(g.glyph_sprite_x[0]);
    GlyphSpritesDrawAll();
}

/* @routine 07:E044 PlayerOnePhaseEnd */
static void player_one_phase_end(void)
{
    if (g.end_timer) {
        if (g.end_timer == 0x0A)
            SoundPort1QueuePush(0x04);
        if (!--g.end_timer) {
            windows.bands_kind = 0x01;
            WindowSlideUpSound();
        }
    } else if (windows.bands_state == 0x05) {
        g.time_shown = (uint16_t)((g.time_shown & 0xFF00) | (uint8_t)g.digit_pair);
        g.time_override = 0;
        g.champ_stage = 0x02;
        g.car[0] = g.car_index_kept;
        g.selection_handler[0] = g.car_handler_kept;
        RaceRestart();
        return;
    }
    GlyphSpritesDrawAll();
}

/* @routine 07:DE46 PlayerOnePhaseStep: player 1's phase in the time-override race. */
static void player_one_phase_step(void)
{
    player_one_stats();
    switch (g.player_phase[0]) {
    case 0x00: RaceStartCountdown(); break;
    case 0x02: RaceStartGlyphsSteps(); break;
    case 0x04: ResultsPanelSteps(); break;
    case 0x06:
    case 0x08: player_one_phase_race(); break;
    case 0x0A: player_one_phase_tally(); break;
    case 0x0C: TimeUpBattle(); break;
    case 0x0E: player_one_phase_end(); break;
    default: break;
    }
}

/* The stunt panel's count byte through the table, then the draw. */
static uint16_t stunt_count_byte(void)
{
    return cart8(TALLY_BANK, (uint16_t)(STUNT_COUNT_STEPS + g.banner_timer[0]));
}

static void stunt_count_down(void)
{
    if (!--g.banner_timer[0])
        g.banner[0] = 0;
}

/* @routine 07:E17D StuntCourseMark: on the stunt courses, the course's bit in the save. */
static void stunt_course_mark(void)
{
    uint8_t bit;

    switch (g.course) {
    case 0x0C: bit = 0x01; break;
    case 0x0D: bit = 0x02; break;
    case 0x0E: bit = 0x04; break;
    case 0x0F: bit = 0x08; break;
    default: return;
    }
    save.courses_open[0] |= bit;
    (void)SaveChecksum();
}

/* @routine 07:E137 StuntPanelWait */
static void stunt_panel_wait(void)
{
    uint8_t v;

    if (PauseOnStart())
        return;
    v = g.stunt_finished;
    if (!v || v == g.stunt_seen)
        return;
    g.stunt_seen = v;
    if (v != 0x08) {
        g.banner[0] = 0x04;
        g.banner_timer[0] = 0x0F;
        return;
    }
    SoundPort0QueuePushRaw(0x02);
    CarFinishSound();
    g.banner[0] = 0x06;
    g.finish_mark = 0;
    g.end_panel_in = 0;
    g.banner_timer[0] = 0x20;
    OamHideForHudMode();
    StuntRecordTime();
    RecordsLoadSpecialCourse();
    stunt_course_mark();
}

/* @routine 07:E1AE StuntPanelCount */
static void stunt_panel_count(void)
{
    ShowGetAllTheStars(stunt_count_byte());
    stunt_count_down();
}

/* @routine 07:E1C8 StuntPanelShow */
static void stunt_panel_show(void)
{
    GlyphRowsAt32(stunt_count_byte());
    stunt_count_down();
}

/* @routine 07:E1E2 StuntPhaseEnd */
static void stunt_phase_end(void)
{
    if (windows.bands_state != 0x05)
        return;
    g.results_step = 0;
    g.stunt_score_counted = 0;
    g.time_counted = 0;
    race_wait_overlay(race_go_on);
}

/* @routine 07:E205 StuntPanelDone */
static void stunt_panel_done(void)
{
    GlyphRowsAt56(0x0020);
    if (--g.banner_timer[0])
        return;
    g.banner[0] = 0x08;
    windows.bands_kind = 0;
    g.split_stage_on = 0x01;
    g.split_stage = 0;
    WindowSlideUpSound();
}

/* @routine 07:E0B7 StuntPhaseRace */
static void stunt_phase_race(void)
{
    if (!g.time_left) {
        g.player_phase[0] = 0x0C;
        g.end_panel_in = 0;
        g.time_up_timer = 0x80;
        SoundPort0QueuePush(0x31);
        return;
    }
    if (g.car_flags_low[0] & 0x01) {
        PaletteDarkenFour(0x801F);
        g.finish_lines = 0;
        g.finish_mark = 0;
        g.player_phase[0] = 0x0E;
        g.finish_timer = 0x0C;
        g.end_ready = 0x0C;
        OamHide75Up();
        SoundReset();
        SoundPort0QueuePush(0x37);
        return;
    }
    switch (g.banner[0]) {
    case 0x00: stunt_panel_wait(); break;
    case 0x02: stunt_panel_count(); break;
    case 0x04: stunt_panel_show(); break;
    case 0x06: stunt_panel_done(); break;
    case 0x08: stunt_phase_end(); break;
    default: break;
    }
}

/* @routine 07:E08F StuntPhaseStep */
static void stunt_phase_step(void)
{
    player_one_stats();
    switch (g.player_phase[0]) {
    case 0x00: RaceStartCountdownStunt(); break;
    case 0x02: RaceStartSteps(); break;
    case 0x04: ResultsPanelSteps(); break;
    case 0x06: stunt_phase_race(); break;
    case 0x08: stunt_phase_end(); break;
    case 0x0A: break;
    case 0x0C: TimeUpStunt(); break;
    case 0x0E: RaceEndWaitBigSprites(); break;
    default: break;
    }
}

/* The car the view follows is flagged in the air: bit 14 of the mode word of
   the camera its second link names. */
static int player_car_raised(void)
{
    const Object *first = obj_at(obj_at(g.player_car)->link);

    return (camera_of(first->link)->mode & 0x4000) != 0;
}

/* @routine 07:DC47 RaceHudModeTwo: modes 2 and 3. */
static void hud_mode_two(void)
{
    g.clock_step = player_car_raised() ? 0x0214 : 0x0182;
    stats_then_phases_battle_demo();
    if (race_flow_stopped())
        return;
    HudTimesDraw();
    CoursePaletteCycle();
    frame_state.pressed_copy[0] = 0;
    frame_state.pressed_copy[1] = 0;
}

static void hud_mode_zero(void)
{
    g.clock_step = player_car_raised() ? 0x0177 : 0x0166;
    if (g.every_course_open == 0x03 && g.player_phase[0] >= 0x08) {
        g.fade_out = 0x01;
        if (g.fade_level & 0x80)
            RaceRestart();
        else
            frame_state.pressed_copy[0] = 0;
        return;
    }
    if (g.time_override) {
        player_one_phase_step();
        if (race_flow_stopped())
            return;
        TimeBonusTally();
        TimeLeftDigits();
        TimeBonusTransfer();
    } else {
        stats_then_phases_one_player();
        if (race_flow_stopped())
            return;
        TimeBonusTally();
        TimeLeftDigits();
        HudHoldDraw();
        CoursePaletteCycle();
    }
    frame_state.pressed_copy[0] = 0;
}

static void hud_mode_one(void)
{
    if (g.hud_blank) {
        g.freeze = 0;
        TimeUpPhaseStep();
        if (race_flow_stopped())
            return;
        HudBlankDraw();
    } else {
        stunt_phase_step();
        if (race_flow_stopped())
            return;
        if (!g.time_up_shown) {
            TimeBonusTally();
            TimeLeftDigits();
            TimeBonusTransfer();
        }
    }
    frame_state.pressed_copy[0] = 0;
    frame_state.pressed_copy[1] = 0;
}

/*
 * @routine 07:DAA7 RaceSoundsAndHud
 * WHAT IT DOES: The race step's sounds and HUD: the soft reset check,
 * player 1's engine sounds, then the mode's work.
 */
void RaceSoundsAndHud(void)
{
    if (!race_resuming()) {
        SoftResetCheck();
        EngineSoundsPlayer1(0, frame_state.pressed_copy[0], frame_state.held[0]);
    }
    switch (g.hud_mode) {
    case 0: hud_mode_zero(); break;
    case 1: hud_mode_one(); break;
    default: hud_mode_two(); break;
    }
    race_resume_done();
}

/* @routine 07:DB97 TimeUpPhaseStart */
static void time_up_phase_start(void)
{
    g.glyph_sprite_x[0] = 0x00DA;
    g.glyph_sprite_y[0] = 0x0014;
    g.glyph_sprite_x[1] = 0xFF9E;
    g.glyph_sprite_y[1] = 0x0040;
    g.player_phase[0] = (uint8_t)(g.player_phase[0] + 2);
}

/* @routine 07:DBC0 TimeUpPhaseRace */
static void time_up_phase_race(void)
{
    if (PauseOnStart())
        return;
    if (g.clock_minutes == 0x09 && g.clock_seconds == 0x59 && (uint8_t)(g.clock_fraction >> 8) == 0x99) {
        g.time_up_timer = 0x20;
        g.player_phase[0] = 0x0C;
        SoundPort0QueuePush(0x31);
        return;
    }
    if (g.rivals_left)
        return;
    g.player_phase[0] = 0x08;
    g.freeze = 0x08;
    g.finish_mark = 0;
    g.end_timer = 0x30;
    SoundPort0QueuePush(0x3E);
    sprite_high_set(0x74, 0x55);        /* sprites $74-$7F: X's ninth bit, small */
    sprite_high_set(0x78, 0x55);
    sprite_high_set(0x7C, 0x55);
    TimeUpRecordUpdate();
}

/* @routine 07:DC15 TimeUpPhaseEnd */
static void time_up_phase_end(void)
{
    if (!g.end_timer) {
        g.results_step = 0;
        race_wait_champion(race_go_on);
        return;
    }
    g.end_timer--;
    HudLapDigitsDraw();
    if (g.lead & 0x80)
        HudLeadWordDraw();
}

/* @routine 07:DB71 TimeUpPhaseStep */
void TimeUpPhaseStep(void)
{
    player_one_stats();
    switch (g.player_phase[0]) {
    case 0x00: time_up_phase_start(); break;
    case 0x02: GlyphSpritesSlideIn(); break;
    case 0x04: GlyphSpritesSlideOut(); break;
    case 0x06: time_up_phase_race(); break;
    case 0x08: time_up_phase_end(); break;
    case 0x0C: TimeUpBattle(); break;
    default: break;
    }
}
