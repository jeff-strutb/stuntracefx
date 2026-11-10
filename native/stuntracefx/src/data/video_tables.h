/*
 * video_tables.h: the video's tables from the cartridge, as typed data (plan
 * section 3, Data). Where each table is in the cartridge is written once, in
 * video_tables.c; video/ reads them only through these.
 *
 * A table's index is what the game keeps for it: an element, or, where the
 * game keeps a byte offset into a table (a sprite group's pieces, a layout's
 * place), that offset. Each says which. Reads past a table's end give the
 * cartridge's next bytes, as the original's did.
 */
#ifndef STUNTRACEFX_VIDEO_TABLES_H
#define STUNTRACEFX_VIDEO_TABLES_H

#include <stdint.h>

/* ---- packed art and maps, unpacked into `out` (`room` bytes) ---- */
typedef enum {
    VIDEO_BLOCK_SPRITE_ART,             /* the race's sprite art */
    VIDEO_BLOCK_SHARED_ART,             /* the sprites' characters $40 on */
    VIDEO_BLOCK_HUD_FRAME_SPEED,        /* the frame and HUD, mode 0 */
    VIDEO_BLOCK_HUD_FRAME_STUNT,        /* the frame and HUD, modes 1 and 3 */
    VIDEO_BLOCK_SPLIT_PANEL_MAP,        /* the two-player panel's map */
    VIDEO_BLOCK_HUD_CARS,               /* mode 0's car pictures for the HUD */
    VIDEO_BLOCK_HUD_PIECES,             /* modes 1 and 3's HUD pieces */
    VIDEO_BLOCK_AFTER_SPEED,            /* what each mode's load leaves unpacked in the chip's RAM */
    VIDEO_BLOCK_AFTER_STUNT,
    VIDEO_BLOCK_AFTER_ALL,
    VIDEO_BLOCK_SPLIT_FRAME_ART,        /* the two-player frame's characters */
    VIDEO_BLOCK_SPLIT_FRAME_MAP,        /* and its map */
    VIDEO_BLOCK_SPLIT_SPRITE_ART,       /* the two-player sprite art */
    VIDEO_BLOCK_COURSE_PICTURES,        /* the courses' pictures (race_course_picture_write) */
    VIDEO_BLOCK_HUD_PICTURE,            /* the HUD's picture (RaceHudLoad) */
    VIDEO_BLOCK_RESULTS_CARS            /* the cars' pictures for the results */
} VideoBlock;
/* returns the length its transfer record gives, or the unpacked size where it has none */
uint32_t video_block_unpack(VideoBlock block, uint8_t *out, uint32_t room);

/* a course's packed blocks: its backdrop's art (a second part on some
   courses) and its backdrop's map (a second map on some) */
typedef enum { COURSE_BACKDROP_ART, COURSE_BACKDROP_ART_2, COURSE_BACKDROP_MAP, COURSE_BACKDROP_MAP_2 } CourseBlock;
int course_block_present(int course, CourseBlock block);
/* returns the length the art's transfer record gives, or the unpacked size for a map */
uint32_t course_block_unpack(int course, CourseBlock block, uint8_t *out, uint32_t room);

/* the turning pictures: a number's entry, following those that name another
   picture instead, and that picture unpacked */
uint16_t turn_picture_resolve(uint16_t number);
uint32_t turn_picture_unpack(uint16_t number, uint8_t *out, uint32_t room);

/* the course's models as the Super FX's program takes them (an address in their bank) */
uint16_t course_models_word(int course);

/* ---- colours ---- */
uint16_t fade_in_step(uint8_t level);                   /* the fade's step for a brightness level */
typedef enum { COLOUR_CYCLE_10, COLOUR_CYCLE_11, COLOUR_CYCLE_12 } ColourCycle;
uint16_t colour_cycle_colour(ColourCycle cycle, int i); /* the cycling colours $10-$12, phase 0-7 */
uint16_t course_day_colour(int course, int colour);     /* a course's 256 colours, day and night */
uint16_t course_night_colour(int course, int colour);
int colour_blends_to_night(int colour);                 /* the colours the weather blends toward night */
typedef enum { KEPT_COLOUR_E1, KEPT_COLOUR_E2, KEPT_COLOUR_E3 } KeptColour;
uint16_t kept_colour(KeptColour which, uint8_t step);   /* the kept item's colours, step 0-12 */
typedef enum { STATUS_COLOURS_KEPT, STATUS_COLOURS_HIGH } StatusColours;
uint16_t status_colour(StatusColours set, int i);       /* the status colour's flashes, 0-3 */
uint16_t course_cycle_colour(int course, int i, uint8_t at);    /* courses 3 and 7: colours 12-15 by the cycle's byte offset */
uint8_t course_cycle_low_byte(int night, uint8_t at);   /* course $0B: colour 15's low byte */
uint16_t gradient_band_start(int band);                 /* a gradient band's start (bytes), band 0-2 */
uint8_t sky_run_length(unsigned at);                    /* the sky lists' run lengths; at: the run */
uint16_t course_sky_colour(int course, int list, unsigned run); /* a course's sky colour by run; list 0 day, 1 night */
int course_has_night_sky(int course);
uint8_t results_row_cycle_bits(int battle, uint8_t phase);  /* the menus' rows' cycling palette bits, phase 0-5 */
uint8_t fixed_colour_level(uint8_t step);               /* the fixed colour's level by its step */

