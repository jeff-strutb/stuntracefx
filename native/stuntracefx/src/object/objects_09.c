/*
 * objects_09.c: the course objects' handlers of bank $09: the set-up a
 * course's object list points at and the per-frame handlers they install.
 *
 * Not converted, nothing in the game reaches them: see object/NOTES.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "fx/fx.h"
#include "hud/hud.h"
#include "math/math.h"
#include "object/handlers.h"
#include "object/object.h"
#include "system/state.h"
#include "video/video.h"

#define SET_LOW(field, v) ((field) = (uint16_t)(((field) & 0xFF00) | (uint8_t)(v)))

static void handler(Object *o, uint16_t h)
{
    o->handler = h;
    o->handler_bank = 0x09;
}

static void handler_none(Object *o)
{
    o->handler = 0;
    o->handler_bank = 0;
}

/* +$24 bit 3, +$3A = 0, +$5F = t: an object lit by its own light */
/* the sign of a 16-bit compare: a below b */
static int below16(uint16_t a, uint16_t b) { return ((uint16_t)(a - b) & 0x8000) != 0; }

static void lamp(Object *o, uint8_t t)
{
    o->flags24 |= 0x08;
    o->byte3a = 0;
    o->byte5f = t;
}

/* a 16-bit add at an angle byte, the next byte taking the carry */
static void add_word_at(uint8_t *p, uint16_t v)
{
    uint16_t w = (uint16_t)((p[0] | p[1] << 8) + v);

    p[0] = (uint8_t)w;
    p[1] = (uint8_t)(w >> 8);
}

/*
 * @routine 09:8B3B ObjectLampTower
 * WHAT IT DOES: Sets up a lamp on a tower: lowered, its light, shape $A104,
 * +$09 bit 0 cleared; and a second light $578 higher when one can be made;
 * then no handler.
 */
void ObjectLampTower(Object *o)
{
    Object *n;

    o->y = (uint16_t)(o->y + 0xFF74);
    o->anim = LOOK_LAMP_LIGHT;
    lamp(o, 0x3C);
    o->model = MODEL_LAMP_TOWER;
    o->flags09 &= 0xFE;
    n = ObjectSpawn(MODEL_LAMP_TOWER_LIGHT);
    if (n) {
        lamp(n, 0x64);
        n->anim = LOOK_LAMP_LIGHT_RAISED;
        ObjectCopyPosition(n, o);
        n->y = (uint16_t)(n->y + 0xFA88);
        ObjectRebucket(n);
    }
    handler_none(o);
}

/* @routine 09:8BD4 ObjectLampLow: a lamp, its light raised; no handler */
void ObjectLampLow(Object *o)
{
    lamp(o, 0x50);
    o->anim = LOOK_LAMP_LIGHT_RAISED;
    o->y = (uint16_t)(o->y + 0xFE84);
    handler_none(o);
}

/* @routine 09:8C0F ObjectScale40: heading $40, a quarter turn */
void ObjectHeading90(Object *o)
{
    o->heading = 0x40;
}

/* @routine 09:8CD6 ObjectScoreGateStep: synced; +$25 bit 0 while both the
   sync and the contact summary have bit 0 */
void ObjectScoreGateStep(Object *o)
{
    if (ObjectSyncFromRecord(o) && (RecordContactSummary(o) & 0x01))
        o->gate.state |= GATE_DONE;
    else
        o->gate.state &= (uint8_t)~GATE_DONE;
}

/* @routine 09:8CAF ObjectScoreGateRecord */
void ObjectScoreGateRecord(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordPosition(o, 0x0F00, 0x0F00, 0x0F00);
    ObjectScoreGateStep(o);
}

/*
 * @routine 09:8C16 ObjectScoreGate
 * WHAT IT DOES: Sets up a scoring gate: its handlers and a counter object
 * tied to it; then the gate's record and frame.
 */
void ObjectScoreGate(Object *o)
{
    Object *n;

    o->handler2 = H9_SCORE_GATE_RECORD;
    o->handler2_bank = 0x09;
    handler(o, H9_SCORE_GATE_STEP);
    n = ObjectSpawn(MODEL_NONE);
    if (n) {
        ObjectCopyPosition(n, o);
        n->score_counter.gate = obj_ref(o);
        handler(n, H9_SCORE_COUNTER_STEP);
        n->score_counter.tick = 0x02;
        n->x = (uint16_t)(n->x + 0xF920);
        n->y = (uint16_t)(n->y + 0x00A0);
        n->z = (uint16_t)(n->z + 0x0F00);
        n->heading = 0x20;
        n->roll = 0x00;
        n->frame5a = 0x80;
        n->score_counter.count = 0;
        n->score_counter.slide = 0x50;
    }
    ObjectScoreGateRecord(o);
}

/* the count down, shown at +$5A with bit 7 */
static void counter_show(Object *o)
{
    if (o->score_counter.count)
        o->score_counter.count--;
    o->frame5a = (uint8_t)(o->score_counter.count | 0x80);
}

/* every third frame, following its gate */
static void counter_tick(Object *o)
{
    if (o->score_counter.tick) {
        o->score_counter.tick--;
        return;
    }
    o->score_counter.tick = 0x02;
    if (!(obj_at(o->score_counter.gate)->gate.state & GATE_DONE)) {
        o->score_counter.state &= (uint8_t)~COUNTER_COUNTING;
        counter_show(o);
        return;
    }
    if (!(o->score_counter.state & COUNTER_COUNTING)) {
        o->score_counter.state |= COUNTER_COUNTING;
        SoundPort0QueuePush(0x32);
    }
    if (o->score_counter.count != 0x09)
        o->score_counter.count++;
    o->frame5a = (uint8_t)(o->score_counter.count | 0x80);
}

static void counter_slide(Object *o)
{
    if (o->score_counter.slide) {
        int moved;

        o->score_counter.slide--;
        o->heading = ApproachQuarterMin4Byte(o->heading, 0x40, &moved);
        o->x = (uint16_t)(o->x + 0x005A);
    }
    counter_tick(o);
}

/*
 * @routine 09:8CF8 ObjectScoreCounterStep
 * WHAT IT DOES: The score counter's frame, by the lap count: rebucketed.
 */
void ObjectScoreCounterStep(Object *o)
{
    if (g.laps_counted[0]) {
        if (g.laps_counted[0] == 0x01) {
            if (!(o->score_counter.state & COUNTER_ON)) {
                o->score_counter.state |= COUNTER_ON;
                o->model = MODEL_SCORE_COUNTER_ON;
            }
            counter_tick(o);
        } else if (g.laps_counted[0] == 0x02) {
            counter_slide(o);
        }
    }
    ObjectRebucket(o);
}

/* @routine 09:8DF8 ObjectFlyingPieceStep */
void ObjectFlyingPieceStep(Object *o)
{
    ObjectSyncFromRecord(o);
    if (--o->piece.life == 0)
        ObjectRelease(o);
}

/*
 * @routine 09:8DDF ObjectFlyingPiece
 * WHAT IT DOES: Sets up a flying piece, released at once with no record.
 */
void ObjectFlyingPiece(Object *o)
{
    handler(o, H9_FLYING_PIECE_STEP);
    if (!GsuRecordNewBouncing(o)) {
        ObjectRelease(o);
        return;
    }
    GsuRecordVelocity(o);
    ObjectFlyingPieceStep(o);
}

static void spark(Object *o, uint8_t t, uint16_t vx, uint16_t vy, uint16_t vz)
{
    Object *n = ObjectSpawn(MODEL_FIREWORK_SPARK);

    if (!n)
        return;
    n->anim = LOOK_FIREWORK_SPARK;
    lamp(n, t);
    ObjectCopyPosition(n, o);
    handler(n, H9_FLYING_PIECE);
    n->vx = vx;
    n->vy = vy;
    n->vz = vz;
    n->piece.life = 0x0F;
}

/* the shell's burst: four sparks */
static void firework_burst(Object *o)
{
    spark(o, 0x14, 0x0028, 0xFFAB, 0x0000);
    spark(o, 0x03, 0xFFD8, 0xFFAB, 0x0000);
    spark(o, 0x0A, 0x0000, 0xFFA6, 0x0028);
    spark(o, 0x01, 0x0000, 0xFFA6, 0xFFD8);
}

/*
 * @routine 09:8E5A ObjectFireworkShellStep
 * WHAT IT DOES: The shell's frame: released when +$0A runs down; its size
 * bytes grow; on a hit it is released with a sound and bursts into sparks.
 */
void ObjectFireworkShellStep(Object *o)
{
    if (--o->piece.life == 0)
        ObjectRelease(o);
    add_word_at(&o->pitch, 0x000C);
    add_word_at(&o->heading, 0x000C);
    add_word_at(&o->roll, 0x000C);
    if (!ObjectSyncFromRecord(o))
        return;
    if (!(RecordContactSummary(o) & 0x01))
        return;
    ObjectRelease(o);
    SoundPort0QueuePush(0x56);
    firework_burst(o);
}

/* @routine 09:8E2D ObjectFireworkShellRecord */
void ObjectFireworkShellRecord(Object *o)
{
    GsuRecordNewBouncing(o);
    GsuRecordVelocity(o);
    GsuRecordSetMass(o, 0x08);
    GsuRecordPosition(o, 0x0190, 0x0190, 0x0190);
    ObjectFireworkShellStep(o);
}

/* @routine 09:8E0F ObjectFireworkShell */
void ObjectFireworkShell(Object *o)
{
    handler(o, H9_FIREWORK_SHELL_STEP);
    o->handler2 = H9_FIREWORK_SHELL_RECORD;
    o->handler2_bank = 0x09;
    ObjectFireworkShellRecord(o);
}

static void shell(Object *o, uint16_t dx, uint8_t pitch, uint8_t heading, uint8_t roll)
{
    Object *n = ObjectSpawn(MODEL_FIREWORK_SHELL);

    if (!n)
        return;
    ObjectCopyPosition(n, o);
    handler(n, H9_FIREWORK_SHELL);
    n->y = (uint16_t)(n->y + 0xFB00);
    n->z = (uint16_t)(n->z + 0xE700);
    n->x = (uint16_t)(n->x + dx);
    n->vx = 0;
    n->vy = 0;
    n->vz = 0x015E;
    n->pitch = pitch;
    n->heading = heading;
    n->roll = roll;
    n->piece.life = 0x1E;
}

/* the record's sync and its contact summary both with bit 0 */
static int hit(Object *o)
{
    return ObjectSyncFromRecord(o) && (RecordContactSummary(o) & 0x01);
}

/* @routine 09:908D ObjectFinishGateStep: when hit on the second lap, the
   first time, a sound and two firework shells */
void ObjectFinishGateStep(Object *o)
{
    if (!hit(o) || g.laps_counted[0] != 0x02 || (o->gate.state & GATE_DONE))
        return;
    o->gate.state |= GATE_DONE;
    SoundPort0QueuePush(0x11);
    shell(o, 0x03C0, 0x3C, 0x32, 0x23);
    shell(o, 0xFC40, 0x78, 0x3C, 0x0C);
}

/* @routine 09:9066 ObjectFinishGateRecord */
void ObjectFinishGateRecord(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordPosition(o, 0x1680, 0x1680, 0x1680);
    ObjectFinishGateStep(o);
}

/* @routine 09:9048 ObjectFinishGate */
void ObjectFinishGate(Object *o)
{
    o->handler2 = H9_FINISH_GATE_RECORD;
    o->handler2_bank = 0x09;
    handler(o, H9_FINISH_GATE_STEP);
    ObjectFinishGateRecord(o);
}

/* A byte eased toward a target by an approach routine: returns 1 when it
   was already there. */
static int ease8(uint8_t *field, uint8_t target, uint8_t (*to)(uint8_t, uint8_t, int *))
{
    int moved;

    *field = to(*field, target, &moved);
    return !moved;
}

static void handlers2(Object *o, uint16_t record_handler, uint16_t frame)
{
    o->handler2 = record_handler;
    o->handler2_bank = 0x09;
    handler(o, frame);
}

/*
 * @routine 09:91F6 ObjectBumperStep
 * WHAT IT DOES: The bumper's frame: while +$48 counts down it bulges; then it
 * settles back; a hit sets +$25 bit 0 the first time with a sound and a new
 * bulge; no hit clears the bit.
 */
void ObjectBumperStep(Object *o)
{
    if (o->bumper.bulge) {
        o->heading = (uint8_t)(o->heading + 0x06);
        o->pitch = (uint8_t)(o->pitch + 0x0A);
        o->roll = (uint8_t)(o->roll + 0x0A);
        o->bumper.bulge = (uint8_t)(o->bumper.bulge + 0xFF);
    } else if (!ease8(&o->pitch, 0x00, ApproachHalfMin2Byte)) {
        ease8(&o->roll, 0x00, ApproachHalfMin2Byte);
        o->heading = (uint8_t)(o->heading + 0x0C);
    }
    if (!hit(o)) {
        o->bumper.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (o->bumper.state & GATE_DONE)
        return;
    o->bumper.state |= GATE_DONE;
    SoundPort0QueuePush(0x54);
    o->bumper.bulge = 0x06;
}

/* @routine 09:91E8 ObjectBumperRecord */
void ObjectBumperRecord(Object *o)
{
    GsuRecordNewFalling(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x01);
    ObjectBumperStep(o);
}

/* @routine 09:91CA ObjectBumper */
void ObjectBumper(Object *o)
{
    handlers2(o, H9_BUMPER_RECORD, H9_BUMPER_STEP);
    ObjectBumperRecord(o);
}

/*
 * @routine 09:92BC ObjectSwitchStep
 * WHAT IT DOES: The switch's frame: once on its count eases toward 3; the
 * count is shown; while off, a hit turns it on with a sound.
 */
void ObjectSwitchStep(Object *o)
{
    if (o->switch_.state & SWITCH_ON)
        ease8(&o->switch_.frame, 0x03, ApproachSixteenthMin16Byte);
    o->frame5a = (uint8_t)(o->switch_.frame | 0x80);
    if (o->switch_.state & SWITCH_ON)
        return;
    if (!hit(o))
        return;
    o->switch_.state |= SWITCH_ON;
    o->heading = (uint8_t)(o->heading + 0x0C);
    SoundPort0QueuePush(0x55);
}

/* @routine 09:92AE ObjectSwitchRecord */
void ObjectSwitchRecord(Object *o)
{
    GsuRecordNewFalling(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x01);
    ObjectSwitchStep(o);
}

/* @routine 09:9289 ObjectSwitch */
void ObjectSwitch(Object *o)
{
    handlers2(o, H9_SWITCH_RECORD, H9_SWITCH_STEP);
    o->frame5a = 0x80;
    ObjectSwitchRecord(o);
}

/* @routine 09:93C4 ObjectPostAStep: on a hit, +$14 eases toward $33, the first post's bit */
void ObjectPostAStep(Object *o)
{
    if (!hit(o))
        return;
    ease8(&o->pitch, 0x33, ApproachHalfMin2Byte);
    g.posts_hit |= 0x01;
}

static void post_record(Object *o, uint16_t v)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordPosition(o, v, v, v);
}

