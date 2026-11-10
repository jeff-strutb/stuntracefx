/*
 * object_handlers.c: object handlers of bank $08: bursts of pieces, a
 * blinking child, the Speed Trax car and signal pair, a car's children,
 * break-off pieces, attachments, flames and so on. A handler that becomes
 * another stores the next one's number and runs it at once.
 *
 * Not converted, nothing in the game reaches them: see object/NOTES.
 */
#include <stdio.h>
#include <string.h>

#include "audio/sound.h"
#include "fx/fx.h"
#include "math/math.h"
#include "race/race.h"
#include "object/handlers.h"
#include "object/object.h"
#include "system/chip.h"
#include "system/state.h"

#define SET_LOW(field, v) ((field) = (uint16_t)(((field) & 0xFF00) | (uint8_t)(v)))

static uint16_t asr1(uint16_t v) { return (uint16_t)((v >> 1) | (v & 0x8000)); }

/* the sign bit of a 16-bit result of a compare */
static int below(uint16_t a, uint16_t b) { return ((uint16_t)(a - b) & 0x8000) != 0; }

/* |w| as the original takes it */
static uint16_t abs16(uint16_t w) { return (w & 0x8000) ? (uint16_t)-w : w; }

/* +$48: whether it has run out; otherwise it counts down */
static int count_down(Object *o)
{
    if (!o->timed.frames)
        return 1;
    o->timed.frames--;
    return 0;
}

/* A new object of the model with handler h at object o's place. */
static void burst_piece(Object *o, uint16_t model, uint16_t h)
{
    Object *n = ObjectSpawn(model);

    if (!n)
        return;
    ObjectSetHandler(n, h);
    ObjectCopyPosition(n, o);
}

/*
 * @routine 08:EC9C ObjectBurstSpawn
 * WHAT IT DOES: The object bursts into four pieces.
 */
void ObjectBurstSpawn(Object *o)
{
    for (int i = 0; i < 4; i++)
        burst_piece(o, model_word_after(MODEL_BURST_PIECE, i), H_BURST_PIECE_START);
}

/*
 * @routine 08:EDB6 ObjectBurstPieceStep
 * WHAT IT DOES: A burst piece's frame: synced from its record; released when
 * it has none or its count runs out, or when its contact summary has bit 7
 * and it is not touching.
 */
void ObjectBurstPieceStep(Object *o)
{
    int touching = ObjectSyncFromRecord(o);

    if (record_contacts.no_record) {
        ObjectRelease(o);
        return;
    }
    if (!touching && (RecordContactSummary(o) & 0x80)) {
        ObjectBreakOffContact(o);
        return;
    }
    if (count_down(o))
        ObjectRelease(o);
}

/*
 * @routine 08:ED39 ObjectBurstPieceStart
 * WHAT IT DOES: A burst piece's first frame: a random velocity, count, angle
 * and a record, then its first step.
 */
void ObjectBurstPieceStart(Object *o)
{
    ObjectSetHandler(o, H_BURST_PIECE_STEP);
    SET_LOW(o->vx, RandomStep() & 0x3F);
    o->vx = (uint16_t)(o->vx - 0x001F);
    SET_LOW(o->vy, RandomStep() & 0x3F);
    o->vy = (uint16_t)(o->vy + 0x001E);
    o->vy = (uint16_t)-o->vy;
    SET_LOW(o->vz, RandomStep() & 0x3F);
    o->vz = (uint16_t)(o->vz - 0x001F);
    o->timed.frames = 0x28;
    o->heading = RandomStep();
    o->pitch = RandomStep();
    GsuRecordNewFallingPart(o);
    GsuRecordVelocity(o);
    ObjectBurstPieceStep(o);
}

/* the player 1 record's +$42, then the object synced with +$36 = 3 */
static void player1_record_42(uint16_t v, Object *o)
{
    rec_put(fxram_get(FXIN_PLAYER1_RECORD), FXREC_SIDES, v);
    ObjectSyncFromRecord(o);
    o->timer = 3;
}

/*
 * @routine 08:EE9F ObjectBlinkerFade
 * WHAT IT DOES: The blinker's second phase: while its count runs its colour
 * steps down and the record stays shown, and at colour 0 the record gets
 * $1010; when the count runs out both it and its child are released.
 */
void ObjectBlinkerFade(Object *o)
{
    uint8_t v;

    if (count_down(o)) {
        uint16_t rec = fxram_get(FXIN_PLAYER1_RECORD);

        rec_and(rec, FXREC_SIDES, 0xEFFF);
        ObjectRelease(obj_at(o->parent));
        ObjectRelease(o);
        return;
    }
    if (!(o->shown_frame & 0x7F)) {
        player1_record_42(0x1010, o);
        return;
    }
    v = (uint8_t)(o->shown_frame - 1);
    if (!(v & 0x80))
        v = (uint8_t)(v + 0x10);
    v &= 0x7F;
    if (v >= 0x10)
        v = (uint8_t)(v - 0x10);
    o->shown_frame = (uint8_t)(v | 0x80);
    player1_record_42(0x1000, o);
}

/*
 * @routine 08:EE63 ObjectBlinkerRise
 * WHAT IT DOES: The blinker's first phase: while its count runs, its record
 * shown and synced; then the fade.
 */
void ObjectBlinkerRise(Object *o)
{
    if (!count_down(o)) {
        player1_record_42(0x1000, o);
        return;
    }
    o->timed.frames = 0x28;
    ObjectSetHandler(o, H_BLINKER_FADE);
    ObjectBlinkerFade(o);
}

/* the record's place, then its flags */
static void record_place(Object *o, uint16_t x, uint16_t y, uint16_t z, uint16_t flags)
{
    GsuRecordPosition(o, x, y, z);
    GsuRecordSetFlags(o, flags);
}

/*
 * @routine 08:EDDD ObjectBlinkerStart
 * WHAT IT DOES: Makes the object a blinker with a child at its place, a
 * count, a colour and a record; then its first phase.
 */
void ObjectBlinkerStart(Object *o)
{
    Object *n = ObjectSpawn(model_word(MODEL_BLINKER));

    if (n) {
        ObjectCopyPosition(n, o);
        o->parent = obj_ref(n);
        ObjectRebucket(n);
    }
    ObjectSetHandler(o, H_BLINKER_RISE);
    o->timed.frames = 0x32;
    o->shown_frame = 0x8F;
    o->script = object_script_word(OBJECT_SCRIPT_BLINKER);
    if (!GsuRecordNewStill(o))
        return;
    record_place(o, 0x02D0, 0x0190, 0x02D0, 0x4000);
    ObjectBlinkerRise(o);
}

/*
 * @routine 08:EF19 ObjectPaceCarStep
 * WHAT IT DOES: The Speed Trax pace car's frame: near-focus, rebucketed,
 * posed. Until the signal says go: once past the mark the signal is told,
 * and its record's bit 11 of +$42 is set while slow; then marked. At go: the
 * record frozen, hidden, moved away, its handler cleared.
 */
void ObjectPaceCarStep(Object *o)
{
    uint16_t rec;

    ObjectNearFocus(o);
    ObjectRebucket(o);
    ObjectPoseFromRecord(o);
    rec = o->record;
    if (g.pace_signal != 0x02) {
        if (!below((uint16_t)(rec_get(rec, FXREC_POS_Z) + 0x0258), g.pace_z))
            g.pace_signal = 0x01;
        rec_and(rec, FXREC_SIDES, 0xF7FF);
        if (below(rec_get(rec, FXREC_SPEED), 0x001E))
            rec_or(rec, FXREC_SIDES, 0x0800);
        GsuRecordKeep(o);
        return;
    }
    rec_or(rec, FXREC_FLAGS1C, 0x0010);
    o->draw_flags |= 0x20;
    o->draw_flags |= 0x08;
    o->y = (uint16_t)(o->y + 0xB1E0);
    o->handler = 0;
    o->handler_bank = 0;
}

/*
 * @routine 08:EF06 ObjectPaceCarStart
 * WHAT IT DOES: The pace car's first frame.
 */
void ObjectPaceCarStart(Object *o)
{
    ObjectCarSpawn(o);
    ObjectSetHandler(o, H_PACE_CAR_STEP);
    ObjectPaceCarStep(o);
}

/*
 * @routine 08:F0B1 ObjectSignalStep
 * WHAT IT DOES: The pace car's signal: synced each frame; once the race
 * nears, its child's model and light step up, and at 7 the player's model
 * changes, the child and the signal are released and the car goes.
 */
