/* old: save/records.c hud/lap_time.c

 * records_check.c: the port's saved-record routines (save/records.c) against
 * the decomp's, on random states: work RAM and game pak RAM random, then
 * shaped where the routine reads an index or a course (the realistic range
 * the game uses, named by each case). Each case runs the decomp's routine on
 * a copy and the port's on the state, and compares both RAMs but the direct
 * page.
 *
 *   check/check.sh records N
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
#include "records.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(RecordsUnpackCourse) OLD(RecordsUnpackCourseTotals) OLD(RecordsUnpackCourseRunning)
OLD(RecordsLoadForCourse) OLD(RecordsLoadCourse) OLD(RecordsLoadCourseLong)
OLD(RecordsLoadSpecialCourse) OLD(RecordsLoadSpecialCourseLong)
OLD(PreviewRowDrawStunt) OLD(PreviewRowDrawAll)
OLD(StuntRecordTime) OLD(TimeUpRecordUpdate)

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

/* Random RAM, and the decomp's register file: data bank $07 (the tag table's
   bank, which the course loads set), direct page 0, the stack high. */
static void fresh(void)
{
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.s = 0x01FF;
    old_cpu.db = 0x07;
    old_cpu.p = (uint8_t)((rnd() & (P_N | P_V | P_Z | P_C | P_I)) | (rnd() & 1 ? P_M : 0) | (rnd() & 1 ? P_X : 0));
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

/* A byte as binary coded decimal of a value below 100. */
static uint8_t bcd(uint32_t v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

/* A random packed time as the save keeps it, with its BCD parts: hundredths
   (0-99), seconds (0-59), minutes (0-9); written to $0E63, $0E64 and $0E66
   as the time-up run leaves them. */
static void random_time_up_time(void)
{
    W8(0x0E63) = bcd(rnd() % 100);
    W8(0x0E64) = bcd(rnd() % 60);
    W8(0x0E66) = bcd(rnd() % 10);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: records_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("RecordsLoadForCourse") {
        fresh();
        W8(0x0E3D) = (uint8_t)rnd();          /* any course: the three ranges */
        old_run(old_abi_RecordsLoadForCourse);
        records_load_for_course();
        same();
    }
    CASE("RecordsLoadSpecialCourse / RecordsLoadSpecialCourseLong") {
        int longform = (int)(rnd() & 1);

        fresh();
        W8(0x0E3D) = (uint8_t)(rnd() % 4 ? 0x0C + rnd() % 4 : rnd());   /* the special courses, now and then any */
        old_run(longform ? old_abi_RecordsLoadSpecialCourseLong : old_abi_RecordsLoadSpecialCourse);
        if (longform)
            records_load_special_course_long();
        else
            records_load_special_course();
        same();
    }
    CASE("RecordsLoadCourse / RecordsLoadCourseLong") {
        int longform = (int)(rnd() & 1);

        fresh();
        W8(0x0E3D) = (uint8_t)(rnd() % 0x0C);   /* a normal course: its lap-time records end at $FFFF */
        old_run(longform ? old_abi_RecordsLoadCourseLong : old_abi_RecordsLoadCourse);
        if (longform)
            records_load_course_long();
        else
            records_load_course();
        same();
    }
    CASE("RecordsUnpackCourse / RecordsUnpackCourseTotals / RecordsUnpackCourseRunning") {
        int k = (int)(rnd() % 3);

        fresh();
        W8(0x0D4A) = (uint8_t)(rnd() % 3);     /* the class: 0-2 */
        old_run(k == 0 ? old_abi_RecordsUnpackCourse : k == 1 ? old_abi_RecordsUnpackCourseTotals
                : old_abi_RecordsUnpackCourseRunning);
        if (k == 0)
            records_unpack_course();
        else if (k == 1)
            records_unpack_course_totals();
        else
            records_unpack_course_running();
        same();
    }
    CASE("PreviewRowDrawStunt / PreviewRowDrawAll") {
        int k = (int)(rnd() & 1);

        fresh();
        W8(0x0E3D) = (uint8_t)(rnd() % 0x10);  /* the courses $00-$0F */
        W8(0x0FA8) = (uint8_t)(rnd() % 4);     /* the row */
        W8(0x0FA9) = 0;
        W8(0x0FAA) = (uint8_t)(rnd() % 3);     /* the place */
        W8(0x0FAB) = 0;
        W8(0x0FA6) = (uint8_t)(rnd() % (k ? 8 : 7));  /* the character */
        W8(0x0FA7) = 0;
        if (rnd() % 4 == 0) {                  /* an empty entry: no time (all, $FFFF) or none (stunt, 0) */
            uint16_t x = (uint16_t)((((3u * W8(0x0E3D)) & 0xFF) << 4) + (W8(0x0FA8) * 3 << 2)
                                    + (W8(0x0FAA) << 2));
            P16(0xFCD0 + x) = k ? 0xFFFF : 0;
        }
        old_run(k ? old_abi_PreviewRowDrawAll : old_abi_PreviewRowDrawStunt);
        if (k)
            preview_row_draw_all();
        else
            preview_row_draw_stunt();
        same();
    }
    CASE("StuntRecordTime") {
        /* a course $0C-$0F, a car 0-3 (the car's entries are the 48 bytes of
           the course); the score's low byte is the BCD sum of two scores */
        fresh();
        W8(0x0E3D) = (uint8_t)(0x0C + rnd() % 4);
        W8(0x0D89) = (uint8_t)(rnd() % 4);
        W8(0x19EC) = (uint8_t)rnd();
        W8(0x0E70) = bcd(rnd() % 100);
        W8(0x19F0) = bcd(rnd() % 100);
        W8(0x0F11) = (uint8_t)rnd();
        W8(0x0F12) = (uint8_t)rnd();
        old_run(old_abi_StuntRecordTime);
        stunt_record_time();
        same();
    }
    CASE("TimeUpRecordUpdate") {
        uint16_t x;
        uint16_t t;
        BcdTime now;
        int mode = (int)(rnd() % 4);

        fresh();
        random_time_up_time();
        W8(0x0E3D) = (uint8_t)rnd();           /* the course: its record at (course - $17) * 5 */
        W8(0x0D89) = (uint8_t)rnd();
        W8(0x0F11) = (uint8_t)rnd();
        W8(0x0F12) = (uint8_t)rnd();
        now.hund = W8(0x0E63), now.sec = W8(0x0E64), now.min = W8(0x0E66);
        t = time_pack(now);
        x = (uint8_t)(5 * (W8(0x0E3D) - 0x17));
        /* the record: random, or equal to the time, or a little above or below */
        if (mode == 1)
            P16(0xFC4A + x) = t;
        else if (mode == 2)
            P16(0xFC4A + x) = (uint16_t)(t + 1);
        else if (mode == 3)
            P16(0xFC4A + x) = (uint16_t)(t - 1);
        old_run(old_abi_TimeUpRecordUpdate);
        time_up_record_update();
        same();
    }
    printf("records_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
