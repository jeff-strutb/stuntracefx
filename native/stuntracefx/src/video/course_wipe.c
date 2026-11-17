/*
 * course_wipe.c: the course screen's wipe over the whole window.
 *
 * In the original the wipe is window 1, a rectangle of rows with a left and
 * right edge each, the colour maths laid inside it: the fixed colour,
 * rising or falling by level, subtracted from the picture, so a black
 * rectangle grows and fills the 256 x 224. Window and colour maths reach
 * only that picture, so on a larger screen the black stopped at a 4:3 area.
 *
 * Only the wipe out of the screen (state $0A) comes here; the zoom to the
 * records' box, the box itself and the previews' bands keep window 1 and
 * the maths at the original's place (picture.c course_lines_send), as they
 * frame cards and must leave the records' text lit.
 *
 * Here the same rectangle is laid over the window as GPU overlays: one quad
 * for each run of rows with the same edges (the rounded corners are rows a
 * few columns apart), black at the level's share. Inside the box it grew
 * from it stays at the original's place; beyond it, it is stretched so the
 * original's last columns and rows reach the window's edges, so it grows on
 * from the box without a jump and at the full size covers the whole window,
 * at any size and aspect. The overlays are over the
 * sprites too, which the maths left alone; a level's share of black is
 * not quite a level's subtraction from each colour, but they meet at none
 * and at all.
 */
#include <math.h>

#include "video/video.h"

enum { FIRST_ID = 0, IDS = 12 };        /* the engine's overlays 0-11: the pause screen's start at 12 */

static nshvc_image black, white;
static int shown;
static struct { int taken; float left, right, top, bottom; } start;  /* the box the wipe grew from */

/* One edge, an original column or row c, to the screen's: between the
   start box's lo and hi at the original's place (origin + c); from the
   original's least (cmin) to lo, and from hi to its most (cmax), stretched
   over what lies between the box and the screen's edge (0 and size). */
static float stretch(float c, float lo, float hi, float cmin, float cmax, float origin, float size)
{
    if (c <= cmin)
        return 0.0f;
    if (c >= cmax)
        return size;
    if (c < lo)
        return (origin + lo) * (c - cmin) / (lo - cmin);
    if (c > hi)
        return origin + hi + (size - origin - hi) * (c - hi) / (cmax - hi);
    return origin + c;
}

void course_wipe_overlay_off(void)
{
    nshvc_image none = { 0 };
    uint32_t id;

    start.taken = 0;
    if (!shown)
        return;
    for (id = FIRST_ID; id < FIRST_ID + IDS; id++)
        nshvc_overlay_set(id, none, NULL, 0);
    shown = 0;
}

static nshvc_image solid(uint32_t argb)
{
    uint32_t texels[4] = { argb, argb, argb, argb };
    nshvc_image i = nshvc_image_create(2, 2, NSHVC_IMAGE_RGBA);

    if (i.id)
        nshvc_image_write(i, texels);
    return i;
}

typedef struct { int line0, line1; int32_t left, right; } Run;     /* rows line0 to line1 - 1 */

void course_wipe_overlay(const int32_t left[224], const int32_t right[224], const nshvc_blend *blend)
{
    const ScreenShape *sh = screen_shape();
    float window_w, window_h;
    Run runs[224];
    int n = 0, line, k;
    float alpha, sx, sy;
    nshvc_image image;

    screen_window(&window_w, &window_h);
    alpha = (blend->layers && blend->use_fixed) ? (float)(blend->fixed_bgr15 & 31) / 31.0f : 0.0f;
    if (alpha <= 0.0f || window_w <= 0.0f || window_h <= 0.0f) {
        course_wipe_overlay_off();
        return;
    }
    if (!black.id) {
        black = solid(0xFF000000u);
        white = solid(0xFFFFFFFFu);
    }
    image = blend->subtract ? black : white;
    for (line = 0; line < 224; line++) {
        if (left[line] > right[line])
            continue;                   /* no window on this row: nothing laid */
        if (n && runs[n - 1].line1 == line && runs[n - 1].left == left[line] && runs[n - 1].right == right[line]) {
            runs[n - 1].line1 = line + 1;
            continue;
        }
        runs[n].line0 = line;
        runs[n].line1 = line + 1;
        runs[n].left = left[line];
        runs[n].right = right[line];
        n++;
    }
    if (n > IDS) {                      /* more steps than overlays (a rectangle eased between two): their bounds */
        Run box = runs[0];

        for (k = 1; k < n; k++) {
            if (runs[k].left < box.left)
                box.left = runs[k].left;
            if (runs[k].right > box.right)
                box.right = runs[k].right;
            box.line1 = runs[k].line1;
        }
        runs[0] = box;
        n = 1;
    }
    /* the rectangle's bounds when the wipe began (the records' box it grows
       from), kept until the overlays go off */
    if (n && !start.taken) {
        start.left = (float)runs[0].left;
        start.right = (float)runs[0].right + 1.0f;
        for (k = 1; k < n; k++) {
            if ((float)runs[k].left < start.left)
                start.left = (float)runs[k].left;
            if ((float)runs[k].right + 1.0f > start.right)
                start.right = (float)runs[k].right + 1.0f;
        }
        start.top = (float)runs[0].line0;
        start.bottom = (float)runs[n - 1].line1;
        start.taken = 1;
    }
    sx = window_w / (float)sh->width;
    sy = window_h / (float)sh->height;
    for (k = 0; k < IDS; k++) {
        nshvc_image none = { 0 };
        float c[6];

        if (k >= n) {
            nshvc_overlay_set((uint32_t)(FIRST_ID + k), none, NULL, 0);
            continue;
        }
        {
            /* the original's columns and rows to the screen's: inside the box
               it began as, at the original's place; beyond it, stretched so
               the original's last column and row reach the window's edges */
            float x0 = stretch((float)runs[k].left, start.left, start.right, 1.0f, 255.0f,
                               (float)sh->origin_x, (float)sh->width);
            float x1 = stretch((float)runs[k].right + 1.0f, start.left, start.right, 1.0f, 255.0f,
                               (float)sh->origin_x, (float)sh->width);
            float y0 = stretch((float)runs[k].line0, start.top, start.bottom, 0.0f, 224.0f,
                               (float)sh->origin_y, (float)sh->height);
            float y1 = stretch((float)runs[k].line1, start.top, start.bottom, 0.0f, 224.0f,
                               (float)sh->origin_y, (float)sh->height);

            /* then the window's pixels, whole ones, so rows laid edge to edge leave no seam */
            x0 = roundf(x0 * sx);
            x1 = roundf(x1 * sx);
            y0 = roundf(y0 * sy);
            y1 = roundf(y1 * sy);
            c[0] = x0, c[1] = y0, c[2] = x1, c[3] = y0, c[4] = x0, c[5] = y1;
        }
        nshvc_overlay_set((uint32_t)(FIRST_ID + k), image, c, alpha);
    }
    shown = 1;
}

