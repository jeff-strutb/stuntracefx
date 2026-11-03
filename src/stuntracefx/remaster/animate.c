/*
 * animate.c: Enhanced's objects as the far view and the in-between frames
 * show them (remaster.h). The original runs an object's handlers only
 * while it is in the fixed area of map cells around the car
 * (BucketTriggersFixedArea, ObjectGroupsActivate: +$36 = 3, counting down
 * to 0 outside), and draws only the block ahead of the view, so nothing it
 * draws far is ever still. Enhanced draws every object however far
 * (draw_list.c), and then:
 *
 *   - an object whose first handler has never run is in the look it has
 *     before its set-up (a dolphin at its centre, a lamp 116 units out of
 *     place and unlit): it is not drawn far until the original runs it,
 *     its trigger, unless what its set-up does to its look is known and
 *     drawn instead (a colour, a scale, a frame shown, a lap light, a lap
 *     ball's, a bumper's or a bonus marker's look: look_known) or there is
 *     none ($08:CD2D, the flags record's);
 *   - an object whose frame moves only itself (the dolphins and the other
 *     pieces circling a centre, their centres, the swing, the floater,
 *     the pop marker, the bob) is kept running wherever it is: +$36 = 3
 *     each step as the trigger list's are; a sound it plays outside the
 *     original's area is left out;
 *   - an object whose frame moves itself and also plays a part in the
 *     race (a lap ball's and a bumper's spin, a bonus marker's bounce, all
 *     with records the cars touch) keeps the original's state; its look is
 *     drawn going on as if it ran every step: the angles, height and scale
 *     the record is drawn with are its own plus what the steps it did not
 *     run would have added, a marker's colour its own cycle;
 *   - a frame that ends a short life (sparks, shards, pieces, trails) is
 *     not drawn far while it is stopped.
 *
 * The in-between frames (tween.c) set an object's angles between two
 * steps to a 256th of a unit, which the model drawer's matrix takes in the
 * angle words' low bytes (render/model.c); a piece circling a centre keeps
 * to its circle between the steps' places (rm_orbit_between); and an
 * object made during the step is not set between from what its slot held
 * before.
 *
 * Nothing here reads the console's memory but by nshvc_wram(), and the
 * game's runs only in Enhanced's race (rm_whole_steps).
 */
#include <math.h>
#include <string.h>

#include "machine.h"
#include "console_ext.h"
#include "remaster.h"

#define OBJECTS 0x136u                  /* object slots, $62 bytes each from $7E:2001 */
#define SLOT(o) ((unsigned)((uint16_t)((o) - 1) / 0x62u))

static uint8_t ran[OBJECTS];            /* its handler has run since it was made */
static uint8_t made[OBJECTS];           /* made during this step (tween.c) */
static uint8_t kept[OBJECTS];           /* kept running this step outside the original's area */
static uint8_t ran_now[OBJECTS];        /* its handler ran this step */

/* drawn-only looks: added to the record's angles +$14/+$15 (256ths) and
   its height; the colour drawn (with `colour`); the marker bounce's own
   offset and speed; and the same at the tick's start, for the in-between
   frames */
typedef struct {
    int32_t a14, a15, dy;
    int16_t off, spd;
    uint8_t col, colour, on;
} Look;
static Look look[OBJECTS], look_was[OBJECTS];

/* the angle fractions (256ths of a unit) of each object's +$14-$16 as the
   in-between frame sets them (tween.c), and each draw record's */
static uint8_t frac[OBJECTS][3];
uint8_t rm_record_frac[RM_RECORDS][3];
const uint8_t *rm_angle_frac;           /* the record being drawn's (render/frame.c, model.c) */
static int tween_num, tween_den;        /* an in-between frame's place, num/den (0/0: none) */

static const uint8_t *w(void) { return nshvc_wram(); }

static uint16_t word(unsigned a) { return (uint16_t)(w()[a] | w()[a + 1] << 8); }

