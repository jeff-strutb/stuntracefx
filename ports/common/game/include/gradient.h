/*
 * gradient.h: the sky gradient's colour lists and HDMA set-up (video/gradient.c).
 */
#ifndef SRFX_GRADIENT_H
#define SRFX_GRADIENT_H

#include <stdint.h>

/* The first colour list, from the colour block at bank:*y (the colours
   from its +$20), into $7F:EF57 from the write offset in $F3. Run lengths
   come from the common run table at $1F:B44E, from +$28 for course $0A
   (the course byte at $0E3D of the colours' bank). *y is left past the
   last colour read. */
void gradient_expand_first(uint8_t bank, uint16_t *y);

/* The second list, as gradient_expand_first: run lengths from +$50 for
   course 1. Written at the same list as the first, from the write offset
   in $F3 (the game uses $0400, the list at $7F:F357). */
void gradient_expand_second(uint8_t bank, uint16_t *y);

/* Both colour lists for a course, x being the course's entry (course * 3)
   in the block table at $03:F6F1, then $100 bytes at $7F:F757 cleared.
   Returns Y past the last colour read. */
uint16_t gradient_build_colours(uint16_t x);

/* Stores `value` at `count` words of the CGADD table at $7F:3497 + x, x
   stepping by two. Returns x past the last entry. */
uint16_t gradient_fill_cgadd(uint16_t value, uint16_t x, uint16_t count);

/* The same fill, as the original keeps a copy of it in bank $0B. */
uint16_t gradient_fill_cgadd_b(uint16_t value, uint16_t x, uint16_t count);

/* The gradient's HDMA set-up: channel 2 (CGADD from $7F:3497) and channel 3
   (CGDATA, indirect, from $7F:3489), their tables filled in. The bank says
   where the course byte at $0E3D is read (bank 0 or 3: the I/O mirror, the
   registers are the same). Returns A: the high byte of the course's block
   address with the last byte of $1F:A81F. */
uint16_t gradient_setup(uint8_t bank);

#endif