void ObjectSignalStep(Object *o)
{
    if (g.pace_signal) {
        Object *child = obj_at(o->parent);
        uint8_t v;

        child->model = model_word(MODEL_SIGNAL_LIGHT_COUNTING);
        if ((child->shown_frame & 0x7F) == 0x07) {
            obj_at(g.player_car)->model = model_word(MODEL_PLAYER_AT_GO);
            ObjectRelease(child);
            ObjectRelease(o);
            g.pace_signal = 0x02;
            return;
        }
        v = (uint8_t)(child->shown_frame + 1);
        if (!(v & 0x80))
            v = (uint8_t)(v + 0x08);
        v &= 0x7F;
        if (v >= 0x08)
            v = (uint8_t)(v - 0x08);
        child->shown_frame = (uint8_t)(v | 0x80);
    }
    ObjectSyncFromRecord(o);
    o->timer = 3;
}

/*
 * @routine 08:EFAF ObjectPaceCarSetup
 * WHAT IT DOES: In Speed Trax (otherwise released): a pace car ahead of the
 * object and a signal on it; the object becomes the signal's driver.
 */
void ObjectPaceCarSetup(Object *o)
{
    Object *n;

    if (g.hud_mode) {
        ObjectRelease(o);
        return;
    }
    g.pace_z = o->z;
    g.pace_signal = 0;
    o->y = (uint16_t)(o->y + 0xFF9C);
    n = ObjectSpawn(model_word(MODEL_NONE));
    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->z = (uint16_t)(n->z + 0xF254);
    n->y = 0;
    n->car.car_script = g.selection_handler[0];
    ObjectSetHandler(n, H_PACE_CAR_START);
    n = ObjectSpawn(model_word(MODEL_SIGNAL_LIGHT));
    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->y = (uint16_t)(n->y + 0x0032);
    n->shown_frame = 0x80;
    ObjectRebucket(n);
    o->parent = obj_ref(n);
    ObjectSetHandler(o, H_SIGNAL_STEP);
    o->script = object_script_word(OBJECT_SCRIPT_SIGNAL);
    if (!GsuRecordNewStill(o))
        return;
    record_place(o, 0x0640, 0x0640, 0x0640, 0xC100);
    o->signal.unread = 0x14;
    ObjectSignalStep(o);
}

/* whether the record's |X speed| + |Z speed| is at least $64 */
static int record_fast(uint16_t rec)
{
    return !below((uint16_t)(abs16(rec_get(rec, FXREC_VEL_X)) + abs16(rec_get(rec, FXREC_VEL_Z))), 0x0064);
}

/*
 * @routine 08:D1BF ObjectBreakOffChance
 * WHAT IT DOES: On a random byte of $7F or more, a new piece at the child's
 * place with the child's parent as its +$27. Returns the piece, or NULL
 * when none was made; the break-off handler goes to the piece.
 */
static Object *ObjectBreakOffChance(Object *child)
{
    Object *n;

    if (RandomStep() < 0x7F)
        return NULL;
    n = ObjectSpawn(model_word(MODEL_BREAK_OFF_PIECE));
    if (!n)
        return NULL;
    ObjectCopyPosition(n, child);
    n->break_off.car = child->parent;
    return n;
}

/* The break-off a car's knock gives a child, if any */
static void child_break_off(Object *o, const Object *p, uint16_t own)
{
    uint16_t d, car_flags, w;
    Object *n;

    if (p->draw_flags & 0x40)
        return;
    car_flags = rec_get(p->record, FXREC_FLAGS40);
    if (!(rec_get(p->record, FXREC_FLAGS64) & 0x0003))
        return;
    w = rec_get(own, FXPART_FLAGS);
    if (!(w & 0x0001) || !(w & 0x0010))
        return;
    d = rec_get(own, FXREC_CONTACT_FLAGS);
    if (d & 0x0080) {
        if ((n = ObjectBreakOffChance(o)) != NULL)
            ObjectSetHandler(n, record_fast(own) ? H_BREAK_OFF_F : H_BREAK_OFF_E);
        return;
    }
    if (d & 0x2000) {
        if (record_fast(own) && (n = ObjectBreakOffChance(o)) != NULL)
            ObjectSetHandler(n, H_BREAK_OFF_D);
        return;
    }
    if (d & 0x0800) {
        if (record_fast(own) && (n = ObjectBreakOffChance(o)) != NULL)
            ObjectSetHandler(n, H_BREAK_OFF_B);
        return;
    }
    if (d & 0x1000) {
        if (record_fast(own) && (n = ObjectBreakOffChance(o)) != NULL)
            ObjectSetHandler(n, H_BREAK_OFF_A);
        return;
    }
    if (d & 0x0060) {
        if (record_fast(own) && (n = ObjectBreakOffChance(o)) != NULL)
            ObjectSetHandler(n, H_BREAK_OFF_G);
        return;
    }
    if (!(car_flags & 0x0800) && !(rec_get(own, FXPART_FLAGS) & 0x0020))
        return;
    if ((n = ObjectBreakOffChance(o)) != NULL)
        ObjectSetHandler(n, H_BREAK_OFF_H);
}

/*
 * @routine 08:BD22 ObjectChildFollow
 * WHAT IT DOES: Keeps a child on its parent: rebucketed; hidden while the
 * parent's record is flagged; otherwise by its record's flags it may break
 * off with a new handler, and it is placed on its record and turned.
 */
void ObjectChildFollow(Object *o)
{
    Object *p;
    uint16_t own;

    ObjectRebucket(o);
    p = obj_at(o->parent);
    if (rec_get(p->record, FXREC_FLAGS16) & 0x0400) {
        rec_or(o->record, FXREC_FLAGS16, 0x0480);
        o->draw_flags |= 0x20;
        return;
    }
    o->draw_flags &= 0xDF;
    rec_and(o->record, FXREC_FLAGS16, 0xFB7F);
    own = o->record;
    child_break_off(o, p, own);
    o->x = (uint16_t)(rec_get(own, FXREC_POS_X) + rec_get(own, FXPART_OFFSET_X));
    o->y = (uint16_t)(0xFFEC - o->size + rec_get(own, FXREC_POS_Y) + rec_get(own, FXPART_OFFSET_Y));
    o->z = (uint16_t)(rec_get(own, FXREC_POS_Z) + rec_get(own, FXPART_OFFSET_Z));
    ObjectTurnFromRecord(own, o);
}

/*
 * @routine 08:BA57 ObjectChildrenSpawn
 * WHAT IT DOES: Gives the object a child for each record in its record's
 * chain, except records with +$46 bit 9: the child's model, +$29 and +$5F
 * from the table at $09:8000 for the record's +$12.
 */
void ObjectChildrenSpawn(Object *o)
{
    uint16_t rec;

    if (!o->record)
        return;
    for (rec = rec_get(o->record, FXCAR_PARTS); rec; rec = rec_get(rec, FXPART_NEXT)) {
        uint16_t t, look;
        Object *n;

        if (rec_get(rec, FXPART_FLAGS) & 0x0200)
            continue;
        t = rec_get(rec, FXPART_CHILD);
        look = object_entry_word(t, DATA_LOOK);
        n = ObjectSpawn(object_entry_word(t, DATA_MODEL));
        if (!n)
            continue;
        if (look) {
            n->draw_flags |= 0x08;
            n->material = 0;
            n->size = (uint8_t)object_entry_word(t, DATA_SIZE);
            n->child.look = look;
            if (g.split_screen)
                n->child.look = look_word_split(n->child.look);
        }
        ObjectSetHandler(n, H_CHILD_START);
        n->record = rec;
        n->parent = obj_ref(o);
    }
}

/* the child's +$24 flags from its parent's; +$36 = 3 */
static void child_flags(Object *o)
{
    Object *p = obj_at(o->parent);

    o->draw_flags &= 0xBF;
    if (p->draw_flags & 0x08)
        o->draw_flags |= 0x20;
    else if (p->draw_flags & 0x40)
        o->draw_flags |= 0x40;
    o->timer = 3;
}

/*
 * @routine 08:BB2E ObjectChildStep
 * WHAT IT DOES: A child's frame on its parent: follows it; when the parent's
 * +$25 has bit 2 it comes loose next frame; its flags and +$36 = 3.
 */
void ObjectChildStep(Object *o)
{
    ObjectChildFollow(o);
    if (obj_at(o->parent)->car.state & CAR_LOOSE)
        ObjectSetHandler(o, H_CHILD_LOOSE);
    child_flags(o);
}

/*
 * @routine 08:BB16 ObjectChildStart
 * WHAT IT DOES: A child's first frame.
 */
void ObjectChildStart(Object *o)
{
    ObjectSetHandler(o, H_CHILD_STEP);
    o->list_kind = obj_at(o->parent)->list_kind;
    ObjectChildStep(o);
}

/* back on the parent: its record again, then the normal step's flags */
static void child_back(Object *o)
{
    o->record = o->child.own_record;
    ObjectSetHandler(o, H_CHILD_STEP);
    child_flags(o);
}

