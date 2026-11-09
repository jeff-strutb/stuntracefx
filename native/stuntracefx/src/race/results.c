/*
 * results.c: the screens after a race, a step an update while the overlay
 * fades (ResultsScreenStep, by HUD mode and the step $0D59): the panels the
 * Super FX unpacks into the map buffer, the laps' times written in, the time
 * bonus counted, the special courses' records with the cars as sprites, and
 * the menu after the race (retry, course, car). Also the championship's
 * screen between its races (ChampionshipScreenStep) and its finish.
 *
 * Not kept: $10A3, which ResultsPicture4000 clears and nothing reads.
 */
#include <stdint.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "data/unpack.h"
#include "hud/hud.h"
#include "menu/menu.h"
#include "race/race.h"
#include "save/save.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"
#include "video/video.h"

/* ---- the map buffer ($7F:93D7) by bytes ---- */

static uint8_t map_byte(uint16_t offset)
{
    uint16_t w = race_map_buffer()[offset >> 1];

    return (uint8_t)((offset & 1) ? w >> 8 : w);
}

static void map_byte_set(uint16_t offset, uint8_t v)
{
    RecordsPanelTile(offset, v);
}

/* A digit's tile from a nibble. */
static uint8_t digit_high(uint8_t v) { return (uint8_t)((v >> 4) + 0xC0); }
static uint8_t digit_low(uint8_t v) { return (uint8_t)((v & 0x0F) + 0xC0); }

static int pressed(uint16_t mask) { return (frame_state.pressed_copy[0] & mask) != 0; }

static void step_next(void) { g.overlay_d59++; }

/* $0EDE-$0EE9: the three laps' splits, then the race's time as kept. */
static uint8_t split_byte(unsigned i)
{
    return i < 9 ? g.lap_splits[i] : g.race_time_kept[i - 9];
}

/* A Super FX picture (program $01:CF42, the unpacker) to the chip's RAM at `at`. */
static void picture_unpack(uint8_t bank, uint16_t end, uint16_t at)
{
    unpack((uint32_t)bank << 16 | end, fx_ram + at, 0xC000);
}

/* A panel: picture `end` in bank `bank` unpacked to $70:3000, its $440 bytes
   copied into the map buffer, then the name's digits. */
static void panel(uint8_t bank, uint16_t end)
{
    unsigned i;

    picture_unpack(bank, end, 0x3000);
    for (i = 0; i < 0x440; i++)
        map_byte_set((uint16_t)i, fx_ram[0x3000 + i]);
    DigitTiles1184();
}

/* ---- the panels' contents ---- */

/* @routine 07:C93A ScoreTiles: score `score` (BCD) at `x`; from $4000 the word spelled instead. */
static void score_tiles(uint16_t x, uint16_t score)
{
    static const uint8_t word[7] = {0xD9, 0xCE, 0xDB, 0xCF, 0xCE, 0xCC, 0xDD};
    uint8_t lo = (uint8_t)score, hi = (uint8_t)(score >> 8);
    unsigned i;

    map_byte_set((uint16_t)(x + 16), digit_low(lo));
    map_byte_set((uint16_t)(x + 14), digit_high(lo));
    if (hi < 0x40) {
        map_byte_set((uint16_t)(x + 2), digit_low(hi));
        map_byte_set(x, digit_high(hi));
        map_byte_set((uint16_t)(x + 6), 0xC4);
        map_byte_set((uint16_t)(x + 8), 0xC0);
        return;
    }
    for (i = 0; i < 7; i++)
        map_byte_set((uint16_t)(x - 2 + 2 * i), word[i]);
    for (i = 0; i < 7; i++)
        map_byte_set((uint16_t)(x - 1 + 2 * i), 0x24);
}

/* @routine 07:C9CE TimeTiles, 07:C933 KeepTimeDigits: a packed time at `x`. */
static void time_tiles(uint16_t x, uint16_t packed)
{
    uint16_t min, sec, hund;

    TimeUnpackBcd(packed, &min, &sec, &hund);
    map_byte_set(x, (uint8_t)((uint8_t)min + 0xC0));
    map_byte_set((uint16_t)(x + 4), digit_high((uint8_t)sec));
    map_byte_set((uint16_t)(x + 6), digit_low((uint8_t)sec));
    map_byte_set((uint16_t)(x + 10), digit_high((uint8_t)hund));
    map_byte_set((uint16_t)(x + 12), digit_low((uint8_t)hund));
}

