/*
 * math.c: the game's fixed scalings, approaches, the random numbers, and
 * vectors from speeds and angles.
 *
 * Not converted, no caller in the game: MulWordByte, RotateXZWide,
 * RotateXZWideFrom08 (02:DF99, 03:C6F4, 03:C788) and 7E:E7A1. Not here:
 * PpuMultiplierSetLowByteTimes8 (7E:E6BA) goes with video's ColourBlendBy, its
 * only caller; ObjectMoveByVector and ObjectVelocityFromHeading are the
 * objects'. race/NOTES lists them all.
 */
#include "data/race_tables.h"
#include "math/math.h"
#include "system/state.h"

/* An arithmetic shift right by one: the sign stays, the low bit falls out. */
static uint16_t asr1(uint16_t v)
{
    return (uint16_t)((v >> 1) | (v & 0x8000));
}

/*
 * @routine 02:DF7D RandomStep
 * WHAT IT DOES: Steps the byte generator: each state byte becomes the
 * previous result minus it, the borrow carried along.
 */
uint8_t RandomStep(void)
{
    uint8_t *s = g.random_state;
    unsigned borrow = 1;
    uint8_t a = s[0];

    for (int i = 1; i <= 4; i++) {
        unsigned r = (unsigned)a - s[i & 3] - borrow;

        borrow = r >= 0x100;
        a = (uint8_t)r;
        s[i & 3] = a;
    }
    return a;
}

/*
 * @routine 07:D9D1 RandomWordStep
 * WHAT IT DOES: Steps the 16-bit generator at $0D18 and returns the value.
 * A result of $AA55 with the shifted-out bit clear stores 0 instead.
 */
uint16_t RandomWordStep(void)
{
    uint16_t x = g.random_word, swapped, v;

    swapped = (uint16_t)((((x & 0xFF) << 8) ^ x) >> 8 | ((((x & 0xFF) << 8) ^ x) << 8));
    v = (uint16_t)(((swapped >> 8) << 1) ^ swapped);
    if (v & 1) {
        v = (uint16_t)((v >> 1) ^ 0xFF80 ^ 0x8180);
        g.random_word = v;
    } else {
        v = (uint16_t)((v >> 1) ^ 0xFF80);
        g.random_word = v == 0xAA55 ? 0 : v;
    }
    return v;
}

/* @routine 03:CC5A ScaleThreeQuarters: v/2 + v/4, each rounded down. */
uint16_t ScaleThreeQuarters(uint16_t v)
{
    uint16_t half = asr1(v);

    return (uint16_t)(asr1(half) + half);
}

/* @routine 03:CC29 ScaleQuarter */
uint16_t ScaleQuarter(uint16_t v)
{
    return asr1(asr1(v));
}

/* @routine 03:CC32 ScaleNineSixteenths: v/2 + v/16. */
uint16_t ScaleNineSixteenths(uint16_t v)
{
    uint16_t half = asr1(v);

    return (uint16_t)(asr1(asr1(asr1(half))) + half);
}

/* @routine 03:CC48 ScaleFiveEighths: v/2 + v/8. */
uint16_t ScaleFiveEighths(uint16_t v)
{
    uint16_t half = asr1(v);

    return (uint16_t)(asr1(asr1(half)) + half);
}

/* @routine 03:CC68 ScaleSevenEighths: v/8 + v/2 + v/4. */
uint16_t ScaleSevenEighths(uint16_t v)
{
    uint16_t half = asr1(v), quarter = asr1(half);

    return (uint16_t)(asr1(quarter) + half + quarter);
}

/* @routine 03:CC7F ScaleFifteenSixteenths: v/2 + v/4 + v/8 + v/16. */
uint16_t ScaleFifteenSixteenths(uint16_t v)
{
    uint16_t half = asr1(v), quarter = asr1(half), eighth = asr1(quarter);

    return (uint16_t)(asr1(eighth) + eighth + half + quarter);
}

/* @routine 03:CC0D ScaleFiveThirtySeconds: v/8 + v/32. */
uint16_t ScaleFiveThirtySeconds(uint16_t v)
{
    uint16_t eighth = asr1(asr1(asr1(v)));

    return (uint16_t)(asr1(asr1(eighth)) + eighth);
}

/*
 * @routine 03:FCE0 Mode7ScaleProduct
 * WHAT IT DOES: Divides value by divisor, its magnitude when it is not
 * positive and the quotient negated, and multiplies the quotient, as a
 * signed word, by the signed factor; returns the product's low word. A
 * divisor of 0 gives a quotient of $FFFF, as the divider did.
 */
uint16_t Mode7ScaleProduct(uint16_t value, int positive, uint8_t divisor, uint8_t factor)
{
    uint16_t n = positive ? value : (uint16_t)-value;
    uint16_t q = divisor ? (uint16_t)(n / divisor) : 0xFFFF;

    if (!positive)
        q = (uint16_t)-q;
    return (uint16_t)((int32_t)(int16_t)q * (int8_t)factor);
}

