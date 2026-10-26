/*
 * flags_record.h: the objects whose Super FX record takes the object's own
 * flags (object/flags_record.c). Each routine takes the object (its offset
 * in the table, objects.h).
 */
#ifndef SRFX_FLAGS_RECORD_H
#define SRFX_FLAGS_RECORD_H

#include <stdint.h>

/* set-ups: the frame ones show a frame first, then give the record */
void object_flags_record_frame(uint16_t obj);
void object_flags_record_setup(uint16_t obj);
void object_frame_swing_setup(uint16_t obj);

/* the frame steps: the shown frame goes to the record's +$12 */
void object_frame_swing_step(uint16_t obj);

/* the record steps */
void object_flags_record(uint16_t obj);
void object_record_flag4_step(uint16_t obj);

#endif
