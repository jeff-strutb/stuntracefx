/*
 * fixmath.h: the game's fixed-point maths.
 *
 * Every function computes exactly what the original computes, rounding and
 * overflow included: where the original's arithmetic loses a bit or rounds
 * one way, so does this, and the comment says so. Values are 16-bit words
 * and bytes that wrap as the original's do. Angles are 0-255 (bytes) or
 * 0-$FFFF (words) for a full turn.
 */
#ifndef SRFX_FIXMATH_H
#define SRFX_FIXMATH_H

#include <stdbool.h>
#include <stdint.h>

/* Arithmetic shifts right (rounding toward minus infinity), defined for
   negative values on every compiler. */
static inline int16_t asr16(int16_t v, int n)
{
    return (int16_t)(v < 0 ? ~(~v >> n) : v >> n);
}

static inline int32_t asr32(int32_t v, int n)
{
    return v < 0 ? ~(~v >> n) : v >> n;
}

/* A sum or difference wrapped to 16 bits, as the original's adds wrap. */
static inline int16_t wrap16(int32_t v)
{
    return (int16_t)(uint16_t)v;
}

/* ---- bcd.c: decimal mode ---- */

/* An 8-bit add or subtract in the S-CPU's decimal mode, carry in and out
   (out: set on a carry, or on no borrow). */
uint8_t bcd_add8(uint8_t a, uint8_t m, bool *carry);
uint8_t bcd_sub8(uint8_t a, uint8_t m, bool *carry);
uint16_t bcd_add16(uint16_t a, uint16_t m, bool *carry);
uint16_t bcd_sub16(uint16_t a, uint16_t m, bool *carry);

/* A binary value as four BCD digits, each counted as the original counts
   it (a value of 10000 or more puts 10 or more in the top digit), and back
   (each digit taken at its weight, 10-15 included). */
uint16_t binary_to_bcd(uint16_t v);
uint16_t bcd_to_binary(uint16_t v);

/* ---- trig.c: the sine tables ---- */

/* The S-CPU's sine and cosine of an 8-bit angle as signed bytes (127 is
   just under 1; the tables at $00:96FE, cosine 64 entries on). */
int8_t sin8(uint8_t angle);
int8_t cos8(uint8_t angle);

/* The Super FX's sine and cosine of a 16-bit angle in 1.15 fixed point:
   the word table at $00:983E (257 entries), linearly interpolated by the
   angle's low byte. */
int16_t fx_sin16(uint16_t angle);
int16_t fx_cos16(uint16_t angle);

/* ---- multiply.c ---- */

/* The S-CPU's signed 8 x 8 multiply: twice |a| (kept to a byte, so -128
   gives 0) times |b|, the high byte of the product, negated when the signs
   differ. About a * b / 128. */
int8_t mul8(int8_t a, int8_t b);

/* a * b / 256 by the PPU's multiplier: bits 8-23 of the signed product. */
int16_t mul16x8(int16_t a, int8_t b);

/* v * 2s / 256 rounded toward zero, the magnitudes multiplied: twice |s|
   kept to a byte (so -128 gives 0). */
int16_t mul_word_byte(int16_t v, int8_t s);

/* Mode7ScaleProduct: value / divisor by the S-CPU's divider, on the
   magnitude (value as positive, or negated first when `positive` is
   false, the quotient negated back), then times the signed factor by the
   PPU's multiplier, kept to 16 bits. A divisor of 0 gives the divider's
   $FFFF. */
uint16_t mode7_scale_product(uint16_t value, bool positive, uint8_t divisor, int8_t factor);

/* ---- approach.c ---- */

/* Moves value toward target: the difference, raised to at least min_step
   either way, then halved `halvings` times (each rounding toward zero);
   *moved is false, and value returned unchanged, when it is there. The
   8-bit form works in bytes; with a min_step of $80 that is -128 whichever
   way, so it always steps down by one (the original's own result). */
int16_t approach16(int16_t value, int16_t target, uint16_t min_step, int halvings, bool *moved);
uint8_t approach8(uint8_t value, uint8_t target, uint8_t min_step, int halvings, bool *moved);

/* ---- scale.c: fixed fractions of a signed word, each part rounded
   toward minus infinity and the parts summed ---- */
int16_t scale_1_4(int16_t v);      /* v/4 */
int16_t scale_3_4(int16_t v);      /* v/2 + v/4 */
int16_t scale_9_16(int16_t v);     /* v/2 + v/16 */
int16_t scale_5_8(int16_t v);      /* v/2 + v/8 */
int16_t scale_7_8(int16_t v);      /* v/8 + v/2 + v/4 */
int16_t scale_15_16(int16_t v);    /* v/16 + v/8 + v/2 + v/4 */
int16_t scale_5_32(int16_t v);     /* v/32 + v/8 */

/* An object's grid key from its x and z: x becomes x/1024 + $20 with bit
   0 cleared, z its top five bits as a signed count of $800 times $40, plus
   $400; the key is their sum. */
uint16_t xz_grid_key(int16_t *x, int16_t *z);

/* One step of a blend of four words between base and target: the factor
   moves one toward `limit` (up) or 0 (down), and each word becomes base +
   (target - base) * factor / limit through mode7_scale_product (the
   difference taken as positive when target >= base, unsigned). False, and
   nothing changed, when the factor is already there. */
