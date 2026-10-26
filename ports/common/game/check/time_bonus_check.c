/* old: race/time_bonus.c gsu/run.c gsu/params.c gsu/screen.c gsu/programs.c render/unused.c render/matrix.c render/arith.c render/angle.c math/fx_math.c gsu/fx_init.c data/unpack.c render/sort.c render/hud.c render/points.c render/fx_points.c render/project.c render/table.c audio/sound_queue.c system/misc.c
 *
 * time_bonus_check.c: the port's race time and time bonus (race/time_bonus.c)
 * against the decomp's, on random states. The decomp's S-CPU code starts its
 * Super FX programs through the bus model (model_gsu_start); the port's
 * programs run natively. Work RAM and the game pak RAM are compared, but
 * the poll counts the original keeps while it waits ($FE-$FF, $1F21-$1F22).
 *
 *   check/check.sh time_bonus N
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
#include "time_bonus.h"
#include "model.h"

/* the work RAM the time and bonus code keeps (as race/time_bonus.c names them) */
#define race_time_bcd W16(0x0E70)
#define bonus_left W16(0x0E74)
#define bonus_shown W16(0x0E6C)
#define bonus_step W16(0x0E7C)
#define time_taken_back W16(0x0E76)
#define time_last_drawn W16(0x0E78)
#define large_digit_bits W8(0x0E69)
#define tally_state W16(0x0D65)
#define tally_frames W8(0x0D5A)
#define counting_flag W8(0x0D5B)
#define anim_step W16(0x0D6B)
#define anim_x W16(0x0D6D)
#define transfer_state W16(0x0D67)
#define transfer_size W16(0x0D69)
#define alt_bytes_flag W16(0x0D4C)
#define engine_phase W8(0x0DD6)

/* ---- the decomp's routines, as renamed ---- */
#define OLD(name) void old_abi_##name(void);
OLD(GsuTimeDigits) OLD(GsuTimeDigitsGrow) OLD(GsuTimeDigitsFull) OLD(GsuBonusDigitsGrow) OLD(GsuBonusDigitsBounce)
OLD(GsuBonusDigitsFull) OLD(TimeDigitBytes) OLD(GsuDigitPair0E74At) OLD(TimeBonusTally) OLD(TimeBonusTransfer)
OLD(TimeLeftDigits)
OLD(GsuRun) OLD(FxClearScreenIfSet) OLD(FxClear6000IfSet) OLD(FxMatrixFromAnglesProgram) OLD(FxInit)
OLD(FxUnpackProgram) OLD(FxLengthProgram) OLD(FxMatrixRotateProgram) OLD(FxAngleProgram) OLD(FxSortObjectsProgram)
OLD(FxNoiseFill) OLD(FxGlyph8x16) OLD(FxGlyph24x16) OLD(FxPictureRise40) OLD(FxPictureRise60) OLD(FxPictureRiseSplit40)
OLD(FxPictureRiseSplit60) OLD(FxGlyph16RowsFromTop) OLD(FxScaledSprite13) OLD(FxScaledSpriteEighth)
OLD(FxGlyph16RowsFromBottom) OLD(FxSprite24x40) OLD(FxScaledSprite12) OLD(FxGlyph8x8) OLD(FxGlyph24x32Bank04)
OLD(FxGlyph16x32) OLD(FxSprite16x32) OLD(FxGlyph16x16) OLD(FxScaledSpriteAdvance)

/* ---- inputs ---- */
static uint64_t rng = 0x3C6EF372FE94F82Bull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

/* ---- the decomp's Super FX programs, by where they start ---- */
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
    {0x07FAF2, old_abi_FxScaledSpriteAdvance},
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

/* ---- running both ---- */
static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration, skipped;

/* the tally's state in the case being run, for the report */
static long case_state = -1;

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 20)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld (state %ld): ", model_current, iteration, case_state);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* The game's values: a random BCD value of at most `top` (a binary value
   of 0 to top, as BCD), or one case in eight any 16-bit word. */
static uint16_t bcd_upto(unsigned top)
{
    if (!(rnd() & 7))
        return (uint16_t)rnd();
    return binary_to_bcd((uint16_t)(rnd() % (top + 1)));
}

/* A time: the race time's range, 0 to 999.99 in BCD. */
static uint16_t time_value(void)
{
    if (!(rnd() & 7))
        return (uint16_t)rnd();
    if (!(rnd() & 7))
        return (uint16_t)(rnd() % 0x100);
    return binary_to_bcd((uint16_t)(rnd() % 1000));
}

