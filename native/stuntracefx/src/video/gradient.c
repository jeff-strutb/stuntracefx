/*
 * gradient.c: colour 1 by line, drawn from lists built once a course. Each
 * list is run-length coded, from the course block a table names.
 */
#include <string.h>

#include "data/cart.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

Gradient gradient;

/* the run lengths, from offset `x` of the table; a colour word each from the
   block, the colours repeated into list `out` */
static void expand_runs(uint8_t bank, uint16_t *block, uint16_t *out, unsigned x)
{
    for (;;) {
        unsigned n = cart8(0x1F, (uint16_t)(0xB44E + x));
        uint16_t colour;

        if (!n)
            return;
        colour = cart16(bank, (uint16_t)(*block + 0x20));
        while (n--)
            *out++ = colour;
        *block = (uint16_t)(*block + 2);
        x++;
    }
}

/* Runs' count table: course 10 uses another run table for the first list, course 1 for the second. */
void GradientBuildColours(int course)
{
    int i = course * 3;
    uint8_t bank = cart8(0x03, (uint16_t)(0xF6F3 + i));
    uint16_t block = cart16(0x03, (uint16_t)(0xF6F1 + i)), second;

    expand_runs(bank, &block, gradient.day, course == 0x0A ? 0x28 : 0);
    second = cart16(0x03, (uint16_t)(0xF745 + i));
    if (second) {
        block = second;
        expand_runs(bank, &block, gradient.night, course == 1 ? 0x50 : 0);
    }
    memset(gradient.black, 0, sizeof gradient.black);
}

/* colour number `n` from the block's start */
uint16_t gradient_colour(uint32_t n)
{
    if (n < GRADIENT_AT_DAY)
        return gradient.band[n];
    if (n < GRADIENT_AT_NIGHT)
        return gradient.day[n - GRADIENT_AT_DAY];
    if (n < GRADIENT_AT_BLACK)
        return gradient.night[n - GRADIENT_AT_NIGHT];
    n -= GRADIENT_AT_BLACK;
    return n < 128 ? gradient.black[n] : 0;
}

/* The gradient's two starts by line, as the course screens set them up: the
   first band from the bands' start, the second from their second half. */
void GradientSetup(void)
{
    pic.race.gradient_start[0] = 0;
    pic.race.gradient_start[1] = 128;
}

/* The two-player screen: each half's gradient starts at its view's place in the day list. */
/* @routine 7E:B2A5 GradientPointersToBank7F */
void GradientStartsFromViews(void)
{
    pic.race.gradient_start[0] = (uint16_t)(GRADIENT_AT_DAY + gradient.view_offset[0] / 2);
    pic.race.gradient_start[1] = (uint16_t)(GRADIENT_AT_DAY + gradient.view_offset[1] / 2);
}
