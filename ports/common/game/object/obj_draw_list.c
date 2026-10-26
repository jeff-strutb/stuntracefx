/*
 * obj_draw_list.c: the list of objects the Super FX draws each frame. The
 * trigger list names buckets of objects; each object drawn writes a
 * 30-byte record (from $7E:F77F) with its position relative to the view,
 * a depth key, its model, angles, link, flags and animation.
 */
#include <stdbool.h>
#include "game.h"
#include "objects.h"
#include "obj_draw_list.h"

#define draw_count      W16(0x0CEE)     /* the records written, at most 49 */
#define trigger_pos     W16(0x1F2B)     /* the offset into the trigger list */
#define trigger_word    W16(0x1F2D)     /* the current trigger's first word */
#define draw_object     W16(0x034C)     /* the object being looked at */
#define draw_side       W8(0x1F07)      /* nonzero: the view is on the far side */
#define view_x          W16(0x00C5)     /* the view position */
#define view_y          W16(0x00C7)
#define view_z          W16(0x00C9)
#define fallback_byte   W8(0x05E9)      /* what a record takes when an object's byte is off */

#define TRIGGER_LIST    0x1AF5          /* two words per trigger, a zero first word ends it */
#define BUCKET_HEADS    0x10000         /* the bucket heads, bank $7F */
#define DRAW_LIST       0xF77F          /* the first record: 30 bytes each */
#define RECORD_SIZE     0x1E
#define MAX_RECORDS     0x31            /* the records drawn, at most */
#define MODEL_NONE      0xA040          /* an object of this model is not drawn */

/* The byte or word at field `f` of the draw record at `r`. */
#define REC8(r, f)      W8((uint16_t)((r) + (f)))
#define REC16(r, f)     W16((uint16_t)((r) + (f)))

/* A byte of an object's flags or fields, when bit 7 of it is set the
   value stands, else the fallback byte is used (the model's byte). */
static uint8_t flag_or_fallback(uint8_t v)
{
    return (v & 0x80) ? v : fallback_byte;
}

/* Whether object `o` is drawn: not when bit 5 of +$24 is set; with bit 6
   set, kind 2 (+$26) only on the near side and the rest only on the far
   side; then while fewer than 49 records are written, and not for model
   $A040. */
static bool draw_wanted(uint16_t o)
{
    uint8_t f = OBJ(o, 0x24)[0];

    if (f & 0x20)
        return false;
    if (f & 0x40) {
        if (*OBJ(o, 0x26) == 0x02) {
            if (draw_side)
                return false;
        } else if (!draw_side) {
            return false;
        }
    }
    if ((int16_t)(draw_count - MAX_RECORDS) >= 0)
        return false;
    return OBJ_W(o, 0x04) != MODEL_NONE;
}

/* The draw record of object `o` at `r`: its position relative to the view
   (absolute when bit 7 of +$24 is set), depth key, model, angles, link
   bytes or 0, flags (bit 1 of +$24 cleared on the object), palette and
   frame bytes and the animation word. Returns the next record's address. */
static uint16_t draw_record(uint16_t o, uint16_t r)
{
    uint16_t a;
    uint8_t v;

    if (OBJ_W(o, 0x24) & 0x0080) {
        REC16(r, 0x12) = OBJ_W(o, 0x0C);
        REC16(r, 0x10) = OBJ_W(o, 0x0E);
        REC16(r, 0x14) = OBJ_W(o, 0x10);
        REC16(r, 0x02) = 0xF060;
    } else {
        /* the depth key: the distance along the view's y, doubled, plus
           $1F40 when bit 0 of +$09 is set */
        a = (uint16_t)(OBJ_W(o, 0x0E) - view_y);
        REC16(r, 0x10) = a;
        if (a & 0x8000)
            a = (uint16_t)(~a + 1);
        REC16(r, 0x02) = (uint16_t)((a << 1) + ((OBJ_W(o, 0x09) & 0x0001) ? 0x1F40 : 0));
        REC16(r, 0x12) = (uint16_t)(OBJ_W(o, 0x0C) - view_x);
        REC16(r, 0x14) = (uint16_t)(OBJ_W(o, 0x10) - view_z);
    }
    REC16(r, 0x08) = OBJ_W(o, 0x04);
    REC16(r, 0x04) = OBJ_W(o, 0x14);
    REC16(r, 0x05) = OBJ_W(o, 0x15);
    REC16(r, 0x06) = OBJ_W(o, 0x16);
    if (OBJ_W(o, 0x24) & 0x0004) {
        REC16(r, 0x0A) = o;
        REC8(r, 0x0C) = *OBJ(o, 0x48);
        REC8(r, 0x0D) = *OBJ(o, 0x49);
        REC8(r, 0x0E) = *OBJ(o, 0x4A);
    } else {
        REC8(r, 0x18) = 0x00;
    }
    v = *OBJ(o, 0x24);
    REC8(r, 0x07) = v;
    *OBJ(o, 0x24) = (uint8_t)(v & 0xFD);
    REC8(r, 0x19) = (uint8_t)(flag_or_fallback(*OBJ(o, 0x5A)) & 0x7F);
    v = draw_side ? flag_or_fallback(*OBJ(o, 0x19)) : flag_or_fallback(*OBJ(o, 0x18));
    REC8(r, 0x1A) = (uint8_t)(v & 0x7F);
    REC8(r, 0x1B) = *OBJ(o, 0x3A);
    REC8(r, 0x1C) = *OBJ(o, 0x5F);
    REC8(r, 0x1D) = *OBJ(o, 0x60);
    REC16(r, 0x16) = OBJ_W(o, 0x5D);
    draw_count = (uint16_t)(draw_count + 1);
    return (uint16_t)(r + RECORD_SIZE);
}

/* The objects of one bucket, from `o` along +$3C, each drawn if wanted;
   returns the next free record. */
static uint16_t draw_bucket(uint16_t o, uint16_t r)
{
    do {
        draw_object = o;
        if (draw_wanted(o))
            r = draw_record(o, r);
        o = OBJ_W(o, 0x3C);
    } while (o);
    return r;
}

/* Clears the count and the trigger position, then walks the trigger list
   from its start: each trigger's bucket (its second word, bits 1-10) is
   drawn. */
void object_draw_list_build(void)
{
    uint16_t pos, a, r = DRAW_LIST;

    draw_count = 0;
    trigger_pos = 0;
    for (;;) {
        pos = trigger_pos;
        a = W16(TRIGGER_LIST + pos);
        if (!a)
            break;
        trigger_word = a;
        a = W16(TRIGGER_LIST + 2 + pos);
        trigger_pos = (uint16_t)(pos + 4);
        a = W16(BUCKET_HEADS + (a & 0x07FE));
        if (a)
            r = draw_bucket(a, r);
    }
}
