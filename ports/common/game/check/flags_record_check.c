/* old: object/flags_record.c object/gsu_record.c object/fx_pool.c object/fx_record.c gsu/run.c math/scale.c
 *
 * flags_record_check.c: the port's flag-record objects (object/flags_record.c)
 * against the decomp's, on random states. Each case runs the decomp's
 * routine on a copy and the port's on the state, and compares both RAMs
 * (the direct page aside). The routines that give an object its record
 * (GsuRecordNew422) run the decomp's Super FX program through the bus model
 * (model_gsu_start), as the game's own calls do; the pools they take from
 * are built whole first. The frame steps get their counters set so that
 * both the step and the wrap-round paths run.
 *
 *   check/check.sh flags_record N
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
#include "objects.h"
#include "gsu_record.h"
#include "flags_record.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(ObjectFlagsRecordFrame) OLD(ObjectFlagsRecordSetup) OLD(ObjectFlagsRecord) OLD(ObjectRecordFlag4Step)
OLD(ObjectFrameSwingSetup) OLD(ObjectFrameSwingStep)
OLD(FxRecordNew426) OLD(FxRecordNew422) OLD(FxRecordNew424) OLD(FxRecordNewCamera) OLD(FxRecordNew42D)

static uint64_t rng = 0x2545F4914F6CDD1Dull;

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
static int poll_counts;     /* the case starts Super FX programs: their poll counts ($FE-$FF) and R10 stack are the decomp's own */

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

/* The decomp's Super FX programs the record routines start, by where they
   start (bank $04). */
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
        OBJ_W(o, 0x06) = obj_of();
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
    int k;

    P16(0x2332) = n ? 0x3000 : 0;
    for (k = 0; k < n; k++) {
        uint16_t r = (uint16_t)(0x3000 + 0x90 * k);

        P16(r + 4) = k + 1 < n ? (uint16_t)(r + 0x90) : 0;
        P16(r + 6) = k ? (uint16_t)(r - 0x90) : 0;
        P16(r + 8) = (rnd() & 1) ? 0x1098 : 0x1D70;
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

/* The frame counters of an object for a frame step: its frame count +$48
   (1 to $40), the count of ticks +$49 (1 to 8), and the tick counter
   (+$4A, or +$4B for a swing) either one short of the ticks, so the step
   runs, or random, so it does not. */
static void frame_counters(uint16_t obj, uint16_t tick_counter)
{
    OBJ(obj, 0x48)[0] = (uint8_t)within(1, 0x40);
    OBJ(obj, 0x49)[0] = (uint8_t)within(1, 8);
    OBJ(obj, tick_counter)[0] = rnd() & 1 ? (uint8_t)(OBJ(obj, 0x49)[0] - 1) : (uint8_t)rnd();
    OBJ(obj, 0x5A)[0] = (uint8_t)rnd();
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: flags_record_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_gsu_start = old_start;

    CASE("ObjectFrameSwingStep") {
        uint16_t obj;

        fresh();
        obj = obj_of();
        frame_counters(obj, 0x4B);
        OBJ(obj, 0x2E)[0] = rnd() % 3 == 0 ? 0 : (uint8_t)within(1, 3);
        OBJ(obj, 0x25)[0] = (uint8_t)rnd();
        OBJ(obj, 0x30)[0] = (uint8_t)rnd();
        old_cpu.x = obj;
        old_run(old_abi_ObjectFrameSwingStep);
        object_frame_swing_step(obj);
        same();
    }
    CASE("ObjectFrameSwingSetup") {
        uint16_t obj;

        fresh_pools();
        obj = obj_of();
        OBJ(obj, 0x4A)[0] = (uint8_t)rnd();
        old_cpu.x = obj;
        old_run(old_abi_ObjectFrameSwingSetup);
        object_frame_swing_setup(obj);
        same();
    }
    CASE("ObjectFlagsRecord / ObjectFlagsRecordSetup / ObjectFlagsRecordFrame") {
        int k = (int)(rnd() % 3);
        uint16_t obj;

        fresh_pools();
        obj = obj_of();
        OBJ_W(obj, 0x06) = (uint16_t)rnd();
        OBJ_W(obj, 0x27) = (uint16_t)rnd();
        OBJ(obj, 0x48)[0] = (uint8_t)rnd();
        old_cpu.x = obj;
        old_run(k == 0 ? old_abi_ObjectFlagsRecord : k == 1 ? old_abi_ObjectFlagsRecordSetup
                : old_abi_ObjectFlagsRecordFrame);
        switch (k) {
        case 0: object_flags_record(obj); break;
        case 1: object_flags_record_setup(obj); break;
        default: object_flags_record_frame(obj);
        }
        same();
    }
    CASE("ObjectRecordFlag4Step") {
        uint16_t obj;

        fresh();
        obj = obj_of();
        old_cpu.x = obj;
        old_run(old_abi_ObjectRecordFlag4Step);
        object_record_flag4_step(obj);
        same();
    }
    printf("flags_record_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
