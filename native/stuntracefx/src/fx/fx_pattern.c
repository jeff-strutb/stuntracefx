/*
 * fx_pattern.c: simple pattern textures drawn as geometry (plan stage 10,
 * "Pattern textures as geometry").
 *
 * A pattern is a texture whose texels are a few flat colours bounded by
 * straight edges, drawn as stairs at the chip's texel size. Each is given
 * here once, in its tile's texel space (a texel x, y covers [x, x+1) x
 * [y, y+1)), as convex regions bounded by the straight edges the stairs
 * stand for, each region showing one texel value. The regions are laid in
 * order, a later one over an earlier (a sign's frame line passing behind
 * its arrow), and a place no region covers shows nothing (the texture's
 * see-through texels). The table is checked against the tile's every texel
 * when first used: the last region holding a texel's centre must show its
 * value, a centre no region holds must be a 0 texel, or the pattern is not
 * used. A pattern with such holes is used only where the options make 0
 * see-through.
 *
 * A face with such a texture is laid with the regions instead: fanned from
 * its first corner, each triangle mapping texel coordinates to view space
 * by its own three corners, as the GPU lays the texture; each region of
 * each tile a triangle covers is clipped to it and handed on as a flat
 * piece in view space, in the regions' order. Nothing here makes a pixel.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_pattern.h"

enum {
    CORNERS = 8,                /* a region's corners */
    POLY = 16,                  /* a clipped piece's corners: a triangle's three edges add 3 */
    MOST_TILES = 64             /* a face over more tiles keeps its texture */
};

/* A convex region of the tile, its corners in order, and the texel value
   it shows. */
typedef struct {
    uint8_t value;
    int n;
    double p[CORNERS][2];
} Region;

enum { PART_BYTE, PART_HIGH, PART_LOW };

struct FxPattern {
    uint8_t bank;
    uint16_t base, mask;
    uint8_t part;               /* which part of the byte the texture reads */
    const char *name;
    int regions;
    const Region *region;
    /* made when first used */
    int made, good, holes, w, h;
};

/* The yellow and black stripes (the barriers' walls). A 32 x 32 tile of two
   values, $A and $F, in stripes 8 texels wide that move one texel left
   every 4 rows (the first and last steps 2 rows, the tile's middle the
   steps' middle). The edges through the middle of each step are
   X = 8k - Y / 4: $A where X + Y / 4 lies in [16k, 16k + 8), else $F. */
static const Region stripes[] = {
    { 0xA, 3, { { 0, 0 }, { 8, 0 }, { 0, 32 } } },
    { 0xF, 4, { { 8, 0 }, { 16, 0 }, { 8, 32 }, { 0, 32 } } },
    { 0xA, 4, { { 16, 0 }, { 24, 0 }, { 16, 32 }, { 8, 32 } } },
    { 0xF, 4, { { 24, 0 }, { 32, 0 }, { 24, 32 }, { 16, 32 } } },
    { 0xA, 3, { { 32, 0 }, { 32, 32 }, { 24, 32 } } },
};

/* The chevrons (the corners' boards; two tiles, the same shape in two
   colours). A 32 x 32 tile: the fill reaches one texel further right each
   row down to rows 15 and 16, which it fills, then one less each row; the
   edges through the steps' corners are X - Y = 16.5 above and X + Y = 48.5
   below, the light value right of them. */
#define CHEVRON(fill, light) \
    { (fill), 6, { { 0, 0 }, { 16.5, 0 }, { 32, 15.5 }, { 32, 16.5 }, { 16.5, 32 }, { 0, 32 } } }, \
    { (light), 3, { { 16.5, 0 }, { 32, 0 }, { 32, 15.5 } } }, \
    { (light), 3, { { 32, 16.5 }, { 32, 32 }, { 16.5, 32 } } }

static const Region chevron_red[] = { CHEVRON(0x2, 0xE) };
static const Region chevron_blue[] = { CHEVRON(0x4, 0xE) };

