/*
 * solids.c: Enhanced's sprites drawn as solid shapes. Some of the race's
 * objects are pictures facing the view, drawn by FxObjectSprite, the
 * picture picked for the angle they are seen at: every car's tyres
 * (shape $A1C8), the bonus course truck's wheels ($A1E4, ObjectSparksStart)
 * the 4WD's eyes ($A190, ObjectLampStart) and the trees' trunks and
 * canopies ($A104, $A120, ObjectLampTower; trees below). In the full window's
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
 * draw record's picture was taken by (rm_record_gaze). Its look is the
 * table its pictures are taken from (frame.c, 01:AC48): its plain one
 * ($9AAF) or, while its car's +$25 bit 6 spins it round (ObjectLampDrop:
 * its own turn +$48 up a sixteenth a step, its size +$5F stepped), the
 * startled one ($9AB1, the same pictures but the five facing the view:
 * more of the eye white, its pupil small); the demo script's blinks
 * ($7E:1A5B, ObjectLampAnimate's tables at $08:E57F-E609) step through
 * three more, the same pictures but those facing the view lidded: $9AB3
 * (the plain pupil under a lid down to the middle), $9AB7 (the startled
 * one's) and $9AB5 (shut). The solid eye turns and sizes by the same
 * bytes, and its face is its look's picture facing the view as marks
 * (Mark), each placed as the picture's texels have it. An eye with the
 * fixed picture of +$25 bit 0 is drawn as its sprite.
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
#include "scene.h"

#define SEGMENTS_MAX 60

/* a profile's ring: its place along the axis and radius (texels); a band
   from it to the next ring in one colour (nib) or shaded by how far it
   faces up: top (nearly straight up, when nonzero), up, across, down */
typedef struct { float a, r; uint8_t nib, top, up, across, down; } Ring;
/* a cap's ring, drawn over the cap outward in: radius, colour */
typedef struct { float r; uint8_t nib; } Disc;
typedef struct {
    uint16_t shape;
    int texels, axis;                   /* the picture's width; the axis (0 x, 2 z) */
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
    { -9, 13, 0xA }, { -8, 14, 0, 0, 0x5, 0x6, 0xA }, { 8, 14, 0xA }, { 9, 13 },
};
static const Disc tyre_side[] = { { 13, 0x9 }, { 6, 0x4 }, { 4, 0xB }, { 2, 0x9 } };
/* an eye: a half dome 14 of its 16 texels across, its flat face ahead
   (the eye) and its dome behind, the whole of it in the picture's middle:
   the dome shaded B nearly straight up, 8 up, 7 across, F down; the face
   plain: its rim C, a ring of B, the white E round the pupil's B and F;
   startled: its rim F, a ring of B, more of it white, the pupil small (B,
   C, F round a C); its back's last cap 7 */
#define LAMP_SHADE 0, 0xB, 0x8, 0x7, 0xF
static const Ring lamp_rings[] = {      /* round the dome from its back: 15, 30, ... 90 degrees */
    { -3.261f, 1.812f, LAMP_SHADE }, { -2.562f, 3.5f, LAMP_SHADE }, { -1.450f, 4.950f, LAMP_SHADE },
    { 0.0f, 6.062f, LAMP_SHADE }, { 1.688f, 6.761f, LAMP_SHADE }, { 3.5f, 7.0f },
};
static const Disc lamp_lens[] = { { 7, 0x7 } };   /* the face's ground, the body colour: its marks go over it (eye_marks) */
/* An eye's face, as its look's picture facing the view has it (16 x 16
   texels, read texel by texel): marks in order, each a disc (r0 and t0,
   t1 0) or a band of a ring (r0 to r1, over turns t0 to t1 in degrees
   from across, down +), round (cx, cy) in texels from the face's middle,
   down +. The rim: the body colour (7) a texel wide, dark (F) round its
   bottom and lower left; inside it a ring of B, grey (C) round its upper
   left when plain. The white (E) a little left of the middle. Plain, the
   pupil black in a ring of B three texels out: a dark arch round its top,
   inside it a grey fleck and a white glint, a grey crescent along its
   bottom. Startled, the ring of B round a grey one: inside, a dark arch
   round a grey middle, white below. The lids: the body colour (7) above
   the middle, its crease a dark arc five texels out round its top with a
   light shine (8, B) inside it, the lash dark along its edge (a band of a ring 50 texels round, nearly straight); shut, the
   whole face 7, lighter (8) inside, the crease and the shine as the
   lid's, the rim dark round its lower left. */
