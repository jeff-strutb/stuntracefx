/*
 * race.h: the race as the rest of the game calls it.
 */
#ifndef STUNTRACEFX_RACE_H
#define STUNTRACEFX_RACE_H

#include <stdint.h>

#include "object/object.h"

void CameraFollowStep(Camera *cam);

enum { RASTER_NONE, RASTER_ONE_PLAYER, RASTER_TWO_PLAYERS, RASTER_COURSE };

/* The race's pacing (plan stage 8, decisions 4 and 5): one whole step every
   6 updates in every mode, the step's work told it took 6 frames, with no
   slowdown. */
enum { RACE_STEP_UPDATES = 6 };

/* The course menu's race steps every 4 updates (the menus' pace,
   MENU_STEP_FRAMES in menu/screens.c). */
enum { RACE_MENU_STEP_UPDATES = 4 };

/* For the pictures drawn between steps (plan stage 10, fx session):
   - race_step_progress: how far a picture drawn now lies from the last
     step's end (0) to the next step's (1): (updates since that step +
     between) / its pace (6, or 4 for the course menu's), clamped; 1 after
     a screen's first step, 0 when no step has handed a picture over since
     the screen's set-up.
   - RaceViewForPicture: the last step's picture part before its frame
     program, for panel 0 and then (two players) panel 1; the caller runs
     FxFrameProgram after each. Writes g, objects (tied objects' facing on
     the course menu), the draw list and fx_ram only. 0: no such panel.
   - race_steps_end: the race's screen handed back, or the course menu's
     race over (the race session begins); the kept step ends dropped. */
float race_step_progress(float between);
int RaceViewForPicture(int panel);
void race_steps_end(void);

/* Whether the race's steps are one player's (race_frame.c). */
int race_one_player(void);

/* The race seen from above (overhead.c), switched with the view button
   beside the pause button (pause.c, input/controls.c), the player's choice
   kept in the options; SRFX_CAMERA=overhead in the environment starts it so:
   - race_overhead_on: the view from above shown, wholly or in part (while
     the view switches), in a one-player race filling the window;
   - race_overhead_reset: a new screen's steps (the view's heading is taken
     again from the player's car at its first picture);
   - race_overhead_view: in a one-player picture's part between steps, after
     the game's view is set from its camera: the overhead view in its place
     (g's view place and angles, the chip's window words, fx_view), or one
     between the two while the view switches; else nothing;
   - race_view_switchable: the view button does something now (a one-player
     race filling the window, not switching already); race_view_overhead:
     the view it is (or is switching to) from above; race_view_toggle: the
     button pressed: the view switches, over 2 seconds;
   - race_view_hold: in each update, first: 1 while the view switches, the
     game held (its logic stopped, its picture as it stood) that update;
     race_view_moving: whether it is switching;
   - race_overhead_between: in each draw, first: the draw's place between
     the updates, for the switch's own smooth motion; it gives the place
     the game's picture is drawn at: while the view switches, the one it
     stood at when the switch began (the game held, its picture as it
     stood), else the draw's own. */
int race_overhead_on(void);
void race_overhead_reset(void);
void race_overhead_view(void);
int race_view_switchable(void);
int race_view_overhead(void);
void race_view_toggle(void);
int race_view_hold(void);
int race_view_moving(void);
float race_overhead_between(float between);

/* The race the course menu shows behind it. RaceStart sets it up; each
   RaceFrameStep is one step of it, with no waiting inside. It draws the
   picture into fx_draw_picture(0) unless step_only is set. */
void RaceStart(void);
void RaceFrameStep(int step_only);

/* wanted (from the menus and saves): */
/* one update of attract demo `which` (0, 1, 2); 0 while it runs, 1 when it
   returned with a button pressed, -1 for the restart (BootWarm) */
int DemoRun(int which);
/* the records panel's map buffer: one tile byte at a byte offset */
void RecordsPanelTile(uint16_t offset, uint8_t tile);
void PlayerSound35(void);
void CourseScreenState8(void);
/* the course screen before a race (course.c) */
void RaceLoad(void);
int CourseScreenBegin(void);            /* 0: the mode runs no race, no screen */
void CourseScreenRasterBlank(void);
int CourseScreenUpdate(void);           /* 1: its fade out is done, the race goes on */
void CourseScreenRasterLine55(void);
void CourseScreenStep(void);
uint8_t CourseFromSelection(void);
/* the race after the course menu: begin once, then each update until nonzero */
void race_session_begin(void);
int race_session_update(void);
/* the next race's course screen left out: its choice made as data, course
   `course` in the mode already set (a launch straight into a course's race,
   menu/screens.c) */
void race_launch_course(int course);

void RaceTimeInit(void);
void TimeLeftAddLap2(void);
void TimeLeftAddLap3(void);
void RaceClockStep(void);
void LapSplit(int offset);
void LapSplitTimes(int offset);
void RaceTimeKeep(void);
unsigned BcdValue(uint16_t v);           /* four decimal digits, a nibble each, as a number */
uint16_t BcdOf(unsigned n);              /* a number (modulo 10000) as four decimal digits */
uint16_t BcdAdd(uint16_t a, uint16_t b); /* decimal sum and difference, wrapping at four digits */
uint16_t BcdSub(uint16_t a, uint16_t b);
unsigned TimeHundredths(const uint8_t t[3]);            /* a [hundredths, seconds, minutes] decimal time in hundredths */
void TimeSetHundredths(uint8_t t[3], unsigned hundredths);
void TimeUnpackBcd(uint16_t packed, uint16_t *minutes, uint16_t *seconds, uint16_t *hundredths);
void ChampionshipFinish(void);

