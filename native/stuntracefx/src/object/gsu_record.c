/*
 * gsu_record.c: each object's record in Super FX RAM, which the renderer and
 * the physics read: making it, copying the object to it and back, the
 * player's controls, and the contacts it holds.
 *
 * Not converted, nothing in the game reaches them:
 * GsuRecordPositionFromObject (08:D8C2), GsuRecordNew424Zero (08:CBFF),
 * GsuRecordNew424B (08:CC31), GsuRecordListFlag80 (08:B467), ReturnLongC096
 * (08:C096), ObjectRecordChildrenMark (08:C097).
 */
#include "audio/sound.h"
#include "fx/fx.h"
#include "math/math.h"
#include "object/handlers.h"
#include "object/object.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"

RecordContacts record_contacts;

static uint8_t rec_get8(uint16_t rec, uint16_t off) { return fx_ram[(uint16_t)(rec + off)]; }

/*
 * @routine 08:CCBE GsuRecordMode2
 * WHAT IT DOES: Sets the record's mode word (+$1A), the bits of the other
 * records' flags (+$16) whose contacts it does not take, to 2: it takes
 * every contact but those of records with bit 1.
 */
void GsuRecordTouchesAll(Object *o)
{
    if (o->record)
        rec_put(o->record, FXREC_MODE, 0x0002);
}

/*
 * @routine 08:CCF1 GsuRecordSetFlags
 * WHAT IT DOES: Sets the bits in the record's flags (+$16).
 */
void GsuRecordSetFlags(Object *o, uint16_t bits)
{
    if (o->record)
        rec_or(o->record, FXREC_FLAGS16, bits);
}

/*
 * @routine 08:CCA3 GsuRecordFlag10
 * WHAT IT DOES: In the record's flags (+$16), sets bit 4 (a record touching
 * it is pushed off it: FxCollidePush) and clears bit 8.
 */
void GsuRecordSolid(Object *o)
{
    if (o->record)
        rec_put(o->record, FXREC_FLAGS16, (uint16_t)((rec_get(o->record, FXREC_FLAGS16) | 0x0010) & 0xFEFF));
}

/*
 * @routine 08:CCD2 GsuRecordPosition
 * WHAT IT DOES: Copies a position into the record's +$0C, +$0E, +$10.
 */
void GsuRecordPosition(Object *o, uint16_t x, uint16_t y, uint16_t z)
{
    if (o->record) {
        rec_put(o->record, FXREC_SIZE, x);
        rec_put(o->record, FXREC_NEXT, y);
        rec_put(o->record, FXREC_FLAGS10, z);
    }
}

/*
 * @routine 08:E3EE GsuRecordWord88
 * WHAT IT DOES: The word at +$88 of the record of the object's +$06 object,
 * minus one.
 */
uint16_t GsuRecordParentRank(const Object *o)
{
    return (uint16_t)(rec_get(obj_at(o->parent)->record, FXREC_RANK) - 1);
}

/*
 * @routine 08:E401 GsuRecordWord58
 * WHAT IT DOES: The word at +$58 of the record of the object's +$06 object;
 * *clear says whether bits 4 and 10 of the record's +$42 are both clear.
 */
uint16_t GsuRecordParentSegment(const Object *o, int *clear)
{
    uint16_t rec = obj_at(o->parent)->record;

    *clear = !(rec_get(rec, FXREC_SIDES) & 0x0410);
    return rec_get(rec, FXREC_SEGMENT);
}

/*
 * @routine 08:D070 GsuRecordFlag4
 * WHAT IT DOES: Sets bit 2 of the record word at +$1C (the record is kept: a
 * record running no handler is freed without it) and returns 1; an
 * object without a record gets $FF in its +$36 and returns 0.
 */
int GsuRecordKeep(Object *o)
{
    if (!o->record) {
        o->timer = 0xFF;
        return 0;
    }
    rec_or(o->record, FXREC_FLAGS1C, 0x0004);
    return 1;
}

