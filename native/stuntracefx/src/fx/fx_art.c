/*
 * fx_art.c: the HUD's pictures drawn as clean geometry (plan stage 10,
 * "Pictures as geometry").
 *
 * A picture here is 2 bits a texel, its colours flat and bounded by edges the
 * art's stairs stand for. Each is given once (fx_art_data.c) as the shapes
 * the art means: loops of corners, the edge straight from corner to corner
 * and each corner rounded by a circle arc tangent to both its edges, so that
 * every curve eases into the straights it meets. The numbers were fitted to
 * the picture's texels by a tool outside the game; each picture states how
 * many of its texels the clean shapes leave on the other side, and the
 * first use checks that exact count against the cartridge's texels (a
 * changed picture or a wrong table shows, and the picture is then drawn
 * from its texels as before).
 *
 * The other colours come from the fill as the art makes them: its outline
 * is the fill grown by one texel (each straight moved out by one, each arc
 * one larger about the same centre, or one smaller at a concave corner), its
 * drop shadow that outline moved right and down, and a bevel's face the fill
 * where the fill also lies a texel down and right.
 *
 * Each colour's region is cut into flat pieces once, in the picture's texel
 * space, by the winding rule: trapezoids between the region's edges, a new
 * one wherever an edge ends or two cross. fx_draw.c lays them at the
 * picture's place and scale in the draw list; the GPU fills them at the
 * output's resolution. Nothing here makes a pixel.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fx/fx_art.h"

/* the same numbers in the game and the fitting tool, whatever either is built with */
#pragma STDC FP_CONTRACT OFF

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* an arc's points: no chord further from the arc than ARC_ERROR texels, and
   none longer than ARC_MOST of a turn */
#define ARC_ERROR 0.004
#define ARC_MOST (M_PI / 16)

static int arc_steps(double r, double sweep)
{
    double step = ARC_MOST;
    int n;

    if (r > ARC_ERROR) {
        double s = 2 * acos(1 - ARC_ERROR / r);

        if (s < step)
            step = s;
    }
    n = (int)ceil(fabs(sweep) / step);
    return n < 1 ? 1 : n;
}

enum { CORNER_POINTS = 512 };

typedef struct {
    double p[CORNER_POINTS][2];
    int n, concave_sharp;
} Turn;

/* Where segment (a, b) properly crosses (c, d): 1 and the point. */
static int crossing(const double *a, const double *b, const double *c, const double *d, double *x)
{
    double rx = b[0] - a[0], ry = b[1] - a[1], sx = d[0] - c[0], sy = d[1] - c[1];
    double den = rx * sy - ry * sx, t, u;

    if (fabs(den) < 1e-15)
        return 0;
    t = ((c[0] - a[0]) * sy - (c[1] - a[1]) * sx) / den;
    u = ((c[0] - a[0]) * ry - (c[1] - a[1]) * rx) / den;
    if (t <= 0 || t >= 1 || u <= 0 || u >= 1)
        return 0;
    x[0] = a[0] + rx * t;
    x[1] = a[1] + ry * t;
    return 1;
}

/* A concave corner gone sharp whose neighbours' arcs reach past it (a cusp's
   two arcs grown): the arcs cut where they cross, the corner's point gone. */
static void cut_crossing(Turn *prev, Turn *at, Turn *next)
{
    double a[CORNER_POINTS + 1][2], x[2];
    int na = 0, i, j, k;

    for (k = 0; k < prev->n; k++) {
        a[na][0] = prev->p[k][0];
        a[na][1] = prev->p[k][1];
        na++;
    }
    a[na][0] = at->p[0][0];
    a[na][1] = at->p[0][1];
    na++;
    for (i = na - 2; i >= 0; i--) {
        /* b: the corner's point, then the next corner's points */
        for (j = 0; j < next->n; j++) {
            const double *c = j == 0 ? at->p[0] : next->p[j - 1], *d = next->p[j];

            if (i == na - 2 && j == 0)
                continue;
            if (crossing(a[i], a[i + 1], c, d, x)) {
                prev->n = i + 1;
                if (prev->n < CORNER_POINTS) {
                    prev->p[prev->n][0] = x[0];
                    prev->p[prev->n][1] = x[1];
                    prev->n++;
                }
                at->n = 0;
                memmove(next->p, next->p + j, sizeof next->p[0] * (size_t)(next->n - j));
                next->n -= j;
                return;
            }
        }
    }
}