/* ---- the race's bands over the whole window ----

   Jeff, 2026-11-17, on the results screen made full-window: "the black
   'load in' transition on it should be properly covering the full window,
   just like we updated the other black 'transition' animation on other menu
   screens to do so. Rebuild it from scratch if necessary".

   In the original the one-player race's opening and closing (the bands,
   windows.c WindowBandsAnimate: setup 8, every layer kept out inside window
   1 and the colour maths there) are window 1 shaped over the 3D's view, the
   frame round it left lit. In the whole window the view is the whole window,
   so the bands' rows are laid as black GPU overlays with the view's
   rectangle (columns 24 to 232, lines 32 to 160, as the race's sprites take
   it, sprites.c) stretched over the window: closed, they cover all of it,
   at any size and aspect. One quad for each run of rows with the same
   edges; more runs than overlays, their bounds. Over everything, as the
   original's window kept every layer, the sprites too, out of it. */
enum { BANDS_FIRST = 9, BANDS_IDS = 3 };     /* the overlays after the touch controls' (0-3) and the FPS counter's (4-8) */
enum { VIEW_LEFT = 24, VIEW_RIGHT = 232, VIEW_TOP = 32, VIEW_BOTTOM = 160 };

static int bands_shown;

void bands_overlay_off(void)
{
    nshvc_image none = { 0 };
    uint32_t id;

    if (!bands_shown)
        return;
    for (id = BANDS_FIRST; id < BANDS_FIRST + BANDS_IDS; id++)
        nshvc_overlay_set(id, none, NULL, 0);
    bands_shown = 0;
}

static float view_to(float c, float lo, float hi, float size)
{
    float t = (c - lo) / (hi - lo);

    return roundf((t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t) * size);
}

void bands_overlay(const int32_t left[224], const int32_t right[224])
{
    float window_w, window_h;
    Run runs[224];
    int n = 0, line, k;

    screen_window(&window_w, &window_h);
    if (window_w <= 0.0f || window_h <= 0.0f) {
        bands_overlay_off();
        return;
    }
    if (!black.id) {
        black = solid(0xFF000000u);
        white = solid(0xFFFFFFFFu);
    }
    for (line = 0; line < 224; line++) {
        if (left[line] > right[line])
            continue;
        if (n && runs[n - 1].line1 == line && runs[n - 1].left == left[line] && runs[n - 1].right == right[line]) {
            runs[n - 1].line1 = line + 1;
            continue;
        }
        runs[n].line0 = line;
        runs[n].line1 = line + 1;
        runs[n].left = left[line];
        runs[n].right = right[line];
        n++;
    }
    if (n > BANDS_IDS) {
        Run box = runs[0];

        for (k = 1; k < n; k++) {
            if (runs[k].left < box.left)
                box.left = runs[k].left;
            if (runs[k].right > box.right)
                box.right = runs[k].right;
            box.line1 = runs[k].line1;
        }
        runs[0] = box;
        n = 1;
    }
    for (k = 0; k < BANDS_IDS; k++) {
        nshvc_image none = { 0 };
        float c[6], x0, x1, y0, y1;

        if (k >= n) {
            nshvc_overlay_set((uint32_t)(BANDS_FIRST + k), none, NULL, 0);
            continue;
        }
        x0 = view_to((float)runs[k].left, VIEW_LEFT, VIEW_RIGHT, window_w);
        x1 = view_to((float)runs[k].right + 1.0f, VIEW_LEFT, VIEW_RIGHT, window_w);
        y0 = view_to((float)runs[k].line0, VIEW_TOP, VIEW_BOTTOM, window_h);
        y1 = view_to((float)runs[k].line1, VIEW_TOP, VIEW_BOTTOM, window_h);
        c[0] = x0, c[1] = y0, c[2] = x1, c[3] = y0, c[4] = x0, c[5] = y1;
        nshvc_overlay_set((uint32_t)(BANDS_FIRST + k), black, c, 1.0f);
    }
    bands_shown = 1;
}
