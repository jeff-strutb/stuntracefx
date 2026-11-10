/*
 * pad_art.c: the Super Famicom controller of the pause screen (pause_art.h
 * pad_draw), drawn from Jeff's vector drawing of it (an Inkscape SVG, 1000 x
 * 600, kept beside this file as SNES_controller.svg) exactly as the drawing
 * gives it: the body's, the shoulder buttons', the D-pad's, its arrows' and
 * its dimple's outlines as its path data, verbatim; its circles and rounded
 * rectangles, every element's transform, its fills and strokes (colours,
 * linear and radial gradients with their stops and transforms, stroke
 * widths, opacities) as its numbers; in its order. Its two text layers'
 * letters (SELECT, START, the face buttons' A B X Y) are the pause screen's
 * 5 x 7 capitals at the drawing's places and sizes, the labels sheared as
 * its italics are.
 *
 * Each shape is drawn by its signed distance at each pixel (inside < 0),
 * the drawing's own coordinates of the pixel's centre found through the
 * element's transform; a path's outline is its curves cut into straight
 * pieces, inside by the even-odd rule. An edge is blended across one pixel.
 *
 * This is the panel's art, drawn once into the image the GPU shows
 * (pause.c); native/checks/pause_check holds it against rsvg-convert's
 * drawing of the same file.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "pause/pause_art.h"

/* ---- the picture being drawn: ARGB, straight alpha ---- */
typedef struct {
    uint32_t *px;
    int w, h;
    float density, k, cx, cy;       /* pixels a point; points a drawing pixel; where (500, 300) goes */
} Canvas;

static void blend(Canvas *m, int x, int y, uint32_t c, float a)
{
    uint32_t *p = &m->px[y * m->w + x], d = *p;
    float da = (d >> 24) / 255.0f, oa;
    int k;
    uint32_t out = 0;
    if (a <= 0)
        return;
    if (a > 1)
        a = 1;
    oa = a + da * (1 - a);
    for (k = 0; k < 3; k++) {
        float s = (c >> (16 - 8 * k) & 0xFF), t = (d >> (16 - 8 * k) & 0xFF);
        float v = oa > 0 ? (s * a + t * da * (1 - a)) / oa : 0;
        out |= (uint32_t)(v + 0.5f) << (16 - 8 * k);
    }
    *p = out | (uint32_t)(oa * 255 + 0.5f) << 24;
}

static float cover(float d) { return d < -0.5f ? 1 : d > 0.5f ? 0 : 0.5f - d; }

/* ---- transforms: SVG's matrix(a, b, c, d, e, f) ---- */
typedef struct {
    double a, b, c, d, e, f;
} Xf;

static const Xf k_id = { 1, 0, 0, 1, 0, 0 };

static Xf xf_mul(Xf p, Xf q)            /* p after q */
{
    Xf r;
    r.a = p.a * q.a + p.c * q.b;
    r.b = p.b * q.a + p.d * q.b;
    r.c = p.a * q.c + p.c * q.d;
    r.d = p.b * q.c + p.d * q.d;
    r.e = p.a * q.e + p.c * q.f + p.e;
    r.f = p.b * q.e + p.d * q.f + p.f;
    return r;
}

static Xf xf_inv(Xf m)
{
    double det = m.a * m.d - m.b * m.c;
    Xf r;
    r.a = m.d / det;
    r.b = -m.b / det;
    r.c = -m.c / det;
    r.d = m.a / det;
    r.e = -(r.a * m.e + r.c * m.f);
    r.f = -(r.b * m.e + r.d * m.f);
    return r;
}

static void xf_apply(const Xf *m, double x, double y, double *ox, double *oy)
{
    *ox = m->a * x + m->c * y + m->e;
    *oy = m->b * x + m->d * y + m->f;
}

static Xf xlate(double x, double y)
{
    Xf r = { 1, 0, 0, 1, x, y };
    return r;
}

static Xf mtx(double a, double b, double c, double d, double e, double f)
{
    Xf r = { a, b, c, d, e, f };
    return r;
}

/* ---- paint: a colour, or a gradient in the element's own coordinates ---- */
typedef struct {
    float at;
    uint32_t c;
    float alpha;
} Stop;

enum { FLAT, LINEAR, RADIAL };
typedef struct {
    int kind;
    uint32_t c;                     /* FLAT */
    float alpha;                    /* the paint's opacity */
    double x1, y1, x2, y2;          /* LINEAR */
    double cx, cy, r;               /* RADIAL */
    Xf gt;                          /* gradientTransform */
    const Stop *stops;
    int nstops;
} Paint;

static Paint flat(uint32_t c)
{
    Paint p;
    memset(&p, 0, sizeof p);
    p.kind = FLAT;
    p.c = c;
    p.alpha = 1;
    return p;
}

#define NSTOPS(s) ((int)(sizeof(s) / sizeof((s)[0])))

static Paint linear(double x1, double y1, double x2, double y2, Xf gt, const Stop *s, int n)
{
    Paint p = flat(0);
    p.kind = LINEAR;
    p.x1 = x1, p.y1 = y1, p.x2 = x2, p.y2 = y2;
    p.gt = gt;
    p.stops = s;
    p.nstops = n;
    return p;
}

static Paint radial(double cx, double cy, double r, Xf gt, const Stop *s, int n)
{
    Paint p = flat(0);
    p.kind = RADIAL;
    p.cx = cx, p.cy = cy, p.r = r;
    p.gt = gt;
    p.stops = s;
    p.nstops = n;
    return p;
}

