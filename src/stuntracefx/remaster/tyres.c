/*
 * tyres.c: Enhanced's tyres as solid wheels. Every car's tyres are child
 * objects of shape $A1C8, each drawn by FxObjectSprite as a 32-texel
 * picture facing the view, the frame picked for the angle it is seen at.
 * In the full window's picture each is drawn in its place as a wheel
 * instead: a cylinder with chamfered shoulders and a hub on each side, at
 * the tyre's place in view space, turned as the model drawer turns a
 * model with the tyre's angles (its car's), its faces filled flat in the
 * colours the pictures use. The sprite's code runs as ever, its pixels
 * left out (fw_sprite_hide); nothing of the Super FX's state is touched.
 *
 * Its size is the pictures': the sprite's, FxObjectSprite's (01:B100),
 * twice the shape's size word ($00AA) plus the object's own ($00CE, its
 * +$5F, shifted by $0032; a car's tyres from $09:8000, the 4WD's 192
 * units), across its 32 texels; the tyre seen side on is 28 texels
 * across, seen along its axle 18 wide, its hub rings 12, 8 and 4 across,
 * so each car's tyres are drawn to their own size. Its colours,
 * the low nibble with the material's high one: the side 9, the shoulders
 * A, the tread by how far it faces up the view (5 up, 6 across, A down),
 * the hub 4 with a ring of B and a centre of 9.
 *
 * FW_SPRITE_TYRES=1 (a check) keeps the sprites.
 */
#include <math.h>
#include <stdlib.h>

#include "gsu.h"
#include "remaster.h"
#include "fullwindow.h"

#define TYRE_SHAPE 0xA1C8u
/* in the sprite's texels (32 across) */
#define TYRE_R 14.0                     /* the tread's radius */
#define TYRE_HALF 9.0                   /* half its width, to the sides */
#define TYRE_SHOULDER 1.0               /* the chamfer from the tread to the sides */
#define TYRE_SEGMENTS_MAX 60

static uint16_t shape;
static int armed, coloured;
static uint8_t hi;
static double pos[3], view[3][3], scale;

static uint16_t ram16(uint16_t a) { return (uint16_t)(gsu_rd8(0x700000u | a) | gsu_rd8(0x700000u | (uint16_t)(a + 1)) << 8); }

static int sprites_kept(void)
{
    static int kept = -1;
    if (kept < 0)
        kept = getenv("FW_SPRITE_TYRES") != NULL;
    return kept;
}

void fw_tyre_object(uint16_t s) { shape = s; }

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

/* FxObjectSprite past its depth test (frame.c): a tyre's place and turn
   kept, its sprite left out of the picture */
void fw_tyre_begin(void)
{
    armed = coloured = 0;
    if (shape != TYRE_SHAPE || !fw_sprite_rows() || !fw_view_active() || sprites_kept())
        return;
    pos[0] = (int16_t)ram16(0x0026);
    pos[1] = (int16_t)ram16(0x0028);
    pos[2] = (int16_t)ram16(0x002A);
    {
        /* the sprite's size (01:B100, B128): a negative own size alone, else
           the shape's twice and the own shifted; 0 as 1; a texel a 32nd */
        int32_t own = (int16_t)ram16(0x00CE), size = own < 0 ? -own : 2 * ram16(0x00AA) + (own << ram16(0x0032));
        scale = (size ? size : 1) / 32.0;
    }
    if (rm_far_shift)                   /* a far object's place and size halved together */
        scale /= 1 << rm_far_shift;
    turn();
    armed = 1;
    fw_sprite_hide(1);
}

/* its material read (FxSpriteMaterial): a colour, so the sprite is drawn */
void fw_tyre_colour(uint8_t colr)
{
    if (!armed)
        return;
    hi = (uint8_t)(colr & 0xF0);
    coloured = 1;
}

/* a model point (x along the axle) to view space */
static void place(double x, double y, double z, double *out)
{
    int j;
    for (j = 0; j < 3; j++)
        out[j] = pos[j] + scale * (x * view[0][j] + y * view[1][j] + z * view[2][j]);
}

/* a face is seen when its normal (in the model) points back at the eye
   from its centre (both in view space) */
static int seen(const double *nm, const double *centre, double *nv)
{
    int j;
    for (j = 0; j < 3; j++)
        nv[j] = nm[0] * view[0][j] + nm[1] * view[1][j] + nm[2] * view[2][j];
    return nv[0] * centre[0] + nv[1] * centre[1] + nv[2] * centre[2] < 0;
}

