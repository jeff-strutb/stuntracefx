/*
 * menu_tables.h: the cartridge's tables the menus read, as typed data
 * (plan section 3, Data). Each table's place in the cartridge image is
 * written once, in menu_tables.c; the menus see what each holds.
 */
#ifndef STUNTRACEFX_DATA_MENU_TABLES_H
#define STUNTRACEFX_DATA_MENU_TABLES_H

#include <stdint.h>

/* --- the title and the selection ---------------------------------------- */

int8_t menu_sine(uint8_t angle);                        /* the signed sine, a byte an angle */
uint16_t menu_button_sequence(int step);                /* the title's button sequence: the button each step takes */
void menu_pair_animations(int pair, uint16_t *first, uint16_t *second);   /* for the objects of kinds 3 and 5 */
uint16_t menu_choice_repeat_model(int choice);          /* player 2's model when both players choose the same */
uint16_t menu_choice_step(int right, int choice);       /* the choice a left (0) or right (1) press steps to */
uint16_t menu_saved_choice(int saved);                  /* a choice as the save keeps it (2 bits) as the choice's word */
uint16_t menu_initial_tile(int letter);                 /* an initial's tile, A-Z */
uint8_t menu_car_setting_row(int choice, int combination, int k);  /* a car's setting row k (0-2) by the three settings' combination */
uint8_t menu_selection_row(int choice);                 /* player 1's choice's setting row */
uint8_t menu_selection_model(int choice, int row);      /* a racer's model by its choice and setting row */
uint16_t menu_setting_animation(int setting, int row);  /* the animation that shows a setting's value */
uint16_t menu_setting_object(int i);                    /* the kind of the object that shows setting i */

/* --- the screens' pictures, tiles, palettes and sounds -------------------- */

typedef enum {
    MENU_PIC_TITLE_SPRITES, MENU_PIC_TITLE_BG2_PAGE2, MENU_PIC_TITLE_BG1_TILES, MENU_PIC_TITLE_BG1_MAP,
    MENU_PIC_TITLE_BG2_TILES, MENU_PIC_TITLE_BG2_PAGE1, MENU_PIC_TITLE_CHECKS_MAP,
    MENU_PIC_GRANITE_MAP, MENU_PIC_MENU_BG2_TILES, MENU_PIC_MENU_BG2_MAP, MENU_PIC_MENU_SPRITES,
    MENU_PIC_GRANITE_FRAMES,            /* the granite's eight animated frames */
    MENU_PIC_SPRITE_FRAMES,             /* the objects' animation frames */
    MENU_PIC_CHAMPION_FRAMES,           /* the championship's selection frames */
    MENU_PIC_CHAMPION_SPRITES, MENU_PIC_CHAMPION_8BPP, MENU_PIC_CHAMPION_BG2_TILES, MENU_PIC_CHAMPION_MAP,
    MENU_PIC_COURSE_PICTURES,           /* the two-player race screen's course pictures */
    MENU_PICS
} MenuPicture;
uint32_t menu_picture_unpack(MenuPicture p, uint8_t *to, uint32_t room);   /* its size, 0 if `room` is short */

/* the two sets of animation frames, as the objects' frame art names its place in them */
typedef enum { MENU_FRAMES_SPRITES, MENU_FRAMES_GRANITE } MenuFrames;
MenuFrames menu_frames_place(uint16_t art, uint32_t *at);

typedef enum {
    MENU_TILES_LOGO_SPRITES,            /* the logo's sprites, $720 bytes */
    MENU_TILES_TITLE_TEXT,              /* the title's checks, four tiles */
    MENU_TILES_LOADER                   /* the loading screen's four */
} MenuTiles;
const uint8_t *menu_tiles(MenuTiles t);                 /* raw 4-bit tiles */

typedef enum { MENU_PALETTE_TITLE, MENU_PALETTE_MENU, MENU_PALETTE_CHAMPION } MenuPalette;
uint16_t menu_palette_colour(MenuPalette p, int i);     /* colour i, 0-255 */

