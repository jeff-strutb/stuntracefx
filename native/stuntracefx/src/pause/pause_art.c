/*
 * pause_art.c: the pause screen's pictures (pause_art.h): a dark plate edged
 * in gold, the title between a racing flag's checks, Jeff's controller
 * (pad_art.c) with a tag beside it for each of its buttons and a line from
 * the tag to the button, the player buttons above it (whose controls are
 * shown), the hint below, RESUME, DEFAULTS and the options. Each is
 * drawn once into an image; pause.c lays the images over the window and the
 * GPU shows them. What changes (a key's name on its tag, the item chosen,
 * the hint) is its own image over the panel's, redrawn only when it changes
 * or, for the item chosen, one of the items' lit pictures shown in its place.
 *
 * Every shape is drawn by each pixel's distance from it, an edge blended
 * across one pixel, over the shape's own bounds only.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nshvc/nshvc.h"
#include "pause/pause_art.h"
#include "system/game.h"

#define GOLD 0xF5D34A
#define PLATE 0x141414
#define WHITE 0xF2F2F2
#define LIT_FILL 0x3A2E08               /* a tag asked for, a panel button chosen */
#define LINE_GREY 0x8A8A94

#define PADK 0.3645f                    /* the controller: points a drawing pixel */
#define PAD_CX 320.0f                   /* where its (500, 300) goes */
#define PAD_CY 190.0f

#define TAG_W 150.0f
#define TAG_H 26.0f
#define MARGIN 2.0f                     /* round an image's box, for its edges' blending */

PauseImg pause_img_new(float x, float y, float w, float h, float density)
{
    PauseImg m;

    m.density = density;
    m.x0 = x;
    m.y0 = y;
    m.w = (int)ceilf(w * density);
    m.h = (int)ceilf(h * density);
    m.px = (uint32_t *)calloc((size_t)m.w * (size_t)m.h, 4);
    return m;
}

/* ---- shapes ---- */

static void blend(PauseImg *m, int x, int y, uint32_t c, float a)
{
    uint32_t *p = &m->px[(size_t)y * (size_t)m->w + (size_t)x], d = *p, out = 0;
    float da = (float)(d >> 24) / 255.0f, oa;
    int k;

    if (a <= 0)
        return;
    if (a > 1)
        a = 1;
    oa = a + da * (1 - a);
    for (k = 0; k < 3; k++) {
        float s = (float)(c >> (16 - 8 * k) & 0xFF), t = (float)(d >> (16 - 8 * k) & 0xFF);
        float v = oa > 0 ? (s * a + t * da * (1 - a)) / oa : 0;
        out |= (uint32_t)(v + 0.5f) << (16 - 8 * k);
    }
    *p = out | (uint32_t)(oa * 255 + 0.5f) << 24;
}

static float cover(float d) { return d < -0.5f ? 1 : d > 0.5f ? 0 : 0.5f - d; }

static float d_circle(float px, float py, float cx, float cy, float r) { return hypotf(px - cx, py - cy) - r; }

