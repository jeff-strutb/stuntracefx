/*
 * gpu_bin.c: the full window's spans laid out for a GPU's fill (gpu_bin.h),
 * the same for every GPU the cartridge has (gpu_metal.m, gpu_vulkan.c).
 */
#include <stdlib.h>
#include <string.h>

#include "gpu_bin.h"

void gpu_bin_sizes(const FwSpan *sp, size_t n, GpuBinSizes *z)
{
    size_t i;

    memset(z, 0, sizeof *z);
    for (i = 0; i < n; i++) {
        if (sp[i].textured == 3) {
            const FwPoly *p = &fw_polys[sp[i].x1];
            size_t rr = (size_t)(sp[i].x0 - sp[i].y);
            z->total += rr;
            z->rows += rr;
            if ((size_t)sp[i].x1 + 1 > z->polys)
                z->polys = (size_t)sp[i].x1 + 1;
            if ((size_t)(p->le + p->ln) > z->edges)
                z->edges = (size_t)(p->le + p->ln);
            if ((size_t)(p->re + p->rn) > z->edges)
                z->edges = (size_t)(p->re + p->rn);
        } else {
            z->total++;
        }
    }
}

int gpu_bin_fill(const FwSpan *sp, size_t n, int h, FwSpan *dst, uint32_t *row_at, uint32_t *row_spans,
                 uint32_t *row_poly)
{
    static uint32_t *put;
    static size_t put_len;
    size_t i, id = 0, k = 0;
    uint32_t run = 0, at = 0, c;
    int r;

    /* each row's count: a difference at a span's or polygon's first row
       and past its last, run on; then their starts */
    memset(row_at, 0, ((size_t)h + 1) * 4);
    for (i = 0; i < n; i++) {
        int32_t end = sp[i].textured == 3 ? sp[i].x0 : sp[i].y + 1;
        row_at[sp[i].y]++;
        row_at[end]--;
    }
    for (r = 0; r < h; r++) {
        run += row_at[r];
        c = run;
        row_at[r] = at;
        at += c;
    }
    row_at[h] = at;
    if (put_len < (size_t)h) {
        free(put);
        put = malloc((size_t)h * 4);
        put_len = put ? (size_t)h : 0;
        if (!put)
            return 0;
    }
    memcpy(put, row_at, (size_t)h * 4);
    for (i = 0; i < n; i++) {
        if (sp[i].textured == 3) {
            int32_t row;
            for (row = sp[i].y; row < sp[i].x0; row++, id++) {
                dst[id].y = row;
                row_poly[2 * k] = (uint32_t)id;
                row_poly[2 * k + 1] = (uint32_t)sp[i].x1;
                k++;
                row_spans[put[row]++] = (uint32_t)id;
            }
        } else {
            dst[id] = sp[i];
            row_spans[put[sp[i].y]++] = (uint32_t)id;
            id++;
        }
    }
    return 1;
}

int gpu_bin_bank(const FwSpan *s)
{
    if (s->textured == 3)
        return fw_polys[s->x1].kind ? fw_polys[s->x1].bank : -1;
    return s->textured ? s->bank : -1;
}
