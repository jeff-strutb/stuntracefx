/*
 * pause_art.h: the pause screen's pictures, each drawn once into an image
 * that pause.c hands the GPU as an overlay. The panel is laid out in its own
 * points, PAUSE_PW x PAUSE_PH; an image holds a box of it at `density`
 * pixels a point.
 */
#ifndef STUNTRACEFX_PAUSE_ART_H
#define STUNTRACEFX_PAUSE_ART_H

#include <stdint.h>

#define PAUSE_PW 640                    /* the panel, in its own points */
#define PAUSE_PH 440

/* An image being drawn: 0xAARRGGBB, straight alpha, as the engine takes
   them. Its pixel (x, y) shows the panel's point (x0 + (x + 0.5) / density,
   y0 + (y + 0.5) / density). */
typedef struct {
    uint32_t *px;
    int w, h;
    float density, x0, y0;
} PauseImg;

/* the box (x, y, w, h), in points, at `density`; px zeroed (NULL: no memory) */
PauseImg pause_img_new(float x, float y, float w, float h, float density);

/* ---- what pause.c lays out (pause_art.c) ---- */

/* the controls on the panel, in their tags' order: the left column top to
   bottom, then the right */
#define PAUSE_CONTROLS 12
typedef struct {
    uint16_t button;                    /* PAD_B... (system/game.h) */
    const char *name;
    uint32_t colour;                    /* its name's on the tag */
    float bx, by, br;                   /* the button's spot on the controller, its reach */
    int right, row;                     /* the tag's column (0 left) and row */
} PauseControl;
extern const PauseControl pause_controls[PAUSE_CONTROLS];

/* the panel's own buttons, in two rows of centred buttons under the controls:
   RESUME, DEFAULTS and QUIT RACE (offered while a race is being driven); FULL
   SCREEN (offered where the window can be) and FPS COUNTER. They show their
   values with their names, so all are drawn into a picture of their own
   (pause_draw_options), not the panel's. A row takes up to three, so the
   controls switch of ORIGINAL / MODERN would be a third in the second row,
   after FPS COUNTER: a new item before PAUSE_ITEMS, named in k_row of
   pause_art.c. (Auto-accelerate, a touch screen's, is the iPhone and iPad
   menu's only, Jeff 2026-11-17.) */
enum { PAUSE_RESUME = PAUSE_CONTROLS, PAUSE_DEFAULTS, PAUSE_QUIT, PAUSE_FULL_SCREEN, PAUSE_FPS, PAUSE_ITEMS };
#define PAUSE_OPTION PAUSE_RESUME                       /* the first of the buttons in the picture of their own */
#define PAUSE_OPTIONS (PAUSE_ITEMS - PAUSE_OPTION)
#define PAUSE_VALUED PAUSE_FULL_SCREEN                  /* from this item on, a button shows ON or OFF */
/* the player buttons above the controller, PLAYER 1 and PLAYER 2 side by
   side: pressing one shows and changes that player's controls, and it is
   shown in use. Items after the rest, drawn into a picture of their own
   with which is in use and which is chosen (pause_draw_player), not the
   panel's, with no lit pictures of their own. */
#define PAUSE_PLAYER1 PAUSE_ITEMS
#define PAUSE_PLAYER2 (PAUSE_ITEMS + 1)
/* whether FULL SCREEN is offered (a window that can be full screen) and
   whether QUIT RACE is (a race is being driven); the rows centre what is */
void pause_art_full_screen(int offered);
void pause_art_quit(int offered);
/* whether a panel button is offered now */
int pause_item_offered(int item);
/* a panel button's rectangle in points, as laid out now */
void pause_button_rect(int item, float *x, float *y, float *w, float *h);
/* the row of a panel button (0 RESUME's, 1 FULL SCREEN's) */
int pause_item_row(int item);

/* the boxes the overlays cover, in points: a control's tag, an item lit
   (a control's tag, line and ring; a panel button), the hint's two lines */
void pause_tag_box(int control, float *x, float *y, float *w, float *h);
void pause_lit_box(int item, float *x, float *y, float *w, float *h);
void pause_hint_box(float *x, float *y, float *w, float *h);

/* the pictures; each fills an image made for its box */
void pause_draw_panel(PauseImg *m);     /* everything that never changes, every item unlit */
void pause_draw_tag(PauseImg *m, int control, const char *key, int asking);
void pause_draw_lit(PauseImg *m, int item);
void pause_draw_hint(PauseImg *m, const char *line1, uint32_t colour1, const char *line2);
/* the panel's buttons with their values (on[k] for PAUSE_OPTION + k), the
   one that is `lit` (an item, or -1) lit; their box, pause_options_box, holds
   every place of both rows */
void pause_options_box(float *x, float *y, float *w, float *h);
void pause_draw_options(PauseImg *m, const int on[PAUSE_OPTIONS], int lit);
/* the player buttons: their box (both), and their picture: player (0 or 1)
   the one in use, lit (0 or 1) the one chosen, -1 neither */
void pause_player_box(float *x, float *y, float *w, float *h);
void pause_draw_player(PauseImg *m, int player, int lit);
/* the item at the panel's point (x, y), for the pointer: a control (its
   tag, or its button on the controller), a panel button shown, or a
   player button; -1 none */
int pause_item_at(float x, float y);

/* ---- the options menu (iOS, whose controls cannot be changed) ---- */

/* the panel of the options menu: the title and the items one under the
   other, no controller; its items by their own pictures, over it */
enum { PAUSE_MENU_RESUME, PAUSE_MENU_AUTO, PAUSE_MENU_FPS, PAUSE_MENU_QUIT, PAUSE_MENU_ITEMS };
void pause_menu_box(int item, float *x, float *y, float *w, float *h);
void pause_draw_menu_panel(PauseImg *m);
/* an item, lit while chosen; `on` is an option item's value */
void pause_draw_menu_item(PauseImg *m, int item, int on, int lit);

/* the pause button in the window's corner, PAUSE_BUTTON points square in its
   own points: two bars, or a play triangle while the game is paused */
#define PAUSE_BUTTON 40
void pause_draw_button(PauseImg *m, int play);
/* the view button beside it (race/overhead.c), the same plate and size:
   what pressing it switches to, a cube seen from above (to_overhead) or a
   road going away from behind the car */
void pause_draw_view_button(PauseImg *m, int to_overhead);
/* those two glyphs' shapes in PAUSE_BUTTON's points (x right, y down), for
   the touch screen's button too (input/controls.c): the cube's three faces
   (top, left, right, 4 corners each) and the road (4 corners) with its
   three centre dashes (a segment and a half width each) */
extern const float pause_view_cube[3][8];
extern const float pause_view_road[8];
extern const float pause_view_dash[3][5];
/* the distance from (px, py) to the convex polygon xy of n corners, inside < 0 */
float pause_poly_distance(float px, float py, const float *xy, int n);

/* ---- the letters and the controller ---- */

const uint8_t *pause_glyph(char c);     /* a 5 x 7 capital's rows, bit 4 leftmost; NULL: none */

/* The controller (pad_art.c) blended over px (w x h, 0xAARRGGBB straight
   alpha): its drawing's pixels at k points each, its (500, 300) at the point
   (cx, cy), `density` pixels a point. */
void pad_draw(uint32_t *px, int w, int h, float density, float k, float cx, float cy);

#endif
