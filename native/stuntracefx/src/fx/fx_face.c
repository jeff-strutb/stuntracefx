/*
 * fx_face.c: the race's shapes in floats (plan stage 10, "Stage 10's 3D").
 *
 * The frame program decides what is drawn: which objects, which level of
 * detail, which faces face the eye, their order and materials. Where it is
 * drawn is made here, from the view-space points, unrounded and unheld:
 *
 *   FxModelProject   each point's view-space sum (the model's point at
 *                    $0340 + 6i plus the object's place at $0026-$002A, in
 *                    16 bits as the chip adds them) is kept by its record
 *                    $04E4 + 6i (fx_face_point);
 *   FxModelFaces     each face that faces the eye: its points and texels
 *                    (fx_face_begin, fx_face_vertex), then its material
 *                    (fx_face_end), which makes its shape;
 *   FxDrawList       the shapes drawn in order (fx_draw.c).
 *
 * The chip's own clip, projection to whole pixels and queue are not used:
 * whether a face is seen is decided here, in floats.
 *
 * A shape is the face clipped against a near plane in view space,
 * projected x = cx + 128 X / Z, y = cy + 128 Y / Z, clipped against the
 * view's rectangle, and fanned. A textured face is cut by planes of equal
 * depth until each piece's far over near depth is at most 1.25, so each
 * piece's texture, laid on by the GPU across the screen, stays near its
 * place in space. Nothing here makes a pixel.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_face.h"
#include "fx/fx_ground.h"
#include "fx/fx_seat.h"
#include "fx/fx_far.h"

enum {
    POINTS = 256,               /* a model's point records ($04E4 + 6i, i a byte) */
    MOST = 64,                  /* a face's points: the chip's are 2 to 8, a solid's cap up to 60 */
    WORK = MOST + 8,            /* each clip adds at most one */
    SHAPES = 512,
    CORNERS = 32768,
    TABLE = 0x04E4
};

#define FOCAL 128.0             /* FxRatioClamp: (R1 / 2) * 256 / R3; the far table the same */
#define NEAR_Z 1.0              /* the near plane: the chip's clip is at 0, its depth 0 taken as 1 */
#define CUT 1.25                /* a textured piece's far over near depth */
#define CHIP_FAR 17406.0        /* $43FE: the depth FxModelProject's far table stops at */
#define HOLE_LEAST 4096.0       /* a hole in the water: at least this much of the plane (64 x 64) */

/* A word of the chip's RAM, read without moving its last RAM address. */
static uint16_t word(uint16_t a)
{
    return (uint16_t)(fx_ram[fx_ram_at(a)] | fx_ram[fx_ram_at((uint16_t)(a ^ 1))] << 8);
}

/* ---- the points, as the frame program projected them ---- */

static struct {
    double x, y, z;
    uint32_t made;
} point[POINTS];
static uint32_t projection;

static FxSurveyPoint survey_sink;
static FxSurveyFace survey_face;
static void *survey_context;

void fx_face_survey(FxSurveyPoint point, FxSurveyFace face, void *context)
{
    survey_sink = point;
    survey_face = face;
    survey_context = context;
}

void fx_face_point(uint16_t record, uint16_t x, uint16_t y, uint16_t z)
{
    unsigned i = (uint16_t)(record - TABLE) / 6u;

    if (survey_sink)
        survey_sink((int16_t)x, (int16_t)y, (int16_t)z, survey_context);

    if (i >= POINTS)
        return;
    if (i == 0)                         /* every projection starts at the first point */
        projection++;
    point[i].x = (int16_t)x;
    point[i].y = (int16_t)y;
    point[i].z = (int16_t)z;
    point[i].made = projection;
}

uint8_t fx_face_facing(uint16_t a, uint16_t b, uint16_t c, uint8_t chip)
{
    uint16_t at[3] = { a, b, c };
    unsigned i[3], k;
    double det;

    for (k = 0; k < 3; k++) {
        if ((uint16_t)(at[k] - TABLE) % 6u)
            return chip;
        i[k] = (uint16_t)(at[k] - TABLE) / 6u;
        if (i[k] >= POINTS || !projection || point[i[k]].made != projection)
            return chip;
    }
    det = point[i[0]].x * (point[i[1]].y * point[i[2]].z - point[i[1]].z * point[i[2]].y)
        - point[i[0]].y * (point[i[1]].x * point[i[2]].z - point[i[1]].z * point[i[2]].x)
        + point[i[0]].z * (point[i[1]].x * point[i[2]].y - point[i[1]].y * point[i[2]].x);
    if (det == 0)
        return chip;
    return (uint8_t)((chip & 0x7F) | (det < 0 ? 0x80 : 0));
}

/* ---- the view: its centre and rectangle in the picture's pixels ---- */

typedef struct {
    double cx, cy, left, right, top, bottom;
} View;

/* The words GsuParamsFromDp gives the chip: the centre, the edges (the
   right one its last column, the bottom one past its last row). */
static View view_now(void)
{
    View v;

    v.cx = (int16_t)word(0x0034);
    v.cy = (int16_t)word(0x0036);
    v.left = (int16_t)word(0x0038);
    v.right = (int16_t)word(0x003A) + 1;
    v.top = (int16_t)word(0x003C);
    v.bottom = (int16_t)word(0x003E);
    return v;
}

void fx_face_view_rect(float *left, float *right, float *top, float *bottom)
{
    View v = view_now();

    *left = (float)v.left;
    *right = (float)v.right;
    *top = (float)v.top;
    *bottom = (float)v.bottom;
}

/* A view-space point's place on the picture: x = cx + 128 X / Z,
   y = cy + 128 Y / Z, unrounded and unheld (Z in front of the near plane). */
static void place(const View *v, double x, double y, double z, double *sx, double *sy)
{
    *sx = v->cx + FOCAL * x / z;
    *sy = v->cy + FOCAL * y / z;
}

