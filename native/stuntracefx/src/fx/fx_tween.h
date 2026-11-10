/*
 * fx_tween.h: what the pictures between steps (fx/fx_tween.c) tell other
 * parts of the game; the race's calls are in fx/fx_draw.h.
 */
#ifndef STUNTRACEFX_FX_TWEEN_H
#define STUNTRACEFX_FX_TWEEN_H

/* While a picture between steps is drawn, how far it is through the step's
   wait (0 the last step's end, 1 this one's); -1 when none is. */
float fx_tween_t(void);

#endif
