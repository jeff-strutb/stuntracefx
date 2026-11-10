/*
 * animate.c: the objects as the far view and the pictures between steps
 * show them (plan stage 10; the earlier remaster's animate.c). The original
 * runs an object's handlers only while it is in the fixed area of map cells
 * around the car (ObjectGroupsActivate: +$36 = 3, counting down to 0
 * outside) and draws only the block ahead of the view, so nothing it draws
 * far is ever still. The far view draws every object however far
 * (draw_list.c's far records), and then:
 *
 *   - an object whose first handler has never run is in the look it has
 *     before its set-up (a dolphin at its centre, a lamp 116 units out of
 *     place and unlit): it is not drawn far until it has run, unless what
 *     its set-up does to its look is known and drawn instead (a colour, a
 *     scale, a frame shown, a lap light, a lap ball's, a bumper's or a
 *     bonus marker's look) or there is none;
 *   - an object whose frame moves only itself (the dolphins and the other
 *     pieces circling a centre, their centres, the swing, the floater, the
 *     pop marker) is kept running wherever it is: +$36 = 3 each step as the
 *     trigger list's are; a sound it plays then is left out;
 *   - an object whose frame moves itself and also plays a part in the race
 *     (a lap ball's and a bumper's spin, a bonus marker's bounce, all with
 *     records the cars touch) keeps the original's state; its look is drawn
 *     going on as if it ran every step: the angles and height it is drawn
 *     with are its own plus what the steps it did not run would have added,
 *     a marker's colour its own cycle;
 *   - a frame that ends a short life (sparks, shards, pieces, trails) is not
 *     drawn far while it is stopped.
 *
 * Between steps (fx/fx_tween.c sets the places and angles; fx_tween_t says
 * how far through its step a picture is), a piece
 * circling a centre keeps to its circle (ObjectOrbitBetween), the drawn-only
 * looks and an eye's gaze go from the last step's to this one's.
 */
#include <math.h>
#include <string.h>

#include "fx/fx_tween.h"
#include "object/object.h"
#include "system/state.h"

static uint8_t ran[OBJECT_COUNT];       /* its handler has run since it was made */
static uint8_t ran_now[OBJECT_COUNT];   /* its handler ran this step */
static uint8_t kept[OBJECT_COUNT];      /* kept running this step outside the original's area */

/* drawn-only looks: added to the record's angles +$04/+$05 (in 256ths of
   their units) and its height; the colour drawn (with `colour`); a marker
   bounce's own offset and speed; and the same at the step's start */
typedef struct {
    int32_t a14, a15, dy;
    int16_t off, spd;
    uint8_t col, colour, on;
} Look;
static Look look[OBJECT_COUNT], look_was[OBJECT_COUNT];
static uint16_t gaze_was[OBJECT_COUNT][2];

static uint32_t handler_of(const Object *o) { return (uint32_t)o->handler_bank << 16 | o->handler; }

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
        return 1;
    }
    return 0;
}

/* frames that end a short life */
static int short_lived(uint32_t h)
{
    switch (h) {
    case 0x08D549: case 0x08EB07: case 0x08E934: case 0x08EDB6: case 0x08E244: /* ObjectPieceFall, ObjectSparkleStep, ObjectTrailStep, ObjectBurstPieceStep, ObjectExhaustStep */
    case 0x09BDE1: case 0x09BE92: case 0x09AE60:                               /* ObjectShardStep, ObjectRisingSpark, ObjectSparkFollow */
    case 0x09E5BD: case 0x09E641: case 0x09E6E1:                               /* ObjectTrailMidStep, LeftStep, RightStep */
    case 0x08EE63: case 0x08EE9F:                                              /* ObjectBlinkerRise, ObjectBlinkerFade */
        return 1;
    }
    return 0;
}

/* a frame that spins: a lap ball (+$15 and +$14 by $10 a step) and its
   set-up; a bumper (+$15 by $0C a step) and its set-up */
static int lap_ball(uint32_t h) { return h == 0x09ADB6 || h == 0x09AF17 || h == 0x09AD5E || h == 0x09AEBF; }
static int bumper(uint32_t h) { return h == 0x0991F6 || h == 0x0991CA; }
/* a bonus marker's frame (its bounce, 09:C11F) and its set-up (offset 0,
   speed 2, colour 0) */
static int marker_setup(uint32_t h) { return h == 0x09BEE8 || h == 0x09BFB0; }
static int marker(uint32_t h) { return h == 0x09BF34 || h == 0x09BFF6 || marker_setup(h); }

/* set-ups whose look the far view draws before they run: their effect on
   the record is known (setup_look) or there is none */