/* The view's words for a rectangle of the screen: the edges as the chip
   has them (the right one its last column, the bottom one past its last
   row), the centre, and the frustum's planes as the original's are made
   (view.c): $92 = 128 * 2^14 / the half-width, $94 = 128 * 2^13 / the
   larger part above or below the centre (0x4EC4 and 0x4000 for the
   original's 208 x 128; 0x71C7 and 0x6666 for its 144 x 80). */
FxViewWords fx_view_words(int x, int y, int width, int height, int centre_y)
{
    FxViewWords w;
    int cx = x + width / 2, half_w = cx - x, half_h;
    long a, b;

    if (width - half_w > half_w)
        half_w = width - half_w;
    half_h = centre_y - y;
    if (y + height - centre_y > half_h)
        half_h = y + height - centre_y;
    if (half_w < 33)
        half_w = 33;
    if (half_h < 17)
        half_h = 17;
    a = (128L << 14) / half_w;
    b = (128L << 13) / half_h;
    w.left = (uint16_t)x;
    w.right = (uint16_t)(x + width - 1);
    w.top = (uint16_t)y;
    w.bottom = (uint16_t)(y + height);
    w.centre_x = (uint16_t)cx;
    w.centre_y = (uint16_t)centre_y;
    w.scale_32 = (uint16_t)(a > 0xFFFF ? 0xFFFF : a);
    w.scale_34 = (uint16_t)(b > 0xFFFF ? 0xFFFF : b);
    return w;
}

/* ---- the checks' count ---- */

static int checking;
static FxFaceCheck counted;

void fx_face_check_on(int o) { checking = o; }
FxFaceCheck fx_face_check(void) { return counted; }

/* A point the chip put wholly on the screen, against its own record: the
   chip floors (or for a near point ceils by sign), so within a pixel. */
static void check_point(unsigned i, const View *v)
{
    uint16_t at = (uint16_t)(TABLE + 6 * i);
    double x, y, dx, dy;

    if (point[i].z <= 0 || point[i].z >= CHIP_FAR || (word((uint16_t)(at + 4)) & 0x00FF))
        return;
    place(v, point[i].x, point[i].y, point[i].z, &x, &y);
    dx = fabs(x - (int16_t)word(at));
    dy = fabs(y - (int16_t)word((uint16_t)(at + 2)));
    counted.vertices++;
    if (point[i].z < 256) {
        counted.near++;
        if (dx > 1.02 || dy > 1.02)
            counted.near_apart++;
    }
    /* the chip's own error: its floor, and a near point's halved pair
       (128 / z), or a far point's table entry, truncated and taken at the
       depth rounded down to even (|x| / 65536 + 128 |x| / z^2) */
    {
        double z = point[i].z, bx, by;

        if (z < 256) {
            bx = by = 1.02 + FOCAL / z;
        } else {
            bx = 1.02 + fabs(point[i].x) / 65536 + FOCAL * fabs(point[i].x) / (z * z);
            by = 1.02 + fabs(point[i].y) / 65536 + FOCAL * fabs(point[i].y) / (z * z);
        }
        if (dx > bx || dy > by)
            counted.past_bound++;
    }
    if (dx > 1.02 || dy > 1.02) {
        counted.apart++;
        if (dx > counted.worst || dy > counted.worst) {
            counted.worst = dx > dy ? dx : dy;
            counted.worst_face = counted.faces;
        }
    }
}

/* ---- the face being queued ---- */

typedef struct {
    double x, y, z, u, v;
} Corner3;

/* The colour FxModelMaterial chose last (fx_face_material), taken by the
   face queued next. */
typedef struct {
    int8_t kind;
    uint8_t tier;
    uint16_t index;
    float light;
} Tint;

static Tint chosen = { -1, 0, 0, 0 };

static struct {
    int open, textured, whole, n;
    int back;                           /* it looks away from the eye (fx_face_back) */
    View view;
    Tint tint;
    Corner3 c[MOST];
} face;

static int back_next;

void fx_face_back(int back) { back_next = back; }

/* The picture's faces so far, for the polygon offset (fx/fx_draw.c,
   "depth"): each with its plane in view space at full size (a far
   object's points scaled back by its halvings), its outline in that plane,
   how it looks and its decal level. They are found by their normals: a
   hash of the normal's cell (NORMAL_CELL a side) over BUCKETS lists. A
   face's outline is its corners with the axis its normal is most along
   dropped (its first PLANE_POINTS; more, its box alone). */
enum { PLANES = 4096, PLANE_POINTS = 8, BUCKETS = 4096, DECAL_MOST = 63 };
#define NORMAL_CELL 0.1                 /* the normals' cells: a neighbour's are within ~6 degrees */
#define SAME_WAY 0.995                  /* two faces this near in direction (~6 degrees) can share a plane */
#define PLANE_NEAR 3.0                  /* a face's corners this near another's plane (and 0.2% of its distance) lie on it */
#define DECAL_PLANE 4.0                 /* a decal level moves a face's plane this far toward the eye (fx_draw.c) */
static struct {
    double n[3], d;
    uint32_t look;
    int cell[3], np, next, nq;
    uint8_t decal;
    double p[PLANE_POINTS][2], lo[2], hi[2];
    double q[PLANE_POINTS][3];          /* its corners (its first PLANE_POINTS) */
} planes[PLANES];
static int nplanes, bucket[BUCKETS];

/* How a face looks, for the polygon offset: two faces that look alike may
   show either where they overlap. A textured face looks like no other. */
static uint32_t look_of(const FxShape *s, int textured)
{
    static uint32_t textures;

    if (textured)
        return 0x80000000u | (textures++ & 0x7FFFFFFFu);
    return (uint32_t)(s->tint + 1) << 24 | (uint32_t)(s->tier & 3) << 22 | (uint32_t)s->material.colour << 8
           | (s->tint == 1 ? (uint32_t)(int)floor(s->light) & 0xFF : s->tint == 2 ? (uint32_t)(s->tint_index & 0xFF) : 0);
}

