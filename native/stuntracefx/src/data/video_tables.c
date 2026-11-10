/*
 * video_tables.c: where the video's tables are in the cartridge (bank and
 * address, the original's), each written once here, and their reads. Every
 * read is the original's: the address sum kept to 16 bits, a word
 * little-endian. The tables in the code the original copied to work RAM
 * are named by the address they ran at ($7E, $7F: data/cart.h).
 */
#include "data/cart.h"
#include "data/unpack.h"
#include "data/video_tables.h"

typedef struct { uint8_t bank; uint16_t at; } Place;

static uint8_t byte_at(Place p, unsigned offset) { return cart8(p.bank, (uint16_t)(p.at + offset)); }
static uint16_t word_at(Place p, unsigned offset) { return cart16(p.bank, (uint16_t)(p.at + offset)); }
static uint8_t ram7e_byte(uint16_t at, unsigned offset) { return ram_code_7e8((uint16_t)(at + offset)); }
static uint16_t ram7e_word(uint16_t at, unsigned offset) { return ram_code_7e16((uint16_t)(at + offset)); }

/* ---- packed art and maps ---- */

/* the records in bank $03 that name packed data: its address, then its
   bank; a transfer record's length is at +5 */
enum { RECORDS_BANK = 0x03 };

static uint32_t record_unpack(uint16_t record, uint8_t *out, uint32_t room)
{
    Place r = {RECORDS_BANK, record};

    return unpack((uint32_t)byte_at(r, 2) << 16 | word_at(r, 0), out, room);
}

static uint16_t transfer_length(uint16_t transfer)
{
    Place t = {RECORDS_BANK, transfer};

    return word_at(t, 5);
}

/* by a record and its transfer record, or (record 0) the packed data's end itself */
static const struct { uint16_t record, transfer; uint32_t end; } k_video_blocks[] = {
    [VIDEO_BLOCK_SPRITE_ART] = {0xF7DA, 0xF80D, 0},
    [VIDEO_BLOCK_SHARED_ART] = {0xF7DD, 0xF814, 0},
    [VIDEO_BLOCK_HUD_FRAME_SPEED] = {0xF7D1, 0xF829, 0},
    [VIDEO_BLOCK_HUD_FRAME_STUNT] = {0xF7D4, 0xF830, 0},
    [VIDEO_BLOCK_SPLIT_PANEL_MAP] = {0xF7E3, 0xF84C, 0},
    [VIDEO_BLOCK_HUD_CARS] = {0, 0, 0x1D9228},
    [VIDEO_BLOCK_HUD_PIECES] = {0, 0, 0x18FA38},
    [VIDEO_BLOCK_AFTER_SPEED] = {0, 0, 0x18FAE8},
    [VIDEO_BLOCK_AFTER_STUNT] = {0, 0, 0x18FB90},
    [VIDEO_BLOCK_AFTER_ALL] = {0, 0, 0x18FC20},
    [VIDEO_BLOCK_SPLIT_FRAME_ART] = {0, 0, 0x17FB84},
    [VIDEO_BLOCK_SPLIT_FRAME_MAP] = {0, 0, 0x17FFAC},
    [VIDEO_BLOCK_SPLIT_SPRITE_ART] = {0, 0, 0x18C770},
    [VIDEO_BLOCK_COURSE_PICTURES] = {0, 0, 0x1DD970},
    [VIDEO_BLOCK_HUD_PICTURE] = {0, 0, 0x1D8078},
    [VIDEO_BLOCK_RESULTS_CARS] = {0, 0, 0x18D920},
};

uint32_t video_block_unpack(VideoBlock block, uint8_t *out, uint32_t room)
{
    if (!k_video_blocks[block].record)
        return unpack(k_video_blocks[block].end, out, room);
    record_unpack(k_video_blocks[block].record, out, room);
    return transfer_length(k_video_blocks[block].transfer);
}

