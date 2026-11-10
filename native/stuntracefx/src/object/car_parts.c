/*
 * car_parts.c: the cars' loose parts and the player cars: a part's place on
 * its car from the part's offset bytes turned by the car's angles, the part
 * knocked loose and flying, falling back into place, and the two player
 * cars' set-ups and frames; with the car set-ups that put a car of a kind
 * on the object list.
 *
 * Not converted, nothing in the game reaches it: ObjectRivalCarB (08:B0B1).
 */
#include "audio/sound.h"
#include "fx/fx.h"
#include "math/math.h"
#include "object/object.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"

/* the life byte of a flying part, in the low byte of +$27 */
static uint8_t part_life(const Object *o) { return o->car_part.life; }
static void part_life_set(Object *o, uint8_t v) { o->car_part.life = v; }

/*
 * @routine 08:AB71 PartOffset
 * WHAT IT DOES: A part's offset on its car: the vector of its bytes
 * +$48-$4A turned by the car's angles and doubled.
 */
static Vec16 PartOffset(const Object *part, const Object *car)
{
    Vec16 v = ObjectTurnVector(car, part->car_part.offset_on_car[0], part->car_part.offset_on_car[1], part->car_part.offset_on_car[2]);

    v.x = (int16_t)(uint16_t)((uint16_t)v.x << 1);
    v.y = (int16_t)(uint16_t)((uint16_t)v.y << 1);
    v.z = (int16_t)(uint16_t)((uint16_t)v.z << 1);
    return v;
}

/* The part on its car at the offset, its angles the car's. */
static Vec16 part_on_car(Object *o, const Object *car)
{
    Vec16 v = PartOffset(o, car);

    ObjectCopyPosition(o, car);
    ObjectCopyAnglesFrom(o, car);
    return v;
}

static void add_vector(Object *o, Vec16 v)
{
    o->x = (uint16_t)(o->x + (uint16_t)v.x);
    o->y = (uint16_t)(o->y + (uint16_t)v.y);
    o->z = (uint16_t)(o->z + (uint16_t)v.z);
}

/*
 * @routine 08:AC27 PartDetachStep
 * WHAT IT DOES: The detaching part's frame: released at +$4B = 8; at 7 the
 * car's +$25 bit 7 is cleared; +$4B counts up and the part sits at its car
 * plus its offset times 1-4.
 */
void PartDetachStep(Object *o)
{
    Object *car = obj_at(o->parent);
    Vec16 v;
    uint8_t n;

    if (o->car_part.steps == 0x08) {
        ObjectRelease(o);
        return;
    }
    if (o->car_part.steps == 0x07)
        car->car.state &= (uint8_t)~CAR_DETACHING;
    o->car_part.steps++;
    v = part_on_car(o, car);
    n = o->car_part.steps;
    if (!((uint8_t)(n - 0x04) & 0x80)) {
        n = (uint8_t)(n - 0x09);
        if (!n) {
            ObjectRebucket(o);
            return;
        }
        n = (uint8_t)-n;
    }
    do {
        add_vector(o, v);
        n--;
    } while (n);
    ObjectRebucket(o);
}

/*
 * @routine 08:AC04 PartDetach
 * WHAT IT DOES: A part comes off its car: the car's +$25 bit 7 set, the
 * part's model by the car's kind, then its frames.
 */
void PartDetach(Object *o)
{
    Object *car = obj_at(o->parent);

    car->car.state |= CAR_DETACHING;
    ObjectCarLookByKind(o, car->car.kind);
    o->car_part.steps = 0x00;
    ObjectSetHandler(o, H_PART_DETACH_STEP);
    PartDetachStep(o);
}

/*
 * @routine 08:AE0F PartReturnStep
 * WHAT IT DOES: The part falling back onto its car: it moves a quarter of
 * the way to its place and turns a quarter of the way to the car's angles,
 * moved with the car's record; sounds on the last count; while +$4B lasts it
 * counts down; then, unless the car's +$25 bit 2 is set, it is released, and
 * with its own +$25 bit 0 the car's bit 2 is cleared.
 */
