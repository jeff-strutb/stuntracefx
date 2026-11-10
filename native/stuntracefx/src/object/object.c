/*
 * object.c: the object table: the records, their lists and the map cells
 * they sit in, spawning and releasing them, running their handlers, and
 * the first group of handlers: sparkles, trails and attached pieces, the
 * cars' records.
 *
 * Not converted, nothing in the game reaches them: ObjectSet5DByKindLong
 * (08:B48A), ObjectGroupsActivateClear08 (7E:F141), ObjectMoveForwardXSign
 * and ZSign (08:B36F, 08:B394), ObjectTrailSignX and SignY (08:E96A,
 * 08:E982), ObjectAttachedSignX (08:EBF5), ObjectListRemove (08:B782),
 * ObjectListClear (08:B7C6), PlayerLinkSet27 (08:B846),
 * ObjectReleaseWhenPassed and Behind (08:B680, 08:B6B8).
 *
 * The Remaster calls of the record (rm_share and the rm_ calls in
 * ObjectApplyVelocity, ObjectInit, ObjectNearFocus, ObjectGroupsActivate,
 * ObjectsRunHandlers and ObjectPlaceOnParent) are left out for now; see
 * object/NOTES.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "data/object_tables.h"
#include "fx/fx.h"
#include "math/math.h"
#include "object/handlers.h"
#include "object/object.h"
#include "system/chip.h"
#include "system/state.h"

ObjectTable objects;
Camera cameras[2];

static uint16_t asr1(uint16_t v) { return (uint16_t)((v >> 1) | (v & 0x8000)); }

/* a two-digit BCD byte plus 1 and a carry, as the S-CPU's decimal adds do it */
static uint8_t bcd_add8(uint8_t v, uint8_t add, int *carry)
{
    unsigned lo = (v & 0x0Fu) + (add & 0x0Fu) + (unsigned)*carry;
    unsigned hi = (v >> 4) + (add >> 4);

    if (lo > 9) {
        lo += 6;
        hi++;
    }
    *carry = hi > 9;
    if (*carry)
        hi += 6;
    return (uint8_t)((hi & 0x0F) << 4 | (lo & 0x0F));
}

static uint16_t bcd_inc16(uint16_t v)
{
    int carry = 0;
    uint8_t lo = bcd_add8((uint8_t)v, 1, &carry);

    return (uint16_t)(lo | bcd_add8((uint8_t)(v >> 8), 0, &carry) << 8);
}

void ObjectSetHandler(Object *o, uint16_t handler)
{
    o->handler = handler;
    o->handler_bank = 0x08;
}

/*
 * @routine 03:89A8 ObjectLinkedSet1A
 * WHAT IT DOES: Stores the value as the handler of the object o links to.
 */
uint16_t ObjectLinkedHandlerSet(Object *o, uint16_t handler)
{
    obj_at(o->link)->handler = handler;
    return o->link;
}

/*
 * @routine 03:D214 ObjectCopy14To, 03:D227 ObjectCopy14From
 * WHAT IT DOES: Copies the three angles from one object to another.
 */
void ObjectCopyAnglesFrom(Object *to, const Object *from)
{
    to->pitch = from->pitch;
    to->heading = from->heading;
    to->roll = from->roll;
}

/*
 * @routine 03:D1E6 ObjectCopyPositionTo, 03:D1FD ObjectCopyPositionFrom
 * WHAT IT DOES: Copies one object's place to another.
 */
void ObjectCopyPosition(Object *to, const Object *from)
{
    to->x = from->x;
    to->y = from->y;
    to->z = from->z;
}

/*
 * @routine 03:C06C ObjectApplyVelocity
 * WHAT IT DOES: Moves the object by its velocity.
 */
void ObjectApplyVelocity(Object *o)
{
    o->x = (uint16_t)(o->x + o->vx);
    o->y = (uint16_t)(o->y + o->vy);
    o->z = (uint16_t)(o->z + o->vz);
}

/*
 * @routine 03:D3DE ObjectPoolInit
 * WHAT IT DOES: Links all the records into the free list and empties the
 * used list.
 */
void ObjectPoolInit(void)
{
    objects.used_head = 0;
    objects.used_last = 0;
    objects.free_head = 1;
    for (uint16_t i = 0; i < OBJECT_COUNT; i++)
        objects.object[i].next = i + 1 < OBJECT_COUNT ? (ObjRef)(1 + (i + 1) * OBJECT_SIZE) : 0;
}

/*
 * @routine 7F:12AC ObjectLinkFlag4000Step
 * WHAT IT DOES: When player 1's camera has bit 14 of its mode set, adds 8
 * to the backdrop's pitch ($06CD).
 */
void BackdropPitchForCameraMode(void)
{
    const Object *director = obj_at(obj_at(g.player_car)->link);

    if (camera_of(director->link)->mode & 0x4000)
        g.backdrop_pitch = (uint16_t)(g.backdrop_pitch + 8);
}

/*
 * @routine 08:B830 ObjectFlag40IfMatch
 * WHAT IT DOES: When the value is player 1's record, sets bit 6 of the
 * player's flags at +$25.
 */
void PlayerStartleIfRecord(uint16_t value)
{
    if (value == fxram_get(FXIN_PLAYER1_RECORD))
        obj_at(g.player_car)->car.state |= CAR_STARTLED;
}

/*
 * @routine 08:E896 ObjectTableE8AF
 * WHAT IT DOES: The byte of the table at $08:E8AF for (value + the object's
 * heading + base) / 8.
 */
uint8_t ObjectFacingFrame32(const Object *o, uint8_t base, uint8_t value)
{
    return facing_frame_32((uint8_t)(value + o->heading + base) / 8);
}

/* 0x100 | the byte ObjectPlaceOnParent last read the table at $08:E8AF by
   for o and the side's camera, while o's frame byte for that side is still
   the one that read gave (other routines write it too); 0 otherwise */
uint16_t object_gaze(const Object *o, int side)
{
    uint16_t gz = objects.gaze[obj_slot(o)][side];

    if (!gz || o->sprite_turn[side] != (uint8_t)(facing_frame_32((uint8_t)gz / 8) | 0x80))
        return 0;
    return gz;
}

