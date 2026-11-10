/*
 * draw_list.c: the list of objects the Super FX draws this frame, built
 * from the objects in the map cells the trigger list names.
 *
 * Not converted, nothing reaches it: ObjectDrawListClear08 (7E:F0D3). The
 * Enhanced mode's far records (rm_ calls) are left out for now.
 */
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

/* Object o's draw record at r: place relative to the view unless bit 7 of
   +$24 says absolute, its depth key, model, angles, link fields, flags,
   palette and frame bytes and the animation word. */
static uint8_t *draw_record(Object *o, uint8_t *r)
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
    o->flags24 &= 0xFD;
    r[0x19] = frame_byte(o->frame5a);
    r[0x1A] = frame_byte(g.draw_side ? o->sprite_turn[1] : o->sprite_turn[0]);
    r[0x1B] = o->byte3a;
    r[0x1C] = o->byte5f;
    r[0x1D] = o->byte60;
    put16(r, 0x16, o->anim);
    objects.record_gaze[g.frame_record_count] = object_gaze(o, g.draw_side ? 1 : 0);
    g.frame_record_count++;
    return r + RECORD_SIZE;
}

/* not when bit 5 of +$24 is set; with bit 6, kind 2 only on the near side
   and the rest only on the far side; at most $31 records, never model $A040 */
static int drawn(const Object *o)
{
    if (o->flags24 & 0x20)
        return 0;
    if ((o->flags24 & 0x40) && (o->byte26 == 0x02 ? g.draw_side != 0 : g.draw_side == 0))
        return 0;
    if ((int16_t)(g.frame_record_count - RECORD_MAX) >= 0)
        return 0;
    return o->model != MODEL_NONE;
}

/*
 * @routine 7E:EF50 ObjectDrawListBuild
 * WHAT IT DOES: Builds the list of objects to draw: for each trigger, walks
 * the objects of its map cell and writes a record for each one drawn.
 */
void ObjectDrawListBuild(void)
{
    uint8_t *r = g.frame_records;

    g.frame_record_count = 0;
    for (unsigned t = 0; g.trigger_list[t][0]; t++) {
        ObjRef ref = objects.bucket_head[(g.trigger_list[t][1] & 0x07FE) / 2];

        while (ref) {
            Object *o = obj_at(ref);

            g.handler_object = ref;
            if (drawn(o))
                r = draw_record(o, r);
            ref = o->bucket_next;
        }
    }
}

uint16_t draw_record_gaze(int record)
{
    if (record < 0 || record >= (int)(sizeof objects.record_gaze / sizeof objects.record_gaze[0]))
        return 0;
    return objects.record_gaze[record];
}