void fx_face_picture(void)
{
    nplanes = 0;
    memset(bucket, 0xFF, sizeof bucket);        /* none: -1 */
}

static unsigned bucket_of(const int *cell)
{
    return ((unsigned)cell[0] * 73856093u ^ (unsigned)cell[1] * 19349663u ^ (unsigned)cell[2] * 83492791u) & (BUCKETS - 1);
}

/* Whether two outlines in one plane overlap (more than touch): their
   boxes, then for convex outlines no edge of either separating them. */
static int outlines_overlap(int a, const double (*pb)[2], int nb, const double *lo, const double *hi)
{
    const double (*pa)[2] = (const double (*)[2])planes[a].p;
    int na = planes[a].np, side, i, k;

    for (k = 0; k < 2; k++) {
        double tol = 1e-4 * (planes[a].hi[k] - planes[a].lo[k] + hi[k] - lo[k]);

        if (planes[a].hi[k] <= lo[k] + tol || hi[k] <= planes[a].lo[k] + tol)
            return 0;
    }
    if (!na || !nb)
        return 1;
    for (side = 0; side < 2; side++) {
        const double (*e)[2] = side ? pb : pa;
        int ne = side ? nb : na;

        for (i = 0; i < ne; i++) {
            double ax = -(e[(i + 1) % ne][1] - e[i][1]), ay = e[(i + 1) % ne][0] - e[i][0];
            double a0 = 1e300, a1 = -1e300, b0 = 1e300, b1 = -1e300, t, tol;

            if (ax == 0 && ay == 0)
                continue;
            for (k = 0; k < na; k++) {
                t = ax * pa[k][0] + ay * pa[k][1];
                a0 = t < a0 ? t : a0;
                a1 = t > a1 ? t : a1;
            }
            for (k = 0; k < nb; k++) {
                t = ax * pb[k][0] + ay * pb[k][1];
                b0 = t < b0 ? t : b0;
                b1 = t > b1 ? t : b1;
            }
            tol = 1e-4 * (a1 - a0 + b1 - b0);
            if (a1 <= b0 + tol || b1 <= a0 + tol)
                return 0;
        }
    }
    return 1;
}

/* Whether a face (corners q, plane nv . x = d) and earlier face i share a
   plane: its corners all lie within PLANE_NEAR (and 0.2% of the plane's
   distance from the eye) of face i's plane, or face i's corners of its
   own. Corners, not the planes' distances, are compared: two faces a
   quarter degree apart (the chip's rounding of a small face's points) far
   off have distances hundreds apart while lying on one another. */
static int lies_on(int i, const double (*q)[3], int n, const double *nv, double d)
{
    double tol = PLANE_NEAR + 0.002 * fabs(planes[i].d), worst = 0, e;
    int k;

    for (k = 0; k < n; k++) {
        e = fabs(planes[i].n[0] * q[k][0] + planes[i].n[1] * q[k][1] + planes[i].n[2] * q[k][2] - planes[i].d);
        worst = e > worst ? e : worst;
    }
    if (worst <= tol)
        return 1;
    tol = PLANE_NEAR + 0.002 * fabs(d);
    worst = 0;
    for (k = 0; k < planes[i].nq; k++) {
        e = fabs(nv[0] * planes[i].q[k][0] + nv[1] * planes[i].q[k][1] + nv[2] * planes[i].q[k][2] - d);
        worst = e > worst ? e : worst;
    }
    return worst <= tol;
}

/* A face's decal level: one more than the highest of the earlier faces of
   the picture it lies over and looks unlike (look), else 0: those on its
   plane (facing the same way within SAME_WAY, its corners on the other's
   plane or the other's on its own, lies_on, as neighbouring track pieces'
   faces made from different points are: a dump of Sunset Valley's tunnel
   showed them 0.4 to 3 apart, small faces turned up to a few degrees)
   whose outlines overlap its own. A face only meeting another at
   an edge (a wall cut into triangles), or lying over one that looks the
   same (where either may show), adds none. Its plane, outline and look are
   kept for the faces after it (not while the hold is on: the ground's
   survey is not of the picture). *dist: its plane's distance from the eye. */
