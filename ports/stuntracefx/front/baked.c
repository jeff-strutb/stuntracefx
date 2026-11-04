/*
 * baked.c: the front end's pictures rasterized ahead (baked.h), unpacked.
 */
#include <stdlib.h>

#include "baked.h"

uint32_t *baked_unpack(int which, int *w, int *h)
{
    const BakedPic *b;
    uint32_t *px;
    size_t i = 0, n;
    unsigned r;
    if (which < 0 || which >= BAKED_COUNT)
        return NULL;
    b = &baked_pics[which];
    n = (size_t)b->w * (size_t)b->h;
    if (!n || !(px = (uint32_t *)malloc(n * 4)))
        return NULL;
    for (r = 0; r < b->nruns && i < n; r++) {
        uint32_t v = b->runs[2 * r], k = b->runs[2 * r + 1];
        while (k-- && i < n)
            px[i++] = v;
    }
    if (i != n) {
        free(px);
        return NULL;
    }
    *w = b->w;
    *h = b->h;
    return px;
}
