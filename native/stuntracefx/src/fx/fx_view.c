/*
 * fx_view.c: a picture drawn from another camera than the game's (see
 * fx_view.h).
 */
#include "fx/fx_view.h"

static FxView view;
static int on;

void fx_view_set(const FxView *v)
{
    view = *v;
    on = 1;
}

void fx_view_clear(void)
{
    on = 0;
}

int fx_view_on(void) { return on; }

float fx_view_zoom(void) { return on && view.zoom > 0 ? view.zoom : 1.0f; }

uint16_t fx_view_light(int axis)
{
    static const uint16_t game[3] = { FX_LIGHT_GAME_X, FX_LIGHT_GAME_Y, FX_LIGHT_GAME_Z };

    if (axis < 0 || axis > 2)
        return 0;
    return on ? (uint16_t)view.light[axis] : game[axis];
}

void fx_view_light_vector(double l[3])
{
    int k;

    for (k = 0; k < 3; k++)
        l[k] = (int16_t)fx_view_light(k) / 32768.0;
}

/* Only a depth in front of the view (below $8000) is moved: one behind it
   keeps the chip's reading of it. */
uint16_t fx_view_depth(uint16_t depth)
{
    if (!on || (depth & 0x8000))
        return depth;
    return depth > view.depth_less ? (uint16_t)(depth - view.depth_less) : 0;
}

int fx_view_merge(void) { return on && view.merge; }
