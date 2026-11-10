/*
 * fx_solid.c: sprites that show solid things drawn as solid shapes (plan
 * stage 10, solids; the design of src/stuntracefx/remaster/solids.c, as
 * approved there). Some of the race's objects are pictures facing the
 * view, drawn by FxObjectSprite with the picture picked for the angle they
 * are seen at: every car's tyres (shape $A1C8), the bonus course truck's
 * wheels ($A1E4) and the trees' trunks and canopies ($A104, $A120). Each
 * is drawn in its place as a solid instead:
 *
 *   tyres, wheels   a shape turned about its axle (a profile of rings, the
 *                   ends capped and ringed), at the object's place in view
 *                   space, turned as the model drawer turns a model with
 *                   the object's angles, its faces flat in the colours its
 *                   pictures use, a side's shade by how far it faces up;
 *   trees           the trunk and the canopy each turned about the world's
 *                   upright, their widths from their paintings, the
 *                   canopy's points moved a little in and out and up and
 *                   down (the same for every tree), so its facets take the
 *                   light each its own way; each face's colour one of its
 *                   painting's by how far it faces the light the painting
 *                   is lit from.
 *
 * The sprite's code runs as ever (its state, its clipping, its colour);
 * only its quads are left out (fx_solid_hiding). The solid's faces are
 * points in view space, projected and clipped by fx_face_triangles and
 * filled by the GPU. Effects stay sprites. A picture of four bits a pixel
 * (two players) keeps its sprites: a solid's colours are the 8-bit
 * palette's.
 *
 * In a shape's own coordinates x is across, y down and z ahead.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_face.h"

#define PI_F 3.14159265f
#define SEGMENTS_MAX 60

/* a profile's ring: its place along the axis and radius (texels); a band
   from it to the next ring in one colour (nib) or shaded by how far it
   faces up: top (nearly straight up, when nonzero), up, across, down */
typedef struct {
    float a, r;
    uint8_t nib, top, up, across, down;
} Ring;
/* a cap's ring, drawn over the cap outward in: radius, colour */
typedef struct {
    float r;
    uint8_t nib;
} Disc;
typedef struct {
    uint16_t shape;
    int texels, axis;                   /* the picture's width; the axis (0 x, 1 y, 2 z) */
    const Ring *rings;
    int nrings;
    const Disc *front, *back;           /* the caps at the last ring (+) and the first (-) */
    int nfront, nback;
    int tree;                           /* a tree's: 1 its trunk, 2 its canopy (shaped by its painting) */
} Solid;

/* a tyre: 28 texels of its 32 across, 18 wide, shoulders a texel; its
   side 9; its hub 4 inside a ring of B round a centre of 9; its tread 5
   up, 6 across, A down; its shoulders A */
static const Ring tyre_rings[] = {
    { -9, 13, 0xA, 0, 0, 0, 0 }, { -8, 14, 0, 0, 0x5, 0x6, 0xA }, { 8, 14, 0xA, 0, 0, 0, 0 }, { 9, 13, 0, 0, 0, 0, 0 },
};
static const Disc tyre_side[] = { { 13, 0x9 }, { 6, 0x4 }, { 4, 0xB }, { 2, 0x9 } };

#define N(a) (int)(sizeof a / sizeof a[0])
static const Solid solids[] = {
    { 0xA1C8, 32, 0, tyre_rings, N(tyre_rings), tyre_side, tyre_side, N(tyre_side), N(tyre_side), 0 },
    { 0xA1E4, 32, 0, tyre_rings, N(tyre_rings), tyre_side, tyre_side, N(tyre_side), N(tyre_side), 0 },
    { 0xA104, 32, 1, NULL, 0, NULL, NULL, 0, 0, 1 },
    { 0xA120, 64, 1, NULL, 0, NULL, NULL, 0, 0, 2 },
};