typedef enum { MENU_SOUNDS_LOGO, MENU_SOUNDS_TITLE, MENU_SOUNDS_CHAMPION } MenuSounds;
void menu_sounds_upload(MenuSounds s);                  /* between sound_upload_begin and sound_upload_end */

/* --- the selection and the championship's screens -------------------------- */

uint16_t selection_control_by_choice(int choice);       /* the control a car's choice gives (a reference the race resolves) */
uint16_t selection_control_by_setting(int setting);     /* and a setting's */
uint16_t selection_choice_saved(int choice);            /* a choice (0-3) as the save keeps it */
uint16_t selection_pair_model(int choice);              /* player 2's model when both choose the same */
uint16_t selection_pair_row(int choice, int model);     /* a player's word by choice and model */
uint8_t menu_cursor_column(int stage);                  /* the class cursor's column by championship stage */
uint16_t menu_champion_frame(int step, int choice);     /* a selection frame's place in the frames, by step and choice (0-3) */
void menu_champion_panel_cells(int class_, int entry, uint16_t *first_tile, uint16_t *attributes);  /* entry 0-3 */
void menu_champion_entry_places(int entry, uint16_t *cells, uint16_t *map_at);   /* entry 0-3's cells and map place */
uint16_t menu_champion_choice_attributes(int choice);   /* the chosen car's cell attributes, by choice (0-3) */

/* --- the objects ---------------------------------------------------------- */

/* The menu objects' handlers. The spawn data and the objects' template name
   them by the original's routines; data/ resolves those to these. */
typedef enum {
    HANDLER_UNKNOWN,                    /* a word the data gives that names no handler here */
    HANDLER_ROW_SET,                    /* a settings row once set up: nothing */
    HANDLER_DONE,                       /* an object done moving, which others wait for: nothing */
    HANDLER_NO_STEP,                    /* an object with no step of its own */
    HANDLER_NO_STEP_B,                  /* the same, a second copy */
    HANDLER_ANIMATE_THEN_EXIT,
    HANDLER_DROP_START,
    HANDLER_DROP_BOUNCE,
    HANDLER_RISE_AWAY,
    HANDLER_COUNTDOWN,
    HANDLER_CROSS_FADE_IN,
    HANDLER_DROP_AWAY,
    HANDLER_AWAY_CHECK,
    HANDLER_CROSS_FADE_OUT,
    HANDLER_LOAD_FIRST,
    HANDLER_LOAD_LAST,
    HANDLER_SET_EXIT,
    HANDLER_SELECT_START,
    HANDLER_WAIT_THEN_ANIMATE,
    HANDLER_ANIMATE_ONLY,
    HANDLER_WAIT_THEN_SEQUENCE,
    HANDLER_BUTTON_SEQUENCE,
    HANDLER_ERASE_CONFIRM,
    HANDLER_ERASE_CONFIRM2,
    HANDLER_ERASE_DONE,
    HANDLER_IDLE,

    HANDLER_SELECT_ARRIVE,
    HANDLER_MODE_SELECT,
    HANDLER_SETTINGS_FRAME,
    HANDLER_PREVIEW_IDLE,
    HANDLER_WAIT_PICTURE,
    HANDLER_PREVIEW_START,
    HANDLER_PREVIEW_COUNTDOWN,
    HANDLER_PREVIEW_BACK_OUT,
    HANDLER_PREVIEW_BACK_WAIT,
    HANDLER_PREVIEW_BACK_END,
    HANDLER_SETTINGS_SELECT,
    HANDLER_SETTINGS_SELECT2,
    HANDLER_INITIALS_FRAME,
    HANDLER_ROW_START,
    HANDLER_SLIDE_TO_PREVIEW,
    HANDLER_SLIDE_IN,
    HANDLER_FADE_IN,
    HANDLER_SLIDE_IN_DONE,
    HANDLER_FADE_UP,
    HANDLER_FOLLOW_SLIDE_FAR,           /* with the sliding sprites, at $1CF less the slide */
    HANDLER_FOLLOW_SLIDE_NEAR,          /* at $194 less the slide */
    HANDLER_FADE_OUT,
    HANDLER_CHOICE_00_START,
    HANDLER_CHOICE_00,
    HANDLER_CHOICE_12_COPY,
    HANDLER_CHOICE_12_START,
    HANDLER_CHOICE_12,
    HANDLER_CHOICE_24_START,
    HANDLER_CHOICE_24,
    HANDLER_CHOICE_48_START,
    HANDLER_CHOICE_38_START,
    HANDLER_CHOICE_38,
    HANDLER_CHOICE_48,

    HANDLER_RESULTS_WAIT_THEN_RISE,
    HANDLER_RESULTS_RISE,
    HANDLER_RESULTS_OPEN_TOP,
    HANDLER_RESULTS_OPEN_BOTTOM,
    HANDLER_RESULTS_PLACE_WAIT,
    HANDLER_RESULTS_PLACE_CAR,
    HANDLER_RESULTS_LEADER_RESUME,
    HANDLER_RESULTS_LEADER_BLINK,
    HANDLER_RESULTS_WAIT_BUTTON,
    HANDLER_RESULTS_LOWER,
    HANDLER_RESULTS_RISE_AGAIN,
    HANDLER_RESULTS_WAIT_END,
    HANDLER_RESULTS_TIME_DIGITS,
    HANDLER_RESULTS_OVER_OR_HIDE,
    HANDLER_RESULTS_INITIALS,
    HANDLER_RESULTS_LAP_TIMES,
    HANDLER_RESULTS_PLACE_0,
    HANDLER_RESULTS_PLACE_0_ANIM,
    HANDLER_RESULTS_PLACE_1,
    HANDLER_RESULTS_PLACE_1_ANIM,
    HANDLER_RESULTS_PLACE_2,
    HANDLER_RESULTS_PLACE_2_ANIM,
    HANDLER_RESULTS_PLACE_3,
    HANDLER_RESULTS_PLACE_3_ANIM,
    HANDLER_SELECTION_FRAMES_CYCLE,
} MenuHandler;

