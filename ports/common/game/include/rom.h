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

/* A word of the tables the original runs from work RAM ($7E:A2D9 on,
   copied there from $02:8000 at power on), read where the cartridge
   keeps them, by the work RAM address the original reads. */
static inline uint16_t ram_code_word(uint16_t addr)
{
    return rom_word(0x028000u + (uint16_t)(addr - 0xA2D9u));
}

#endif
