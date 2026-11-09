/*
 * records_screen.c: the course screens' records pages: the best times, names
 * and cars kept in the save, the Speed Trax pages of each course's times,
 * and the menu under them. The course screens' state machine ($0D40, race)
 * runs these; each is one frame's work.
 */
#include <stdint.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "hud/hud.h"
#include "menu/menu.h"
#include "race/race.h"
#include "save/save.h"
#include "system/game.h"
#include "system/state.h"
#include "video/video.h"
#include "video/picture.h"

static uint16_t pad(void) { return frame_state.pressed[0]; }

/* The slots of the metasprite groups the page has drawn, the words at
   $0D71-$0D7B (the first is 0); the sprite palette routines read $0D75,
   $0D79 and $0D7B. */
enum { S_71, S_73, S_75, S_77, S_79, S_7B };

static uint16_t *slot_at(int s)
{
    switch (s) {
    case S_71: return &g.sprite_slot_d71;
    case S_73: return &g.sprite_slot_d73;
    case S_75: return &g.sprite_row_d75;
    case S_77: return &g.sprite_slot_d77;
    case S_79: return &g.sprite_row_d79;
    default:   return &g.oam_first_free;
    }
}

/* A slot's sprite buffer offset for the OamByteSet calls. */
static uint16_t y_of(int s) { return (uint16_t)(s * 4); }

static void origin(uint8_t x, uint8_t y)
{
    g.sprite_origin[0] = x;
    g.sprite_origin[1] = y;
}

static void md_first(uint16_t id)
{
    *slot_at(S_71) = 0;
    MetaspriteDraw(id, 0);
}

/* The next group from the slot past the last: the table's count at `at`,
   plus the previous slot. */
static void md_next(uint32_t at, int prev, int here, uint16_t id)
{
    int v = cart16((uint8_t)(at >> 16), (uint16_t)at);

    if (prev >= 0)
        v += *slot_at(prev);
    *slot_at(here) = (uint16_t)v;
    MetaspriteDraw(id, v);
}

/*
 * @routine 02:F90B RecordsMenuBack
 * WHAT IT DOES: Leaves a records page for the course select: sound $2F, the
 * course screens' state 8, the large sprites hidden.
 */
static void records_menu_back(void)
{
    sound_port0_push(0x2F);
    CourseScreenState8();
    OamBufferHideLarge();
    g.byte_0e92 = 0xF0;
}

/*
 * @routine 02:F8F5 RecordsMenuGo
 * WHAT IT DOES: Takes a records page's first choice: sound $46, state $0A,
 * colour maths $A7 and the large sprites hidden.
 */
static void records_menu_go(void)
{
    sound_port0_push(0x46);
    g.course_screen_state = 0x0A;
    ColourMathA7Start();
    OamBufferHideLarge();
}

typedef void (*AttrFill)(int first);

/* The cursor at `choice`, attribute fills from entries 1, 5 and $0C (the
   last for three choices only), the cursor sprite moved. */
static void choice_show(uint8_t choice, AttrFill f1, AttrFill f5, AttrFill f12, uint8_t lines)
{
    g.records_cursor = choice;
    f1(1);
    f5(5);
    if (f12)
        f12(0x0C);
    OamSprite0AddY(lines);
}

static void records_view(void)
{
    g.course_screen_state = 0x0C;
    g.records_wait = 0;
    sound_port0_push(0x24);
}

/* The three-choice menus' highlights. */
static void all_top_show(void) { choice_show(0x00, OamAttr36x4, OamAttr38x7, OamAttr38x6, 0x00); }
static void all_middle_show(void) { choice_show(0x04, OamAttr38x4, OamAttr36x7, OamAttr38x6, 0x0F); }
static void all_bottom_show(void) { choice_show(0x02, OamAttr38x4, OamAttr38x7, OamAttr36x6, 0x1E); }
static void speed_top_show(void) { choice_show(0x00, OamAttr36x4, OamAttr38x6, NULL, 0x00); }
static void speed_bottom_show(void) { choice_show(0x02, OamAttr38x4, OamAttr36x6, NULL, 0x0F); }
static void alt_top_show(void) { choice_show(0x00, OamAttr36x4, OamAttr38x4, NULL, 0x00); }
static void alt_bottom_show(void) { choice_show(0x02, OamAttr38x4, OamAttr36x4, NULL, 0x0F); }

