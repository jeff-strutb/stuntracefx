/*
 * race.h: the race as the rest of the game calls it.
 */
#ifndef STUNTRACEFX_RACE_H
#define STUNTRACEFX_RACE_H

#include <stdint.h>

#include "object/object.h"

void CameraFollowStep(Camera *cam);

enum { RASTER_NONE, RASTER_ONE_PLAYER, RASTER_TWO_PLAYERS, RASTER_COURSE };

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
int race_pacing_record(const char *path);   /* check only: each step's frames from a recording */
void race_session_begin(void);
int race_session_update(void);

void RaceTimeInit(void);
void RaceTimeAddD814(void);
void RaceTimeAddD815(void);
void RaceClockStep(void);
void LapSplit(int offset);
void LapSplitTimes(int offset);
void RaceTimeKeep(void);
uint16_t BcdAdd16(uint16_t a, uint16_t b, unsigned *carry);
uint16_t BcdSub16(uint16_t a, uint16_t b, unsigned *carry);
void TimeUnpackBcd(uint16_t packed, uint16_t *minutes, uint16_t *seconds, uint16_t *hundredths);
void ChampionshipFinish(void);

/* race_frame.c */
void RaceFrameRun(void);
void RaceFrameRunTwoPlayers(void);
void RaceVarsReset(void);
void RaceModeSetup(void);
void Set7EFFA6Sequence(void);
void AttractCourseSetup(void);
void ViewSetupTail(void);

/* flow.c */
void race_session_begin(void);
int race_session_update(void);
int DemoRun(int which);
int race_flow_stopped(void);
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
void TilemapRowsTo5C00(void);           /* 7E:B24C */

/* time_bonus.c, player_stats.c */
void GsuTimeDigits(void);
void GsuTimeDigitsFull(void);
void RaceEndCountdown(void);
void RaceEndCountdownFade(void);

/* hud_setup.c */
uint16_t RecordOffsetAllCourses(void);
void HudBestTimeDigits(uint16_t packed);
uint16_t TimePackBcd(uint8_t hundredths, uint8_t seconds, uint8_t minutes);

/* misc.c */
void Countdown0DF8(void);

/* player_stats.c */
void SpeedSampleAdd(uint16_t speed, int player);
void PlayerStatsFromRecord(uint16_t rec, int player);
void PlayerStatsAndPaletteAnimate(void);
void RaceSoundsPlayer1(void);
void RaceSoundsPlayer2(void);
void RaceSoundsAndHud(void);
void TimeUpPhaseStep(void);

/* time_bonus.c */
void TimeBonusTally(void);
void TimeBonusTransfer(void);
void TimeLeftDigits(void);
void GsuDigitPair0E74At(uint16_t x);

/* hud_setup.c */
void RaceHudSetup(void);
void RaceHudSetupTwoPlayers(void);

#endif