/* The bonus: a BCD value of $00 to $99, or, one case in eight, any word. */
static uint16_t bonus_value(void)
{
    return bcd_upto(99);
}

/* set while the decomp's side runs, to say which side ran away */
static int in_old;

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    model_pixels_empty();
    in_old = 1;
    f();
    in_old = 0;
    model_pixels_flush();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

/* The poll counts the decomp's wait keeps, and the programs' stack at
   $0240-$02A0, are not compared. The piece programs count in direct page
   $F1-$F4 (as gsu_check's piece cases leave out): scratch, set per case. */
static int scratch;

static int counted(size_t i)
{
    return i == 0xFE || i == 0xFF || i == 0x1F21 || i == 0x1F22 || (scratch && i >= 0xF1 && i <= 0xF4);
}

static sigjmp_buf escape;

static void runaway(int sig)
{
    (void)sig;
    siglongjmp(escape, 1);
}

/* a case either side runs away in (a loop on an input the game never
   gives) is skipped */
/* seconds a case may run (GUARD_SECS overrides the 5 of a normal run) */
static unsigned guard_secs(void)
{
    const char *s = getenv("GUARD_SECS");

    return s ? (unsigned)atoi(s) : 5;
}

#define GUARD if (sigsetjmp(escape, 1)) { skipped++; fprintf(stderr, "%s, case %ld: skipped, %s ran past %u s\n", model_current, iteration, in_old ? "the decomp" : "the port", guard_secs()); in_old = 0; alarm(0); continue; } alarm(guard_secs())

static void same_ram(void)
{
    size_t i;

    alarm(0);
    fx_pixels_flush();
    cases++;
    for (i = 0; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && !counted(i)) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0)) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
}

/* The game's sizes for the digit programs: the size tables' values
   ($08 to $20 in steps the tables give). */
static uint16_t size_value(void)
{
    static const uint16_t sizes[] = {0x0008, 0x0010, 0x0018, 0x001C, 0x0020};

    return sizes[rnd() % 5];
}

/* The game's animation steps ($0D6B): $FFFE to start, then $00 to $08. */
static uint16_t anim_value(void)
{
    static const uint16_t steps[] = {0xFFFE, 0x0000, 0x0002, 0x0004, 0x0006, 0x0008};

    return steps[rnd() % 6];
}

/* The sound queue and the player, as the game keeps them: fresh() sets
   the ring's head and count and the mute and player bytes at random. */
