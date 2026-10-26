/*
 * flags_record.c: the objects whose Super FX record takes the object's own
 * flags: their set-ups, the frame step that swings a picture frame and
 * copies it to the record's +$12, the record step that ORs the object's
 * +$06 and +$27 words into the record's +$16, and the step that only marks
 * the record. The records themselves are in gsu_record.c. Left out, as
 * nothing in the game refers to their set-ups: the frame steps that cycle
 * a picture forwards or backwards (ObjectFrameCycleStep,
 * ObjectFrameCycleBackStep).
 */
#include "game.h"
#include "objects.h"
#include "gsu_record.h"
#include "flags_record.h"

#define frame_scratch  W8(0x1F58)   /* a frame number, held while a step works */
#define frame_last     W8(0x1F59)   /* the last frame of a swing */
#define REC_W(rec, off) P16((uint16_t)((rec) + (off)))

/* The object's byte at `field`: its shown frame is kept at +$5A. */
#define OBJ_B(o, f)    (*OBJ(o, f))

/* Shows the frame kept in the object's byte `field` (with bit 7 set). */
static void show_frame_of(uint16_t obj, uint16_t field)
{
    frame_scratch = OBJ_B(obj, field);
    OBJ_B(obj, 0x5A) = (uint8_t)(frame_scratch | 0x80);
}

/* Shows frame 0. */
static void show_frame_zero(uint16_t obj)
{
    OBJ_B(obj, 0x5A) = 0x80;
}

/* The object's handler is h (+$1A), bank $08 (+$1C). */
static void set_handler08(uint16_t obj, uint16_t h)
{
    obj_handler(obj) = h;
    OBJ_B(obj, 0x1C) = 0x08;
}

/* The end of the set-ups: the second handler (+$1D) is object_flags_record,
   bank $08 (+$1F), and the record is given now. */
static void flags_record_tail(uint16_t obj)
{
    obj_handler_b(obj) = 0xCD55;
    OBJ_B(obj, 0x1F) = 0x08;
    object_flags_record(obj);
}

/* The shown frame (low seven bits) goes to the object's record +$12 when
   it has one; then the record is marked. */
static void frame_to_record(uint16_t obj)
{
    uint16_t rec = obj_record(obj);

    if (rec)
        REC_W(rec, 0x12) = (uint16_t)(OBJ_B(obj, 0x5A) & 0x7F);
    object_record_flag4_step(obj);
}

/* Shows frame +$48 (the object's frame count, through $1F58), then sets
   the object up as object_flags_record_setup does. */
void object_flags_record_frame(uint16_t obj)
{
    show_frame_of(obj, 0x48);
    object_flags_record_setup(obj);
}

/* A picture that swings between its first and last frames: handler
   object_frame_swing_step, frame 0 shown, its pause (+$2E) from +$4A, then
   the flags record. */
void object_frame_swing_setup(uint16_t obj)
{
    set_handler08(obj, 0xCB15);
    show_frame_zero(obj);
    OBJ_B(obj, 0x2E) = OBJ_B(obj, 0x4A);
    flags_record_tail(obj);
}

/* The swinging object's frame. While its pause (+$2E) is not 0 it counts
   down. Otherwise frame +$30 is shown, and every +$49 frames (counted in
   +$4B) it steps one frame towards the last frame (+$48 - 1, kept in $1F59)
   or, with bit 0 of +$25 set, towards frame 0. At the end of a swing the
   direction (bit 0 of +$25) flips and the pause restarts from +$4A. Then the
   shown frame goes to the record. */
void object_frame_swing_step(uint16_t obj)
{
    if (OBJ_B(obj, 0x2E)) {
        OBJ_B(obj, 0x2E)--;
        frame_to_record(obj);
        return;
    }
    show_frame_of(obj, 0x30);
    frame_scratch = OBJ_B(obj, 0x48);
    frame_last = (uint8_t)(frame_scratch - 1);
    OBJ_B(obj, 0x4B) = (uint8_t)(OBJ_B(obj, 0x4B) + 1);
    if (OBJ_B(obj, 0x4B) != OBJ_B(obj, 0x49)) {
        frame_to_record(obj);
        return;
    }
    OBJ_B(obj, 0x4B) = 0;
    if (obj_25(obj) & 0x01) {
        if (OBJ_B(obj, 0x5A) & 0x7F) {
            OBJ_B(obj, 0x30)--;
            frame_to_record(obj);
            return;
        }
    } else if ((OBJ_B(obj, 0x5A) & 0x7F) != frame_last) {
        OBJ_B(obj, 0x30)++;
        frame_to_record(obj);
        return;
    }
    obj_25(obj) ^= 0x01;
    OBJ_B(obj, 0x2E) = OBJ_B(obj, 0x4A);
    frame_to_record(obj);
}

/* A handler that only marks the object's record (GsuRecordFlag4). */
void object_record_flag4_step(uint16_t obj)
{
    record_mark_flag4(obj);
}

/* Gives the object a record from the second pool (record_new_422) and ORs
   the object's +$06 word, then its +$27 word, into the record's +$16. Then
   the record is marked. With no room in the pool it returns at once. */
void object_flags_record(uint16_t obj)
{
    uint16_t rec;

    if (!record_new_422(obj))
        return;
    rec = obj_record(obj);
    REC_W(rec, 0x16) = (uint16_t)(REC_W(rec, 0x16) | OBJ_W(obj, 0x06));
    REC_W(rec, 0x16) = (uint16_t)(REC_W(rec, 0x16) | OBJ_W(obj, 0x27));
    object_record_flag4_step(obj);
}

/* Sets up an object whose record takes the object's flags: +$27 = 0, the
   handler object_record_flag4_step, the second handler object_flags_record,
   then its record. */
void object_flags_record_setup(uint16_t obj)
{
    OBJ_W(obj, 0x27) = 0x0000;
    set_handler08(obj, 0xCD7E);
    flags_record_tail(obj);
}
