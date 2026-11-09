/*
 * actors.h: the menu screens' objects. A record has 22 words; the game's
 * spawn tables write them by offset, so the layout is the data's.
 */
#ifndef STUNTRACEFX_ACTORS_H
#define STUNTRACEFX_ACTORS_H

#include <stdint.h>

#define ACTOR_COUNT 22

typedef struct {
    uint16_t x_frac, x;                 /* +00 */
    uint16_t y_frac, y;                 /* +04 */
    uint16_t handler;                   /* +08: as the spawn data names it */
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
    uint16_t word26, word28, word2a;
} Actor;

/* the handlers, by the numbers the spawn data stores */
enum {
    HANDLER_ANIMATE_THEN_EXIT = 0xCD6C,
    HANDLER_WAIT_THEN_ANIMATE = 0xD5EC,
    HANDLER_ANIMATE_ONLY = 0xD5F6,
    HANDLER_IDLE = 0xD8A8
};

typedef struct {
    Actor actor[ACTOR_COUNT];
    uint8_t free_stack[ACTOR_COUNT];
    uint8_t free_used, live_count;
    uint8_t live[ACTOR_COUNT];
    uint8_t current;                    /* 0xFF: freed while running */
    uint16_t scroll_x, scroll_y;        /* subtracted from the objects' positions */
    uint16_t fast_animation;
    uint16_t exit_request;
    uint8_t groups[0x2C00];             /* the sprite groups, changed as the objects run */
} Actors;

extern Actors actors;

void actors_load(void);
void actors_reset(void);
void actors_spawn_list(uint16_t list);
void actors_run(void);
void actors_build_sprites(void);
Actor *actor_find(uint16_t kind);
void actor_animate_frame(Actor *a);

#endif