static void fresh(void)
{
    size_t i;

    scratch = 0;
    case_state = -1;
    for (i = 0; i < sizeof wram; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    /* the data bank: a bank with work RAM at $0000 and the tables in ROM
       at $8000 and up, as the digit tables are read through it. The ROM is
       1 MB (banks $00-$1F; the model does not mirror it past that). */
    old_cpu.db = (uint8_t)(rnd() % 0x20);
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
    memset(&old_gsu, 0, sizeof old_gsu);
    /* the game's screens: the race's at $2C00 (4 or 8 bits a pixel), the
       menus' at $9400 (4 bits) */
    old_gsu.scbr = fx_screen.base = (uint8_t)(rnd() & 1 ? 0x0B : 0x25);
    W8(0x0374) = fx_screen.base == 0x25 || (rnd() & 1) ? 0x01 : 0x03;
    fx_screen.mode = 0, fx_screen.height = 0;
    {
        static const uint8_t zero[8];

        fx_pixels_set(0, 0xFFFF, 0, zero);
        fx_pixels_set(1, 0xFFFF, 0, zero);
    }
    /* the ring's head is an index into its 16 bytes; the count 0 to 16 (the
       game's values; a larger count queues nothing) */
    sound_port0_head = (uint8_t)(rnd() % 16);
    sound_port0_count = (uint8_t)rnd();
    sound_muted = (rnd() & 3) ? 0 : (uint8_t)rnd();
    current_player = (uint8_t)(rnd() & 1);
}

/* The tally's variables in states the game reaches: the state (0, 2, ...
   24), the animation step, the bonus, the time and the counters. */
static void tally_shape(void)
{
    tally_state = (uint16_t)(2 * (rnd() % 13));
    /* the animation step: $FFFE only into state 4 (the step is added before
       the draw); otherwise the game's steps $00 to $08 */
    anim_step = (tally_state == 4 && (rnd() & 1)) ? 0xFFFE : (uint16_t)(2 * (rnd() % 5));
    /* $0D6D: the game's $30 and $37, or the scale table's entries for the
       bonus's size ($8F less the table at $07:9806) */
    anim_x = (rnd() & 1) ? (uint16_t)(0x0030 + (rnd() & 1) * 7) : (uint16_t)(0x008F - rom_word(0x079806u + 2 * (rnd() % 5)));
    bonus_shown = (rnd() & 1) ? 0 : bonus_value();
    bonus_left = bonus_value();
    bonus_step = (uint16_t)(rnd() % 5);
    time_taken_back = time_value();
    time_last_drawn = time_value();
    race_time_bcd = time_value();
    tally_frames = (uint8_t)(rnd() % 12);
    /* the piece counter the game steps from 0 to $14 */
    W16(0x10CB) = (uint16_t)(rnd() % 0x15);
    W16(0x0D5B) = (uint16_t)(rnd() % 3 ? rnd() % 3 : rnd());
    alt_bytes_flag = (rnd() & 3) ? 0 : (uint16_t)rnd();
    wram_19F0 = (rnd() & 3) ? 0 : bcd_upto(99);
    wram_19EE = (rnd() & 1) ? bcd_upto(99) : bonus_left;
    large_digit_bits = (uint8_t)rnd();
}

/* The transfer's variables: the counting flag, the bonus, the time, and the
   transfer's digit step and size (4 to $20 in steps of 4, or 0 when the
   shrink step may run out). */
static void transfer_shape(void)
{
    tally_state = (uint16_t)(2 * (rnd() % 13));
    counting_flag = (uint8_t)((rnd() & 3) ? 1 : rnd());
    wram_19F0 = (rnd() & 3) ? 0 : bcd_upto(99);
    wram_19EE = (rnd() & 1) ? bcd_upto(99) : bonus_left;
    bonus_left = bonus_value();
    race_time_bcd = time_value();
    transfer_state = (uint16_t)(2 * (1 + rnd() % 3));
    transfer_size = (uint16_t)(4 * (1 + rnd() % 8));
    if (transfer_state == 6 && !(rnd() & 3))
        transfer_size = 0;
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: time_bonus_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_gsu_start = old_start;
    signal(SIGALRM, runaway);

    /* The state table: the tally's state k (2k) jumps to its code; the
       transfer's table holds the digit step's code at $07:8C43. */
    {
        static const uint16_t labels[13] = {0x8ED1, 0x8EB9, 0x8ED3, 0x8F27, 0x8F86, 0x8FD9, 0x900E,
                                            0x904A, 0x90A1, 0x9116, 0x913E, 0x9151, 0x9165};
        unsigned k;

        for (k = 0; k < 13; k++)
            if (rom_word(0x078E9F + 2 * k) != labels[k]) {
                fprintf(stderr, "check: the tally's state %u at $07:8E9F is $%04X, not $%04X\n", 2 * k,
                        rom_word(0x078E9F + 2 * k), labels[k]);
                return 2;
            }
        if (rom_word(0x078C43 + 2) != 0x8C4B || rom_word(0x078C43 + 4) != 0x8C73 || rom_word(0x078C43 + 6) != 0x8C67) {
            fprintf(stderr, "check: the transfer table at $07:8C43 is not grow, draw, shrink\n");
            return 2;
        }
    }

    CASE("GsuTimeDigits") {
        uint8_t bank, mode;

        fresh();
        bank = old_cpu.db;
        W16(0x0E70) = time_value();
        FXU(0x0210) = size_value();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        GUARD;
        old_run(old_abi_GsuTimeDigits);
        mode = draw_race_time(bank);
        if (mode != (uint8_t)old_cpu.a)
            fail("screen mode %02X, the decomp's %02X", mode, (uint8_t)old_cpu.a);
        same_ram();
    }
    CASE("GsuTimeDigitsGrow") {
        uint8_t bank, mode;

        fresh();
        bank = old_cpu.db;
        W16(0x0E70) = time_value();
        anim_step = anim_value();
        if (anim_step == 0xFFFE)
            anim_step = 0x0000;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        GUARD;
        old_run(old_abi_GsuTimeDigitsGrow);
        mode = draw_race_time_grow(bank);
        if (mode != (uint8_t)old_cpu.a)
            fail("screen mode %02X, the decomp's %02X", mode, (uint8_t)old_cpu.a);
        same_ram();
    }
    CASE("GsuTimeDigitsFull") {
        uint8_t bank, mode;

        fresh();
        bank = old_cpu.db;
        W16(0x0E70) = time_value();
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        GUARD;
        old_run(old_abi_GsuTimeDigitsFull);
        mode = draw_race_time_full(bank);
        if (mode != (uint8_t)old_cpu.a)
            fail("screen mode %02X, the decomp's %02X", mode, (uint8_t)old_cpu.a);
        same_ram();
    }
    CASE("GsuBonusDigitsGrow") {
        uint8_t bank = 0;

        fresh();
        bank = old_cpu.db;
        anim_step = anim_value();
        if (anim_step == 0xFFFE)
            anim_step = 0x0000;
        bonus_shown = (rnd() & 3) ? bonus_value() | 1 : 0;
        bonus_left = bonus_value();
        GUARD;
        old_run(old_abi_GsuBonusDigitsGrow);
        draw_bonus_digits_grow(bank);
        same_ram();
    }
    CASE("GsuBonusDigitsBounce") {
        uint8_t bank;

        fresh();
        bank = old_cpu.db;
        anim_step = (uint16_t)(2 * (rnd() % 8));
        bonus_shown = (rnd() & 3) ? bonus_value() | 1 : 0;
        bonus_left = bonus_value();
        GUARD;
        old_run(old_abi_GsuBonusDigitsBounce);
        draw_bonus_digits_bounce(bank);
        same_ram();
    }
    CASE("GsuBonusDigitsFull") {
        uint8_t bank;

        fresh();
        bank = old_cpu.db;
        bonus_shown = (rnd() & 3) ? bonus_value() | 1 : 0;
        bonus_left = bonus_value();
        GUARD;
        old_run(old_abi_GsuBonusDigitsFull);
        draw_bonus_digits_full(bank);
        same_ram();
    }
    CASE("TimeDigitBytes") {
        fresh();
        alt_bytes_flag = (rnd() & 1) ? 0 : (uint16_t)rnd();
        time_taken_back = (uint16_t)(rnd() % 0x48);
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        GUARD;
        old_run(old_abi_TimeDigitBytes);
        time_digit_bytes();
        same_ram();
    }
    CASE("GsuDigitPair0E74At") {
        uint8_t bank;
        uint16_t x = (uint16_t)(rnd() % 0x100);

        fresh();
        bank = old_cpu.db;
        bonus_left = bonus_value();
        FXU(0x020E) = 0x0010;
        FXU(0x0210) = size_value();
        old_cpu.a = x;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        GUARD;
        old_run(old_abi_GsuDigitPair0E74At);
        draw_bonus_digits_at(bank, x);
        same_ram();
    }
    CASE("TimeBonusTally") {
        uint8_t bank;

        fresh();
        /* the data bank $07, where the game keeps the tables the frame's
           pieces read (the port's piece helper reads its table there) */
        old_cpu.db = 0x07;
        bank = old_cpu.db;
        tally_shape();
        case_state = tally_state;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        GUARD;
        scratch = 1;
        old_run(old_abi_TimeBonusTally);
        time_bonus_tally(bank);
        same_ram();
        scratch = 0;
    }
    CASE("TimeBonusTransfer") {
        uint8_t bank;

        fresh();
        bank = old_cpu.db;
        transfer_shape();
        GUARD;
        old_run(old_abi_TimeBonusTransfer);
        time_bonus_transfer(bank);
        same_ram();
    }
    CASE("TimeLeftDigits") {
        uint8_t bank;

        fresh();
        bank = old_cpu.db;
        counting_flag = (uint8_t)((rnd() & 3) ? 1 : 0);
        engine_phase = (rnd() & 7) ? (uint8_t)rnd() : 0x0C;
        race_time_bcd = time_value();
        time_last_drawn = (rnd() & 1) ? race_time_bcd : time_value();
        /* the large digit's entry: the high nibble picks one of the ten
           entries of the table at $07:8DDA (8 bytes each); entries 10 up
           are the next table's words, not sizes, so the nibble is 0 to 9 */
        large_digit_bits = (uint8_t)((rnd() % 10) << 4 | (rnd() & 0x0F));
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        GUARD;
        old_run(old_abi_TimeLeftDigits);
        time_left_digits(bank);
        same_ram();
    }

    printf("time_bonus_check: %ld cases, %ld differ, %ld skipped (ran away)\n", cases, failed, skipped);
    return failed != 0;
}
