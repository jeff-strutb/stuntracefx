/*
 * gsu_record.h: each object's Super FX record (object/gsu_record.c).
 *
 * The routines return the value the original leaves in A, and the flags
 * the callers test are given as booleans.
 */
#ifndef SRFX_GSU_RECORD_H
#define SRFX_GSU_RECORD_H

#include <stdbool.h>
#include <stdint.h>

/* the record's mode, flags and position */
uint16_t record_set_mode_2(uint16_t obj);
uint16_t record_or_flags(uint16_t obj, uint16_t bits);
uint16_t record_flags_bit4_on_bit8_off(uint16_t obj);
uint16_t record_set_position(uint16_t obj, uint16_t x, uint16_t y, uint16_t z);
uint16_t record_mark_flag4(uint16_t obj);
uint16_t record_set_2a_high(uint16_t obj, uint8_t value);
uint16_t record_bits_60_pair(void);

/* the parent object's record (the object's +$06) */
uint16_t parent_record_word_88(uint16_t obj, uint16_t *parent);
uint16_t parent_record_word_58(uint16_t obj, uint16_t *parent, bool *clear);

/* velocities and the record's copies of the object */
uint16_t record_take_velocity(uint16_t obj);
uint16_t velocity_from_record_of(uint16_t obj, uint16_t from);
uint16_t velocity_from_record_scaled(uint16_t obj);
uint16_t record_copy_from_object(uint16_t obj);
uint16_t object_pose_from_record(uint16_t obj);
uint16_t object_list_record_words_88(void);

/* steps and limits */
uint16_t record_step_6a(uint16_t obj, bool *ran);
uint16_t record_land(uint16_t obj);
uint16_t record_flags_8a_set(uint16_t rec, uint8_t bits);
uint16_t record_flags_8a_clear(uint16_t rec, uint8_t bits);

/* the records given to objects (Super FX programs of bank $04) */
bool record_new_422(uint16_t obj);
bool record_new_42d(uint16_t obj);
bool record_new_424_piece(uint16_t obj);
bool record_new_424(uint16_t obj);
bool record_new_424_alt(uint16_t obj);
bool record_new_camera(uint16_t obj);
bool record_new_426(uint16_t obj);

/* the contact list (records touched this frame) */
uint16_t contact_flags_sum(void);
bool record_next_contact(uint16_t *touched);
uint16_t record_contact_summary(uint16_t obj, uint16_t *last);

#endif