static uint32_t handler(uint16_t o)
{
    unsigned a = 0x2000u + o;
    return (uint32_t)w()[a + 0x1C] << 16 | word(a + 0x1A);
}

static int on(void) { return rm_enhanced() && rm_whole_steps(); }

/* frames that move only the object itself */
static int moves_itself(uint32_t h)
{
    switch (h) {
    case 0x09CEE8: case 0x09CFD5:       /* ObjectFanStep (a dolphin), its centre */
    case 0x09D023: case 0x09D131:       /* ObjectWheelStep (a dolphin), its centre */
    case 0x09CAF8: case 0x09CBC4:       /* ObjectSpinnerStep, its centre */
    case 0x09C9F0: case 0x09CAA3:       /* ObjectRingStep, its centre */
    case 0x09C62D:                      /* ObjectSwingStep */
    case 0x09C95C: case 0x09C956:       /* ObjectFloaterStep, ObjectFloaterSpawnerStep */
    case 0x09AD50:                      /* ObjectPopMarkerStep */
    case 0x09C478:                      /* ObjectBobStep */
        return 1;
    }
    return 0;
}

/* frames that end a short life */
static int short_lived(uint32_t h)
{
    switch (h) {
    case 0x08D549: case 0x08EB07: case 0x08E934: case 0x08EDB6: case 0x08E244:
    case 0x09BDE1: case 0x09BE92: case 0x09AE60:
    case 0x09E5BD: case 0x09E641: case 0x09E6E1:
    case 0x08EE63: case 0x08EE9F:
        return 1;
    }
    return 0;
}

/* a frame that spins: a lap ball (+$15 and +$14 by $10 a step) and its
   set-up; a bumper (+$15 by $0C a step, settled) and its set-up */
static int lap_ball(uint32_t h) { return h == 0x09ADB6 || h == 0x09AF17 || h == 0x09AD5E || h == 0x09AEBF; }
static int bumper(uint32_t h) { return h == 0x0991F6 || h == 0x0991CA; }
/* a bonus marker's frame (MarkerBounceY, 09:C11F) and its set-up (offset
   0, speed 2, colour 0: bonus_marker2_init) */
static int marker_setup(uint32_t h) { return h == 0x09BEE8 || h == 0x09BFB0; }
static int marker(uint32_t h) { return h == 0x09BF34 || h == 0x09BFF6 || marker_setup(h); }

/* set-ups whose look the far view draws before they run: their effect on
   the record is known (rm_record_look) or there is none */
static int look_known(uint32_t h)
{
    switch (h) {
    case 0x000000: case 0x08CD2D:       /* no handler; ObjectFlagsRecordSetup */
    case 0x09C5C8: case 0x09C5D1: case 0x09C5DA: case 0x09C5E3: case 0x09C5EC: case 0x09C5F5:   /* ObjectColour2-7 */
    case 0x098C0F: case 0x09C5FE: case 0x09C605: case 0x09C60C:   /* ObjectScale40, E0, A0, 20 */
    case 0x09C90C:                      /* ObjectFloaterSpawner: its scale $40 (its floater is its trigger's) */
    case 0x08CA45: case 0x08CAF6:       /* ObjectFlagsRecordFrame, ObjectFrameSwingSetup: a frame shown */
    case 0x09C244: case 0x09C280: case 0x09C2C5:   /* ObjectLapLight, Left, Right */
        return 1;
    }
    return lap_ball(h) || bumper(h) || marker(h);
}

void rm_object_made(uint16_t o)
{
    unsigned k = SLOT(o);

    if (k >= OBJECTS)
        return;
    ran[k] = kept[k] = ran_now[k] = 0;
    made[k] = 1;
    memset(&look[k], 0, sizeof look[k]);
    memset(&look_was[k], 0, sizeof look_was[k]);
}

/* ObjectGroupsActivate's end (object/object.c): the objects that move only
   themselves kept running */
