/*
 * pause_art.c: the pause screen's pictures (pause_art.h): a dark plate edged
 * in gold, the title between a racing flag's checks, Jeff's controller
 * (pad_art.c) with a tag beside it for each of its buttons and a line from
 * the tag to the button, the hint below, RESUME and DEFAULT KEYS. Each is
 * drawn once into an image; pause.c lays the images over the window and the
 * GPU shows them. What changes (a key's name on its tag, the item chosen,
 * the hint) is its own image over the panel's, redrawn only when it changes
 * or, for the item chosen, one of the items' lit pictures shown in its place.
 *
 * Every shape is drawn by each pixel's distance from it, an edge blended
 * across one pixel, over the shape's own bounds only.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "nshvc/nshvc.h"
#include "pause/pause_art.h"

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
    { NSHVC_L, "L", WHITE, 228.9f, 124.8f, 10, 0, 0 },
    { NSHVC_UP, "UP", WHITE, 227.9f, 173.1f, 7, 0, 1 },
    { NSHVC_LEFT, "LEFT", WHITE, 211.1f, 189.8f, 7, 0, 2 },
    { NSHVC_RIGHT, "RIGHT", WHITE, 244.4f, 189.8f, 7, 0, 3 },
    { NSHVC_DOWN, "DOWN", WHITE, 227.9f, 206.4f, 7, 0, 4 },
    { NSHVC_SELECT, "SELECT", WHITE, 297.6f, 201.0f, 8, 0, 5 },
    { NSHVC_R, "R", WHITE, 411.3f, 124.8f, 10, 1, 0 },
    { NSHVC_X, "X", 0x5A8CF5, 407.7f, 163.6f, 10.5f, 1, 1 },
    { NSHVC_Y, "Y", 0x3CC070, 383.3f, 188.2f, 10.5f, 1, 2 },
    { NSHVC_A, "A", 0xF0504A, 437.2f, 191.1f, 10.5f, 1, 3 },
    { NSHVC_B, "B", 0xF5D34A, 412.6f, 215.9f, 10.5f, 1, 4 },
    { NSHVC_START, "START", WHITE, 332.3f, 201.0f, 8, 1, 5 },
};

static const struct {
    const char *name;
    float x, y, w, h;
} k_button[2] = {
    { "RESUME", 196, 356, 112, 30 },
    { "DEFAULT KEYS", 320, 356, 136, 30 },
};

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
    end = k->button & (NSHVC_A | NSHVC_B | NSHVC_X | NSHVC_Y) ? k->br : 0;
    *fx = k->bx - dx / l * end;
    *fy = k->by - dy / l * end;
    *dot = end == 0;
}

#define RING_GAP 2.5f                   /* the chosen button's ring, out from its reach */

void pause_lit_box(int item, float *x, float *y, float *w, float *h)
{
    float x0, y0, x1, y1;

    if (item >= PAUSE_CONTROLS) {
        x0 = k_button[item - PAUSE_CONTROLS].x;
        y0 = k_button[item - PAUSE_CONTROLS].y;
        x1 = x0 + k_button[item - PAUSE_CONTROLS].w;
        y1 = y0 + k_button[item - PAUSE_CONTROLS].h;
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

static void panel_button(PauseImg *m, int b, int lit)
{
    float x = k_button[b].x, y = k_button[b].y, w = k_button[b].w, h = k_button[b].h;

    /* lit, over the button unlit: whole, so the unlit letters do not show */
    rrect(m, x, y, w, h, 8, lit ? LIT_FILL : PLATE, lit ? 1 : 0.85f);
    rrect_edge(m, x + 0.75f, y + 0.75f, w - 1.5f, h - 1.5f, 7.5f, lit ? 1.75f : 1, GOLD, 1);
    text_centre(m, k_button[b].name, x + w / 2, y + (h - 7 * 1.75f) / 2, 1.75f, lit ? GOLD : WHITE);
}

void pause_draw_panel(PauseImg *m)
{
    int i, b;

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
    for (b = 0; b < 2; b++)
        panel_button(m, b, 0);
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

    if (item >= PAUSE_CONTROLS) {
        panel_button(m, item - PAUSE_CONTROLS, 1);
        return;
    }
    k = &pause_controls[item];
    control_line(item, &ex, &ey, &fx, &fy, &dot);
    line(m, ex, ey, fx, fy, 1.75f, GOLD, 1);
    if (dot)
        disc(m, fx, fy, 2.2f, GOLD, 1);
    ring(m, k->bx, k->by, k->br + RING_GAP, 2, GOLD, 1);
    tag_at(item, &tx, &ty);
    rrect_edge(m, tx + 0.75f, ty + 0.75f, TAG_W - 1.5f, TAG_H - 1.5f, 5.5f, 1.75f, GOLD, 1);
}

void pause_draw_hint(PauseImg *m, const char *line1, uint32_t colour1, const char *line2)
{
    text_centre(m, line1, PAUSE_PW / 2.0f, 312, 2, colour1);
    text_centre(m, line2, PAUSE_PW / 2.0f, 334, 1.25f, 0xA0A0AA);
}