int fx_art_trace(const FxArtCorner *c_in, int n_in, double grow, double dx, double dy, double (*out)[2], int room)
{
    Turn *t;
    FxArtCorner *c;
    int i, m = 0, n = 0;
    double area0 = 0, area1 = 0;

    /* corners on the same place are one (the larger radius) */
    c = malloc(sizeof *c * (size_t)(n_in > 0 ? n_in : 1));
    if (!c)
        abort();
    for (i = 0; i < n_in; i++) {
        if (n > 0 && fabs(c_in[i].x - c[n - 1].x) < 1e-6 && fabs(c_in[i].y - c[n - 1].y) < 1e-6) {
            if (c_in[i].r > c[n - 1].r)
                c[n - 1].r = c_in[i].r;
            continue;
        }
        c[n++] = c_in[i];
    }
    while (n > 1 && fabs(c[0].x - c[n - 1].x) < 1e-6 && fabs(c[0].y - c[n - 1].y) < 1e-6) {
        if (c[n - 1].r > c[0].r)
            c[0].r = c[n - 1].r;
        n--;
    }
    if (n < 3) {
        free(c);
        return 0;
    }
    t = malloc(sizeof *t * (size_t)n);
    if (!t)
        abort();
    for (i = 0; i < n; i++) {
        const FxArtCorner *a = &c[(i + n - 1) % n], *v = &c[i], *b = &c[(i + 1) % n];
        double d0x = v->x - a->x, d0y = v->y - a->y, d1x = b->x - v->x, d1y = b->y - v->y;
        double l0 = hypot(d0x, d0y), l1 = hypot(d1x, d1y), cr, dt, phi, k, px, py, r;

        area0 += (double)v->x * b->y - (double)b->x * v->y;
        t[i].n = 0;
        t[i].concave_sharp = 0;
        if (l0 < 1e-12 || l1 < 1e-12)
            continue;
        d0x /= l0;
        d0y /= l0;
        d1x /= l1;
        d1y /= l1;
        cr = d0x * d1y - d0y * d1x;
        dt = d0x * d1x + d0y * d1y;
        phi = atan2(cr, dt);    /* > 0: a turn to the right, round the shape (convex) */
        /* the corner moved: where the straights moved out by grow meet (the
           outward normal is the left of travel, (dy, -dx)) */
        k = 1 + dt;
        if (k < 1e-9) {
            px = v->x + grow * d0y;
            py = v->y - grow * d0x;
        } else {
            px = v->x + grow * (d0y + d1y) / k;
            py = v->y - grow * (d0x + d1x) / k;
        }
        r = v->r <= 0 ? (phi > 0 ? grow : 0) : phi > 0 ? v->r + grow : v->r - grow;
        if (fabs(phi) < 1e-9 || r <= 1e-9) {
            t[i].p[0][0] = px;
            t[i].p[0][1] = py;
            t[i].n = 1;
            t[i].concave_sharp = phi < 0;
        } else {
            double tl = r * tan(fabs(phi) / 2), t1x = px - d0x * tl, t1y = py - d0y * tl;
            double nx = phi > 0 ? -d0y : d0y, ny = phi > 0 ? d0x : -d0x;   /* towards the centre */
            double cx = t1x + nx * r, cy = t1y + ny * r, a0 = atan2(t1y - cy, t1x - cx);
            int s = arc_steps(r, phi), j;

            if (s >= CORNER_POINTS)
                s = CORNER_POINTS - 1;
            for (j = 0; j <= s; j++) {
                t[i].p[j][0] = cx + r * cos(a0 + phi * j / s);
                t[i].p[j][1] = cy + r * sin(a0 + phi * j / s);
            }
            t[i].n = s + 1;
        }
    }
    if (grow != 0)
        for (i = 0; i < n; i++)
            if (t[i].concave_sharp && t[i].n == 1)
                cut_crossing(&t[(i + n - 1) % n], &t[i], &t[(i + 1) % n]);
    for (i = 0; i < n; i++) {
        int j;

        for (j = 0; j < t[i].n; j++) {
            if (m == room) {
                free(t);
                free(c);
                return 0;
            }
            out[m][0] = t[i].p[j][0] + dx;
            out[m][1] = t[i].p[j][1] + dy;
            m++;
        }
    }
    free(t);
    free(c);
    /* a hole the growing has closed: turned round, or nothing left */
    for (i = 0; i < m; i++)
        area1 += out[i][0] * out[(i + 1) % m][1] - out[(i + 1) % m][0] * out[i][1];
    if (grow != 0 && (area1 * area0 <= 0 || fabs(area1) < 1e-6))
        return 0;
    return m;
}

