/*
 * fx_clip.c: a polygon from its points to the screen: clipped against the
 * four sides of the view and a near plane, split in depth, projected.
 *
 * A polygon is a list of vertices of five words (x, y, z and two
 * attributes) in one of two lists; fx_list_in holds the address of the one
 * being read, fx_list_out of the one being written, and each clip writes
 * the other and the two are swapped. Each edge's crossing with a plane is
 * found by halving the edge (DIV2 of the sums: at most 12 midpoints), the
 * attributes halved with the position, not by dividing.
 */
#include "game.h"
#include "render.h"

typedef struct {
    int16_t x, y, z, a1, a2;
} FxVertex;

static FxVertex vertex_at(uint16_t at)
{
    FxVertex v = {FXW(at), FXW(at + 2), FXW(at + 4), FXW(at + 6), FXW(at + 8)};

    return v;
}

static void vertex_put(uint16_t at, const FxVertex *v)
{
    FXW(at) = v->x;
    FXW(at + 2) = v->y;
    FXW(at + 4) = v->z;
    FXW(at + 6) = v->a1;
    FXW(at + 8) = v->a2;
}

static int16_t *axis_of(FxVertex *v, int axis)
{
    return axis ? &v->y : &v->x;
}

/* A plane: the view's sides are z = c * slope (c the vertex's x or y,
   doubled `more` times after the 1.15 product), on the negative side for
   the left and bottom ones; the near plane is z = near. */
typedef struct {
    bool is_near;
    int axis, more;
    bool neg;
    int16_t slope, near;
} FxPlane;

static int16_t scaled(const FxPlane *p, int16_t c)
{
    int16_t v = mul15(c, p->slope);
    int i;

    for (i = 0; i < p->more; i++)
        v = wrap16(v * 2);
    return v;
}

/* Whether (c, z) is on the kept side: the sign of z - c * slope, or of
   near - z (near + z for a negative near). `neg` asks z + c * slope, as
   the clip drivers test the negative side. */
static bool inside(const FxPlane *p, int16_t c, int16_t z, bool neg)
{
    if (p->is_near)
        return wrap16(p->near >= 0 ? p->near - z : p->near + z) >= 0;
    return wrap16(neg ? z + scaled(p, c) : z - scaled(p, c)) >= 0;
}

static int16_t test_value(const FxPlane *p, int16_t c, int16_t z)
{
    if (p->is_near)
        return wrap16(p->near >= 0 ? p->near - z : p->near + z);
    return wrap16(z - scaled(p, c));
}

static FxVertex midpoint(const FxVertex *a, const FxVertex *b)
{
    FxVertex m;

    m.x = div2(wrap16(a->x + b->x));
    m.y = div2(wrap16(a->y + b->y));
    m.z = div2(wrap16(a->z + b->z));
    m.a1 = div2(wrap16(a->a1 + b->a1));
    m.a2 = div2(wrap16(a->a2 + b->a2));
    return m;
}

/* The segment a-b against the plane (its own tests, z - c * slope): 0 when
   both ends are kept, -1 when neither is; else the end outside is
   replaced by the crossing and 1. */
static int clip_segment(const FxPlane *p, FxVertex *a, FxVertex *b)
{
    bool a_in = test_value(p, *axis_of(a, p->axis), a->z) >= 0;
    bool b_in = test_value(p, *axis_of(b, p->axis), b->z) >= 0;
    FxVertex o, in, m;
    int n = 12;

    if (a_in && b_in)
        return 0;
    if (!a_in && !b_in)
        return -1;
    o = a_in ? *b : *a;
    in = a_in ? *a : *b;
    for (;;) {
        int16_t t;

        m = midpoint(&o, &in);
        t = test_value(p, *axis_of(&m, p->axis), m.z);
        if (t == 0 || --n == 0)
            break;
        if (t >= 0)
            in = m;
        else
            o = m;
    }
    *(a_in ? b : a) = m;
    return 1;
}

/* A vertex to the output list, the count up one. */
static void emit(uint16_t *out, const FxVertex *v)
{
    vertex_put(*out, v);
    *out = (uint16_t)(*out + 10);
    fx_vertex_count++;
}