/* a course's records, three bytes a course */
static const Place
    COURSE_ART = {RECORDS_BANK, 0xF4F9}, COURSE_ART_2 = {RECORDS_BANK, 0xF54D},
    COURSE_MAP = {RECORDS_BANK, 0xF5A1}, COURSE_MAP_2 = {RECORDS_BANK, 0xF5F5};
enum { COURSE_ART_TRANSFER = 0xF853, COURSE_ART_2_TRANSFER = 0xF85A };

int course_block_present(int course, CourseBlock block)
{
    unsigned c = (unsigned)course * 3;

    if (block == COURSE_BACKDROP_ART_2)
        return word_at(COURSE_ART_2, c) != 0;
    if (block == COURSE_BACKDROP_MAP_2)
        return byte_at(COURSE_MAP_2, c + 2) != 0;
    return 1;
}

uint32_t course_block_unpack(int course, CourseBlock block, uint8_t *out, uint32_t room)
{
    unsigned c = (unsigned)course * 3;

    switch (block) {
    case COURSE_BACKDROP_ART:
        record_unpack((uint16_t)(COURSE_ART.at + c), out, room);
        return transfer_length(COURSE_ART_TRANSFER);
    case COURSE_BACKDROP_ART_2:
        record_unpack((uint16_t)(COURSE_ART_2.at + c), out, room);
        return transfer_length(COURSE_ART_2_TRANSFER);
    case COURSE_BACKDROP_MAP:
        return record_unpack((uint16_t)(COURSE_MAP.at + c), out, room);
    default:
        return record_unpack((uint16_t)(COURSE_MAP_2.at + c), out, room);
    }
}

/* the turning pictures: three bytes each, the packed data's end and bank;
   an end below $8000 is another picture's number */
static const Place TURN_PICTURES = {0x07, 0xC648};

uint16_t turn_picture_resolve(uint16_t number)
{
    for (;;) {
        uint16_t end = word_at(TURN_PICTURES, (unsigned)(number * 3));

        if (end >= 0x8000)
            return number;
        number = end;
    }
}

uint32_t turn_picture_unpack(uint16_t number, uint8_t *out, uint32_t room)
{
    unsigned at = (unsigned)(number * 3);

    return unpack((uint32_t)byte_at(TURN_PICTURES, at + 2) << 16 | word_at(TURN_PICTURES, at), out, room);
}

static const Place COURSE_MODELS = {RECORDS_BANK, 0xF799};

uint16_t course_models_word(int course) { return word_at(COURSE_MODELS, (unsigned)(course * 2)); }

/* ---- colours ---- */

enum {
    FADE_IN_STEPS = 0xAC9D,             /* $7E */
    CYCLE_10 = 0xB71B, CYCLE_11 = 0xB6FB, CYCLE_12 = 0xB70B,
    KEPT_E3 = 0xB4E9, KEPT_E1 = 0xB505, KEPT_E2 = 0xB521,
    STATUS_KEPT = 0xB5B9, STATUS_HIGH = 0xB5B1,
    GRADIENT_BAND_STARTS = 0xE69C,
};

uint16_t fade_in_step(uint8_t level) { return ram7e_word(FADE_IN_STEPS, (uint8_t)(level << 1)); }

uint16_t colour_cycle_colour(ColourCycle cycle, int i)
{
    uint16_t table = cycle == COLOUR_CYCLE_10 ? CYCLE_10 : cycle == COLOUR_CYCLE_11 ? CYCLE_11 : CYCLE_12;

    return ram7e_word(table, (unsigned)(i * 2));
}

/* a course's colour blocks, three bytes a course (address, bank): its
   colours 16-255 by day and by night, and 0-15 by day and by night; the
   day low block runs on into the day sky's colours ($20 on), the night low
   block's address into the night sky's (in the day block's bank) */
