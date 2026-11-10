/*
 * actors.c: the menu screens' objects: 22 records run once a frame through
 * a handler number, drawn as sprite groups from a table the handlers change.
 */
#include <stdio.h>
#include <string.h>

#include "data/cart.h"
#include "menu/actors.h"
#include "menu/menu.h"
#include "system/game.h"
#include "video/between.h"
#include "video/picture.h"

Actors actors;
uint16_t menu_idle;                     /* the idle count that starts the demos */

enum {
    GROUPS_BANK = 0x0B, GROUPS_FROM = 0x8000,   /* the groups' block in the cartridge */
    GROUP_BASE = 0x1356 - 0x0800,               /* where group numbers count from in it */
    TEMPLATE_AT = 0xEA0F,
    SPAWN_LISTS = 0xE4E7, SPAWN_ENTRIES = 0xE547,
    ANIMATIONS = 0xAB60,
    FIRST_FREE_GROUP = 0x2881,
    MENU_BANK = 0x0B,                           /* the bank of the tables below */
    UPLOAD_SLOTS = 0xCCBC,                      /* each sprite's art slot */
    UPLOAD_BASES = 0xCD14,                      /* each sprite's art in the frames */
    UPLOAD_FRAMES = 0xCC7C,                     /* a frame's offset, bit 15 its vertical flip */
    SPRITE_TILES_SLOT = 0x4000,                 /* the slot of the sprite tile set's tile 0 */
    CROSS_FADE_LEVELS = 0xCFE5                  /* the cross-fade's factor by step */
};

/* the block is modified as objects run: this is the copy */
uint8_t *menu_tables_at(void) { return actors.groups; }
uint8_t *menu_table_byte(uint16_t at) { return &actors.groups[at - 0x0800]; }

void actors_load(void)
{
    memcpy(actors.groups, cart_at(GROUPS_BANK, GROUPS_FROM), 0x2C00);
}

uint16_t group_word(uint16_t at)
{
    return (uint16_t)(actors.groups[GROUP_BASE + at] | actors.groups[GROUP_BASE + at + 1] << 8);
}

void group_word_set(uint16_t at, uint16_t value)
{
    actors.groups[GROUP_BASE + at] = (uint8_t)value;
    actors.groups[GROUP_BASE + at + 1] = (uint8_t)(value >> 8);
}

/*
 * @routine 0B:B479 MenuActorPoolInit
 * WHAT IT DOES: Empties the menu object pool: every record free, idle and
 * hidden, and the next group copy back at its first place.
 */
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
    actors.next_group = FIRST_FREE_GROUP;
    pic.sprite_count = 0;
}

/*
 * @routine 0B:B4C3 MenuActorAlloc
 * @routine 0B:B506 MenuActorNew
 * WHAT IT DOES: Takes a free record for the live list and fills it from the
 * template.
 */
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
        ((uint16_t *)&actors.actor[n + (i >= 22)])[i % 22] = cart16(MENU_BANK, (uint16_t)(TEMPLATE_AT + i * 2));
    return &actors.actor[n];
}

/*
 * @routine 0B:B4E5 MenuActorFree
 * WHAT IT DOES: Returns the record being run to the free stack and removes
 * it from the live list by moving the last entry into its place.
 */
void actor_free(Actor *a)
{
    actors.free_stack[--actors.free_used] = (uint8_t)(a - actors.actor);
    actors.live_count--;
    actors.live[actors.current] = actors.live[actors.live_count];
}

/*
 * @routine 0B:E4B5 MenuActorSpawn
 * WHAT IT DOES: Makes a new object and applies a spawn entry list to it:
 * each entry is a field offset and a value, ended by a negative offset.
 */
Actor *actor_spawn(uint16_t entry)
{
    Actor *a = actor_new();
    uint16_t f;

    if (!a)
        return NULL;
    while (!((f = cart16(MENU_BANK, (uint16_t)(SPAWN_ENTRIES + entry))) & 0x8000)) {
        ((uint16_t *)a)[f / 2] = cart16(MENU_BANK, (uint16_t)(SPAWN_ENTRIES + entry + 2));
        entry = (uint16_t)(entry + 4);
    }
    return a;
}

/*
 * @routine 0B:E458 MenuActorSpawnList
 * WHAT IT DOES: Spawns every object of a spawn list. A full pool stops the
 * game in the original; no script fills it.
 */
void actors_spawn_list(uint16_t list)
{
    uint16_t e;

    while (!((e = cart16(MENU_BANK, (uint16_t)(SPAWN_LISTS + list))) & 0x8000)) {
        actor_spawn(e);
        list = (uint16_t)(list + 2);
    }
}

/*
 * @routine 0B:E28D MenuActorFind
 * WHAT IT DOES: Finds the live object of a kind, searching from the end of
 * the list.
 */
Actor *actor_find(uint16_t kind)
{
    int i;

    for (i = actors.live_count - 1; i >= 0; i--)
        if (actors.actor[actors.live[i]].kind == kind)
            return &actors.actor[actors.live[i]];
    return NULL;
}

