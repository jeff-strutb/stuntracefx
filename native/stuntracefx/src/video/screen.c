/*
 * screen.c: the screen's size from the window's shape, and where the HUD
 * floats on it.
 *
 * Wider than 4:3, the screen is 224 lines and the nearest even width;
 * narrower, 256 across and the nearest even height; up to 1024 each way.
 * The engine shows it at the console's pixel shape (aspect 0), so a window
 * that no even size fits exactly shows a sliver of border, never a
 * stretched picture. The original's 256 x 224 sits in the middle (the
 * origin), so the original's lines and columns stay about the screen's
 * centre.
 *
 * The HUD's box is the screen's middle, at most 340 wide (16:9 at 224
 * lines), so the HUD does not run to the far edges of a very wide window.
 * It is also kept inside the window's safe area (a notch, rounded corners,
 * a home bar).
 */
#include <math.h>

#include "nshvc/nshvc.h"
#include "video/video.h"

enum {
    SCREEN_MAX = 1024,
    HUD_BOX_MAX = 340,
    MIDDLE_LEFT = 128 - 8,              /* a piece wholly left of this is the left's */
    MIDDLE_RIGHT = 128 + 8,             /* wholly right of this, the right's */
    VIEW_LEFT = 24,                     /* the original's view on its screen */
    VIEW_RIGHT = 232,
    VIEW_CENTRE_Y = 96,
};

static ScreenShape shape = { 256, 224, 0, 0, { 0, 0, 0, 0 } };
static float check_window[6];          /* --window: width, height, then safe left, top, right, bottom */
static int checking;

void screen_window_set(const float *values, int count)
{
    int i;

    for (i = 0; i < 6; i++)
        check_window[i] = i < count ? values[i] : 0.0f;
    checking = values[0] > 0.0f && values[1] > 0.0f;
}

void screen_update(void)
{
    nshvc_display d = nshvc_display_info();
    float ww = (float)d.width, wh = (float)d.height, safe[4], a, per;
    int w = 256, h = 224, i;

    safe[0] = d.safe_left;
    safe[1] = d.safe_top;
    safe[2] = d.safe_right;
    safe[3] = d.safe_bottom;
    if (checking) {
        ww = check_window[0];
        wh = check_window[1];
        for (i = 0; i < 4; i++)
            safe[i] = check_window[2 + i];
    }
    if (ww > 0.0f && wh > 0.0f) {
        a = (ww / wh) / (4.0f / 3.0f);
        if (a >= 1.0f) {
            w = (int)(256.0f * a / 2.0f + 0.5f) * 2;
            if (w > SCREEN_MAX)
                w = SCREEN_MAX;
        } else {
            h = (int)(224.0f / a / 2.0f + 0.5f) * 2;
            if (h > SCREEN_MAX)
                h = SCREEN_MAX;
        }
    }
    shape.width = w;
    shape.height = h;
    shape.origin_x = (w - 256) / 2;
    shape.origin_y = (h - 224) / 2;
    /* window pixels to screen pixels: the screen is shown whole, so the
       smaller of the two scales */
    per = ww > 0.0f && wh > 0.0f ? fminf((float)w / ww, (float)h / wh) : 0.0f;
    for (i = 0; i < 4; i++)
        shape.safe[i] = safe[i] > 0.0f ? (int)ceilf(safe[i] * per) : 0;
}

const ScreenShape *screen_shape(void)
{
    return &shape;
}

/* the HUD box's inset from the screen's left and right edges */
static int box_left(void)
{
    int margin = shape.width > HUD_BOX_MAX ? (shape.width - HUD_BOX_MAX) / 2 : 0;

    return margin > shape.safe[0] ? margin : shape.safe[0];
}

static int box_right(void)
{
    int margin = shape.width > HUD_BOX_MAX ? (shape.width - HUD_BOX_MAX) / 2 : 0;

    return margin > shape.safe[2] ? margin : shape.safe[2];
}

/* A piece of the original's screen spanning columns x0 to x1: left of the
   middle it goes to the HUD box's left edge, right of it to the right edge,
   across it it stays centred. The columns to add to the original's. */
int screen_anchor_x(int x0, int x1)
{
    if (x1 < MIDDLE_LEFT)
        return box_left();
    if (x0 > MIDDLE_RIGHT)
        return shape.width - 256 - box_right();
    return shape.origin_x;
}

/* The same for a piece the original drew inside its view, against the
   view's edges rather than the screen's: the view's left edge goes to the
   box's left edge, its right edge to the box's right edge. */
int screen_view_anchor_x(int x0, int x1)
{
    if (x1 < MIDDLE_LEFT)
        return box_left() - VIEW_LEFT;
    if (x0 > MIDDLE_RIGHT)
        return shape.width - box_right() - VIEW_RIGHT;
    return shape.origin_x;
}

/* Down the screen by a piece's middle: above the view's centre it stays at
   the top, below it it goes to the bottom, about it it moves with the
   centre (the race's messages). The lines to add to the original's. */
int screen_anchor_y(int y0, int y1)
{
    int c = (y0 + y1) / 2;

    if (c < VIEW_CENTRE_Y - 16)
        return shape.safe[1];
    if (c > VIEW_CENTRE_Y + 16)
        return screen_bottom_shift();
    return shape.origin_y;
}

/* the lines the bottom band (the race's panels) moves down by */
int screen_bottom_shift(void)
{
    return shape.height - 224 - shape.safe[3];
}
