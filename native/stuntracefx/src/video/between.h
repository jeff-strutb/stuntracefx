/*
 * between.h: the 2D drawn between updates (plan stage 10's frame rate:
 * pictures between updates on displays faster than 60 Hz).
 *
 * The engine draws once per display refresh and passes `between`, how far
 * real time is past the last update towards the next (0 to 1). A picture
 * shows what moved at its place between the last two updates' values; what
 * changes in steps (digits, frames of art, colours, layers and sprites
 * switched on or off) shows as the newest update left it. The race's 3D
 * and its hills take their own in-between state from fx_tween; this is the
 * rest: the sprites, the layers' scrolls, the windows and the per-line
 * tables of edges and scrolls.
 */
#ifndef STUNTRACEFX_BETWEEN_H
#define STUNTRACEFX_BETWEEN_H

#include <stdint.h>

#include "video/picture.h"

/* A value between its last two updates' values: was + (now - was) *
   between, rounded. More than `far` apart, the nearer of the two (was
   before halfway, now from halfway on), for a thing placed anew or
   jumping. */
int32_t tween_position(int32_t was, int32_t now, float between, int32_t far);

/* The same for a value that wraps every `period` (a scroll), the shorter
   way round. */
int32_t tween_wrapped(int32_t was, int32_t now, float between, int32_t far, int32_t period);

/* A table worked out at draw time (per-line edges or scrolls), eased entry
   by entry against the table the update before left. It is kept per update,
   so all the pictures of one update ease from the same pair. With none
   kept (no picture showed the update before), or a table of another
   length, it is left as it is. `period` 0 for values that don't wrap,
   TWEEN_SCROLL for a scroll (round 1024, or 256 for a value kept in a
   byte). */
enum { TWEEN_SCROLL = -1 };
typedef struct {
    uint64_t at;
    int have, n, was_n;
    int32_t was[1024], now[1024];
} TweenTable;
void tween_table(TweenTable *keep, int32_t *values, int n, int32_t far, int32_t period);

/* main's draw calls begin first and end last. In between, pic holds the
   moving 2D eased to this picture: the menu screens by the menus' hook,
   the race's screens (the race, the course screen, the results) here. End
   puts the game's own pic back. */
void video_between_begin(float between);
void video_between_end(void);
float video_between(void);              /* this picture's between; 1 outside a draw */
int video_between_drawing(void);        /* between begin and end */
void video_between_menu_set(void (*ease)(Picture *shown, float between));

#endif
