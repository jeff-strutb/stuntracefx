/* old: object/fx_sums.c object/fx_motion.c object/fx_pool.c object/fx_collide.c object/fx_record.c math/fx_math.c math/fx_rotate.c
 *
 * physics_check.c: the port's Super FX physics (object/fx_*.c) against
 * the decomp's routines on random states. Each runs on the same game pak
 * RAM and registers; the RAM is compared, the Super FX's stack aside, and
 * the registers a caller uses.
 *
 *   check/check.sh physics N
 */
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define gsu old_gsu
#define gsu_rd8 old_gsu_rd8
#define gsu_wr8 old_gsu_wr8
#define gsu_plot old_gsu_plot
#define gsu_rpix old_gsu_rpix
#define gsu_code old_gsu_code
#define gsu_cache old_gsu_cache
#define gsu_cyc old_gsu_cyc
#define gsu_time old_gsu_time
#define gsu_buffer old_gsu_buffer
#include "gsu.h"
#undef gsu

#include "game.h"
#include "render.h"
#include "physics.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(FxSumsClear) OLD(FxSumsRotateLocal) OLD(FxSumsApply) OLD(FxSumsPushWorld) OLD(FxSumsPushLocal) OLD(FxPushApply)
OLD(FxPushesApply) OLD(FxPushAt4) OLD(FxPushAt2) OLD(FxPushAt1) OLD(FxPushAtHalf) OLD(FxPushAtQuarter) OLD(FxPushAt4Twist)
OLD(FxHitAddScaled) OLD(FxHitAddUnit) OLD(FxSumScale) OLD(FxRecordMove)
OLD(FxPoolTake) OLD(FxPoolsMerge) OLD(FxListSweep) OLD(FxListClearBit0) OLD(FxObjectFree) OLD(FxRecordNew426) OLD(FxRecordNew424)
OLD(FxRecordNew426B) OLD(FxRecordNew42D) OLD(FxRecordNewCamera) OLD(FxRecordNew422) OLD(FxCollidePush) OLD(FxRecordsApart)
OLD(FxContactNote) OLD(FxRecordTouches) OLD(FxAllTouches)
OLD(FxPushSet) OLD(FxRecordHoldY) OLD(FxBearingTo) OLD(FxRecordAlignment) OLD(FxRecordRaise6A) OLD(FxRecordLower82)
OLD(FxTimersProgram)

static uint64_t rng = 0x5D2B7C94618E1F3Aull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static int16_t within(int lo, int hi)
{
    return (int16_t)(lo + (int)(rnd() % (unsigned)(hi - lo + 1)));
}

static uint8_t rom_copy[0x100000];
static uint8_t start_p[sizeof pakram], old_p[sizeof pakram];
static long cases, failed, iteration;
static volatile const char *side = "";

static void runaway(int sig)
{
    (void)sig;
    fprintf(stderr, "physics_check: %s, case %ld: %s ran away\n", model_current, iteration, side);
    _exit(3);
}

/* random RAM and registers, the game's screen, the view's edges */
static void fresh(void)
{
    static const uint8_t bases[2] = {0x0B, 0x25};
    static const uint8_t zero[8];
    size_t i;

    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    memset(&old_gsu, 0, sizeof old_gsu);
    for (i = 0; i < 16; i++)
        old_gsu.r[i] = (uint16_t)rnd();
    old_gsu.r[10] = 0x0240;
    old_gsu.r[11] = 0xFFF0;
    old_gsu.scmr = (rnd() & 1) ? 1 : 3;
    old_gsu.scbr = bases[rnd() & 1];
    old_gsu.colr = (uint8_t)rnd();
    old_gsu.por = (uint8_t)(rnd() & 0x0F);
    fx_screen.mode = old_gsu.scmr & 3;
    fx_screen.height = 0;
    fx_screen.base = old_gsu.scbr;
    fx_colour = old_gsu.colr;
    fx_plot_options = old_gsu.por;
    fx_pixels_set(0, 0xFFFF, 0, zero);
    fx_pixels_set(1, 0xFFFF, 0, zero);
    FXU(0x001C) = (uint16_t)(rnd() % 0x20);
    if (rnd() & 3) {
        fx_screen_centre_x = 108, fx_screen_centre_y = 64;
        fx_edges[0] = 0, fx_edges[1] = 216, fx_edges[2] = 0, fx_edges[3] = 128;
    } else {
        fx_screen_centre_x = within(40, 160), fx_screen_centre_y = within(30, 100);
        fx_edges[0] = within(0, 50), fx_edges[1] = within(150, 216);
        fx_edges[2] = within(0, 50), fx_edges[3] = within(80, 128);
    }
}

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 25)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld: ", model_current, iteration);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* the decomp's routine, its pixels written out; then the RAM put back for
   the port's */