/* below the parent by the child's +$5F and $14 */
static uint16_t child_y(uint8_t size, uint16_t a)
{
    return (uint16_t)((uint16_t)-size + 0xFFEC + a);
}

/*
 * @routine 08:BC73 ObjectChildReturn
 * WHAT IT DOES: A loose child going back: a quarter of the way to its place
 * on the parent's record, moved and rebucketed; back on its parent once
 * the parent is no longer loose; its flags, +$36 = 3.
 */
void ObjectChildReturn(Object *o)
{
    Object *p = obj_at(o->parent);
    uint16_t rec = o->child.own_record, tx, ty, tz;
    int moved;

    ObjectVelocityFromRecordOf(o, p);
    tx = (uint16_t)(rec_get(rec, FXREC_POS_X) + rec_get(rec, FXPART_OFFSET_X));
    ty = (uint16_t)(child_y(o->size, rec_get(rec, FXREC_POS_Y)) + rec_get(rec, FXPART_OFFSET_Y));
    tz = (uint16_t)(rec_get(rec, FXREC_POS_Z) + rec_get(rec, FXPART_OFFSET_Z));
    ObjectTurnFromRecord(rec, o);
    o->x = ApproachQuarterMin4(o->x, tx, &moved);
    o->y = ApproachQuarterMin4(o->y, ty, &moved);
    o->z = ApproachQuarterMin4(o->z, tz, &moved);
    ObjectApplyVelocity(o);
    ObjectRebucket(o);
    if (!(p->car.state & CAR_LOOSE)) {
        child_back(o);
        return;
    }
    child_flags(o);
}

/*
 * @routine 08:BBE2 ObjectChildFlying
 * WHAT IT DOES: A loose child's flight: synced from its record, broken off
 * on contact, it drops and may spray while its count runs; then released or
 * it goes back.
 */
void ObjectChildFlying(Object *o)
{
    Object *p;

    if (!ObjectSyncFromRecord(o)) {
        p = obj_at(o->parent);
        if ((p->car.state & CAR_WRECKED) && (RecordContactSummary(o) & 0x80)) {
            ObjectBreakOffContact(o);
            return;
        }
    }
    p = obj_at(o->parent);
    if ((p->car.state & CAR_WRECKED) && !o->child.flight) {
        if (rec_get(p->record, FXREC_FLAGS64) & 0x0010) {
            ObjectRelease(o);
            return;
        }
        ObjectPieceSprayChance(o);
        child_flags(o);
        return;
    }
    if (!o->child.flight) {
        ObjectSetHandler(o, H_CHILD_RETURN);
        ObjectChildReturn(o);
        return;
    }
    o->child.flight--;
    o->y = child_y(o->size, o->y);
    ObjectPieceSprayChance(o);
    child_flags(o);
}

/*
 * @routine 08:BB7C ObjectChildLoose
 * WHAT IT DOES: A child comes loose: a record of its own with the parent's
 * record's velocity, its X and Z velocity from its old record halved, then
 * its flight.
 */
void ObjectChildLoose(Object *o)
{
    Object *p;

    ObjectSetHandler(o, H_CHILD_FLYING);
    o->child.own_record = o->record;
    GsuRecordNewFallingPart(o);
    p = obj_at(o->parent);
    ObjectVelocityFromRecordOf(o, p);
    o->vx = asr1(rec_get(o->child.own_record, FXPART_WORLD));
    o->vy = 0xFFCE;
    o->vz = asr1(rec_get(o->child.own_record, FXPART_WORLD + 4));
    GsuRecordVelocity(o);
    o->child.flight = (uint8_t)((RandomStep() & 0x0F) + 0x0A);
    ObjectChildFlying(o);
}

/*
 * @routine 08:D549 ObjectPieceFall
 * WHAT IT DOES: A broken-off piece's frame: rebucketed; its turn bytes its
 * count with bit 7; released at its end, otherwise moved; +$36 = 3.
 */
void ObjectPieceFall(Object *o)
{
    ObjectRebucket(o);
    o->sprite_turn[0] = (uint8_t)(o->break_off.frame | 0x80);
    o->sprite_turn[1] = o->sprite_turn[0];
    if (o->break_off.frame == o->break_off.last_frame) {
        ObjectRelease(o);
        return;
    }
    o->break_off.frame++;
    ObjectApplyVelocity(o);
    o->timer = 3;
}

/* The fall: count 0, end `end`, +$25 bit 0 unless 6 frames */
static void piece_handler(Object *o, uint8_t end)
{
    ObjectSetHandler(o, H_PIECE_FALL);
    o->break_off.frame = 0;
    if (end == 0x06) {
        o->break_off.last_frame = 0x06;
        return;
    }
    o->break_off.state |= PIECE_LONG;
    o->break_off.last_frame = end;
}

static void piece_look(Object *o, uint16_t look, int adv, uint8_t f5)
{
    o->anim = look;
    if (adv)
        ObjectLookForSplit(o);
    o->draw_flags |= 0x08;
    o->material = 0;
    o->size = f5;
}

static void piece_drop(Object *o) { o->y = (uint16_t)(o->y + 0x0028); }

static void piece_fall4(Object *o)
{
    piece_handler(o, 0x04);
    ObjectPieceFall(o);
}

/* @routine 08:D1F7 ObjectBreakOffA: a child broken off, look $998D */
void ObjectBreakOffA(Object *o)
{
    piece_look(o, look_word(LOOK_BREAK_OFF_A), 0, 0x14);
    ObjectVelocityFromRecordScaled(o);
    piece_drop(o);
    piece_fall4(o);
}

/* @routine 08:D249 ObjectBreakOffB */
void ObjectBreakOffB(Object *o)
{
    piece_look(o, look_word(LOOK_BREAK_OFF_B), 0, 0x14);
    ObjectVelocityFromRecord(o);
    piece_drop(o);
    piece_handler(o, 0x03);
    ObjectPieceFall(o);
}

/* @routine 08:D2CF ObjectBreakOffD */
void ObjectBreakOffD(Object *o)
{
    piece_look(o, look_word(LOOK_BREAK_OFF_D), 1, 0x14);
    ObjectVelocityFromRecord(o);
    piece_drop(o);
    piece_fall4(o);
}

/* @routine 08:D303 ObjectBreakOffE */
void ObjectBreakOffE(Object *o)
{
    piece_look(o, look_word(LOOK_BREAK_OFF_E), 1, 0x14);
    ObjectVelocityFromRecord(o);
    piece_drop(o);
    piece_fall4(o);
}

static void break_off_f(Object *o, uint8_t f5)
{
    o->anim = look_word(LOOK_BREAK_OFF_E_FLAT);
    ObjectLookForSplit(o);
    o->draw_flags |= 0x08;
    o->material = 0;
    o->size = f5;
    piece_drop(o);
    piece_fall4(o);
}

/*
 * @routine 08:D337 ObjectBreakOffContact
 * WHAT IT DOES: A piece hit on contact.
 */
void ObjectBreakOffContact(Object *o)
{
    break_off_f(o, 0x6E);
}

/* @routine 08:D33F ObjectBreakOffF: +$5F = half the parent's record speed + $14, at most $78 */
void ObjectBreakOffF(Object *o)
{
    uint16_t size = (uint16_t)(asr1(abs16(rec_get(obj_at(o->break_off.car)->record, FXREC_SPEED))) + 0x0014);

    if (!below(size, 0x0078))
        size = 0x0078;
    ObjectVelocityFromRecord(o);
    break_off_f(o, (uint8_t)size);
}

/* @routine 08:D39F ObjectBreakOffG */
void ObjectBreakOffG(Object *o)
{
    piece_look(o, look_word(LOOK_BREAK_OFF_G), 1, 0x14);
    piece_drop(o);
    ObjectVelocityFromRecordScaled(o);
    piece_fall4(o);
}

/* @routine 08:D3D3 ObjectBreakOffH */
void ObjectBreakOffH(Object *o)
{
    piece_look(o, look_word(LOOK_BREAK_OFF_H), 1, 0x0A);
    piece_drop(o);
    ObjectVelocityFromRecordScaled(o);
    piece_fall4(o);
}

/*
 * @routine 08:D460 ObjectPieceSpray
 * WHAT IT DOES: A spray piece's first frame: falls for 6 frames; its offset
 * (0, 0, random -$28 to -$19) turned by its parent's angles and doubled
 * added to its place; its parent's record velocity scaled, Y - $14.
 */
