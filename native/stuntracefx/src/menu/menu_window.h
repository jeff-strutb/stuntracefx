/*
 * menu_window.h: the menus over the whole window (plan stage 10). The
 * original's 256 x 224 picture sits at the screen's origin (video's screen
 * shape); each screen's background goes on past it, and its other layers
 * are kept to it.
 */
#ifndef STUNTRACEFX_MENU_WINDOW_H
#define STUNTRACEFX_MENU_WINDOW_H

#include <stdint.h>

#include "nshvc/nshvc.h"

/* the layers a screen keeps to the original's picture (picture.h layer_clip) */
enum { KEEP_BG1 = 1, KEEP_BG2 = 2, KEEP_BG3 = 4, KEEP_SPRITES = 16 };

/* A screen opening: the layers kept to the picture, and whether BG1 is the
   course menu's granite with the car window cut in it (its map is built at
   the window's size, the granite going on around the picture). */
void menu_window_screen(uint8_t kept, int granite_bg1);

/* What a menu page (32 x 32 words) now holds, for the granite's map. */
void menu_window_page(int page, const uint16_t *words);

/* The 3D car's place: the original's (32, 96) on the picture, at its origin. */
void menu_window_car_origin(void);

/* The menus' update ends: the layers, the granite's map and the windows as
   the larger screen shows them. It begins: the original's back, for the
   game's own code. */
void menu_window_frame_end(void);
void menu_window_frame_begin(void);

/* the menus' updates so far: a picture between two updates eases between
   the last two (menu_tween.c) */
uint32_t menu_window_updates(void);

/* A window's edge from the original's columns to the screen's: the left
   edge at column 0 and the right at 255 reach the screen's edges. */
int32_t menu_window_left(int32_t x);
int32_t menu_window_right(int32_t x);

#endif
