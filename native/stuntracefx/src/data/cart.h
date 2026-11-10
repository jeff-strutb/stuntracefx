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

/* tables in the code the original copied to work RAM ($7E:A2D9 up from ROM
   $01:0000, $7F:0800 up from $05:8000), by the address they ran at */
static inline const uint8_t *ram_code_7e(uint16_t a) { return &cart[0x10000u + (uint32_t)(a - 0xA2D9u)]; }
static inline uint16_t ram_code_7e16(uint16_t a) { const uint8_t *p = ram_code_7e(a); return (uint16_t)(p[0] | p[1] << 8); }
static inline uint8_t ram_code_7e8(uint16_t a) { return *ram_code_7e(a); }
static inline const uint8_t *ram_code_7f(uint16_t a) { return &cart[0x58000u + (uint32_t)(a - 0x0800u)]; }
static inline uint16_t ram_code_7f16(uint16_t a) { const uint8_t *p = ram_code_7f(a); return (uint16_t)(p[0] | p[1] << 8); }

#endif
