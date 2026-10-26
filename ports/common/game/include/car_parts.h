/*
 * car_parts.h: a car's loose parts (object/car_parts.c). A part is an
 * object whose +$06 is its car; it sits on the car at the car's position
 * plus an offset, turned by the car's angles, until it is knocked loose.
 */
#ifndef SRFX_CAR_PARTS_H
#define SRFX_CAR_PARTS_H

#include <stdint.h>

/* The part's offset on its car, as x, y, z (each doubled, 16-bit). */
void part_offset(uint16_t part, uint16_t car, uint16_t off[3]);

/* A part comes off its car: its frame is then part_detach_step. */
void part_detach(uint16_t part);

/* The detaching part's frame: it sits on its car, offset a few steps out,
   and is released after its eighth step. */
void part_detach_step(uint16_t part);

/* The flying part turns back to its car: its frame is then part_return_step. */
void part_return(uint16_t part);

/* The returning part's frame: it moves and turns toward its place on the car. */
void part_return_step(uint16_t part);

#endif
