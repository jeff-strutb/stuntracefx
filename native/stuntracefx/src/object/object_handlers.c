/*
 * object_handlers.c: object handlers of bank $08: bursts of pieces, a
 * blinking child, the Speed Trax car and signal pair, a car's children,
 * break-off pieces, attachments, flames and so on. A handler that becomes
 * another stores the next one's number and runs it at once.
 *
 * Not converted, nothing in the game reaches them: see object/NOTES.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
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
    if (!o->count48)
        return 1;
    o->count48--;
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
    burst_piece(o, 0xA5F0, H_BURST_PIECE_START);
    burst_piece(o, 0xA60C, H_BURST_PIECE_START);
    burst_piece(o, 0xA628, H_BURST_PIECE_START);
    burst_piece(o, 0xA644, H_BURST_PIECE_START);
}

/*
 * @routine 08:EDB6 ObjectBurstPieceStep
 * WHAT IT DOES: A burst piece's frame: synced from its record; released when
 * it has none or its count runs out, or when its contact summary has bit 7
 * and it is not touching.
 */
void ObjectBurstPieceStep(Object *o)
{
    uint8_t a = ObjectSyncFromRecord(o);

    if (record_contacts.no_record) {
        ObjectRelease(o);
        return;
    }
    if (!(a & 0x01) && (RecordContactSummary(o) & 0x80)) {
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
    o->count48 = 0x28;
    o->heading = RandomStep();
    o->pitch = RandomStep();
    GsuRecordNew426(o);
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
    if (!(o->frame5a & 0x7F)) {
        player1_record_42(0x1010, o);
        return;
    }
    v = (uint8_t)(o->frame5a - 1);
    if (!(v & 0x80))
        v = (uint8_t)(v + 0x10);
    v &= 0x7F;
    if (v >= 0x10)
        v = (uint8_t)(v - 0x10);
    o->frame5a = (uint8_t)(v | 0x80);
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
    o->count48 = 0x28;
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
    Object *n = ObjectSpawn(0xA91C);

    if (n) {
        ObjectCopyPosition(n, o);
        o->parent = obj_ref(n);
        ObjectRebucket(n);
    }
    ObjectSetHandler(o, H_BLINKER_RISE);
    o->count48 = 0x32;
    o->frame5a = 0x8F;
    o->script = 0xD354;
    if (!GsuRecordNew422(o))
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
    if (g.script_byte_1f71 != 0x02) {
        if (!below((uint16_t)(rec_get(rec, FXREC_POS_Z) + 0x0258), g.pace_z))
            g.script_byte_1f71 = 0x01;
        rec_and(rec, FXREC_SIDES, 0xF7FF);
        if (below(rec_get(rec, FXREC_SPEED), 0x001E))
            rec_or(rec, FXREC_SIDES, 0x0800);
        GsuRecordFlag4(o);
        return;
    }
    rec_or(rec, FXREC_FLAGS1C, 0x0010);
    o->flags24 |= 0x20;
    o->flags24 |= 0x08;
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
    if (g.script_byte_1f71) {
        Object *child = obj_at(o->parent);
        uint8_t v;

        child->model = 0xA794;
        if ((child->frame5a & 0x7F) == 0x07) {
            obj_at(g.player_car)->model = 0xA75C;
            ObjectRelease(child);
            ObjectRelease(o);
            g.script_byte_1f71 = 0x02;
            return;
        }
        v = (uint8_t)(child->frame5a + 1);
        if (!(v & 0x80))
            v = (uint8_t)(v + 0x08);
        v &= 0x7F;
        if (v >= 0x08)
            v = (uint8_t)(v - 0x08);
        child->frame5a = (uint8_t)(v | 0x80);
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
    g.script_byte_1f71 = 0;
    o->y = (uint16_t)(o->y + 0xFF9C);
    n = ObjectSpawn(0xA040);
    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->z = (uint16_t)(n->z + 0xF254);
    n->y = 0;
    n->link27 = g.selection_handler[0];
    ObjectSetHandler(n, H_PACE_CAR_START);
    n = ObjectSpawn(0xA7B0);
    if (!n)
        return;
    ObjectCopyPosition(n, o);
    n->y = (uint16_t)(n->y + 0x0032);
    n->frame5a = 0x80;
    ObjectRebucket(n);
    o->parent = obj_ref(n);
    ObjectSetHandler(o, H_SIGNAL_STEP);
    o->script = 0xC6DC;
    if (!GsuRecordNew422(o))
        return;
    record_place(o, 0x0640, 0x0640, 0x0640, 0xC100);
    o->count48 = 0x14;
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
 * place with the child's parent as its +$27. Returns 1 when one was made.
 */
static int ObjectBreakOffChance(Object *child)
{
    Object *n;

    if (RandomStep() < 0x7F)
        return 0;
    n = ObjectSpawn(0xA1AC);
    if (!n)
        return 0;
    ObjectCopyPosition(n, child);
    n->link27 = child->parent;
    return 1;
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
    uint16_t own, d, f4c;

    ObjectRebucket(o);
    p = obj_at(o->parent);
    if (rec_get(p->record, FXREC_FLAGS16) & 0x0400) {
        rec_or(o->record, FXREC_FLAGS16, 0x0480);
        o->flags24 |= 0x20;
        return;
    }
    o->flags24 &= 0xDF;
    rec_and(o->record, FXREC_FLAGS16, 0xFB7F);
    own = o->record;
    if (p->flags24 & 0x40)
        goto place;
    f4c = rec_get(p->record, FXREC_FLAGS40);
    if (!(rec_get(p->record, FXREC_FLAGS64) & 0x0003))
        goto place;
    {
        uint16_t w = rec_get(own, FXREC_FACE_46);

        if (!(w & 0x0001) || !(w & 0x0010))
            goto place;
    }
    d = rec_get(own, 0x002C);
    if (d & 0x0080) {
        if (ObjectBreakOffChance(o))
            ObjectSetHandler(o, record_fast(own) ? H_BREAK_OFF_F : H_BREAK_OFF_E);
        goto place;
    }
    if (d & 0x2000) {
        if (record_fast(own) && ObjectBreakOffChance(o))
            ObjectSetHandler(o, H_BREAK_OFF_D);
        goto place;
    }
    if (d & 0x0800) {
        if (record_fast(own) && ObjectBreakOffChance(o))
            ObjectSetHandler(o, H_BREAK_OFF_B);
        goto place;
    }
    if (d & 0x1000) {
        if (record_fast(own) && ObjectBreakOffChance(o))
            ObjectSetHandler(o, H_BREAK_OFF_A);
        goto place;
    }
    if (d & 0x0060) {
        if (record_fast(own) && ObjectBreakOffChance(o))
            ObjectSetHandler(o, H_BREAK_OFF_G);
        goto place;
    }
    if (!(f4c & 0x0800) && !(rec_get(own, FXREC_FACE_46) & 0x0020))
        goto place;
    if (ObjectBreakOffChance(o))
        ObjectSetHandler(o, H_BREAK_OFF_H);
place:
    o->x = (uint16_t)(rec_get(own, FXREC_POS_X) + rec_get(own, 0x004A));
    o->y = (uint16_t)(0xFFEC - o->byte5f + rec_get(own, FXREC_POS_Y) + rec_get(own, 0x004C));
    o->z = (uint16_t)(rec_get(own, FXREC_POS_Z) + rec_get(own, 0x004E));
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
    for (rec = rec_get(o->record, 0x003E); rec; rec = rec_get(rec, 0x0034)) {
        uint16_t t, w52;
        Object *n;

        if (rec_get(rec, FXREC_FACE_46) & 0x0200)
            continue;
        t = rec_get(rec, 0x0012);
        w52 = cart16(0x09, (uint16_t)(0x8002 + t));
        n = ObjectSpawn(cart16(0x09, (uint16_t)(0x8000 + t)));
        if (!n)
            continue;
        if (w52) {
            n->flags24 |= 0x08;
            n->byte3a = 0;
            n->byte5f = (uint8_t)cart16(0x09, (uint16_t)(0x8004 + t));
            n->word29 = w52;
            if (g.split_screen)
                n->word29 = (uint16_t)(n->word29 + 2);
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

    o->flags24 &= 0xBF;
    if (p->flags24 & 0x08)
        o->flags24 |= 0x20;
    else if (p->flags24 & 0x40)
        o->flags24 |= 0x40;
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
    if (obj_at(o->parent)->flags25 & 0x04)
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
    o->byte26 = obj_at(o->parent)->byte26;
    ObjectChildStep(o);
}

/* back on the parent: its record again, then the normal step's flags */
static void child_back(Object *o)
{
    o->record = o->link27;
    ObjectSetHandler(o, H_CHILD_STEP);
    child_flags(o);
}

/* below the parent by the child's +$5F and $14 */
static uint16_t child_y(uint8_t byte5f, uint16_t a)
{
    return (uint16_t)((uint16_t)-byte5f + 0xFFEC + a);
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
    uint16_t rec = o->link27, tx, ty, tz;
    int moved;

    ObjectVelocityFromRecordOf(o, p);
    tx = (uint16_t)(rec_get(rec, FXREC_POS_X) + rec_get(rec, 0x004A));
    ty = (uint16_t)(child_y(o->byte5f, rec_get(rec, FXREC_POS_Y)) + rec_get(rec, 0x004C));
    tz = (uint16_t)(rec_get(rec, FXREC_POS_Z) + rec_get(rec, 0x004E));
    ObjectTurnFromRecord(rec, o);
    o->x = ApproachQuarterMin4(o->x, tx, &moved);
    o->y = ApproachQuarterMin4(o->y, ty, &moved);
    o->z = ApproachQuarterMin4(o->z, tz, &moved);
    ObjectApplyVelocity(o);
    ObjectRebucket(o);
    if (!(p->flags25 & 0x04)) {
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

    if (!(ObjectSyncFromRecord(o) & 0x01)) {
        p = obj_at(o->parent);
        if ((p->flags25 & 0x10) && (RecordContactSummary(o) & 0x80)) {
            ObjectBreakOffContact(o);
            return;
        }
    }
    p = obj_at(o->parent);
    if ((p->flags25 & 0x10) && !o->byte4b) {
        if (rec_get(p->record, FXREC_FLAGS64) & 0x0010) {
            ObjectRelease(o);
            return;
        }
        ObjectSpawnChanceD460(o);
        child_flags(o);
        return;
    }
    if (!o->byte4b) {
        ObjectSetHandler(o, H_CHILD_RETURN);
        ObjectChildReturn(o);
        return;
    }
    o->byte4b--;
    o->y = child_y(o->byte5f, o->y);
    ObjectSpawnChanceD460(o);
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
    o->link27 = o->record;
    GsuRecordNew426(o);
    p = obj_at(o->parent);
    ObjectVelocityFromRecordOf(o, p);
    o->vx = asr1(rec_get(o->link27, 0x0036));
    o->vy = 0xFFCE;
    o->vz = asr1(rec_get(o->link27, 0x003A));
    GsuRecordVelocity(o);
    o->byte4b = (uint8_t)((RandomStep() & 0x0F) + 0x0A);
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
    o->sprite_turn[0] = (uint8_t)(o->count48 | 0x80);
    o->sprite_turn[1] = o->sprite_turn[0];
    if (o->count48 == o->byte49) {
        ObjectRelease(o);
        return;
    }
    o->count48++;
    ObjectApplyVelocity(o);
    o->timer = 3;
}

/* The fall: count 0, end `end`, +$25 bit 0 unless 6 frames */
static void piece_handler(Object *o, uint8_t end)
{
    ObjectSetHandler(o, H_PIECE_FALL);
    o->count48 = 0;
    if (end == 0x06) {
        o->byte49 = 0x06;
        return;
    }
    o->flags25 |= 0x01;
    o->byte49 = end;
}

static void piece_look(Object *o, uint16_t look, int adv, uint8_t f5)
{
    o->anim = look;
    if (adv)
        ObjectAdvance5DIfFlag(o);
    o->flags24 |= 0x08;
    o->byte3a = 0;
    o->byte5f = f5;
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
    piece_look(o, 0x998D, 0, 0x14);
    ObjectVelocityFromRecordScaled(o);
    piece_drop(o);
    piece_fall4(o);
}

/* @routine 08:D249 ObjectBreakOffB */
void ObjectBreakOffB(Object *o)
{
    piece_look(o, 0x9B96, 0, 0x14);
    ObjectVelocityFromRecord(o);
    piece_drop(o);
    piece_handler(o, 0x03);
    ObjectPieceFall(o);
}

/* @routine 08:D2CF ObjectBreakOffD */
void ObjectBreakOffD(Object *o)
{
    piece_look(o, 0x9B8F, 1, 0x14);
    ObjectVelocityFromRecord(o);
    piece_drop(o);
    piece_fall4(o);
}

/* @routine 08:D303 ObjectBreakOffE */
void ObjectBreakOffE(Object *o)
{
    piece_look(o, 0x9B79, 1, 0x14);
    ObjectVelocityFromRecord(o);
    piece_drop(o);
    piece_fall4(o);
}

static void break_off_f(Object *o, uint8_t f5)
{
    o->anim = 0x9B63;
    ObjectAdvance5DIfFlag(o);
    o->flags24 |= 0x08;
    o->byte3a = 0;
    o->byte5f = f5;
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
    uint16_t a = (uint16_t)(asr1(abs16(rec_get(obj_at(o->link27)->record, FXREC_SPEED))) + 0x0014);

    if (!below(a, 0x0078))
        a = 0x0078;
    ObjectVelocityFromRecord(o);
    break_off_f(o, (uint8_t)a);
}

/* @routine 08:D39F ObjectBreakOffG */
void ObjectBreakOffG(Object *o)
{
    piece_look(o, 0x9B56, 1, 0x14);
    piece_drop(o);
    ObjectVelocityFromRecordScaled(o);
    piece_fall4(o);
}

/* @routine 08:D3D3 ObjectBreakOffH */
void ObjectBreakOffH(Object *o)
{
    piece_look(o, 0x9B40, 1, 0x0A);
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
    uint16_t f4c;
    Vec16 v;

    piece_handler(o, 0x06);
    f4c = (uint16_t)((RandomStep() & 0x0F) + 0xFFD8);
    piece_look(o, 0x997C, 0, 0x0A);
    p = obj_at(o->link27);
    RotateXZFrom08(0, (int8_t)(uint8_t)f4c, p->pitch, &v.y, &v.z);
    RotateXZ(0, (int8_t)v.z, p->heading, &v.x, &v.z);
    o->x = (uint16_t)(o->x + (uint16_t)((uint16_t)v.x << 1));
    o->z = (uint16_t)(o->z + (uint16_t)((uint16_t)v.z << 1));
    o->y = (uint16_t)(o->y + (uint16_t)((uint16_t)v.y << 1));
    ObjectVelocityFromRecordScaled(o);
    o->vy = (uint16_t)(o->vy + 0xFFEC);
    ObjectAdvance5DIfFlag(o);
    ObjectPieceFall(o);
}

/* hidden, +$36 = 3 */
static void attach_hide(Object *o)
{
    o->flags24 |= 0x20;
    o->timer = 3;
}

/* whether the parent is gone or loose: then the object hides */
static int attach_parent_gone(Object *o)
{
    const Object *p = obj_at(o->parent);

    if ((p->flags24 & 0x08) || (p->flags25 & 0x04)) {
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
    Vec16 v = {sx8((uint8_t)o->vx), sx8((uint8_t)o->vy), sx8((uint8_t)o->vz)};

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
    uint16_t i = fxram_get(0x2392);
    unsigned n = cart8(0x09, (uint16_t)(0x8090 + i));

    i = (uint16_t)(i + 1);
    for (; n; n--, i = (uint16_t)(i + 5)) {
        Object *a = ObjectSpawn(0xA040);
        uint16_t k, look;

        if (!a)
            continue;
        a->parent = obj_ref(o);
        k = cart16(0x09, (uint16_t)(0x8090 + i));
        a->model = cart16(0x09, (uint16_t)(0x8000 + k));
        look = cart16(0x09, (uint16_t)(0x8002 + k));
        a->handler = cart16(0x09, (uint16_t)(0x8006 + k));
        a->handler_bank = cart8(0x09, (uint16_t)(0x8008 + k));
        if (look) {
            a->flags24 |= 0x08;
            a->byte3a = 0;
            a->byte5f = (uint8_t)cart16(0x09, (uint16_t)(0x8004 + k));
            a->anim = look;
        }
        a->vx = (uint16_t)sx8(cart8(0x09, (uint16_t)(0x8092 + i)));
        a->vy = (uint16_t)sx8(cart8(0x09, (uint16_t)(0x8093 + i)));
        a->vz = (uint16_t)sx8(cart8(0x09, (uint16_t)(0x8094 + i)));
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
    o->pitch = fx_ram[(uint16_t)(rec + 0x0027)];
    o->heading = fx_ram[(uint16_t)(rec + 0x0025)];
    ObjectApplyVelocity(o);
    ObjectRebucket(o);
    o->flags24 &= 0xDF;
    o->flags24 &= 0xBF;
    if (obj_at(o->parent)->flags24 & 0x40) {
        o->flags24 |= 0x40;
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
    o->heading = (uint8_t)(fx_ram[(uint16_t)(p->record + 0x003B)] + p->heading);
    o->pitch = p->pitch;
    o->roll = p->roll;
    offset_on_parent(o, p, 3, 1);
    ObjectRebucket(o);
    o->timer = 3;
}

static void spark(Object *o, uint16_t vx, uint16_t vz)
{
    Object *n = ObjectSpawn(0xA1E4);

    if (!n)
        return;
    n->parent = obj_ref(o);
    ObjectSetHandler(n, H_SPARK_START);
    n->vx = vx;
    n->vy = 0x000A;
    n->vz = vz;
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
    o->flags24 |= 0x08;
    o->byte3a = 0;
    o->byte5f = 0x6E;
    ObjectSetHandler(o, H_SPARK_STEP);
    o->word29 = 0x9BBF;
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
    GsuRecordFlag4(o);
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
    rec_put(o->record, 0x008E, p->record);
    rec_or(o->record, FXREC_FLAGS1C, 0x0008);
    rec_put(p->record, 0x008E, o->record);
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

    if (o->byte4b) {
        o->byte4b--;
        o->count48 = (uint8_t)GsuRecordWord88(o);
    }
    p = obj_at(o->parent);
    if ((p->flags25 & 0x08) || (p->flags25 & 0x10) || ObjectParentRecordBit8(o)) {
        attach_hide(o);
        return;
    }
    if (o->byte39 == 0x01) {
        v = (uint8_t)GsuRecordWord88(o);
        if (v != o->count48) {
            o->count48 = v;
            o->byte39 = 0x02;
            o->byte49 = 0x08;
        }
        v = (uint8_t)GsuRecordWord58(o, &clear);
        if (!clear) {
            uint8_t w = (uint8_t)(v - o->byte4a);

            if (w & 0x80) {
                w = (uint8_t)-w;
                if ((uint8_t)(w - 0x02) & 0x80)
                    goto state2;
                if ((uint8_t)(w - 0x0A) & 0x80) {
                    o->byte39 = 0x03;
                    o->byte49 = 0x08;
                    SoundPort0QueuePush(0x3B);
                }
            }
        }
        o->byte4a = v;
    }
state2:
    if (o->byte39 == 0x02)
        o->sprite_turn[0] = (uint8_t)((uint8_t)GsuRecordWord88(o) | 0x80);
    if (o->byte39 == 0x03) {
        o->sprite_turn[0] = 0x84;
        o->byte4a = (uint8_t)GsuRecordWord58(o, &clear);
    }
    v = o->byte49;
    if (!v) {
        attach_hide(o);
        return;
    }
    v--;
    if (!v) {
        o->byte49 = 0;
        o->byte39 = 0x01;
        attach_hide(o);
        return;
    }
    o->byte49 = v;
    if (!(v & 0x03)) {
        attach_hide(o);
        return;
    }
    o->flags24 &= 0xDF;
    p = obj_at(o->parent);
    ObjectCopyPosition(o, p);
    ObjectApplyVelocity(o);
    o->flags24 &= 0x7F;
    {
        const Camera *cam = camera_of(obj_at(p->link)->link);

        if (cam->mode & 0x4000) {
            o->x = 0x0000;
            o->y = 0xFF9C;
            o->z = 0x01F4;
            o->flags24 |= 0x80;
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
        o->byte4a = (uint8_t)GsuRecordWord58(o, &clear);
        o->byte49 = 0;
        o->vy = (uint16_t)((uint16_t)sx8((uint8_t)o->vy) << 1);
        o->byte4b = 0x0A;
        o->byte39 = 0x01;
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

    if (!(o->flags25 & 0x10)) {
        if (!(rec_get(rec, FXREC_FLAGS40) & 0x0100)) {
            if (g.course_flags & 0x0020) {
                uint16_t r = rec_get(rec, 0x003E);

                found = 0;
                do {
                    if ((rec_get(r, 0x002C) & 0x0080) && (rec_get(r, FXREC_FACE_46) & 0x0001) && (rec_get(r, FXREC_FACE_46) & 0x0010))
                        found = 1;
                    r = rec_get(r, 0x0034);
                } while (r);
                if (found) {
                    if (!g.split_screen)
                        fxram_put(FX_EFFECT_MAX, 0x0010);
                    SoundAtRateForLevel(abs16(rec_get(rec, FXREC_SPEED)));
                }
            } else {
                if (rec_get(rec, 0x0092) & 0x0020)
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
        v = fx_ram[(uint16_t)(rec + 0x0057)];
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
        w = rec_get(rec, 0x0094);
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
        w = rec_get(rec, 0x002C);
        if (w & 0x0100)
            SoundPort0QueuePush(0x0A);
        else if (w & 0x0200)
            SoundPort0QueuePush(0x0D);
        w = rec_get(rec, 0x0062);
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
                o->flags25 |= 0x20;
                ObjectSpawnAttached(o, 0x0A, 0x00);
            }
        }
        w = rec_get(rec, FXREC_FLAGS40);
        if (w & 0x0001) {
            if (!CarCrashParts(o, 0x01)) {
                o->flags25 |= 0x10;
                if (!g.split_screen)
                    ObjectBurstSpawn(o);
            }
        } else if (w & 0x0040) {
            o->flags25 |= 0x20;
            ObjectSpawnSparklePair(o);
            CarCrashParts(o, 0x00);
        } else if (w & 0x0020) {
            CarCrashParts(o, 0x01);
            if (!g.split_screen)
                ObjectBurstSpawn(o);
        }
        if (!(o->flags24 & 0x40)) {
            if (rec_get(rec, FXREC_FLAGS40) & 0x0002) {
                ObjectSpawnTrail(o);
                ObjectSpawnD460UnlessFlag(o);
            } else if ((rec_get(rec, FXREC_SIDES) & 0x0400) && RandomStep() >= 0xF2) {
                ObjectSpawnTrail(o);
            }
            if (!below(rec_get(rec, 0x0082), 0x0199)) {
                uint16_t *f = &g.player_flags[g.current_player ? 1 : 0];

                if (!(*f & 0x0020)) {
                    g.save_bad_flag = 0x11;
                    SoundPort0QueuePush(0x3D);
                    *f |= 0x0020;
                }
                if (!(o->flags25 & 0x04) && RandomStep() >= 0x4C)
                    ObjectSpawnD460UnlessFlag(o);
            }
        }
    }
    if (o->flags25 & 0x84)
        o->flags24 |= 0x20;
    else
        o->flags24 &= 0xDF;
    if (camera_of(obj_at(o->link)->link)->mode & 0x4000)
        o->flags24 |= 0x40;
    else
        o->flags24 &= 0xBF;
}

/*
 * @routine 08:E443 ObjectOffsetOnParent2
 * WHAT IT DOES: An attachment placed at its offset turned by its parent's
 * angles and doubled; hidden when the parent is hidden.
 */
void ObjectOffsetOnParent2(Object *o)
{
    Object *p = obj_at(o->parent);

    if ((p->flags24 & 0x08) || (p->flags24 & 0x20)) {
        attach_hide(o);
        return;
    }
    o->sprite_turn[0] = 0x80;
    offset_on_parent(o, p, 1, 1);
    o->flags24 &= 0xDF;
    ObjectRebucket(o);
}

/* On its parent, rebucketed, +$24 bits 5 and 6 cleared, bit 6 again when
   the parent has it; +$36 = 3. */
static void lamp_place(Object *o)
{
    Object *p = obj_at(o->parent);

    ObjectPlaceOnParent(p, o);
    ObjectRebucket(o);
    o->flags24 &= 0xDF;
    o->flags24 &= 0xBF;
    p = obj_at(o->parent);
    if (p->flags24 & 0x40)
        o->flags24 |= 0x40;
    o->timer = 3;
}

/* with the parent's +$25 bit 4 released; otherwise the drop */
static void lamp_drop_start(Object *o)
{
    if (obj_at(o->parent)->flags25 & 0x10) {
        ObjectRelease(o);
        return;
    }
    ObjectSetHandler(o, H_LAMP_DROP);
    o->byte49 = 0;
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

    if (p->flags24 & 0x08) {
        attach_hide(o);
    } else if (p->flags25 & 0x04) {
        lamp_drop_start(o);
    } else if (p->flags25 & 0x20) {
        ObjectLampDropBStart(o);
    } else if (p->flags25 & 0x40) {
        o->anim = 0x9AB1;
        lamp_drop_start(o);
    } else {
        lamp_place(o);
    }
}

/* one step of a lamp's look: unless its count is 4 */
static void lamp_look_step(Object *o, uint16_t table)
{
    if (o->byte4b == 0x04)
        return;
    o->byte4b++;
    o->anim = cart16(0x08, (uint16_t)(table + (uint8_t)(o->byte4b << 1)));
}

/*
 * @routine 08:E51E ObjectLampAnimate
 * WHAT IT DOES: A lamp's look by the race's light bits, then its frame on
 * the parent.
 */
void ObjectLampAnimate(Object *o)
{
    switch (g.lamp_lights & 0x1C) {
    case 0x08: lamp_look_step(o, 0xE5AD); break;
    case 0x04: lamp_look_step(o, 0xE57F); break;
    case 0x0C: lamp_look_step(o, 0xE5DB); break;
    case 0x1C: lamp_look_step(o, 0xE609); break;
    case 0x10: o->anim = 0x9AAF; break;
    default: o->byte4b = 0; break;
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
        o->byte4b = 0;
        ObjectSetHandler(o, H_LAMP_ANIMATE);
        ObjectLampAnimate(o);
        return;
    }
    o->anim = 0x9AAF;
    ObjectSetHandler(o, H_LAMP_ON_PARENT);
    ObjectLampOnParent(o);
}

/*
 * @routine 08:E613 ObjectLampStart
 * WHAT IT DOES: A lamp's first frame.
 */
void ObjectLampStart(Object *o)
{
    o->byte26 = obj_at(o->parent)->byte26;
    ObjectLampWait(o);
}

/*
 * @routine 08:E6DE ObjectLampDrop
 * WHAT IT DOES: A dropping lamp: its count stepped; when done back to
 * waiting; placed on its parent either way.
 */
void ObjectLampDrop(Object *o)
{
    o->count48 = (uint8_t)(o->count48 + 0x10);
    if (ObjectCountStep5F(o, 0x08, 0x10, 0x05)) {
        o->count48 = 0;
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
    if (ObjectCountStep5F(o, 0x03, 0x06, 0x0A)) {
        o->byte49 = 0;
        ObjectSetHandler(o, H_LAMP_WAIT);
        o->flags25 &= 0xFE;
    }
    lamp_place(o);
}

/* @routine 08:E712 ObjectLampDropBStart */
void ObjectLampDropBStart(Object *o)
{
    ObjectSetHandler(o, H_LAMP_DROP_B);
    o->byte49 = 0;
    o->flags25 |= 0x01;
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
    uint16_t x, off[3];
    int moved;

    if (!(c->mode & 0x0020) && (o->flags25 & 0x01)) {
        uint8_t count = cart8(0x09, (uint16_t)(0x8090 + c->word32));
        uint8_t back = cart8(0x09, (uint16_t)(0x8091 + c->word32));
        uint8_t v;

        o->flags25 &= 0xFE;
        SoundPort0QueuePush(0x24);
        if ((o->flags25 & 0x02) && (o->flags25 &= 0xFD, back)) {
            v = (uint8_t)(back + count - 1);
            if (v == c->byte34)
                v = 0;
        } else {
            v = (uint8_t)(c->byte34 + 1);
            if (!((uint8_t)(v - count) & 0x80))
                v = 0;
        }
        c->byte34 = v;
        o->link27 = 0x0007;
    }
    if (obj_at(o->parent)->flags25 & 0x04) {
        o->link27 = 0x0007;
        x = 0x0000;
    } else {
        x = (uint16_t)((uint16_t)(c->byte34 << 3) + c->word32);
    }
    x = (uint16_t)(x + 2);
    c->mode = cart16(0x09, (uint16_t)(0x8090 + x));
    off[0] = cart16(0x09, (uint16_t)(0x8092 + x));
    off[1] = cart16(0x09, (uint16_t)(0x8094 + x));
    off[2] = cart16(0x09, (uint16_t)(0x8096 + x));
    if (c->mode & 0x0080) {
        if (o->link27) {
            o->link27--;
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

static const uint16_t ring_model[4] = {0xA660, 0xA6D0, 0xA67C, 0xA5D4};
static const uint16_t ring_vx[4] = {0xFF90, 0xFFDB, 0x0025, 0x0070};

/* In phase 8 with a record in hand: the ring's Y, its parts, spin or idle. */
static void ring_spawn(Object *o)
{
    uint16_t rec;
    unsigned made = 0;

    if (g.player_phase[0] != 0x08)
        goto done;
    rec = fxram_get(FXIN_PLAYER1_RECORD);
    if (!rec)                           /* the original then runs off an 8-bit store as 16 bits */
        return;
    o->y = (uint16_t)(rec_get(rec, FXREC_POS_Y) + 0xE4A8);
    o->link27 = (uint16_t)(rec_get(rec, FXREC_POS_Y) + 0xFE0C);
    for (int i = 0; i < 4; i++) {
        Object *n = ObjectSpawn(ring_model[i]);

        if (!n)
            continue;
        ObjectSetHandler(n, H_RING_PART_START);
        n->vx = ring_vx[i];
        n->parent = obj_ref(o);
        made++;
    }
    if (made != 4) {
        ObjectSetHandler(o, H_RING_IDLE);
        return;
    }
    o->roll = 0x40;
    o->heading = (uint8_t)(o->heading + 0x20);
    ObjectSetHandler(o, H_RING_SPIN);
done:
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
    o->y = ApproachQuarterMin4(o->y, o->link27, &moved);
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
    if (!o->count48) {
        ObjectRelease(o);
        return;
    }
    o->count48--;
}

/*
 * @routine 08:9DF1 ObjectRingPartStart
 * WHAT IT DOES: A ring part's first frame.
 */
void ObjectRingPartStart(Object *o)
{
    o->count48 = 0x28;
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
    ObjectSet5DByKind(o, (uint8_t)g.selection_out[0]);
    if (o->count48)
        o->count48--;
    else
        rec_or(o->record, FXREC_FLAGS1C, 0x0010);
    ObjectRebucket(o);
    ObjectPoseFromRecord(o);
    o->timer = 3;
    GsuRecordFlag4(o);
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
        uint8_t n = (uint8_t)((uint8_t)g.word_116a << 1) / 2;

        if (n == 0)
            g.word_116c = obj_ref(o);
        else
            g.word_116e[n - 1] = obj_ref(o);
    }
    g.word_116a = (uint16_t)((g.word_116a & 0xFF00) | (uint8_t)(g.word_116a + 1));
    o->count48 = 0x04;
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
    if (o->link27) {
        o->link27--;
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
        e = GsuTableEntryNext(e);
    return e;
}

/* the camera's point at the table's entry plus (dx, dy, dz) */
static void director_place(Object *o, uint16_t entry, uint16_t dx, uint16_t dy, uint16_t dz)
{
    uint16_t out[3];
    Camera *c = camera_of(o->link);

    GsuTableEntryTo1F4C(entry, out);
    c->x = (uint16_t)(out[0] + dx);
    c->y = (uint16_t)(out[1] + dy);
    c->z = (uint16_t)(out[2] + dz);
}

static void director_still(Object *o)
{
    o->link27 = 0;
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
            uint16_t next = GsuTableEntryNext(e), out[3];

            c->word04 = e;
            director_place(o, next, 0x03E8, 0xFA24, 0x03E8);
            o->link27 = 0x0018;
            GsuTableEntryTo1F4C(GsuTableEntryNext(next), out);
            o->vx = slope((uint16_t)(out[0] - c->x));
            o->vy = slope((uint16_t)(out[1] - c->y));
            o->vz = slope((uint16_t)(out[2] - c->z));
        }
    }
    if (o->link27) {
        o->link27--;
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

void CameraDirectorTableStart(Object *o)
{
    Camera *c = camera_of(o->link);

    c->word04 = 0;
    c->byte34 = 0;
    o->flags25 &= 0xFE;
    o->flags25 &= 0xFD;
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
    ObjectPathStart(o, 0x87F1);
    CameraFollowLink(o);
    CameraDirectorPath(o);
}

/*
 * @routine 08:8EA2 CameraDirectorWait
 * WHAT IT DOES: The replay camera waits out its count, then to the table.
 */
void CameraDirectorWait(Object *o)
{
    if (!o->count48) {
        CameraDirectorTableStart(o);
        return;
    }
    o->count48--;
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
    o->count48 = 0x50;
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
    c->object = g.word_1176;
    c->place[0] = 0x0000;
    c->place[1] = 0xFF38;
    c->place[2] = 0x02BC;
    c->mode = (uint16_t)((c->mode & 0xFF00) | 0x01);
}

/* the camera table a course's record follows (gsu/params.c) */
uint16_t GsuTableEntryNext(uint16_t entry)
{
    uint8_t bank = fx_ram[FX_TRACK_TABLE_BANK];
    uint16_t last = cart16(bank, fxram_get(FX_TRACK_TABLE));

    if (entry == last)
        entry = 0;
    return (uint16_t)(entry + 1);
}

void GsuTableEntryTo1F4C(uint16_t entry, uint16_t out[3])
{
    uint8_t bank = fx_ram[FX_TRACK_TABLE_BANK];
    uint16_t at = (uint16_t)((uint16_t)((uint16_t)(entry - 1) << 3) + fxram_get(FX_TRACK_TABLE) + 2);

    for (int i = 0; i < 3; i++)
        out[i] = cart16(bank, (uint16_t)(at + 2 * i));
}
