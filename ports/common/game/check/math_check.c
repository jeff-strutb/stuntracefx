/*
 * math_check.c: the port's maths (fixmath.h) against the decomp's (src/
 * stuntracefx/math), on random and edge inputs.
 *
 * The decomp's routines are built with every symbol renamed old_*, and run
 * here on a small model of what they touch: work RAM, the ROM, the Super FX
 * RAM and registers, and the console's multipliers and divider. Time is
 * ignored. For each routine the outputs that carry meaning (results, moved
 * or not, the state it keeps) are compared; scratch it leaves behind and
 * exit flags are not. Exit 0 when every case agrees.
 *
 *   math_check ROM [N]       N cases per routine (default 200000)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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
#include "gsu.h"
#undef cpu
#undef gsu

#include "fixmath.h"

const uint8_t *srfx_rom_image;

/* ---- the model the decomp's routines run on ---- */
Regs65816 old_cpu;
RegsGSU old_gsu;
static uint8_t wram[0x20000], fxram[0x20000];
static uint8_t m7_latch, m7b, wrmpya;
static uint16_t m7a, wrdiv, rddiv, rdmpy;

static long cases, failed;
static const char *current;

static void die(const char *what, uint32_t a)
{
    fprintf(stderr, "math_check: %s: the model has no %s at $%06X\n", current, what, a);
    exit(2);
}

uint8_t old_rd8(uint32_t a)
{
    uint8_t bank = (uint8_t)(a >> 16);
    uint16_t o = (uint16_t)a;
    int32_t p;

    if (bank == 0x7E || bank == 0x7F)
        return wram[a - 0x7E0000];
    if ((bank & 0x7F) >= 0x40)
        die("read", a);
    if (o < 0x2000)
        return wram[o];
    if (o >= 0x8000)
        return srfx_rom_image[(bank & 0x3F) * 0x8000 + (o & 0x7FFF)];
    p = (int32_t)(int16_t)m7a * (int8_t)m7b;
    switch (o) {
    case 0x2134: return (uint8_t)p;
    case 0x2135: return (uint8_t)(p >> 8);
    case 0x2136: return (uint8_t)(p >> 16);
    case 0x4214: return (uint8_t)rddiv;
    case 0x4215: return (uint8_t)(rddiv >> 8);
    case 0x4216: return (uint8_t)rdmpy;
    case 0x4217: return (uint8_t)(rdmpy >> 8);
    }
    die("read", a);
    return 0;
}

void old_wr8(uint32_t a, uint8_t v)
{
    uint8_t bank = (uint8_t)(a >> 16);
    uint16_t o = (uint16_t)a;

    if (bank == 0x7E || bank == 0x7F) {
        wram[a - 0x7E0000] = v;
        return;
    }
    if ((bank & 0x7F) >= 0x40 || o >= 0x8000)
        die("write", a);
    if (o < 0x2000) {
        wram[o] = v;
        return;
    }
    switch (o) {
    case 0x211B: m7a = (uint16_t)(v << 8 | m7_latch); m7_latch = v; return;
    case 0x211C: m7b = v; m7_latch = v; return;
    case 0x4202: wrmpya = v; return;
    case 0x4203: rdmpy = (uint16_t)(wrmpya * v); return;
    case 0x4204: wrdiv = (uint16_t)((wrdiv & 0xFF00) | v); return;
    case 0x4205: wrdiv = (uint16_t)((wrdiv & 0x00FF) | v << 8); return;
    case 0x4206:
        rddiv = v ? (uint16_t)(wrdiv / v) : 0xFFFF;
        rdmpy = v ? (uint16_t)(wrdiv % v) : wrdiv;
        return;
    }
    die("write", a);
}

uint8_t old_gsu_rd8(uint32_t a)
{
    uint8_t bank = (uint8_t)(a >> 16);

    if (bank < 0x60)
        return srfx_rom_image[(bank & 0x3F) * 0x8000 + (a & 0x7FFF)];
    if (bank == 0x70 || bank == 0x71)
        return fxram[a - 0x700000];
    die("Super FX read", a);
    return 0;
}

void old_gsu_wr8(uint32_t a, uint8_t v)
{
    if ((a >> 16) == 0x70 || (a >> 16) == 0x71)
        fxram[a - 0x700000] = v;
    else
        die("Super FX write", a);
}

