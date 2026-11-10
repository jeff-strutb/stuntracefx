/*
 * menu_tables.c: where the menus' tables are in the cartridge image (each
 * written once, as the image's offset, as in assets.csv), read as what they
 * hold.
 */
#include <string.h>

#include "data/cart.h"
#include "data/menu_tables.h"

enum {
    SINE = 0x0016FE,                    /* bytes, by angle */
    SELECTION_ROWS = 0x01B7B1,          /* bytes, by choice */
    SELECTION_MODELS = 0x01B7B5,        /* bytes, 4 a choice, by setting row */
    CAR_SETTINGS = 0x05B00C,            /* words, by choice: where its rows start among CAR_SETTING_ROWS */
    CAR_SETTING_ROWS = 0x05B014,        /* bytes, 3 a combination */
    SAVED_CHOICES = 0x05B9BA,           /* words, by saved value */
    SETTING_OBJECTS = 0x05C15E,         /* words, by setting */
    SETTING_ANIMATIONS = 0x05C1B0,      /* words, 4 a setting, by row */
    INITIAL_TILES = 0x05C528,           /* words, by letter */
    CHOICE_REPEAT = 0x05C758,           /* words, by choice */
    CHOICE_LEFT = 0x05C81F,             /* words, by choice */
    CHOICE_RIGHT = 0x05C827,            /* words, by choice */
    PAIR_ANIMATIONS = 0x05C862,         /* two words a pair */
    BUTTON_SEQUENCE = 0x05D66D          /* words, by step */
};

/* the objects' block, and their tables in the cartridge */
enum {
    BLOCK = 0x058000,                   /* the block's first $2C00 bytes */
    BLOCK_LOADED = 0x2C00,
    BLOCK_ORIGIN = 0x0800,              /* the address the data's places in the block count from */
    GROUP_ORIGIN = 0x1356,              /* and the one groups count from */
    GROUPS_FREE = 0x2881,               /* the groups the objects copy go from here */
    GROUPS_END = 0x2C81,
    ACTOR_TEMPLATE = 0x05EA0F,          /* 44 words */
    SPAWN_LISTS = 0x05E4E7,             /* words: entries */
    SPAWN_ENTRIES = 0x05E547,           /* a field offset and a value */
    ANIMATIONS = 0x05AB60,              /* a duration and a group a step */
    FRAME_ART_SLOTS = 0x05CCBC,         /* words: video words, $4000 the sprite set's tile 0 */
    FRAME_ART_BASES = 0x05CD14,         /* words */
    FRAME_OFFSETS = 0x05CC7C,           /* words */
    SPRITE_SET_SLOT = 0x4000,
    CROSS_FADE_LEVELS = 0x05CFE5        /* words, by step (even) */
};

/* the per-line tables' places in the block, by the data's own addresses */
static const uint16_t table_places[MENU_TABLES] = {
    [MENU_TABLE_BLINK_COLOURS] = 0x0DA8,
    [MENU_TABLE_CHAMPION_SCROLL_X] = 0x12D5, [MENU_TABLE_CHAMPION_SCROLL_Y] = 0x12E2,
    [MENU_TABLE_CHAMPION_MAIN] = 0x12F2, [MENU_TABLE_CHAMPION_WINDOW] = 0x12FD, [MENU_TABLE_CHAMPION_WINDOW_B] = 0x1308,
    [MENU_TABLE_TITLE_MAIN_A] = 0x1315, [MENU_TABLE_TITLE_MAIN_B] = 0x131A, [MENU_TABLE_TITLE_MAIN_C] = 0x131F,
    [MENU_TABLE_TITLE_WINDOW] = 0x1324,
    [MENU_TABLE_PREVIEW_SCROLL_X] = 0x133A, [MENU_TABLE_PREVIEW_COLOUR] = 0x1341,
    [MENU_TABLE_PREVIEW_MASK] = 0x1348, [MENU_TABLE_PREVIEW_WINDOW] = 0x1353
};

/* the words the runs name, by the data's own addresses for them */
static const struct { uint16_t name; MenuWord word; } line_words[] = {
    {0x18BB, MENU_WORD_SLIDE_TOP}, {0x18BD, MENU_WORD_SLIDE_LOW},
    {0x1919, MENU_WORD_PANEL_CLASS}, {0x191B, MENU_WORD_PANEL_TOP}, {0x191D, MENU_WORD_PANEL_LOW}, {0x191F, MENU_WORD_PANEL_STILL}
};

static uint8_t byte_at(uint32_t at) { return cart[at & (CART_SIZE - 1)]; }
static uint16_t word_at(uint32_t at) { return (uint16_t)(byte_at(at) | byte_at(at + 1) << 8); }