/* ---- each texel's share ----

   The exact area: each edge's part in each texel adds its height to every
   texel right of it in its row, and to its own texel the share right of it
   (a trapezoid); summing along the row gives each texel's share. */

static void row_part(double *row, int w, double xa, double ya, double xb, double yb)
{
    double xs[64], ys[64];
    int n = 0, i, step = xb > xa ? 1 : -1;
    int c, c_end;

    xs[n] = xa;
    ys[n++] = ya;
    if (xa != xb) {
        c = step > 0 ? (int)floor(xa) + 1 : (int)ceil(xa) - 1;
        c_end = step > 0 ? (int)ceil(xb) - 1 : (int)floor(xb) + 1;
        for (; (step > 0 ? c <= c_end : c >= c_end) && n < 63; c += step) {
            if ((step > 0 && (c <= xa || c >= xb)) || (step < 0 && (c >= xa || c <= xb)))
                continue;
            xs[n] = c;
            ys[n++] = ya + (c - xa) * (yb - ya) / (xb - xa);
        }
    }
    xs[n] = xb;
    ys[n++] = yb;
    for (i = 0; i + 1 < n; i++) {
        double xm = (xs[i] + xs[i + 1]) / 2, d = ys[i + 1] - ys[i];
        int cx = (int)floor(xm);

        if (cx < 0)
            row[0] += d;
        else if (cx < w) {
            row[cx + 1] += d * (cx + 1 - xm);
            row[cx + 2] += d * (xm - cx);
        }
    }
}

static void edge_cells(double *acc, int w, int h, double x0, double y0, double x1, double y1)
{
    double lo = y0 < y1 ? y0 : y1, hi = y0 < y1 ? y1 : y0, s0, s1, dxdy;
    int r;

    if (y0 == y1 || hi <= 0 || lo >= h)
        return;
    dxdy = (x1 - x0) / (y1 - y0);
    s0 = lo < 0 ? 0 : lo;
    s1 = hi > h ? h : hi;
    for (r = (int)floor(s0); r < h && r < s1; r++) {
        double ta = s0 > r ? s0 : r, tb = s1 < r + 1 ? s1 : r + 1, ya, yb;

        if (tb <= ta)
            continue;
        ya = y1 > y0 ? ta : tb;
        yb = y1 > y0 ? tb : ta;
        row_part(acc + (size_t)r * (w + 2), w, x0 + (ya - y0) * dxdy, ya, x0 + (yb - y0) * dxdy, yb);
    }
}

