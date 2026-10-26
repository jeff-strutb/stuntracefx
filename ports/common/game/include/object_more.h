/*
 * object_more.h: the object routines of object/object.c beyond the table
 * itself (objects.h): placing objects from the course's tables, the groups
 * and triggers that wake them, the sparkles, trails and attached pieces, the
 * billboard tilt and the timed objects. The handlers among them are plain
 * functions of the object; the routines that install one just store its
 * address, as the original does.
 */
#ifndef SRFX_OBJECT_MORE_H
#define SRFX_OBJECT_MORE_H

#include <stdbool.h>
#include <stdint.h>

/* object/object_more.c: the flags and tables an object is judged by */
uint16_t object_link_flag_4000_step(void);
bool object_flag40_if_match(uint16_t a);
uint8_t object_table_e8af(uint16_t obj, uint8_t a);
uint8_t object_table_c056(uint16_t obj);
uint16_t object_velocity_from_record(uint16_t obj);
uint16_t object_set_5d_by_kind(uint16_t obj, uint8_t kind);

/* placing the course's objects */
uint16_t object_place_from_table(uint8_t list, uint8_t entry, uint16_t *obj);
uint8_t object_link_event_trigger(void);
uint8_t object_link_event_trigger_2(void);
void object_link_speed_set(uint16_t a, bool m8);
void object_link_bob_start(uint8_t kind);
uint16_t object_place_all(void);

/* the frame's counts and distances */
bool object_count_step_5f(uint16_t obj, uint8_t *a);
uint16_t object_near_focus(uint16_t obj);
uint16_t object_rebucket(uint16_t obj);
uint8_t object_step_48_to_8(uint16_t obj);
void object_advance_5d_if_flag(uint16_t obj);

/* the linked objects and the groups that wake them */
uint16_t object_linked_handler_by_state(uint16_t obj, uint16_t *y, uint16_t a);
uint16_t object_attach_linked(uint16_t obj, uint16_t other);
uint16_t object_groups_activate(uint16_t *y);
void bucket_triggers_from_view(void);
void bucket_triggers_fixed_area(void);

/* spawns: a sparkle, a puff, a trail, a piece attached to a parent */
void object_spawn_chance_d460(uint16_t obj);
void object_spawn_d460_unless_flag(uint16_t obj);
void object_spawn_sparkle_pair(uint16_t obj);
void object_sparkle_start(uint16_t obj);
void object_sparkle_step(uint16_t obj);
void object_spawn_puff(uint16_t obj);
void object_spawn_trail(uint16_t obj);
void object_trail_start(uint16_t obj);
void object_trail_step(uint16_t obj);
void object_spawn_attached(uint16_t obj);
void object_attached_start(uint16_t obj);
void object_attached_step(uint16_t obj);
void object_place_on_parent(uint16_t parent, uint16_t obj);
void object_move_forward(uint16_t obj);
void object_billboard_tilt(uint16_t obj);
void object_timed_setup(uint16_t obj);
void object_timed_step(uint16_t obj);
void object_car_pose_step(uint16_t obj);

/* the release of an object by a handler in another bank: object_release */
void object_release_long(uint16_t obj);

#endif