typedef struct { float cx, cy, r0, r1, t0, t1; uint8_t nib; } Mark;
#define DISC(x, y, r, c) { x, y, 0, r, 0, 0, c }
#define BAND(x, y, r0, r1, t0, t1, c) { x, y, r0, r1, t0, t1, c }
#define RIM BAND(0, 0, 6.0f, 7.0f, 45, 200, 0xF), DISC(0, 0, 6.0f, 0xB)
#define RIM_PLAIN RIM, BAND(0, 0, 5.0f, 6.0f, 200, 300, 0xC)
#define PUPIL_PLAIN \
    DISC(-0.4f, 0.5f, 4.6f, 0xE), DISC(0, 0, 3.0f, 0xB), DISC(0, 0, 2.2f, 0xF), \
    BAND(0, 0.3f, 1.4f, 2.3f, 40, 140, 0xC), DISC(0, -1.0f, 1.0f, 0xC), DISC(0.5f, -1.4f, 0.5f, 0xE)
#define PUPIL_STARTLED \
    DISC(-0.1f, 0.2f, 5.0f, 0xE), DISC(0, 0.5f, 2.9f, 0xB), DISC(0, 0.5f, 2.3f, 0xC), DISC(0, 2.0f, 1.0f, 0xE), \
    BAND(0, 0.5f, 0.9f, 1.6f, 180, 360, 0xF), DISC(0, 0.5f, 0.8f, 0xC)
#define LID \
    BAND(0, 0, 0, 7.0f, 180, 360, 0x7), BAND(0, 0, 4.7f, 5.6f, 200, 340, 0xF), \
    DISC(0, -3.0f, 1.5f, 0x8), DISC(0, -3.0f, 0.8f, 0xB), BAND(0, 49.2f, 49.2f, 50.0f, 262, 278, 0xF)
static const Mark eye_plain[] = { RIM_PLAIN, PUPIL_PLAIN };
static const Mark eye_startled[] = { RIM, PUPIL_STARTLED };
static const Mark eye_half[] = { RIM_PLAIN, PUPIL_PLAIN, LID };
static const Mark eye_half_startled[] = { RIM, PUPIL_STARTLED, LID };
static const Mark eye_shut[] = { DISC(0, 0, 7.0f, 0x7), DISC(0, 0.5f, 5.0f, 0x8), BAND(0, 0, 4.7f, 5.6f, 200, 340, 0xF),
                                 BAND(0, 0, 2.2f, 3.0f, 210, 330, 0xB), BAND(0, 0, 6.0f, 7.0f, 90, 200, 0xF) };
static const Disc lamp_back[] = { { 1.812f, 0x7 } };

#define N(a) (int)(sizeof a / sizeof a[0])
static const Solid solids[] = {
    { 0xA1C8, 32, 0, tyre_rings, N(tyre_rings), tyre_side, tyre_side, N(tyre_side), N(tyre_side) },
    { 0xA1E4, 32, 0, tyre_rings, N(tyre_rings), tyre_side, tyre_side, N(tyre_side), N(tyre_side) },
    { 0xA190, 16, 2, lamp_rings, N(lamp_rings), lamp_lens, lamp_back, N(lamp_lens), N(lamp_back) },
    { 0xA104, 32, 1, NULL, 0, NULL, NULL, 0, 0, 1 },
    { 0xA120, 64, 1, NULL, 0, NULL, NULL, 0, 0, 2 },
};