void fx_art_cover(const double (*p)[2], const int *len, int loops, int w, int h, double *cov)
{
    double *acc = calloc((size_t)h * (w + 2), sizeof *acc);
    int l, i, first = 0, x, y;

    if (!acc)
        abort();
    for (l = 0; l < loops; l++) {
        for (i = 0; i < len[l]; i++) {
            const double *a = p[first + i], *b = p[first + (i + 1) % len[l]];

            edge_cells(acc, w, h, a[0], a[1], b[0], b[1]);
        }
        first += len[l];
    }
    /* the shape on the right of travel: going down, it lies to the left,
       so the sums run negative inside */
    for (y = 0; y < h; y++) {
        const double *row = acc + (size_t)y * (w + 2);
        double s = row[0];

        for (x = 0; x < w; x++) {
            s += row[x + 1];
            cov[y * w + x] = -s;
        }
    }
    free(acc);
}

int fx_art_inside(const double (*p)[2], const int *len, int loops, double x, double y)
{
    int l, i, first = 0, wind = 0;

    for (l = 0; l < loops; l++) {
        for (i = 0; i < len[l]; i++) {
            const double *a = p[first + i], *b = p[first + (i + 1) % len[l]];

            if ((a[1] <= y && b[1] > y) || (b[1] <= y && a[1] > y)) {
                double xc = a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]);

                if (xc > x)
                    wind += b[1] > a[1] ? 1 : -1;
            }
        }
        first += len[l];
    }
    return wind != 0;
}

void fx_art_share(const double (*p)[2], const int *len, int loops, int w, int h, int k, double *share)
{
    int total = 0, l, sy, x;
    double *xs;
    int *sg;

    for (l = 0; l < loops; l++)
        total += len[l];
    xs = malloc(sizeof *xs * (size_t)(total + 1));
    sg = malloc(sizeof *sg * (size_t)(total + 1));
    if (!xs || !sg)
        abort();
    memset(share, 0, sizeof *share * (size_t)w * h);
    for (sy = 0; sy < h * k; sy++) {
        double y = (sy + 0.5) / k;
        int n = 0, i, j, first = 0, wind = 0, at = 0, sx;

        for (l = 0; l < loops; l++) {
            for (i = 0; i < len[l]; i++) {
                const double *a = p[first + i], *b = p[first + (i + 1) % len[l]];

                if ((a[1] <= y && b[1] > y) || (b[1] <= y && a[1] > y)) {
                    xs[n] = a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]);
                    sg[n] = b[1] > a[1] ? 1 : -1;
                    n++;
                }
            }
            first += len[l];
        }
        for (i = 1; i < n; i++)         /* few: insertion */
            for (j = i; j > 0 && xs[j - 1] > xs[j]; j--) {
                double t = xs[j]; int s = sg[j];

                xs[j] = xs[j - 1]; sg[j] = sg[j - 1];
                xs[j - 1] = t; sg[j - 1] = s;
            }
        for (sx = 0; sx < w * k; sx++) {
            double xx = (sx + 0.5) / k;

            while (at < n && xs[at] < xx)
                wind += sg[at++];
            if (wind != 0) {
                x = sx / k;
                share[(sy / k) * w + x] += 1.0 / ((double)k * k);
            }
        }
    }
    free(xs);
    free(sg);
}

/* ---- flat pieces ---- */

typedef struct {
    double x0, y0, x1, y1;      /* top to bottom */
    int wind, set;
} Edge;

typedef struct {
    int e;
    double xa, xb, xm;
} Cross;

typedef struct {
    int left, right;            /* their edges */
    double top, bottom;
    int open;
} Trap;

static int by_y(const void *a, const void *b)
{
    double d = *(const double *)a - *(const double *)b;

    return d < 0 ? -1 : d > 0;
}

static int by_x(const void *a, const void *b)
{
    double d = ((const Cross *)a)->xm - ((const Cross *)b)->xm;

    return d < 0 ? -1 : d > 0;
}

static double edge_x(const Edge *e, double y)
{
    return e->x0 + (y - e->y0) * (e->x1 - e->x0) / (e->y1 - e->y0);
}

