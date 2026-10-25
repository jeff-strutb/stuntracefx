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

/* ---- object/fx_motion.c ---- */
void fx_record_move(uint16_t rec, uint16_t kind);

#endif