/* The sign with a chevron (">") over four panes. A 32 x 32 tile: the
   frame's lines, as their texels lie, under the chevron; the chevron's
   white from row 6 to row 24 between X - Y = -0.5 and 11.5 above its
   point (Y = 15.5) and between X + Y = 30.5 and 42.5 below it, its fill
   from row 7 to row 23 between X - Y = 1.5 and 9.5 and X + Y = 32.5 and
   40.5 (each edge through the middle of the texels' steps). The panes are
   see-through. */
static const Region sign[] = {
    { 0xE, 4, { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } } },
    { 0xB, 4, { { 1, 0 }, { 31, 0 }, { 31, 1 }, { 1, 1 } } },
    { 0xC, 4, { { 31, 0 }, { 32, 0 }, { 32, 32 }, { 31, 32 } } },
    { 0xB, 4, { { 0, 1 }, { 1, 1 }, { 1, 15 }, { 0, 15 } } },
    { 0xC, 4, { { 1, 1 }, { 15, 1 }, { 15, 2 }, { 1, 2 } } },
    { 0xB, 4, { { 15, 1 }, { 16, 1 }, { 16, 15 }, { 15, 15 } } },
    { 0xC, 4, { { 16, 1 }, { 30, 1 }, { 30, 2 }, { 16, 2 } } },
    { 0xB, 4, { { 30, 1 }, { 31, 1 }, { 31, 31 }, { 30, 31 } } },
    { 0xC, 4, { { 1, 2 }, { 2, 2 }, { 2, 15 }, { 1, 15 } } },
    { 0xC, 4, { { 16, 2 }, { 17, 2 }, { 17, 15 }, { 16, 15 } } },
    { 0xE, 4, { { 0, 15 }, { 1, 15 }, { 1, 16 }, { 0, 16 } } },
    { 0xB, 4, { { 1, 15 }, { 14, 15 }, { 14, 16 }, { 1, 16 } } },
    { 0xC, 4, { { 14, 15 }, { 15, 15 }, { 15, 16 }, { 14, 16 } } },
    { 0xB, 4, { { 15, 15 }, { 30, 15 }, { 30, 16 }, { 15, 16 } } },
    { 0xB, 4, { { 0, 16 }, { 1, 16 }, { 1, 30 }, { 0, 30 } } },
    { 0xC, 4, { { 1, 16 }, { 30, 16 }, { 30, 17 }, { 1, 17 } } },
    { 0xC, 4, { { 1, 17 }, { 2, 17 }, { 2, 30 }, { 1, 30 } } },
    { 0xB, 4, { { 15, 17 }, { 16, 17 }, { 16, 30 }, { 15, 30 } } },
    { 0xC, 4, { { 16, 17 }, { 17, 17 }, { 17, 30 }, { 16, 30 } } },
    { 0xB, 4, { { 0, 30 }, { 31, 30 }, { 31, 31 }, { 0, 31 } } },
    { 0xC, 4, { { 0, 31 }, { 31, 31 }, { 31, 32 }, { 0, 32 } } },
    /* the chevron's white, above its point and below */
    { 0xE, 4, { { 5.5, 6 }, { 17.5, 6 }, { 27, 15.5 }, { 15, 15.5 } } },
    { 0xE, 4, { { 15, 15.5 }, { 27, 15.5 }, { 17.5, 25 }, { 5.5, 25 } } },
    /* its fill */
    { 0x2, 4, { { 8.5, 7 }, { 16.5, 7 }, { 25, 15.5 }, { 17, 15.5 } } },
    { 0x2, 4, { { 17, 15.5 }, { 25, 15.5 }, { 16.5, 24 }, { 8.5, 24 } } },
};

#define PATTERN(bank, base, mask, part, name, regions) \
    { (bank), (base), (mask), (part), (name), (int)(sizeof (regions) / sizeof (regions)[0]), (regions), 0, 0, 0, 0, 0 }