/* While the countdown runs, one step of it and nothing else (returns 0). */
static int menu_wait(void)
{
    if (!g.records_wait)
        return 1;
    g.records_wait--;
    return 0;
}

/*
 * @routine 02:F51F RecordsMenuAllTop .. 02:F7BB RecordsMenuTwoBottom
 * WHAT IT DOES: The menu under a records page: the cursor's handlers. B or
 * Start takes the choice, Up and Down move the cursor (sound $45), and the
 * current choice stays highlighted.
 */
static void menu_all(void)
{
    uint16_t p = pad();

    if (!menu_wait())
        return;
    if (p & 0x4000) {
        records_menu_back();
        return;
    }
    switch (g.records_cursor) {
    case 0:
        if (p & 0x9000) { records_menu_go(); return; }
        if (p & 0x0400) { sound_port0_push(0x45); all_middle_show(); return; }
        all_top_show();
        break;
    case 2:
        if (p & 0x9000) { records_menu_back(); return; }
        if (p & 0x0800) { sound_port0_push(0x45); all_middle_show(); return; }
        all_bottom_show();
        break;
    default:
        if (g.byte_0d24 != 3 && (p & 0x9000)) { records_view(); return; }
        if (p & 0x0800) { sound_port0_push(0x45); all_top_show(); return; }
        if (p & 0x0400) { sound_port0_push(0x45); all_bottom_show(); return; }
        all_middle_show();
        break;
    }
}

static void menu_two(void)
{
    uint16_t p = pad();

    if (!menu_wait())
        return;
    if (p & 0x4000) {
        records_menu_back();
        return;
    }
    if (g.records_cursor == 0) {
        if (p & 0x9000) { records_menu_go(); return; }
        if (p & 0x0400) { sound_port0_push(0x45); speed_bottom_show(); return; }
        speed_top_show();
    } else {
        if (p & 0x9000) { records_menu_back(); return; }
        if (p & 0x0800) { sound_port0_push(0x45); speed_top_show(); return; }
        speed_bottom_show();
    }
}

static void speed_alt_leave(void)
{
    g.time_override = 0;
    sound_port0_push(0x2F);
    g.course_screen_state = 0x0E;
    g.champ_stage++;
    OamBufferHideLarge();
    g.byte_0e92 = 0xF0;
}

static void menu_speed_alt(void)
{
    uint16_t p = pad();

    if (!menu_wait())
        return;
    if (g.records_cursor == 0) {
        if (p & 0x4000) { speed_alt_leave(); return; }
        if (p & 0x9000) { records_menu_go(); return; }
        if (p & 0x0400) { sound_port0_push(0x45); alt_bottom_show(); return; }
        alt_top_show();
    } else {
        if (p & 0x4000 || p & 0x9000) { speed_alt_leave(); return; }
        if (p & 0x0800) { sound_port0_push(0x45); alt_top_show(); return; }
        alt_bottom_show();
    }
}

static void menu_stunt_three(void)
{
    uint16_t p = pad();

    if (!menu_wait())
        return;
    if (p & 0x4000) {
        records_menu_back();
        return;
    }
    switch (g.records_cursor) {
    case 0:
        if (p & 0x9000) { records_menu_go(); return; }
        if (p & 0x0400) { sound_port0_push(0x45); all_middle_show(); return; }
        all_top_show();
        break;
    case 2:
        if (p & 0x9000) { records_menu_back(); return; }
        if (p & 0x0800) { sound_port0_push(0x45); all_middle_show(); return; }
        all_bottom_show();
        break;
    default:
        if (p & 0x9000) { records_view(); return; }
        if (p & 0x0800) { sound_port0_push(0x45); all_top_show(); return; }
        if (p & 0x0400) { sound_port0_push(0x45); all_bottom_show(); return; }
        all_middle_show();
        break;
    }
}

