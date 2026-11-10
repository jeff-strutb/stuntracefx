/*
 * actors.h: the menu screens' objects. A record has 22 words; the game's
 * spawn tables write them by offset, so the layout is the data's.
 */
#ifndef STUNTRACEFX_ACTORS_H
#define STUNTRACEFX_ACTORS_H

#include <stdint.h>

#include "data/menu_tables.h"

#define ACTOR_COUNT 22

typedef struct {
    uint16_t x_frac, x;                 /* +00 */
    uint16_t y_frac, y;                 /* +04 */
    uint16_t handler;                   /* +08: a MenuHandler */
    uint16_t count;                     /* +0A */
    uint16_t large;                     /* +0C */
    uint16_t tile;                      /* +0E */
    uint16_t hidden;                    /* +10: $FFFF hides it; a single sprite's attributes */
    uint16_t group;                     /* +12: the sprite group, 0 a single sprite, $FFFF none */
    uint16_t anim, anim_pos, anim_timer; /* +14 */
    uint16_t kind;                      /* +1A */
    uint16_t vx_frac, vx;               /* +1C; +1E also a frame count */
    uint16_t vy_frac, vy;               /* +20 */
    uint16_t palette;                   /* +24 */
    uint16_t spare[3];                  /* +26-+2A: read by no handler */
} Actor;


typedef struct {
    Actor actor[ACTOR_COUNT];
    uint8_t free_stack[ACTOR_COUNT];
    uint8_t free_used, live_count;
    uint8_t live[ACTOR_COUNT];
    uint8_t current;                    /* the live slot being run */
    uint16_t scroll_x, scroll_y;        /* subtracted from the objects' positions */
    uint16_t fast_animation;
    uint16_t exit_request;
    uint16_t next_group;                /* $120B: where the next copied group goes */
    uint16_t sprite_key[128];           /* who drew each of pic's sprites: the object's slot << 8, its group
                                           entry + 1 (0 a single sprite); the pictures between updates match by it */
    uint8_t groups[0x3800];             /* $7F:0800-$7F:3FFF: the sprite groups and line tables from the cartridge
                                           ($0800-$33FF), and from $3BD7 the groups the objects copy for themselves */
} Actors;

extern Actors actors;

void actors_load(void);
void actors_reset(void);
void actors_spawn_list(uint16_t list);
Actor *actor_spawn(uint16_t entry);
void actors_run(void);
void actors_build_sprites(void);
Actor *actor_find(uint16_t kind);
void actor_free(Actor *a);
void actors_set_hidden(uint16_t kind, uint16_t value);
void actor_animate_frame(Actor *a);
void actor_start_animation(Actor *a, uint16_t anim);
void actor_copy_group(Actor *a);
uint16_t group_word(uint16_t at);
void group_word_set(uint16_t at, uint16_t value);
void actor_tile_set(Actor *a, uint16_t entry, uint16_t tile);
void group_palette_set(Actor *a, uint16_t palette_bits);

void menu_palette_scale(int first, int count, uint16_t factor);
void cross_fade(uint16_t n);
extern uint16_t menu_idle;

/* the frame's end, for the handlers that need it */
void actors_upload_frames(Actor *a, uint16_t frame, uint16_t first);
/* the sprite art rows queued this frame, copied (the menus' frame end) */
void actors_rows_flush(void);

#endif
