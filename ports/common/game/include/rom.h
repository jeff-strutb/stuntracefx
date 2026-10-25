/*
 * rom.h: the cartridge's data, read where the game keeps it.
 *
 * The player's own ROM is built into the program; nothing taken from it is
 * in the source. Tables are found by the address the original reads them
 * at, in the cartridge's LoROM layout (banks $00-$3F and their mirrors,
 * $8000-$FFFF), as the S-CPU and the Super FX both see it.
 */
#ifndef SRFX_ROM_H
#define SRFX_ROM_H

#include <stdint.h>

extern const uint8_t *srfx_rom_image;

static inline const uint8_t *rom_at(uint32_t addr)
{
    return srfx_rom_image + ((addr >> 16) & 0x3Fu) * 0x8000u + (addr & 0x7FFFu);
}

/* A little-endian word of the ROM. */
static inline uint16_t rom_word(uint32_t addr)
{
    const uint8_t *p = rom_at(addr);

    return (uint16_t)(p[0] | p[1] << 8);
}

#endif