void old_cyc(uint32_t c) { (void)c; }
void old_cpu_idle(uint32_t c) { (void)c; }
void old_gsu_code(uint32_t a, uint32_t n) { (void)a; (void)n; }
void old_gsu_cache(uint16_t r15) { (void)r15; }
void old_gsu_cyc(uint32_t c) { (void)c; }
void old_gsu_time(int w) { (void)w; }
void old_gsu_buffer(int w) { (void)w; }
void old_gsu_plot(uint8_t x, uint8_t y) { (void)x; (void)y; die("plot", 0); }
uint8_t old_gsu_rpix(uint8_t x, uint8_t y) { (void)x; (void)y; die("rpix", 0); return 0; }

/* ---- the decomp's routines, as renamed ---- */
#define OLD(name) void old_abi_##name(void);
#define ROUTINES(X) \
    X(ApproachHalfMin2) X(ApproachQuarterMin4) X(ApproachEighthMin8) X(ApproachSixteenthMin16) \
    X(ApproachThirtySecondMin32) X(ApproachSixtyFourthMin64) X(ApproachOneTwentyEighthMin128) \
    X(ApproachHalfMin2Byte) X(ApproachQuarterMin4Byte) X(ApproachEighthMin8Byte) X(ApproachSixteenthMin16Byte) \
    X(ApproachThirtySecondMin32Byte) X(ApproachSixtyFourthMin64Byte) X(ApproachOneTwentyEighthMin128Byte) \
    X(ScaleQuarter) X(ScaleThreeQuarters) X(ScaleNineSixteenths) X(ScaleFiveEighths) X(ScaleSevenEighths) \
    X(ScaleFifteenSixteenths) X(ScaleFiveThirtySeconds) X(ScaleDp02Dp94) X(ScaleObjectXZ) X(ScaleDp02Dp94Long) \
    X(Mode7ScaleProduct) X(BlendStepUp) X(BlendStepDown) \
    X(RandomStep) X(RandomStepLong) X(RandomWordStep) \
    X(PpuMultiplierSetLowByteTimes8) X(PpuMultiply) \
    X(RotateXZ) X(RotateXZFrom08) X(RotateXY) X(MulWordByte) X(MulWordByteLong) X(RotateXZWide) \
    X(RotateXZWideFrom08) \
    X(HeadingFlat) X(HeadingFlatMirrored) X(HeadingPitched) X(HeadingPitchedVia1F7C) X(HeadingPitchedFrom1F7C) \
    X(HeadingFlatQuarterTurn) X(HeadingFlatSpeedA) X(ObjectMoveByVector) X(ObjectMoveByVectorTimes2) \
    X(ObjectMoveByVectorTimes4) X(ObjectVelocityFromHeading) X(ObjectVelocityFromHeadingTimes2) \
    X(ObjectVelocityFromHeadingTimes4) X(ObjectVelocityFromHeadingTimes8) X(ObjectVelocityLevelFromDp) \
    X(FxDivide) X(FxDivideHalvedIfLarge) X(FxSquareRoot) X(FxNormalize) X(FxAngleVector) X(FxTurn) \
    X(FxMatrixTranspose04) X(FxMatrixFromAngles) X(FxAngleOf)
ROUTINES(OLD)

/* ---- inputs ---- */
static uint64_t rng = 0x9E3779B97F4A7C15ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

/* a word, one time in four an edge of its range */
static uint16_t w16(void)
{
    static const uint16_t edge[] = {0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, 0x7FFE, 0x7FFF, 0x8000, 0x8001, 0xFF80, 0xFFFE, 0xFFFF};

    return (rnd() & 3) ? (uint16_t)rnd() : edge[rnd() % (sizeof edge / sizeof edge[0])];
}

static uint8_t b8(void)
{
    static const uint8_t edge[] = {0, 1, 2, 0x3F, 0x40, 0x7F, 0x80, 0x81, 0xC0, 0xFE, 0xFF};

    return (rnd() & 3) ? (uint8_t)rnd() : edge[rnd() % (sizeof edge / sizeof edge[0])];
}

static uint16_t rw(uint32_t a) { return (uint16_t)(old_rd8(a) | old_rd8(a + 1) << 8); }
static void ww(uint32_t a, uint16_t v) { old_wr8(a, (uint8_t)v); old_wr8(a + 1, (uint8_t)(v >> 8)); }
static uint16_t fw(uint16_t a) { return (uint16_t)(fxram[a] | fxram[a ^ 1] << 8); }
static void fww(uint16_t a, uint16_t v) { fxram[a] = (uint8_t)v; fxram[a ^ 1] = (uint8_t)(v >> 8); }

