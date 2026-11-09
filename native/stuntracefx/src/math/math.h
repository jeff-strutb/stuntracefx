/*
 * math.h: the game's fixed arithmetic. The original kept scratch in direct
 * page $3E and $40 and $1F2B; here results are returned.
 */
#ifndef STUNTRACEFX_MATH_H
#define STUNTRACEFX_MATH_H

#include <stdint.h>

/* a vector as the object code works on them */
typedef struct { int16_t x, y, z; } Vec16;

uint8_t RandomStep(void);
uint16_t RandomWordStep(void);

uint16_t ScaleThreeQuarters(uint16_t v);
uint16_t ScaleQuarter(uint16_t v);
uint16_t ScaleNineSixteenths(uint16_t v);
uint16_t ScaleFiveEighths(uint16_t v);
uint16_t ScaleSevenEighths(uint16_t v);
uint16_t ScaleFifteenSixteenths(uint16_t v);
uint16_t ScaleFiveThirtySeconds(uint16_t v);
uint16_t Mode7ScaleProduct(uint16_t value, int positive, uint8_t divisor, uint8_t factor);
uint16_t ScaleDp02Dp94(uint16_t *x, uint16_t *z);

/* Moving `value` toward `target` by a share of the difference, at least the
   minimum either way; *moved is 0 when it was already there. */
uint16_t ApproachHalfMin2(uint16_t value, uint16_t target, int *moved);
uint16_t ApproachQuarterMin4(uint16_t value, uint16_t target, int *moved);
uint16_t ApproachEighthMin8(uint16_t value, uint16_t target, int *moved);
uint16_t ApproachSixteenthMin16(uint16_t value, uint16_t target, int *moved);
uint16_t ApproachThirtySecondMin32(uint16_t value, uint16_t target, int *moved);
uint16_t ApproachSixtyFourthMin64(uint16_t value, uint16_t target, int *moved);
uint16_t ApproachOneTwentyEighthMin128(uint16_t value, uint16_t target, int *moved);
uint8_t ApproachHalfMin2Byte(uint8_t value, uint8_t target, int *moved);
uint8_t ApproachQuarterMin4Byte(uint8_t value, uint8_t target, int *moved);
uint8_t ApproachEighthMin8Byte(uint8_t value, uint8_t target, int *moved);
uint8_t ApproachSixteenthMin16Byte(uint8_t value, uint8_t target, int *moved);
uint8_t ApproachThirtySecondMin32Byte(uint8_t value, uint8_t target, int *moved);
uint8_t ApproachSixtyFourthMin64Byte(uint8_t value, uint8_t target, int *moved);
uint8_t ApproachOneTwentyEighthMin128Byte(uint8_t value, uint8_t target, int *moved);

/* Velocities from a speed and angles: $02, $08, $94 in the original. */
Vec16 HeadingFlat(int8_t speed, uint8_t heading);
Vec16 HeadingFlatMirrored(int8_t speed, uint8_t heading);
Vec16 HeadingFlatQuarterTurn(int8_t speed, uint8_t heading);
Vec16 HeadingFlatSpeedA(int8_t speed, uint8_t heading);
Vec16 HeadingPitched(int8_t speed, uint8_t heading, uint8_t pitch);
Vec16 HeadingPitchedFrom1F7C(int8_t speed, uint8_t heading, uint8_t pitch);
Vec16 VectorDoubled(Vec16 v);

/* Rotating 8-bit vectors by an 8-bit angle; the two results are sign-extended. */
void RotateXZ(int8_t x, int8_t z, uint8_t angle, int16_t *x_out, int16_t *z_out);
void RotateXZFrom08(int8_t x, int8_t z, uint8_t angle, int16_t *x_out, int16_t *z_out);
void RotateXY(int8_t x, int8_t y, uint8_t angle, int16_t *x_out, int16_t *y_out);

#endif
