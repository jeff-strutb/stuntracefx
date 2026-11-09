/*
 * menu_tables.h: the menu screens' per-line tables: which set the screen
 * uses and which of its channels are on, as the screen's own code says.
 */
#ifndef STUNTRACEFX_MENU_TABLES_H
#define STUNTRACEFX_MENU_TABLES_H

#include "menu/actors.h"

enum { MENU_LINES_TITLE, MENU_LINES_PREVIEW, MENU_LINES_CHAMPION };

/* `channels`: the channels on, as bits 3-7 of HDMAEN */
void menu_lines_set(int set, uint8_t channels);

/* the course menu's graphics, loaded a step a frame after the title */
void menu_load_screen_open(void);
void menu_load_step(Actor *a);
void menu_grid_load(uint16_t first_tile, int page);
void menu_frames_vram_tick(void);

/* one frame of the championship's selection animation, in BG2's tiles */
void menu_champion_frames_tile(uint16_t step);

#endif