bool blend_step(uint16_t *factor, uint16_t limit, bool up, const uint16_t base[4], const uint16_t target[4],
                uint16_t out[4]);

/* ---- random.c ---- */

/* The byte generator: four bytes, each but the first the previous result
   less it (with the borrow carried, from $E6 - $E7 - 1), then the first
   the last result less the first. Returns the new first byte. */
uint8_t random_step(uint8_t s[4]);

/* The word generator: the low byte XORed into the high and the bytes
   swapped, then XORed with the old low byte shifted left, shifted right
   and XORed with $FF80, and with $8180 when the bit shifted out was set.
   A result of $AA55 with that bit clear leaves the state 0. Returns the
   result. */
uint16_t random_word_step(uint16_t *state);

/* ---- rotate.c ---- */

/* The 8-bit rotation of (x, v) by the angle: x' = x cos + v sin and v' =
   v cos - x sin with mul8, summed in bytes, sign-extended. */
void rotate8(int8_t x, int8_t v, uint8_t angle, int16_t *x_out, int16_t *v_out);

/* The 16-bit rotation: z' = x sin + z cos and x' = x cos - z sin with
   mul_word_byte. */
void rotate16(int16_t x, int16_t z, uint8_t angle, int16_t *x_out, int16_t *z_out);

/* ---- heading.c: a velocity from a speed and angles ---- */

typedef struct {
    int16_t x, y, z;
} Vec3s;

/* Flat: x = speed sin h, z = speed cos h (mul8), y = 0. */
Vec3s heading_vector(int8_t speed, uint8_t heading);

/* With a pitch: x = (speed sin h) cos p, z = (speed cos h) cos p, y =
   speed sin p, each product mul8. */
Vec3s heading_vector_pitched(int8_t speed, uint8_t heading, uint8_t pitch);

/* Each component shifted left by k, kept to 16 bits. */
Vec3s vec3_shl(Vec3s v, int k);

/* ---- fx_math.c: the Super FX's arithmetic ---- */

typedef struct {
    uint16_t quot, rem;
} FxDiv;

/* n / d for a quotient that fits in 16 bits (n's high word below d):
   restoring division, the partial remainder kept to 16 bits as it is
   shifted, so a remainder of $8000 or more loses its bit 15. */
FxDiv fx_divide(uint32_t n, uint16_t d);

/* As fx_divide, the divisor and the dividend halved first when either the
   divisor or the dividend's high word has bit 15 set; *d is left the
   divisor used. */
FxDiv fx_divide_halved_if_large(uint32_t n, uint16_t *d);

typedef struct {
    uint16_t root, rem;
} FxSqrt;

/* n / d signed, on the magnitudes (a negative n negated as 32 bits), the
   quotient negated when the signs differ; the remainder is left as the
   passes leave it, twice the true one plus the last bit shifted in. */
FxDiv fx_divide_signed(uint32_t n, uint16_t d);

/* The pair (x, y) halved and turned into ratios over d with 8 bits of
   fraction, the first held at $3FFF (and the second scaled to keep the
   pair's ratio) when it would pass it. */
void fx_ratio_clamp(int16_t *x, int16_t *y, uint16_t d);

/* The Super FX's products: a * b >> 15 (FMULT, ROL: 1.15) and a * b >> 16
   (FMULT), and DIV2 (a halving that rounds -1 to 0). */
int16_t mul15(int16_t a, int16_t b);
int16_t fmult(int16_t a, int16_t b);
int16_t div2(int16_t v);

/* The integer square root of n and n - root * root (kept to 16 bits). */
FxSqrt fx_sqrt(uint32_t n);

/* The vector scaled to length $7FFF (1.0 in 1.15), zero left zero. */
void fx_normalize(int16_t v[3]);

/* The angle of (x, y), 0-$FFFF for a full turn, from the arctangent table
   at $00:9E40. */
uint16_t fx_angle_of(int16_t x, int16_t y);

/* The vector of `length` at the angle: x = -(sin * length / 256) * 16 and
   z = (cos * length / 256) * 16, with the 8-bit tables. */
void fx_angle_vector(uint8_t angle, int16_t length, int16_t *x, int16_t *z);

/* (x, z) turned by the angle with the 8-bit tables as signed values times
   256: x' = (sin x + cos z) / 2^17, z' = (cos x - sin z) / 2^17, each a
   32-bit sum whose high word is halved. In the second, the borrow from the
   low words is taken the other way round (sin z below cos x), as the
   original does. */
void fx_turn(int16_t *x, int16_t *z, uint8_t angle);

/* A 3x3 matrix of words, row by row, transposed in place. */
void fx_matrix_transpose(int16_t m[9]);

/* The rotation matrix of three angles (1.15 products of the 16-bit sines
   and cosines, s and c of angles 0, 1, 2): s0s1s2 + c1c2, s0s1c2 - c1s2,
   c0s1, c0s2, c0c2, -s0, s0c1s2 - s1c2, s0c1c2 + s1s2, c0c1. */
void fx_matrix_from_angles(const uint16_t angles[3], int16_t m[9]);

#endif