/* a gradient at t (padded past its ends), as colour and alpha */
static uint32_t stops_at(const Stop *s, int n, float t, float *alpha)
{
    int i, k;
    uint32_t out = 0;
    if (t <= s[0].at) {
        *alpha = s[0].alpha;
        return s[0].c;
    }
    for (i = 1; i < n; i++)
        if (t <= s[i].at) {
            float span = s[i].at - s[i - 1].at, f = span > 0 ? (t - s[i - 1].at) / span : 1;
            for (k = 0; k < 3; k++) {
                float a = (float)(s[i - 1].c >> (16 - 8 * k) & 0xFF), b = (float)(s[i].c >> (16 - 8 * k) & 0xFF);
                out |= (uint32_t)(a + (b - a) * f + 0.5f) << (16 - 8 * k);
            }
            *alpha = s[i - 1].alpha + (s[i].alpha - s[i - 1].alpha) * f;
            return out;
        }
    *alpha = s[n - 1].alpha;
    return s[n - 1].c;
}

static uint32_t paint_at(const Paint *p, double x, double y, float *alpha)
{
    double gx, gy;
    float t, a;
    uint32_t c;
    if (p->kind == FLAT) {
        *alpha = p->alpha;
        return p->c;
    }
    {
        Xf inv = xf_inv(p->gt);
        xf_apply(&inv, x, y, &gx, &gy);
    }
    if (p->kind == LINEAR) {
        double vx = p->x2 - p->x1, vy = p->y2 - p->y1;
        t = (float)(((gx - p->x1) * vx + (gy - p->y1) * vy) / (vx * vx + vy * vy));
    } else {
        t = (float)(hypot(gx - p->cx, gy - p->cy) / p->r);
    }
    c = stops_at(p->stops, p->nstops, t, &a);
    *alpha = a * p->alpha;
    return c;
}

/* ---- shapes in their element's coordinates ---- */
enum { CIRCLE, RRECT, POLY };
typedef struct {
    int kind;
    double cx, cy, r;               /* CIRCLE */
    double x, y, w, h, rx, ry;      /* RRECT: its corners' radii as SVG takes them */
    double *pts;                    /* POLY: x, y pairs, closed */
    int npts;
    double bx0, by0, bx1, by1;      /* its bounds */
} Shape;

static double d_shape(const Shape *s, double x, double y)
{
    if (s->kind == CIRCLE)
        return hypot(x - s->cx, y - s->cy) - s->r;
    if (s->kind == RRECT) {
        /* elliptical corners: the rectangle squeezed down by rx / ry, its
           distance a circle's, brought back by the smaller of the two */
        double q = s->ry > 0 ? s->rx / s->ry : 1, r = s->rx;
        double qx = fabs(x - (s->x + s->w / 2)) - (s->w / 2 - r), qy = fabs(y - (s->y + s->h / 2)) * q - (s->h * q / 2 - r);
        return (hypot(fmax(qx, 0), fmax(qy, 0)) + fmin(fmax(qx, qy), 0) - r) * (q > 1 ? 1 / q : 1);
    }
    {
        double best = 1e30;
        int i, in = 0;
        for (i = 0; i < s->npts; i++) {
            const double *a = s->pts + 2 * i, *b = s->pts + 2 * ((i + 1) % s->npts);
            double vx = b[0] - a[0], vy = b[1] - a[1], l = vx * vx + vy * vy, t, dx, dy;
            t = l > 0 ? ((x - a[0]) * vx + (y - a[1]) * vy) / l : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            dx = x - a[0] - t * vx;
            dy = y - a[1] - t * vy;
            if (dx * dx + dy * dy < best)
                best = dx * dx + dy * dy;
            if ((a[1] > y) != (b[1] > y) && x < a[0] + (y - a[1]) * vx / vy)
                in = !in;
        }
        return in ? -sqrt(best) : sqrt(best);
    }
}

/* SVG path data (absolute M, L, C, Z, as these paths use) cut into straight
   pieces, a curve into as many as keep each within FLAT of it: a cubic cut
   into n even pieces strays at most 3/4 L / n^2 from them, L the larger of
   |P0 - 2 P1 + P2| and |P1 - 2 P2 + P3| */
#define FLAT 0.01                       /* drawing pixels */
#define MAX_CUTS 64

static int curve_cuts(double x0, double y0, const double v[6])
{
    double l = fmax(hypot(x0 - 2 * v[0] + v[2], y0 - 2 * v[1] + v[3]),
                    hypot(v[0] - 2 * v[2] + v[4], v[1] - 2 * v[3] + v[5]));
    int n = (int)ceil(sqrt(0.75 * l / FLAT));
    return n < 1 ? 1 : n > MAX_CUTS ? MAX_CUTS : n;
}

static Shape path_shape(const char *d)
{
    Shape s;
    int cap = 64, n = 0, i;
    double cx = 0, cy = 0, v[6];
    char cmd = 0;
    memset(&s, 0, sizeof s);
    s.kind = POLY;
    s.pts = (double *)malloc((size_t)cap * 2 * sizeof *s.pts);
    if (!s.pts)
        return s;                       /* no memory: nothing drawn */
    while (*d) {
        int need, cuts;
        while (*d == ' ' || *d == ',' || *d == '\n')
            d++;
        if (!*d)
            break;
        if ((*d >= 'A' && *d <= 'Z') || (*d >= 'a' && *d <= 'z')) {
            cmd = *d++;
            if (cmd == 'z' || cmd == 'Z')
                continue;
            continue;
        }
        need = cmd == 'C' ? 6 : 2;
        for (i = 0; i < need; i++) {
            char *e;
            while (*d == ' ' || *d == ',')
                d++;
            v[i] = strtod(d, &e);
            d = e;
        }
        cuts = cmd == 'C' ? curve_cuts(cx, cy, v) : 1;
        if (n + cuts + 1 > cap) {
            double *grown = (double *)realloc(s.pts, (size_t)(cap * 2 + MAX_CUTS) * 2 * sizeof *s.pts);
            if (!grown)
                break;                  /* no memory: the outline so far */
            cap = cap * 2 + MAX_CUTS;
            s.pts = grown;
        }
        if (cmd == 'C') {
            int k;
            for (k = 1; k <= cuts; k++) {
                double t = (double)k / cuts, u = 1 - t;
                s.pts[2 * n] = u * u * u * cx + 3 * u * u * t * v[0] + 3 * u * t * t * v[2] + t * t * t * v[4];
                s.pts[2 * n + 1] = u * u * u * cy + 3 * u * u * t * v[1] + 3 * u * t * t * v[3] + t * t * t * v[5];
                n++;
            }
            cx = v[4];
            cy = v[5];
        } else {                        /* M or L (an M's further pairs are L's) */
            s.pts[2 * n] = cx = v[0];
            s.pts[2 * n + 1] = cy = v[1];
            n++;
            if (cmd == 'M')
                cmd = 'L';
        }
    }
    s.npts = n;
    s.bx0 = s.by0 = 1e30;
    s.bx1 = s.by1 = -1e30;
    for (i = 0; i < n; i++) {
        s.bx0 = fmin(s.bx0, s.pts[2 * i]);
        s.bx1 = fmax(s.bx1, s.pts[2 * i]);
        s.by0 = fmin(s.by0, s.pts[2 * i + 1]);
        s.by1 = fmax(s.by1, s.pts[2 * i + 1]);
    }
    return s;
}

