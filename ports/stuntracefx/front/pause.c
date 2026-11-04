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
 * DEFAULT KEYS puts the console's keys back.
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
    SHAPE(m, x, y, x + w, y + h, c, a, d_roundrect(px, py, x, y, w, h, r));
}

static void rrect_edge(Img *m, float x, float y, float w, float h, float r, float t, uint32_t c, float a)
{
    SHAPE(m, x - t, y - t, x + w + t, y + h + t, c, a, fabsf(d_roundrect(px, py, x, y, w, h, r)) - t / 2);
}

static void disc(Img *m, float cx, float cy, float r, uint32_t c, float a)
{
    SHAPE(m, cx - r, cy - r, cx + r, cy + r, c, a, d_circle(px, py, cx, cy, r));
}

static void line(Img *m, float ax, float ay, float bx, float by, float t, uint32_t c, float a)
{
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
    { NSHVC_L, "L", WHITE, 221, 109, 13, 0, 0 },
    { NSHVC_UP, "UP", WHITE, 221.4f, 180, 8, 0, 1 },
    { NSHVC_LEFT, "LEFT", WHITE, 203.2f, 196.6f, 8, 0, 2 },
    { NSHVC_RIGHT, "RIGHT", WHITE, 239.5f, 196.6f, 8, 0, 3 },
    { NSHVC_DOWN, "DOWN", WHITE, 221.4f, 213.2f, 8, 0, 4 },
    { NSHVC_SELECT, "SELECT", WHITE, 282.5f, 207.5f, 10, 0, 5 },
    { NSHVC_R, "R", WHITE, 419, 109, 13, 1, 0 },
    { NSHVC_X, "X", 0x5A8CF5, 403.6f, 171.8f, 12.6f, 1, 1 },
    { NSHVC_Y, "Y", 0x3CC070, 373.7f, 198.4f, 12.6f, 1, 2 },
    { NSHVC_A, "A", 0xF0504A, 433.5f, 197.5f, 12.6f, 1, 3 },
    { NSHVC_B, "B", 0xF5D34A, 403.6f, 218.8f, 12.6f, 1, 4 },
    { NSHVC_START, "START", WHITE, 317.4f, 207.5f, 10, 1, 5 },
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
static int paused, asking = -1, hover = HIT_NONE, button_hover, loaded;
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

/* The controller, the Super Famicom's as photographed: its shapes laid
   out in the photograph's pixels (1600 x 1200, the body 1,355 across) and
   brought to the panel's points by RX and RY (0.2214 a pixel, its middle
   at the panel's (320, 190)); a length by RS. The body is the two grips'
   circles (r 320 about (480, 640) and (1195, 640)) and the middle between
   their centres, its top the line along the circles' tops (y 320) and its
   underside at 905, joined to the grips by a fillet below their centres
   only, so the top runs straight into them and the underside curves up
   between them. Each shoulder button is a band 56 pixels deep along the
   outline, from 40 degrees above the grip's centre line (cut across the
   outline) to x 550 on the top (and its mirror), its ends rounded. The
   photograph's logo is left out. */
#define RSC 0.2214f
#define RX(x) (320 + ((x) - 837.5f) * RSC)
#define RY(y) (190 + ((y) - 650) * RSC)
#define RS(v) ((v) * RSC)

static float smin(float a, float b, float k)
{
    float h = fmaxf(k - fabsf(a - b), 0) / k;
    return fminf(a, b) - h * h * k / 4;
}

/* the body's distance at panel point (px, py), in points */
static float d_body(float px, float py)
{
    float l = d_circle(px, py, RX(480), RY(640), RS(320)), r = d_circle(px, py, RX(1195), RY(640), RS(320));
    float mid = d_roundrect(px, py, RX(480), RY(320), RS(715), RS(585), RS(1));
    float lobes = fminf(l, r);
    return py > RY(640) ? smin(lobes, mid, RS(150)) : fminf(lobes, mid);
}

/* a shoulder button's distance: side -1 the left (L), 1 the right (R) */
static float d_shoulder(float px, float py, int side)
{
    const float r = RS(14);
    float cx = side < 0 ? RX(480) : RX(1195), cy = RY(640);
    /* the outline 56 pixels out */
    float band = d_body(px, py) - RS(56);
    /* the cut across the outline: the line from the grip's centre at 40
       degrees above its centre line, outward; and the one at x 550 */
    float ux = side * cosf(40 * 3.14159265f / 180), uy = -sinf(40 * 3.14159265f / 180);
    float across = side * (ux * (py - cy) - uy * (px - cx));     /* > 0 below the cut */
    float inner = side < 0 ? px - RX(550) : RX(1125) - px;
    float cut = fmaxf(across, inner);
    return hypotf(fmaxf(band + r, 0), fmaxf(cut + r, 0)) + fminf(fmaxf(band + r, cut + r), 0) - r;
}

/* a triangle (an arrow on the D-pad) by its three corners, in points */
static float d_tri(float px, float py, const float *c)
{
    float d = -1e9f;
    int i;
    for (i = 0; i < 3; i++) {
        const float *a = c + 2 * i, *b = c + 2 * ((i + 1) % 3), *o = c + 2 * ((i + 2) % 3);
        float nx = b[1] - a[1], ny = a[0] - b[0], n = hypotf(nx, ny), e;
        if ((o[0] - a[0]) * nx + (o[1] - a[1]) * ny > 0)
            nx = -nx, ny = -ny;
        e = ((px - a[0]) * nx + (py - a[1]) * ny) / n;
        d = fmaxf(d, e);
    }
    return d;
}

static void text_plain(Img *m, const char *s, float cx, float top, float dot, uint32_t c)
{
    const char *p;
    float x = cx - text_width(s, dot) / 2;
    for (p = s; *p; p++, x += 6 * dot) {
        const uint8_t *g = glyph(*p);
        int r, b;
        if (g)
            for (r = 0; r < 7; r++)
                for (b = 0; b < 5; b++)
                    if (g[r] >> (4 - b) & 1)
                        rrect(m, x + b * dot, top + r * dot, dot, dot, 0, c, 1);
    }
}

static void draw_controller(Img *m)
{
    const uint32_t body = 0xDCDCE0, edge = 0xA9A9B1, shoulder = 0xC9C9CF, dpad = 0x77777D, plate = 0x9D9DA3;
    const float bx0 = RX(150), by0 = RY(240), bx1 = RX(1525), by1 = RY(975);
    static const float arrows[4][6] = {      /* the D-pad's arrows, photograph pixels */
        { 392, 588, 378, 612, 406, 612 }, { 392, 772, 378, 748, 406, 748 },
        { 300, 680, 324, 666, 324, 694 }, { 484, 680, 460, 666, 460, 694 } };
    int i, k;
    /* the cable: up from the notch in an arc over to the left shoulder,
       behind it (a cubic through the photograph's (790, 305) and (300,
       330), pulled by (770, 40) and (360, 50)) */
    {
        float lx = RX(790), ly = RY(305);
        for (i = 1; i <= 32; i++) {
            float t = i / 32.0f, u = 1 - t;
            float x = u * u * u * 790 + 3 * u * u * t * 770 + 3 * u * t * t * 360 + t * t * t * 300;
            float y = u * u * u * 305 + 3 * u * u * t * 40 + 3 * u * t * t * 50 + t * t * t * 330;
            line(m, lx, ly, RX(x), RY(y), RS(22), 0x3C3C42, 1);
            lx = RX(x);
            ly = RY(y);
        }
    }
    /* the shadow under it, the shoulders, the body */
    SHAPE(m, bx0, by0, bx1, by1 + 8, 0x000000, 0.35f, d_body(px - 1.5f, py - 6));
    SHAPE(m, RX(90), RY(250), RX(560), RY(560), shoulder, 1, d_shoulder(px, py, -1));
    SHAPE(m, RX(1115), RY(250), RX(1585), RY(560), shoulder, 1, d_shoulder(px, py, 1));
    SHAPE(m, RX(90), RY(250), RX(560), RY(560), edge, 1, fabsf(d_shoulder(px, py, -1)) - 0.5f);
    SHAPE(m, RX(1115), RY(250), RX(1585), RY(560), edge, 1, fabsf(d_shoulder(px, py, 1)) - 0.5f);
    SHAPE(m, bx0, by0, bx1, by1, body, 1, d_body(px, py));
    SHAPE(m, bx0, by0, bx1, by1, edge, 1, fabsf(d_body(px, py)) - 0.6f);
    /* the cable's notch */
    rrect(m, RX(775), RY(316), RS(30), RS(14), RS(4), 0x8E8E96, 1);
    /* the lines from each tag to its button, under the buttons */
    for (i = 0; i < CTLS; i++) {
        float tx, ty, ex;
        int lit = hover == i || asking == i;
        tag_box(i, &tx, &ty);
        ex = k_ctl[i].right ? tx : tx + TAG_W;
        line(m, ex, ty + TAG_H / 2.0f, k_ctl[i].bx, k_ctl[i].by, lit ? 1.75f : 1.1f, lit ? GOLD : 0x7A7A84, 1);
    }
    /* the D-pad: its dish, the cross with its edge, the arrows, the dimple */
    disc(m, RX(400), RY(680), RS(172), 0xE9E9EC, 1);
    SHAPE(m, RX(220), RY(500), RX(580), RY(860), 0xBDBDC4, 1, fabsf(d_circle(px, py, RX(400), RY(680), RS(172))) - 0.5f);
    rrect(m, RX(348), RY(555), RS(88), RS(250), RS(10), 0x4C4C52, 1);
    rrect(m, RX(267), RY(636), RS(250), RS(88), RS(10), 0x4C4C52, 1);
    rrect(m, RX(352), RY(559), RS(80), RS(242), RS(8), dpad, 1);
    rrect(m, RX(271), RY(640), RS(242), RS(80), RS(8), dpad, 1);
    for (i = 0; i < 4; i++) {
        float t[6];
        for (k = 0; k < 3; k++) {
            t[2 * k] = RX(arrows[i][2 * k]);
            t[2 * k + 1] = RY(arrows[i][2 * k + 1]);
        }
        SHAPE(m, fminf(t[0], fminf(t[2], t[4])), fminf(t[1], fminf(t[3], t[5])), fmaxf(t[0], fmaxf(t[2], t[4])),
              fmaxf(t[1], fmaxf(t[3], t[5])), 0x96969C, 1, d_tri(px, py, t));
    }
    disc(m, RX(392), RY(680), RS(16), 0x5E5E64, 1);
    /* SELECT and START: pills sloping up to the right, their names under */
    for (i = 0; i < 2; i++) {
        float cx = i ? 826 : 668;
        line(m, RX(cx - 36), RY(758), RX(cx + 36), RY(700), RS(46), 0x5A5A60, 1);
        line(m, RX(cx - 34), RY(755), RX(cx + 34), RY(701), RS(36), 0x8A8A90, 1);
    }
    text_plain(m, "SELECT", RX(668), RY(800), 0.9f, 0x8A8A92);
    text_plain(m, "START", RX(826), RY(800), 0.9f, 0x8A8A92);
    /* the face buttons' dish (inside the body), its two lighter troughs,
       the buttons' letters printed by them */
    SHAPE(m, RX(940), RY(390), RX(1520), RY(970), plate, 1,
          fmaxf(d_circle(px, py, RX(1228), RY(680), RS(287)), d_body(px, py) + RS(4)));
    line(m, RX(1080), RY(688), RX(1215), RY(568), RS(170), 0xBEBEC4, 1);
    line(m, RX(1215), RY(780), RX(1350), RY(684), RS(170), 0xBEBEC4, 1);
    text_plain(m, "X", RX(1315), RY(490), 1.5f, 0xE2E2E6);
    text_plain(m, "A", RX(1437), RY(605), 1.5f, 0xE2E2E6);
    text_plain(m, "Y", RX(990), RY(745), 1.5f, 0xE2E2E6);
    text_plain(m, "B", RX(1110), RY(855), 1.5f, 0xE2E2E6);
    for (i = 0; i < CTLS; i++)
        if (k_ctl[i].control & (NSHVC_A | NSHVC_B | NSHVC_X | NSHVC_Y)) {
            static const uint32_t face[4] = { 0x0B4FC4, 0x2A8A52, 0xDC1423, 0xE8BE08 };   /* X Y A B */
            uint32_t c = face[k_ctl[i].control == NSHVC_X ? 0 : k_ctl[i].control == NSHVC_Y ? 1
                              : k_ctl[i].control == NSHVC_A ? 2 : 3];
            float r = k_ctl[i].br;
            disc(m, k_ctl[i].bx + 0.6f, k_ctl[i].by + 1.2f, r + 1.2f, 0x000000, 0.3f);
            disc(m, k_ctl[i].bx, k_ctl[i].by, r + 1, 0xD2D2D8, 1);
            disc(m, k_ctl[i].bx, k_ctl[i].by, r, c, 1);
            disc(m, k_ctl[i].bx - 3.5f, k_ctl[i].by - 4, 3.5f, WHITE, 0.22f);
        }
    /* a ring on the button under the mouse or asked for */
    for (i = 0; i < CTLS; i++)
        if (hover == i || asking == i)
            SHAPE(m, k_ctl[i].bx - 20, k_ctl[i].by - 20, k_ctl[i].bx + 20, k_ctl[i].by + 20, GOLD, 1,
                  fabsf(d_circle(px, py, k_ctl[i].bx, k_ctl[i].by, k_ctl[i].br + 3)) - 1);
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
    return 1;
}

int pause_pointer(float x, float y, int buttons)
{
    int pressed = buttons & ~mouse_buttons & 1, h;
    mouse_x = x;
    mouse_y = y;
    mouse_buttons = buttons;
    button_hover = over_button(x, y);
    h = paused ? panel_hit(x, y) : HIT_NONE;
    if (h != hover) {
        hover = h;
        panel_dirty = 1;
    }
    if (pressed && button_hover) {
        set_paused(!paused);
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

void pause_frame(void)
{
    float bx, by, k, cx, cy;
    pause_at_check();
    if (!loaded) {
        Img a = button_img(0), b = button_img(1);
        loaded = 1;
        pause_keys_load();
        /* the panel first: the overlays are drawn in the order made, and
           the play button goes over it where a small window brings them
           together */
        panel = img_new(PW, PH);
        draw_panel(&panel);
        id_panel = front_overlay_image(panel.px, panel.w, panel.h);
        panel_dirty = 0;
        id_pause = front_overlay_image(a.px, a.w, a.h);
        id_play = front_overlay_image(b.px, b.w, b.h);
        free(a.px);
        free(b.px);
    }
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
   1 the play button, 2 the panel as it is now */
uint32_t *pause_picture(int which, int *w, int *h)
{
    Img m = which == 0 ? button_img(0) : which == 1 ? button_img(1) : img_new(PW, PH);
    if (which == 2)
        draw_panel(&m);
    *w = m.w;
    *h = m.h;
    return m.px;
}
