/* old: race/player_stats.c save/checksum.c gsu/run.c gsu/params.c gsu/screen.c gsu/programs.c render/unused.c render/matrix.c render/arith.c render/angle.c math/fx_math.c gsu/fx_init.c data/unpack.c render/sort.c render/hud.c render/points.c render/fx_points.c render/project.c render/table.c hud/lap_time.c audio/sound_queue.c system/misc.c
 *
 * player_stats_check.c: the port's race/player_stats.c against the decomp's,
 * on random states: the speed sample and its totals, the Stunt Trax panel's
 * count lines and course mark, and the time-up phase's first step and
 * slides. The Super FX programs the glyph routines start run in the bus
 * model (model_gsu_start, as gsu_check does). Each case runs the decomp's
 * routine on a copy and the port's on the state, and compares both RAMs
 * from $0100 up (the direct page is the decomp's scratch), leaving out the
 * Super FX's return stack in the game pak RAM ($0240-$02A0) and the poll
 * counts the original keeps ($1F21-$1F22).
 *
 *   check/check.sh player_stats N
 */
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
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
#include "render.h"
#include "glue.h"
#include "player_stats.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(SpeedSampleAdd) OLD(TimeUpPhaseStart) OLD(StuntCourseMark) OLD(StuntPanelCount) OLD(StuntPanelShow)
OLD(TimeUpPhaseSlideIn) OLD(TimeUpPhaseSlideOut) OLD(GlyphRowsAt32) OLD(GlyphSpritesSlideIn) OLD(GlyphSpritesSlideOut)
OLD(FxClearScreenIfSet) OLD(FxClear6000IfSet) OLD(FxMatrixFromAnglesProgram) OLD(FxInit)
OLD(FxUnpackProgram) OLD(FxLengthProgram) OLD(FxMatrixRotateProgram) OLD(FxAngleProgram) OLD(FxSortObjectsProgram)
OLD(FxNoiseFill) OLD(FxGlyph8x16) OLD(FxGlyph24x16) OLD(FxPictureRise40) OLD(FxPictureRise60) OLD(FxPictureRiseSplit40)
OLD(FxPictureRiseSplit60) OLD(FxGlyph16RowsFromTop) OLD(FxScaledSprite13) OLD(FxScaledSpriteEighth)
OLD(FxGlyph16RowsFromBottom) OLD(FxSprite24x40) OLD(FxScaledSprite12) OLD(FxGlyph8x8) OLD(FxGlyph24x32Bank04)
OLD(FxGlyph16x32) OLD(FxSprite16x32) OLD(FxGlyph16x16)

static uint64_t rng = 0x3C6EF372FE94F82Bull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

/* ---- the decomp's programs, by where they start (as gsu_check) ---- */
static const struct {
    uint32_t at;
    void (*run)(void);
} olds[] = {
    {0x01AECA, old_abi_FxClearScreenIfSet},
    {0x01AEE1, old_abi_FxClear6000IfSet},
    {0x018325, old_abi_FxMatrixFromAnglesProgram},
    {0x048800, old_abi_FxInit},
    {0x01CF42, old_abi_FxUnpackProgram},
    {0x0180BC, old_abi_FxLengthProgram},
    {0x0182CE, old_abi_FxMatrixRotateProgram},
    {0x0181E3, old_abi_FxAngleProgram},
    {0x01AEF8, old_abi_FxSortObjectsProgram},
    {0x07EE88, old_abi_FxNoiseFill},
    {0x07F329, old_abi_FxGlyph8x16},
    {0x07F441, old_abi_FxGlyph24x16},
    {0x07F0A5, old_abi_FxPictureRise40},
    {0x07F0B4, old_abi_FxPictureRise60},
    {0x07EEF5, old_abi_FxPictureRiseSplit40},
    {0x07EFCD, old_abi_FxPictureRiseSplit60},
    {0x07F15B, old_abi_FxGlyph16RowsFromTop},
    {0x07FA1B, old_abi_FxScaledSprite13},
    {0x07FA5A, old_abi_FxScaledSpriteEighth},
    {0x07F121, old_abi_FxGlyph16RowsFromBottom},
    {0x07FA88, old_abi_FxSprite24x40},
    {0x07FA35, old_abi_FxScaledSprite12},
    {0x07F280, old_abi_FxGlyph8x8},
    {0x07F3D2, old_abi_FxGlyph24x32Bank04},
    {0x07F6DF, old_abi_FxGlyph16x32},
    {0x07FAB0, old_abi_FxSprite16x32},
    {0x07F5C9, old_abi_FxGlyph16x16},
};