void ObjectPieceSpray(Object *o)
{
    Object *p;
    uint16_t dz;
    Vec16 v;

    piece_handler(o, 0x06);
    dz = (uint16_t)((RandomStep() & 0x0F) + 0xFFD8);
    piece_look(o, look_word(LOOK_STAR_SPARKLE), 0, 0x0A);
    p = obj_at(o->break_off.car);
    RotateXZFrom08(0, (int8_t)(uint8_t)dz, p->pitch, &v.y, &v.z);
    RotateXZ(0, (int8_t)v.z, p->heading, &v.x, &v.z);
    o->x = (uint16_t)(o->x + (uint16_t)((uint16_t)v.x << 1));
    o->z = (uint16_t)(o->z + (uint16_t)((uint16_t)v.z << 1));
    o->y = (uint16_t)(o->y + (uint16_t)((uint16_t)v.y << 1));
    ObjectVelocityFromRecordScaled(o);
    o->vy = (uint16_t)(o->vy + 0xFFEC);
    ObjectLookForSplit(o);
    ObjectPieceFall(o);
}

/* hidden, +$36 = 3 */
static void attach_hide(Object *o)
{
    o->draw_flags |= 0x20;
    o->timer = 3;
}

/* whether the parent is gone or loose: then the object hides */
static int attach_parent_gone(Object *o)
{
    const Object *p = obj_at(o->parent);

    if ((p->draw_flags & 0x08) || (p->car.state & CAR_LOOSE)) {
        attach_hide(o);
        return 1;
    }
    return 0;
}

/* The object's offset (its velocity bytes) turned by the parent's angles,
   shifted left sh times and added to the parent's place into the object's.
   Without the pitch turn when with14 is 0. */
static void offset_on_parent(Object *o, const Object *p, int sh, int with14)
{
    Vec16 v = {sx8((uint8_t)o->attached.on_x), sx8((uint8_t)o->attached.on_y), sx8((uint8_t)o->attached.on_z)};

    RotateXY((int8_t)v.x, (int8_t)v.y, p->roll, &v.x, &v.y);
    if (with14)
        RotateXZFrom08((int8_t)v.y, (int8_t)v.z, p->pitch, &v.y, &v.z);
    RotateXZ((int8_t)v.x, (int8_t)v.z, p->heading, &v.x, &v.z);
    o->x = (uint16_t)(p->x + (uint16_t)((uint16_t)v.x << sh));
    o->z = (uint16_t)(p->z + (uint16_t)((uint16_t)v.z << sh));
    o->y = (uint16_t)(p->y + (uint16_t)((uint16_t)v.y << sh));
}

/*
 * @routine 08:DCB4 ObjectAttachSpawn
 * WHAT IT DOES: Gives the object the attachments listed for its entry
 * ($70:2392 indexes the list at $09:8090: a count, then five bytes each):
 * each a new object with the object as its +$06 and its model, look,
 * handler and velocity from the table.
 */
void ObjectAttachSpawn(Object *o)
{
    uint16_t i = fxram_get(FX_CAR_ATTACHMENTS);
    unsigned n = object_list_byte(i);

    i = (uint16_t)(i + 1);
    for (; n; n--, i = (uint16_t)(i + 5)) {
        Object *a = ObjectSpawn(model_word(MODEL_NONE));
        uint16_t k, look;

        if (!a)
            continue;
        a->parent = obj_ref(o);
        k = object_list_word(i);
        a->model = object_entry_word(k, DATA_MODEL);
        look = object_entry_word(k, DATA_LOOK);
        a->handler = object_entry_word(k, DATA_HANDLER);
        a->handler_bank = (uint8_t)object_entry_word(k, DATA_HANDLER_BANK);
        if (look) {
            a->draw_flags |= 0x08;
            a->material = 0;
            a->size = (uint8_t)object_entry_word(k, DATA_SIZE);
            a->anim = look;
        }
        a->attached.on_x = (uint16_t)sx8(object_list_byte((uint16_t)(i + 2)));
        a->attached.on_y = (uint16_t)sx8(object_list_byte((uint16_t)(i + 3)));
        a->attached.on_z = (uint16_t)sx8(object_list_byte((uint16_t)(i + 4)));
    }
}

/*
 * @routine 08:DDB4 ObjectAttachFollow
 * WHAT IT DOES: An attachment's frame: hidden when its parent is gone or its
 * record is flagged; otherwise on the parent's record's place and heading,
 * moved by its velocity, rebucketed; bit 6 of +$24 follows the parent's.
 */
void ObjectAttachFollow(Object *o)
{
    const Object *p;
    uint16_t rec;

    if (attach_parent_gone(o))
        return;
    p = obj_at(o->parent);
    rec = p->record;
    if (rec_get(rec, FXREC_FLAGS1C) & 0x0010) {
        attach_hide(o);
        return;
    }
    o->x = rec_get(rec, FXREC_POS_X);
    o->z = rec_get(rec, FXREC_POS_Z);
    o->y = rec_get(rec, FXREC_Y_HELD);
    o->pitch = fx_ram[(uint16_t)(rec + FXREC_PITCH + 1)];
    o->heading = fx_ram[(uint16_t)(rec + FXREC_HEADING + 1)];
    ObjectApplyVelocity(o);
    ObjectRebucket(o);
    o->draw_flags &= 0xDF;
    o->draw_flags &= 0xBF;
    if (obj_at(o->parent)->draw_flags & 0x40) {
        o->draw_flags |= 0x40;
        o->timer = 3;
    }
}

/*
 * @routine 08:DD8D ObjectAttachStart
 * WHAT IT DOES: An attachment's first frame: its Y velocity's low byte
 * sign-extended and doubled, then its frame.
 */
void ObjectAttachStart(Object *o)
{
    ObjectSetHandler(o, H_ATTACH_FOLLOW);
    o->vy = (uint16_t)((uint16_t)sx8((uint8_t)o->vy) << 1);
    ObjectAttachFollow(o);
}

/*
 * @routine 08:DF4C ObjectAttachTurned
 * WHAT IT DOES: An attachment turned with its parent: hidden when the
 * parent is gone; otherwise the parent's heading plus its record's turn,
 * placed by its offset times 8.
 */
void ObjectAttachTurned(Object *o)
{
    Object *p;

    if (attach_parent_gone(o))
        return;
    p = obj_at(o->parent);
    o->heading = (uint8_t)(fx_ram[(uint16_t)(p->record + FXCAR_STEER_BIAS + 1)] + p->heading);
    o->pitch = p->pitch;
    o->roll = p->roll;
    offset_on_parent(o, p, 3, 1);
    ObjectRebucket(o);
    o->timer = 3;
}

static void spark(Object *o, uint16_t vx, uint16_t vz)
{
    Object *n = ObjectSpawn(model_word(MODEL_TRUCK_WHEEL));

    if (!n)
        return;
    n->parent = obj_ref(o);
    ObjectSetHandler(n, H_SPARK_START);
    n->wheel.on_x = vx;
    n->wheel.on_y = 0x000A;
    n->wheel.on_z = vz;
}

/*
 * @routine 08:DE31 ObjectSparksStart
 * WHAT IT DOES: Gives the object four sparks, then ObjectAttachTurned.
 */
void ObjectSparksStart(Object *o)
{
    ObjectSetHandler(o, H_ATTACH_TURNED);
    spark(o, 0xFFE2, 0x003C);
    spark(o, 0x001E, 0x003C);
    spark(o, 0xFFE2, 0xFFEC);
    spark(o, 0x001E, 0xFFEC);
    ObjectAttachTurned(o);
}

/*
 * @routine 08:E073 ObjectSparkStep
 * WHAT IT DOES: A spark's frame: on its parent plus its offset times 8; the
 * parent's angles; tilted to the camera; rebucketed; +$36 = 3.
 */
void ObjectSparkStep(Object *o)
{
    Object *p = obj_at(o->parent);

    offset_on_parent(o, p, 3, 1);
    ObjectCopyAnglesFrom(o, p);
    ObjectBillboardTilt(o);
    ObjectRebucket(o);
    o->timer = 3;
}

/*
 * @routine 08:E048 ObjectSparkStart
 * WHAT IT DOES: A spark's first frame.
 */
void ObjectSparkStart(Object *o)
{
    o->draw_flags |= 0x08;
    o->material = 0;
    o->size = 0x6E;
    ObjectSetHandler(o, H_SPARK_STEP);
    o->wheel.look = look_word(LOOK_WHEEL);
    ObjectSparkStep(o);
}

/*
 * @routine 08:E1C7 ObjectCarAttachedStep
 * WHAT IT DOES: A car carried on another: rebucketed, posed, marked.
 */
void ObjectCarAttachedStep(Object *o)
{
    ObjectRebucket(o);
    ObjectPoseFromRecord(o);
    GsuRecordKeep(o);
}

/*
 * @routine 08:E158 ObjectCarAttachedStart
 * WHAT IT DOES: A car carried on its parent: at the parent's place, $640
 * back and $1E0 up, a car record marked carried, each record's +$8E naming
 * the other's record; then its frame.
 */