static uint8_t decal_of(const Corner3 *c, int n, uint32_t look, double *dist)
{
    double nv[3] = { 0, 0, 0 }, q[MOST][3], len, d, p[PLANE_POINTS][2], lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 };
    int i, k, level = 0, drop, np, cell[3], dc[3];
    double scale = ldexp(1.0, fx_far_shift());

    *dist = 0;
    for (i = 0; i < n; i++) {
        q[i][0] = c[i].x * scale;
        q[i][1] = c[i].y * scale;
        q[i][2] = c[i].z * scale;
    }
    for (i = 0; i < n; i++) {
        const double *a = q[i], *b = q[(i + 1) % n];

        nv[0] += (a[1] - b[1]) * (a[2] + b[2]);
        nv[1] += (a[2] - b[2]) * (a[0] + b[0]);
        nv[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    len = sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);
    if (len <= 0)
        return 0;
    for (i = 0; i < 3; i++) {
        nv[i] /= len;
        cell[i] = (int)floor((nv[i] + 1.0) / NORMAL_CELL);
    }
    d = nv[0] * q[0][0] + nv[1] * q[0][1] + nv[2] * q[0][2];
    *dist = fabs(d);
    drop = fabs(nv[0]) >= fabs(nv[1]) && fabs(nv[0]) >= fabs(nv[2]) ? 0 : fabs(nv[1]) >= fabs(nv[2]) ? 1 : 2;
    np = n <= PLANE_POINTS ? n : 0;
    for (i = 0; i < n; i++) {
        double u = q[i][drop == 0 ? 1 : 0], v = q[i][drop == 2 ? 1 : 2];

        if (i < np) {
            p[i][0] = u;
            p[i][1] = v;
        }
        lo[0] = u < lo[0] ? u : lo[0];
        hi[0] = u > hi[0] ? u : hi[0];
        lo[1] = v < lo[1] ? v : lo[1];
        hi[1] = v > hi[1] ? v : hi[1];
    }
    for (dc[0] = -1; dc[0] <= 1; dc[0]++)
        for (dc[1] = -1; dc[1] <= 1; dc[1]++)
            for (dc[2] = -1; dc[2] <= 1; dc[2]++) {
                int at[3] = { cell[0] + dc[0], cell[1] + dc[1], cell[2] + dc[2] };

                for (i = bucket[bucket_of(at)]; i >= 0; i = planes[i].next)
                    if (planes[i].cell[0] == at[0] && planes[i].cell[1] == at[1] && planes[i].cell[2] == at[2]
                        && planes[i].look != look && planes[i].decal + 1 > level
                        && nv[0] * planes[i].n[0] + nv[1] * planes[i].n[1] + nv[2] * planes[i].n[2] > SAME_WAY
                        && lies_on(i, (const double (*)[3])q, n, nv, d)
                        && outlines_overlap(i, (const double (*)[2])p, np, lo, hi))
                        level = planes[i].decal + 1;
            }
    if (level > DECAL_MOST)
        level = DECAL_MOST;
    if (nplanes < PLANES && !fx_draw_holding()) {
        unsigned b = bucket_of(cell);

        memcpy(planes[nplanes].n, nv, sizeof nv);
        memcpy(planes[nplanes].cell, cell, sizeof cell);
        planes[nplanes].d = d;
        planes[nplanes].look = look;
        planes[nplanes].decal = (uint8_t)level;
        planes[nplanes].np = np;
        memcpy(planes[nplanes].p, p, sizeof p);
        planes[nplanes].nq = n < PLANE_POINTS ? n : PLANE_POINTS;
        memcpy(planes[nplanes].q, q, sizeof planes[nplanes].q);
        for (k = 0; k < 2; k++) {
            planes[nplanes].lo[k] = lo[k];
            planes[nplanes].hi[k] = hi[k];
        }
        planes[nplanes].next = bucket[b];
        bucket[b] = nplanes;
        nplanes++;
    }
    return (uint8_t)level;
}

static FxShape shapes[SHAPES];
static int nshapes;
static FxCorner corners[CORNERS];
static uint32_t ncorners;

static void commit(const FxMaterial *m);

/* The model's distance tier is the place of its colour table ($0052) past
   the tables' base ($004E), $88 bytes a tier (FxModelProject), or of its
   light table among the four of its set ($2C bytes each: $A1C5 near, $A275
   far). A lit face's row is kept as the nearest tier's, the haze added
   apart. */
void fx_face_material(int kind, uint16_t index, uint16_t table, uint16_t light)
{
    unsigned tier = (uint16_t)(word(0x0052) - word(0x004E)) / 0x88u;

    chosen.kind = (int8_t)kind;
    chosen.index = index;
    chosen.light = 0;
    if (kind == 1) {
        uint16_t set = table >= 0xA275 ? 0xA275 : 0xA1C5, at = (uint16_t)(set + 2 * index);

        tier = (uint16_t)(table - set) / 0x2Cu;
        chosen.index = (uint16_t)(fx_rom_read(3, at) | fx_rom_read(3, (uint16_t)(at + 1)) << 8);
        chosen.light = (float)((int16_t)light / 1024.0);
    }
    chosen.tier = (uint8_t)(tier > 3 ? 3 : tier);
}

void fx_face_begin(int textured)
{
    face.open = 1;
    face.textured = textured != 0;
    face.tint = chosen;
    chosen.kind = -1;
    face.back = back_next;
    back_next = 0;
    face.whole = 1;
    face.n = 0;
    face.view = view_now();
    if (checking)
        counted.faces++;
}

void fx_face_vertex(uint8_t p, uint8_t u, uint8_t v)
{
    Corner3 *c;

    if (!face.open)
        return;
    if (face.n == MOST || point[p].made != projection || !projection) {
        face.whole = 0;
        return;
    }
    c = &face.c[face.n++];
    c->x = point[p].x;
    c->y = point[p].y;
    c->z = point[p].z;
    c->u = u;
    c->v = v;
    if (checking)
        check_point(p, &face.view);
}

void fx_face_end(const FxMaterial *material)
{
    if (!face.open)
        return;
    face.open = 0;
    commit(material);
}

void fx_face_reset(void)
{
    face.open = 0;
    nshapes = 0;
    ncorners = 0;
}

uint32_t fx_face_count(void) { return (uint32_t)nshapes; }

const FxShape *fx_face_at(uint32_t i) { return &shapes[i]; }

int fx_face_full(void)
{
    /* a face's most: its clipped polygon fanned, twice over when the water
       cuts it */
    return nshapes >= SHAPES - 1 || ncorners + 2u * 3u * WORK > CORNERS;
}

const FxCorner *fx_face_corners(void) { return corners; }

/* ---- making the shape ---- */

/* The part of the polygon on one side of z = at: above (keep_far) or
   below; the crossing corners interpolated in space. */
static int clip_depth(const Corner3 *in, int n, Corner3 *out, double at, int keep_far)
{
    int i, m = 0;

    for (i = 0; i < n; i++) {
        const Corner3 *a = &in[i], *b = &in[(i + 1) % n];
        int ain = keep_far ? a->z >= at : a->z <= at, bin = keep_far ? b->z >= at : b->z <= at;

        if (ain)
            out[m++] = *a;
        if (ain != bin) {
            double t = (at - a->z) / (b->z - a->z);

            out[m].x = a->x + (b->x - a->x) * t;
            out[m].y = a->y + (b->y - a->y) * t;
            out[m].z = at;
            out[m].u = a->u + (b->u - a->u) * t;
            out[m].v = a->v + (b->v - a->v) * t;
            m++;
        }
    }
    return m;
}

