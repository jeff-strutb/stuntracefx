/*
 * lap.c: lap splits. The race clock (race_time_fraction, race_time_seconds,
 * race_time_minutes, all BCD) is marked at each lap; a lap's time is the
 * clock minus the mark, and the clock becomes the new mark. The data bank
 * is work RAM ($7E), so the lap's bytes are named directly.
 */
#include <stdbool.h>
#include "game.h"
#include "lap.h"
#include "fixmath.h"

#define lap_mark            (*(BcdTime *)&wram[0x0EDB])     /* the last mark */
#define race_clock_copy     (*(BcdTime *)&wram[0x0EE7])     /* RaceTimeKeep's copy */
#define lap_split_at(x)     ((BcdTime *)&wram[0x0EDE + (x)])  /* the lap's time, slot x */
#define lap_clock_at(x)     ((BcdTime *)&wram[0x0EF0 + (x)])  /* the clock at the lap, slot x */
#define hud_lap_min         W8(0x0F69)                       /* the HUD's digits of the lap */
#define hud_lap_sec_hi      W8(0x0F6A)
#define hud_lap_sec_lo      W8(0x0F6B)
#define hud_lap_hund_hi     W8(0x0F6C)
#define hud_lap_hund_lo     W8(0x0F6D)

/* Records lap slot x: the lap's time (clock minus mark, the seconds
   borrowing a minute from the minutes copy) goes to the slot; the clock is
   copied to the lap's clock slot and becomes the mark; the clock's digits
   go to the HUD. Returns the race minutes. */
uint8_t lap_split(uint8_t x)
{
    bool carry = true;
    uint8_t mins_copy = race_time_minutes, v;
    uint8_t frac, secs, mins;

    v = bcd_sub8(race_time_fraction, lap_mark.hund, &carry);
    lap_split_at(x)->hund = v;

    v = bcd_sub8(race_time_seconds, lap_mark.sec, &carry);
    if (!carry) {
        /* a borrow: add a minute's 60 seconds, and take the minute */
        carry = false;
        v = bcd_add8(v, 0x60, &carry);
        mins_copy--;                    /* a binary decrement, as the original's DEC */
    }
    lap_split_at(x)->sec = v;

    carry = true;
    v = bcd_sub8(mins_copy, lap_mark.min, &carry);
    lap_split_at(x)->min = v;

    /* the clock becomes the mark, and is kept with the lap */
    frac = race_time_fraction;
    secs = race_time_seconds;
    mins = race_time_minutes;
    lap_mark.hund = frac;
    lap_mark.sec = secs;
    lap_mark.min = mins;
    lap_clock_at(x)->hund = frac;
    lap_clock_at(x)->sec = secs;
    lap_clock_at(x)->min = mins;

    /* the HUD's digits: hundredths and seconds as two digits each */
    hud_lap_hund_lo = frac & 0x0F;
    hud_lap_hund_hi = frac >> 4;
    hud_lap_sec_lo = secs & 0x0F;
    hud_lap_sec_hi = secs >> 4;
    hud_lap_min = mins;
    return mins;
}

/* The race clock copied to $0EE7-$0EE9 (hundredths, seconds, minutes). */
void race_time_keep(void)
{
    race_clock_copy.hund = race_time_fraction;
    race_clock_copy.sec = race_time_seconds;
    race_clock_copy.min = race_time_minutes;
}