static float d_segment(float px, float py, float ax, float ay, float bx, float by)
{
    float vx = bx - ax, vy = by - ay, t = ((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return hypotf(px - ax - t * vx, py - ay - t * vy);
}

static float d_roundrect(float px, float py, float x, float y, float w, float h, float r)
{
    float qx = fabsf(px - (x + w / 2)) - (w / 2 - r), qy = fabsf(py - (y + h / 2)) - (h / 2 - r);
    return hypotf(fmaxf(qx, 0), fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - r;
}

/* A shape inside the box (x0, y0)-(x1, y1), in points: `expr` its distance
   in points from the point (px, py), inside < 0; its colour and alpha. */
#define SHAPE(m, left, top, right, bottom, c, alpha, expr)                                             \
    do {                                                                                               \
        int xa_ = (int)floorf(((left) - (m)->x0) * (m)->density) - 1;                                  \
        int ya_ = (int)floorf(((top) - (m)->y0) * (m)->density) - 1;                                   \
        int xb_ = (int)ceilf(((right) - (m)->x0) * (m)->density) + 1;                                  \
        int yb_ = (int)ceilf(((bottom) - (m)->y0) * (m)->density) + 1, x_, y_;                         \
        if (!(m)->px)                                                                                  \
            break;                                                                                     \
        xa_ = xa_ < 0 ? 0 : xa_;                                                                       \
        ya_ = ya_ < 0 ? 0 : ya_;                                                                       \
        xb_ = xb_ > (m)->w ? (m)->w : xb_;                                                             \
        yb_ = yb_ > (m)->h ? (m)->h : yb_;                                                             \
        for (y_ = ya_; y_ < yb_; y_++)                                                                 \
            for (x_ = xa_; x_ < xb_; x_++) {                                                           \
                float px = (m)->x0 + ((float)x_ + 0.5f) / (m)->density;                                \
                float py = (m)->y0 + ((float)y_ + 0.5f) / (m)->density;                                \
                blend((m), x_, y_, (c), (alpha) * cover((expr) * (m)->density));                       \
            }                                                                                          \
    } while (0)

static void rrect(PauseImg *m, float x, float y, float w, float h, float r, uint32_t c, float a)
{
    SHAPE(m, x, y, x + w, y + h, c, a, d_roundrect(px, py, x, y, w, h, r));
}

/* a rounded rectangle's edge, t points wide, centred on its outline */
static void rrect_edge(PauseImg *m, float x, float y, float w, float h, float r, float t, uint32_t c, float a)
{
    SHAPE(m, x - t, y - t, x + w + t, y + h + t, c, a, fabsf(d_roundrect(px, py, x, y, w, h, r)) - t / 2);
}

static void disc(PauseImg *m, float cx, float cy, float r, uint32_t c, float a)
{
    SHAPE(m, cx - r, cy - r, cx + r, cy + r, c, a, d_circle(px, py, cx, cy, r));
}

static void ring(PauseImg *m, float cx, float cy, float r, float t, uint32_t c, float a)
{
    SHAPE(m, cx - r - t, cy - r - t, cx + r + t, cy + r + t, c, a, fabsf(d_circle(px, py, cx, cy, r)) - t / 2);
}

static void line(PauseImg *m, float ax, float ay, float bx, float by, float t, uint32_t c, float a)
{
    SHAPE(m, fminf(ax, bx) - t, fminf(ay, by) - t, fmaxf(ax, bx) + t, fmaxf(ay, by) + t, c, a,
          d_segment(px, py, ax, ay, bx, by) - t / 2);
}

/* ---- the letters: 5 x 7 capitals, digits and the keys' marks ---- */

static const struct {
    char c;
    uint8_t row[7];
} k_font[] = {
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } }, { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { 'D', { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
    { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'Q', { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D } }, { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } }, { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
    { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } }, { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
    { 'Y', { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } }, { 'Z', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F } },
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } }, { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C } },
    { '/', { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } }, { ';', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08 } },
    { '=', { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 } }, { ',', { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 } },
    { '\'', { 0x0C, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00 } }, { '`', { 0x08, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 } },
    { '[', { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E } }, { ']', { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E } },
    { '\\', { 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01 } }, { '+', { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 } },
    { '*', { 0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00 } }, { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 } },
};

const uint8_t *pause_glyph(char c)
{
    size_t k;

    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    for (k = 0; k < sizeof k_font / sizeof k_font[0]; k++)
        if (k_font[k].c == c)
            return k_font[k].row;
    return NULL;
}

static float text_width(const char *s, float dot) { return ((float)strlen(s) * 6 - 1) * dot; }

/* text from (x, top), each font pixel a square of `dot` points, with a dark
   shadow down and right as the game's own letters have */
static void text_at(PauseImg *m, const char *s, float x0, float top, float dot, uint32_t c)
{
    int pass;

    for (pass = 0; pass < 2; pass++) {
        const char *p;
        float x = x0;

        for (p = s; *p; p++, x += 6 * dot) {
            const uint8_t *g = pause_glyph(*p);
            int r, b;

            if (!g)
                continue;
            for (r = 0; r < 7; r++)
                for (b = 0; b < 5; b++)
                    if (g[r] >> (4 - b) & 1) {
                        float gx = x + (float)b * dot, gy = top + (float)r * dot, o = pass ? 0 : dot * 0.45f;

                        rrect(m, gx + o, gy + o, dot, dot, 0, pass ? c : 0x000000, pass ? 1 : 0.7f);
                    }
        }
    }
}

static void text_centre(PauseImg *m, const char *s, float cx, float top, float dot, uint32_t c)
{
    text_at(m, s, cx - text_width(s, dot) / 2, top, dot, c);
}

