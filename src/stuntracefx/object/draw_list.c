/*
 * draw_list.c: the list of objects the Super FX draws this frame, built in
 * work RAM from the objects in the buckets the trigger list names.
 */
#include <string.h>

#include "machine.h"
#include "remaster.h"

#define DRAW_COUNT 0x7E0CEEu      /* records written (at most $31) */
#define TRIGGER_POS 0x7E1F2Bu     /* offset into the trigger list */
#define TRIGGER_WORD 0x7E1F2Du    /* the current trigger's first word */
#define TRIGGER_LIST 0x7E1AF5u    /* two words per trigger, 0 ends it */
#define DRAW_NEXT 0x00003Eu       /* direct page: the next record's address */
#define DRAW_LIST 0xF77Fu         /* the records, 30 bytes each, bank $7E */
#define DRAW_SIDE 0x7E1F07u       /* nonzero: the view is on the far side */
#define OBJ(o, f) (0x7E2000u + (uint16_t)((f) + (o)))
#define REC(r, f) (rec_bank + (uint16_t)((f) + (r)))

/* Remaster, Enhanced: the bank the records go to (work RAM's; the far
   records' in the Super FX's second bank, rm_far_records), whether this
   one is far (the object's own fields left as they are, not counted),
   and the objects given a near record this frame */
static uint32_t rec_bank = 0x7E0000u;
static int rec_far;
static uint8_t rec_near[0x8000];

/* Object `o`'s draw record at `r`: position (relative to the view at
   $C5/$C7/$C9 unless bit 7 of +$24 says absolute) and its depth key at
   +$02, model +$08, angles +$04-$07, link fields or 0, flags, palette and
   frame bytes and the animation word. */
static uint16_t draw_record(uint16_t o, uint16_t r)
{
    uint16_t a, t;
    uint8_t v;

    if (rd16(OBJ(o, 0x24)) & 0x0080) {
        wr16(REC(r, 0x12), (rd16(OBJ(o, 0x0C))));
        wr16(REC(r, 0x10), (rd16(OBJ(o, 0x0E))));
        wr16(REC(r, 0x14), (rd16(OBJ(o, 0x10))));
        wr16(REC(r, 0x02), 0xF060);
        /* BRA */
    } else {
        a = rd16(OBJ(o, 0x0E));
        a = (uint16_t)(a - rd16(0x0000C7));
        wr16(REC(r, 0x10), a);
        if (a & 0x8000)
            a = (uint16_t)(~a + 1);
        wr16(REC(r, 0x02), (uint16_t)(a << 1));
        t = (rd16(OBJ(o, 0x09)) & 0x0001) ? (0x1F40) : 0x0000;
        wr16(REC(r, 0x02), ((uint16_t)(t + rd16(REC(r, 0x02)))));
        a = rd16(OBJ(o, 0x0C));
        wr16(REC(r, 0x12), ((uint16_t)(a - rd16(0x0000C5))));
        a = rd16(OBJ(o, 0x10));
        wr16(REC(r, 0x14), ((uint16_t)(a - rd16(0x0000C9))));
    }
    wr16(REC(r, 0x08), (rd16(OBJ(o, 0x04))));
    wr16(REC(r, 0x04), (rd16(OBJ(o, 0x14))));
    wr16(REC(r, 0x05), (rd16(OBJ(o, 0x15))));
    wr16(REC(r, 0x06), (rd16(OBJ(o, 0x16))));
    if (rd16(OBJ(o, 0x24)) & 0x0004) {
        wr16(REC(r, 0x0A), o);
        wr8(REC(r, 0x0C), (rd8(OBJ(o, 0x48))));
        wr8(REC(r, 0x0D), (rd8(OBJ(o, 0x49))));
        wr8(REC(r, 0x0E), (rd8(OBJ(o, 0x4A))));
        /* BRA */
    } else {
        wr8(REC(r, 0x18), 0x00);
    }
    v = rd8(OBJ(o, 0x24));
    wr8(REC(r, 0x07), v);
    if (!rec_far)
        wr8(OBJ(o, 0x24), (uint8_t)(v & 0xFD));
    v = rd8(OBJ(o, 0x5A));
    if (!(v & 0x80))
        v = rd8(0x7E05E9);
    wr8(REC(r, 0x19), (uint8_t)(v & 0x7F));
    if (rd8(DRAW_SIDE)) {
        v = rd8(OBJ(o, 0x19));
        if (!(v & 0x80))
            v = rd8(0x7E05E9);
    } else {
        v = rd8(OBJ(o, 0x18));
        if (!(v & 0x80))
            v = rd8(0x7E05E9);
    }
    wr8(REC(r, 0x1A), (uint8_t)(v & 0x7F));
    wr8(REC(r, 0x1B), (rd8(OBJ(o, 0x3A))));
    wr8(REC(r, 0x1C), (rd8(OBJ(o, 0x5F))));
    wr8(REC(r, 0x1D), (rd8(OBJ(o, 0x60))));
    wr16(REC(r, 0x16), (rd16(OBJ(o, 0x5D))));
    if (rec_far)
        return (uint16_t)(r + RM_FAR_SIZE);
    rec_near[o & 0x7FFF] = 1;
    wr16_rmw(DRAW_COUNT, ((uint16_t)(rd16_rmw(DRAW_COUNT) + 1)));
    return (uint16_t)(r + 0x1E);
}