static int add_edges(Edge *edges, int n, int most, const double (*p)[2], const int *len, const int *set, int loops)
{
    int l, i, first = 0;

    for (l = 0; l < loops; l++) {
        for (i = 0; i < len[l]; i++) {
            const double *a = p[first + i], *b = p[first + (i + 1) % len[l]];

            if (a[1] == b[1])
                continue;
            if (n == most)
                return -1;
            if (a[1] < b[1]) {
                edges[n].x0 = a[0]; edges[n].y0 = a[1]; edges[n].x1 = b[0]; edges[n].y1 = b[1]; edges[n].wind = 1;
            } else {
                edges[n].x0 = b[0]; edges[n].y0 = b[1]; edges[n].x1 = a[0]; edges[n].y1 = a[1]; edges[n].wind = -1;
            }
            edges[n].set = set ? set[l] : 0;
            n++;
        }
        first += len[l];
    }
    return n;
}

int fx_art_pieces(const double (*p)[2], const int *len, const int *set, int loops, int sets, float (*tri)[2], int room)
{
    int total = 0, l, ne, nys = 0, i, j, s, nt = 0, nc, ntrap = 0, most_trap = 0, out = 0;
    Edge *edges;
    double *ys;
    Cross *cross;
    Trap *trap;

    if (sets < 1)
        sets = 1;
    for (l = 0; l < loops; l++)
        total += len[l];
    edges = malloc(sizeof *edges * (size_t)(total + 1));
    cross = malloc(sizeof *cross * (size_t)(total + 1));
    if (!edges || !cross)
        abort();
    ne = add_edges(edges, 0, total, p, len, set, loops);
    if (ne < 0)
        abort();
    /* the slabs' bounds: every end and every crossing of two edges */
    {
        size_t most = (size_t)ne * 2 + 16;

        ys = malloc(sizeof *ys * most);
        if (!ys)
            abort();
        for (i = 0; i < ne; i++) {
            ys[nys++] = edges[i].y0;
            ys[nys++] = edges[i].y1;
        }
        for (i = 0; i < ne; i++)
            for (j = i + 1; j < ne; j++) {
                const Edge *pe = &edges[i], *q = &edges[j];
                double rx = pe->x1 - pe->x0, ry = pe->y1 - pe->y0, sx = q->x1 - q->x0, sy = q->y1 - q->y0;
                double den = rx * sy - ry * sx, t, u;

                if (pe->y1 <= q->y0 || q->y1 <= pe->y0 || fabs(den) < 1e-12)
                    continue;
                t = ((q->x0 - pe->x0) * sy - (q->y0 - pe->y0) * sx) / den;
                u = ((q->x0 - pe->x0) * ry - (q->y0 - pe->y0) * rx) / den;
                if (t <= 1e-9 || t >= 1 - 1e-9 || u <= 1e-9 || u >= 1 - 1e-9)
                    continue;
                if ((size_t)nys == most) {
                    most *= 2;
                    ys = realloc(ys, sizeof *ys * most);
                    if (!ys)
                        abort();
                }
                ys[nys++] = pe->y0 + t * ry;
            }
    }
    qsort(ys, (size_t)nys, sizeof *ys, by_y);
    trap = NULL;
    for (s = 0; s + 1 < nys; s++) {
        double ya = ys[s], yb = ys[s + 1], ym = (ya + yb) / 2;
        int wind[4] = { 0, 0, 0, 0 }, left = -1, k;

        if (yb - ya < 1e-9)
            continue;
        nc = 0;
        for (i = 0; i < ne; i++)
            if (edges[i].y0 < ym && edges[i].y1 > ym) {
                cross[nc].e = i;
                cross[nc].xa = edge_x(&edges[i], ya);
                cross[nc].xb = edge_x(&edges[i], yb);
                cross[nc].xm = (cross[nc].xa + cross[nc].xb) / 2;
                nc++;
            }
        qsort(cross, (size_t)nc, sizeof *cross, by_x);
        for (k = 0; k < ntrap; k++)
            trap[k].open = trap[k].open && trap[k].bottom == ya ? 2 : 0;    /* 2: may go on */
        for (i = 0; i < nc; i++) {
            int was = 1, now = 1, q;

            for (q = 0; q < sets; q++)
                was = was && wind[q] != 0;
            wind[edges[cross[i].e].set & 3] += edges[cross[i].e].wind;
            for (q = 0; q < sets; q++)
                now = now && wind[q] != 0;
            if (!was && now)
                left = cross[i].e;
            else if (was && !now) {
                int right = cross[i].e, found = -1;

                for (k = 0; k < ntrap; k++)
                    if (trap[k].open == 2 && trap[k].left == left && trap[k].right == right) {
                        found = k;
                        break;
                    }
                if (found >= 0) {
                    trap[found].bottom = yb;
                    trap[found].open = 3;
                } else {
                    if (ntrap == most_trap) {
                        most_trap = most_trap ? most_trap * 2 : 256;
                        trap = realloc(trap, sizeof *trap * (size_t)most_trap);
                        if (!trap)
                            abort();
                    }
                    trap[ntrap].left = left;
                    trap[ntrap].right = right;
                    trap[ntrap].top = ya;
                    trap[ntrap].bottom = yb;
                    trap[ntrap].open = 3;
                    ntrap++;
                }
            }
        }
        for (k = 0; k < ntrap; k++)
            trap[k].open = trap[k].open == 3;
    }
    for (i = 0; i < ntrap && out >= 0; i++) {
        const Edge *le = &edges[trap[i].left], *re = &edges[trap[i].right];
        double y0 = trap[i].top, y1 = trap[i].bottom;
        double l0 = edge_x(le, y0), l1 = edge_x(le, y1), r0 = edge_x(re, y0), r1 = edge_x(re, y1);

        if (r0 - l0 > 1e-9) {
            if (nt == room) {
                out = -1;
                break;
            }
            tri[3 * nt][0] = (float)l0; tri[3 * nt][1] = (float)y0;
            tri[3 * nt + 1][0] = (float)r0; tri[3 * nt + 1][1] = (float)y0;
            tri[3 * nt + 2][0] = (float)r1; tri[3 * nt + 2][1] = (float)y1;
            nt++;
        }
        if (r1 - l1 > 1e-9) {
            if (nt == room) {
                out = -1;
                break;
            }
            tri[3 * nt][0] = (float)l0; tri[3 * nt][1] = (float)y0;
            tri[3 * nt + 1][0] = (float)r1; tri[3 * nt + 1][1] = (float)y1;
            tri[3 * nt + 2][0] = (float)l1; tri[3 * nt + 2][1] = (float)y1;
            nt++;
        }
    }
    free(trap);
    free(ys);
    free(cross);
    free(edges);
    return out < 0 ? -1 : nt;
}