/* The polygon in fx_list_in (fx_vertex_count vertices) against the plane
   into fx_list_out: each edge from a vertex to the next (the last to the
   first); a vertex kept is copied, an edge crossing adds its crossing. On
   the negative side the coordinate is negated for the crossing search and
   the crossing's taken back (the original's halvings round differently
   there). Returns the new count, or 0 when fewer than 3 are left or there
   were 2; *out = past the output. */
static uint16_t clip_polygon(const FxPlane *p, uint16_t *out_end)
{
    uint16_t count = fx_vertex_count, in = fx_list_in, out = fx_list_out, at = in, k;

    fx_vertex_count = 0;
    if (count == 2) {
        *out_end = out;
        return 0;
    }
    k = count;
    do {
        FxVertex a = vertex_at(at), b;
        bool a_in, b_in;

        at = (uint16_t)(k == 1 ? in : at + 10);
        b = vertex_at(at);
        a_in = inside(p, *axis_of(&a, p->axis), a.z, p->neg);
        b_in = inside(p, *axis_of(&b, p->axis), b.z, p->neg);
        if (a_in)
            emit(&out, &a);
        if (a_in != b_in) {
            if (p->neg) {
                *axis_of(&a, p->axis) = (int16_t)-(uint16_t)*axis_of(&a, p->axis);
                *axis_of(&b, p->axis) = (int16_t)-(uint16_t)*axis_of(&b, p->axis);
            }
            clip_segment(p, &a, &b);
            {
                FxVertex *moved = a_in ? &b : &a;

                if (p->neg)
                    *axis_of(moved, p->axis) = (int16_t)-(uint16_t)*axis_of(moved, p->axis);
                emit(&out, moved);
            }
        }
    } while (--k);
    *out_end = out;
    return wrap16(fx_vertex_count - 3) >= 0 ? fx_vertex_count : 0;
}

#define FX_SLOPE_Y FXW(0x0094)
#define FX_SLOPE_X FXW(0x0092)

uint16_t fx_polygon_clip_top(void)
{
    FxPlane p = {false, 1, 2, false, FX_SLOPE_Y, 0};
    uint16_t end;

    return clip_polygon(&p, &end);
}

uint16_t fx_polygon_clip_bottom(void)
{
    FxPlane p = {false, 1, 2, true, FX_SLOPE_Y, 0};
    uint16_t end;

    return clip_polygon(&p, &end);
}

uint16_t fx_polygon_clip_right(void)
{
    FxPlane p = {false, 0, 1, false, FX_SLOPE_X, 0};
    uint16_t end;

    return clip_polygon(&p, &end);
}

uint16_t fx_polygon_clip_left(void)
{
    FxPlane p = {false, 0, 1, true, FX_SLOPE_X, 0};
    uint16_t end;

    return clip_polygon(&p, &end);
}

uint16_t fx_polygon_clip_near(int16_t near)
{
    FxPlane p = {true, 0, 0, false, 0, near};
    uint16_t end;

    return clip_polygon(&p, &end);
}

static void lists_swap(void)
{
    uint16_t t = fx_list_in;

    fx_list_in = fx_list_out;
    fx_list_out = t;
}

/* The vertices of fx_list_in to the screen (fx_project_vertex), each
   pulled in to the edge it is past (Y to the top or bottom edge, X to 0 or
   the right edge), out at fx_pairs_end four words each: x, y and the two
   attributes doubled; the outcodes ORed in fx_codes, which ends cleared,
   and fx_points_n = the count. Returns past the output. */
uint16_t fx_polygon_project(void)
{
    uint16_t count = fx_vertex_count, at = fx_list_in, out = fx_pairs_end, k = count;

    fx_codes = 0;
    do {
        FxVertex v = vertex_at(at);
        uint16_t code = fx_project_vertex(&v.x, &v.y, v.z);

        if (code & 1)
            v.y = fx_edges[2];
        if (code & 2)
            v.y = fx_edges[3];
        if (code & 4)
            v.x = 0;
        if (code & 8)
            v.x = fx_edges[1];
        FXW(out) = v.x;
        FXW(out + 2) = v.y;
        FXW(out + 4) = wrap16(v.a1 * 2);
        FXW(out + 6) = wrap16(v.a2 * 2);
        out = (uint16_t)(out + 8);
        at = (uint16_t)(at + 10);
        fx_codes |= code;
    } while (--k);
    fx_codes = 0;
    fx_points_n = fx_vertex_count;
    return out;
}