/* @routine 09:939D ObjectPostARecord */
void ObjectPostARecord(Object *o)
{
    post_record(o, 0x01E0);
    ObjectPostAStep(o);
}

/* @routine 09:937F ObjectPostA */
void ObjectPostA(Object *o)
{
    handlers2(o, H9_POST_A_RECORD, H9_POST_A_STEP);
    ObjectPostARecord(o);
}

/* the sky ball launched from post o, rising toward target_z (the
   original stores the post there first and its caller then the target) */
static void sky_ball(Object *o, uint16_t target_z)
{
    Object *n = ObjectSpawn(MODEL_BALL);

    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->sky_ball.target_z = target_z;
    handler(n, H9_SKY_BALL_STEP);
    n->y = (uint16_t)(n->y + 0xFF40);
    n->z = (uint16_t)(n->z + 0x2BC0);
    n->heading = 0x80;
    n->sky_ball.unread3 = 0;
    n->sky_ball.unread1 = 0;
    n->sky_ball.unread2 = 0;
    n->sky_ball.life = 0x0A;
    n->sky_ball.follow = 0x0A;
    SoundPort0QueuePush(0x0E);
}

/*
 * @routine 09:9432 ObjectPostBStep
 * WHAT IT DOES: The second post's frame: on a hit its bit the first time;
 * once both posts are hit, the first time, a sky ball whose target is this
 * post's z less $2300.
 */
void ObjectPostBStep(Object *o)
{
    if (!hit(o))
        return;
    if (!(o->gate.state & POST_COUNTED)) {
        o->gate.state |= POST_COUNTED;
        g.posts_hit |= 0x02;
    }
    if (g.posts_hit != 0x03 || (o->gate.state & POST_BALL_LAUNCHED))
        return;
    o->gate.state |= POST_BALL_LAUNCHED;
    sky_ball(o, (uint16_t)(o->z + 0xDD00));
}

/* @routine 09:940B ObjectPostBRecord */
void ObjectPostBRecord(Object *o)
{
    post_record(o, 0x0F00);
    ObjectPostBStep(o);
}

/* @routine 09:93ED ObjectPostB */
void ObjectPostB(Object *o)
{
    handlers2(o, H9_POST_B_RECORD, H9_POST_B_STEP);
    ObjectPostBRecord(o);
}

/* the ball's X toward the player's, its Z toward the player's, $380 back */
static void ball_follow(Object *o)
{
    Object *p = obj_at(g.player_car);
    int moved;

    o->x = ApproachQuarterMin4(o->x, p->x, &moved);
    o->z = ApproachHalfMin2(o->z, p->z, &moved);
    o->z = (uint16_t)(o->z + 0xFC80);
}

static void ball_drop(Object *o, uint16_t dz, uint16_t dy, uint8_t t)
{
    o->z = (uint16_t)(o->z + dz);
    o->y = (uint16_t)(o->y + dy);
    ease8(&o->pitch, t, ApproachSixteenthMin16Byte);
}

/*
 * @routine 09:9520 ObjectSkyBallStep
 * WHAT IT DOES: The sky ball's frame: while short of its target it rises and
 * is released when +$0A runs out; past it, it follows the player for +$49
 * frames, then the first time drops a ball and keeps falling; rebucketed.
 */
void ObjectSkyBallStep(Object *o)
{
    if (o->sky_ball.target_z == o->z || below16(o->sky_ball.target_z, o->z)) {
        if (o->sky_ball.follow) {
            o->sky_ball.follow--;
            ball_follow(o);
        } else {
            if (!(o->sky_ball.state & SKY_BALL_DROPPED)) {
                Object *n;

                o->sky_ball.state |= SKY_BALL_DROPPED;
                n = ObjectSpawn(MODEL_RAISE_BALL);
                if (n) {
                    ObjectCopyPosition(n, o);
                    handler(n, H9_BALL_DROP);
                }
            }
            ball_drop(o, 0xFE80, 0xFFE2, 0xF8);
        }
    } else {
        ball_drop(o, 0xFFB0, 0xFF10, 0xC0);
        if (--o->sky_ball.life == 0)
            ObjectRelease(o);
    }
    ObjectRebucket(o);
}

/*
 * @routine 09:967F ObjectBallDropStep
 * WHAT IT DOES: The dropped ball's frame: it grows; on a car's contact it
 * raises that car's record +$6A, a sound, a spark at the player, and is
 * released.
 */
void ObjectBallDropStep(Object *o)
{
    uint16_t r;
    Object *n;

    o->heading = (uint8_t)(o->heading + 0x10);
    o->pitch = (uint8_t)(o->pitch + 0x10);
    if (!ObjectSyncFromRecord(o))
        return;
    for (;;) {
        r = RecordNextContact();
        if (!r)
            return;
        if (record_contacts.summary & 0x01)
            break;
    }
    GsuRecordBoostRaise(r, 0x40);
    SoundPort0QueuePush(0x25);
    n = ObjectSpawn(MODEL_RAISE_BALL);
    if (n) {
        Object *p = obj_at(g.player_car);

        n->follow_spark.life = 0x0A;
        ObjectCopyPosition(n, p);
        n->follow_spark.rise = 0;
        n->follow_spark.followed = g.player_car;
        handler(n, H9_BALL_SPARK);
    }
    ObjectRelease(o);
}

/* @routine 09:9652 ObjectBallDropRecord */
void ObjectBallDropRecord(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x14);
    GsuRecordPosition(o, 0x00A0, 0x00A0, 0x00A0);
    ObjectBallDropStep(o);
}

/* @routine 09:9634 ObjectBallDrop */
void ObjectBallDrop(Object *o)
{
    handlers2(o, H9_BALL_DROP_RECORD, H9_BALL_DROP_STEP);
    ObjectBallDropRecord(o);
}

static void cannon_ball(Object *o, uint16_t h, uint16_t dx, uint16_t dy, uint16_t dz, uint8_t s,
                        uint16_t rise, uint16_t drift, uint8_t life, uint8_t home)
{
    Object *n = ObjectSpawn(MODEL_BALL);

    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->cannon_ball.gate = obj_ref(o);
    handler(n, h);
    n->x = (uint16_t)(n->x + dx);
    n->y = (uint16_t)(n->y + dy);
    n->z = (uint16_t)(n->z + dz);
    n->heading = s;
    n->cannon_ball.unread = 0;
    n->cannon_ball.rise = rise;
    n->cannon_ball.drift = drift;
    n->cannon_ball.life = life;
    n->cannon_ball.homing = home;
    SoundPort0QueuePush(0x0E);
}

/*
 * @routine 09:9746 ObjectCannonGateStep
 * WHAT IT DOES: The cannon gate's frame: when hit on a lap other than the
 * one it last fired on, it takes that lap; lap 0 fires three balls one way,
 * lap 2 three the other.
 */
void ObjectCannonGateStep(Object *o)
{
    uint8_t lap;

    if (!hit(o))
        return;
    lap = g.laps_counted[0];
    if (lap == o->lap_gate.lap)
        return;
    o->lap_gate.lap = lap;
    if (lap == 0x00) {
        cannon_ball(o, H9_CANNON_BALL_LEFT, 0xC400, 0xF880, 0x0500, 0xC0, 0xFD60, 0x0000, 0x05, 0x09);
        cannon_ball(o, H9_CANNON_BALL_LEFT, 0xC400, 0xFC40, 0xF880, 0xC0, 0xFD40, 0xFE00, 0x05, 0x09);
        cannon_ball(o, H9_CANNON_BALL_LEFT, 0xC400, 0xF9C0, 0x0000, 0xC0, 0xFD20, 0x0200, 0x05, 0x09);
    } else if (lap == 0x02) {
        cannon_ball(o, H9_CANNON_BALL_RIGHT, 0x2580, 0xF880, 0x0500, 0x40, 0xFDC0, 0x0000, 0x12, 0x06);
        cannon_ball(o, H9_CANNON_BALL_RIGHT, 0x2580, 0xFC40, 0xF880, 0x40, 0xFDA0, 0xFF00, 0x12, 0x06);
        cannon_ball(o, H9_CANNON_BALL_RIGHT, 0x2580, 0xF9C0, 0x0000, 0x40, 0xFDC0, 0x0100, 0x12, 0x06);
    }
}

/* @routine 09:971F ObjectCannonGateRecord */
void ObjectCannonGateRecord(Object *o)
{
    post_record(o, 0x0F00);
    ObjectCannonGateStep(o);
}

/* @routine 09:96FC ObjectCannonGate */
void ObjectCannonGate(Object *o)
{
    handlers2(o, H9_CANNON_GATE_RECORD, H9_CANNON_GATE_STEP);
    o->lap_gate.lap = 0x04;
    ObjectCannonGateRecord(o);
}

/* the cannon ball's frame: for +$49 frames it homes on its gate, then
   flies off (dx a frame) until its life runs out; rebucketed */
static void cannon_ball_frame(Object *o, uint16_t dx)
{
    if (o->cannon_ball.homing) {
        const Object *gate = obj_at(o->cannon_ball.gate);
        int moved;

        o->cannon_ball.homing--;
        o->x = ApproachQuarterMin4(o->x, gate->x, &moved);
        o->z = ApproachQuarterMin4(o->z, gate->z, &moved);
        o->z = (uint16_t)(o->z + o->cannon_ball.drift);
        o->y = ApproachHalfMin2(o->y, gate->y, &moved);
        o->y = (uint16_t)(o->y + o->cannon_ball.rise);
    } else {
        o->z = (uint16_t)(o->z + o->cannon_ball.drift);
        o->x = (uint16_t)(o->x + dx);
        if (--o->cannon_ball.life == 0)
            ObjectRelease(o);
    }
    ObjectRebucket(o);
}

/* @routine 09:9ACB ObjectCannonBallLeft */
void ObjectCannonBallLeft(Object *o) { cannon_ball_frame(o, 0x0100); }
/* @routine 09:9AE8 ObjectCannonBallRight */
void ObjectCannonBallRight(Object *o) { cannon_ball_frame(o, 0xFC80); }

/* As hit(), the object's place given to its record instead */
static int hit_to(Object *o)
{
    return ObjectSyncToRecord(o) && (RecordContactSummary(o) & 0x01);
}

static void wave_ball(Object *o, uint16_t h, uint16_t dx, uint16_t dy, uint16_t dz, uint8_t s,
                      uint16_t rise, uint16_t gx, uint16_t gz, uint8_t life)
{
    Object *n = ObjectSpawn(MODEL_BALL);

    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->wave_ball.gate = obj_ref(o);
    handler(n, h);
    n->x = (uint16_t)(n->x + dx);
    n->y = (uint16_t)(n->y + dy);
    n->z = (uint16_t)(n->z + dz);
    n->heading = s;
    n->wave_ball.rise = rise;
    o->x = (uint16_t)(o->x + gx);
    o->z = (uint16_t)(o->z + gz);
    n->wave_ball.life = life;
    SoundPort0QueuePush(0x0E);
}

static void wave_gate_frame(Object *o, int which)
{
    if (!hit_to(o) || g.laps_counted[0] != 0x01 || (o->gate.state & GATE_DONE))
        return;
    o->gate.state |= GATE_DONE;
    if (which == 0)
        wave_ball(o, H9_WAVE_BALL_BIG, 0xDA80, 0xFE80, 0xDD00, 0xE0, 0xFF00, 0x0F00, 0x0F00, 0x0F);
    else if (which == 1)
        wave_ball(o, H9_WAVE_BALL_STEP, 0x2300, 0xFC40, 0xE200, 0x20, 0xFF60, 0xF100, 0x0F00, 0x0A);
    else
        wave_ball(o, H9_WAVE_BALL_STEP, 0x2300, 0xFC40, 0x1E00, 0x60, 0xFF60, 0xF100, 0xF100, 0x0C);
}

/* @routine 09:9C1C ObjectWaveGateAStep */
void ObjectWaveGateAStep(Object *o) { wave_gate_frame(o, 0); }
/* @routine 09:9C69 ObjectWaveGateBStep */
void ObjectWaveGateBStep(Object *o) { wave_gate_frame(o, 1); }
/* @routine 09:9CB6 ObjectWaveGateCStep */
void ObjectWaveGateCStep(Object *o) { wave_gate_frame(o, 2); }

/* @routine 09:9BF5 ObjectWaveGateRecord */
void ObjectWaveGateRecord(Object *o)
{
    post_record(o, 0x0F00);
    wave_gate_frame(o, 0);
}

/* @routine 09:9BD7 ObjectWaveGateA */
void ObjectWaveGateA(Object *o)
{
    handlers2(o, H9_WAVE_GATE_RECORD, H9_WAVE_GATE_A_STEP);
    ObjectWaveGateRecord(o);
}

/* @routine 09:9C4A ObjectWaveGateB */
void ObjectWaveGateB(Object *o) { handlers2(o, H9_WAVE_GATE_RECORD, H9_WAVE_GATE_B_STEP); }
/* @routine 09:9C97 ObjectWaveGateC */
void ObjectWaveGateC(Object *o) { handlers2(o, H9_WAVE_GATE_RECORD, H9_WAVE_GATE_C_STEP); }

