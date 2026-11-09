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

void TimeUnpackBcd(uint16_t packed, uint16_t *minutes, uint16_t *seconds, uint16_t *hundredths);
void ChampionshipFinish(void);

#endif