void rm_objects_keep_running(void)
{
    uint8_t *m = nshvc_wram();
    uint16_t o;
    unsigned n = 0;

    memset(kept, 0, sizeof kept);
    if (!on())
        return;
    for (o = word(0x0329); o && n < OBJECTS; o = word(0x2000u + o), n++) {
        unsigned k = SLOT(o);
        uint8_t s = m[0x2036u + o];
        if (k >= OBJECTS || !ran[k] || s == 0x03 || (s & 0x80) || !moves_itself(handler(o)))
            continue;
        m[0x2036u + o] = 0x03;
        kept[k] = 1;
    }
}

int rm_object_kept(uint16_t o)
{
    unsigned k = SLOT(o);

    return k < OBJECTS && kept[k];
}

/* ObjectsRunHandlers (object/object.c): object o's turn this step, `run`
   whether a handler ran; the drawn-only look of the frames that play a
   part in the race goes on a step as if it ran */
void rm_object_turn(uint16_t o, int run)
{
    unsigned k = SLOT(o), a = 0x2000u + o;
    uint32_t h;
    Look *l;

    if (k >= OBJECTS)
        return;
    if (run)
        ran[k] = 1;
    ran_now[k] = (uint8_t)run;
    if (!on())
        return;
    h = handler(o);
    l = &look[k];
    if (lap_ball(h)) {
        l->on = 1;
        if (!run) {
            l->a14 += 0x10 << 8;
            l->a15 += 0x10 << 8;
        }
    } else if (bumper(h)) {
        l->on = 1;
        if (!run)
            l->a15 += 0x0C << 8;
    } else if (marker(h)) {
        int setup = marker_setup(h);    /* not set up: its set-up's offset 0, speed 2 */
        int16_t real = setup ? 0 : (int16_t)word(a + 0x27);
        if (!l->on) {
            l->on = l->colour = 1;
            l->off = real;
            l->spd = setup ? 2 : (int16_t)word(a + 0x29);
            l->col = setup ? 0 : (uint8_t)(w()[a + 0x5A] & 0x7F);
            if (!setup)
                return;                 /* first seen set up: its own look this step */
        }
        l->off = (int16_t)(l->off + l->spd);
        if (l->off == 0x0E || l->off == -0x0E)
            l->spd = (int16_t)-l->spd;
        l->dy += l->off - (run ? real : 0);
        l->col = (uint8_t)((l->col + 1) % 0x0A);
        if (!run)
            l->a15 += 0x08 << 8;
    } else
        l->on = 0;
}

void rm_animate_tick_start(void)
{
    memcpy(look_was, look, sizeof look);
    memset(made, 0, sizeof made);
}

/* whether the far view leaves object o out (draw_list.c far_records) */
int rm_far_left_out(uint16_t o)
{
    unsigned k = SLOT(o);
    uint32_t h = handler(o);

    if (k >= OBJECTS)
        return 0;
    if (!ran[k] && !look_known(h))
        return 1;                       /* waiting for its set-up: its trigger */
    return !ran_now[k] && short_lived(h);
}

/* a value of the drawn-only look at the in-between frame's place */
static int32_t between(int32_t was, int32_t now)
{
    if (!tween_den)
        return now;
    return was + (int32_t)((int64_t)(now - was) * tween_num / tween_den);
}

/* What a set-up that has not run gives the record (rec), far: the colour
   ObjectColourN puts in +$18 (the record's +$1A, on the near side), the
   scale +$15 (+$05) of ObjectScaleNN and ObjectFloaterSpawner's frame, the
   frame +$5A (+$19) of ObjectFlagsRecordFrame (+$48) and
   ObjectFrameSwingSetup (0), and a lap light's colour 0 and scale. */