/* the ball homes on its gate and rises; released when its life runs out */
static void wave_home(Object *o, const Object *gate)
{
    int moved;

    o->x = ApproachQuarterMin4(o->x, gate->x, &moved);
    o->z = ApproachQuarterMin4(o->z, gate->z, &moved);
    o->y = ApproachQuarterMin4(o->y, gate->y, &moved);
    o->y = (uint16_t)(o->y + o->wave_ball.rise);
    if (--o->wave_ball.life == 0)
        ObjectRelease(o);
}

/* @routine 09:9E9A ObjectWaveBallStep */
void ObjectWaveBallStep(Object *o)
{
    wave_home(o, obj_at(o->wave_ball.gate));
    ObjectRebucket(o);
}

/* @routine 09:A03F ObjectStarSparkleStep: released when +$0A runs out; its frame +$18 steps through 0-5 */
void ObjectStarSparkleStep(Object *o)
{
    uint8_t v;

    if (--o->piece.life == 0)
        ObjectRelease(o);
    v = (uint8_t)(o->sprite_turn[0] + 1);
    if (!(v & 0x80))
        v = (uint8_t)(v + 0x06);
    v &= 0x7F;
    if (v >= 0x06)
        v = (uint8_t)(v - 0x06);
    o->sprite_turn[0] = (uint8_t)(v | 0x80);
    ObjectRebucket(o);
}

/* a sparkle at the object's place, $C8 back */
static void sparkle(Object *o)
{
    Object *n = ObjectSpawn(MODEL_COMMON);

    if (!n)
        return;
    n->anim = LOOK_STAR_SPARKLE;
    n->sprite_turn[0] = 0x82;
    lamp(n, 0x64);
    ObjectCopyPosition(n, o);
    n->z = (uint16_t)(n->z + 0xFF38);
    n->piece.life = 0x08;
    handler(n, H9_STAR_SPARKLE_STEP);
}

/* @routine 09:9F63 ObjectWaveBallBigStep */
void ObjectWaveBallBigStep(Object *o)
{
    if (o->wave_ball.state & WAVE_BALL_HIT) {
        o->roll = (uint8_t)(o->roll + 0x18);
        o->heading = (uint8_t)(o->heading + 0x18);
        o->z = (uint16_t)(o->z + 0xFFCE);
        o->x = (uint16_t)(o->x + 0x028A);
        o->y = (uint16_t)(o->y + 0xFF06);
        sparkle(o);
    } else {
        wave_home(o, obj_at(o->wave_ball.gate));
    }
    if (!hit_to(o))
        return;
    if (o->wave_ball.state & WAVE_BALL_HIT)
        return;
    o->wave_ball.state |= WAVE_BALL_HIT;
    SoundPort0QueuePush(0x59);
}

/* @routine 09:9F3C ObjectWaveBallBigRecord */
void ObjectWaveBallBigRecord(Object *o)
{
    post_record(o, 0x0320);
    ObjectWaveBallBigStep(o);
}

/* @routine 09:9F14 ObjectWaveBallBigSetup */
void ObjectWaveBallBigSetup(Object *o)
{
    handlers2(o, H9_WAVE_BALL_BIG_RECORD, H9_WAVE_BALL_BIG_STEP);
    o->wave_ball.rise = 0xFFB0;
    ObjectWaveBallBigRecord(o);
}

/* @routine 09:9F05 ObjectWaveBallBig */
void ObjectWaveBallBig(Object *o)
{
    handler(o, H9_WAVE_BALL_BIG_SETUP);
    ObjectWaveBallBigSetup(o);
}

/*
 * @routine 09:A0C0 ObjectLanternStep
 * WHAT IT DOES: The lantern's frame: on its first hit a sound, it is
 * released and bursts into four sparks; no hit clears the bit.
 */
void ObjectLanternStep(Object *o)
{
    if (!hit(o)) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    SoundPort0QueuePush(0x10);
    ObjectRelease(o);
    spark(o, 0x14, 0x0028, 0xFFD3, 0x0000);
    spark(o, 0x03, 0xFFD8, 0xFFD3, 0x0000);
    spark(o, 0x0A, 0x0000, 0xFFCE, 0x0028);
    spark(o, 0x01, 0x0000, 0xFFCE, 0xFFD8);
}

/* @routine 09:A0B2 ObjectLanternRecord */
void ObjectLanternRecord(Object *o)
{
    GsuRecordNewSolidTrigger(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x02);
    ObjectLanternStep(o);
}

/* @routine 09:A06A ObjectLantern */
void ObjectLantern(Object *o)
{
    o->y = (uint16_t)(o->y + 0xFF88);
    o->anim = LOOK_LANTERN;
    lamp(o, 0x64);
    handlers2(o, H9_LANTERN_RECORD, H9_LANTERN_STEP);
    ObjectLanternRecord(o);
}

/* the record's contacts (bit 0 of the sync's result) */
static int synced(Object *o) { return ObjectSyncFromRecord(o) != 0; }

/* the next contact with bit 0; its touched record, or 0 when none */
static uint16_t next_contact(void)
{
    uint16_t r;

    while ((r = RecordNextContact()) != 0)
        if (record_contacts.summary & 0x01)
            return r;
    return 0;
}

static void gate_trigger(Object *o, uint16_t s)
{
    post_record(o, s);
}

/* @routine 09:A2D8 ObjectChimeGateStep: a chime entering and leaving */
void ObjectChimeGateStep(Object *o)
{
    if (hit(o)) {
        if (o->gate.state & CHIME_IN)
            return;
        o->gate.state |= CHIME_IN;
        o->gate.state &= (uint8_t)~CHIME_OUT;
        SoundPort1QueuePush(0x13);
        return;
    }
    if (o->gate.state & CHIME_OUT)
        return;
    o->gate.state |= CHIME_OUT;
    o->gate.state &= (uint8_t)~CHIME_IN;
    SoundPort1QueuePush(0x0E);
}

/* @routine 09:A2B1 ObjectChimeGateRecord */
void ObjectChimeGateRecord(Object *o)
{
    gate_trigger(o, 0x0F00);
    ObjectChimeGateStep(o);
}

/* @routine 09:A293 ObjectChimeGate */
void ObjectChimeGate(Object *o)
{
    handlers2(o, H9_CHIME_GATE_RECORD, H9_CHIME_GATE_STEP);
    ObjectChimeGateRecord(o);
}

/* a flash gate's frame: on the first hit the screen flashes and a fade starts */
static void flash_frame(Object *o, uint16_t flash, uint16_t step, uint16_t end)
{
    if (hit(o)) {
        if (o->gate.state & GATE_DONE)
            return;
        o->gate.state |= GATE_DONE;
        windows.level_step = flash;
        palettes.weather_step = step;
        palettes.weather_goal = end;
        g.picture_effects |= 0x21;
        return;
    }
    o->gate.state &= (uint8_t)~GATE_DONE;
}

/* @routine 09:A371 ObjectBrightGateStep */
void ObjectBrightGateStep(Object *o) { flash_frame(o, 0x0020, 0x0200, 0x2000); }

/* @routine 09:A34A ObjectBrightGateRecord */
void ObjectBrightGateRecord(Object *o)
{
    gate_trigger(o, 0x0780);
    ObjectBrightGateStep(o);
}

/* @routine 09:A32C ObjectBrightGate */
void ObjectBrightGate(Object *o)
{
    handlers2(o, H9_BRIGHT_GATE_RECORD, H9_BRIGHT_GATE_STEP);
    ObjectBrightGateRecord(o);
}

/* @routine 09:A408 ObjectDarkGateStep */
void ObjectDarkGateStep(Object *o) { flash_frame(o, 0xFFE0, 0xFE00, 0xFE00); }

/* @routine 09:A3E1 ObjectDarkGateRecord */
void ObjectDarkGateRecord(Object *o)
{
    gate_trigger(o, 0x0780);
    ObjectDarkGateStep(o);
}

/* @routine 09:A3C3 ObjectDarkGate */
void ObjectDarkGate(Object *o)
{
    handlers2(o, H9_DARK_GATE_RECORD, H9_DARK_GATE_STEP);
    ObjectDarkGateRecord(o);
}

/* the first car contact sets or clears bit 1 of its record's +$8A and plays a sound */
static void car_flag_frame(Object *o, int on, uint8_t snd)
{
    uint16_t r = synced(o) ? next_contact() : 0;

    if (!r) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    if (!palettes.weather) {
        if (on)
            GsuRecordCourseFlagsSet(r, 0x02);
        else
            GsuRecordCourseFlagsClear(r, 0x02);
    }
    SoundPort1QueuePush(snd);
}

/* @routine 09:A49D ObjectCarFlagOffGateStep */
void ObjectCarFlagOffGateStep(Object *o) { car_flag_frame(o, 0, 0x11); }

/* @routine 09:A476 ObjectCarFlagOffGateRecord */
void ObjectCarFlagOffGateRecord(Object *o)
{
    gate_trigger(o, 0x0780);
    ObjectCarFlagOffGateStep(o);
}

/* @routine 09:A458 ObjectCarFlagOffGate */
void ObjectCarFlagOffGate(Object *o)
{
    handlers2(o, H9_CAR_FLAG_OFF_RECORD, H9_CAR_FLAG_OFF_STEP);
    ObjectCarFlagOffGateRecord(o);
}

/* @routine 09:A52C ObjectCarFlagOnGateStep */
void ObjectCarFlagOnGateStep(Object *o) { car_flag_frame(o, 1, 0x10); }

/* @routine 09:A505 ObjectCarFlagOnGateRecord */
void ObjectCarFlagOnGateRecord(Object *o)
{
    gate_trigger(o, 0x0780);
    ObjectCarFlagOnGateStep(o);
}

/* @routine 09:A4E7 ObjectCarFlagOnGate */
void ObjectCarFlagOnGate(Object *o)
{
    handlers2(o, H9_CAR_FLAG_ON_RECORD, H9_CAR_FLAG_ON_STEP);
    ObjectCarFlagOnGateRecord(o);
}

/* the gate family: setup, restart (its record) and frame */
#define GATE(Setup, Record, Step, hr, hs, size)             \
    void Record(Object *o) { gate_trigger(o, size); Step(o); }   \
    void Setup(Object *o) { handlers2(o, hr, hs); Record(o); }

/* the slide's step, target and direction */
static void slide_set(uint16_t step, uint16_t target, uint16_t dir)
{
    g.slide_step = step;
    g.slide_target = target;
    g.slide_direction = dir;
}

static void fade_set(uint16_t step, uint16_t end)
{
    palettes.weather_step = step;
    palettes.weather_goal = end;
}

/* @routine 09:A5B9 ObjectSlideGateStep: on lap 1 the first contact clears
   the car's +$8A bit 1 and starts the slide down and a fade */
void ObjectSlideGateStep(Object *o)
{
    uint16_t r = synced(o) ? next_contact() : 0;

    if (!r || g.laps_counted[0] != 0x01 || (o->gate.state & GATE_DONE))
        return;
    o->gate.state |= GATE_DONE;
    g.picture_effects |= 0xA0;
    GsuRecordCourseFlagsClear(r, 0x02);
    fade_set(0x0020, 0x2000);
    slide_set(0x0040, 0xFFC0, 0x8000);
}

/* @routine 09:A574 ObjectSlideGate, 09:A592 ObjectSlideGateRecord */
GATE(ObjectSlideGate, ObjectSlideGateRecord, ObjectSlideGateStep, H9_SLIDE_GATE_RECORD, H9_SLIDE_GATE_STEP, 0x0780)

/* @routine 09:A6BE GateFlashOnClock: a sweep with a sound on even clock ticks */
static void GateFlashOnClock(void)
{
    if (((uint8_t)(g.clock_fraction >> 8) >> 4) & 0x01)
        return;
    g.picture_effects |= 0x10;
    SoundPort0QueuePush(0x36);
}

/* @routine 09:A6DC GateFadeUp */
static void GateFadeUp(void)
{
    g.picture_effects |= 0x20;
    fade_set(0x0080, 0x2000);
}

/* @routine 09:A666 ObjectLapFlashGateStep: a hit on laps 0 and 1 flashes;
   on lap 2 a fade and the gate is released */
void ObjectLapFlashGateStep(Object *o)
{
    uint8_t lap;

    if (!hit(o))
        return;
    lap = g.laps_counted[0];
    if (lap > 0x02)
        return;
    if (lap == 0x02) {
        if (o->gate.state & LAP_FLASH_FADED)
            return;
        o->gate.state |= LAP_FLASH_FADED;
        GateFadeUp();
        ObjectRelease(o);
        return;
    }
    if (o->gate.state & LAP_FLASH_FLASHED)
        return;
    o->gate.state |= LAP_FLASH_FLASHED;
    GateFlashOnClock();
}

/* @routine 09:A621 ObjectLapFlashGate, 09:A63F ObjectLapFlashGateRecord */
GATE(ObjectLapFlashGate, ObjectLapFlashGateRecord, ObjectLapFlashGateStep, H9_LAP_FLASH_RECORD, H9_LAP_FLASH_STEP, 0x0780)

/* @routine 09:A73E ObjectSweepGateStep */
void ObjectSweepGateStep(Object *o)
{
    if (!hit(o)) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (g.laps_counted[0] != 0x00 || (o->gate.state & GATE_DONE))
        return;
    o->gate.state |= GATE_DONE;
    g.picture_effects |= 0x10;
    SoundPort0QueuePush(0x36);
    ObjectRelease(o);
}

/* @routine 09:A6F9 ObjectSweepGate, 09:A717 ObjectSweepGateRecord */
GATE(ObjectSweepGate, ObjectSweepGateRecord, ObjectSweepGateStep, H9_SWEEP_GATE_RECORD, H9_SWEEP_GATE_STEP, 0x0780)

