/*
 * gpu_bin.h: the full window's spans (fullwindow.h FwSpan) laid out for a
 * GPU's fill: every row's spans in their order, a polygon's (textured 3)
 * one a row with its place left for the GPU's rows kernel (fw_poly_row's
 * own steps) to make, and which polygon and row each such place is.
 */
#ifndef SRFX_GPU_BIN_H
#define SRFX_GPU_BIN_H

#include <stddef.h>
#include <stdint.h>

#include "fullwindow.h"

/* the sizes: total spans (a polygon's a row), the polygons' rows, and how
   many polygons and edges the spans name */
typedef struct {
    size_t total, rows, polys, edges;
} GpuBinSizes;
void gpu_bin_sizes(const FwSpan *sp, size_t n, GpuBinSizes *z);
/* the layout, into the GPU's memory: dst (total spans: a span's own, or a
   polygon row's place, its y set), row_at (h + 1: each row's first in
   row_spans), row_spans (total: span indices row by row), row_poly (two
   words a polygon row: its place and its polygon). 0 when out of memory. */
int gpu_bin_fill(const FwSpan *sp, size_t n, int h, FwSpan *dst, uint32_t *row_at, uint32_t *row_spans,
                 uint32_t *row_poly);
/* the bank a span's texels come from, or -1 (flat) */
int gpu_bin_bank(const FwSpan *s);

/* ---- the display list's faces (scene.h), the same for every GPU ---- */
/* a vertex of a face for the GPU: its point, its face (the list's index),
   its u, v made nearest its face's first corner's (the texture repeats
   every 256) */
typedef struct {
    uint32_t point, face;
    float u, v;
} GpuVert;
/* a span the GPU leaves to the reference (a check's, FW_GPUCHECK) */
static inline int gpu_bin_ref_only(const FwSpan *s) { return s->pad1[0] == 1; }
/* a span that is a display-list face (textured 6, x1 its index) */
static inline int gpu_bin_face(const FwSpan *s) { return s->textured == 6; }
/* the vertices of a run of face spans (each a face, in order): ntri
   triangles' corners (3 each, each face a fan) and nline lines' ends (2
   each) counted, then written when tri and line are not NULL; *runs the
   runs of the same kind in order, kind[i] 1 for lines, len[i] its vertices
   (when not NULL, room for n) */
void gpu_bin_faces(const FwSpan *sp, size_t n, GpuVert *tri, size_t *ntri, GpuVert *line, size_t *nline,
                   size_t *runs, uint8_t *kind, uint32_t *len);

/* The check (FW_GPUCHECK): a GPU's picture against the reference's, each
   pixel apart counted by how far the nearest texel with the GPU's value is
   in the reference (1, 2, 3, farther: [1], [2], [3], [0]), and of the
   farther ones those where the reference drew nothing */
typedef struct {
    long fills, apart, at[4], far_empty, far_most, far_most_at;
} GpuCheck;
void gpu_bin_check(GpuCheck *c, const uint8_t *gpu, const uint8_t *ref, int w, int h);
void gpu_bin_check_report(const char *who, const GpuCheck *c, long faces);
/* FW_GPUDUMP=N (a check): fill N's reference, the GPU's and their
   difference as PGMs into FW_GPUDUMP_DIR (.), and each pixel apart with the
   display-list face the GPU drew last there (ids: face + 1 a pixel, 0 none;
   NULL: not known) into gpufaces.txt; 1 when this fill was dumped */
int gpu_bin_dump(long fill, const uint8_t *ref, const uint8_t *gpu, const uint32_t *ids, int w, int h);

#endif