static void setup_look(uint16_t o, uint32_t rec, uint32_t h)
{
    const uint8_t *m = w();

    switch (h) {
    case 0x09C5C8: case 0x09C5D1: case 0x09C5DA: case 0x09C5E3: case 0x09C5EC: case 0x09C5F5:
        if (!m[0x1F07])
            wr8(rec + 0x1A, (uint8_t)(2 + (h - 0x09C5C8) / 9));
        break;
    case 0x098C0F: case 0x09C90C: wr8(rec + 0x05, 0x40); break;
    case 0x09C5FE: wr8(rec + 0x05, 0xE0); break;
    case 0x09C605: wr8(rec + 0x05, 0xA0); break;
    case 0x09C60C: wr8(rec + 0x05, 0x20); break;
    case 0x08CA45: wr8(rec + 0x19, (uint8_t)(m[0x2048u + o] & 0x7F)); break;
    case 0x08CAF6: wr8(rec + 0x19, 0x00); break;
    case 0x09C244: case 0x09C280: case 0x09C2C5:
        wr8(rec + 0x19, 0x00);
        wr8(rec + 0x05, (uint8_t)(m[0x2015u + o] + (h == 0x09C280 ? 0xE0 : h == 0x09C2C5 ? 0x20 : 0)));
        break;
    }
}

/* draw_list.c draw_record: object o's record at rec, number i, far or
   not: its angles with their fractions, its drawn-only look, and, far,
   what its set-up gives it before it runs */
void rm_record_look(uint16_t o, uint32_t rec, unsigned i, int far)
{
    unsigned k = SLOT(o), j;
    uint8_t f[3] = { 0, 0, 0 };
    int32_t a;

    if (k < OBJECTS && tween_den)
        memcpy(f, frac[k], 3);
    if (k < OBJECTS && on() && far && !ran[k])
        setup_look(o, rec, handler(o));
    if (k < OBJECTS && look[k].on && on()) {
        const Look *l = &look[k], *lw = &look_was[k];
        a = (int32_t)(rd8(rec + 0x04) << 8 | f[0]) + between(lw->a14, l->a14);
        wr8(rec + 0x04, (uint8_t)(a >> 8));
        f[0] = (uint8_t)a;
        a = (int32_t)(rd8(rec + 0x05) << 8 | f[1]) + between(lw->a15, l->a15);
        wr8(rec + 0x05, (uint8_t)(a >> 8));
        f[1] = (uint8_t)a;
        if (l->dy || lw->dy)            /* the height relative to the view */
            wr16(rec + 0x10, (uint16_t)(rd16(rec + 0x10) + between(lw->dy, l->dy)));
        if (l->colour)                  /* +$5A's colour: the record's +$19 */
            wr8(rec + 0x19, tween_den && 2 * tween_num < tween_den ? lw->col : l->col);
    }
    if (i < RM_RECORDS)
        for (j = 0; j < 3; j++)
            rm_record_frac[i][j] = f[j];
}

/* the drawn-only height added to object o's (draw_list.c far_record) */
int32_t rm_look_dy(uint16_t o)
{
    unsigned k = SLOT(o);

    if (k >= OBJECTS || !look[k].on || !on())
        return 0;
    return between(look_was[k].dy, look[k].dy);
}

/* tween.c rm_tween_buffers: an in-between frame num/den of the way */
void rm_animate_between(int num, int den)
{
    tween_num = num;
    tween_den = den;
    if (!den)
        memset(frac, 0, sizeof frac);
}

void rm_object_frac(unsigned k, unsigned i, uint8_t f)
{
    if (k < OBJECTS && i < 3)
        frac[k][i] = f;
}

int rm_object_made_now(unsigned k)
{
    return k < OBJECTS && made[k];
}

/* A piece circling its centre (+$27): the vector (r1, r2, r3) turned by the
   centre's +$16 (RotateXY: x' = x cos + y sin, y' = y cos - x sin), then,
   with xz, its +$14 (RotateXZFrom08, on y and z) and +$15 (RotateXZ, by
   -A, on x and z), 32 times, from the centre's place (09:CEE8 and the
   others' orbit). Fitted to the game's own places at the steps (Aqua
   Tunnel's dolphins and spinners): within 121 units, its 8-bit rounding. */