/* @routine 09:A7CD ObjectSlideChimeGateStep */
void ObjectSlideChimeGateStep(Object *o)
{
    ObjectSyncFromRecord(o);
    if (RecordContactSummary(o) & 0x01) {
        if (!(o->gate.state & SLIDE_CHIME_IN)) {
            o->gate.state |= SLIDE_CHIME_IN;
            o->gate.state &= (uint8_t)~SLIDE_CHIME_OUT;
            SoundPort1QueuePush(0x13);
        }
        if (g.laps_counted[0] != 0x01 || (o->gate.state & SLIDE_CHIME_SLID))
            return;
        o->gate.state |= SLIDE_CHIME_SLID;
        g.picture_effects |= 0xA0;
        slide_set(0x0080, 0xFF80, 0x8000);
        g.slide_wobble_step = 0xFF;
        fade_set(0x0020, 0x2000);
        return;
    }
    if (!(o->gate.state & SLIDE_CHIME_OUT)) {
        o->gate.state |= SLIDE_CHIME_OUT;
        SoundPort1QueuePush(0x0E);
    }
    o->gate.state &= (uint8_t)~SLIDE_CHIME_IN;
}

/* @routine 09:A788 ObjectSlideChimeGate, 09:A7A6 ObjectSlideChimeGateRecord */
GATE(ObjectSlideChimeGate, ObjectSlideChimeGateRecord, ObjectSlideChimeGateStep, H9_SLIDE_CHIME_RECORD, H9_SLIDE_CHIME_STEP, 0x0F00)

static void fade_frame(Object *o, uint8_t lap, uint16_t step, uint16_t end)
{
    if (!synced(o) || !next_contact() || g.laps_counted[0] != lap || (o->gate.state & GATE_DONE))
        return;
    o->gate.state |= GATE_DONE;
    g.picture_effects |= 0x20;
    fade_set(step, end);
}

/* @routine 09:A8BC ObjectFadeGate0Step */
void ObjectFadeGate0Step(Object *o) { fade_frame(o, 0x00, 0x000A, 0x1FFE); }
/* @routine 09:A877 ObjectFadeGate0, 09:A895 ObjectFadeGate0Record */
GATE(ObjectFadeGate0, ObjectFadeGate0Record, ObjectFadeGate0Step, H9_FADE_0_RECORD, H9_FADE_0_STEP, 0x0780)
/* @routine 09:A94C ObjectFadeGate2Step */
void ObjectFadeGate2Step(Object *o) { fade_frame(o, 0x02, 0x0020, 0x1F20); }
/* @routine 09:A907 ObjectFadeGate2, 09:A925 ObjectFadeGate2Record */
GATE(ObjectFadeGate2, ObjectFadeGate2Record, ObjectFadeGate2Step, H9_FADE_2_RECORD, H9_FADE_2_STEP, 0x0780)

/* @routine 09:A9DC ObjectSlideZoneStep: while a car is in the zone the
   slide runs up; otherwise back down */
void ObjectSlideZoneStep(Object *o)
{
    if (hit(o)) {
        palettes.target = 0x7FFF;
        slide_set(0x0100, 0x1000, 0x0000);
        g.picture_effects |= 0x80;
        return;
    }
    slide_set(0x0100, 0xFF00, 0x8000);
    g.slide_wobble_step = 0xFF;
}

/* @routine 09:A997 ObjectSlideZone, 09:A9B5 ObjectSlideZoneRecord */
GATE(ObjectSlideZone, ObjectSlideZoneRecord, ObjectSlideZoneStep, H9_SLIDE_ZONE_RECORD, H9_SLIDE_ZONE_STEP, 0x0F00)

/* @routine 09:AA78 ObjectBonusMarkerStep */
void ObjectBonusMarkerStep(Object *o)
{
    uint16_t r = synced(o) ? next_contact() : 0;

    if (!r || g.laps_counted[0] != 0x00)
        return;
    g.bonus_markers_taken++;
    GsuRecordCourseFlagsSet(r, 0x01);
    if (((uint8_t)(g.clock_fraction >> 8) >> 4) & 0x03) {
        g.picture_effects |= 0x10;
        SoundPort0QueuePush(0x36);
    }
    ObjectRelease(o);
}

/* @routine 09:AA33 ObjectBonusMarker, 09:AA51 ObjectBonusMarkerRecord */
GATE(ObjectBonusMarker, ObjectBonusMarkerRecord, ObjectBonusMarkerStep, H9_BONUS_MARKER_RECORD, H9_BONUS_MARKER_STEP, 0x0780)

/* @routine 09:AB01 ObjectBonusGateStep */
void ObjectBonusGateStep(Object *o)
{
    uint16_t r = synced(o) ? next_contact() : 0;

    if (!r) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    g.picture_effects = (uint8_t)((g.picture_effects & 0xF7) | 0x04);
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    SoundPort1QueuePush(0x10);
    if (g.laps_counted[0] != 0x00 || !g.bonus_markers_taken)
        return;
    GsuRecordCourseFlagsSet(r, 0x01);
}

/* @routine 09:AABC ObjectBonusGate, 09:AADA ObjectBonusGateRecord */
GATE(ObjectBonusGate, ObjectBonusGateRecord, ObjectBonusGateStep, H9_BONUS_GATE_RECORD, H9_BONUS_GATE_STEP, 0x0780)

/* @routine 09:AB9B ObjectBonusEndGateStep */
void ObjectBonusEndGateStep(Object *o)
{
    uint16_t r = synced(o) ? next_contact() : 0;

    if (!r) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    g.picture_effects |= 0x0A;
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    SoundPort1QueuePush(0x0F);
    if (g.laps_counted[0] == 0x02 || !g.bonus_markers_taken)
        return;
    GsuRecordCourseFlagsClear(r, 0x01);
}

/* @routine 09:AB56 ObjectBonusEndGate, 09:AB74 ObjectBonusEndGateRecord */
GATE(ObjectBonusEndGate, ObjectBonusEndGateRecord, ObjectBonusEndGateStep, H9_BONUS_END_RECORD, H9_BONUS_END_STEP, 0x0780)

/* with a HUD mode the object goes */
static int released_by_hud(Object *o)
{
    if (!g.hud_mode)
        return 0;
    ObjectRelease(o);
    return 1;
}

/* @routine 09:ABF3 ObjectPopMarker */
void ObjectPopMarker(Object *o)
{
    Object *n;

    if (released_by_hud(o))
        return;
    n = ObjectSpawn(MODEL_POP_MARKER);
    if (n) {
        ObjectCopyPosition(n, o);
        n->heading = o->heading;
        n->y = (uint16_t)(n->y + 0xFE20);
        handler(n, H9_POP_MARKER_STEP);
    }
    handler_none(o);
}

/* the race-time bonus: a BCD add of $19E6 into $0E70 */
static uint16_t bcd_add_bonus(uint16_t v, uint8_t add)
{
    unsigned lo = (v & 0x0Fu) + (add & 0x0Fu), hi = ((v >> 4) & 0x0Fu) + (add >> 4);
    unsigned b1, carry = 0;

    if (lo > 9) {
        lo += 6;
        hi++;
    }
    if (hi > 9) {
        hi += 6;
        carry = 1;
    }
    b1 = (hi & 0x0F) << 4 | (lo & 0x0F);
    {
        unsigned b2 = (v >> 8) & 0xFFu, l2 = (b2 & 0x0Fu) + carry, h2 = b2 >> 4;

        if (l2 > 9) {
            l2 += 6;
            h2++;
        }
        if (h2 > 9)
            h2 += 6;
        return (uint16_t)(b1 | ((h2 & 0x0Fu) << 4 | (l2 & 0x0Fu)) << 8);
    }
}

/* @routine 09:AD07 ObjectTimeGateStep: a hit on a lap it has not counted
   counts it, sound $41, adds the bonus to the race time */
void ObjectTimeGateStep(Object *o)
{
    if (!hit(o) || g.laps_counted[0] == o->lap_count.lap)
        return;
    o->lap_count.lap = g.laps_counted[0];
    SoundPort0QueuePush(0x41);
    g.time_left = bcd_add_bonus(g.time_left, g.time_gate_bonus);
}

static void time_gate(Object *o, uint16_t record_handler)
{
    if (released_by_hud(o))
        return;
    handlers2(o, record_handler, H9_TIME_GATE_STEP);
    o->lap_count.lap = 0x04;
}

/* @routine 09:AC4F ObjectTimeGate */
void ObjectTimeGate(Object *o) { time_gate(o, H9_TIME_GATE_RECORD); }
/* @routine 09:AC83 ObjectTimeGateRecord */
void ObjectTimeGateRecord(Object *o) { gate_trigger(o, 0x0780); }
/* @routine 09:ACAB ObjectTimeGateSmall */
void ObjectTimeGateSmall(Object *o) { time_gate(o, H9_TIME_GATE_SMALL_RECORD); }
/* @routine 09:ACDF ObjectTimeGateSmallRecord */
void ObjectTimeGateSmallRecord(Object *o) { gate_trigger(o, 0x03C0); }

/* @routine 09:AD50 ObjectPopMarkerStep */
void ObjectPopMarkerStep(Object *o)
{
    o->pitch = (uint8_t)(o->pitch + 0xF8);
    ObjectRebucket(o);
}

static int lap_matches_0a(const Object *o) { return g.laps_counted[0] == o->lap_gate.lap; }

static void lap_take_0a(Object *o)
{
    o->lap_gate.lap = g.laps_counted[0];
    o->model = MODEL_NONE;
}

/* a spark of `model` at the player's place following it */
static void spark_follow(uint16_t model)
{
    Object *n = ObjectSpawn(model);
    ObjRef p = g.player_car;

    if (n) {
        n->follow_spark.life = 0x0A;
        ObjectCopyPosition(n, obj_at(p));
        n->follow_spark.rise = 0;
        n->follow_spark.followed = p;
        handler(n, H9_BALL_SPARK);
    }
}

static void lap_ball_record(Object *o, uint16_t model)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x14);
    o->model = lap_matches_0a(o) ? MODEL_NONE : model;
}

/* @routine 09:ADB6 ObjectLapBallUpStep */
void ObjectLapBallUpStep(Object *o)
{
    uint16_t r;

    o->heading = (uint8_t)(o->heading + 0x10);
    o->pitch = (uint8_t)(o->pitch + 0x10);
    if (!synced(o))
        return;
    for (;;) {
        r = RecordNextContact();
        if (!r)
            return;
        if ((record_contacts.summary & 0x0B) == 0x01 || (record_contacts.summary & 0x0B) == 0x02)
            break;
    }
    if (lap_matches_0a(o))
        return;
    lap_take_0a(o);
    GsuRecordBoostRaise(r, 0x40);
    SoundPort0QueuePush(0x25);
    spark_follow(MODEL_RAISE_BALL);
}

/* @routine 09:AD81 ObjectLapBallUpRecord */
void ObjectLapBallUpRecord(Object *o)
{
    lap_ball_record(o, MODEL_RAISE_BALL);
    ObjectLapBallUpStep(o);
}

/* @routine 09:AD5E ObjectLapBallUp */
void ObjectLapBallUp(Object *o)
{
    handlers2(o, H9_LAP_BALL_UP_RECORD, H9_LAP_BALL_UP_STEP);
    o->lap_gate.lap = 0x04;
    ObjectLapBallUpRecord(o);
}

/* @routine 09:AF17 ObjectLapBallDownStep */
void ObjectLapBallDownStep(Object *o)
{
    uint16_t r;

    o->heading = (uint8_t)(o->heading + 0x10);
    o->pitch = (uint8_t)(o->pitch + 0x10);
    if (!synced(o) || (r = next_contact()) == 0 || lap_matches_0a(o))
        return;
    lap_take_0a(o);
    GsuRecordRepair(r, 0x40);
    SoundPort0QueuePush(0x1B);
    spark_follow(MODEL_LOWER_BALL);
}

/* @routine 09:AEE2 ObjectLapBallDownRecord */
void ObjectLapBallDownRecord(Object *o)
{
    lap_ball_record(o, MODEL_LOWER_BALL);
    ObjectLapBallDownStep(o);
}

/* @routine 09:AEBF ObjectLapBallDown */
void ObjectLapBallDown(Object *o)
{
    handlers2(o, H9_LAP_BALL_DOWN_RECORD, H9_LAP_BALL_DOWN_STEP);
    o->lap_gate.lap = 0x04;
    ObjectLapBallDownRecord(o);
}

/* @routine 09:AFFC ObjectCountGateStep */
void ObjectCountGateStep(Object *o)
{
    if (!hit(o) || (o->gate.state & GATE_DONE))
        return;
    o->gate.state |= GATE_DONE;
    HitsToScoreAdd();
    g.stunt_finished = 0x08;
}

/* @routine 09:AFB7 ObjectCountGate, 09:AFD5 ObjectCountGateRecord */
GATE(ObjectCountGate, ObjectCountGateRecord, ObjectCountGateStep, H9_COUNT_GATE_RECORD, H9_COUNT_GATE_STEP, 0x0780)

/* @routine 09:B092 ObjectCountMarkerStep */
void ObjectCountMarkerStep(Object *o)
{
    if (g.switches_hit[0])
        ObjectFrameCountTo8(o);
    ObjectSyncFromRecord(o);
}

/* @routine 09:B065 ObjectCountMarkerRecord */
void ObjectCountMarkerRecord(Object *o)
{
    GsuRecordNewSolidTrigger(o);
    GsuRecordTouchesAll(o);
    GsuRecordSetMass(o, 0x14);
    GsuRecordPosition(o, 0x0500, 0x0500, 0x0500);
    ObjectCountMarkerStep(o);
}

/* @routine 09:B02A ObjectCountMarker */
void ObjectCountMarker(Object *o)
{
    handlers2(o, H9_COUNT_MARKER_RECORD, H9_COUNT_MARKER_STEP);
    o->script = OBJECT_SCRIPT_MARKER;
    o->frame5a = 0x80;
    o->meter.frame = 0x04;
    ObjectCountMarkerRecord(o);
}

