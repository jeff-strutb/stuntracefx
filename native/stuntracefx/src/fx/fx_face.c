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
#include "fx/fx_pattern.h"

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
    uint16_t entry;
    View view;
    Tint tint;
    Corner3 c[MOST];
} face;

static FxShape shapes[SHAPES];
static int nshapes, cursor;
static FxCorner corners[CORNERS];
static uint32_t ncorners;

static void commit(uint16_t end);

/* ---- pattern faces (fx_pattern.c): kept whole, with their view, and cut
   when drawn with the draw list's texture offsets ---- */

enum { PATTERN_FACES = 256 };

static struct {
    const FxPattern *pattern;
    View view;
    int n;
    Corner3 c[MOST];
} pattern_faces[PATTERN_FACES];
static int npatterns;

/* The texture's key from the face's draw-list entry, laid out as
   FxDrawList reads it: the kind, the bank, the colour and options word,
   the base and the mask. */
static const FxPattern *entry_pattern(uint16_t entry)
{
    uint16_t p = (uint16_t)(entry + 1);
    uint8_t bank = fx_ram[p], options = (uint8_t)(fx_ram[(uint16_t)(p + 2)] & 0x1F);
    uint16_t base = (uint16_t)(fx_ram[(uint16_t)(p + 3)] | fx_ram[(uint16_t)(p + 4)] << 8);
    uint16_t mask = (uint16_t)(fx_ram[(uint16_t)(p + 5)] | fx_ram[(uint16_t)(p + 6)] << 8);

    return fx_pattern_find(bank, base, mask, options);
}

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