/* A polygon whose depth spans too much (three times its nearest z below
   its farthest) split at (farthest + 3 * nearest) / 4: the near part
   clipped to it, its header (the byte below the output) given its count's
   complement, projected, and the header copied after it; then the far
   part. The z range starts at -16000 and 16000, and a z that raises the
   farthest is not compared with the nearest. Returns the count left, or
   0. */
uint16_t fx_polygon_near_clip(void)
{
    int16_t zmax = (int16_t)0xC180, zmin = 0x3E80;
    uint16_t count = fx_vertex_count, at, k, plane, header, out, n;

    if (fx_list_in != FX_LIST_B) {
        k = count;
        at = (uint16_t)(fx_list_in + 4);
        do {
            int16_t z = FXW(at);

            if (wrap16(zmax - z) < 0)
                zmax = z;
            else if (wrap16(zmin - z) >= 0)
                zmin = z;
            at = (uint16_t)(at + 10);
        } while (--k);
    } else {
        uint16_t to = fx_list_out;

        k = count;
        at = fx_list_in;
        do {
            FxVertex v = vertex_at(at);

            vertex_put(to, &v);
            if (wrap16(zmax - v.z) < 0)
                zmax = v.z;
            else if (wrap16(zmin - v.z) >= 0)
                zmin = v.z;
            at = (uint16_t)(at + 10);
            to = (uint16_t)(to + 10);
        } while (--k);
        lists_swap();
    }
    fx_count_save = fx_vertex_count;
    if (wrap16(wrap16(zmin * 3) - zmax) >= 0)
        return fx_vertex_count;
    plane = (uint16_t)((uint16_t)(zmax + zmin * 3) >> 2);
    n = fx_polygon_clip_near((int16_t)plane);
    lists_swap();
    if (!n)
        return 0;
    header = (uint16_t)(fx_pairs_end - 8);
    pakram[header] = (uint8_t)~n;
    out = fx_polygon_project();
    for (k = 0; k < 4; k++, out = (uint16_t)(out + 2))
        FXU(out) = FXU(header + 2 * k);
    fx_pairs_end = out;
    fx_list_out = FX_LIST_B;
    fx_list_in = FX_LIST_A;
    fx_vertex_count = fx_count_save;
    if (!fx_polygon_clip_near((int16_t)-plane))
        return 0;
    lists_swap();
    return fx_vertex_count;
}

/* A polygon of fx_points_n points (their index bytes just before `script`
   in the ROM bank) to the screen: gathered (fx_points_gather) into the
   list at FX_LIST_B, clipped against each side its outcodes cross (bit 1
   top, bit 0 bottom, bit 3 right, bit 2 left; bit 4, behind the view,
   means all four), split in depth and projected. Returns the count, or 0
   (with fx_vertex_count cleared) when a clip leaves nothing. */
uint16_t fx_polygon_draw(uint16_t outcodes, uint8_t bank, uint16_t script)
{
    static uint16_t (*const clip[4])(void) = {fx_polygon_clip_top, fx_polygon_clip_bottom, fx_polygon_clip_right,
                                              fx_polygon_clip_left};
    static const uint16_t bit[4] = {2, 1, 8, 4};
    uint16_t n = fx_points_n;
    int i;

    fx_codes = outcodes;
    fx_vertex_count = n;
    fx_points_gather(n, bank, (uint16_t)(script - n));
    fx_list_in = FX_LIST_B;
    fx_list_out = FX_LIST_A;
    if (fx_codes & 0x10)
        fx_codes = 0x0F;
    for (i = 0; i < 4; i++)
        if (fx_codes & bit[i]) {
            uint16_t left = clip[i]();

            lists_swap();
            if (!left)
                goto none;
        }
    if (!fx_polygon_near_clip())
        goto none;
    fx_polygon_project();
    return fx_vertex_count;
none:
    fx_vertex_count = 0;
    return 0;
}

/* ---- a face of the object's own points to the screen ---- */

typedef struct {
    int16_t x, y, z;
} FxPoint;

static FxPoint point_at(uint16_t at)
{
    FxPoint p = {FXW(at), FXW(at + 2), FXW(at + 4)};

    return p;
}

