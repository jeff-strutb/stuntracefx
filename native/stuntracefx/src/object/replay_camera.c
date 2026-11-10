/*
 * replay_camera.c: the replay camera director's handlers: a director object
 * moves the camera its +$4F links to around its car (+$06).
 *
 * Not converted, nothing in the game reaches them (see object/NOTES):
 * CameraSnapToTable, CameraTableStep, CameraFixedAhead, CameraHoldStep and
 * Step2, CameraFixedSide, CameraFollowCar and Step, CameraLiftOff and Step,
 * CameraBeside, BesideStep and BesideWait, CameraTrackHalf and Step,
 * CameraReturn and Step, CameraPathFollow and Step, CameraPath8643 and
 * 86FB, CameraLowBehind.
 */
#include <string.h>

#include "audio/sound.h"
#include "fx/fx.h"
#include "math/math.h"
#include "object/handlers.h"
#include "object/object.h"
#include "system/state.h"

static Camera *cam_of(const Object *o) { return camera_of(o->link); }
static Object *car_of(const Object *o) { return obj_at(o->parent); }

static void set_mode(Object *o, uint16_t mode) { cam_of(o)->mode = mode; }

/* the director's tail */
static void tail(Object *o)
{
    CameraFollowLink(o);
    o->timer = 3;
}

/* The camera put back on its car: mode 1, its turn words cleared, its
   place from its base, one step. */
static void snap(Object *o)
{
    Camera *c = cam_of(o);

    set_mode(o, 0x0001);
    memset(c->turn_words, 0, sizeof c->turn_words);
    c->place[0] = c->place_base[0];
    c->place[1] = c->place_base[1];
    c->place[2] = c->place_base[2];
    CameraFollowLink(o);
}

/*
 * @routine 08:8F03 CameraSnapToCar
 * WHAT IT DOES: Puts the camera back on its car at once: then its mode as
 * it was.
 */
void CameraSnapToCar(Object *o)
{
    uint16_t m = cam_of(o)->mode;

    snap(o);
    cam_of(o)->mode = m;
}

/* half the distance, rounded toward zero */
static uint16_t halve_toward_zero(uint16_t d)
{
    uint16_t r = (uint16_t)((d >> 1) | (d & 0x8000));

    return (r & 0x8000) ? (uint16_t)(r + (d & 1)) : r;
}

/*
 * @routine 08:8FFE CameraOverheadStep
 * WHAT IT DOES: The overhead shot's frame: while the count runs the car's
 * record +$42 gets bits 4 and 12, then bit 12 goes; on a cue the camera's
 * height flips with a sound; the camera moves half way to the director's
 * height and three quarters of the way from the director to the car in X
 * and Z; then the tail.
 */
void CameraOverheadStep(Object *o)
{
    Camera *c = cam_of(o);
    uint16_t rec = car_of(o)->record;
    const Object *t;

    if (o->director.frames) {
        o->director.frames--;
        rec_or(rec, FXREC_SIDES, 0x1010);
    } else {
        rec_and(rec, FXREC_SIDES, 0xEFFF);
    }
    if (o->director.state & DIRECTOR_CUE) {
        uint8_t v;

        o->director.state &= (uint8_t)~DIRECTOR_CUE;
        v = (uint8_t)(c->byte34 + 1);
        if (!((uint8_t)(v - 0x02) & 0x80)) {
            c->byte34 = 0;
            SoundPort0QueuePush(0x2F);
            o->y = 0xFC18;
        } else {
            c->byte34 = v;
            SoundPort0QueuePush(0x24);
            o->y = 0xF830;
        }
    }
    c->y = (uint16_t)(halve_toward_zero((uint16_t)(o->y - c->y)) + c->y);
    t = obj_at(c->object);
    c->x = (uint16_t)(ScaleThreeQuarters((uint16_t)(t->x - o->x)) + o->x);
    c->z = (uint16_t)(ScaleThreeQuarters((uint16_t)(t->z - o->z)) + o->z);
    tail(o);
}

/*
 * @routine 08:8F8E CameraOverhead
 * WHAT IT DOES: The overhead shot: the director and the camera's point at
 * the car's X and Z, $12C nearer, height -$3E8; mode 3; then its frame.
 */
void CameraOverhead(Object *o)
{
    Camera *c = cam_of(o);
    const Object *t = obj_at(c->object);

    o->x = t->x;
    o->y = 0xFC18;
    o->z = (uint16_t)(t->z + 0xFED4);
    c->x = o->x;
    c->y = o->y;
    c->z = o->z;
    set_mode(o, 0x0003);
    CameraFollowLink(o);
    ObjectSetHandler(o, H_CAMERA_OVERHEAD_STEP);
    o->director.frames = 0x0A;
    CameraOverheadStep(o);
}