static Shape circle(double cx, double cy, double r)
{
    Shape s;
    memset(&s, 0, sizeof s);
    s.kind = CIRCLE;
    s.cx = cx, s.cy = cy, s.r = r;
    s.bx0 = cx - r, s.by0 = cy - r, s.bx1 = cx + r, s.by1 = cy + r;
    return s;
}

/* SVG's rect: rx and ry each at most half its side, a missing ry (< 0)
   rx's (taken before the halving) */
static Shape rrect2(double x, double y, double w, double h, double rx, double ry)
{
    Shape s;
    memset(&s, 0, sizeof s);
    s.kind = RRECT;
    if (ry < 0)
        ry = rx;
    s.x = x, s.y = y, s.w = w, s.h = h;
    s.rx = fmin(rx, w / 2);
    s.ry = fmin(ry, h / 2);
    s.bx0 = x, s.by0 = y, s.bx1 = x + w, s.by1 = y + h;
    return s;
}

static Shape rrect(double x, double y, double w, double h, double rx) { return rrect2(x, y, w, h, rx, -1); }

/*
 * A path's pieces by the cells of a grid over it, each cell listing the
 * pieces that come within `reach` of it, so that a pixel is measured against
 * those alone. Past `reach` (half the stroke and a pixel) every coverage is
 * whole or none, so the pixels come out as from measuring every piece. A
 * cell with no piece near it lies wholly inside or wholly outside the path,
 * known from its centre.
 */
typedef struct {
    double x0, y0, cs;                  /* the grid's corner and its cells' side, element units */
    int gw, gh;
    int *first;                         /* each cell's pieces: list[first[c]] to list[first[c + 1]] */
    int *list;
    signed char *inside;                /* a cell with no piece: 1 inside, 0 outside, -1 not yet known */
} Near;

static int poly_inside(const Shape *s, double x, double y)
{
    int i, in = 0;
    for (i = 0; i < s->npts; i++) {
        const double *a = s->pts + 2 * i, *b = s->pts + 2 * ((i + 1) % s->npts);
        double vx = b[0] - a[0], vy = b[1] - a[1];
        if ((a[1] > y) != (b[1] > y) && x < a[0] + (y - a[1]) * vx / vy)
            in = !in;
    }
    return in;
}

static double piece_dist2(const Shape *s, int i, double x, double y)
{
    const double *a = s->pts + 2 * i, *b = s->pts + 2 * ((i + 1) % s->npts);
    double vx = b[0] - a[0], vy = b[1] - a[1], l = vx * vx + vy * vy, t, dx, dy;
    t = l > 0 ? ((x - a[0]) * vx + (y - a[1]) * vy) / l : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    dx = x - a[0] - t * vx;
    dy = y - a[1] - t * vy;
    return dx * dx + dy * dy;
}

static void near_free(Near *g)
{
    free(g->first);
    free(g->list);
    free(g->inside);
    memset(g, 0, sizeof *g);
}

/* the piece's cells: those its bounds and `reach` touch */
static void piece_cells(const Near *g, const Shape *s, int i, double reach, int *c0, int *r0, int *c1, int *r1)
{
    const double *a = s->pts + 2 * i, *b = s->pts + 2 * ((i + 1) % s->npts);
    *c0 = (int)floor((fmin(a[0], b[0]) - reach - g->x0) / g->cs);
    *c1 = (int)floor((fmax(a[0], b[0]) + reach - g->x0) / g->cs);
    *r0 = (int)floor((fmin(a[1], b[1]) - reach - g->y0) / g->cs);
    *r1 = (int)floor((fmax(a[1], b[1]) + reach - g->y0) / g->cs);
    *c0 = *c0 < 0 ? 0 : *c0;
    *r0 = *r0 < 0 ? 0 : *r0;
    *c1 = *c1 >= g->gw ? g->gw - 1 : *c1;
    *r1 = *r1 >= g->gh ? g->gh - 1 : *r1;
}

/* 0: no memory (the caller measures every piece) */
static int near_make(Near *g, const Shape *s, double reach)
{
    double bw = s->bx1 - s->bx0 + 2 * reach, bh = s->by1 - s->by0 + 2 * reach;
    int i, n, c0, r0, c1, r1, c, r, total;
    int *at;

    memset(g, 0, sizeof *g);
    if (s->npts < 2)
        return 0;
    g->cs = fmax(fmax(bw, bh) / 128, reach);
    g->x0 = s->bx0 - reach;
    g->y0 = s->by0 - reach;
    g->gw = (int)(bw / g->cs) + 1;
    g->gh = (int)(bh / g->cs) + 1;
    n = g->gw * g->gh;
    g->first = (int *)calloc((size_t)n + 1, sizeof *g->first);
    g->inside = (signed char *)malloc((size_t)n);
    at = (int *)malloc((size_t)n * sizeof *at);
    if (!g->first || !g->inside || !at) {
        free(at);
        near_free(g);
        return 0;
    }
    memset(g->inside, -1, (size_t)n);
    for (i = 0; i < s->npts; i++) {
        piece_cells(g, s, i, reach, &c0, &r0, &c1, &r1);
        for (r = r0; r <= r1; r++)
            for (c = c0; c <= c1; c++)
                g->first[r * g->gw + c + 1]++;
    }
    for (c = 0; c < n; c++)
        g->first[c + 1] += g->first[c];
    total = g->first[n];
    g->list = (int *)malloc((size_t)(total ? total : 1) * sizeof *g->list);
    if (!g->list) {
        free(at);
        near_free(g);
        return 0;
    }
    memcpy(at, g->first, (size_t)n * sizeof *at);
    for (i = 0; i < s->npts; i++) {
        piece_cells(g, s, i, reach, &c0, &r0, &c1, &r1);
        for (r = r0; r <= r1; r++)
            for (c = c0; c <= c1; c++)
                g->list[at[r * g->gw + c]++] = i;
    }
    free(at);
    return 1;
}

