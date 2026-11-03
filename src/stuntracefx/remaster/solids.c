/*
 * solids.c: Enhanced's sprites drawn as solid shapes. Some of the race's
 * objects are pictures facing the view, drawn by FxObjectSprite, the
 * picture picked for the angle they are seen at: every car's tyres
 * (shape $A1C8), the bonus course truck's wheels ($A1E4, ObjectSparksStart)
 * and the 4WD's eyes ($A190, ObjectLampStart). In the full window's
 * picture each is drawn in its place as a solid instead: a shape turned
 * about its axis (a profile of rings, ends capped and ringed), at the
 * object's place in view space, turned as the model drawer turns a model
 * with the object's angles (its car's), its faces filled flat in the
 * colours its pictures use. The sprite's code runs as ever, its pixels
 * left out (fw_sprite_hide); nothing of the Super FX's state is touched.
 *
 * A shape's size is its pictures': the sprite's, FxObjectSprite's (01:B100),
 * twice the shape's size word ($00AA) plus the object's own ($00CE, its
 * +$5F, shifted by $0032; each car's tyres their own from $09:8000)
 * across the picture's texels; the shapes below are in those texels,
 * measured from the pictures seen side on and along their axes. Their
 * colours are the low nibble with the material's high one; a side's
 * shading by how far it faces up the view, as the pictures shade it.
 *
 * In the model x is across, y down and z ahead (the tyres' places on
 * their car, the camera behind it).
 *
 * The eyes are the game's: where one looks is the byte ObjectPlaceOnParent
 * reads the table at $08:E8AF by (the camera's turn, the car's heading,
 * the eye's own turn and its car's record's +$3A; rm_gaze), its picture
 * one of twelve round it: the lens to the view at 119.5, turned to the
 * left of the screen as the byte falls, side on at 55.5 (64 a quarter
 * turn), level. The solid eye is turned by that byte whole, the one its
 * draw record's picture was taken by (rm_record_gaze). An eye with
 * any other look than its plain one ($9AAF: a crash's, the demo script's
 * ($7E:1A5B), the fixed picture of +$25 bit 0) is drawn as its sprite.
 *
 * FW_SPRITE_SOLIDS=1 (a check) keeps the sprites.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gsu.h"
#include "console_ext.h"
#include "remaster.h"
#include "fullwindow.h"

#define SEGMENTS_MAX 60

/* a profile's ring: its place along the axis and radius (texels); a band
   from it to the next ring in one colour (nib) or shaded by how far it
   faces up: top (nearly straight up, when nonzero), up, across, down */
typedef struct { double a, r; uint8_t nib, top, up, across, down; } Ring;
/* a cap's ring, drawn over the cap outward in: radius, colour */
typedef struct { double r; uint8_t nib; } Disc;
typedef struct {
    uint16_t shape;
    int texels, axis;                   /* the picture's width; the axis (0 x, 2 z) */
    const Ring *rings;
    int nrings;
    const Disc *front, *back;           /* the caps at the last ring (+) and the first (-) */
    int nfront, nback;
} Solid;

/* a tyre: 28 texels of its 32 across, 18 wide, shoulders a texel; its
   side 9; its hub 4 inside a ring of B round a centre of 9; its tread 5
   up, 6 across, A down; its shoulders A */
static const Ring tyre_rings[] = {
    { -9, 13, 0xA }, { -8, 14, 0, 0, 0x5, 0x6, 0xA }, { 8, 14, 0xA }, { 9, 13 },
};
static const Disc tyre_side[] = { { 13, 0x9 }, { 6, 0x4 }, { 4, 0xB }, { 2, 0x9 } };
/* an eye: 14 of its 16 texels across, 14 long, the lens ahead, the can
   straight for 10 and its back rounded over the last 4: shaded B nearly
   straight up, 8 up, 7 across, F down; the lens's rim C, a ring of B, the
   glass E round the reflector's B and F; the back's middle 7 */
#define LAMP_SHADE 0, 0xB, 0x8, 0x7, 0xF
static const Ring lamp_rings[] = {
    { -7, 2, LAMP_SHADE }, { -6.3, 4.2, LAMP_SHADE }, { -5, 6, LAMP_SHADE }, { -3, 7, LAMP_SHADE }, { 7, 7 },
};
static const Disc lamp_lens[] = { { 7, 0xC }, { 6.2, 0xB }, { 5.2, 0xE }, { 3.2, 0xB }, { 2.2, 0xF } };
static const Disc lamp_back[] = { { 2, 0x7 } };

#define N(a) (int)(sizeof a / sizeof a[0])
static const Solid solids[] = {
    { 0xA1C8, 32, 0, tyre_rings, N(tyre_rings), tyre_side, tyre_side, N(tyre_side), N(tyre_side) },
    { 0xA1E4, 32, 0, tyre_rings, N(tyre_rings), tyre_side, tyre_side, N(tyre_side), N(tyre_side) },
    { 0xA190, 16, 2, lamp_rings, N(lamp_rings), lamp_lens, lamp_back, N(lamp_lens), N(lamp_back) },
};