void PartReturnStep(Object *o)
{
    Object *car;
    Vec16 v;
    int moved;

    o->timer = 3;
    car = obj_at(o->parent);
    ObjectVelocityFromRecordOf(o, car);
    v = PartOffset(o, car);
    o->x = ApproachQuarterMin4(o->x, (uint16_t)(car->x + (uint16_t)v.x), &moved);
    o->y = ApproachQuarterMin4(o->y, (uint16_t)(car->y + (uint16_t)v.y), &moved);
    o->z = ApproachQuarterMin4(o->z, (uint16_t)(car->z + (uint16_t)v.z), &moved);
    o->pitch = ApproachQuarterMin4Byte(o->pitch, car->pitch, &moved);
    o->heading = ApproachQuarterMin4Byte(o->heading, car->heading, &moved);
    o->roll = ApproachQuarterMin4Byte(o->roll, car->roll, &moved);
    ObjectApplyVelocity(o);
    if (o->car_part.steps == 0x01)
        SoundPort0QueuePush((o->car_part.state & PART_MARKED) ? 0x4D : 0x4E);
    ObjectRebucket(o);
    o->timer = 3;
    if (o->car_part.steps) {
        o->car_part.steps--;
        return;
    }
    if (!(car->car.state & CAR_LOOSE))
        ObjectSetHandler(o, H_RELEASE);
    if (o->car_part.state & PART_HIT)
        car->car.state &= (uint8_t)~CAR_LOOSE;
}

/*
 * @routine 08:ADE2 PartReturn
 * WHAT IT DOES: The flying part turns back to its car.
 */
void PartReturn(Object *o)
{
    ObjectSetHandler(o, H_PART_RETURN_STEP);
    o->car_part.steps = 0x0A;
    SoundPort0QueuePush((o->car_part.state & PART_MARKED) ? 0x4C : 0x4B);
    PartReturnStep(o);
}

/*
 * @routine 08:AD70 PartFlyStep
 * WHAT IT DOES: The flying part's frame: follows its record; with no
 * contacts, while its car's +$25 bit 4 is set, a summary with bit 7 breaks
 * it off. Then, unless the car's bit 4 is set and +$4B is 0, it tumbles, or
 * at +$4B = 0 turns back; a life counts down and releases it; it may spray.
 */
void PartFlyStep(Object *o)
{
    Object *car;
    uint8_t v;

    o->timer = 3;
    if (!ObjectSyncFromRecord(o)) {
        car = obj_at(o->parent);
        if ((car->car.state & CAR_WRECKED) && (RecordContactSummary(o) & 0x80)) {
            ObjectBreakOffContact(o);
            return;
        }
    }
    o->timer = 3;
    car = obj_at(o->parent);
    if (!(car->car.state & CAR_WRECKED) || o->car_part.steps) {
        if (!o->car_part.steps) {
            PartReturn(o);
            return;
        }
        o->car_part.steps--;
        o->pitch = (uint8_t)(o->pitch + 0x08);
        o->heading = (uint8_t)(o->heading + 0x04);
    }
    v = part_life(o);
    if (!(v & 0x80)) {
        v--;
        if (!v) {
            ObjectRelease(o);
            return;
        }
        part_life_set(o, v);
    }
    ObjectPieceSprayChance(o);
}

/*
 * @routine 08:ACB9 PartKnockOff
 * WHAT IT DOES: A part knocked off its car: its model by the car's kind,
 * placed at the car plus its offset, the car's record velocity, a record of
 * its own; with none free it is released. Its velocity grows by the offset
 * (Y doubled), given to the record, then its flying frame.
 */