/* @routine 09:B103 ObjectCountTriggerStep */
void ObjectCountTriggerStep(Object *o)
{
    uint16_t r;

    if (!synced(o))
        return;
    for (;;) {
        r = RecordNextContact();
        if (!r)
            return;
        if (record_contacts.summary & 0x01)
            break;
    }
    if (!(o->gate.state & GATE_DONE)) {
        o->gate.state |= GATE_DONE;
        HitsCollect(r);
    }
    g.switches_hit[0] = 0x01;
}

/* @routine 09:B0DC ObjectCountTriggerRecord */
void ObjectCountTriggerRecord(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordPosition(o, 0x03C0, 0x03C0, 0x03C0);
    ObjectCountTriggerStep(o);
}

/* @routine 09:B0BE ObjectCountTrigger */
void ObjectCountTrigger(Object *o)
{
    handlers2(o, H9_COUNT_TRIGGER_RECORD, H9_COUNT_TRIGGER_STEP);
    ObjectCountTriggerRecord(o);
}

/*
 * @routine 09:AE60 ObjectSparkFollow
 * WHAT IT DOES: A following spark's frame: at its object's place, released
 * when its life runs out; rises by a speed that grows each frame and grows.
 */
void ObjectSparkFollow(Object *o)
{
    ObjectCopyPosition(o, obj_at(o->follow_spark.followed));
    if (--o->follow_spark.life == 0)
        ObjectRelease(o);
    o->y = (uint16_t)(o->y + 0xFF40);
    o->follow_spark.rise = (uint16_t)(o->follow_spark.rise + 0xFFD8);
    o->y = (uint16_t)(o->y + o->follow_spark.rise);
    o->heading = (uint8_t)(o->heading + 0x10);
    o->pitch = (uint8_t)(o->pitch + 0x10);
    ObjectRebucket(o);
}

/* @routine 09:C438 ObjectCountUpTo2: +$48 up to at most 2, shown at +$5A */
static void count_up_to_2(Object *o)
{
    if (o->lap_light.frame != 0x02)
        o->lap_light.frame++;
    o->frame5a = (uint8_t)(o->lap_light.frame | 0x80);
}

static void life_down(Object *o)
{
    if (--o->piece.life == 0)
        ObjectRelease(o);
}

/* @routine 09:C4E1 ObjectBreakablePieceStep */
void ObjectBreakablePieceStep(Object *o)
{
    ObjectSyncFromRecord(o);
    life_down(o);
    o->pitch = (uint8_t)(o->pitch + 0x18);
    o->heading = (uint8_t)(o->heading + 0x18);
    o->roll = (uint8_t)(o->roll + 0x18);
}

/* @routine 09:C4C0 ObjectBreakablePiece */
void ObjectBreakablePiece(Object *o)
{
    handler(o, H9_BREAKABLE_PIECE_STEP);
    if (!GsuRecordNewBouncing(o)) {
        ObjectRelease(o);
        return;
    }
    GsuRecordVelocity(o);
    GsuRecordSetMass(o, 0x00);
    ObjectBreakablePieceStep(o);
}

static void burst_piece(Object *o, uint16_t kind, uint16_t dy, uint16_t vx, uint16_t vy, uint16_t vz)
{
    Object *n = ObjectSpawn(kind);

    if (!n)
        return;
    ObjectCopyPosition(n, o);
    handler(n, H9_BREAKABLE_PIECE);
    n->y = (uint16_t)(n->y + dy);
    n->vx = vx;
    n->vy = vy;
    n->vz = vz;
    n->piece.life = 0x0F;
}

/* @routine 09:D79C ObjectBreakableBurst: eight pieces */
static void breakable_burst(Object *o)
{
    static const uint16_t v[8][3] = {
        {0x01B3, 0xFFFB, 0x0302}, {0x0186, 0xFFF9, 0x02B2}, {0xFEA2, 0xFFF6, 0x0000},
        {0xFE5C, 0xFFF6, 0x0316}, {0x0258, 0xFFF6, 0x0212}, {0xFED9, 0xFFEC, 0x019F},
        {0xFE5C, 0xFFFB, 0x0000}, {0xFEFC, 0xFFF6, 0xFDBC},
    };
    int i;

    for (i = 0; i < 8; i++)
        burst_piece(o, MODEL_BREAKABLE_PIECE, 0x0000, v[i][0], v[i][1], v[i][2]);
}

/* @routine 09:C560 ObjectBreakableStep: it grows by $0C, the three words
   overlapping; a car's hit bursts it */
void ObjectBreakableStep(Object *o)
{
    life_down(o);
    add_word_at(&o->pitch, 0x000C);
    add_word_at(&o->heading, 0x000C);
    add_word_at(&o->roll, 0x000C);
    if (!hit(o))
        return;
    ObjectRelease(o);
    SoundPort0QueuePush(0x5A);
    breakable_burst(o);
}

/* @routine 09:C531 ObjectBreakableRecord */
void ObjectBreakableRecord(Object *o)
{
    if (!GsuRecordNewBouncing(o)) {
        ObjectRelease(o);
        return;
    }
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x46);
    GsuRecordPosition(o, 0x00C0, 0x00C0, 0x00C0);
    ObjectBreakableStep(o);
}

/* @routine 09:C513 ObjectBreakable */
void ObjectBreakable(Object *o)
{
    handlers2(o, H9_BREAKABLE_RECORD, H9_BREAKABLE_STEP);
    ObjectBreakableRecord(o);
}

/* @routine 09:C5C8 ObjectColour2, 09:C5D1 ObjectColour3, 09:C5DA ObjectColour4,
   09:C5E3 ObjectColour5, 09:C5EC ObjectColour6, 09:C5F5 ObjectColour7 */
#define COLOUR(n) void ObjectColour##n(Object *o) { o->sprite_turn[0] = (uint8_t)(n | 0x80); }
COLOUR(2) COLOUR(3) COLOUR(4) COLOUR(5) COLOUR(6) COLOUR(7)

/* @routine 09:C5FE ObjectScaleE0, 09:C605 ObjectScaleA0, 09:C60C ObjectScale20: the headings $E0, $A0 and $20 */
void ObjectHeading315(Object *o) { o->heading = 0xE0; }
void ObjectHeading225(Object *o) { o->heading = 0xA0; }
void ObjectHeading45(Object *o) { o->heading = 0x20; }

/* @routine 09:C62D ObjectSwingStep: it moves to one side with its heading
   eased, and every $50 frames turns to the other */
void ObjectSwingStep(Object *o)
{
    int moved;

    if (o->sway.state & SWAY_BACK) {
        o->x = (uint16_t)(o->x + 0x0028);
        o->heading = ApproachEighthMin8Byte(o->heading, 0xC0, &moved);
    } else {
        o->x = (uint16_t)(o->x + 0xFFD8);
        o->heading = ApproachEighthMin8Byte(o->heading, 0x40, &moved);
    }
    if (o->sway.wait) {
        o->sway.wait--;
    } else {
        o->sway.state ^= SWAY_BACK;
        o->sway.wait = 0x50;
    }
    ObjectRebucket(o);
}

/* @routine 09:C613 ObjectSwing */
void ObjectSwing(Object *o)
{
    o->heading = 0x00;
    o->sway.wait = 0x28;
    handler(o, H9_SWING_STEP);
    ObjectSwingStep(o);
}

/* A lap's crowd: at the gate moved, sound $52 */
static void crowd_spawn(Object *o, uint16_t dx, uint16_t dy, uint16_t dz, uint8_t heading, uint8_t life, uint16_t h)
{
    Object *n = ObjectSpawn(MODEL_CROWD);

    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->x = (uint16_t)(n->x + dx);
    n->y = (uint16_t)(n->y + dy);
    n->z = (uint16_t)(n->z + dz);
    n->heading = heading;
    n->pitch = 0xF8;
    n->piece.life = life;
    handler(n, h);
    SoundPort0QueuePush(0x52);
}

/* @routine 09:C7C9 LapCrowdStep0 (with 09:C7D2 LapCrowdMove0) */
void LapCrowdStep0(Object *o)
{
    o->x = (uint16_t)(o->x + 0xFF4C);
    o->y = (uint16_t)(o->y + 0xFFF6);
    life_down(o);
    ObjectRebucket(o);
}

/* @routine 09:C7FC LapCrowdStep1 (with 09:C805 LapCrowdMove1) */
void LapCrowdStep1(Object *o)
{
    o->x = (uint16_t)(o->x + 0x0104);
    o->y = (uint16_t)(o->y + 0xFFF6);
    o->z = (uint16_t)(o->z + 0x00C8);
    life_down(o);
    ObjectRebucket(o);
}

/* @routine 09:C83D LapCrowdStep2 (with 09:C846 LapCrowdMove2) */
void LapCrowdStep2(Object *o)
{
    o->y = (uint16_t)(o->y + 0xFFF3);
    o->z = (uint16_t)(o->z + 0x00C8);
    life_down(o);
    ObjectRebucket(o);
}

/* @routine 09:C8BA ObjectLapCrowdGateStep: each lap's first hit brings out
   that lap's crowd (09:C694, C6FB, C762 LapCrowdSpawn0-2) */
void ObjectLapCrowdGateStep(Object *o)
{
    uint8_t lap = g.laps_counted[0];

    if (!ObjectSyncToRecord(o) || !(RecordContactSummary(o) & 0x01) || lap == o->lap_count.lap)
        return;
    o->lap_count.lap = lap;
    if (lap == 0x00)
        crowd_spawn(o, 0x0F00, 0xFE40, 0xD800, 0x40, 0x1C, H9_CROWD_STEP_0);
    else if (lap == 0x01)
        crowd_spawn(o, 0xF600, 0xFE20, 0xD440, 0xE0, 0x16, H9_CROWD_STEP_1);
    else if (lap == 0x02)
        crowd_spawn(o, 0x03C0, 0xFE20, 0xD080, 0x00, 0x16, H9_CROWD_STEP_2);
}

/* @routine 09:C893 ObjectLapCrowdGateRecord */
void ObjectLapCrowdGateRecord(Object *o)
{
    gate_trigger(o, 0x0780);
    ObjectLapCrowdGateStep(o);
}

/* @routine 09:C870 ObjectLapCrowdGate */
void ObjectLapCrowdGate(Object *o)
{
    handlers2(o, H9_LAP_CROWD_RECORD, H9_LAP_CROWD_STEP);
    o->lap_count.lap = 0x04;
    ObjectLapCrowdGateRecord(o);
}

/* @routine 09:C956 ObjectFloaterSpawnerStep */
void ObjectFloaterSpawnerStep(Object *o) { o->heading = 0x40; }

/* @routine 09:C90C ObjectFloaterSpawner */
void ObjectFloaterSpawner(Object *o)
{
    Object *n;

    handler(o, H9_FLOATER_SPAWNER_STEP);
    n = ObjectSpawn(MODEL_FLOATER);
    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->y = (uint16_t)(n->y + 0x06E0);
    n->sway.wait = 0x28;
    handler(n, H9_FLOATER_STEP);
}

/* @routine 09:C95C ObjectFloaterStep */
void ObjectFloaterStep(Object *o)
{
    ObjectRebucket(o);
    o->heading = (uint8_t)(o->heading + 0x08);
    o->y = (uint16_t)(o->y + ((o->sway.state & SWAY_BACK) ? 0xFFEC : 0x0014));
    if (o->sway.wait) {
        o->sway.wait--;
        return;
    }
    o->sway.state ^= SWAY_BACK;
    o->sway.wait = 0x28;
}

/* The orbit: the vector turned by the centre's angles (roll only without
   `all_three`), times 32 from the centre's place. Returns the centre. */
static const Object *orbit(Object *o, int8_t x, int8_t y, int8_t z, int all_three)
{
    const Object *c = obj_at(o->orbiter.centre);
    Vec16 v;

    if (all_three) {
        v = ObjectTurnVector(c, x, y, z);
    } else {
        v.z = z;
        RotateXY(x, y, c->roll, &v.x, &v.y);
    }
    o->x = (uint16_t)(c->x + (uint16_t)(v.x << 5));
    o->z = (uint16_t)(c->z + (uint16_t)(v.z << 5));
    o->y = (uint16_t)(c->y + (uint16_t)(v.y << 5));
    return c;
}

/* An orbiter's set-up: a centre of kind $A040 at its place */
static void orbiter(Object *o, uint16_t centre_handler, uint16_t frame)
{
    Object *c = ObjectSpawn(MODEL_NONE);

    if (c) {
        ObjectCopyPosition(c, o);
        o->orbiter.centre = obj_ref(c);
        handler(c, centre_handler);
    }
    handler(o, frame);
}

/* the colour +$5A stepped on through 0 to n-1, bit 7 kept: its value and
   whether it came to n */
static uint8_t colour_next(const Object *o, uint8_t n, int *reached)
{
    uint8_t v = (uint8_t)(o->frame5a + 1);

    if (!(v & 0x80))
        v = (uint8_t)(v + n);
    v &= 0x7F;
    *reached = v >= n;
    return v;
}

static void colour_cycle(Object *o, uint8_t n)
{
    int reached;
    uint8_t v = colour_next(o, n, &reached);

    if (reached)
        v = (uint8_t)(v - n);
    o->frame5a = (uint8_t)(v | 0x80);
}

/* @routine 09:C9F0 ObjectRingStep */
void ObjectRingStep(Object *o)
{
    o->heading = orbit(o, 0x64, 0x00, 0x00, 1)->heading;
    ObjectRebucket(o);
}

/* @routine 09:C9B0 ObjectRing */
void ObjectRing(Object *o)
{
    orbiter(o, H9_RING_CENTRE_STEP, H9_RING_STEP);
    ObjectRingStep(o);
}

/* @routine 09:CAA3 ObjectRingCentreStep */
void ObjectRingCentreStep(Object *o)
{
    o->heading = (uint8_t)(o->heading + 0x01);
    ObjectRebucket(o);
}

/* @routine 09:CAF8 ObjectSpinnerStep */
void ObjectSpinnerStep(Object *o)
{
    o->heading = orbit(o, 0x28, 0x0A, 0x00, 1)->heading;
    colour_cycle(o, 0x06);
    ObjectRebucket(o);
}

