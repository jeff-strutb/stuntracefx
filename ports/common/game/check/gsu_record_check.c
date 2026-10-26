/* old: object/gsu_record.c object/fx_pool.c object/fx_record.c gsu/run.c math/scale.c object/object.c math/random.c audio/sound_queue.c
 *
 * gsu_record_check.c: the port's object records (object/gsu_record.c)
 * against the decomp's, on random states. Each case runs the decomp's
 * routine on a copy and the port's on the state, and compares both RAMs
 * (the direct page aside) and the results the caller uses. The records
 * the course gives new objects run the decomp's Super FX programs through
 * the bus model (model_gsu_start), as the game's own calls do; the pools
 * they take from are built whole first.
 *
 *   check/check.sh gsu_record N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define gsu old_gsu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#define gsu_rd8 old_gsu_rd8
#define gsu_wr8 old_gsu_wr8
#define gsu_plot old_gsu_plot
#define gsu_rpix old_gsu_rpix
#define gsu_code old_gsu_code
#define gsu_cache old_gsu_cache
#define gsu_cyc old_gsu_cyc
#define gsu_time old_gsu_time
#define gsu_buffer old_gsu_buffer
#include "machine.h"
#include "cpu_ops.h"
#include "gsu.h"
#undef cpu
#undef gsu

#include "game.h"
#include "glue.h"
#include "objects.h"
#include "gsu_record.h"
#include "model.h"

#define obj_parent(o) OBJ_W(o, 0x06)

#define OLD(name) void old_abi_##name(void);
OLD(GsuRecordMode2) OLD(GsuRecordSetFlags) OLD(GsuRecordFlag10) OLD(GsuRecordPosition) OLD(GsuRecordWord88)
OLD(GsuRecordWord58) OLD(GsuRecordFlag4) OLD(GsuRecordVelocity) OLD(GsuRecordWords88) OLD(GsuRecordStep6A)
OLD(ObjectVelocityFromRecordOf) OLD(GsuRecordFromObject) OLD(ObjectVelocityFromRecordScaled) OLD(GsuRecordBits60)
OLD(GsuRecordNew422) OLD(GsuRecordNew42D) OLD(GsuRecordNew424Piece) OLD(GsuRecordNew424) OLD(GsuRecordNew424Alt)
OLD(GsuRecordNewCamera) OLD(GsuRecordNew426) OLD(GsuRecordFlags8ASet) OLD(GsuRecordFlags8AClear)
OLD(RecordContactFlags) OLD(GsuRecordLand) OLD(RecordNextContact) OLD(RecordContactSummary)
OLD(ObjectPoseFromRecord) OLD(GsuRecordSet2AHigh)
OLD(FxRecordNew422) OLD(FxRecordNew424) OLD(FxRecordNew426) OLD(FxRecordNew42D) OLD(FxRecordNewCamera)
OLD(FxRecordNew426B) OLD(FxRecordRaise6A) OLD(FxRecordLower82)
OLD(GsuRecordLower82) OLD(GsuRecordRaise6A) OLD(ObjectTurnFromRecord)
OLD(ObjectSyncFromRecord) OLD(ObjectSyncToRecord) OLD(ObjectSyncAngles) OLD(ObjectSyncFromRecordFall)
OLD(ObjectSyncFallStep) OLD(ObjectRecordMarkStep) OLD(ObjectSpinStep) OLD(ObjectSpinRecord)
OLD(ObjectSpinSetup)
OLD(RecordButtonSequence) OLD(CarControlsToRecord) OLD(PlayerControlsFrame)

static uint64_t rng = 0x3C6EF372FE94F82Bull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static long within(long lo, long hi)
{
    return lo + (long)(rnd() % (unsigned long)(hi - lo + 1));
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration;
static int poll_counts;             /* the case starts Super FX programs: their poll counts ($FE-$FF) and R10 stack are the decomp's own */

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 20)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld: ", model_current, iteration);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* ---- the decomp's Super FX programs the record routines start, by where
   they start (bank $04) ---- */
