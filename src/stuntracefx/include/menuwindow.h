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
/* The menus' sprites on a wider screen (MenuOamBuild): the columns the
   screen has past the console's picture on each side (0: the console's
   screen), and each sprite's X page for the column it is meant at, the
   whole table's from mw_sprites_begin */
int mw_sprite_side(void);
void mw_sprites_begin(void);
void mw_sprite_page(int sprite, int page);

#endif