/* Sprites 0, 2 and 4 of the group from slot $0D73: tile $F0, flipped. */
static void page_mark_flip(void)
{
    uint16_t y = y_of(*slot_at(S_73));
    int i;

    for (i = 0; i < 3; i++) {
        OamByteSet((uint16_t)(y + i * 8 + 2), 0xF0);
        OamByteSet((uint16_t)(y + i * 8 + 3), (uint8_t)(OamByte((uint16_t)(y + i * 8 + 3)) | 0x40));
    }
}

/*
 * @routine 02:F0FA RecordsPageMarkFlipHide
 * @routine 02:F115 RecordsPageMarkFlip
 * WHAT IT DOES: Turns the page mark for the Speed Trax records' middle and
 * last pages.
 */
static void page_mark_flip_hide(void)
{
    uint16_t y = y_of(*slot_at(S_73));
    int i;

    for (i = 0; i < 3; i++)
        OamByteSet((uint16_t)(y + 5 + i * 8), 0xF0);
    page_mark_flip();
}

/* --- the rows of times ---------------------------------------------- */

/*
 * @routine 02:FDA8 RecordTimeDigitsLarge
 * @routine 02:FE04 RecordTimeDigitsSmall
 * @routine 02:FE38 RecordScoreDigits
 * WHAT IT DOES: A packed time or a Stunt Trax score as digit sprites from a
 * slot.
 */
static void large_digit(uint16_t at, uint8_t d)
{
    OamByteSet(at, (uint8_t)(d + 0x50));
    OamByteSet((uint16_t)(at + 4), (uint8_t)(d + 0x60));
}

static void time_digits_large(uint16_t time, int s)
{
    uint16_t min, sec, hund;
    uint16_t y = y_of(s);

    TimeUnpackBcd(time, &min, &sec, &hund);
    large_digit((uint16_t)(y + 0x12), min & 0x0F);
    large_digit((uint16_t)(y + 0x22), (uint8_t)(sec >> 4));
    large_digit((uint16_t)(y + 0x2A), sec & 0x0F);
    large_digit((uint16_t)(y + 0x3A), (uint8_t)(hund >> 4));
    large_digit((uint16_t)(y + 0x42), hund & 0x0F);
}

static void time_digits_small(uint16_t time, int s)
{
    uint16_t min, sec, hund;
    uint16_t y = y_of(s);

    TimeUnpackBcd(time, &min, &sec, &hund);
    OamByteSet((uint16_t)(y + 0x0E), min & 0x0F);
    OamByteSet((uint16_t)(y + 0x16), (uint8_t)(sec >> 4));
    OamByteSet((uint16_t)(y + 0x1A), sec & 0x0F);
    OamByteSet((uint16_t)(y + 0x22), (uint8_t)(hund >> 4));
    OamByteSet((uint16_t)(y + 0x26), hund & 0x0F);
}

static void score_digits(uint16_t score, int s)
{
    static const uint16_t perfect[7] = {0x3619, 0x360E, 0x361B, 0x360F, 0x360E, 0x360C, 0x361D};
    uint16_t y = y_of(s);
    int i;

    OamByteSet((uint16_t)(y + 0x2E), score & 0x0F);
    if (score >= 0x4000) {
        OamByteSet((uint16_t)(y + 0x2A), (uint8_t)((score >> 4) & 0x0F));
        for (i = 0; i < 7; i++) {
            OamByteSet((uint16_t)(y + 0x0E + i * 4), (uint8_t)perfect[i]);
            OamByteSet((uint16_t)(y + 0x0F + i * 4), (uint8_t)(perfect[i] >> 8));
        }
        return;
    }
    OamByteSet((uint16_t)(y + 0x2A), (uint8_t)((score >> 4) & 0x0F));
    OamByteSet((uint16_t)(y + 0x16), (uint8_t)((score >> 8) & 0x0F));
    OamByteSet((uint16_t)(y + 0x12), (uint8_t)(score >> 12));
    OamByteSet((uint16_t)(y + 0x0D), 0xF0);
    OamByteSet((uint16_t)(y + 0x25), 0xF0);
}

