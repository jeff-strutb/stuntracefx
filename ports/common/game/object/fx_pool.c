/*
 * fx_pool.c: the Super FX's two pools of record memory, and the records'
 * lists.
 *
 * A pool is a chain of blocks from its head block ($1098 or $1D70): a
 * block's first word links to the next (0 ends the chain), its second is
 * its size in bytes, header included. A block taken whole stays on the
 * chain with its size negated; a record cut from the end of a bigger
 * block is off the chain, its size positive. Records in use are on the
 * list of all records at $2332 (+$04 next, +$06 previous); a record's
 * contacts are on its list from +$18 (linked through +$0E, bit 0 of +$10
 * marking those still wanted).
 */
#include "game.h"
#include "physics.h"

#define FX_ALL_RECORDS          0x2332u
#define FX_NEW_RECORD       0x238Eu         /* the new record, for the S-CPU (0: none) */

/* A block of `size` bytes from the pool at `head`: one of exactly that
   size, else the end of the first bigger one (taken whole when less than
   4 bytes would be left). Returns it, or 0. */
uint16_t fx_pool_block(uint16_t head, uint16_t size)
{
    uint16_t b;

    for (b = head; b; b = FXU(b))
        if (FXU(b + 2) == size) {
            FXU(b + 2) = (uint16_t)-size;
            return b;
        }
    for (b = head; b; b = FXU(b)) {
        int16_t spare = wrap16(FXU(b + 2) - size);

        if (spare < 0)
            continue;
        if (wrap16(spare - 4) < 0) {
            FXU(b + 2) = (uint16_t)-(uint16_t)(size + spare);
            return b;
        }
        FXU(b + 2) = (uint16_t)spare;
        FXU(b + spare + 2) = size;
        return (uint16_t)(b + spare);
    }
    return 0;
}

/* A block given back: one taken whole is free again where it is; one cut
   from another goes on the front of the chain at `head`. */
static void block_free(uint16_t b, uint16_t head)
{
    int16_t size = FXW(b + 2);

    if (size < 0) {
        FXW(b + 2) = wrap16(-size);
    } else {
        FXU(b) = FXU(head);
        FXU(head) = b;
    }
}

/* A new record of `size` bytes from the pool at `head`: cleared from +4,
   +$08 = the pool, on the front of the list of all records, +$1C bit 2
   set. Returns it, or 0. */
uint16_t fx_pool_take(uint16_t head, uint16_t size)
{
    uint16_t r = fx_pool_block(head, size), next, i;
    int16_t s, n;

    if (!r)
        return 0;
    s = FXW(r + 2);
    n = wrap16((s < 0 ? wrap16(-s) : s) - 4);
    for (i = 0; n > 0 && i < (uint16_t)n; i++)
        pakram[(uint16_t)(r + 4 + i)] = 0;
    FXU(r + 8) = head;
    next = FXU(FX_ALL_RECORDS);
    FXU(r + 4) = next;
    FXU(FX_ALL_RECORDS) = r;
    if (next)
        FXU(next + 6) = r;
    FXU(r + 0x1C) |= 4;
    return r;
}

/* Neighbouring free blocks merged, the first pool's chain then the
   second's: a free block whose end is the next block, that block free,
   takes it in; the walk goes on from the block taken in. */
static void merge(uint16_t b)
{
    while (FXU(b)) {
        uint16_t next = FXU(b);
        int16_t size = FXW(b + 2);

        if (size >= 0 && (uint16_t)(b + size) == next && FXW(next + 2) >= 0) {
            FXW(b + 2) = wrap16(size + FXW(next + 2));
            FXU(b) = FXU(next);
        }
        b = next;
    }
}

void fx_pools_merge(void)
{
    merge(0x1098);
    merge(0x1D70);
}

/* bit 0 of +$10 cleared in every record of the list at owner + $18 */
void fx_list_clear_bit0(uint16_t owner)
{
    uint16_t r;

    for (r = FXU(owner + 0x18); r; r = FXU(r + 0x0E))
        FXU(r + 0x10) &= (uint16_t)~1;
}

/* The list at owner + $18 swept: a record marked (bit 0 of +$10) stays,
   unmarked; one not marked is unlinked and its block given back (to the
   first pool). */
void fx_list_sweep(uint16_t owner)
{
    uint16_t r = FXU(owner + 0x18), kept = 0;

    while (r) {
        uint16_t next = FXU(r + 0x0E);

        if (FXU(r + 0x10) & 1) {
            FXU(r + 0x10) &= (uint16_t)~1;
            kept = r;
        } else {
            if (kept)
                FXU(kept + 0x0E) = next;
            else
                FXU(owner + 0x18) = next;
            block_free(r, 0x1098);
        }
        r = next;
    }
}

static uint16_t children_next;  /* the next child to free (R4, which a nested free leaves 0) */

static void object_free_children(uint16_t rec);

/* A record freed: its contacts swept, its children freed, taken off the
   list of all records and its block given back to its pool (+$08). */
void fx_object_free(uint16_t rec)
{
    uint16_t prev, next;

    if (FXU(rec + 0x18)) {
        fx_list_clear_bit0(rec);
        fx_list_sweep(rec);
    }
    if (FXU(rec + 0x16) & 2)
        object_free_children(rec);
    prev = FXU(rec + 6);
    next = FXU(rec + 4);
    if (prev)
        FXU(prev + 4) = next;
    else
        FXU(FX_ALL_RECORDS) = next;
    if (next)
        FXU(next + 6) = prev;
    block_free(rec, FXU(rec + 8));
}

/* Each child (from +$3E, linked through +$34) freed; a child's own
   children end the walk. */
static void object_free_children(uint16_t rec)
{
    children_next = FXU(rec + 0x3E);
    while (children_next) {
        uint16_t c = children_next;

        children_next = FXU(c + 0x34);
        fx_object_free(c);
    }
}

/* ---- the programs that make records for the S-CPU ---- */

static uint16_t record_new(uint16_t size, uint16_t pool)
{
    uint16_t r = fx_pool_take(pool, size);

    FXU(FX_NEW_RECORD) = r;
    return r;
}

static void record_new_first(uint16_t mass, uint16_t flags, uint16_t mode)
{
    uint16_t r = record_new(0x34, 0x1098);

    if (r) {
        FXU(r + 0x2A) = mass;
        FXU(r + 0x16) = flags;
        FXU(r + 0x1A) = mode;
    }
}

void fx_record_new_426(void) { record_new_first(0x40, 0x08, 0x0426); }
void fx_record_new_424(void) { record_new_first(0x40, 0x08, 0x0424); }
void fx_record_new_426b(void) { record_new_first(0x40, 0x20, 0x0426); }
void fx_record_new_42d(void) { record_new_first(0x00, 0x20, 0x042D); }

void fx_record_new_camera(void)
{
    uint16_t r = record_new(0x34, 0x1098);

    if (r) {
        FXU(r + 0x16) = 0x20;
        FXU(r + 0x1A) = 0x042E;
    }
}

/* a $2A-byte record from the second pool, counted at $2482 and put at the
   head of the list at $2398 (linked back through +$26) */
void fx_record_new_422(void)
{
    uint16_t r = record_new(0x2A, 0x1D70), head;

    if (!r)
        return;
    FXU(0x2482)++;
    head = FXU(0x2398);
    if (head)
        FXU(head + 0x26) = r;
    FXU(0x2398) = r;
    FXU(r + 0x16) |= 0x0181;
    FXU(r + 0x1A) = 0x0422;
}