/* the signed distance (inside < 0), exact within `reach`, beyond it at
   least `reach` with its sign */
static double near_distance(Near *g, const Shape *s, double reach, double x, double y)
{
    int c = (int)floor((x - g->x0) / g->cs), r = (int)floor((y - g->y0) / g->cs), k, cell;
    double best;

    if (c < 0 || r < 0 || c >= g->gw || r >= g->gh)
        return 2 * reach;               /* past every piece's reach, outside the bounds */
    cell = r * g->gw + c;
    if (g->first[cell] == g->first[cell + 1]) {
        if (g->inside[cell] < 0)
            g->inside[cell] = (signed char)poly_inside(s, g->x0 + (c + 0.5) * g->cs, g->y0 + (r + 0.5) * g->cs);
        return g->inside[cell] ? -2 * reach : 2 * reach;
    }
    best = 1e30;
    for (k = g->first[cell]; k < g->first[cell + 1]; k++) {
        double d2 = piece_dist2(s, g->list[k], x, y);
        if (d2 < best)
            best = d2;
    }
    best = sqrt(best);
    return poly_inside(s, x, y) ? -best : best;
}

/* the element drawn: its shape under transform t, filled (fill NULL:
   none) then stroked (stroke NULL: none) `sw` wide, as SVG paints them */
static void draw(Canvas *m, const Shape *s, Xf t, const Paint *fill, const Paint *stroke, double sw)
{
    /* the drawing's pixel to the canvas's: p = (X - 500) * k + cx, then
       density; and back */
    Xf to_px = mtx(m->k * m->density, 0, 0, m->k * m->density, (m->cx - 500 * m->k) * m->density,
                   (m->cy - 300 * m->k) * m->density);
    Xf whole = xf_mul(to_px, t), back = xf_inv(whole);
    double scale = sqrt(fabs(whole.a * whole.d - whole.b * whole.c)), pad = sw / 2 + 2 / scale;
    double cx[4] = { s->bx0 - pad, s->bx1 + pad, s->bx0 - pad, s->bx1 + pad };
    double cy[4] = { s->by0 - pad, s->by0 - pad, s->by1 + pad, s->by1 + pad };
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    double reach = (stroke && sw > 0 ? sw / 2 : 0) + 1 / scale;
    Near near;
    int i, x, y, grid = s->kind == POLY && near_make(&near, s, reach);
    for (i = 0; i < 4; i++) {
        double px, py;
        xf_apply(&whole, cx[i], cy[i], &px, &py);
        x0 = fmin(x0, px), y0 = fmin(y0, py), x1 = fmax(x1, px), y1 = fmax(y1, py);
    }
    x0 = fmax(0, floor(x0)), y0 = fmax(0, floor(y0));
    x1 = fmin(m->w, ceil(x1)), y1 = fmin(m->h, ceil(y1));
    for (y = (int)y0; y < (int)y1; y++)
        for (x = (int)x0; x < (int)x1; x++) {
            double lx, ly, d;
            float a;
            uint32_t c;
            xf_apply(&back, x + 0.5, y + 0.5, &lx, &ly);
            d = (grid ? near_distance(&near, s, reach, lx, ly) : d_shape(s, lx, ly)) * scale;     /* canvas pixels */
            if (fill) {
                c = paint_at(fill, lx, ly, &a);
                blend(m, x, y, c, a * cover((float)d));
            }
            if (stroke && sw > 0) {
                c = paint_at(stroke, lx, ly, &a);
                blend(m, x, y, c, a * cover((float)(fabs(d) - sw * scale / 2)));
            }
        }
    if (grid)
        near_free(&near);
}