/* race_frame.c */
void RaceFrameRun(void);
void RaceFrameRunTwoPlayers(void);
void RaceVarsReset(void);
void RaceModeSetup(void);
void PlaceEntriesInOrder(void);
void AttractCourseSetup(void);
void ViewSetupTail(void);

/* flow.c */
void race_session_begin(void);
int race_session_update(void);
int DemoRun(int which);
int race_flow_stopped(void);
int race_ending_running(void);        /* the ending's three demos are playing */
void race_wait_overlay(void (*after)(void));
void race_wait_champion(void (*after)(void));
void race_go_on(void);
void OverlayFrameStep(void);
void PauseFrameStep(void);
int PauseOnStart(void);                 /* 1: paused, the step stops here */
int OamStagePause(void);
int OamStagePause2(void);
void OamStagePauseLoop(void);
void OamStagePause2Loop(void);
void race_pause_begin(int leaves);      /* 1: the pause only ends by leaving the race */
void race_pause_over(int second_half);
int race_resuming(void);                /* a step going on after a pause */
void race_resume_done(void);
void RaceFrameRunResume(void);
void RaceFrameRunTwoPlayersResume(int second_half);
int ChampionshipScreenFrame(void);
void race_frame_upload(void);
void SoftResetCheck(void);
void session_race_ended(int to_course_menu);

/* phases.c */
void PhaseNothing(void);
void RaceStartCountdown(void);
void RaceStartCountdownStunt(void);
void RaceReadyShort(void);
void RaceReadyLong(void);
void RaceStartSteps(void);
void RaceStartGlyphsSteps(void);
void RaceStartTimer(void);
void RaceStartTimerSteps(void);
void ResultsPanelSteps(void);
void CarFinishSound(void);
void CarFinishSoundP2(void);
void RaceFinishSounds(void);
void RaceLapCrossed(int player);
void RacePhaseLap(void);
void RacePhaseLapP1(void);
void RacePhaseLapP2(void);
void PhaseGlyphsFive(void);
void PhaseGlyphsSix(void);
void PhaseGlyphsFourA(void);
void PhaseGlyphsFourB(void);
void RaceEndWaitRow6B(void);
void RaceEndWaitBigSprites(void);
void ResultsWindowWait(void);
void RaceEndWindowSteps(void);
void PauseResultsP2(void);
void PauseResultsFourB(void);
void PauseResults(void);
void RaceEndWindowNine(void);
void RaceEndLeave(void);
void TimeUpBattle(void);
void TimeUpStunt(void);
void TimeUpRace(int player);

/* race_frame.c, boot flow */
void RaceFinishSplit(int player);
void RaceFinishPhases(int player, uint8_t phase);
void RaceRestart(void);
void RaceEndToAttract(void);
void RaceEndVideoReset(void);
void SoftResetToBootWarm(void);
void race_leave_to_champion(void);       /* the championship's closing screen, then the restart */
void race_champion_screen(void (*after)(void));   /* the champion's screen (the menus' ChampionScreenRun), then `after` */
void BootSoftRestart(void);             /* the ending: three demos, then the game from its start */
void ResultsScreenStep(void);
/* from video (race_video.c): the palette field of `count` entries of the
   map buffer from byte offset `at` (odd: the attribute bytes), and the map's
   rows to BG2 at $5C00 */
void TilemapPaletteCycle(unsigned at, int count);   /* 07:CEDF, every fourth frame */
void TilemapPaletteFill(unsigned at, int count, uint8_t bits);  /* 07:CF14 */
void ResultsMapRowsUpload(void);         /* 7E:B24C */

/* time_bonus.c, player_stats.c */
void GsuTimeDigits(void);
void GsuTimeDigitsFull(void);
void RaceEndCountdown(void);
void RaceEndCountdownFade(void);

/* hud_setup.c */
void HudBestTimeDigits(uint16_t packed);
uint16_t TimePackBcd(uint8_t hundredths, uint8_t seconds, uint8_t minutes);

/* misc.c */
void TryUsed(void);             /* 07:EE11: a try used at a race's end */

/* player_stats.c */
void SpeedSampleAdd(uint16_t speed, int player);
void PlayerStatsFromRecord(uint16_t rec, int player);
/* how far a player's car has gone in the race, in laps (player_stats.c) */
double PlayerRaceProgress(int player);
void PlayerStatsAndPaletteAnimate(void);
void RaceSoundsPlayer1(void);
void RaceSoundsPlayer2(void);
void RaceSoundsAndHud(void);
void TimeUpPhaseStep(void);

/* time_bonus.c */
void TimeBonusTally(void);
void TimeBonusTransfer(void);
void TimeLeftDigits(void);
void BonusDigitsDrawAt(uint16_t x);

/* hud_setup.c */
void RaceHudSetup(void);
void RaceHudSetupTwoPlayers(void);

#endif
