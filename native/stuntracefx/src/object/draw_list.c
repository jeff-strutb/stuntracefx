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
#include <string.h>

#include "fx/fx_far.h"
#include "fx/fx_rt.h"
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

/* Object o's draw record at r: place relative to the view unless bit 7 of
   +$24 says absolute, its depth key, model, angles, link fields, flags,
   palette and frame bytes and the animation word. A far record (far) is
   the same record, the object left as it is and not counted. */
static uint8_t *draw_record(Object *o, uint8_t *r, int far)
{
    if (o->flags24 & 0x80) {
        put16(r, 0x12, o->x);
        put16(r, 0x10, o->y);
        put16(r, 0x14, o->z);
        put16(r, 0x02, 0xF060);
    } else {
        uint16_t dy = (uint16_t)(o->y - g.view_pos[1]);

        put16(r, 0x10, dy);
        if (dy & 0x8000)
            dy = (uint16_t)(~dy + 1);
        put16(r, 0x02, (uint16_t)((uint16_t)(dy << 1) + ((o->flags09 & 0x01) ? 0x1F40 : 0x0000)));
        put16(r, 0x12, (uint16_t)(o->x - g.view_pos[0]));
        put16(r, 0x14, (uint16_t)(o->z - g.view_pos[2]));
    }
    put16(r, 0x08, o->model);
    r[0x04] = o->pitch;
    r[0x05] = o->heading;
    r[0x06] = o->roll;
    if (o->flags24 & 0x04) {
        put16(r, 0x0A, obj_ref(o));
        r[0x0C] = o->count48;
        r[0x0D] = o->byte49;
        r[0x0E] = o->byte4a;
    } else {
        r[0x18] = 0;
    }
    r[0x07] = o->flags24;
    if (!far)
        o->flags24 &= 0xFD;
    r[0x19] = frame_byte(o->frame5a);
    r[0x1A] = frame_byte(g.draw_side ? o->sprite_turn[1] : o->sprite_turn[0]);
    r[0x1B] = o->byte3a;
    r[0x1C] = o->byte5f;
    r[0x1D] = o->byte60;
    put16(r, 0x16, o->anim);
    if (far)
        return r + FX_FAR_SIZE;
    near_drawn[obj_slot(o)] = 1;
    objects.record_gaze[g.frame_record_count] = object_gaze(o, g.draw_side ? 1 : 0);
    g.frame_record_count++;
    return r + RECORD_SIZE;
}

/* by the object itself: not when bit 5 of +$24 is set; with bit 6, kind 2
   only on the near side and the rest only on the far side; never model none */
static int drawable(const Object *o)
{
    if (o->flags24 & 0x20)
        return 0;
    if ((o->flags24 & 0x40) && (o->byte26 == 0x02 ? g.draw_side != 0 : g.draw_side == 0))
        return 0;
    return o->model != MODEL_NONE;
}

/* in the near list: drawable, and at most $31 records */
static int drawn(const Object *o)
{
    return (int16_t)(g.frame_record_count - RECORD_MAX) < 0 && drawable(o);
}

/* A far record: the object's record, its place from the view in 32 bits
   and its key's lift, at `at` in the chip's second 64 KB. */
static void far_record(Object *o, uint16_t at)
{
    uint8_t *r = &fx_ram[FX_FAR_BANK + at];
    int32_t d[3];
    unsigned k;

    draw_record(o, r, 1);
    d[0] = (int32_t)(int16_t)o->x - (int16_t)g.view_pos[0];
    d[1] = (int32_t)(int16_t)o->y - (int16_t)g.view_pos[1];
    d[2] = (int32_t)(int16_t)o->z - (int16_t)g.view_pos[2];
    for (k = 0; k < 3; k++) {
        put16(r, FX_FAR_PLACE + 4 * k, (uint16_t)d[k]);
        put16(r, FX_FAR_PLACE + 4 * k + 2, (uint16_t)((uint32_t)d[k] >> 16));
    }
    r[FX_FAR_LIFT] = (uint8_t)(o->flags09 & 0x01);
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
            if (near_drawn[obj_slot(o)] || (o->flags24 & 0x84) || n >= FX_FAR_MAX)
                continue;
            if (!drawable(o) || o->model < 0x8000)
                continue;
            far_record(o, (uint16_t)(FX_FAR_RECORDS + n * FX_FAR_SIZE));
            n++;
        }
    }
    fx_ram[FX_FAR_BANK + FX_FAR_COUNT] = (uint8_t)n;
    fx_ram[FX_FAR_BANK + FX_FAR_COUNT + 1] = (uint8_t)(n >> 8);
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
