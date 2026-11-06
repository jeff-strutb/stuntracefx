/*
 * pause.c: the pause screen on the hosts with a keyboard (front.h; not on
 * iOS, whose touch scheme has its own pause and no keys to lay out).
 *
 * A pause button sits in the window's top left corner in menus and races
 * alike. Clicked, or Escape pressed, the console stops the game where it is
 * (nshvc_ext_pause: the machine still, its sound silent, the last frame
 * blurred and darkened behind the game's pictures) and a panel shows a
 * Super Famicom controller with the key that makes each of its buttons on a
 * tag beside it, drawn as the front end's other pictures are (art.c: dark
 * plates edged in gold, the badge's 5 x 7 capitals, a racing flag's checks
 * by the title). Clicking a button or its tag asks for its new key; the
 * next key pressed takes it (Escape cancels), and a key another button had
 * goes over to the one whose key was taken, so every button keeps one.
 * The keys are kept in the console's save folder (keys.cfg) and laid out
 * again at each start. RESUME, the pause button again or Escape goes on;
 * DEFAULT KEYS puts the console's keys back. While paused the mode's badge
 * (ORIGINAL or ENHANCED) stays in view beside the play button, and a click
 * on it changes the mode as Tab does (Tab too).
 *
 * The panel is one picture redrawn when what it shows changes (a key, the
 * button under the mouse, the one asked for); its shapes are drawn by each
 * pixel's distance from them, an edge blended across one pixel, over
 * their own bounds only.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "front.h"
#include "baked.h"
#ifdef SRFX_NSHVC_HANDOFF
#include "nshvc_handoff.h"
#endif

#define DENSITY 2                   /* pixels a point */
#define PW 640                      /* the panel, in its own points */
#define PH 400
#define BTN 40                      /* the pause button's size, points */
#define MARGIN 10                   /* its distance from the window's corner */

#define GOLD 0xF5D34A
#define PLATE 0x141414
#define WHITE 0xF2F2F2

/* ---- pictures ---- */
typedef struct {
    uint32_t *px;
    int w, h;
} Img;

#ifdef SRFX_NSHVC_HANDOFF
/* The panel on the GPU (SC_GPU, the console's overlay from a texture): draw_panel's
   shapes recorded in their order, not rasterized here; the GPU's kernel shades each
   as SHAPE and blend do below. A shape's pixels as SHAPE finds them. */
static HandShape *rec;
static size_t rec_n, rec_cap;
static int rec_on;

static void rec_shape(int kind, float x0, float y0, float x1, float y1, uint32_t c, float a, float p0, float p1, float p2,
                      float p3, float p4, float p5)
{
    int xa = (int)floorf(x0 * DENSITY) - 1, ya = (int)floorf(y0 * DENSITY) - 1;
    int xb = (int)ceilf(x1 * DENSITY) + 1, yb = (int)ceilf(y1 * DENSITY) + 1;
    HandShape *s;
    if (rec_n == rec_cap) {
        size_t cap = rec_cap ? 2 * rec_cap : 2048;
        HandShape *g = (HandShape *)realloc(rec, cap * sizeof *rec);
        if (!g)
            return;
        rec = g;
        rec_cap = cap;
    }
    xa = xa < 0 ? 0 : xa;
    ya = ya < 0 ? 0 : ya;
    xb = xb > PW * DENSITY ? PW * DENSITY : xb;
    yb = yb > PH * DENSITY ? PH * DENSITY : yb;
    if (xa >= xb || ya >= yb)
        return;
    s = &rec[rec_n++];
    s->p[0] = p0, s->p[1] = p1, s->p[2] = p2, s->p[3] = p3, s->p[4] = p4, s->p[5] = p5;
    s->colour = c;
    s->alpha = a;
    s->kind = kind;
    s->x0 = xa;
    s->x1 = xb;
    s->rows = (uint32_t)ya | (uint32_t)yb << 16;
}
#else
#define rec_on 0
#define rec_shape(...) ((void)0)
#endif