/*
 * @routine 08:C03C ObjectTableC056
 * WHAT IT DOES: The byte of the table at $08:C056 for (the object's heading
 * - turn) / 4.
 */
uint8_t ObjectFacingFrame64(const Object *o, uint8_t turn)
{
    return facing_frame_64((uint8_t)(o->heading - turn) / 4);
}

/*
 * @routine 08:B75B ObjectListAppend
 * WHAT IT DOES: Adds the object to the zero-ended list of up to 32; a full
 * list is left alone. Returns whether it was added.
 */
int ObjectListAppend(Object *o)
{
    if (objects.list_length == 0x0040)
        return 0;
    objects.list[objects.list_length / 2] = obj_ref(o);
    objects.list_length = (uint16_t)(objects.list_length + 2);
    objects.list[objects.list_length / 2] = 0;
    return 1;
}

/*
 * @routine 08:8364 ObjectBucketInsert
 * WHAT IT DOES: Puts the object at the head of its map cell's list.
 */
void ObjectBucketInsert(Object *o)
{
    uint16_t head = objects.bucket_head[o->bucket / 2];

    o->bucket_prev = 0;
    o->bucket_next = head;
    if (head)
        obj_at(head)->bucket_prev = obj_ref(o);
    objects.bucket_head[o->bucket / 2] = obj_ref(o);
}

/*
 * @routine 08:8392 ObjectBucketRemove
 * WHAT IT DOES: Takes the object out of its map cell's list.
 */
void ObjectBucketRemove(Object *o)
{
    uint16_t prev = o->bucket_prev, next = o->bucket_next;

    if (prev) {
        obj_at(prev)->bucket_next = next;
        if (next)
            obj_at(next)->bucket_prev = prev;
        return;
    }
    objects.bucket_head[o->bucket / 2] = next;
    if (next)
        obj_at(next)->bucket_prev = 0;
}

/*
 * @routine 08:83CC ObjectRebucket
 * WHAT IT DOES: Recomputes the object's map cell from its place and, when it
 * changed, moves the object to the new cell's list.
 */
void ObjectRebucket(Object *o)
{
    uint16_t x = o->x, z = o->z, key = AreaCellOf(&x, &z);

    if (key == o->bucket)
        return;
    ObjectBucketRemove(o);
    o->bucket = key;
    ObjectBucketInsert(o);
}

/*
 * @routine 08:D5AB ObjectVelocityFromRecord
 * WHAT IT DOES: Sets the X and Z velocity to an eighth of those of the
 * record of the object named by +$27.
 */
void ObjectVelocityFromRecord(Object *o)
{
    uint16_t rec = obj_at(o->break_off.car)->record;

    o->vx = asr1(asr1(asr1(rec_get(rec, FXREC_VEL_X))));
    o->vz = asr1(asr1(asr1(rec_get(rec, FXREC_VEL_Z))));
}

/*
 * @routine 08:B48E ObjectSet5DByKind
 * WHAT IT DOES: Sets the animation word by the kind: 1, 2 or 3, or any other.
 */
uint16_t ObjectCarLookByKind(Object *o, uint8_t kind)
{
    o->anim = kind == 1 ? look_word(LOOK_CAR_COUPE) : kind == 2 ? look_word(LOOK_CAR_4WD) : kind == 3 ? look_word(LOOK_CAR_2WD) : look_word(LOOK_CAR_F_TYPE);
    return o->anim;
}

/*
 * @routine 08:B8F9 ObjectPlaceFromTable
 * WHAT IT DOES: Sets up the object at place `list` (from 1) of the course's
 * starters: its control word, and (unless the cars keep their places) its
 * place and heading from entry `entry` (from 1) of the start table.
 */
static void ObjectPlaceFromTable(uint8_t list, uint8_t entry, uint16_t word_2b, uint16_t word_2d, uint16_t word_4c)
{
    Object *o = obj_at(objects.place_objects[list - 1]);
    uint16_t *e = objects.place_table[(uint8_t)(entry - 1)];

    o->car.car_script = word_2d;
    if (!(g.course_options & 1)) {
        o->x = e[0];
        o->y = e[1];
        o->z = e[2];
        o->heading = (uint8_t)e[3];
    }
    o->car.kind = (uint8_t)word_2b;
    o->car.attachments = (uint8_t)word_4c;
}

/*
 * @routine 08:B8B4 ObjectPlaceAll
 * WHAT IT DOES: Sets up every starter of the course by ObjectPlaceFromTable.
 */
void ObjectPlaceAll(void)
{
    uint16_t n = asr1(objects.place_count);
    uint8_t list = 1;
    unsigned i = 0;

    if (!n) {                           /* no course places none; the original would run on 65,536 times */
        g.place_list_next = 1;          /* and end with its count back at 1 */
        return;
    }
    do {
        ObjectPlaceFromTable(list++, (uint8_t)objects.place_entries[i], g.selection_out[i],
                             g.selection_handler[i], g.selection_derived[i]);
        i++;
        n--;
    } while (n);
    g.place_list_next = list;
}

/*
 * @routine 08:E760 ObjectCountStep5F
 * WHAT IT DOES: Counts +$49 up; while it is within `limit` +$5F rises by
 * `step`, then falls, and the routine returns true when the count reaches
 * `end`.
 */
int ObjectSizeRiseFall(Object *o, uint8_t limit, uint8_t end, uint8_t step)
{
    uint8_t n = ++o->eye.size_count;

    if (n == limit || ((uint8_t)(n - limit) & 0x80)) {
        o->size = (uint8_t)(o->size + step);
        return 0;
    }
    o->size = (uint8_t)(o->size - step);
    return n == end;
}


/*
 * @routine 08:B5FA ObjectNearFocus
 * WHAT IT DOES: Keeps a car near: clears bit 3 of +$24, zeroes +$5F and sets
 * the animation by its kind. The original turned it far, a sprite (bit 3,
 * +$5F = $32, the far animation word at $99A3 by its kind, a direction
 * into the sprite turn), once camera 1 was $1194 or more away along x and z;
 * the game draws it as a model however far, as the remaster's Enhanced did.
 */