/*
 * @routine 02:FF01 RecordNameLetters
 * @routine 02:FF37 RecordNameLettersSmall
 * @routine 02:FEBF RecordNameLettersLarge
 * @routine 02:FEA7 RecordLetterTile
 * WHAT IT DOES: The three 5-bit letters of a name into tile bytes from a
 * slot: the small sprites' tiles, or large letters of two tiles each.
 */
static void name_letters(uint16_t name, int s, uint16_t first)
{
    uint16_t y = y_of(s);
    int i;

    for (i = 0; i < 3; i++)
        OamByteSet((uint16_t)(y + first + i * 4), (uint8_t)(((name >> (i * 5)) & 0x1F) + 0x0A));
}

static uint16_t letter_tile(uint16_t letter)
{
    uint16_t v = (uint16_t)(letter + 0x0A);

    return (uint16_t)(((v & 0x0F) | ((v & 0x30) << 1)) + 0x50);
}

static void name_letters_large(uint16_t name, int s)
{
    uint16_t y = y_of(s);
    static const uint16_t at[3] = {0x4A, 0x52, 0x5A};
    int i;

    for (i = 0; i < 3; i++) {
        uint16_t t = letter_tile((name >> (i * 5)) & 0x1F);

        OamByteSet((uint16_t)(y + at[i]), (uint8_t)t);
        OamByteSet((uint16_t)(y + at[i] + 4), (uint8_t)(t + 0x10));
    }
}

/* ---- each records page's rows ---------------------------------------- */

/* The tag's car piece set: 0, 3, 6 or 9 for each block of $0C. */
static uint16_t tag_car_set(uint8_t tag)
{
    return tag >= 0x24 ? 9 : tag >= 0x18 ? 6 : tag >= 0x0C ? 3 : 0;
}

/* One of the three special records rows. */
static void special_row(int row_slot, int k, int stunt, void (*place)(uint16_t set, int slot))
{
    uint16_t v = g.record_times[k];

    if (stunt ? v != 0 : v != 0xFFFF) {
        if (stunt)
            score_digits(v, row_slot);
        else
            time_digits_small(v, row_slot);
        if (stunt)
            name_letters(g.record_names[k], row_slot, 0x3E);
        else
            name_letters(g.record_names[k], row_slot, 0x2A);
        place(tag_car_set((uint8_t)g.record_tags[k]), row_slot);
        return;
    }
    OamHideEntries0To2(row_slot);
    if (stunt)
        OamHideEntries3And9(row_slot);
}

/* The course's best time, name and car from the save's records, five bytes
   a course; with no time the row's first sprites are hidden. */
static void best_time(const SaveBest *table, int first, int d_time, int d_rest)
{
    const SaveBest *r = &table[g.course - first];
    int s_time = *slot_at(S_75) + d_time, s_rest = *slot_at(S_75) + d_rest;

    if (r->time != 0xFFFF) {
        time_digits_large(r->time, s_time);
        name_letters_large(r->name, s_time);
        SpritePiecesCarAt51(r->car, s_rest);
        return;
    }
    OamByteSet((uint16_t)(y_of(s_rest) + 1), 0x0F);
    OamByteSet((uint16_t)(y_of(s_rest) + 5), 0x0F);
    OamByteSet((uint16_t)(y_of(s_rest) + 9), 0x0F);
}

/*
 * @routine 02:EEF2 SpeedRecordsPage
 * WHAT IT DOES: The Speed Trax records pages: Right and Left step the page
 * 0-4 (sound $45); page 0 the best three times, pages 1-4 the first times of
 * each group of the course's laps with the page mark turned; then the page's
 * title with the course byte set for it and put back.
 */
static void speed_records_page(void)
{
    uint16_t p = pad();
    uint8_t old, v;

    if (p & 0x0100) {
        if (g.times_step < 4) {
            sound_port0_push(0x45);
            g.times_step++;
        }
    } else if (p & 0x0200) {
        if (g.times_step) {
            sound_port0_push(0x45);
            g.times_step--;
        }
    }
    if (g.times_step >= 5)
        g.times_step = 0;
    switch (g.times_step) {
    case 0: record_rows_first_times(0x24); break;
    case 4: page_mark_flip_hide(); record_rows_group_firsts(0x09); break;
    default:
        page_mark_flip();
        record_rows_group_firsts((uint8_t)((g.times_step - 1) * 3));
        break;
    }
    old = g.course;
    v = g.times_step ? (uint8_t)(g.course_class * 4 + g.times_step - 1) : 0x1C;
    g.course = v;
    SpriteTextAt895E();
    g.course = old;
}

