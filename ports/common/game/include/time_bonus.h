/*
 * time_bonus.h: the race time on screen and the time bonus counted into it
 * (race/time_bonus.c).
 */
#ifndef SRFX_TIME_BONUS_H
#define SRFX_TIME_BONUS_H

#include <stdint.h>

/* The race time drawn at the digit size set in $70:0210; returns the
   screen mode value the S-CPU keeps. `bank` is the data bank the digit
   tables are read through. */
uint8_t draw_race_time(uint8_t bank);
/* The race time at the size of animation step $0D6B. */
uint8_t draw_race_time_grow(uint8_t bank);
/* The race time at full size. */
uint8_t draw_race_time_full(uint8_t bank);

/* The two digits of the bonus at $0E74 at the size of animation step
   $0D6B (grow, bounce), or at full size; drawn only while $0E6C is set. */
void draw_bonus_digits_grow(uint8_t bank);
void draw_bonus_digits_bounce(uint8_t bank);
void draw_bonus_digits_full(uint8_t bank);

/* The two digits of the bonus at $0E74 at (x, 8). */
void draw_bonus_digits_at(uint8_t bank, uint16_t x);

/* The renderer bytes for the time's digits. */
void time_digit_bytes(void);

/* One frame of the time bonus tally: the state machine at $0D65. */
void time_bonus_tally(uint8_t bank);
/* One frame of the transfer of the time bonus at $19F0 into the race time. */
void time_bonus_transfer(uint8_t bank);
/* One frame of the race time's large digit or two or three digits. */
void time_left_digits(uint8_t bank);

#endif