void ObjectNearFocus(Object *o)
{
    o->draw_flags &= 0xF7;
    o->size = 0;
    ObjectCarLookByKind(o, o->car.kind);
}

/*
 * @routine 08:B5B3 ObjectRecordFlag70Step
 * WHAT IT DOES: While the race is not frozen and +$48 still counts, counts
 * it down; otherwise sets bit 2 of the record's word +$70 when frozen,
 * clears it when not.
 */
void ObjectRecordHoldWhileFrozen(Object *o)
{
    if (!g.freeze && o->car.wait) {
        o->car.wait--;
        return;
    }
    if (!o->record)
        CrashWhite();
    if (g.freeze)
        rec_or(o->record, FXREC_DIRECTION, 0x0004);
    else
        rec_and(o->record, FXREC_DIRECTION, 0xFFFB);
}

/*
 * @routine 08:B4C6 ObjectLinkedHandlerByState
 * WHAT IT DOES: Once per object (bit 3 of +$25), gives the object it links
 * to a new handler by the state: 8 by the record's rank, $0A and $0C by
 * $0E59, the rest by $19ED.
 */
void ObjectLinkedHandlerByState(Object *o, uint8_t state)
{
    uint16_t handler;

    if (o->car.state & CAR_EVENT_TAKEN)
        return;
    if (state == 0x08) {
        rec_or(o->record, FXREC_FLAGS64, 0x0008);
        rec_or(o->record, FXREC_DIRECTION, 0x0020);
        switch ((uint8_t)rec_get(o->record, FXREC_RANK)) {
        case 0x01: handler = H_CAMERA_DRIFT; break;
        case 0x04: handler = H_CAMERA_RISE; break;
        default: handler = H_CAMERA_DRIFT_LONG; break;
        }
    } else if (state == 0x0C || state == 0x0A) {
        rec_and(o->record, FXREC_SIDES, 0xFBCF);
        if (!g.time_up_shown)
            return;
        handler = H_CAMERA_ORBIT;
    } else {
        if (g.stunt_finished != 0x08)
            return;
        handler = H_CAMERA_STILL;
    }
    ObjectSetHandler(obj_at(o->link), handler);
    if (!(g.course_options & 1))
        GsuRecordStarterRanks();
    o->car.state |= CAR_EVENT_TAKEN;
}

/*
 * @routine 08:8525 ObjectAttachLinked
 * WHAT IT DOES: Links object o to the other object, whose camera block is
 * set up: kind 5, tied to o, its +$32 from the table at $09:8090, the rest
 * cleared. The table entry is the word at $70:2396 plus 2 for a kind-$8E2A
 * object, or 4, 8 or 6 for HUD mode 1, 2 or 3.
 */
void ObjectAttachLinked(Object *o, Object *other)
{
    Camera *cam = camera_of(other->link);
    uint16_t t = fxram_get(FX_CAR_CAMERA_POINTS);

    o->link = obj_ref(other);
    other->parent = obj_ref(o);
    cam->object = obj_ref(o);
    if (other->handler == H_DIRECTOR_PATH_START)
        t = (uint16_t)(t + 2);
    else if (g.hud_mode == 1)
        t = (uint16_t)(t + 4);
    else if (g.hud_mode == 2)
        t = (uint16_t)(t + 8);
    else if (g.hud_mode == 3)
        t = (uint16_t)(t + 6);
    cam->word32 = object_list_word(t);
    cam->word35 = cam->word37 = cam->word39 = 0;
    memset(cam->turn_words, 0, sizeof cam->turn_words);
    cam->mode = 0x0005;
    cam->byte34 = 0;
}

/*
 * @routine 7E:F0E1 ObjectGroupsActivate
 * WHAT IT DOES: For each trigger of the list at $1AF5, sets +$36 of every
 * object in its bucket to 3, skipping objects that are negative, and for
 * code $23 those with bit 4 of +$24.
 */
void ObjectGroupsActivate(void)
{
    for (unsigned t = 0; g.trigger_list[t][0]; t++) {
        uint16_t code = g.trigger_list[t][0];
        ObjRef r = objects.bucket_head[(g.trigger_list[t][1] & 0x07FE) / 2];

        while (r) {
            Object *o = obj_at(r);

            if (!((uint8_t)code == 0x23 && (o->draw_flags & 0x10)) && !(o->timer & 0x80))
                o->timer = 3;
            r = o->bucket_next;
        }
    }
    ObjectsKeepRunning();               /* the far ones that move only themselves (animate.c) */
}

/*
 * @routine 7E:C58D BucketTriggersFromArea
 * WHAT IT DOES: Builds the trigger list from a rectangle of map cells: for
 * each cell inside the window and not $2E, an entry of its code and its
 * bucket number.
 */
static void BucketTriggersFromArea(TriggerArea area)
{
    uint16_t row = (uint16_t)(((uint16_t)((area.top << 8) >> 2)) | (area.top & 0x80 ? 0xC000 : 0));
    uint16_t col0 = (uint16_t)-(uint16_t)(area.left << 1);
    const uint8_t *cell = area.cells;
    unsigned at = 0;

    for (unsigned rows = area.rows; rows; rows--) {
        uint16_t col = col0;

        for (unsigned cols = area.cols; cols; cols--, col = (uint16_t)(col + 2)) {
            uint8_t c = *cell++;

            if (((uint16_t)(col + g.bucket_origin[1]) & 0xFFC0) || ((uint16_t)(row + g.bucket_origin[2]) & 0xF800) || c == 0x2E)
                continue;
            g.trigger_list[at][0] = c;
            g.trigger_list[at][1] = (uint16_t)(g.bucket_origin[0] + col + row);
            at++;
        }
        row = (uint16_t)(row - 0x0040);
    }
    g.trigger_list[at][0] = 0;
    g.trigger_list[at][1] = 0;
}

/*
 * @routine 7E:C520 BucketTriggersFromView
 * WHAT IT DOES: Builds the trigger list from the area the view's height
 * selects, with the window from the view's origin.
 */
