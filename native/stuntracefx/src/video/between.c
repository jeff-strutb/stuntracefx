/*
 * between.c: the 2D drawn between updates (between.h).
 *
 * At the start of each update the world the update before left is kept:
 * pic, and the rest video reads while it draws (g, windows, palettes,
 * gradient); the art and maps that update wrote are written then, not
 * while it ran. A picture during the update shows that kept world, with
 * its moving parts eased towards the newest by `between`.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "nshvc/nshvc.h"
#include "system/state.h"
#include "video/between.h"
#include "video/video.h"

enum {
    SPRITE_FAR = 32,                    /* a sprite moved further: placed anew */
    SCROLL_FAR = 64,                    /* a layer scrolled further: set anew */
    EDGE_FAR = 64,                      /* a window edge moved further */
};

/* ---- places between two updates ---- */

int32_t tween_position(int32_t was, int32_t now, float between, int32_t far)
{
    int32_t d = now - was;

    if (d > far || d < -far)
        return between < 1.0f ? was : now;
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
        return between < 1.0f ? was : now;
    return tween_position(now - (int32_t)d, now, between, far);
}

/* A scroll's ease: the shorter way round 1024 (the scroll registers'
   reach), or round 256 for a value kept in a byte; a jump past both is set
   anew at the newest update. */
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
    return between < 1.0f ? was : now;
}

static int32_t value_tween(int32_t was, int32_t now, float t, int32_t far, int32_t period)
{
    if (period == TWEEN_SCROLL)
        return scroll_tween(was, now, t);
    return period > 0 ? tween_wrapped(was, now, t, far, period) : tween_position(was, now, t, far);
}

