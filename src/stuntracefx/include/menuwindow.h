/*
 * menuwindow.h: Enhanced mode's menus in the whole window (remaster/
 * menuwindow.c).
 */
#ifndef SRFX_MENUWINDOW_H
#define SRFX_MENUWINDOW_H

#include <stdint.h>

/* each write the S-CPU makes (the front end's write hook): the PPU's
   registers' last values */
void mw_ppu_write(uint32_t addr, uint8_t value);
/* the last value written to PPU register addr ($2100-$213F) */
uint8_t mw_ppu_last(uint16_t addr);
/* at each frame's end (rm_frame_end), after the full window's */
void mw_frame_end(void);

#endif
