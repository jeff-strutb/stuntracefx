/*
 * actors.c: the menu screens' objects: 22 records run once a frame through
 * a handler number, drawn as sprite groups from a table the handlers change.
 */
#include <string.h>

#include <stdio.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "menu/actors.h"
#include "system/game.h"
#include "video/picture.h"

Actors actors;

enum {
    GROUPS_BANK = 0x0B, GROUPS_FROM = 0x8000,   /* the groups' block in the cartridge */
    GROUP_BASE = 0x1356 - 0x0800,               /* where group numbers count from in it */
    TEMPLATE_AT = 0xEA0F,
    SPAWN_LISTS = 0xE4E7, SPAWN_ENTRIES = 0xE547,
    ANIMATIONS = 0xAB60
};

static void menu_handler_missing(uint16_t handler)
{
    static uint16_t last;

    if (handler != last)
        fprintf(stderr, "stuntracefx: menu handler %04X is not converted yet\n", handler);
    last = handler;
}

void menu_load_start(Actor *a);

void actors_load(void)
{
    memcpy(actors.groups, cart_at(GROUPS_BANK, GROUPS_FROM), sizeof actors.groups);
}

static uint16_t group_word(uint16_t at)
{
    return (uint16_t)(actors.groups[GROUP_BASE + at] | actors.groups[GROUP_BASE + at + 1] << 8);
}

/* the line tables sit in the same block as the groups */
uint8_t *menu_tables_at(void) { return actors.groups; }

void actors_reset(void)
{
    int i;

    for (i = 0; i < ACTOR_COUNT; i++) {
        actors.free_stack[i] = (uint8_t)i;
        actors.actor[i].handler = HANDLER_IDLE;
        actors.actor[i].hidden = 0xFFFF;
    }
    actors.free_used = 0;
    actors.live_count = 0;
    pic.sprite_count = 0;
}

static Actor *actor_new(void)
{
    uint8_t n;
    int i;

    if (actors.free_used == ACTOR_COUNT)
        return NULL;
    n = actors.free_stack[actors.free_used++];
    actors.live[actors.live_count++] = n;
    /* The original copies twice a record's length, so the template's second
       half lands on the following record. For the last record it would run
       past the records, which needs a full pool; no play has one. */
    for (i = 0; i < 44 && n + (i >= 22) < ACTOR_COUNT; i++)
        ((uint16_t *)&actors.actor[n + (i >= 22)])[i % 22] = cart16(0x0B, (uint16_t)(TEMPLATE_AT + i * 2));
    return &actors.actor[n];
}

static void actor_spawn(uint16_t entry)
{
    Actor *a = actor_new();
    uint16_t f;

    if (!a)
        return;
    while (!((f = cart16(0x0B, (uint16_t)(SPAWN_ENTRIES + entry))) & 0x8000)) {
        ((uint16_t *)a)[f / 2] = cart16(0x0B, (uint16_t)(SPAWN_ENTRIES + entry + 2));
        entry = (uint16_t)(entry + 4);
    }
}

void actors_spawn_list(uint16_t list)
{
    uint16_t e;

    while (!((e = cart16(0x0B, (uint16_t)(SPAWN_LISTS + list))) & 0x8000)) {
        actor_spawn(e);
        list = (uint16_t)(list + 2);
    }
}

Actor *actor_find(uint16_t kind)
{
    int i;

    for (i = actors.live_count - 1; i >= 0; i--)
        if (actors.actor[actors.live[i]].kind == kind)
            return &actors.actor[actors.live[i]];
    return NULL;
}

/* One step of a's animation: +anim is the animation's place in the table;
   each entry is a duration and a sprite group. A negative duration goes
   back to the first entry. */
static void actor_animate(Actor *a)
{
    uint16_t at, d;

    if (a->anim & 0x8000)
        return;
    a->anim_timer--;
    if (!(a->anim_timer & 0x8000))
        return;
    a->anim_pos = (uint16_t)(a->anim_pos + 4);
    at = (uint16_t)(a->anim_pos + a->anim);
    d = cart16(0x0B, (uint16_t)(ANIMATIONS + at));
    if (d & 0x8000) {
        a->anim_pos = 0;
        at = a->anim;
        d = cart16(0x0B, (uint16_t)(ANIMATIONS + at));
    }
    a->group = cart16(0x0B, (uint16_t)(ANIMATIONS + 2 + at));
    a->anim_timer = (uint16_t)(d - 1);
}