/*
 * @routine 08:D83D GsuRecordVelocity
 * WHAT IT DOES: Copies the object's velocity, times 8, into its record;
 * returns 1 if it has one.
 */
int GsuRecordVelocity(Object *o)
{
    if (!o->record)
        return 0;
    rec_put(o->record, FXREC_VEL_X, (uint16_t)(o->vx << 3));
    rec_put(o->record, FXREC_VEL_Y, (uint16_t)(o->vy << 3));
    rec_put(o->record, FXREC_VEL_Z, (uint16_t)(o->vz << 3));
    return 1;
}

/*
 * @routine 08:B863 GsuRecordWords88
 * WHAT IT DOES: For each starter, copies its record's +$88 into the list at
 * $7E:FFA6.
 */
void GsuRecordStarterRanks(void)
{
    uint16_t n = (uint16_t)((objects.place_count >> 1) | (objects.place_count & 0x8000));

    if (!n)                             /* no course leaves none; the original would run on */
        return;
    for (unsigned i = 0; i < n; i++)
        objects.place_entries[i] = rec_get(obj_at(objects.place_objects[i])->record, FXREC_RANK);
}

/*
 * @routine 08:B2E9 GsuRecordStep6A
 * WHAT IT DOES: In a split screen, advances the record's word at +$6A by 1
 * (when its +$88 is 1) or 3, up to $180, unless bit 10 of +$42 is set.
 */
void GsuRecordBoostRecharge(Object *o)
{
    uint16_t rec, s;

    if (!g.split_screen)
        return;
    rec = o->record;
    if (rec_get(rec, FXREC_SIDES) & 0x0400)
        return;
    s = (uint16_t)(rec_get(rec, FXREC_6A) + (rec_get(rec, FXREC_RANK) == 1 ? 1 : 3));
    if (!((uint16_t)(s - 0x0180) & 0x8000))
        s = 0x0180;
    rec_put(rec, FXREC_6A, s);
}

static uint16_t asr3(uint16_t v) { return (uint16_t)((int16_t)v >> 3); }

/*
 * @routine 08:DC6D ObjectVelocityFromRecordOf
 * WHAT IT DOES: Sets the object's velocity to an eighth of the other's
 * record's velocity.
 */
void ObjectVelocityFromRecordOf(Object *o, const Object *from)
{
    uint16_t rec = from->record;

    o->vx = asr3(rec_get(rec, FXREC_VEL_X));
    o->vy = asr3(rec_get(rec, FXREC_VEL_Y));
    o->vz = asr3(rec_get(rec, FXREC_VEL_Z));
}

/*
 * @routine 08:D86F GsuRecordFromObject
 * WHAT IT DOES: Copies the object's state into its record: its offsets, its
 * script word, place, heading and frame.
 */
void GsuRecordFromObject(Object *o)
{
    uint16_t rec = o->record;

    if (!rec)
        return;
    rec_put(rec, 0x000C, o->offset_x);
    rec_put(rec, 0x000E, o->offset_y);
    rec_put(rec, 0x0010, o->offset_z);
    rec_put(rec, 0x0014, o->script);
    rec_put(rec, FXREC_POS_X, o->x);
    rec_put(rec, FXREC_POS_Y, o->y);
    rec_put(rec, FXREC_POS_Z, o->z);
    rec_put(rec, FXREC_HEADING, (uint16_t)(o->heading << 8));
    rec_put(rec, 0x0012, o->shown_frame & 0x7F);
}

/*
 * @routine 08:D5DF ObjectVelocityFromRecordScaled
 * WHAT IT DOES: As ObjectVelocityFromRecord with each value then taken by
 * three quarters: X and Z velocity = 3/32 of the record values of the
 * object named by +$27.
 */
void ObjectVelocityFromRecordScaled(Object *o)
{
    uint16_t rec = obj_at(o->break_off.car)->record;

    o->vx = ScaleThreeQuarters(asr3(rec_get(rec, FXREC_VEL_X)));
    o->vz = ScaleThreeQuarters(asr3(rec_get(rec, FXREC_VEL_Z)));
}

