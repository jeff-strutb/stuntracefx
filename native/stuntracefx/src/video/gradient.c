/*
 * gradient.c: colour 1 by line, drawn from lists built once a course. Each
 * list is run-length coded, from the course block a table names.
 */
#include <string.h>

#include "data/video_tables.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

Gradient gradient;

/* the run lengths, from run `x` of the table; a colour each from the
   course's sky list, the colours repeated into list `out` */
static void expand_runs(int course, int list, uint16_t *out, unsigned x)
{
    unsigned run;

    for (run = 0;; run++, x++) {
        unsigned n = sky_run_length(x);
        uint16_t colour;

        if (!n)
            return;
        colour = course_sky_colour(course, list, run);
        while (n--)
            *out++ = colour;
    }
}

/* Runs' count table: course 10 uses another run table for the first list, course 1 for the second. */
void GradientBuildColours(int course)
{
    expand_runs(course, 0, gradient.day, course == 0x0A ? 0x28 : 0);
    if (course_has_night_sky(course))
        expand_runs(course, 1, gradient.night, course == 1 ? 0x50 : 0);
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
