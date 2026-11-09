/*
 * fx_draw.h: the Super FX's drawing as draw lists. The picture is built on
 * the CPU as a list of shapes and filled by the GPU.
 */
#ifndef STUNTRACEFX_FX_DRAW_H
#define STUNTRACEFX_FX_DRAW_H

#include <stdint.h>

#include "nshvc/nshvc.h"

void fx_draw_init(void);
/* The list the Super FX's frame draws into: its size in pixels. */
void fx_draw_set_target(nshvc_draw_list list, uint32_t width, uint32_t height);

#endif
