/* old: object/draw_list.c
 *
 * obj_draw_list_check.c: the port's draw list build (object/obj_draw_list.c)
 * against the decomp's, on random states: the work RAM and game pak RAM
 * random, then the object buckets and the trigger list made whole (each
 * object in one bucket, bucket chains finite; a trigger list ended by a zero
 * word, every trigger's first word nonzero). Each case runs the decomp's
 * routine on a copy and the port's on the state, and compares both RAMs
 * from $0100 up (the decomp's $3E is direct-page scratch, not compared).
 *
 *   check/check.sh obj_draw_list N
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
#include "obj_draw_list.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(ObjectDrawListBuild)

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

/* Random RAM, then whole buckets: the heads of $7F:0000-$07FE cleared, `n`
   objects (of the 309 records) each put in one of the 64 buckets the
   triggers can name, and a trigger list of 0-12 triggers at $1AF5, each
   first word nonzero and each second word any value (its bucket, bits
   1-10, is a cleared head or one of the buckets above); a zero word ends
   it. Chains are finite: each object is inserted once. */
static void fresh(void)
{
    int order[0x136], i, j, n, t, nt;
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    for (k = 0; k < 0x800; k += 2)
        W16(0x10000 + k) = 0;
    for (i = 0; i < 0x136; i++)
        order[i] = i;
    for (i = 0x135; i > 0; i--) {
        int tmp;

        j = (int)(rnd() % (unsigned)(i + 1));
        tmp = order[i], order[i] = order[j], order[j] = tmp;
    }
    n = (int)(rnd() % 0x136);
    for (i = 0; i < n; i++) {
        uint16_t o = record(order[i]);

        OBJ_W(o, 0x40) = (uint16_t)(rnd() % 0x40 * 2);
        if (rnd() % 8)                  /* most not hidden by bit 5 of +$24, so the record limit binds */
            *OBJ(o, 0x24) &= (uint8_t)~0x20;
        object_bucket_insert(o);
    }
    nt = (int)(rnd() % 13);
    for (t = 0; t < nt; t++) {
        W16(0x1AF5 + 4 * t) = (uint16_t)(rnd() | 1);
        /* most triggers name a bucket of the 64 (so the list draws), the
           rest any word; a bucket may be named more than once */
        W16(0x1AF7 + 4 * t) = rnd() % 4 ? (uint16_t)(rnd() % 0x40 * 2) : (uint16_t)rnd();
    }
    W16(0x1AF5 + 4 * nt) = 0;
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)rnd();
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

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: obj_draw_list_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("ObjectDrawListBuild") {
        fresh();
        old_run(old_abi_ObjectDrawListBuild);
        object_draw_list_build();
        same();
    }
    printf("obj_draw_list_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