/*
 * @routine 09:FBB2 GsuRecordBits60
 * WHAT IT DOES: Keeps bits 5-6 of each player's record's +$40.
 */
void GsuRecordBobTriggers(void)
{
    g.bob_trigger[0] = rec_get(fxram_get(FXIN_PLAYER1_RECORD), FXREC_FLAGS40) & 0x0060;
    g.bob_trigger[1] = rec_get(fxram_get(FXIN_PLAYER2_RECORD), FXREC_FLAGS40) & 0x0060;
}

/* the tail of the record-making routines: the new record becomes the
   object's +$22; with one, its +$0A is set and it is filled from the
   object. Returns 1 when the object got one. */
static int record_new(Object *o, uint16_t word0a)
{
    uint16_t rec = fxram_get(GSU_RECORD_ARG);

    o->record = rec;
    if (!rec)
        return 0;
    rec_put(rec, 0x000A, word0a);
    GsuRecordFromObject(o);
    return 1;
}

/* @routine 08:CB98 GsuRecordNew422: a record that runs nothing, from the second pool (mode $0422) */
int GsuRecordNewStill(Object *o) { FxRecordNew422(); return record_new(o, FXRUN_NONE); }
/* @routine 08:CBB2 GsuRecordNew42D: a trigger: no mass, it marks what touches it (mode $042D) */
int GsuRecordNewTrigger(Object *o) { FxRecordNew42D(); return record_new(o, FXRUN_MARK_TOUCHED); }
/* @routine 08:CBCC GsuRecordNew424Piece: a piece that falls and bounces (mode $0424) */
int GsuRecordNewBouncing(Object *o) { FxRecordNew424(); return record_new(o, FXRUN_BOUNCE); }
/* @routine 08:CBE6 GsuRecordNew424: a body of mass $40 that falls while it touches something (mode $0424) */
int GsuRecordNewFalling(Object *o) { FxRecordNew424(); return record_new(o, FXRUN_FALL); }
/* @routine 08:CC18 GsuRecordNew424Alt: a trigger of mass $40, marking what touches it (mode $0424) */
int GsuRecordNewSolidTrigger(Object *o) { FxRecordNew424(); return record_new(o, FXRUN_MARK_TOUCHED); }
/* @routine 08:CC4A GsuRecordNewCamera */
int GsuRecordNewCamera(Object *o) { FxRecordNewCamera(); return record_new(o, FXRUN_COLLIDE_ONLY); }
/* @routine 08:CC63 GsuRecordNew426: as GsuRecordNewFalling, mode $0426: it also takes contacts with records whose +$16 has bit 1 */
int GsuRecordNewFallingPart(Object *o) { FxRecordNew426(); return record_new(o, FXRUN_FALL); }

/* the record and the amount (as the high byte) go to the Super FX */
static void record_amount_args(uint16_t rec, uint8_t amount)
{
    fxram_put(GSU_IN_AMOUNT, (uint16_t)(amount << 8));
    fxram_put(GSU_RECORD_ARG, rec);
}

/*
 * @routine 08:B80A GsuRecordLower82
 * WHAT IT DOES: Lowers +$82 of the record by an amount; if it is player 1's
 * record, flags the object.
 */
void GsuRecordRepair(uint16_t rec, uint8_t amount)
{
    record_amount_args(rec, amount);
    PlayerStartleIfRecord(rec);
    FxRecordLower82();
}

/*
 * @routine 08:B72E GsuRecordFlags8ASet, 08:B743 GsuRecordFlags8AClear
 * WHAT IT DOES: Sets or clears the bits in +$8A of the record (FxCarFromScript
 * starts it from the course's flags at $26F2).
 */
void GsuRecordCourseFlagsSet(uint16_t rec, uint8_t bits) { rec_or(rec, FXREC_FLAGS8A, bits); }
void GsuRecordCourseFlagsClear(uint16_t rec, uint8_t bits) { rec_and(rec, FXREC_FLAGS8A, (uint16_t)~bits & 0xFFFF); }