void ObjectCarAttachedStart(Object *o)
{
    Object *p = obj_at(o->parent);

    ObjectCopyPosition(o, p);
    o->z = (uint16_t)(o->z - 0x0640);
    o->y = (uint16_t)(o->y + 0xFE20);
    ObjectCarRecordNew(o, 0);
    rec_put(o->record, FXCAR_PARTNER, p->record);
    rec_or(o->record, FXREC_FLAGS1C, 0x0008);
    rec_put(p->record, FXCAR_PARTNER, o->record);
    ObjectSetHandler(o, H_CAR_ATTACHED_STEP);
    ObjectCarAttachedStep(o);
}

/* @routine 08:E426 ObjectParentRecordBit8 */
static int ObjectParentRecordBit8(const Object *o)
{
    return (rec_get(obj_at(o->parent)->record, FXREC_FLAGS40) & 0x0100) != 0;
}

/*
 * @routine 08:E244 ObjectExhaustStep
 * WHAT IT DOES: An exhaust flame's frame by its state: hidden while the
 * parent is gone or loose; state 1 watches for a gear change or a drop in
 * the parent's segment, state 2 and 3 show their frame for 8 frames; every
 * fourth frame it is placed at the parent's place, moved by its velocity.
 */
void ObjectExhaustStep(Object *o)
{
    Object *p;
    uint8_t v;
    int clear;

    if (o->rank_sign.settle) {
        o->rank_sign.settle--;
        o->rank_sign.rank = (uint8_t)GsuRecordParentRank(o);
    }
    p = obj_at(o->parent);
    if ((p->car.state & CAR_EVENT_TAKEN) || (p->car.state & CAR_WRECKED) || ObjectParentRecordBit8(o)) {
        attach_hide(o);
        return;
    }
    if (o->rank_sign.state == 0x01) {
        v = (uint8_t)GsuRecordParentRank(o);
        if (v != o->rank_sign.rank) {
            o->rank_sign.rank = v;
            o->rank_sign.state = 0x02;
            o->rank_sign.shown = 0x08;
        }
        v = (uint8_t)GsuRecordParentSegment(o, &clear);
        {
            uint8_t w = (uint8_t)(v - o->rank_sign.segment);
            int small_drop = !clear && (w & 0x80) && ((uint8_t)(0 - w - 0x02) & 0x80);

            if (!clear && (w & 0x80) && !small_drop && ((uint8_t)(0 - w - 0x0A) & 0x80)) {
                o->rank_sign.state = 0x03;
                o->rank_sign.shown = 0x08;
                SoundPort0QueuePush(0x3B);
            }
            if (!small_drop)
                o->rank_sign.segment = v;
        }
    }
    if (o->rank_sign.state == 0x02)
        o->sprite_turn[0] = (uint8_t)((uint8_t)GsuRecordParentRank(o) | 0x80);
    if (o->rank_sign.state == 0x03) {
        o->sprite_turn[0] = 0x84;
        o->rank_sign.segment = (uint8_t)GsuRecordParentSegment(o, &clear);
    }
    v = o->rank_sign.shown;
    if (!v) {
        attach_hide(o);
        return;
    }
    v--;
    if (!v) {
        o->rank_sign.shown = 0;
        o->rank_sign.state = 0x01;
        attach_hide(o);
        return;
    }
    o->rank_sign.shown = v;
    if (!(v & 0x03)) {
        attach_hide(o);
        return;
    }
    o->draw_flags &= 0xDF;
    p = obj_at(o->parent);
    ObjectCopyPosition(o, p);
    ObjectApplyVelocity(o);
    o->draw_flags &= 0x7F;
    {
        const Camera *cam = camera_of(obj_at(p->link)->link);

        if (cam->mode & 0x4000) {
            o->x = 0x0000;
            o->y = 0xFF9C;
            o->z = 0x01F4;
            o->draw_flags |= 0x80;
            o->x = (uint16_t)(o->x + cam->x);
            o->y = (uint16_t)(o->y + cam->y);
            o->z = (uint16_t)(o->z + cam->z);
            ObjectRebucket(o);
            o->x = (uint16_t)(o->x - cam->x);
            o->y = (uint16_t)(o->y - cam->y);
            o->z = (uint16_t)(o->z - cam->z);
        } else {
            ObjectRebucket(o);
        }
    }
    o->timer = 3;
}

/*
 * @routine 08:E1D2 ObjectExhaustStart
 * WHAT IT DOES: An exhaust flame's first frame: released when its parent
 * cannot show one, or the course or HUD mode forbids; otherwise its state,
 * and its first step.
 */
void ObjectExhaustStart(Object *o)
{
    if (!(rec_get(obj_at(o->parent)->record, FXREC_DIRECTION) & 0x0008)
        && (fxram_get(FX_COURSE_FLAGS) & 0x0100) && g.hud_mode != 1 && g.hud_mode != 2 && g.hud_mode != 3) {
        int clear;

        ObjectSetHandler(o, H_EXHAUST_STEP);
        o->rank_sign.segment = (uint8_t)GsuRecordParentSegment(o, &clear);
        o->rank_sign.shown = 0;
        o->vy = (uint16_t)((uint16_t)sx8((uint8_t)o->vy) << 1);
        o->rank_sign.settle = 0x0A;
        o->rank_sign.state = 0x01;
        ObjectExhaustStep(o);
        return;
    }
    ObjectRelease(o);
}

/* A latch bit in the player's flags and its sound: when the bit is not as
   wanted, the sound and the bit changed */
static void latch(uint16_t bit, int on, void (*snd)(void))
{
    uint16_t *w = &g.player_flags[g.current_player ? 1 : 0];

    if (((*w & bit) != 0) == on)
        return;
    snd();
    if (on)
        *w |= bit;
    else
        *w = (uint16_t)(*w & ~bit);
}

/*
 * @routine 08:C124 PlayerCarEvents
 * WHAT IT DOES: The player car's per-frame events from its record: engine
 * and landing sounds, a hit's sparks, a scrape's sounds, a crash, trails,
 * the boost, and its +$24 flags at the end.
 */
void PlayerCarEvents(Object *o)
{
    uint16_t rec = o->record, w;
    uint8_t v;
    int found;

    if (!(o->car.state & CAR_WRECKED)) {
        if (!(rec_get(rec, FXREC_FLAGS40) & 0x0100)) {
            if (g.course_flags & 0x0020) {
                uint16_t r = rec_get(rec, FXCAR_PARTS);

                found = 0;
                do {
                    if ((rec_get(r, FXREC_CONTACT_FLAGS) & 0x0080) && (rec_get(r, FXPART_FLAGS) & 0x0001) && (rec_get(r, FXPART_FLAGS) & 0x0010))
                        found = 1;
                    r = rec_get(r, FXPART_NEXT);
                } while (r);
                if (found) {
                    if (!g.split_screen)
                        fxram_put(FX_EFFECT_MAX, 0x0010);
                    SoundAtRateForLevel(abs16(rec_get(rec, FXREC_SPEED)));
                }
            } else {
                if (rec_get(rec, FXCAR_PART_CONTACTS) & 0x0020)
                    latch(0x0004, 1, PlayerSound12);
                else
                    latch(0x0004, 0, PlayerSound0C);
            }
        }
        if (rec_get(rec, FXREC_SIDES) & 0x0400)
            latch(0x0001, 1, PlayerSound23);
        else
            latch(0x0001, 0, PlayerSound24);
        found = (rec_get(rec, FXREC_SIDES) & 0x2000) && (rec_get(rec, FXREC_FLAGS40) & 0x0800)
                && !(rec_get(rec, FXREC_FLAGS40) & 0x0100);
        latch(0x0002, found, found ? PlayerSound0D : PlayerSound0B);
        v = fx_ram[(uint16_t)(rec + FXCAR_SLIDE + 1)];
        if (!(v & 0x80)) {
            if (v < 0x01)
                PlayerStopHeldSound();
            else if (v < 0x02)
                PlayerHoldSound32();
            else if (v < 0x04)
                PlayerHoldSound33();
            else if (v < 0x05)
                PlayerHoldSound34();
            else if (v < 0x64)
                PlayerHoldSound35();
        }
        w = rec_get(rec, FXCAR_PUSH_TOTAL);
        if (w && !(w & 0x8000) && !below(w, 0x0032)) {
            if (below(w, 0x01F4))
                SoundPort0QueuePush(0x54);
            else if (below(w, 0x0320))
                SoundPort0QueuePush(0x55);
            else if (below(w, 0x044C))
                SoundPort0QueuePush(0x56);
            else if (below(w, 0x07D0))
                SoundPort0QueuePush(0x57);
            ObjectSpawnSparklePair(o);
        }
        w = rec_get(rec, FXREC_CONTACT_FLAGS);
        if (w & 0x0100)
            SoundPort0QueuePush(0x0A);
        else if (w & 0x0200)
            SoundPort0QueuePush(0x0D);
        w = rec_get(rec, FXCAR_IMPACT);
        if (w & 0x8000) {
            w = (uint16_t)-w;
            if (!below(w, 0x0041)) {
                if (below(w, 0x0050))
                    SoundPort0QueuePush(0x16);
                else if (below(w, 0x005A))
                    SoundPort0QueuePush(0x17);
                else if (below(w, 0x006E))
                    SoundPort0QueuePush(0x18);
                else if (below(w, 0x03E8))
                    SoundPort0QueuePush(0x19);
            }
            if (!below(w, 0x0046)) {
                o->car.state |= CAR_JOLTED;
                ObjectSpawnAttached(o, 0x0A, 0x00);
            }
        }
        w = rec_get(rec, FXREC_FLAGS40);
        if (w & 0x0001) {
            if (!CarCrashParts(o, 0x01)) {
                o->car.state |= CAR_WRECKED;
                if (!g.split_screen)
                    ObjectBurstSpawn(o);
            }
        } else if (w & 0x0040) {
            o->car.state |= CAR_JOLTED;
            ObjectSpawnSparklePair(o);
            CarCrashParts(o, 0x00);
        } else if (w & 0x0020) {
            CarCrashParts(o, 0x01);
            if (!g.split_screen)
                ObjectBurstSpawn(o);
        }
        if (!(o->draw_flags & 0x40)) {
            if (rec_get(rec, FXREC_FLAGS40) & 0x0002) {
                ObjectSpawnTrail(o);
                ObjectPieceSprayUnlessSplit(o);
            } else if ((rec_get(rec, FXREC_SIDES) & 0x0400) && RandomStep() >= 0xF2) {
                ObjectSpawnTrail(o);
            }
            if (!below(rec_get(rec, FXREC_DAMAGE), 0x0199)) {
                uint16_t *f = &g.player_flags[g.current_player ? 1 : 0];

                if (!(*f & 0x0020)) {
                    g.save_bad_flag = 0x11;
                    SoundPort0QueuePush(0x3D);
                    *f |= 0x0020;
                }
                if (!(o->car.state & CAR_LOOSE) && RandomStep() >= 0x4C)
                    ObjectPieceSprayUnlessSplit(o);
            }
        }
    }
    if (o->car.state & (CAR_LOOSE | CAR_DETACHING))
        o->draw_flags |= 0x20;
    else
        o->draw_flags &= 0xDF;
    if (camera_of(obj_at(o->link)->link)->mode & 0x4000)
        o->draw_flags |= 0x40;
    else
        o->draw_flags &= 0xBF;
}