void PartKnockOff(Object *o)
{
    Object *car = obj_at(o->parent);
    Vec16 v;

    ObjectCarLookByKind(o, car->car.kind);
    v = part_on_car(o, car);
    add_vector(o, v);
    ObjectVelocityFromRecordOf(o, car);
    if (!GsuRecordNewFallingPart(o)) {
        if (o->car_part.state & PART_HIT) {
            car->car.state &= (uint8_t)~CAR_LOOSE;
            car->car.state &= (uint8_t)~CAR_WRECKED;
        }
        ObjectRelease(o);
        return;
    }
    if (!(part_life(o) & 0x80))
        rec_or(o->record, FXREC_FLAGS16, 0x0080);
    o->vx = (uint16_t)(o->vx + (uint16_t)v.x);
    o->vy = (uint16_t)(o->vy + (uint16_t)((uint16_t)v.y << 1));
    o->vz = (uint16_t)(o->vz + (uint16_t)v.z);
    GsuRecordVelocity(o);
    ObjectSetHandler(o, H_PART_FLY_STEP);
    PartFlyStep(o);
}

/*
 * @routine 08:AC97 PartHit
 * WHAT IT DOES: A part hit off its car: merges the Super FX pools, sets its
 * +$25 bit 0 and the car's bit 2, and knocks it off.
 */
void PartHit(Object *o)
{
    FxPoolsMergeProgram();
    o->car_part.state |= PART_HIT;
    obj_at(o->parent)->car.state |= CAR_LOOSE;
    PartKnockOff(o);
}

/* the car's frame after its input */
static void player_drive(Object *o)
{
    GsuRecordLand(o);
    PlayerCarEvents(o);
    ObjectPoseFromRecord(o);
    GsuRecordBoostRecharge(o);
    ObjectMoveForward(o);
    ObjectRebucket(o);
    o->timer = 3;
}

/*
 * @routine 08:AF93 PlayerCar1Step
 * WHAT IT DOES: Player 1's car frame: its drive, +$36 = 3; +$24 bit 1
 * follows bit 1 of its record's +$1C.
 */
void PlayerCar1Step(Object *o)
{
    g.current_player = 0;
    player_drive(o);
    o->draw_flags &= 0xFD;
    if (rec_get(o->record, FXREC_FLAGS1C) & 0x0002)
        o->draw_flags |= 0x02;
}

/* A player car's record; with one, it is the record at `match` and gets
   bit 3 of +$1C. */
static void player_record(Object *o, uint16_t n, uint16_t match)
{
    ObjectCarRecordNew(o, n);
    if (o->record) {
        fxram_put(match, o->record);
        rec_or(o->record, FXREC_FLAGS1C, 0x0008);
    }
}

/* The player car's set-up after its record. */
static void player_attach(Object *o, ObjectHandler h, ObjRef bare)
{
    ObjectSetHandler(o, h);
    ObjectAttachLinked(o, obj_at(bare));
    ObjectCarLookByKind(o, o->car.kind);
    g.player_flags[0] = 0;
    o->car.idle_frames = 0;
}

/*
 * @routine 08:AF0F PlayerCar1
 * WHAT IT DOES: Sets up player 1's car: on the object list, $A0 higher; a
 * replay's mark in a one-player race with the path camera; its record; its
 * handler; attached to its camera director; then its frame.
 */
void PlayerCar1(Object *o)
{
    o->list_kind = 0x01;
    ObjectListAppend(o);
    o->y = (uint16_t)(o->y + 0xFF60);
    if (g.hud_mode == 0 && obj_at(g.director[0])->handler == object_handler_word(H_DIRECTOR_PATH_START))
        o->car.car_script = car_script_word(CAR_BONUS_TRUCK);
    player_record(o, 0x0001, FXIN_PLAYER1_RECORD);
    player_attach(o, H_PLAYER_CAR_1_STEP, g.director[0]);
    PlayerCar1Step(o);
}

/*
 * @routine 08:B049 PlayerCar2Step
 * WHAT IT DOES: Player 2's car frame: current player 1, its buttons as the
 * controls' input, then the drive.
 */
void PlayerCar2Step(Object *o)
{
    g.current_player = 1;
    g.control_held = frame_state.held[1];
    g.control_prev = g.race_last[1];
    player_drive(o);
}

/*
 * @routine 08:AFD7 PlayerCar2
 * WHAT IT DOES: Sets up player 2's car, as PlayerCar1 with its own record
 * and director.
 */
