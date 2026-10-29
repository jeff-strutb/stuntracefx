/*
 * view.c: the race's wide view (game/include/widescreen.h).
 *
 * The view is eight words (WIDESCREEN.md, the audit): the screen centre,
 * the edges, and the frustum planes' slopes. With focal length f, half-width
 * hw and half-height hh, the planes are floor(2^14 f / hw) and floor(2^13 f
 * / hh); f = 128 gives the original's words for one player and for two.
 * Until the console's wider screens exist the only wider view is a check's
 * (PORT_FOCAL): another focal length on the original's 208 x 128.
 */
#include <stdio.h>
#include <stdlib.h>

#include "widescreen.h"
#include "fullwindow.h"

/* the one-player race's centre and edges, as ViewParamsByPlayers sets
   them (two players' and the menus' differ) */
static const uint16_t k_race1p[6] = {0x0068, 0x0040, 0x0000, 0x00CF, 0x0000, 0x0080};

static int ready, focal = 128, check, active;
static long checks, differ;

static uint16_t plane_x(int f, int hw) { return (uint16_t)(((uint32_t)f << 14) / (uint32_t)hw); }
static uint16_t plane_y(int f, int hh) { return (uint16_t)(((uint32_t)f << 13) / (uint32_t)hh); }

static void report(void)
{
    fprintf(stderr, "focalcheck: %ld divisions, %ld differ\n", checks, differ);
}

static void setup(void)
{
    const char *e;

    ready = 1;
    if ((e = getenv("PORT_FOCAL")) && *e) {
        focal = atoi(e);
        /* the planes are signed 1.15 multipliers */
        if (focal < 16 || plane_x(focal, 104) >= 0x8000 || plane_y(focal, 64) >= 0x8000) {
            fprintf(stderr, "PORT_FOCAL=%s: outside 16-207\n", e);
            exit(2);
        }
    }
    if ((e = getenv("PORT_FOCALCHECK")) && *e == '1') {
        check = 1;
        /* the formula against the game's own words, one player and two */
        if (plane_x(128, 104) != 0x4EC4 || plane_y(128, 64) != 0x4000 ||
            plane_x(128, 72) != 0x71C7 || plane_y(128, 40) != 0x6666) {
            fprintf(stderr, "focalcheck: the plane formula misses the game's words\n");
            exit(1);
        }
        atexit(report);
    }
}

int ws_focal(void)
{
    if (!ready)
        setup();
    if (fw_view_active())               /* Enhanced's full-window picture */
        return fw_focal();
    return active ? focal : 128;
}

int ws_view_planes(const uint16_t w[6], uint16_t planes[2])
{
    int i;

    if (!ready)
        setup();
    active = 0;
    for (i = 0; i < 6; i++)
        if (w[i] != k_race1p[i])
            return 0;
    if (focal == 128)
        return 0;
    active = 1;
    planes[0] = plane_x(focal, 104);
    planes[1] = plane_y(focal, 64);
    return 1;
}

int ws_cull_keeps(uint16_t over, uint16_t depth)
{
    int f = ws_focal(), hw = fw_view_active() ? fw_half_width() : 104;

    /* the view's half-angle passes 45 degrees only when hw > f */
    return hw > f && (uint32_t)over * (uint32_t)f < (uint32_t)depth * (uint32_t)hw;
}

int ws_focal_check(void)
{
    if (!ready)
        setup();
    return check;
}

void ws_focal_checked(int same, uint16_t a, uint16_t b, uint16_t d)
{
    checks++;
    if (!same && ++differ <= 10)
        fprintf(stderr, "focalcheck: differs at a=%04X b=%04X d=%04X\n", a, b, d);
}