static int look_known(uint32_t h)
{
    switch (h) {
    case 0x000000: case 0x08CD2D:       /* no handler; ObjectFlagsRecordSetup */
    case 0x09C5C8: case 0x09C5D1: case 0x09C5DA: case 0x09C5E3: case 0x09C5EC: case 0x09C5F5: /* ObjectColour2-7 */
    case 0x098C0F: case 0x09C5FE: case 0x09C605: case 0x09C60C: /* ObjectHeading90, E0, A0, 20 */
    case 0x09C90C:                      /* ObjectFloaterSpawner: its scale $40 (its floater is its trigger's) */
    case 0x08CA45: case 0x08CAF6:       /* ObjectFlagsRecordFrame, ObjectFrameSwingSetup: a frame shown */
    case 0x09C244: case 0x09C280: case 0x09C2C5: /* ObjectLapLight, Left, Right */
        return 1;
    }
    return lap_ball(h) || bumper(h) || marker(h);
}

void ObjectMade(Object *o)
{
    unsigned k = obj_slot(o);

    ran[k] = kept[k] = ran_now[k] = 0;
    memset(&look[k], 0, sizeof look[k]);
    memset(&look_was[k], 0, sizeof look_was[k]);
    gaze_was[k][0] = gaze_was[k][1] = 0;
}

void ObjectsStepStart(void)
{
    memcpy(look_was, look, sizeof look);
    memcpy(gaze_was, objects.gaze, sizeof gaze_was);
}

/* ObjectGroupsActivate's end: the objects that move only themselves kept
   running */
void ObjectsKeepRunning(void)
{
    ObjRef r = objects.used_head;
    unsigned n = 0;

    memset(kept, 0, sizeof kept);
    for (; r && n < OBJECT_COUNT; r = obj_at(r)->next, n++) {
        Object *o = obj_at(r);
        unsigned k = obj_slot(o);

        if (!ran[k] || o->timer == 0x03 || (o->timer & 0x80) || !moves_itself(handler_of(o)))
            continue;
        o->timer = 0x03;
        kept[k] = 1;
    }
}

int ObjectKept(const Object *o) { return kept[obj_slot(o)]; }

/* ObjectsRunHandlers: object o's turn this step, `run` whether a handler
   ran; the drawn-only look of the frames that play a part in the race goes
   on a step as if it ran */
void ObjectTurn(const Object *o, int run)
{
    unsigned k = obj_slot(o);
    uint32_t h = handler_of(o);
    Look *l = &look[k];

    if (run)
        ran[k] = 1;
    ran_now[k] = (uint8_t)run;
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
        int16_t real = setup ? 0 : o->bonus_marker.bounce;

        if (!l->on) {
            l->on = l->colour = 1;
            l->off = real;
            l->spd = setup ? 2 : o->bonus_marker.bounce_speed;
            l->col = setup ? 0 : (uint8_t)(o->frame5a & 0x7F);
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
    } else {
        l->on = 0;
    }
}

int ObjectFarLeftOut(const Object *o)
{
    unsigned k = obj_slot(o);
    uint32_t h = handler_of(o);

    if (!ran[k] && !look_known(h))
        return 1;                       /* waiting for its set-up: its trigger */
    return !ran_now[k] && short_lived(h);
}

int ObjectsDrawnBetween(void) { return fx_tween_t() >= 0.0f; }

/* a value of the drawn-only look at the picture's place in its step */
static int32_t between(int32_t was, int32_t now)
{
    float t = fx_tween_t();

    if (t < 0.0f)
        return now;
    return was + (int32_t)lroundf((float)(now - was) * t);
}

/* What a set-up that has not run gives the record r, far: the colour
   ObjectColourN puts in +$18 (the record's +$1A, on the near side), the
   scale +$15 (+$05) of ObjectScaleNN and ObjectFloaterSpawner's, the frame
   +$5A (+$19) of ObjectFlagsRecordFrame (+$48) and ObjectFrameSwingSetup
   (0), and a lap light's colour 0 and scale. */
static void setup_look(const Object *o, uint8_t *r, uint32_t h)
{
    switch (h) {
    case 0x09C5C8: case 0x09C5D1: case 0x09C5DA: case 0x09C5E3: case 0x09C5EC: case 0x09C5F5:
        if (!g.draw_side)
            r[0x1A] = (uint8_t)(2 + (h - 0x09C5C8) / 9);
        break;
    case 0x098C0F: case 0x09C90C: r[0x05] = 0x40; break;
    case 0x09C5FE: r[0x05] = 0xE0; break;
    case 0x09C605: r[0x05] = 0xA0; break;
    case 0x09C60C: r[0x05] = 0x20; break;
    case 0x08CA45: r[0x19] = (uint8_t)(o->count48 & 0x7F); break;
    case 0x08CAF6: r[0x19] = 0x00; break;
    case 0x09C244: case 0x09C280: case 0x09C2C5:
        r[0x19] = 0x00;
        r[0x05] = (uint8_t)(o->heading + (h == 0x09C280 ? 0xE0 : h == 0x09C2C5 ? 0x20 : 0));
        break;
    }
}

