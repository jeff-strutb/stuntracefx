/*
 * records.h: the saved records in the game pak RAM (save/records.c).
 */
#ifndef SRFX_RECORDS_H
#define SRFX_RECORDS_H

#include "game.h"

/* ---- save/records.c ---- */

/* The course's records (its class at $0D4A) unpacked for the records
   screens: twelve times into record_times, nine name bytes at $10CF. */
void records_unpack_course(void);
void records_unpack_course_totals(void);    /* then each group's total */
void records_unpack_course_running(void);   /* then each group's running totals */

/* Loads the records of the course at $0E3D for the course screens: the
   special courses ($0C-$0F) and the normal ones (below $0C); none above. */
void records_load_for_course(void);
void records_load_course(void);             /* a normal course (below $0C) */
void records_load_course_long(void);
void records_load_special_course(void);     /* a special course ($0C-$0F) */
void records_load_special_course_long(void);

/* One character of the records panel, drawn at its tilemap place. */
void preview_row_draw_stunt(void);          /* Stunt Trax's records */
void preview_row_draw_all(void);            /* the other courses' records */

/* The time-up run's end: its time against the course's record. */
void time_up_record_update(void);

/* Stunt Trax's score for the car ($0D89) against the course's three best. */
void stunt_record_time(void);

/* ---- the save's checks (save/records.c) ---- */

/* SaveValidate and what it runs. The save is at pak $70 throughout. */
uint8_t save_record_sum_b(uint8_t x);       /* the sum of the record at $FC5E + x */
uint8_t save_sums_records_b(void);          /* sums of the first three records; returns the fourth's */
uint8_t save_records_check_a(void);         /* four records at $FC4A; returns the last first byte */
uint8_t save_records_check_b(void);         /* three records at $FC5E */
void save_blocks_check(void);               /* blocks 12-15 */
void save_tables_check(void);               /* the three save tables */
void save_blocks_validate(void);            /* blocks 0-11 */
void save_blocks_fourth_bad(uint8_t blk);   /* a block's reset then the rest, from blk */
void save_validate(void);                   /* the whole save */
void save_unlocks_check(void);              /* the unlock bytes' checksum, then save_validate */

/* ---- the records panel's characters and rows (save/records.c) ---- */

/* Sprite offsets y are the sprite buffer's (sprite number * 4). */
void record_name_letters(uint16_t letters, uint16_t y);         /* three letters, + $0A each */
void record_name_letters_small(uint16_t letters, uint16_t y);
void record_name_letters_large(uint16_t letters, uint16_t y);   /* large letters, two tiles each */
uint16_t record_letter_tile(uint16_t letter);                   /* the top tile of a large letter */
uint8_t record_tag_car_set(uint8_t t);                          /* the car's piece set for a tag */
void record_time_digits_large(uint16_t packed, uint16_t y);     /* a packed time as five large digits */
void record_time_digits_small(uint16_t packed, uint16_t y);     /* ... as five small digits */
void record_score_digits(uint16_t score, uint16_t y);           /* a Stunt Trax score (or PERFECT) */
/* One records row: `page` is the page's offset, `time` the time's offset
   in the group. */
void record_row_draw(uint16_t sprite, uint8_t index, uint8_t name0, uint8_t name1, uint8_t car);
void record_row1(uint8_t page, uint8_t time);
void record_row2(uint8_t page, uint8_t time);
void record_row3(uint8_t page, uint8_t time);
void record_row1_time0(uint8_t page);
void record_row2_time4(uint8_t page);
void record_row3_time8(uint8_t page);
void record_rows_group_firsts(uint8_t page);
void record_rows_first_times(uint8_t page);
/* The record times against the best: $0FA3 and the sound. */
void record_time_compare(uint16_t x);
void record_compare_time0(void);
void record_compare_time1(void);
/* A special course's checksum, and the insert of a time into its records. */
void special_records_checksum(void);
void special_record_insert(uint16_t t);

#endif