static void old_run(void (*f)(void))
{
    memcpy(start_p, pakram, sizeof pakram);
    model_pixels_empty();
    side = "the decomp's routine";
    alarm(5);
    f();
    model_pixels_flush();
    side = "the port's";
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same_ram(void)
{
    size_t i;

    fx_pixels_flush();
    alarm(0);
    cases++;
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0)) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            if (getenv("ALLDIFF"))
                for (i++; i < sizeof pakram; i++)
                    if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0))
                        fprintf(stderr, "  $%04zX = %02X, the decomp's %02X (was %02X)\n", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
}

static void same_reg(const char *what, uint16_t mine, uint16_t theirs)
{
    if (mine != theirs)
        fail("%s %04X, the decomp's %04X", what, mine, theirs);
}

/* a random world and own-axes sum, matrix, push slots, turn */
static void sums(void)
{
    int k;

    for (k = 0; k < 6; k++)
        FXW(0x2340 + 2 * k) = (rnd() & 3) ? within(-0x3000, 0x3000) : (int16_t)rnd();
    for (k = 0; k < 9; k++)
        FXW(0x00E4 + 2 * k) = (int16_t)rnd();
    if (rnd() & 1)
        for (k = 0; k < 6; k++)
            FXU(0x2340 + 2 * k) = 0;
    for (k = 0; k < 20; k++) {
        uint16_t p = (uint16_t)(0x23A4 + 10 * k);

        FXU(p) = (rnd() & 3) == 0 ? 0 : (rnd() & 7) == 0 ? (uint16_t)rnd() : (uint16_t)(rnd() & 0xFF);
        FXW(p + 2) = (rnd() & 7) == 0 ? 0x7D00 : (rnd() & 3) == 0 ? (int16_t)rnd() : within(0, 0x4000);
    }
    {
        static const uint16_t turns[6] = {0, 0x40, 0x80, 0xC0, 0x20, 0xE7};

        FXU(0x247E) = (rnd() & 7) ? turns[rnd() % 6] : (uint16_t)(rnd() & 0xFF);
    }
}

/* a pool's chain from `head`: blocks of random sizes one after another up
   to `end`, each free, in use (its size negated) or cut off the chain */
static void pool(uint16_t head, uint16_t end)
{
    uint16_t b = head, last = head;

    FXU(head) = 0;
    while (b + 8 < end) {
        uint16_t size = (uint16_t)(8 + 2 * (rnd() % 24)), kind = rnd() % 4;

        if (b + size > end)
            size = (uint16_t)(end - b);
        if (kind == 3 && b != head) {               /* cut from the one before: off the chain */
            FXU(b + 2) = size;
        } else {
            FXU(b + 2) = kind == 2 ? (uint16_t)-size : size;
            if (b != head)
                FXU(last) = b;
            FXU(b) = 0;
            last = b;
        }
        b = (uint16_t)(b + size);
    }
}

/* n records at $3000 on, $90 bytes apart, on the list of all records,
   near each other, with random flags, sizes and contact lists */
static uint16_t recs[12];
static int nrecs;