void BucketTriggersFromView(void)
{
    uint16_t at = (uint16_t)((((g.view_angles[1] >> 8 & 0xFF) >> 3) + 1) & 0xFE);
    memcpy(g.bucket_origin, g.view_area_origin, sizeof g.bucket_origin);
    BucketTriggersFromArea(trigger_area(trigger_area_for_height(at)));
}

/*
 * @routine 7E:C553 BucketTriggersFixedArea
 * WHAT IT DOES: Builds the trigger list from the fixed area records, chosen
 * by split screen or by bit 7 of the course's flags.
 */
void BucketTriggersFixedArea(void)
{
    memcpy(g.bucket_origin, g.view_fixed_origin, sizeof g.bucket_origin);
    BucketTriggersFromArea(trigger_area(trigger_area_fixed(g.split_screen || (g.course_flags & 0x0080) ? TRIGGER_FIXED_SPLIT : TRIGGER_FIXED_SINGLE)));
}

/*
 * @routine 03:CB5C ObjectAlloc
 * WHAT IT DOES: Takes a record from the free list and links it into the
 * used list after the last one taken. Returns 0 when none is free.
 */
ObjRef ObjectAlloc(void)
{
    ObjRef r = objects.free_head, last = objects.used_last;
    Object *o;

    if (!r)
        return 0;
    o = obj_at(r);
    objects.free_head = o->next;
    if (!last) {
        o->next = objects.used_head;
        o->prev = 0;
        objects.used_head = r;
    } else {
        o->next = obj_at(last)->next;
        obj_at(last)->next = r;
        o->prev = last;
    }
    objects.used_last = r;
    if (o->next)
        obj_at(o->next)->prev = r;
    return r;
}

/*
 * @routine 03:CB25 ObjectInit
 * WHAT IT DOES: Clears the record from +$04 on, puts it in bucket 0's list
 * and sets its +$36 to 0 and its +$59 to $69.
 */
void ObjectInit(Object *o)
{
    objects.slot_changes[obj_slot(o)]++;
    objects.gaze[obj_slot(o)][0] = objects.gaze[obj_slot(o)][1] = 0;
    ObjectMade(o);
    memset(&o->model, 0, sizeof(Object) - offsetof(Object, model));
    ObjectBucketInsert(o);
    o->made_mark = 0x69;
}

/*
 * @routine 03:CCBB ObjectSpawn
 * WHAT IT DOES: Unless the frame's records have reached $2E, takes and
 * clears a new object, with +$36 = 3 and the model given. Returns it, or
 * NULL when none was made.
 */
Object *ObjectSpawn(uint16_t model)
{
    ObjRef r;
    Object *o;

    if (!((uint8_t)(g.frame_record_count - 0x2E) & 0x80))
        return NULL;
    r = ObjectAlloc();
    if (!r)
        return NULL;
    o = obj_at(r);
    ObjectInit(o);
    o->timer = 3;
    o->model = model;
    return o;
}

/*
 * @routine 03:B8A5 ObjectFree
 * WHAT IT DOES: Gives the object back: out of its bucket and the used list
 * and to the head of the free list.
 */
void ObjectFree(Object *o)
{
    ObjRef r = obj_ref(o), prev = o->prev, next = o->next;

    objects.slot_changes[obj_slot(o)]++;
    objects.gaze[obj_slot(o)][0] = objects.gaze[obj_slot(o)][1] = 0;
    ObjectBucketRemove(o);
    if (!prev) {
        objects.used_head = next;
        /* the original also clears the following record's +$02 with no next */
        if (next)
            obj_at(next)->prev = 0;
        objects.used_last = 0;
    } else {
        obj_at(prev)->next = next;
        if (next)
            obj_at(next)->prev = prev;
        else
            objects.used_last = prev;
    }
    o->next = objects.free_head;
    objects.free_head = r;
}

/*
 * @routine 03:CCA1 ObjectRelease
 * WHAT IT DOES: Frees the object, unless it is the object whose handler is
 * running, which instead counts one up in free_requests for the run loop to free
 * afterwards. Also the F927 handler (ObjectReleaseLong, 08:F927) and
 * ObjectReleaseY (03:CCB0).
 */
void ObjectRelease(Object *o)
{
    if (obj_ref(o) == g.handler_object)
        objects.free_requests++;
    else
        ObjectFree(o);
}

/*
 * @routine 08:AB46 ObjectCallHandler
 * WHAT IT DOES: Runs the object's handler.
 */
void ObjectCallHandler(Object *o)
{
    object_handler_call(o, (uint32_t)o->handler_bank << 16 | o->handler);
}

/* Run a handler as the run loop does: the object noted for ObjectRelease */
static void run_handler(Object *o, uint16_t word, uint8_t bank)
{
    if (!word)
        return;
    g.handler_object = obj_ref(o);
    object_handler_call(o, (uint32_t)bank << 16 | word);
}

/*
 * @routine 03:CAB8 ObjectRunHandlerA
 * WHAT IT DOES: Runs the object's first handler, unless its word is 0.
 */
void ObjectRunHandlerA(Object *o)
{
    run_handler(o, o->handler, o->handler_bank);
}

/*
 * @routine 03:CAEB ObjectRunHandlerB
 * WHAT IT DOES: Runs the object's second handler, unless its word is 0 or
 * the object has a record.
 */
static void ObjectRunHandlerB(Object *o)
{
    if (!o->handler2 || o->record)
        return;
    run_handler(o, o->handler2, o->handler2_bank);
}

/*
 * @routine 03:D388 ObjectsRunHandlers
 * WHAT IT DOES: The objects' frame: counts the frame, then for each object
 * of the used list steps its countdown +$36: a negative one restarts at 3
 * and runs the second handler; one above 1 counts down and runs the first;
 * one that reaches 1 clears the record and itself. An object a handler
 * asked to free is freed once its next link is read.
 */