static const Place
    DAY_COLOURS = {RECORDS_BANK, 0xF649}, NIGHT_COLOURS = {RECORDS_BANK, 0xF69D},
    DAY_LOW_COLOURS = {RECORDS_BANK, 0xF6F1}, NIGHT_LOW_COLOURS = {RECORDS_BANK, 0xF745};

static uint16_t block_colour(Place blocks, int course, unsigned offset)
{
    unsigned c = (unsigned)course * 3;
    Place b = {byte_at(blocks, c + 2), word_at(blocks, c)};

    return word_at(b, offset);
}

uint16_t course_day_colour(int course, int colour)
{
    return block_colour(colour < 16 ? DAY_LOW_COLOURS : DAY_COLOURS, course, (unsigned)(2 * colour));
}

uint16_t course_night_colour(int course, int colour)
{
    return block_colour(colour < 16 ? NIGHT_LOW_COLOURS : NIGHT_COLOURS, course, (unsigned)(2 * colour));
}

uint16_t course_sky_colour(int course, int list, unsigned run)
{
    unsigned c = (unsigned)course * 3;
    Place b = {byte_at(DAY_LOW_COLOURS, c + 2), word_at(list ? NIGHT_LOW_COLOURS : DAY_LOW_COLOURS, c)};

    return word_at(b, 2 * run + 0x20);
}

int course_has_night_sky(int course) { return word_at(NIGHT_LOW_COLOURS, (unsigned)course * 3) != 0; }

static const Place BLEND_BITS = {RECORDS_BANK, 0xEF1F};

int colour_blends_to_night(int colour)
{
    uint16_t bits = word_at(BLEND_BITS, (unsigned)((colour >> 3) & 0x1E));

    return (bits >> (colour & 15)) & 1;
}

uint16_t kept_colour(KeptColour which, uint8_t step)
{
    uint16_t table = which == KEPT_COLOUR_E1 ? KEPT_E1 : which == KEPT_COLOUR_E2 ? KEPT_E2 : KEPT_E3;

    return ram7e_word(table, (unsigned)(step * 2));
}

uint16_t status_colour(StatusColours set, int i)
{
    return ram7e_word(set == STATUS_COLOURS_KEPT ? STATUS_KEPT : STATUS_HIGH, (unsigned)(i * 2));
}

/* courses 3 and 7's cycling colours 12-15, four lists of words; course $0B's
   colour 15 low bytes, by day and late */
static const uint16_t k_cycle_3[4] = {0xE45F, 0xE48B, 0xE4B7, 0xE4E3};
static const uint16_t k_cycle_7[4] = {0xE50F, 0xE53B, 0xE4B7, 0xE4E3};
enum { CYCLE_BANK = 0x04, CYCLE_LOW_DAY = 0xE407, CYCLE_LOW_LATE = 0xE433 };

uint16_t course_cycle_colour(int course, int i, uint8_t at)
{
    Place p = {CYCLE_BANK, course == 3 ? k_cycle_3[i] : k_cycle_7[i]};

    return word_at(p, at);
}

uint8_t course_cycle_low_byte(int late, uint8_t at)
{
    Place p = {CYCLE_BANK, late ? CYCLE_LOW_LATE : CYCLE_LOW_DAY};

    return byte_at(p, at);
}

uint16_t gradient_band_start(int band) { return ram7e_word(GRADIENT_BAND_STARTS, (unsigned)(band * 2)); }

static const Place SKY_RUNS = {0x1F, 0xB44E};

uint8_t sky_run_length(unsigned at) { return byte_at(SKY_RUNS, at); }

static const Place ROW_CYCLE = {0x07, 0xCF26}, ROW_CYCLE_BATTLE = {0x07, 0xCF2C};

uint8_t results_row_cycle_bits(int battle, uint8_t phase) { return byte_at(battle ? ROW_CYCLE_BATTLE : ROW_CYCLE, phase); }

static const Place FIXED_LEVELS = {RECORDS_BANK, 0xFE7B};