/*
 * @routine 08:91D0 CameraPathStep
 * WHAT IT DOES: The path shot's frame: along its path; once there, a flag
 * set and back to the table, else the tail.
 */
void CameraPathStep(Object *o)
{
    if (CameraApproachTarget(o)) {
        fxram_put(FX_REPLAY_FLAGS, (uint16_t)(fxram_get(FX_REPLAY_FLAGS) | 0x0001));
        CameraDirectorTableStart(o);
        return;
    }
    tail(o);
}

static void path_shot(Object *o, uint16_t path)
{
    ObjectPathStart(o, path);
    CameraFollowLink(o);
    CameraPathStep(o);
}

/* @routine 08:917D CameraPathA: a path shot on path $885F */
void CameraPathA(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_PATH_STEP);
    set_mode(o, 0x0001);
    path_shot(o, PATH_SHOT_A);
}

/* @routine 08:919F CameraPathB: on path $882D, or $85B9 in HUD mode 3 */
void CameraPathB(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_PATH_STEP);
    set_mode(o, 0x0001);
    path_shot(o, g.hud_mode == 3 ? PATH_DEMO_SHOT : PATH_SHOT_B);
}

/* The drift frames: the camera's point moved by the director's velocity,
   the velocity eased toward (0, ty, 0); while the count runs the tail,
   then `done`. */
static void drift_frame(Object *o, uint16_t ty, void (*done)(Object *))
{
    Camera *c = cam_of(o);
    int moved;

    c->x = (uint16_t)(c->x + o->vx);
    c->y = (uint16_t)(c->y + o->vy);
    c->z = (uint16_t)(c->z + o->vz);
    o->vx = ApproachEighthMin8(o->vx, 0x0000, &moved);
    o->vy = ApproachEighthMin8(o->vy, ty, &moved);
    o->vz = ApproachEighthMin8(o->vz, 0x0000, &moved);
    if (!o->director.frames) {
        done(o);
        return;
    }
    o->director.frames--;
    tail(o);
}

static void drift_shot(Object *o, uint16_t handler, uint8_t count, int flat)
{
    ObjectSetHandler(o, handler);
    set_mode(o, 0x0003);
    o->director.frames = count;
    ObjectVelocityFromRecordOf(o, car_of(o));
    if (flat)
        o->vy = 0;
}

/* @routine 08:921D CameraDriftStep */
void CameraDriftStep(Object *o) { drift_frame(o, 0x0000, CameraDirectorSkip2); }

/* @routine 08:91EB CameraDrift */
void CameraDrift(Object *o)
{
    drift_shot(o, H_CAMERA_DRIFT_STEP, 0x1E, 1);
    CameraDriftStep(o);
}

/* @routine 08:9311 CameraDriftLongStep */
void CameraDriftLongStep(Object *o) { drift_frame(o, 0x0000, CameraDirectorSkip2); }

/* @routine 08:92DF CameraDriftLong */
void CameraDriftLong(Object *o)
{
    drift_shot(o, H_CAMERA_DRIFT_LONG_STEP, 0x28, 1);
    CameraDriftLongStep(o);
}

/* the zoom eased toward `to` */
static void zoom_frame(Object *o, uint16_t to, uint16_t (*fn)(uint16_t, uint16_t, int *))
{
    Camera *c = cam_of(o);
    int moved;

    c->pitch = fn(c->pitch, to, &moved);
    tail(o);
}

/* @routine 08:9488 CameraZoomInStep */
void CameraZoomInStep(Object *o) { zoom_frame(o, 0x1000, ApproachThirtySecondMin32); }

/* @routine 08:946C CameraZoomIn */
void CameraZoomIn(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_ZOOM_IN_STEP);
    set_mode(o, 0x0002);
    CameraZoomInStep(o);
}

/* @routine 08:93FF CameraRiseStep */
void CameraRiseStep(Object *o) { drift_frame(o, 0xFFF6, CameraZoomIn); }

/* @routine 08:93D7 CameraRise */
void CameraRise(Object *o)
{
    drift_shot(o, H_CAMERA_RISE_STEP, 0x28, 0);
    CameraRiseStep(o);
}

/* @routine 08:9502 CameraZoomOutStep */
void CameraZoomOutStep(Object *o) { zoom_frame(o, 0x2000, ApproachSixtyFourthMin64); }

/* @routine 08:94E6 CameraZoomOut */
void CameraZoomOut(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_ZOOM_OUT_STEP);
    set_mode(o, 0x0654);
    CameraZoomOutStep(o);
}