static void centre_of(const double (*p)[3], int n, double *c)
{
    int i, j;
    for (j = 0; j < 3; j++) {
        for (c[j] = 0, i = 0; i < n; i++)
            c[j] += p[i][j];
        c[j] /= n;
    }
}

/* a disc on a side (x), radius r, n points */
static void disc(double x, double r, int n, uint8_t nib)
{
    double p[TYRE_SEGMENTS_MAX][3];
    int i;
    for (i = 0; i < n; i++) {
        double t = 2 * M_PI * i / n;
        place(x, r * cos(t), r * sin(t), p[i]);
    }
    fw_view_face((const double (*)[3])p, n, (uint8_t)(hi | nib));
}

static void wheel(void)
{
    double pr, f = fw_focal(), side = TYRE_HALF, rs = TYRE_R - TYRE_SHOULDER, xt = TYRE_HALF - TYRE_SHOULDER;
    double p[4][3], c[3], nv[3], nm[3];
    int n, i, s;

    /* segments enough that the rim's flats are within half a texel of
       the circle: n >= pi / acos(1 - 0.5 / r), r its radius on the screen */
    pr = pos[2] > 0 ? TYRE_R * scale * f / pos[2] : TYRE_R;
    n = pr > 0.5 ? (int)ceil(M_PI / acos(1 - 0.5 / pr)) : 8;
    n = (n + 3) & ~3;
    if (n < 8)
        n = 8;
    if (n > TYRE_SEGMENTS_MAX)
        n = TYRE_SEGMENTS_MAX;
    for (i = 0; i < n; i++) {           /* the tread and its shoulders: a convex ring, its faces seen apart */
        double t0 = 2 * M_PI * i / n, t1 = 2 * M_PI * (i + 1) / n, tm = (t0 + t1) / 2;
        double up;
        place(xt, TYRE_R * cos(t0), TYRE_R * sin(t0), p[0]);
        place(xt, TYRE_R * cos(t1), TYRE_R * sin(t1), p[1]);
        place(-xt, TYRE_R * cos(t1), TYRE_R * sin(t1), p[2]);
        place(-xt, TYRE_R * cos(t0), TYRE_R * sin(t0), p[3]);
        nm[0] = 0, nm[1] = cos(tm), nm[2] = sin(tm);
        centre_of((const double (*)[3])p, 4, c);
        if (seen(nm, c, nv)) {
            up = -nv[1] / sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);     /* view y is down */
            fw_view_face((const double (*)[3])p, 4, (uint8_t)(hi | (up > 0.45 ? 0x5 : up < -0.45 ? 0xA : 0x6)));
        }
        for (s = -1; s <= 1; s += 2) {
            place(s * xt, TYRE_R * cos(t0), TYRE_R * sin(t0), p[0]);
            place(s * xt, TYRE_R * cos(t1), TYRE_R * sin(t1), p[1]);
            place(s * side, rs * cos(t1), rs * sin(t1), p[2]);
            place(s * side, rs * cos(t0), rs * sin(t0), p[3]);
            nm[0] = s * M_SQRT1_2, nm[1] = cos(tm) * M_SQRT1_2, nm[2] = sin(tm) * M_SQRT1_2;
            centre_of((const double (*)[3])p, 4, c);
            if (seen(nm, c, nv))
                fw_view_face((const double (*)[3])p, 4, (uint8_t)(hi | 0xA));
        }
    }
    for (s = -1; s <= 1; s += 2) {      /* the sides, their hubs over them */
        int nh = n / 2 < 8 ? 8 : n / 2;
        place(s * side, 0, 0, c);
        nm[0] = s, nm[1] = 0, nm[2] = 0;
        if (!seen(nm, c, nv))
            continue;
        disc(s * side, rs, n, 0x9);
        disc(s * side, 6.0, nh, 0x4);
        disc(s * side, 4.0, nh, 0xB);
        disc(s * side, 2.0, nh, 0x9);
    }
}

/* FxObjectSprite returned (frame.c, 01:AD2D) */
void fw_tyre_end(void)
{
    if (!armed)
        return;
    fw_sprite_hide(0);
    if (coloured)
        wheel();
    armed = coloured = 0;
}