static const Solid *object, *armed;
static unsigned record;
static int coloured;
static uint8_t hi;
static double pos[3], view[3][3], scale;

static uint16_t ram16(uint16_t a) { return (uint16_t)(gsu_rd8(0x700000u | a) | gsu_rd8(0x700000u | (uint16_t)(a + 1)) << 8); }

static int sprites_kept(void)
{
    static int kept = -1;
    if (kept < 0)
        kept = getenv("FW_SPRITE_SOLIDS") != NULL;
    return kept;
}

uint32_t rm_gaze[0x8000];
uint16_t rm_record_object[RM_RECORDS], rm_record_gaze[RM_RECORDS];
static uint32_t gaze_was[0x8000];

void rm_gaze_tick_start(void) { memcpy(gaze_was, rm_gaze, sizeof gaze_was); }
uint32_t rm_gaze_of(uint16_t o) { return (rm_tween_start_side ? gaze_was : rm_gaze)[o & 0x7FFF]; }

/* FxObjectsDraw's object (01:ABC2): its shape and draw record */
void fw_solid_object(uint16_t shape, unsigned rec)
{
    int i;
    record = rec;
    object = NULL;
    for (i = 0; i < N(solids); i++)
        if (solids[i].shape == shape)
            object = &solids[i];
}

/* The rotation the model drawer turns a model's points by (model.c,
   01:8741): the matrix of the angles at $0020 (Z), $0022 (X), $0024 (Y),
   each the byte negated as a turn's high byte (FxMatrixFromAngles01's
   rows), transposed, times the view's at $00E4 (FxMatrixApplyVectors).
   A point p in the model is p . view in view space. */
static void turn(void)
{
    double a[3], s[3], c[3], m[3][3], e[3][3];
    int i, j, k;

    for (i = 0; i < 3; i++) {
        a[i] = (double)(uint16_t)-(int)((ram16((uint16_t)(0x0020 + 2 * i)) & 0xFF) << 8) / 65536.0 * 2 * M_PI;
        s[i] = sin(a[i]);
        c[i] = cos(a[i]);
    }
    {
        double sz = s[0], cz = c[0], sx = s[1], cx = c[1], sy = s[2], cy = c[2];
        m[0][0] = sx * sy * sz + cx * cy; m[0][1] = sx * cy * sz - cx * sy; m[0][2] = sx * cz;
        m[1][0] = sy * cz;                m[1][1] = cy * cz;                m[1][2] = -sz;
        m[2][0] = cx * sy * sz - sx * cy; m[2][1] = cx * cy * sz + sx * sy; m[2][2] = cx * cz;
    }
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            e[i][j] = (int16_t)ram16((uint16_t)(0x00E4 + 6 * i + 2 * j)) / 32768.0;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            for (view[i][j] = 0, k = 0; k < 3; k++)
                view[i][j] += m[k][i] * e[k][j];
}

/* FxObjectSprite past its depth test (frame.c): a solid's place, size and
   turn kept, its sprite left out of the picture */
void fw_solid_begin(void)
{
    armed = NULL;
    coloured = 0;
    if (!object || !fw_sprite_rows() || !fw_view_active() || sprites_kept())
        return;
    if (object->shape == 0xA190) {      /* an eye: by its gaze, in its plain look */
        const uint8_t *w = nshvc_wram();
        uint16_t o = record < RM_RECORDS ? rm_record_object[record] : 0, g = record < RM_RECORDS ? rm_record_gaze[record] : 0;
        double phi, d[3];
        if (w[0x1A5B] || !(g & 0x100) || (w[(uint16_t)(0x205D + o)] | w[(uint16_t)(0x205E + o)] << 8) != 0x9AAF)
            return;
        phi = (119.5 - (g & 0xFF)) * 2 * M_PI / 256;
        d[0] = -sin(phi), d[1] = 0, d[2] = -cos(phi);
        view[0][0] = d[2], view[0][1] = 0, view[0][2] = -d[0];      /* across: down x ahead */
        view[1][0] = 0, view[1][1] = 1, view[1][2] = 0;             /* down: the view's */
        view[2][0] = d[0], view[2][1] = d[1], view[2][2] = d[2];    /* the lens */
    }
    pos[0] = (int16_t)ram16(0x0026);
    pos[1] = (int16_t)ram16(0x0028);
    pos[2] = (int16_t)ram16(0x002A);
    {
        /* the sprite's size (01:B100, B128): a negative own size alone, else
           the shape's twice and the own shifted; 0 as 1 */
        int32_t own = (int16_t)ram16(0x00CE), size = own < 0 ? -own : 2 * ram16(0x00AA) + (own << ram16(0x0032));
        scale = (double)(size ? size : 1) / object->texels;
    }
    if (rm_far_shift)                   /* a far object's place and size halved together */
        scale /= 1 << rm_far_shift;
    if (object->shape != 0xA190)
        turn();
    armed = object;
    fw_sprite_hide(1);
}