static const Solid *object, *armed;
static int coloured;
static uint8_t hi, tex_bank, tex_nib;
static uint16_t tex_base;
static float pos[3], view[3][3], scale;

static uint16_t word(uint16_t a)
{
    return (uint16_t)(fx_ram[fx_ram_at(a)] | fx_ram[fx_ram_at((uint16_t)(a ^ 1))] << 8);
}

/* the output's pixels a picture pixel: how fine a solid's round must be */
static float output_focal(void)
{
    float sx, sy;

    fx_draw_list_scale(&sx, &sy);
    return 128.0f * (sx > sy ? sx : sy);
}

/* a face of n view-space points in colour c, into the picture */
static void emit(const float (*p)[3], int n, uint8_t c)
{
    static FxCorner corners[3 * (SEGMENTS_MAX + 8)];
    uint32_t k = fx_face_triangles(p, n, corners, N(corners));

    if (k)
        fx_draw_flat(corners, k, c);
}

/* Interim, until Jeff has seen each look: the solids are drawn only where
   this is set (a private build for the pictures); the game keeps the
   sprites. */
static const int solids_shown = 0;

/* FxObjectsDraw's object: its shape (the draw record's model) */
void fx_solid_object(uint16_t shape)
{
    int i;

    object = NULL;
    if (!solids_shown)
        return;
    for (i = 0; i < N(solids); i++)
        if (solids[i].shape == shape)
            object = &solids[i];
}

/* The rotation the model drawer turns a model's points by (model.c): the
   matrix of the angles at $0020 (Z), $0022 (X), $0024 (Y), each the byte
   negated as a turn's high byte with its fraction between steps
   (FxMatrixFromAngles01's rows), transposed, times the view's at $00E4
   (FxMatrixApplyVectors). A point p in the model is p . view in view
   space. */
static void turn(void)
{
    float a[3], s[3], c[3], m[3][3], e[3][3];
    int i, j, k;

    for (i = 0; i < 3; i++) {
        uint16_t w = (uint16_t)-(int)((word((uint16_t)(0x0020 + 2 * i)) & 0xFF) << 8 | fx_angle_fraction(i));

        a[i] = (float)w / 65536.0f * 2 * PI_F;
        s[i] = sinf(a[i]);
        c[i] = cosf(a[i]);
    }
    {
        float sz = s[0], cz = c[0], sx = s[1], cx = c[1], sy = s[2], cy = c[2];

        m[0][0] = sx * sy * sz + cx * cy; m[0][1] = sx * cy * sz - cx * sy; m[0][2] = sx * cz;
        m[1][0] = sy * cz;                m[1][1] = cy * cz;                m[1][2] = -sz;
        m[2][0] = cx * sy * sz - sx * cy; m[2][1] = cx * cy * sz + sx * sy; m[2][2] = cx * cz;
    }
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            e[i][j] = (int16_t)word((uint16_t)(0x00E4 + 6 * i + 2 * j)) / 32768.0f;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            for (view[i][j] = 0, k = 0; k < 3; k++)
                view[i][j] += m[k][i] * e[k][j];
}

/* FxObjectSprite past its depth test: a solid's place, size and turn kept,
   its sprite's quads left out */
void fx_solid_begin(void)
{
    int32_t own, size;

    armed = NULL;
    coloured = 0;
    if (!object || !fx_draw_full_colour())
        return;
    pos[0] = (int16_t)word(0x0026);
    pos[1] = (int16_t)word(0x0028);
    pos[2] = (int16_t)word(0x002A);
    /* the sprite's size (01:B100, B128): a negative own size alone, else
       the shape's twice and the own shifted by $0032; 0 as 1 */
    own = (int16_t)word(FX_MODEL_CE);
    size = own < 0 ? -own : 2 * word(FX_SPRITE_SIZE) + (own << (word(0x0032) & 15));
    scale = (float)(size ? size : 1) / object->texels;
    if (object->tree) {                 /* upright in the world: the view's matrix alone */
        int i, j;

        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++)
                view[i][j] = (int16_t)word((uint16_t)(0x00E4 + 6 * i + 2 * j)) / 32768.0f;
    } else {
        turn();
    }
    armed = object;
}