/* ---- the pictures ---- */

#ifndef FX_ART_GEOMETRY_ONLY

enum {
    POINTS = 16384,             /* a layer's loops' points, grown and moved copies included */
    LOOPS = 256,
    TRIANGLES = 65536
};

typedef struct {
    int made, good, layers;
    struct {
        uint8_t value;
        int n;
        float (*tri)[2];
    } layer[4];
} Made;

static Made *made;

static uint8_t texel(const FxArtPicture *p, int x, int y)
{
    uint8_t b = fx_art_rom(p->bank, (uint16_t)(p->at + y * 32 + x / 4));

    return (uint8_t)((b >> (2 * (x & 3))) & 3);
}

/* A layer's loops, grown and moved, after those already in `pts`. */
static void trace_layer(const FxArtLayer *l, double grow, double dx, double dy, double (*pts)[2], int *len, int *loops,
                        int *used)
{
    int i;

    for (i = 0; i < l->loops && *loops < LOOPS; i++) {
        int m = fx_art_trace(l->loop[i].c, l->loop[i].n, grow, dx, dy, pts + *used, POINTS - *used);

        if (m > 0) {
            len[(*loops)++] = m;
            *used += m;
        }
    }
}

typedef struct {
    double (*p)[2];
    int len[LOOPS], tag[LOOPS], loops, used;
} Set;

static void set_init(Set *s)
{
    s->p = malloc(sizeof *s->p * POINTS);
    if (!s->p)
        abort();
    s->loops = s->used = 0;
}

