/* old: object/object.c
 *
 * object_check.c: the port's object table (object/objects.c) against the
 * decomp's, on random states: the object table's lists and buckets made
 * whole (the used and free lists, each used object in one bucket), the
 * rest of work RAM random. Each case runs the decomp's routine on a copy
 * and the port's on the state, and compares both RAMs but the direct page.
 *
 *   check/check.sh object N
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
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(ObjectPoolInit) OLD(ObjectInit) OLD(ObjectBucketInsert) OLD(ObjectBucketRemove) OLD(ObjectAlloc)
OLD(ObjectSpawn) OLD(ObjectFree) OLD(ObjectFreeLong) OLD(ObjectRelease) OLD(ObjectReleaseY) OLD(ObjectsRunHandlers)
OLD(ObjectCopy14To) OLD(ObjectCopy14From) OLD(ObjectCopyPositionTo) OLD(ObjectCopyPositionFrom)
OLD(ObjectApplyVelocity) OLD(ObjectListAppend) OLD(ObjectLinkedSet1A)

static uint64_t rng = 0x510E527FADE682D1ull;

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

/* Random RAM with whole lists: `n` objects used (in a random order), the
   rest free; each used one in a bucket. */
static void fresh(int n)
{
    int order[0x136], i, j;
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
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
    for (k = 0; k < 0x80; k += 2)
        W16(0x10000 + k) = 0;
    for (i = 0; i < n; i++) {
        OBJ_W(used[i], 0x40) = (uint16_t)(rnd() % 0x40 * 2);
        object_bucket_insert(used[i]);
    }
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)((rnd() & (P_N | P_V | P_Z | P_C | P_I)) | P_M);
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

static uint16_t some_used(void)
{
    return nused ? used[rnd() % (unsigned)nused] : record(0);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: object_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("ObjectPoolInit") {
        fresh((int)(rnd() % 0x137));
        old_run(old_abi_ObjectPoolInit);
        object_pool_init();
        same();
    }
    CASE("ObjectInit / ObjectBucketInsert / ObjectBucketRemove") {
        int k = (int)(rnd() % 3);
        uint16_t o;

        fresh(1 + (int)(rnd() % 0x135));
        o = k == 0 && objects_free ? objects_free : some_used();   /* init: a record just taken */
        if (k == 1) {                   /* a record out of its bucket, put back */
            object_bucket_remove(o);
            OBJ_W(o, 0x40) = (uint16_t)(rnd() % 0x40 * 2);
        }
        old_cpu.x = old_cpu.y = o;
        old_cpu.p &= (uint8_t)~P_X;
        if (k == 0)
            old_cpu.p |= P_M;
        old_run(k == 0 ? old_abi_ObjectInit : k == 1 ? old_abi_ObjectBucketInsert : old_abi_ObjectBucketRemove);
        switch (k) {
        case 0: object_init(o); break;
        case 1: object_bucket_insert(o); break;
        default: object_bucket_remove(o);
        }
        same();
    }
    CASE("ObjectAlloc / ObjectSpawn") {
        int k = (int)(rnd() & 1);
        uint16_t o;

        fresh((int)(rnd() % 0x137));
        W8(0x0CEE) = (uint8_t)(rnd() & 1 ? rnd() % 0x30 : rnd());
        W16(0x1F2B) = (uint16_t)rnd();
        old_cpu.p &= (uint8_t)~P_X;
        old_run(k ? old_abi_ObjectSpawn : old_abi_ObjectAlloc);
        if (k)
            object_spawn(&o);
        else
            object_alloc(&o);
        same();
    }
    CASE("ObjectFree / ObjectRelease / ObjectReleaseY") {
        int k = (int)(rnd() % 4);
        uint16_t o;

        fresh(1 + (int)(rnd() % 0x135));
        o = some_used();
        if (rnd() & 1)
            object_running = o;
        old_cpu.x = old_cpu.y = o;
        old_cpu.p &= (uint8_t)~P_X;
        old_run(k == 0 ? old_abi_ObjectFree : k == 1 ? old_abi_ObjectFreeLong : k == 2 ? old_abi_ObjectRelease
                : old_abi_ObjectReleaseY);
        if (k < 2)
            object_free(o);
        else
            object_release(o);
        same();
    }
    CASE("ObjectsRunHandlers (without handlers)") {
        int j;

        fresh((int)(rnd() % 0x137));
        for (j = 0; j < nused; j++) {
            obj_handler(used[j]) = 0;
            obj_handler_b(used[j]) = 0;
            if (rnd() & 1)
                obj_countdown(used[j]) = (uint8_t)(rnd() % 4);
        }
        old_cpu.p &= (uint8_t)~P_X;
        old_run(old_abi_ObjectsRunHandlers);
        objects_run_handlers();
        same();
    }
    CASE("ObjectCopy / ObjectApplyVelocity / ObjectListAppend / ObjectLinkedSet1A") {
        int k = (int)(rnd() % 8);
        uint16_t a, b;

        fresh(2 + (int)(rnd() % 0x134));
        a = some_used(), b = some_used();
        W16(0xFFFD) = (uint16_t)(rnd() % 0x21 * 2);
        if (k == 7)
            obj_link(a) = b;
        old_cpu.x = a, old_cpu.y = b;
        old_cpu.p &= (uint8_t)~P_X;
        if (k == 7)
            old_cpu.p &= (uint8_t)~P_M;
        old_run(k == 0 ? old_abi_ObjectCopy14To : k == 1 ? old_abi_ObjectCopy14From : k == 2 ? old_abi_ObjectCopyPositionTo
                : k == 3 ? old_abi_ObjectCopyPositionFrom : k == 4 ? old_abi_ObjectApplyVelocity
                : k == 5 || k == 6 ? old_abi_ObjectListAppend : old_abi_ObjectLinkedSet1A);
        switch (k) {
        case 0: object_copy_14(a, b); break;
        case 1: object_copy_14(b, a); break;
        case 2: object_copy_position(a, b); break;
        case 3: object_copy_position(b, a); break;
        case 4: object_apply_velocity(a); break;
        case 5: case 6: object_list_append(a); break;
        default: object_linked_set_1a(a, old_cpu.a);
        }
        same();
    }
    printf("object_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