/* @routine 07:CA0E NameTiles16, 07:CA41 NameTiles26: the three letters as tiles $CA + the letter. */
static void name_tiles(uint16_t x, uint16_t name)
{
    map_byte_set(x, (uint8_t)((name & 0x1F) + 0xCA));
    map_byte_set((uint16_t)(x + 2), (uint8_t)(((name >> 5) & 0x1F) + 0xCA));
    map_byte_set((uint16_t)(x + 4), (uint8_t)(((name >> 10) & 0x1F) + 0xCA));
}

/* @routine 07:C911 PlaceFrameFromValue: the car sprite's frame for a record's tag. */
static uint8_t place_frame(uint8_t tag)
{
    g.piece_set = (uint16_t)((g.piece_set & 0xFF00) | (tag >= 0x24 ? 9 : tag >= 0x18 ? 6 : tag >= 0x0C ? 3 : 0));
    return (uint8_t)g.piece_set;
}

/* The cars of the three records with a time or score, at Y $54, $6C, $84,
   from sprite 76 on, each sprite's size byte $60. */
static void record_cars(uint8_t x, uint16_t empty)
{
    static const uint8_t ys[3] = {0x54, 0x6C, 0x84};
    unsigned n;

    for (n = 0; n < 3; n++) {
        if (g.record_times[n] == empty)
            continue;
        place_frame((uint8_t)g.record_tags[n]);
        g.piece_tile = 0xF4;
        SpritePieces3(x, ys[n], g.piece_set, 0x3E, (int)(0x4C + 4 * n));
        OamByteSet((uint16_t)(0x213 + n), 0x60);
    }
}

/* The records' wait: B or START on to step `next`; else the player's place
   row cycles and the cars are drawn. */
static void records_wait(uint8_t next, const uint16_t *rows, int count, uint8_t x, uint16_t empty)
{
    if (pressed(0x9000)) {
        SoundPort0QueuePush(0x46);
        g.overlay_d59 = next;
        OamByteSet(0x213, 0x55);
        OamByteSet(0x214, 0x55);
        OamByteSet(0x215, 0x55);
        g.overlay_state = 0x05;
        return;
    }
    if (g.record_place)
        TilemapPaletteCycle(g.record_place == 2 ? rows[1] : g.record_place > 2 ? rows[2] : rows[0], count);
    if ((g.frame_count & 0x07) == 0x07)
        g.piece_selector = (uint8_t)(g.piece_selector + 1 >= 4 ? 0 : g.piece_selector + 1);
    g.piece_palette = 0x3E;
    g.piece_x = x;
    g.piece_tile = 0xF4;
    record_cars(x, empty);
}

/* @routine 07:C731 StuntRecordsWait */
static void StuntRecordsWait(void)
{
    static const uint16_t rows[3] = {0x018F, 0x024F, 0x09AF};

    records_wait(0x0A, rows, 7, 0x40, 0x0000);
}

/* @routine 07:C811 TimedRecordsWait */
static void TimedRecordsWait(void)
{
    static const uint16_t rows[3] = {0x0195, 0x0255, 0x0315};

    records_wait(0x0D, rows, 11, 0x58, 0xFFFF);
}

/* @routine 07:C8FE ResultsNext */
static void ResultsNext(void)
{
    g.palette_animate = 0;
    g.split_stage_on = 0x06;
    g.split_stage = 0x00;
    step_next();
}

/* @routine 07:C8FA ResultsUnpackNext */
static void ResultsUnpackNext(void)
{
    RecordsUnpackCourseRunning();
    ResultsNext();
}

/* @routine 07:CA74 StuntRecordsDraw */
static void StuntRecordsDraw(void)
{
    unsigned n;

    panel(0x1F, 0xFB21);
    for (n = 0; n < 3 && g.record_times[n]; n++) {
        score_tiles((uint16_t)(0x0190 + 0xC0 * n), g.record_times[n]);
        name_tiles((uint16_t)(0x0190 + 0xC0 * n + 26), g.record_names[n]);
    }
    step_next();
    g.overlay_state = 0x02;
}

/* @routine 07:CAFC TimedRecordsDraw */
static void TimedRecordsDraw(void)
{
    unsigned n;

    panel(0x1C, 0xF3F2);
    for (n = 0; n < 3 && g.record_times[n] != 0xFFFF; n++) {
        time_tiles((uint16_t)(0x0194 + 0xC0 * n), g.record_times[n]);
        name_tiles((uint16_t)(0x0194 + 0xC0 * n + 16), g.record_names[n]);
    }
    step_next();
    g.overlay_state = 0x02;
}

/* @routine 07:CAD5 ResultsPanelFAB5 */
static void ResultsPanelFAB5(void) { panel(0x1F, 0xFAB5); step_next(); }
/* @routine 07:CB7C ResultsPanelF38E */
static void ResultsPanelF38E(void) { panel(0x1C, 0xF38E); step_next(); }

