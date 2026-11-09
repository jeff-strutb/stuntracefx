/*
 * cart.h: the cartridge's data, as its own format has it. The game's tables,
 * art and scripts hold references to one another as bank and address, so the
 * data is kept as an image, read through these; the image is loaded from the
 * game's package at start.
 */
#ifndef STUNTRACEFX_CART_H
#define STUNTRACEFX_CART_H

#include <stdint.h>

#define CART_SIZE 0x100000u

extern uint8_t cart[CART_SIZE];

void cart_load(void);

static inline uint32_t cart_offset(uint8_t bank, uint16_t addr)
{
    return (bank < 0x40 ? (uint32_t)bank << 15 | (addr & 0x7FFFu) : (uint32_t)(bank - 0x40) << 16 | addr) & (CART_SIZE - 1);
}
static inline const uint8_t *cart_at(uint8_t bank, uint16_t addr) { return &cart[cart_offset(bank, addr)]; }
static inline uint8_t cart8(uint8_t bank, uint16_t addr) { return cart[cart_offset(bank, addr)]; }
static inline uint16_t cart16(uint8_t bank, uint16_t addr)
{
    return (uint16_t)(cart8(bank, addr) | cart8(bank, (uint16_t)(addr + 1)) << 8);
}
static inline uint32_t cart24(uint8_t bank, uint16_t addr)
{
    return (uint32_t)cart16(bank, addr) | (uint32_t)cart8(bank, (uint16_t)(addr + 2)) << 16;
}

#endif
