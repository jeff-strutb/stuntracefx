/*
 * player_stats.h: the race's player statistics and the phases that run on
 * the race's phase word, as far as the port has them (race/player_stats.c).
 */
#ifndef SRFX_PLAYER_STATS_H
#define SRFX_PLAYER_STATS_H

#include <stdint.h>

/* A speed sample for player `player` (0 or 1) from `speed` (two's
   complement): |speed| * $3C / 256 kept to $FF, stored as the player's
   sample and added to the player's 32-bit total, the count of samples
   going up with it. A total that would pass $FFFFFFFF stays there and the
   sample is not counted. Returns the total's high word. */
uint16_t speed_sample_add(uint16_t player, uint16_t speed);

/* The end of the Stunt Trax panel once all eight stunts are done: the bit
   for the course ($0C-$0F) in the save's byte $FC06, then the save's
   checksum. Any other course: nothing. */
void stunt_course_mark(void);

/* The Stunt Trax panel's count states: the count's line is drawn (the two-line
   glyph program for stunt_panel_count, the rows at y $20 for stunt_panel_show),
   then the count $0E3F goes down; at 0 the panel's state $0E41 is cleared. */
void stunt_panel_count(void);
void stunt_panel_show(void);

/* The time-up phase's first step: two glyph sprites placed and the phase
   word $0DD6 stepped by two. */
void time_up_phase_start(void);
/* The time-up phase's slides of the glyph sprites in and out. */
void time_up_phase_slide_in(void);
void time_up_phase_slide_out(void);

#endif
