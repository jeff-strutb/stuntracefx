/* old: object/object.c math/rotate.c math/scale.c math/random.c object/gsu_record.c
 *
 * object_more_check.c: the port's object routines (object/object_more.c)
 * against the decomp's (object/object.c and the maths it calls), on random
 * states. Each case shapes the state the routine expects (a used object
 * list in whole buckets, the object's record and links valid, the tables the
 * original reads from its work-RAM code copy ($7E:A2D9-F77F) put back from
 * the cartridge), runs the decomp's routine on a copy and the port's on the
 * state, and compares both RAMs but the direct page.
 *
 *   check/check.sh object_more N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#include "machine.h"
#include "cpu_ops.h"
#undef cpu

#include "game.h"
#include "objects.h"
#include "object_more.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(ObjectLinkFlag4000Step) OLD(ObjectFlag40IfMatch) OLD(ObjectTableE8AF) OLD(ObjectTableC056)
OLD(ObjectVelocityFromRecord) OLD(ObjectSet5DByKind) OLD(ObjectPlaceFromTable) OLD(ObjectPlaceAll)
OLD(ObjectLinkEventTrigger) OLD(ObjectLinkEventTrigger2) OLD(ObjectLinkSpeedSet) OLD(ObjectLinkBobStart)
OLD(ObjectCountStep5F) OLD(ObjectNearFocus) OLD(ObjectRebucket) OLD(ObjectLinkedHandlerByState)
OLD(ObjectAttachLinked) OLD(ObjectGroupsActivate) OLD(BucketTriggersFromView) OLD(BucketTriggersFixedArea)
OLD(ObjectStep48To8) OLD(ObjectSpawnChanceD460) OLD(ObjectAdvance5DIfFlag) OLD(ObjectReleaseLong)
OLD(ObjectSpawnSparklePair) OLD(ObjectSparkleStep) OLD(ObjectSparkleStart) OLD(ObjectSpawnD460UnlessFlag)
OLD(ObjectMoveForward) OLD(ObjectSpawnTrail) OLD(ObjectTrailStart) OLD(ObjectTrailStep)
OLD(ObjectSpawnAttached) OLD(ObjectAttachedStart) OLD(ObjectAttachedStep) OLD(ObjectPlaceOnParent)
OLD(ObjectBillboardTilt) OLD(ObjectSpawnPuff) OLD(ObjectTimedSetup) OLD(ObjectTimedStep)
OLD(ObjectCarPoseStep)

static uint64_t rng = 0x2545F4914F6CDD1Dull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration;
static uint16_t used[0x136];
static int nused;

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

static uint16_t record(int i)
{
    return (uint16_t)(1 + 0x62 * i);
}

/* A random used object (the list's records are the only ones with a valid
   bucket and links). */
static uint16_t some_used(void)
{
    return nused ? used[rnd() % (unsigned)nused] : record(0);
}

/* Random RAM with the object table whole: `n` objects used (in a random
   order), the rest free, each used one in a bucket, with a valid record
   (+$22), parent (+$27, +$06) and link (+$4F). The code copy is put back. */
static void fresh(int n)
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
        used[i] = record(order[i]);
        obj_next(used[i]) = i + 1 < n ? record(order[i + 1]) : 0;
        obj_prev(used[i]) = i ? record(order[i - 1]) : 0;
    }
    objects_used = n ? used[0] : 0;
    objects_last = n ? used[n - 1 - (int)(rnd() % (unsigned)n)] : 0;
    for (i = n; i < 0x136; i++)
        obj_next(record(order[i])) = i + 1 < 0x136 ? record(order[i + 1]) : 0;
    objects_free = n < 0x136 ? record(order[n]) : 0;
    for (k = 0x10000; k < 0x10800; k++)           /* the bucket heads, all empty */
        wram[k] = 0;
    for (i = 0; i < n; i++) {
        OBJ_W(used[i], 0x22) = (uint16_t)(0x0400 * (1 + rnd() % 0x0D));
        OBJ_W(used[i], 0x40) = (uint16_t)(rnd() % 0x40 * 2);
    }
    for (i = 0; i < n; i++) {
        OBJ_W(used[i], 0x27) = some_used();
        OBJ_W(used[i], 0x06) = some_used();
        OBJ_W(used[i], 0x4F) = some_used();
        object_bucket_insert(used[i]);
    }
    W8(0x0CEE) = (uint8_t)(rnd() % 0x50);
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.db = 0x7E;
}