int fx_solid_hiding(void)
{
    return armed != NULL;
}

/* its material read (FxSpriteMaterial, COLOR and CMODE): its colour's high
   nibble, and its painting, which shapes a tree */
void fx_solid_colour(uint8_t colr, uint8_t por)
{
    if (!armed)
        return;
    hi = (uint8_t)(colr & 0xF0);
    tex_bank = (uint8_t)word(FX_TEX_BANK);
    tex_base = word(FX_TEX_BASE);
    tex_nib = (por & 0x04) ? 1 : 0;     /* COLOR's: the high nibble, else the low */
    coloured = 1;
}

/* ---- tyres and wheels ---- */

/* a point of the solid, along its axis a, round it at turn t, radius r, in
   view space */
static void place(const Solid *s, float a, float t, float r, float *out)
{
    float m[3];
    int j;

    m[s->axis] = a;
    m[s->axis ? 0 : 1] = r * cosf(t);
    m[s->axis ? 1 : 2] = r * sinf(t);
    for (j = 0; j < 3; j++)
        out[j] = pos[j] + scale * (m[0] * view[0][j] + m[1] * view[1][j] + m[2] * view[2][j]);
}

/* a face's outward normal (from its first three points, turned away from
   the solid's centre) and whether it is seen: it faces the eye */
static int face_seen(const float (*p)[3], int n, float *nv)
{
    float u[3], v[3], c[3], d = 0, e = 0;
    int i, j;

    for (j = 0; j < 3; j++) {
        u[j] = p[1][j] - p[0][j];
        v[j] = p[n - 1][j] - p[0][j];
        for (c[j] = 0, i = 0; i < n; i++)
            c[j] += p[i][j];
        c[j] /= n;
    }
    nv[0] = u[1] * v[2] - u[2] * v[1];
    nv[1] = u[2] * v[0] - u[0] * v[2];
    nv[2] = u[0] * v[1] - u[1] * v[0];
    for (j = 0; j < 3; j++)
        d += nv[j] * (c[j] - pos[j]);
    if (d < 0)
        for (j = 0; j < 3; j++)
            nv[j] = -nv[j];
    for (j = 0; j < 3; j++)
        e += nv[j] * c[j];
    return e < 0;
}

static uint8_t shade(const Ring *g, const float *n)
{
    float up = -n[1] / sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);   /* view y is down */

    if (!g->up)
        return g->nib;
    if (g->top && up > 0.8f)
        return g->top;
    return up > 0.45f ? g->up : up < -0.45f ? g->down : g->across;
}

/* a cap at a, its rings over it outward in, when it faces the eye */
static void cap(const Solid *s, float a, const Disc *d, int nd, int n)
{
    static float p[SEGMENTS_MAX][3];
    float nv[3];
    int i, k;

    for (k = 0; k < nd; k++) {
        int m = k ? (n / 2 < 8 ? 8 : n / 2) : n;

        for (i = 0; i < m; i++)
            place(s, a, 2 * PI_F * i / m, d[k].r, p[i]);
        if (!k && !face_seen((const float (*)[3])p, m, nv))
            return;
        emit((const float (*)[3])p, m, (uint8_t)(hi | d[k].nib));
    }
}

