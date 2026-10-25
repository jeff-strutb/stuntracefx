/*
 * physics.h: the Super FX's physics, as the port has it (object/fx_*.c).
 *
 * Records are in the game pak RAM; a record's fields are named where they
 * are used. The two vector sums: FX_WORLD_SUM (x, y, z in world axes) and
 * FX_LOCAL_SUM (in the object's own axes, turned by the matrix at $00E4).
 * The pushes: twenty slots of five words from $23A4 (on, amount, a
 * direction x, y, z); the first is the push the routines set themselves,
 * the other nineteen ($23AE on) the faces a collision found.
 */
#ifndef SRFX_PHYSICS_H
#define SRFX_PHYSICS_H

#include <stdint.h>

#define FX_WORLD_SUM        0x2340u
#define FX_LOCAL_SUM        0x2346u
#define FX_PUSH             0x23A4u
#define FX_PUSHES           0x23AEu
#define FX_HIT_POINT        0x2334u

/* ---- object/fx_sums.c ---- */
void fx_sums_clear(void);
void fx_sums_rotate_local(void);
void fx_sums_apply(uint16_t rec);
void fx_sums_push_world(int16_t v[3]);          /* v held to +-5000 first */
void fx_sums_push_local(int16_t v[3]);
uint16_t fx_push_apply(uint16_t *at);           /* 1 applied, else 8 (none) or 6 (skipped) */
uint16_t fx_pushes_apply(void);                 /* the flags the nineteen set */
void fx_push_at(uint16_t which, const int16_t point[3]);   /* which: the table offset at $2654 */
void fx_hit_add_scaled(int16_t v[3], int16_t scale);
void fx_hit_add_unit(int16_t v[3]);
void fx_sum_scale(int16_t scale);

/* ---- object/fx_pool.c ---- */
uint16_t fx_pool_block(uint16_t head, uint16_t size);
uint16_t fx_pool_take(uint16_t head, uint16_t size);
void fx_pools_merge(void);
void fx_list_clear_bit0(uint16_t owner);
void fx_list_sweep(uint16_t owner);
void fx_object_free(uint16_t rec);
void fx_record_new_426(void);       /* programs the S-CPU starts: the new record at $238E */
void fx_record_new_424(void);
void fx_record_new_426b(void);
void fx_record_new_42d(void);
void fx_record_new_camera(void);
void fx_record_new_422(void);

/* ---- object/fx_collide.c ---- */
void fx_collide_push(uint16_t contact, uint16_t other, uint16_t rec);
int fx_records_apart(uint16_t a, uint16_t b);
void fx_contact_note(uint16_t owner, uint16_t other);
void fx_record_touches(uint16_t rec);
void fx_all_touches(void);

/* ---- object/fx_record.c ---- */
uint16_t fx_push_set(int16_t amount);
void fx_record_hold_y(uint16_t rec);
uint16_t fx_bearing_to(uint16_t rec, int16_t x, int16_t z);
void fx_record_alignment(uint16_t rec);
void fx_record_raise_6a(void);      /* programs the S-CPU starts */
void fx_record_lower_82(void);
void fx_timers_program(void);

/* ---- object/fx_car.c ---- */
void fx_car_move_parts(uint16_t car);
void fx_car_pose(uint16_t car);
void fx_car_new(void);              /* a program the S-CPU starts */
void fx_car_hit(uint16_t car);

/* ---- race/fx_traffic.c, race/fx_rank.c ---- */
void fx_traffic_sides(uint16_t car);
void fx_spread_cars(uint16_t car);
void fx_nearest_car(void);
void fx_rank_cars(void);

/* ---- race/fx_course.c, race/fx_drive.c ---- */
void fx_course_entry_read(uint16_t at, uint16_t w[4]);
void fx_lap_segment(uint16_t car);
uint16_t fx_course_nearest(int16_t x, int16_t y, int16_t z, uint16_t *at, uint16_t w[3]);
uint16_t fx_course_next_entry(uint16_t car, uint16_t n, uint16_t *at, uint16_t w[4]);
uint16_t fx_record_heading_ahead(uint16_t car);
int16_t fx_course_next_distance(uint16_t car, uint16_t n);
void fx_car_place_on_segment(uint16_t car);
void fx_car_catch_up(uint16_t car);
void fx_drive_decide(uint16_t car, int16_t x, int16_t z, int16_t want);
int16_t fx_steer_from_input(uint16_t car, uint16_t bits);

/* ---- object/fx_motion.c ---- */
void fx_record_move(uint16_t rec, uint16_t kind);

#endif
