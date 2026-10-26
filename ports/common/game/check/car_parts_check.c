/* old: object/car_parts.c object/object.c object/gsu_record.c math/rotate.c math/approach.c math/scale.c audio/sound_queue.c
 *
 * car_parts_check.c: the port's car part routines (object/car_parts.c)
 * against the decomp's, on random states: the object table whole (lists
 * and buckets, as object_check builds them), a part and its car two used
 * objects with the part's +$06 its car, each used object's Super FX record
 * in game pak RAM, the rest of work RAM random. Each case runs the decomp's
 * routine on a copy and the port's on the state, and compares both RAMs but
 * the direct page.
 *
 *   check/check.sh car_parts N
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
#include "car_parts.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(PartOffset) OLD(PartDetach) OLD(PartDetachStep) OLD(PartReturn) OLD(PartReturnStep)

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

/* Random RAM with whole lists: `n` objects used (in a random order), the
   rest free; each used one in a bucket and with its Super FX record in game
   pak RAM ($70:0100-$70:2080, where the game keeps them). */
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
    /* the bucket heads (bank $7F, one per grid key) start empty, so every
       list is whole: a position's key is kx + kz, below $800 (kx up to $3E,
       kz up to $7C0), and a part moved by its car can take any of them */
    for (k = 0; k < 0x800; k += 2)
        W16(0x10000 + k) = 0;
    for (i = 0; i < n; i++) {
        OBJ_W(used[i], 0x40) = (uint16_t)(rnd() % 0x40 * 2);
        OBJ_W(used[i], 0x22) = (uint16_t)(0x100 + 0x80 * (rnd() % 0x40));
        object_bucket_insert(used[i]);
    }
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)((rnd() & (P_N | P_V | P_Z | P_C | P_I)) | P_M);
}

/* Half the time a falling part sits near its car (its position within 32 of
   the car's and its +$14-$16 within 8), so the approach steps are small and
   their minimums matter; the rest of the time it is anywhere. */
static void near_car(uint16_t part, uint16_t car)
{
    unsigned i;

    if (!(rnd() & 1))
        return;
    for (i = 0; i < 3; i++) {
        OBJ_W(part, 0x0C + 2 * i) = (uint16_t)(OBJ_W(car, 0x0C + 2 * i) + (int)(rnd() % 64) - 32);
        *OBJ(part, 0x14 + i) = (uint8_t)(*OBJ(car, 0x14 + i) + (int)(rnd() % 16) - 8);
    }
}

/* A part and its car, two used objects: the part's +$06 is the car, and both
   registers hold them as the routines' entries have it (X = part, Y = car;
   M 8-bit, X 16-bit). */
static void pick_part_car(uint16_t *part, uint16_t *car)
{
    *part = used[rnd() % (unsigned)nused];
    do
        *car = used[rnd() % (unsigned)nused];
    while (*car == *part);
    OBJ_W(*part, 0x06) = *car;
    old_cpu.x = *part;
    old_cpu.y = *car;
    old_cpu.p &= (uint8_t)~P_X;
    old_cpu.p |= P_M;
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

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: car_parts_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    model_current = "PartOffset";
    for (i = 0; i < n; i++) {
        uint16_t part, car, off[3];

        iteration = i;
        fresh(2 + (int)(rnd() % 0x135));
        pick_part_car(&part, &car);
        old_run(old_abi_PartOffset);
        part_offset(part, car, off);
        same();
    }
    model_current = "PartDetach";
    for (i = 0; i < n; i++) {
        uint16_t part, car;

        iteration = i;
        fresh(2 + (int)(rnd() % 0x135));
        pick_part_car(&part, &car);
        old_run(old_abi_PartDetach);
        part_detach(part);
        same();
    }
    /* the count +$4B is 0 to 8 here (8 releases the part) */
    model_current = "PartDetachStep";
    for (i = 0; i < n; i++) {
        uint16_t part, car;

        iteration = i;
        fresh(2 + (int)(rnd() % 0x135));
        pick_part_car(&part, &car);
        *OBJ(part, 0x4B) = (uint8_t)(rnd() % 9);
        old_run(old_abi_PartDetachStep);
        part_detach_step(part);
        same();
    }
    /* the count +$4B is 0 to 10 here (PartReturn sets 10, then it counts down) */
    model_current = "PartReturn";
    for (i = 0; i < n; i++) {
        uint16_t part, car;

        iteration = i;
        fresh(2 + (int)(rnd() % 0x135));
        pick_part_car(&part, &car);
        near_car(part, car);
        old_run(old_abi_PartReturn);
        part_return(part);
        same();
    }
    model_current = "PartReturnStep";
    for (i = 0; i < n; i++) {
        uint16_t part, car;

        iteration = i;
        fresh(2 + (int)(rnd() % 0x135));
        pick_part_car(&part, &car);
        *OBJ(part, 0x4B) = (uint8_t)(rnd() % 11);
        near_car(part, car);
        old_run(old_abi_PartReturnStep);
        part_return_step(part);
        same();
    }
    printf("car_parts_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