/* @routine 08:94DD CameraOrbitStep */
void CameraOrbitStep(Object *o)
{
    if (CameraApproachTarget(o))
        CameraZoomOut(o);
    else
        tail(o);
}

/*
 * @routine 08:94A1 CameraOrbit
 * WHAT IT DOES: The orbit shot: on path $88BB, the car's record flagged.
 */
void CameraOrbit(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_ORBIT_STEP);
    set_mode(o, 0x0655);
    ObjectPathStart(o, PATH_ORBIT);
    rec_or(car_of(o)->record, FXREC_FLAGS40, 0x0004);
    CameraOrbitStep(o);
}

/* @routine 08:958C CameraEndStep */
void CameraEndStep(Object *o) { tail(o); }

/* @routine 08:9564 CameraEnd */
void CameraEnd(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_END_STEP);
    rec_put(car_of(o)->record, FXREC_FLAGS1C, 0x0010);
    tail(o);
}

/*
 * @routine 08:953C CameraStillStep
 * WHAT IT DOES: The still shot's frame: the car's record held, the
 * camera's point from the table; at the count's end the end shot.
 */
void CameraStillStep(Object *o)
{
    rec_put(car_of(o)->record, FXREC_SIDES, 0x0410);
    if (!o->director.frames) {
        CameraEnd(o);
        return;
    }
    o->director.frames--;
    CameraPointFromTable(o);
    tail(o);
}

/* @routine 08:951B CameraStill */
void CameraStill(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_STILL_STEP);
    o->director.frames = 0x0A;
    cam_of(o)->word37 = 0x8000;
    CameraStillStep(o);
}

/* @routine 08:998B CameraPathGoStep */
void CameraPathGoStep(Object *o)
{
    CameraApproachTarget(o);
    tail(o);
}

/* @routine 08:997C CameraPathGo */
void CameraPathGo(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_PATH_GO_STEP);
    CameraPathGoStep(o);
}

/* A path shot that goes: on path p, mode 1 unless `keep_mode`, then
   CameraPathGo. */
static void path_go(Object *o, uint16_t path, int keep_mode)
{
    ObjectPathStart(o, path);
    if (!keep_mode)
        set_mode(o, 0x0001);
    CameraFollowLink(o);
    CameraPathGo(o);
}

/* @routine 08:9896 CameraPath8603 */
void CameraPathGo1(Object *o) { path_go(o, PATH_GO_1, 0); }
/* @routine 08:98CC CameraPath8679 */
void CameraPathGo2(Object *o) { path_go(o, PATH_GO_2, 0); }
/* @routine 08:98E7 CameraPath869B */
void CameraPathGo3(Object *o) { path_go(o, PATH_GO_3, 0); }
/* @routine 08:9902 CameraPath86B3 */
void CameraPathGo4(Object *o) { path_go(o, PATH_GO_4, 0); }
/* @routine 08:991D CameraPath86CB */
void CameraPathGo5(Object *o) { path_go(o, PATH_GO_5, 0); }
/* @routine 08:9946 CameraPath86E3 */
void CameraPathGo6(Object *o) { path_go(o, PATH_GO_6, 0); }
/* @routine 08:9938 CameraPath8709: the camera's mode kept */
void CameraPathGo7(Object *o) { path_go(o, PATH_GO_7, 1); }

/*
 * @routine 08:9A25 CameraLowHoldStep
 * WHAT IT DOES: The low shot's frame: the camera 100 above its target.
 */
void CameraLowHoldStep(Object *o)
{
    Camera *c = cam_of(o);

    c->place[0] = 0x0000;
    c->place[1] = 0xFF9C;
    c->place[2] = 0x0000;
    tail(o);
}

/* @routine 08:9A16 CameraLowHold */
void CameraLowHold(Object *o)
{
    ObjectSetHandler(o, H_CAMERA_LOW_HOLD_STEP);
    CameraLowHoldStep(o);
}

/*
 * @routine 08:9A0D CameraPathLowStep
 * WHAT IT DOES: Along its path; once there, the low shot held.
 */
void CameraPathLowStep(Object *o)
{
    if (CameraApproachTarget(o)) {
        CameraLowHold(o);
        return;
    }
    tail(o);
}

static void path_low(Object *o, uint16_t path)
{
    ObjectPathStart(o, path);
    set_mode(o, 0x0001);
    CameraFollowLink(o);
    ObjectSetHandler(o, H_CAMERA_PATH_LOW_STEP);
    CameraPathLowStep(o);
}

/* @routine 08:99DC CameraPathLow87B7 */
void CameraPathLow2(Object *o) { path_low(o, PATH_LOW_2); }
/* @routine 08:99E6 CameraPathLow875B */
void CameraPathLow1(Object *o) { path_low(o, PATH_LOW_1); }