/* ---- the layout ---- */

const PauseControl pause_controls[PAUSE_CONTROLS] = {
    /* the spots: the drawing's (pad_art.c) at PADK, its (500, 300) at (PAD_CX, PAD_CY) */
    /* the spots: the drawing's (pad_art.c) at PADK, its (500, 300) at (PAD_CX, PAD_CY) */
    { PAD_L, "L", WHITE, 228.9f, 124.8f, 10, 0, 0 },
    { PAD_UP, "UP", WHITE, 227.9f, 173.1f, 7, 0, 1 },
    { PAD_LEFT, "LEFT", WHITE, 211.1f, 189.8f, 7, 0, 2 },
    { PAD_RIGHT, "RIGHT", WHITE, 244.4f, 189.8f, 7, 0, 3 },
    { PAD_DOWN, "DOWN", WHITE, 227.9f, 206.4f, 7, 0, 4 },
    { PAD_SELECT, "SELECT", WHITE, 297.6f, 201.0f, 8, 0, 5 },
    { PAD_R, "R", WHITE, 411.3f, 124.8f, 10, 1, 0 },
    { PAD_X, "X", 0x5A8CF5, 407.7f, 163.6f, 10.5f, 1, 1 },
    { PAD_Y, "Y", 0x3CC070, 383.3f, 188.2f, 10.5f, 1, 2 },
    { PAD_A, "A", 0xF0504A, 437.2f, 191.1f, 10.5f, 1, 3 },
    { PAD_B, "B", 0xF5D34A, 412.6f, 215.9f, 10.5f, 1, 4 },
    { PAD_START, "START", WHITE, 332.3f, 201.0f, 8, 1, 5 },
};

#define BTN_W 190.0f
#define BTN_H 30.0f
#define BTN_GAP 12.0f
#define BTN_Y0 356.0f                   /* the first row's top */
#define BTN_ROW 40.0f                   /* from one row's top to the next */
#define BTN_COLS 3                      /* the most a row takes */

static const char *const k_name[PAUSE_ITEMS - PAUSE_CONTROLS] = {
    "RESUME", "DEFAULTS", "QUIT RACE", "FULL SCREEN", "FPS COUNTER",
};
/* the rows, left to right (-1: none); a switch for the controls goes after FPS COUNTER */
static const int k_row[2][BTN_COLS] = {
    { PAUSE_RESUME, PAUSE_DEFAULTS, PAUSE_QUIT },
    { PAUSE_FULL_SCREEN, PAUSE_FPS, -1 },
};
static int full_screen_offered = 1;
static int quit_offered;

void pause_art_full_screen(int offered) { full_screen_offered = offered != 0; }
void pause_art_quit(int offered) { quit_offered = offered != 0; }

int pause_item_offered(int item)
{
    return item == PAUSE_FULL_SCREEN ? full_screen_offered : item == PAUSE_QUIT ? quit_offered : 1;
}

int pause_item_row(int item)
{
    int r, c;

    for (r = 0; r < 2; r++)
        for (c = 0; c < BTN_COLS; c++)
            if (k_row[r][c] == item)
                return r;
    return 0;
}

/* a panel button's rectangle: each row's offered buttons side by side, the row
   centred (FPS COUNTER in the middle of its row when it is alone there) */
static void button_at(int b, float *x, float *y, float *w, float *h)
{
    int item = b + PAUSE_CONTROLS, row = pause_item_row(item), n = 0, at = 0, c;

    for (c = 0; c < BTN_COLS; c++) {
        int it = k_row[row][c];

        if (it < 0 || !pause_item_offered(it))
            continue;
        if (it == item)
            at = n;
        n++;
    }
    *w = BTN_W;
    *h = BTN_H;
    *y = BTN_Y0 + (float)row * BTN_ROW;
    *x = (PAUSE_PW - ((float)n * BTN_W + (float)(n > 0 ? n - 1 : 0) * BTN_GAP)) / 2 + (float)at * (BTN_W + BTN_GAP);
}

void pause_button_rect(int item, float *x, float *y, float *w, float *h) { button_at(item - PAUSE_CONTROLS, x, y, w, h); }

/* the tag itself, without the image's margin */
static void tag_at(int i, float *x, float *y)
{
    *x = pause_controls[i].right ? PAUSE_PW - 12 - TAG_W : 12;
    *y = 96 + (float)pause_controls[i].row * 36;
}

