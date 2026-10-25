/*
 * fx_init.c: the Super FX program that sets up its RAM at power on.
 */
#include "game.h"

/* The record pools cleared to one free block each ($0CD8 bytes at $1098,
   $05BE at $1D70) and the starting words set. (The original first writes
   $1234 to $235C and reads it back, setting $26F0 to $0F on a mismatch,
   which no machine gives.) */
void fx_init_program(void)
{
    static const uint16_t zeros[] = {0x2732, 0x26F0, 0x2B94, 0x2B92, 0x2684, 0x2686, 0x1090, 0x2470, 0x273C, 0x235C};
    unsigned i;

    for (i = 0; i < 0x0B2C; i++)
        FXU(0x1098 + 2 * i) = 0;
    FXU(0x2636) = 0x0100;
    FXU(0x2382) = 0xFFFF;
    FXU(0x2380) = 0xFFFF;
    FXU(0x2642) = 0xFFFF;
    FXU(0x1098) = 0;
    FXU(0x109A) = 0x0CD8;
    FXU(0x1D70) = 0;
    FXU(0x1D72) = 0x05BE;
    for (i = 0; i < sizeof zeros / sizeof zeros[0]; i++)
        FXU(zeros[i]) = 0;
    FXU(0x2472) = 3;
}