/*
 * @routine 08:B7D6 GsuRecordRaise6A
 * WHAT IT DOES: Unless bit 0 of +$40 is set, raises +$6A by an amount, to
 * at most $180; if it is player 1's record, flags the object.
 */
void GsuRecordBoostRaise(uint16_t rec, uint8_t amount)
{
    record_amount_args(rec, amount);
    if (rec_get(rec, FXREC_FLAGS40) & 0x0001)
        return;
    PlayerStartleIfRecord(fxram_get(GSU_RECORD_ARG));
    FxRecordRaise6A();
}

/*
 * @routine 08:BF94 ObjectTurnFromRecord
 * WHAT IT DOES: Takes the object's roll and heading from the record, then
 * its tilt for drawing (ObjectBillboardTilt).
 */
void ObjectTurnFromRecord(uint16_t rec, Object *o)
{
    o->roll = rec_get8(rec, 0x0029);
    o->heading = rec_get8(rec, 0x0025);
    ObjectBillboardTilt(o);
}

/*
 * @routine 08:CE02 ObjectSyncFromRecord
 * WHAT IT DOES: Rebuckets the object, marks its record, and takes its place
 * back from the record after giving the record its heading and frame; bit 1
 * of +$24 follows bit 1 of the record's +$1C. Returns 1 when the record's
 * contact list (record_contacts.next) is not empty, and also when that +$1C
 * bit and +$24 bit 0 are both set: the callers' test of the original's
 * result reads that bit too. 0 with no record.
 */
int ObjectSyncFromRecord(Object *o)
{
    uint16_t rec;
    int odd = 0;

    ObjectRebucket(o);
    GsuRecordKeep(o);
    o->draw_flags &= 0xFD;
    rec = o->record;
    record_contacts.no_record = !rec;
    if (!rec) {
        record_contacts.next = 0;
        return 0;
    }
    record_contacts.next = rec_get(rec, FXREC_LIST);
    rec_put(rec, FXREC_HEADING, (uint16_t)(o->heading << 8));
    rec_put(rec, 0x0012, o->shown_frame & 0x7F);
    o->x = rec_get(rec, FXREC_POS_X);
    o->y = rec_get(rec, FXREC_POS_Y);
    o->z = rec_get(rec, FXREC_POS_Z);
    if (rec_get(rec, FXREC_FLAGS1C) & 0x0002) {
        o->draw_flags |= 0x02;
        odd = o->draw_flags & 0x01;
    }
    return record_contacts.next != 0 || odd;
}

/*
 * @routine 08:CEC3 ObjectSyncToRecord
 * WHAT IT DOES: As ObjectSyncFromRecord the other way: after the heading and
 * frame, the object's place goes to the record; +$24 is left alone. Returns
 * 1 when the contact list is not empty, and also when the object's Z is odd
 * (what the callers' test of the original's result reads). 0 with no record.
 */
int ObjectSyncToRecord(Object *o)
{
    uint16_t rec;

    ObjectRebucket(o);
    GsuRecordKeep(o);
    rec = o->record;
    record_contacts.no_record = !rec;
    if (!rec) {
        record_contacts.next = 0;
        return 0;
    }
    rec_put(rec, FXREC_HEADING, (uint16_t)(o->heading << 8));
    rec_put(rec, 0x0012, o->shown_frame & 0x7F);
    record_contacts.next = rec_get(rec, FXREC_LIST);
    rec_put(rec, FXREC_POS_X, o->x);
    rec_put(rec, FXREC_POS_Y, o->y);
    rec_put(rec, FXREC_POS_Z, o->z);
    return record_contacts.next != 0 || (o->z & 0x0001);
}

/*
 * @routine 08:D01E RecordContactFlags
 * WHAT IT DOES: Adds to the summary what a record's contact word and flags
 * say: 4 plus its lowest set bit of bits 0-2 (or the whole word), 8 for flags
 * bit 0, $80 for flags bit 11.
 */
