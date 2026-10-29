/*
 * art.c: the touch controls' pictures (front.h), drawn here once as ARGB at
 * twice the window's points and handed to the console's overlay: the
 * steering wheel under the finger (a gold rim, a hub and three spokes), the
 * two pedals (a ribbed pad with its name) and the pause button (two bars);
 * and on every host the mode's badge (ORIGINAL or ENHANCED, Tab).
 * Shapes are by their distance from each pixel's centre, a pixel's edge
 * blended across one pixel.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "front.h"

#define DENSITY 2                   /* pixels a point */

typedef struct {
    uint32_t *px;
    int w, h;
} Img;

static Img img_new(float wpt, float hpt)
{
    Img m;
    m.w = (int)(wpt * DENSITY);
    m.h = (int)(hpt * DENSITY);
    m.px = (uint32_t *)calloc((size_t)m.w * m.h, 4);
    return m;
}

/* colour c (0xRRGGBB) at coverage a (0-1) over what is there */
static void blend(Img *m, int x, int y, uint32_t c, float a)
{
    uint32_t *p = &m->px[y * m->w + x], d = *p;
    float da = (d >> 24) / 255.0f, oa = a + da * (1 - a);
    int k;
    uint32_t out = 0;
    if (a <= 0)
        return;
    for (k = 0; k < 3; k++) {
        float s = (c >> (16 - 8 * k) & 0xFF), t = (d >> (16 - 8 * k) & 0xFF);
        float v = oa > 0 ? (s * a + t * da * (1 - a)) / oa : 0;
        out |= (uint32_t)(v + 0.5f) << (16 - 8 * k);
    }
    *p = out | (uint32_t)(oa * 255 + 0.5f) << 24;
}

static float cover(float d) { return d < -0.5f ? 1 : d > 0.5f ? 0 : 0.5f - d; }

/* distances in points from (px, py), a pixel's centre */
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

/* a shape over the whole picture: dist(point) and how it is painted */
#define SHAPE(m, c, alpha, expr)                                                              \
    do {                                                                                      \
        int x_, y_;                                                                           \
        for (y_ = 0; y_ < (m)->h; y_++)                                                       \
            for (x_ = 0; x_ < (m)->w; x_++) {                                                 \
                float px = (x_ + 0.5f) / DENSITY, py = (y_ + 0.5f) / DENSITY;                 \
                blend((m), x_, y_, (c), (alpha) * cover((expr) * DENSITY));                   \
            }                                                                                 \
    } while (0)

/* 5 x 7 capitals for the pedals' names and the mode's badge */
static const struct { char c; uint8_t row[7]; } k_font[] = {
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } }, { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { 'D', { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } },
    { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } }, { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } }, { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } }, { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
};

/* text centred on (cx, top), each font pixel a square of `dot` points */
static void text(Img *m, const char *s, float cx, float top, float dot, uint32_t c)
{
    int n = (int)strlen(s), i, r, b;
    float x0 = cx - (n * 6 - 1) * dot / 2;
    for (i = 0; i < n; i++) {
        const uint8_t *g = NULL;
        size_t k;
        for (k = 0; k < sizeof k_font / sizeof k_font[0]; k++)
            if (k_font[k].c == s[i])
                g = k_font[k].row;
        if (!g)
            continue;
        for (r = 0; r < 7; r++)
            for (b = 0; b < 5; b++)
                if (g[r] >> (4 - b) & 1) {
                    float x = x0 + (i * 6 + b) * dot, y = top + r * dot;
                    SHAPE(m, c, 1, d_roundrect(px, py, x, y, dot, dot, 0));
                }
    }
}

static Img wheel(void)
{
    const float size = 160, k = size / 100;
    static const float s[3][4] = { { 50, 40, 50, 10 }, { 59, 56, 84, 70 }, { 41, 56, 16, 70 } };
    const uint32_t gold = 0xF5D34A;
    Img m = img_new(size, size);
    int i;
    /* the glow: the rim's, faded over 8 points */
    SHAPE(&m, gold, 0.45f * fmaxf(0, 1 - fabsf(d_circle(px, py, 50 * k, 50 * k, 44 * k)) / (4.5f * k + 8)), 0.0f - 1);
    SHAPE(&m, gold, 1, fabsf(d_circle(px, py, 50 * k, 50 * k, 44 * k)) - 4.5f * k);
    SHAPE(&m, gold, 1, d_circle(px, py, 50 * k, 50 * k, 11 * k));
    for (i = 0; i < 3; i++)
        SHAPE(&m, gold, 1, d_segment(px, py, s[i][0] * k, s[i][1] * k, s[i][2] * k, s[i][3] * k) - 3.5f * k);
    return m;
}

static Img pedal(float w, float h, uint32_t c, const char *name)
{
    Img m = img_new(w, h);
    int i;
    SHAPE(&m, 0x141414, 0.55f, d_roundrect(px, py, 4, 4, w - 8, h - 8, 16));
    SHAPE(&m, c, 1, fabsf(d_roundrect(px, py, 4, 4, w - 8, h - 8, 16)) - 2);
    for (i = 0; i < 4 && 22 + i * 16 < h - 40; i++)
        SHAPE(&m, c, 1, d_segment(px, py, 22, 22 + i * 16, w - 22, 22 + i * 16) - 2.5f);
    text(&m, name, w / 2, h - 32, 1.5f, c);
    return m;
}

