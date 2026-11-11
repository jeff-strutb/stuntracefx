/*
 * fx_seat.h: the cars seated on the road (fx/fx_seat.c).
 */
#ifndef STUNTRACEFX_FX_SEAT_H
#define STUNTRACEFX_FX_SEAT_H

#include <stdint.h>

/* A picture begins (fx_draw_begin): its surfaces so far become its last. */
void fx_seat_picture(int picture);
/* An object begins (fx_far_record): its draw record and its points'
   halvings; 0 at the end of the objects. */
void fx_seat_object(uint16_t record, int shift);
/* A face of the object drawing now, n view-space points (at its scale): a
   car's moved to its seat; any other's kept as a surface the cars can
   stand on. */
void fx_seat_face(double (*c)[3], int n);
/* A solid of the object drawing now (fx_solid.c), its place in view
   space: a car's tyre stood on the road, any other part of a car moved
   with it. */
void fx_seat_solid(float pos[3]);
/* A sprite of the object drawing now (fx_face.c's fx_sprite_place), its
   place in view space: a car's moved with it; any other's placed under the
   road brought up onto it. */
void fx_seat_sprite(double p[3]);

#endif
