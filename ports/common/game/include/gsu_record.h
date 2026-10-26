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

/* the Super FX programs that take an amount, and the record's position and
   turn */
uint16_t record_lower_82(uint16_t rec, uint8_t amount);
uint16_t record_raise_6a(uint16_t rec, uint8_t amount);
uint16_t object_turn_from_record(uint16_t rec, uint16_t obj);

/* the object's sync with its record; `y` is the record's +$18 and `carry`
   is set when there is no record */
uint16_t object_sync_from_record(uint16_t obj, uint16_t *y, bool *carry);
uint16_t object_sync_to_record(uint16_t obj, uint16_t *y, bool *carry);
uint16_t object_sync_angles(uint16_t obj, uint16_t *y, bool *carry);
uint16_t object_sync_from_record_fall(uint16_t obj, uint16_t *y);
void object_sync_fall_step(uint16_t obj);
void object_record_mark_step(uint16_t obj);

/* the spinning object and the record lists and records of the course */
void object_spin_step(uint16_t obj);
void object_spin_record(uint16_t obj);
void object_spin_setup(uint16_t obj);

/* the buttons: the record's sequence, the car's controls, the players' frame */
uint16_t record_button_sequence(uint16_t rec);
void car_controls_to_record(uint16_t obj);
uint16_t player_controls_frame(void);

#endif
