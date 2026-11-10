/*
 * fx_face.h: the race's shapes in floats (fx/fx_face.c), between the frame
 * program, which says what is drawn, and the draw lists (fx/fx_draw.c).
 */
#ifndef STUNTRACEFX_FX_FACE_H
#define STUNTRACEFX_FX_FACE_H

#include <stdint.h>

/* A corner of a shape: its place on the picture in pixels, unrounded, and
   its texel. */
typedef struct {
    float x, y, u, v;
} FxCorner;

/* A face's shape: triangles (or a line's two ends) standing for the queue's
   entries from `entry` up to `end`. */
typedef struct {
    uint16_t entry, end;
    uint8_t line;
    uint32_t first, count;
} FxShape;

/* The faces committed since the queue was last drawn are made into shapes. */
void fx_face_settle(void);
/* The shape standing for the entry at `entry`, or NULL. */
const FxShape *fx_face_shape(uint16_t entry);
const FxCorner *fx_face_corners(void);
/* The queue was drawn: its shapes are gone. */
void fx_face_reset(void);

/* What the checks count: vertices of faces wholly on the screen, and those
   more than 1.02 pixels from the chip's own. */
typedef struct {
    uint32_t faces, vertices, apart, worst_face;
    double worst;
    /* the chip's near points (depth below $100, FxProjectPoint), whose
       pair FxRatioClamp halves before dividing; and every vertex past the
       chip's own error (fx_face.c check_point) */
    uint32_t near, near_apart, past_bound;
} FxFaceCheck;
void fx_face_check_on(int on);
FxFaceCheck fx_face_check(void);

/* A solid's face (fx_solid.c) of n view-space points, near-clipped,
   projected and kept to the view: its triangles' corners in `out`; the
   count made, 0 when none (n at most 64). */
uint32_t fx_face_triangles(const float (*p)[3], int n, FxCorner *out, uint32_t room);

/* The view's rectangle in the picture's pixels: left, right (past its last
   column), top, bottom (past its last row). */
void fx_face_view_rect(float *left, float *right, float *top, float *bottom);

/* A scaled sprite's place in floats, for fx_draw's sprite quads: its
   picture centre and the scale from the chip's whole-pixel box (whose
   corner is at left, top) to it. Returns 0 when the box is not the one
   fx_sprite_place last saw. */
int fx_sprite_map(uint16_t left, uint16_t top, float *cx, float *cy, float *bx, float *by, float *k);

#endif