static void draw(const Solid *s)
{
    float rmax = 0, pr, p[4][3], nv[3];
    int n, i, k;

    /* segments enough that the rim's flats are within half an output pixel
       of the circle: n >= pi / acos(1 - 0.5 / r), r its radius on the
       output */
    for (k = 0; k < s->nrings; k++)
        if (s->rings[k].r > rmax)
            rmax = s->rings[k].r;
    pr = pos[2] > 0 ? rmax * scale * output_focal() / pos[2] : rmax;
    n = pr > 0.5f ? (int)ceilf(PI_F / acosf(1 - 0.5f / pr)) : 8;
    n = (n + 3) & ~3;
    if (n < 8)
        n = 8;
    if (n > SEGMENTS_MAX)
        n = SEGMENTS_MAX;
    /* the bands: a convex solid, so the faces it shows never overlap */
    for (k = 0; k + 1 < s->nrings; k++) {
        const Ring *g = &s->rings[k], *h = &s->rings[k + 1];

        for (i = 0; i < n; i++) {
            float t0 = 2 * PI_F * i / n, t1 = 2 * PI_F * (i + 1) / n;

            place(s, g->a, t0, g->r, p[0]);
            place(s, g->a, t1, g->r, p[1]);
            place(s, h->a, t1, h->r, p[2]);
            place(s, h->a, t0, h->r, p[3]);
            if (face_seen((const float (*)[3])p, 4, nv))
                emit((const float (*)[3])p, 4, (uint8_t)(hi | shade(g, nv)));
        }
    }
    cap(s, s->rings[s->nrings - 1].a, s->front, s->nfront, n);
    cap(s, s->rings[0].a, s->back, s->nback, n);
}

/* ---- trees ---- */
/*
 * A tree in the race's polygon style, of its paintings' size and shape:
 * its trunk and its canopy each turned about the world's upright, their
 * half widths from their paintings (the trunk every two rows in ten
 * sides, the canopy every three in 44, the widest of each), the canopy's
 * outline eased ring to ring and every other ring turned half a side, so
 * its facets interlock. Flat faces in the paintings' colours by how far
 * each faces the light, from where the painting is lit (where its lighter
 * colours lie): each colour from a light on, set once for the painting by
 * drawing the part from the front (its faces seen, least lit first, taking
 * the painting's colours darkest first, each for its share of the painting
 * by area on the screen), the same every picture after. A painting faces
 * the view whatever the tree's angles, so a tree stands upright.
 */
#define PAINT_ROWS 64
typedef struct {
    int used, kind, ready;              /* the painting it is, measured once; its colours' lights set */
    uint8_t bank, nib;
    uint16_t base;
    float half[PAINT_ROWS], light[3], share[16], from[16];
    int top, bottom, ncolours;
    uint8_t colour[16];                 /* darkest (the higher nibble) first */
} Paint;

static uint8_t texel(int u, int v)
{
    return (uint8_t)((fx_rom_read(tex_bank, (uint16_t)(tex_base + (v << 8) + u)) >> (4 * tex_nib)) & 15);
}

static void paint_measure(const Solid *s, Paint *p)
{
    float count[16] = { 0 }, total = 0, mean = 0, cx = 0, cy = 0, lx = 0, ly = 0, l;
    int u, v, k, T = s->texels;
    uint8_t c, rank[16] = { 0 };

    memset(p, 0, sizeof *p);
    p->top = -1;
    for (v = 0; v < T; v++) {
        int left = -1, right = -1;

        for (u = 0; u < T; u++)
            if ((c = texel(u, v))) {
                if (left < 0)
                    left = u;
                right = u;
                count[c]++, total++;
            }
        p->half[v] = left < 0 ? 0 : (right + 1 - left) / 2.0f;
        if (left >= 0) {
            if (p->top < 0)
                p->top = v;
            p->bottom = v;
        }
    }
    for (c = 15; c >= 1; c--)
        if (count[c])
            rank[c] = (uint8_t)p->ncolours, p->colour[p->ncolours++] = c;
    if (!total)
        return;
    for (k = 0; k < p->ncolours; k++)
        p->share[k] = count[p->colour[k]] / total;
    for (v = 0; v < T; v++)
        for (u = 0; u < T; u++)
            if ((c = texel(u, v)))
                mean += rank[c], cx += u + 0.5f, cy += v + 0.5f;
    mean /= total, cx /= total, cy /= total;
    for (v = 0; v < T; v++)
        for (u = 0; u < T; u++)
            if ((c = texel(u, v)))
                lx += (rank[c] - mean) * (u + 0.5f - cx), ly += (rank[c] - mean) * (v + 0.5f - cy);
    l = sqrtf(lx * lx + ly * ly);
    p->light[0] = l ? 0.75f * lx / l : 0, p->light[1] = l ? 0.75f * ly / l : 0, p->light[2] = -0.66f;
    l = sqrtf(p->light[0] * p->light[0] + p->light[1] * p->light[1] + p->light[2] * p->light[2]);
    for (k = 0; k < 3; k++)
        p->light[k] /= l;
}

