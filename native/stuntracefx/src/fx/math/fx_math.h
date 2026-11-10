/*
 * fx_math.h: the Super FX logic's arithmetic: its 32-bit divide, square root and vector scaling (fx_math.c), and its
 * rotations (fx_rotate.c).
 */
#ifndef STUNTRACEFX_FX_MATH_H
#define STUNTRACEFX_FX_MATH_H

#include <stdint.h>

typedef struct { int16_t x, y, z; } FxVec3;

/* a vector scaled to length $7FFF (1.0 in 1.15), with what the physics reads after it: the shift (the square root's top
   bit number), the high word of the scaled x, and the low word of z times the scale before the shift */
typedef struct {
    FxVec3 unit;
    uint16_t shift, x_high, z_low;
} FxNormalized;

int FxNormalize(FxVec3 v, FxNormalized *out);           /* 0 for the zero vector, *out as it was */
uint16_t FxSquareRoot(uint32_t value);
uint16_t FxDivide(uint32_t dividend, uint16_t divisor); /* the quotient must fit in 16 bits */
uint16_t FxDivideHalvedIfLarge(uint32_t dividend, uint16_t divisor);

/* The cartridge bank the physics last chose for a table read, where that choice carries on into FxTurn: the chip reads
   FxTurn's sine and cosine in whatever ROM bank it has, and FxContactResolve and FxPushApply call it without choosing
   one, so after a read of car data ($09), part data ($04) or a collision model's points (the model's bank) it reads
   those banks' bytes at the tables' place. Set where the physics chooses a bank that can reach those calls (all nine,
   by the fx chip-state analysis); the other calls pass $00, which they choose just before. */
extern uint8_t fx_table_bank;

typedef struct { int16_t x, z; } FxTurned;

/* (x, z) turned in the plane by `angle` (256 a full turn), the sine and cosine read in `table_bank` */
FxTurned FxTurn(int16_t x, uint8_t angle, int16_t z, uint8_t table_bank);

/* the vector along `heading` (its high byte the angle, 256 a full turn), in the plane: x = -sin * length * 8,
   z = cos * length * 8 (sin and cos the table's bytes / 128), y 0 */
FxVec3 FxAngleVector(uint16_t heading, int16_t length);

#endif
