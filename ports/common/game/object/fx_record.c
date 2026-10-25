/*
 * fx_record.c: small Super FX routines on a record, and three programs
 * the S-CPU starts.
 */
#include "game.h"
#include "physics.h"

#define FX_ARG_RECORD       FXU(0x238E)     /* a program's record, from the S-CPU */
#define FX_ARG_AMOUNT       FXW(0x2390)     /* and its amount */

/* The push (FX_PUSH) set to `amount` straight up ($8100), on when the
   amount is not negative. Returns whether it is on. */
uint16_t fx_push_set(int16_t amount)
{
    uint16_t on = amount >= 0;

    FXU(FX_PUSH) = on;
    FXW(FX_PUSH + 2) = amount;
    FXU(FX_PUSH + 4) = 0;
    FXU(FX_PUSH + 8) = 0;
    FXU(FX_PUSH + 6) = 0x8100;
    return on;
}

/* +$60 follows the height +$20 while bit 8 of +$40 is clear; held, it
   becomes 0 once below the height. */
void fx_record_hold_y(uint16_t rec)
{
    if (!(FXU(rec + 0x40) & 0x0100))
        FXU(rec + 0x60) = FXU(rec + 0x20);
    if (wrap16(FXW(rec + 0x60) - FXW(rec + 0x20)) < 0)
        FXU(rec + 0x60) = 0;
}

/* the angle from the record's place (+$1E, +$22) to (x, z), negated */
uint16_t fx_bearing_to(uint16_t rec, int16_t x, int16_t z)
{
    return (uint16_t)-fx_angle_of(wrap16(x - FXW(rec + 0x1E)), wrap16(z - FXW(rec + 0x22)));
}

/* +$6C: how closely the record's velocity points along the matrix's
   third column ($00F0): the unit velocity dotted with it; $7FFF when its
   speed +$4A is below 10 either way. */
void fx_record_alignment(uint16_t rec)
{
    int16_t v[3], speed = FXW(rec + 0x4A), r;
    uint32_t sum = 0;
    int i;

    for (i = 0; i < 3; i++)
        v[i] = FXW(rec + 0x2E + 2 * i);
    fx_normalize(v);
    for (i = 0; i < 3; i++)
        sum += (uint32_t)((int32_t)v[i] * FXW(0x00F0 + 2 * i));
    r = (int16_t)(uint16_t)((sum << 1) >> 16);
    if (speed < 0)
        speed = wrap16(-speed);
    if (wrap16(speed - 10) < 0)
        r = 0x7FFF;
    FXW(rec + 0x6C) = r;
}

/* A program: +$6A of the record at FX_ARG_RECORD raised by the amount
   times $300 / 65536, to at most $0180. */
void fx_record_raise_6a(void)
{
    uint16_t rec = FX_ARG_RECORD;
    int16_t v = wrap16(fmult(0x0300, FX_ARG_AMOUNT) + FXW(rec + 0x6A));

    if (wrap16(v - 0x0180) >= 0)
        v = 0x0180;
    FXW(rec + 0x6A) = v;
}

/* A program: +$82 lowered by the amount times $400 / 65536, to no less
   than 0. */
void fx_record_lower_82(void)
{
    uint16_t rec = FX_ARG_RECORD;
    int16_t v = wrap16(FXW(rec + 0x82) - fmult(0x0400, FX_ARG_AMOUNT));

    FXW(rec + 0x82) = v < 0 ? 0 : v;
}

/* A program: the twenty particle records' timers (+0) counted down; with
   flags (+1) bit 4 and the timer below $20 the colour (+2) follows the
   table at $14:D0BC; a record with flags bit 0 has it cleared, any other
   ends (its timer and key +$0C cleared), as does one already at 0. */
void fx_timers_program(void)
{
    uint16_t p;

    for (p = 0x273E; p < 0x273E + 20 * 14; p += 14) {
        if (pakram[p]) {
            uint8_t t = --pakram[p];

            if ((pakram[p + 1] & 0x10) && t < 0x20)
                pakram[p + 2] = rom_at(0x140000u | (uint16_t)(0xD0BC + t))[0];
            if (pakram[p + 1] & 1) {
                pakram[p + 1] &= (uint8_t)~1;
                continue;
            }
            pakram[p] = 0;
        }
        FXU(p + 0x0C) = 0;
    }
}