uint16_t RecordContactFlags(void)
{
    RecordContacts *c = &record_contacts;
    uint16_t w = c->word;

    if (w) {
        if (w & 0x0001)
            w = 0x0001;
        else if (w & 0x0002)
            w = 0x0002;
        else if (w & 0x0004)
            w = 0x0004;
        c->summary |= (uint16_t)(w | 0x0004);
    }
    if (c->flags & 0x0001)
        c->summary |= 0x0008;
    if (c->flags & 0x0800)
        c->summary |= 0x0080;
    return c->summary;
}

/*
 * @routine 08:B402 GsuRecordLand
 * WHAT IT DOES: When the landing is flagged and not already landed, stops
 * the record and sets +$82 to $200 and in +$40 clears bits 5-6 and sets
 * bits 0 and 12.
 */
void GsuRecordLand(Object *o)
{
    uint16_t rec;

    if (!fxram_get(FX_RECORDS_422))
        return;
    rec = o->record;
    if (!(rec_get(rec, 0x002C) & 0x0040) || !(rec_get(rec, FXREC_FLAGS8A) & 0x0010) || (rec_get(rec, FXREC_FLAGS40) & 0x0100))
        return;
    rec_put(rec, FXREC_VEL_X, 0);
    rec_put(rec, FXREC_VEL_Y, 0);
    rec_put(rec, FXREC_VEL_Z, 0);
    rec_put(rec, 0x0036, 0);
    rec_put(rec, 0x0034, 0);
    rec_put(rec, 0x0038, 0);
    if (rec_get(rec, 0x0082) == 0x0200)
        return;
    rec_put(rec, 0x0082, 0x0200);
    rec_put(rec, FXREC_FLAGS40, (uint16_t)((rec_get(rec, FXREC_FLAGS40) & 0xFF9F) | 0x1001));
}

/*
 * @routine 08:CF41 RecordNextContact
 * WHAT IT DOES: Walks the contact list from record_contacts.next to the next
 * entry with bit 1 of +$10 set; takes its touched record's contact word and
 * flags, and their RecordContactFlags. Returns the touched record, or 0
 * when there was none.
 */
uint16_t RecordNextContact(void)
{
    RecordContacts *c = &record_contacts;
    uint16_t e = c->next, r;

    if (!e)
        return 0;
    while (!(rec_get(e, 0x0010) & 0x0002)) {
        e = rec_get(e, 0x000E);
        if (!e)
            return 0;
    }
    r = rec_get(e, 0x000C);
    c->next = rec_get(e, 0x000E);
    c->word = (rec_get(r, FXREC_FLAGS16) & 0x0002) ? rec_get(r, FXREC_FLAGS64) : 0;
    c->flags = rec_get(r, FXREC_FLAGS16);
    c->summary = 0;
    RecordContactFlags();
    return r;
}

/*
 * @routine 08:CF92 RecordContactSummary
 * WHAT IT DOES: Sums up every contact of the object's record into the
 * summary: 4 and bits from the record's flags, and RecordContactFlags over
 * the OR of the contact words and flags of every touched record.
 */
uint16_t RecordContactSummary(const Object *o)
{
    RecordContacts *c = &record_contacts;
    uint16_t rec = o->record, e;

    if (!rec)
        return 0;
    c->summary = 0;
    if (rec_get(rec, FXREC_FLAGS1C) & 0x0001) {
        c->summary |= 0x0010;
        if (fxram_get(FX_COURSE_FLAGS) & 0x0020)
            c->summary |= 0x0080;
    }
    c->summary |= rec_get(rec, 0x002C) & 0x0020;
    c->flags = 0;
    c->word = 0;
    for (e = rec_get(rec, FXREC_LIST); e; e = rec_get(e, 0x000E)) {
        uint16_t r, f;

        if (!(rec_get(e, 0x0010) & 0x0002))
            continue;
        r = rec_get(e, 0x000C);
        f = rec_get(r, FXREC_FLAGS16);
        if (f & 0x0002)
            c->word |= rec_get(r, FXREC_FLAGS64);
        c->flags |= f;
    }
    return RecordContactFlags();
}

