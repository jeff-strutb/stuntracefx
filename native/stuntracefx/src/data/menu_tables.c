/*
 * menu_tables.c: where the menus' tables are in the cartridge image (each
 * written once, as the image's offset, as in assets.csv), read as what they
 * hold.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "data/menu_tables.h"
#include "data/unpack.h"

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

/* the results', the two-player race screen's and the records' tables */
enum {
    PLACE_CAR_ANIMATIONS = 0x05DA0F,    /* words, by car */
    LEADER_CAR_ANIMATIONS = 0x05DA17,   /* words, by car */
    LEADER_PALETTES = 0x05DA86,         /* words, by step */
    LETTER_TILES = 0x05DDD3,            /* words, by letter */
    COURSE_SPRITE_LISTS = 0x05FCA2,     /* words */
    RECORD_TAGS = 0x03B4EB,             /* words, by entry */
    PANEL_ROWS_STUNT = 0x03D8D4,        /* words, by row */
    PANEL_COLUMNS_STUNT = 0x03D8DC,     /* bytes, by character */
    PANEL_ROWS = 0x03D8E3,
    PANEL_COLUMNS = 0x03D8EB
};

/* the sprite lists' places among COURSE_SPRITE_LISTS */
static const uint16_t course_sprite_lists[] = {
    [MENU_COURSE_SPRITES_0] = 0x0000, [MENU_COURSE_SPRITES_0_SIZES] = 0x0154,
    [MENU_COURSE_SPRITES_64] = 0x00A8, [MENU_COURSE_SPRITES_64_SIZES] = 0x0162
};

