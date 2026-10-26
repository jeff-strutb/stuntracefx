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

#endif