/*
 * @routine 02:EDD2 SpeedRecordsScreen
 * WHAT IT DOES: The Speed Trax course select's records page.
 */
void SpeedRecordsScreen(void)
{
    if (g.time_override) {
        origin(0x89, 0x89);
        md_first(0x001A);
        md_next(0x04EE31 + 4, -1, S_73, 0x0019);
        md_next(0x04EE31, S_73, S_75, 0x0000);
        g.oam_first_free = g.sprite_row_d75;
        OamClassName();
        SpriteTextAt895E();
        OamPaletteForD75();
        menu_speed_alt();
        return;
    }
    origin(0x89, 0x89);
    md_first(g.champ_stage ? 0x001B : 0x0005);
    md_next(0x04EDDD + 4, -1, S_73, 0x0001);
    md_next(0x04EDCB + 6, S_73, S_75, 0x0002);
    md_next(0x04EDCB + 10, S_75, S_77, 0x0003);
    md_next(0x04EDCB + 14, S_77, S_79, 0x0004);
    md_next(0x04EDDD, S_79, S_7B, 0x0000);
    OamClassName();
    speed_records_page();
    OamPaletteFromTableE26A();
    if (!g.champ_stage) {
        menu_two();
        return;
    }
    g.byte_0e92 = 0xF0;
    if (pad() & 0x9000) {
        sound_port0_push(0x46);
        g.course_screen_state = 0x0A;
        ColourMathA7Start();
    }
}

/*
 * @routine 02:F148 StuntRecordsScreen
 * WHAT IT DOES: The Stunt Trax course select's records page.
 */
void StuntRecordsScreen(void)
{
    if (g.hud_blank) {
        uint16_t p = pad();

        origin(0x81, 0x89);
        if (p & 0x0100) {
            sound_port0_push(0x45);
            g.course = g.course >= 0x1A ? 0x17 : (uint8_t)(g.course + 1);
        } else if (p & 0x0200) {
            sound_port0_push(0x45);
            g.course = g.course <= 0x17 ? 0x1A : (uint8_t)(g.course - 1);
        }
        md_first(0x0015);
        md_next(0x04EE21, -1, S_73, 0x0013);
        md_next(0x04EE0D + 0xC, S_73, S_75, 0x0014);
        best_time(save.stunt, 0x0017, 0, 1);
        SpriteTextAt8152();
        menu_two();
        return;
    }
    origin(0x81, 0x89);
    md_first(0x0012);
    md_next(0x04EDED + 8, -1, S_73, 0x0007);
    md_next(0x04EDDD + 0xC, S_73, S_75, 0x0008);
    md_next(0x04EDED, S_75, S_77, 0x0009);
    md_next(0x04EDED + 4, S_77, S_79, 0x0006);
    SpriteTextAt8152();
    special_row(*slot_at(S_77), 2, 1, SpritePiecesAt46Y93);
    special_row(*slot_at(S_75), 1, 1, SpritePiecesAt46Y82);
    special_row(*slot_at(S_73), 0, 1, SpritePiecesAt46Y71);
    OamPaletteFromTableE30F();
    menu_stunt_three();
}

/*
 * @routine 02:F30A BattleRecordsScreen
 * WHAT IT DOES: The Battle Trax course select's records page.
 */
void BattleRecordsScreen(void)
{
    origin(0x81, 0x89);
    md_first(0x000D);
    md_next(0x04EDFD + 4, -1, S_73, 0x000C);
    md_next(0x04EDFD, S_73, S_75, 0x000B);
    SpritePiecesAtLeft((uint16_t)((g.car[0] * 3) & 0xFF), *slot_at(S_73));
    SpritePiecesAtRight((uint16_t)((g.car[1] * 3) & 0xFF), *slot_at(S_73) + 3);
    SpriteTextAt8152();
    OamPalettesE28D();
    menu_two();
}