/* the screens' pictures (packed: where each ends), tiles, palettes and sounds */
static const uint32_t pictures[MENU_PICS] = {
    [MENU_PIC_TITLE_SPRITES] = 0x0E5C38,
    [MENU_PIC_TITLE_BG2_PAGE2] = 0x0E49DC,
    [MENU_PIC_TITLE_BG1_TILES] = 0x0E3ABC,
    [MENU_PIC_TITLE_BG1_MAP] = 0x0E3E58,
    [MENU_PIC_TITLE_BG2_TILES] = 0x0E4664,
    [MENU_PIC_TITLE_BG2_PAGE1] = 0x0E4818,
    [MENU_PIC_TITLE_CHECKS_MAP] = 0x0E3ECC,
    [MENU_PIC_GRANITE_MAP] = 0x0E5C8C,
    [MENU_PIC_MENU_BG2_TILES] = 0x0BCC78,
    [MENU_PIC_MENU_BG2_MAP] = 0x0BCE78,
    [MENU_PIC_MENU_SPRITES] = 0x0BDD8C,
    [MENU_PIC_GRANITE_FRAMES] = 0x0E9068,
    [MENU_PIC_SPRITE_FRAMES] = 0x0BF8E8,
    [MENU_PIC_CHAMPION_FRAMES] = 0x026BCB,
    [MENU_PIC_CHAMPION_SPRITES] = 0x0C4770,
    [MENU_PIC_CHAMPION_8BPP] = 0x0C198C,
    [MENU_PIC_CHAMPION_BG2_TILES] = 0x0E189C,
    [MENU_PIC_CHAMPION_MAP] = 0x02687B,
    [MENU_PIC_COURSE_PICTURES] = 0x0ED970
};
enum {
    SPRITE_FRAMES_ART = 0x3FD7,         /* the frame art's names for the two sets of frames: */
    GRANITE_FRAMES_ART = 0xA7D7,        /* where the original unpacked them in its RAM */
    LOGO_SPRITES = 0x0E5C8C, TITLE_TEXT_TILES = 0x0E3E58, LOADER_TILES = 0x05D56C,
    PALETTE_TITLE = 0x0E189C, PALETTE_MENU = 0x0E63AC, PALETTE_CHAMPION = 0x026BCB,
    SOUND_TITLE_SET = 0x0F73B5,           /* packed */
    SOUND_CHAMPION_SET = 0x0EF884,        /* packed */
    SOUND_LOGO_SET = 0x0CC27C, SOUND_SAMPLES_A = 0x0D85F8, SOUND_SAMPLES_B = 0x0C9A98,
    CONTROL_BY_CHOICE = 0x01B6F1,         /* words, by choice */
    CONTROL_BY_SETTING = 0x01B704,        /* words, by setting */
    CHOICE_SAVED = 0x01B540,              /* words, by choice */
    PAIR_MODELS = 0x01B464,               /* words, by choice */
    PAIR_ROWS = 0x01B46C,                 /* words, 4 a choice, by model */
    CURSOR_COLUMNS = 0x016B49,            /* bytes, by stage */
    CHAMPION_FRAME_STEPS = 0x05DFCB,      /* words, by step */
    CHAMPION_FRAME_CHOICES = 0x05DFD3,    /* words, by choice */
    CHAMPION_CLASS_CELLS = 0x05ED9F,      /* 16 bytes a class: first tile and attributes, by entry */
    CHAMPION_ENTRY_PLACES = 0x05EDCF,     /* words: the four entries' cells, then their map places */
    CHAMPION_CHOICE_ATTRS = 0x05EDDF      /* words, by choice */
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

uint16_t menu_place_car_animation(int leader, int car)
{
    return word_at((leader ? LEADER_CAR_ANIMATIONS : PLACE_CAR_ANIMATIONS) + (uint32_t)(car & 3) * 2);
}

uint16_t menu_leader_palette(int step) { return word_at(LEADER_PALETTES + (uint32_t)step * 2); }
uint16_t menu_letter_tile(int letter) { return word_at(LETTER_TILES + (uint32_t)letter * 2); }

uint16_t menu_course_sprite_word(MenuSpriteList list, int k)
{
    return word_at(COURSE_SPRITE_LISTS + course_sprite_lists[list] + (uint32_t)k * 2);
}

uint16_t records_first_tag(int entry) { return word_at(RECORD_TAGS + (uint32_t)entry * 2); }

uint16_t records_panel_row_start(int stunt, int row)
{
    return word_at((stunt ? PANEL_ROWS_STUNT : PANEL_ROWS) + (uint32_t)row * 2);
}

uint8_t records_panel_column(int stunt, int character)
{
    return byte_at((stunt ? PANEL_COLUMNS_STUNT : PANEL_COLUMNS) + (uint32_t)character);
}

/* the unpacker's and the sound driver's uploads take a bank and address */
static uint32_t bank_address(uint32_t image) { return (image >> 15) << 16 | 0x8000 | (image & 0x7FFF); }

uint32_t menu_picture_unpack(MenuPicture p, uint8_t *to, uint32_t room)
{
    return unpack(bank_address(pictures[p]), to, room);
}

MenuFrames menu_frames_place(uint16_t art, uint32_t *at)
{
    if (art >= GRANITE_FRAMES_ART) {
        *at = (uint32_t)(art - GRANITE_FRAMES_ART);
        return MENU_FRAMES_GRANITE;
    }
    *at = (uint32_t)(art - SPRITE_FRAMES_ART);
    return MENU_FRAMES_SPRITES;
}

const uint8_t *menu_tiles(MenuTiles t)
{
    static const uint32_t places[] = { [MENU_TILES_LOGO_SPRITES] = LOGO_SPRITES, [MENU_TILES_TITLE_TEXT] = TITLE_TEXT_TILES,
                                       [MENU_TILES_LOADER] = LOADER_TILES };

    return &cart[places[t]];
}

uint16_t menu_palette_colour(MenuPalette p, int i)
{
    static const uint32_t places[] = { [MENU_PALETTE_TITLE] = PALETTE_TITLE, [MENU_PALETTE_MENU] = PALETTE_MENU,
                                       [MENU_PALETTE_CHAMPION] = PALETTE_CHAMPION };

    return word_at(places[p] + (uint32_t)i * 2);
}

static void sound_cart(uint32_t image)
{
    uint32_t r = bank_address(image);

    sound_upload_cart((uint8_t)(r >> 16), (uint16_t)r);
}

static void sound_packed(uint32_t image)
{
    uint32_t r = bank_address(image);

    sound_upload_packed((uint16_t)r, (uint8_t)(r >> 16));
}

void menu_sounds_upload(MenuSounds s)
{
    switch (s) {
    case MENU_SOUNDS_LOGO:
        sound_cart(SOUND_LOGO_SET);
        break;
    case MENU_SOUNDS_TITLE:
    case MENU_SOUNDS_CHAMPION:
        sound_packed(s == MENU_SOUNDS_TITLE ? SOUND_TITLE_SET : SOUND_CHAMPION_SET);
        sound_cart(SOUND_SAMPLES_A);
        sound_cart(SOUND_SAMPLES_B);
        break;
    }
}

uint16_t selection_control_by_choice(int choice) { return word_at(CONTROL_BY_CHOICE + (uint32_t)choice * 2); }
uint16_t selection_control_by_setting(int setting) { return word_at(CONTROL_BY_SETTING + (uint32_t)setting * 2); }
uint16_t selection_choice_saved(int choice) { return word_at(CHOICE_SAVED + (uint32_t)(choice & 3) * 2); }
uint16_t selection_pair_model(int choice) { return word_at(PAIR_MODELS + (uint32_t)choice * 2); }
uint16_t selection_pair_row(int choice, int model) { return word_at(PAIR_ROWS + (uint32_t)((choice << 2) + model) * 2); }
uint8_t menu_cursor_column(int stage) { return byte_at(CURSOR_COLUMNS + (uint32_t)stage); }

uint16_t menu_champion_frame(int step, int choice)
{
    return (uint16_t)(word_at(CHAMPION_FRAME_STEPS + (uint32_t)step * 2) + word_at(CHAMPION_FRAME_CHOICES + (uint32_t)(choice & 3) * 2));
}

void menu_champion_panel_cells(int class_, int entry, uint16_t *first_tile, uint16_t *attributes)
{
    uint32_t at = CHAMPION_CLASS_CELLS + (uint32_t)(class_ * 16 + entry * 4);

    *first_tile = word_at(at);
    *attributes = word_at(at + 2);
}

void menu_champion_entry_places(int entry, uint16_t *cells, uint16_t *map_at)
{
    *cells = word_at(CHAMPION_ENTRY_PLACES + (uint32_t)entry * 2);
    *map_at = word_at(CHAMPION_ENTRY_PLACES + 8 + (uint32_t)entry * 2);
}

uint16_t menu_champion_choice_attributes(int choice) { return word_at(CHAMPION_CHOICE_ATTRS + (uint32_t)(choice & 3) * 2); }

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
