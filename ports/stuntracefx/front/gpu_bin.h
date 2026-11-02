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

#endif