/* B or START on the records: to `skip` for a placed player, else the next step. */
static void records_choice(uint8_t skip, int check_skipped)
{
    if (!pressed(0x9000))
        return;
    if ((check_skipped && g.race_skipped) || !g.record_place)
        step_next();
    else
        g.overlay_d59 = skip;
    SoundPort0QueuePush(0x46);
}

/* @routine 07:CBA3 StuntRecordsChoice */
static void StuntRecordsChoice(void)
{
    if (g.stunt_score_high == 0x40)
        TilemapPaletteCycle(0x0199, 7);
    records_choice(0x0C, 1);
}

/* @routine 07:CBE8 TimedRecordsChoice */
static void TimedRecordsChoice(void) { records_choice(0x0F, 0); }

/* @routine 07:CC11 ResultsPanelFB99 */
static void ResultsPanelFB99(void)
{
    g.split_stage = 0;
    g.race_again = 0;
    palettes.tile_cycle = 0;
    panel(0x1F, 0xFB99);
    step_next();
    g.split_stage = 0x02;
}

/* @routine 07:D3F6 ResultsTotalTiles */
static void ResultsTotalTiles(void);
/* @routine 07:D4F8 RecordTimeTiles */
static void RecordTimeTiles(void);

/* @routine 07:CC4A ResultsPanelFA39 */
static void ResultsPanelFA39(void)
{
    panel(0x1F, 0xFA39);
    if (g.race_time[2])
        ResultsTotalTiles();
    if (g.times_bcd[3][2] != 0xFF)
        RecordTimeTiles();
    step_next();
}

/* @routine 07:CC83 ResultsPicture4000: picture $18:C770 to $70:4000. */
static void ResultsPicture4000(void)
{
    picture_unpack(0x18, 0xC770, 0x4000);
    step_next();
    g.overlay_state = 0;
}

/* $10A0 = the car's picture word from the table at $07:CCB7. */
static void car_picture(void)
{
    g.overlay_picture = cart16(0x07, (uint16_t)(0xCCB7 + g.car[0] * 2));
}

/* @routine 07:CC9B ResultsCarPicture */
static void ResultsCarPicture(void)
{
    g.split_stage = 0x12;
    step_next();
    car_picture();
}

/* ---- the menu after the race ---- */

static void menu_rows(unsigned at, int count, void (*a)(void), void (*c)(void))
{
    TilemapPaletteCycle(at, count);
    a();
    c();
}

static void rows_retry(void) { menu_rows(0x0187, 9, TilemapRow207Palette4, TilemapRow287Palette4); }
static void rows_course(void) { menu_rows(0x0207, 13, TilemapRow187Palette4, TilemapRow287Palette4); }
static void rows_car(void) { menu_rows(0x0287, 10, TilemapRow187Palette4, TilemapRow207Palette4); }

/* Car 3 only in mode 3 or once the save's progress has bit 2. */
static int car3_open(void) { return g.hud_mode == 3 || (save.progress & 0x04); }

static void car_change(int right)
{
    uint8_t c;

    SoundPort0QueuePush(0x45);
    if (right) {
        c = (uint8_t)(g.car[0] + 1);
        if (c == 0x03)
            c = car3_open() ? 0x03 : 0x00;
        else if (c > 0x03)
            c = 0x00;
    } else {
        c = (uint8_t)(g.car[0] - 1);
        if (c & 0x80)
            c = car3_open() ? 0x03 : 0x02;
    }
    g.car[0] = c;
    g.choice_p1 = (uint16_t)((g.choice_p1 & 0xFF00) | cart8(0x07, (uint16_t)(0xCE93 + c)));
    SelectionSave();
    g.split_stage = 0x12;
    car_picture();
}

/*
 * @routine 07:CCBF ResultsMenu
 * WHAT IT DOES: The menu after the race until its fade out starts: retry,
 * course or car, moved by up and down; B or START on retry or course starts
 * the fade out, on car the car's choice, where the cars blink and right and
 * left change it, and B or START goes back.
 */