void pause_tag_box(int control, float *x, float *y, float *w, float *h)
{
    tag_at(control, x, y);
    *x -= MARGIN;
    *y -= MARGIN;
    *w = TAG_W + 2 * MARGIN;
    *h = TAG_H + 2 * MARGIN;
}

/* a control's line: from the tag's inner edge to the button's edge (a face
   button) or its spot (the rest, with a dot there) */
static void control_line(int i, float *ex, float *ey, float *fx, float *fy, int *dot)
{
    const PauseControl *k = &pause_controls[i];
    float tx, ty, dx, dy, l, end;

    tag_at(i, &tx, &ty);
    *ex = k->right ? tx : tx + TAG_W;
    *ey = ty + TAG_H / 2;
    dx = k->bx - *ex;
    dy = k->by - *ey;
    l = hypotf(dx, dy);
    end = k->button & (PAD_A | PAD_B | PAD_X | PAD_Y) ? k->br : 0;
    *fx = k->bx - dx / l * end;
    *fy = k->by - dy / l * end;
    *dot = end == 0;
}

#define RING_GAP 2.5f                   /* the chosen button's ring, out from its reach */

void pause_lit_box(int item, float *x, float *y, float *w, float *h)
{
    float x0, y0, x1, y1;

    if (item >= PAUSE_CONTROLS) {
        float bw, bh;

        button_at(item - PAUSE_CONTROLS, &x0, &y0, &bw, &bh);
        x1 = x0 + bw;
        y1 = y0 + bh;
    } else {
        const PauseControl *k = &pause_controls[item];
        float tx, ty, reach = k->br + RING_GAP + 1;

        tag_at(item, &tx, &ty);
        x0 = fminf(tx, k->bx - reach);
        y0 = fminf(ty, k->by - reach);
        x1 = fmaxf(tx + TAG_W, k->bx + reach);
        y1 = fmaxf(ty + TAG_H, k->by + reach);
    }
    *x = x0 - MARGIN;
    *y = y0 - MARGIN;
    *w = x1 - x0 + 2 * MARGIN;
    *h = y1 - y0 + 2 * MARGIN;
}

void pause_hint_box(float *x, float *y, float *w, float *h)
{
    *x = 20;
    *y = 306;
    *w = PAUSE_PW - 40;
    *h = 42;
}

/* ---- the pictures ---- */

/* a racing flag's checks, cols x 2 squares of `sq` points from (x, y) */
static void checks(PauseImg *m, float x, float y, int cols, float sq)
{
    int c, r;

    for (r = 0; r < 2; r++)
        for (c = 0; c < cols; c++)
            rrect(m, x + (float)c * sq, y + (float)r * sq, sq, sq, 0, (c + r) & 1 ? 0x101010 : WHITE, 0.9f);
}

/* a button at (x, y, w, h) with text, lit while chosen */
static void button_text(PauseImg *m, float x, float y, float w, float h, const char *text, int lit)
{
    /* lit, over the button unlit: whole, so the unlit letters do not show */
    rrect(m, x, y, w, h, 8, lit ? LIT_FILL : PLATE, lit ? 1 : 0.85f);
    rrect_edge(m, x + 0.75f, y + 0.75f, w - 1.5f, h - 1.5f, 7.5f, lit ? 1.75f : 1, GOLD, 1);
    text_centre(m, text, x + w / 2, y + (h - 7 * 1.75f) / 2, 1.75f, lit ? GOLD : WHITE);
}

static void panel_button_text(PauseImg *m, int b, const char *text, int lit)
{
    float x, y, w, h;

    button_at(b, &x, &y, &w, &h);
    button_text(m, x, y, w, h, text, lit);
}

void pause_options_box(float *x, float *y, float *w, float *h)
{
    /* every place of both rows, offered or not, so the box never changes */
    float total = (float)BTN_COLS * BTN_W + (float)(BTN_COLS - 1) * BTN_GAP;

    *x = (PAUSE_PW - total) / 2 - MARGIN;
    *y = BTN_Y0 - MARGIN;
    *w = total + 2 * MARGIN;
    *h = BTN_ROW + BTN_H + 2 * MARGIN;
}