/* @routine 09:CAB1 ObjectSpinner */
void ObjectSpinner(Object *o)
{
    o->frame5a = 0x80;
    orbiter(o, H9_SPINNER_CENTRE_STEP, H9_SPINNER_STEP);
    ObjectSpinnerStep(o);
}

/* @routine 09:CBC4 ObjectSpinnerCentreStep */
void ObjectSpinnerCentreStep(Object *o)
{
    o->heading = (uint8_t)(o->heading + 0xF8);
    o->pitch = (uint8_t)(o->pitch + 0x0C);
    ObjectRebucket(o);
}

/* @routine 09:CC20 ObjectBellGateStep */
void ObjectBellGateStep(Object *o)
{
    if (!hit(o)) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    SoundPort0QueuePush(0x2E);
}

/* @routine 09:CBDB ObjectBellGate, 09:CBF9 ObjectBellGateRecord */
GATE(ObjectBellGate, ObjectBellGateRecord, ObjectBellGateStep, H9_BELL_RECORD, H9_BELL_STEP, 0x0F00)

/* A fan or wheel: circles its centre taking the centre's roll; over part
   of the turn it shows the lit model, with `sound` the first time */
static void fan_frame(Object *o, int all_three, uint16_t lit, uint8_t sound)
{
    uint8_t roll;

    o->roll = orbit(o, 0x64, 0x46, 0x00, all_three)->roll;
    roll = o->roll;
    if (roll == 0x3A || ((uint8_t)(roll - 0x3A) & 0x80) || !(roll == 0xC0 || ((uint8_t)(roll - 0xC0) & 0x80))) {
        o->model = MODEL_NONE;
        o->orbiter.state &= (uint8_t)~ORBITER_LIT;
    } else {
        o->model = lit;
        if (!(o->orbiter.state & ORBITER_LIT)) {
            o->orbiter.state |= ORBITER_LIT;
            if (sound && !ObjectKept(o))    /* not when kept running far (animate.c) */
                SoundPort0QueuePush(sound);
        }
    }
    o->roll = (uint8_t)(o->roll + 0xAA);
    ObjectRebucket(o);
}

/* @routine 09:CEE8 ObjectFanStep */
void ObjectFanStep(Object *o) { fan_frame(o, 0, MODEL_FAN_LIT, 0x00); }

/* @routine 09:CEA8 ObjectFan */
void ObjectFan(Object *o)
{
    orbiter(o, H9_FAN_CENTRE_STEP, H9_FAN_STEP);
    ObjectFanStep(o);
}

/* @routine 09:CFD5 ObjectFanCentreStep */
void ObjectFanCentreStep(Object *o)
{
    o->roll = (uint8_t)(o->roll + 0xF8);
    ObjectRebucket(o);
}

/* @routine 09:D023 ObjectWheelStep */
void ObjectWheelStep(Object *o) { fan_frame(o, 1, MODEL_WHEEL_LIT, 0x38); }

/* @routine 09:CFE3 ObjectWheel */
void ObjectWheel(Object *o)
{
    orbiter(o, H9_WHEEL_CENTRE_STEP, H9_WHEEL_STEP);
    ObjectWheelStep(o);
}

/* @routine 09:D131 ObjectWheelCentreStep */
void ObjectWheelCentreStep(Object *o)
{
    o->roll = (uint8_t)(o->roll + 0x08);
    ObjectRebucket(o);
}

/* The rails take their first colour from work RAM $00: the race step's
   flag (2 while a step runs). */
static uint8_t rail_colour(void) { return (uint8_t)(g.frame_stage_00 | 0x80); }

/* The rail's slide: its Z stepped toward the target +$27; 1 when already
   there. */
static int rail_slide(Object *o, uint16_t step)
{
    if (o->z == o->rail.target_z)
        return 1;
    o->z = (uint16_t)(((uint16_t)(o->z - o->rail.target_z) & 0x8000) ? o->z + step : o->z - step);
    ObjectSyncToRecord(o);
    return 0;
}

/* @routine 09:D3AF RailStep, 09:D3FC RailStop */
void RailStep(Object *o)
{
    colour_cycle(o, 0x06);
    if (rail_slide(o, 0x0280))
        handler(o, H9_RAIL_DROP);
}

/* @routine 09:D498 RailRiseStep */
void RailRiseStep(Object *o)
{
    colour_cycle(o, 0x06);
    if (rail_slide(o, 0x0140))
        ObjectRelease(o);
}

/* @routine 09:D460 RailRise */
void RailRise(Object *o)
{
    o->rail.target_z = (uint16_t)(o->z + 0x0500);
    o->frame5a = rail_colour();
    o->model = MODEL_RAIL_RISING;
    handler(o, H9_RAIL_RISE_STEP);
    RailRiseStep(o);
}

/* @routine 09:D42C RailDropStep */
void RailDropStep(Object *o)
{
    int reached;
    uint8_t v = colour_next(o, 0x07, &reached);

    if (reached) {
        RailRise(o);
        return;
    }
    o->frame5a = (uint8_t)(v | 0x80);
    o->z = (uint16_t)(o->z + 0x0040);
    ObjectSyncToRecord(o);
}

/* @routine 09:D40C RailDrop */
void RailDrop(Object *o)
{
    o->frame5a = rail_colour();
    o->model = MODEL_RAIL_DROPPING;
    handler(o, H9_RAIL_DROP_STEP);
    RailDropStep(o);
}

static void rail(Object *o, uint16_t dx, uint16_t dz, uint16_t travel)
{
    Object *n = ObjectSpawn(MODEL_RAIL_RISING);

    if (!n)
        return;
    n->frame5a = rail_colour();
    n->heading = 0x80;
    ObjectCopyPosition(n, o);
    n->x = (uint16_t)(n->x + dx);
    n->z = (uint16_t)(n->z + dz);
    n->rail.target_z = (uint16_t)(n->z + travel);
    handler(n, H9_RAIL_STEP);
}

/* @routine 09:D184 ObjectFinaleGateStep (with 09:D1AB RailsSpawn) */
void ObjectFinaleGateStep(Object *o)
{
    if (!hit(o) || g.laps_counted[0] != 0x02)
        return;
    rail(o, 0x0000, 0x0000, 0x1180);
    rail(o, 0xFD80, 0xFD80, 0x1400);
    rail(o, 0xFB00, 0x0000, 0x1180);
    rail(o, 0xF9C0, 0xFB00, 0x1680);
    rail(o, 0xF740, 0x0000, 0x1180);
    SoundPort0QueuePush(0x33);
    ObjectRelease(o);
}

/* @routine 09:D13F ObjectFinaleGate, 09:D15D ObjectFinaleGateRecord */
GATE(ObjectFinaleGate, ObjectFinaleGateRecord, ObjectFinaleGateStep, H9_FINALE_RECORD, H9_FINALE_STEP, 0x2A80)

/* A breakable at the gate's place moved; the gate's own angles set */
static void breakable_put(Object *o, uint16_t dy, uint16_t dz, uint16_t dx, uint8_t size)
{
    Object *n = ObjectSpawn(MODEL_FINALE_BREAKABLE);

    if (!n)
        return;
    ObjectCopyPosition(n, o);
    handler(n, H9_BREAKABLE);
    n->y = (uint16_t)(n->y + dy);
    n->z = (uint16_t)(n->z + dz);
    n->x = (uint16_t)(n->x + dx);
    n->vx = 0;
    n->vy = 0;
    n->vz = 0;
    o->pitch = size;
    o->heading = size;
    o->roll = size;
    n->piece.life = 0x1C;
}

/* the lap a gate has not counted yet (+$0A): counts it and returns 1 */
static int new_lap_0a(Object *o)
{
    if (!hit(o) || g.laps_counted[0] == o->lap_gate.lap)
        return 0;
    o->lap_gate.lap = g.laps_counted[0];
    return 1;
}

/* @routine 09:D53E ObjectBreakableGateStep (with 09:D58E BreakablePairA) */
void ObjectBreakableGateStep(Object *o)
{
    if (!new_lap_0a(o) || g.laps_counted[0] != 0x02)
        return;
    breakable_put(o, 0xFCE0, 0xF980, 0x0B40, 0x5A);
    breakable_put(o, 0xFC40, 0xF920, 0x0F00, 0x23);
    SoundPort0QueuePush(0x53);
}

/* @routine 09:D517 ObjectBreakableGateRecord */
void ObjectBreakableGateRecord(Object *o)
{
    gate_trigger(o, 0x1E00);
    ObjectBreakableGateStep(o);
}

/* @routine 09:D4F4 ObjectBreakableGate */
void ObjectBreakableGate(Object *o)
{
    handlers2(o, H9_BREAKABLE_GATE_RECORD, H9_BREAKABLE_GATE_STEP);
    o->lap_gate.lap = 0x04;
    ObjectBreakableGateRecord(o);
}

/* @routine 09:DD68 ObjectBreakableGate2Step (with 09:DDDE BreakablePairC,
   09:DEE5 BreakablePairD) */
void ObjectBreakableGate2Step(Object *o)
{
    if (!new_lap_0a(o))
        return;
    if (g.laps_counted[0] == 0x00) {
        GateFlashOnClock();
    } else if (g.laps_counted[0] == 0x01) {
        breakable_put(o, 0xFB00, 0xF880, 0xEE80, 0x5A);
        breakable_put(o, 0xFA60, 0xF920, 0xF100, 0x23);
        SoundPort0QueuePush(0x53);
    } else if (g.laps_counted[0] == 0x02) {
        breakable_put(o, 0xFB00, 0xFB40, 0xEE80, 0x5A);
        breakable_put(o, 0xFA60, 0xFA60, 0xF100, 0x23);
        SoundPort0QueuePush(0x53);
    }
}

/* @routine 09:DD41 ObjectBreakableGate2Record */
void ObjectBreakableGate2Record(Object *o)
{
    gate_trigger(o, 0x0780);
    ObjectBreakableGate2Step(o);
}

/* @routine 09:DD1E ObjectBreakableGate2 */
void ObjectBreakableGate2(Object *o)
{
    handlers2(o, H9_BREAKABLE_GATE2_RECORD, H9_BREAKABLE_GATE2_STEP);
    o->lap_gate.lap = 0x04;
    ObjectBreakableGate2Record(o);
}

/* @routine 09:E018 ObjectDuoGateStep: each player's first touch makes
   sound $5A as that player */
void ObjectDuoGateStep(Object *o)
{
    if (!synced(o)) {
        o->gate.state &= (uint8_t)~(DUO_FIRST | DUO_SECOND);
        return;
    }
    if (!(RecordContactSummary(o) & 0x01)) {
        o->gate.state &= (uint8_t)~DUO_FIRST;
    } else if (!(o->gate.state & DUO_FIRST)) {
        o->gate.state |= DUO_FIRST;
        g.current_player = 0x00;
        SoundPort0QueuePush(0x5A);
    }
    if (!(RecordContactSummary(o) & 0x02)) {
        o->gate.state &= (uint8_t)~DUO_SECOND;
        return;
    }
    if (o->gate.state & DUO_SECOND)
        return;
    o->gate.state |= DUO_SECOND;
    g.current_player = 0x01;
    SoundPort0QueuePush(0x5A);
}

static void record424alt(Object *o, uint8_t high)
{
    GsuRecordNewSolidTrigger(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, high);
}

/* @routine 09:E00A ObjectDuoGateRecord */
void ObjectDuoGateRecord(Object *o)
{
    record424alt(o, 0x14);
    ObjectDuoGateStep(o);
}

/* @routine 09:DFEC ObjectDuoGate */
void ObjectDuoGate(Object *o)
{
    handlers2(o, H9_DUO_RECORD, H9_DUO_STEP);
    ObjectDuoGateRecord(o);
}

/* @routine 09:E0C8 ObjectBurstGateStep (with 09:DA5D ObjectBreakableBurst2) */
void ObjectBurstGateStep(Object *o)
{
    static const uint16_t p[8][4] = {
        {MODEL_BREAKABLE_PIECE_2, 0x01B3, 0xFFFB, 0x0302}, {MODEL_BREAKABLE_PIECE, 0x0186, 0xFFF9, 0x02B2},
        {MODEL_BREAKABLE_PIECE_2, 0xFEA2, 0xFFF6, 0x0000}, {MODEL_BREAKABLE_PIECE, 0xFE5C, 0xFFF6, 0x0316},
        {MODEL_BREAKABLE_PIECE_2, 0x0258, 0xFFF6, 0x0212}, {MODEL_BREAKABLE_PIECE, 0xFED9, 0xFFEC, 0x019F},
        {MODEL_BREAKABLE_PIECE_2, 0xFE5C, 0xFFFB, 0x0000}, {MODEL_BREAKABLE_PIECE_2, 0xFEFC, 0xFFF6, 0xFDBC},
    };
    int i;

    if (!hit(o)) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    for (i = 0; i < 8; i++)
        burst_piece(o, p[i][0], 0x0000, p[i][1], p[i][2], p[i][3]);
    SoundPort0QueuePush(0x5A);
}

/* @routine 09:E0BA ObjectBurstGateRecord */
void ObjectBurstGateRecord(Object *o)
{
    record424alt(o, 0x14);
    ObjectBurstGateStep(o);
}

/* @routine 09:E097 ObjectBurstGate: +$14 is direct page $80's byte, which
   only the power-on clear writes (traced over the all-courses play) */
void ObjectBurstGate(Object *o)
{
    handlers2(o, H9_BURST_GATE_RECORD, H9_BURST_GATE_STEP);
    o->pitch = 0x00;
    ObjectBurstGateRecord(o);
}

/* @routine 09:E0FF ObjectLampPost */
void ObjectLampPost(Object *o)
{
    Object *n;

    o->y = (uint16_t)(o->y + 0xFF74);
    o->anim = LOOK_LAMP_LIGHT;
    lamp(o, 0x64);
    handler(o, H9_LAMP_POST_LIGHT);
    n = ObjectSpawn(MODEL_LAMP_POST_LIGHT);
    if (!n)
        return;
    lamp(n, 0x64);
    n->anim = LOOK_LAMP_LIGHT_RAISED;
    ObjectCopyPosition(n, o);
    n->y = (uint16_t)(n->y + 0xFD94);
    ObjectRebucket(n);
}

