/*
 * race_tables.h: the race's and the HUD's tables from the cartridge, as
 * typed data (plan section 3, Data). Where each table is in the cartridge
 * is written once, in race_tables.c; race/, hud/ and math/ read them only
 * through these.
 *
 * A table's index is what the game keeps for it: an element, or, where a
 * step keeps a byte offset into a word table (the HUD's animation steps,
 * the course screen's boxes), that offset. Each says which. Reads past a
 * table's end give the cartridge's next bytes, as the original's did.
 */
#ifndef STUNTRACEFX_RACE_TABLES_H
#define STUNTRACEFX_RACE_TABLES_H

#include <stdint.h>

/* ---- math ---- */
uint8_t trig_sine(uint8_t angle);       /* the byte sine and cosine tables */
uint8_t trig_cosine(uint8_t angle);

/* ---- the attract demos' scripts, by the byte offset of a script's field ---- */
uint8_t demo_script_byte(uint16_t at);
uint16_t demo_script_word(uint16_t at);

/* ---- the course screen ---- */
typedef enum { COURSE_LIST_STUNT, COURSE_LIST_BATTLE } CourseList;
uint8_t course_in_list(CourseList list, uint16_t slot);         /* the course at a menu slot */
uint16_t course_zoom_box_word(uint16_t box, int i);             /* a zoom box's four words; box: its byte offset */
uint16_t course_zoom_target_word(uint16_t target, int i);       /* the zoom target's four words; target: its byte offset */
uint16_t course_flash_sprite_place(uint16_t stage);             /* the flashing cursor's place by the menu stage */
uint16_t course_palette_colour(int i);                          /* the race's 256 colours */

/* ---- the race load's packed art blocks ---- */
typedef enum {
    RACE_BLOCK_COURSE_ART, RACE_BLOCK_SHARED_ART, RACE_BLOCK_SPLIT_ART,
    RACE_BLOCK_VIEWS_SPEED, RACE_BLOCK_VIEWS_TEST_RUN, RACE_BLOCK_VIEWS_BATTLE, RACE_BLOCK_VIEWS_STUNT,
    RACE_BLOCK_MODE_ART_SPEED, RACE_BLOCK_MODE_ART_BATTLE, RACE_BLOCK_MODE_ART_STUNT
} RaceBlock;
/* the block unpacked into `out` (`room` bytes); returns its length from its transfer record */
uint32_t race_block_unpack(RaceBlock block, uint8_t *out, uint32_t room);

/* ---- the race's times ---- */
uint8_t course_lap_time(uint8_t entry, int lap);                /* the BCD seconds for lap 0-2; entry: its byte offset (CourseIndex12) */
uint8_t class_time(uint8_t course_class);                       /* the time override's seconds by class */
uint8_t stunt_time(uint8_t entry);                              /* Stunt Trax's seconds; entry: (course - $0C) * 4 + car */

/* ---- the race's phases and tallies ---- */
uint16_t start_time_size(uint16_t step);                        /* the start's time digits' size by the race timer */
uint8_t banner_size(uint16_t step);                             /* a lap banner's size by its timer */
uint16_t tally_glyph(uint16_t i);                               /* the tally's glyphs and where each one's x ends */
uint16_t tally_x_end(uint16_t i);
uint8_t stunt_count_step(uint16_t step);                        /* the stunt count's steps by the banner timer */

/* ---- the results ---- */
uint16_t results_car_picture(uint16_t car);                     /* the car's picture for the results' overlay */
uint8_t results_car_choice(uint16_t i);
uint16_t champion_palette_colour(int i);
uint8_t results_totals_word_letter(int i);

/* ---- the HUD ---- */
uint8_t hud_sprite_list_byte(uint16_t at);                      /* the sprite lists' bytes; at: a list's byte offset */
uint16_t split_panel_sprite_word(uint16_t at);                  /* the split screen's panel and map sprites; at: byte offset */
uint16_t split_map_sprite_word(uint16_t at);
uint8_t split_sprite_size(uint16_t i);
uint8_t class_name_tile(uint16_t at);                           /* the class names' tiles; at: byte offset */
uint16_t hud_big_digit_glyph(uint16_t at);                      /* the digits' glyphs; at: the digit's byte offset (digit * 2) */
uint16_t hud_small_digit_glyph(uint16_t at);
uint16_t hud_digit_grow_size(uint16_t step);                    /* by the HUD's animation step (a word table's byte offset) */
uint16_t hud_frame_x_offset(uint16_t step);
uint16_t hud_frame_tail_a_width(uint16_t step);
uint16_t hud_frame_tail_a_step(uint16_t step);
uint16_t hud_frame_tail_b_width(uint16_t step);
uint16_t hud_frame_tail_b_step(uint16_t step);
uint16_t hud_frame_middle_width(uint16_t at);                   /* the time frame's middle; at: byte offset */
uint16_t hud_frame_middle_step(uint16_t at);
uint8_t hud_laps_to_go_height(uint16_t step);

/* the countdown digit's shape: its size, its scale and its place */
typedef struct { uint16_t size, scale, x, y; } HudShape;
HudShape hud_countdown_shape(uint16_t n);

/* the glyph sprites: where each starts, its glyph; the grow steps; the messages */
uint16_t glyph_sprite_start_x(uint16_t i);
uint16_t glyph_sprite_start_glyph(uint16_t i);
uint16_t glyph_grow_size(uint16_t at);                          /* at: the grow step's byte offset */
uint16_t glyph_grow_scale(uint16_t at);
uint16_t glyph_grow_x_offset(uint16_t at);
uint16_t glyph_grow_y_offset(uint16_t at);
uint16_t hud_message_letters(uint16_t index);                   /* message `index` (a byte offset into the list): its letters */
uint16_t hud_message_glyph(uint16_t index, uint16_t k);         /* its letter k's glyph word, 0 a space */

#endif