/* ---- the drawing's data ---- */
/* the body (path2383) */
static const char k_body[] =
    "M 716.44579,130.79902 C 704.94404,130.80117 250.01492,130.91308 246.27921,131.13652 C 242.40615,131.36817 238.56416,131.71362 234.80421,132.18652 C 230.96675,132.66917 227.21036,133.28642 223.47921,134.02402 C 219.70058,134.771 215.94833,135.63678 212.26671,136.64902 C 208.56854,137.66581 204.92927,138.81072 201.31671,140.09902 C 197.71972,141.38177 194.15347,142.81826 190.62921,144.37402 C 187.15029,145.90976 183.69898,147.59355 180.27921,149.39902 C 176.92814,151.16821 173.60957,153.06734 170.30421,155.09902 C 167.08501,157.07775 163.89034,159.16807 160.70421,161.39902 C 157.52493,163.62517 154.45753,165.93659 151.47921,168.33652 C 148.4599,170.76949 145.51485,173.27451 142.70421,175.87402 C 139.86043,178.50417 137.15717,181.22336 134.52921,184.01152 C 131.87477,186.82778 129.3112,189.74692 126.87921,192.71152 C 124.42635,195.70157 122.09096,198.73379 119.86671,201.86152 C 117.62684,205.01121 115.49736,208.26071 113.49171,211.53652 C 111.47495,214.83046 109.56923,218.1788 107.79171,221.58652 C 106.00684,225.00834 104.3454,228.48892 102.80421,232.01152 C 101.258,235.5456 99.865169,239.11505 98.566709,242.73652 C 97.265409,246.36592 96.054179,250.02101 95.004209,253.72402 C 93.952929,257.43165 93.063519,261.16935 92.266709,264.93652 C 91.469599,268.70513 90.781639,272.52256 90.241709,276.33652 C 89.701909,280.14953 89.284449,283.96742 89.004209,287.81152 C 88.724359,291.65037 88.572699,295.50587 88.554209,299.36152 C 88.535759,303.20873 88.647459,307.06167 88.891709,310.91152 C 89.135199,314.74941 89.509669,318.59752 90.016709,322.42402 C 90.521479,326.23344 91.160819,330.00218 91.929209,333.78652 C 92.693449,337.5504 93.601789,341.31102 94.629209,345.03652 C 95.649569,348.73641 96.796929,352.41282 98.079209,356.06152 C 99.350799,359.6798 100.74776,363.26906 102.27921,366.82402 C 103.79513,370.34289 105.45644,373.84233 107.22921,377.28652 C 108.98129,380.69052 110.8491,384.05518 112.85421,387.37402 C 114.83186,390.64742 116.92834,393.87017 119.15421,397.04902 C 121.38036,400.2283 123.69178,403.29572 126.09171,406.27402 C 128.52468,409.29333 131.02969,412.23838 133.62921,415.04902 C 136.25936,417.8928 138.97855,420.59604 141.76671,423.22402 C 144.58297,425.87846 147.50211,428.44204 150.46671,430.87402 C 153.45676,433.32688 156.48898,435.66227 159.61671,437.88652 C 162.7664,440.12639 166.01591,442.25586 169.29171,444.26152 C 172.58565,446.27828 175.93399,448.184 179.34171,449.96152 C 182.76353,451.7464 186.24411,453.40784 189.76671,454.94902 C 193.30079,456.49523 196.87024,457.88805 200.49171,459.18652 C 204.12111,460.48782 207.7762,461.69906 211.47921,462.74902 C 215.18684,463.8003 218.92454,464.68971 222.69171,465.48652 C 226.46032,466.28362 230.27775,466.97159 234.09171,467.51152 C 237.90472,468.05132 241.72261,468.46877 245.56671,468.74902 C 249.40556,469.02887 253.26106,469.18053 257.11671,469.19902 C 260.96392,469.21746 264.81686,469.10577 268.66671,468.86152 C 272.5046,468.61803 276.35271,468.24357 280.17921,467.73652 C 283.98863,467.23175 287.75737,466.59242 291.54171,465.82402 C 295.30559,465.05979 299.06621,464.15144 302.79171,463.12402 C 306.4916,462.10367 310.16801,460.95629 313.81671,459.67402 C 317.43499,458.40243 321.02427,457.0055 324.57921,455.47402 C 328.09808,453.95811 331.59752,452.29679 335.04171,450.52402 C 338.44571,448.77195 341.81037,446.90415 345.12921,444.89902 C 346.7659,443.9102 351.6329,439.91632 354.80421,439.46152 C 358.01787,439.00065 638.75136,437.62832 642.00829,438.59902 L 645.56702,439.65968 C 647.55654,440.25264 651.59109,442.95916 654.87079,444.89902 C 658.20824,446.87304 661.55429,448.77195 664.95829,450.52402 C 668.40246,452.29679 671.90192,453.95811 675.42079,455.47402 C 678.97574,457.0055 682.56501,458.40243 686.18329,459.67402 C 689.83197,460.95629 693.5084,462.10367 697.20829,463.12402 C 700.93379,464.15144 704.65691,465.05979 708.42079,465.82402 C 712.20512,466.59242 716.01137,467.23175 719.82079,467.73652 C 723.64729,468.24357 727.4954,468.61803 731.33329,468.86152 C 735.18317,469.10577 739.03608,469.21746 742.88329,469.19902 C 746.73895,469.18053 750.59444,469.02887 754.43329,468.74902 C 758.27738,468.46877 762.09528,468.05132 765.90829,467.51152 C 769.72229,466.97159 773.50218,466.28362 777.27079,465.48652 C 781.038,464.68971 784.81316,463.8003 788.52079,462.74902 C 792.22379,461.69906 795.87889,460.48782 799.50829,459.18652 C 803.12975,457.88805 806.69921,456.49523 810.23329,454.94902 C 813.75589,453.40784 817.23647,451.7464 820.65829,449.96152 C 824.06604,448.184 827.41435,446.27828 830.70829,444.26152 C 833.98406,442.25586 837.1961,440.12639 840.34579,437.88652 C 843.47351,435.66227 846.54324,433.32688 849.53329,430.87402 C 852.49791,428.44204 855.41703,425.87846 858.23329,423.22402 C 861.02148,420.59604 863.70314,417.8928 866.33329,415.04902 C 868.93284,412.23838 871.47533,409.29333 873.90829,406.27402 C 876.30822,403.29572 878.61963,400.2283 880.84579,397.04902 C 883.07163,393.87017 885.16814,390.64742 887.14579,387.37402 C 889.15088,384.05518 891.01872,380.69052 892.77079,377.28652 C 894.54353,373.84233 896.20487,370.34289 897.72079,366.82402 C 899.25228,363.26906 900.6492,359.6798 901.92079,356.06152 C 903.20307,352.41282 904.35043,348.73641 905.37079,345.03652 C 906.39822,341.31102 907.30655,337.5504 908.07079,333.78652 C 908.8392,330.00218 909.47852,326.23344 909.98329,322.42402 C 910.49034,318.59752 910.8648,314.74941 911.10829,310.91152 C 911.35251,307.06167 911.46425,303.20873 911.44579,299.36152 C 911.42733,295.50587 911.27564,291.65037 910.99579,287.81152 C 910.71555,283.96742 910.29809,280.14953 909.75829,276.33652 C 909.21837,272.52256 908.5304,268.70513 907.73329,264.93652 C 906.93648,261.16935 906.00957,257.43165 904.95829,253.72402 C 903.90829,250.02101 902.73459,246.36592 901.43329,242.73652 C 900.13483,239.11505 898.7045,235.5456 897.15829,232.01152 C 895.61707,228.48892 893.95566,225.00834 892.17079,221.58652 C 890.3933,218.1788 888.52505,214.83046 886.50829,211.53652 C 884.50267,208.26071 882.37315,205.01121 880.13329,201.86152 C 877.90904,198.73379 875.57365,195.70157 873.12079,192.71152 C 870.68879,189.74692 868.12523,186.82778 865.47079,184.01152 C 862.84283,181.22336 860.10207,178.50417 857.25829,175.87402 C 854.44766,173.27451 851.5401,170.76949 848.52079,168.33652 C 845.5425,165.93659 842.47507,163.62517 839.29579,161.39902 C 836.10965,159.16807 832.91499,157.07775 829.69579,155.09902 C 826.39044,153.06734 823.03436,151.16821 819.68329,149.39902 C 816.26348,147.59355 812.84971,145.90976 809.37079,144.37402 C 805.84655,142.81826 802.28028,141.38177 798.68329,140.09902 C 795.07075,138.81072 791.39396,137.66581 787.69579,136.64902 C 784.01419,135.63678 780.29942,134.771 776.52079,134.02402 C 772.78961,133.28642 768.99575,132.66917 765.15829,132.18652 C 761.39838,131.71362 757.59385,131.36817 753.72079,131.13652 C 750.35378,130.93514 716.86367,130.79902 716.44579,130.79902 z";