static void ResultsMenu(void)
{
    uint8_t c = g.race_again;

    if (g.fade_out)
        return;
    if (pressed(0x9000)) {
        if (c < 0x02) {
            g.fade_out = 0x01;
            SoundPort0QueuePush(0x46);
            return;
        }
        if (c == 0x02) {
            g.race_again++;
            SoundPort0QueuePush(0x46);
            rows_car();
            return;
        }
        g.race_again = 0;
        SoundPort0QueuePush(0x46);
        OamByteSet(0x20D, 0x55);
        rows_retry();
        return;
    }
    g.split_stage = 0x02;
    switch (c) {
    case 0x00:
        if (pressed(0x0400) && !g.race_skipped) {
            g.race_again++;
            SoundPort0QueuePush(0x45);
            rows_course();
            return;
        }
        rows_retry();
        return;
    case 0x01:
        if (pressed(0x0800)) {
            g.race_again = 0;
            SoundPort0QueuePush(0x45);
            rows_retry();
            return;
        }
        if (pressed(0x0400)) {
            g.race_again++;
            SoundPort0QueuePush(0x45);
            rows_car();
            return;
        }
        rows_course();
        return;
    case 0x02:
        if (!pressed(0x0800)) {
            rows_car();
            return;
        }
        SoundPort0QueuePush(0x45);
        g.race_again = 0x01;
        rows_course();
        return;
    default:
        break;
    }
    /* the car's choice: sprites 52 and 53 blink by bit 3 of the frame count */
    if (g.frame_count & 0x08) {
        uint8_t y = (uint8_t)(g.overlay_y + 0x18);

        OamByteSet(0xD0, (uint8_t)(g.overlay_x - 0x08));
        OamByteSet(0xD4, (uint8_t)(g.overlay_x + 0x40));
        OamByteSet(0xD1, y);
        OamByteSet(0xD5, y);
        OamByteSet(0xD2, 0x19);
        OamByteSet(0xD6, 0x19);
        OamByteSet(0xD3, 0x76);
        OamByteSet(0xD7, 0x36);
        OamByteSet(0x20D, 0x50);
    } else {
        OamByteSet(0x20D, 0x55);
    }
    if (pressed(0x0100))
        car_change(1);
    else if (pressed(0x0200))
        car_change(0);
    rows_car();
}

/* ---- the championship's screen's palette and the totals ---- */

static void champion_palette(void)
{
    int i;

    for (i = 0; i < 256; i++)
        palettes.buffer[i] = cart16(0x1F, (uint16_t)(0x8600 + 2 * i));
}

/* BG2 at (-24, -32) and its map at $5C00 (BG2SC $5C). */
static void results_bg2(void)
{
    g.backdrop_y[0] = 0xFFE0;
    g.backdrop_x[0] = 0xFFE8;
}

/* @routine 07:CF32 ResultsChampionScreen */
static void ResultsChampionScreen(void)
{
    champion_palette();
    results_bg2();
    g.results_page_shown = 1;
    step_next();
}

/* @routine 07:CF5B ResultsWaitWindow */
static void ResultsWaitWindow(void)
{
    if (!windows.bands_state) {
        step_next();
        SoundPort0QueuePushRaw(0x28);
    }
    g.split_stage = 0x06;
}

/* The results' text line: $0D45's low nibble 2 and line $0E3D as sprites from 96 on. */
static void total_line(void)
{
    g.hdma_enable = (uint8_t)((g.hdma_enable & 0xF0) | 0x02);
    OamTextLine(0x60, 0xC0, 0x67, 0x2F, 0x38);
}

static void total_line_end(uint16_t at)
{
    g.split_stage = 0x04;
    step_next();
    g.results_at = at;
}

/* @routine 07:CF6F ResultsTotalLine */
static void ResultsTotalLine(void)
{
    total_line();
    total_line_end(0x0190);
}

/* @routine 07:D576 OamWordRow5: the five letters at $07:D5AD as sprites 90-94. */
static void OamWordRow5(void)
{
    unsigned i;

    for (i = 0; i < 5; i++) {
        uint16_t at = (uint16_t)(0x16C + 4 * i);

        OamByteSet(at, (uint8_t)(0xA5 + 8 * i));
        OamByteSet((uint16_t)(at + 1), 0x4E);
        OamByteSet((uint16_t)(at + 2), (uint8_t)(cart8(0x07, (uint16_t)(0xD5AD + i)) + 0xC0));
        OamByteSet((uint16_t)(at + 3), 0x38);
    }
}

/* @routine 07:CFE8 ResultsTotalLineWords */
static void ResultsTotalLineWords(void)
{
    total_line();
    OamWordRow5();
    total_line_end(0x018E);
    if (g.race_time_best[2])
        ResultsTotalTiles();
}

/* ---- counting the results up ---- */

