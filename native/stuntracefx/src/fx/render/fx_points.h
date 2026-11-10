/*
 * fx_points.h: a collision model's points (fx_points.c).
 */
#ifndef STUNTRACEFX_FX_POINTS_H
#define STUNTRACEFX_FX_POINTS_H

#include <stdint.h>

#include "data/ref.h"

/* Unpacks the point script `script` (its collision record's reference) into
   the point table FxCollideModel reads; `frame`, the model's frame, picks
   among its points where the script picks by frame. */
void FxModelPoints(CartRef script, uint16_t frame);

#endif