void actor_animate_frame(Actor *a)
{
    int i;

    actor_animate(a);
    if (actors.fast_animation)
        for (i = 0; i < 3; i++)
            actor_animate(a);
}

uint16_t menu_idle;                     /* the idle count that starts the demos */

/* Colours first to first + count - 1 scaled from the saved palette by factor / 32. */
void menu_palette_scale(int first, int count, uint16_t factor)
{
    int i;

    for (i = first; i < first + count; i++) {
        uint16_t c = pic.palette_saved[i];
        uint16_t r = (uint16_t)((((c & 0x1F) * factor) >> 5) & 0x1F);
        uint16_t g = (uint16_t)((((c >> 5 & 0x1F) * factor) >> 5) & 0x1F);
        uint16_t b = (uint16_t)((((c >> 10 & 0x1F) * factor) >> 5) & 0x1F);

        pic.palette[i] = (uint16_t)(r | g << 5 | b << 10);
    }
}

/* One cross-fade frame for count n: colours 64-127 by f / 32, colours 12-15
   by (32 - f) / 32 (the second factor as the original makes it). */
static void cross_fade(uint16_t n)
{
    uint16_t f = cart16(0x0B, (uint16_t)(0xCFE5 + (n & 0xFFFE))), g;

    menu_palette_scale(0x40, 0x40, f);
    g = (uint16_t)(((uint16_t)(f - 1) ^ 0x001F) + 1);
    menu_palette_scale(0x0C, 4, g);
}

static void actors_set_hidden(uint16_t kind, uint16_t value)
{
    int i;

    for (i = actors.live_count - 1; i >= 0; i--)
        if (actors.actor[actors.live[i]].kind == kind)
            actors.actor[actors.live[i]].hidden = value;
}

/* BG1's vertical scroll follows the object's height and its sprite follows too. */
static void height_to_scroll(Actor *a)
{
    pic.bg[0].scroll_y = (int16_t)(uint16_t)(0 - a->vy_frac);
    a->y = (uint16_t)(a->vy_frac + 0x4D);
}

/* The speed in +vy grows by 1 every second frame and moves the height. */
static void drop_step(Actor *a)
{
    a->count = (uint16_t)((a->count + 1) & 1);
    if (!a->count)
        a->vy++;
    a->vy_frac = (uint16_t)(a->vy_frac + a->vy);
}

static void drop_away_finish(Actor *a)
{
    pic.bg[0].scroll_y = 0;
    pic.line_tables = 0;
    memset(pic.windows.mask_sub, 0, sizeof pic.windows.mask_sub);
    memset(pic.windows.mask_main, 0, sizeof pic.windows.mask_main);
    pic.blend.use_fixed = 0;
    pic.blend.layers = 1;
    pic.sub_screen = SHOW_BG2;
    pic.bg[0].map = menu_video.pages_0_1;
    pic.bg[1].map = menu_video.pages_1_2;
    pic.bg[0].tiles = menu_video.tiles_a;
    pic.bg[1].tiles = menu_video.tiles_b;
    pic.main_screen = SHOW_BG1 | SHOW_BG2 | SHOW_BG3 | SHOW_SPRITES;
    actors_set_hidden(0x0011, 0xFFFF);
    a->hidden = 0xFFFF;
    a->handler = HANDLER_CROSS_FADE_OUT;
    a->count = 0x0040;
}

/* B or Start: every kind $11 object goes, the kind $13 object rises away, this one waits. */
static int select_confirm(Actor *a)
{
    Actor *bg;

    if (!(frame_menu_pressed() & 0x9000))
        return 0;
    actors_set_hidden(0x0011, 0xFFFF);
    bg = actor_find(0x0013);
    if (bg) {
        bg->handler = HANDLER_RISE_AWAY;
        bg->vy = 0xFFF6;
    }
    a->handler = HANDLER_AWAY_CHECK;
    a->hidden = 0xFFFF;
    sound_port0_push(0x46);
    return 1;
}