typedef struct {
    double x, y, u, v, iz;              /* iz: one over the depth, linear across the screen */
} Corner2;

/* The part of the projected polygon on one side of a line x = at (axis 0)
   or y = at (axis 1): keep_high keeps the side above. */
static int clip_side(const Corner2 *in, int n, Corner2 *out, int axis, double at, int keep_high)
{
    int i, m = 0;

    for (i = 0; i < n; i++) {
        const Corner2 *a = &in[i], *b = &in[(i + 1) % n];
        double av = axis ? a->y : a->x, bv = axis ? b->y : b->x;
        int ain = keep_high ? av >= at : av <= at, bin = keep_high ? bv >= at : bv <= at;

        if (ain)
            out[m++] = *a;
        if (ain != bin) {
            double t = (at - av) / (bv - av);

            out[m].x = a->x + (b->x - a->x) * t;
            out[m].y = a->y + (b->y - a->y) * t;
            out[m].u = a->u + (b->u - a->u) * t;
            out[m].v = a->v + (b->v - a->v) * t;
            out[m].iz = a->iz + (b->iz - a->iz) * t;
            if (axis)
                out[m].y = at;
            else
                out[m].x = at;
            m++;
        }
    }
    return m;
}

static void project(const Corner3 *in, int n, Corner2 *out, const View *v)
{
    int i;

    for (i = 0; i < n; i++) {
        place(v, in[i].x, in[i].y, in[i].z, &out[i].x, &out[i].y);
        out[i].u = in[i].u;
        out[i].v = in[i].v;
        out[i].iz = 1.0 / in[i].z;
    }
}

/* The projected polygon kept to the view's rectangle and fanned into the
   shape's corners. Returns 0 when the corners are full. */
static int fan(const Corner3 *in, int n, const View *v)
{
    Corner2 a[WORK], b[WORK];
    int i;

    project(in, n, a, v);
    n = clip_side(a, n, b, 0, v->left, 1);
    n = clip_side(b, n, a, 0, v->right, 0);
    n = clip_side(a, n, b, 1, v->top, 1);
    n = clip_side(b, n, a, 1, v->bottom, 0);
    if (n < 3)
        return 1;
    if (ncorners + (uint32_t)(n - 2) * 3 > CORNERS)
        return 0;
    for (i = 1; i + 1 < n; i++) {
        const Corner2 *t[3] = { &a[0], &a[i], &a[i + 1] };
        int j;

        for (j = 0; j < 3; j++) {
            FxCorner *c = &corners[ncorners++];

            c->x = (float)t[j]->x;
            c->y = (float)t[j]->y;
            c->u = (float)t[j]->u;
            c->v = (float)t[j]->v;
            c->iz = (float)ldexp(t[j]->iz, -fx_far_shift());   /* a far object's points are halved */
        }
    }
    return 1;
}

/* A solid's face (fx_solid.c): n view-space points, clipped against the
   near plane, projected and kept to the view's rectangle, fanned into
   `out`. Returns the corners made (0: nothing seen, or no room). WORK
   bounds n. */
uint32_t fx_face_triangles(const float (*p)[3], int n, FxCorner *out, uint32_t room)
{
    double q[MOST][3];
    int i, j;

    if (n < 3 || n > MOST)
        return 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < 3; j++)
            q[i][j] = p[i][j];
    return fx_face_triangles_d((const double (*)[3])q, n, out, room);
}

uint32_t fx_face_triangles_d(const double (*p)[3], int n, FxCorner *out, uint32_t room)
{
    Corner3 a[WORK], b[WORK];
    Corner2 c[WORK], d[WORK];
    View v = view_now();
    int i, m;
    uint32_t k = 0;

    if (n < 3 || n > MOST)
        return 0;
    for (i = 0; i < n; i++) {
        a[i].x = p[i][0];
        a[i].y = p[i][1];
        a[i].z = p[i][2];
        a[i].u = a[i].v = 0;
    }
    m = clip_depth(a, n, b, NEAR_Z, 1);
    if (m < 3)
        return 0;
    project(b, m, c, &v);
    m = clip_side(c, m, d, 0, v.left, 1);
    m = clip_side(d, m, c, 0, v.right, 0);
    m = clip_side(c, m, d, 1, v.top, 1);
    m = clip_side(d, m, c, 1, v.bottom, 0);
    if (m < 3 || (uint32_t)(m - 2) * 3 > room)
        return 0;
    for (i = 1; i + 1 < m; i++) {
        const Corner2 *t[3] = { &c[0], &c[i], &c[i + 1] };
        int j;

        for (j = 0; j < 3; j++) {
            out[k].x = (float)t[j]->x;
            out[k].y = (float)t[j]->y;
            out[k].u = out[k].v = 0;
            out[k].iz = (float)ldexp(t[j]->iz, -fx_far_shift());
            k++;
        }
    }
    return k;
}

/* A line: its segment kept in front of the near plane and in the view's
   rectangle (each end clipped on the line's own parameter). */
