/*
 * fx_art.h: the HUD's pictures drawn as clean geometry (plan stage 10,
 * "Pictures as geometry"): each picture's shapes as straights and circle
 * arcs, fitted to its texels, drawn as flat pieces at the output's
 * resolution in place of its texels.
 */
#ifndef STUNTRACEFX_FX_ART_H
#define STUNTRACEFX_FX_ART_H

#include <stdint.h>

/* A corner of a shape's edge, in its picture's texel space (x right, y down;
   texel x, y covers [x, x+1) x [y, y+1)). The edge runs straight from corner
   to corner, each corner rounded by the circle arc of radius r tangent to
   both its edges (0: sharp). Corners go round with the shape on their right:
   clockwise on the screen round its outside, anticlockwise round a hole. */
typedef struct {
    float x, y, r;
} FxArtCorner;

typedef struct {
    int n;
    const FxArtCorner *c;
} FxArtLoop;

typedef struct {
    uint8_t value;              /* the picture's colour number it shows (1-3) */
    int loops;
    const FxArtLoop *loop;
    float grow;                 /* layered: its loops grown by this (an outline drawn from its fill) */
} FxArtLayer;

enum {
    /* layer 0 the fill, shown in 3; 2 is the art's in-between texels at the
       fill's edge, which the GPU's smoothing of that edge makes; 1 the fill
       grown by one texel, and that moved 1, 2 ... `shadow` texels right and
       down (the drop shadow), under the fill */
    FX_ART_OUTLINED,
    /* layer 0 the fill: 1 the fill grown by one texel, 2 the fill, 3 the
       fill where the fill also lies one texel right, down, and both (the
       face; the 2 left showing is the bevel), in that order */
    FX_ART_BEVELLED,
    /* each layer its own region and value, a later one over an earlier */
    FX_ART_LAYERED
};

typedef struct {
    const char *name;
    uint8_t bank;
    uint16_t at;                /* its top left texel's byte: rows 32 bytes apart, 4 texels a byte, the low bits leftmost */
    uint8_t w, h;
    uint8_t style, shadow;
    int layers;
    const FxArtLayer *layer;
    int off;                    /* its texels the clean shapes leave on the other side, as fitted */
    /* console art (bank FX_ART_TILES): its 8 x 8 tiles row by row from tile
       set `at` (fx_art_tile_set), 4 bits a texel; and for an outlined or
       bevelled picture the tile colours its values 1-3 are */
    const uint16_t *tiles;
    uint8_t colour[4];
} FxArtPicture;

#define FX_ART_TILES 0xFF

extern const FxArtPicture fx_art_pictures[];
extern const int fx_art_picture_count;

/* the cartridge's byte (the game's fx_rom_read; a tool's own) */
uint8_t fx_art_rom(uint8_t bank, uint16_t at);

/* tile set n's unpacked 4-bit tiles, 32 bytes a tile, the SNES's planes
   (the game's: the video blocks it unpacks; a tool's own); NULL if none */
const uint8_t *fx_art_tile_set(uint16_t n, uint32_t *bytes);

/* ---- the geometry, shared with the fitting tool ---- */

/* The loop's edge as points, its straights and arcs, grown by `grow` (0: the
   shape's own edge) and moved by (dx, dy): each straight moved out by grow,
   each arc keeping its centre (r + grow at a convex corner, r - grow at a
   concave one, sharp when that is not above 0). Returns the point count, 0
   when `room` is short. */
int fx_art_trace(const FxArtCorner *c, int n, double grow, double dx, double dy, double (*out)[2], int room);

/* Each texel's share inside the loops (their insides on their right, by the
   winding rule), w x h row by row into cov: exact for the points' polygon. */
void fx_art_cover(const double (*p)[2], const int *len, int loops, int w, int h, double *cov);

/* Each texel's share of k x k evenly spread points inside the loops by the
   winding rule, w x h row by row into share. */
void fx_art_share(const double (*p)[2], const int *len, int loops, int w, int h, int k, double *share);

/* Whether (x, y) is inside the loops by the winding rule (not 0). */
int fx_art_inside(const double (*p)[2], const int *len, int loops, double x, double y);

/* The region inside, by the winding rule, every one of `sets` sets of loops
   (each loop's set in `set`, 0 to sets - 1; NULL: all in set 0), as flat
   triangles (3 points each) into tri. Returns the triangle count; -1 when
   room is short. */
int fx_art_pieces(const double (*p)[2], const int *len, const int *set, int loops, int sets, float (*tri)[2], int room);

/* ---- the pictures ---- */

/* The picture a run of rows starts in: the picture holding the texel row
   at `at` in `bank`, and that row's place in it (its column, a multiple of
   4, and row). NULL when none. */
const FxArtPicture *fx_art_find(uint8_t bank, uint16_t at, int *column, int *row);

/* The flat pieces of a picture's colour `value`, in its texel space, made
   (and checked against its texels) the first time it is asked for: triangle
   count and points. 0 pieces when the picture failed its check (then it is
   drawn from its texels as before). */
int fx_art_good(const FxArtPicture *p);
int fx_art_layers(const FxArtPicture *p);
const float (*fx_art_layer(const FxArtPicture *p, int layer, int *triangles, uint8_t *value))[2];

#endif
