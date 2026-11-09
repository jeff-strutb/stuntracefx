/*
 * fade.c: the race's fade, a level whose whole part is the brightness and
 * whose fraction is carried in $0D60. In and out by g.fade_out.
 */
#include "system/state.h"
#include "video/video.h"

static uint16_t level_get(void) { return (uint16_t)(g.fade_level << 8 | g.fade_low); }
static void level_set(uint16_t v) { g.fade_level = (uint8_t)(v >> 8); g.fade_low = (uint8_t)v; }

static uint16_t fade_in_by(uint16_t step)
{
    uint16_t sum = (uint16_t)(level_get() + step);

    if (sum >= 0x0F00)
        sum = 0x0F00;
    level_set(sum);
    return sum;
}

/* $58 down a step; below 0 becomes the fade's end, bit 15 */
static uint16_t fade_out(void)
{
    uint16_t level = level_get();

    if (!(level & 0x8000))
        level = level >= 0x58 ? (uint16_t)(level - 0x58) : 0x8000;
    level_set(level);
    return level;
}

uint16_t FadeStepFixed(void)
{
    return g.fade_out ? fade_out() : fade_in_by(0x0098);
}

/* Fading in, the step for the present brightness. */
uint16_t FadeStep(void)
{
    if (g.fade_out)
        return fade_out();
    return fade_in_by(ram_code_7e16((uint16_t)(0xAC9D + (uint8_t)(g.fade_level << 1))));
}
