/*
 * fx_track.h: the track a car's tyres stand on in the depth-tested 3D
 * (docs/stuntracefx/native/DEPTH.md, 5): the course's collision models,
 * the ground the game's own simulation drives the cars on, placed where
 * each piece is drawn in the picture.
 */
#ifndef STUNTRACEFX_FX_TRACK_H
#define STUNTRACEFX_FX_TRACK_H

#include <stdint.h>

/* A picture begun: no pieces yet. */
void fx_track_picture(void);
/* The object `object` (its slot + 1, draw_record_object) drawn with its
   model's axes `turn` (row i: model axis i in view space) at `place` in
   view space: its collision model, when its record has one, kept for the
   cars seated after it. Once a picture an object. */
void fx_track_piece(uint16_t object, const double place[3], const double turn[3][3]);
/* The ground under point p of car `car` (its slot + 1, draw_record_car),
   up the world's up in view space: the topmost surface of the collision
   models of the pieces the car's record touches whose cell holds the
   point straight over or under p, no more than 100 over p and no more
   than 100 under it; its plane n . x = d, n facing up. 0 when none. */
int fx_track_ground(uint16_t car, const double p[3], const double up[3], double n[3], double *d);

#endif
