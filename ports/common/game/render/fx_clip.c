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