void pause_draw_options(PauseImg *m, const int on[PAUSE_OPTIONS], int lit)
{
    int i;

    for (i = PAUSE_OPTION; i < PAUSE_ITEMS; i++) {
        char text[32];

        if (!pause_item_offered(i))
            continue;
        if (i >= PAUSE_VALUED)
            snprintf(text, sizeof text, "%s %s", k_name[i - PAUSE_CONTROLS], on[i - PAUSE_OPTION] ? "ON" : "OFF");
        else
            snprintf(text, sizeof text, "%s", k_name[i - PAUSE_CONTROLS]);
        panel_button_text(m, i - PAUSE_CONTROLS, text, lit == i);
    }
}

/* the player buttons, side by side: between the title and the controller,
   clear of the controller's top (its drawing reaches 120.7 points down), of
   the lines from the top tags (109 points down and lower) and of the tags'
   columns (to 162 and from 478 points across) */
#define PLAYER_W 140.0f
#define PLAYER_H 30.0f
#define PLAYER_GAP 12.0f
#define PLAYER_Y 62.0f
#define PLAYER_X(i) ((PAUSE_PW - 2 * PLAYER_W - PLAYER_GAP) / 2 + (float)(i) * (PLAYER_W + PLAYER_GAP))

void pause_player_box(float *x, float *y, float *w, float *h)
{
    *x = PLAYER_X(0) - MARGIN;
    *y = PLAYER_Y - MARGIN;
    *w = 2 * PLAYER_W + PLAYER_GAP + 2 * MARGIN;
    *h = PLAYER_H + 2 * MARGIN;
}

/* each: the one in use filled gold, its letters dark; the one chosen with
   the chosen panel button's look, or, in use, a white edge */
void pause_draw_player(PauseImg *m, int player, int lit)
{
    static const char *const name[2] = { "PLAYER 1", "PLAYER 2" };
    int i;

    for (i = 0; i < 2; i++) {
        float x = PLAYER_X(i), y = PLAYER_Y, w = PLAYER_W, h = PLAYER_H;

        if (i != player) {
            button_text(m, x, y, w, h, name[i], lit == i);
            continue;
        }
        rrect(m, x, y, w, h, 8, GOLD, 1);
        if (lit == i)
            rrect_edge(m, x + 1, y + 1, w - 2, h - 2, 7, 2, WHITE, 1);
        text_centre(m, name[i], x + w / 2, y + (h - 7 * 1.75f) / 2, 1.75f, PLATE);
    }
}