static void records(int n)
{
    int k, j;

    nrecs = n;
    FXU(0x2332) = n ? 0x3000 : 0;
    for (k = 0; k < n; k++) {
        uint16_t r = recs[k] = (uint16_t)(0x3000 + 0x90 * k);

        FXU(r + 4) = k + 1 < n ? (uint16_t)(r + 0x90) : 0;
        FXU(r + 6) = k ? (uint16_t)(r - 0x90) : 0;
        FXU(r + 8) = (rnd() & 1) ? 0x1098 : 0x1D70;
        FXW(r + 2) = (rnd() & 1) ? -0x34 : 0x34;
        for (j = 0; j < 3; j++) {
            FXW(r + 0x1E + 2 * j) = within(-0x300, 0x300);
            FXW(r + 0x0C + 2 * j) = within(0x40, 0x200);
            FXW(r + 0x2E + 2 * j) = within(-0x800, 0x800);
        }
        FXU(r + 0x16) = (uint16_t)(rnd() & 0x00FF);
        FXU(r + 0x1A) = (uint16_t)(rnd() & rnd() & 0x00FF);
        FXU(r + 0x2A) = (uint16_t)within(1, 0x200);
        FXU(r + 0x18) = 0;
        FXU(r + 0x3E) = 0;
    }
    /* contacts: blocks cut from the first pool's region */
    for (k = 0; k < n; k++) {
        int m = (int)(rnd() % 3);

        for (j = 0; j < m; j++) {
            uint16_t c = (uint16_t)(0x2000 + 0x40 * (k * 3 + j));

            FXU(c + 2) = (rnd() & 1) ? 0x12 : (uint16_t)-0x12;
            FXU(c + 0x0C) = recs[rnd() % n];
            FXU(c + 0x10) = (uint16_t)(rnd() & 7);
            FXU(c + 0x0E) = FXU(recs[k] + 0x18);
            FXU(recs[k] + 0x18) = c;
        }
    }
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 20000, i;
    int k;

    if (argc < 2) {
        fprintf(stderr, "usage: physics_check ROM [N]\n");
        return 2;
    }
    signal(SIGALRM, runaway);
    model_load_rom(argv[1]);
    memcpy(rom_copy, srfx_rom_image, sizeof rom_copy);
    srfx_rom_image = rom_copy;

    CASE("FxSumsClear") {
        fresh();
        old_run(old_abi_FxSumsClear);
        fx_sums_clear();
        same_ram();
    }
    CASE("FxSumsRotateLocal") {
        fresh();
        sums();
        old_run(old_abi_FxSumsRotateLocal);
        fx_sums_rotate_local();
        same_ram();
    }
    CASE("FxSumsApply") {
        uint16_t rec = (uint16_t)(0x3000 + (rnd() % 0x800) * 2);

        fresh();
        sums();
        old_gsu.r[9] = rec;
        old_run(old_abi_FxSumsApply);
        fx_sums_apply(rec);
        same_ram();
    }
    CASE("FxSumsPushWorld / FxSumsPushLocal") {
        int16_t v[3];
        int local = rnd() & 1;

        fresh();
        sums();
        for (k = 0; k < 3; k++)
            v[k] = (rnd() & 1) ? within(-6000, 6000) : (int16_t)rnd();
        if (!(rnd() & 7))
            v[rnd() % 3] = (rnd() & 1) ? 5000 : -5000;
        for (k = 0; k < 3; k++)
            old_gsu.r[4 + k] = (uint16_t)v[k];
        old_run(local ? old_abi_FxSumsPushLocal : old_abi_FxSumsPushWorld);
        (local ? fx_sums_push_local : fx_sums_push_world)(v);
        same_ram();
        for (k = 0; k < 3; k++)
            same_reg("R4-R6", (uint16_t)v[k], old_gsu.r[4 + k]);
    }
    CASE("FxPushApply") {
        uint16_t at = (uint16_t)(0x23A4 + 10 * (rnd() % 20)), r0;

        fresh();
        sums();
        for (k = 0; k < 3; k++)
            FXW(at + 4 + 2 * k) = (int16_t)rnd();
        old_gsu.r[3] = at;
        old_run(old_abi_FxPushApply);
        r0 = fx_push_apply(&at);
        same_ram();
        same_reg("R0", r0, old_gsu.r[0]);
        same_reg("R3", at, old_gsu.r[3]);
    }
    CASE("FxPushesApply") {
        uint16_t r0;

        fresh();
        sums();
        old_run(old_abi_FxPushesApply);
        r0 = fx_pushes_apply();
        same_ram();
        same_reg("R0", r0, old_gsu.r[0]);
    }
    CASE("FxPushAt*") {
        static void (*const olds[6])(void) = {old_abi_FxPushAt4, old_abi_FxPushAt2, old_abi_FxPushAt1, old_abi_FxPushAtHalf,
                                              old_abi_FxPushAtQuarter, old_abi_FxPushAt4Twist};
        int16_t p[3];
        int w = (int)(rnd() % 6);

        fresh();
        sums();
        for (k = 0; k < 3; k++)
            old_gsu.r[1 + k] = (uint16_t)(p[k] = (rnd() & 1) ? within(-0x400, 0x400) : (int16_t)rnd());
        old_run(olds[w]);
        fx_push_at((uint16_t)(2 * w), p);
        same_ram();
    }
    CASE("FxHitAddScaled / FxHitAddUnit") {
        int16_t v[3], s = (int16_t)rnd();
        int unit = rnd() & 1;

        fresh();
        sums();
        for (k = 0; k < 3; k++)
            v[k] = (rnd() & 3) ? (int16_t)rnd() : 0;
        for (k = 0; k < 3; k++)
            old_gsu.r[1 + k] = (uint16_t)v[k];
        old_gsu.r[6] = (uint16_t)s;
        old_run(unit ? old_abi_FxHitAddUnit : old_abi_FxHitAddScaled);
        if (unit)
            fx_hit_add_unit(v);
        else
            fx_hit_add_scaled(v, s);
        same_ram();
    }
    CASE("FxSumScale") {
        int16_t s = (int16_t)rnd();

        fresh();
        sums();
        old_gsu.r[6] = (uint16_t)s;
        old_run(old_abi_FxSumScale);
        fx_sum_scale(s);
        same_ram();
    }
    CASE("FxRecordMove") {
        uint16_t rec = (uint16_t)(0x3000 + (rnd() % 0x800) * 2), kind = (rnd() & 1) ? 6 : (uint16_t)(rnd() % 8);

        fresh();
        old_gsu.r[8] = rec;
        old_gsu.r[1] = kind;
        old_run(old_abi_FxRecordMove);
        fx_record_move(rec, kind);
        same_ram();
    }

    CASE("FxPoolTake") {
        uint16_t head = (rnd() & 1) ? 0x1098 : 0x1D70, size = (uint16_t)(4 + 2 * (rnd() % 40)), r;

        fresh();
        pool(0x1098, 0x1098 + 0x300);
        pool(0x1D70, 0x1D70 + 0x200);
        records((int)(rnd() % 4));
        old_gsu.r[3] = head;
        old_gsu.r[4] = size;
        old_run(old_abi_FxPoolTake);
        r = fx_pool_take(head, size);
        same_ram();
        same_reg("R1", r, old_gsu.r[1]);
    }
    CASE("FxPoolsMerge") {
        fresh();
        pool(0x1098, 0x1098 + 0x300);
        pool(0x1D70, 0x1D70 + 0x200);
        old_run(old_abi_FxPoolsMerge);
        fx_pools_merge();
        same_ram();
    }
    CASE("FxListClearBit0 / FxListSweep") {
        int sweep = rnd() & 1;

        fresh();
        pool(0x1098, 0x1098 + 0x300);
        records(1 + (int)(rnd() % 6));
        k = (int)(rnd() % nrecs);
        old_gsu.r[6] = recs[k];
        old_run(sweep ? old_abi_FxListSweep : old_abi_FxListClearBit0);
        (sweep ? fx_list_sweep : fx_list_clear_bit0)(recs[k]);
        same_ram();
    }
    CASE("FxObjectFree") {
        uint16_t r;

        fresh();
        pool(0x1098, 0x1098 + 0x300);
        pool(0x1D70, 0x1D70 + 0x200);
        records(1 + (int)(rnd() % 8));
        r = recs[rnd() % nrecs];
        if (rnd() & 1) {                    /* children, linked through +$34, some with their own */
            for (k = 0; k < nrecs; k++)
                if (recs[k] != r && (rnd() & 1)) {
                    FXU(recs[k] + 0x34) = FXU(r + 0x3E);
                    FXU(r + 0x3E) = recs[k];
                    FXU(recs[k] + 0x16) &= (uint16_t)~2;
                }
            FXU(r + 0x16) |= 2;
        } else {
            FXU(r + 0x16) &= (uint16_t)~2;
        }
        old_gsu.r[1] = r;
        old_run(old_abi_FxObjectFree);
        fx_object_free(r);
        same_ram();
    }
    CASE("FxRecordNew*") {
        static void (*const olds[6])(void) = {old_abi_FxRecordNew426, old_abi_FxRecordNew424, old_abi_FxRecordNew426B,
                                              old_abi_FxRecordNew42D, old_abi_FxRecordNewCamera, old_abi_FxRecordNew422};
        static void (*const ports[6])(void) = {fx_record_new_426, fx_record_new_424, fx_record_new_426b, fx_record_new_42d,
                                               fx_record_new_camera, fx_record_new_422};
        int w = (int)(rnd() % 6);

        fresh();
        pool(0x1098, 0x1098 + 0x300);
        pool(0x1D70, 0x1D70 + 0x200);
        records((int)(rnd() % 4));
        FXU(0x2398) = (rnd() & 1) ? 0 : (uint16_t)(0x3800 + 2 * (rnd() % 0x400));   /* records are at even addresses */
        old_run(olds[w]);
        ports[w]();
        same_ram();
    }
    CASE("FxCollidePush") {
        fresh();
        records(3);
        old_gsu.r[1] = 0x2000;
        for (k = 0; k < 4; k++)
            FXW(0x2004 + 2 * k) = within(-0x400, 0x400);
        old_gsu.r[7] = recs[0];
        old_gsu.r[9] = recs[1];
        old_run(old_abi_FxCollidePush);
        fx_collide_push(0x2000, recs[0], recs[1]);
        same_ram();
    }
    CASE("FxRecordsApart") {
        int a;

        fresh();
        records(3);
        if (rnd() & 1) {                    /* still, along x, near where they meet */
            int16_t reach = wrap16(FXW(recs[0] + 0x0C) + FXW(recs[1] + 0x0C) - 0x0294);

            for (k = 0; k < 3; k++)
                FXW(recs[0] + 0x2E + 2 * k) = FXW(recs[1] + 0x2E + 2 * k) = 0;
            FXW(recs[0] + 0x22) = FXW(recs[1] + 0x22);
            FXW(recs[0] + 0x1E) = wrap16(FXW(recs[1] + 0x1E) + (reach < 0 ? 0 : reach) / 2 + within(-6, 6));
        }
        old_gsu.r[7] = recs[0];
        old_gsu.r[8] = recs[1];
        old_run(old_abi_FxRecordsApart);
        a = fx_records_apart(recs[0], recs[1]);
        same_ram();
        same_reg("R0", (uint16_t)a, old_gsu.r[0]);
    }
    CASE("FxContactNote") {
        fresh();
        pool(0x1098, 0x1098 + ((rnd() & 3) ? 0x300 : 0x20));
        records(4);
        {
            uint16_t owner = recs[rnd() % 4], other = recs[rnd() % 4];

            old_gsu.r[6] = owner;
            old_gsu.r[8] = other;
            old_run(old_abi_FxContactNote);
            fx_contact_note(owner, other);
        }
        same_ram();
    }
    CASE("FxRecordTouches / FxAllTouches") {
        int all = rnd() & 1;
        uint16_t r;

        fresh();
        pool(0x1098, 0x1098 + 0x300);
        records(2 + (int)(rnd() % 6));
        r = recs[rnd() % nrecs];
        old_gsu.r[1] = r;
        old_run(all ? old_abi_FxAllTouches : old_abi_FxRecordTouches);
        if (all)
            fx_all_touches();
        else
            fx_record_touches(r);
        same_ram();
    }

    CASE("FxPushSet") {
        int16_t a = (int16_t)rnd();
        uint16_t on;

        fresh();
        old_gsu.r[2] = (uint16_t)a;
        old_run(old_abi_FxPushSet);
        on = fx_push_set(a);
        same_ram();
        same_reg("R0", on, old_gsu.r[0]);
    }
    CASE("FxRecordHoldY / FxRecordAlignment") {
        uint16_t r = (uint16_t)(0x3000 + 2 * (rnd() % 0x400));
        int which = rnd() & 1;

        fresh();
        for (k = 0; k < 9; k++)
            FXW(0x00E4 + 2 * k) = (int16_t)rnd();
        if (rnd() & 1)
            FXW(r + 0x4A) = within(-12, 12);
        if (!(rnd() & 7))
            for (k = 0; k < 3; k++)
                FXU(r + 0x2E + 2 * k) = 0;
        old_gsu.r[8] = r;
        old_run(which ? old_abi_FxRecordAlignment : old_abi_FxRecordHoldY);
        (which ? fx_record_alignment : fx_record_hold_y)(r);
        same_ram();
    }
    CASE("FxBearingTo") {
        uint16_t r = (uint16_t)(0x3000 + 2 * (rnd() % 0x400)), a;
        int16_t x = (int16_t)rnd(), z = (int16_t)rnd();

        fresh();
        if (rnd() & 1)
            z = FXW(r + 0x22);
        old_gsu.r[8] = r, old_gsu.r[1] = (uint16_t)x, old_gsu.r[3] = (uint16_t)z;
        old_run(old_abi_FxBearingTo);
        a = fx_bearing_to(r, x, z);
        same_ram();
        same_reg("R0", a, old_gsu.r[0]);
    }
    CASE("FxRecordRaise6A / FxRecordLower82 / FxTimersProgram") {
        int w = (int)(rnd() % 3);

        fresh();
        FXU(0x238E) = (uint16_t)(0x3000 + 2 * (rnd() % 0x400));
        old_run(w == 0 ? old_abi_FxRecordRaise6A : w == 1 ? old_abi_FxRecordLower82 : old_abi_FxTimersProgram);
        (w == 0 ? fx_record_raise_6a : w == 1 ? fx_record_lower_82 : fx_timers_program)();
        same_ram();
    }

    printf("physics_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