static FxPattern patterns[] = {
    PATTERN(0x12, 0xA040, 0x1F1F, PART_HIGH, "the yellow and black stripes", stripes),
    PATTERN(0x12, 0xE040, 0x1F1F, PART_HIGH, "the red chevron", chevron_red),
    PATTERN(0x12, 0xA080, 0x1F1F, PART_HIGH, "the blue chevron", chevron_blue),
    PATTERN(0x12, 0xE000, 0x1F1F, PART_LOW, "the chevron sign", sign),
};

/* The part of a convex polygon on the side a x + b y <= c. */
static int clip(const double (*in)[2], int n, double (*out)[2], double a, double b, double c)
{
    int i, m = 0;

    for (i = 0; i < n && m < POLY; i++) {
        const double *p = in[i], *q = in[(i + 1) % n];
        double dp = a * p[0] + b * p[1] - c, dq = a * q[0] + b * q[1] - c;

        if (dp <= 0) {
            out[m][0] = p[0];
            out[m][1] = p[1];
            m++;
        }
        if (((dp < 0 && dq > 0) || (dp > 0 && dq < 0)) && m < POLY) {
            double t = dp / (dp - dq);

            out[m][0] = p[0] + (q[0] - p[0]) * t;
            out[m][1] = p[1] + (q[1] - p[1]) * t;
            m++;
        }
    }
    return m;
}

static uint8_t tile_texel(const FxPattern *p, int x, int y)
{
    uint8_t v = fx_rom_read(p->bank, (uint16_t)(((uint16_t)(y << 8 | x) & p->mask) + p->base));

    return p->part == PART_HIGH ? (uint8_t)(v >> 4) : p->part == PART_LOW ? (uint8_t)(v & 0x0F) : v;
}

/* A point in a convex region, its edges counted in. */
static int inside(const Region *r, double x, double y)
{
    int k, side = 0;

    for (k = 0; k < r->n; k++) {
        const double *p = r->p[k], *q = r->p[(k + 1) % r->n];
        double cross = (q[0] - p[0]) * (y - p[1]) - (q[1] - p[1]) * (x - p[0]);

        if (fabs(cross) < 1e-9)
            continue;
        if (!side)
            side = cross > 0 ? 1 : -1;
        else if ((cross > 0) != (side > 0))
            return 0;
    }
    return 1;
}

/* The tile's every texel checked against the regions. */
static void make(FxPattern *p)
{
    int r, x, y, wrong = 0;

    p->made = 1;
    p->w = (p->mask & 0xFF) + 1;
    p->h = (p->mask >> 8) + 1;
    for (y = 0; y < p->h; y++)
        for (x = 0; x < p->w; x++) {
            uint8_t want = tile_texel(p, x, y);
            int shown = -1;

            for (r = 0; r < p->regions; r++)
                if (inside(&p->region[r], x + 0.5, y + 0.5))
                    shown = p->region[r].value;
            if (shown < 0) {
                p->holes = 1;
                wrong += want != 0;
            } else {
                wrong += shown != want;
            }
        }
    p->good = !wrong;
    if (wrong)
        fprintf(stderr, "stuntracefx: %s: %d of %d texels not in a region of their value; the texture is drawn as it is\n",
                p->name, wrong, p->w * p->h);
}

const FxPattern *fx_pattern_find(uint8_t bank, uint16_t base, uint16_t mask, uint8_t options)
{
    uint8_t part = (options & 0x04) ? PART_HIGH : (options & 0x08) ? PART_LOW : PART_BYTE;
    size_t i;

    for (i = 0; i < sizeof patterns / sizeof patterns[0]; i++) {
        FxPattern *p = &patterns[i];

        if (p->bank != bank || p->base != base || p->mask != mask || p->part != part)
            continue;
        if (!p->made)
            make(p);
        /* holes are see-through only where 0 is (options bit 0 clear) */
        return p->good && !(p->holes && (options & 0x01)) ? p : NULL;
    }
    return NULL;
}