/* The decimal add and subtract of the processor's decimal mode. */
static uint8_t dec_add(uint8_t a, uint8_t b, int *carry)
{
    int lo = (a & 0x0F) + (b & 0x0F) + *carry, sum;

    if (lo > 0x09)
        lo += 0x06;
    sum = (a & 0xF0) + (b & 0xF0) + (lo > 0x0F ? 0x10 : 0) + (lo & 0x0F);
    if (sum > 0x9F)
        sum += 0x60;
    *carry = sum > 0xFF;
    return (uint8_t)sum;
}

/* carry is the 65816's: 1 no borrow */
static uint8_t dec_sub(uint8_t a, uint8_t b, int *carry)
{
    int lo = (a & 0x0F) - (b & 0x0F) - !*carry, r;

    if (lo < 0)
        lo = ((lo - 0x06) & 0x0F) - 0x10;
    r = (a & 0xF0) - (b & 0xF0) + lo;
    if (r < 0)
        r -= 0x60;
    *carry = r >= 0;
    return (uint8_t)r;
}

/* A count one more (decimal) shown as two digits in the buffer. */
static void count_digits(uint8_t *count, uint16_t hi, uint16_t lo)
{
    int c = 0;

    *count = dec_add(*count, 0x01, &c);
    map_byte_set(lo, digit_low(*count));
    map_byte_set(hi, digit_high(*count));
}

/* @routine 07:D055 StuntResultsCount */
static void StuntResultsCount(void)
{
    static const uint8_t word[7] = {0xD9, 0xCE, 0xDB, 0xCF, 0xCE, 0xCC, 0xDD};
    unsigned i;

    g.split_stage = 0x02;
    if (g.stunt_score_high == 0x40) {
        for (i = 0; i < 7; i++)
            map_byte_set((uint16_t)(0x0198 + 2 * i), word[i]);
        TilemapPaletteCycle(0x0199, 7);
    } else if (g.stunt_score_high != g.byte_e80) {
        count_digits(&g.byte_e80, 0x0198, 0x019A);
        return;
    }
    if ((uint8_t)g.time_left != g.byte_e81) {
        count_digits(&g.byte_e81, 0x0218, 0x021A);
        return;
    }
    SoundPort0QueuePushRaw(0x3F);
    step_next();
}

/*
 * One character of a lap's split at the digit position $0E43, by the
 * character $0E3F (0-12, even): the minutes' digit, ':', the seconds' two,
 * '.', the hundredths' two; the last moves the position to the next line ($74
 * on, $F4 after the fourth's, with sound $3F) and the step on.
 */
static void lap_char(unsigned y)
{
    uint16_t at = g.results_at;
    uint8_t v;

    switch (g.banner_timer[0] | g.banner_timer[1] << 8) {
    case 0x00: v = (uint8_t)(split_byte(y + 2) + 0xC0); break;
    case 0x02: v = 0xE6; break;
    case 0x04: v = digit_low((uint8_t)(split_byte(y + 1) >> 4)); break;
    case 0x06: v = digit_low(split_byte(y + 1)); break;
    case 0x08: v = 0xE7; break;
    case 0x0A: v = digit_high(split_byte(y)); break;
    case 0x0C:
        map_byte_set(at, digit_low(split_byte(y)));
        step_next();
        if (g.overlay_d59 == 0x0B) {
            SoundPort0QueuePushRaw(0x3F);
            g.results_at = (uint16_t)(at + 0x00F4);
        } else {
            g.results_at = (uint16_t)(at + 0x0074);
        }
        g.banner_timer[0] = g.banner_timer[1] = 0;
        return;
    default:
        return;                         /* past the table's entries: not reached */
    }
    map_byte_set(at, v);
    g.banner_timer[0] = (uint8_t)(g.banner_timer[0] + 2);
    g.results_at = (uint16_t)((g.results_at & 0xFF00) | (uint8_t)(g.results_at + 2));
}

/* @routine 07:D0F5 LapCharsLap1 */
static void LapCharsLap1(void)
{
    g.split_stage = 0x02;
    lap_char(0);
}

/* @routine 07:D107 LapCharsLap2 */
static void LapCharsLap2(void) { lap_char(3); }
/* @routine 07:D114 LapCharsLap3 */
static void LapCharsLap3(void) { lap_char(6); }
/* @routine 07:D121 LapCharsTotal */
static void LapCharsTotal(void) { lap_char(9); }

/* @routine 07:D12E RaceTimeTiles: the race's time shown and kept for the car. */
static void RaceTimeTiles(void)
{
    uint8_t *kept = g.best_times[g.champ_stage];

    map_byte_set(0x038E, digit_low(g.race_time_kept[2]));
    map_byte_set(0x0392, digit_high(g.race_time_kept[1]));
    map_byte_set(0x0394, digit_low(g.race_time_kept[1]));
    map_byte_set(0x0398, digit_high(g.race_time_kept[0]));
    map_byte_set(0x039A, digit_low(g.race_time_kept[0]));
    kept[0] = g.race_time_kept[0];
    kept[1] = g.race_time_kept[1];
    kept[2] = g.race_time_kept[2];
    step_next();
}