static int line(const Corner3 *in, const View *v)
{
    Corner3 p[2] = { in[0], in[1] };
    double x[2], y[2], t0 = 0, t1 = 1, dx, dy;
    int i;

    for (i = 0; i < 2; i++)
        if (p[i].z < NEAR_Z) {
            const Corner3 *o = &p[!i];
            double t;

            if (o->z < NEAR_Z)
                return 1;
            t = (NEAR_Z - p[i].z) / (o->z - p[i].z);
            p[i].x += (o->x - p[i].x) * t;
            p[i].y += (o->y - p[i].y) * t;
            p[i].z = NEAR_Z;
        }
    for (i = 0; i < 2; i++)
        place(v, p[i].x, p[i].y, p[i].z, &x[i], &y[i]);
    dx = x[1] - x[0];
    dy = y[1] - y[0];
    {
        /* Liang-Barsky against the four sides */
        double pq[4][2] = { { -dx, x[0] - v->left }, { dx, v->right - x[0] }, { -dy, y[0] - v->top }, { dy, v->bottom - y[0] } };

        for (i = 0; i < 4; i++) {
            double pp = pq[i][0], q = pq[i][1];

            if (pp == 0) {
                if (q < 0)
                    return 1;
            } else if (pp < 0) {
                if (q / pp > t1)
                    return 1;
                if (q / pp > t0)
                    t0 = q / pp;
            } else {
                if (q / pp < t0)
                    return 1;
                if (q / pp < t1)
                    t1 = q / pp;
            }
        }
    }
    if (ncorners + 2 > CORNERS)
        return 0;
    {
        double iz0 = ldexp(1.0 / p[0].z, -fx_far_shift()), iz1 = ldexp(1.0 / p[1].z, -fx_far_shift());

        corners[ncorners].x = (float)(x[0] + dx * t0);
        corners[ncorners].y = (float)(y[0] + dy * t0);
        corners[ncorners].u = corners[ncorners].v = 0;
        corners[ncorners].iz = (float)(iz0 + (iz1 - iz0) * t0);
        ncorners++;
        corners[ncorners].x = (float)(x[0] + dx * t1);
        corners[ncorners].y = (float)(y[0] + dy * t1);
        corners[ncorners].u = corners[ncorners].v = 0;
        corners[ncorners].iz = (float)(iz0 + (iz1 - iz0) * t1);
        ncorners++;
    }
    return 1;
}

/* ---- the water's cut (fx_ground.c: Aqua Tunnel) ----

   While a picture's water cuts the picture (fx_face_cut), each face is cut
   by its plane n . v = d in view space: the part on the far side of it from
   the eye is kept apart (FxShape under_*) and drawn before the water, the
   rest after it. Where a model crosses the plane (a track piece going below
   the water), the region its faces' crossings enclose on the plane is a hole
   in the water: what lies past the water is drawn again inside the hole,
   after the water (fx_draw.c).

   A far object (fx_far.c) is drawn with its view-space points halved
   fx_far_shift() times, its projection the same: its points are tested
   against the plane at that scale (cut_d), and its crossings kept at full
   size for its hole. Tested at full size, a far piece under the water
   (Aqua Tunnel's glass tube) counted as above it and was drawn after the
   water, over it (Jeff's picture, 2026-11-11). */

static struct {
    int on;
    double n[3], d;
} cut;

enum { HOLES = 64, HOLE_POINTS = 256, HOLE_MOST = 32 };

static FxHole holes[HOLES];
static int nholes;
static double hole_point[HOLE_POINTS][3];
static int nhole_points;
static uint32_t hole_model;             /* the projection the points are of */

void fx_face_cut(int on, const double n[3], double d)
{
    cut.on = on;
    if (on) {
        memcpy(cut.n, n, sizeof cut.n);
        cut.d = d;
    }
    nholes = 0;
    nhole_points = 0;
    hole_model = 0;
}

int fx_face_cutting(void) { return cut.on; }

/* the plane's distance from the eye at the scale of the points being drawn */
static double cut_d(void)
{
    return ldexp(cut.d, -fx_far_shift());
}

/* a view-space point's side of the water: 1 past it from the eye */
int fx_face_past_water(double x, double y, double z)
{
    double s;

    if (!cut.on)
        return 0;
    s = cut.n[0] * x + cut.n[1] * y + cut.n[2] * z - cut_d();
    return cut.d > 0 ? s > 0 : s < 0;
}

static double side_of(const Corner3 *c)
{
    double s = cut.n[0] * c->x + cut.n[1] * c->y + cut.n[2] * c->z - cut_d();

    return cut.d > 0 ? s : -s;          /* > 0: past the water */
}

/* the part of a polygon on one side of the water: past it (past) or not */
static int clip_water(const Corner3 *in, int n, Corner3 *out, int past)
{
    int i, m = 0;

    for (i = 0; i < n; i++) {
        const Corner3 *a = &in[i], *b = &in[(i + 1) % n];
        double sa = side_of(a), sb = side_of(b);
        int ain = past ? sa >= 0 : sa <= 0, bin = past ? sb >= 0 : sb <= 0;

        if (ain)
            out[m++] = *a;
        if (ain != bin) {
            double t = sa / (sa - sb);

            out[m].x = a->x + (b->x - a->x) * t;
            out[m].y = a->y + (b->y - a->y) * t;
            out[m].z = a->z + (b->z - a->z) * t;
            out[m].u = a->u + (b->u - a->u) * t;
            out[m].v = a->v + (b->v - a->v) * t;
            m++;
        }
    }
    return m;
}

/* The points where a face's edges cross the water, kept for its model's
   hole, at full size (a far object's scaled back). */
static void hole_points_of(const Corner3 *in, int n)
{
    int i, shift = fx_far_shift();

    for (i = 0; i < n && nhole_points < HOLE_POINTS; i++) {
        const Corner3 *a = &in[i], *b = &in[(i + 1) % n];
        double sa = side_of(a), sb = side_of(b), t;

        if ((sa > 0) == (sb > 0) || sa == sb)
            continue;
        t = sa / (sa - sb);
        hole_point[nhole_points][0] = ldexp(a->x + (b->x - a->x) * t, shift);
        hole_point[nhole_points][1] = ldexp(a->y + (b->y - a->y) * t, shift);
        hole_point[nhole_points][2] = ldexp(a->z + (b->z - a->z) * t, shift);
        nhole_points++;
    }
}

static int hull_turn(const double *o, const double *a, const double *b)
{
    double c = (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);

    return c > 0;
}

static int by_x(const void *pa, const void *pb)
{
    const double *a = pa, *b = pb;

    return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : a[1] < b[1] ? -1 : a[1] > b[1];
}

