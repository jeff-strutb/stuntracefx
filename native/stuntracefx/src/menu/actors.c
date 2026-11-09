/*
 * actors.c: the menu screens' objects: 22 records run once a frame through
 * a handler number, drawn as sprite groups from a table the handlers change.
 */
#include <string.h>

#include "data/cart.h"
#include "menu/actors.h"
#include "video/picture.h"

Actors actors;

enum {
    GROUPS_BANK = 0x0B, GROUPS_FROM = 0x8000,   /* the groups' block in the cartridge */
    GROUP_BASE = 0x1356 - 0x0800,               /* where group numbers count from in it */
    TEMPLATE_AT = 0xEA0F,
    SPAWN_LISTS = 0xE4E7, SPAWN_ENTRIES = 0xE547,
    ANIMATIONS = 0xAB60
};

void actors_load(void)
{
    memcpy(actors.groups, cart_at(GROUPS_BANK, GROUPS_FROM), sizeof actors.groups);
}

static uint16_t group_word(uint16_t at)
{
    return (uint16_t)(actors.groups[GROUP_BASE + at] | actors.groups[GROUP_BASE + at + 1] << 8);
}

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

static void actor_run(Actor *a)
{
    switch (a->handler) {
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