void ObjectsRunHandlers(void)
{
    ObjRef r = objects.used_head;

    g.object_frames++;
    ObjectsStepStart();
    while (r) {
        Object *o = obj_at(r);
        uint8_t v = o->timer;

        objects.free_requests = 0;
        if (v && ((v & 0x80) || v != 1)) {
            if (v & 0x80) {
                o->timer = 3;
                ObjectRunHandlerB(o);
            } else {
                o->timer = (uint8_t)(v - 1);
                ObjectRunHandlerA(o);
            }
            ObjectTurn(o, 1);
            if (objects.free_requests) {
                ObjRef next = o->next;

                ObjectFree(o);
                r = next;
                continue;
            }
        } else {
            ObjectTurn(o, 0);
            if (v) {
                o->record = 0;
                o->timer = 0;
            }
        }
        r = o->next;
    }
}

/*
 * @routine 08:826E ObjectSpawnFromScript
 * WHAT IT DOES: Makes an object from the course script entry `entry`:
 * model, script word, place, handler, flags, offsets (added to those of the
 * record +$20 names when bit 9 of the flags is set; X and Z swapped when the
 * spawn heading is not 0 or $80), then its bucket, and +$36 = 3 when its
 * handler starts with a NOP.
 */
Object *ObjectSpawnFromScript(CartRef entry)
{
    ObjRef r = ObjectAlloc();
    Object *o;
    uint16_t w;

    if (!r) {
        CrashGreen();
        return NULL;
    }
    o = obj_at(r);
    ObjectInit(o);
    o->heading = g.spawn_mirror;
    o->x = course_script_word(entry, ENTRY_X);
    o->y = course_script_word(entry, ENTRY_Y);
    o->z = course_script_word(entry, ENTRY_Z);
    o->model = course_script_word(entry, ENTRY_MODEL);
    o->script = course_script_word(entry, ENTRY_SCRIPT);
    o->handler = course_script_word(entry, ENTRY_HANDLER);
    o->handler_bank = course_script_byte(entry, ENTRY_HANDLER_BANK);
    w = course_script_word(entry, ENTRY_FLAGS);
    o->script_flags = w;
    if (w & 0x0200) {
        if (o->script) {                /* the model record's offsets (the data's are all from $8000) */
            o->offset_x = model_record_offset(o->script, 0);
            o->offset_y = model_record_offset(o->script, 1);
            o->offset_z = model_record_offset(o->script, 2);
        } else {                        /* none: the original reads work RAM $000A-$000F there */
            o->offset_x = g.last_worked_y;
            o->offset_y = 0;            /* $000C-$000F: written only by the power-on clear */
            o->offset_z = 0;
        }
    }
    o->offset_x = (uint16_t)(o->offset_x + course_script_word(entry, ENTRY_OFFSET_X));
    o->offset_y = (uint16_t)(o->offset_y + course_script_word(entry, ENTRY_OFFSET_Y));
    o->offset_z = (uint16_t)(o->offset_z + course_script_word(entry, ENTRY_OFFSET_Z));
    if (g.spawn_mirror != 0x00 && g.spawn_mirror != 0x80) {
        uint16_t t = o->offset_x;

        o->offset_x = o->offset_z;
        o->offset_z = t;
    }
    ObjectRebucket(o);
    if (handler_runs_from_start((uint32_t)o->handler_bank << 16 | o->handler))
        o->timer = 3;
    return o;
}

/*
 * @routine 09:B0A1 ObjectStep48To8
 * WHAT IT DOES: Counts +$48 up to 8, and shows it (with bit 7) as the frame.
 */
void ObjectFrameCountTo8(Object *o)
{
    if (o->meter.frame != 8)
        o->meter.frame++;
    o->shown_frame = (uint8_t)(o->meter.frame | 0x80);
}

/*
 * @routine 08:D181 ObjectSpawnChanceD460
 * WHAT IT DOES: With a chance of 78 in 256, makes a new object of the
 * common model with handler $D460 at the object's place, tied to it.
 */
void ObjectPieceSprayChance(Object *o)
{
    Object *n;

    if (RandomStep() < 0xB2)
        return;
    n = ObjectSpawn(model_word(MODEL_COMMON));
    if (!n)
        return;
    ObjectSetHandler(n, H_PIECE_SPRAY);
    ObjectCopyPosition(n, o);
    n->break_off.car = obj_ref(o);
}

/*
 * @routine 08:D14A ObjectSpawnD460UnlessFlag
 * WHAT IT DOES: As ObjectPieceSprayChance without the chance, unless split.
 */
void ObjectPieceSprayUnlessSplit(Object *o)
{
    Object *n;

    if (g.split_screen)
        return;
    n = ObjectSpawn(model_word(MODEL_COMMON));
    if (!n)
        return;
    ObjectSetHandler(n, H_PIECE_SPRAY);
    ObjectCopyPosition(n, o);
    n->break_off.car = obj_ref(o);
}

/*
 * @routine 08:F911 ObjectAdvance5DIfFlag
 * WHAT IT DOES: Adds 2 to the animation word when the screen is split.
 */
void ObjectLookForSplit(Object *o)
{
    if (g.split_screen)
        o->anim = look_word_split(o->anim);
}

static void spawn_sparkle(Object *o)
{
    Object *n = ObjectSpawn(model_word(MODEL_COMMON));

    if (!n)
        return;
    ObjectSetHandler(n, H_SPARKLE_START);
    n->parent = obj_ref(o);
    ObjectCopyPosition(n, o);
    ObjectRebucket(n);
}

/*
 * @routine 08:E9F4 ObjectSpawnSparklePair
 * WHAT IT DOES: Makes two sparkle objects at the object, each with it as
 * parent.
 */
void ObjectSpawnSparklePair(Object *o)
{
    spawn_sparkle(o);
    spawn_sparkle(o);
}

/*
 * @routine 08:EB07 ObjectSparkleStep
 * WHAT IT DOES: A sparkle's frame: on its parent's place, moved, its
 * velocity accelerated; released when its count runs out.
 */