/*
 * @routine 08:83F8 ScaleDp02Dp94
 * WHAT IT DOES: Rescales the pair to an area's coordinates: x becomes
 * (x >> 10) + $20 with bit 0 cleared, z (its top five bits >> 5) + $400;
 * returns their sum.
 */
uint16_t AreaCellOf(uint16_t *x, uint16_t *z)
{
    uint16_t a = asr1(asr1(*x)), b;

    a = (uint16_t)(int16_t)(int8_t)(uint8_t)(a >> 8);
    a = (uint16_t)((uint16_t)(a + 0x20) & 0xFFFE);
    *x = a;
    b = (uint16_t)((int16_t)(*z & 0xF800) >> 5);
    b = (uint16_t)(b + 0x0400);
    *z = b;
    return (uint16_t)(b + a);
}

/* One halving that rounds negative results toward zero. */
static uint16_t halve(uint16_t v, uint16_t sign)
{
    uint16_t r = (uint16_t)((v >> 1) | (v & sign));

    return (r & sign) ? (uint16_t)(r + (v & 1)) : r;
}

/* The approach all the routines below share, for a word and for a byte:
   the step is the difference (wrapping as the value does), at least `min`
   either way, halved `shifts` times. */
static uint16_t approach_word(uint16_t value, uint16_t target, uint16_t min, int shifts, int *moved)
{
    uint16_t d;

    *moved = target != value;
    if (!*moved)
        return value;
    d = (uint16_t)(target - value);
    if (!(d & 0x8000)) {
        if (d < min)
            d = min;
    } else if (!((uint16_t)(d + min) & 0x8000)) {
        d = (uint16_t)-min;
    }
    for (int i = 0; i < shifts; i++)
        d = halve(d, 0x8000);
    return (uint16_t)(d + value);
}

static uint8_t approach_byte(uint8_t value, uint8_t target, uint8_t min, int shifts, int *moved)
{
    uint8_t d;

    *moved = target != value;
    if (!*moved)
        return value;
    d = (uint8_t)(target - value);
    if (!(d & 0x80)) {
        if (d < min)
            d = min;
    } else if (!((uint8_t)(d + min) & 0x80)) {
        d = (uint8_t)-min;
    }
    for (int i = 0; i < shifts; i++)
        d = (uint8_t)halve(d, 0x0080);
    return (uint8_t)(d + value);
}

/* @routine 03:CDC1 ApproachHalfMin2 */
uint16_t ApproachHalfMin2(uint16_t value, uint16_t target, int *moved) { return approach_word(value, target, 2, 1, moved); }
/* @routine 03:CDF4 ApproachQuarterMin4 */
uint16_t ApproachQuarterMin4(uint16_t value, uint16_t target, int *moved) { return approach_word(value, target, 4, 2, moved); }
/* @routine 03:CE30 ApproachEighthMin8 */
uint16_t ApproachEighthMin8(uint16_t value, uint16_t target, int *moved) { return approach_word(value, target, 8, 3, moved); }
/* @routine 03:CE75 ApproachSixteenthMin16 */
uint16_t ApproachSixteenthMin16(uint16_t value, uint16_t target, int *moved) { return approach_word(value, target, 16, 4, moved); }
/* @routine 03:CEC3 ApproachThirtySecondMin32 */
uint16_t ApproachThirtySecondMin32(uint16_t value, uint16_t target, int *moved) { return approach_word(value, target, 32, 5, moved); }
/* @routine 03:CF1A ApproachSixtyFourthMin64 */
uint16_t ApproachSixtyFourthMin64(uint16_t value, uint16_t target, int *moved) { return approach_word(value, target, 64, 6, moved); }
/* @routine 03:CF7A ApproachOneTwentyEighthMin128 */
uint16_t ApproachOneTwentyEighthMin128(uint16_t value, uint16_t target, int *moved) { return approach_word(value, target, 128, 7, moved); }
/* @routine 03:CFE3 ApproachHalfMin2Byte */
uint8_t ApproachHalfMin2Byte(uint8_t value, uint8_t target, int *moved) { return approach_byte(value, target, 2, 1, moved); }
/* @routine 03:D00F ApproachQuarterMin4Byte */
uint8_t ApproachQuarterMin4Byte(uint8_t value, uint8_t target, int *moved) { return approach_byte(value, target, 4, 2, moved); }
/* @routine 03:D042 ApproachEighthMin8Byte */
uint8_t ApproachEighthMin8Byte(uint8_t value, uint8_t target, int *moved) { return approach_byte(value, target, 8, 3, moved); }
/* @routine 03:D07C ApproachSixteenthMin16Byte */
uint8_t ApproachSixteenthMin16Byte(uint8_t value, uint8_t target, int *moved) { return approach_byte(value, target, 16, 4, moved); }
/* @routine 03:D0BD ApproachThirtySecondMin32Byte */
uint8_t ApproachThirtySecondMin32Byte(uint8_t value, uint8_t target, int *moved) { return approach_byte(value, target, 32, 5, moved); }
/* @routine 03:D105 ApproachSixtyFourthMin64Byte */
uint8_t ApproachSixtyFourthMin64Byte(uint8_t value, uint8_t target, int *moved) { return approach_byte(value, target, 64, 6, moved); }
/* @routine 03:D154 ApproachOneTwentyEighthMin128Byte: $80 is -128 as a byte, so it always steps down by 1. */
uint8_t ApproachOneTwentyEighthMin128Byte(uint8_t value, uint8_t target, int *moved) { return approach_byte(value, target, 128, 7, moved); }

