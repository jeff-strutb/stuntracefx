/*
 * fx_pattern.c: simple pattern textures drawn as geometry (plan stage 10,
 * "Pattern textures as geometry").
 *
 * A pattern is a texture whose texels are a few flat colours bounded by
 * straight edges, drawn as stairs at the chip's texel size. Each is given
 * here once, in its tile's texel space (a texel x, y covers [x, x+1) x
 * [y, y+1)), as convex regions bounded by the straight edges the stairs
 * stand for, each region showing one texel value. The table is checked
 * against the tile's every texel when first used: each texel's centre must
 * lie in a region of its own value, or the pattern is not used.
 *
 * A face with such a texture is laid with the regions instead: fanned from
 * its first corner, each triangle mapping texel coordinates to view space
 * by its own three corners, as the GPU lays the texture; each region of
 * each tile a triangle covers is clipped to it and handed on as a flat
 * piece in view space. Nothing here makes a pixel.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_pattern.h"

enum {
    BOUNDS = 4,                 /* a region's edges */
    REGIONS = 8,                /* a pattern's regions */
    POLY = 16,                  /* a clipped piece's corners: a square, two edges and a triangle's three add 5 */
    MOST_TILES = 64             /* a face over more tiles keeps its texture */
};

/* The side a x + b y <= c of an edge. */
typedef struct {
    double a, b, c;
} Edge;

typedef struct {
    uint8_t value;              /* the texel value it shows */
    int edges;
    Edge edge[BOUNDS];
} Region;

enum { PART_BYTE, PART_HIGH, PART_LOW };

struct FxPattern {
    uint8_t bank;
    uint16_t base, mask;
    uint8_t part;               /* which part of the byte the texture reads */
    const char *name;
    int regions;
    Region region[REGIONS];
    /* made when first used */
    int made, good, w, h;
    int corners[REGIONS];
    double poly[REGIONS][POLY][2];
};

/* The yellow and black stripes (the barriers' walls). A 32 x 32 tile of two
   values, $A and $F, in stripes 8 texels wide that move one texel left
   every 4 rows (the first and last steps 2 rows, the tile's middle the
   steps' middle). The edges through the middle of each step are
   X = 8k - Y / 4: $A where X + Y / 4 lies in [16k, 16k + 8), else $F. */
#define STRIPE(k, v) { (v), 2, { { -1, -0.25, -8.0 * (k) }, { 1, 0.25, 8.0 * (k) + 8 } } }

static FxPattern patterns[] = {
    { 0x12, 0xA040, 0x1F1F, PART_HIGH, "the yellow and black stripes", 5,
      { STRIPE(0, 0x0A), STRIPE(1, 0x0F), STRIPE(2, 0x0A), STRIPE(3, 0x0F), STRIPE(4, 0x0A) },
      0, 0, 0, 0, { 0 }, { { { 0 } } } },
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

static int inside(const Region *r, double x, double y)
{
    int k;

    for (k = 0; k < r->edges; k++)
        if (r->edge[k].a * x + r->edge[k].b * y > r->edge[k].c + 1e-9)
            return 0;
    return 1;
}

/* The regions cut to the tile, and the tile's every texel checked. */
static void make(FxPattern *p)
{
    int r, x, y, wrong = 0;

    p->made = 1;
    p->w = (p->mask & 0xFF) + 1;
    p->h = (p->mask >> 8) + 1;
    for (r = 0; r < p->regions; r++) {
        double a[POLY][2] = { { 0, 0 }, { p->w, 0 }, { p->w, p->h }, { 0, p->h } }, b[POLY][2];
        int n = 4, k;

        for (k = 0; k < p->region[r].edges; k++) {
            n = clip((const double (*)[2])a, n, b, p->region[r].edge[k].a, p->region[r].edge[k].b, p->region[r].edge[k].c);
            memcpy(a, b, sizeof(double) * 2 * (size_t)n);
        }
        p->corners[r] = n;
        memcpy(p->poly[r], a, sizeof(double) * 2 * (size_t)n);
    }
    for (y = 0; y < p->h; y++)
        for (x = 0; x < p->w; x++) {
            uint8_t want = tile_texel(p, x, y);
            int found = 0;

            for (r = 0; r < p->regions && !found; r++)
                found = p->region[r].value == want && inside(&p->region[r], x + 0.5, y + 0.5);
            wrong += !found;
        }
    p->good = !wrong;
    if (wrong)
        fprintf(stderr, "stuntracefx: %s: %d of %d texels outside their regions; the texture is drawn as it is\n",
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
        return p->good ? p : NULL;
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
        Edge side[3];
        int i, j, e, r;

        /* the triangle's inside: left of each edge when its corners turn
           counter-clockwise in u, v, right when clockwise */
        for (e = 0; e < 3; e++) {
            const double *p = ta[e], *q = ta[(e + 1) % 3];

            side[e].a = sign * (q[1] - p[1]);
            side[e].b = -sign * (q[0] - p[0]);
            side[e].c = side[e].a * p[0] + side[e].b * p[1];
        }
        for (j = (int)floor(v0 / pattern->h); j <= (int)floor(v1 / pattern->h); j++)
            for (i = (int)floor(u0 / pattern->w); i <= (int)floor(u1 / pattern->w); i++)
                for (r = 0; r < pattern->regions; r++) {
                    double a[POLY][2], b[POLY][2], p[POLY][3], area = 0;
                    int m = pattern->corners[r], k;

                    for (k = 0; k < m; k++) {
                        a[k][0] = pattern->poly[r][k][0] + (double)i * pattern->w;
                        a[k][1] = pattern->poly[r][k][1] + (double)j * pattern->h;
                    }
                    for (e = 0; e < 3 && m >= 3; e++) {
                        m = clip((const double (*)[2])a, m, b, side[e].a, side[e].b, side[e].c);
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
                    piece(context, (const double (*)[3])p, m, pattern->region[r].value);
                }
    }
    return 1;
}