/* the left shoulder button (path4756); the right is it mirrored (path4853) */
static const char k_shoulder[] =
    "M 145.19609,174.21869 C 141.42278,173.49838 140.93022,170.35988 141.23842,165.35798 C 141.64992,158.67946 142.37797,157.39075 143.93816,155.28302 C 146.73104,151.51004 151.24723,147.83439 154.10771,145.71651 C 156.92935,143.62741 159.83538,141.61534 162.84742,139.67751 C 165.86594,137.73549 168.89255,135.91589 171.9424,134.19343 C 175.07388,132.42488 178.21787,130.77171 181.39265,129.23166 C 184.63252,127.66002 187.90227,126.1943 191.19818,124.85745 C 194.53705,123.50319 197.91569,122.25274 201.32346,121.13612 C 204.74597,120.01467 208.1938,119.01804 211.69742,118.13294 C 215.18537,117.2518 218.74023,116.49814 222.32008,115.84791 C 225.85495,115.20584 229.41373,114.66853 233.04932,114.24839 C 236.61147,113.83673 240.25135,113.53602 243.92066,113.33437 C 245.69026,113.23712 315.53648,113.11525 339.86949,113.11525 C 345.9525,114.49025 352.23994,124.25979 347.55579,130.88479 C 321.87164,130.88479 248.14707,131.0248 246.27921,131.13652 C 242.40615,131.36817 238.56416,131.71362 234.80421,132.18652 C 230.96675,132.66917 227.21036,133.28642 223.47921,134.02402 C 219.70058,134.771 215.94833,135.63678 212.26671,136.64902 C 208.56854,137.66581 204.92927,138.81072 201.31671,140.09902 C 197.71972,141.38177 194.15347,142.81826 190.62921,144.37402 C 187.15029,145.90976 183.69898,147.59355 180.27921,149.39902 C 176.92814,151.16821 173.60957,153.06734 170.30421,155.09902 C 167.08501,157.07775 163.89034,159.16807 160.70421,161.39902 C 154.89041,165.71706 150.62519,169.41383 145.19609,174.21869 z";
/* the D-pad's cross (rect3221) and its top (path3319) */
static const char k_cross[] =
    "M 230.57544,224.95464 C 226.12097,224.95464 222.53488,228.54074 222.53488,232.99522 L 222.53488,275.20824 L 180.32186,275.20824 C 175.86738,275.20824 172.28128,278.79433 172.28128,283.2488 L 172.28128,316.7512 C 172.28128,321.20567 175.86738,324.79176 180.32186,324.79176 L 222.53488,324.79176 L 222.53488,367.00478 C 222.53488,371.45926 226.12097,375.04536 230.57544,375.04536 L 264.07784,375.04536 C 268.53231,375.04536 272.1184,371.45926 272.1184,367.00478 L 272.1184,324.79176 L 314.33142,324.79176 C 318.7859,324.79176 322.372,321.20567 322.372,316.7512 L 322.372,283.2488 C 322.372,278.79433 318.7859,275.20824 314.33142,275.20824 L 272.1184,275.20824 L 272.1184,232.99522 C 272.1184,228.54074 268.53231,224.95464 264.07784,224.95464 L 230.57544,224.95464 z";
static const char k_cross_top[] =
    "M 233.57956,231.91492 C 230.05118,231.91492 227.21068,235.12897 227.21068,239.12136 L 227.21068,279.69459 L 185.83605,279.69459 C 182.3077,279.69459 179.46717,282.90864 179.46717,286.90101 L 179.46717,313.06028 C 179.46717,317.05264 182.3077,320.26669 185.83605,320.26669 L 227.21068,320.26669 L 227.21068,361.06555 C 227.21068,365.05793 230.05118,368.272 233.57956,368.272 L 260.43559,368.272 C 263.96396,368.272 266.80446,365.05793 266.80446,361.06555 L 266.80446,320.26669 L 308.98812,320.26669 C 312.51646,320.26669 315.35699,317.05264 315.35699,313.06028 L 315.35699,286.90101 C 315.35699,282.90864 312.51646,279.69459 308.98812,279.69459 L 267.48132,279.69459 L 267.48132,239.12136 C 267.48132,235.12897 264.6408,231.91492 261.11244,231.91492 L 233.57956,231.91492 z";
