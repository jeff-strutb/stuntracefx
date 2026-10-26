/*
 * objects.c: the object table (objects.h): taking and giving back
 * records, the buckets, the frame's run of the handlers, and the small
 * moves and copies the handlers share.
 */
#include "game.h"
#include "objects.h"

#define BUCKET(b)       W16(0x10000u + (b))     /* the list heads, bank $7F */
#define object_count    W8(0x0CEE)              /* the objects the course has made */

/* Every record free, in order. */
void object_pool_init(void)
{
    uint16_t x = 1;
    int i;

    objects_used = objects_last = 0;
    objects_free = 1;
    for (i = 0; i < 0x135; i++, x += 0x62)
        obj_next(x) = (uint16_t)(x + 0x62);
    obj_next(x) = 0;
}

/* At the head of its bucket's list. */
void object_bucket_insert(uint16_t obj)
{
    uint16_t head = BUCKET(OBJ_W(obj, 0x40));

    OBJ_W(obj, 0x3E) = 0;
    OBJ_W(obj, 0x3C) = head;
    if (head)
        OBJ_W(head, 0x3E) = obj;
    BUCKET(OBJ_W(obj, 0x40)) = obj;
}

void object_bucket_remove(uint16_t obj)
{
    uint16_t prev = OBJ_W(obj, 0x3E), next = OBJ_W(obj, 0x3C);

    if (prev) {
        OBJ_W(prev, 0x3C) = next;
        if (next)
            OBJ_W(next, 0x3E) = prev;
        return;
    }
    BUCKET(OBJ_W(obj, 0x40)) = next;
    if (next)
        OBJ_W(next, 0x3E) = 0;
}

/* Cleared from +$04 on, in bucket 0, its countdown 0, +$59 = $69. */
void object_init(uint16_t obj)
{
    int i;

    for (i = 0; i < 0x2F; i++)
        OBJ_W(obj, 0x04 + 2 * i) = 0;
    OBJ_W(obj, 0x40) = 0;
    object_bucket_insert(obj);
    obj_countdown(obj) = 0;
    *OBJ(obj, 0x59) = 0x69;
}

/* A record from the free list, put in the used list after the last one
   taken (or at its head when there is none). False when none is free. */
bool object_alloc(uint16_t *obj)
{
    uint16_t last = objects_last, rec = objects_free, next;

    if (!rec)
        return false;
    objects_free = obj_next(rec);
    if (!last) {
        obj_next(rec) = objects_used;
        obj_prev(rec) = 0;
        objects_used = rec;
    } else {
        obj_next(rec) = obj_next(last);
        obj_next(last) = rec;
        obj_prev(rec) = last;
    }
    objects_last = rec;
    if ((next = obj_next(rec)))
        obj_prev(next) = rec;
    *obj = rec;
    return true;
}

/* A new object, cleared, its countdown 3 and its kind $1F2B, unless the
   course has made its $2E or none is free. */
bool object_spawn(uint16_t *obj)
{
    if (!((uint8_t)(object_count - 0x2E) & 0x80) || !object_alloc(obj)) {
        *obj = 0;
        return false;
    }
    object_init(*obj);
    obj_countdown(*obj) = 3;
    obj_kind(*obj) = W16(0x1F2B);
    return true;
}

/* Out of its bucket and the used list, to the head of the free list. As
   the original, freeing the used list's head also forgets the last one
   taken, and gives its next (or record 0) no previous. */
void object_free(uint16_t obj)
{
    uint16_t prev = obj_prev(obj), next = obj_next(obj);

    object_bucket_remove(obj);
    if (!prev) {
        objects_used = next;
        obj_prev(next) = 0;
        objects_last = 0;
    } else {
        obj_next(prev) = next;
        if (next)
            obj_prev(next) = prev;
        else
            objects_last = prev;
    }
    obj_next(obj) = objects_free;
    objects_free = obj;
}

/* Freed, unless it is the object whose handler is running: that one is
   counted to be freed after its handler. */
void object_release(uint16_t obj)
{
    if (obj == object_running)
        object_release_asked++;
    else
        object_free(obj);
}

/* A handler (word at `at`, bank at `at` + 2) run for the object, kept at
   $0354-$0356 and the object at $034C as the original keeps them. */
static void run_handler(uint16_t obj, uint8_t at)
{
    uint16_t h;

    W8(0x0356) = *OBJ(obj, at + 2);
    if (!(h = OBJ_W(obj, at)))
        return;
    W16(0x0354) = h;
    if (at == 0x1D && obj_record(obj))
        return;
    object_running = obj;
    object_handler_run((uint32_t)W8(0x0356) << 16 | h, obj);
}

/* The objects' frame: the frame counted ($05E9); then each object's
   countdown: a negative one restarts at 3 and runs the second handler,
   one above 1 counts down and runs the first, one at 1 stops (its record
   and countdown cleared); an object its handler asked to free is freed. */
void objects_run_handlers(void)
{
    uint16_t x, next;

    W16(0x05E9)++;
    for (x = objects_used; x; x = next) {
        uint8_t v;

        object_release_asked = 0;
        v = obj_countdown(x);
        if (v == 1) {
            *OBJ(x, 0x22) = *OBJ(x, 0x23) = 0;
            obj_countdown(x) = 0;
        } else if (v) {
            if (v & 0x80) {
                obj_countdown(x) = 3;
                run_handler(x, 0x1D);
            } else {
                obj_countdown(x) = (uint8_t)(v - 1);
                run_handler(x, 0x1A);
            }
            if (object_release_asked) {
                next = obj_next(x);
                object_free(x);
                continue;
            }
        }
        next = obj_next(x);
    }
}

/* The object's first handler, $1F58 kept across it. */
void object_call_handler(uint16_t obj)
{
    uint8_t keep = W8(0x1F58);

    object_handler_run((uint32_t)*OBJ(obj, 0x1C) << 16 | obj_handler(obj), obj);
    W8(0x1F58) = keep;
}

void object_copy_14(uint16_t from, uint16_t to)
{
    int i;

    for (i = 0; i < 3; i++)
        *OBJ(to, 0x14 + i) = *OBJ(from, 0x14 + i);
}

void object_copy_position(uint16_t from, uint16_t to)
{
    int i;

    for (i = 0; i < 3; i++)
        OBJ_W(to, 0x0C + 2 * i) = OBJ_W(from, 0x0C + 2 * i);
}

void object_apply_velocity(uint16_t obj)
{
    int i;

    for (i = 0; i < 3; i++)
        OBJ_W(obj, 0x0C + 2 * i) = (uint16_t)(OBJ_W(obj, 0x0C + 2 * i) + obj_vel(obj, i));
}

/* On the list at $FFBD (32 at most, its length in bytes at $FFFD). */
void object_list_append(uint16_t obj)
{
    uint16_t n = W16(0xFFFD);

    if (n == 0x40)
        return;
    W16(0xFFBD + n) = obj;
    W16(0xFFFD) = n = (uint16_t)(n + 2);
    W16(0xFFBD + n) = 0;
}

/* +$1A of the object it links to. */
void object_linked_set_1a(uint16_t obj, uint16_t v)
{
    OBJ_W(obj_link(obj), 0x1A) = v;
}