/* @routine 07:D232 ResultsBlinkWait */
static void ResultsBlinkWait(void)
{
    if (++g.banner_timer[0] < 0x3C) {
        TilemapPaletteCycle(0x038F, 7);
        return;
    }
    g.banner_timer[0] = 0;
    step_next();
    SoundPort0QueuePushRaw(0x28);
    TilemapPaletteFill(0x038F, 7, 0x00);
}

/* A time's minutes digit: blank (tile $0F, bit 0 of its attribute set) for 0. */
static void minutes_tile(uint8_t mins, uint16_t at)
{
    uint8_t t = digit_high(mins);

    if (t == 0xC0) {
        map_byte_set(at, 0x0F);
        map_byte_set((uint16_t)(at + 1), (uint8_t)(map_byte((uint16_t)(at + 1)) | 0x01));
    } else {
        map_byte_set(at, t);
        map_byte_set((uint16_t)(at + 1), (uint8_t)(map_byte((uint16_t)(at + 1)) & 0xE2));
    }
}

static void ResultsTotalTiles(void)
{
    minutes_tile(g.race_time[2], 0x0220);
    map_byte_set(0x0222, digit_low(g.race_time[2]));
    map_byte_set(0x0224, 0xE6);
    map_byte_set(0x0226, digit_high(g.race_time[1]));
    map_byte_set(0x0228, digit_low(g.race_time[1]));
    map_byte_set(0x022A, 0xE7);
    map_byte_set(0x022C, digit_high(g.race_time[0]));
    map_byte_set(0x022E, digit_low(g.race_time[0]));
}

static void RecordTimeTiles(void)
{
    const uint8_t *r = g.times_bcd[g.champ_stage];
    uint8_t t = digit_high(r[2]);

    if (t == 0xC0) {
        map_byte_set(0x0360, 0x0F);
        map_byte_set(0x0361, (uint8_t)(map_byte(0x0361) | 0x01));
    } else {
        map_byte_set(0x0360, t);
    }
    map_byte_set(0x0362, digit_low(r[2]));
    map_byte_set(0x0364, 0xE6);
    map_byte_set(0x0366, digit_high(r[1]));
    map_byte_set(0x0368, digit_low(r[1]));
    map_byte_set(0x036A, 0xE7);
    map_byte_set(0x036C, digit_high(r[0]));
    map_byte_set(0x036E, digit_low(r[0]));
}

/* Seconds past 59 carry a minute (decimal). */
static void seconds_carry(void)
{
    int c = 1;

    if (g.race_time[1] >= 0x60) {
        g.race_time[1] = dec_sub(g.race_time[1], 0x60, &c);
        c = 0;
        g.race_time[2] = dec_add(g.race_time[2], 0x01, &c);
    }
}

/* The bonus moved by hundredths `lo` and seconds `mid` into the total. */
static void bonus_move(uint8_t lo, uint8_t mid)
{
    int c = 1;
    uint8_t *bonus = g.race_time_best, *total = g.race_time;

    bonus[0] = dec_sub(bonus[0], lo, &c);
    bonus[1] = dec_sub(bonus[1], mid, &c);
    c = 0;
    total[0] = dec_add(total[0], lo, &c);
    total[1] = dec_add(total[1], mid, &c);
}

/*
 * @routine 07:D268 ResultsBonusCount
 * WHAT IT DOES: Counts the time bonus into the total a step a frame (from
 * frame $12 B or START adds all of it at once): 2.17 seconds a step with a
 * minute or more, else $73 or $57 hundredths by the seconds, else one; then
 * the bonus's digits and the total's.
 */