/* the D-pad's up arrow (path3924); the others are it turned (use3931-3935) */
static const char k_arrow[] =
    "M 231.71636,267.52693 C 230.30056,264.68711 232.56543,260.21886 232.56543,260.21886 L 237.28482,250.75281 L 242.00416,241.28677 C 242.00416,241.28677 243.89192,237.6307 246.72354,237.6307 C 249.55516,237.6307 251.44289,241.28677 251.44289,241.28677 L 256.16226,250.75281 L 260.88163,260.21886 C 260.88163,260.21886 263.43321,264.52109 262.01739,267.36091 C 260.60161,270.20075 256.16226,269.68493 256.16226,269.68493 L 246.72354,269.68493 L 237.28482,269.68493 C 237.28482,269.68493 233.13218,270.36673 231.71636,267.52693 z";

static const Stop s_body_fill[] = { { 0, 0x8D8D8C, 1 }, { 0.25416961f, 0xF6F6F4, 1 }, { 1, 0xF3F3EF, 1 } };
static const Stop s_body_line[] = { { 0, 0x5B5B5A, 1 }, { 0.31944445f, 0x5D5D5D, 1 }, { 1, 0xF9F9F9, 1 } };
static const Stop s_shoulder_fill[] = { { 0, 0xD3D3D3, 1 }, { 0.53977019f, 0xEDEDED, 1 }, { 1, 0x4D4D4D, 1 } };
static const Stop s_shoulder_line[] = { { 0, 0xDCDCD9, 1 }, { 0.65597826f, 0xF0F0EE, 1 }, { 1, 0xF3F3EF, 1 } };
static const Stop s_dish_fill[] = { { 0, 0x000000, 1 }, { 0.3888889f, 0xDCDCDC, 1 }, { 1, 0xEEEEE9, 1 } };
static const Stop s_dish_line[] = { { 0, 0xF3F3F1, 1 }, { 0.94f, 0x7F7F7E, 1 }, { 1, 0xE6E6E3, 1 } };
static const Stop s_plate_line[] = { { 0, 0x2A2D2E, 1 }, { 0.98000002f, 0xD7D6D7, 1 }, { 0.99000001f, 0x000000, 1 },
                                     { 1, 0x859094, 1 } };
static const Stop s_green[] = { { 0, 0x00DEA5, 1 }, { 1, 0x00F8B9, 1 } };
static const Stop s_pill[] = { { 0, 0x899194, 1 }, { 1, 0x5E6667, 1 }, { 1, 0x515759, 1 } };
static const Stop s_dark[] = { { 0, 0x303638, 1 }, { 1, 0x424A4F, 1 } };
static const Stop s_notch[] = { { 0, 0xF0F0EC, 1 }, { 1, 0xBABAB7, 1 } };

/* ---- the letters: the pause screen's capitals (pause_art.c) ---- */

/* text whose capitals' top left is (x, top) and height cap, drawing
   pixels; sheared right by `slant` a unit of height (an italic) */
static void letters(Canvas *m, const char *s, double x, double top, double cap, double slant, uint32_t c)
{
    double dot = cap / 7;
    for (; *s; s++, x += 6 * dot) {
        const uint8_t *g = pause_glyph(*s);
        int r, b;
        if (!g)
            continue;
        for (r = 0; r < 7; r++)
            for (b = 0; b < 5; b++)
                if (g[r] >> (4 - b) & 1) {
                    Shape q = rrect(x + b * dot + (6 - r) * dot * slant, top + r * dot, dot, dot, 0);
                    Paint p = flat(c);
                    draw(m, &q, k_id, &p, NULL, 0);
                }
    }
}