typedef struct { uint32_t h; int r1, r2, r3, xz; } Orbit;
static const Orbit orbits[] = {
    { 0x09CEE8, 0x64, 0x46, 0x00, 0 },  /* ObjectFanStep: a dolphin */
    { 0x09D023, 0x64, 0x46, 0x00, 1 },  /* ObjectWheelStep: a dolphin */
    { 0x09C9F0, 0x64, 0x00, 0x00, 1 },  /* ObjectRingStep */
    { 0x09CAF8, 0x28, 0x0A, 0x00, 1 },  /* ObjectSpinnerStep */
};

static void orbit_at(const Orbit *b, double a16, double a14, double a15, double v[3])
{
    double k = 2 * M_PI / 256, c, s, x = b->r1, y = b->r2, z = b->r3, t;

    c = cos(a16 * k), s = sin(a16 * k);
    t = x * c + y * s, y = y * c - x * s, x = t;
    if (b->xz) {
        c = cos(a14 * k), s = sin(a14 * k);
        t = y * c + z * s, z = z * c - y * s, y = t;
        c = cos(-a15 * k), s = sin(-a15 * k);
        t = x * c + z * s, z = z * c - x * s, x = t;
    }
    v[0] = 32 * x, v[1] = 32 * y, v[2] = 32 * z;
}

static uint16_t word_of(const uint8_t *m, unsigned a) { return (uint16_t)(m[a] | m[a + 1] << 8); }

/* tween.c: object slot k's place num/den of the way, set between the two
   steps' places along a line, moved onto its circle: the circle's place
   at the centre's angles set between, less the line between the circle's
   places at the two ends (0 at both: the steps' own places stay) */
void rm_orbit_between(uint8_t *to, const uint8_t *now, const uint8_t *was, unsigned k, int num, int den)
{
    unsigned o = 0x2000u + 1 + k * 0x62u, c, i;
    uint32_t h = (uint32_t)now[o + 0x1C] << 16 | word_of(now, o + 0x1A);
    const Orbit *b = NULL;
    double a0[3], a1[3], at[3], p0[3], p1[3], pt[3], t = (double)num / den;

    for (i = 0; i < sizeof orbits / sizeof orbits[0]; i++)
        if (orbits[i].h == h)
            b = &orbits[i];
    if (!b || h != ((uint32_t)was[o + 0x1C] << 16 | word_of(was, o + 0x1A)))
        return;
    c = word_of(now, o + 0x27);
    if (c != word_of(was, o + 0x27) || !c || c >= 1 + 0x136u * 0x62u)
        return;
    c += 0x2000u;
    for (i = 0; i < 3; i++) {           /* +$14, +$15, +$16: each the short way round */
        int d = (int8_t)(uint8_t)(now[c + 0x14 + i] - was[c + 0x14 + i]);
        a0[i] = was[c + 0x14 + i];
        a1[i] = a0[i] + d;
        at[i] = a0[i] + d * t;
    }
    orbit_at(b, a0[2], a0[0], a0[1], p0);
    orbit_at(b, a1[2], a1[0], a1[1], p1);
    orbit_at(b, at[2], at[0], at[1], pt);
    for (i = 0; i < 3; i++) {
        unsigned f = o + 0x0C + 2 * i;
        int16_t n = (int16_t)word_of(now, f), w0 = (int16_t)word_of(was, f);
        int d = (int16_t)(uint16_t)(n - w0), v;

        if (d > 0x0800 || d < -0x0800)  /* not set between (tween.c's FAR) */
            return;
        v = (int16_t)word_of(to, f) + (int)lround(pt[i] - (p0[i] + (p1[i] - p0[i]) * t));
        to[f] = (uint8_t)v;
        to[f + 1] = (uint8_t)(v >> 8);
    }
}