/* the loops added since `from` belong to set `tag` (fx_art_pieces' sets) */
static void set_tag(Set *s, int from, int tag)
{
    int i;

    for (i = from; i < s->loops; i++)
        s->tag[i] = tag;
}

static void keep_pieces(Made *m, uint8_t value, const Set *a, int sets)
{
    static float tri[TRIANGLES * 3][2];
    int n = fx_art_pieces((const double (*)[2])a->p, a->len, a->tag, a->loops, sets, tri, TRIANGLES);
    int k = m->layers++;

    m->layer[k].value = value;
    m->layer[k].n = n > 0 ? n : 0;
    m->layer[k].tri = NULL;
    if (n > 0) {
        m->layer[k].tri = malloc(sizeof tri[0] * 3 * (size_t)n);
        if (!m->layer[k].tri)
            abort();
        memcpy(m->layer[k].tri, tri, sizeof tri[0] * 3 * (size_t)n);
    }
}

/* The clean shapes' value at each texel, as the fitting tool makes it, and
   the count apart from the art's. Outlined: the fill's exact share, 3 from
   3/4 up, 2 above 1/4 (the in-between texels), else 1 where half of 8 x 8
   points lie in the outline or its shadow; bevelled and layered: the
   topmost colour at the texel's centre. */
static int check(const FxArtPicture *p, const Set *fill, const Set *green, const Set *face, const Set *layer)
{
    int w = p->w, h = p->h, x, y, k, wrong = 0;
    double *a = malloc(sizeof *a * (size_t)w * h), *b = malloc(sizeof *b * (size_t)w * h);
    double *c = malloc(sizeof *c * (size_t)w * h);

    if (!a || !b || !c)
        abort();
    if (p->style == FX_ART_OUTLINED) {
        fx_art_cover((const double (*)[2])fill->p, fill->len, fill->loops, w, h, a);
        fx_art_share((const double (*)[2])green->p, green->len, green->loops, w, h, 8, b);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                double cf = a[y * w + x];
                int v = cf >= 0.75 - 1e-9 ? 3 : cf > 0.25 + 1e-9 ? 2 : b[y * w + x] >= 0.5 ? 1 : 0;

                wrong += v != texel(p, x, y);
            }
    } else if (p->style == FX_ART_BEVELLED) {
        /* the face: the texel's centre in the fill and in each of its three
           copies moved a texel left, up, and both (face->p holds them, tagged 1-3) */
        fx_art_share((const double (*)[2])fill->p, fill->len, fill->loops, w, h, 1, a);
        fx_art_share((const double (*)[2])green->p, green->len, green->loops, w, h, 1, c);
        for (y = 0; y < w * h; y++)
            b[y] = a[y] > 0;
        for (k = 1; k <= 3; k++) {
            int l, first = 0, n = 0, len[LOOPS];
            double *d = malloc(sizeof *d * (size_t)w * h);
            const double (*start)[2] = NULL;

            if (!d)
                abort();
            for (l = 0; l < face->loops; l++) {
                if (face->tag[l] == k) {
                    if (!start)
                        start = (const double (*)[2])face->p + first;
                    len[n++] = face->len[l];
                }
                first += face->len[l];
            }
            if (start)
                fx_art_share(start, len, n, w, h, 1, d);
            for (y = 0; y < w * h; y++)
                b[y] = b[y] && start && d[y] > 0;
            free(d);
        }
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                int i = y * w + x, v = b[i] ? 3 : a[i] > 0 ? 2 : c[i] > 0 ? 1 : 0;

                wrong += v != texel(p, x, y);
            }
    } else {
        int *v = calloc((size_t)w * h, sizeof *v);

        if (!v)
            abort();
        for (k = 0; k < p->layers; k++) {
            fx_art_share((const double (*)[2])layer[k].p, layer[k].len, layer[k].loops, w, h, 1, a);
            for (y = 0; y < w * h; y++)
                if (a[y] > 0)
                    v[y] = p->layer[k].value;
        }
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                wrong += v[y * w + x] != texel(p, x, y);
        free(v);
    }
    free(a);
    free(b);
    free(c);
    return wrong;
}

