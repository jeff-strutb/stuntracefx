/*
 * screen_setup.h: the screen's set-up routines (video/screen_setup.c).
 */
#ifndef SRFX_SCREEN_SETUP_H
#define SRFX_SCREEN_SETUP_H

#include <stdint.h>

/* The picture put to a known state: mode 3, the layers' maps and
   characters, colour 0 black, the windows and colour maths off, no HDMA;
   the Super FX's screen base $0B and its buffer start $2C00. x's low byte
   is the fixed colour's data (COLDATA), its high byte the set-up register
   (SETINI). */
void screen_ppu_reset(uint16_t x);

/* The race screen: the windows and colour maths, mode $0B with its maps and
   characters, colour 1 black, every layer's scroll at 0, and the two HDMA
   tables (the window bands and the BG2 vertical scroll, a line at a time)
   that the frame runs. */
void race_screen_setup(void);

#endif
