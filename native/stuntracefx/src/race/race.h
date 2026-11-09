/*
 * race.h: the race as the rest of the game calls it.
 */
#ifndef STUNTRACEFX_RACE_H
#define STUNTRACEFX_RACE_H

#include <stdint.h>

#include "object/object.h"

void CameraFollowStep(Camera *cam);

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
/* the race after the course menu: begin once, then each update until nonzero */
void race_session_begin(void);
int race_session_update(void);

void RaceTimeInit(void);
void RaceTimeAddD814(void);
void RaceTimeAddD815(void);
void RaceClockStep(void);
void LapSplit(int offset);
void LapSplitTimes(int offset);
void RaceTimeKeep(void);
void TimeUnpackBcd(uint16_t packed, uint16_t *minutes, uint16_t *seconds, uint16_t *hundredths);
void ChampionshipFinish(void);

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

/* time_bonus.c, player_stats.c */
void GsuTimeDigits(void);
void GsuTimeDigitsFull(void);
void RaceEndCountdown(void);
void RaceEndCountdownFade(void);

/* hud_setup.c */
uint16_t RecordOffsetAllCourses(void);
void HudBestTimeDigits(void);
uint16_t TimePackBcd(uint8_t hundredths, uint8_t seconds, uint8_t minutes);

/* misc.c */
void Countdown0DF8(void);

#endif
