/*
 * baked.h: the front end's fixed pictures rasterized ahead (front_baked.c,
 * written by `build.sh bake` from the drawing code in art.c, pause.c and
 * pad_art.c, which no host runs): each an ARGB picture (straight alpha)
 * kept as runs of one value, row after row. A host only unpacks them.
 */
#ifndef SRFX_BAKED_H
#define SRFX_BAKED_H

#include <stdint.h>

enum {
    BAKED_WHEEL,                    /* the touch scheme's (art.c): its wheel, */
    BAKED_BRAKE,                    /* pedals and pause bars */
    BAKED_BOOST,
    BAKED_PAUSE_BARS,
    BAKED_ORIGINAL,                 /* the mode's badges (art.c) */
    BAKED_ENHANCED,
    BAKED_BTN_PAUSE,                /* the pause screen's buttons (pause.c) */
    BAKED_BTN_PLAY,
    BAKED_PAD,                      /* its controller, panel-sized (pad_art.c) */
    BAKED_COUNT
};

typedef struct {
    int w, h;
    unsigned nruns;
    const uint32_t *runs;           /* value, count; value, count; ... */
} BakedPic;

extern const BakedPic baked_pics[BAKED_COUNT];

/* picture `which` unpacked into new memory (free it), its size in *w, *h;
   NULL when the table has none */
uint32_t *baked_unpack(int which, int *w, int *h);

#endif