void ObjectRecordLook(const Object *o, uint8_t *r, int far)
{
    unsigned k = obj_slot(o);
    const Look *l = &look[k], *lw = &look_was[k];

    if (far && !ran[k])
        setup_look(o, r, handler_of(o));
    if (!l->on)
        return;
    r[0x04] = (uint8_t)((r[0x04] * 256 + between(lw->a14, l->a14)) >> 8);
    r[0x05] = (uint8_t)((r[0x05] * 256 + between(lw->a15, l->a15)) >> 8);
    if (l->dy || lw->dy) {              /* the height relative to the view */
        uint16_t y = (uint16_t)(r[0x10] | r[0x11] << 8);

        y = (uint16_t)(y + between(lw->dy, l->dy));
        r[0x10] = (uint8_t)y;
        r[0x11] = (uint8_t)(y >> 8);
    }
    if (l->colour) {                    /* +$5A's colour: the record's +$19 */
        float t = fx_tween_t();

        r[0x19] = t >= 0.0f && t < 0.5f ? lw->col : l->col;
    }
}

int32_t ObjectLookDy(const Object *o)
{
    unsigned k = obj_slot(o);

    if (!look[k].on)
        return 0;
    return between(look_was[k].dy, look[k].dy);
}

/* The gaze of an eye drawn between steps: from the last step's to this
   one's, the short way round, when both are known */
uint16_t ObjectGazeBetween(const Object *o, int side, uint16_t gaze)
{
    uint16_t was = gaze_was[obj_slot(o)][side];
    float t = fx_tween_t();
    int d;

    if (t < 0.0f || !gaze || !was)
        return gaze;
    d = (int8_t)(uint8_t)(gaze - was);
    return (uint16_t)(0x100 | (uint8_t)(was + lroundf((float)d * t)));
}

/*
 * A piece circling its centre (+$27): the vector (r1, r2, r3) turned by the
 * centre's +$16 (RotateXY: x' = x cos + y sin, y' = y cos - x sin), then,
 * with xz, its +$14 (RotateXZFrom08, on y and z) and +$15 (RotateXZ, by -A,
 * on x and z), 32 times, from the centre's place (09:CEE8 and the others'
 * orbit). Fitted to the game's own places at the steps (Aqua Tunnel's
 * dolphins and spinners): within 121 units, its 8-bit rounding.
 */
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

int ObjectOrbitBetween(int slot, const ObjectTable *was, const ObjectTable *end, float t, int16_t add[3])
{
    const Object *ow = &was->object[slot], *oe = &end->object[slot];
    uint32_t h = handler_of(oe);
    const Orbit *b = NULL;
    const Object *cw, *ce;
    double a0[3], a1[3], at[3], p0[3], p1[3], pt[3];
    unsigned i;

    for (i = 0; i < sizeof orbits / sizeof orbits[0]; i++)
        if (orbits[i].h == h)
            b = &orbits[i];
    if (!b || h != handler_of(ow) || oe->orbiter.centre != ow->orbiter.centre || !oe->orbiter.centre
        || oe->orbiter.centre >= 1 + OBJECT_COUNT * OBJECT_SIZE || (oe->orbiter.centre - 1) % OBJECT_SIZE)
        return 0;
    for (i = 0; i < 3; i++) {
        uint16_t pw[3] = {ow->x, ow->y, ow->z}, pe[3] = {oe->x, oe->y, oe->z};
        int d = (int16_t)(uint16_t)(pe[i] - pw[i]);

        if (d > 0x0800 || d < -0x0800)  /* not set between (fx_tween's FAR) */
            return 0;
    }
    cw = &was->object[(oe->orbiter.centre - 1) / OBJECT_SIZE];
    ce = &end->object[(oe->orbiter.centre - 1) / OBJECT_SIZE];
    for (i = 0; i < 3; i++) {           /* +$14, +$15, +$16: each the short way round */
        uint8_t angw[3] = {cw->pitch, cw->heading, cw->roll}, ange[3] = {ce->pitch, ce->heading, ce->roll};
        int d = (int8_t)(uint8_t)(ange[i] - angw[i]);

        a0[i] = angw[i];
        a1[i] = a0[i] + d;
        at[i] = a0[i] + d * (double)t;
    }
    orbit_at(b, a0[2], a0[0], a0[1], p0);
    orbit_at(b, a1[2], a1[0], a1[1], p1);
    orbit_at(b, at[2], at[0], at[1], pt);
    for (i = 0; i < 3; i++)
        add[i] = (int16_t)lround(pt[i] - (p0[i] + (p1[i] - p0[i]) * (double)t));
    return 1;
}
