/*
 * state.c: the storage behind state.h.
 */
#include "state.h"
#include "rom.h"

uint8_t wram[0x20000];
uint8_t pakram[0x10000];

/* A byte as the game reads it in a bank: work RAM ($7E-$7F, and its first
   $2000 bytes in the ROM's banks), the game pak RAM ($70), the ROM. */
uint8_t bank_byte(uint8_t bank, uint16_t at)
{
    if (bank == 0x7E || bank == 0x7F)
        return wram[(uint32_t)(bank - 0x7E) << 16 | at];
    if (bank == 0x70)
        return pakram[at];
    if ((bank & 0x7F) < 0x40 && at < 0x2000)
        return wram[at];
    return rom_at((uint32_t)bank << 16 | at)[0];
}
