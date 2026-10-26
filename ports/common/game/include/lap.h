/*
 * lap.h: lap splits and the race time copy (race/lap.c).
 */
#ifndef SRFX_LAP_H
#define SRFX_LAP_H

#include <stdint.h>

/* Records a lap in slot x (0, 3 or 6): the time since the last mark goes
   into the slot, and the clock becomes the new mark. Returns the race
   minutes. */
uint8_t lap_split(uint8_t x);

/* Copies the race clock (hundredths, seconds, minutes) to its copy at
   $7E:0EE7-$0EE9. */
void race_time_keep(void);

#endif