/* the mode's badge: its name on a dark rounded plate, edged in its colour */
#define BADGE_W 112
#define BADGE_H 26
static Img badge(const char *name, uint32_t c)
{
    Img m = img_new(BADGE_W, BADGE_H);
    SHAPE(&m, 0x141414, 0.7f, d_roundrect(px, py, 1, 1, BADGE_W - 2, BADGE_H - 2, 6));
    SHAPE(&m, c, 1, fabsf(d_roundrect(px, py, 1, 1, BADGE_W - 2, BADGE_H - 2, 6)) - 0.75f);
    text(&m, name, BADGE_W / 2.0f, (BADGE_H - 7 * 1.75f) / 2, 1.75f, c);
    return m;
}

static Img pause_bars(void)
{
    Img m = img_new(56, 44);
    /* the shadow, then the bars */
    SHAPE(&m, 0x000000, 0.6f * fmaxf(0, 1 - fmaxf(0, fminf(d_roundrect(px, py, 17, 10, 8, 24, 2),
                                                             d_roundrect(px, py, 31, 10, 8, 24, 2))) / 3), 0.0f - 1);
    SHAPE(&m, 0xFFFFFF, 1, fminf(d_roundrect(px, py, 17, 10, 8, 24, 2), d_roundrect(px, py, 31, 10, 8, 24, 2)));
    return m;
}

static int id_wheel = -1, id_brake = -1, id_boost = -1, id_pause = -1;
static int id_original = -1, id_enhanced = -1;

static int hand_over(Img m)
{
    int id = front_overlay_image(m.px, m.w, m.h);
    free(m.px);
    return id;
}

void art_make(void)
{
    id_wheel = hand_over(wheel());
    id_brake = hand_over(pedal(84, 128, 0xE84A3C, "BRAKE"));
    id_boost = hand_over(pedal(84, 112, 0x4AA8F5, "BOOST"));
    id_pause = hand_over(pause_bars());
}

void art_place(const TouchView *v)
{
    const float s = 1.0f / DENSITY;
    if (id_wheel < 0)
        return;
    front_overlay_place(id_wheel, v->wheel_x, v->wheel_y, s, v->wheel_angle, 0.9f, v->driving && v->wheel);
    front_overlay_place(id_brake, v->brake_x, v->brake_y, s * (v->brake ? 0.94f : 1), 0, v->brake ? 0.85f : 0.4f,
                        v->driving);
    front_overlay_place(id_boost, v->boost_x, v->boost_y, s * (v->boost ? 0.94f : 1), 0, v->boost ? 0.85f : 0.4f,
                        v->driving);
    front_overlay_place(id_pause, v->pause_x, v->pause_y, s, 0, 0.75f, v->driving);
}

void art_make_mode(void)
{
    id_original = hand_over(badge("ORIGINAL", 0xF5D34A));
    id_enhanced = hand_over(badge("ENHANCED", 0x4AA8F5));
}

/* the badge shows at launch and on each change of mode: BADGE_SHOW frames
   at full, then fades out over BADGE_FADE */
#define BADGE_SHOW 180
#define BADGE_FADE 30

void art_place_mode(int enhanced, float left, float top)
{
    static int shown_mode = -1;
    static uint32_t since;
    const float s = 1.0f / DENSITY, cx = left + BADGE_W / 2.0f, cy = top + BADGE_H / 2.0f;
    uint32_t age;
    float a;
    if (id_original < 0)
        return;
    if (enhanced != shown_mode) {
        shown_mode = enhanced;
        since = front_frames();
    }
    age = front_frames() - since;
    a = age < BADGE_SHOW ? 1.0f
      : age < BADGE_SHOW + BADGE_FADE ? 1.0f - (float)(age - BADGE_SHOW) / BADGE_FADE : 0.0f;
    front_overlay_place(id_original, cx, cy, s, 0, 0.85f * a, !enhanced && a > 0);
    front_overlay_place(id_enhanced, cx, cy, s, 0, 0.85f * a, enhanced && a > 0);
}

/* the pictures as made, for a check (touch_check.c): 0 wheel, 1 brake,
   2 boost, 3 pause, 4 the ORIGINAL badge, 5 the ENHANCED one */
uint32_t *art_picture(int which, int *w, int *h)
{
    Img m = which == 0 ? wheel() : which == 1 ? pedal(84, 128, 0xE84A3C, "BRAKE")
          : which == 2 ? pedal(84, 112, 0x4AA8F5, "BOOST") : which == 3 ? pause_bars()
          : which == 4 ? badge("ORIGINAL", 0xF5D34A) : badge("ENHANCED", 0x4AA8F5);
    *w = m.w;
    *h = m.h;
    return m.px;
}
