/*
 * race_tables.c: where the race's and the HUD's tables are in the cartridge
 * (bank and address, the original's), each written once here, and their
 * reads. Every read is the original's: the address sum kept to 16 bits, a
 * word little-endian.
 */
#include "data/cart.h"
#include "data/race_tables.h"
#include "data/unpack.h"

typedef struct { uint8_t bank; uint16_t at; } Place;

static const Place
    SINE = {0x00, 0x96FE}, COSINE = {0x00, 0x973E},
    DEMO_SCRIPTS = {0x03, 0x8004},
    COURSE_SCRIPTS = {0x03, 0xF4A5},            /* three bytes a course: its script's address, then its bank */
    COURSE_MENU_SCRIPT = {0x05, 0x8000},
    STUNT_COURSES = {0x03, 0xFB86}, BATTLE_COURSES = {0x03, 0xFB8A},
    ZOOM_BOXES = {0x03, 0xFC11}, ZOOM_TARGETS = {0x03, 0xFB8E},
    FLASH_SPRITE_PLACES = {0x02, 0xFC58},
    COURSE_PALETTE = {0x1F, 0x8200},
    COURSE_TIMES = {0x03, 0xD813}, CLASS_TIMES = {0x03, 0xD810}, STUNT_TIMES = {0x03, 0xD8A3},
    START_TIME_SIZES = {0x07, 0xE33A}, BANNER_SIZES = {0x07, 0xE95A},
    TALLY_GLYPHS = {0x07, 0xDFA0}, TALLY_X_ENDS = {0x07, 0xDF70}, STUNT_COUNT_STEPS = {0x07, 0xE115},
    CAR_PICTURES = {0x07, 0xCCB7}, CAR_CHOICES = {0x07, 0xCE93},
    CHAMPION_PALETTE = {0x1F, 0x8600}, TOTALS_WORD_LETTERS = {0x07, 0xD5AD},
    SPRITE_LISTS = {0x03, 0xF883},
    SPLIT_PANEL_SPRITES = {0x03, 0xE7EA}, SPLIT_MAP_SPRITES = {0x03, 0xE80F}, SPLIT_SPRITE_SIZES = {0x03, 0xE80A},
    CLASS_NAME_TILES = {0x02, 0xEFD6},
    BIG_DIGIT_GLYPHS = {0x07, 0x8E3E}, SMALL_DIGIT_GLYPHS = {0x07, 0x8E52},
    DIGIT_GROW_SIZES = {0x07, 0x918F}, FRAME_X_OFFSETS = {0x07, 0x9806},
    FRAME_TAIL_A_WIDTHS = {0x07, 0x97EE}, FRAME_TAIL_A_STEPS = {0x07, 0x97FA},
    FRAME_TAIL_B_WIDTHS = {0x07, 0x97D6}, FRAME_TAIL_B_STEPS = {0x07, 0x97E2},
    FRAME_MIDDLE_WIDTHS = {0x07, 0x97BE}, FRAME_MIDDLE_STEPS = {0x07, 0x97CA},
    LAPS_TO_GO_HEIGHTS = {0x07, 0x85CC},
    COUNTDOWN_SHAPES = {0x07, 0x8DDA},          /* 8 bytes each: size, scale, x, y */
    GLYPH_SPRITE_XS = {0x04, 0xDA7A}, GLYPH_SPRITE_GLYPHS = {0x04, 0xDA8E},
    GROW_SIZES = {0x04, 0xDAA2}, GROW_SCALES = {0x04, 0xDAB2}, GROW_X_OFFSETS = {0x04, 0xDAC2},
    GROW_Y_OFFSETS = {0x04, 0xDAD2},
    MESSAGES = {0x04, 0xDF6D};                  /* each the address in bank $04 of a length byte, then a glyph word a letter */

static uint8_t byte_at(Place p, unsigned offset) { return cart8(p.bank, (uint16_t)(p.at + offset)); }
static uint16_t word_at(Place p, unsigned offset) { return cart16(p.bank, (uint16_t)(p.at + offset)); }

