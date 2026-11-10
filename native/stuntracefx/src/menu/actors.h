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
    uint16_t spare[3];                  /* +26-+2A: read by no handler */
} Actor;

/* the handlers, by the numbers the spawn data stores */
enum {
    HANDLER_ROW_SET = 0xC47B,           /* a settings row once set up: nothing */
    HANDLER_DONE = 0xCE89,              /* an object done moving, which others wait for: nothing */
    HANDLER_NO_STEP = 0xDF7B,           /* an object with no step of its own */
    HANDLER_NO_STEP_B = 0xE0DF,         /* the same, a second copy */
    HANDLER_ANIMATE_THEN_EXIT = 0xCD6C,
    HANDLER_DROP_START = 0xCD7C,
    HANDLER_DROP_BOUNCE = 0xCE19,
    HANDLER_RISE_AWAY = 0xCE8A,
    HANDLER_COUNTDOWN = 0xCF9C,
    HANDLER_CROSS_FADE_IN = 0xCFAB,
    HANDLER_DROP_AWAY = 0xCF27,
    HANDLER_AWAY_CHECK = 0xD068,
    HANDLER_CROSS_FADE_OUT = 0xD14A,
    HANDLER_LOAD_FIRST = 0xD1F7,
    HANDLER_LOAD_LAST = 0xD476,
    HANDLER_SET_EXIT = 0xD4A5,
    HANDLER_SELECT_START = 0xD4AE,
    HANDLER_WAIT_THEN_ANIMATE = 0xD5EC,
    HANDLER_ANIMATE_ONLY = 0xD5F6,
    HANDLER_WAIT_THEN_SEQUENCE = 0xD5FA,
    HANDLER_BUTTON_SEQUENCE = 0xD606,
    HANDLER_ERASE_CONFIRM = 0xD71B,
    HANDLER_ERASE_CONFIRM2 = 0xD7C2,
    HANDLER_ERASE_DONE = 0xD872,
    HANDLER_IDLE = 0xD8A8,

    HANDLER_SELECT_ARRIVE = 0xB814,
    HANDLER_MODE_SELECT = 0xB952,
    HANDLER_SETTINGS_FRAME = 0xBA4E,
    HANDLER_PREVIEW_IDLE = 0xBC9A,
    HANDLER_WAIT_PICTURE = 0xBCD2,
    HANDLER_PREVIEW_START = 0xBCF1,
    HANDLER_PREVIEW_COUNTDOWN = 0xBD55,
    HANDLER_PREVIEW_BACK_OUT = 0xBE29,
    HANDLER_PREVIEW_BACK_WAIT = 0xBE65,
    HANDLER_PREVIEW_BACK_END = 0xBE84,
    HANDLER_SETTINGS_SELECT = 0xBEEF,
    HANDLER_SETTINGS_SELECT2 = 0xBFB8,
    HANDLER_INITIALS_FRAME = 0xC225,
    HANDLER_ROW_START = 0xC46D,
    HANDLER_SLIDE_TO_PREVIEW = 0xC5EF,
    HANDLER_SLIDE_IN = 0xC872,
    HANDLER_FADE_IN = 0xC896,
    HANDLER_SLIDE_IN_DONE = 0xC8BF,
    HANDLER_FADE_UP = 0xC95F,
    HANDLER_FOLLOW_SLIDE_FAR = 0xC985,  /* with the sliding sprites, at $1CF less the slide */
    HANDLER_FOLLOW_SLIDE_NEAR = 0xC993, /* at $194 less the slide */
    HANDLER_FADE_OUT = 0xC9C4,
    HANDLER_CHOICE_00_START = 0xCAE6,
    HANDLER_CHOICE_00 = 0xCAEE,
    HANDLER_CHOICE_12_COPY = 0xCAF8,
    HANDLER_CHOICE_12_START = 0xCB01,
    HANDLER_CHOICE_12 = 0xCB06,
    HANDLER_CHOICE_24_START = 0xCB13,
    HANDLER_CHOICE_24 = 0xCB1B,
    HANDLER_CHOICE_48_START = 0xCB28,
    HANDLER_CHOICE_38_START = 0xCB3D,
    HANDLER_CHOICE_38 = 0xCB42,
    HANDLER_CHOICE_48 = 0xCB74,

    HANDLER_RESULTS_WAIT_THEN_RISE = 0xD8A9,
    HANDLER_RESULTS_RISE = 0xD8B5,
    HANDLER_RESULTS_OPEN_TOP = 0xD8FA,
    HANDLER_RESULTS_OPEN_BOTTOM = 0xD91B,
    HANDLER_RESULTS_PLACE_WAIT = 0xD985,
    HANDLER_RESULTS_PLACE_CAR = 0xD9AF,
    HANDLER_RESULTS_LEADER_RESUME = 0xDA1F,
    HANDLER_RESULTS_LEADER_BLINK = 0xDA33,
    HANDLER_RESULTS_WAIT_BUTTON = 0xDA8A,
    HANDLER_RESULTS_LOWER = 0xDAD1,
    HANDLER_RESULTS_RISE_AGAIN = 0xDC6D,
    HANDLER_RESULTS_WAIT_END = 0xDC9F,
    HANDLER_RESULTS_TIME_DIGITS = 0xDCBF,
    HANDLER_RESULTS_OVER_OR_HIDE = 0xDD01,
    HANDLER_RESULTS_INITIALS = 0xDD1E,
    HANDLER_RESULTS_LAP_TIMES = 0xDE0B,
    HANDLER_RESULTS_PLACE_0 = 0xDFDB,
    HANDLER_RESULTS_PLACE_0_ANIM = 0xE018,
    HANDLER_RESULTS_PLACE_1 = 0xE01C,
    HANDLER_RESULTS_PLACE_1_ANIM = 0xE059,
    HANDLER_RESULTS_PLACE_2 = 0xE05D,
    HANDLER_RESULTS_PLACE_2_ANIM = 0xE09A,
    HANDLER_RESULTS_PLACE_3 = 0xE09E,
    HANDLER_RESULTS_PLACE_3_ANIM = 0xE0DB,
    HANDLER_SELECTION_FRAMES_CYCLE = 0xDF7C
};

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