/*
 * @routine 0B:C9A1 MenuActorsSet10
 * WHAT IT DOES: Sets the hidden field of every live object of a kind.
 */
void actors_set_hidden(uint16_t kind, uint16_t value)
{
    int i;

    for (i = actors.live_count - 1; i >= 0; i--)
        if (actors.actor[actors.live[i]].kind == kind)
            actors.actor[actors.live[i]].hidden = value;
}

/*
 * @routine 0B:E250 MenuActorAnimate
 * WHAT IT DOES: One step of an object's animation: +anim is the animation's
 * place in the table; each entry is a duration and a sprite group; a
 * negative duration goes back to the first entry.
 */
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
    d = cart16(MENU_BANK, (uint16_t)(ANIMATIONS + at));
    if (d & 0x8000) {
        a->anim_pos = 0;
        at = a->anim;
        d = cart16(MENU_BANK, (uint16_t)(ANIMATIONS + at));
    }
    a->group = cart16(MENU_BANK, (uint16_t)(ANIMATIONS + 2 + at));
    a->anim_timer = (uint16_t)(d - 1);
}

/*
 * @routine 0B:E23D MenuActorAnimateFrame
 * WHAT IT DOES: Steps an object's animation once a frame, or four times
 * when the fast animation flag is set.
 */
void actor_animate_frame(Actor *a)
{
    int i;

    actor_animate(a);
    if (actors.fast_animation)
        for (i = 0; i < 3; i++)
            actor_animate(a);
}

/* animation `anim` unless it is the one playing */
void actor_start_animation(Actor *a, uint16_t anim)
{
    if (a->anim == anim)
        return;
    a->anim = anim;
    a->anim_pos = 0;
    a->anim_timer = 0;
}

/*
 * @routine 0B:E1C5 MenuActorCopyGroup
 * WHAT IT DOES: Gives an object its own copy of the sprite group its
 * animation starts with, at the next free place in the group area.
 */
void actor_copy_group(Actor *a)
{
    uint16_t src = cart16(MENU_BANK, (uint16_t)(ANIMATIONS + 2 + a->anim));
    uint16_t size = (uint16_t)(((uint16_t)(group_word(src) + 1) << 3) + 2);
    uint16_t dst = actors.next_group;
    int i;

    if ((uint16_t)(dst + size) >= 0x2C81) {
        /* the original stops the game here (Hang); no play gets there */
        fprintf(stderr, "stuntracefx: the menu objects' sprite groups are full\n");
        return;
    }
    a->group = dst;
    actors.next_group = (uint16_t)(actors.next_group + size);
    for (i = 0; i < size; i += 2)
        group_word_set((uint16_t)(dst + i), group_word((uint16_t)(src + i)));
}

/*
 * @routine 0B:C4E4 MenuActorSetTiles
 * WHAT IT DOES: Sets the tile numbers of two sprite entries of an object's
 * group: entry `entry` gets `tile`, the entry before it that tile + $10.
 */
void actor_tile_set(Actor *a, uint16_t entry, uint16_t tile)
{
    uint16_t e = (uint16_t)(entry + a->group), w;

    w = group_word((uint16_t)(e + 4));
    group_word_set((uint16_t)(e + 4), (uint16_t)((w & 0xFE00) | tile));
    w = group_word((uint16_t)(e - 8 + 4));
    group_word_set((uint16_t)(e - 8 + 4), (uint16_t)((w & 0xFE00) | (uint16_t)(tile + 0x10)));
}

/* Replaces bits 9-11 (the palette) of every sprite of the object's group. */
void group_palette_set(Actor *a, uint16_t palette_bits)
{
    uint16_t at = (uint16_t)(a->group + 2);
    int n = (int16_t)group_word(a->group);

    for (; n >= 0; n--, at = (uint16_t)(at + 8))
        group_word_set((uint16_t)(at + 4), (uint16_t)((group_word((uint16_t)(at + 4)) & 0xF1FF) | palette_bits));
}

/*
 * @routine 7F:1007 VramDmaQueueAdd (the sprite art rows the objects queue)
 * @routine 7F:0F77 VramDmaQueueFlush (as the menus' frame end runs it)
 * WHAT IT DOES: The rows of sprite art the objects ask for wait for the
 * frame's end, as the original's VRAM queue waited for the vertical blank:
 * a picture a screen loads after its objects run does not cover them. The
 * queue holds 64; more are dropped, as the original's were. The rows go
 * last first, so of two for the same tiles the first asked for stays.
 */
enum { ROWS_QUEUED = 64 };
static struct { uint16_t tile, src; } rows[ROWS_QUEUED];
static int rows_n;

static void row_queue(uint16_t tile, uint16_t src)
{
    if (rows_n == ROWS_QUEUED)
        return;
    rows[rows_n].tile = tile;
    rows[rows_n].src = src;
    rows_n++;
}

