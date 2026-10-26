/*
 * objects.h: the game's objects, kept by the S-CPU: records of $62 bytes
 * in work RAM from $2000, each named by its offset there (OBJ, state.h).
 * Records in use form a list (next at +$00, previous at +$02), the rest a
 * free list; each is also in one of the buckets that place it on the map
 * (+$3C next, +$3E previous, +$40 the bucket, the heads in bank $7F).
 * An object's work is done by its handlers, routines named by their
 * address in the original (+$1A/+$1C, run each frame, and +$1D/+$1F, run
 * every third), found in the port's table of them (object/handlers.c).
 */
#ifndef SRFX_OBJECTS_H
#define SRFX_OBJECTS_H

#include <stdbool.h>
#include <stdint.h>

#define OBJ_W(o, f)     (*(w16 *)OBJ(o, f))
#define obj_next(o)     OBJ_W(o, 0x00)
#define obj_prev(o)     OBJ_W(o, 0x02)
#define obj_kind(o)     OBJ_W(o, 0x04)
#define obj_handler(o)  OBJ_W(o, 0x1A)  /* bank at +$1C */
#define obj_handler_b(o) OBJ_W(o, 0x1D) /* bank at +$1F */
#define obj_record(o)   OBJ_W(o, 0x22)  /* its Super FX record, or 0 */
#define obj_vel(o, i)   OBJ_W(o, 0x2E + 2 * (i))
#define obj_countdown(o) (*OBJ(o, 0x36))
#define obj_link(o)     OBJ_W(o, 0x4F)

#define objects_used    W16(0x0329)
#define objects_last    W16(0x032B)
#define objects_free    W16(0x032D)
#define object_running  W16(0x034C)     /* the object whose handler runs */
#define object_release_asked W8(0x035C) /* its handler asked for it to be freed */

/* object/objects.c */
void object_pool_init(void);
void object_init(uint16_t obj);
void object_bucket_insert(uint16_t obj);
void object_bucket_remove(uint16_t obj);
bool object_alloc(uint16_t *obj);
bool object_spawn(uint16_t *obj);
void object_free(uint16_t obj);
void object_release(uint16_t obj);
void objects_run_handlers(void);
void object_call_handler(uint16_t obj);
void object_copy_14(uint16_t from, uint16_t to);
void object_copy_position(uint16_t from, uint16_t to);
void object_apply_velocity(uint16_t obj);
void object_list_append(uint16_t obj);
void object_linked_set_1a(uint16_t obj, uint16_t v);

/* object/handlers.c: a handler by its address in the original */
typedef void ObjectHandler(uint16_t obj);
ObjectHandler *object_handler(uint32_t addr);
void object_handler_run(uint32_t addr, uint16_t obj);

#endif