static void ResultsBonusCount(void)
{
    uint8_t *bonus = g.race_time_best, *total = g.race_time;
    uint8_t n = (uint8_t)(g.banner_timer[0] + 1);
    int c;

    if (n >= 0xFA)
        n = 0xFA;
    g.banner_timer[0] = n;
    if (n >= 0x12 && pressed(0x9000)) {
        c = 0;
        total[0] = dec_add(total[0], bonus[0], &c);
        total[1] = dec_add(total[1], bonus[1], &c);
        if (c) {
            c = 1;
            total[1] = dec_sub(total[1], 0x60, &c);
            c = 0;
            total[2] = dec_add(total[2], 0x01, &c);
            c = 0;
        }
        if (total[1] >= 0x60) {
            c = 1;
            total[1] = dec_sub(total[1], 0x60, &c);
            c = 0;
            total[2] = dec_add(total[2], 0x01, &c);
        }
        total[2] = dec_add(total[2], bonus[2], &c);
        bonus[0] = bonus[1] = bonus[2] = 0;
        step_next();
        g.overlay_state = 0x02;
        SoundPort0QueuePushRaw(0x3F);
    } else {
        if (bonus[2]) {
            c = 1;
            bonus[0] = dec_sub(bonus[0], 0x17, &c);
            bonus[1] = dec_sub(bonus[1], 0x02, &c);
            if (!c) {
                c = 0;
                bonus[1] = dec_add(bonus[1], 0x60, &c);
                c = 1;
                bonus[2] = dec_sub(bonus[2], 0x01, &c);
            }
            c = 0;
            total[0] = dec_add(total[0], 0x17, &c);
            total[1] = dec_add(total[1], 0x02, &c);
        } else if (!bonus[1]) {
            c = 1;
            bonus[0] = dec_sub(bonus[0], 0x01, &c);
            if (!bonus[0]) {
                step_next();
                g.overlay_state = 0x02;
                SoundPort0QueuePushRaw(0x3F);
            }
            c = 0;
            total[0] = dec_add(total[0], 0x01, &c);
            total[1] = dec_add(total[1], 0x00, &c);
        } else {
            bonus_move(bonus[1] == 0x01 ? 0x73 : 0x57, 0x00);
        }
        seconds_carry();
    }
    map_byte_set(0x038E, digit_low(bonus[2]));
    map_byte_set(0x0392, digit_high(bonus[1]));
    map_byte_set(0x0394, digit_low(bonus[1]));
    map_byte_set(0x0398, digit_high(bonus[0]));
    map_byte_set(0x039A, digit_low(bonus[0]));
    ResultsTotalTiles();
}

/* @routine 07:D471 ResultsRecordCompare */
static void ResultsRecordCompare(void)
{
    const uint8_t *r = g.times_bcd[g.champ_stage];
    int i, cmp = 0;

    for (i = 2; i >= 0 && !cmp; i--)
        if (g.race_time[i] != r[i])
            cmp = g.race_time[i] < r[i] ? -1 : 1;
    if (cmp > 0) {
        TilemapPaletteFill(0x0221, 8, 0x10);
    } else if (cmp < 0) {
        if (r[2] != 0xFF)
            TilemapPaletteFill(0x0361, 8, 0x10);
        TilemapPaletteCycle(0x0221, 8);
    }
    if (pressed(0x9000))
        g.fade_out = 0x01;
    if (g.overlay_state == 0x01)
        g.overlay_state = 0x03;
}

/* ---- the step ---- */

typedef void (*StepFn)(void);

/* The tables at $07:C6C3 (mode 0), $07:C715 (mode 1) and $07:C6EB (mode 3). */
static const StepFn steps_race[] = {
    ResultsUnpackNext, ResultsPicture4000, ResultsCarPicture, ResultsPanelFA39,
    ResultsChampionScreen, ResultsTotalLineWords, TimesKeepAndAdvance, ResultsWaitWindow,
    LapCharsLap1, LapCharsLap2, LapCharsLap3, LapCharsTotal,
    ResultsBlinkWait, ResultsBonusCount, RaceTimeTiles, ResultsRecordCompare
};
static const StepFn steps_stunt[] = {
    ResultsNext, ResultsPicture4000, ResultsCarPicture, ResultsPanelFAB5,
    ResultsChampionScreen, ResultsTotalLine, TimesKeepAndAdvance, ResultsWaitWindow,
    StuntResultsCount, StuntRecordsChoice, ResultsPanelFB99, ResultsMenu,
    StuntRecordsDraw, StuntRecordsWait
};
static const StepFn steps_all[] = {
    ResultsNext, ResultsPicture4000, ResultsCarPicture, ResultsPanelF38E,
    ResultsChampionScreen, ResultsTotalLine, TimesKeepAndAdvance, ResultsWaitWindow,
    LapCharsLap1, LapCharsLap2, LapCharsLap3, LapCharsTotal,
    TimedRecordsChoice, ResultsPanelFB99, ResultsMenu, TimedRecordsDraw,
    TimedRecordsWait
};

static void steps(const StepFn *t, unsigned n)
{
    if (g.overlay_d59 < n)
        t[g.overlay_d59]();
}

#define STEPS(t) (t), (sizeof (t) / sizeof (t)[0])

