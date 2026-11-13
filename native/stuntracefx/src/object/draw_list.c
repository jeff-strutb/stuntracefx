/*
 * draw_list.c: the list of objects the Super FX draws this frame, built
 * from the objects in the map cells the trigger list names.
 *
 * Not converted, nothing reaches it: ObjectDrawListClear08 (7E:F0D3).
 *
 * The race's view distance (plan stage 10, fx/fx_far.h): every other object
 * the list would draw, in every cell of the map, gets a far record in the
 * chip's RAM's second 64 KB, which the frame program draws behind the near
 * ones (fx/fx_far.c). The objects are not touched by it: what is activated,
 * and when, stays the trigger list's.
 */
#include <math.h>
#include <string.h>

#include "fx/fx_far.h"
#include "fx/fx_rt.h"
#include "fx/fx_view.h"
#include "object/object.h"
#include "system/state.h"

enum { RECORD_SIZE = 0x1E, RECORD_MAX = 0x31 };

static void put16(uint8_t *r, unsigned off, uint16_t v)
{
    r[off] = (uint8_t)v;
    r[off + 1] = (uint8_t)(v >> 8);
}

/* the shown frame byte: the field if bit 7 is set, else the game's frame */
static uint8_t frame_byte(uint8_t v)
{
    return (uint8_t)(((v & 0x80) ? v : (uint8_t)g.object_frames) & 0x7F);
}

/* the objects given a near record this list, by slot */
static uint8_t near_drawn[OBJECT_COUNT];

/*
 * Each draw record's car, for seating the cars on the road in the
 * depth-tested 3D (docs/stuntracefx/native/DEPTH.md, 5): the cars are the
 * objects with tyres (a child on its record, child_car); an object belongs
 * to a car when it is the car (its body) or follows it (its +$06 the car:
 * its tyres, shadow, eyes and a wreck's pieces, as 2d4d4c3a measured them);
 * 0 none, else the car's slot + 1. Kept per near record (by its number) and
 * per far record (by its number), with whether the record is a tyre.
 *
 * +$06 is a parent only for an object spawned to follow one; an object a
 * course script placed keeps its flags there (ObjectSpawnFromScript), so it
 * is compared with the car's reference and never followed (following one
 * as a reference took the draw list out of the object table: 2026-11-13,
 * a crash in draw_record on Easy Ride).
 */
static uint8_t has_tyres[OBJECT_COUNT];
static uint16_t record_car[2][FX_FAR_MAX];
static uint8_t record_tyre[2][FX_FAR_MAX];

static const Object *child_car(const Object *o);

static void cars_mark(void)
{
    ObjRef ref = objects.used_head;
    unsigned n = 0;

    memset(has_tyres, 0, sizeof has_tyres);
    for (; ref && n < OBJECT_COUNT; ref = obj_at(ref)->next, n++) {
        const Object *car = child_car(obj_at(ref));

        if (car)
            has_tyres[obj_slot(car)] = 1;
    }
}

static uint16_t car_of(const Object *o)
{
    unsigned k = obj_slot(o), at;

    if (has_tyres[k])
        return (uint16_t)(k + 1);
    /* its +$06, when it names an object: the car it follows */
    if (!o->parent || (o->parent - 1u) % OBJECT_SIZE)
        return 0;
    at = (o->parent - 1u) / OBJECT_SIZE;
    return at < OBJECT_COUNT && has_tyres[at] ? (uint16_t)(at + 1) : 0;
}

static void record_car_note(int far, unsigned n, const Object *o)
{
    if (n >= FX_FAR_MAX)
        return;
    record_car[far][n] = car_of(o);
    record_tyre[far][n] = child_car(o) != NULL;
}

uint16_t draw_record_car(int far, unsigned n, int *tyre)
{
    if (n >= FX_FAR_MAX) {
        *tyre = 0;
        return 0;
    }
    *tyre = record_tyre[far != 0][n];
    return record_car[far != 0][n];
}