/*
 * @routine 02:F37A AllRecordsScreen
 * WHAT IT DOES: The all-courses select's records page.
 */
void AllRecordsScreen(void)
{
    if (g.time_override) {
        origin(0x89, 0x89);
        md_first(0x0018);
        md_next(0x04EE21 + 0xC, -1, S_73, 0x0016);
        md_next(0x04EE21 + 4, S_73, S_75, 0x0017);
        best_time(save.special, 0x0014, -1, 0);
        SpriteTextAt8952();
        menu_two();
        return;
    }
    origin(0x89, 0x89);
    md_first(0x0012);
    md_next(0x04EDFD + 8, -1, S_73, 0x000F);
    md_next(0x04EDFD + 0xC, S_73, S_75, 0x0010);
    md_next(0x04EE0D, S_75, S_77, 0x0011);
    md_next(0x04EE0D + 4, S_77, S_79, 0x000E);
    SpriteTextAt8952();
    OamPaletteFromTableE26AByMode();
    special_row(*slot_at(S_77), 2, 0, SpritePiecesAt62Y95);
    special_row(*slot_at(S_75), 1, 0, SpritePiecesAt62Y83);
    special_row(*slot_at(S_73), 0, 0, SpritePiecesAt62Y71);
    menu_all();
}

/* ---- the time rows of the Speed Trax pages --------------------------- */

/*
 * @routine 02:F077 RecordRowDraw
 * WHAT IT DOES: One records row from sprite slot `s`: the time at the page's
 * offset as six digit sprites, the car, the three letters of the name; a
 * time with no seconds hides the row's first three sprites.
 */
static void record_row_draw(int s, uint8_t name[3], uint8_t page_offset, uint8_t time_offset, uint8_t row_y)
{
    const uint8_t *t = &g.times_bcd[0][(uint8_t)(page_offset + time_offset)];
    uint16_t y = y_of(s);

    if (t[1] == 0xFF) {
        OamByteSet((uint16_t)(y + 1), 0xF0);
        OamByteSet((uint16_t)(y + 5), 0xF0);
        OamByteSet((uint16_t)(y + 9), 0xF0);
        return;
    }
    OamByteSet((uint16_t)(y + 0x26), (uint8_t)(t[0] >> 4));
    OamByteSet((uint16_t)(y + 0x2A), t[0] & 0x0F);
    OamByteSet((uint16_t)(y + 0x1A), (uint8_t)(t[1] >> 4));
    OamByteSet((uint16_t)(y + 0x1E), t[1] & 0x0F);
    OamByteSet((uint16_t)(y + 0x0E), (uint8_t)(t[2] >> 4));
    OamByteSet((uint16_t)(y + 0x12), t[2] & 0x0F);
    SpritePieces3(0x57, row_y, (uint16_t)((name[2] * 3) & 0x0F), 0, s);
    name_letters((uint16_t)(name[0] | name[1] << 8), s + 1, 0x2A);
}

static void record_row(int n, uint8_t page_offset, uint8_t time_offset)
{
    uint8_t name[3];

    name[0] = g.names_bytes[n * 3];
    name[1] = g.names_bytes[n * 3 + 1];
    name[2] = g.names_bytes[n * 3 + 2];
    record_row_draw(n == 0 ? *slot_at(S_75) : n == 1 ? *slot_at(S_77) : *slot_at(S_79), name, page_offset, time_offset, (uint8_t)(0x7C + n * 0x11));
}

/*
 * @routine 02:EFEC RecordRowsFirstTimes
 * WHAT IT DOES: The three rows with times 0, 1 and 2.
 * @routine 02:EFE2 RecordRowsGroupFirsts
 * WHAT IT DOES: The three rows with the first time of each group.
 */
void record_rows_first_times(uint8_t page_offset)
{
    int n;

    for (n = 0; n < 3; n++)
        record_row(n, page_offset, (uint8_t)(n * 3));
}

void record_rows_group_firsts(uint8_t page_offset)
{
    record_row(0, page_offset, 0x00);
    record_row(1, page_offset, 0x0C);
    record_row(2, page_offset, 0x18);
}