uint8_t trig_sine(uint8_t angle) { return byte_at(SINE, angle); }
uint8_t trig_cosine(uint8_t angle) { return byte_at(COSINE, angle); }
uint8_t trig_sine_in_bank(uint8_t bank, uint8_t angle) { return cart8(bank, (uint16_t)(SINE.at + angle)); }
uint8_t trig_cosine_in_bank(uint8_t bank, uint8_t angle) { return cart8(bank, (uint16_t)(COSINE.at + angle)); }

uint8_t demo_script_byte(uint16_t at) { return byte_at(DEMO_SCRIPTS, at); }
uint16_t demo_script_word(uint16_t at) { return word_at(DEMO_SCRIPTS, at); }

CartRef course_script(uint8_t course)
{
    unsigned i = (uint8_t)(course * 3);
    CartRef r = {byte_at(COURSE_SCRIPTS, i + 2), word_at(COURSE_SCRIPTS, i)};

    return r;
}

CartRef course_menu_script(void)
{
    CartRef r = {COURSE_MENU_SCRIPT.bank, COURSE_MENU_SCRIPT.at};

    return r;
}

uint8_t course_in_list(CourseList list, uint16_t slot)
{
    return byte_at(list == COURSE_LIST_STUNT ? STUNT_COURSES : BATTLE_COURSES, slot);
}
uint16_t course_zoom_box_word(uint16_t box, int i) { return word_at(ZOOM_BOXES, (unsigned)(box + 2 * i)); }
uint16_t course_zoom_target_word(uint16_t target, int i) { return word_at(ZOOM_TARGETS, (unsigned)(target + 2 * i)); }
uint16_t course_flash_sprite_place(uint16_t stage) { return word_at(FLASH_SPRITE_PLACES, (unsigned)(stage * 2)); }
uint16_t course_palette_colour(int i) { return word_at(COURSE_PALETTE, (unsigned)(2 * i)); }

/* Each block's two records in bank 3: where its packed data is (an address,
   then its bank), and its transfer (its length at +5). */
static const struct { uint16_t packed, transfer; } k_blocks[] = {
    [RACE_BLOCK_COURSE_ART] = {0xF7E6, 0xF837},
    [RACE_BLOCK_SHARED_ART] = {0xF801, 0xF822},
    [RACE_BLOCK_SPLIT_ART] = {0xF804, 0xF845},
    [RACE_BLOCK_VIEWS_SPEED] = {0xF7F2, 0xF868},
    [RACE_BLOCK_VIEWS_TEST_RUN] = {0xF7FB, 0xF868},
    [RACE_BLOCK_VIEWS_BATTLE] = {0xF7F5, 0xF868},
    [RACE_BLOCK_VIEWS_STUNT] = {0xF7F8, 0xF868},
    [RACE_BLOCK_MODE_ART_SPEED] = {0xF7E9, 0xF83E},
    [RACE_BLOCK_MODE_ART_BATTLE] = {0xF7EF, 0xF83E},
    [RACE_BLOCK_MODE_ART_STUNT] = {0xF7EC, 0xF83E},
};
enum { BLOCK_RECORDS_BANK = 0x03 };

uint32_t race_block_unpack(RaceBlock block, uint8_t *out, uint32_t room)
{
    Place packed = {BLOCK_RECORDS_BANK, k_blocks[block].packed}, transfer = {BLOCK_RECORDS_BANK, k_blocks[block].transfer};

    unpack((uint32_t)byte_at(packed, 2) << 16 | word_at(packed, 0), out, room);
    return word_at(transfer, 5);
}

uint8_t course_lap_time(uint8_t entry, int lap) { return byte_at(COURSE_TIMES, (unsigned)(lap + entry)); }
uint8_t class_time(uint8_t course_class) { return byte_at(CLASS_TIMES, course_class); }
uint8_t stunt_time(uint8_t entry) { return byte_at(STUNT_TIMES, entry); }

