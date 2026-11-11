/*
 * pause.h: the pause screen, the game's own pause (pause.c).
 */
#ifndef STUNTRACEFX_PAUSE_H
#define STUNTRACEFX_PAUSE_H

#include "nshvc/nshvc.h"

/* Each update, before the game reads its pads (input/controls.c's): 0 while
   the pause screen holds the game (its logic stopped, its picture as it
   stood), else 1, with any button still held from the pause screen taken
   out of pad until let go. The devices themselves are read for Escape, the
   corner button and a binding's key or controller button. */
int pause_update(const nshvc_input *input, uint16_t pad[2]);

/* Whether the pause screen holds the game: the picture is drawn as the last
   update the game ran left it (draw's `between` at 1). */
int pause_holding(void);

/* In draw, first: 1 when the pause screen has set the whole picture (the
   last picture before the pause, blurred on the GPU), so the game sets
   none; 0 when the game draws its own (not paused, or no blur here). */
int pause_draw(void);

/* Whether the pause screen is making its blur (the three pictures after it
   opens): no overlay may show, the touch screen's controls included. */
int pause_capturing(void);

/* The game's own updates: nshvc_updates() less those the pause screen held.
   The clock for whatever the game times by updates (the race's pictures
   between steps), so that a pause stops it. */
uint64_t pause_game_updates(void);

#endif