static const Solid *object, *armed;
static const Mark *face;                /* an eye's face for its look (else the solid's front) */
static int nface;
static unsigned record;
static int coloured;
static uint8_t hi, tex_bank, tex_nib;
static uint16_t tex_base;
static float pos[3], view[3][3], scale;
/* The solid in the GPU's display list (scene.h): its float slot (scale
   times view, its place), and where the eye is in the solid's own
   coordinates (eye_obj: -view^T pos / scale), for its faces' facing,
   shading, lighting and order, decided here without turning a point */
static int fslot = -1;
static float eye_obj[3];

static void object_begin(void)
{
    float m[9];
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            m[3 * i + j] = scale * view[i][j];
    fslot = sc_list_fslot(m, pos);
    for (i = 0; i < 3; i++) {
        float d = 0;
        for (j = 0; j < 3; j++)
            d += view[i][j] * pos[j];
        eye_obj[i] = -d / scale;
    }
}

/* a point of the solid's own coordinates in view space (for the CPU's
   reference only) */
static void to_view(const float *o, float *out)
{
    int j;
    for (j = 0; j < 3; j++)
        out[j] = pos[j] + scale * (o[0] * view[0][j] + o[1] * view[1][j] + o[2] * view[2][j]);
}

/* a face of n points in the solid's own coordinates, colour c: the GPU's
   from the display list, and the CPU's from its view-space points when
   there is no list or the check wants the reference */
#define EMIT_MAX 72
static void emit(const float (*o)[3], int n, uint8_t c)
{
    static float v[EMIT_MAX][3];
    int id, i;
    if (fslot < 0) {                    /* no list: the points are view space already */
        fw_view_face(o, n, c);
        return;
    }
    id = sc_list_float_face(fslot, o, n, c);
    if (id >= 0 && !fw_gpu_face(id))
        return;
    for (i = 0; i < n && i < EMIT_MAX; i++)
        to_view(o[i], v[i]);
    fw_view_face((const float (*)[3])v, n < EMIT_MAX ? n : EMIT_MAX, c);
    if (id >= 0)
        fw_gpu_face_done();
}

/* a normal in the solid's coordinates turned into view space */
static void normal_view(const float *n, float *out)
{
    int j;
    for (j = 0; j < 3; j++)
        out[j] = n[0] * view[0][j] + n[1] * view[1][j] + n[2] * view[2][j];
}

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
    float a[3], s[3], c[3], m[3][3], e[3][3];
    int i, j, k;

    for (i = 0; i < 3; i++) {
        a[i] = (float)(uint16_t)-(int)((ram16((uint16_t)(0x0020 + 2 * i)) & 0xFF) << 8) / 65536.0f * 2 * (float)M_PI;
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
            e[i][j] = (int16_t)ram16((uint16_t)(0x00E4 + 6 * i + 2 * j)) / 32768.0f;
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
    face = NULL;
    if (!object || !fw_sprite_rows() || !fw_view_active() || sprites_kept() || fw_bp_active() || fw_nibble())   /* (a bitmap picture keeps its sprites; two players' too: the solids' colours are the 8-bit palette's) */
        return;
    if (object->shape == 0xA190) {      /* an eye: by its gaze, in its plain look */
        const uint8_t *w = nshvc_wram();
        uint16_t o = record < RM_RECORDS ? rm_record_object[record] : 0, g = record < RM_RECORDS ? rm_record_gaze[record] : 0;
        uint16_t look = (uint16_t)(w[(uint16_t)(0x205D + o)] | w[(uint16_t)(0x205E + o)] << 8);
        float phi, d[3];
        switch (look) {
        case 0x9AAF: face = eye_plain, nface = N(eye_plain); break;
        case 0x9AB1: face = eye_startled, nface = N(eye_startled); break;
        case 0x9AB3: face = eye_half, nface = N(eye_half); break;
        case 0x9AB5: face = eye_shut, nface = N(eye_shut); break;
        case 0x9AB7: face = eye_half_startled, nface = N(eye_half_startled); break;
        default: face = NULL;
        }
        if (!(g & 0x100) || !face)
            return;
        phi = (119.5f - (g & 0xFF)) * 2 * (float)M_PI / 256;
        d[0] = -sinf(phi), d[1] = 0, d[2] = -cosf(phi);
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
        scale = (float)(size ? size : 1) / object->texels;
    }
    if (rm_far_shift)                   /* a far object's place and size halved together */
        scale /= 1 << rm_far_shift;
    if (object->tree) {                 /* upright in the world: the view's matrix alone */
        int i, j;
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++)
                view[i][j] = (int16_t)ram16((uint16_t)(0x00E4 + 6 * i + 2 * j)) / 32768.0f;
    } else if (object->shape != 0xA190)
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
    tex_bank = (uint8_t)ram16(0x00A6);  /* its painting (FxSpriteMaterial's): a tree is shaped by it */
    tex_base = ram16(0x00A8);
    tex_nib = gsu.por & 0x04 ? 1 : 0;   /* COLOR's: the high nibble, else the low */
    coloured = 1;
}