void ObjectSparkleStep(Object *o)
{
    ObjectCopyPosition(o, obj_at(o->parent));
    ObjectApplyVelocity(o);
    o->vx = (uint16_t)(o->vx + o->sparkle.accel_x);
    o->vy = (uint16_t)(o->vy + o->sparkle.accel_y);
    o->vz = (uint16_t)(o->vz + o->sparkle.accel_z);
    if (!o->sparkle.frames) {
        ObjectRelease(o);
        return;
    }
    o->sparkle.frames--;
    ObjectRebucket(o);
}

/*
 * @routine 08:EA6B ObjectSparkleStart
 * WHAT IT DOES: A sparkle's first frame: its look and count, a random
 * acceleration, then its first step.
 */
void ObjectSparkleStart(Object *o)
{
    uint8_t r;

    o->draw_flags |= 0x08;
    o->anim = look_word(LOOK_SPARKLE);
    r = RandomStep();
    o->sprite_turn[0] = (uint8_t)((r & 0x03) | 0x80);
    ObjectSetHandler(o, H_SPARKLE_STEP);
    o->size = 0x0A;
    o->sparkle.frames = 0x05;
    o->sparkle.accel_x = (uint16_t)(o->sparkle.accel_x + (RandomStep() & 0x7F));
    o->sparkle.accel_y = (uint16_t)(o->sparkle.accel_y + (RandomStep() & 0x3F));
    o->sparkle.accel_z = (uint16_t)(o->sparkle.accel_z + (RandomStep() & 0x7F));
    o->sparkle.accel_x = (uint16_t)(o->sparkle.accel_x - 0x3F);
    o->sparkle.accel_y = (uint16_t)-o->sparkle.accel_y;
    o->sparkle.accel_z = (uint16_t)(o->sparkle.accel_z - 0x3F);
    ObjectSparkleStep(o);
}

/* three-step turn by an object's angles: roll, pitch, heading */
Vec16 ObjectTurnVector(const Object *o, int8_t x, int8_t y, int8_t z)
{
    Vec16 v = {x, y, z};

    RotateXY((int8_t)v.x, (int8_t)v.y, o->roll, &v.x, &v.y);
    RotateXZFrom08((int8_t)v.y, (int8_t)v.z, o->pitch, &v.y, &v.z);
    RotateXZ((int8_t)v.x, (int8_t)v.z, o->heading, &v.x, &v.z);
    return v;
}

/*
 * @routine 08:B34B ObjectMoveForward
 * WHAT IT DOES: Moves the object forward by the speed from the table at
 * $08:B32B for its record's +$5E, as (0, speed, 0) turned by its angles and
 * times 4.
 */
void ObjectMoveForward(Object *o)
{
    uint8_t speed = jump_rise(rec_get(o->record, FXCAR_JUMP_TIMER));
    Vec16 v = ObjectTurnVector(o, 0, (int8_t)speed, 0);

    o->x = (uint16_t)(o->x + (uint16_t)(v.x * 4));
    o->z = (uint16_t)(o->z + (uint16_t)(v.z * 4));
    o->y = (uint16_t)(o->y + (uint16_t)(v.y * 4));
}

/* The object at its parent's (+$27) place plus (0, y, z) turned by the
   parent's pitch and heading and doubled, then rebucketed. */
static void parent_place(Object *o, int8_t y, int8_t z)
{
    Object *p = obj_at(o->emitter.source);
    Vec16 v = {0, y, z};

    RotateXZFrom08((int8_t)v.y, (int8_t)v.z, p->pitch, &v.y, &v.z);
    RotateXZ((int8_t)v.x, (int8_t)v.z, p->heading, &v.x, &v.z);
    o->x = (uint16_t)(p->x + (uint16_t)(v.x * 2));
    o->z = (uint16_t)(p->z + (uint16_t)(v.z * 2));
    o->y = (uint16_t)(p->y + (uint16_t)(v.y * 2));
}

/*
 * @routine 08:E8CF ObjectSpawnTrail
 * WHAT IT DOES: Unless split, makes a trail object tied to this one.
 */
void ObjectSpawnTrail(Object *o)
{
    Object *n;

    if (g.split_screen)
        return;
    n = ObjectSpawn(model_word(MODEL_COMMON));
    if (!n)
        return;
    n->draw_flags |= 0x04;
    n->emitter.kind = 0x03;
    n->emitter.count = 0x14;
    n->emitter.effect_x = 0x0A;
    ObjectSetHandler(n, H_TRAIL_START);
    n->emitter.source = obj_ref(o);
}

static void trail_place(Object *o)
{
    parent_place(o, (int8_t)0xF6, (int8_t)(uint8_t)o->vz);
    o->vz = (uint16_t)((o->vz & 0xFF00) | (uint8_t)(o->vz + 0xFB));
    ObjectRebucket(o);
}

/*
 * @routine 08:E919 ObjectTrailStart
 * WHAT IT DOES: A trail's first frame: its count, handler, offset, then
 * placed by its parent.
 */
void ObjectTrailStart(Object *o)
{
    o->emitter.life = 0x0C;
    ObjectSetHandler(o, H_TRAIL_STEP);
    o->vz = (uint16_t)((o->vz & 0xFF00) | 0xD8);
    trail_place(o);
}

/* the frame's look of a trail or attached piece, then its count */
static int trail_tick(Object *o)
{
    o->draw_flags |= 0x04;
    o->emitter.kind = 0;
    o->emitter.count = 0;
    o->emitter.effect_x = 0;
    if (!o->emitter.life) {
        ObjectRelease(o);
        return 0;
    }
    o->emitter.life--;
    return 1;
}

/*
 * @routine 08:E934 ObjectTrailStep
 * WHAT IT DOES: A trail's frame: held on its parent, released when its
 * count runs out.
 */
void ObjectTrailStep(Object *o)
{
    if (trail_tick(o))
        trail_place(o);
}

/*
 * @routine 08:EB4D ObjectSpawnAttached
 * WHAT IT DOES: Unless split, makes an attached piece tied to this object
 * at the offset (0, y, z) kept in its velocity bytes.
 */
