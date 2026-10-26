/* old: race/course.c video/colour_math.c audio/sound_queue.c system/raster.c video/oam.c math/scale.c
 *
 * course_check.c: the port's course routines (race/course.c) against the
 * decomp's, on random states: work RAM and game pak RAM random, the pads
 * random, the cursor, counts and states in the ranges the game keeps them
 * (the cursor 0-3, the records' counts 0-$2A, the ROM table offsets the
 * window zoom reads). Each case runs the decomp's routine on a copy and
 * the port's on the state, and compares both RAMs from $0100 up, and any
 * value the routine returns.
 *
 * The decomp's calls into other files (the sound queue, the colour maths,
 * the window table) run their decomp code; the cursor and fade dispatch
 * reach their handlers by address (model_routines).
 *
 *   check/check.sh course N
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
#include "course.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(CourseFromSelection) OLD(CourseFromSelectionLong)
OLD(CourseCursorAt0) OLD(CourseCursorAt1) OLD(CourseCursorAt2) OLD(CourseCursorAt3)
OLD(CourseCursorAt2Open) OLD(CourseCursorAt3Open) OLD(CourseCursorStep) OLD(RaceFadeStageDispatch)
OLD(CourseWindowZoomSetup) OLD(LevelColumnsSet4) OLD(TimeUpSelectDraw) OLD(TimeUpCursorStep)
OLD(CourseRecordsOpening) OLD(CourseRecordsReopen) OLD(CourseRecordsClosing)
OLD(CourseScreenWipeOut) OLD(CourseScreenWipeIn) OLD(CoursePreviewReset)
OLD(CourseScreenStartSpeed) OLD(CourseScreenStartAll) OLD(CourseScreenStartBattle) OLD(CourseScreenStartStunt)
OLD(CourseScreenNothing) OLD(CourseScreenZoomOut) OLD(CourseScreenZoomIn) OLD(CourseScreenState8)
OLD(CourseZoomStunt) OLD(CourseZoomBattle) OLD(CourseZoomFromSelection4) OLD(CourseZoomFromSelection)
OLD(CourseZoomTimeUp) OLD(CourseZoomTimeUpNext) OLD(CourseRecordsShown)
OLD(CourseScreenFadeLeave) OLD(CourseScreenFadeBack)

/* the handlers the decomp reaches by address: the cursor's table ($02:EC28,
   places 0-3) and the fade's ($02:EC18, the open places at 2 and 3) */
static const ModelRoutine routines[] = {
    {0x02ECB8, old_abi_CourseCursorAt0},
    {0x02ECD9, old_abi_CourseCursorAt1},
    {0x02ECFA, old_abi_CourseCursorAt2},
    {0x02ED1B, old_abi_CourseCursorAt3},
    {0x02EC30, old_abi_CourseCursorAt2Open},
    {0x02EC73, old_abi_CourseCursorAt3Open},
};

static uint64_t rng = 0x7A3C59B1E2F40D17ull;

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

/* Random RAM and a register set: 8-bit A and index registers, the data
   bank the course's RAM is in. */
static void fresh(void)
{
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    /* the sound rings as the game keeps them: a head within the ring of
       16 bytes and a count up to it; unmuted most of the time */
    W8(0x0DC8) = (uint8_t)(rnd() % 16);
    W8(0x0DCA) = (uint8_t)(rnd() % 16);
    W8(0x0DCC) = (uint8_t)(rnd() % 17);
    W8(0x0DCE) = (uint8_t)(rnd() % 17);
    W8(0x0E4E) = rnd() % 4 ? 0x00 : (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)((rnd() & (P_N | P_V | P_Z | P_C)) | P_M | P_X);
}

/* The pads: a direction or two, or none, with other bits now and then. */
static void random_pads(void)
{
    static const uint16_t dirs[] = {0x0100, 0x0200, 0x0400, 0x0800};
    uint16_t p = 0;

    if (rnd() % 4)
        p |= dirs[rnd() % 4];
    if (rnd() % 4 == 0)
        p |= dirs[rnd() % 4];
    if (rnd() % 8 == 0)
        p |= (uint16_t)(rnd() & 0x00F0);
    W16(0x0D1A) = p;
}

