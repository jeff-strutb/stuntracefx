/*
 * fps.h: the pictures drawn a second, shown over the picture (fps.c).
 */
#ifndef STUNTRACEFX_FPS_H
#define STUNTRACEFX_FPS_H

/* In draw, each picture: counted, and the count shown at the window's top
   right (SRFX_FPS=0 in the environment: not shown). */
void fps_draw(void);
/* In draw, in its place: not shown (the pause screen's blur is being made
   from the pictures). */
void fps_hide(void);

#endif