static void point_put(uint16_t at, const FxPoint *p)
{
    FXW(at) = p->x;
    FXW(at + 2) = p->y;
    FXW(at + 4) = p->z;
}

/* where a-b crosses z = 0, as fx_segment_cross_z */
static void point_cross(FxPoint *a, FxPoint *b)
{
    int16_t pa[3] = {a->x, a->y, a->z}, pb[3] = {b->x, b->y, b->z};

    fx_segment_cross_z(pa, pb);
    a->x = pa[0], a->y = pa[1], a->z = pa[2];
    b->x = pb[0], b->y = pb[1], b->z = pb[2];
}

/* A shape of n points (their index bytes at bank:script) to the screen:
   each point (FX_POINTS, six bytes an index) moved by the object's
   position (fx_vector_out) into the list at fx_pairs_end; clipped to z >=
   0 into FX_LIST_A, fx_vertex_count the count left (a line's two ends
   kept, crossed where one is behind, whatever they are); each vertex then
   projected (fx_project_vertex) back to fx_pairs_end as x and y words,
   the outcodes ORed into fx_codes. Returns the count, or 0 when fewer
   than 3 are left or every vertex is outside one side of the view (a bit
   of $0F00 clear in the ORed codes). */
uint16_t fx_polygon_gather(uint16_t n, uint8_t bank, uint16_t script)
{
    uint16_t list = fx_pairs_end, at = list, out = FX_LIST_A, k = n;

    do {
        FxPoint p = point_at((uint16_t)(FX_POINTS + rom_at((uint32_t)bank << 16 | script++)[0] * 6));

        p.x = wrap16(p.x + fx_vector_out[0]);
        p.y = wrap16(p.y + fx_vector_out[1]);
        p.z = wrap16(p.z + fx_vector_out[2]);
        point_put(at, &p);
        at = (uint16_t)(at + 6);
    } while (--k);
    fx_vertex_count = 0;
    if (n == 2) {
        FxPoint a = point_at(list), b = point_at((uint16_t)(list + 6));

        point_cross(&a, &b);
        point_put(out, &a);
        point_put((uint16_t)(out + 6), &b);
        fx_vertex_count = 2;
    } else {
        at = list;
        k = n;
        do {
            FxPoint a = point_at(at), b;

            at = (uint16_t)(k == 1 ? list : at + 6);
            b = point_at(at);
            if ((a.z & b.z) < 0)
                continue;
            if (a.z >= 0) {
                point_put(out, &a);
                out = (uint16_t)(out + 6);
                fx_vertex_count++;
                if (b.z < 0) {
                    point_cross(&a, &b);
                    point_put(out, &b);
                    out = (uint16_t)(out + 6);
                    fx_vertex_count++;
                }
            } else {
                point_cross(&a, &b);
                point_put(out, &a);
                out = (uint16_t)(out + 6);
                fx_vertex_count++;
            }
        } while (--k);
        if (wrap16(fx_vertex_count - 3) < 0)
            return 0;
    }
    fx_codes = 0;
    at = FX_LIST_A;
    out = list;
    k = fx_vertex_count;
    do {
        FxPoint p = point_at(at);

        fx_codes |= fx_project_vertex(&p.x, &p.y, p.z);
        FXW(out) = p.x;
        FXW(out + 2) = p.y;
        out = (uint16_t)(out + 4);
        at = (uint16_t)(at + 6);
    } while (--k);
    return (fx_codes & 0x0F00) == 0x0F00 ? fx_vertex_count : 0;
}

/* ---- a projected polygon clipped to the screen's sides ---- */

/* A side of the screen (its word among fx_edges): `y` for a row (top,
   bottom); `strict` for the far ones (right, bottom), whose inside is at
   or before them, compared within 16 bits; `inset` puts a crossing one
   row inside (a line's bottom). */
typedef struct {
    int edge;
    bool y, strict, inset;
} FxSide;

static bool side_inside(const FxSide *s, const int16_t v[2])
{
    int16_t at = v[s->y], edge = fx_edges[s->edge];

    return s->strict ? wrap16(at - edge) <= 0 : at >= edge;
}

/* where in-out crosses the side, from the inside end: the other axis
   moved by (side - in) times its difference over this one's
   (fx_divide_signed) */
