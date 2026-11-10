/*
 * menu_tween.c: the menu screens' pictures between two updates (plan stage
 * 10, "pictures between updates on displays faster than 60 Hz"). A picture
 * between update N-1 and update N is one moment: N-1's whole frame, with
 * only what moves eased toward N (video/between.h, the whole-frame
 * contract). While it is drawn video hands the menus N-1's pic; menu_tween
 * eases its layers' scrolls, its two windows' edges and its sprites toward
 * N's (video_between_newest). The art and maps the menus write wait one
 * update with everything else (video_tiles_write, video_map_write); the
 * per-line tables are worked out for both updates (menu_lines.c).
 */
#include <string.h>

#include "menu/actors.h"
#include "menu/menu_window.h"
#include "video/between.h"
#include "video/picture.h"

enum {
    FAR_SCROLL = 64,                    /* further than an update moves a layer: a jump, shown at N-1's until N
                                           (the granite's map is written 72 pixels past the screen) */
    FAR_EDGE = 64,                      /* the same for a window's edge */
    FAR_SPRITE = 32,                    /* and a sprite */
    SCROLL_PERIOD = 1024                /* a scroll's wrap */
};

/* who drew each sprite, as update N-1 and update N left their lists */
static struct {
    uint32_t update, count;
    uint16_t key[128];
} keys[2];                              /* [0] N-1, [1] N */

void menu_tween_keep(void)
{
    keys[0] = keys[1];
    keys[1].update = menu_window_updates();
    keys[1].count = pic.sprite_count > 128 ? 128 : pic.sprite_count;
    memcpy(keys[1].key, actors.sprite_key, keys[1].count * sizeof keys[1].key[0]);
}

/* N's sprite drawn by `key`, or -1 */
static int newest_sprite(uint16_t key, uint32_t hint)
{
    uint32_t j;

    if (hint < keys[1].count && keys[1].key[hint] == key)
        return (int)hint;
    for (j = 0; j < keys[1].count; j++)
        if (keys[1].key[j] == key)
            return (int)j;
    return -1;
}

static int covers(const nshvc_window *w) { return w->left <= w->right; }

void menu_tween(Picture *p, float between)
{
    const Picture *n = video_between_newest();
    uint32_t i;
    int l, k;

    /* both lists are the menus' own, of two updates running */
    if (!n || keys[1].update != menu_window_updates() || keys[0].update + 1 != keys[1].update)
        return;
    for (l = 0; l < 3; l++) {
        p->bg[l].scroll_x = tween_wrapped(p->bg[l].scroll_x, n->bg[l].scroll_x, between, FAR_SCROLL, SCROLL_PERIOD);
        p->bg[l].scroll_y = tween_wrapped(p->bg[l].scroll_y, n->bg[l].scroll_y, between, FAR_SCROLL, SCROLL_PERIOD);
    }
    for (k = 0; k < 2; k++)
        if (covers(&p->windows.w[k]) && covers(&n->windows.w[k])) {
            p->windows.w[k].left = tween_position(p->windows.w[k].left, n->windows.w[k].left, between, FAR_EDGE);
            p->windows.w[k].right = tween_position(p->windows.w[k].right, n->windows.w[k].right, between, FAR_EDGE);
        }
    for (i = 0; i < p->sprite_count && i < keys[0].count; i++) {
        int j = newest_sprite(keys[0].key[i], i);

        if (j < 0)
            continue;                   /* gone at N: where N-1 left it */
        p->sprites[i].x = tween_position(p->sprites[i].x, n->sprites[j].x, between, FAR_SPRITE);
        p->sprites[i].y = tween_position(p->sprites[i].y, n->sprites[j].y, between, FAR_SPRITE);
    }
}