/* a point of the solid, along its axis a, round it at turn t, radius r,
   in the solid's own coordinates */
static void place(const Solid *s, float a, float t, float r, float *out)
{
    float m[3];
    int j;
    m[s->axis] = a;
    m[s->axis ? 0 : 1] = r * cosf(t);
    m[s->axis ? 1 : 2] = r * sinf(t);
    if (fslot >= 0) {                   /* the display list's: its own coordinates */
        memcpy(out, m, sizeof m);
        return;
    }
    for (j = 0; j < 3; j++)             /* the CPU's alone: view space, as ever */
        out[j] = pos[j] + scale * (m[0] * view[0][j] + m[1] * view[1][j] + m[2] * view[2][j]);
}

/* a face's outward normal (from its first three points, turned away from
   the solid's centre), in the solid's coordinates, and whether it is
   seen: it faces the eye from its centre */
static int face_seen(const float (*p)[3], int n, float *nv, float *centre)
{
    float u[3], v[3], d = 0, e = 0;
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
    for (j = 0; j < 3; j++)             /* outward: from the solid's centre (its origin, or pos in view space) */
        d += nv[j] * (centre[j] - (fslot >= 0 ? 0 : pos[j]));
    if (d < 0)
        for (j = 0; j < 3; j++)
            nv[j] = -nv[j];
    for (j = 0; j < 3; j++)             /* towards the eye (at eye_obj, or the view's origin) */
        e += nv[j] * (centre[j] - (fslot >= 0 ? eye_obj[j] : 0));
    return e < 0;
}

static uint8_t shade(const Ring *g, const float *n)
{
    float nv[3], up;
    if (fslot >= 0)
        normal_view(n, nv);
    else
        memcpy(nv, n, sizeof nv);
    up = -nv[1] / sqrtf(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);   /* view y is down */
    if (!g->up)
        return g->nib;
    if (g->top && up > 0.8f)
        return g->top;
    return up > 0.45f ? g->up : up < -0.45f ? g->down : g->across;
}

/* a cap at a, its rings over it outward in, when it faces the eye */
static int cap_seen;                    /* the last cap's face is towards the eye */

static void cap(const Solid *s, float a, const Disc *d, int nd, int n)
{
    static float p[SEGMENTS_MAX][3];
    float nv[3], c[3];
    int i, k;
    for (k = 0; k < nd; k++) {
        int m = k ? (n / 2 < 8 ? 8 : n / 2) : n;
        for (i = 0; i < m; i++)
            place(s, a, 2 * (float)M_PI * i / m, d[k].r, p[i]);
        if (!k && !(cap_seen = face_seen((const float (*)[3])p, m, nv, c)))
            return;
        emit((const float (*)[3])p, m, (uint8_t)(hi | d[k].nib));
    }
}

