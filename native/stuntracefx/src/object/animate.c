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

static ObjectHandler handler_of(const Object *o) { return object_handler_of(o->handler_bank, o->handler); }

/* frames that move only the object itself */
static int moves_itself(ObjectHandler h)
{
    switch (h) {
    case H_FAN_STEP: case H_FAN_CENTRE_STEP:       /* ObjectFanStep (a dolphin), its centre */
    case H_WHEEL_STEP: case H_WHEEL_CENTRE_STEP:       /* ObjectWheelStep (a dolphin), its centre */
    case H_SPINNER_STEP: case H_SPINNER_CENTRE_STEP:       /* ObjectSpinnerStep, its centre */
    case H_RING_STEP: case H_RING_CENTRE_STEP:       /* ObjectRingStep, its centre */
    case H_SWING_STEP:                      /* ObjectSwingStep */
    case H_FLOATER_STEP: case H_FLOATER_SPAWNER_STEP:       /* ObjectFloaterStep, ObjectFloaterSpawnerStep */
    case H_POP_MARKER_STEP:                      /* ObjectPopMarkerStep */
        return 1;
    }
    return 0;
}

/* frames that end a short life */
static int short_lived(ObjectHandler h)
{
    switch (h) {
    case H_PIECE_FALL: case H_SPARKLE_STEP: case H_TRAIL_STEP: case H_BURST_PIECE_STEP: case H_EXHAUST_STEP: /* ObjectPieceFall, ObjectSparkleStep, ObjectTrailStep, ObjectBurstPieceStep, ObjectExhaustStep */
    case H_SHARD_STEP: case H_RISING_SPARK: case H_BALL_SPARK:                               /* ObjectShardStep, ObjectRisingSpark, ObjectSparkFollow */
    case H_TRAIL_MID_STEP: case H_TRAIL_LEFT_STEP: case H_TRAIL_RIGHT_STEP:                               /* ObjectTrailMidStep, LeftStep, RightStep */
    case H_BLINKER_RISE: case H_BLINKER_FADE:                                              /* ObjectBlinkerRise, ObjectBlinkerFade */
        return 1;
    }
    return 0;
}

/* a frame that spins: a lap ball (+$15 and +$14 by $10 a step) and its
   set-up; a bumper (+$15 by $0C a step) and its set-up */
static int lap_ball(ObjectHandler h) { return h == H_LAP_BALL_UP_STEP || h == H_LAP_BALL_DOWN_STEP || h == H_LAP_BALL_UP || h == H_LAP_BALL_DOWN; }
static int bumper(ObjectHandler h) { return h == H_BUMPER_STEP || h == H_BUMPER; }
/* a bonus marker's frame (its bounce, 09:C11F) and its set-up (offset 0,
   speed 2, colour 0) */
static int marker_setup(ObjectHandler h) { return h == H_BONUS_MARKER_2 || h == H_BONUS_MARKER_5; }
static int marker(ObjectHandler h) { return h == H_BONUS_MARKER2_STEP || h == H_BONUS_MARKER5_STEP || marker_setup(h); }

/* set-ups whose look the far view draws before they run: their effect on
   the record is known (setup_look) or there is none */
static int look_known(ObjectHandler h)
{
    switch (h) {
    case H_NONE: case H_FLAGS_RECORD_SETUP:       /* no handler; ObjectFlagsRecordSetup */
    case H_COLOUR_2: case H_COLOUR_3: case H_COLOUR_4: case H_COLOUR_5: case H_COLOUR_6: case H_COLOUR_7: /* ObjectColour2-7 */
    case H_HEADING_90: case H_HEADING_315: case H_HEADING_225: case H_HEADING_45: /* ObjectHeading90, E0, A0, 20 */
    case H_FLOATER_SPAWNER:                      /* ObjectFloaterSpawner: its scale $40 (its floater is its trigger's) */
    case H_FLAGS_RECORD_FRAME: case H_FRAME_SWING_SETUP:       /* ObjectFlagsRecordFrame, ObjectFrameSwingSetup: a frame shown */
    case H_LAP_LIGHT: case H_LAP_LIGHT_LEFT: case H_LAP_LIGHT_RIGHT: /* ObjectLapLight, Left, Right */
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
    ObjectHandler h = handler_of(o);
    Look *l = &look[k];

    if (run) {
        ran[k] = 1;
        ObjectChildPlaced(o);           /* a car's tyre: its seat (draw_list.c) */
    }
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
            l->col = setup ? 0 : (uint8_t)(o->shown_frame & 0x7F);
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
    ObjectHandler h = handler_of(o);

    if (!ran[k] && !look_known(h))
        return 1;                       /* waiting for its set-up: its trigger */
    return !ran_now[k] && short_lived(h);
}

/* a value of the drawn-only look at the picture's place in its step */
static int32_t between(int32_t was, int32_t now)
{
    float t = fx_tween_t();

    if (t < 0.0f)
        return now;
    return was + (int32_t)lroundf((float)(now - was) * t);
}

/* ObjectColour2-7's colour */
static uint8_t colour_of(ObjectHandler h)
{
    switch (h) {
    case H_COLOUR_2: return 2;
    case H_COLOUR_3: return 3;
    case H_COLOUR_4: return 4;
    case H_COLOUR_5: return 5;
    case H_COLOUR_6: return 6;
    default: return 7;
    }
}

/* What a set-up that has not run gives the record r, far: the colour
   ObjectColourN puts in +$18 (the record's +$1A, on the near side), the
   scale +$15 (+$05) of ObjectScaleNN and ObjectFloaterSpawner's, the frame
   +$5A (+$19) of ObjectFlagsRecordFrame (+$48) and ObjectFrameSwingSetup
   (0), and a lap light's colour 0 and scale. */
static void setup_look(const Object *o, uint8_t *r, ObjectHandler h)
{
    switch (h) {
    case H_COLOUR_2: case H_COLOUR_3: case H_COLOUR_4: case H_COLOUR_5: case H_COLOUR_6: case H_COLOUR_7:
        if (!g.draw_side)
            r[0x1A] = colour_of(h);
        break;
    case H_HEADING_90: case H_FLOATER_SPAWNER: r[0x05] = 0x40; break;
    case H_HEADING_315: r[0x05] = 0xE0; break;
    case H_HEADING_225: r[0x05] = 0xA0; break;
    case H_HEADING_45: r[0x05] = 0x20; break;
    case H_FLAGS_RECORD_FRAME: r[0x19] = (uint8_t)(o->flags_record.frame & 0x7F); break;
    case H_FRAME_SWING_SETUP: r[0x19] = 0x00; break;
    case H_LAP_LIGHT: case H_LAP_LIGHT_LEFT: case H_LAP_LIGHT_RIGHT:
        r[0x19] = 0x00;
        r[0x05] = (uint8_t)(o->heading + (h == H_LAP_LIGHT_LEFT ? 0xE0 : h == H_LAP_LIGHT_RIGHT ? 0x20 : 0));
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
typedef struct { ObjectHandler h; int r1, r2, r3, xz; } Orbit;
static const Orbit orbits[] = {
    { H_FAN_STEP, 0x64, 0x46, 0x00, 0 },  /* ObjectFanStep: a dolphin */
    { H_WHEEL_STEP, 0x64, 0x46, 0x00, 1 },  /* ObjectWheelStep: a dolphin */
    { H_RING_STEP, 0x64, 0x00, 0x00, 1 },  /* ObjectRingStep */
    { H_SPINNER_STEP, 0x28, 0x0A, 0x00, 1 },  /* ObjectSpinnerStep */
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
    ObjectHandler h = handler_of(oe);
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