/* The title's button sequence: eight presses in order, X restarting it;
   any other press is a selection. */
static void button_sequence(Actor *a)
{
    uint16_t pressed;

    actor_animate_frame(a);
    pressed = frame_menu_pressed();
    if (!pressed)
        return;
    if (pressed & 0x0800) {
        a->count = 0;
        menu_idle = 0;
    }
    if (!(pressed & cart16(0x0B, (uint16_t)(0xD66D + (uint16_t)(a->count << 1))))) {
        a->count = 0;
        select_confirm(a);
        return;
    }
    frame_menu_pressed_clear(0);
    a->count++;
    if (a->count != 8)
        return;
    pic.line_tables = LINES_WINDOW | LINES_TM_C;
    pic.windows.mask_sub[0] = pic.windows.mask_sub[1] = pic.windows.mask_sub[2] = 1;
    pic.blend.fixed_bgr15 = 0x7FFF;
    pic.blend.layers = 0x27;
    pic.blend.subtract = 1;
    pic.blend.half = 0;
    pic.blend.use_fixed = 1;
    pic.blend.clip = 0;
    pic.blend.prevent = 2;
    memset(pic.windows.layer_use, 0, sizeof pic.windows.layer_use);
    pic.windows.layer_use[4] = 0;
    pic.windows.layer_use[5] = NSHVC_WIN1 | NSHVC_WIN1_INVERT;
    sound_port0_push(0x46);
    a->handler = HANDLER_ERASE_CONFIRM;
    a->count = 0;
    if (a->anim != 0x0152) {
        a->anim = 0x0152;
        a->anim_pos = 0;
        a->anim_timer = 0;
    }
}

static void actor_run(Actor *a)
{
    switch (a->handler) {
    case HANDLER_IDLE:
    case HANDLER_IDLE_C47B:
    case HANDLER_IDLE_CE89:
        break;
    case HANDLER_WAIT_THEN_SEQUENCE:
        a->vx--;
        if (!(a->vx & 0x8000))
            break;
        a->handler = HANDLER_BUTTON_SEQUENCE;
        a->count = 0;
        /* fall through */
    case HANDLER_BUTTON_SEQUENCE:
        button_sequence(a);
        break;
    case HANDLER_COUNTDOWN:
        a->count--;
        if (a->count & 0x8000) {
            a->handler = HANDLER_CROSS_FADE_IN;
            a->count = 0;
        }
        break;
    case HANDLER_CROSS_FADE_IN: {
        uint16_t n = (uint16_t)(a->count + 1);

        if (n == 0x0040) {
            a->handler = HANDLER_IDLE_CE89;
            break;
        }
        a->count = n;
        cross_fade(n);
        break;
    }
    case HANDLER_DROP_START:
        a->count--;
        if (!(a->count & 0x8000))
            break;
        a->handler = HANDLER_DROP_BOUNCE;
        a->vy = 1;
        pic.bg[0].scroll_y = (int16_t)(uint16_t)(0 - a->vy_frac);
        pic.line_tables = LINES_TM_C;
        pic.blend.use_fixed = 1;
        pic.blend.layers = 0;
        memset(pic.windows.mask_sub, 0, sizeof pic.windows.mask_sub);
        pic.bg[0].map = menu_video.pages_2_3;
        pic.bg[1].map = menu_video.pages_0_1;
        pic.bg[0].tiles = menu_video.sprites;
        pic.bg[1].tiles = menu_video.tiles_a;
        break;
    case HANDLER_DROP_BOUNCE:
        actor_animate_frame(a);
        drop_step(a);
        if (!(a->vy_frac & 0x8000)) {
            uint16_t v = (uint16_t)(a->vy - 2);

            if (v & 0x8000) {
                a->handler = HANDLER_IDLE_CE89;
                break;
            }
            a->vy = (uint16_t)(0 - v);
            a->vy_frac = 0;
            sound_port0_push(0x19);
        }
        height_to_scroll(a);
        break;
    case HANDLER_DROP_AWAY:
        actor_animate_frame(a);
        drop_step(a);
        if (!(a->vy_frac & 0x8000)) {
            if (a->vy_frac >= 0x00DC) {
                a->handler = HANDLER_IDLE_CE89;
                break;
            }
            if (a->vy_frac >= 0x001E)
                pic.line_tables = LINES_TM_B;
        }
        height_to_scroll(a);
        break;
    case HANDLER_RISE_AWAY:
        actor_animate_frame(a);
        drop_step(a);
        if ((a->vy_frac & 0x8000) && a->vy_frac < 0xFFA8) {
            a->handler = HANDLER_DROP_AWAY;
            sound_port0_push_raw(0x13);
        }
        height_to_scroll(a);
        break;
    case HANDLER_AWAY_CHECK: {
        const Actor *bg = actor_find(0x0013);

        if (bg && bg->handler == HANDLER_IDLE_CE89)
            drop_away_finish(a);
        break;
    }
    case HANDLER_CROSS_FADE_OUT: {
        uint16_t n = (uint16_t)(a->count - 2);

        if (!(n & 0x8000)) {
            a->count = n;
            cross_fade(n);
            break;
        }
        menu_load_start(a);
        break;
    }
    case HANDLER_ANIMATE_THEN_EXIT:
        actor_animate_frame(a);
        a->count--;
        if (a->count & 0x8000)
            actors.exit_request = 1;
        break;
    case HANDLER_WAIT_THEN_ANIMATE:
        a->vx--;
        if (!(a->vx & 0x8000))
            break;
        a->handler = HANDLER_ANIMATE_ONLY;
        /* fall through */
    case HANDLER_ANIMATE_ONLY:
        actor_animate_frame(a);
        break;
    default:
        menu_handler_missing(a->handler);
        break;
    }
}