/* The objects' block: their sprite groups and the screens' per-line tables,
   loaded once and changed as the objects run (actors.groups). The data
   refers to places in it, and to groups, by its own references, which
   these resolve. */
enum { MENU_BLOCK_BYTES = 0x3800 };
void menu_block_load(uint8_t *block);                   /* the block as the cartridge has it */
uint32_t menu_group_place(uint16_t group);              /* a group's place in the block */
uint16_t menu_groups_free_from(void);                   /* the first group the objects may copy into */
uint16_t menu_groups_end(void);                         /* and the end of that room */

uint16_t menu_actor_template(int i);                    /* the 44 words a new object starts from (handlers as MenuHandler) */
uint16_t menu_spawn_list_entry(uint16_t list);          /* a spawn list's entry at `list`; bit 15 ends the list */
uint16_t menu_spawn_field(uint16_t entry);              /* a spawn entry's field offset; bit 15 ends it */
uint16_t menu_spawn_value(uint16_t entry);              /* and the value it gives the field (a handler as MenuHandler) */
uint16_t menu_animation_steps(uint16_t at);             /* an animation step's duration; bit 15 ends the animation */
uint16_t menu_animation_group(uint16_t at);             /* and its sprite group */
uint16_t menu_frame_art_tile(int i);                    /* a sprite's tile in the sprite set for its art (i: 2 a sprite) */
uint16_t menu_frame_art_base(int i);                    /* and where its art is in the frames */
uint16_t menu_frame_offset(int f);                      /* a frame's offset in them (f: 2 a frame), bit 15 its vertical flip */
uint16_t menu_cross_fade_level(int n);                  /* the cross-fade's factor by step */

