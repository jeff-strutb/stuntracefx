/*
 * time.c: the race clock and the time left, kept in binary-coded decimal;
 * lap splits.
 */
#include "data/cart.h"
#include "race/race.h"
#include "system/state.h"

/* Decimal add of two four-digit values and a carry in; *carry is the carry out. */
static uint16_t bcd_add16(uint16_t a, uint16_t b, unsigned *carry)
{
    uint16_t r = 0;
    unsigned c = *carry;

    for (int shift = 0; shift < 16; shift += 4) {
        unsigned d = ((a >> shift) & 0xFu) + ((b >> shift) & 0xFu) + c;

        c = d > 9;
        if (c)
            d -= 10;
        r |= (uint16_t)(d << shift);
    }
    *carry = c;
    return r;
}

/* Decimal subtract, a - b - (1 - carry); *carry = 1 when there was no borrow. */
static uint16_t bcd_sub16(uint16_t a, uint16_t b, unsigned *carry)
{
    uint16_t r = 0;
    unsigned borrow = !*carry;

    for (int shift = 0; shift < 16; shift += 4) {
        int d = (int)((a >> shift) & 0xFu) - (int)((b >> shift) & 0xFu) - (int)borrow;

        borrow = d < 0;
        if (borrow)
            d += 10;
        r |= (uint16_t)(d << shift);
    }
    *carry = !borrow;
    return r;
}

/* @routine 03:EFBB CourseIndex12: tables with twelve bytes per course and three per entry. */
static uint8_t course_index_12(uint8_t entry)
{
    return (uint8_t)((uint8_t)(((uint8_t)(g.course * 2) + g.course) * 4) + (uint8_t)(entry * 3));
}

/* @routine 03:EF9F RaceTimeAddD815, 03:EF93 RaceTimeAddD814: the time left gains this course's value. */
void RaceTimeAddD815(void)
{
    unsigned c = 0;

    g.time_left = bcd_add16(cart8(0x03, (uint16_t)(0xD815 + course_index_12(g.car[0]))), g.time_left, &c);
}

void RaceTimeAddD814(void)
{
    unsigned c = 0;

    g.time_left = bcd_add16(cart8(0x03, (uint16_t)(0xD814 + course_index_12(g.car[0]))), g.time_left, &c);
}

/*
 * @routine 03:EF3F RaceTimeInit
 * WHAT IT DOES: The time left's start by the mode: the course's entry, the
 * class's with the time override, in stunt mode by course and car, or 0.
 */
void RaceTimeInit(void)
{
    uint8_t v = 0;

    g.countdown_fraction = 0x9999;
    g.countdown_fraction_2 = 0x9999;
    if (g.hud_mode == 0) {
        if (g.time_override == 0)
            v = cart8(0x03, (uint16_t)(0xD813 + course_index_12(g.car[0])));
        else
            v = cart8(0x03, (uint16_t)(0xD810 + g.course_class));
    } else if (g.hud_mode == 1 && !g.hud_blank) {
        v = cart8(0x03, (uint16_t)(0xD8A3 + (uint8_t)((uint8_t)((uint8_t)(g.course - 0x0C) * 4) + g.car[0])));
    }
    g.time_left = v;
}

/*
 * @routine 7E:B072 RaceClockStep
 * WHAT IT DOES: While the clock runs, adds the step to the race time and
 * takes it off the countdown, borrowing from the time left; the clock holds
 * at 9:59.9999 and the countdown stops when the time left is gone.
 */
void RaceClockStep(void)
{
    unsigned c;
    uint16_t a;

    if (!g.finish_mark)
        return;
    c = 0;
    g.clock_fraction = bcd_add16(g.clock_fraction, g.clock_step, &c);
    a = bcd_add16(g.clock_seconds, 0, &c);
    g.clock_seconds = a;
    if (a >= 0x0060) {
        g.clock_seconds = 0;
        c = 0;
        a = bcd_add16(g.clock_minutes, 1, &c);
        g.clock_minutes = a;
        if (a >= 0x0010) {
            g.clock_minutes = 0x0009;
            g.clock_seconds = 0x0059;
            g.clock_fraction = 0x9999;
        }
    }
    c = 1;
    g.countdown_fraction = bcd_sub16(g.countdown_fraction, g.clock_step, &c);
    if (!c) {
        if (g.time_left) {
            c = 1;
            g.time_left = bcd_sub16(g.time_left, 1, &c);
        } else {
            g.countdown_fraction = 0;
            g.time_left = 0;
        }
    }
}

/*
 * @routine 07:D5C7 LapSplit
 * WHAT IT DOES: Records a lap at `offset` (0, 3, 6): the clock minus the
 * last mark into the lap's times, the clock itself kept, the mark moved to
 * the clock and its digits spread out for the HUD.
 */
void LapSplitTimes(int offset)
{
    uint8_t frac = (uint8_t)(g.clock_fraction >> 8);
    uint8_t secs = (uint8_t)g.clock_seconds, mins = (uint8_t)g.clock_minutes;
    uint8_t minutes = mins;
    unsigned c = 1;
    uint16_t v;

    v = bcd_sub16(frac, g.lap_mark[0], &c);
    g.lap_splits[offset] = (uint8_t)v;
    v = bcd_sub16(secs, g.lap_mark[1], &c);
    if (!c) {
        c = 0;
        v = bcd_add16(v, 0x60, &c);
        minutes--;
    }
    g.lap_splits[offset + 1] = (uint8_t)v;
    c = 1;
    g.lap_splits[offset + 2] = (uint8_t)bcd_sub16(minutes, g.lap_mark[2], &c);
}

void LapSplit(int offset)
{
    uint8_t frac = (uint8_t)(g.clock_fraction >> 8);
    uint8_t secs = (uint8_t)g.clock_seconds, mins = (uint8_t)g.clock_minutes;

    LapSplitTimes(offset);
    g.lap_mark[0] = frac;
    g.lap_clock[offset] = frac;
    g.best_digits[4] = frac & 0x0F;
    g.best_digits[3] = frac >> 4;
    g.lap_mark[1] = secs;
    g.lap_clock[offset + 1] = secs;
    g.best_digits[2] = secs & 0x0F;
    g.best_digits[1] = secs >> 4;
    g.lap_mark[2] = mins;
    g.lap_clock[offset + 2] = mins;
    g.best_digits[0] = mins;
}

/* @routine 07:D5B2 RaceTimeKeep */
void RaceTimeKeep(void)
{
    g.race_time_kept[0] = (uint8_t)(g.clock_fraction >> 8);
    g.race_time_kept[1] = (uint8_t)g.clock_seconds;
    g.race_time_kept[2] = (uint8_t)g.clock_minutes;
}
