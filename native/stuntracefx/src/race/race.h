/*
 * race.h: the race as the rest of the game calls it.
 */
#ifndef STUNTRACEFX_RACE_H
#define STUNTRACEFX_RACE_H

#include <stdint.h>

#include "object/object.h"

void CameraFollowStep(Camera *cam);

/* The race the course menu shows behind it. RaceStart sets it up; each
   RaceFrameStep is one step of it, with no waiting inside. show = 0 draws
   the picture, show = 1 runs the step alone. */
void RaceStart(void);
void RaceFrameStep(int show);

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