static void old_start(uint8_t bank, uint16_t start)
{
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

/* the decomp's routines it reaches with call_routine, by address */
static const ModelRoutine routines[] = {
    {0x078821, old_abi_GlyphRowsAt32},
    {0x07A492, old_abi_GlyphSpritesSlideIn},
    {0x07A451, old_abi_GlyphSpritesSlideOut},
};

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration, skipped;

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

/* Random RAM, the data bank $07 the routines are entered with, and the
   Super FX's screen as the game leaves it. */
static void fresh(void)
{
    size_t i;

    for (i = 0; i < sizeof wram; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x07;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
    memset(&old_gsu, 0, sizeof old_gsu);
    old_gsu.scbr = fx_screen.base = (uint8_t)(rnd() & 1 ? 0x0B : 0x25);
    W8(0x0374) = fx_screen.base == 0x25 || (rnd() & 1) ? 0x01 : 0x03;
    fx_screen.mode = 0, fx_screen.height = 0;
    {
        static const uint8_t zero[8];

        fx_pixels_set(0, 0xFFFF, 0, zero);
        fx_pixels_set(1, 0xFFFF, 0, zero);
    }
}

static sigjmp_buf escape;

static void runaway(int sig)
{
    (void)sig;
    siglongjmp(escape, 1);
}

/* a case the decomp runs away in (a loop on a random input) is skipped; one
   the port runs away in after the decomp finished is a difference */
static int old_finished;

#define GUARD if (sigsetjmp(escape, 1)) { if (old_finished) fail("the port runs away, the decomp does not"); else skipped++; alarm(0); continue; } alarm(5)

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    old_finished = 0;
    model_pixels_empty();
    f();
    model_pixels_flush();
    old_finished = 1;
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same_ram(void)
{
    size_t i;

    alarm(0);
    fx_pixels_flush();
    cases++;
    for (i = 0x100; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && i != 0x1F21 && i != 0x1F22) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0)) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
}

/* a player's total as a random one, near its limit one time in three; its
   count sometimes near its wrap */
static void random_total(uint16_t p)
{
    uint16_t sum = p == 0 ? 0x0DDC : 0x0DE0;

    if (rnd() % 3 == 0) {
        W16(sum + 2) = 0xFFFF;
        W16(sum) = (uint16_t)(0xFF00 + rnd() % 0x100);
    }
    if (rnd() & 1)
        W16(p == 0 ? 0x0DE4 : 0x0DE6) = 0xFFFF;
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: player_stats_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_gsu_start = old_start;
    model_routines = routines;
    model_nroutines = (int)(sizeof routines / sizeof routines[0]);
    signal(SIGALRM, runaway);

    CASE("SpeedSampleAdd") {
        uint16_t p = (uint16_t)(rnd() & 1), speed, v;

        fresh();
        GUARD;
        speed = (uint16_t)rnd();
        if (rnd() & 1)
            speed &= 0x03FF;            /* the speeds of a car, mostly */
        if (rnd() % 50 == 0)
            speed = 0x8000;
        random_total(p);
        old_cpu.a = speed;
        old_cpu.y = p;
        old_cpu.p &= (uint8_t)~P_M;
        old_run(old_abi_SpeedSampleAdd);
        v = speed_sample_add(p, speed);
        same_ram();
        if (v != old_cpu.a)
            fail("A %04X, the decomp's %04X", v, old_cpu.a);
    }
    CASE("TimeUpPhaseStart") {
        fresh();
        GUARD;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_run(old_abi_TimeUpPhaseStart);
        time_up_phase_start();
        same_ram();
    }
    CASE("StuntCourseMark") {
        fresh();
        GUARD;
        W8(0x0E3D) = rnd() % 8 == 0 ? (uint8_t)rnd() : (uint8_t)(0x0C + rnd() % 4);
        old_cpu.p &= (uint8_t)~P_M;
        old_run(old_abi_StuntCourseMark);
        stunt_course_mark();
        same_ram();
    }
    CASE("StuntPanelCount / StuntPanelShow") {
        int k = (int)(rnd() & 1);

        fresh();
        GUARD;
        W8(0x0E3F) = (uint8_t)(rnd() % 0x21);      /* the counts the panel sets ($0F, $20), then down */
        W8(0x0E41) = (uint8_t)rnd();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_run(k ? old_abi_StuntPanelShow : old_abi_StuntPanelCount);
        if (k)
            stunt_panel_show();
        else
            stunt_panel_count();
        same_ram();
    }
    CASE("TimeUpPhaseSlideIn / TimeUpPhaseSlideOut") {
        int k = (int)(rnd() & 1), e;

        fresh();
        GUARD;
        for (e = 0; e < 48; e += 2) {           /* the glyph sprites on the screen */
            W16(0x0FB0 + e) = (uint16_t)(rnd() % 0xE0);
            W16(0x0FE0 + e) = (uint16_t)(rnd() % 0xA0);
        }
        W8(0x0E55) = (uint8_t)(1 + rnd() % 0x10);   /* the slide-in's countdown, from $10 */
        W8(0x0E5B) = (uint8_t)rnd();
        W8(0x0DD6) = (uint8_t)rnd();
        /* the sprite sizes the glyph program takes from $020E (across) and
           $0210 (down) are the ones the game's glyph code leaves there:
           $0010 or $0100 across, $0020, $0100 or $0200 down (a random size
           walks the program over the screen for far too long) */
        FXU(0x020E) = rnd() & 1 ? 0x0010 : 0x0100;
        FXU(0x0210) = (uint16_t)(rnd() % 3 == 0 ? 0x0020 : rnd() & 1 ? 0x0100 : 0x0200);
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_run(k ? old_abi_TimeUpPhaseSlideOut : old_abi_TimeUpPhaseSlideIn);
        if (k)
            time_up_phase_slide_out();
        else
            time_up_phase_slide_in();
        same_ram();
    }
    printf("player_stats_check: %ld cases, %ld differ\n", cases, failed);
    if (skipped)
        printf("player_stats_check: %ld cases skipped (a runaway on either side)\n", skipped);
    return failed != 0;
}
