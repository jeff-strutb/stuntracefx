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
#define PAUSE_PH 400

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

/* the panel's own buttons; the option shows its value with its name, so it
   is drawn into its own picture (pause_draw_option), not the panel's */
enum { PAUSE_RESUME = PAUSE_CONTROLS, PAUSE_DEFAULTS, PAUSE_OPTION, PAUSE_ITEMS };

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
/* the auto-accelerate button with its value, lit (chosen) or not; its box is pause_lit_box(PAUSE_OPTION) */
void pause_draw_option(PauseImg *m, int on, int lit);

/* the pause button in the window's corner, PAUSE_BUTTON points square in its
   own points: two bars, or a play triangle while the game is paused */
#define PAUSE_BUTTON 40
void pause_draw_button(PauseImg *m, int play);

/* ---- the letters and the controller ---- */

const uint8_t *pause_glyph(char c);     /* a 5 x 7 capital's rows, bit 4 leftmost; NULL: none */

/* The controller (pad_art.c) blended over px (w x h, 0xAARRGGBB straight
   alpha): its drawing's pixels at k points each, its (500, 300) at the point
   (cx, cy), `density` pixels a point. */
void pad_draw(uint32_t *px, int w, int h, float density, float k, float cx, float cy);

#endif