void fx_face_begin(uint16_t entry, int textured)
{
    settle_face();
    face.open = 1;
    face.entry = entry;
    face.textured = textured != 0;
    face.tint = chosen;
    chosen.kind = -1;
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
    npatterns = 0;
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

/* ---- the water's cut (fx_ground.c: Aqua Tunnel) ----

   While a picture's water cuts the picture (fx_face_cut), each face is cut
   by its plane n . v = d in view space: the part on the far side of it from
   the eye is kept apart (FxShape under_*) and drawn before the water, the
   rest after it. Where a model crosses the plane (a track piece going below
   the water), the region its faces' crossings enclose on the plane is a hole
   in the water: what lies past the water is drawn again inside the hole,
   after the water (fx_draw.c). */

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

/* a view-space point's side of the water: 1 past it from the eye */
int fx_face_past_water(double x, double y, double z)
{
    double s;

    if (!cut.on)
        return 0;
    s = cut.n[0] * x + cut.n[1] * y + cut.n[2] * z - cut.d;
    return cut.d > 0 ? s > 0 : s < 0;
}

static double side_of(const Corner3 *c)
{
    double s = cut.n[0] * c->x + cut.n[1] * c->y + cut.n[2] * c->z - cut.d;

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
   hole. */
static void hole_points_of(const Corner3 *in, int n)
{
    int i;

    for (i = 0; i < n && nhole_points < HOLE_POINTS; i++) {
        const Corner3 *a = &in[i], *b = &in[(i + 1) % n];
        double sa = side_of(a), sb = side_of(b), t;

        if ((sa > 0) == (sb > 0) || sa == sb)
            continue;
        t = sa / (sa - sb);
        hole_point[nhole_points][0] = a->x + (b->x - a->x) * t;
        hole_point[nhole_points][1] = a->y + (b->y - a->y) * t;
        hole_point[nhole_points][2] = a->z + (b->z - a->z) * t;
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

/* The face noted, queued from its entry up to `end`, made into a shape. A
   face not wholly known (a point not projected with this model's table)
   makes none, and the chip's entries are drawn as they stand. */
static void commit(uint16_t end)
{
    FxShape *s;
    uint32_t first = ncorners;
    int ok = 1;

    if (!face.whole || face.n < 2 || nshapes == SHAPES)
        return;
    if (survey_face && face.n >= 3) {
        double c[MOST][3];
        int i;

        for (i = 0; i < face.n; i++) {
            c[i][0] = face.c[i].x;
            c[i][1] = face.c[i].y;
            c[i][2] = face.c[i].z;
        }
        survey_face((const double (*)[3])c, face.n, fx_ram[(uint16_t)(face.entry + 1)], face.textured, survey_context);
    }
    s = &shapes[nshapes];
    s->entry = face.entry;
    s->end = end;
    s->line = face.n == 2;
    s->tint = face.textured ? -1 : face.tint.kind;
    s->tier = face.tint.tier;
    s->tint_index = face.tint.index;
    s->light = face.tint.light;
    s->under_first = s->under_count = 0;
    s->pattern = 0;
    if (!s->line && face.textured && npatterns < PATTERN_FACES) {
        const FxPattern *pattern = entry_pattern(face.entry);

        if (pattern) {
            pattern_faces[npatterns].pattern = pattern;
            pattern_faces[npatterns].view = face.view;
            pattern_faces[npatterns].n = face.n;
            memcpy(pattern_faces[npatterns].c, face.c, sizeof(Corner3) * (size_t)face.n);
            s->pattern = (uint16_t)++npatterns;
        }
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

/* ---- a pattern face's pieces ---- */

typedef struct {
    const View *view;
    FxCorner *out;
    uint8_t *values, *past;
    uint32_t room, made;
    int full;
} PatternCut;

/* A flat piece's part on one side of the water (past: 1 past it from the
   eye): near clip, projection, the view's rectangle, fanned. */
static void piece_part(PatternCut *pc, const Corner3 *a, int n, uint8_t value, uint8_t past)
{
    Corner3 b[WORK];
    Corner2 c[WORK], d[WORK];
    int i, m;

    m = clip_depth(a, n, b, NEAR_Z, 1);
    if (m < 3)
        return;
    project(b, m, c, pc->view);
    m = clip_side(c, m, d, 0, pc->view->left, 1);
    m = clip_side(d, m, c, 0, pc->view->right, 0);
    m = clip_side(c, m, d, 1, pc->view->top, 1);
    m = clip_side(d, m, c, 1, pc->view->bottom, 0);
    if (m < 3)
        return;
    if (pc->made + (uint32_t)(m - 2) * 3 > pc->room) {
        pc->full = 1;
        return;
    }
    for (i = 1; i + 1 < m; i++) {
        const Corner2 *t[3] = { &c[0], &c[i], &c[i + 1] };
        int j;

        for (j = 0; j < 3; j++) {
            FxCorner *o = &pc->out[pc->made];

            o->x = (float)t[j]->x;
            o->y = (float)t[j]->y;
            o->u = o->v = 0;
            pc->past[pc->made] = past;
            pc->values[pc->made++] = value;
        }
    }
}

/* A flat piece; while the water cuts the picture, cut by it as a face is,
   each part to its side. */
static void cut_piece(void *context, const double (*p)[3], int n, uint8_t value)
{
    PatternCut *pc = (PatternCut *)context;
    Corner3 a[WORK], w[WORK];
    int i, m;

    if (n > MOST || pc->full)
        return;
    for (i = 0; i < n; i++) {
        a[i].x = p[i][0];
        a[i].y = p[i][1];
        a[i].z = p[i][2];
        a[i].u = a[i].v = 0;
    }
    if (!cut.on) {
        piece_part(pc, a, n, value, 0);
        return;
    }
    if ((m = clip_water(a, n, w, 0)) >= 3)
        piece_part(pc, w, m, value, 0);
    if ((m = clip_water(a, n, w, 1)) >= 3)
        piece_part(pc, w, m, value, 1);
}

uint32_t fx_face_pattern(const FxShape *sh, double u_off, double v_off, FxCorner *out, uint8_t *values, uint8_t *past, uint32_t room)
{
    double xyz[MOST][3], uv[MOST][2];
    PatternCut pc;
    int i, k;

    if (!sh->pattern || sh->pattern > npatterns)
        return 0;
    k = sh->pattern - 1;
    for (i = 0; i < pattern_faces[k].n; i++) {
        xyz[i][0] = pattern_faces[k].c[i].x;
        xyz[i][1] = pattern_faces[k].c[i].y;
        xyz[i][2] = pattern_faces[k].c[i].z;
        uv[i][0] = pattern_faces[k].c[i].u + u_off;
        uv[i][1] = pattern_faces[k].c[i].v + v_off;
    }
    pc.view = &pattern_faces[k].view;
    pc.out = out;
    pc.values = values;
    pc.past = past;
    pc.room = room;
    pc.made = 0;
    pc.full = 0;
    if (!fx_pattern_cut(pattern_faces[k].pattern, (const double (*)[3])xyz, (const double (*)[2])uv, pattern_faces[k].n, cut_piece, &pc) || pc.full)
        return 0;
    return pc.made;
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

    sprite.set = screen_size != 0;
    sprite.past = fx_face_past_water((int16_t)x, (int16_t)y, d);
    sprite.cx = v.cx + FOCAL * (int16_t)x / d;
    sprite.cy = v.cy + FOCAL * (int16_t)y / d;
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
