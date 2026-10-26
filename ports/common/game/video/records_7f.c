/*
 * records_7f.c: two tables of four-byte display records in bank $7F, at
 * $7F:43D7 and $7F:47FF (and blocks at $7F:45B7-$7F:4BB7), filled from the
 * position lists in bank $09 that end with $FF.
 */
#include "game.h"
#include "smallvideo.h"

/* A byte of bank $7F, by its address there. */
#define BANK7F(a)   W8(0x10000u + (uint32_t)(a))

/* For each pair (x, y) of the list at bank:$F634, up to the $FF that ends
   it: the record ($10, x - $28, y - $28, $A0) at $7F:43D7 and the record
   ($60, x + $28, y + $28, $F0) at $7F:47FF, four bytes each on every pair. */
void records_7f_from_f634(uint8_t bank)
{
    uint16_t x = 0, y = 0xF634;
    uint8_t v, d;

    while ((v = bank_byte(bank, y)) != 0xFF) {
        d = (uint8_t)(v - 0x28);
        BANK7F(0x43D8u + x) = d;
        BANK7F(0x4800u + x) = (uint8_t)(d + 0x50);
        BANK7F(0x43D7u + x) = 0x10;
        BANK7F(0x47FFu + x) = 0x60;
        d = (uint8_t)(bank_byte(bank, (uint16_t)(y + 1)) - 0x28);
        BANK7F(0x43D9u + x) = d;
        BANK7F(0x4801u + x) = (uint8_t)(d + 0x50);
        BANK7F(0x43DAu + x) = 0xA0;
        BANK7F(0x4802u + x) = 0xF0;
        x = (uint16_t)(x + 4);
        y = (uint16_t)(y + 2);
    }
}

/* For each pair (x, y) of the list at bank:$F170, up to its $FF: the
   record ($18, x, y, $E7) at $7F:43D7. Then eight 32-byte blocks from
   bank $09 ($F88B-$F96B) go to $7F:43D7, $45B7, $45D7, $47B7, $47D7,
   $49B7, $49D7 and $4BB7, and $40 bytes from $09:$F849 go to $7F:$5187
   (the DMA through WMDATA). Returns the low byte of the list's end. */
uint8_t records_7f_from_f170(uint8_t bank)
{
    static const uint16_t from[8] = {0xF88B, 0xF8AB, 0xF8CB, 0xF8EB, 0xF90B, 0xF92B, 0xF94B, 0xF96B};
    static const uint16_t to[8] = {0x43D7, 0x45B7, 0x45D7, 0x47B7, 0x47D7, 0x49B7, 0x49D7, 0x4BB7};
    uint16_t x = 0, y = 0xF170;
    uint8_t v;
    unsigned i, k;

    while ((v = bank_byte(bank, y)) != 0xFF) {
        BANK7F(0x43D8u + x) = v;
        BANK7F(0x43D7u + x) = 0x18;
        BANK7F(0x43D9u + x) = bank_byte(bank, (uint16_t)(y + 1));
        BANK7F(0x43DAu + x) = 0xE7;
        x = (uint16_t)(x + 4);
        y = (uint16_t)(y + 2);
    }
    for (i = 0; i < 0x20; i++)
        for (k = 0; k < 8; k++)
            BANK7F(to[k] + i) = bank_byte(0x09, (uint16_t)(from[k] + i));
    for (i = 0; i < 0x40; i++)
        BANK7F(0x5187u + i) = bank_byte(0x09, (uint16_t)(0xF849u + i));
    return (uint8_t)y;
}
