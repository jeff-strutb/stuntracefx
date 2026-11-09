/*
 * fx_draw.h: the Super FX's drawing as draw lists. The picture is built on
 * the CPU as a list of shapes and filled by the GPU.
 */
#ifndef STUNTRACEFX_FX_DRAW_H
#define STUNTRACEFX_FX_DRAW_H

#include <stdint.h>

#include "nshvc/nshvc.h"

void fx_draw_init(void);
/* Which of the two pictures the Super FX's next drawing goes into. */
void fx_draw_begin(int picture);
/* Where the picture's top left goes on a screen other than the race's (the
   course menu's car); (0, 0) gives the race's places back. */
void fx_draw_set_origin(int x, int y);
/* The palette row a 4-bit or 2-bit picture shows in (the race's two-player
   screen: 2); an 8-bit picture uses the whole palette. */
void fx_draw_set_palette_row(int row);
/* The layer showing picture n (with two players, both). */
nshvc_draw_list fx_draw_picture(int picture);
/* The list drawn into, by size in pixels (the checks). */
void fx_draw_set_target(nshvc_draw_list list, uint32_t width, uint32_t height);

#endif