/* ---- the backdrop ---- */
uint16_t backdrop_one_scroll_base(int buffer);          /* one player: BG2's scroll for each picture buffer */
uint16_t backdrop_two_scroll_base(int half);            /* two players: each half's */
uint16_t level_column_place(int column);                /* the level columns: a column's place in the map (bytes) */
uint16_t level_column_tile(uint16_t at);                /* and their full rows' words; at: bytes */

/* ---- sprites ---- */
typedef struct { uint8_t x, y, tile, attr, size; } SpritePiece;
/* the sprite groups (MetaspriteDraw): a group's first piece (a byte offset
   into the pieces) and its count word, and a piece */
uint16_t sprite_group_first(int group);
uint16_t sprite_group_count(int group);
SpritePiece sprite_group_piece(uint16_t at);
/* the flashing groups: the same, by set */
uint16_t flash_group_first(int set);
uint16_t flash_group_count(int set);
SpritePiece flash_group_piece(uint16_t at);
/* the three-piece sets: a selector's set, a set's first piece (byte offset), a piece */
uint8_t piece_set_of(uint8_t selector);
uint16_t piece_set_first(uint16_t set);
SpritePiece piece_set_piece(uint16_t at);
/* the results' car overlay: a car picture's palette bits, and the pieces by byte offset */
uint8_t car_picture_palette(uint16_t picture);
SpritePiece overlay_piece(unsigned at);                 /* x, y and tile */
/* a course's name as sprite glyphs: byte 0 its length, then a glyph a byte (bit 7: a space) */
uint8_t course_name_byte(int course, unsigned i);
/* the records pages' sprite palettes */
uint8_t records_palette_stunt(uint8_t stage);
uint8_t records_palette_speed(uint8_t i);
uint8_t records_palette_battle(uint8_t stage);
uint8_t records_palette_car(uint8_t car);
/* the HUD's strips: a strip's tile word i by its selector */
uint16_t strip4_tile(uint16_t selector, int i);
uint16_t strip6_tile(uint16_t selector, int i);

/* ---- the windows ---- */
/* The band lists name a band's edges by where the original kept them: the
   normal edges, the view's, each half's view, the split's, a row of the
   edge rows, or a byte of the edge records. */
typedef enum { EDGES_NORMAL, EDGES_VIEW, EDGES_TOP_VIEW, EDGES_BOTTOM_VIEW, EDGES_SPLIT, EDGES_ROW, EDGES_RECORD } EdgesKind;
typedef struct { uint8_t kind; uint16_t at; } EdgesRef;  /* at: the row, or the records' byte */
enum { WINDOW_EDGE_ROWS = 448, WINDOW_EDGE_RECORDS_SIZE = 0x0E00 };
EdgesRef edges_ref_on(EdgesRef edges, int lines);       /* a repeating band's edges `lines` lines on */
typedef struct { uint8_t lines; EdgesRef edges; } WindowBandEntry;

/* the layouts' bands by the layout's place (bytes) */
typedef enum { LAYOUT_ONE_TOP, LAYOUT_ONE_MIDDLE, LAYOUT_TWO_TOP, LAYOUT_TWO_MIDDLE, LAYOUT_PAUSED_TOP, LAYOUT_PAUSED_MIDDLE } LayoutLines;
uint8_t window_layout_lines(LayoutLines which, uint16_t at);
EdgesRef window_layout_edges(int paused, uint16_t at);
uint16_t window_layout_limit(int split);
/* a band list by its selector, 0-ended: up to `max` entries into `out`, their count returned */
int window_band_list(uint8_t select, WindowBandEntry *out, int max);
/* a preset's three bands */
void window_band_preset(int paused, uint8_t index, WindowBandEntry out[3]);
uint8_t window_oscillator_rate(uint8_t level);          /* level 1-4 */
/* the fixed colour bands' limits: the value that stops a step, and the floor */
typedef struct { uint8_t stop, floor; } BandLimits;
BandLimits window_top_band_limits(uint8_t at);
BandLimits window_low_band_limits(int raised);
uint8_t window_level_threshold(int at);                 /* the level steps' thresholds by byte offset, $FF-ended */
/* the race's setup */
void window_preset_edges(EdgesKind which, uint8_t out[4]); /* normal, view, top view, bottom view, split */
uint8_t window_record_pair_byte(int two_players, unsigned at);  /* the records' pairs, $FF-ended */
uint8_t window_record_block_byte(int block, unsigned at);       /* one player's eight 32-byte blocks */
uint8_t window_record_last_byte(unsigned at);                   /* and its last block's $40 bytes */
WindowBandEntry window_start_band(int i);               /* the four bands a race starts with */
/* an edge list by its selector (0, 2, ... $0C): its first row and its pairs to the $FF, up to `max` */
int window_edge_list(uint16_t select, int *first_row, uint8_t pairs[][2], int max);
uint16_t window_level_word(int at);                     /* the fixed colour's table, words to $FFFF; at: bytes */
uint8_t time_gate_bonus(uint8_t course_class, uint8_t stage);
uint8_t window_top_band_start(uint8_t at);              /* by the car (at: car * 2) */

#endif