/* a point of the tree (x and z across, y the world's down, texels from its
   painting's middle) in view space */
static void tree_point(float x, float y, float z, float *out)
{
    int j;

    for (j = 0; j < 3; j++)
        out[j] = pos[j] + scale * (x * view[0][j] + y * view[1][j] + z * view[2][j]);
}

#define TREE_FACES 4096
typedef struct {
    float p[4][3], key, lit, area;
    int n;
    uint8_t c;
} TreeFace;
static TreeFace faces[TREE_FACES];
static TreeFace *by_lit[TREE_FACES];

static int face_cmp(const void *a, const void *b)
{
    float x = ((const TreeFace *)a)->key, y = ((const TreeFace *)b)->key;

    return x > y ? -1 : x < y;
}

static int lit_cmp(const void *a, const void *b)
{
    float x = (*(TreeFace *const *)a)->lit, y = (*(TreeFace *const *)b)->lit;

    return x < y ? -1 : x > y;
}

#define TREE_SIDES_MAX 48
#define RINGS_MAX (PAINT_ROWS / 2 + 4)
static int nf, calibrating;
static float total_area;

/* a face of m points kept when seen, outward away from the upright at down
   ay; its light against the painting's and its area on the screen */
static void keep_face(const float (*q)[3], int m, float ay, const Paint *pt)
{
    float u[3], v[3], nv[3], c[3] = { 0, 0, 0 }, axis[3], d = 0, e = 0, l;
    TreeFace *f;
    int i, j;

    if (nf >= TREE_FACES)
        return;
    f = &faces[nf];
    for (j = 0; j < 3; j++) {
        u[j] = q[1][j] - q[0][j];
        v[j] = q[m - 1][j] - q[0][j];
        for (i = 0; i < m; i++)
            c[j] += q[i][j] / m;
    }
    nv[0] = u[1] * v[2] - u[2] * v[1];
    nv[1] = u[2] * v[0] - u[0] * v[2];
    nv[2] = u[0] * v[1] - u[1] * v[0];
    tree_point(0, ay, 0, axis);
    for (j = 0; j < 3; j++)
        d += nv[j] * (c[j] - axis[j]);
    if (d < 0)                          /* outward: away from the upright */
        for (j = 0; j < 3; j++)
            nv[j] = -nv[j];
    for (j = 0; j < 3; j++)
        e += nv[j] * c[j];
    if (e >= 0)
        return;
    l = sqrtf(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);
    f->lit = l ? (nv[0] * pt->light[0] + nv[1] * pt->light[1] + nv[2] * pt->light[2]) / l : -1;
    for (i = 0; i < m; i++)
        memcpy(f->p[i], q[i], sizeof f->p[i]);
    f->n = m;
    f->key = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
    f->area = 0;
    if (calibrating) {                  /* its area on the screen, for the painting's colours */
        for (j = 0; j < m; j++) {
            const float *a = q[j], *b = q[(j + 1) % m];

            if (a[2] > 0 && b[2] > 0)
                f->area += (a[0] / a[2]) * (b[1] / b[2]) - (b[0] / b[2]) * (a[1] / a[2]);
        }
        f->area = fabsf(f->area) * 128.0f * 128.0f / 2;
    }
    total_area += f->area;
    nf++;
}

