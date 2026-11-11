/*
 * perf.c: where the time goes (Jeff, 2026-11-11: the game struggles to keep
 * above 60 pictures a second, even with simple pictures; quantify it before
 * making it faster).
 *
 * With SRFX_PROFILE=1 in the environment, once a second of real time, one
 * line to stderr: the pictures and updates in that second; the game's update
 * (each, mean and worst); the game's draw (each picture, mean and worst) and
 * its stages (the engine's profiling zones, perf.h, a picture's mean); the
 * engine's building of a picture after the game's draw (engine) and the
 * render thread's drawing and showing of it (render: on Metal this waits for
 * the GPU's last picture before encoding this one, so it is the GPU's time
 * not hidden behind the game's); and the draw calls and vertices a picture
 * sends. The engine's numbers are of the picture before (nshvc_profile_frame
 * gives the last finished); over a second the means are the same.
 *
 * The engine builds a picture only after the render thread has taken the one
 * before (one picture in flight), so a picture's time is about the larger of
 * the game's draw plus the engine's build, and the render thread's.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nshvc/nshvc.h"
#include "system/perf.h"

const char perf_tween_state[] = "tween state";
const char perf_frame[] = "frame program";
const char perf_ground[] = "ground";
const char perf_far[] = "far objects";
const char perf_near[] = "near objects";
const char perf_compose[] = "compose";
const char perf_video[] = "2D";

static const char *const zone[] = { perf_tween_state, perf_frame, perf_ground, perf_far, perf_near, perf_compose, perf_video };
enum { ZONES = sizeof zone / sizeof zone[0] };

static int on = -1;
static double from = -1, update_at, draw_at;
static struct {
    unsigned pictures, updates;
    double update, update_worst, draw, draw_worst, engine, engine_worst, render, render_worst;
    double zone[ZONES];
    double calls, vertices;
} s;
static uint32_t calls_now, vertices_now;

static int enabled(void)
{
    if (on < 0) {
        const char *e = getenv("SRFX_PROFILE");

        on = e && *e && strcmp(e, "0");
    }
    return on;
}

void perf_draw_call(uint32_t vertices)
{
    calls_now++;
    vertices_now += vertices;
}

void perf_update_begin(void)
{
    if (enabled())
        update_at = nshvc_seconds();
}

void perf_update_end(void)
{
    double ms;

    if (!enabled())
        return;
    ms = (nshvc_seconds() - update_at) * 1000.0;
    s.updates++;
    s.update += ms;
    if (ms > s.update_worst)
        s.update_worst = ms;
}

void perf_draw_begin(void)
{
    if (!enabled())
        return;
    draw_at = nshvc_seconds();
    calls_now = vertices_now = 0;
}

void perf_draw_end(void)
{
    nshvc_profile p;
    nshvc_display d;
    double now, ms;
    uint32_t i, k;

    if (!enabled())
        return;
    now = nshvc_seconds();
    ms = (now - draw_at) * 1000.0;
    p = nshvc_profile_frame();
    s.pictures++;
    s.draw += ms;
    if (ms > s.draw_worst)
        s.draw_worst = ms;
    s.engine += p.engine_ms;
    if (p.engine_ms > s.engine_worst)
        s.engine_worst = p.engine_ms;
    s.render += p.render_ms;
    if (p.render_ms > s.render_worst)
        s.render_worst = p.render_ms;
    for (i = 0; i < p.zone_count; i++)
        for (k = 0; k < ZONES; k++)
            if (p.zones[i].name == zone[k])
                s.zone[k] += p.zones[i].ms;
    s.calls += calls_now;
    s.vertices += vertices_now;
    if (from < 0)
        from = now;
    if (now - from < 1.0)
        return;
    {
        double secs = now - from, n = s.pictures ? (double)s.pictures : 1, u = s.updates ? (double)s.updates : 1;

        d = nshvc_display_info();
        fprintf(stderr,
                "perf %dx%d: %.1f pictures/s, %.1f updates/s | update %.2f ms (worst %.2f) | draw %.2f (worst %.2f):"
                " tween state %.2f, frame program %.2f [ground %.2f, far objects %.2f, near objects %.2f],"
                " compose %.2f, 2D %.2f | engine %.2f (worst %.2f) | render %.2f (worst %.2f) |"
                " %.0f draw calls, %.0f vertices a picture\n",
                d.width, d.height, s.pictures / secs, s.updates / secs, s.update / u, s.update_worst, s.draw / n,
                s.draw_worst, s.zone[0] / n, s.zone[1] / n, s.zone[2] / n, s.zone[3] / n, s.zone[4] / n, s.zone[5] / n,
                s.zone[6] / n, s.engine / n, s.engine_worst, s.render / n, s.render_worst, s.calls / n, s.vertices / n);
    }
    memset(&s, 0, sizeof s);
    from = now;
}