/* The model's crossings made into a hole: their convex hull on the plane
   (in two directions across it), kept when it encloses more than a sliver
   (a road crossing the water makes a line, not a hole), clipped against the
   near plane and projected. */
static void hole_finish(void)
{
    double e1[3], e2[3], q[HOLE_POINTS][5], hull[2 * HOLE_POINTS][5], area = 0;
    Corner3 h3[HOLE_MOST + 8], c3[HOLE_MOST + 8];
    View v = view_now();
    int n = nhole_points, i, k = 0, t, m;
    FxHole *hole;

    nhole_points = 0;
    if (n < 3 || nholes == HOLES)
        return;
    /* two directions across the plane: n x (an axis not along n) */
    {
        double a[3] = { fabs(cut.n[0]) < 0.9 ? 1 : 0, fabs(cut.n[0]) < 0.9 ? 0 : 1, 0 }, l;

        e1[0] = cut.n[1] * a[2] - cut.n[2] * a[1];
        e1[1] = cut.n[2] * a[0] - cut.n[0] * a[2];
        e1[2] = cut.n[0] * a[1] - cut.n[1] * a[0];
        l = sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
        for (i = 0; i < 3; i++)
            e1[i] /= l;
        e2[0] = cut.n[1] * e1[2] - cut.n[2] * e1[1];
        e2[1] = cut.n[2] * e1[0] - cut.n[0] * e1[2];
        e2[2] = cut.n[0] * e1[1] - cut.n[1] * e1[0];
    }
    for (i = 0; i < n; i++) {
        q[i][0] = hole_point[i][0] * e1[0] + hole_point[i][1] * e1[1] + hole_point[i][2] * e1[2];
        q[i][1] = hole_point[i][0] * e2[0] + hole_point[i][1] * e2[1] + hole_point[i][2] * e2[2];
        q[i][2] = hole_point[i][0];
        q[i][3] = hole_point[i][1];
        q[i][4] = hole_point[i][2];
    }
    qsort(q, (size_t)n, sizeof q[0], by_x);
    for (i = 0; i < n; i++) {           /* the lower hull, then the upper */
        while (k >= 2 && !hull_turn(hull[k - 2], hull[k - 1], q[i]))
            k--;
        memcpy(hull[k++], q[i], sizeof q[0]);
    }
    for (i = n - 2, t = k + 1; i >= 0; i--) {
        while (k >= t && !hull_turn(hull[k - 2], hull[k - 1], q[i]))
            k--;
        memcpy(hull[k++], q[i], sizeof q[0]);
    }
    k--;
    if (k < 3 || k > HOLE_MOST)
        return;
    for (i = 0; i < k; i++)
        area += hull[i][0] * hull[(i + 1) % k][1] - hull[(i + 1) % k][0] * hull[i][1];
    if (fabs(area) / 2 < HOLE_LEAST)
        return;
    for (i = 0; i < k; i++) {
        h3[i].x = hull[i][2];
        h3[i].y = hull[i][3];
        h3[i].z = hull[i][4];
        h3[i].u = h3[i].v = 0;
    }
    m = clip_depth(h3, k, c3, NEAR_Z, 1);
    if (m < 3)
        return;
    hole = &holes[nholes++];
    hole->n = m;
    for (i = 0; i < m; i++) {
        double x, y;

        place(&v, c3[i].x, c3[i].y, c3[i].z, &x, &y);
        hole->x[i] = (float)x;
        hole->y[i] = (float)y;
    }
}

const FxHole *fx_face_holes(int *n)
{
    if (cut.on && nhole_points)
        hole_finish();
    *n = cut.on ? nholes : 0;
    return holes;
}

/* A polygon in view space made into the shape's corners (near-clipped, cut
   by depth when textured, fanned). Returns 0 when the corners are full. */
static int polygon_corners(const Corner3 *in, int n0, int textured)
{
    Corner3 a[WORK];
    int n, ok = 1;

    n = clip_depth(in, n0, a, NEAR_Z, 1);
    if (n >= 3 && textured) {
        double zmin = a[0].z, zmax = a[0].z, r;
        int i, m = 1;

        for (i = 1; i < n; i++) {
            if (a[i].z < zmin)
                zmin = a[i].z;
            if (a[i].z > zmax)
                zmax = a[i].z;
        }
        if (zmax > zmin * CUT) {
            m = (int)ceil(log(zmax / zmin) / log(CUT));
            if (m > 16)
                m = 16;
        }
        r = pow(zmax / zmin, 1.0 / m);
        for (i = 0; i < m && ok; i++) {
            Corner3 b[WORK], c[WORK];
            int k = n;

            memcpy(b, a, sizeof(Corner3) * (size_t)n);
            if (i > 0)
                k = clip_depth(a, k, b, zmin * pow(r, i), 1);
            if (i < m - 1) {
                k = clip_depth(b, k, c, zmin * pow(r, i + 1), 0);
                memcpy(b, c, sizeof(Corner3) * (size_t)k);
            }
            if (k >= 3)
                ok = fan(b, k, &face.view);
        }
    } else if (n >= 3) {
        ok = fan(a, n, &face.view);
    }
    return ok;
}

/* A flat face seen from its back (fx_face_back) shaded by its own shape,
   by the side the eye sees: its normal (Newell's, turned toward the eye),
   the light as the chip lights a face (fx_ground_light). A lit face takes
   its shade row's colour at that level, as the chip colours a lit face
   turned that way; its material's light is not used, as a face's material
   may light it by another face's normal (a two-sided part's two faces share
   one). A face painted in the course's own rock (fx_ground_rock_back) takes
   the rock's shade for a face turned that way, as the land's rock is
   shaded; any other keeps its colour. */
