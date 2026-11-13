/*
 * fx_weather.c: the race's rain and snow drawn at the window's resolution
 * and across the whole view.
 *
 * Jeff (2026-11-12, Big Ravine's rain, White Land's snow): "the rain is
 * not properly resolution/fov agnostic". The chip's FxWeatherDraw
 * (render/frame.c) keeps 80 drops at $2864, ten bytes each (a life, a
 * colour, x, y and z in the drops' own space, then the drop's last place
 * on the screen as two bytes), moves them each picture (x by the view's
 * turn, y down by the fall, z toward the eye by the speed, each wrapping:
 * x from $1FF to -$1FF, y from $FF to -$FF, z below 0 to $1FF) and
 * projects one whose move did not wrap by the factor in the table at
 * $14:8BC4 + 2z, 128 * 32768 / (z + 300) (the 3D's own focal length, 128
 * pixels): on the screen at (cx + x f / 32768, cy + y f / 32768 - 20). It
 * then drew a rain drop as a line of the picture's pixels from its last
 * place to its new one, and a flake as one pixel (four, two by two, at
 * $100 or nearer). That drawing was the console's, and broke three ways in
 * the window: the last place kept in a byte each, so past column 255 it
 * wrapped and the line ran across the screen; the picture's pixels, so the
 * rain was steps of big squares at any window's size; and the drops' space
 * spread for the original's 208 x 128 view (at $1FF deep a drop lands at
 * most 81 pixels either side of the centre and 40 above or below), so a
 * wider or taller view had none at its sides, top and bottom.
 *
 * Now the chip's drops move as ever (their state, their seed, their life,
 * their colours untouched, the chip's plots run and draw nothing) and only
 * their drawing is this:
 *  - each drop's place projected as the chip projects it, by its table,
 *    unrounded, at its pixel's centre;
 *  - a rain drop a streak from where its move began to where it ended
 *    (the same two places the chip's line joins on two pictures in a row;
 *    kept in the drop's own record, so the pictures between steps and the
 *    two players' need nothing kept here), RAIN_WIDE of the window's
 *    pixels across;
 *  - a flake round, of the area the chip's one pixel or four had;
 *  - the drops' space laid side by side by its wraps (1,022 across, 510 up
 *    and down) as far as the view reaches, so every part of any view has
 *    drops as the original's middle had, none more being moved;
 *  - each kept to the view's rectangle.
 */
#include <math.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_face.h"
#include "fx/fx_rt.h"
#include "fx/fx_weather.h"

#define FIELD_X 1022.0f         /* x wraps from $1FF to -$1FF */
#define FIELD_Y 510.0f          /* y from $FF to -$FF */
#define RAIN_WIDE 1.0f          /* a streak's width, the window's pixels */
#define SNOW_SIDES 8
#define MARGIN 2.0f             /* picture pixels past the view a copy is still tried */
#define COPIES_MOST 64          /* copies each way at most */

static int on, kind;
static float cx, cy, left, right, top, bottom, list_x, list_y;

/* a word of the chip's RAM, read without moving its last RAM address */
static float word_at(uint16_t a)
{
    return (float)(int16_t)(fx_ram[fx_ram_at(a)] | fx_ram[fx_ram_at((uint16_t)(a ^ 1))] << 8);
}

/* the chip's factor for depth z: its table's word at $14:(z + 300) * 2 -
   $7694, read as the chip reads it (fx_rom_read moves nothing) */
static float factor(uint16_t z)
{
    uint16_t a = (uint16_t)((uint16_t)((z + 0x012C) * 2) - 0x7694);

    return (float)(int16_t)(fx_rom_read(0x14, a) | fx_rom_read(0x14, (uint16_t)(a + 1)) << 8) / 32768.0f;
}

void fx_weather_begin(int weather)
{
    kind = weather;
    on = 1;
    cx = word_at(0x0034);
    cy = word_at(0x0036);
    fx_face_view_rect(&left, &right, &top, &bottom);
    fx_draw_world_scale(&list_x, &list_y);
    if (list_x <= 0)
        list_x = 1;
    if (list_y <= 0)
        list_y = 1;
}

void fx_weather_end(void)
{
    on = 0;
}

int fx_weather_drawing(void)
{
    return on;
}

/* polygon p (n corners, at most 12, picture pixels) kept to the view's
   rectangle and drawn in the colour the chip's plot had */
static void draw_kept(float (*p)[2], int n)
{
    float a[20][2], b[20][2];
    FxCorner tri[3 * 18];
    int side, i, m;

    memcpy(a, p, (size_t)n * sizeof a[0]);
    for (side = 0; side < 4 && n >= 3; side++) {
        int axis = side >> 1, keep_low = side & 1;
        float edge = side == 0 ? left : side == 1 ? right : side == 2 ? top : bottom;

        m = 0;
        for (i = 0; i < n; i++) {
            const float *s = a[i], *e = a[(i + 1) % n];
            int s_in = keep_low ? s[axis] <= edge : s[axis] >= edge, e_in = keep_low ? e[axis] <= edge : e[axis] >= edge;

            if (s_in)
                memcpy(b[m++], s, sizeof b[0]);
            if (s_in != e_in) {
                float t = (edge - s[axis]) / (e[axis] - s[axis]);

                b[m][0] = s[0] + (e[0] - s[0]) * t;
                b[m][1] = s[1] + (e[1] - s[1]) * t;
                m++;
            }
        }
        n = m;
        memcpy(a, b, (size_t)n * sizeof a[0]);
    }
    if (n < 3)
        return;
    for (i = 1, m = 0; i + 1 < n; i++) {
        int k, at[3] = { 0, i, i + 1 };

        for (k = 0; k < 3; k++, m++) {
            tri[m].x = a[at[k]][0];
            tri[m].y = a[at[k]][1];
            tri[m].u = tri[m].v = 0;
        }
    }
    fx_draw_flat(tri, (uint32_t)m, fxs.colr);
}

