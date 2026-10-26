/*
 * clock.c: the race clock, a step a frame: the race time counts up (BCD:
 * $0E62 the fraction, $0E64 seconds, $0E66 minutes) and the countdown
 * $0E68 down, borrowing from $0E70.
 */
#include "game.h"
#include "fixmath.h"

#define race_clock_on   W8(0x0E5B)
#define race_clock_step_by W16(0x0D29)

/* While the clock runs: the time up by the step, the seconds carrying at
   60 and the time held at 9:59.9999 from 10 minutes; the countdown down
   by the step, and once $0E70 is spent it stops at 0. */
void race_clock_step(void)
{
    bool c = false;
    uint16_t a;

    if (!race_clock_on)
        return;
    W16(0x0E62) = bcd_add16(W16(0x0E62), race_clock_step_by, &c);
    a = W16(0x0E64) = bcd_add16(W16(0x0E64), 0, &c);
    if (a >= 0x60) {
        W16(0x0E64) = 0;
        c = false;
        a = W16(0x0E66) = bcd_add16(W16(0x0E66), 1, &c);
        if (a >= 0x10) {
            W16(0x0E66) = 0x0009;
            W16(0x0E64) = 0x0059;
            W16(0x0E62) = 0x9999;
        }
    }
    c = true;
    W16(0x0E68) = bcd_sub16(W16(0x0E68), race_clock_step_by, &c);
    if (c)
        return;
    if (W16(0x0E70)) {
        c = true;
        W16(0x0E70) = bcd_sub16(W16(0x0E70), 1, &c);
    } else
        W16(0x0E68) = W16(0x0E70) = 0;
}
