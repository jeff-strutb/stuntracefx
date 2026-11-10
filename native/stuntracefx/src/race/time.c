/*
 * time.c: the race clock and the time left, kept in binary-coded decimal;
 * lap splits.
 */
#include "data/cart.h"
#include "race/race.h"
#include "system/state.h"

/* The game keeps its times and counts as four decimal digits, a nibble
   each (the original's decimal mode). Their value, and a value (taken
   modulo 10000) as such a number. */
unsigned BcdValue(uint16_t v)
{
    return (v >> 12 & 0xFu) * 1000u + (v >> 8 & 0xFu) * 100u + (v >> 4 & 0xFu) * 10u + (v & 0xFu);
}

uint16_t BcdOf(unsigned n)
{
    n %= 10000u;
    return (uint16_t)((n / 1000u) << 12 | (n / 100u % 10u) << 8 | (n / 10u % 10u) << 4 | n % 10u);
}

/* Four-digit decimal sum and difference, wrapping as four digits do. */
uint16_t BcdAdd(uint16_t a, uint16_t b)
{
    return BcdOf(BcdValue(a) + BcdValue(b));
}

uint16_t BcdSub(uint16_t a, uint16_t b)
{
    return BcdOf(BcdValue(a) + 10000u - BcdValue(b));
}

/* A time as the results and the HUD keep it, [0] hundredths, [1] seconds,
   [2] minutes, two decimal digits each: its value in hundredths of a
   second, and a value back into one, the minutes wrapping at 100 as two
   digits do. */
unsigned TimeHundredths(const uint8_t t[3])
{
    return BcdValue(t[2]) * 6000u + BcdValue(t[1]) * 100u + BcdValue(t[0]);
}

void TimeSetHundredths(uint8_t t[3], unsigned hundredths)
{
    t[0] = (uint8_t)BcdOf(hundredths % 100u);
    t[1] = (uint8_t)BcdOf(hundredths / 100u % 60u);
    t[2] = (uint8_t)BcdOf(hundredths / 6000u % 100u);
}

/* @routine 03:EFBB CourseIndex12: tables with twelve bytes per course and three per entry. */
static uint8_t course_index_12(uint8_t entry)
{
    return (uint8_t)((uint8_t)(((uint8_t)(g.course * 2) + g.course) * 4) + (uint8_t)(entry * 3));
}

/* @routine 03:EF9F RaceTimeAddD815, 03:EF93 RaceTimeAddD814 (TimeLeftAddLap3, TimeLeftAddLap2):
   the time left gains this course's time for the lap ahead ($03:D815 the third, $D814 the second). */
void TimeLeftAddLap3(void)
{
    g.time_left = BcdAdd(cart8(0x03, (uint16_t)(0xD815 + course_index_12(g.car[0]))), g.time_left);
}

void TimeLeftAddLap2(void)
{
    g.time_left = BcdAdd(cart8(0x03, (uint16_t)(0xD814 + course_index_12(g.car[0]))), g.time_left);
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
 * at 9:59.9999 and the countdown stops when the time left is gone. The
 * fractions are in ten-thousandths of a second.
 */
void RaceClockStep(void)
{
    unsigned step = BcdValue(g.clock_step), fraction, seconds, countdown;

    if (!g.finish_mark)
        return;
    fraction = BcdValue(g.clock_fraction) + step;
    seconds = BcdValue(g.clock_seconds) + fraction / 10000u;
    g.clock_fraction = BcdOf(fraction);
    g.clock_seconds = BcdOf(seconds);
    if (seconds >= 60) {
        unsigned minutes = BcdValue(g.clock_minutes) + 1;

        g.clock_seconds = 0;
        g.clock_minutes = BcdOf(minutes);
        if (minutes >= 10) {
            g.clock_minutes = 0x0009;
            g.clock_seconds = 0x0059;
            g.clock_fraction = 0x9999;
        }
    }
    countdown = BcdValue(g.countdown_fraction);
    g.countdown_fraction = BcdOf(countdown + 10000u - step);
    if (countdown >= step)
        return;
    if (g.time_left) {
        g.time_left = BcdSub(g.time_left, 1);
    } else {
        g.countdown_fraction = 0;
        g.time_left = 0;
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
    int hundredths = (int)BcdValue(g.clock_fraction >> 8) - (int)BcdValue(g.lap_mark[0]);
    int seconds = (int)BcdValue((uint8_t)g.clock_seconds) - (int)BcdValue(g.lap_mark[1]);
    uint8_t minutes = (uint8_t)g.clock_minutes;

    if (hundredths < 0) {
        hundredths += 100;
        seconds--;
    }
    if (seconds < 0) {
        seconds += 60;
        minutes--;
    }
    g.lap_splits[offset] = (uint8_t)BcdOf((unsigned)hundredths);
    g.lap_splits[offset + 1] = (uint8_t)BcdOf((unsigned)seconds);
    g.lap_splits[offset + 2] = (uint8_t)BcdSub(minutes, g.lap_mark[2]);
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