uint16_t start_time_size(uint16_t step) { return word_at(START_TIME_SIZES, step); }
uint8_t banner_size(uint16_t step) { return byte_at(BANNER_SIZES, step); }
uint16_t tally_glyph(uint16_t i) { return word_at(TALLY_GLYPHS, (unsigned)(2 * i)); }
uint16_t tally_x_end(uint16_t i) { return word_at(TALLY_X_ENDS, (unsigned)(2 * i)); }
uint8_t stunt_count_step(uint16_t step) { return byte_at(STUNT_COUNT_STEPS, step); }

uint16_t results_car_picture(uint16_t car) { return word_at(CAR_PICTURES, (unsigned)(car * 2)); }
uint8_t results_car_choice(uint16_t i) { return byte_at(CAR_CHOICES, i); }
uint16_t champion_palette_colour(int i) { return word_at(CHAMPION_PALETTE, (unsigned)(2 * i)); }
uint8_t results_totals_word_letter(int i) { return byte_at(TOTALS_WORD_LETTERS, (unsigned)i); }

uint8_t hud_sprite_list_byte(uint16_t at) { return byte_at(SPRITE_LISTS, at); }
uint16_t split_panel_sprite_word(uint16_t at) { return word_at(SPLIT_PANEL_SPRITES, at); }
uint16_t split_map_sprite_word(uint16_t at) { return word_at(SPLIT_MAP_SPRITES, at); }
uint8_t split_sprite_size(uint16_t i) { return byte_at(SPLIT_SPRITE_SIZES, i); }
uint8_t class_name_tile(uint16_t at) { return byte_at(CLASS_NAME_TILES, at); }
uint16_t hud_big_digit_glyph(uint16_t at) { return word_at(BIG_DIGIT_GLYPHS, at); }
uint16_t hud_small_digit_glyph(uint16_t at) { return word_at(SMALL_DIGIT_GLYPHS, at); }
uint16_t hud_digit_grow_size(uint16_t step) { return word_at(DIGIT_GROW_SIZES, step); }
uint16_t hud_frame_x_offset(uint16_t step) { return word_at(FRAME_X_OFFSETS, step); }
uint16_t hud_frame_tail_a_width(uint16_t step) { return word_at(FRAME_TAIL_A_WIDTHS, step); }
uint16_t hud_frame_tail_a_step(uint16_t step) { return word_at(FRAME_TAIL_A_STEPS, step); }
uint16_t hud_frame_tail_b_width(uint16_t step) { return word_at(FRAME_TAIL_B_WIDTHS, step); }
uint16_t hud_frame_tail_b_step(uint16_t step) { return word_at(FRAME_TAIL_B_STEPS, step); }
uint16_t hud_frame_middle_width(uint16_t at) { return word_at(FRAME_MIDDLE_WIDTHS, at); }
uint16_t hud_frame_middle_step(uint16_t at) { return word_at(FRAME_MIDDLE_STEPS, at); }
uint8_t hud_laps_to_go_height(uint16_t step) { return byte_at(LAPS_TO_GO_HEIGHTS, step); }

/* The glyphs' pictures in bank $15, each set a sheet of rows 32 bytes apart,
   2 bits a pixel: the 16 x 32 set from $8000 (eight to a row of $400), the
   8 x 16 set from $9400 (sixteen to a row of $200), the big sprites' 24 x 40
   letters from $A000 (five to a row, the rows at $A000, $A500, $AA00 and
   $B000), the HUD's pieces from $9A00
   (the original names that one $1A00, the same bytes), the rank badges and
   Stunt Trax's marks. A glyph's number in the first two sets: the digits,
   then the letters, then the marks. */
enum { SHEET_16X32 = 0x8000, SHEET_8X16 = 0x9400, SHEET_24X40 = 0xA000, SHEET_PIECES = 0x1A00 };
static const uint16_t k_pictures[] = {
    [HUD_RANK_FIRST] = 0x080E, [HUD_RANK_OTHER] = 0x0A0E,
    [HUD_STUNT_MARK] = 0x0C08, [HUD_STUNT_MARK_USED] = 0x0E08,
};
static const char k_big_letters[] = "ACEFGHIKLMNOPRSTUV";

