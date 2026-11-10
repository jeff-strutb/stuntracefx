/*
 * flags_record.c: objects whose Super FX record takes the object's own
 * flags (08:CA45-CD83): their set-ups, the swinging picture frame copied
 * to the record's +$12, and the step that only marks the record.
 *
 * Not converted, nothing in the game reaches them: ObjectStopMarked
 * (08:CA2D), ObjectFrameCycleSetup and Step (08:CA56, 08:CA6F),
 * ObjectFrameCycleBackSetup and Step (08:CAA2, 08:CAC3),
 * ObjectFlagsRecordSetup20 (08:CD25).
 */
#include "object/object.h"

/* the low byte of a word field used as a byte */

/* the shown frame to the record's +$12, then the record marked */
static void frames_to_record(Object *o)
{
    if (o->record)
        rec_put(o->record, FXREC_FRAME, o->shown_frame & 0x7F);
    GsuRecordKeep(o);
}

/*
 * @routine 08:CD7E ObjectRecordFlag4Step
 * WHAT IT DOES: A handler that only marks the object's record.
 */
void ObjectRecordFlag4Step(Object *o)
{
    GsuRecordKeep(o);
}

/*
 * @routine 08:CD55 ObjectFlagsRecord
 * WHAT IT DOES: Gives the object a record from the second pool, ORs its
 * +$06 and +$27 words into the record's flags, and marks it.
 */
void ObjectFlagsRecord(Object *o)
{
    if (!GsuRecordNewStill(o))
        return;
    rec_or(o->record, FXREC_FLAGS16, o->script_flags);
    rec_or(o->record, FXREC_FLAGS16, o->flags_record.record_flags);
    GsuRecordKeep(o);
}

/* +$1D = ObjectFlagsRecord, then the record */
static void flags_record_tail(Object *o)
{
    ObjectSetHandler2(o, H_FLAGS_RECORD);
    ObjectFlagsRecord(o);
}

/*
 * @routine 08:CD2D ObjectFlagsRecordSetup
 * WHAT IT DOES: Sets up an object whose record takes its flags.
 */
void ObjectFlagsRecordSetup(Object *o)
{
    o->flags_record.record_flags = 0;
    ObjectSetHandler(o, H_RECORD_FLAG4_STEP);
    flags_record_tail(o);
}

/*
 * @routine 08:CA45 ObjectFlagsRecordFrame
 * WHAT IT DOES: Shows frame +$48, then as ObjectFlagsRecordSetup.
 */
void ObjectFlagsRecordFrame(Object *o)
{
    o->shown_frame = (uint8_t)(o->flags_record.frame | 0x80);
    ObjectFlagsRecordSetup(o);
}

/*
 * @routine 08:CB15 ObjectFrameSwingStep
 * WHAT IT DOES: The swinging object's frame: while its pause counts it
 * holds; otherwise every +$49 frames it steps one frame toward its last
 * frame or, with +$25 bit 0, toward frame 0; at the end the direction flips
 * and the pause restarts. Then the frame to the record.
 */
void ObjectFrameSwingStep(Object *o)
{
    uint8_t last;

    if (o->frame_swing.paused) {
        o->frame_swing.paused--;
        frames_to_record(o);
        return;
    }
    o->shown_frame = (uint8_t)(o->frame_swing.frame | 0x80);
    last = (uint8_t)(o->frame_swing.frames - 1);
    o->frame_swing.waited++;
    if (o->frame_swing.waited != o->frame_swing.wait) {
        frames_to_record(o);
        return;
    }
    o->frame_swing.waited = 0;
    if (o->frame_swing.state & SWING_BACK) {
        if (o->shown_frame & 0x7F) {
            o->frame_swing.frame--;
            frames_to_record(o);
            return;
        }
    } else if ((o->shown_frame & 0x7F) != last) {
        o->frame_swing.frame++;
        frames_to_record(o);
        return;
    }
    o->frame_swing.state ^= SWING_BACK;
    o->frame_swing.paused = o->frame_swing.pause;
    frames_to_record(o);
}

/*
 * @routine 08:CAF6 ObjectFrameSwingSetup
 * WHAT IT DOES: Sets up an object whose picture swings between its first
 * and last frames, then the flags record.
 */
void ObjectFrameSwingSetup(Object *o)
{
    ObjectSetHandler(o, H_FRAME_SWING_STEP);
    o->shown_frame = 0x80;
    o->frame_swing.paused = o->frame_swing.pause;
    flags_record_tail(o);
}