int8_t menu_sine(uint8_t angle) { return (int8_t)byte_at(SINE + angle); }
uint16_t menu_button_sequence(int step) { return word_at(BUTTON_SEQUENCE + (uint32_t)(uint16_t)(step << 1)); }

void menu_pair_animations(int pair, uint16_t *first, uint16_t *second)
{
    *first = word_at(PAIR_ANIMATIONS + (uint32_t)(pair << 2));
    *second = word_at(PAIR_ANIMATIONS + (uint32_t)(pair << 2) + 2);
}

uint16_t menu_choice_repeat_model(int choice) { return word_at(CHOICE_REPEAT + (uint32_t)choice * 2); }
uint16_t menu_choice_step(int right, int choice) { return word_at((right ? CHOICE_RIGHT : CHOICE_LEFT) + (uint32_t)choice * 2); }
uint16_t menu_saved_choice(int saved) { return word_at(SAVED_CHOICES + (uint32_t)(saved & 3) * 2); }
uint16_t menu_initial_tile(int letter) { return word_at(INITIAL_TILES + (uint32_t)letter * 2); }

uint8_t menu_car_setting_row(int choice, int combination, int k)
{
    uint16_t first = word_at(CAR_SETTINGS + (uint32_t)choice * 2);

    return byte_at(CAR_SETTING_ROWS + (uint16_t)(first + combination * 3 + k));
}

uint8_t menu_selection_row(int choice) { return byte_at(SELECTION_ROWS + (uint32_t)choice); }
uint8_t menu_selection_model(int choice, int row) { return byte_at(SELECTION_MODELS + (uint32_t)(choice * 4 + row)); }
uint16_t menu_setting_animation(int setting, int row) { return word_at(SETTING_ANIMATIONS + (uint32_t)(setting * 8 + row * 2)); }
uint16_t menu_setting_object(int i) { return word_at(SETTING_OBJECTS + (uint32_t)i * 2); }

void menu_block_load(uint8_t *block)
{
    memcpy(block, &cart[BLOCK], BLOCK_LOADED);
}

uint32_t menu_group_place(uint16_t group) { return (uint32_t)(GROUP_ORIGIN - BLOCK_ORIGIN) + group; }
uint16_t menu_groups_free_from(void) { return GROUPS_FREE; }
uint16_t menu_groups_end(void) { return GROUPS_END; }

uint16_t menu_actor_template(int i) { return word_at(ACTOR_TEMPLATE + (uint32_t)i * 2); }
uint16_t menu_spawn_list_entry(uint16_t list) { return word_at(SPAWN_LISTS + list); }
uint16_t menu_spawn_field(uint16_t entry) { return word_at(SPAWN_ENTRIES + entry); }
uint16_t menu_spawn_value(uint16_t entry) { return word_at(SPAWN_ENTRIES + (uint32_t)entry + 2); }
uint16_t menu_animation_steps(uint16_t at) { return word_at(ANIMATIONS + at); }
uint16_t menu_animation_group(uint16_t at) { return word_at(ANIMATIONS + (uint32_t)at + 2); }
uint16_t menu_frame_art_tile(int i) { return (uint16_t)((word_at(FRAME_ART_SLOTS + (uint32_t)i) - SPRITE_SET_SLOT) / 16); }
uint16_t menu_frame_art_base(int i) { return word_at(FRAME_ART_BASES + (uint32_t)i); }
uint16_t menu_frame_offset(int f) { return word_at(FRAME_OFFSETS + (uint32_t)f); }
uint16_t menu_cross_fade_level(int n) { return word_at(CROSS_FADE_LEVELS + (uint32_t)(n & 0xFFFE)); }

uint32_t menu_table_place(MenuTable t) { return (uint32_t)(table_places[t] - BLOCK_ORIGIN); }

/* the championship's horizontal scroll to the course menu's window: all of
   the screens' tables, the window's three bytes the last */
_Static_assert(MENU_LINE_TABLES_BYTES == 0x1353 + 3 - 0x12D5, "the per-line tables' bytes");
uint32_t menu_line_tables_place(void) { return (uint32_t)(table_places[MENU_TABLE_CHAMPION_SCROLL_X] - BLOCK_ORIGIN); }

MenuWord menu_line_word(uint16_t name)
{
    unsigned i;

    for (i = 0; i < sizeof line_words / sizeof line_words[0]; i++)
        if (line_words[i].name == name)
            return line_words[i].word;
    return MENU_WORD_NONE;
}