/* The tree's points: ring k's point c (0 to sides, point sides point 0 one
   turn on) at radius r[k] times jag[k][c mod sides], angle 2 pi (c + o) /
   sides (o half a side on a turned ring), down y[k] + rise[k][c mod sides] */
static void tree_model(float (*m)[TREE_SIDES_MAX + 1][3], const float *y, const float *r, int nr, int sides,
                       const float (*jag)[TREE_SIDES_MAX], const float (*rise)[TREE_SIDES_MAX], int stagger)
{
    int k, c;

    for (k = 0; k < nr; k++) {
        float o = stagger && (k & 1) ? 0.5f : 0;

        for (c = 0; c <= sides; c++) {
            int w = c % sides;
            float ra = r[k] * jag[k][w], t = 2 * PI_F * (c + o) / sides;

            m[k][c][0] = ra * cosf(t);
            m[k][c][1] = y[k] + rise[k][w];
            m[k][c][2] = ra * sinf(t);
        }
    }
}

static float placed[RINGS_MAX][TREE_SIDES_MAX + 1][3];

/* a body turned about the upright, its seen faces drawn farthest first (it
   need not be convex), every other ring turned half a side when staggered */
static void tree_body(const float *y, int nr, int sides, float (*m)[TREE_SIDES_MAX + 1][3], const float *r,
                      int stagger, Paint *pt)
{
    int k, i;
    float at;

    nf = 0, total_area = 0;
    for (k = 0; k < nr; k++)
        for (i = 0; i <= sides; i++)
            tree_point(m[k][i][0], m[k][i][1], m[k][i][2], placed[k][i]);
    for (k = 0; k + 1 < nr; k++) {
        float o0 = stagger && (k & 1) ? 0.5f : 0, o1 = stagger && ((k + 1) & 1) ? 0.5f : 0, ay = (y[k] + y[k + 1]) / 2;

        for (i = 0; i < sides; i++) {
            const float *a0 = placed[k][i], *a1 = placed[k][i + 1], *b0 = placed[k + 1][i], *b1 = placed[k + 1][i + 1];
            float q[4][3];

            if (!r[k]) {                /* a closed end: triangles to its point */
                memcpy(q[0], a0, sizeof q[0]), memcpy(q[1], b1, sizeof q[0]), memcpy(q[2], b0, sizeof q[0]);
                keep_face((const float (*)[3])q, 3, ay, pt);
            } else if (!r[k + 1]) {
                memcpy(q[0], a0, sizeof q[0]), memcpy(q[1], a1, sizeof q[0]), memcpy(q[2], b0, sizeof q[0]);
                keep_face((const float (*)[3])q, 3, ay, pt);
            } else if (o0 == o1) {      /* a band of four-sided faces */
                memcpy(q[0], a0, sizeof q[0]), memcpy(q[1], a1, sizeof q[0]), memcpy(q[2], b1, sizeof q[0]), memcpy(q[3], b0, sizeof q[0]);
                keep_face((const float (*)[3])q, 4, ay, pt);
            } else if (o1 > o0) {       /* the next ring's point beside this edge: b0 */
                memcpy(q[0], a0, sizeof q[0]), memcpy(q[1], a1, sizeof q[0]), memcpy(q[2], b0, sizeof q[0]);
                keep_face((const float (*)[3])q, 3, ay, pt);
                memcpy(q[0], b0, sizeof q[0]), memcpy(q[1], a1, sizeof q[0]), memcpy(q[2], b1, sizeof q[0]);
                keep_face((const float (*)[3])q, 3, ay, pt);
            } else {                    /* this ring turned: the next ring's b1 beside this edge */
                memcpy(q[0], a0, sizeof q[0]), memcpy(q[1], a1, sizeof q[0]), memcpy(q[2], b1, sizeof q[0]);
                keep_face((const float (*)[3])q, 3, ay, pt);
                memcpy(q[0], a0, sizeof q[0]), memcpy(q[1], b1, sizeof q[0]), memcpy(q[2], b0, sizeof q[0]);
                keep_face((const float (*)[3])q, 3, ay, pt);
            }
        }
    }
    if (calibrating) {                  /* each colour's light: where the faces, least lit first, pass its share */
        float edge;

        for (i = 0; i < nf; i++)
            by_lit[i] = &faces[i];
        qsort(by_lit, (size_t)nf, sizeof by_lit[0], lit_cmp);
        pt->from[0] = -2;
        for (k = 1, i = 0, at = 0, edge = pt->share[0] * total_area; k < pt->ncolours; k++, edge += pt->share[k - 1] * total_area) {
            while (i < nf && at + by_lit[i]->area / 2 <= edge)
                at += by_lit[i++]->area;
            pt->from[k] = i < nf ? by_lit[i]->lit : 2;
        }
        return;
    }
    for (i = 0; i < nf; i++) {          /* each face's colour by its light */
        for (k = pt->ncolours - 1; k > 0 && faces[i].lit < pt->from[k]; k--)
            ;
        faces[i].c = pt->colour[k];
    }
    qsort(faces, (size_t)nf, sizeof faces[0], face_cmp);
    for (i = 0; i < nf; i++)
        emit((const float (*)[3])faces[i].p, faces[i].n, (uint8_t)(hi | faces[i].c));
}

