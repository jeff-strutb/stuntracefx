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
/* The screen's list's pixels per screen pixel: what race_frame_art_draw
   scales its art by. */
void fx_draw_list_scale(float *sx, float *sy);
/* Whether the screen's list is at the output's pixels (on once the frame
   art scales with it) or at the screen's 256 x 224. */
void fx_draw_output_pixels(int on);
/* The words the chip's view takes (GsuParamsFromDp's g.view_*): the view
   over a rectangle of the screen, x, y, width, height in screen pixels, its
   horizon's centre at x + width / 2 across and centre_y down; focal length
   128 (the original's scale, which the hills' scroll matches). */
typedef struct {
    uint16_t left, right, top, bottom, centre_x, centre_y, scale_32, scale_34;
} FxViewWords;
FxViewWords fx_view_words(int x, int y, int width, int height, int centre_y);
/* Pictures between two steps (fx/fx_tween.c). The race calls
   fx_tween_step_end after each step's picture; to draw at t of the way
   through a step's wait (0 the last step's end, 1 this one's) it calls
   fx_tween_begin(t), runs the picture's part of a step (the view, the
   objects' draw records, the sort, fx_draw_begin and the frame program),
   then fx_tween_end, which puts every byte of the state back. The picture
   shown is then that 3D with the step's HUD. fx_tween_reset when a race
   starts or ends. fx_tween_view: the hills' angles ($06CF heading, $06CD
   pitch) of the picture between shown, 0 when none is. */
void fx_tween_step_end(void);
void fx_tween_reset(void);
int fx_tween_begin(float t);
void fx_tween_end(void);
int fx_tween_view(uint16_t *heading, uint16_t *pitch);
/* fx_tween.c's side of fx_draw: the picture between drawn into its own
   recording, then shown; and none shown. */
void fx_draw_tween_on(void);
void fx_draw_tween_shown(void);
void fx_draw_tween_off(void);
/* The step's end; the picture the last step drew its panel n into (-1:
   none). */
void fx_draw_step_end(void);
int fx_draw_step_picture(int panel);
/* A picture between steps at draw(between): the race's progress through the
   step's wait, its picture part of each panel (RaceViewForPicture), the
   frame program into the picture that panel showed. Called by draw before
   the screen is set. */
void fx_tween_draw(float between);
/* The list drawn into, by size in pixels (the checks). */
void fx_draw_set_target(nshvc_draw_list list, uint32_t width, uint32_t height);

#endif