/*
 * A car's child on its record (a tyre: ObjectChildStep, $08:BB2E) is drawn
 * on its seat on its car whenever the game's own place for it is not one:
 *
 *   - parked: ObjectChildFollow hides it (+$24 bit 5) while its car's record
 *     has bit 10 of +$16 (ObjectPoseFromRecord: the car's countdown +$36 at
 *     1, the car leaving the active cells) and leaves it where it was. The
 *     far view draws the car all the same, so the child is drawn with it, as
 *     a far record of its own;
 *   - unseated: while that bit is set the chip runs the car along the course
 *     (FxCarFollowCourse: FxCarCatchUp, its +$1C bit 4 set) and puts each of
 *     its parts' records on the car's place. The step the bit is cleared,
 *     ObjectChildFollow places the child from that record, at the car's
 *     centre, and turns it as the record was turned when the car left; only
 *     the next physics program seats the parts again (FxCarPose:
 *     FxCarMoveParts, the bit cleared). The original drew a car that far as
 *     its sprite, its children hidden (ObjectNearFocus, child_flags); here
 *     the car is its model, so for that step the child's draw record, near
 *     or far, is on its seat instead, and so is the step's end the pictures
 *     between steps start from (ObjectsKeptForPictures).
 *
 * Its seat: its place on the car's record in the record's turn (its heading
 * +$24), learnt each step the game places it on its seat (ObjectChildPlaced,
 * after its handler: its record's place plus its offset +$4A-$4E, Y less its
 * +$5F and $14); put on the car as drawn (its place, its heading +$15),
 * turned as it was (+$14, +$16) with its steer on the car's heading (its
 * record's +$25 on the car record's). A seat is its car's and record's (a
 * slot is used again). Nothing of the game's is written: only the draw
 * records and fx_tween's kept copy have it.
 */
static struct { int16_t at[3]; ObjRef car; uint16_t rec; uint8_t a14, steer, a16, known, unseated; } child_seat[OBJECT_COUNT];

/* a car's child on its record: its car, else NULL */
static const Object *child_car(const Object *o)
{
    const Object *car;

    if (object_handler_of(o->handler_bank, o->handler) != H_CHILD_STEP || !o->parent || !o->record)
        return NULL;
    car = obj_at(o->parent);
    return car->record ? car : NULL;
}

/* record r on record cr's place, as FxCarCatchUp puts a car's parts */
static int on_place_of(uint16_t r, uint16_t cr)
{
    return rec_get(r, FXREC_POS_X) == rec_get(cr, FXREC_POS_X) && rec_get(r, FXREC_POS_Y) == rec_get(cr, FXREC_POS_Y)
           && rec_get(r, FXREC_POS_Z) == rec_get(cr, FXREC_POS_Z);
}

void ObjectChildPlaced(const Object *o)
{
    unsigned k = obj_slot(o);
    const Object *car = child_car(o);
    uint16_t cr, r;
    double h, c, s, x, z;

    child_seat[k].unseated = 0;
    if (!car || (o->draw_flags & 0x20))
        return;
    cr = car->record;
    r = o->record;
    if (on_place_of(r, cr)) {
        /* placed from a record the course left on its car's place, not yet seated */
        child_seat[k].unseated = (rec_get(cr, FXREC_FLAGS1C) & 0x0010) != 0;
        return;
    }
    if (rec_get(cr, FXREC_FLAGS16) & 0x0400)
        return;
    h = rec_get(cr, FXREC_HEADING) * (2 * M_PI / 65536);
    c = cos(h), s = sin(h);
    x = (int16_t)(uint16_t)(rec_get(r, FXREC_POS_X) + rec_get(r, FXPART_OFFSET_X) - rec_get(cr, FXREC_POS_X));
    z = (int16_t)(uint16_t)(rec_get(r, FXREC_POS_Z) + rec_get(r, FXPART_OFFSET_Z) - rec_get(cr, FXREC_POS_Z));
    child_seat[k].at[0] = (int16_t)lround(x * c + z * s);
    child_seat[k].at[1] = (int16_t)(uint16_t)(rec_get(r, FXREC_POS_Y) + rec_get(r, FXPART_OFFSET_Y) - o->size - 0x0014
                                              - rec_get(cr, FXREC_POS_Y));
    child_seat[k].at[2] = (int16_t)lround(z * c - x * s);
    child_seat[k].a14 = o->pitch;
    child_seat[k].steer = (uint8_t)(fx_ram[(uint16_t)(r + FXREC_HEADING + 1)] - fx_ram[(uint16_t)(cr + FXREC_HEADING + 1)]);
    child_seat[k].a16 = o->roll;
    child_seat[k].car = o->parent;
    child_seat[k].rec = r;
    child_seat[k].known = 1;
}

/* a parked or unseated child: 1 and where it is drawn, on its seat on its
   car as the car is drawn now; else 0 */
static int seat_place(const Object *o, uint16_t at[3], uint8_t turn[3])
{
    unsigned k = obj_slot(o);
    const Object *car = child_car(o);
    double h, c, s, x, z;

    if (!car || !child_seat[k].known || child_seat[k].car != o->parent || child_seat[k].rec != o->record)
        return 0;
    if ((o->draw_flags & 0x20) ? !(rec_get(car->record, FXREC_FLAGS16) & 0x0400) : !child_seat[k].unseated)
        return 0;
    h = car->heading * (2 * M_PI / 256);
    c = cos(h), s = sin(h);
    x = child_seat[k].at[0], z = child_seat[k].at[2];
    at[0] = (uint16_t)(car->x + (int16_t)lround(x * c - z * s));
    at[1] = (uint16_t)(car->y + child_seat[k].at[1]);
    at[2] = (uint16_t)(car->z + (int16_t)lround(z * c + x * s));
    turn[0] = child_seat[k].a14;
    turn[1] = (uint8_t)(car->heading + child_seat[k].steer);
    turn[2] = child_seat[k].a16;
    return 1;
}

