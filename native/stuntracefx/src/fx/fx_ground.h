/*
 * fx_ground.h: the race's ground and water as a surface (fx/fx_ground.c).
 */
#ifndef STUNTRACEFX_FX_GROUND_H
#define STUNTRACEFX_FX_GROUND_H

/* The surface of the course into the picture being drawn: the frame
   program's first drawing after the screen clear (render/frame.c). */
void fx_ground_draw(void);
/* The water's holes (the picture's, fx_face_holes, where a track piece goes
   below it): the ocean leaves them out (render/fx_draw.c, at the 3D's end). */
#include "fx/fx_face.h"
void fx_ground_water_holes(const FxHole *holes, int n);

#endif