uint8_t fixed_colour_level(uint8_t step) { return byte_at(FIXED_LEVELS, step); }

/* ---- the backdrop ---- */

enum { BACKDROP_ONE_SCROLLS = 0xE811, BACKDROP_TWO_SCROLLS = 0xE920 };   /* $7E */

uint16_t backdrop_one_scroll_base(int buffer) { return ram7e_word(BACKDROP_ONE_SCROLLS, (unsigned)(buffer * 2)); }
uint16_t backdrop_two_scroll_base(int half) { return ram7e_word(BACKDROP_TWO_SCROLLS, (unsigned)(half * 2)); }

static const Place LEVEL_COLUMN_PLACES = {0x02, 0xFB6E}, LEVEL_COLUMN_TILES = {0x02, 0xFB4E};

uint16_t level_column_place(int column) { return word_at(LEVEL_COLUMN_PLACES, (unsigned)(2 * column)); }
uint16_t level_column_tile(uint16_t at) { return word_at(LEVEL_COLUMN_TILES, at); }

/* ---- sprites ---- */

static SpritePiece piece_at(Place pieces, uint16_t at)
{
    SpritePiece s;

    s.x = byte_at(pieces, at);
    s.y = byte_at(pieces, (unsigned)at + 1);
    s.tile = byte_at(pieces, (unsigned)at + 2);
    s.attr = byte_at(pieces, (unsigned)at + 3);
    s.size = byte_at(pieces, (unsigned)at + 4);
    return s;
}

/* four bytes a group: its first piece's byte offset, its count word; five bytes a piece */
static const Place SPRITE_GROUPS = {0x04, 0xEDCB}, SPRITE_PIECES = {0x04, 0xEE4B};
static const Place FLASH_GROUPS = {0x1C, 0xE5AC}, FLASH_PIECES = {0x1C, 0xE62C};
static const Place PIECE_SETS_BY_SELECTOR = {0x07, 0xB981}, PIECE_SETS = {0x1C, 0xF09E}, PIECE_SET_PIECES = {0x1C, 0xF11E};

uint16_t sprite_group_first(int group) { return word_at(SPRITE_GROUPS, (unsigned)(group * 4)); }
uint16_t sprite_group_count(int group) { return word_at(SPRITE_GROUPS, (unsigned)(group * 4 + 2)); }
SpritePiece sprite_group_piece(uint16_t at) { return piece_at(SPRITE_PIECES, at); }
uint16_t flash_group_first(int set) { return word_at(FLASH_GROUPS, (unsigned)(set * 4)); }
uint16_t flash_group_count(int set) { return word_at(FLASH_GROUPS, (unsigned)(set * 4 + 2)); }
SpritePiece flash_group_piece(uint16_t at) { return piece_at(FLASH_PIECES, at); }
uint8_t piece_set_of(uint8_t selector) { return byte_at(PIECE_SETS_BY_SELECTOR, selector); }
uint16_t piece_set_first(uint16_t set) { return word_at(PIECE_SETS, (unsigned)((uint16_t)(set << 2) & 0xFF)); }
SpritePiece piece_set_piece(uint16_t at) { return piece_at(PIECE_SET_PIECES, at); }

/* the three car pictures the overlay's palettes go by (the operands of its
   comparisons), and its pieces: three bytes each, x, y, tile */
static const Place CAR_PICTURE_WORDS = {0x04, 0xE066}, OVERLAY_PIECES = {0x04, 0xE1B6};

uint8_t car_picture_palette(uint16_t picture)
{
    if (picture == word_at(CAR_PICTURE_WORDS, 0))
        return 0;
    if (picture == word_at(CAR_PICTURE_WORDS, 2))
        return 2;
    if (picture == word_at(CAR_PICTURE_WORDS, 4))
        return 4;
    return 8;
}