int fx_pattern_cut(const FxPattern *pattern, const double (*xyz)[3], const double (*uv)[2], int n,
                   FxPatternPiece piece, void *context)
{
    int t, tiles = 0;

    if (n < 3)
        return 0;
    for (t = 1; t + 1 < n; t++) {
        const double *a = uv[0], *b = uv[t], *c = uv[t + 1];
        double det = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
        double u0 = fmin(a[0], fmin(b[0], c[0])), u1 = fmax(a[0], fmax(b[0], c[0]));
        double v0 = fmin(a[1], fmin(b[1], c[1])), v1 = fmax(a[1], fmax(b[1], c[1]));

        if (fabs(det) < 1e-9)
            return 0;
        tiles += ((int)floor(u1 / pattern->w) - (int)floor(u0 / pattern->w) + 1)
               * ((int)floor(v1 / pattern->h) - (int)floor(v0 / pattern->h) + 1);
        if (tiles > MOST_TILES)
            return 0;
    }
    for (t = 1; t + 1 < n; t++) {
        const double *ta[3] = { uv[0], uv[t], uv[t + 1] };
        const double *pa = xyz[0], *pb = xyz[t], *pc = xyz[t + 1];
        double du1 = ta[1][0] - ta[0][0], dv1 = ta[1][1] - ta[0][1];
        double du2 = ta[2][0] - ta[0][0], dv2 = ta[2][1] - ta[0][1];
        double det = du1 * dv2 - du2 * dv1, sign = det > 0 ? 1 : -1;
        double u0 = fmin(ta[0][0], fmin(ta[1][0], ta[2][0])), u1 = fmax(ta[0][0], fmax(ta[1][0], ta[2][0]));
        double v0 = fmin(ta[0][1], fmin(ta[1][1], ta[2][1])), v1 = fmax(ta[0][1], fmax(ta[1][1], ta[2][1]));
        double side[3][3];
        int i, j, e, r;

        /* the triangle's inside: left of each edge when its corners turn
           counter-clockwise in u, v, right when clockwise; a x + b y <= c */
        for (e = 0; e < 3; e++) {
            const double *p = ta[e], *q = ta[(e + 1) % 3];

            side[e][0] = sign * (q[1] - p[1]);
            side[e][1] = -sign * (q[0] - p[0]);
            side[e][2] = side[e][0] * p[0] + side[e][1] * p[1];
        }
        for (j = (int)floor(v0 / pattern->h); j <= (int)floor(v1 / pattern->h); j++)
            for (i = (int)floor(u0 / pattern->w); i <= (int)floor(u1 / pattern->w); i++)
                for (r = 0; r < pattern->regions; r++) {
                    const Region *g = &pattern->region[r];
                    double a[POLY][2], b[POLY][2], p[POLY][3], area = 0;
                    int m = g->n, k;

                    for (k = 0; k < m; k++) {
                        a[k][0] = g->p[k][0] + (double)i * pattern->w;
                        a[k][1] = g->p[k][1] + (double)j * pattern->h;
                    }
                    for (e = 0; e < 3 && m >= 3; e++) {
                        m = clip((const double (*)[2])a, m, b, side[e][0], side[e][1], side[e][2]);
                        memcpy(a, b, sizeof(double) * 2 * (size_t)m);
                    }
                    if (m < 3)
                        continue;
                    for (k = 0; k < m; k++)
                        area += a[k][0] * a[(k + 1) % m][1] - a[(k + 1) % m][0] * a[k][1];
                    if (fabs(area) < 1e-9)
                        continue;
                    /* each corner into view space by the triangle's own map */
                    for (k = 0; k < m; k++) {
                        double du = a[k][0] - ta[0][0], dv = a[k][1] - ta[0][1];
                        double s = (du * dv2 - dv * du2) / det, q = (du1 * dv - dv1 * du) / det;

                        p[k][0] = pa[0] + s * (pb[0] - pa[0]) + q * (pc[0] - pa[0]);
                        p[k][1] = pa[1] + s * (pb[1] - pa[1]) + q * (pc[1] - pa[1]);
                        p[k][2] = pa[2] + s * (pb[2] - pa[2]) + q * (pc[2] - pa[2]);
                    }
                    piece(context, (const double (*)[3])p, m, g->value);
                }
    }
    return 1;
}