void pad_draw(uint32_t *px, int w, int h, float density, float k, float cx, float cy)
{
    Canvas m = { px, w, h, density, k, cx, cy };
    const Xf rot45 = mtx(0.7071068, -0.7071068, 0.7071068, 0.7071068, 0, 0);
    Shape shoulder = path_shape(k_shoulder), body = path_shape(k_body), cross = path_shape(k_cross),
          cross_top = path_shape(k_cross_top), arrow = path_shape(k_arrow), sh;
    Paint f, l;
    int i;

    /* the shoulder buttons: the left (the groups' transforms come to a
       move up of 2.452), the right the same mirrored about x 500.1747 */
    f = radial(245.17468, 143.66698, 104.51264,
               mtx(1.7645947, -0.297109, 7.9491748e-2, 0.4721195, -198.87963, 148.68252), s_shoulder_fill,
               NSTOPS(s_shoulder_fill));
    l = linear(140.66206, 143.66698, 349.68732, 143.66698, k_id, s_shoulder_line, NSTOPS(s_shoulder_line));
    draw(&m, &shoulder, xlate(0, 2.1213203 - 4.5732233), &f, &l, 1);
    draw(&m, &shoulder, mtx(-1, 0, 0, 1, 1000.3494, 2.1213203 - 4.5732233), &f, &l, 1);
    /* the body */
    f = linear(500, 508, 512.44775, 82, k_id, s_body_fill, NSTOPS(s_body_fill));
    l = linear(500, 508, 512.44775, 82, k_id, s_body_line, NSTOPS(s_body_line));
    draw(&m, &body, k_id, &f, &l, 5);
    /* the D-pad's dish */
    sh = circle(170, 300, 96);
    f = radial(170, 300, 96.805557, k_id, s_dish_fill, NSTOPS(s_dish_fill));
    l = radial(170, 300, 97.111115, k_id, s_dish_line, NSTOPS(s_dish_line));
    draw(&m, &sh, mtx(1.1486534, 0, 0, 1.1486534, 52.05556, -44.596021), &f, &l, 3.33333325);
    /* the face buttons' plate and its two troughs */
    sh = circle(621, 301, 152);
    f = flat(0x777F82);
    l = radial(621, 301, 154.5, k_id, s_plate_line, NSTOPS(s_plate_line));
    draw(&m, &sh, xlate(119.05204, 0), &f, &l, 5);
    sh = rrect(235.90645, 652.72327, 159, 65, 36);
    f = flat(0xF4F4F1);
    l = flat(0xFDFDFA);
    draw(&m, &sh, rot45, &f, &l, 0.99999994);
    draw(&m, &sh, xf_mul(xlate(80, 75), rot45), &f, &l, 0.99999994);
    /* the face buttons: each a dark ring (x 1.05) under its colour (x 0.95);
       Y, B, A, X in the drawing's order */
    {
        static const struct { double rx, ry, tx, ty; uint32_t fill, line; } k_btn[4] = {
            { 103.92704, -17.899999, 158.17704, 11.900001, 0, 0x00AB7F },        /* Y: green, a gradient */
            { 184.42704, 58.100001, 238.67704, 87.900001, 0xFFBB58, 0xBA853D },  /* B */
            { 251.92704, -9.8999993, 306.17704, 19.900001, 0xFF4856, 0xC72B3E }, /* A */
            { 170.92704, -85.399999, 225.17704, -55.599999, 0x0075FA, 0x0054B3 },/* X */
        };
        sh = circle(542.5, 298, 27.5);
        for (i = 0; i < 4; i++) {
            f = flat(0x1A1A1A);
            l = flat(0xEBEBE8);
            draw(&m, &sh, mtx(1.05, 0, 0, 1.05, k_btn[i].rx, k_btn[i].ry), &f, &l, 0.95238096);
            f = i ? flat(k_btn[i].fill) : linear(542.5, 334.09967, 542.5, 257.43439, k_id, s_green, NSTOPS(s_green));
            l = flat(k_btn[i].line);
            draw(&m, &sh, mtx(0.95, 0, 0, 0.95, k_btn[i].tx, k_btn[i].ty), &f, &l, 2.10526323);
        }
    }
    /* SELECT (g3574) */
    sh = rrect(32.659458, 523.89368, 68.390419, 19.478285, 7.3457208);
    f = flat(0x1A1A1A);
    l = flat(0xE2E2DF);
    draw(&m, &sh, xf_mul(xlate(14, 0), rot45), &f, &l, 1.83643019);
    sh = rrect(34.395237, 525.7442, 64.702263, 15.777154, 5.9500341);
    f = radial(63.996368, 530.88281, 32.95863,
               mtx(1.7547305, -5.1588377e-3, 5.5715772e-4, 0.189512, -45.845799, 433.35427), s_pill, NSTOPS(s_pill));
    l = flat(0x252A2A);
    draw(&m, &sh, xf_mul(xlate(14, 0), rot45), &f, &l, 1.215);
    /* the D-pad: the cross, its top, the four arrows, the dimple */
    f = flat(0x1A1A1A);
    l = flat(0x8E979E);
    draw(&m, &cross, k_id, &f, &l, 0.93806696);
    f = flat(0x4D5254);
    l = flat(0x63686B);
    draw(&m, &cross_top, k_id, &f, &l, 1.91442239);
    {
        const Xf up = k_id, down = mtx(1, 0, 0, -1, 0, 598.76219), turn = mtx(0, -1, 1, 0, -52.533083, 546.22911);
        const Xf t[4] = { up, down, turn, xf_mul(turn, down) };
        f = radial(246.84801, 253.6937, 16.627798, mtx(1, 0, 0, 1.0280112, 0, -7.1062765), s_dark, NSTOPS(s_dark));
        l = flat(0x4F5457);
        l.alpha = 0.8627451f;
        for (i = 0; i < 4; i++)
            draw(&m, &arrow, t[i], &f, &l, 2.06114793);
    }
    sh = circle(306.00046, 299.30283, 10.429825);
    f = radial(306.00046, 299.30283, 11.154825, k_id, s_dark, NSTOPS(s_dark));
    l = flat(0x4F5457);
    l.alpha = 0.86170206f;
    draw(&m, &sh, mtx(1.6288945, 0, 0, 1.6288945, -251.11585, -187.53276), &f, &l, 1.45000005);
    /* START (g4168) */
    sh = rrect(29.909458, 521.14368, 68.390419, 19.478285, 7.3457208);
    f = flat(0x1A1A1A);
    l = flat(0xE2E2DF);
    draw(&m, &sh, xf_mul(xlate(112.9467, 0), rot45), &f, &l, 1.83643019);
    sh = rrect(31.645237, 522.9942, 64.702263, 15.777154, 5.9500341);
    f = radial(63.996368, 530.88275, 32.95863, mtx(1, 0, 0, 0.25778, 0, 394.0318), s_pill, NSTOPS(s_pill));
    l = flat(0x252A2A);
    draw(&m, &sh, xf_mul(xlate(112.9467, 0), rot45), &f, &l, 1.215);
    /* the cable's notch (rect3564) */
    sh = rrect2(481.75, 128.3867, 36.5, 8.3873806, 0.69999999, 1.1679767);
    f = linear(492.77176, 132.51852, 492.77176, 134.27342, mtx(1, 0, 0, 1.6685383, 9.3750002, -85.831423), s_notch,
               NSTOPS(s_notch));
    draw(&m, &sh, k_id, &f, NULL, 0);
    /* the letters: SELECT and START (Arial Italic 16, #474b4c, capitals
       11.5 high), A B Y X (Arial Black 24, #f7f7f4, capitals 17.2 high),
       each at its text box's top left plus its font's ascent less its
       capitals' height (Arial 0.905 - 0.716 of the size, Arial Black
       1.1 - 0.716) */
    letters(&m, "SELECT", 410.0, 367.5 + 3.0, 11.5, 0.2, 0x474B4C);
    letters(&m, "START", 504.73212, 367.36186 + 3.0, 11.5, 0.2, 0x474B4C);
    letters(&m, "A", 850.31056, 250.92033 + 9.2, 17.2, 0, 0xF7F7F4);
    letters(&m, "B", 697.28594, 391.98744 + 9.2, 17.2, 0, 0xF7F7F4);
    letters(&m, "Y", 616.95098, 318.80150 + 9.2, 17.2, 0, 0xF7F7F4);
    letters(&m, "X", 767.92167, 176.31589 + 9.2, 17.2, 0, 0xF7F7F4);
    free(shoulder.pts);
    free(body.pts);
    free(cross.pts);
    free(cross_top.pts);
    free(arrow.pts);
}
