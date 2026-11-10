/*
 * between.h: the 2D drawn between updates (plan stage 10's frame rate:
 * pictures between updates on displays faster than 60 Hz).
 *
 * The engine draws once per display refresh and passes `between`, how far
 * real time is past the last update towards the next (0 to 1). A picture is
 * one moment: the update before's, `between` of the way to the newest. What
 * changes in steps (digits, art, maps, colours, layers and sprites switched,
 * the screen itself) shows as the update before left it, whole; what moves
 * (sprites' and layers' places, windows' edges, per-line edges and scrolls)
 * is eased from the update before's place towards the newest. The newest
 * update's steps show from the next update on. So the art and maps the game
 * writes during an update wait one update too (video_tiles_write and the
 * calls beside it), and every picture, headless ones (between 0) too, is a
 * whole frame of the update before, with only its moving parts on the way.
 *
 * The race's 3D and its hills take their own in-between state from
 * fx_tween, on their own clock.
 */
#ifndef STUNTRACEFX_BETWEEN_H
#define STUNTRACEFX_BETWEEN_H

#include <stdint.h>

#include "nshvc/nshvc.h"
#include "video/picture.h"

/* A place between the update before's and the newest: was + (now - was) *
   between, rounded. More than `far` apart it is a thing placed anew, which
   happens at the newest update: was until then. */
int32_t tween_position(int32_t was, int32_t now, float between, int32_t far);

/* The same for a value that wraps every `period` (a scroll), the shorter
   way round. */
int32_t tween_wrapped(int32_t was, int32_t now, float between, int32_t far, int32_t period);

/* A per-line table's moving values: `values` holds the update before's (as
   worked out while that update's world is in, video_between_world_in),
   eased entry by entry towards `now` (worked out from the newest world
   before it went out). `period` 0 for values that don't wrap, TWEEN_SCROLL
   for a scroll (round 1024, or 256 for a value kept in a byte). */
enum { TWEEN_SCROLL = -1 };
void tween_lines(int32_t *values, const int32_t *now, int n, int32_t far, int32_t period);

/* The first version's table, eased against the table the pictures of the
   update before kept; kept for the menus' per-line tables until they move
   to tween_lines (with the world swapped in, it no longer gives the update
   before's values). */
typedef struct {
    uint64_t at;
    int have, n, was_n;
    int32_t was[1024], now[1024];
} TweenTable;
void tween_table(TweenTable *keep, int32_t *values, int n, int32_t far, int32_t period);

/* main's update calls begin before the game's update and end after it:
   begin runs the update before's resource writes, then keeps the world
   that update left (pic, g, windows, palettes, gradient); while the game
   runs, video_tiles_write and the calls beside it wait for the next
   begin. */
void video_update_begin(void);
void video_update_end(void);
int video_deferring(void);
void video_tiles_write(nshvc_tiles set, uint32_t first, const void *data, uint32_t count, nshvc_tile_format format, uint32_t bytes);
void video_map_write(nshvc_map map, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *words);
void video_copy(void *to, const void *from, uint32_t bytes);   /* into a copy the picture is drawn from */
void video_fill(void *to, uint8_t byte, uint32_t bytes);

/* main's draw calls begin first and end last. In between, pic is the
   picture's: the update before's, its moving parts eased (the menu screens
   by the menus' hook, from video_between_newest(); the race's screens
   here), and so are windows, palettes and gradient. video_draw takes that
   update's game state (g) in around its own work (world_in, world_out),
   not the 3D's compose, which runs on its own clock. End puts the game's
   own back. With
   between at 1 (a held picture) or no update before kept, the picture is
   the newest, its waiting writes run first. */
void video_between_begin(float between);
void video_between_end(void);
void video_between_world_in(void);
void video_between_world_out(void);
int video_between_world_is_in(void);
float video_between(void);              /* this picture's between; 1 outside a draw */
int video_between_drawing(void);        /* between begin and end */
int video_between_eased(void);          /* this picture is the update before's, eased */
const Picture *video_between_newest(void);   /* during a draw, the game's newest pic */

/* The menu screens' hook. The first contract (video_between_menu_set): pic
   is the newest, eased in place by the menus' own history. The whole-frame
   contract (video_between_menu_was_set): pic is the update before's, its
   moving parts eased towards video_between_newest(), and the menus' art
   and maps written through video_tiles_write and the calls beside it (the
   menus' map pages, menu_page_write, follow it then). */
void video_between_menu_set(void (*ease)(Picture *shown, float between));
void video_between_menu_was_set(void (*ease)(Picture *shown, float between));
int video_between_menus_whole(void);

#endif