/* @routine 09:E1B1 ObjectLampPostStep (with 09:E1CE LampPostHit, 09:E1FF
   LampPostBurst) */
void ObjectLampPostStep(Object *o)
{
    if (!hit(o)) {
        o->gate.state &= (uint8_t)~LAMP_POST_RANG;
        return;
    }
    if (!(o->gate.state & LAMP_POST_BURST)) {
        o->gate.state |= LAMP_POST_BURST;
        burst_piece(o, MODEL_BREAKABLE_PIECE, 0xFFA6, 0x0023, 0xFFFE, 0x000A);
        burst_piece(o, MODEL_BREAKABLE_PIECE, 0xFF60, 0xFFE2, 0xFFFF, 0x000F);
        burst_piece(o, MODEL_BREAKABLE_PIECE, 0xFFA6, 0x001E, 0xFFFE, 0xFFF3);
        burst_piece(o, MODEL_BREAKABLE_PIECE, 0xFF4C, 0xFFDD, 0xFFFF, 0xFFF6);
    }
    if (o->gate.state & LAMP_POST_RANG)
        return;
    o->gate.state |= LAMP_POST_RANG;
    SoundPort0QueuePush(0x58);
}

/* @routine 09:E1A3 ObjectLampPostRecord */
void ObjectLampPostRecord(Object *o)
{
    record424alt(o, 0x08);
    ObjectLampPostStep(o);
}

/* @routine 09:E185 ObjectLampPostLight */
void ObjectLampPostLight(Object *o)
{
    handlers2(o, H9_LAMP_POST_RECORD, H9_LAMP_POST_STEP);
    ObjectLampPostRecord(o);
}

