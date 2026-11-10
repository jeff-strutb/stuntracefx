/*
 * arith.h: the Super FX's square root and the vector length the race asks
 * it for (arith.c), as plain functions of their values.
 */
#ifndef STUNTRACEFX_FX_RENDER_ARITH_H
#define STUNTRACEFX_FX_RENDER_ARITH_H

#include <stdint.h>

/* The chip's integer square root of a 32-bit value, its remainder kept to
   16 bits as the chip kept it. */
uint16_t FxSqrt32(uint32_t value);

/* The length of the vector (x, z), each a signed word: the root of the sum
   of their squares (the program $01:80BC). */
uint16_t FxVectorLength(uint16_t x, uint16_t z);

#endif
