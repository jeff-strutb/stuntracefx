/* old: race/lap.c
 *
 * lap_check.c: the port's lap splits (race/lap.c) against the decomp's, on
 * random states: the work RAM and game pak RAM random, the race clock and
 * the lap mark valid BCD as the race keeps them (fraction and seconds
 * below 100 and 60, minutes 0-9), the lap slot x one the game uses (0, 3
 * or 6). Each case runs the decomp's routine on a copy and the port's on
 * the state, and compares both RAMs but the direct page, and the minutes
 * the split returns.
 *
 *   check/check.sh lap N
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
#include "lap.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(LapSplit) OLD(RaceTimeKeep)

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

static uint8_t bcd(uint32_t limit)
{
    uint32_t v = rnd() % limit;

    return (uint8_t)((v / 10) << 4 | v % 10);
}

/* Random RAM; the race clock and the lap mark as the race keeps them. */
static void fresh(void)
{
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    race_time_fraction = bcd(100);
    race_time_seconds = bcd(60);
    race_time_minutes = bcd(10);
    if (rnd() % 4 == 0) {               /* the mark also from the start, or nearly */
        W8(0x0EDB) = race_time_fraction;
        W8(0x0EDC) = race_time_seconds;
        W8(0x0EDD) = race_time_minutes;
    } else {
        W8(0x0EDB) = bcd(100);
        W8(0x0EDC) = bcd(60);
        W8(0x0EDD) = bcd(10);
    }
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)((rnd() & (P_N | P_V | P_Z | P_C | P_I)) | P_M | P_X);
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
    static const uint8_t slots[3] = { 0, 3, 6 };

    if (argc < 2) {
        fprintf(stderr, "usage: lap_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("LapSplit") {
        uint8_t x = slots[rnd() % 3], ret;

        fresh();
        old_cpu.x = x;
        old_run(old_abi_LapSplit);
        ret = lap_split(x);
        if (ret != (uint8_t)old_cpu.a)
            fail("returns minutes %02X, the decomp's %02X", ret, (uint8_t)old_cpu.a);
        same();
    }
    CASE("RaceTimeKeep") {
        fresh();
        old_run(old_abi_RaceTimeKeep);
        race_time_keep();
        same();
    }
    printf("lap_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