/* @routine 09:E391 ObjectBumpPostStep */
void ObjectBumpPostStep(Object *o)
{
    if (o->bumper.bulge) {
        o->heading = (uint8_t)(o->heading + 0x06);
        o->bumper.bulge--;
    }
    if (!hit(o)) {
        o->bumper.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (o->bumper.state & GATE_DONE)
        return;
    o->bumper.state |= GATE_DONE;
    SoundPort0QueuePush(0x59);
    o->bumper.bulge = 0x06;
}

/* @routine 09:E383 ObjectBumpPostRecord */
void ObjectBumpPostRecord(Object *o)
{
    GsuRecordNewFalling(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x01);
    ObjectBumpPostStep(o);
}

/* @routine 09:E360 ObjectBumpPost */
void ObjectBumpPost(Object *o)
{
    handlers2(o, H9_BUMP_POST_RECORD, H9_BUMP_POST_STEP);
    o->bumper.bulge = 0x00;
    ObjectBumpPostRecord(o);
}

/* @routine 09:E47D GateSlideStep */
static void gate_slide_step(Object *o)
{
    if (!(o->slide_lap.state & SLIDE_LAP_STARTED)) {
        o->slide_lap.state |= SLIDE_LAP_STARTED;
        g.slide_gate_state = 0x01;
        SoundPort0QueuePush(0x5F);
    }
    if (o->x == o->slide_lap.target_x) {
        g.slide_gate_state = 0x02;
        return;
    }
    o->x = (uint16_t)(((uint16_t)(o->x - o->slide_lap.target_x) & 0x8000) ? o->x + 0x0280 : o->x - 0x0280);
}

/* @routine 09:E441 ObjectSlideLapGateStep */
void ObjectSlideLapGateStep(Object *o)
{
    if (!ObjectSyncToRecord(o) || !(RecordContactSummary(o) & 0x01))
        return;
    if (g.laps_counted[0] == 0x01)
        gate_slide_step(o);
    else if (g.laps_counted[0] == 0x02)
        ease8(&o->heading, 0x80, ApproachHalfMin2Byte);
}

/* @routine 09:E41A ObjectSlideLapGateRecord */
void ObjectSlideLapGateRecord(Object *o)
{
    gate_trigger(o, 0x2A80);
    ObjectSlideLapGateStep(o);
}

/* @routine 09:E3E4 ObjectSlideLapGate */
void ObjectSlideLapGate(Object *o)
{
    handlers2(o, H9_SLIDE_LAP_RECORD, H9_SLIDE_LAP_STEP);
    o->slide_lap.target_x = (uint16_t)(o->x + 0xDF80);
    ObjectSlideLapGateRecord(o);
}

/* @routine 09:E4F2 ObjectSlideMeterStep */
void ObjectSlideMeterStep(Object *o)
{
    if (g.slide_gate_state) {
        if (g.slide_gate_state == 0x01) {
            if (o->meter.frame != 0x07)
                o->meter.frame++;
        } else if (o->meter.frame) {
            o->meter.frame--;
        }
        o->frame5a = (uint8_t)(o->meter.frame | 0x80);
    }
    ObjectRebucket(o);
}

/* @routine 09:E4D6 ObjectSlideMeter */
void ObjectSlideMeter(Object *o)
{
    handler(o, H9_SLIDE_METER_STEP);
    o->meter.frame = 0x00;
    o->frame5a = 0x80;
    ObjectSlideMeterStep(o);
}

/* @routine 09:E581 ObjectTrailZoneStep */
void ObjectTrailZoneStep(Object *o) { g.trail_zone_on = hit(o) ? 0x01 : 0x00; }

/* @routine 09:E53C ObjectTrailZone, 09:E55A ObjectTrailZoneRecord */
GATE(ObjectTrailZone, ObjectTrailZoneRecord, ObjectTrailZoneStep, H9_TRAIL_ZONE_RECORD, H9_TRAIL_ZONE_STEP, 0x1680)

/* A trail's frame: in the zone it follows the player */
static void trail_frame(Object *o, uint16_t dz, uint16_t dx, uint16_t drift)
{
    if (g.trail_zone_on) {
        const Object *p = obj_at(g.player_car);
        int moved;

        o->heading = (uint8_t)(p->heading + 0x80);
        o->z = (uint16_t)(ApproachHalfMin2(o->z, p->z, &moved) + dz);
        o->x = (uint16_t)(ApproachQuarterMin4(o->x, p->x, &moved) + dx);
        o->trail.drift = 0x1A;
        /* @routine 09:E59D TrailFlip */
        if (rec_get(p->record, FXREC_SPEED) & 0x8000)
            add_word_at(&o->heading, 0x0080);
        o->roll = 0x00;
    } else if (o->trail.drift) {
        o->trail.drift--;
        o->z = (uint16_t)(o->z + 0xFF00);
        o->x = (uint16_t)(o->x + drift);
    }
    ObjectRebucket(o);
}

/* @routine 09:E5BD ObjectTrailMidStep */
void ObjectTrailMidStep(Object *o) { trail_frame(o, 0xFFFD, 0x0000, 0x0000); }
/* @routine 09:E641 ObjectTrailLeftStep */
void ObjectTrailLeftStep(Object *o) { trail_frame(o, 0x0003, 0xFFE2, 0x0040); }
/* @routine 09:E6E1 ObjectTrailRightStep */
void ObjectTrailRightStep(Object *o) { trail_frame(o, 0x0003, 0x001E, 0xFFC0); }

/* @routine 09:E7C6 ObjectFadeGate1Step */
void ObjectFadeGate1Step(Object *o)
{
    if (!hit(o) || g.laps_counted[0] != 0x01 || (o->gate.state & GATE_DONE))
        return;
    o->gate.state |= GATE_DONE;
    g.picture_effects |= 0x20;
    fade_set(0x0080, 0x2000);
}

/* @routine 09:E781 ObjectFadeGate1, 09:E79F ObjectFadeGate1Record */
GATE(ObjectFadeGate1, ObjectFadeGate1Record, ObjectFadeGate1Step, H9_FADE_1_RECORD, H9_FADE_1_STEP, 0x0780)

/* Player 1 in the zone switches the alternate backdrop; player 2 sets the
   same bit of $0DA7, which nothing reads. */
static void window_gate_frame(Object *o, int on)
{
    if (!synced(o) || !(RecordContactSummary(o) & 0x01))
        return;
    if (on)
        g.picture_effects |= 0x40;
    else
        g.picture_effects &= 0xBF;
}

/* @routine 09:E852 ObjectWindowOnGateStep */
void ObjectWindowOnGateStep(Object *o) { window_gate_frame(o, 1); }
/* @routine 09:E80D ObjectWindowOnGate, 09:E82B ObjectWindowOnGateRecord */
GATE(ObjectWindowOnGate, ObjectWindowOnGateRecord, ObjectWindowOnGateStep, H9_WINDOW_ON_RECORD, H9_WINDOW_ON_STEP, 0x0780)
/* @routine 09:E8C4 ObjectWindowOffGateStep */
void ObjectWindowOffGateStep(Object *o) { window_gate_frame(o, 0); }
/* @routine 09:E87F ObjectWindowOffGate, 09:E89D ObjectWindowOffGateRecord */
GATE(ObjectWindowOffGate, ObjectWindowOffGateRecord, ObjectWindowOffGateStep, H9_WINDOW_OFF_RECORD, H9_WINDOW_OFF_STEP, 0x0780)

/* the first touch makes `sound` on port 1 */
static void beep_frame(Object *o, uint8_t sound)
{
    if (!hit(o)) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    SoundPort1QueuePush(sound);
}

/* @routine 09:E936 ObjectBeepGate11Step */
void ObjectBeepGate11Step(Object *o) { beep_frame(o, 0x11); }
/* @routine 09:E8F1 ObjectBeepGate11, 09:E90F ObjectBeepGate11Record */
GATE(ObjectBeepGate11, ObjectBeepGate11Record, ObjectBeepGate11Step, H9_BEEP_11_RECORD, H9_BEEP_11_STEP, 0x0780)
/* @routine 09:E9AE ObjectBeepGate10Step */
void ObjectBeepGate10Step(Object *o) { beep_frame(o, 0x10); }
/* @routine 09:E969 ObjectBeepGate10, 09:E987 ObjectBeepGate10Record */
GATE(ObjectBeepGate10, ObjectBeepGate10Record, ObjectBeepGate10Step, H9_BEEP_10_RECORD, H9_BEEP_10_STEP, 0x0780)

/* a hit sets picture-effect bits; the first touch makes `sound` on port 1 */
static void slide_bits_frame(Object *o, uint8_t and_mask, uint8_t or_mask, uint8_t sound)
{
    if (!hit(o)) {
        o->gate.state &= (uint8_t)~GATE_DONE;
        return;
    }
    g.picture_effects = (uint8_t)((g.picture_effects & and_mask) | or_mask);
    if (o->gate.state & GATE_DONE)
        return;
    o->gate.state |= GATE_DONE;
    SoundPort1QueuePush(sound);
}

/* @routine 09:EA26 ObjectSlideBitsGateStep */
void ObjectSlideBitsGateStep(Object *o) { slide_bits_frame(o, 0xFF, 0x0A, 0x0F); }
/* @routine 09:E9E1 ObjectSlideBitsGate, 09:E9FF ObjectSlideBitsGateRecord */
GATE(ObjectSlideBitsGate, ObjectSlideBitsGateRecord, ObjectSlideBitsGateStep, H9_SLIDE_BITS_RECORD, H9_SLIDE_BITS_STEP, 0x0780)
/* @routine 09:EAA8 ObjectSlideBitGateStep */
void ObjectSlideBitGateStep(Object *o) { slide_bits_frame(o, 0xF7, 0x04, 0x10); }
/* @routine 09:EA63 ObjectSlideBitGate, 09:EA81 ObjectSlideBitGateRecord */
GATE(ObjectSlideBitGate, ObjectSlideBitGateRecord, ObjectSlideBitGateStep, H9_SLIDE_BIT_RECORD, H9_SLIDE_BIT_STEP, 0x0780)

/* Player 1's touch sets the weather and the picture effects; player 2's
   the weather (with `weather2`) and $0DA7, which nothing reads. */
static void window_set_frame(Object *o, uint8_t weather, uint8_t and_mask, uint8_t or_mask, int weather2)
{
    if (!synced(o))
        return;
    if (RecordContactSummary(o) & 0x01) {
        palettes.weather = weather;
        g.picture_effects = (uint8_t)((g.picture_effects & and_mask) | or_mask);
    }
    if ((RecordContactSummary(o) & 0x02) && weather2)
        palettes.weather = weather;
}

/* @routine 09:EB2C ObjectWindowSetGateStep */
void ObjectWindowSetGateStep(Object *o) { window_set_frame(o, 0x1F, 0x00, 0x44, 1); }
/* @routine 09:EAE7 ObjectWindowSetGate, 09:EB05 ObjectWindowSetGateRecord */
GATE(ObjectWindowSetGate, ObjectWindowSetGateRecord, ObjectWindowSetGateStep, H9_WINDOW_SET_RECORD, H9_WINDOW_SET_STEP, 0x0780)
/* @routine 09:EBAE ObjectWindowResetGateStep */
void ObjectWindowResetGateStep(Object *o) { window_set_frame(o, 0x00, 0x00, 0x0A, 0); }
/* @routine 09:EB69 ObjectWindowResetGate, 09:EB87 ObjectWindowResetGateRecord */
GATE(ObjectWindowResetGate, ObjectWindowResetGateRecord, ObjectWindowResetGateStep, H9_WINDOW_RESET_RECORD, H9_WINDOW_RESET_STEP, 0x0780)

/*
 * @routine 09:EC2A ObjectSlideGateUpStep
 * A car's hit, unless the slide check is on, starts the slide up. Two paths
 * of the original reach code with the wrong register width:
 * - a hit with the fog's colour at $7FFF runs $09:EC71's operand bytes as
 *   other instructions and ends by running off into ROM data at $CA:E7EC.
 *   That is a crash, so here the frame simply ends.
 * - no hit with the slide check on runs $09:ECB9's bytes as others; all
 *   they do is set a bit of direct page $5C, which nothing reads, and go
 *   on to the sound.
 */
void ObjectSlideGateUpStep(Object *o)
{
    if (hit(o)) {
        if (!(g.picture_effects & 0x01)) {
            if (palettes.target == 0x7FFF)
                return;
            palettes.target = 0x0800;
            slide_set(0x0100, 0x0700, 0x0000);
            g.picture_effects |= 0x80;
        }
        if (o->gate.state & CHIME_IN)
            return;
        o->gate.state |= CHIME_IN;
        o->gate.state &= (uint8_t)~CHIME_OUT;
        SoundPort1QueuePush(0x11);
        return;
    }
    if (g.picture_effects & 0x01) {
        SoundPort1QueuePush(0x10);
        return;
    }
    if (palettes.target != 0x7FFF)
        slide_set(0x0100, 0xFF00, 0x8000);
    if (o->gate.state & CHIME_OUT)
        return;
    o->gate.state |= CHIME_OUT;
    o->gate.state &= (uint8_t)~CHIME_IN;
    SoundPort1QueuePush(0x10);
}

/* @routine 09:EBE5 ObjectSlideGateUp, 09:EC03 ObjectSlideGateUpRecord */
GATE(ObjectSlideGateUp, ObjectSlideGateUpRecord, ObjectSlideGateUpStep, H9_SLIDE_GATE_UP_RECORD, H9_SLIDE_GATE_UP_STEP, 0x0780)

/* One shard: at 4 times (x, y, z) turned by the object's angles, z the
   byte at work RAM $00 (the race step's flag) */
static void shard(Object *o, uint16_t kind, uint8_t s48, uint8_t s49, uint8_t s4a, int8_t x, int8_t y)
{
    Object *n = ObjectSpawn(kind);
    Vec16 v;

    if (!n)
        return;
    n->shard.drift_x = s48;
    n->shard.drift_y = s49;
    n->shard.unread = s4a;
    ObjectCopyPosition(n, o);
    ObjectCopyAnglesFrom(n, o);
    v = ObjectTurnVector(o, x, y, (int8_t)g.frame_stage_00);
    n->x = (uint16_t)(o->x + (uint16_t)(v.x * 4));
    n->z = (uint16_t)(o->z + (uint16_t)(v.z * 4));
    n->y = (uint16_t)(o->y + (uint16_t)(v.y * 4));
    n->shard.life = 0x06;
    handler(n, H9_SHARD_STEP);
}

static const uint8_t shatter_args[6][5] = {
    {0xEE, 0xF3, 0xF0, 0xF0, 0xBA},
    {0xEE, 0x0D, 0xF0, 0xE5, 0xD8},
    {0x00, 0x0C, 0xFC, 0x00, 0xEE},
    {0x12, 0x0D, 0x10, 0x1B, 0xD8},
    {0x12, 0xF3, 0x10, 0x10, 0xBA},
    {0x00, 0x0C, 0x04, 0x00, 0x0F},
};

static void shatter(Object *o, const uint16_t kinds[6])
{
    int k;

    for (k = 0; k < 6; k++)
        shard(o, kinds[k], shatter_args[k][0], shatter_args[k][1], shatter_args[k][2],
              (int8_t)shatter_args[k][3], (int8_t)shatter_args[k][4]);
}

/* @routine 09:B30B ObjectShatter */
static void object_shatter(Object *o)
{
    static const uint16_t kinds[6] = {MODEL_SHARDS, MODEL_SHARDS + MODEL_SIZE, MODEL_SHARDS + 2 * MODEL_SIZE,
                                      MODEL_SHARDS + 3 * MODEL_SIZE, MODEL_SHARDS + 4 * MODEL_SIZE, MODEL_SHARDS + 5 * MODEL_SIZE};

    shatter(o, kinds);
}

/* @routine 09:B876 ObjectShatter2 */
static void object_shatter2(Object *o)
{
    static const uint16_t kinds[6] = {MODEL_SHARDS_2, MODEL_SHARDS_2 + MODEL_SIZE, MODEL_SHARDS_2 + 2 * MODEL_SIZE,
                                      MODEL_SHARDS_2 + 3 * MODEL_SIZE, MODEL_SHARDS_2 + 4 * MODEL_SIZE, MODEL_SHARDS_2 + 5 * MODEL_SIZE};

    shatter(o, kinds);
}

/* @routine 09:BDE1 ObjectShardStep: it drifts by its spin vector, turned
   by pitch then heading, times 4 */
void ObjectShardStep(Object *o)
{
    Vec16 v = {(int8_t)o->shard.drift_x, (int8_t)o->shard.drift_y, (int8_t)g.frame_stage_00};

    RotateXZFrom08((int8_t)v.y, (int8_t)v.z, o->pitch, &v.y, &v.z);
    RotateXZ((int8_t)v.x, (int8_t)v.z, o->heading, &v.x, &v.z);
    o->x = (uint16_t)(o->x + (uint16_t)(v.x * 4));
    o->z = (uint16_t)(o->z + (uint16_t)(v.z * 4));
    o->y = (uint16_t)(o->y + (uint16_t)(v.y * 4));
    o->heading = (uint8_t)(o->heading + 0x12);
    life_down(o);
    ObjectRebucket(o);
}

/* @routine 09:C11F MarkerBounceY */
static void marker_bounce_y(Object *o)
{
    o->bonus_marker.bounce = (int16_t)(o->bonus_marker.bounce + o->bonus_marker.bounce_speed);
    if (o->bonus_marker.bounce == 14 || o->bonus_marker.bounce == -14)
        o->bonus_marker.bounce_speed = (int16_t)-o->bonus_marker.bounce_speed;
    o->y = (uint16_t)(o->y + o->bonus_marker.bounce);
    colour_cycle(o, 0x0A);
    o->heading = (uint8_t)(o->heading + 0x08);
}

static void lap_light(Object *o, uint8_t turn)
{
    if (released_by_hud(o))
        return;
    o->frame5a = 0x80;
    o->lap_light.lap = 0x04;
    o->heading = (uint8_t)(o->heading + turn);
    handlers2(o, H9_LAP_LIGHT_RECORD, H9_LAP_LIGHT_STEP);
}

/* @routine 09:C244 ObjectLapLight */
void ObjectLapLight(Object *o) { lap_light(o, 0x00); }
/* @routine 09:C280 ObjectLapLightLeft */
void ObjectLapLightLeft(Object *o) { lap_light(o, 0xE0); }
/* @routine 09:C2C5 ObjectLapLightRight */
void ObjectLapLightRight(Object *o) { lap_light(o, 0x20); }

/* @routine 09:C312 ObjectLapLightStep */
void ObjectLapLightStep(Object *o)
{
    if (g.laps_counted[0] != o->lap_light.lap) {
        o->lap_light.state &= (uint8_t)~LAP_LIGHT_LIT;
        o->frame5a = 0x80;
        o->lap_light.frame = 0x00;
    }
    if (o->lap_light.state & LAP_LIGHT_LIT) {
        ObjectSyncFromRecord(o);
        count_up_to_2(o);
        return;
    }
    if (!hit(o))
        return;
    o->lap_light.lap = g.laps_counted[0];
    SoundPort0QueuePush(0x5C);
    o->lap_light.state |= LAP_LIGHT_LIT;
    ObjectSpawnPuff(obj_at(g.player_car));
}

/* @routine 09:C30A ObjectLapLightRecord */
void ObjectLapLightRecord(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    ObjectLapLightStep(o);
}

/* @routine 09:BE92 ObjectRisingSpark */
void ObjectRisingSpark(Object *o)
{
    ObjectCopyPosition(o, obj_at(o->follow_spark.followed));
    life_down(o);
    o->y = (uint16_t)(o->y + 0xFF80);
    o->follow_spark.rise = (uint16_t)(o->follow_spark.rise + 0xFFD8);
    o->y = (uint16_t)(o->y + o->follow_spark.rise);
    o->heading = (uint8_t)(o->heading + 0x12);
    ObjectRebucket(o);
}

/* A bonus marker: bounces; a car's contact counts, shatters it, puts a
   rising spark at the player and adds `bonus` (BCD) to the hit count */
static void bonus_marker_frame(Object *o, void (*shatter_it)(Object *), uint16_t kind, uint8_t bonus)
{
    Object *n;

    marker_bounce_y(o);
    if (!ObjectSyncToRecord(o) || !next_contact())
        return;
    StuntScoreUp(o);
    shatter_it(o);
    n = ObjectSpawn(kind);
    if (n) {
        n->follow_spark.life = 0x0A;
        ObjectCopyPosition(n, obj_at(g.player_car));
        n->follow_spark.rise = 0x0000;
        n->follow_spark.followed = g.player_car;
        handler(n, H9_RISING_SPARK);
    }
    g.hits_bcd = bcd_add_bonus(g.hits_bcd, bonus);
    ObjectRelease(o);
}

static void bonus_marker_init(Object *o)
{
    o->frame5a = 0x80;
    o->bonus_marker.bounce = 0x0000;
    o->bonus_marker.bounce_speed = 0x0002;
}

/* @routine 09:BF34 ObjectBonusMarker2Step */
void ObjectBonusMarker2Step(Object *o) { bonus_marker_frame(o, object_shatter, MODEL_BONUS_2, 0x02); }

/* @routine 09:BF26 ObjectBonusMarker2Record */
void ObjectBonusMarker2Record(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordSetMass(o, 0x01);
    ObjectBonusMarker2Step(o);
}

/* @routine 09:BEE8 ObjectBonusMarker2 */
void ObjectBonusMarker2(Object *o)
{
    bonus_marker_init(o);
    o->bonus_marker.unread = 0x01;
    handlers2(o, H9_BONUS_MARKER2_RECORD, H9_BONUS_MARKER2_STEP);
    ObjectBonusMarker2Record(o);
}

/* @routine 09:BFF6 ObjectBonusMarker5Step */
void ObjectBonusMarker5Step(Object *o) { bonus_marker_frame(o, object_shatter2, MODEL_BONUS_5, 0x05); }

/* @routine 09:BFEE ObjectBonusMarker5Record */
void ObjectBonusMarker5Record(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    ObjectBonusMarker5Step(o);
}

/* @routine 09:BFB0 ObjectBonusMarker5 */
void ObjectBonusMarker5(Object *o)
{
    bonus_marker_init(o);
    handlers2(o, H9_BONUS_MARKER5_RECORD, H9_BONUS_MARKER5_STEP);
    o->bonus_marker.unread = 0x01;
    ObjectBonusMarker5Record(o);
}

static void hit_lamp(Object *o, uint16_t record_handler, uint16_t step)
{
    handlers2(o, record_handler, step);
    o->script = OBJECT_SCRIPT_MARKER;
    o->frame5a = 0x80;
    o->meter.frame = 0x04;
}

static void hit_lamp_record(Object *o)
{
    GsuRecordNewSolidTrigger(o);
    GsuRecordTouchesAll(o);
    GsuRecordSetMass(o, 0x14);
    GsuRecordPosition(o, 0x0500, 0x0500, 0x0500);
}

static void hit_lamp_frame(Object *o, uint8_t lit)
{
    if (lit)
        ObjectFrameCountTo8(o);
    ObjectSyncFromRecord(o);
}

static void hit_switch_record(Object *o)
{
    GsuRecordNewTrigger(o);
    GsuRecordSolid(o);
    GsuRecordPosition(o, 0x03C0, 0x03C0, 0x03C0);
}

/* returns 1 on a car's contact, having moved the count the first time */
static int hit_switch_frame(Object *o)
{
    uint16_t r;

    if (!synced(o) || (r = next_contact()) == 0)
        return 0;
    if (!(o->gate.state & GATE_DONE)) {
        o->gate.state |= GATE_DONE;
        HitsCollect(r);
    }
    return 1;
}

/* @routine 09:B19B ObjectSwitchLampBStep */
void ObjectSwitchLampBStep(Object *o) { hit_lamp_frame(o, g.switches_hit[1]); }
/* @routine 09:B16E ObjectSwitchLampBRecord */
void ObjectSwitchLampBRecord(Object *o) { hit_lamp_record(o); ObjectSwitchLampBStep(o); }
/* @routine 09:B133 ObjectSwitchLampB */
void ObjectSwitchLampB(Object *o)
{
    hit_lamp(o, H9_SWITCH_LAMP_B_RECORD, H9_SWITCH_LAMP_B_STEP);
    ObjectSwitchLampBRecord(o);
}

/* @routine 09:B1EF ObjectSwitchBStep */
void ObjectSwitchBStep(Object *o)
{
    if (hit_switch_frame(o))
        g.switches_hit[1] = 0x01;
}
/* @routine 09:B1C8 ObjectSwitchBRecord */
void ObjectSwitchBRecord(Object *o) { hit_switch_record(o); ObjectSwitchBStep(o); }
/* @routine 09:B1AA ObjectSwitchB */
void ObjectSwitchB(Object *o)
{
    handlers2(o, H9_SWITCH_B_RECORD, H9_SWITCH_B_STEP);
    ObjectSwitchBRecord(o);
}

/* @routine 09:B287 ObjectSwitchLampCStep */
void ObjectSwitchLampCStep(Object *o) { hit_lamp_frame(o, g.switches_hit[2]); }
/* @routine 09:B25A ObjectSwitchLampCRecord */
void ObjectSwitchLampCRecord(Object *o) { hit_lamp_record(o); ObjectSwitchLampCStep(o); }
/* @routine 09:B21F ObjectSwitchLampC */
void ObjectSwitchLampC(Object *o)
{
    hit_lamp(o, H9_SWITCH_LAMP_C_RECORD, H9_SWITCH_LAMP_C_STEP);
    ObjectSwitchLampCRecord(o);
}

/* @routine 09:B2DB ObjectSwitchCStep */
void ObjectSwitchCStep(Object *o)
{
    if (hit_switch_frame(o))
        g.switches_hit[2] = 0x01;
}
/* @routine 09:B2B4 ObjectSwitchCRecord */
void ObjectSwitchCRecord(Object *o) { hit_switch_record(o); ObjectSwitchCStep(o); }
/* @routine 09:B296 ObjectSwitchC */
void ObjectSwitchC(Object *o)
{
    handlers2(o, H9_SWITCH_C_RECORD, H9_SWITCH_C_STEP);
    ObjectSwitchCRecord(o);
}