/* an eye's face's marks (Mark) on its face plane at a, in order */
static void eye_marks(const Solid *s, float a, const Mark *mk, int nm, int n)
{
    static float p[SEGMENTS_MAX][3];
    float q[4][3];
    int k, i;
    for (k = 0; k < nm; k++) {
        const Mark *m = &mk[k];
        if (m->t0 == m->t1) {           /* a disc */
            int sides = m->r1 < 1.5f ? 12 : n;
            for (i = 0; i < sides; i++) {
                float t = 2 * (float)M_PI * i / sides, x = m->cx + m->r1 * cosf(t), y = m->cy + m->r1 * sinf(t);
                place(s, a, atan2f(y, x), hypotf(x, y), p[i]);
            }
            emit((const float (*)[3])p, sides, (uint8_t)(hi | m->nib));
            continue;
        }
        {                               /* a band of a ring */
            float t0 = m->t0 * (float)M_PI / 180, t1 = m->t1 * (float)M_PI / 180;
            int steps = (int)ceilf(n * (t1 - t0) / (2 * (float)M_PI));
            if (steps < 2)
                steps = 2;
            for (i = 0; i < steps; i++) {
                float u0 = t0 + (t1 - t0) * i / steps, u1 = t0 + (t1 - t0) * (i + 1) / steps;
                float xy[4][2] = { { m->cx + m->r0 * cosf(u0), m->cy + m->r0 * sinf(u0) },
                                    { m->cx + m->r0 * cosf(u1), m->cy + m->r0 * sinf(u1) },
                                    { m->cx + m->r1 * cosf(u1), m->cy + m->r1 * sinf(u1) },
                                    { m->cx + m->r1 * cosf(u0), m->cy + m->r1 * sinf(u0) } };
                int j, np = m->r0 > 0 ? 4 : 3;  /* from the middle: a wedge, its first two corners one */
                for (j = 0; j < np; j++) {
                    int jj = m->r0 > 0 ? j : j + 1;
                    place(s, a, atan2f(xy[jj][1], xy[jj][0]), hypotf(xy[jj][0], xy[jj][1]), q[j]);
                }
                emit((const float (*)[3])q, np, (uint8_t)(hi | m->nib));
            }
        }
    }
}

static void draw(const Solid *s)
{
    /* the larger of the focal lengths across and down: a texel the finer way */
    float rmax = 0, pr, f = fmaxf(fw_focal_x(), fw_focal_y()), p[4][3], nv[3], c[3];
    int n, i, k;

    object_begin();
    /* segments enough that the rim's flats are within half a texel of
       the circle: n >= pi / acos(1 - 0.5 / r), r its radius on the screen */
    for (k = 0; k < s->nrings; k++)
        if (s->rings[k].r > rmax)
            rmax = s->rings[k].r;
    pr = pos[2] > 0 ? rmax * scale * f / pos[2] : rmax;
    n = pr > 0.5f ? (int)ceilf((float)M_PI / acosf(1 - 0.5f / pr)) : 8;
    n = (n + 3) & ~3;
    if (n < 8)
        n = 8;
    if (n > SEGMENTS_MAX)
        n = SEGMENTS_MAX;
    /* the bands: a convex solid, so the faces it shows never overlap */
    for (k = 0; k + 1 < s->nrings; k++) {
        const Ring *g = &s->rings[k], *h = &s->rings[k + 1];
        for (i = 0; i < n; i++) {
            float t0 = 2 * (float)M_PI * i / n, t1 = 2 * (float)M_PI * (i + 1) / n;
            place(s, g->a, t0, g->r, p[0]);
            place(s, g->a, t1, g->r, p[1]);
            place(s, h->a, t1, h->r, p[2]);
            place(s, h->a, t0, h->r, p[3]);
            if (face_seen((const float (*)[3])p, 4, nv, c))
                emit((const float (*)[3])p, 4, (uint8_t)(hi | shade(g, nv)));
        }
    }
    cap(s, s->rings[s->nrings - 1].a, s->front, s->nfront, n);
    if (face && cap_seen)
        eye_marks(s, s->rings[s->nrings - 1].a, face, nface, n);
    cap(s, s->rings[0].a, s->back, s->nback, n);
}