/* a fresh state: work RAM and registers random, as the game never leaves
   them in any particular state for these routines */
static void fresh(void)
{
    size_t i;

    for (i = 0; i < 0x400; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0x0600; i < 0x0640; i++)
        wram[i] = (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = w16();
    old_cpu.x = w16();
    old_cpu.y = w16();
    old_cpu.p = (uint8_t)(rnd() & ~P_D);
    memset(&old_gsu, 0, sizeof old_gsu);
    for (i = 0; i < 16; i++)
        old_gsu.r[i] = w16();
    old_gsu.r[10] = (uint16_t)(0x1000 + (rnd() & 0x0FFE));    /* the R10 stack, in RAM */
}

/* ---- the comparisons ---- */

static void expect(int ok, const char *fmt, ...)
{
    cases++;
    if (ok)
        return;
    if (++failed <= 20) {
        va_list ap;
        va_start(ap, fmt);
        fprintf(stderr, "%s: ", current);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
        va_end(ap);
    }
}

static void check_approach(long n)
{
    static const struct { void (*f)(void); const char *name; int wide; uint16_t min; int halvings; } t[] = {
        {old_abi_ApproachHalfMin2, "ApproachHalfMin2", 1, 2, 1},
        {old_abi_ApproachQuarterMin4, "ApproachQuarterMin4", 1, 4, 2},
        {old_abi_ApproachEighthMin8, "ApproachEighthMin8", 1, 8, 3},
        {old_abi_ApproachSixteenthMin16, "ApproachSixteenthMin16", 1, 16, 4},
        {old_abi_ApproachThirtySecondMin32, "ApproachThirtySecondMin32", 1, 32, 5},
        {old_abi_ApproachSixtyFourthMin64, "ApproachSixtyFourthMin64", 1, 64, 6},
        {old_abi_ApproachOneTwentyEighthMin128, "ApproachOneTwentyEighthMin128", 1, 128, 7},
        {old_abi_ApproachHalfMin2Byte, "ApproachHalfMin2Byte", 0, 2, 1},
        {old_abi_ApproachQuarterMin4Byte, "ApproachQuarterMin4Byte", 0, 4, 2},
        {old_abi_ApproachEighthMin8Byte, "ApproachEighthMin8Byte", 0, 8, 3},
        {old_abi_ApproachSixteenthMin16Byte, "ApproachSixteenthMin16Byte", 0, 16, 4},
        {old_abi_ApproachThirtySecondMin32Byte, "ApproachThirtySecondMin32Byte", 0, 32, 5},
        {old_abi_ApproachSixtyFourthMin64Byte, "ApproachSixtyFourthMin64Byte", 0, 64, 6},
        {old_abi_ApproachOneTwentyEighthMin128Byte, "ApproachOneTwentyEighthMin128Byte", 0, 128, 7},
    };
    size_t k;
    long i;

    for (k = 0; k < sizeof t / sizeof t[0]; k++) {
        current = t[k].name;
        for (i = 0; i < n; i++) {
            uint16_t value, target, got, want;
            bool moved;

            fresh();
            target = old_cpu.a;
            value = (rnd() & 7) ? w16() : (uint16_t)(target + (rnd() & 0x3F) - 0x20);
            ww(0x3E, value);
            t[k].f();
            if (t[k].wide) {
                got = rw(0x3E);
                want = (uint16_t)approach16((int16_t)value, (int16_t)target, t[k].min, t[k].halvings, &moved);
            } else {
                got = old_rd8(0x3E);
                want = approach8((uint8_t)value, (uint8_t)target, (uint8_t)t[k].min, t[k].halvings, &moved);
            }
            expect(got == want && !(old_cpu.p & P_C) == moved, "value %04X target %04X: %04X moved %d, want %04X %d",
                   value, target, got, !(old_cpu.p & P_C), want, moved);
        }
    }
}

static void check_scale(long n)
{
    static const struct { void (*f)(void); const char *name; int16_t (*g)(int16_t); } t[] = {
        {old_abi_ScaleQuarter, "ScaleQuarter", scale_1_4},
        {old_abi_ScaleThreeQuarters, "ScaleThreeQuarters", scale_3_4},
        {old_abi_ScaleNineSixteenths, "ScaleNineSixteenths", scale_9_16},
        {old_abi_ScaleFiveEighths, "ScaleFiveEighths", scale_5_8},
        {old_abi_ScaleSevenEighths, "ScaleSevenEighths", scale_7_8},
        {old_abi_ScaleFifteenSixteenths, "ScaleFifteenSixteenths", scale_15_16},
        {old_abi_ScaleFiveThirtySeconds, "ScaleFiveThirtySeconds", scale_5_32},
    };
    size_t k;
    long i;

    for (k = 0; k < sizeof t / sizeof t[0]; k++) {
        current = t[k].name;
        for (i = 0; i < n; i++) {
            uint16_t v;

            fresh();
            old_cpu.db = (rnd() & 1) ? 0x7E : 0x00;
            v = old_cpu.a;
            t[k].f();
            expect(old_cpu.a == (uint16_t)t[k].g((int16_t)v), "%04X: %04X, want %04X", v, old_cpu.a,
                   (uint16_t)t[k].g((int16_t)v));
        }
    }

    current = "ScaleDp02Dp94 / ScaleObjectXZ / ScaleDp02Dp94Long";
    for (i = 0; i < n; i++) {
        int16_t x, z;
        uint16_t key;
        int which = (int)(rnd() % 3);

        fresh();
        if (which == 1) {
            old_cpu.x = (uint16_t)(rnd() & 0x1FFE);
            ww(0x7E200C + old_cpu.x, w16());
            ww(0x7E2010 + old_cpu.x, w16());
            x = (int16_t)rw(0x7E200C + old_cpu.x);
            z = (int16_t)rw(0x7E2010 + old_cpu.x);
            old_abi_ScaleObjectXZ();
        } else {
            x = (int16_t)rw(0x02);
            z = (int16_t)rw(0x94);
            which ? old_abi_ScaleDp02Dp94Long() : old_abi_ScaleDp02Dp94();
        }
        key = xz_grid_key(&x, &z);
        expect(old_cpu.a == key && rw(0x02) == (uint16_t)x && rw(0x94) == (uint16_t)z, "key %04X (%04X %04X), want %04X (%04X %04X)",
               old_cpu.a, rw(0x02), rw(0x94), key, (uint16_t)x, (uint16_t)z);
    }

    current = "Mode7ScaleProduct";
    for (i = 0; i < n; i++) {
        uint16_t v, want;
        int pos;

        fresh();
        wram[0x13408] = b8();
        wram[0x1340A] = b8();
        v = old_cpu.a;
        pos = old_cpu.p & P_C;
        old_abi_Mode7ScaleProduct();
        want = mode7_scale_product(v, pos, wram[0x1340A], (int8_t)wram[0x13408]);
        expect(old_cpu.a == want, "%04X %d / %02X * %02X: %04X, want %04X", v, pos, wram[0x1340A], wram[0x13408],
               old_cpu.a, want);
    }

    for (int up = 0; up < 2; up++) {
        current = up ? "BlendStepUp" : "BlendStepDown";
        for (i = 0; i < n; i++) {
            uint16_t factor, limit, base[4], target[4], out[4], was[4];
            bool stepped;
            int j, ok;

            fresh();
            limit = (rnd() & 1) ? b8() : w16();
            factor = (rnd() & 3) ? (uint16_t)(rnd() % ((limit & 0xFF) + 1)) : (rnd() & 1) ? limit : 0;
            ww(0x7F3408, factor);
            ww(0x7F340A, limit);
            for (j = 0; j < 4; j++) {
                base[j] = w16();
                target[j] = w16();
                ww(0x7F3442 + 2 * j, base[j]);
                ww(0x7F343A + 2 * j, target[j]);
                was[j] = out[j] = rw(0x7F3432 + 2 * j);
            }
            up ? old_abi_BlendStepUp() : old_abi_BlendStepDown();
            stepped = blend_step(&factor, limit, up, base, target, out);
            ok = rw(0x7F3408) == factor;
            (void)stepped;
            for (j = 0; j < 4; j++)
                ok &= rw(0x7F3432 + 2 * j) == out[j];
            expect(ok, "factor %04X limit %04X: factor %04X, want %04X; word 0 %04X (was %04X), want %04X", factor,
                   limit, rw(0x7F3408), factor, rw(0x7F3432), was[0], out[0]);
        }
    }
}

static void check_random(long n)
{
    long i;

    current = "RandomStep / RandomStepLong";
    for (i = 0; i < n; i++) {
        uint8_t s[4], r;
        int j;

        fresh();
        for (j = 0; j < 4; j++)
            s[j] = wram[0xE6 + j] = b8();
        (i & 1) ? old_abi_RandomStepLong() : old_abi_RandomStep();
        r = random_step(s);
        expect((uint8_t)old_cpu.a == r && !memcmp(s, wram + 0xE6, 4), "a %02X want %02X", (uint8_t)old_cpu.a, r);
    }
    current = "RandomWordStep";
    for (i = 0; i < 0x10000; i++) {             /* every state */
        uint16_t s, r;

        fresh();
        old_cpu.db = (rnd() & 1) ? 0x7E : 0x00;
        s = (uint16_t)i;
        ww(0x0D18, s);
        old_abi_RandomWordStep();
        r = random_word_step(&s);
        expect(old_cpu.a == r && rw(0x0D18) == s, "a %04X state %04X, want %04X %04X", old_cpu.a, rw(0x0D18), r, s);
    }
}

static void check_multiply(long n)
{
    long i;

    current = "PpuMultiplierSetLowByteTimes8 + PpuMultiply";
    for (i = 0; i < n; i++) {
        uint8_t v = b8(), b = b8();
        int16_t a;

        fresh();
        if (i & 1) {
            old_cpu.a = v;
            old_abi_PpuMultiplierSetLowByteTimes8();
            a = (int16_t)(v << 3);
        } else {
            a = (int16_t)w16();
            old_wr8(0x211B, (uint8_t)a);
            old_wr8(0x211B, (uint8_t)(a >> 8));
        }
        old_cpu.a = b;
        old_abi_PpuMultiply();
        expect(old_cpu.a == (uint16_t)mul16x8(a, (int8_t)b), "%04X * %02X: %04X, want %04X", (uint16_t)a, b, old_cpu.a,
               (uint16_t)mul16x8(a, (int8_t)b));
    }

    current = "MulWordByte / MulWordByteLong";
    for (i = 0; i < n; i++) {
        uint16_t v = w16();
        uint8_t s = b8();
        int16_t want = mul_word_byte((int16_t)v, (int8_t)s);

        fresh();
        old_cpu.db = 0;
        ww(0x67, v);
        wram[0x69] = s;
        (i & 1) ? old_abi_MulWordByteLong() : old_abi_MulWordByte();
        expect(rw(0x6A) == (uint16_t)want && old_cpu.a == (uint16_t)want, "%04X * %02X: %04X, want %04X", v, s, rw(0x6A),
               (uint16_t)want);
    }
}

static void check_rotate(long n)
{
    static const struct { void (*f)(void); const char *name; uint16_t x, v, xo, vo; int neg; } t[] = {
        {old_abi_RotateXZ, "RotateXZ", 0x02, 0x94, 0x04, 0x5F2, 1},
        {old_abi_RotateXZFrom08, "RotateXZFrom08", 0x08, 0x94, 0x0A, 0x5F2, 0},
        {old_abi_RotateXY, "RotateXY", 0x02, 0x08, 0x04, 0x0A, 0},
    };
    size_t k;
    long i;

    for (k = 0; k < sizeof t / sizeof t[0]; k++) {
        current = t[k].name;
        for (i = 0; i < n; i++) {
            uint8_t x, v, angle;
            int16_t xo, vo;

            fresh();
            x = b8();
            v = b8();
            wram[t[k].x] = x;
            wram[t[k].v] = v;
            angle = (uint8_t)old_cpu.a;
            t[k].f();
            rotate8((int8_t)x, (int8_t)v, (uint8_t)(t[k].neg ? -angle : angle), &xo, &vo);
            expect(rw(t[k].xo) == (uint16_t)xo && rw(t[k].vo) == (uint16_t)vo, "%02X %02X by %02X: %04X %04X, want %04X %04X",
                   x, v, angle, rw(t[k].xo), rw(t[k].vo), (uint16_t)xo, (uint16_t)vo);
        }
    }

    for (k = 0; k < 2; k++) {
        current = k ? "RotateXZWideFrom08" : "RotateXZWide";
        for (i = 0; i < n; i++) {
            uint16_t x = w16(), z = w16();
            uint8_t angle;
            int16_t xo, zo;

            fresh();
            ww(k ? 0x08 : 0x02, x);
            ww(0x94, z);
            angle = (uint8_t)old_cpu.a;
            k ? old_abi_RotateXZWideFrom08() : old_abi_RotateXZWide();
            rotate16((int16_t)x, (int16_t)z, (uint8_t)(k ? -angle : angle), &xo, &zo);
            expect(rw(k ? 0x0A : 0x04) == (uint16_t)xo && rw(0x5F2) == (uint16_t)zo, "%04X %04X by %02X: %04X %04X, want %04X %04X",
                   x, z, angle, rw(k ? 0x0A : 0x04), rw(0x5F2), (uint16_t)xo, (uint16_t)zo);
        }
    }
}

static int same_vec(uint32_t xa, uint32_t ya, uint32_t za, Vec3s v)
{
    return rw(xa) == (uint16_t)v.x && rw(ya) == (uint16_t)v.y && rw(za) == (uint16_t)v.z;
}

static void check_heading(long n)
{
    long i;
    int k;

    for (k = 0; k < 7; k++) {
        static const char *names[] = {"HeadingFlat", "HeadingFlatMirrored", "HeadingPitched", "HeadingPitchedVia1F7C",
                                      "HeadingPitchedFrom1F7C", "HeadingFlatQuarterTurn", "HeadingFlatSpeedA"};
        current = names[k];
        for (i = 0; i < n; i++) {
            uint32_t o;
            uint8_t speed, h, p;
            Vec3s want;

            fresh();
            old_cpu.db = 0x7E;
            old_cpu.x = (uint16_t)(rnd() & 0x1FFF);
            o = 0x7E0000 + old_cpu.x;
            speed = b8();
            h = b8();
            p = b8();
            wram[0x2017 + old_cpu.x] = speed;
            wram[0x2015 + old_cpu.x] = h;
            wram[0x2014 + old_cpu.x] = p;
            (void)o;
            switch (k) {
            case 0: old_abi_HeadingFlat(); want = heading_vector((int8_t)speed, h); break;
            case 1: old_abi_HeadingFlatMirrored(); want = heading_vector((int8_t)speed, (uint8_t)(1 - h)); break;
            case 2: old_abi_HeadingPitched(); want = heading_vector_pitched((int8_t)speed, h, (uint8_t)-p); break;
            case 3: old_abi_HeadingPitchedVia1F7C(); want = heading_vector_pitched((int8_t)speed, (uint8_t)-h, p); break;
            case 4:
                old_cpu.db = 0;
                wram[0x82] = speed;
                wram[0x1F7D] = h;
                wram[0x1F7C] = p;
                old_abi_HeadingPitchedFrom1F7C();
                want = heading_vector_pitched((int8_t)speed, (uint8_t)-h, p);
                break;
            case 5:
                old_cpu.a = speed;
                old_abi_HeadingFlatQuarterTurn();
                want = heading_vector((int8_t)speed, (uint8_t)(0x41 - h));
                break;
            default:
                old_cpu.a = speed;
                old_abi_HeadingFlatSpeedA();
                want = heading_vector((int8_t)speed, (uint8_t)(h + 1));
                break;
            }
            expect(same_vec(0x02, 0x08, 0x94, want), "speed %02X h %02X p %02X: %04X %04X %04X, want %04X %04X %04X", speed, h,
                   p, rw(0x02), rw(0x08), rw(0x94), (uint16_t)want.x, (uint16_t)want.y, (uint16_t)want.z);
        }
    }

    for (k = 0; k < 5; k++) {
        static const char *names[] = {"ObjectVelocityFromHeading", "ObjectVelocityFromHeadingTimes2",
                                      "ObjectVelocityFromHeadingTimes4", "ObjectVelocityFromHeadingTimes8",
                                      "ObjectVelocityLevelFromDp"};
        static void (*const f[])(void) = {old_abi_ObjectVelocityFromHeading, old_abi_ObjectVelocityFromHeadingTimes2,
                                          old_abi_ObjectVelocityFromHeadingTimes4, old_abi_ObjectVelocityFromHeadingTimes8,
                                          old_abi_ObjectVelocityLevelFromDp};
        current = names[k];
        for (i = 0; i < n; i++) {
            uint32_t vel;
            Vec3s want;

            fresh();
            old_cpu.db = 0x7E;
            old_cpu.x = (uint16_t)(rnd() & 0x1F00);
            vel = 0x7E202E + old_cpu.x;
            wram[0x82] = b8();
            wram[0x1F7D] = b8();
            wram[0x1F7C] = b8();
            if (k < 4) {
                want = vec3_shl(heading_vector_pitched((int8_t)wram[0x82], (uint8_t)-wram[0x1F7D], wram[0x1F7C]), k);
            } else {
                want = heading_vector_pitched((int8_t)wram[0x94], 0xC0, wram[0x02]);
                want.z = 0;
            }
            f[k]();
            expect(same_vec(vel, vel + 2, vel + 4, want), "%04X %04X %04X, want %04X %04X %04X", rw(vel), rw(vel + 2),
                   rw(vel + 4), (uint16_t)want.x, (uint16_t)want.y, (uint16_t)want.z);
        }
    }

    for (k = 0; k < 3; k++) {
        static const char *names[] = {"ObjectMoveByVector", "ObjectMoveByVectorTimes2", "ObjectMoveByVectorTimes4"};
        static void (*const f[])(void) = {old_abi_ObjectMoveByVector, old_abi_ObjectMoveByVectorTimes2,
                                          old_abi_ObjectMoveByVectorTimes4};
        current = names[k];
        for (i = 0; i < n; i++) {
            uint32_t pos;
            Vec3s d, want;

            fresh();
            old_cpu.db = 0x7E;
            old_cpu.x = (uint16_t)(rnd() & 0x1F00);
            pos = 0x7E200C + old_cpu.x;
            ww(pos, w16());
            ww(pos + 2, w16());
            ww(pos + 4, w16());
            d.x = (int16_t)rw(0x02);
            d.y = (int16_t)rw(0x08);
            d.z = (int16_t)rw(0x94);
            d = vec3_shl(d, k);
            want.x = wrap16((int16_t)rw(pos) + d.x);
            want.y = wrap16((int16_t)rw(pos + 2) + d.y);
            want.z = wrap16((int16_t)rw(pos + 4) + d.z);
            f[k]();
            expect(same_vec(pos, pos + 2, pos + 4, want), "%04X %04X %04X, want %04X %04X %04X", rw(pos), rw(pos + 2),
                   rw(pos + 4), (uint16_t)want.x, (uint16_t)want.y, (uint16_t)want.z);
        }
    }
}

static void check_fx(long n)
{
    long i;

    current = "FxDivide";
    for (i = 0; i < n; i++) {
        uint32_t num;
        FxDiv q;

        fresh();
        num = (uint32_t)old_gsu.r[0] << 16 | old_gsu.r[4];
        q = fx_divide(num, old_gsu.r[6]);
        old_abi_FxDivide();
        expect(old_gsu.r[4] == q.quot && old_gsu.r[0] == q.rem, "%08X / %04X: %04X r %04X, want %04X r %04X", num,
               old_gsu.r[6], old_gsu.r[4], old_gsu.r[0], q.quot, q.rem);
    }
    current = "FxDivideHalvedIfLarge";
    for (i = 0; i < n; i++) {
        uint32_t num;
        uint16_t d;
        FxDiv q;

        fresh();
        num = (uint32_t)old_gsu.r[0] << 16 | old_gsu.r[4];
        d = old_gsu.r[6];
        q = fx_divide_halved_if_large(num, &d);
        old_abi_FxDivideHalvedIfLarge();
        expect(old_gsu.r[4] == q.quot && old_gsu.r[0] == q.rem && old_gsu.r[6] == d, "%08X / %04X: %04X r %04X d %04X, want %04X r %04X d %04X",
               num, d, old_gsu.r[4], old_gsu.r[0], old_gsu.r[6], q.quot, q.rem, d);
    }
    current = "FxSquareRoot";
    for (i = 0; i < n; i++) {
        uint32_t num;
        FxSqrt r;

        fresh();
        num = (uint32_t)old_gsu.r[4] << 16 | old_gsu.r[5];
        r = fx_sqrt(num);
        old_abi_FxSquareRoot();
        expect(old_gsu.r[6] == r.root && old_gsu.r[8] == r.rem, "%08X: %04X r %04X, want %04X r %04X", num, old_gsu.r[6],
               old_gsu.r[8], r.root, r.rem);
    }
    current = "FxNormalize";
    for (i = 0; i < n; i++) {
        int16_t v[3];
        int j;

        fresh();
        for (j = 0; j < 3; j++) {
            if (!(rnd() & 7))
                old_gsu.r[1 + j] = 0;
            else if (rnd() & 1)
                old_gsu.r[1 + j] = (uint16_t)(int16_t)((int16_t)rnd() >> (rnd() % 15));
            v[j] = (int16_t)old_gsu.r[1 + j];
        }
        fx_normalize(v);
        old_abi_FxNormalize();
        expect(old_gsu.r[1] == (uint16_t)v[0] && old_gsu.r[2] == (uint16_t)v[1] && old_gsu.r[3] == (uint16_t)v[2],
               "%04X %04X %04X, want %04X %04X %04X", old_gsu.r[1], old_gsu.r[2], old_gsu.r[3], (uint16_t)v[0],
               (uint16_t)v[1], (uint16_t)v[2]);
    }
    current = "FxAngleOf";
    for (i = 0; i < n; i++) {
        uint16_t want;

        fresh();
        if (!(rnd() & 7))
            old_gsu.r[2] = (rnd() & 1) ? old_gsu.r[1] : (uint16_t)-old_gsu.r[1];
        want = fx_angle_of((int16_t)old_gsu.r[1], (int16_t)old_gsu.r[2]);
        old_abi_FxAngleOf();
        expect(old_gsu.r[0] == want, "(%04X, %04X): %04X, want %04X", old_gsu.r[1], old_gsu.r[2], old_gsu.r[0], want);
    }
    current = "FxAngleVector";
    for (i = 0; i < n; i++) {
        int16_t x, z;

        fresh();
        fx_angle_vector((uint8_t)(old_gsu.r[0] >> 8), (int16_t)old_gsu.r[4], &x, &z);
        old_abi_FxAngleVector();
        expect(old_gsu.r[1] == (uint16_t)x && old_gsu.r[3] == (uint16_t)z, "%04X: %04X %04X, want %04X %04X", old_gsu.r[0],
               old_gsu.r[1], old_gsu.r[3], (uint16_t)x, (uint16_t)z);
    }
    current = "FxTurn";
    for (i = 0; i < n; i++) {
        int16_t x, z;

        fresh();
        old_gsu.r[2] = b8();
        old_gsu.rombr = 0;
        x = (int16_t)old_gsu.r[1];
        z = (int16_t)old_gsu.r[3];
        fx_turn(&x, &z, (uint8_t)old_gsu.r[2]);
        old_abi_FxTurn();
        expect(old_gsu.r[1] == (uint16_t)x && old_gsu.r[3] == (uint16_t)z, "by %02X: %04X %04X, want %04X %04X", old_gsu.r[2],
               old_gsu.r[1], old_gsu.r[3], (uint16_t)x, (uint16_t)z);
    }
    current = "FxMatrixTranspose04";
    for (i = 0; i < n; i++) {
        int16_t m[9];
        uint16_t at;
        int j, ok = 1;

        fresh();
        at = (uint16_t)(0x2000 + (rnd() & 0x1FFE));
        old_gsu.r[9] = at;
        for (j = 0; j < 9; j++) {
            m[j] = (int16_t)w16();
            fww((uint16_t)(at + 2 * j), (uint16_t)m[j]);
        }
        fx_matrix_transpose(m);
        old_abi_FxMatrixTranspose04();
        for (j = 0; j < 9; j++)
            ok &= fw((uint16_t)(at + 2 * j)) == (uint16_t)m[j];
        expect(ok, "matrix at %04X", at);
    }
    current = "FxMatrixFromAngles";
    for (i = 0; i < n; i++) {
        int16_t m[9];
        uint16_t at, angles[3];
        int j, ok = 1;

        fresh();
        at = (uint16_t)(0x2000 + (rnd() & 0x1FFE));
        old_gsu.r[9] = at;
        for (j = 0; j < 3; j++) {
            angles[j] = w16();
            fww((uint16_t)(0x20 + 2 * j), angles[j]);
        }
        fx_matrix_from_angles(angles, m);
        old_abi_FxMatrixFromAngles();
        for (j = 0; j < 9; j++)
            ok &= fw((uint16_t)(at + 2 * j)) == (uint16_t)m[j];
        expect(ok, "angles %04X %04X %04X: word 0 %04X want %04X", angles[0], angles[1], angles[2], fw(at), (uint16_t)m[0]);
    }
}

int main(int argc, char **argv)
{
    static uint8_t rom[0x100000];
    FILE *f;
    long n = argc > 2 ? atol(argv[2]) : 200000;

    if (argc < 2 || !(f = fopen(argv[1], "rb")) || fread(rom, 1, sizeof rom, f) != sizeof rom) {
        fprintf(stderr, "usage: math_check ROM [N]\n");
        return 2;
    }
    fclose(f);
    srfx_rom_image = rom;
    check_approach(n);
    check_scale(n);
    check_random(n);
    check_multiply(n);
    check_rotate(n);
    check_heading(n);
    check_fx(n);
    printf("math_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