/* Whether object `o` is drawn: not when bit 5 of +$24 is set; with bit 6
   set, kind 2 (+$26) only on the near side ($1F07 = 0) and the rest only
   on the far side; then at most $31 records, and never model $A040. */
static int drawn(uint16_t o)
{
    uint8_t f = rd8(OBJ(o, 0x24));
    int16_t n;

    if (f & 0x20)
        return 0;
    if (f & 0x40) {
        if (rd8(OBJ(o, 0x26)) == 0x02) {
            if (rd8(DRAW_SIDE))
                return 0;
        } else { if (!rd8(DRAW_SIDE)) {
            return 0;
        } }
    }
    n = (int16_t)(rd16(DRAW_COUNT) - 0x0031);
    if (n >= 0)
        return 0;
    return (rd16(OBJ(o, 0x04)) != 0xA040);
}

/*
 * @routine 7E:EF50 ObjectDrawListBuild
 * WHAT IT DOES: Builds the list of objects to draw: for each trigger (two
 * words at $1AF5, 0 ends the list), walks the objects of the bucket its
 * second word names ($7F:0000 + the word AND $7FE, linked by +$3C) and,
 * for each one drawn (not hidden by bit 5 of +$24; with bit 6, kind 2 only
 * on the near side and the others only on the far side, $1F07; at most
 * $31; never model $A040), writes a 30-byte draw record from $7E:F77F:
 * position relative to the view and a depth key, model, angles, link and
 * frame fields, flags (clearing bit 1 of +$24) and palette bytes. $0CEE
 * counts the records; $034C holds the object being looked at.
 * HOW: Data bank $7E; the record address is kept in direct page $3E
 * between buckets, the trigger position in $1F2B. The test of bit 6 for
 * kind-2 objects is not reached in the traces so far.
 * Entry: D = $0000. Exit: A = 0; X = the trigger position; Y = the last
 * object looked at (0 after a bucket's end); P and the data bank unchanged.
 */
/* The objects of a bucket from o (o itself first unless `next`), each
   drawn writing its record from r; then $3E = the next record and Y = 0. */
static void draw_bucket(uint16_t o, uint16_t r, int next)
{
    do {
        if (!next) {
            wr16(0x7E034C, o);
            if (drawn(o))
                r = draw_record(o, r);
        }
        next = 0;
        o = rd16(OBJ(o, 0x3C));
    } while ((o));
    cpu.y = 0;
    wr16(DRAW_NEXT, r);
}

/* The triggers from $1F2B on, each bucket's objects drawn; A = 0 and X =
   the trigger position at the end. `end` is the time from the last list
   word read to the end: the exit's PLB, PLP and RTS, or, for the arm that
   reads the pushed data bank itself, up to that read. */
static void draw_triggers(void)
{
    uint16_t pos, a;

    for (;;) {
        pos = rd16(TRIGGER_POS);
        a = rd16(TRIGGER_LIST + pos);
        cpu.x = pos;
        cpu.a = a;
        if (!a)
            return;
        wr16(TRIGGER_WORD, a);
        a = rd16(TRIGGER_LIST + 2 + pos);
        pos = (uint16_t)(pos + 4);
        wr16(TRIGGER_POS, pos);
        a = rd16(0x7F0000 + (a & 0x07FE));
        if (!a)
            continue;
        draw_bucket(a, rd16(DRAW_NEXT), 0);
    }
}

/* Remaster, Enhanced: view distance past the original's. The draw list
   comes from the cells of a block ahead of the view (the trigger list),
   at most $31 records; every other object the list would draw, in every
   cell of the map, gets a record too, in the Super FX's second bank
   ($71, RM_FAR_*), with its place from the view in 32 bits, which the
   Super FX's frame program draws behind the near ones (render/frame.c).
   The objects themselves are not touched: what is activated, and when,
   is the trigger list's as before. Left out: objects placed in view
   space (+$24 bit 7) and effect emitters (bit 2), the near cell's. */