/* The course cursor's place and the save's fourth-course bits, as the
   select has them: the place 0-3, the bits of $FC06 all set half the time
   (the fourth course open). */
static void random_cursor(void)
{
    W8(0x0D48) = (uint8_t)(rnd() % 4);
    P8(0xFC06) = rnd() & 1 ? (uint8_t)(0x0F | (rnd() & 0xF0)) : (uint8_t)rnd();
    W8(0x0D63) = (uint8_t)rnd();
    random_pads();
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
        fprintf(stderr, "usage: course_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_routines = routines;
    model_nroutines = (int)(sizeof routines / sizeof routines[0]);

    CASE("CourseFromSelection / CourseFromSelectionLong") {
        int k = (int)(rnd() & 1);
        uint8_t got;

        fresh();
        W8(0x0D4A) = (uint8_t)(rnd() % 3);
        W8(0x0D48) = (uint8_t)(rnd() % 4);
        W8(0x0D4C) = rnd() % 4 == 0 ? (uint8_t)(1 + rnd() % 255) : 0;
        old_run(k ? old_abi_CourseFromSelectionLong : old_abi_CourseFromSelection);
        got = course_from_selection();
        if (got != (uint8_t)old_cpu.a)
            fail("course %02X, the decomp's %02X", got, (uint8_t)old_cpu.a);
        same();
    }
    CASE("CourseCursorAt0 / At1 / At2 / At3 / At2Open / At3Open") {
        int k = (int)(rnd() % 6);

        fresh();
        random_cursor();
        old_run(k == 0 ? old_abi_CourseCursorAt0 : k == 1 ? old_abi_CourseCursorAt1
                : k == 2 ? old_abi_CourseCursorAt2 : k == 3 ? old_abi_CourseCursorAt3
                : k == 4 ? old_abi_CourseCursorAt2Open : old_abi_CourseCursorAt3Open);
        switch (k) {
        case 0: course_cursor_bottom_left(); break;
        case 1: course_cursor_bottom_right(); break;
        case 2: course_cursor_top_left(); break;
        case 3: course_cursor_top_right(); break;
        case 4: course_cursor_top_left_open(); break;
        default: course_cursor_top_right_open(); break;
        }
        same();
    }
    CASE("CourseCursorStep") {
        fresh();
        random_cursor();
        old_cpu.p |= P_M | P_X;                              /* 8-bit: the cursor is a byte */
        old_run(old_abi_CourseCursorStep);
        course_cursor_step();
        same();
    }
    CASE("RaceFadeStageDispatch") {
        fresh();
        random_cursor();
        old_run(old_abi_RaceFadeStageDispatch);
        course_cursor_fade_dispatch();
        same();
    }
    CASE("CourseWindowZoomSetup") {
        uint16_t x = (uint16_t)(0x80 + 2 * (rnd() % 0x40));     /* the box offsets, $80-$FE */
        uint16_t y = (uint16_t)(2 * (rnd() % 0x10));            /* the target offsets, $00-$1E */

        fresh();
        old_cpu.db = 0x03;                  /* the tables are read in bank $03 */
        old_cpu.x = x;
        old_cpu.y = y;
        old_cpu.p &= (uint8_t)~(P_M | P_X);                    /* 16-bit */
        old_run(old_abi_CourseWindowZoomSetup);
        course_window_zoom_setup(x, y);
        same();
    }
    CASE("LevelColumnsSet4") {
        uint8_t v = (uint8_t)rnd();
        uint16_t x = (uint16_t)(4 * (rnd() % 3));          /* the row's group: 0, 4 or 8 */

        fresh();
        old_cpu.a = v;
        old_cpu.x = x;
        old_cpu.p |= P_M;                                  /* 8-bit stores */
        old_run(old_abi_LevelColumnsSet4);
        level_columns_set4(v, x);
        same();
    }
    CASE("TimeUpSelectDraw") {
        fresh();
        W8(0x0D4E) = rnd() & 1 ? 0x00 : 0xFF;
        W8(0x0D48) = (uint8_t)(rnd() % 4);
        old_run(old_abi_TimeUpSelectDraw);
        time_up_select_draw();
        same();
    }
    CASE("TimeUpCursorStep") {
        fresh();
        random_cursor();
        W8(0x0D4E) = rnd() & 1 ? 0x00 : 0xFF;
        old_cpu.p |= P_M;
        old_run(old_abi_TimeUpCursorStep);
        time_up_cursor_step();
        same();
    }
    CASE("CourseRecordsOpening") {
        fresh();
        W8(0x0D42) = (uint8_t)(rnd() % 4);                 /* counting down to 0 */
        W8(0x0D4C) = rnd() & 1 ? 0x00 : 0x01;
        old_run(old_abi_CourseRecordsOpening);
        course_records_opening();
        same();
    }
    CASE("CourseRecordsReopen") {
        fresh();
        W8(0x0D42) = (uint8_t)(rnd() % 0x2B);              /* counting up to $2A */
        old_run(old_abi_CourseRecordsReopen);
        course_records_reopen();
        same();
    }
    CASE("CourseRecordsClosing") {
        fresh();
        W8(0x0D42) = (uint8_t)(rnd() % 0x2B);
        W8(0x0D4A) = (uint8_t)(rnd() % 3);
        W8(0x0D48) = (uint8_t)(rnd() % 4);
        W8(0x0D43) = (uint8_t)rnd();
        W8(0x0D44) = (uint8_t)rnd();
        old_run(old_abi_CourseRecordsClosing);
        course_records_closing();
        same();
    }
    CASE("CourseScreenWipeOut") {
        fresh();
        W8(0x10AD) = rnd() & 1 ? (uint8_t)(rnd() % 8) : (uint8_t)rnd();
        W8(0x10B0) = (uint8_t)rnd();
        W8(0x10B3) = (uint8_t)rnd();
        old_run(old_abi_CourseScreenWipeOut);
        course_screen_wipe_out();
        same();
    }
    CASE("CourseScreenWipeIn") {
        fresh();
        W8(0x10AD) = (uint8_t)(rnd() % 0x60);
        W8(0x10B0) = (uint8_t)rnd();
        W8(0x10B3) = (uint8_t)rnd();
        old_run(old_abi_CourseScreenWipeIn);
        course_screen_wipe_in();
        same();
    }
    CASE("CoursePreviewReset") {
        fresh();
        old_run(old_abi_CoursePreviewReset);
        course_preview_reset();
        same();
    }
    CASE("CourseScreenStartSpeed / All / Battle / Stunt / Nothing / State8") {
        int k = (int)(rnd() % 6);

        fresh();
        W8(0x0D40) = (uint8_t)rnd();
        W8(0x10D8) = (uint8_t)rnd();
        old_run(k == 0 ? old_abi_CourseScreenStartSpeed : k == 1 ? old_abi_CourseScreenStartAll
                : k == 2 ? old_abi_CourseScreenStartBattle : k == 3 ? old_abi_CourseScreenStartStunt
                : k == 4 ? old_abi_CourseScreenNothing : old_abi_CourseScreenState8);
        switch (k) {
        case 0: course_screen_start_speed(); break;
        case 1: course_screen_start_all(); break;
        case 2: course_screen_start_battle(); break;
        case 3: course_screen_start_stunt(); break;
        case 4: course_screen_nothing(); break;
        default: course_screen_state8(); break;
        }
        same();
    }
    CASE("CourseScreenZoomOut") {
        uint16_t lo = (uint16_t)rnd();

        fresh();
        W16(0x13434) = lo;
        /* half the cases with the edges near each other, so the zoom ends */
        W16(0x13438) = rnd() & 1 ? (uint16_t)(lo + rnd() % 0x1400) : (uint16_t)rnd();
        old_run(old_abi_CourseScreenZoomOut);
        course_screen_zoom_out();
        same();
    }
    CASE("CourseScreenZoomIn") {
        uint16_t lo = (uint16_t)rnd();

        fresh();
        W16(0x13434) = lo;
        W16(0x13438) = (uint16_t)rnd();
        /* the target below the edge half the cases, so the zoom ends */
        W16(0x1343C) = rnd() & 1 ? (uint16_t)(lo - rnd() % 0x600) : (uint16_t)rnd();
        W16(0x13440) = (uint16_t)rnd();
        old_run(old_abi_CourseScreenZoomIn);
        course_screen_zoom_in();
        same();
    }
    CASE("CourseZoomStunt / Battle / TimeUp / TimeUpNext") {
        int k = (int)(rnd() % 4);

        fresh();
        random_cursor();
        W16(0x0D48) = (uint16_t)(rnd() % 4);                 /* the cursor word: the place, high byte 0 */
        W16(0x0D2B) = rnd() & 1 ? 0x0001 : 0x0000;           /* the HUD mode word, both zoom boxes */
        W16(0x0E3D) = (uint16_t)(rnd() % 0x30);              /* the course, for the time-up next step */
        old_run(k == 0 ? old_abi_CourseZoomStunt : k == 1 ? old_abi_CourseZoomBattle
                : k == 2 ? old_abi_CourseZoomTimeUp : old_abi_CourseZoomTimeUpNext);
        switch (k) {
        case 0: course_zoom_stunt(); break;
        case 1: course_zoom_battle(); break;
        case 2: course_zoom_time_up(); break;
        default: course_zoom_time_up_next(); break;
        }
        same();
    }
    CASE("CourseZoomFromSelection4 / CourseZoomFromSelection") {
        int k = (int)(rnd() & 1);

        fresh();
        W8(0x0D4A) = (uint8_t)(rnd() % 3);
        W8(0x0D48) = (uint8_t)(rnd() % 4);
        W8(0x0D4C) = rnd() % 4 == 0 ? (uint8_t)(1 + rnd() % 255) : 0;
        W8(0x0D63) = (uint8_t)(rnd() & 1);
        W16(0x0D2B) = rnd() & 1 ? 0x0001 : 0x0000;
        old_run(k ? old_abi_CourseZoomFromSelection4 : old_abi_CourseZoomFromSelection);
        if (k)
            course_zoom_from_selection4();
        else
            course_zoom_from_selection();
        same();
    }
    CASE("CourseRecordsShown") {
        fresh();
        W8(0x0D42) = (uint8_t)(rnd() % 0x2B);              /* counting down: 0 ends the panel */
        W8(0x0D4A) = (uint8_t)(rnd() % 3);
        W8(0x0D48) = (uint8_t)(rnd() % 4);
        W8(0x0D63) = (uint8_t)(rnd() & 1);
        W8(0x0D4C) = rnd() % 4 == 0 ? (uint8_t)(1 + rnd() % 255) : 0;
        W16(0x0D2B) = rnd() & 1 ? 0x0001 : 0x0000;
        old_run(old_abi_CourseRecordsShown);
        course_records_shown();
        same();
    }
    CASE("CourseScreenFadeLeave / CourseScreenFadeBack") {
        int k = (int)(rnd() & 1);

        fresh();
        /* the blend as the zoom leaves it: limit 15, the factor 0-15 mostly */
        W16(0x1340A) = rnd() % 8 ? 0x000F : (uint16_t)rnd();
        W16(0x13408) = rnd() % 8 ? (uint16_t)(rnd() % 16) : (uint16_t)rnd();
        for (int j = 0; j < 4; j++) {
            W16(0x13442 + 2 * j) = (uint16_t)rnd();        /* the bases */
            W16(0x1343A + 2 * j) = (uint16_t)rnd();        /* the targets */
        }
        W8(0x0D2B) = rnd() & 1 ? 0x00 : 0x01;
        W8(0x0D24) = (uint8_t)(rnd() % 5);
        W8(0x0D4C) = rnd() % 4 == 0 ? 0x01 : 0x00;
        W8(0x0D40) = (uint8_t)rnd();
        old_run(k ? old_abi_CourseScreenFadeBack : old_abi_CourseScreenFadeLeave);
        if (k)
            course_screen_fade_back();
        else
            course_screen_fade_leave();
        same();
    }
    printf("course_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