/* A tree drawn at the detail its size on the output asks: sides enough that
   its flats are within half an output pixel of round at its widest, on four
   levels (the full count, a half, a quarter, an eighth, at least 5); its
   rings merged two, four or eight painting rows at a time while they would
   come closer than 1.5 output pixels apart. Each level made once for its
   painting; the colours' lights set from the full detail, so a face's
   colour means the same light at every level. */
typedef struct {
    int ok, nr, sides;
    float T, y[RINGS_MAX], r[RINGS_MAX];
    float (*m)[TREE_SIDES_MAX + 1][3];
} TreeLevel;
static TreeLevel levels[4][4][4];       /* painting, sides level, rings level */

static TreeLevel *tree_level(const Solid *s, Paint *pp, int paint, int sl, int rl)
{
    static float jag[RINGS_MAX][TREE_SIDES_MAX], rise[RINGS_MAX][TREE_SIDES_MAX];
    TreeLevel *L = &levels[paint][sl][rl];
    float T = (float)s->texels, *y = L->y, *r = L->r;
    int full = s->tree == 1 ? 10 : 44, sides = full >> sl, step = (s->tree == 1 ? 2 : 3) << rl, nr = 0, v, k, i;

    if (L->ok && L->T == T)
        return L;
    if (sides < 5)
        sides = 5;
    y[nr] = pp->top - T / 2, r[nr++] = 0;
    for (v = pp->top; v <= pp->bottom && nr < RINGS_MAX - 1; v += step) {
        float h = 0;

        for (k = v; k < v + step && k <= pp->bottom; k++)
            if (pp->half[k] > h)
                h = pp->half[k];
        y[nr] = v + step / 2.0f - T / 2, r[nr++] = h;
    }
    if (s->tree == 2) {                 /* the canopy's outline eased: each ring with its neighbours */
        float was[RINGS_MAX];

        memcpy(was, r, sizeof was);
        for (k = 1; k + 1 < nr; k++)
            r[k] = 0.25f * was[k - 1] + 0.5f * was[k] + 0.25f * was[k + 1];
    }
    y[nr] = pp->bottom + 1 - T / 2, r[nr++] = 0;
    for (k = 0; k < nr; k++)            /* the canopy's points in and out and up and down a little, the same for every tree */
        for (i = 0; i < sides; i++) {
            unsigned hsh = (unsigned)(k * 7919 + i * 104729) * 2654435761u;

            jag[k][i] = s->tree == 2 ? 0.91f + 0.18f * ((hsh >> 16) & 0xFF) / 255.0f : 1.0f;
            rise[k][i] = s->tree == 2 && r[k] ? 1.6f * (((hsh >> 8) & 0xFF) / 255.0f - 0.5f) : 0;
        }
    if (!L->m && !(L->m = malloc(sizeof *L->m * RINGS_MAX)))
        return NULL;
    tree_model(L->m, y, r, nr, sides, (const float (*)[TREE_SIDES_MAX])jag, (const float (*)[TREE_SIDES_MAX])rise, s->tree == 2);
    L->nr = nr;
    L->sides = sides;
    L->T = T;
    L->ok = 1;
    return L;
}