static void old_start(uint8_t bank, uint16_t start)
{
    static const struct {
        uint32_t at;
        void (*run)(void);
    } olds[] = {
        {0x048B33, old_abi_FxRecordNew426},
        {0x048B64, old_abi_FxRecordNew422},
        {0x048C0A, old_abi_FxRecordNew424},
        {0x048C6C, old_abi_FxRecordNewCamera},
        {0x048C95, old_abi_FxRecordNew42D},
        {0x048C3B, old_abi_FxRecordNew426B},
        {0x04BF51, old_abi_FxRecordRaise6A},
        {0x04BF75, old_abi_FxRecordLower82},
    };
    uint32_t at = (uint32_t)(bank & 0x7F) << 16 | start;
    unsigned i;

    for (i = 0; i < sizeof olds / sizeof olds[0]; i++)
        if (olds[i].at == at) {
            old_gsu.r[10] = 0x0240;
            olds[i].run();
            return;
        }
    fprintf(stderr, "check: %s: the decomp started $%06X, which the check lacks\n", model_current, (unsigned)at);
    exit(2);
}

/* A record's address in the game pak RAM: an even address in the first
   pool's area (the records of the objects). */
static uint16_t rec_addr(void)
{
    return (uint16_t)(0x1100 + 2 * (rnd() % 0x200));
}

/* An object's offset in the table (the records are $62 bytes from $0001). */
static uint16_t obj_of(void)
{
    return (uint16_t)(1 + 0x62 * (rnd() % 0x135));
}

/* Random RAM; every object given a record (mostly) and a parent that is an
   object of the table. */
