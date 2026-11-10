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
    /* how the chip chose a flat face's colour (fx_face_material): -1 not
       seen (a textured face), 0 its own index, 1 lit, 2 from the distance
       table; that index (for a lit face its shade row's address, in the
       nearest tier's table), the model's distance tier (0-3), and a lit
       face's light level unrounded */
    int8_t tint;
    uint8_t tier;
    uint16_t tint_index;
    float light;
    /* while the water cuts the picture (fx_face_cut): the face's part past
       the water from the eye, drawn before it (count 0: none) */
    uint32_t under_first, under_count;
    uint16_t pattern;           /* a pattern face's place among them (fx_face_pattern), plus one; 0 none */
} FxShape;

/* The faces committed since the queue was last drawn are made into shapes. */
void fx_face_settle(void);
/* The shape standing for the entry at `entry`, or NULL. */
const FxShape *fx_face_shape(uint16_t entry);
const FxCorner *fx_face_corners(void);
/* The queue was drawn: its shapes are gone. */
void fx_face_reset(void);

/* A pattern face's pieces (fx_pattern.c) with the texture's offsets the
   draw list adds: triangles' corners in `out`, each corner's texel value in
   `values` and in `past` 1 where the piece lies past the water from the eye
   (while the water cuts the picture, each piece is cut by it as a face is);
   the count made, 0 when the face is drawn with its texture (too many
   tiles, no area, or no room). */
uint32_t fx_face_pattern(const FxShape *sh, double u_off, double v_off, FxCorner *out, uint8_t *values, uint8_t *past, uint32_t room);

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
/* The same from points in doubles (the ground's plane, far across). */
uint32_t fx_face_triangles_d(const double (*p)[3], int n, FxCorner *out, uint32_t room);

/* The water's cut (fx_ground.c, Aqua Tunnel): on for a picture, its plane
   n . v = d in view space (d > 0: the eye above it). A view-space point
   past the water from the eye; whether the scaled sprite last placed is.
   The holes: where a model crosses the plane, the region its crossings
   enclose, in the picture's pixels (convex, at most 40 corners). */
typedef struct {
    int n;
    float x[40], y[40];
} FxHole;
void fx_face_cut(int on, const double n[3], double d);
int fx_face_cutting(void);
int fx_face_past_water(double x, double y, double z);
int fx_sprite_past_water(void);
const FxHole *fx_face_holes(int *n);

/* The ground's survey (fx_ground.c): while sinks are set, every model point
   the frame program projects goes to `point` too, in view space, and every
   face the chip queues to `face`: its corners in view space, its colour
   byte (the draw list's, after the kind; a textured face's is its
   texture's) and whether it is textured (NULLs end it). */
typedef void (*FxSurveyPoint)(double x, double y, double z, void *context);
typedef void (*FxSurveyFace)(const double (*corner)[3], int n, uint8_t colour, int textured, void *context);
void fx_face_survey(FxSurveyPoint point, FxSurveyFace face, void *context);

/* The view's rectangle in the picture's pixels: left, right (past its last
   column), top, bottom (past its last row). */
void fx_face_view_rect(float *left, float *right, float *top, float *bottom);

/* A scaled sprite's place in floats, for fx_draw's sprite quads: its
   picture centre and the scale from the chip's whole-pixel box (whose
   corner is at left, top) to it. Returns 0 when the box is not the one
   fx_sprite_place last saw. */
int fx_sprite_map(uint16_t left, uint16_t top, float *cx, float *cy, float *bx, float *by, float *k);

#endif