static int in_rect(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

int pause_item_at(float px, float py)
{
    float x, y, w, h;
    int i;

    for (i = 0; i < 2; i++)
        if (in_rect(px, py, PLAYER_X(i), PLAYER_Y, PLAYER_W, PLAYER_H))
            return PAUSE_PLAYER1 + i;
    for (i = PAUSE_CONTROLS; i < PAUSE_ITEMS; i++) {
        if (!pause_item_offered(i))
            continue;
        button_at(i - PAUSE_CONTROLS, &x, &y, &w, &h);
        if (in_rect(px, py, x, y, w, h))
            return i;
    }
    for (i = 0; i < PAUSE_CONTROLS; i++) {
        tag_at(i, &x, &y);
        if (in_rect(px, py, x, y, TAG_W, TAG_H))
            return i;
    }
    /* a button on the controller: the nearest within its ring */
    {
        int best = -1;
        float bd = 0;

        for (i = 0; i < PAUSE_CONTROLS; i++) {
            const PauseControl *k = &pause_controls[i];
            float d = hypotf(px - k->bx, py - k->by);

            if (d <= k->br + RING_GAP && (best < 0 || d < bd)) {
                best = i;
                bd = d;
            }
        }
        return best;
    }
}

void pause_draw_panel(PauseImg *m)
{
    int i;

    /* the plate, edged in gold */
    rrect(m, 2, 2, PAUSE_PW - 4, PAUSE_PH - 4, 18, 0x0E1220, 0.62f);
    rrect_edge(m, 3, 3, PAUSE_PW - 6, PAUSE_PH - 6, 17, 1.5f, GOLD, 1);
    /* the title between a flag's checks */
    text_centre(m, "PAUSED", PAUSE_PW / 2.0f, 22, 3.5f, GOLD);
    checks(m, 136, 26, 10, 9);
    checks(m, PAUSE_PW - 136 - 90, 26, 10, 9);
    /* the controller, and the lines from each tag to its button */
    if (m->px)
        pad_draw(m->px, m->w, m->h, m->density, PADK, PAD_CX - m->x0, PAD_CY - m->y0);
    for (i = 0; i < PAUSE_CONTROLS; i++) {
        float ex, ey, fx, fy;
        int dot;

        control_line(i, &ex, &ey, &fx, &fy, &dot);
        line(m, ex, ey, fx, fy, 1.1f, LINE_GREY, 1);
        if (dot)
            disc(m, fx, fy, 1.6f, LINE_GREY, 1);
    }
}

/* ---- the options menu ---- */

static const char *const k_menu_name[PAUSE_MENU_ITEMS] = { "RESUME", "AUTO-ACCELERATE", "FPS COUNTER", "QUIT RACE" };

/* an item's own rectangle, without the image's margin */
static void menu_at(int item, float *x, float *y, float *w, float *h)
{
    *w = 340;
    *h = 46;
    *x = (PAUSE_PW - *w) / 2;
    *y = 100 + (float)item * 62;
}

void pause_menu_box(int item, float *x, float *y, float *w, float *h)
{
    menu_at(item, x, y, w, h);
    *x -= MARGIN;
    *y -= MARGIN;
    *w += 2 * MARGIN;
    *h += 2 * MARGIN;
}

void pause_draw_menu_panel(PauseImg *m)
{
    rrect(m, 2, 2, PAUSE_PW - 4, PAUSE_PH - 4, 18, 0x0E1220, 0.62f);
    rrect_edge(m, 3, 3, PAUSE_PW - 6, PAUSE_PH - 6, 17, 1.5f, GOLD, 1);
    text_centre(m, "PAUSED", PAUSE_PW / 2.0f, 22, 3.5f, GOLD);
    checks(m, 136, 26, 10, 9);
    checks(m, PAUSE_PW - 136 - 90, 26, 10, 9);
}

void pause_draw_menu_item(PauseImg *m, int item, int on, int lit)
{
    float x, y, w, h;
    char text[32];
    const char *name = k_menu_name[item];

    menu_at(item, &x, &y, &w, &h);
    if (item == PAUSE_MENU_AUTO || item == PAUSE_MENU_FPS) {
        snprintf(text, sizeof text, "%s %s", name, on ? "ON" : "OFF");
        name = text;
    }
    /* as the panel buttons: lit, over the button unlit, whole */
    rrect(m, x, y, w, h, 10, lit ? LIT_FILL : PLATE, lit ? 1 : 0.85f);
    rrect_edge(m, x + 0.75f, y + 0.75f, w - 1.5f, h - 1.5f, 9.25f, lit ? 2 : 1, GOLD, 1);
    text_centre(m, name, x + w / 2, y + (h - 7 * 2.25f) / 2, 2.25f, lit ? GOLD : WHITE);
}

void pause_draw_tag(PauseImg *m, int control, const char *key, int asking)
{
    const PauseControl *k = &pause_controls[control];
    float tx, ty, dot, room = TAG_W - 66;
    const char *shown = asking ? "..." : key;

    tag_at(control, &tx, &ty);
    rrect(m, tx, ty, TAG_W, TAG_H, 6, asking ? LIT_FILL : PLATE, 0.85f);
    rrect_edge(m, tx + 0.75f, ty + 0.75f, TAG_W - 1.5f, TAG_H - 1.5f, 5.5f, 0.9f, k->colour == WHITE ? 0x7A7A84 : k->colour, 1);
    text_at(m, k->name, tx + 9, ty + (TAG_H - 7 * 1.5f) / 2, 1.5f, k->colour);
    if (!*shown)
        return;
    dot = fminf(1.5f, room / (6.0f * (float)strlen(shown) - 1));
    text_at(m, shown, tx + TAG_W - 9 - text_width(shown, dot), ty + (TAG_H - 7 * dot) / 2, dot, asking ? GOLD : WHITE);
}

void pause_draw_lit(PauseImg *m, int item)
{
    const PauseControl *k;
    float tx, ty, ex, ey, fx, fy;
    int dot;

    if (item >= PAUSE_OPTION)
        return;                         /* a panel button: pause.c draws the buttons' own picture with it lit */
    k = &pause_controls[item];
    control_line(item, &ex, &ey, &fx, &fy, &dot);
    line(m, ex, ey, fx, fy, 1.75f, GOLD, 1);
    if (dot)
        disc(m, fx, fy, 2.2f, GOLD, 1);
    ring(m, k->bx, k->by, k->br + RING_GAP, 2, GOLD, 1);
    tag_at(item, &tx, &ty);
    rrect_edge(m, tx + 0.75f, ty + 0.75f, TAG_W - 1.5f, TAG_H - 1.5f, 5.5f, 1.75f, GOLD, 1);
}

void pause_draw_button(PauseImg *m, int play)
{
    const float b = PAUSE_BUTTON;

    rrect(m, 1, 1, b - 2, b - 2, 9, PLATE, 0.72f);
    rrect_edge(m, 1.5f, 1.5f, b - 3, b - 3, 8.5f, 1.5f, GOLD, 1);
    if (play) {
        /* a triangle pointing right, (15, 11) (15, 29) (29, 20): the
           farthest of its three edges' lines, each outward */
        const float n = hypotf(9, 14);

        SHAPE(m, 10, 8, 32, 32, WHITE, 1,
              fmaxf(15 - px, fmaxf(((px - 15) * 9 - (py - 11) * 14) / n, ((px - 29) * 9 + (py - 20) * 14) / n)));
    } else {
        rrect(m, 13, 11, 5, 18, 1.5f, WHITE, 1);
        rrect(m, 22, 11, 5, 18, 1.5f, WHITE, 1);
    }
}

const float pause_view_cube[3][8] = {
    { 20, 9, 29, 14.2f, 20, 19.4f, 11, 14.2f },         /* the top */
    { 11, 14.2f, 20, 19.4f, 20, 30.2f, 11, 25 },        /* the left */
    { 20, 19.4f, 29, 14.2f, 29, 25, 20, 30.2f },        /* the right */
};
const float pause_view_road[8] = { 9, 31, 31, 31, 22.5f, 10, 17.5f, 10 };
const float pause_view_dash[3][5] = { { 20, 12, 20, 15, 0.6f }, { 20, 18, 20, 22, 0.8f }, { 20, 25, 20, 30, 1.0f } };

float pause_poly_distance(float px, float py, const float *xy, int n)
{
    float area = 0, d = -1e30f;
    int i;

    for (i = 0; i < n; i++) {
        int j = (i + 1) % n;

        area += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
    }
    for (i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float ex = xy[2 * j] - xy[2 * i], ey = xy[2 * j + 1] - xy[2 * i + 1], len = hypotf(ex, ey);
        float s;

        if (len <= 0)
            continue;
        s = ((px - xy[2 * i]) * ey - (py - xy[2 * i + 1]) * ex) / len;
        s = area > 0 ? s : -s;
        d = s > d ? s : d;
    }
    return d;
}

void pause_draw_view_button(PauseImg *m, int to_overhead)
{
    const float b = PAUSE_BUTTON;
    int k;

    rrect(m, 1, 1, b - 2, b - 2, 9, PLATE, 0.72f);
    rrect_edge(m, 1.5f, 1.5f, b - 3, b - 3, 8.5f, 1.5f, GOLD, 1);
    if (to_overhead) {
        /* the cube: its faces lit from above, a seam of the plate between */
        static const float shade[3] = { 1, 0.72f, 0.5f };

        for (k = 0; k < 3; k++)
            SHAPE(m, 8, 6, 32, 34, WHITE, shade[k], pause_poly_distance(px, py, pause_view_cube[k], 4) + 0.6f);
    } else {
        SHAPE(m, 6, 8, 34, 34, WHITE, 0.9f, pause_poly_distance(px, py, pause_view_road, 4));
        for (k = 0; k < 3; k++)
            SHAPE(m, 16, 9, 24, 32, PLATE, 1,
                  d_segment(px, py, pause_view_dash[k][0], pause_view_dash[k][1], pause_view_dash[k][2],
                            pause_view_dash[k][3]) - pause_view_dash[k][4]);
    }
}

void pause_draw_hint(PauseImg *m, const char *line1, uint32_t colour1, const char *line2)
{
    text_centre(m, line1, PAUSE_PW / 2.0f, 312, 2, colour1);
    text_centre(m, line2, PAUSE_PW / 2.0f, 334, 1.25f, 0xA0A0AA);
}