static void blend(Img *m, int x, int y, uint32_t c, float a)
{
    uint32_t *p = &m->px[y * m->w + x], d = *p;
    float da = (d >> 24) / 255.0f, oa = a + da * (1 - a);
    int k;
    uint32_t out = 0;
    if (a <= 0)
        return;
    if (a > 1)
        a = 1;
    for (k = 0; k < 3; k++) {
        float s = (c >> (16 - 8 * k) & 0xFF), t = (d >> (16 - 8 * k) & 0xFF);
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

/* a shape inside the box (x0, y0)-(x1, y1), points: dist(px, py) and its
   colour and alpha */
#define SHAPE(m, x0, y0, x1, y1, c, alpha, expr)                                                       \
    do {                                                                                               \
        int xa_ = (int)floorf((x0) * DENSITY) - 1, ya_ = (int)floorf((y0) * DENSITY) - 1;              \
        int xb_ = (int)ceilf((x1) * DENSITY) + 1, yb_ = (int)ceilf((y1) * DENSITY) + 1, x_, y_;        \
        if (xa_ < 0) xa_ = 0;                                                                          \
        if (ya_ < 0) ya_ = 0;                                                                          \
        if (xb_ > (m)->w) xb_ = (m)->w;                                                                \
        if (yb_ > (m)->h) yb_ = (m)->h;                                                                \
        for (y_ = ya_; y_ < yb_; y_++)                                                                 \
            for (x_ = xa_; x_ < xb_; x_++) {                                                           \
                float px = (x_ + 0.5f) / DENSITY, py = (y_ + 0.5f) / DENSITY;                          \
                blend((m), x_, y_, (c), (alpha) * cover((expr) * DENSITY));                            \
            }                                                                                          \
    } while (0)

static void rrect(Img *m, float x, float y, float w, float h, float r, uint32_t c, float a)
{
    if (rec_on) {
        rec_shape(HAND_RRECT, x, y, x + w, y + h, c, a, x, y, w, h, r, 0);
        return;
    }
    SHAPE(m, x, y, x + w, y + h, c, a, d_roundrect(px, py, x, y, w, h, r));
}

static void rrect_edge(Img *m, float x, float y, float w, float h, float r, float t, uint32_t c, float a)
{
    if (rec_on) {
        rec_shape(HAND_RRECT_EDGE, x - t, y - t, x + w + t, y + h + t, c, a, x, y, w, h, r, t);
        return;
    }
    SHAPE(m, x - t, y - t, x + w + t, y + h + t, c, a, fabsf(d_roundrect(px, py, x, y, w, h, r)) - t / 2);
}

static void disc(Img *m, float cx, float cy, float r, uint32_t c, float a)
{
    if (rec_on) {
        rec_shape(HAND_DISC, cx - r, cy - r, cx + r, cy + r, c, a, cx, cy, r, 0, 0, 0);
        return;
    }
    SHAPE(m, cx - r, cy - r, cx + r, cy + r, c, a, d_circle(px, py, cx, cy, r));
}

static void line(Img *m, float ax, float ay, float bx, float by, float t, uint32_t c, float a)
{
    if (rec_on) {
        rec_shape(HAND_LINE, fminf(ax, bx) - t, fminf(ay, by) - t, fmaxf(ax, bx) + t, fmaxf(ay, by) + t, c, a, ax, ay, bx, by, t, 0);
        return;
    }
    SHAPE(m, fminf(ax, bx) - t, fminf(ay, by) - t, fmaxf(ax, bx) + t, fmaxf(ay, by) + t, c, a,
          d_segment(px, py, ax, ay, bx, by) - t / 2);
}

/* ---- the letters: 5 x 7 capitals, digits and the keys' marks ---- */
static const struct { char c; uint8_t row[7]; } k_font[] = {
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

static const uint8_t *glyph(char c)
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
static void text_at(Img *m, const char *s, float x0, float top, float dot, uint32_t c)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        const char *p;
        float x = x0;
        for (p = s; *p; p++, x += 6 * dot) {
            const uint8_t *g = glyph(*p);
            int r, b;
            if (!g)
                continue;
            for (r = 0; r < 7; r++)
                for (b = 0; b < 5; b++)
                    if (g[r] >> (4 - b) & 1) {
                        float gx = x + b * dot, gy = top + r * dot, o = pass ? 0 : dot * 0.45f;
                        rrect(m, gx + o, gy + o, dot, dot, 0, pass ? c : 0x000000, pass ? 1 : 0.7f);
                    }
        }
    }
}

static void text_centre(Img *m, const char *s, float cx, float top, float dot, uint32_t c)
{
    text_at(m, s, cx - text_width(s, dot) / 2, top, dot, c);
}

/* ---- the controller's buttons, their tags and the panel's buttons ---- */
typedef struct {
    unsigned control;
    const char *name;
    uint32_t colour;                /* its name's on the tag */
    float bx, by, br;               /* the button's spot on the controller, its reach */
    int right, row;                 /* the tag's column (0 left) and row */
} Ctl;

static const Ctl k_ctl[] = {
    /* the spots: the drawing's (pad_art.c) through PADX, PADY */
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
#define CTLS ((int)(sizeof k_ctl / sizeof k_ctl[0]))

#define TAG_W 150
#define TAG_H 26
static void tag_box(int i, float *x, float *y)
{
    *x = k_ctl[i].right ? PW - 12 - TAG_W : 12;
    *y = 96 + k_ctl[i].row * 36.0f;
}

/* the panel's own buttons: RESUME and DEFAULT KEYS */
enum { HIT_NONE = -1, HIT_RESUME = 100, HIT_DEFAULTS = 101 };
static const struct { int id; const char *name; float x, y, w, h; } k_btn[] = {
    { HIT_RESUME, "RESUME", 196, 356, 112, 30 },
    { HIT_DEFAULTS, "DEFAULT KEYS", 320, 356, 136, 30 },
};

/* what is at (x, y), the panel's points: a control's index, a panel
   button's id, or HIT_NONE */
int pause_hit(float x, float y)
{
    int i;
    size_t k;
    for (i = 0; i < CTLS; i++) {
        float tx, ty;
        tag_box(i, &tx, &ty);
        if ((x >= tx && x < tx + TAG_W && y >= ty && y < ty + TAG_H) ||
            hypotf(x - k_ctl[i].bx, y - k_ctl[i].by) <= k_ctl[i].br)
            return i;
    }
    for (k = 0; k < sizeof k_btn / sizeof k_btn[0]; k++)
        if (x >= k_btn[k].x && x < k_btn[k].x + k_btn[k].w && y >= k_btn[k].y && y < k_btn[k].y + k_btn[k].h)
            return k_btn[k].id;
    return HIT_NONE;
}

/* ---- the keys ---- */
/* keys the front end keeps for itself (main.c on_key): Tab the mode, F1
   and F2 the options, Escape the pause */
int pause_key_reserved(int vk) { return vk == 0x09 || vk == 0x70 || vk == 0x71 || vk == 0x1B; }

/* the console's defaults (nativeshvc's input.c) */
static const struct { unsigned control; int vk; } k_default[] = {
    { NSHVC_UP, 0x26 }, { NSHVC_DOWN, 0x28 }, { NSHVC_LEFT, 0x25 }, { NSHVC_RIGHT, 0x27 }, { NSHVC_B, 'Z' },
    { NSHVC_A, 'X' }, { NSHVC_Y, 'A' }, { NSHVC_X, 'S' }, { NSHVC_L, 'Q' }, { NSHVC_R, 'W' },
    { NSHVC_START, 0x0D }, { NSHVC_SELECT, 0x10 },
};

static int ctl_index(unsigned control)
{
    int i;
    for (i = 0; i < CTLS; i++)
        if (k_ctl[i].control == control)
            return i;
    return -1;
}

/* control i's key becomes vk; the button that had vk takes i's old key */
void pause_rebind(int i, int vk)
{
    int old = nshvc_ext_key_for(k_ctl[i].control), j, other = -1;
    if (old == vk)
        return;
    for (j = 0; j < CTLS; j++)
        if (j != i && nshvc_ext_key_for(k_ctl[j].control) == vk)
            other = j;
    nshvc_ext_key_bind(k_ctl[i].control, vk);
    if (other >= 0)
        nshvc_ext_key_bind(k_ctl[other].control, old);
}

static void cfg_path(char *out, size_t n) { snprintf(out, n, "%s/keys.cfg", nshvc_save_dir()); }

void pause_keys_save(void)
{
    char path[1024];
    FILE *f;
    int i;
    cfg_path(path, sizeof path);
    if (!(f = fopen(path, "w")))
        return;
    for (i = 0; i < CTLS; i++)
        fprintf(f, "%s %d\n", k_ctl[i].name, nshvc_ext_key_for(k_ctl[i].control));
    fclose(f);
}

/* the saved keys laid out, each through pause_rebind so a file that names
   one key twice still leaves every button a key */
void pause_keys_load(void)
{
    char path[1024], line_s[128], name[32];
    FILE *f;
    int vk, i;
    cfg_path(path, sizeof path);
    if (!(f = fopen(path, "r")))
        return;
    while (fgets(line_s, sizeof line_s, f))
        if (sscanf(line_s, "%31s %d", name, &vk) == 2 && vk > 0 && vk < 0xFF && !pause_key_reserved(vk))
            for (i = 0; i < CTLS; i++)
                if (!strcmp(name, k_ctl[i].name))
                    pause_rebind(i, vk);
    fclose(f);
}

static void keys_default(void)
{
    size_t k;
    for (k = 0; k < sizeof k_default / sizeof k_default[0]; k++)
        pause_rebind(ctl_index(k_default[k].control), k_default[k].vk);
}

/* ---- the state ---- */
static int paused, asking = -1, hover = HIT_NONE, button_hover, badge_hover, loaded;
static int id_pause = -1, id_play = -1, id_panel = -1, panel_dirty = 1;
static Img panel;
static float mouse_x, mouse_y;
static int mouse_buttons;

int pause_active(void) { return paused; }
int pause_asking(void) { return asking; }

static void set_paused(int on)
{
    paused = on;
    asking = -1;
    panel_dirty = 1;
    /* the frame behind: blurred 10 points, a dark blue laid over at 55% */
    nshvc_ext_pause(on, 10, 0x8C0A0E1Au);
}

/* ---- the pictures ---- */
static Img img_new(float wpt, float hpt)
{
    Img m;
    m.w = (int)(wpt * DENSITY);
    m.h = (int)(hpt * DENSITY);
    m.px = (uint32_t *)calloc((size_t)m.w * m.h, 4);
    return m;
}

#if PAUSE_CHECK
/* the pause and play buttons as drawn: the bake's and the checks' (a host
   unpacks BAKED_BTN_PAUSE and BAKED_BTN_PLAY) */
static Img button_img(int play)
{
    Img m = img_new(BTN, BTN);
    rrect(&m, 1, 1, BTN - 2, BTN - 2, 9, PLATE, 0.72f);
    rrect_edge(&m, 1.5f, 1.5f, BTN - 3, BTN - 3, 8.5f, 1.5f, GOLD, 1);
    if (play) {
        /* a triangle pointing right, (15, 11) (15, 29) (29, 20): the
           farthest of its three edges' lines, each outward */
        const float n = hypotf(9, 14);
        SHAPE(&m, 10, 8, 32, 32, WHITE, 1,
              fmaxf(15 - px, fmaxf(((px - 15) * 9 - (py - 11) * 14) / n, ((px - 29) * 9 + (py - 20) * 14) / n)));
    } else {
        rrect(&m, 13, 11, 5, 18, 1.5f, WHITE, 1);
        rrect(&m, 22, 11, 5, 18, 1.5f, WHITE, 1);
    }
    return m;
}
#endif

/* a racing flag's checks, cols x 2 squares of `sq` points from (x, y) */
static void checks(Img *m, float x, float y, int cols, float sq)
{
    int c, r;
    for (r = 0; r < 2; r++)
        for (c = 0; c < cols; c++)
            rrect(m, x + c * sq, y + r * sq, sq, sq, 0, (c + r) & 1 ? 0x101010 : WHITE, 0.9f);
}

static void key_label(char *out, size_t n, int vk)
{
    const char *s = vk ? nshvc_ext_key_name(vk) : "";
    size_t i;
    if (!vk)
        s = "NONE";
    else if (!*s)
        s = "?";
    for (i = 0; i + 1 < n && s[i]; i++)
        out[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 'a' + 'A' : s[i]);
    out[i] = 0;
}

/* The controller (pad_art.c): the vector drawing's pixels at PADK points
   each, its (500, 300) at the panel's (320, 190), rasterized ahead
   (baked.h BAKED_PAD, from pad_draw) and unpacked once */
#define PADK 0.3645f
static uint32_t *pad_px;

const uint8_t *pause_glyph(char c) { return glyph(c); }

static void draw_controller(Img *m)
{
    int i, n = m->w * m->h;
    if (!pad_px) {
        int w = 0, h = 0;
        pad_px = baked_unpack(BAKED_PAD, &w, &h);
        if (pad_px && (w != m->w || h != m->h)) {
            free(pad_px);               /* (not the panel's size the table was made at: none) */
            pad_px = NULL;
        }
    }
    if (rec_on) {
        if (pad_px)
            rec_shape(HAND_PAD, 0, 0, PW, PH, 0, 1, 0, 0, 0, 0, 0, 0);
    } else if (pad_px)
        for (i = 0; i < n; i++)
            if (pad_px[i] >> 24) {
                uint32_t c = pad_px[i];
                blend(m, i % m->w, i / m->w, c & 0xFFFFFF, (float)(c >> 24) / 255.0f);
            }
    /* the lines from each tag to its button, over it to the button's edge
       (a dot at the end), and a ring on the one under the mouse or asked
       for */
    for (i = 0; i < CTLS; i++) {
        float tx, ty, ex, ey, dx, dy, l, end;
        int lit = hover == i || asking == i;
        uint32_t c = lit ? GOLD : 0x8A8A94;
        tag_box(i, &tx, &ty);
        ex = k_ctl[i].right ? tx : tx + TAG_W;
        ey = ty + TAG_H / 2.0f;
        dx = k_ctl[i].bx - ex;
        dy = k_ctl[i].by - ey;
        l = hypotf(dx, dy);
        end = k_ctl[i].control & (NSHVC_A | NSHVC_B | NSHVC_X | NSHVC_Y) ? k_ctl[i].br : 0;
        line(m, ex, ey, k_ctl[i].bx - dx / l * end, k_ctl[i].by - dy / l * end, lit ? 1.75f : 1.1f, c, 1);
        if (!end)
            disc(m, k_ctl[i].bx, k_ctl[i].by, lit ? 2.2f : 1.6f, c, 1);
        if (lit && rec_on)
            rec_shape(HAND_RING, k_ctl[i].bx - 20, k_ctl[i].by - 20, k_ctl[i].bx + 20, k_ctl[i].by + 20, GOLD, 1, k_ctl[i].bx,
                      k_ctl[i].by, k_ctl[i].br + 2.5f, 1, 0, 0);
        else if (lit)
            SHAPE(m, k_ctl[i].bx - 20, k_ctl[i].by - 20, k_ctl[i].bx + 20, k_ctl[i].by + 20, GOLD, 1,
                  fabsf(d_circle(px, py, k_ctl[i].bx, k_ctl[i].by, k_ctl[i].br + 2.5f)) - 1);
    }
}

static void draw_tags(Img *m)
{
    int i;
    for (i = 0; i < CTLS; i++) {
        float tx, ty, dot, room = TAG_W - 66;
        char key[32];
        int lit = hover == i || asking == i;
        tag_box(i, &tx, &ty);
        rrect(m, tx, ty, TAG_W, TAG_H, 6, asking == i ? 0x3A2E08 : PLATE, 0.85f);
        rrect_edge(m, tx + 0.75f, ty + 0.75f, TAG_W - 1.5f, TAG_H - 1.5f, 5.5f, lit ? 1.75f : 0.9f,
                   lit ? GOLD : k_ctl[i].colour == WHITE ? 0x7A7A84 : k_ctl[i].colour, 1);
        text_at(m, k_ctl[i].name, tx + 9, ty + (TAG_H - 7 * 1.5f) / 2, 1.5f, k_ctl[i].colour);
        if (asking == i)
            snprintf(key, sizeof key, "...");
        else
            key_label(key, sizeof key, nshvc_ext_key_for(k_ctl[i].control));
        dot = fminf(1.5f, room / (6.0f * (float)strlen(key) - 1));
        text_at(m, key, tx + TAG_W - 9 - text_width(key, dot), ty + (TAG_H - 7 * dot) / 2, dot,
                asking == i ? GOLD : WHITE);
    }
}

static void draw_panel(Img *m)
{
    size_t k;
    if (!rec_on)
        memset(m->px, 0, (size_t)m->w * m->h * 4);
    /* the plate, edged in gold */
    rrect(m, 2, 2, PW - 4, PH - 4, 18, 0x0E1220, 0.62f);
    rrect_edge(m, 3, 3, PW - 6, PH - 6, 17, 1.5f, GOLD, 1);
    /* the title between a flag's checks */
    text_centre(m, "PAUSED", PW / 2.0f, 22, 3.5f, GOLD);
    checks(m, 136, 26, 10, 9);
    checks(m, PW - 136 - 90, 26, 10, 9);
    draw_controller(m);
    draw_tags(m);
    /* what to do */
    if (asking >= 0) {
        char s[64];
        snprintf(s, sizeof s, "PRESS A KEY FOR %s", k_ctl[asking].name);
        text_centre(m, s, PW / 2.0f, 312, 2, GOLD);
        text_centre(m, "ESCAPE TO CANCEL", PW / 2.0f, 334, 1.25f, 0xA0A0AA);
    } else {
        text_centre(m, "CLICK A BUTTON TO CHANGE ITS KEY", PW / 2.0f, 312, 2, WHITE);
        text_centre(m, "ESCAPE TO RESUME", PW / 2.0f, 334, 1.25f, 0xA0A0AA);
    }
    for (k = 0; k < sizeof k_btn / sizeof k_btn[0]; k++) {
        int lit = hover == k_btn[k].id;
        rrect(m, k_btn[k].x, k_btn[k].y, k_btn[k].w, k_btn[k].h, 8, lit ? 0x3A2E08 : PLATE, 0.85f);
        rrect_edge(m, k_btn[k].x + 0.75f, k_btn[k].y + 0.75f, k_btn[k].w - 1.5f, k_btn[k].h - 1.5f, 7.5f,
                   lit ? 1.75f : 1, GOLD, 1);
        text_centre(m, k_btn[k].name, k_btn[k].x + k_btn[k].w / 2, k_btn[k].y + (k_btn[k].h - 7 * 1.75f) / 2,
                    1.75f, lit ? GOLD : WHITE);
    }
}

/* ---- where things are in the window ---- */
static void button_place(float *cx, float *cy)
{
    float l, t, r, b;
    nshvc_safe_area(&l, &t, &r, &b);
    *cx = l + MARGIN + BTN / 2.0f;
    *cy = t + MARGIN + BTN / 2.0f;
}

float pause_badge_left(void)
{
    float l, t, r, b;
    nshvc_safe_area(&l, &t, &r, &b);
    return l + MARGIN + BTN + 8;
}

float pause_badge_top(void)
{
    float l, t, r, b;
    nshvc_safe_area(&l, &t, &r, &b);
    return t + MARGIN + (BTN - ART_BADGE_H) / 2.0f;
}

int pause_badge_lit(void) { return badge_hover; }

static int over_badge(float x, float y)
{
    float l = pause_badge_left(), t = pause_badge_top();
    return x >= l && x < l + ART_BADGE_W && y >= t && y < t + ART_BADGE_H;
}

/* the panel's scale (window points a panel point) and centre */
static void panel_place(float *k, float *cx, float *cy)
{
    float w, h;
    nshvc_window_size(&w, &h);
    *k = fminf((w - 24) / PW, (h - 24) / PH);
    if (*k > 2)
        *k = 2;
    if (*k < 0.25f)
        *k = 0.25f;
    *cx = w / 2;
    *cy = h / 2;
}

static int over_button(float x, float y)
{
    float cx, cy;
    button_place(&cx, &cy);
    return fabsf(x - cx) <= BTN / 2.0f && fabsf(y - cy) <= BTN / 2.0f;
}

static int panel_hit(float x, float y)
{
    float k, cx, cy;
    panel_place(&k, &cx, &cy);
    return pause_hit((x - cx) / k + PW / 2.0f, (y - cy) / k + PH / 2.0f);
}

/* ---- the console's hooks, through main.c ---- */
static void choose(int hit)
{
    if (hit == HIT_RESUME) {
        set_paused(0);
    } else if (hit == HIT_DEFAULTS) {
        keys_default();
        pause_keys_save();
        asking = -1;
        panel_dirty = 1;
    } else if (hit >= 0 && hit < CTLS) {
        asking = asking == hit ? -1 : hit;
        panel_dirty = 1;
    }
}

int pause_key(int vk, int down)
{
    if (!paused) {
        if (down && vk == 0x1B) {
            set_paused(1);
            return 1;
        }
        return 0;
    }
    /* paused: a key let go goes on to the console, so nothing held at the
       pause stays held after it; the rest are the panel's */
    if (!down)
        return 0;
    if (asking >= 0) {
        if (vk == 0x1B) {
            asking = -1;
        } else if (!pause_key_reserved(vk) && *nshvc_ext_key_name(vk)) {
            pause_rebind(asking, vk);
            pause_keys_save();
            asking = -1;
        }
        panel_dirty = 1;
        return 1;
    }
    if (vk == 0x1B)
        set_paused(0);
    /* Tab: the mode, as when not paused (main.c on_key) */
    return vk != 0x09;
}

int pause_pointer(float x, float y, int buttons)
{
    int pressed = buttons & ~mouse_buttons & 1, h;
    static int log = -1;
    if (log < 0)
        log = getenv("SRFX_PAUSELOG") != NULL;
    if (log) {                          /* SRFX_PAUSELOG=1 (a check): what the pointer hook hears */
        float w, wh, l, t, r, b;
        nshvc_window_size(&w, &wh);
        nshvc_safe_area(&l, &t, &r, &b);
        fprintf(stderr, "pause: pointer (%.1f, %.1f) buttons %d, over the button %d, pressed %d, paused %d; window %.0f x %.0f, safe %.0f %.0f\n",
                x, y, buttons, over_button(x, y), pressed, paused, w, wh, l, t);
    }
    mouse_x = x;
    mouse_y = y;
    mouse_buttons = buttons;
    button_hover = over_button(x, y);
    badge_hover = paused && over_badge(x, y);
    h = paused ? panel_hit(x, y) : HIT_NONE;
    if (h != hover) {
        hover = h;
        panel_dirty = 1;
    }
    if (pressed && button_hover) {
        set_paused(!paused);
        return 1;
    }
    if (pressed && badge_hover) {       /* the mode, as Tab */
        front_set_enhanced(!front_enhanced());
        return 1;
    }
    if (pressed && paused)
        choose(h);
    return paused || button_hover;
}

/* SRFX_PAUSE_AT=N:M (a check): paused at the Nth frame shown, for M
   frames shown; a run so paused must leave the same --pacing record as
   one not (the machine still while paused) */
static void pause_at_check(void)
{
    static long n = -1, m, shown;
    if (n < 0) {
        const char *e = getenv("SRFX_PAUSE_AT");
        n = 0;
        if (!e || sscanf(e, "%ld:%ld", &n, &m) != 2)
            n = 0;
    }
    if (!n)
        return;
    shown++;
    if (shown == n || shown == n + m) {
        set_paused(shown == n);
        fprintf(stderr, "pause: %s at frame shown %ld, the machine's frame %u\n", paused ? "paused" : "going on",
                shown, front_frames());
    }
}

#ifdef SRFX_NSHVC_HANDOFF
/* the panel's GPU: its shapes drawn and handed; the overlay's id */
static int panel_gpu_on;

static int panel_gpu_draw(void)
{
    rec_n = 0;
    rec_on = 1;
    draw_panel(&panel);
    rec_on = 0;
    return fw_handoff->panel(rec, rec_n, pad_px, panel.w, panel.h);
}
#endif

void pause_frame(void)
{
    float bx, by, k, cx, cy;
    pause_at_check();
    if (!loaded) {
        Img a, b;
        a.px = baked_unpack(BAKED_BTN_PAUSE, &a.w, &a.h);
        b.px = baked_unpack(BAKED_BTN_PLAY, &b.w, &b.h);
        loaded = 1;
        pause_keys_load();
        /* the panel first: the overlays are drawn in the order made, and
           the play button goes over it where a small window brings them
           together */
#ifdef SRFX_NSHVC_HANDOFF
        if (fw_handoff && sc_gpu_on() && fw_handoff->ready()) {
            panel.w = PW * DENSITY;
            panel.h = PH * DENSITY;
            panel.px = NULL;
            id_panel = panel_gpu_draw();
            if (id_panel < 0)
                fw_cpu_fallback("the pause panel");
            panel_gpu_on = id_panel >= 0;
        }
        if (!panel_gpu_on)
#endif
        {
            panel = img_new(PW, PH);
            draw_panel(&panel);
            id_panel = front_overlay_image(panel.px, panel.w, panel.h);
        }
        panel_dirty = 0;
        id_pause = a.px ? front_overlay_image(a.px, a.w, a.h) : -1;
        id_play = b.px ? front_overlay_image(b.px, b.w, b.h) : -1;
        free(a.px);
        free(b.px);
    }
#ifdef SRFX_NSHVC_HANDOFF
    if (panel_gpu_on && paused) {       /* handed every frame it shows (contract 2.22): anew when it changed */
        if (panel_dirty) {
            if (panel_gpu_draw() < 0)
                fw_cpu_fallback("the pause panel");
            panel_dirty = 0;
        } else
            fw_handoff->panel_again(panel.w, panel.h);
    }
#endif
    if (paused && panel_dirty && panel.px) {
        draw_panel(&panel);
        nshvc_overlay_update(id_panel, panel.px);
        panel_dirty = 0;
    }
    button_place(&bx, &by);
    panel_place(&k, &cx, &cy);
    front_overlay_place(id_panel, cx, cy, k / DENSITY, 0, 1, paused);
    front_overlay_place(id_pause, bx, by, 1.0f / DENSITY, 0, button_hover ? 1 : 0.7f, !paused);
    front_overlay_place(id_play, bx, by, 1.0f / DENSITY, 0, button_hover ? 1 : 0.85f, paused);
}

/* the pictures as made, for a check (pause_check.c): 0 the pause button,
   1 the play button, 2 the panel as it is now, 3 the controller alone */
#if PAUSE_CHECK
uint32_t *pause_picture(int which, int *w, int *h)
{
    Img m = which == 0 ? button_img(0) : which == 1 ? button_img(1) : img_new(PW, PH);
    if (which == 2)
        draw_panel(&m);
    else if (which == 3)                /* the vector drawing itself: the bake's and the check's only */
        pad_draw(m.px, m.w, m.h, DENSITY, PADK, PW / 2.0f, 190);
    *w = m.w;
    *h = m.h;
    return m.px;
}
#endif
