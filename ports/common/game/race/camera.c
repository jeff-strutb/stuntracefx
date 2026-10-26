/*
 * camera.c: the race camera (camera.h). A camera record in work RAM
 * follows its target object: its mode bits (copied from the record) pick
 * how it lags behind. Its speed lead is set from the target's speed, its
 * view angles are eased toward the target's and turned into the Super FX
 * view matrix, and its position is pulled toward the target's position.
 */
#include "game.h"
#include "objects.h"
#include "glue.h"
#include "camera.h"

#define cam_flags       W16(0x05FA)     /* the mode bits, copied from the record */
#define cam_target      W16(0x1F33)     /* the object followed, copied from the record */
#define cam_lag         W16(0x1F2B)     /* the angle still to go; scale_lag also leaves v/8 here */
#define view_angle(i)   W16(0x065E + 2 * (i))   /* the three angles the view matrix is made from */
#define turned(i)       W16(0x00B7 + 2 * (i))   /* the record's offset turned by the view */
#define CAM(cam, off)   W16((uint16_t)((cam) + (off)))  /* a field of the camera record */

/* A signed halving n times (n >= 1; the sign stays), rounding toward zero:
   the last bit shifted out is added back when the result is negative. */
static uint16_t halve_round(uint16_t v, unsigned n)
{
    unsigned last = 0;

    while (n--) {
        last = v & 1u;
        v = (uint16_t)((v >> 1) | (v & 0x8000u));
    }
    return (v & 0x8000u) ? (uint16_t)(v + last) : v;
}

/* A signed halving n times, rounding down (toward minus infinity). */
static uint16_t halve_floor(uint16_t v, unsigned n)
{
    while (n--)
        v = (uint16_t)((v >> 1) | (v & 0x8000u));
    return v;
}

/* The angle word of an object's heading (+$15) or pitch (+$14): the byte
   in the high half, the low half clear. */
static uint16_t target_heading(uint16_t obj)
{
    return (uint16_t)(obj_heading(obj) << 8);
}

static uint16_t target_pitch(uint16_t obj)
{
    return (uint16_t)(obj_pitch(obj) << 8);
}

/* Moves `cur` a 1/2^n step of the way to `goal` (16-bit, rounding down). */
static uint16_t ease_to(uint16_t cur, uint16_t goal, unsigned n)
{
    return (uint16_t)(cur + halve_floor((uint16_t)(goal - cur), n));
}

/* The speed lead (+$26-$2A), kept only with bit 2 of the mode bits. The
   target's speed (+$4A of its Super FX record) is taken as a magnitude,
   halved (the one half at $0094, the quarter at $0008), and the lead is
   their negatives. The x lead ($0002) is zero. */
static void speed_lead(uint16_t cam)
{
    uint16_t speed, half, quarter;

    if (!(cam_flags & 0x0004)) {
        CAM(cam, 0x26) = 0;
        CAM(cam, 0x28) = 0;
        CAM(cam, 0x2A) = 0;
        return;
    }
    speed = P16(0x004A + obj_record(cam_target));
    if (speed & 0x8000)
        speed = (uint16_t)(0 - speed);
    half = halve_round(speed, 1);
    quarter = halve_round(half, 2);
    W16(0x0094) = half;
    W16(0x0008) = quarter;
    W16(0x0002) = 0;
    CAM(cam, 0x26) = (uint16_t)(0 - W16(0x0002));
    CAM(cam, 0x28) = (uint16_t)(0 - W16(0x0008));
    CAM(cam, 0x2A) = (uint16_t)(0 - W16(0x0094));
}

/* ScaleFiveThirtySeconds on the lag: 5/32 of it (v/32 + v/8), and the
   lag word is left holding v/8, as the original leaves it. */
static uint16_t scale_lag(uint16_t v)
{
    cam_lag = halve_floor(v, 3);
    return (uint16_t)scale_5_32((int16_t)v);
}

/* The turn eased toward the target's heading (negated, as the view turns).
   Its lag is the goal less the turn now. The record's own flags (+$42 of
   the record at $70:247A) decide a half-step case first; otherwise the
   mode bits 10-12 pick the step: a half of the lag, a quarter, 5/32 or
   5/64 and 5/128 of it, or the lag whole. */