/* its material read (FxSpriteMaterial): a colour, so the sprite is drawn */
void fw_solid_colour(uint8_t colr)
{
    if (!armed)
        return;
    hi = (uint8_t)(colr & 0xF0);
    coloured = 1;
}

/* a point of the solid, along its axis a, round it at turn t, radius r, to
   view space */
static void place(const Solid *s, double a, double t, double r, double *out)
{
    double m[3];
    int j;
    m[s->axis] = a;
    m[s->axis ? 0 : 1] = r * cos(t);
    m[s->axis ? 1 : 2] = r * sin(t);
    for (j = 0; j < 3; j++)
        out[j] = pos[j] + scale * (m[0] * view[0][j] + m[1] * view[1][j] + m[2] * view[2][j]);
}

/* a face's outward normal (from its first three points, turned away from
   the solid's centre) and whether it is seen: it faces the eye from its
   centre */
static int face_seen(const double (*p)[3], int n, double *nv, double *centre)
{
    double u[3], v[3], d = 0, e = 0;
    int i, j;
    for (j = 0; j < 3; j++) {
        u[j] = p[1][j] - p[0][j];
        v[j] = p[n - 1][j] - p[0][j];
        for (centre[j] = 0, i = 0; i < n; i++)
            centre[j] += p[i][j];
        centre[j] /= n;
    }
    nv[0] = u[1] * v[2] - u[2] * v[1];
    nv[1] = u[2] * v[0] - u[0] * v[2];
    nv[2] = u[0] * v[1] - u[1] * v[0];
    for (j = 0; j < 3; j++)
        d += nv[j] * (centre[j] - pos[j]);
    if (d < 0)
        for (j = 0; j < 3; j++)
            nv[j] = -nv[j];
    for (j = 0; j < 3; j++)
        e += nv[j] * centre[j];
    return e < 0;
}

static uint8_t shade(const Ring *g, const double *nv)
{
    double up = -nv[1] / sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);   /* view y is down */
    if (!g->up)
        return g->nib;
    if (g->top && up > 0.8)
        return g->top;
    return up > 0.45 ? g->up : up < -0.45 ? g->down : g->across;
}

/* a cap at a, its rings over it outward in, when it faces the eye */
static void cap(const Solid *s, double a, const Disc *d, int nd, int n)
{
    static double p[SEGMENTS_MAX][3];
    double nv[3], c[3];
    int i, k;
    for (k = 0; k < nd; k++) {
        int m = k ? (n / 2 < 8 ? 8 : n / 2) : n;
        for (i = 0; i < m; i++)
            place(s, a, 2 * M_PI * i / m, d[k].r, p[i]);
        if (!k && !face_seen((const double (*)[3])p, m, nv, c))
            return;
        fw_view_face((const double (*)[3])p, m, (uint8_t)(hi | d[k].nib));
    }
}

static void draw(const Solid *s)
{
    double rmax = 0, pr, f = fw_focal(), p[4][3], nv[3], c[3];
    int n, i, k;

    /* segments enough that the rim's flats are within half a texel of
       the circle: n >= pi / acos(1 - 0.5 / r), r its radius on the screen */
    for (k = 0; k < s->nrings; k++)
        if (s->rings[k].r > rmax)
            rmax = s->rings[k].r;
    pr = pos[2] > 0 ? rmax * scale * f / pos[2] : rmax;
    n = pr > 0.5 ? (int)ceil(M_PI / acos(1 - 0.5 / pr)) : 8;
    n = (n + 3) & ~3;
    if (n < 8)
        n = 8;
    if (n > SEGMENTS_MAX)
        n = SEGMENTS_MAX;
    /* the bands: a convex solid, so the faces it shows never overlap */
    for (k = 0; k + 1 < s->nrings; k++) {
        const Ring *g = &s->rings[k], *h = &s->rings[k + 1];
        for (i = 0; i < n; i++) {
            double t0 = 2 * M_PI * i / n, t1 = 2 * M_PI * (i + 1) / n;
            place(s, g->a, t0, g->r, p[0]);
            place(s, g->a, t1, g->r, p[1]);
            place(s, h->a, t1, h->r, p[2]);
            place(s, h->a, t0, h->r, p[3]);
            if (face_seen((const double (*)[3])p, 4, nv, c))
                fw_view_face((const double (*)[3])p, 4, (uint8_t)(hi | shade(g, nv)));
        }
    }
    cap(s, s->rings[s->nrings - 1].a, s->front, s->nfront, n);
    cap(s, s->rings[0].a, s->back, s->nback, n);
}

/* FxObjectSprite returned (frame.c, 01:AD2D) */
void fw_solid_end(void)
{
    if (!armed)
        return;
    fw_sprite_hide(0);
    if (coloured)
        draw(armed);
    armed = NULL;
    coloured = 0;
}