void tween_lines(int32_t *values, const int32_t *now, int n, int32_t far, int32_t period)
{
    float t = video_between();
    int i;

    if (t >= 1.0f) {
        memcpy(values, now, (size_t)n * sizeof values[0]);
        return;
    }
    for (i = 0; i < n; i++)
        values[i] = value_tween(values[i], now[i], t, far, period);
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

/* ---- the resource writes of an update, run at the next update's start ---- */

enum { WRITE_TILES, WRITE_MAP, WRITE_COPY, WRITE_FILL };

typedef struct {
    int kind;
    uint32_t bytes;                     /* the data's (a fill's: the bytes it fills) */
    uint32_t carried;                   /* the data that follows */
    union {
        struct { nshvc_tiles set; uint32_t first, count; nshvc_tile_format format; } tiles;
        struct { nshvc_map map; uint32_t x, y, w, h; } map;
        struct { void *to; uint8_t byte; } copy;
    } u;
} Write;

static uint8_t *queue;
static size_t queue_used, queue_size;
static int deferring;

static size_t write_room(uint32_t carried)
{
    return (sizeof(Write) + carried + 15u) & ~(size_t)15u;
}

static void queue_add(const Write *w, const void *data)
{
    size_t room = write_room(w->carried);

    if (queue_used + room > queue_size) {
        size_t size = queue_size ? queue_size : (size_t)1 << 16;
        uint8_t *bigger;

        while (queue_used + room > size)
            size *= 2;
        bigger = (uint8_t *)realloc(queue, size);
        if (!bigger)
            abort();                    /* no memory left for the game's own writes */
        queue = bigger;
        queue_size = size;
    }
    memcpy(queue + queue_used, w, sizeof *w);
    if (w->carried)
        memcpy(queue + queue_used + sizeof *w, data, w->carried);
    queue_used += room;
}

static void queue_run(void)
{
    size_t at = 0;

    while (at < queue_used) {
        Write w;
        const uint8_t *data = queue + at + sizeof w;

        memcpy(&w, queue + at, sizeof w);
        switch (w.kind) {
        case WRITE_TILES:
            nshvc_tiles_write(w.u.tiles.set, w.u.tiles.first, data, w.u.tiles.count, w.u.tiles.format);
            break;
        case WRITE_MAP:
            nshvc_map_write_snes(w.u.map.map, w.u.map.x, w.u.map.y, w.u.map.w, w.u.map.h,
                                 (const uint16_t *)(const void *)data);
            break;
        case WRITE_COPY:
            memcpy(w.u.copy.to, data, w.bytes);
            break;
        default:
            memset(w.u.copy.to, w.u.copy.byte, w.bytes);
            break;
        }
        at += write_room(w.carried);
    }
    queue_used = 0;
}

int video_deferring(void)
{
    return deferring;
}

void video_tiles_write(nshvc_tiles set, uint32_t first, const void *data, uint32_t count, nshvc_tile_format format, uint32_t bytes)
{
    Write w;

    if (!deferring) {
        nshvc_tiles_write(set, first, data, count, format);
        return;
    }
    memset(&w, 0, sizeof w);
    w.kind = WRITE_TILES;
    w.bytes = w.carried = bytes;
    w.u.tiles.set = set;
    w.u.tiles.first = first;
    w.u.tiles.count = count;
    w.u.tiles.format = format;
    queue_add(&w, data);
}

void video_map_write(nshvc_map map, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *words)
{
    Write q;

    if (!deferring) {
        nshvc_map_write_snes(map, x, y, w, h, words);
        return;
    }
    memset(&q, 0, sizeof q);
    q.kind = WRITE_MAP;
    q.bytes = q.carried = w * h * 2u;
    q.u.map.map = map;
    q.u.map.x = x;
    q.u.map.y = y;
    q.u.map.w = w;
    q.u.map.h = h;
    queue_add(&q, words);
}

void video_copy(void *to, const void *from, uint32_t bytes)
{
    Write w;

    if (!deferring) {
        memmove(to, from, bytes);
        return;
    }
    memset(&w, 0, sizeof w);
    w.kind = WRITE_COPY;
    w.bytes = w.carried = bytes;
    w.u.copy.to = to;
    queue_add(&w, from);
}

void video_fill(void *to, uint8_t byte, uint32_t bytes)
{
    Write w;

    if (!deferring) {
        memset(to, byte, bytes);
        return;
    }
    memset(&w, 0, sizeof w);
    w.kind = WRITE_FILL;
    w.bytes = bytes;
    w.carried = 0;
    w.u.copy.to = to;
    w.u.copy.byte = byte;
    queue_add(&w, NULL);
}

/* ---- the world an update left ---- */

static struct {
    Picture pic;
    Game g;
    Windows windows;
    Palettes palettes;
    Gradient gradient;
} was, game;                            /* the update before's; the game's own while a picture draws */
static int have_was, drawing, eased, colours_in, world_in;
static float between_now = 1.0f;
static void (*menu_ease)(Picture *, float), (*menu_ease_was)(Picture *, float);

void video_update_begin(void)
{
    queue_run();                        /* the update before's writes: this update's pictures show them */
    was.pic = pic;
    was.g = g;
    was.windows = windows;
    was.palettes = palettes;
    was.gradient = gradient;
    have_was = 1;
    deferring = 1;
}

void video_update_end(void)
{
    deferring = 0;
}

float video_between(void)
{
    return drawing ? between_now : 1.0f;
}

int video_between_drawing(void)
{
    return drawing;
}

int video_between_eased(void)
{
    return drawing && eased;
}

const Picture *video_between_newest(void)
{
    return drawing ? &game.pic : &pic;
}

void video_between_menu_set(void (*ease)(Picture *shown, float between))
{
    menu_ease = ease;
}

void video_between_menu_was_set(void (*ease)(Picture *shown, float between))
{
    menu_ease_was = ease;
}

int video_between_menus_whole(void)
{
    return menu_ease_was != NULL;
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

/* The race's screens: the update before's picture `p`, its moving parts
   towards the newest `now`: the sprites by slot (the race's code writes
   its HUD's into fixed slots), each that kept its look; the layers'
   scrolls; the windows' edges; the two halves' scrolls (a finished
   player's sliding panel; the hills between steps take the 3D's angles
   instead, race_two_player_list). */
static void race_ease(Picture *p, const Picture *now, float t)
{
    uint32_t i, n = p->sprite_count < now->sprite_count ? p->sprite_count : now->sprite_count;

    if (n > 128)
        n = 128;
    for (i = 0; i < n; i++)
        if (same_look(&p->sprites[i], &now->sprites[i])) {
            p->sprites[i].x = tween_position(p->sprites[i].x, now->sprites[i].x, t, SPRITE_FAR);
            p->sprites[i].y = tween_position(p->sprites[i].y, now->sprites[i].y, t, SPRITE_FAR);
        }
    for (i = 0; i < 3; i++) {
        p->bg[i].scroll_x = scroll_tween(p->bg[i].scroll_x, now->bg[i].scroll_x, t);
        p->bg[i].scroll_y = scroll_tween(p->bg[i].scroll_y, now->bg[i].scroll_y, t);
    }
    for (i = 0; i < 2; i++) {
        p->windows.w[i].left = tween_position(p->windows.w[i].left, now->windows.w[i].left, t, EDGE_FAR);
        p->windows.w[i].right = tween_position(p->windows.w[i].right, now->windows.w[i].right, t, EDGE_FAR);
        if (p->race.panel[i] == now->race.panel[i]) {
            p->race.backdrop_x[i] = scroll_tween(p->race.backdrop_x[i], now->race.backdrop_x[i], t);
            p->race.backdrop_y[i] = scroll_tween(p->race.backdrop_y[i], now->race.backdrop_y[i], t);
        }
    }
}

void video_between_begin(float between)
{
    between_now = between < 0.0f ? 0.0f : between > 1.0f ? 1.0f : between;
    game.pic = pic;
    drawing = 1;
    /* a whole frame of the update before: the race's screens, and the
       menus' once they draw them so */
    eased = have_was && between_now < 1.0f && (was.pic.race.on || menu_ease_was);
    video_newest_lines_take();          /* the newest world's moving per-line tables, while it is in */
    if (!eased) {
        queue_run();                    /* the newest picture: its update's writes with it */
        if (!pic.race.on && menu_ease)
            menu_ease(&pic, between_now);
        return;
    }
    /* the update before's colours, gradient and windows for the whole
       picture, the 3D's compose (the ground's colour) too */
    game.windows = windows;
    game.palettes = palettes;
    game.gradient = gradient;
    windows = was.windows;
    palettes = was.palettes;
    gradient = was.gradient;
    colours_in = 1;
    pic = was.pic;
    if (!pic.race.on) {
        menu_ease_was(&pic, between_now);
        return;
    }
    if (same_screen(&pic, &game.pic))
        race_ease(&pic, &game.pic, between_now);
}

/* video_draw: the update before's game state (g) while it draws. Not
   during the 3D's compose: the 3D runs on its own clock from the game's. */
void video_between_world_in(void)
{
    if (!video_between_eased() || world_in)
        return;
    game.g = g;
    g = was.g;
    world_in = 1;
}

void video_between_world_out(void)
{
    if (!world_in)
        return;
    g = game.g;
    world_in = 0;
}

int video_between_world_is_in(void)
{
    return world_in;
}

void video_between_end(void)
{
    if (!drawing)
        return;
    video_between_world_out();
    if (colours_in) {
        windows = game.windows;
        palettes = game.palettes;
        gradient = game.gradient;
        colours_in = 0;
    }
    pic = game.pic;
    drawing = 0;
    eased = 0;
}