/*
 * @routine 08:E443 ObjectOffsetOnParent2
 * WHAT IT DOES: An attachment placed at its offset turned by its parent's
 * angles and doubled; hidden when the parent is hidden.
 */
void ObjectOffsetOnParent2(Object *o)
{
    Object *p = obj_at(o->parent);

    if ((p->draw_flags & 0x08) || (p->draw_flags & 0x20)) {
        attach_hide(o);
        return;
    }
    o->sprite_turn[0] = 0x80;
    offset_on_parent(o, p, 1, 1);
    o->draw_flags &= 0xDF;
    ObjectRebucket(o);
}

/* On its parent, rebucketed, +$24 bits 5 and 6 cleared, bit 6 again when
   the parent has it; +$36 = 3. */
static void lamp_place(Object *o)
{
    Object *p = obj_at(o->parent);

    ObjectPlaceOnParent(p, o);
    ObjectRebucket(o);
    o->draw_flags &= 0xDF;
    o->draw_flags &= 0xBF;
    p = obj_at(o->parent);
    if (p->draw_flags & 0x40)
        o->draw_flags |= 0x40;
    o->timer = 3;
}

/* with the parent's +$25 bit 4 released; otherwise the drop */
static void lamp_drop_start(Object *o)
{
    if (obj_at(o->parent)->car.state & CAR_WRECKED) {
        ObjectRelease(o);
        return;
    }
    ObjectSetHandler(o, H_LAMP_DROP);
    o->eye.size_count = 0;
    ObjectLampDrop(o);
}

/*
 * @routine 08:E653 ObjectLampOnParent
 * WHAT IT DOES: A lamp's frame on its parent: hidden when the parent is
 * gone, drops when loose, bounces, or placed on the parent.
 */
void ObjectLampOnParent(Object *o)
{
    Object *p = obj_at(o->parent);

    if (p->draw_flags & 0x08) {
        attach_hide(o);
    } else if (p->car.state & CAR_LOOSE) {
        lamp_drop_start(o);
    } else if (p->car.state & CAR_JOLTED) {
        ObjectLampDropBStart(o);
    } else if (p->car.state & CAR_STARTLED) {
        o->anim = look_word(LOOK_EYE_STARTLED);
        lamp_drop_start(o);
    } else {
        lamp_place(o);
    }
}

/* one step of a lamp's look: unless its count is 4 */
static void lamp_look_step(Object *o, EyeBlink blink)
{
    if (o->eye.blink == 0x04)
        return;
    o->eye.blink++;
    o->anim = eye_blink_look(blink, o->eye.blink);
}

/*
 * @routine 08:E51E ObjectLampAnimate
 * WHAT IT DOES: A lamp's look by the race's light bits, then its frame on
 * the parent.
 */
void ObjectLampAnimate(Object *o)
{
    switch (g.lamp_lights & 0x1C) {
    case 0x08: lamp_look_step(o, EYE_BLINK_TO_STARTLED); break;
    case 0x04: lamp_look_step(o, EYE_BLINK_PLAIN); break;
    case 0x0C: lamp_look_step(o, EYE_BLINK_TO_PLAIN); break;
    case 0x1C: lamp_look_step(o, EYE_BLINK_STARTLED); break;
    case 0x10: o->anim = look_word(LOOK_EYE_PLAIN); break;
    default: o->eye.blink = 0; break;
    }
    ObjectLampOnParent(o);
}

/*
 * @routine 08:E61C ObjectLampWait
 * WHAT IT DOES: A lamp waiting for the lights: then animating, or on its
 * parent.
 */
void ObjectLampWait(Object *o)
{
    if (g.lamp_lights & 0x20) {
        o->eye.blink = 0;
        ObjectSetHandler(o, H_LAMP_ANIMATE);
        ObjectLampAnimate(o);
        return;
    }
    o->anim = look_word(LOOK_EYE_PLAIN);
    ObjectSetHandler(o, H_LAMP_ON_PARENT);
    ObjectLampOnParent(o);
}

/*
 * @routine 08:E613 ObjectLampStart
 * WHAT IT DOES: A lamp's first frame.
 */
void ObjectLampStart(Object *o)
{
    o->list_kind = obj_at(o->parent)->list_kind;
    ObjectLampWait(o);
}

/*
 * @routine 08:E6DE ObjectLampDrop
 * WHAT IT DOES: A dropping lamp: its count stepped; when done back to
 * waiting; placed on its parent either way.
 */
void ObjectLampDrop(Object *o)
{
    o->eye.turn = (uint8_t)(o->eye.turn + 0x10);
    if (ObjectSizeRiseFall(o, 0x08, 0x10, 0x05)) {
        o->eye.turn = 0;
        ObjectSetHandler(o, H_LAMP_WAIT);
    }
    lamp_place(o);
}

/*
 * @routine 08:E72C ObjectLampDropB
 * WHAT IT DOES: A bouncing lamp: its count stepped; when done back to
 * waiting.
 */
void ObjectLampDropB(Object *o)
{
    if (ObjectSizeRiseFall(o, 0x03, 0x06, 0x0A)) {
        o->eye.size_count = 0;
        ObjectSetHandler(o, H_LAMP_WAIT);
        o->eye.state &= (uint8_t)~EYE_BOUNCING;
    }
    lamp_place(o);
}

/* @routine 08:E712 ObjectLampDropBStart */
void ObjectLampDropBStart(Object *o)
{
    ObjectSetHandler(o, H_LAMP_DROP_B);
    o->eye.size_count = 0;
    o->eye.state |= EYE_BOUNCING;
    ObjectLampDropB(o);
}

/*
 * @routine 08:9A4F CameraPointFromTable
 * WHAT IT DOES: Steps the object's camera point: its index in the course's
 * list moves on a cue; the point's mode and target offset from the list's
 * entry; the point moves a quarter of the way to the target.
 */