static void fresh(void)
{
    size_t k;
    int i;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    for (i = 0; i < 0x135; i++) {
        uint16_t o = (uint16_t)(1 + 0x62 * i);

        obj_record(o) = rnd() % 4 ? rec_addr() : 0;
        obj_parent(o) = obj_of();
    }
    memset(&old_cpu, 0, sizeof old_cpu);
    memset(&old_gsu, 0, sizeof old_gsu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = obj_of();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = P_M;
    poll_counts = 0;
    model_pixels_empty();
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    model_pixels_flush();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same(void)
{
    size_t i;

    cases++;
    for (i = 0x100; i < sizeof wram; i++) {
        if (poll_counts && (i == 0xFE || i == 0xFF))
            continue;
        if (wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    }
    for (i = 0; i < sizeof pakram; i++) {
        if (poll_counts && i >= 0x0240 && i < 0x02A0)   /* the programs' R10 stack */
            continue;
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    }
}

/* A register the caller uses: the port's value under `mask` against the
   decomp's. */
static void reg(const char *what, unsigned port, unsigned want, unsigned mask)
{
    if (((port ^ want) & mask) != 0)
        fail("%s $%04X, the decomp's $%04X", what, port & mask, want & mask);
}

/* ---- the pools the record programs take from, and the records list (as
   the physics check builds them) ---- */
static void pool_chain(uint16_t head, uint16_t end)
{
    uint16_t b = head, last = head;

    P16(head) = 0;
    while (b + 8 < end) {
        uint16_t size = (uint16_t)(8 + 2 * (rnd() % 24)), kind = (uint16_t)(rnd() % 4);

        if (b + size > end)
            size = (uint16_t)(end - b);
        if (kind == 3 && b != head) {
            P16(b + 2) = size;
        } else {
            P16(b + 2) = kind == 2 ? (uint16_t)-size : size;
            if (b != head)
                P16(last) = b;
            P16(b) = 0;
            last = b;
        }
        b = (uint16_t)(b + size);
    }
}

static void records_list(int n)
{
    int k, j;

    P16(0x2332) = n ? 0x3000 : 0;
    for (k = 0; k < n; k++) {
        uint16_t r = (uint16_t)(0x3000 + 0x90 * k);

        P16(r + 4) = k + 1 < n ? (uint16_t)(r + 0x90) : 0;
        P16(r + 6) = k ? (uint16_t)(r - 0x90) : 0;
        P16(r + 8) = (rnd() & 1) ? 0x1098 : 0x1D70;
        FXW(r + 2) = (sw16)((rnd() & 1) ? -0x34 : 0x34);
        for (j = 0; j < 3; j++) {
            FXW(r + 0x1E + 2 * j) = (sw16)within(-0x300, 0x300);
            FXW(r + 0x0C + 2 * j) = (sw16)within(0x40, 0x200);
            FXW(r + 0x2E + 2 * j) = (sw16)within(-0x800, 0x800);
        }
        P16(r + 0x16) = (uint16_t)(rnd() & 0x00FF);
        P16(r + 0x1A) = (uint16_t)(rnd() & rnd() & 0x00FF);
        P16(r + 0x2A) = (uint16_t)within(1, 0x200);
        P16(r + 0x18) = 0;
        P16(r + 0x3E) = 0;
    }
}

/* A case that makes records: the pools whole, the records list, the list
   head random as the physics check has it. */
static void fresh_pools(void)
{
    fresh();
    poll_counts = 1;
    pool_chain(0x1098, 0x1398);
    pool_chain(0x1D70, 0x1F70);
    records_list((int)(rnd() % 4));
    P16(0x2398) = (rnd() & 1) ? 0 : (uint16_t)(0x3800 + 2 * (rnd() % 0x400));
}

/* A contact list of `n` records from $1600 on, $100 bytes apart: each entry
   linked through +$0E, its touched record (+$0C) a record of the table, and
   bit 1 of +$10 set by chance. Returns the first entry, 0 for none. */
static uint16_t contact_chain(int n)
{
    int k;
    uint16_t head = 0;

    for (k = n - 1; k >= 0; k--) {
        uint16_t e = (uint16_t)(0x1600 + 0x100 * k);

        P16(e + 0x0E) = head;
        P16(e + 0x10) = (uint16_t)(rnd() & 0x0003);
        P16(e + 0x0C) = rec_addr();
        head = e;
    }
    return head;
}

/* The records a caller's record gives its contact list: its +$18. */
static void rec_contacts(uint16_t rec, int n)
{
    P16(rec + 0x18) = contact_chain(n);
}

/* The record of the object, with its fields the way the game keeps them. */
static uint16_t object_rec(uint16_t obj)
{
    uint16_t rec = rec_addr();

    obj_record(obj) = rec;
    return rec;
}


/* ---- object table with buckets, as object_more_check builds it: `n` used
   objects (in a random order) each in a bucket, with parent, link and
   position fields; the rest free. The record of each (+$22) is set by the
   case. ---- */
static uint16_t used[0x136];
static int nused;

static uint16_t obj_at(int i)
{
    return (uint16_t)(1 + 0x62 * i);
}

static uint16_t some_used(void)
{
    return nused ? used[rnd() % (unsigned)nused] : obj_of();
}

static void fresh_objects(int n)
{
    int order[0x136], i, j;
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    for (k = 0xA2D9; k <= 0xF77F; k++)           /* the code copy, as at power on */
        wram[k] = ram_code_byte((uint16_t)k);
    for (i = 0; i < 0x136; i++)
        order[i] = i;
    for (i = 0x135; i > 0; i--) {
        int t;

        j = (int)(rnd() % (unsigned)(i + 1));
        t = order[i], order[i] = order[j], order[j] = t;
    }
    nused = n;
    for (i = 0; i < n; i++) {
        used[i] = obj_at(order[i]);
        obj_next(used[i]) = i + 1 < n ? obj_at(order[i + 1]) : 0;
        obj_prev(used[i]) = i ? obj_at(order[i - 1]) : 0;
    }
    objects_used = n ? used[0] : 0;
    objects_last = n ? used[n - 1 - (int)(rnd() % (unsigned)n)] : 0;
    for (i = n; i < 0x136; i++)
        obj_next(obj_at(order[i])) = i + 1 < 0x136 ? obj_at(order[i + 1]) : 0;
    objects_free = n < 0x136 ? obj_at(order[n]) : 0;
    for (k = 0x10000; k < 0x10800; k++)           /* the bucket heads, all empty */
        wram[k] = 0;
    for (i = 0; i < n; i++)
        OBJ_W(used[i], 0x40) = (uint16_t)(rnd() % 0x40 * 2);
    for (i = 0; i < n; i++)                       /* a record for most, so the lists read valid ones */
        OBJ_W(used[i], 0x22) = rnd() % 5 ? rec_addr() : 0;
    for (i = 0; i < n; i++) {
        OBJ_W(used[i], 0x27) = some_used();
        OBJ_W(used[i], 0x06) = some_used();
        OBJ_W(used[i], 0x4F) = some_used();
        object_bucket_insert(used[i]);
    }
    W8(0x0CEE) = (uint8_t)(rnd() % 0x50);
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.db = 0x7E;
    old_cpu.p = P_M;
    poll_counts = 0;
    model_pixels_empty();
}

/* A used object given a record (or none, one time in five). */
static uint16_t used_with_record(void)
{
    uint16_t obj = some_used();

    obj_record(obj) = rnd() % 5 ? rec_addr() : 0;
    return obj;
}

/* The linked-object handler's state, as object_more_check shapes it for the
   object `obj` (its record's +$88 and +$42 and the $0DD6 table). */
static void linked_state(uint16_t obj)
{
    uint16_t y;

    for (y = 0; y < 2; y++)
        W8(0x0DD6 + y) = (uint8_t)(rnd() % 4 == 0 ? (uint8_t)rnd() : (rnd() % 2 ? 0x08 : (rnd() % 2 ? 0x0C : 0x0A)));
    W8(0x0E59) = (uint8_t)(rnd() & 1);
    W8(0x19ED) = (uint8_t)(rnd() & 1 ? 0x08 : rnd());
    if (obj_record(obj)) {
        P8(0x88 + obj_record(obj)) = (uint8_t)(rnd() % 6);
        P16(0x42 + obj_record(obj)) = (uint16_t)rnd();
    }
    if (rnd() & 1) {
        W16(0x1AEE) = 0x0001;
    } else {
        W16(0x1AEE) = 0x0000;
        W16(0xFFB8) = 2;
        W16(0xFFAE) = some_used();
    }
}

/* The pools the record programs take from and the records list, on an
   object table already built (as fresh_pools does on fresh()). */
static void pools_on_objects(void)
{
    poll_counts = 1;
    pool_chain(0x1098, 0x1398);
    pool_chain(0x1D70, 0x1F70);
    records_list((int)(rnd() % 4));
    P16(0x2398) = (rnd() & 1) ? 0 : (uint16_t)(0x3800 + 2 * (rnd() % 0x400));
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: gsu_record_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_gsu_start = old_start;

    CASE("GsuRecordMode2") {
        uint16_t obj;
        uint16_t r;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordMode2);
        r = record_set_mode_2(obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordSetFlags") {
        uint16_t obj = obj_of(), bits = (uint16_t)rnd(), r;

        fresh();
        obj = obj_of();
        W16(0x0002) = bits;                 /* the bits, in direct page $02 */
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordSetFlags);
        r = record_or_flags(obj, bits);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordFlag10") {
        uint16_t obj;
        uint16_t r;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordFlag10);
        r = record_flags_bit4_on_bit8_off(obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordPosition") {
        uint16_t obj, x, y, z, r;

        fresh();
        obj = obj_of();
        x = (uint16_t)rnd();                /* the position, in direct page $02, $08, $94 */
        y = (uint16_t)rnd();
        z = (uint16_t)rnd();
        W16(0x0002) = x;
        W16(0x0008) = y;
        W16(0x0094) = z;
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordPosition);
        r = record_set_position(obj, x, y, z);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordWord88") {
        uint16_t obj, py = 0, r;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordWord88);
        r = parent_record_word_88(obj, &py);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
        reg("Y", py, old_cpu.y, 0xFFFF);
    }
    CASE("GsuRecordWord58") {
        uint16_t obj, py = 0, r;
        bool clear;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordWord58);
        r = parent_record_word_58(obj, &py, &clear);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
        reg("Y", py, old_cpu.y, 0xFFFF);
    }
    CASE("GsuRecordFlag4") {
        uint16_t obj, r;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordFlag4);
        r = record_mark_flag4(obj);
        same();
        reg("A low byte", r, old_cpu.a, 0x00FF);
    }
    CASE("GsuRecordVelocity") {
        uint16_t obj, r;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordVelocity);
        r = record_take_velocity(obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordWords88") {
        /* the list at $7E:FFAE: a few objects (the count word follows it at
           $FFB8, so the list stays short) */
        uint16_t r;
        int cnt = (int)within(1, 5), k;

        fresh();
        W16(0xFFB8) = (uint16_t)(2 * cnt);
        for (k = 0; k < cnt; k++)
            W16(0xFFAE + 2 * k) = obj_of();
        old_run(old_abi_GsuRecordWords88);
        r = object_list_record_words_88();
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordStep6A") {
        uint16_t obj, r;
        bool ran;

        fresh();
        obj = obj_of();
        W8(0x0D62) = (uint8_t)(rnd() % 5 ? 1 + rnd() % 255 : 0);
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordStep6A);
        r = record_step_6a(obj, &ran);
        same();
        reg("A low byte", r, old_cpu.a, 0x00FF);
    }
    CASE("ObjectVelocityFromRecordOf") {
        uint16_t obj, from, r;

        fresh();
        obj = obj_of();
        from = obj_of();
        old_cpu.x = obj;
        old_cpu.y = from;
        old_run(old_abi_ObjectVelocityFromRecordOf);
        r = velocity_from_record_of(obj, from);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordFromObject") {
        uint16_t obj, r;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordFromObject);
        r = record_copy_from_object(obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("ObjectVelocityFromRecordScaled") {
        uint16_t obj, r;

        fresh();
        obj = obj_of();
        OBJ_W(obj, 0x27) = obj_of();        /* the object whose record is read */
        old_cpu.x = obj;
        old_run(old_abi_ObjectVelocityFromRecordScaled);
        r = velocity_from_record_scaled(obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordBits60") {
        uint16_t r;

        fresh();
        P16(0x2476) = rec_addr();
        P16(0x2478) = rec_addr();
        old_run(old_abi_GsuRecordBits60);
        r = record_bits_60_pair();
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordNew422 / 42D / 424 / 424Piece / 424Alt / Camera / 426") {
        static void (*const olds[7])(void) = {old_abi_GsuRecordNew422, old_abi_GsuRecordNew42D,
                                              old_abi_GsuRecordNew424Piece, old_abi_GsuRecordNew424,
                                              old_abi_GsuRecordNew424Alt, old_abi_GsuRecordNewCamera,
                                              old_abi_GsuRecordNew426};
        int w = (int)(rnd() % 7);
        uint16_t obj;
        bool made = false;

        fresh_pools();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(olds[w]);
        switch (w) {
        case 0: made = record_new_422(obj); break;
        case 1: made = record_new_42d(obj); break;
        case 2: made = record_new_424_piece(obj); break;
        case 3: made = record_new_424(obj); break;
        case 4: made = record_new_424_alt(obj); break;
        case 5: made = record_new_camera(obj); break;
        default: made = record_new_426(obj); break;
        }
        same();
        reg("A", made ? 0 : 0x00FF, old_cpu.a, 0x00FF);
    }
    CASE("GsuRecordFlags8ASet") {
        uint16_t rec = rec_addr(), r;
        uint8_t bits = (uint8_t)rnd();

        fresh();
        old_cpu.y = rec;
        old_cpu.a = bits;
        P16(rec + 0x8A) = (uint16_t)rnd();
        old_run(old_abi_GsuRecordFlags8ASet);
        r = record_flags_8a_set(rec, bits);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordFlags8AClear") {
        uint16_t rec = rec_addr(), r;
        uint8_t bits = (uint8_t)rnd();

        fresh();
        old_cpu.y = rec;
        old_cpu.a = bits;
        P16(rec + 0x8A) = (uint16_t)rnd();
        old_run(old_abi_GsuRecordFlags8AClear);
        r = record_flags_8a_clear(rec, bits);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("RecordContactFlags") {
        uint16_t r;

        fresh();
        W16(0x1F3A) = (uint16_t)(rnd() & rnd() & 0x0FFF);
        W16(0x1F36) = (uint16_t)rnd();
        W16(0x1F38) = (uint16_t)rnd();
        old_run(old_abi_RecordContactFlags);
        r = contact_flags_sum();
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordLand") {
        uint16_t obj, rec, r;

        fresh();
        obj = obj_of();
        rec = object_rec(obj);
        P16(0x2482) = rnd() % 4 ? (uint16_t)(1 + rnd() % 0x3FF) : 0;
        if (rnd() % 4)
            P16(rec + 0x2C) |= 0x0040;
        if (rnd() % 4)
            P16(rec + 0x8A) |= 0x0010;
        if (rnd() % 4)
            P16(rec + 0x40) &= (uint16_t)~0x0100;
        old_cpu.x = obj;
        old_run(old_abi_GsuRecordLand);
        r = record_land(obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("RecordNextContact") {
        uint16_t t, obj;
        bool found;
        int n_list = (int)within(0, 5);

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        W16(0x1F3C) = contact_chain(n_list);
        if (n_list == 0 && (rnd() & 1))
            W16(0x1F3C) = 0;                /* an empty list */
        t = old_cpu.y;
        old_run(old_abi_RecordNextContact);
        found = record_next_contact(&t);
        same();
        reg("Y", t, old_cpu.y, 0xFFFF);
        reg("carry", found, (old_cpu.p & P_C) != 0, 1);
    }
    CASE("RecordContactSummary") {
        uint16_t obj, rec, y, r;
        int n_list = (int)within(0, 5);

        fresh();
        obj = obj_of();
        rec = object_rec(obj);
        rec_contacts(rec, n_list);
        if (rnd() % 5 == 0)
            obj_record(obj) = 0;            /* no record: nothing summed */
        old_cpu.x = obj;
        y = old_cpu.y;
        old_run(old_abi_RecordContactSummary);
        r = record_contact_summary(obj, &y);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
        reg("Y", y, old_cpu.y, 0xFFFF);
    }
    CASE("ObjectPoseFromRecord") {
        uint16_t obj, r;

        fresh();
        obj = obj_of();
        object_rec(obj);
        old_cpu.x = obj;
        old_run(old_abi_ObjectPoseFromRecord);
        r = object_pose_from_record(obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordSet2AHigh") {
        uint16_t obj, r;
        uint8_t value = (uint8_t)rnd();

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_cpu.a = value;
        old_run(old_abi_GsuRecordSet2AHigh);
        r = record_set_2a_high(obj, value);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("GsuRecordLower82 / GsuRecordRaise6A") {
        uint16_t rec = rec_addr(), r;
        uint8_t amount = (uint8_t)rnd();
        int raise = (int)(rnd() & 1);

        fresh();
        poll_counts = 1;                    /* the program's poll counts and R10 stack */
        W16(0x0346) = obj_of();
        P16(0x2476) = rnd() & 1 ? rec : rec_addr();
        if (rnd() & 1)
            P16(rec + 0x40) |= 0x0001;
        else
            P16(rec + 0x40) &= (uint16_t)~0x0001;
        old_cpu.y = rec;
        old_cpu.a = amount;
        if (raise) {
            old_run(old_abi_GsuRecordRaise6A);
            r = record_raise_6a(rec, amount);
        } else {
            old_run(old_abi_GsuRecordLower82);
            r = record_lower_82(rec, amount);
        }
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("ObjectTurnFromRecord") {
        uint16_t rec = rec_addr(), obj = obj_of(), r;

        fresh();
        W8(0x1A75) = (uint8_t)rnd();
        W8(0x1A77) = (uint8_t)rnd();
        W8(0x1AB0) = (uint8_t)rnd();
        W8(0x0D62) = rnd() % 2 ? 0 : (uint8_t)(1 + rnd() % 255);
        old_cpu.x = rec;
        old_cpu.y = obj;
        old_run(old_abi_ObjectTurnFromRecord);
        r = object_turn_from_record(rec, obj);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("ObjectSyncFromRecord / ObjectSyncToRecord / ObjectSyncAngles") {
        static uint16_t (*const port_sync[3])(uint16_t, uint16_t *, bool *) = {
            object_sync_from_record, object_sync_to_record, object_sync_angles};
        static void (*const olds[3])(void) = {old_abi_ObjectSyncFromRecord, old_abi_ObjectSyncToRecord,
                                              old_abi_ObjectSyncAngles};
        int w = (int)(rnd() % 3);
        uint16_t obj, y = 0, r;
        bool carry = false;

        fresh_objects((int)(1 + rnd() % 0x80));
        obj = used_with_record();
        W16(0x1F3C) = (uint16_t)rnd();
        old_cpu.x = obj;
        old_cpu.y = (uint16_t)rnd();
        old_run(olds[w]);
        r = port_sync[w](obj, &y, &carry);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
        reg("Y", y, old_cpu.y, 0xFFFF);
        reg("carry", carry, (old_cpu.p & P_C) != 0, 1);
    }
    CASE("ObjectSyncFromRecordFall / ObjectSyncFallStep") {
        int step = (int)(rnd() & 1);
        uint16_t obj, y = 0, r = 0;

        fresh_objects((int)(1 + rnd() % 0x80));
        obj = used_with_record();
        if (!obj_record(obj))
            obj_record(obj) = rec_addr();
        old_cpu.x = obj;
        old_cpu.y = (uint16_t)rnd();
        if (step) {
            old_run(old_abi_ObjectSyncFallStep);
            object_sync_fall_step(obj);
        } else {
            old_run(old_abi_ObjectSyncFromRecordFall);
            r = object_sync_from_record_fall(obj, &y);
            reg("A", r, old_cpu.a, 0xFFFF);
            reg("Y", y, old_cpu.y, 0xFFFF);
        }
        same();
    }
    CASE("ObjectRecordMarkStep") {
        uint16_t obj;

        fresh_objects((int)(1 + rnd() % 0x80));
        obj = used_with_record();
        old_cpu.x = obj;
        old_run(old_abi_ObjectRecordMarkStep);
        object_record_mark_step(obj);
        same();
    }
    CASE("ObjectSpinStep") {
        uint16_t obj;

        fresh_objects((int)(1 + rnd() % 0x80));
        obj = used_with_record();
        old_cpu.x = obj;
        old_run(old_abi_ObjectSpinStep);
        object_spin_step(obj);
        same();
    }
    CASE("ObjectSpinRecord / ObjectSpinSetup") {
        int setup = (int)(rnd() & 1);
        uint16_t obj;

        fresh_objects((int)(1 + rnd() % 0x80));
        pools_on_objects();
        obj = used_with_record();
        old_cpu.x = obj;
        if (setup) {
            old_run(old_abi_ObjectSpinSetup);
            object_spin_setup(obj);
        } else {
            old_run(old_abi_ObjectSpinRecord);
            object_spin_record(obj);
        }
        same();
    }
    CASE("RecordButtonSequence") {
        uint16_t rec = rec_addr(), r;

        fresh();
        poll_counts = 1;
        W16(0x1A33) = (uint16_t)(rnd() % 20);
        W16(0x1A35) = (uint16_t)(rnd() % 5);
        W16(0x1AF0) = (uint16_t)((rnd() & rnd()) | (rnd() & 0x0040));
        P16(rec + 0x40) = (uint16_t)rnd();
        old_cpu.x = rec;
        old_run(old_abi_RecordButtonSequence);
        r = record_button_sequence(rec);
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    CASE("CarControlsToRecord") {
        uint16_t obj;

        fresh_objects((int)(1 + rnd() % 0x80));
        pools_on_objects();
        obj = used_with_record();
        OBJ_W(obj, 0x4F) = some_used();
        *OBJ(obj, 0x25) = (uint8_t)(rnd() % 4 == 0 ? ((uint8_t)rnd() | 0x04) : ((uint8_t)rnd() & 0xFB));
        OBJ_W(obj, 0x29) = rnd() % 3 == 0 ? (uint16_t)rnd() : (uint16_t)(rnd() % 14);
        if (obj_record(obj)) {
            if (rnd() % 4 == 0)
                P16(obj_record(obj) + 0x1C) |= 0x0010;
            P16(obj_record(obj) + 0x6A) = rnd() % 2 ? 0 : (uint16_t)rnd();
            P16(obj_record(obj) + 0x5E) = rnd() % 2 ? 0 : (uint16_t)rnd();
        }
        W16(0x1AF0) = (uint16_t)rnd();
        W16(0x1AF2) = (uint16_t)rnd();
        W8(0x0D62) = rnd() % 2 ? 0 : (uint8_t)(1 + rnd() % 255);
        W8(0x0D53) = rnd() % 4 ? 0 : (uint8_t)(1 + rnd() % 255);
        W8(0x1A5D) = (uint8_t)(rnd() & 1);
        old_cpu.x = obj;
        old_run(old_abi_CarControlsToRecord);
        car_controls_to_record(obj);
        same();
    }
    CASE("PlayerControlsFrame") {
        uint16_t r;

        fresh_objects((int)(2 + rnd() % 0x80));
        pools_on_objects();
        W16(0x0346) = used_with_record();
        W16(0x034A) = used_with_record();
        OBJ_W(W16(0x0346), 0x4F) = some_used();
        OBJ_W(W16(0x034A), 0x4F) = some_used();
        W16(0x0309) = (uint16_t)rnd();
        W16(0x030D) = (uint16_t)rnd();
        W16(0x1F23) = (uint16_t)rnd();
        W16(0x1F25) = (uint16_t)rnd();
        W8(0x0D62) = rnd() % 2 ? 0 : (uint8_t)(1 + rnd() % 255);
        W8(0x0D53) = rnd() % 4 ? 0 : (uint8_t)(1 + rnd() % 255);
        linked_state(W16(0x0346));
        linked_state(W16(0x034A));
        old_run(old_abi_PlayerControlsFrame);
        r = player_controls_frame();
        same();
        reg("A", r, old_cpu.a, 0xFFFF);
    }
    printf("gsu_record_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