static void make(const FxArtPicture *p, Made *m)
{
    Set fill, green, face, layer[4];
    int k, wrong = 0;

    m->made = 1;
    m->layers = 0;
    if (p->style == FX_ART_LAYERED) {
        for (k = 0; k < p->layers && k < 4; k++) {
            set_init(&layer[k]);
            trace_layer(&p->layer[k], p->layer[k].grow, 0, 0, layer[k].p, layer[k].len, &layer[k].loops, &layer[k].used);
            set_tag(&layer[k], 0, 0);
        }
        wrong = check(p, NULL, NULL, NULL, layer);
        for (k = 0; k < p->layers && k < 4; k++) {
            keep_pieces(m, p->layer[k].value, &layer[k], 1);
            free(layer[k].p);
        }
    } else {
        set_init(&fill);
        set_init(&green);
        set_init(&face);
        trace_layer(&p->layer[0], 0, 0, 0, fill.p, fill.len, &fill.loops, &fill.used);
        set_tag(&fill, 0, 0);
        if (p->style == FX_ART_OUTLINED) {
            for (k = 0; k <= p->shadow; k++)
                trace_layer(&p->layer[0], 1, k, k, green.p, green.len, &green.loops, &green.used);
            set_tag(&green, 0, 0);
            wrong = check(p, &fill, &green, NULL, NULL);
            keep_pieces(m, 1, &green, 1);
            keep_pieces(m, 3, &fill, 1);
        } else {
            static const int moves[4][2] = { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } };

            trace_layer(&p->layer[0], 1, 0, 0, green.p, green.len, &green.loops, &green.used);
            set_tag(&green, 0, 0);
            for (k = 0; k < 4; k++) {
                int from = face.loops;

                trace_layer(&p->layer[0], 0, moves[k][0], moves[k][1], face.p, face.len, &face.loops, &face.used);
                set_tag(&face, from, k);
            }
            wrong = check(p, &fill, &green, &face, NULL);
            keep_pieces(m, 1, &green, 1);
            keep_pieces(m, 2, &fill, 1);
            keep_pieces(m, 3, &face, 4);
        }
        free(fill.p);
        free(green.p);
        free(face.p);
    }
    m->good = wrong == p->off;
    if (!m->good)
        fprintf(stderr, "stuntracefx: %s: %d of %d texels not as the clean shapes make them, %d expected; the picture is drawn from its texels\n",
                p->name, wrong, p->w * p->h, p->off);
}

static Made *made_of(const FxArtPicture *p)
{
    Made *m;

    if (!made) {
        made = calloc((size_t)fx_art_picture_count, sizeof *made);
        if (!made)
            abort();
    }
    m = &made[p - fx_art_pictures];
    if (!m->made)
        make(p, m);
    return m;
}

const FxArtPicture *fx_art_find(uint8_t bank, uint16_t at, int *column, int *row)
{
    int i;

    for (i = 0; i < fx_art_picture_count; i++) {
        const FxArtPicture *p = &fx_art_pictures[i];
        uint16_t d = (uint16_t)(at - p->at);

        if (p->bank == bank && d / 32 < p->h && (d % 32) * 4 < p->w) {
            *column = (d % 32) * 4;
            *row = d / 32;
            return p;
        }
    }
    return NULL;
}

int fx_art_good(const FxArtPicture *p)
{
    return made_of(p)->good;
}

int fx_art_layers(const FxArtPicture *p)
{
    Made *m = made_of(p);

    return m->good ? m->layers : 0;
}

const float (*fx_art_layer(const FxArtPicture *p, int layer, int *triangles, uint8_t *value))[2]
{
    Made *m = made_of(p);

    *triangles = m->layer[layer].n;
    *value = m->layer[layer].value;
    return (const float (*)[2])m->layer[layer].tri;
}

#endif
