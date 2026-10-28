/*
 * widescreen.h: the race's wide view (WIDESCREEN.md), as the game's C asks
 * for it; game/remaster/view.c answers. While no wider view is in force every
 * answer is the original's: focal length 128, the view's words as the game
 * set them, the sort's 45 degree cull.
 *
 *   PORT_FOCAL=N       a check: the one-player race drawn at focal length N
 *                      on the original's 208 x 128 view (zoomed out below
 *                      128, in above)
 *   PORT_FOCALCHECK=1  a check: every projection's division done both ways
 *                      at focal 128, the original's and the focal one's,
 *                      and counted where they differ (reported at exit)
 */
#ifndef SRFX_WIDESCREEN_H
#define SRFX_WIDESCREEN_H

#include <stdint.h>

/* The projection's focal length, view units to pixels (128 the
   original's). */
int ws_focal(void);

/* The view words GsuParamsFromDp copies to the Super FX: given the first
   six as the game set them (the centre and the edges, $0034-$003E), the
   frustum planes ($0092, $0094) to write instead of the game's; 1 when they
   are the one-player race's and a wider view is in force (planes then
   set), 0 otherwise. */
int ws_view_planes(const uint16_t w[6], uint16_t planes[2]);

/* The sort's cull: 1 when an object the original's 45 degree test drops
   (|X| - radius not below |Z|) is inside the wide view after all. */
int ws_cull_keeps(uint16_t over, uint16_t depth);

/* PORT_FOCALCHECK: whether it is on, and one comparison's outcome. */
int ws_focal_check(void);
void ws_focal_checked(int same, uint16_t a, uint16_t b, uint16_t d);

#endif