/* The registers for a routine: A, X, Y, the widths (M 8-bit when m8, X
   16-bit), direct page 0, the data bank $7E. */
static void regs(uint16_t a, uint16_t x, uint16_t y, int m8)
{
    old_cpu.a = a;
    old_cpu.x = x;
    old_cpu.y = y;
    old_cpu.d = 0;
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)((rnd() & (P_N | P_V | P_Z | P_C)) | (m8 ? P_M : 0));
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same(void)
{
    size_t i;

    cases++;
    for (i = 0x100; i < sizeof wram; i++)
        if (wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X", i, pakram[i], old_p[i]);
            return;
        }
}

/* A rounded-down record address (below $2000) for the links bank 0 reads. */
static uint16_t low_object(void)
{
    return record((int)(rnd() % 0x20));
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: object_more_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("ObjectLinkFlag4000Step") {
        uint16_t r;
        int k;

        fresh(1 + (int)(rnd() % 0x80));
        /* the links among the low records only: the chain's reads are bank 0 */
        for (k = 0; k < 0x20; k++)
            OBJ_W(record(k), 0x4F) = low_object();
        W16(0x0346) = low_object();
        OBJ_W(W16(0x0346), 0x4F) = low_object();
        W16(0x06CD) = (uint16_t)rnd();
        if (rnd() & 1)
            W16(OBJ_W(OBJ_W(W16(0x0346), 0x4F), 0x4F)) |= 0x4000;
        else
            W16(OBJ_W(OBJ_W(W16(0x0346), 0x4F), 0x4F)) &= 0xBFFF;
        regs(0, 0, 0, 0);
        old_run(old_abi_ObjectLinkFlag4000Step);
        r = object_link_flag_4000_step();
        if (r != old_cpu.a)
            fail("A %04X, the decomp's %04X", r, (uint16_t)old_cpu.a);
        same();
    }
    CASE("ObjectFlag40IfMatch") {
        int hit = (int)(rnd() & 1);
        uint16_t a;
        bool m;

        fresh(1 + (int)(rnd() % 0x80));
        W16(0x0346) = some_used();
        P16(0x2476) = (uint16_t)rnd();
        a = hit ? P16(0x2476) : (uint16_t)rnd();
        regs(a, 0, 0, 0);
        old_run(old_abi_ObjectFlag40IfMatch);
        m = object_flag40_if_match(a);
        if (m != (hit || a == P16(0x2476)))
            fail("match %d", (int)m);
        same();
    }
    CASE("ObjectTableE8AF") {
        uint16_t obj;
        uint8_t a = (uint8_t)rnd(), v;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x1F4C) = (uint8_t)rnd();
        regs(a, obj, 0, 1);
        old_run(old_abi_ObjectTableE8AF);
        v = object_table_e8af(obj, a);
        if (v != (uint8_t)old_cpu.a)
            fail("byte %02X, the decomp's %02X", v, (uint8_t)old_cpu.a);
        same();
    }
    CASE("ObjectTableC056") {
        uint16_t obj = 0;
        uint8_t v;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x1F5A) = (uint8_t)rnd();
        regs(0, 0, obj, 1);
        old_run(old_abi_ObjectTableC056);
        v = object_table_c056(obj);
        if (v != (uint8_t)old_cpu.a)
            fail("byte %02X, the decomp's %02X", v, (uint8_t)old_cpu.a);
        same();
    }
    CASE("ObjectVelocityFromRecord") {
        uint16_t obj = some_used();

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        OBJ_W(obj, 0x27) = some_used();
        {
            uint16_t r = OBJ_W(OBJ_W(obj, 0x27), 0x22);
            P16((uint16_t)(0x2E + r)) = (uint16_t)rnd();
            P16((uint16_t)(0x32 + r)) = (uint16_t)rnd();
        }
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectVelocityFromRecord);
        object_velocity_from_record(obj);
        same();
    }
    CASE("ObjectSet5DByKind") {
        uint16_t obj;
        uint8_t kind = (uint8_t)(rnd() % 6);

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        regs(kind, obj, 0, 1);
        old_run(old_abi_ObjectSet5DByKind);
        object_set_5d_by_kind(obj, kind);
        same();
    }
    CASE("ObjectPlaceFromTable") {
        uint8_t list = (uint8_t)(1 + rnd() % 4), entry = (uint8_t)(1 + rnd() % 8);
        uint16_t o;
        int k;

        fresh(1 + (int)(rnd() % 0x80));
        for (k = 0; k < 4; k++)
            W16(0xFFAE + 2 * k) = some_used();
        W16(0x1AEE) = (uint16_t)rnd() & (rnd() & 1 ? 0x0001 : 0xFFFE);
        W16(0x1F2D) = (uint16_t)rnd();
        W8(0x1F2B) = (uint8_t)rnd();
        W8(0x1F4C) = (uint8_t)rnd();
        regs(list, entry, 0, 0);
        old_run(old_abi_ObjectPlaceFromTable);
        object_place_from_table(list, entry, &o);
        same();
    }
    CASE("ObjectPlaceAll") {
        /* the list at $FFAE holds up to 4 words before the count at $FFB8 */
        int cnt = 1 + (int)(rnd() % 4), k;

        fresh(1 + (int)(rnd() % 0x80));
        W16(0xFFB8) = (uint16_t)(2 * cnt);
        for (k = 0; k < cnt; k++) {
            W16(0xFFA6 + 2 * k) = (uint16_t)(1 + rnd() % 8);
            W16(0xFFAE + 2 * k) = some_used();
        }
        for (k = 0; k < 0x20; k++)
            W16(0x18D3 + 2 * k) = (uint16_t)rnd(), W16(0x18DB + 2 * k) = (uint16_t)rnd(),
                W16(0x18E3 + 2 * k) = (uint16_t)rnd();
        W16(0x1AEE) = (uint16_t)(rnd() & 1);
        regs(0, 0, 0, 1);
        old_run(old_abi_ObjectPlaceAll);
        object_place_all();
        same();
    }
    CASE("ObjectLinkEventTrigger") {
        uint16_t x;

        fresh(1 + (int)(rnd() % 0x80));
        x = some_used();
        W16(0x0346) = x;
        OBJ_W(x, 0x4F) = some_used();
        OBJ_W(OBJ_W(x, 0x4F), 0x4F) = some_used();
        W8(0x0DA4) = (uint8_t)(rnd() & 1 ? rnd() : rnd() & 0x7F);
        W16(0x1A24) = (uint16_t)(rnd() & 1 ? rnd() : 0);
        {
            uint16_t y = OBJ_W(OBJ_W(x, 0x4F), 0x4F);
            W16(y) = (uint16_t)(rnd() & 1 ? 0x4000 | rnd() : rnd() & 0x3FFF);
        }
        regs(0, 0, 0, 1);
        old_run(old_abi_ObjectLinkEventTrigger);
        object_link_event_trigger();
        same();
    }
    CASE("ObjectLinkEventTrigger2") {
        uint16_t x;

        fresh(1 + (int)(rnd() % 0x80));
        x = some_used();
        W16(0x034A) = x;
        OBJ_W(x, 0x4F) = some_used();
        OBJ_W(OBJ_W(x, 0x4F), 0x4F) = some_used();
        W8(0x0DA6) = (uint8_t)(rnd() & 1 ? rnd() : rnd() & 0x7F);
        W16(0x1A26) = (uint16_t)(rnd() & 1 ? rnd() : 0);
        {
            uint16_t y = OBJ_W(OBJ_W(x, 0x4F), 0x4F);
            W16(y) = (uint16_t)(rnd() & 1 ? 0x4000 | rnd() : rnd() & 0x3FFF);
        }
        regs(0, 0, 0, 1);
        old_run(old_abi_ObjectLinkEventTrigger2);
        object_link_event_trigger_2();
        same();
    }
    CASE("ObjectLinkSpeedSet / ObjectLinkBobStart") {
        int k = (int)(rnd() & 1), m8 = (int)(rnd() & 1);
        uint16_t a = (uint16_t)rnd();

        fresh(1 + (int)(rnd() % 0x80));
        regs(a, 0, 0, k ? 1 : m8);
        if (k) {
            old_run(old_abi_ObjectLinkBobStart);
            object_link_bob_start((uint8_t)a);
        } else {
            old_run(old_abi_ObjectLinkSpeedSet);
            object_link_speed_set(a, m8);
        }
        same();
    }
    CASE("ObjectCountStep5F") {
        uint16_t obj;
        uint8_t a = 0;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        *OBJ(obj, 0x49) = (uint8_t)(rnd() % 0x10);
        W8(0x1F58) = (uint8_t)(rnd() % 0x10);
        W8(0x1F59) = (uint8_t)(rnd() % 0x10);
        W8(0x1F5A) = (uint8_t)rnd();
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectCountStep5F);
        object_count_step_5f(obj, &a);
        same();
    }
    CASE("ObjectNearFocus") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W16(0x1A6C) = (uint16_t)rnd();
        W16(0x1A70) = (uint16_t)rnd();
        W8(0x1A75) = (uint8_t)rnd();
        *OBJ(obj, 0x49) = (uint8_t)(rnd() % 0x10);
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectNearFocus);
        object_near_focus(obj);
        same();
    }
    CASE("ObjectRebucket") {
        uint16_t obj = some_used();

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        OBJ_W(obj, 0x0C) = (uint16_t)rnd();
        OBJ_W(obj, 0x10) = (uint16_t)rnd();
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectRebucket);
        object_rebucket(obj);
        same();
    }
    CASE("ObjectLinkedHandlerByState") {
        uint16_t obj, link, y, yy, a;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        link = some_used();
        OBJ_W(obj, 0x4F) = link;
        *OBJ(obj, 0x25) = (uint8_t)(rnd() & 0xF7);
        y = (uint16_t)(rnd() % 0x10);
        W8(0x0DD6 + y) = (uint8_t)(rnd() % 4 == 0 ? (uint8_t)rnd() : (rnd() % 2 ? 0x08 : (rnd() % 2 ? 0x0C : 0x0A)));
        W8(0x0E59) = (uint8_t)(rnd() & 1);
        W8(0x19ED) = (uint8_t)(rnd() & 1 ? 0x08 : rnd());
        P8(0x88 + OBJ_W(obj, 0x22)) = (uint8_t)(rnd() % 6);
        P16(0x42 + OBJ_W(obj, 0x22)) = (uint16_t)rnd();
        if (rnd() & 1) {
            W16(0x1AEE) = 0x0001;
        } else {
            W16(0x1AEE) = 0x0000;
            W16(0xFFB8) = 2;
            W16(0xFFAE) = some_used();
        }
        yy = y;
        regs(0, obj, y, 1);
        old_run(old_abi_ObjectLinkedHandlerByState);
        a = object_linked_handler_by_state(obj, &yy, 0);
        (void)a;
        same();
    }
    CASE("ObjectAttachLinked") {
        uint16_t obj, other, blk;

        fresh(2 + (int)(rnd() % 0x80));
        obj = some_used();
        other = some_used();
        if (other == obj)
            other = used[(rnd() % (unsigned)nused)];
        blk = (uint16_t)(0x4000 + 0x40 * (rnd() % 0x40));
        OBJ_W(other, 0x4F) = blk;
        OBJ_W(other, 0x1A) = (uint16_t)(rnd() & 1 ? 0x8E2A : rnd());
        P16(0x2396) = (uint16_t)(rnd() % 0x30);
        W8(0x0D2B) = (uint8_t)(rnd() % 5);
        regs(0, obj, other, 1);
        old_run(old_abi_ObjectAttachLinked);
        object_attach_linked(obj, other);
        same();
    }
    CASE("ObjectGroupsActivate") {
        int k = (int)(rnd() % 5), j;
        uint16_t y = 0;

        fresh(1 + (int)(rnd() % 0x80));
        for (j = 0; j < 0x10; j++)
            W16(0x1AF5 + 4 * j) = W16(0x1AF7 + 4 * j) = (uint16_t)rnd();
        for (j = 0; j < k; j++) {
            W16(0x1AF5 + 4 * j) = (uint16_t)(rnd() & 1 ? 0x23 : 1 + rnd() % 0xFF);
            W16(0x1AF7 + 4 * j) = (uint16_t)(rnd() % 0x400 * 2);
        }
        W16(0x1AF5 + 4 * k) = 0;
        for (j = 0; j < nused; j++)
            *OBJ(used[j], 0x24) = (uint8_t)rnd(), *OBJ(used[j], 0x36) = (uint8_t)rnd();
        regs(0, 0, 0, 0);
        old_run(old_abi_ObjectGroupsActivate);
        object_groups_activate(&y);
        same();
    }
    CASE("BucketTriggersFromView / BucketTriggersFixedArea") {
        int k = (int)(rnd() & 1), j;

        fresh(1 + (int)(rnd() % 0x80));
        W8(0x0667) = (uint8_t)rnd();
        W8(0x0D62) = (uint8_t)(rnd() & 1 ? 1 : 0);
        W8(0x1AEC) = (uint8_t)rnd();
        for (j = 0; j < 6; j++)
            W8(0x1EF5 + j) = (uint8_t)rnd(), W8(0x1EFB + j) = (uint8_t)rnd();
        regs(0, 0, 0, 1);
        if (k) {
            old_run(old_abi_BucketTriggersFromView);
            bucket_triggers_from_view();
        } else {
            old_run(old_abi_BucketTriggersFixedArea);
            bucket_triggers_fixed_area();
        }
        same();
    }
    CASE("ObjectStep48To8") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        *OBJ(obj, 0x48) = (uint8_t)(rnd() % 0x12);
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectStep48To8);
        object_step_48_to_8(obj);
        same();
    }
    CASE("ObjectSpawnChanceD460") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectSpawnChanceD460);
        object_spawn_chance_d460(obj);
        same();
    }
    CASE("ObjectAdvance5DIfFlag") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x0D62) = (uint8_t)(rnd() & 1);
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectAdvance5DIfFlag);
        object_advance_5d_if_flag(obj);
        same();
    }
    CASE("ObjectReleaseLong") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        if (rnd() & 1)
            object_running = obj;
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectReleaseLong);
        object_release_long(obj);
        same();
    }
    CASE("ObjectSpawnSparklePair") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectSpawnSparklePair);
        object_spawn_sparkle_pair(obj);
        same();
    }
    CASE("ObjectSparkleStep") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        *OBJ(obj, 0x48) = (uint8_t)(rnd() % 3);
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectSparkleStep);
        object_sparkle_step(obj);
        same();
    }
    CASE("ObjectSparkleStart") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectSparkleStart);
        object_sparkle_start(obj);
        same();
    }
    CASE("ObjectSpawnD460UnlessFlag") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x0D62) = (uint8_t)(rnd() % 3 == 0 ? 1 : 0);
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectSpawnD460UnlessFlag);
        object_spawn_d460_unless_flag(obj);
        same();
    }
    CASE("ObjectMoveForward") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        P16((uint16_t)(0x5E + OBJ_W(obj, 0x22))) = (uint16_t)(rnd() % 0x80);
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectMoveForward);
        object_move_forward(obj);
        same();
    }
    CASE("ObjectSpawnTrail / ObjectTrailStart / ObjectTrailStep") {
        int k = (int)(rnd() % 3);
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x0D62) = (uint8_t)(rnd() % 4 == 0 ? 1 : 0);
        *OBJ(obj, 0x4B) = (uint8_t)(rnd() % 3);
        regs(0, obj, 0, 1);
        if (k == 0) {
            old_run(old_abi_ObjectSpawnTrail);
            object_spawn_trail(obj);
        } else if (k == 1) {
            old_run(old_abi_ObjectTrailStart);
            object_trail_start(obj);
        } else {
            old_run(old_abi_ObjectTrailStep);
            object_trail_step(obj);
        }
        same();
    }
    CASE("ObjectSpawnAttached / ObjectAttachedStart / ObjectAttachedStep") {
        int k = (int)(rnd() % 3);
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x0D62) = (uint8_t)(rnd() % 4 == 0 ? 1 : 0);
        W8(0x1F59) = (uint8_t)rnd();
        W8(0x1F5A) = (uint8_t)rnd();
        *OBJ(obj, 0x4B) = (uint8_t)(rnd() % 3);
        regs(0, obj, 0, 1);
        if (k == 0) {
            old_run(old_abi_ObjectSpawnAttached);
            object_spawn_attached(obj);
        } else if (k == 1) {
            old_run(old_abi_ObjectAttachedStart);
            object_attached_start(obj);
        } else {
            old_run(old_abi_ObjectAttachedStep);
            object_attached_step(obj);
        }
        same();
    }
    CASE("ObjectPlaceOnParent") {
        uint16_t parent, obj;

        fresh(2 + (int)(rnd() % 0x80));
        parent = some_used();
        obj = some_used();
        if (parent == obj)
            obj = used[(rnd() % (unsigned)nused)];
        *OBJ(obj, 0x25) = (uint8_t)rnd();
        P16((uint16_t)(0x3A + OBJ_W(parent, 0x22))) = (uint16_t)rnd();
        W8(0x1A75) = (uint8_t)rnd();
        W8(0x1AB0) = (uint8_t)rnd();
        regs(0, parent, obj, 1);
        old_run(old_abi_ObjectPlaceOnParent);
        object_place_on_parent(parent, obj);
        same();
    }
    CASE("ObjectBillboardTilt") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x0D62) = (uint8_t)(rnd() & 1);
        W8(0x1A75) = (uint8_t)rnd();
        W8(0x1A77) = (uint8_t)rnd();
        W8(0x1AB0) = (uint8_t)rnd();
        regs(0, 0, obj, 1);
        old_run(old_abi_ObjectBillboardTilt);
        object_billboard_tilt(obj);
        same();
    }
    CASE("ObjectSpawnPuff") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        W8(0x1A28) = (uint8_t)rnd();
        W8(0x19EE) = (uint8_t)rnd();
        W8(0x19EF) = (uint8_t)rnd();
        regs(0, obj, obj, 1);
        old_run(old_abi_ObjectSpawnPuff);
        object_spawn_puff(obj);
        same();
    }
    CASE("ObjectTimedSetup / ObjectTimedStep") {
        int k = (int)(rnd() & 1);
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        *OBJ(obj, 0x4B) = (uint8_t)(rnd() % 3);
        regs(0, obj, 0, 1);
        if (k) {
            old_run(old_abi_ObjectTimedSetup);
            object_timed_setup(obj);
        } else {
            old_run(old_abi_ObjectTimedStep);
            object_timed_step(obj);
        }
        same();
    }
    CASE("ObjectCarPoseStep") {
        uint16_t obj;

        fresh(1 + (int)(rnd() % 0x80));
        obj = some_used();
        OBJ_W(obj, 0x0C) = (uint16_t)rnd();
        OBJ_W(obj, 0x10) = (uint16_t)rnd();
        regs(0, obj, 0, 1);
        old_run(old_abi_ObjectCarPoseStep);
        object_car_pose_step(obj);
        same();
    }
    printf("object_more_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