/* a streak from (x0, y0) to (x1, y1), picture pixels, RAIN_WIDE of the
   window's pixels across (its sides set out in the list's pixels, which
   are the window's, so it is as wide whichever way it runs) */
static void streak(float x0, float y0, float x1, float y1)
{
    float dx = (x1 - x0) * list_x, dy = (y1 - y0) * list_y, l = sqrtf(dx * dx + dy * dy), nx, ny;
    float q[4][2];

    if (l < 1e-3f) {                    /* not moved on the screen: as long as wide, down */
        dx = 0, dy = RAIN_WIDE, l = RAIN_WIDE;
        y1 = y0 + RAIN_WIDE / list_y;
    }
    nx = -dy / l * (RAIN_WIDE / 2) / list_x;
    ny = dx / l * (RAIN_WIDE / 2) / list_y;
    q[0][0] = x0 + nx, q[0][1] = y0 + ny;
    q[1][0] = x1 + nx, q[1][1] = y1 + ny;
    q[2][0] = x1 - nx, q[2][1] = y1 - ny;
    q[3][0] = x0 - nx, q[3][1] = y0 - ny;
    draw_kept(q, 4);
}

/* a round flake at (x, y) of area a, picture pixels: an octagon of radius
   r has area 2 sqrt(2) r^2 */
static void flake(float x, float y, float a)
{
    float q[SNOW_SIDES][2], r = sqrtf(a / (2.0f * 1.41421356f));
    int i;

    for (i = 0; i < SNOW_SIDES; i++) {
        q[i][0] = x + r * cosf(6.2831853f * i / SNOW_SIDES);
        q[i][1] = y + r * sinf(6.2831853f * i / SNOW_SIDES);
    }
    draw_kept(q, SNOW_SIDES);
}

/* the copies k of a place c + (v + period k) f lying within [lo, hi] */
static void copies(float c, float v, float f, float period, float lo, float hi, int *k0, int *k1)
{
    float a = ((lo - MARGIN - c) / f - v) / period, b = ((hi + MARGIN - c) / f - v) / period;

    *k0 = a < -COPIES_MOST ? -COPIES_MOST : (int)ceilf(a);
    *k1 = b > COPIES_MOST ? COPIES_MOST : (int)floorf(b);
}

void fx_weather_drop(int16_t x, int16_t y, uint16_t z, int16_t nx, int16_t ny, uint16_t nz)
{
    float f = factor(z), nf = factor(nz);
    /* where the chip's pixel for a place lies, its centre: (cx + x f, cy +
       y f - 20) and half a pixel on */
    float ox = cx + 0.5f, oy = cy - 20 + 0.5f;
    int kx0, kx1, ky0, ky1, a, b, kx, ky;

    if (!on || f <= 0 || nf <= 0)
        return;
    copies(ox, nx, nf, FIELD_X, left, right, &kx0, &kx1);
    copies(oy, ny, nf, FIELD_Y, top, bottom, &ky0, &ky1);
    if (kind == 1) {                    /* rain: the copies its start reaches too */
        copies(ox, x, f, FIELD_X, left, right, &a, &b);
        kx0 = a < kx0 ? a : kx0, kx1 = b > kx1 ? b : kx1;
        copies(oy, y, f, FIELD_Y, top, bottom, &a, &b);
        ky0 = a < ky0 ? a : ky0, ky1 = b > ky1 ? b : ky1;
    }
    for (ky = ky0; ky <= ky1; ky++)
        for (kx = kx0; kx <= kx1; kx++) {
            float sx = ox + (nx + kx * FIELD_X) * nf, sy = oy + (ny + ky * FIELD_Y) * nf;

            if (kind == 1) {
                float px = ox + (x + kx * FIELD_X) * f, py = oy + (y + ky * FIELD_Y) * f;

                if (fmaxf(px, sx) < left - MARGIN || fminf(px, sx) > right + MARGIN || fmaxf(py, sy) < top - MARGIN
                    || fminf(py, sy) > bottom + MARGIN)
                    continue;
                streak(px, py, sx, sy);
            } else {
                /* the chip's flake: four pixels, two by two from its own,
                   at $100 or nearer, else its one */
                if (sx < left - MARGIN || sx > right + MARGIN || sy < top - MARGIN || sy > bottom + MARGIN)
                    continue;
                if (nz <= 0x0100)
                    flake(sx + 0.5f, sy + 0.5f, 4.0f);
                else
                    flake(sx, sy, 1.0f);
            }
        }
}