static void back_shade(FxShape *s)
{
    double n[3] = { 0, 0, 0 }, light;
    int i, c;

    for (i = 0; i < face.n; i++) {
        const Corner3 *a = &face.c[i], *b = &face.c[(i + 1) % face.n];

        n[0] += (a->y - b->y) * (a->z + b->z);
        n[1] += (a->z - b->z) * (a->x + b->x);
        n[2] += (a->x - b->x) * (a->y + b->y);
    }
    if (n[0] * face.c[0].x + n[1] * face.c[0].y + n[2] * face.c[0].z > 0)
        for (i = 0; i < 3; i++)
            n[i] = -n[i];
    if (face.tint.kind == 1) {
        double level;

        light = fx_ground_light(n);
        level = floor(light);
        level = level < 6 ? 6 : level > 15 ? 15 : level;
        s->material.colour = fx_rom_read(3, (uint16_t)(face.tint.index + (int)level - 6));
        s->light = (float)light;
        return;
    }
    c = fx_ground_rock_back(s->material.colour, n);
    if (c >= 0)
        s->material.colour = (uint8_t)c;
}

/* The face noted made into a shape with its material. A face not wholly
   known (a point not projected with this model's table) makes none. */
static void commit(const FxMaterial *m)
{
    FxShape *s;
    uint32_t first = ncorners;
    int ok = 1;

    if (!face.whole || face.n < 2 || nshapes == SHAPES)
        return;
    {                                   /* a car seated on the road (fx_seat.c), or a surface for the cars */
        double sc[MOST][3];
        int i;

        for (i = 0; i < face.n; i++) {
            sc[i][0] = face.c[i].x;
            sc[i][1] = face.c[i].y;
            sc[i][2] = face.c[i].z;
        }
        fx_seat_face(sc, face.n);
        for (i = 0; i < face.n; i++) {
            face.c[i].x = sc[i][0];
            face.c[i].y = sc[i][1];
            face.c[i].z = sc[i][2];
        }
    }
    if (survey_face && face.n >= 3) {
        double c[MOST][3];
        int i;

        for (i = 0; i < face.n; i++) {
            c[i][0] = face.c[i].x;
            c[i][1] = face.c[i].y;
            c[i][2] = face.c[i].z;
        }
        survey_face((const double (*)[3])c, face.n, m->colour, face.textured, !face.textured && face.tint.kind == 1,
                    face.tint.light, survey_context);
    }
    s = &shapes[nshapes];
    s->material = *m;
    s->line = face.n == 2;
    s->tint = face.textured ? -1 : face.tint.kind;
    s->tier = face.tint.tier;
    s->tint_index = face.tint.index;
    s->light = face.tint.light;
    s->under_first = s->under_count = 0;
    s->decal = 0;
    s->decal_k = 0;
    s->back = face.back != 0;
    if (face.back && !face.textured && face.n >= 3)
        back_shade(s);
    if (face.n >= 3) {
        double dist = 0;

        s->decal = decal_of(face.c, face.n, look_of(s, face.textured), &dist);
        s->decal_k = (float)(dist > 2 * DECAL_PLANE ? DECAL_PLANE / dist : 0.5);
    }
    if (s->line) {
        ok = line(face.c, &face.view);
    } else if (cut.on) {
        Corner3 near_part[WORK], past_part[WORK];
        int nn, np;

        if (hole_model != projection) {
            if (nhole_points)
                hole_finish();
            hole_model = projection;
        }
        hole_points_of(face.c, face.n);
        nn = clip_water(face.c, face.n, near_part, 0);
        np = clip_water(face.c, face.n, past_part, 1);
        if (nn >= 3)
            ok = polygon_corners(near_part, nn, face.textured);
        s->first = first;
        s->count = ncorners - first;
        if (ok && np >= 3) {
            s->under_first = ncorners;
            ok = polygon_corners(past_part, np, face.textured);
            s->under_count = ncorners - s->under_first;
        }
        if (!ok) {
            ncorners = first;
            return;
        }
        nshapes++;
        return;
    } else {
        ok = polygon_corners(face.c, face.n, face.textured);
    }
    if (!ok) {
        ncorners = first;
        return;
    }
    s->first = first;
    s->count = ncorners - first;
    nshapes++;
}

/* ---- scaled sprites ---- */

static struct {
    int set, past;
    uint16_t left, top, bx, by;
    double cx, cy, k;
} sprite;

/* FxObjectSprite's place: x, y and depth the object's view-space place,
   size the size it scaled by the depth (0 for a sprite of fixed size), the
   chip's centre and size in whole pixels after its cap. */
void fx_sprite_place(uint16_t x, uint16_t y, uint16_t depth, uint16_t size, uint16_t centre_x, uint16_t centre_y, uint16_t screen_size)
{
    View v = view_now();
    double d = depth ? depth : 1, half = (screen_size >> 1) + (screen_size & 1);

    double at[3] = { (int16_t)x, (int16_t)y, d };

    fx_seat_sprite(at);                 /* a car's moved with it, one under the road onto it (fx_seat.c) */
    d = at[2] > 1 ? at[2] : 1;
    sprite.set = screen_size != 0;
    sprite.past = fx_face_past_water(at[0], at[1], d);
    sprite.cx = v.cx + FOCAL * at[0] / d;
    sprite.cy = v.cy + FOCAL * at[1] / d;
    sprite.k = size ? (FOCAL * size / d) / screen_size : 1.0;
    sprite.bx = centre_x;
    sprite.by = centre_y;
    sprite.left = (uint16_t)(centre_x - half);
    sprite.top = (uint16_t)(centre_y - half);
}

int fx_sprite_past_water(void) { return sprite.set && sprite.past; }

int fx_sprite_map(uint16_t left, uint16_t top, float *cx, float *cy, float *bx, float *by, float *k)
{
    if (!sprite.set || left != sprite.left || top != sprite.top)
        return 0;
    sprite.set = 0;
    *cx = (float)sprite.cx;
    *cy = (float)sprite.cy;
    *bx = (int16_t)sprite.bx;
    *by = (int16_t)sprite.by;
    *k = (float)sprite.k;
    return 1;
}