void ObjectSpawnAttached(Object *o, uint8_t y_off, uint8_t z_off)
{
    Object *n;

    if (g.split_screen)
        return;
    n = ObjectSpawn(model_word(MODEL_COMMON));
    if (!n)
        return;
    n->draw_flags |= 0x04;
    n->emitter.kind = 0x02;
    n->emitter.count = 0x05;
    n->emitter.effect_x = 0x0A;
    ObjectSetHandler(n, H_ATTACHED_START);
    n->emitter.source = obj_ref(o);
    n->vy = (uint16_t)((n->vy & 0xFF00) | y_off);
    n->vz = (uint16_t)((n->vz & 0xFF00) | z_off);
}

static void attached_place(Object *o)
{
    parent_place(o, (int8_t)(uint8_t)o->vy, (int8_t)(uint8_t)o->vz);
    ObjectRebucket(o);
}

/*
 * @routine 08:EBA3 ObjectAttachedStart
 * WHAT IT DOES: An attached piece's first frame.
 */
void ObjectAttachedStart(Object *o)
{
    o->emitter.life = 0x0C;
    ObjectSetHandler(o, H_ATTACHED_STEP);
    attached_place(o);
}

/*
 * @routine 08:EBB9 ObjectAttachedStep
 * WHAT IT DOES: An attached piece's frame.
 */
void ObjectAttachedStep(Object *o)
{
    if (trail_tick(o))
        attached_place(o);
}

/*
 * @routine 08:E798 ObjectPlaceOnParent
 * WHAT IT DOES: Puts object o on the parent: the sprite's turn bytes from
 * the table by each camera's turn (or $8C with bit 0 of +$25), then the
 * place: the parent's plus o's offset (its velocity bytes) turned by the
 * parent's angles and doubled.
 */
void ObjectPlaceOnParent(Object *parent, Object *o)
{
    Vec16 v;

    if (o->eye.state & EYE_BOUNCING) {
        o->sprite_turn[0] = o->sprite_turn[1] = 0x8C;
        objects.gaze[obj_slot(o)][0] = objects.gaze[obj_slot(o)][1] = 0;
    } else {
        uint16_t lift = parent->record ? (uint16_t)(rec_get(parent->record, FXCAR_STEER_BIAS) << 2) : 0;
        uint8_t base = (uint8_t)((uint8_t)(lift >> 8) + o->eye.turn);

        for (int i = 0; i < 2; i++) {
            uint8_t turn = (uint8_t)(cameras[i].turn >> 8);

            o->sprite_turn[i] = (uint8_t)(ObjectFacingFrame32(parent, base, turn) | 0x80);
            objects.gaze[obj_slot(o)][i] = (uint16_t)(0x100 | (uint8_t)(turn + parent->heading + base));
        }
    }
    v = ObjectTurnVector(parent, (int8_t)(uint8_t)o->vx, (int8_t)(uint8_t)o->vy, (int8_t)(uint8_t)o->vz);
    o->x = (uint16_t)(parent->x + (uint16_t)(v.x * 2));
    o->z = (uint16_t)(parent->z + (uint16_t)(v.z * 2));
    o->y = (uint16_t)(parent->y + (uint16_t)(v.y * 2));
    g.last_worked_y = (uint16_t)(v.y * 2);
}

/*
 * The camera record the original reads at work RAM $0000, where a link of 0
 * leads: in a race's first step, before the cars are linked. Its words are
 * what was last left there (traced in race1): $0000 the race step's flag
 * ($00, high byte never written), $0002 ObjectPlaceAll's count (0 when it
 * ends), $0008 ObjectPlaceAll's starter number, $000A the last worked Y,
 * $000C-$0013 only the power-on clear.
 */
static Camera low_ram_camera;

Camera *camera_at_link(uint16_t link)
{
    static int reported;

    if (link == LINK_CAMERA_1 || link == LINK_CAMERA_2)
        return camera_of(link);
    if (link && !reported) {
        reported = 1;
        fprintf(stderr, "stuntracefx: a link to $%04X read as a camera; the work RAM camera used\n", link);
    }
    memset(&low_ram_camera, 0, sizeof low_ram_camera);
    low_ram_camera.mode = g.frame_stage_00;
    low_ram_camera.object = 0;
    low_ram_camera.x = g.place_list_next;
    low_ram_camera.y = g.last_worked_y;
    return &low_ram_camera;
}

int camera_in_low_ram(const Camera *c) { return c == &low_ram_camera; }

/* the view bob a car's cue starts, for one of the two cameras */
static void view_event_trigger(int i, ObjRef car)
{
    const Object *director;

    if (g.view_event[i] & 0x80)
        return;
    director = obj_at(obj_at(car)->link);
    if (!(camera_at_link(director->link)->mode & 0x4000) || !g.bob_trigger[i])
        return;
    g.bob_trigger[i] = 0;
    g.view_event[i] = 0x80;
    g.bob_kind[i] = 0x1A;
    g.bob_phase[i] = 0;
    g.bob_22[i] = 0;
    g.bob_speed[i] = 0xFE;
}

/*
 * @routine 7E:D397 ObjectLinkEventTrigger
 * WHAT IT DOES: Starts player 1's view bob when player 1's camera has bit 14
 * of its mode set and $1A24 is set.
 */
void ObjectLinkEventTrigger(void)
{
    view_event_trigger(0, g.player_car);
}

/*
 * @routine 7E:D3DD ObjectLinkEventTrigger2
 * WHAT IT DOES: ObjectLinkEventTrigger for player 2.
 */
void ObjectLinkEventTrigger2(void)
{
    view_event_trigger(1, g.player2_car);
}

/*
 * @routine 7E:D41D ObjectLinkSpeedSet
 * WHAT IT DOES: Sets bob 1's speed.
 */
void ObjectLinkSpeedSet(uint8_t speed)
{
    g.bob_speed[1] = speed;
}

/*
 * @routine 7E:D410 ObjectLinkBobStart
 * WHAT IT DOES: Starts bob 1 with the given kind.
 */
void ObjectLinkBobStart(uint8_t kind)
{
    g.bob_kind[1] = kind;
    g.bob_phase[1] = 0;
    g.bob_22[1] = 0;
    ObjectLinkSpeedSet(0xFE);
}

