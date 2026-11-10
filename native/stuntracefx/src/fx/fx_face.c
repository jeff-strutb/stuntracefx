/*
 * fx_face.c: the race's shapes in floats (plan stage 10, "Stage 10's 3D").
 *
 * The frame program goes on deciding what is drawn at the console's scale:
 * which objects, which level of detail, which faces face the eye, their
 * order and materials. Where it is drawn is made again here, from the
 * view-space points the program projected, unrounded and unheld:
 *
 *   FxModelProject   each point's view-space sum (the model's point at
 *                    $0340 + 6i plus the object's place at $0026-$002A, in
 *                    16 bits as the chip adds them) is kept beside its
 *                    projected record at $04E4 + 6i (fx_face_point);
 *   FxModelFaces     each face's queue entry, its points and texels are
 *                    noted (fx_face_begin, fx_face_vertex);
 *   FxDrawList       the faces whose entries the queue kept are made into
 *                    shapes (fx_face_settle) and drawn in place of the
 *                    chip's entries, from the face's first to its last.
 *
 * A shape is the face clipped against a near plane in view space,
 * projected x = cx + 128 X / Z, y = cy + 128 Y / Z, clipped against the
 * view's rectangle, and fanned. A textured face is cut by planes of equal
 * depth until each piece's far over near depth is at most 1.25, so each
 * piece's texture, laid on by the GPU across the screen, stays near its
 * place in space. Nothing here makes a pixel.
 */
#include <math.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_face.h"

enum {
    POINTS = 256,               /* a model's point records ($04E4 + 6i, i a byte) */
    MOST = 64,                  /* a face's points: the chip's are 2 to 8, a solid's cap up to 60 */
    WORK = MOST + 8,            /* each clip adds at most one */
    SHAPES = 512,
    CORNERS = 32768,
    TABLE = 0x04E4,
    LIST_END = 0x005C
};

#define FOCAL 128.0             /* FxRatioClamp: (R1 / 2) * 256 / R3; the far table the same */
#define NEAR_Z 1.0              /* the near plane: the chip's clip is at 0, its depth 0 taken as 1 */
#define CUT 1.25                /* a textured piece's far over near depth */
#define CHIP_FAR 17406.0        /* $43FE: the depth FxModelProject's far table stops at */

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

void fx_face_point(uint16_t record, uint16_t x, uint16_t y, uint16_t z)
{
    unsigned i = (uint16_t)(record - TABLE) / 6u;

    if (i >= POINTS)
        return;
    if (i == 0)                         /* every projection starts at the first point */
        projection++;
    point[i].x = (int16_t)x;
    point[i].y = (int16_t)y;
    point[i].z = (int16_t)z;
    point[i].made = projection;
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

static struct {
    int open, textured, whole, n;
    uint16_t entry;
    View view;
    Corner3 c[MOST];
} face;

static FxShape shapes[SHAPES];
static int nshapes, cursor;
static FxCorner corners[CORNERS];
static uint32_t ncorners;

static void commit(uint16_t end);

/* The face noted last is settled: kept when the queue's end has moved past
   its entry (the chip queued it), dropped when it has not. */
static void settle_face(void)
{
    uint16_t end;

    if (!face.open)
        return;
    face.open = 0;
    end = word(LIST_END);
    if (end > face.entry)
        commit(end);
}

void fx_face_begin(uint16_t entry, int textured)
{
    settle_face();
    face.open = 1;
    face.entry = entry;
    face.textured = textured != 0;
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

void fx_face_settle(void)
{
    settle_face();
    cursor = 0;
}

void fx_face_reset(void)
{
    face.open = 0;
    nshapes = 0;
    ncorners = 0;
    cursor = 0;
}

const FxShape *fx_face_shape(uint16_t entry)
{
    while (cursor < nshapes && shapes[cursor].entry < entry)
        cursor++;
    if (cursor < nshapes && shapes[cursor].entry == entry)
        return &shapes[cursor];
    return NULL;
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
    double x, y, u, v;
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
    corners[ncorners].x = (float)(x[0] + dx * t0);
    corners[ncorners].y = (float)(y[0] + dy * t0);
    corners[ncorners].u = corners[ncorners].v = 0;
    ncorners++;
    corners[ncorners].x = (float)(x[0] + dx * t1);
    corners[ncorners].y = (float)(y[0] + dy * t1);
    corners[ncorners].u = corners[ncorners].v = 0;
    ncorners++;
    return 1;
}

/* The face noted, queued from its entry up to `end`, made into a shape. A
   face not wholly known (a point not projected with this model's table)
   makes none, and the chip's entries are drawn as they stand. */
static void commit(uint16_t end)
{
    Corner3 a[WORK];
    FxShape *s;
    uint32_t first = ncorners;
    int n, ok = 1;

    if (!face.whole || face.n < 2 || nshapes == SHAPES)
        return;
    s = &shapes[nshapes];
    s->entry = face.entry;
    s->end = end;
    s->line = face.n == 2;
    if (s->line) {
        ok = line(face.c, &face.view);
    } else {
        n = clip_depth(face.c, face.n, a, NEAR_Z, 1);
        if (n >= 3 && face.textured) {
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
    int set;
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

    sprite.set = screen_size != 0;
    sprite.cx = v.cx + FOCAL * (int16_t)x / d;
    sprite.cy = v.cy + FOCAL * (int16_t)y / d;
    sprite.k = size ? (FOCAL * size / d) / screen_size : 1.0;
    sprite.bx = centre_x;
    sprite.by = centre_y;
    sprite.left = (uint16_t)(centre_x - half);
    sprite.top = (uint16_t)(centre_y - half);
}

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