void CameraPointFromTable(Object *o)
{
    Camera *c = camera_of(o->link);
    uint16_t point, off[3];
    int moved;

    if (!(c->mode & 0x0020) && (o->director.state & DIRECTOR_CUE)) {
        uint8_t count = object_list_byte(c->word32);
        uint8_t back = object_list_byte((uint16_t)(c->word32 + 1));
        uint8_t v;

        o->director.state &= (uint8_t)~DIRECTOR_CUE;
        SoundPort0QueuePush(0x24);
        if ((o->director.state & DIRECTOR_BACK) && (o->director.state &= (uint8_t)~DIRECTOR_BACK, back)) {
            v = (uint8_t)(back + count - 1);
            if (v == c->byte34)
                v = 0;
        } else {
            v = (uint8_t)(c->byte34 + 1);
            if (!((uint8_t)(v - count) & 0x80))
                v = 0;
        }
        c->byte34 = v;
        o->director.steps = 0x0007;
    }
    if (obj_at(o->parent)->car.state & CAR_LOOSE) {
        o->director.steps = 0x0007;
        point = 0x0000;
    } else {
        point = (uint16_t)((uint16_t)(c->byte34 << 3) + c->word32);
    }
    point = (uint16_t)(point + 2);
    c->mode = object_list_word(point);
    off[0] = object_list_word((uint16_t)(point + 2));
    off[1] = object_list_word((uint16_t)(point + 4));
    off[2] = object_list_word((uint16_t)(point + 6));
    if (c->mode & 0x0080) {
        if (o->director.steps) {
            o->director.steps--;
            c->mode |= 0x8201;
            off[0] = 0x0000;
            off[1] = 0x0000;
            off[2] = 0xFFCE;
        } else {
            c->mode |= 0x4000;
        }
    }
    for (int i = 0; i < 3; i++)
        off[i] = (uint16_t)(off[i] + c->turn_words[i] + c->turn_words[3 + i]);
    for (int i = 0; i < 3; i++)
        c->place[i] = ApproachQuarterMin4(c->place[i], off[i], &moved);
    for (int i = 0; i < 3; i++)
        c->place_base[i] = off[i];
}

static const uint16_t ring_vx[4] = {0xFF90, 0xFFDB, 0x0025, 0x0070};

/* The ring's Y, its parts, spin or idle. Returns 1 if the ring spins. */
static int ring_parts(Object *o)
{
    uint16_t rec = fxram_get(FXIN_PLAYER1_RECORD);
    unsigned made = 0;

    if (!rec)                           /* the original then runs off an 8-bit store as 16 bits */
        return 0;
    o->y = (uint16_t)(rec_get(rec, FXREC_POS_Y) + 0xE4A8);
    o->ring.target_y = (uint16_t)(rec_get(rec, FXREC_POS_Y) + 0xFE0C);
    for (int i = 0; i < 4; i++) {
        Object *n = ObjectSpawn(ring_part_model_word(i));

        if (!n)
            continue;
        ObjectSetHandler(n, H_RING_PART_START);
        n->ring_part.on_x = ring_vx[i];
        n->parent = obj_ref(o);
        made++;
    }
    if (made != 4) {
        ObjectSetHandler(o, H_RING_IDLE);
        return 0;
    }
    o->roll = 0x40;
    o->heading = (uint8_t)(o->heading + 0x20);
    ObjectSetHandler(o, H_RING_SPIN);
    return 1;
}

/* In phase 8 the ring is made; then its timer. */
static void ring_spawn(Object *o)
{
    if (g.player_phase[0] == 0x08 && !ring_parts(o))
        return;
    o->timer = 3;
}

static void ring_start(Object *o, uint8_t heading, uint16_t vx, uint16_t vy, uint16_t vz)
{
    o->heading = heading;
    o->vx = vx;
    o->vy = vy;
    o->vz = vz;
    ring_spawn(o);
}

/* @routine 08:9BD9 ObjectRingStartZ */
void ObjectRingStartZ(Object *o) { ring_start(o, 0x00, 0x0000, 0x0000, 0xFDA8); }
/* @routine 08:9BF7 ObjectRingStartX */
void ObjectRingStartX(Object *o) { ring_start(o, 0xC0, 0xFDA8, 0x0000, 0x0000); }
/* @routine 08:9C15 ObjectRingStartBackZ */
void ObjectRingStartBackZ(Object *o) { ring_start(o, 0x80, 0x0000, 0x0000, 0x0258); }
/* @routine 08:9C33 ObjectRingStartBackX */
void ObjectRingStartBackX(Object *o) { ring_start(o, 0x40, 0x0258, 0x0000, 0x0000); }

/* @routine 08:9DAE ObjectRingIdle: a ring whose parts were not all made */
void ObjectRingIdle(Object *o)
{
    ObjectSetHandler(o, H_RING_IDLE);
}

/*
 * @routine 08:9DBE ObjectRingSpin
 * WHAT IT DOES: A ring's frame: moved, its Y a quarter of the way to +$27,
 * it spins; rebucketed; +$36 = 3.
 */
void ObjectRingSpin(Object *o)
{
    int moved;

    ObjectApplyVelocity(o);
    o->y = ApproachQuarterMin4(o->y, o->ring.target_y, &moved);
    o->roll = (uint8_t)(o->roll + 0xFA);
    o->heading = (uint8_t)(o->heading + 0xFF);
    ObjectRebucket(o);
    o->timer = 3;
}

/*
 * @routine 08:9E05 ObjectRingPartStep
 * WHAT IT DOES: A ring part's frame: on its parent at its offset turned by
 * the parent's roll and heading, times 16; released when its count runs
 * out.
 */
void ObjectRingPartStep(Object *o)
{
    Object *p = obj_at(o->parent);

    offset_on_parent(o, p, 4, 0);
    o->roll = p->roll;
    o->heading = p->heading;
    ObjectRebucket(o);
    o->timer = 3;
    if (!o->ring_part.frames) {
        ObjectRelease(o);
        return;
    }
    o->ring_part.frames--;
}

/*
 * @routine 08:9DF1 ObjectRingPartStart
 * WHAT IT DOES: A ring part's first frame.
 */
void ObjectRingPartStart(Object *o)
{
    o->ring_part.frames = 0x28;
    ObjectSetHandler(o, H_RING_PART_STEP);
    ObjectRingPartStep(o);
}

/*
 * @routine 08:9FB2 ObjectDemoCarStep
 * WHAT IT DOES: A demo car's frame: its look by kind; while its count runs
 * it counts down, at 0 its record is flagged; rebucketed, posed, marked.
 */
void ObjectDemoCarStep(Object *o)
{
    ObjectCarLookByKind(o, (uint8_t)g.selection_out[0]);
    if (o->car.wait)
        o->car.wait--;
    else
        rec_or(o->record, FXREC_FLAGS1C, 0x0010);
    ObjectRebucket(o);
    ObjectPoseFromRecord(o);
    o->timer = 3;
    GsuRecordKeep(o);
}

/*
 * @routine 08:9F3B ObjectDemoCarStart
 * WHAT IT DOES: A demo car's first frame: a car record, the camera on it,
 * its number kept; then its first step.
 */
void ObjectDemoCarStart(Object *o)
{
    Camera *c = &cameras[0];

    ObjectCarRecordNew(o, 4);
    if (o->record) {
        rec_or(o->record, FXREC_DIRECTION, 0x0008);
        rec_or(o->record, FXREC_FLAGS16, 0x0080);
    }
    c->object = obj_ref(o);
    c->place[0] = 0x0000;
    c->place[1] = 0xFF38;
    c->place[2] = 0x0258;
    c->mode = (uint16_t)((c->mode & 0xFF00) | 0x01);
    ObjectSetHandler(o, H_DEMO_CAR_STEP);
    {
        unsigned n = g.demo_car_count & 0x7Fu;

        /* the original's table runs on past four into $1174; RaceStart
           clears the count and the menus read four */
        if (n < 4)
            g.demo_cars[n] = obj_ref(o);
        else
            fprintf(stderr, "ObjectDemoCarStart: a fifth demo car (%u)\n", n);
    }
    g.demo_car_count++;
    o->car.wait = 0x04;
    ObjectDemoCarStep(o);
}

/*
 * @routine 08:89E6 CameraApproachTarget
 * WHAT IT DOES: Moves the camera point an eighth of the way to its target;
 * while the object's count runs, the target moves by the object's velocity
 * and it returns 0; otherwise the object's next path command, whose 1
 * ends the path.
 */
int CameraApproachTarget(Object *o)
{
    Camera *c = camera_of(o->link);
    int moved;

    for (int i = 0; i < 3; i++)
        c->place[i] = ApproachEighthMin8(c->place[i], c->place_base[i], &moved);
    if (o->director.steps) {
        o->director.steps--;
        c->place_base[0] = (uint16_t)(c->place_base[0] + o->vx);
        c->place_base[1] = (uint16_t)(c->place_base[1] + o->vy);
        c->place_base[2] = (uint16_t)(c->place_base[2] + o->vz);
        return 0;
    }
    return ObjectPathCommand(o);
}