static void far_record(uint16_t o, unsigned i)
{
    uint16_t r = (uint16_t)(RM_FAR_RECORDS + i * RM_FAR_SIZE);
    int32_t d[3];
    unsigned k;

    draw_record(o, r);
    d[0] = (int16_t)rd16(OBJ(o, 0x0C)) - (int16_t)rd16(0x0000C5);
    d[1] = (int16_t)rd16(OBJ(o, 0x0E)) - (int16_t)rd16(0x0000C7);
    d[2] = (int16_t)rd16(OBJ(o, 0x10)) - (int16_t)rd16(0x0000C9);
    for (k = 0; k < 3; k++) {
        wr16(REC(r, RM_FAR_PLACE + 4 * k), (uint16_t)d[k]);
        wr16(REC(r, RM_FAR_PLACE + 4 * k + 2), (uint16_t)((uint32_t)d[k] >> 16));
    }
    /* the depth key's lift for drawing first (as draw_record's $1F40) */
    wr8(REC(r, RM_FAR_LIFT), (uint8_t)(rd16(OBJ(o, 0x09)) & 0x0001));
}

static void far_records(void)
{
    unsigned cell, n = 0;
    uint16_t o;

    rec_bank = 0x710000u;
    rec_far = 1;
    for (cell = 0; cell < 0x400; cell++)
        for (o = rd16(0x7F0000 + 2 * cell); o; o = rd16(OBJ(o, 0x3C))) {
            uint8_t f = rd8(OBJ(o, 0x24));
            if (rec_near[o & 0x7FFF] || (f & 0x84) || n >= RM_FAR_MAX)
                continue;
            if ((f & 0x20) || rd16(OBJ(o, 0x04)) == 0xA040 || rd16(OBJ(o, 0x04)) < 0x8000)
                continue;
            if (f & 0x40) {     /* drawn on one side only, as drawn() */
                if ((rd8(OBJ(o, 0x26)) == 0x02) == (rd8(DRAW_SIDE) != 0))
                    continue;
            }
            far_record(o, n++);
        }
    wr16(0x710000u | RM_FAR_COUNT, (uint16_t)n);
    rec_bank = 0x7E0000u;
    rec_far = 0;
}

void ObjectDrawListBuild(void)
{
    int far = rm_enhanced() && rm_whole_steps();     /* a race's (not the menus' 3D) */

    if (far)
        memset(rec_near, 0, sizeof rec_near);
    wr16(DRAW_COUNT, 0x0000);
    wr16(TRIGGER_POS, 0x0000);
    wr16(DRAW_NEXT, DRAW_LIST);
    draw_triggers();
    if (far)
        far_records();
}

void abi_ObjectDrawListBuild(void)
{
    uint8_t p = cpu.p;

    ObjectDrawListBuild();
    cpu.p = p;
}

/*
 * @routine 7E:F0D3 ObjectDrawListClear08
 * WHAT IT DOES: An arm after ObjectDrawListBuild's RTS that nothing
 * reaches: clears bit 1 of object Y's +$08, then goes on with
 * ObjectDrawListBuild's walk from the object after it in its bucket (+$3C)
 * and the rest of the trigger list ($1F2B), X the next draw record. A
 * direct call is its only way in.
 * HOW: SEP #$20 for the AND #$FD, REP #$20, BRA into the bucket loop
 * (7E:F0C0); ends with ObjectDrawListBuild's PLB, PLP, RTS.
 * Entry: M 16-bit, X 16-bit, D = $0000, data bank $7E; Y = object; X = the
 * next draw record; the caller's P and data bank pushed above the return
 * (PHP, PHB). Exit: as ObjectDrawListBuild, P and the data bank those
 * pushed.
 */
void abi_ObjectDrawListClear08(void)
{
    uint8_t db, p;

    wr8(OBJ(cpu.y, 0x08), ((uint8_t)(rd8(OBJ(cpu.y, 0x08)) & 0xFD)));
    draw_bucket(cpu.y, cpu.x, 1);
    draw_triggers();
    db = rd8((uint16_t)(cpu.s + 1));
    p = rd8((uint16_t)(cpu.s + 2));
    cpu.db = db;
    cpu.p = p;
    if (p & P_X) {
        cpu.x &= 0x00FF;
        cpu.y &= 0x00FF;
    }
}
