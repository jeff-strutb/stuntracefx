/*
 * smallvideo.h: the small video and save routines: the backdrop's scroll
 * from the view, the colour maths settings, the two record tables in bank
 * $7F, and the save's checksum.
 */
#ifndef SRFX_SMALLVIDEO_H
#define SRFX_SMALLVIDEO_H

#include <stdint.h>

/* video/backdrop.c */
/* 8 when the vehicle of `player` (0 or not) is raised, else 0. */
uint16_t player_vehicle_raised(uint8_t player);
/* The backdrop's scroll for the current half: across, down and the raster
   value are stored; the vertical scroll is returned. */
uint16_t backdrop_scroll_from_view(void);

/* video/colour_math.c */
void colour_math_a3(void);                  /* subtract, half, BG1 and BG2 */
void colour_math_a3_long(void);
void colour_math_a7_start(void);            /* sound $04 on port 1, then mode $A7, fixed step 0 */
uint8_t fixed_colour_from_step(void);       /* the fixed colour's intensity, returned */
void colour_math_off(void);
void colour_math_off_long(void);

/* video/records_7f.c */
/* The records of the position list at bank:$F634 (bank: the data bank). */
void records_7f_from_f634(uint8_t bank);
/* The records of the list at bank:$F170, then the eight blocks and the
   $40-byte DMA copy; returns the low byte of the list's end. */
uint8_t records_7f_from_f170(uint8_t bank);

/* save/checksum.c */
/* The save's checksum, stored twice in the save; returned. */
uint8_t save_checksum(void);

#endif