void actors_rows_flush(void)
{
    while (rows_n > 0) {
        rows_n--;
        video_tiles_write(menu_video.sprites, rows[rows_n].tile, menu_frame_data(rows[rows_n].src), 2,
                          NSHVC_TILES_PLANAR, 2 * 32);
    }
}

/*
 * @routine 0B:CBC3 MenuActorUploadFrames
 * WHAT IT DOES: Shows animation frame `frame` on every sprite of the
 * object's group: each sprite's graphics (two rows of two tiles, from the
 * frames in menu_frames) are queued for its slot in the sprite tile set, and bit 15
 * of the frame word becomes the sprite's vertical flip.
 */
void actors_upload_frames(Actor *a, uint16_t frame, uint16_t first)
{
    uint16_t f = (uint16_t)(frame << 1), e = (uint16_t)(a->group + 2), i = first;
    int n = (int16_t)group_word(a->group);

    for (; n >= 0; n--, e = (uint16_t)(e + 8), i = (uint16_t)(i + 2), f = (uint16_t)((f + 2) & 0x003F)) {
        uint16_t slot = cart16(MENU_BANK, (uint16_t)(UPLOAD_SLOTS + i));
        uint16_t base = cart16(MENU_BANK, (uint16_t)(UPLOAD_BASES + i));
        uint16_t v = cart16(MENU_BANK, (uint16_t)(UPLOAD_FRAMES + f));
        uint16_t tile = (uint16_t)((slot - SPRITE_TILES_SLOT) / 16);
        uint16_t src = (uint16_t)((v & 0x7FFF) + base);

        group_word_set((uint16_t)(e + 4), (uint16_t)((group_word((uint16_t)(e + 4)) & 0x7FFF) | (v & 0x8000)));
        row_queue(tile, src);
        row_queue((uint16_t)(tile + 0x10), (uint16_t)(src + 0x0200));
    }
}

/*
 * @routine 0B:E303 MenuPaletteScale
 * WHAT IT DOES: Colours first to first + count - 1 scaled from the saved
 * palette by factor / 32.
 */
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
void cross_fade(uint16_t n)
{
    uint16_t f = cart16(MENU_BANK, (uint16_t)(CROSS_FADE_LEVELS + (n & 0xFFFE))), g;

    menu_palette_scale(0x40, 0x40, f);
    g = (uint16_t)(((uint16_t)(f - 1) ^ 0x001F) + 1);
    menu_palette_scale(0x0C, 4, g);
}

/*
 * @routine 0B:B450 MenuActorsRun
 * WHAT IT DOES: Runs every live menu object's handler once, from the last
 * in the live list to the first.
 */
void actors_run(void)
{
    int i;

    menu.fixed_colour_queued = 0;       /* a run is one frame's queue of byte writes */
    for (i = actors.live_count - 1; i >= 0; i--) {
        actors.current = (uint8_t)i;
        menu_actor_run(&actors.actor[actors.live[i]]);
    }
    menu_typing_run_end();
}

/* Adds a sprite if it is on the screen: X from -16 to 255, Y from -16 to 223;
   `key` says who drew it. */
static void sprite_add(uint16_t sx, uint16_t sy, uint16_t attr, int large, uint16_t key)
{
    nshvc_sprite *s;

    if ((sx & 0x8000) ? sx < 0xFFF0 : sx >= 0x0100)
        return;
    if ((sy & 0x8000) ? sy < 0xFFF0 : sy >= 0x00E0)
        return;
    actors.sprite_key[pic.sprite_count] = key;
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

/*
 * @routine 0B:B64A MenuOamBuild
 * WHAT IT DOES: Builds the sprite list from the objects, last live object
 * first, relative to the scroll position; an object with hidden = $FFFF or
 * group $FFFF is skipped, group 0 is a single sprite from the record,
 * otherwise a group of entries placed at the object.
 */
void actors_build_sprites(void)
{
    int i;

    pic.sprite_count = 0;
    for (i = actors.live_count - 1; i >= 0; i--) {
        const Actor *a = &actors.actor[actors.live[i]];
        uint16_t px, py, at;
        int n, k;

        if (a->hidden == 0xFFFF || a->group == 0xFFFF)
            continue;
        px = (uint16_t)(a->x - actors.scroll_x);
        py = (uint16_t)(a->y - actors.scroll_y);
        if (a->group == 0) {
            if (pic.sprite_count < 128)
                sprite_add(px, py, (uint16_t)(a->tile | a->hidden), a->large & 1, (uint16_t)(actors.live[i] << 8));
            continue;
        }
        at = a->group;
        n = (int16_t)group_word(at);
        at = (uint16_t)(at + 2);
        for (k = 1; n >= 0 && pic.sprite_count < 128; n--, k++, at = (uint16_t)(at + 8))
            sprite_add((uint16_t)(group_word(at) + px), (uint16_t)(group_word((uint16_t)(at + 2)) + py),
                       group_word((uint16_t)(at + 4)), group_word((uint16_t)(at + 6)) & 1,
                       (uint16_t)(actors.live[i] << 8 | (k & 0xFF)));
    }
}
