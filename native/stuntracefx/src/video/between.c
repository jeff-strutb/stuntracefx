/*
 * between.c: the 2D drawn between updates (between.h).
 *
 * "Was" is the state the update before left: the picture keeps what pic
 * held at its draw, by update, and the first picture of the next update
 * takes it as was. A picture eases only from the update just before its
 * own; after a gap (no picture showed that update, the display slower than
 * the updates) it shows the newest state as it is.
 */
#include <math.h>
#include <string.h>

#include "nshvc/nshvc.h"
#include "video/between.h"
#include "video/video.h"

enum {
    SPRITE_FAR = 32,                    /* a sprite moved further: placed anew */
    SCROLL_FAR = 64,                    /* a layer scrolled further: set anew */
    EDGE_FAR = 64,                      /* a window edge moved further */
};

int32_t tween_position(int32_t was, int32_t now, float between, int32_t far)
{
    int32_t d = now - was;

    if (d > far || d < -far)
        return between < 0.5f ? was : now;
    return was + (int32_t)lrintf((float)d * between);
}

int32_t tween_wrapped(int32_t was, int32_t now, float between, int32_t far, int32_t period)
{
    int64_t d = ((int64_t)now - was) % period;

    if (d < 0)
        d += period;
    if (d >= period / 2)
        d -= period;
    if (d > far || d < -far)
        return between < 0.5f ? was : now;
    return tween_position(now - (int32_t)d, now, between, far);
}

/* A scroll's ease: the shorter way round 1024 (the scroll registers'
   reach), or round 256 for a value kept in a byte; a jump past both keeps
   the nearer of the two. */
static int32_t scroll_tween(int32_t was, int32_t now, float between);

static int32_t value_tween(int32_t was, int32_t now, float t, int32_t far, int32_t period)
{
    if (period == TWEEN_SCROLL)
        return scroll_tween(was, now, t);
    return period > 0 ? tween_wrapped(was, now, t, far, period) : tween_position(was, now, t, far);
}

static int32_t scroll_tween(int32_t was, int32_t now, float between)
{
    static const int32_t period[2] = { 1024, 256 };
    int k;

    for (k = 0; k < 2; k++) {
        int64_t d = ((int64_t)now - was) % period[k];

        if (d < 0)
            d += period[k];
        if (d >= period[k] / 2)
            d -= period[k];
        if (d <= SCROLL_FAR && d >= -SCROLL_FAR)
            return tween_position(now - (int32_t)d, now, between, SCROLL_FAR);
    }
    return between < 0.5f ? was : now;
}

void tween_table(TweenTable *keep, int32_t *values, int n, int32_t far, int32_t period)
{
    uint64_t u = nshvc_updates();
    float t = video_between();
    int i;

    if (n > 1024)
        n = 1024;
    if (keep->at + 1 == u) {
        memcpy(keep->was, keep->now, sizeof keep->was);
        keep->was_n = keep->n;
        keep->have = 1;
    } else if (keep->at != u) {
        keep->have = 0;
    }
    memcpy(keep->now, values, (size_t)n * sizeof values[0]);
    keep->n = n;
    keep->at = u;
    if (!keep->have || keep->was_n != n || t >= 1.0f)
        return;
    for (i = 0; i < n; i++)
        values[i] = value_tween(keep->was[i], values[i], t, far, period);
}

/* ---- the picture's moving 2D ---- */

static Picture game_pic, was_pic, kept_pic;    /* the game's own; the update before's; the last kept */
static uint64_t kept_at = UINT64_MAX;
static int have_was, drawing;
static float between_now = 1.0f;
static void (*menu_ease)(Picture *, float);

float video_between(void)
{
    return drawing ? between_now : 1.0f;
}

int video_between_drawing(void)
{
    return drawing;
}

void video_between_menu_set(void (*ease)(Picture *shown, float between))
{
    menu_ease = ease;
}

static int same_look(const nshvc_sprite *a, const nshvc_sprite *b)
{
    return a->tiles.id == b->tiles.id && a->tile == b->tile && a->width == b->width && a->height == b->height &&
           a->palette == b->palette && a->priority == b->priority && a->hflip == b->hflip && a->vflip == b->vflip;
}

/* the same screen: the race's kind, split and course screen unchanged */
static int same_screen(const Picture *a, const Picture *b)
{
    return a->race.on == b->race.on && a->race.split == b->race.split && a->race.course_screen == b->race.course_screen &&
           a->mode == b->mode;
}

/* The race's screens: the sprites by slot (the race's code writes its HUD's
   into fixed slots), each that kept its look; the layers' scrolls; the
   windows' edges; the two halves' scrolls (a finished player's sliding
   panel; the hills between steps take the 3D's angles instead,
   race_two_player_list). */
static void race_ease(Picture *p, const Picture *w, float t)
{
    uint32_t i, n = p->sprite_count < w->sprite_count ? p->sprite_count : w->sprite_count;

    if (n > 128)
        n = 128;
    for (i = 0; i < n; i++)
        if (same_look(&p->sprites[i], &w->sprites[i])) {
            p->sprites[i].x = tween_position(w->sprites[i].x, p->sprites[i].x, t, SPRITE_FAR);
            p->sprites[i].y = tween_position(w->sprites[i].y, p->sprites[i].y, t, SPRITE_FAR);
        }
    for (i = 0; i < 3; i++) {
        p->bg[i].scroll_x = scroll_tween(w->bg[i].scroll_x, p->bg[i].scroll_x, t);
        p->bg[i].scroll_y = scroll_tween(w->bg[i].scroll_y, p->bg[i].scroll_y, t);
    }
    for (i = 0; i < 2; i++) {
        p->windows.w[i].left = tween_position(w->windows.w[i].left, p->windows.w[i].left, t, EDGE_FAR);
        p->windows.w[i].right = tween_position(w->windows.w[i].right, p->windows.w[i].right, t, EDGE_FAR);
        if (p->race.panel[i] == w->race.panel[i]) {
            p->race.backdrop_x[i] = scroll_tween(w->race.backdrop_x[i], p->race.backdrop_x[i], t);
            p->race.backdrop_y[i] = scroll_tween(w->race.backdrop_y[i], p->race.backdrop_y[i], t);
        }
    }
}

void video_between_begin(float between)
{
    uint64_t u = nshvc_updates();

    between_now = between < 0.0f ? 0.0f : between > 1.0f ? 1.0f : between;
    game_pic = pic;
    drawing = 1;
    if (kept_at + 1 == u) {
        was_pic = kept_pic;
        have_was = 1;
    } else if (kept_at != u) {
        have_was = 0;
    }
    kept_pic = pic;
    kept_at = u;
    if (!pic.race.on) {
        if (menu_ease)
            menu_ease(&pic, between_now);
        return;
    }
    if (have_was && between_now < 1.0f && same_screen(&pic, &was_pic))
        race_ease(&pic, &was_pic, between_now);
}

void video_between_end(void)
{
    if (!drawing)
        return;
    pic = game_pic;
    drawing = 0;
}