void ObjectsKeptForPictures(ObjectTable *kept)
{
    ObjRef ref = objects.used_head;
    unsigned n = 0;

    for (; ref && n < OBJECT_COUNT; ref = obj_at(ref)->next, n++) {
        const Object *o = obj_at(ref);
        Object *e = &kept->object[obj_slot(o)];
        uint16_t at[3];
        uint8_t turn[3];

        if (!seat_place(o, at, turn))
            continue;
        e->x = at[0], e->y = at[1], e->z = at[2];
        e->pitch = turn[0], e->heading = turn[1], e->roll = turn[2];
    }
}

/* a draw record's place p relative to the view (+$12, +$10, +$14) and its
   depth key +$02 from its height there and object o's lift */
static void view_place(uint8_t *r, const Object *o, const uint16_t p[3])
{
    uint16_t dy = (uint16_t)(p[1] - g.view_pos[1]);

    put16(r, 0x10, dy);
    if (dy & 0x8000)
        dy = (uint16_t)(~dy + 1);
    put16(r, 0x02, (uint16_t)((uint16_t)(dy << 1) + ((o->depth_lift & 0x01) ? 0x1F40 : 0x0000)));
    put16(r, 0x12, (uint16_t)(p[0] - g.view_pos[0]));
    put16(r, 0x14, (uint16_t)(p[2] - g.view_pos[2]));
}

/* Object o's draw record at r: place relative to the view unless bit 7 of
   +$24 says absolute, its depth key, model, angles, link fields, flags,
   palette and frame bytes and the animation word. A far record (far) is
   the same record, the object left as it is and not counted. A near one of
   an unseated child is on its seat. */
static uint8_t *draw_record(Object *o, uint8_t *r, int far)
{
    if (o->draw_flags & 0x80) {
        put16(r, 0x12, o->x);
        put16(r, 0x10, o->y);
        put16(r, 0x14, o->z);
        put16(r, 0x02, 0xF060);
    } else {
        uint16_t p[3] = {o->x, o->y, o->z};

        view_place(r, o, p);
    }
    put16(r, 0x08, o->model);
    r[0x04] = o->pitch;
    r[0x05] = o->heading;
    r[0x06] = o->roll;
    if (o->draw_flags & 0x04) {
        put16(r, 0x0A, obj_ref(o));
        r[0x0C] = o->emitter.count;
        r[0x0D] = o->emitter.effect_x;
        r[0x0E] = o->emitter.kind;
    } else {
        r[0x18] = 0;
    }
    r[0x07] = o->draw_flags;
    if (!far)
        o->draw_flags &= 0xFD;
    r[0x19] = frame_byte(o->shown_frame);
    r[0x1A] = frame_byte(g.draw_side ? o->sprite_turn[1] : o->sprite_turn[0]);
    r[0x1B] = o->material;
    r[0x1C] = o->size;
    r[0x1D] = o->texture_v;
    put16(r, 0x16, o->anim);
    ObjectRecordLook(o, r, far);        /* what it is drawn with besides its own (animate.c) */
    if (far)
        return r + FX_FAR_SIZE;
    {
        uint16_t at[3];
        uint8_t turn[3];

        if (!(o->draw_flags & 0x80) && seat_place(o, at, turn)) {
            view_place(r, o, at);
            r[0x04] = turn[0];
            r[0x05] = turn[1];
            r[0x06] = turn[2];
        }
    }
    near_drawn[obj_slot(o)] = 1;
    record_car_note(0, g.frame_record_count, o);
    objects.record_gaze[g.frame_record_count] = ObjectGazeBetween(o, g.draw_side ? 1 : 0, object_gaze(o, g.draw_side ? 1 : 0));
    g.frame_record_count++;
    return r + RECORD_SIZE;
}

/* by the object itself: not when bit 5 of +$24 is set (unless `hidden_ok`);
   with bit 6, kind 2 only on the near side and the rest only on the far
   side; never model none */
static int drawable(const Object *o, int hidden_ok)
{
    if ((o->draw_flags & 0x20) && !hidden_ok)
        return 0;
    if ((o->draw_flags & 0x40) && (o->list_kind == 0x02 ? g.draw_side != 0 : g.draw_side == 0))
        return 0;
    return o->model != model_word(MODEL_NONE);
}

/* in the near list: drawable, and at most $31 records */
static int drawn(const Object *o)
{
    return (int16_t)(g.frame_record_count - RECORD_MAX) < 0 && drawable(o, 0);
}