/*
 * @routine 08:BFA4 ObjectBillboardTilt
 * WHAT IT DOES: The tilt of the object's picture as camera 1 sees it: half
 * its roll plus the camera's, rounded to 4 and held to -8..8, negated when
 * its heading is $40 or more from the camera's turn; the animation word is
 * that plus 8 and +$29; then the sprite's turn bytes for each camera.
 */
void ObjectBillboardTilt(Object *o)
{
    uint8_t turn = (uint8_t)-(uint8_t)(cameras[0].turn >> 8);
    uint8_t tilt = (uint8_t)(o->roll + (uint8_t)(cameras[0].roll >> 8));
    uint8_t facing;

    tilt = (uint8_t)(tilt >> 1 | (tilt & 0x80));
    if (tilt & 0x80)
        tilt = (uint8_t)-((uint8_t)-tilt & 0xFC);
    tilt &= 0xFC;
    if (!((uint8_t)(tilt - 0x08) & 0x80))
        tilt = 0x08;
    if ((uint8_t)(tilt - 0xF8) & 0x80)
        tilt = 0xF8;
    facing = (uint8_t)(o->heading - turn);
    if (facing & 0x80)
        facing = (uint8_t)-facing;
    if (!((uint8_t)(facing - 0x40) & 0x80))
        tilt = (uint8_t)-tilt;
    o->anim = look_word_tilted(o->child.look, (int8_t)tilt);
    o->sprite_turn[0] = (uint8_t)(ObjectFacingFrame64(o, turn) | 0x80);
    if (g.split_screen)
        o->sprite_turn[1] = (uint8_t)(ObjectFacingFrame64(o, (uint8_t)-(uint8_t)(cameras[1].turn >> 8)) | 0x80);
}

/*
 * @routine 09:C37A ObjectSpawnPuff
 * WHAT IT DOES: Counts a hit in the binary and the decimal counters, then
 * makes a puff of the common model at the object, $1F4 higher.
 */
void ObjectSpawnPuff(Object *o)
{
    Object *n;

    g.hits++;
    g.hits_bcd = bcd_inc16(g.hits_bcd);
    n = ObjectSpawn(model_word(MODEL_COMMON));
    if (!n)
        return;
    n->anim = look_word(LOOK_PUFF);
    n->puff.unread2 = 0xFE0C;
    n->draw_flags |= 0x08;
    n->material = 0;
    n->size = 0x14;
    ObjectCopyPosition(n, o);
    n->y = (uint16_t)(n->y + 0xFE0C);
    n->puff.unread1 = 0x07;
}

/*
 * @routine 08:B23D ObjectCarFrame
 * WHAT IT DOES: A car object's frame: unless player 1's car has bit 3 of
 * +$25 and this record bit 4 of +$1C, steps the record's flag, its near
 * focus, its pose and its bucket; otherwise marks the record.
 */
void ObjectCarFrame(Object *o)
{
    if ((obj_at(g.player_car)->car.state & CAR_EVENT_TAKEN) && (rec_get(o->record, FXREC_FLAGS1C) & 0x0010)) {
        rec_or(o->record, FXREC_FLAGS16, 0x0420);
        o->timer = 3;
        o->draw_flags |= 0x20;
        return;
    }
    ObjectRecordHoldWhileFrozen(o);
    ObjectNearFocus(o);
    ObjectPoseFromRecord(o);
    ObjectRebucket(o);
}

/*
 * @routine 08:B1FC ObjectCarSpawnPlace
 * WHAT IT DOES: A car after its record is made: the record marked, the car
 * frame as handler, its kind's animation, then the first frame.
 */
static void ObjectCarSpawnPlace(Object *o)
{
    if (!o->record)
        CrashWhite();
    rec_or(o->record, FXREC_FLAGS64, 0x0008);
    rec_or(o->record, FXREC_FLAGS40, 0x0200);
    ObjectSetHandler(o, H_CAR_FRAME);
    ObjectCarLookByKind(o, o->car.kind);
    ObjectCarFrame(o);
}

/*
 * @routine 08:B1E5 ObjectCarSpawn
 * WHAT IT DOES: Starts the object as a car: raised $1E0, a record made,
 * then ObjectCarSpawnPlace.
 */
void ObjectCarSpawn(Object *o)
{
    o->y = (uint16_t)(o->y + 0xFE20);
    ObjectCarRecordNew(o, 4);
    ObjectCarSpawnPlace(o);
}

/*
 * @routine 08:B99A ObjectCarRecordTake
 * WHAT IT DOES: Takes the record the program made ($70:238E; none: a
 * return), its model and turn, then the children and the attachments.
 */
static void ObjectCarRecordTake(Object *o)
{
    uint16_t rec = fxram_get(GSU_RECORD_ARG);

    if (!rec)
        return;
    o->record = rec;
    o->model = fxram_get(GSU_IN_AMOUNT);
    o->sprite_turn[0] = (uint8_t)(fx_ram[FX_CAR_FRAME] | 0x80);
    o->draw_flags |= 0x10;
    ObjectChildrenSpawn(o);
    ObjectAttachSpawn(o);
    o->car.unread1 = 0x03;
    o->car.unread2 = 0x01;
}

/*
 * @routine 08:B956 ObjectCarRecordNew
 * WHAT IT DOES: Makes the object's car record: the program given the
 * amount, the object's place and heading, its +$4A and +$27, then its
 * result taken.
 */
void ObjectCarRecordNew(Object *o, uint16_t amount)
{
    fxram_put(GSU_IN_AMOUNT, amount);
    fxram_put(FX_NEW_CAR_X, o->x);
    fxram_put(FX_NEW_CAR_Y, o->y);
    fxram_put(FX_NEW_CAR_Z, o->z);
    fxram_put(FX_NEW_CAR_HEADING, (uint16_t)(o->heading << 8));
    fxram_put(FX_CAR_ATTACHMENTS, o->car.attachments);
    fxram_put(GSU_RECORD_ARG, o->car.car_script);
    FxCarNew();
    ObjectCarRecordTake(o);
}