SpritePiece overlay_piece(unsigned at)
{
    SpritePiece s = {0, 0, 0, 0, 0};

    s.x = byte_at(OVERLAY_PIECES, at);
    s.y = byte_at(OVERLAY_PIECES, at + 1);
    s.tile = byte_at(OVERLAY_PIECES, at + 2);
    return s;
}

/* the courses' names: a word each (by the course's byte, doubled) naming its text in the same bank */
static const Place COURSE_NAMES = {0x07, 0x8000};

uint8_t course_name_byte(int course, unsigned i)
{
    Place text = {COURSE_NAMES.bank, word_at(COURSE_NAMES, (uint8_t)(course * 2))};

    return byte_at(text, i);
}

static const Place
    RECORDS_STUNT_PALETTES = {0x04, 0xE30F}, RECORDS_SPEED_PALETTES = {0x04, 0xE26A},
    RECORDS_BATTLE_PALETTES = {0x04, 0xE30B}, RECORDS_CAR_PALETTES = {0x04, 0xE313};

uint8_t records_palette_stunt(uint8_t stage) { return byte_at(RECORDS_STUNT_PALETTES, stage); }
uint8_t records_palette_speed(uint8_t i) { return byte_at(RECORDS_SPEED_PALETTES, i); }
uint8_t records_palette_battle(uint8_t stage) { return byte_at(RECORDS_BATTLE_PALETTES, stage); }
uint8_t records_palette_car(uint8_t car) { return byte_at(RECORDS_CAR_PALETTES, car); }

/* the strips' tile words, eight bytes a strip of four, sixteen of six ($7F) */
enum { STRIP4_TILES = 0x09CD, STRIP6_TILES = 0x0A66 };

static uint16_t ram7f_word_at(const uint8_t *table, int i) { return (uint16_t)(table[2 * i] | table[2 * i + 1] << 8); }

uint16_t strip4_tile(uint16_t selector, int i)
{
    return ram7f_word_at(ram_code_7f((uint16_t)(STRIP4_TILES + ((selector >> 1) & 0x00F8))), i);
}

uint16_t strip6_tile(uint16_t selector, int i)
{
    return ram7f_word_at(ram_code_7f((uint16_t)(STRIP6_TILES + ((selector & 0x0038) << 1))), i);
}

/* ---- the windows ---- */

/*
 * Where the original kept window edges, which the band lists name: the
 * normal edges at $7F:F857, the view's and each half's and the split's at
 * $FFD1-$FFDD in that order, the edge rows from $F883 (four bytes a row),
 * the edge records from $43D7. Anything else is the normal edges, as the
 * original's tables never name it.
 */
enum { EDGES_AT_NORMAL = 0xF857, EDGES_AT_NAMED = 0xFFD1, EDGES_AT_ROWS = 0xF883, EDGES_AT_RECORDS = 0x43D7 };

static EdgesRef edges_by_word(uint16_t w)
{
    EdgesRef r = {EDGES_NORMAL, 0};

    if (w >= EDGES_AT_NAMED && w < EDGES_AT_NAMED + 16 && !((w - EDGES_AT_NAMED) & 3)) {
        r.kind = (uint8_t)(EDGES_VIEW + (w - EDGES_AT_NAMED) / 4);
    } else if (w >= EDGES_AT_ROWS && w < EDGES_AT_ROWS + WINDOW_EDGE_ROWS * 4) {
        r.kind = EDGES_ROW;
        r.at = (uint16_t)((w - EDGES_AT_ROWS) / 4);
    } else if (w >= EDGES_AT_RECORDS && w + 4u <= EDGES_AT_RECORDS + WINDOW_EDGE_RECORDS_SIZE) {
        r.kind = EDGES_RECORD;
        r.at = (uint16_t)(w - EDGES_AT_RECORDS);
    }
    return r;
}