/* @routine 07:C6AA ResultsScreenStep */
void ResultsScreenStep(void)
{
    switch (g.hud_mode) {
    case 0: steps(STEPS(steps_race)); break;
    case 1: steps(STEPS(steps_stunt)); break;
    case 3: steps(STEPS(steps_all)); break;
    default: break;                     /* mode 2 has none */
    }
}

/* One frame of 07:C683 OverlayFramesUntilFaded: the step and the overlay's sprites. */
void OverlayFrameStep(void)
{
    ResultsScreenStep();
    OverlaySpritesStep();
}

/* ---- the championship's screen between its races ---- */

/* @routine 07:BA71 (stage 0) */
void ChampionshipStageStart(void)
{
    g.palette_animate = 0;
    g.split_stage_on = 0x06;
    g.split_stage = 0x00;
    windows.bands_kind = 0;
    g.overlay_state = 0;
    WindowSlideUpSound();
    step_next();
    OamHide36And96();
}

/* @routine 07:BA56 (stage 1) */
void ChampionshipStageWaitOpen(void)
{
    if (windows.bands_state != 0x05)
        return;
    step_next();
    results_bg2();
}

/* @routine 07:BA9A (stage 4) */
void ChampionshipStagePicture(void)
{
    int i;

    g.race_again = 0;
    palettes.tile_cycle = 0;
    picture_unpack(0x1F, 0xFB99, 0x3000);
    for (i = 0; i < 0x440; i++)
        map_byte_set((uint16_t)i, fx_ram[0x3000 + i]);
    DigitTiles1184();
    step_next();
    g.hdma_enable = (uint8_t)((g.hdma_enable | 0x02) & 0xF3);
    champion_palette();
}

/* @routine 07:BA3A (stage 7) */
void ChampionshipStageMap(void)
{
    results_bg2();
    g.results_page_shown = 1;
    step_next();
    windows.bands_kind = 0;
}

/* @routine 07:BA8F (stage 8) */
void ChampionshipStageWaitClosed(void)
{
    if (!windows.bands_state)
        step_next();
}

/*
 * @routine 07:C30B WindowClosedDigitStep
 * WHAT IT DOES: Once the window is closed: with the override set, it is
 * cleared, the cursor is 2 and the race runs again (RaceRestart, 07:EC6B);
 * without it the race is left (RaceEndLeave, 07:EBDD). 0 while waiting.
 */
int WindowClosedDigitStep(void)
{
    if (windows.bands_state != 0x05)
        return 0;
    if (!g.time_override) {
        RaceEndLeave();
        return 2;
    }
    g.time_override = 0;
    g.champ_stage = 0x02;
    RaceRestart();
    return 1;
}

/* @routine 07:BA15 ChampionshipScreenStep: the table at $07:BA26 */
static void ChampionshipScreenStep(void)
{
    static const StepFn t[] = {
        ChampionshipStageStart, ChampionshipStageWaitOpen, ResultsPicture4000, ResultsCarPicture,
        ChampionshipStagePicture, ResultsTotalLine, TimesKeepAndAdvance, ChampionshipStageMap,
        ChampionshipStageWaitClosed, ResultsMenu
    };

    if (!g.hud_mode) {
        WindowClosedDigitStep();
        return;
    }
    steps(STEPS(t));
}

/*
 * @routine 07:B9EE ChampionshipScreenLoop
 * WHAT IT DOES: One frame of the championship's screen: its step, the
 * overlay's sprites, both pads' presses and held buttons cleared; 1 once the
 * fade is done.
 */
int ChampionshipScreenFrame(void)
{
    ChampionshipScreenStep();
    if (race_flow_stopped())
        return 0;
    OverlaySpritesStep();
    frame_state.pressed_copy[0] = frame_state.pressed_copy[1] = 0;
    frame_state.held[0] = frame_state.held[1] = 0;
    return (g.fade_level & 0x80) != 0;
}

/* ---- the championship's finish ---- */

/* 07:EE26's rest, once the overlay has faded: the run's time in the records,
   then the champion's screen and the restart. */
static void championship_finish_records(void)
{
    g.champ_stage = 0;
    RecordTimeInsert();
    RecordsUnpackCourseRunning();
    race_champion_screen(g.course_class == 0x02 ? BootSoftRestart : SoftResetToBootWarm);
}

/*
 * @routine 07:EE26 ChampionshipFinish
 * WHAT IT DOES: The end of a championship after its last times screen: the
 * overlay faded out, the run's time put in the records, the champion's
 * screen; then the game restarts: the ending after class 2, else the course
 * menu.
 */
void ChampionshipFinish(void)
{
    g.overlay_d59 = 0;
    race_wait_overlay(championship_finish_records);
}