/* @routine 08:C8FF RecordButtonSequence: bit 6, 7, 14 each within 5 frames
   of the last, then bit 11 between 12 and 16 frames later; a reward. */
static void RecordButtonSequence(uint16_t rec, uint16_t held)
{
    static const uint16_t press[3] = {0x0040, 0x0080, 0x4000};
    uint16_t n = ++g.seq_count, s = g.seq_stage;

    if (s <= 2) {
        if (!((uint16_t)(n - 5) & 0x8000)) {
            g.seq_count = 0;
            g.seq_stage = 0;
            return;
        }
        if (!(held & press[s]))
            return;
        g.seq_count = 0;
        g.seq_stage++;
        return;
    }
    if (s == 3) {
        if (!((uint16_t)(n - 0x11) & 0x8000)) {
            g.seq_count = 0;
            g.seq_stage = 0;
            return;
        }
        if (!(held & 0x0800))
            return;
        if ((uint16_t)(n - 0x0C) & 0x8000) {
            g.seq_count = 0;
            g.seq_stage = 0;
            return;
        }
    }
    SoundPort0QueuePush(0x48);
    rec_or(rec, FXREC_FLAGS40, 0x2000);
    rec_put(rec, FXREC_6A, 0x0180);
    rec_put(rec, 0x0400, 0x1000);
    g.seq_count = 0;
    g.seq_stage = 0;
}

/*
 * @routine 08:C60F CarControlsToRecord
 * WHAT IT DOES: Turns a player's buttons into the car's controls in its
 * record; or, with bit 2 of +$25, only marks the record.
 */
void CarControlsToRecord(Object *o, uint16_t held, uint16_t prev, int player)
{
    uint16_t rec = o->record, v;

    if (o->car.state & CAR_LOOSE) {
        if (rec) {
            rec_or(rec, FXREC_FLAGS40, 0x0200);
            v = rec_get(rec, FXREC_SIDES);
            if (!(v & 0x1000))
                rec_put(rec, FXREC_SIDES, (uint16_t)(v & 0xD800));
        }
        return;
    }
    if (!rec || (rec_get(rec, FXREC_FLAGS1C) & 0x0010))
        return;
    if (held & 0x2000) {
        RecordButtonSequence(rec, held);
        if (!(prev & 0x2000)) {
            Object *linked = obj_at(o->link);

            linked->director.state |= DIRECTOR_CUE;
            if ((held & 0x0020) && (held & 0x0010))
                linked->director.state |= DIRECTOR_BACK;
        }
    }
    if (g.split_screen) {
        uint16_t limit = (uint16_t)((RandomStep() & 0x0003) + 0x006E);

        if (!(o->car.idle_frames & 0x8000)) {
            if ((uint16_t)(o->car.idle_frames - limit) & 0x8000) {
                o->car.idle_frames++;
                if (!((uint16_t)(o->car.idle_frames - 0x000A) & 0x8000)
                    && (held & 0x8000 || held & 0x4000 || held & 0x0080 || held & 0x0040))
                    o->car.idle_frames = 0xFFFF;
            } else if (!(rec_get(rec, FXREC_FLAGS64) & 0x0008)) {
                Object *linked = obj_at(o->link);

                rec_or(rec, FXREC_FLAGS64, 0x0008);
                linked->director.state |= DIRECTOR_CUE;
                linked->director.state |= DIRECTOR_BACK;
            }
        }
    }
    if (g.freeze)
        return;
    rec_and(rec, FXREC_FLAGS40, 0xFDFF);
    v = rec_get(rec, FXREC_SIDES);
    if (v & 0x1000)
        return;
    rec_put(rec, FXREC_SIDES, (uint16_t)(v & 0xD800));
    if (held & 0x0200)
        rec_or(rec, FXREC_SIDES, 0x0001);
    else if (held & 0x0100)
        rec_or(rec, FXREC_SIDES, 0x0002);
    if (held & 0x0020)
        rec_or(rec, FXREC_SIDES, 0x0004);
    else if (held & 0x0010)
        rec_or(rec, FXREC_SIDES, 0x0008);
    if (held & 0x0080) {
        rec_or(rec, FXREC_SIDES, 0x2040);
        if ((uint16_t)(rec_get(rec, FXREC_SPEED) - 0x0032) & 0x8000)
            rec_or(rec, FXREC_SIDES, 0x0020);
    }
    if ((held & 0x4000) && !rec_get(rec, FXREC_6A)) {
        if (!(g.player_flags[player] & 0x0010)) {
            SoundPort0QueuePush(0x5E);
            g.player_flags[player] |= 0x0010;
        }
    } else {
        if (held & 0x4000)
            rec_or(rec, FXREC_SIDES, 0x0400);
        g.player_flags[player] &= 0xFFEF;
    }
    if ((held & 0x0040) && !(prev & 0x0040) && !rec_get(rec, FXCAR_JUMP_TIMER) && !(rec_get(rec, FXREC_FLAGS40) & 0x0100))
        rec_or(rec, FXREC_SIDES, 0x0080);
    if (held & 0x8000)
        rec_or(rec, FXREC_SIDES, 0x0010);
    if (rec_get(rec, FXREC_FLAGS40) & 0x0100) {
        if (held & 0x0400)
            rec_put(rec, 0x0026, (uint16_t)(rec_get(rec, 0x0026) - 0x0600));
        if (held & 0x0800)
            rec_put(rec, 0x0026, (uint16_t)(rec_get(rec, 0x0026) + 0x0600));
    }
}