static void side_cross(const FxSide *s, const int16_t in[2], const int16_t out_[2], int16_t at[2])
{
    int a = s->y, o = !s->y;
    uint16_t d_a = (uint16_t)(out_[a] - in[a]), d_o = (uint16_t)(out_[o] - in[o]);
    int32_t p = (int32_t)wrap16(fx_edges[s->edge] - in[a]) * (int16_t)d_o;

    at[o] = wrap16(fx_divide_signed((uint32_t)p, d_a).quot + in[o]);
    at[a] = wrap16(fx_edges[s->edge] - s->inset);
}

static void pair_put(uint16_t *at, const int16_t v[2])
{
    FXW(*at) = v[0];
    FXW(*at + 2) = v[1];
    *at = (uint16_t)(*at + 4);
}

/* The fx_vertex_count vertices (x, y words) at `in` clipped against the
   side into `out`, fx_count_save counted from what it held to the
   vertices written; the first vertex is first copied after the last (at
   its count's low byte, signed, times four), and fx_vertex_count ends 0. */
static void side_clip(const FxSide *s, uint16_t in, uint16_t out)
{
    uint16_t at = in;

    FXW((uint16_t)(in + (int8_t)fx_vertex_count * 4)) = FXW(in);
    FXW((uint16_t)(in + (int8_t)fx_vertex_count * 4 + 2)) = FXW(in + 2);
    do {
        int16_t a[2] = {FXW(at), FXW(at + 2)}, b[2] = {FXW(at + 4), FXW(at + 6)}, c[2];

        at = (uint16_t)(at + 4);
        if (!side_inside(s, a)) {
            if (!side_inside(s, b)) {
                fx_count_save--;
            } else {
                side_cross(s, b, a, c);
                pair_put(&out, c);
            }
        } else if (side_inside(s, b)) {
            pair_put(&out, a);
        } else {
            fx_count_save++;
            pair_put(&out, a);
            side_cross(s, a, b, c);
            pair_put(&out, c);
        }
    } while (--fx_vertex_count);
}

/* The projected polygon at fx_pairs_end (n vertices, x and y words)
   clipped against the sides of the screen its outcodes in fx_codes cross
   (bit 2 left, 3 right, 0 top, 1 bottom), between it and FX_LIST_A
   (fx_list_in and fx_list_out, swapped after each side), and left at
   fx_pairs_end again. Returns the count, or 0 when fewer than 3 are left.
   A line (fewer than 3) goes against all four sides, from fx_pairs_end
   to FX_LIST_A and back twice, as a polygon of its ends (so a crossing
   is found from each), and returns 2, or 0 when fewer than 2 are left. */
uint16_t fx_polygon_clip_screen(uint16_t n)
{
    static const FxSide line_sides[4] = {{0, 0, 0, 0}, {1, 0, 1, 0}, {2, 1, 0, 0}, {3, 1, 1, 1}};
    static const FxSide sides[4] = {{0, 0, 0, 0}, {1, 0, 1, 0}, {2, 1, 0, 0}, {3, 1, 1, 0}};
    static const uint16_t bit[4] = {4, 8, 1, 2};
    uint16_t list = fx_pairs_end;
    int i;

    fx_list_in = list;
    fx_list_out = FX_LIST_A;
    fx_count_save = n;
    if (wrap16(n - 3) < 0) {
        for (i = 0; i < 4; i++) {
            fx_vertex_count = n;
            side_clip(&line_sides[i], i & 1 ? FX_LIST_A : list, i & 1 ? list : FX_LIST_A);
            n = fx_count_save;
            if (wrap16(n - 2) < 0)
                return 0;
        }
        return 2;
    }
    for (i = 0; i < 4; i++)
        if (fx_codes & bit[i]) {
            fx_vertex_count = n;
            side_clip(&sides[i], fx_list_in, fx_list_out);
            lists_swap();
            n = fx_count_save;
            if (wrap16(n - 3) < 0)
                return 0;
        }
    if (fx_list_in != list) {
        uint16_t from = fx_list_in, to = fx_list_out, k = n;

        do {
            FXW(to) = FXW(from);
            FXW(to + 2) = FXW(from + 2);
            to = (uint16_t)(to + 4);
            from = (uint16_t)(from + 4);
        } while (--k);
        n = fx_count_save;
    }
    return n;
}
