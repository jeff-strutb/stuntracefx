/*
 * menu_tween.c: the menu screens' pictures between two updates (plan stage
 * 10, "pictures between updates on displays faster than 60 Hz"). While a
 * picture is drawn, video_between_begin keeps the game's pic aside and asks
 * the menus to ease theirs in place (menu_tween): the layers' scrolls, the
 * two windows' edges and the sprites go between the last two updates'
 * places by `between`; video_between_end puts the game's pic back. What
 * changes in steps (art, colours, which layers and sprites show) is not
 * touched. The per-line tables ease the same way (menu_lines.c).
 */
#include <string.h>

#include "menu/actors.h"
#include "menu/menu_window.h"
#include "video/between.h"
#include "video/picture.h"

enum {
    FAR_SCROLL = 64,                    /* further than an update moves a layer: shown where it is (the
                                           granite's map is written 72 pixels past the screen) */
    FAR_EDGE = 64,                      /* the same for a window's edge */
    FAR_SPRITE = 32,                    /* and a sprite */
    SCROLL_PERIOD = 1024                /* a scroll's wrap */
};

/* what moves on the menus' picture, as one update left it */
typedef struct {
    uint32_t update;
    int32_t scroll[3][2];
    nshvc_window window[2];
    uint32_t sprite_count;
    uint16_t key[128];
    int32_t x[128], y[128];
} Moving;

static Moving was, now;
static int have_now;

static void take(Moving *m, const Picture *p, uint32_t update)
{
    uint32_t i;
    int l;

    m->update = update;
    for (l = 0; l < 3; l++) {
        m->scroll[l][0] = p->bg[l].scroll_x;
        m->scroll[l][1] = p->bg[l].scroll_y;
    }
    m->window[0] = p->windows.w[0];
    m->window[1] = p->windows.w[1];
    m->sprite_count = p->sprite_count > 128 ? 128 : p->sprite_count;
    for (i = 0; i < m->sprite_count; i++) {
        m->key[i] = actors.sprite_key[i];
        m->x[i] = p->sprites[i].x;
        m->y[i] = p->sprites[i].y;
    }
}

/* the last update's sprite drawn by `key`, or -1 */
static int was_sprite(uint16_t key, uint32_t hint)
{
    uint32_t j;

    if (hint < was.sprite_count && was.key[hint] == key)
        return (int)hint;
    for (j = 0; j < was.sprite_count; j++)
        if (was.key[j] == key)
            return (int)j;
    return -1;
}

static int covers(const nshvc_window *w) { return w->left <= w->right; }

void menu_tween(Picture *p, float between)
{
    uint32_t update = menu_window_updates(), i;
    int l, k;

    if (!have_now || now.update != update) {
        if (have_now)
            was = now;
        take(&now, p, update);
        have_now = 1;
    }
    if (was.update + 1 != now.update)
        return;                         /* the picture before was not of the update before */
    for (l = 0; l < 3; l++) {
        p->bg[l].scroll_x = tween_wrapped(was.scroll[l][0], now.scroll[l][0], between, FAR_SCROLL, SCROLL_PERIOD);
        p->bg[l].scroll_y = tween_wrapped(was.scroll[l][1], now.scroll[l][1], between, FAR_SCROLL, SCROLL_PERIOD);
    }
    for (k = 0; k < 2; k++)
        if (covers(&was.window[k]) && covers(&now.window[k])) {
            p->windows.w[k].left = tween_position(was.window[k].left, now.window[k].left, between, FAR_EDGE);
            p->windows.w[k].right = tween_position(was.window[k].right, now.window[k].right, between, FAR_EDGE);
        }
    for (i = 0; i < now.sprite_count; i++) {
        int j = was_sprite(now.key[i], i);

        if (j < 0)
            continue;                   /* new this update: where it is */
        p->sprites[i].x = tween_position(was.x[j], now.x[i], between, FAR_SPRITE);
        p->sprites[i].y = tween_position(was.y[j], now.y[i], between, FAR_SPRITE);
    }
}