/* the number of c among the digits, the letters and `marks` after them; -1 for ' ' */
static int glyph_number(char c, const char *marks)
{
    int i;

    if (c == ' ')
        return -1;
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'Z')
        return 10 + (c - 'A');
    for (i = 0; marks[i]; i++)
        if (marks[i] == c)
            return 36 + i;
    return -1;
}

uint16_t hud_glyph_16x32(char c)
{
    int n = glyph_number(c, ".'\"!");

    return n < 0 ? 0 : (uint16_t)(SHEET_16X32 + (n / 8) * 0x400 + (n % 8) * 4);
}
uint16_t hud_glyph_8x16_number(uint8_t n) { return (uint16_t)(SHEET_8X16 + (n / 16) * 0x200 + (n % 16) * 2); }
uint16_t hud_glyph_8x16(char c)
{
    int n = glyph_number(c, ".'\"");

    return n < 0 ? 0 : hud_glyph_8x16_number((uint8_t)n);
}
uint16_t hud_glyph_24x40(char c)
{
    int n;

    for (n = 0; k_big_letters[n] && k_big_letters[n] != c; n++)
        ;
    static const uint16_t rows[4] = {SHEET_24X40, 0xA500, 0xAA00, 0xB000};   /* the fourth row is not $500 on */

    return c == ' ' || !k_big_letters[n] ? 0 : (uint16_t)(rows[n / 5] + (n % 5) * 6);
}
/* the places' sheet in bank $04, 24 x 32 glyphs six bytes apart: "123td" in
   its first row, "snr" in the row $400 below */
uint16_t hud_place_glyph(char c)
{
    static const char rows[2][6] = {"123td", "snr"};
    int r, i;

    for (r = 0; r < 2; r++)
        for (i = 0; rows[r][i]; i++)
            if (rows[r][i] == c)
                return (uint16_t)(0x8000 + r * 0x400 + i * 6);
    return 0;
}
enum { GLYPHS_BANK = 0x15, PLACES_BANK = 0x04, BANNER_PICTURE = 0x3D00 };
uint8_t hud_glyphs_bank(void) { return GLYPHS_BANK; }
uint8_t hud_places_bank(void) { return PLACES_BANK; }
uint16_t hud_banner_picture(uint16_t column) { return (uint16_t)(BANNER_PICTURE + column / 4); }
uint16_t hud_pieces_at(uint16_t x, uint16_t line) { return (uint16_t)(SHEET_PIECES + line * 32 + x / 4); }
uint16_t hud_picture(HudPicture p) { return k_pictures[p]; }

HudShape hud_countdown_shape(uint16_t n)
{
    uint16_t at = (uint16_t)(n * 8);
    HudShape s;

    s.size = word_at(COUNTDOWN_SHAPES, at);
    s.scale = word_at(COUNTDOWN_SHAPES, (unsigned)(at + 2));
    s.x = word_at(COUNTDOWN_SHAPES, (unsigned)(at + 4));
    s.y = word_at(COUNTDOWN_SHAPES, (unsigned)(at + 6));
    return s;
}

uint16_t glyph_sprite_start_x(uint16_t i) { return word_at(GLYPH_SPRITE_XS, (unsigned)(2 * i)); }
uint16_t glyph_sprite_start_glyph(uint16_t i) { return word_at(GLYPH_SPRITE_GLYPHS, (unsigned)(2 * i)); }
uint16_t glyph_grow_size(uint16_t at) { return word_at(GROW_SIZES, at); }
uint16_t glyph_grow_scale(uint16_t at) { return word_at(GROW_SCALES, at); }
uint16_t glyph_grow_x_offset(uint16_t at) { return word_at(GROW_X_OFFSETS, at); }
uint16_t glyph_grow_y_offset(uint16_t at) { return word_at(GROW_Y_OFFSETS, at); }

/* a message's own place in bank $04, from the list */
static Place message(uint16_t index) { Place p = {MESSAGES.bank, word_at(MESSAGES, index)}; return p; }
uint16_t hud_message_letters(uint16_t index) { return (uint16_t)(word_at(message(index), 0) & 0x00FF); }
uint16_t hud_message_glyph(uint16_t index, uint16_t k) { return word_at(message(index), (unsigned)(1 + 2 * k)); }