/* A far record: the object's record, its place from the view in 32 bits
   and its key's lift, at `at` in the chip's second 64 KB; a child on its seat
   on its car (`place`, `turn`), its key's height part from there. */
static void far_record(Object *o, uint16_t at, const uint16_t *place, const uint8_t *turn)
{
    uint8_t *r = &fx_ram[FX_FAR_BANK + at];
    uint16_t p[3] = {o->x, o->y, o->z};
    int32_t d[3], eye[3];
    unsigned k;

    draw_record(o, r, 1);
    if (place) {
        int16_t dy = (int16_t)(uint16_t)(place[1] - g.view_pos[1]);

        put16(r, 0x02, (uint16_t)(((dy < 0 ? -dy : dy) << 1) + ((o->depth_lift & 0x01) ? 0x1F40 : 0)));
        r[0x04] = turn[0];
        r[0x05] = turn[1];
        r[0x06] = turn[2];
        p[0] = place[0], p[1] = place[1], p[2] = place[2];
    }
    /* from the view's place, whole when a picture's view keeps it so (one
       past the 16 bits g's place wraps at: race/overhead.c) */
    if (!fx_view_eye(eye))
        for (k = 0; k < 3; k++)
            eye[k] = (int16_t)g.view_pos[k];
    d[0] = (int32_t)(int16_t)p[0] - eye[0];
    d[1] = (int32_t)(int16_t)p[1] - eye[1] + ObjectLookDy(o);
    d[2] = (int32_t)(int16_t)p[2] - eye[2];
    for (k = 0; k < 3; k++) {
        put16(r, FX_FAR_PLACE + 4 * k, (uint16_t)d[k]);
        put16(r, FX_FAR_PLACE + 4 * k + 2, (uint16_t)((uint32_t)d[k] >> 16));
    }
    r[FX_FAR_LIFT] = (uint8_t)(o->depth_lift & 0x01);
    /* a child on its seat on a car in the near list: drawn among the near ones */
    r[FX_FAR_PARKED] = (uint8_t)(place && near_drawn[obj_slot(obj_at(o->parent))]);
}

/* Every object of every cell the near list did not draw and would: by its
   rules, except those placed in view space (+$24 bit 7) and effect
   emitters (bit 2), and none past the count's room. */
static void far_records(void)
{
    unsigned cell, n = 0;

    for (cell = 0; cell < sizeof objects.bucket_head / sizeof objects.bucket_head[0]; cell++) {
        ObjRef ref = objects.bucket_head[cell];

        while (ref) {
            Object *o = obj_at(ref);

            ref = o->bucket_next;
            uint16_t place[3];
            uint8_t turn[3];
            int seated;

            if (near_drawn[obj_slot(o)] || (o->draw_flags & 0x84) || n >= FX_FAR_MAX)
                continue;
            if (ObjectFarLeftOut(o))    /* waiting for its set-up, or a short life stopped (animate.c) */
                continue;
            seated = seat_place(o, place, turn);     /* parked or unseated */
            if (!drawable(o, seated) || !model_word_in_cartridge(o->model))
                continue;
            far_record(o, (uint16_t)(FX_FAR_RECORDS + n * FX_FAR_SIZE), seated ? place : NULL, turn);
            record_car_note(1, n, o);
            n++;
        }
    }
    fx_ram[FX_FAR_BANK + FX_FAR_COUNT] = (uint8_t)n;
    fx_ram[FX_FAR_BANK + FX_FAR_COUNT + 1] = (uint8_t)(n >> 8);
    for (cell = 0; cell < 3; cell++)    /* the view's place, for the ground's surface */
        put16(&fx_ram[FX_FAR_BANK + FX_FAR_VIEW], 2 * cell, g.view_pos[cell]);
}

/*
 * @routine 7E:EF50 ObjectDrawListBuild
 * WHAT IT DOES: Builds the list of objects to draw: for each trigger, walks
 * the objects of its map cell and writes a record for each one drawn.
 */
void ObjectDrawListBuild(void)
{
    uint8_t *r = g.frame_records;

    memset(near_drawn, 0, sizeof near_drawn);
    cars_mark();
    g.frame_record_count = 0;
    for (unsigned t = 0; g.trigger_list[t][0]; t++) {
        ObjRef ref = objects.bucket_head[(g.trigger_list[t][1] & 0x07FE) / 2];

        while (ref) {
            Object *o = obj_at(ref);

            g.handler_object = ref;
            if (drawn(o))
                r = draw_record(o, r, 0);
            ref = o->bucket_next;
        }
    }
    far_records();
}

uint16_t draw_record_gaze(int record)
{
    if (record < 0 || record >= (int)(sizeof objects.record_gaze / sizeof objects.record_gaze[0]))
        return 0;
    return objects.record_gaze[record];
}