/* ---- trees ------------------------------------------------------------- */
/*
 * A tree in the race's polygon style, of its paintings' size and shape:
 * its trunk and its canopy each turned about the world's upright, their
 * half widths from their paintings (the trunk every two rows in ten
 * sides, the canopy every three in 44, the widest of each), the canopy's
 * points moved in and out and up and down a little (the same for every
 * tree), so its small facets, about as tall as wide, take the light each
 * its own way and it reads as the painting's foliage.
 * The canopy's outline is eased ring to ring and every other ring turned
 * half a side, so its facets interlock. Flat faces in the paintings'
 * colours by how far each faces the light, from where the painting is
 * lit (where its lighter colours lie): each colour from a light on, set
 * once for the painting by drawing the part from the front (its faces
 * seen, least lit first, taking the painting's colours darkest first,
 * each for its share of the painting by area on the screen), the same
 * every picture after, so a face changes colour only as its light does. A painting faces the view
 * whatever the tree's angles, so a tree stands upright.
 */
#define PAINT_ROWS 64
typedef struct {
    int used, kind, texels;             /* the painting it is (its bank, base, nibble), measured once */
    uint8_t bank, nib;
    uint16_t base;
    float half[PAINT_ROWS], light[3], share[16], from[16];
    int top, bottom, ncolours;
    uint8_t colour[16];                 /* darkest (the higher nibble) first */
} Paint;