static uint16_t turn_eased(uint16_t cur, uint16_t y, uint16_t flags)
{
    uint16_t goal = (uint16_t)(0 - target_heading(y));
    uint16_t lag = (uint16_t)(goal - cur);
    uint16_t record = P16(0x247A);
    uint16_t w;
    unsigned n;

    if ((P16(0x0042 + record) & 0x000C) && !(flags & 0x0200))
        return (uint16_t)(cur + halve_floor(lag, 1));
    cam_lag = lag;
    switch (flags & 0x1C00) {
    case 0x0400:
        /* the record's speed word at $70:006C + record, as a magnitude:
           below $7EF4 a quarter of the lag, else an eighth */
        w = P16(0x006C + record);
        if (w & 0x8000)
            w = (uint16_t)(0 - w);
        n = ((uint16_t)(w - 0x7EF4) & 0x8000) ? 2 : 1;
        return (uint16_t)(cur + halve_round(lag, n));
    case 0x0800:
        return (uint16_t)(cur + halve_round(lag, 2));
    case 0x0C00:
        return (uint16_t)(cur + halve_round(scale_lag(lag), 1));
    case 0x1000:
        return (uint16_t)(cur + halve_round(scale_lag(lag), 2));
    default:
        return (uint16_t)(cur + lag);
    }
}

/* One frame of the camera record at `cam`, following its target. */
void camera_follow_step(uint16_t cam, uint16_t caller_y)
{
    uint16_t flags, y, turn, pitch, goal, cur;
    unsigned i;

    cam_target = CAM(cam, 0x02);
    cam_flags = CAM(cam, 0x00);
    if (cam_flags & 0x0002)
        return;                         /* held: only the mode bits and target are taken */

    speed_lead(cam);

    /* the record's offset (+$1A-$1E) goes to the Super FX as the vector
       to turn (direct page $02, $08, $94) */
    W16(0x0002) = CAM(cam, 0x1A);
    W16(0x0008) = CAM(cam, 0x1C);
    W16(0x0094) = CAM(cam, 0x1E);

    flags = cam_flags;
    if (flags & 0x0100) {
        /* bit 8: the view stays level */
        view_angle(0) = 0;
        view_angle(1) = 0;
        view_angle(2) = 0;
    } else {
        y = cam_target;
        if (!(flags & 0x1C00)) {
            turn = (uint16_t)(0 - target_heading(y));
            pitch = halve_floor(target_pitch(y), 2);
        } else {
            turn = turn_eased(CAM(cam, 0x16), y, flags);
            pitch = halve_round(target_pitch(y), 2);
        }
        CAM(cam, 0x16) = turn;
        view_angle(1) = turn;
        CAM(cam, 0x14) = pitch;
        view_angle(0) = pitch;
        view_angle(2) = 0;
    }

    /* the view: the three angles plus the record's offsets (+$35, +$37,
       +$39), the pitch negated; the view matrix made by the Super FX, kept
       at $064C, and the offset turned by the view */
    view_angle(0) = (uint16_t)(0 - (view_angle(0) + CAM(cam, 0x35)));
    view_angle(1) = (uint16_t)(view_angle(1) + CAM(cam, 0x37));
    view_angle(2) = (uint16_t)(view_angle(2) + CAM(cam, 0x39));
    gsu_view_matrix();
    for (i = 0; i < 9; i++)
        W16(0x064C + 2 * i) = W16(0x0608 + 2 * i);
    gsu_vector_turn();

    /* the position follows the target's plus the turned offset: with bit 3
       each axis eases (x and z by a quarter, y by an eighth); else at once,
       except the height, which eases with bit 4 */
    flags = cam_flags;
    if (flags & 0x0008) {
        /* the Y the position reads: the target (or with bit 8 the caller's Y),
           whose low byte alone is kept by the view call's wrapper (Gsu8325
           clears Y's high byte): a quirk of the original, kept */
        y = (flags & 0x0100 ? caller_y : cam_target) & 0x00FF;
        CAM(cam, 0x08) = ease_to(CAM(cam, 0x08), (uint16_t)(OBJ_W(y, 0x0C) + turned(0)), 2);
        CAM(cam, 0x0A) = ease_to(CAM(cam, 0x0A), (uint16_t)(OBJ_W(y, 0x0E) + turned(1)), 3);
        CAM(cam, 0x0C) = ease_to(CAM(cam, 0x0C), (uint16_t)(OBJ_W(y, 0x10) + turned(2)), 2);
    } else {
        y = cam_target;
        CAM(cam, 0x08) = (uint16_t)(OBJ_W(y, 0x0C) + turned(0));
        goal = (uint16_t)(OBJ_W(y, 0x0E) + turned(1));
        if (flags & 0x0010) {
            cur = CAM(cam, 0x0A);
            /* a half of the way when the goal is below, else a quarter */
            goal = ease_to(cur, goal, (goal - cur) & 0x8000 ? 1 : 2);
        }
        CAM(cam, 0x0A) = goal;
        CAM(cam, 0x0C) = (uint16_t)(OBJ_W(y, 0x10) + turned(2));
    }
}