static uint16_t edges_word(EdgesRef r)
{
    switch (r.kind) {
    case EDGES_VIEW: case EDGES_TOP_VIEW: case EDGES_BOTTOM_VIEW: case EDGES_SPLIT:
        return (uint16_t)(EDGES_AT_NAMED + 4 * (r.kind - EDGES_VIEW));
    case EDGES_ROW:
        return (uint16_t)(EDGES_AT_ROWS + 4 * r.at);
    case EDGES_RECORD:
        return (uint16_t)(EDGES_AT_RECORDS + r.at);
    default:
        return EDGES_AT_NORMAL;
    }
}

/* as the original stepped a repeating band's address four bytes a line */
EdgesRef edges_ref_on(EdgesRef edges, int lines) { return edges_by_word((uint16_t)(edges_word(edges) + 4 * lines)); }

static WindowBandEntry band_entry(uint8_t lines, uint16_t edges)
{
    WindowBandEntry e;

    e.lines = lines;
    e.edges = edges_by_word(edges);
    return e;
}

/* $7E: the layouts' counts and edges by the layout's place, the layouts'
   ends, the band lists' and presets' places, the oscillator's rates, the
   fixed colour bands' limits and the level steps' thresholds */
enum {
    LAYOUT_ONE_TOP_LINES = 0xE0B4, LAYOUT_ONE_MIDDLE_LINES = 0xE0B5,
    LAYOUT_TWO_TOP_LINES = 0xE0C2, LAYOUT_TWO_MIDDLE_LINES = 0xE0C3,
    LAYOUT_PAUSED_TOP_LINES = 0xE0CA, LAYOUT_PAUSED_MIDDLE_LINES = 0xE0CB,
    LAYOUT_EDGES = 0xE0D2, LAYOUT_PAUSED_EDGES = 0xE0E0, LAYOUT_LIMITS = 0xDBC5,
    BAND_LISTS = 0xD8DE, BAND_PRESETS = 0xD9C5, BAND_PRESETS_PAUSED = 0xDA1D,
    OSCILLATOR_RATES = 0xD30D, TOP_BAND_LIMITS = 0xD5AA, LOW_BAND_LIMITS = 0xD56E, LEVEL_THRESHOLDS = 0xD5B3,
};

uint8_t window_layout_lines(LayoutLines which, uint16_t at)
{
    static const uint16_t tables[6] = {
        LAYOUT_ONE_TOP_LINES, LAYOUT_ONE_MIDDLE_LINES, LAYOUT_TWO_TOP_LINES,
        LAYOUT_TWO_MIDDLE_LINES, LAYOUT_PAUSED_TOP_LINES, LAYOUT_PAUSED_MIDDLE_LINES,
    };

    return ram7e_byte(tables[which], at);
}

EdgesRef window_layout_edges(int paused, uint16_t at)
{
    return edges_by_word(ram7e_word(paused ? LAYOUT_PAUSED_EDGES : LAYOUT_EDGES, at));
}

uint16_t window_layout_limit(int split) { return ram7e_word(LAYOUT_LIMITS, split ? 2u : 0u); }

/* a list's entries: a count, then its edges' word; a count of 0 ends it */
int window_band_list(uint8_t select, WindowBandEntry *out, int max)
{
    const uint8_t *p = ram_code_7e(ram7e_word(BAND_LISTS, select));
    int n = 0;

    while (*p && n < max) {
        out[n++] = band_entry(p[0], (uint16_t)(p[1] | p[2] << 8));
        p += 3;
    }
    return n;
}

void window_band_preset(int paused, uint8_t index, WindowBandEntry out[3])
{
    const uint8_t *p = ram_code_7e(ram7e_word(paused ? BAND_PRESETS_PAUSED : BAND_PRESETS, index));
    int i;

    for (i = 0; i < 3; i++, p += 3)
        out[i] = band_entry(p[0], (uint16_t)(p[1] | p[2] << 8));
}