static void paint_measure(const Solid *s, Paint *p)
{
    float count[16] = { 0 }, total = 0, mean = 0, cx = 0, cy = 0, lx = 0, ly = 0, l;
    int u, v, k, T = s->texels;
    uint8_t c, rank[16] = { 0 };
#define TEXEL(u, v) ((fw_texel(tex_bank, (uint16_t)(tex_base + ((v) << 8) + (u))) >> (4 * tex_nib)) & 15)
    memset(p, 0, sizeof *p);
    p->top = -1;
    for (v = 0; v < T; v++) {
        int left = -1, right = -1;
        for (u = 0; u < T; u++)
            if ((c = TEXEL(u, v))) {
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
            if ((c = TEXEL(u, v)))
                mean += rank[c], cx += u + 0.5f, cy += v + 0.5f;
    mean /= total, cx /= total, cy /= total;
    for (v = 0; v < T; v++)
        for (u = 0; u < T; u++)
            if ((c = TEXEL(u, v)))
                lx += (rank[c] - mean) * (u + 0.5f - cx), ly += (rank[c] - mean) * (v + 0.5f - cy);
#undef TEXEL
    l = sqrtf(lx * lx + ly * ly);
    p->light[0] = l ? 0.75f * lx / l : 0, p->light[1] = l ? 0.75f * ly / l : 0, p->light[2] = -0.66f;
    l = sqrtf(p->light[0] * p->light[0] + p->light[1] * p->light[1] + p->light[2] * p->light[2]);
    for (k = 0; k < 3; k++)
        p->light[k] /= l;
}

/* a point of the tree: x and z across, y the world's down (texels from its
   painting's middle) */
/* a tree's point in its own coordinates (its slot turns it) */
static void tree_point(float x, float y, float z, float *out)
{
    int j;
    if (fslot >= 0) {                   /* the display list's: its own coordinates */
        out[0] = x;
        out[1] = y;
        out[2] = z;
        return;
    }
    for (j = 0; j < 3; j++)
        out[j] = pos[j] + scale * (x * view[0][j] + y * view[1][j] + z * view[2][j]);
}

#define TREE_FACES 4096
typedef struct { float p[4][3], key, lit, area; int n; uint8_t c; } TreeFace;
static TreeFace faces[TREE_FACES];

static int face_cmp(const void *a, const void *b)
{
    float x = ((const TreeFace *)a)->key, y = ((const TreeFace *)b)->key;
    return x > y ? -1 : x < y;
}

static TreeFace *by_lit[TREE_FACES];
static int lit_cmp(const void *a, const void *b)
{
    float x = (*(TreeFace *const *)a)->lit, y = (*(TreeFace *const *)b)->lit;
    return x < y ? -1 : x > y;
}

/* a body turned about the upright: ring k at down y[k], its points at
   radius r[k] (times jag[k][i], round it); its faces seen kept, drawn
   farthest first (it need not be convex) */
#define TREE_SIDES_MAX 48
/* a face of m points kept when seen, outward away from the upright at
   down ay; its light against the painting's and its area on the screen */
static int nf;
static float total_area;
static int calibrating;                  /* the painting's colours being set (tree_body) */
static void keep_face(const float (*q)[3], int m, float ay, const Paint *pt)
{
    float u[3], v[3], nv[3], c[3] = { 0 }, axis[3], d = 0, e = 0, l, fxy = fw_focal_x() * fw_focal_y(), cv[3], nvv[3];
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
    (void)cv;
    if (d < 0)                          /* outward: away from the upright */
        for (j = 0; j < 3; j++)
            nv[j] = -nv[j];
    for (j = 0; j < 3; j++)
        e += nv[j] * (c[j] - (fslot >= 0 ? eye_obj[j] : 0));
    if (e >= 0)
        return;
    l = sqrtf(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);
    if (fslot >= 0)
        normal_view(nv, nvv);
    else
        memcpy(nvv, nv, sizeof nvv);
    f->lit = l ? (nvv[0] * pt->light[0] + nvv[1] * pt->light[1] + nvv[2] * pt->light[2]) / l : -1;
    for (i = 0; i < m; i++)
        memcpy(f->p[i], q[i], sizeof f->p[i]);
    f->n = m;
    if (fslot >= 0)                     /* its order: its centre's distance from the eye */
        to_view(c, cv);
    else
        memcpy(cv, c, sizeof cv);
    f->key = cv[0] * cv[0] + cv[1] * cv[1] + cv[2] * cv[2];
    f->area = 0;
    if (calibrating || fslot < 0) {     /* its area on the screen, for the painting's colours (a fixed view) */
        float a[3], b[3];
        for (j = 0; j < m; j++) {
            if (fslot >= 0)
                to_view(q[j], a), to_view(q[(j + 1) % m], b);
            else
                memcpy(a, q[j], sizeof a), memcpy(b, q[(j + 1) % m], sizeof b);
            if (a[2] > 0 && b[2] > 0)
                f->area += (a[0] / a[2]) * (b[1] / b[2]) - (b[0] / b[2]) * (a[1] / a[2]);
        }
        f->area = fabsf(f->area) * fxy / 2;
    }
    total_area += f->area;
    nf++;
}

/* a body turned about the upright: ring k at down y[k], its points at
   radius r[k] (times jag[k][i], round it), every other ring turned half a
   side when staggered (the band between two such rings in triangles,
   each edge of a ring to the other ring's point beside it); its faces
   seen drawn farthest first (it need not be convex) */
/* The tree's points as tree_body's faces take them: ring k's point c (0
   to sides: point sides is point 0 turned the whole way round, its angle
   one turn on, as the faces' last edge reaches it) at radius r[k] times
   jag[k][c mod sides], angle 2 pi (c + o) / sides (o half a side on a
   turned ring), down y[k] + rise[k][c mod sides]: each made once for its
   painting, by the same steps a face's corner was (its cos and sin), and
   each tree's corners placed once (tree_point) for all the faces on it. */
#define RINGS_MAX (PAINT_ROWS / 2 + 4)
static float placed[RINGS_MAX][TREE_SIDES_MAX + 1][3];

static void tree_model(float (*m)[TREE_SIDES_MAX + 1][3], const float *y, const float *r, int nr, int sides,
                       const float (*jag)[TREE_SIDES_MAX], const float (*rise)[TREE_SIDES_MAX], int stagger)
{
    int k, c;
    for (k = 0; k < nr; k++) {
        float o = stagger && (k & 1) ? 0.5f : 0;
        for (c = 0; c <= sides; c++) {
            int w = c % sides;
            float ra = r[k] * jag[k][w], t = 2 * (float)M_PI * (c + o) / sides;
            m[k][c][0] = ra * cosf(t);
            m[k][c][1] = y[k] + rise[k][w];
            m[k][c][2] = ra * sinf(t);
        }
    }
}

static void tree_body(const float *y, int nr, int sides, float (*m)[TREE_SIDES_MAX + 1][3], const float *r,
                      int stagger, Paint *pt)
{
    int k, i;
    float at;
    nf = 0, total_area = 0;
    object_begin();                     /* its slot and the eye in its coordinates (calibration's own view too) */
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

/* A tree drawn at the detail its size on the screen asks: the wheels'
   rule (draw) for its sides, enough that its flats are within half a
   texel of round at its widest, on four levels (the full count, a half,
   a quarter, an eighth, at least 5); its rings merged two, four or eight
   painting rows at a time while they would come closer than 1.5 texels
   apart. Each level's rings and ring points are made once for its
   painting (TreeLevel); the colours' lights are set from the full
   detail, so a face's colour means the same light at every level. */
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
    float T = s->texels, *y = L->y, *r = L->r;
    int full = s->tree == 1 ? 10 : 44, sides = full >> sl, step = (s->tree == 1 ? 2 : 3) << rl, nr = 0, v, k, i;
    if (L->ok && L->T == T)
        return L;
    if (sides < 5)
        sides = 5;
    y[nr] = pp->top - T / 2, r[nr++] = 0;
    for (v = pp->top; v <= pp->bottom; v += step) {
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
    tree_model(L->m, y, r, nr, sides, (const float (*)[TREE_SIDES_MAX])jag, (const float (*)[TREE_SIDES_MAX])rise,
               s->tree == 2);
    L->nr = nr;
    L->sides = sides;
    L->T = T;
    L->ok = 1;
    return L;
}

static void tree(const Solid *s)
{
    int i, sl, rl, paint, full = s->tree == 1 ? 10 : 44, step = s->tree == 1 ? 2 : 3;
    static Paint paints[4];
    Paint *pp = NULL;
    TreeLevel *L;
    float px, rmax = 0, f = fmaxf(fw_focal_x(), fw_focal_y());
    for (i = 0; i < 4 && !pp; i++)      /* the painting, measured once */
        if (paints[i].used && paints[i].kind == s->tree && paints[i].bank == tex_bank && paints[i].base == tex_base &&
            paints[i].nib == tex_nib)
            pp = &paints[i];
    if (!pp) {
        for (i = 0; i < 4 && paints[i].used; i++)
            ;
        pp = &paints[i < 4 ? i : 0];
        paint_measure(s, pp);
        pp->used = 1, pp->kind = s->tree, pp->bank = tex_bank, pp->base = tex_base, pp->nib = tex_nib;
        pp->texels = 0;                 /* its colours' lights: not yet */
        for (sl = 0; sl < 4; sl++)      /* its levels: not yet */
            for (rl = 0; rl < 4; rl++)
                levels[pp - paints][sl][rl].ok = 0;
    }
    paint = (int)(pp - paints);
    if (!pp->ncolours || pp->top < 0)
        return;
    if (!pp->texels) {                  /* its colours' lights, once, at the full detail: the part from the front, far off */
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
        pp->texels = s->texels;
    }
    /* its detail: texels on the screen a painting texel, at its depth */
    for (i = pp->top; i <= pp->bottom; i++)
        if (pp->half[i] > rmax)
            rmax = pp->half[i];
    px = pos[2] > 0 ? scale * f / pos[2] : 1e9f;
    {
        float pr = rmax * px;
        int n = pr > 0.5f ? (int)ceilf((float)M_PI / acosf(1 - 0.5f / pr)) : 5;
        for (sl = 3; sl > 0 && (full >> sl < 5 ? 5 : full >> sl) < n; sl--)
            ;
    }
    for (rl = 0; rl < 3 && step * (1 << rl) * px < 1.5f; rl++)
        ;
    if (!(L = tree_level(s, pp, paint, sl, rl)))
        return;
    tree_body(L->y, L->nr, L->sides, L->m, L->r, s->tree == 2, pp);
}

/* FxObjectSprite returned (frame.c, 01:AD2D) */
void fw_solid_end(void)
{
    if (!armed)
        return;
    fw_sprite_hide(0);
    if (coloured && armed->tree)
        tree(armed);
    else if (coloured)
        draw(armed);
    armed = NULL;
    coloured = 0;
}