/* The signed 8 x 8 multiply the code does by hand: the magnitudes, the
   first doubled and kept to eight bits, the high byte of the product,
   negated when the signs differ. */
static uint8_t smul(uint8_t a, uint8_t b)
{
    int negative = ((a ^ b) & 0x80) != 0;
    uint8_t ma = (uint8_t)((a & 0x80 ? (uint8_t)-a : a) << 1);
    uint8_t mb = b & 0x80 ? (uint8_t)-b : b;
    uint8_t h = (uint8_t)(((unsigned)ma * mb) >> 8);

    return negative ? (uint8_t)-h : h;
}

static Vec16 flat(int8_t speed, uint8_t i)
{
    Vec16 v;

    v.x = (int8_t)smul((uint8_t)speed, trig_sine(i));
    v.y = 0;
    v.z = (int8_t)smul((uint8_t)speed, trig_cosine(i));
    return v;
}

static Vec16 pitched(int8_t speed, uint8_t ia, uint8_t ib)
{
    Vec16 v;

    v.x = (int8_t)smul(smul((uint8_t)speed, trig_sine(ia)), trig_cosine(ib));
    v.z = (int8_t)smul(smul((uint8_t)speed, trig_cosine(ia)), trig_cosine(ib));
    v.y = (int8_t)smul((uint8_t)speed, trig_sine(ib));
    return v;
}

/* @routine 03:B93E HeadingFlat: speed sin, 0, speed cos of the heading. */
Vec16 HeadingFlat(int8_t speed, uint8_t heading) { return flat(speed, heading); }
/* @routine 03:BA12 HeadingFlatMirrored: for the heading 1 - h. */
Vec16 HeadingFlatMirrored(int8_t speed, uint8_t heading) { return flat(speed, (uint8_t)(~heading + 2)); }
/* @routine 03:BE8D HeadingFlatQuarterTurn: for the heading $41 - h. */
Vec16 HeadingFlatQuarterTurn(int8_t speed, uint8_t heading) { return flat(speed, (uint8_t)(0x40 - heading + 1)); }
/* @routine 03:BF62 HeadingFlatSpeedA: for the heading h + 1. */
Vec16 HeadingFlatSpeedA(int8_t speed, uint8_t heading) { return flat(speed, (uint8_t)(heading + 1)); }
/* @routine 03:BAEB HeadingPitched: the pitch negated. */
Vec16 HeadingPitched(int8_t speed, uint8_t heading, uint8_t pitch) { return pitched(speed, heading, (uint8_t)-pitch); }
/* @routine 03:BCC7 HeadingPitchedFrom1F7C: the heading negated. */
Vec16 HeadingPitchedMirrored(int8_t speed, uint8_t heading, uint8_t pitch) { return pitched(speed, (uint8_t)-heading, pitch); }

Vec16 VectorDoubled(Vec16 v)
{
    Vec16 r = {(int16_t)(v.x * 2), (int16_t)(v.y * 2), (int16_t)(v.z * 2)};

    return r;
}

/* Two 8-bit vector components rotated by a sine and cosine: x cos + v sin
   and v cos - x sin, each sign-extended from eight bits. */
static void rotate8(uint8_t s, uint8_t c, int8_t x, int8_t v, int16_t *out_x, int16_t *out_v)
{
    *out_x = (int8_t)(uint8_t)(smul((uint8_t)x, c) + smul((uint8_t)v, s));
    *out_v = (int8_t)(uint8_t)(smul((uint8_t)v, c) - smul((uint8_t)x, s));
}

/* @routine 03:C27F RotateXZ: by the angle negated. */
void RotateXZ(int8_t x, int8_t z, uint8_t angle, int16_t *x_out, int16_t *z_out)
{
    uint8_t i = (uint8_t)-angle;

    rotate8(trig_sine(i), trig_cosine(i), x, z, x_out, z_out);
}

/* @routine 03:C3FF RotateXZFrom08: by the angle itself. */
void RotateXZFrom08(int8_t x, int8_t z, uint8_t angle, int16_t *x_out, int16_t *z_out)
{
    rotate8(trig_sine(angle), trig_cosine(angle), x, z, x_out, z_out);
}

/* @routine 03:C57C RotateXY */
void RotateXY(int8_t x, int8_t y, uint8_t angle, int16_t *x_out, int16_t *y_out)
{
    rotate8(trig_sine(angle), trig_cosine(angle), x, y, x_out, y_out);
}