/*
 * @routine 08:958F CameraFollowLink
 * WHAT IT DOES: Steps the object's camera point: it follows its target and
 * faces it.
 */
void CameraFollowLink(Object *o)
{
    Camera *c = camera_of(o->link);

    CameraFollowStep(c);
    ObjectFaceTied(c);
}

static void director_tail(Object *o)
{
    CameraFollowLink(o);
    o->timer = 3;
}

static void director_mode(Object *o, uint16_t mode)
{
    camera_of(o->link)->mode = mode;
}

/* the entry of the course's camera table the parent's record is on, n on */
static uint16_t director_entry(const Object *o, unsigned n)
{
    uint16_t e = rec_get(obj_at(o->parent)->record, FXREC_SEGMENT);

    while (n--)
        e = TrackTableEntryNext(e);
    return e;
}

/* the camera's point at the table's entry plus (dx, dy, dz) */
static void director_place(Object *o, uint16_t entry, uint16_t dx, uint16_t dy, uint16_t dz)
{
    uint16_t out[3];
    Camera *c = camera_of(o->link);

    TrackTableEntryPlace(entry, out);
    c->x = (uint16_t)(out[0] + dx);
    c->y = (uint16_t)(out[1] + dy);
    c->z = (uint16_t)(out[2] + dz);
}

static void director_still(Object *o)
{
    o->director.steps = 0;
    o->vx = o->vy = o->vz = 0;
}

static uint16_t slope(uint16_t a)
{
    for (int i = 0; i < 5; i++)
        a = asr1(a);
    return a;
}

/*
 * @routine 08:8D2D CameraDirectorTrack
 * WHAT IT DOES: The replay camera on the course's camera table: when the
 * parent's record moves to a new entry the camera goes to the next entry's
 * place and starts rising a thirty-second of the way to the entry after;
 * then it moves by its velocity and the point steps.
 */
void CameraDirectorTrack(Object *o)
{
    uint16_t e = director_entry(o, 0);
    Camera *c = NULL;

    if (e) {
        c = camera_of(o->link);
        if (e != c->word04 && !(e & 0x0003)) {
            uint16_t next = TrackTableEntryNext(e), out[3];

            c->word04 = e;
            director_place(o, next, 0x03E8, 0xFA24, 0x03E8);
            o->director.steps = 0x0018;
            TrackTableEntryPlace(TrackTableEntryNext(next), out);
            o->vx = slope((uint16_t)(out[0] - c->x));
            o->vy = slope((uint16_t)(out[1] - c->y));
            o->vz = slope((uint16_t)(out[2] - c->z));
        }
    }
    if (o->director.steps) {
        o->director.steps--;
        c = camera_of(o->link);
        c->y = (uint16_t)(c->y + 0x0019);
    }
    if (c) {                            /* with no entry and no count the original adds to low memory */
        c->x = (uint16_t)(c->x + o->vx);
        c->z = (uint16_t)(c->z + o->vz);
    }
    director_tail(o);
}

static void director_track_start(Object *o)
{
    ObjectSetHandler(o, H_DIRECTOR_TRACK);
    director_mode(o, 0x0003);
    fxram_put(FX_REPLAY_FLAGS, (uint16_t)(fxram_get(FX_REPLAY_FLAGS) & 0xFFFE));
    director_still(o);
    CameraDirectorTrack(o);
}

/*
 * @routine 08:8C99 CameraDirectorSkip2
 * WHAT IT DOES: The replay camera two entries on, then tracking.
 */
void CameraDirectorSkip2(Object *o)
{
    director_place(o, director_entry(o, 2), 0x03E8, 0xFE0C, 0x03E8);
    director_track_start(o);
}

/*
 * @routine 08:8BFB CameraDirectorSkip3
 * WHAT IT DOES: The replay camera three entries on, then tracking.
 */
void CameraDirectorSkip3(Object *o)
{
    director_place(o, director_entry(o, 3), 0x03E8, 0xFE0C, 0x03E8);
    director_track_start(o);
}

/*
 * @routine 08:8BD7 CameraDirectorHold
 * WHAT IT DOES: The replay camera held where it is, following its target.
 */
void CameraDirectorHold(Object *o)
{
    ObjectSetHandler(o, H_DIRECTOR_HOLD);
    director_mode(o, 0x0003);
    CameraFollowLink(o);
    director_tail(o);
}

/*
 * @routine 08:8C43 CameraDirectorSkip5
 * WHAT IT DOES: The replay camera five entries on, that entry kept as its
 * own, then held there.
 */
void CameraDirectorSkip5(Object *o)
{
    uint16_t e = director_entry(o, 5);

    camera_of(o->link)->word04 = e;
    director_place(o, e, 0x03E8, 0xFE0C, 0x03E8);
    CameraDirectorHold(o);
}

/*
 * @routine 08:8B5B CameraDirectorNext
 * WHAT IT DOES: The replay camera at the next entry, 200 up, then tracking
 * (the replay flag left as it is).
 */
void CameraDirectorNext(Object *o)
{
    ObjectSetHandler(o, H_DIRECTOR_TRACK);
    director_mode(o, 0x0003);
    director_place(o, director_entry(o, 1), 0x0000, 0xFF38, 0x0000);
    director_still(o);
    CameraDirectorTrack(o);
}

void CameraDirectorTableStart(Object *o)
{
    Camera *c = camera_of(o->link);

    c->word04 = 0;
    c->byte34 = 0;
    o->director.state &= (uint8_t)~DIRECTOR_CUE;
    o->director.state &= (uint8_t)~DIRECTOR_BACK;
    ObjectSetHandler(o, H_DIRECTOR_TABLE);
    CameraDirectorTable(o);
}

/*
 * @routine 08:8EF5 CameraDirectorTable
 * WHAT IT DOES: The replay camera from the camera point table, then the
 * point's step.
 */
void CameraDirectorTable(Object *o)
{
    CameraPointFromTable(o);
    director_tail(o);
}

/*
 * @routine 08:8E51 CameraDirectorPath
 * WHAT IT DOES: The replay camera along its path; at its end to the table.
 */
void CameraDirectorPath(Object *o)
{
    if (CameraApproachTarget(o))
        CameraDirectorTableStart(o);
    else
        director_tail(o);
}

/*
 * @routine 08:8E2A CameraDirectorPathStart
 * WHAT IT DOES: The replay camera starts on path $87F1.
 */
void CameraDirectorPathStart(Object *o)
{
    ObjectSetHandler(o, H_DIRECTOR_PATH);
    director_mode(o, 0x0001);
    ObjectPathStart(o, PATH_DIRECTOR);
    CameraFollowLink(o);
    CameraDirectorPath(o);
}

/*
 * @routine 08:8EA2 CameraDirectorWait
 * WHAT IT DOES: The replay camera waits out its count, then to the table.
 */
void CameraDirectorWait(Object *o)
{
    if (!o->director.frames) {
        CameraDirectorTableStart(o);
        return;
    }
    o->director.frames--;
    director_tail(o);
}

/*
 * @routine 08:8E5D CameraDirectorBehind
 * WHAT IT DOES: The replay camera behind the car, then waiting.
 */
void CameraDirectorBehind(Object *o)
{
    Camera *c = camera_of(o->link);

    director_mode(o, 0x0021);
    c->place_base[0] = 0x0000;
    c->place_base[1] = 0xFF74;
    c->place_base[2] = 0xFE0C;
    CameraSnapToCar(o);
    ObjectSetHandler(o, H_DIRECTOR_WAIT);
    o->director.frames = 0x50;
    CameraDirectorWait(o);
}

/*
 * @routine 08:A00F CameraOnMarkedCar
 * WHAT IT DOES: The camera on the car kept at $1176 at (0, -$C8, $2BC),
 * mode 1.
 */
void CameraOnMarkedCar(Object *o)
{
    Camera *c = &cameras[0];

    (void)o;
    c->object = 0;                      /* $1176: only the unreached ObjectMarkerFollow writes it */
    c->place[0] = 0x0000;
    c->place[1] = 0xFF38;
    c->place[2] = 0x02BC;
    c->mode = (uint16_t)((c->mode & 0xFF00) | 0x01);
}

/* the camera table a course's record follows (gsu/params.c) */
uint16_t TrackTableEntryNext(uint16_t entry)
{
    uint16_t last = track_table_last(cart_ref(fx_ram[FX_TRACK_TABLE_BANK], fxram_get(FX_TRACK_TABLE)));

    if (entry == last)
        entry = 0;
    return (uint16_t)(entry + 1);
}

void TrackTableEntryPlace(uint16_t entry, uint16_t out[3])
{
    track_table_place(cart_ref(fx_ram[FX_TRACK_TABLE_BANK], fxram_get(FX_TRACK_TABLE)), entry, out);
}