uint8_t window_oscillator_rate(uint8_t level) { return ram_code_7e8((uint16_t)(OSCILLATOR_RATES + level - 1)); }

static BandLimits limits_at(uint16_t at)
{
    BandLimits l;

    l.stop = ram_code_7e8(at);
    l.floor = ram_code_7e8((uint16_t)(at + 1));
    return l;
}

BandLimits window_top_band_limits(uint8_t at) { return limits_at((uint16_t)(TOP_BAND_LIMITS + at)); }
BandLimits window_low_band_limits(int raised) { return limits_at((uint16_t)(LOW_BAND_LIMITS + (raised ? 2 : 0))); }
uint8_t window_level_threshold(int at) { return ram7e_byte(LEVEL_THRESHOLDS, (unsigned)at); }

/* bank $09: the five fixed edges (four bytes each), the records' pairs
   (two players', one player's), one player's eight blocks (32 bytes apart)
   and last block, the four starting bands (count and edges' word, four bytes
   each), the edge lists' first rows (as row * 4) and places, the fixed
   colour's table, the time gates' bonuses by class and the top band's start
   by car */
static const Place
    PRESET_EDGES = {0x09, 0xF999},
    RECORD_PAIRS_TWO = {0x09, 0xF634}, RECORD_PAIRS_ONE = {0x09, 0xF170},
    RECORD_BLOCKS = {0x09, 0xF88B}, RECORD_LAST = {0x09, 0xF849},
    START_BANDS = {0x09, 0xF163},
    EDGE_LIST_ROWS = {0x09, 0xF155}, EDGE_LIST_PLACES = {0x09, 0xF98B},
    LEVEL_TABLE = {0x09, 0xFB74},
    TIME_GATE_BONUSES = {0x09, 0xF115}, TOP_BAND_STARTS = {0x09, 0xF14D};

void window_preset_edges(EdgesKind which, uint8_t out[4])
{
    unsigned at = (unsigned)(which == EDGES_NORMAL ? 0 : 4 * (1 + which - EDGES_VIEW));
    int i;

    for (i = 0; i < 4; i++)
        out[i] = byte_at(PRESET_EDGES, at + (unsigned)i);
}

uint8_t window_record_pair_byte(int two_players, unsigned at) { return byte_at(two_players ? RECORD_PAIRS_TWO : RECORD_PAIRS_ONE, at); }
uint8_t window_record_block_byte(int block, unsigned at) { return byte_at(RECORD_BLOCKS, (unsigned)(0x20 * block) + at); }
uint8_t window_record_last_byte(unsigned at) { return byte_at(RECORD_LAST, at); }

WindowBandEntry window_start_band(int i)
{
    unsigned at = (unsigned)(4 * i);

    return band_entry(byte_at(START_BANDS, at), (uint16_t)(byte_at(START_BANDS, at + 1) | byte_at(START_BANDS, at + 2) << 8));
}

int window_edge_list(uint16_t select, int *first_row, uint8_t pairs[][2], int max)
{
    Place list = {0x09, word_at(EDGE_LIST_PLACES, select)};
    unsigned at = 0;
    int n = 0;

    *first_row = word_at(EDGE_LIST_ROWS, select) / 4;
    for (;;) {
        if (n < max) {
            pairs[n][0] = byte_at(list, at);
            pairs[n][1] = byte_at(list, at + 1);
        }
        at += 2;
        n++;
        if (byte_at(list, at) == 0xFF)
            break;
    }
    return n < max ? n : max;
}

uint16_t window_level_word(int at) { return word_at(LEVEL_TABLE, (unsigned)at); }

uint8_t time_gate_bonus(uint8_t course_class, uint8_t stage)
{
    Place bonuses = {0x09, word_at(TIME_GATE_BONUSES, 2u * course_class)};

    return byte_at(bonuses, stage);
}

uint8_t window_top_band_start(uint8_t at) { return byte_at(TOP_BAND_STARTS, at); }