/*
 * @routine 08:C5A5 PlayerControlsFrame
 * WHAT IT DOES: The players' controls for a race frame: each player's
 * buttons to their car's record, then the linked handler by their race
 * phase; this frame's buttons kept as last frame's.
 */
void PlayerControlsFrame(void)
{
    for (int p = 0; p < (g.split_screen ? 2 : 1); p++) {
        Object *car = obj_at(p ? g.player2_car : g.player_car);

        g.current_player = (uint8_t)p;
        g.control_held = frame_state.held[p];
        g.control_prev = g.race_last[p];
        CarControlsToRecord(car, g.control_held, g.control_prev, p);
        ObjectLinkedHandlerByState(car, g.player_phase[p]);
    }
    for (int p = 0; p < 2; p++) {
        g.race_last[p] = frame_state.held[p];
        frame_state.held[p] = 0;
    }
}

/*
 * @routine 08:C9A9 ObjectPoseFromRecord
 * WHAT IT DOES: Takes the object's pose back from its record: place and
 * the angle bytes +$27, +$29, +$25 to pitch, roll and heading; copies the
 * record's +$40 to +$90. When its +$36 is 1 it becomes 2 and bit 10 of the
 * record's flags is set, else that bit is cleared.
 */
void ObjectPoseFromRecord(Object *o)
{
    uint16_t rec;

    o->car.state &= (uint8_t)~(CAR_JOLTED | CAR_STARTLED);
    if (o->timer == 1) {
        o->timer = 2;
        rec_or(o->record, FXREC_FLAGS16, 0x0400);
    } else {
        rec_and(o->record, FXREC_FLAGS16, 0xFBFF);
    }
    rec = o->record;
    rec_put(rec, 0x0090, rec_get(rec, FXREC_FLAGS40));
    o->x = rec_get(rec, FXREC_POS_X);
    o->y = rec_get(rec, FXREC_POS_Y);
    o->z = rec_get(rec, FXREC_POS_Z);
    o->pitch = rec_get8(rec, 0x0027);
    o->roll = rec_get8(rec, 0x0029);
    o->heading = rec_get8(rec, 0x0025);
}

/*
 * @routine 08:CD08 GsuRecordSet2AHigh
 * WHAT IT DOES: Sets the record's +$2A to the value as the high byte, when
 * it has one.
 */
void GsuRecordSetMass(Object *o, uint8_t value)
{
    if (o->record)
        rec_put(o->record, 0x002A, (uint16_t)(value << 8));
}

