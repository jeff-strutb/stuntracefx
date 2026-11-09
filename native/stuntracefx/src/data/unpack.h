#ifndef STUNTRACEFX_UNPACK_H
#define STUNTRACEFX_UNPACK_H

#include <stdint.h>

/* the data packed ending just below cart position `end`; returns its size, 0 if `room` is too small */
uint32_t unpack(uint32_t end, uint8_t *out, uint32_t room);

#endif