/* the screens' per-line tables in the block, and the blink's colours */
typedef enum {
    MENU_TABLE_BLINK_COLOURS,           /* colour $D9's two blink colours */
    MENU_TABLE_CHAMPION_SCROLL_X,       /* the championship: BG2's horizontal scroll (2-byte runs) */
    MENU_TABLE_CHAMPION_SCROLL_Y,       /* BG2's vertical scroll (runs naming words) */
    MENU_TABLE_CHAMPION_MAIN,           /* the main screen's layers */
    MENU_TABLE_CHAMPION_WINDOW,         /* window 1's right edge */
    MENU_TABLE_CHAMPION_WINDOW_B,       /* window 1's right edge, the second set */
    MENU_TABLE_TITLE_MAIN_A,            /* the title: the main screen's layers, three sets */
    MENU_TABLE_TITLE_MAIN_B,
    MENU_TABLE_TITLE_MAIN_C,
    MENU_TABLE_TITLE_WINDOW,            /* window 1's edges (2-byte runs) */
    MENU_TABLE_PREVIEW_SCROLL_X,        /* the course menu: BG2's horizontal scroll (runs naming words) */
    MENU_TABLE_PREVIEW_COLOUR,          /* the colour maths' prevent and clip */
    MENU_TABLE_PREVIEW_MASK,            /* the layers the windows mask */
    MENU_TABLE_PREVIEW_WINDOW,          /* window 1's left edge */
    MENU_TABLES
} MenuTable;
uint32_t menu_table_place(MenuTable t);                 /* its place in the block */
enum { MENU_LINE_TABLES_BYTES = 0x81 };                 /* the block's bytes the per-line tables are in, */
uint32_t menu_line_tables_place(void);                  /* from here */

/* --- the championship's results, the two-player race screen ---------------- */

uint16_t menu_place_car_animation(int leader, int car); /* a place's car animation by car (0-3), the leader's or the others' */
uint16_t menu_leader_palette(int step);                 /* the leader marker's palette bits, by its blink's step (0-1) */
uint16_t menu_letter_tile(int letter);                  /* a results name's letter tile, 0-31 */

/* the two-player race screen's sprite lists: places and looks of sprites 0
   and 64 up, then their X ninth bits and sizes; each ended by $FFFF */
typedef enum {
    MENU_COURSE_SPRITES_0, MENU_COURSE_SPRITES_0_SIZES,
    MENU_COURSE_SPRITES_64, MENU_COURSE_SPRITES_64_SIZES
} MenuSpriteList;
uint16_t menu_course_sprite_word(MenuSpriteList list, int k);  /* word k of the list */
uint16_t menu_course_picture_place(int course);         /* where a course's picture is among the unpacked pictures (packed form) */

/* the speed gauge's needle sprite by speed band (0-$95), for the car its
   course script makes (the four cars and the others) */
uint8_t menu_gauge_needle(uint16_t car_script, int band);

/* --- the records ---------------------------------------------------------- */

uint16_t records_first_tag(int entry);                  /* each sorted entry's first tag, 0-11 */
uint16_t records_sprite_group_count(int group);         /* a sprite group's count word, which places the next group drawn after it */
uint16_t records_panel_row_start(int stunt, int row);   /* a records panel's row in the map, Stunt Trax's or the others' */
uint8_t records_panel_column(int stunt, int character); /* and a character's column in it */

/* the menus' words a per-line table's run names */
typedef enum {
    MENU_WORD_NONE = -1,
    MENU_WORD_SLIDE_TOP, MENU_WORD_SLIDE_LOW,
    MENU_WORD_PANEL_CLASS, MENU_WORD_PANEL_TOP, MENU_WORD_PANEL_LOW, MENU_WORD_PANEL_STILL,
    MENU_WORDS
} MenuWord;
MenuWord menu_line_word(uint16_t name);                 /* the word a run's two bytes name */

#endif