void PlayerCar2(Object *o)
{
    o->list_kind = 0x02;
    ObjectListAppend(o);
    o->y = (uint16_t)(o->y + 0xFF60);
    player_record(o, 0x0002, FXIN_PLAYER2_RECORD);
    player_attach(o, H_PLAYER_CAR_2_STEP, g.director[1]);
    PlayerCar2Step(o);
}

/*
 * @routine 08:B1E1 ObjectCarSpawnListed
 * WHAT IT DOES: Puts the object on the object list, then ObjectCarSpawn.
 */
void ObjectCarSpawnListed(Object *o)
{
    ObjectListAppend(o);
    ObjectCarSpawn(o);
}

static void car_of_kind(Object *o, uint8_t v, uint8_t k)
{
    o->car.wait = v;
    o->list_kind = k;
    ObjectCarSpawnListed(o);
}

/* @routine 08:B1BA ObjectCarKind3 */
void ObjectCarKind3(Object *o) { car_of_kind(o, 0x00, 0x03); }
/* @routine 08:B1C7 ObjectCarKind4 */
void ObjectCarKind4(Object *o) { car_of_kind(o, 0x05, 0x04); }
/* @routine 08:B1D4 ObjectCarKind5 */
void ObjectCarKind5(Object *o) { car_of_kind(o, 0x00, 0x05); }

/* one shed part: its kind and offset bytes, a count, whether it is marked */
typedef struct {
    uint8_t o48, o49, o4a, count;
    int mark;
} ShedPart;

/* The parts of one car set (their models: data/'s crash_part_model_word): the
   first knocked off at once on a hit. */
static const ShedPart crash_sets[4][6] = {
    {   /* 08:A5E4: the F-Type's */
        {0x00, 0xE3, 0xC5, 0x23, 1}, {0x00, 0xF9, 0x2D, 0x00, 0},
        {0xE9, 0xFA, 0xF8, 0x0A, 1}, {0x17, 0xFA, 0xF8, 0x14, 0},
        {0x00, 0xF6, 0xF6, 0x19, 1}, {0x00, 0xF7, 0xDA, 0x1E, 0}
    },
    {   /* 08:A88B: the Coupe's */
        {0x00, 0xEC, 0xC5, 0x23, 1}, {0x00, 0xFA, 0x21, 0x00, 0},
        {0x00, 0xD5, 0xEE, 0x0A, 1}, {0xEF, 0xF6, 0xEC, 0x14, 0},
        {0x11, 0xF6, 0xEC, 0x19, 1}, {0x00, 0xF8, 0xC9, 0x1E, 0}
    },
    {   /* 08:A33D: the 4WD's */
        {0x00, 0xEE, 0x42, 0x23, 1}, {0x00, 0xF1, 0x3C, 0x00, 0},
        {0x00, 0xCE, 0x0F, 0x0A, 1}, {0x00, 0xD0, 0xE9, 0x14, 0},
        {0x00, 0xEF, 0xF6, 0x19, 1}, {0x00, 0x0F, 0x02, 0x1E, 0}
    },
    {   /* 08:A096: the 2WD's */
        {0xD8, 0xEC, 0xB6, 0x23, 1}, {0x00, 0xDC, 0x14, 0x00, 0},
        {0x00, 0xEC, 0x1C, 0x0A, 1}, {0x00, 0xE0, 0xE2, 0x14, 0},
        {0x00, 0xE8, 0xC6, 0x19, 1}, {0x28, 0xEC, 0xB6, 0x1E, 0}
    }
};

/* One part shed. Returns 1 when the crash must end at once: no first part,
   or the first part's hit left the car's +$25 bit 2 clear. */