void actors_run(void)
{
    int i;

    for (i = actors.live_count - 1; i >= 0; i--)
        actor_run(&actors.actor[actors.live[i]]);
}

/* Adds a sprite if it is on the screen: X from -16 to 255, Y from -16 to 223. */
static void sprite_add(uint16_t sx, uint16_t sy, uint16_t attr, int large)
{
    nshvc_sprite *s;

    if ((sx & 0x8000) ? sx < 0xFFF0 : sx >= 0x0100)
        return;
    if ((sy & 0x8000) ? sy < 0xFFF0 : sy >= 0x00E0)
        return;
    s = &pic.sprites[pic.sprite_count++];
    s->x = (int16_t)sx;
    s->y = (int16_t)sy;
    s->width = s->height = large ? 2 : 1;
    s->tiles = menu_video.sprites;
    s->tile = attr & 0x01FF;
    s->palette = (attr >> 9) & 7;
    s->priority = (attr >> 12) & 3;
    s->hflip = (attr >> 14) & 1;
    s->vflip = (attr >> 15) & 1;
}

void actors_build_sprites(void)
{
    int i;

    pic.sprite_count = 0;
    for (i = actors.live_count - 1; i >= 0; i--) {
        const Actor *a = &actors.actor[actors.live[i]];
        uint16_t px, py, at;
        int n;

        if (a->hidden == 0xFFFF || a->group == 0xFFFF)
            continue;
        px = (uint16_t)(a->x - actors.scroll_x);
        py = (uint16_t)(a->y - actors.scroll_y);
        if (a->group == 0) {
            if (pic.sprite_count < 128)
                sprite_add(px, py, (uint16_t)(a->tile | a->hidden), a->large & 1);
            continue;
        }
        at = a->group;
        n = (int16_t)group_word(at);
        at = (uint16_t)(at + 2);
        for (; n >= 0 && pic.sprite_count < 128; n--, at = (uint16_t)(at + 8))
            sprite_add((uint16_t)(group_word(at) + px), (uint16_t)(group_word((uint16_t)(at + 2)) + py),
                       group_word((uint16_t)(at + 4)), group_word((uint16_t)(at + 6)) & 1);
    }
}
