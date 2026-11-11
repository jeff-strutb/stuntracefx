/*
 * fx_ground.h: the race's ground and water as a surface (fx/fx_ground.c).
 */
#ifndef STUNTRACEFX_FX_GROUND_H
#define STUNTRACEFX_FX_GROUND_H

#include <stdint.h>

/* The surface of the course into the picture being drawn: the frame
   program's first drawing after the screen clear (render/frame.c). */
void fx_ground_draw(void);
/* The water's holes (the picture's, fx_face_holes, where a track piece goes
   below it): the ocean leaves them out (render/fx_draw.c, at the 3D's end). */
#include "fx/fx_face.h"
void fx_ground_water_holes(const FxHole *holes, int n);
/* The light level the chip gives a lit face, unrounded: n its normal in view
   space, toward the eye (the course's fit of the chip's scale; without one,
   the chip's own). Used for a face seen from its back (fx/fx_face.c). */
double fx_ground_light(const double n[3]);
/* The colour byte a face painted in one of the course's own rock shades
   (the survey's run) takes seen from its back: on a course whose land is
   its rock (Sunset Valley), the shade a mesa's face turned the same way
   takes (n its normal in view space, toward the eye), so the back of a
   canyon's wall is rock like the land beside it. -1: not the course's
   rock; the face keeps its colour. */
int fx_ground_rock_back(uint8_t colour, const double n[3]);

#endif