static int shed(Object *car, uint16_t model, const ShedPart *part, uint8_t flags, int first)
{
    Object *n = ObjectSpawn(model);

    if (!n)
        return first;
    n->car_part.offset_on_car[0] = (int8_t)part->o48;
    n->car_part.offset_on_car[1] = (int8_t)part->o49;
    n->car_part.offset_on_car[2] = (int8_t)part->o4a;
    n->parent = obj_ref(car);
    n->car_part.steps = part->count;
    part_life_set(n, (flags & 0x02) ? 0x14 : 0xFF);
    if (part->mark)
        n->car_part.state |= PART_MARKED;
    if (!(flags & 0x01)) {
        ObjectSetHandler(n, H_PART_DETACH);
        return 0;
    }
    if (!first) {
        ObjectSetHandler(n, H_PART_KNOCK_OFF);
        return 0;
    }
    ObjectSetHandler(n, H_PART_HIT);
    ObjectCallHandler(n);
    return !(car->car.state & CAR_LOOSE);
}

/*
 * @routine 08:A034 CarCrashParts
 * WHAT IT DOES: A car crashes and sheds its six parts by its model (+$27):
 * flags bit 0 a hit, bit 1 a 20-frame life; then a sound. Returns 1 with
 * nothing done when the car has already crashed, its model is none of these
 * or no first part could be made, or when the first part's hit left the
 * car's bit 2 clear; else 0.
 */
int CarCrashParts(Object *o, uint8_t flags)
{
    static const CarScript cars[8] = {CAR_F_TYPE, CAR_COUPE, CAR_4WD, CAR_2WD, CAR_RIVAL_F_TYPE, CAR_RIVAL_COUPE, CAR_RIVAL_4WD, CAR_RIVAL_2WD};
    int i;

    if (o->car.state & (CAR_LOOSE | CAR_DETACHING))
        return 1;
    for (i = 0; i < 8 && o->car.car_script != car_script_word(cars[i]); i++)
        ;
    if (i == 8)
        return 1;
    for (int p = 0; p < 6; p++)
        if (shed(o, crash_part_model_word(i & 3, p), &crash_sets[i & 3][p], flags, p == 0) && p == 0)
            return 1;
    SoundPort0QueuePush((flags & 0x01) ? 0x5B : 0x43);
    return 0;
}

/*
 * @routine 08:B0F4 ObjectRivalCarStep
 * WHAT IT DOES: The rival car's frame: with the course's bit 10 its record's
 * +$1C bit 5 follows +$36 = 1; one frame in 14 its record's +$70 bit 1
 * flips; when its record's +$2C bit 2 is set it crashes, counted; with +$25
 * bit 2, +$24 bit 5; then the car's frame.
 */
void ObjectRivalCarStep(Object *o)
{
    uint16_t rec = o->record;

    if (g.course_flags & 0x0400) {
        if (o->timer == 0x01) {
            o->timer = 0x02;
            rec_or(rec, FXREC_FLAGS1C, 0x0020);
        } else {
            rec_and(rec, FXREC_FLAGS1C, 0xFFDF);
        }
    }
    if (RandomStep() >= 0xF2)
        rec_put(rec, FXREC_DIRECTION, (uint16_t)(rec_get(rec, FXREC_DIRECTION) ^ 0x0002));
    if (rec_get(rec, FXREC_CONTACT_FLAGS) & 0x0004) {
        o->car.state |= CAR_WRECKED;
        if (!CarCrashParts(o, 0x03)) {
            g.rivals_left--;
            rec_or(rec, FXREC_FLAGS16, 0x0020);
            rec_or(rec, FXREC_FLAGS1C, 0x0010);
        }
    }
    if (o->car.state & CAR_LOOSE)
        o->draw_flags |= 0x20;
    ObjectCarFrame(o);
}

/*
 * @routine 08:B079 ObjectRivalCar
 * WHAT IT DOES: Sets up a rival car: counted, list kind 0, spawned, its
 * record's +$64 bit 4 set, then its frame.
 */
void ObjectRivalCar(Object *o)
{
    g.rivals_left++;
    o->car.wait = 0;
    o->list_kind = 0;
    ObjectCarSpawnListed(o);
    rec_or(o->record, FXREC_FLAGS64, 0x0010);
    ObjectSetHandler(o, H_RIVAL_CAR_STEP);
    ObjectRivalCarStep(o);
}
