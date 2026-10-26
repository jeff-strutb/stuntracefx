/*
 * checksum.c: the checksum of the battery-backed save data in the game pak
 * RAM (bank $70): an 8-bit sum with carry of nine of its bytes.
 */
#include "game.h"
#include "smallvideo.h"

/* The checksum of the save's nine bytes at $70:FC06-$70:FC0E, added in
   this order, each add taking the carry of the one before. It is stored
   twice, at $70:FC14 and $70:FC15, and returned. */
uint8_t save_checksum(void)
{
    static const uint16_t order[9] = {0xFC0B, 0xFC06, 0xFC07, 0xFC0C, 0xFC0D, 0xFC0E, 0xFC08, 0xFC09, 0xFC0A};
    unsigned sum = 0, carry = 0, i;

    for (i = 0; i < 9; i++) {
        sum = (sum & 0xFF) + P8(order[i]) + carry;
        carry = sum > 0xFF;
    }
    P8(0xFC14) = (uint8_t)sum;
    P8(0xFC15) = (uint8_t)sum;
    return (uint8_t)sum;
}
