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
/* The output's pixels per picture pixel of what the 3D's world draws now:
   the list's scale, times the view's zoom while a zoomed view's world is
   drawn (fx/fx_view.h); for detail set by the output's pixels. */
void fx_draw_world_scale(float *sx, float *sy);
/* The 3D's world is drawn (render/frame.c, before its text notice): what
   the frame program draws after it is on the screen as drawn, not
   magnified with a zoomed view's world. */
void fx_draw_world_end(void);
/* Each object as FxObjectsDraw takes it (render/frame.c): whether it is
   placed in the view's space (its draw record's flags bit 7: the rank sign
   the race holds before its camera), which is drawn where the chip puts
   it, not magnified with a zoomed view's world; 0 after the objects. */
void fx_draw_held_by_view(int held);
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
   starts or ends. fx_tween_backdrop_angles: while a picture between steps
   is shown (1), its hills' angles as BackdropAnglesFromView made them, each
   player's ($06CF heading, $06CD pitch; one player's in both places). */
void fx_tween_step_end(void);
void fx_tween_reset(void);
int fx_tween_begin(float t);
void fx_tween_end(void);
int fx_tween_backdrop_angles(uint16_t heading[2], uint16_t pitch[2]);
/* fx_tween.c's side of fx_draw: the picture between drawn into its own
   recording, then shown; and none shown. */
void fx_draw_tween_on(void);
void fx_draw_tween_shown(void);
void fx_draw_tween_off(void);
void fx_draw_recompose(void);
/* The horizon of picture n as composed now (its picture between steps when
   one is shown): the line through (x, row), in the screen's pixels, rising
   `slope` rows a column (the view's roll). For the hills and the sky to sit
   on the 3D's horizon. 0 when no view is known. */
int fx_draw_horizon(int picture, float *x, float *row, float *slope);
/* The same for player `panel`'s view as drawn now: while a picture's 3D is
   being drawn for that panel (its frame program, a picture between steps
   included, up to the HUD), that picture's horizon, else the one shown in
   that panel (two players: that half's; one player: the one view). The
   rows are the screen's pixels, the picture's place on the screen
   included (two players: the half's rectangle). 0 when no view is known. */
int fx_draw_horizon_now(int panel, float *x, float *row, float *slope);
int fx_draw_shown_picture(void);
/* The step's end; the picture the last step drew its panel n into (-1:
   none). */
void fx_draw_step_end(void);
int fx_draw_step_picture(int panel);
/* A picture between steps at draw(between): the race's progress through the
   step's wait, its picture part of each panel (RaceViewForPicture), the
   frame program into the picture that panel showed. Called by draw before
   the screen is set. */
void fx_tween_draw(float between);
/* Solids (fx/fx_solid.c): their triangles into the picture in a palette
   colour; whether the picture takes the whole palette (8 bits a pixel);
   whether a solid is drawn in the place of the sprite being drawn. */
#include "fx/fx_face.h"
void fx_draw_flat(const FxCorner *c, uint32_t n, uint8_t colour);
void fx_draw_flat_entry(const FxCorner *c, uint32_t n, uint8_t entry);
int fx_draw_full_colour(void);
/* Whether the picture is two players' Enhanced one: 4 bits a pixel, each
   player's 3D filling its part of the window, its faces' colours computed
   (fx_draw.c, "two players' colours"). */
int fx_draw_two_players(void);
/* The ground's surface (fx/fx_ground.c): triangles over `image` drawn by
   `shader` with its eight inputs, each vertex's u, v its screen place less
   (ox, oy); and the picture being drawn (0, or 1 for two players' second). */
void fx_draw_surface(const FxCorner *c, uint32_t n, nshvc_image image, nshvc_shader shader, const float in[8], float ox, float oy);
/* The land's polygons (fx/fx_ground.c): triangles of n vertices, x and y
   in the picture's pixels, each vertex's colour and depth its own (z in
   (0, 1): past every other shape of the 3D, drawn at depth 0, so they
   cover none of them). */
void fx_draw_land(const nshvc_vertex *v, uint32_t n);
/* An instanced mesh (nshvc 6.2) in the picture being drawn: its view in
   the picture's pixels (cx, cy the view's centre, f its 128 a unit, the
   picture's pixels square), drawn into the list, or kept among the
   picture's shapes and drawn in their order, the view scaled to the list's
   pixels as the shapes' vertices are; the instances copied. Which
   picture's shapes are being made (fx_draw_mesh_slot): 0 and 1 the
   pictures, 2 and 3 those between steps, 4 a list drawn into directly, -1
   none; a mesh drawn in one keeps what was written to it until that
   picture is made again. */
void fx_draw_mesh(nshvc_mesh m, const nshvc_view *view, const nshvc_instance *instances, uint32_t count);
int fx_draw_mesh_slot(void);
/* The 3D's faces queued so far drawn now, in order. */
void fx_draw_queued(void);
/* Whether the 3D is drawn with its depths (docs/stuntracefx/native/DEPTH.md):
   faces seen from behind drawn (fx_face_backs), each vertex at its true
   depth. Off until the engine's depth settings are in. */
int fx_draw_depth(void);
/* The ground under view-space point p (docs/stuntracefx/native/DEPTH.md, 5):
   of the faces this picture has drawn that are not a car's and face up or
   down (up the world's up in view space), the topmost that the line from
   p straight up and down meets no more than 100 over p; its plane n . x = d,
   n facing up. 0 when none. */
int fx_draw_ground(const double p[3], const double up[3], double n[3], double *d);
int fx_draw_picture_now(void);
int fx_draw_tweening(void);
/* The palette entry a face's colour byte is shown in at the screen's depth
   (its kept bits and palette row; 0: not plotted), and where the picture
   being drawn sits on the screen: its view's pixel (0, 0) at screen pixel
   (x, y). */
uint8_t fx_draw_ink(uint8_t c);
void fx_draw_offset(int *x, int *y);
/* What is drawn from now on lies past the water from the eye (1) or not
   (0), while the water cuts the picture (fx_face_cut): fx_solid.c's solids
   say so by their places. */
void fx_draw_side(int past);
/* While on, nothing the frame program does is drawn (the ground's survey of
   the course's shapes). */
void fx_draw_hold(int on);
int fx_solid_hiding(void);
/* The list drawn into, by size in pixels (the checks). */
void fx_draw_set_target(nshvc_draw_list list, uint32_t width, uint32_t height);

#endif