static void tree(const Solid *s)
{
    static Paint paints[4];
    Paint *pp = NULL;
    TreeLevel *L;
    float px, rmax = 0;
    int i, sl, rl, paint, full = s->tree == 1 ? 10 : 44, step = s->tree == 1 ? 2 : 3;

    for (i = 0; i < 4 && !pp; i++)      /* the painting, measured once */
        if (paints[i].used && paints[i].kind == s->tree && paints[i].bank == tex_bank && paints[i].base == tex_base
            && paints[i].nib == tex_nib)
            pp = &paints[i];
    if (!pp) {
        for (i = 0; i < 4 && paints[i].used; i++)
            ;
        pp = &paints[i < 4 ? i : 0];
        paint_measure(s, pp);
        pp->used = 1, pp->kind = s->tree, pp->bank = tex_bank, pp->base = tex_base, pp->nib = tex_nib;
        for (sl = 0; sl < 4; sl++)
            for (rl = 0; rl < 4; rl++)
                levels[pp - paints][sl][rl].ok = 0;
    }
    paint = (int)(pp - paints);
    if (!pp->ncolours || pp->top < 0)
        return;
    if (!pp->ready) {                   /* its colours' lights, once, at the full detail: the part from the front, far off */
        float kp[3], kv[3][3], ks = scale;
        int a, b;

        if (!(L = tree_level(s, pp, paint, 0, 0)))
            return;
        memcpy(kp, pos, sizeof kp), memcpy(kv, view, sizeof kv);
        for (a = 0; a < 3; a++)
            for (b = 0; b < 3; b++)
                view[a][b] = a == b;
        pos[0] = pos[1] = 0, pos[2] = 1e5f, scale = 1;
        calibrating = 1;
        tree_body(L->y, L->nr, L->sides, L->m, L->r, s->tree == 2, pp);
        calibrating = 0;
        memcpy(pos, kp, sizeof kp), memcpy(view, kv, sizeof kv), scale = ks;
        pp->ready = 1;
    }
    /* its detail: output pixels a painting texel, at its depth */
    for (i = pp->top; i <= pp->bottom; i++)
        if (pp->half[i] > rmax)
            rmax = pp->half[i];
    px = pos[2] > 0 ? scale * output_focal() / pos[2] : 1e9f;
    {
        float pr = rmax * px;
        int n = pr > 0.5f ? (int)ceilf(PI_F / acosf(1 - 0.5f / pr)) : 5;

        for (sl = 3; sl > 0 && (full >> sl < 5 ? 5 : full >> sl) < n; sl--)
            ;
    }
    for (rl = 0; rl < 3 && step * (1 << rl) * px < 1.5f; rl++)
        ;
    if (!(L = tree_level(s, pp, paint, sl, rl)))
        return;
    tree_body(L->y, L->nr, L->sides, L->m, L->r, s->tree == 2, pp);
}

/* FxObjectSprite returned: the solid drawn in the sprite's place in the
   order */
void fx_solid_end(void)
{
    const Solid *s = armed;

    armed = NULL;
    if (!s || !coloured)
        return;
    coloured = 0;
    if (s->tree)
        tree(s);
    else
        draw(s);
}
