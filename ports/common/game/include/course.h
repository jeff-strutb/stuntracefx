/*
 * course.h: the course select's cursors, the course screens' states and
 * the records panel (race/course.c). Routines that reach the menu's
 * sprites, the records' loading or the zoom into a course are not here.
 */
#ifndef SRFX_COURSE_H
#define SRFX_COURSE_H

#include <stdint.h>

/* The course the menu's selections name (CourseFromSelection; the long
   form for other banks is the same): the row times 4 plus the place, or
   the row plus $14 with the override set. Stored as the course number and
   returned. */
uint8_t course_from_selection(void);

/* The course cursor, by the place it is on, moving it by the pressed
   buttons (each move with the move sound). */
void course_cursor_bottom_left(void);       /* place 0 */
void course_cursor_bottom_right(void);      /* place 1 */
void course_cursor_top_left(void);          /* place 2 */
void course_cursor_top_right(void);         /* place 3 */
/* The same for the places where a fourth course can open: Up opens the
   place when the fourth course's bits are all set in the save. */
void course_cursor_top_left_open(void);
void course_cursor_top_right_open(void);
/* The place's handler (places 0-3 only), and the fade's variant of it,
   with the open places. */
void course_cursor_step(void);
void course_cursor_fade_dispatch(void);

/* Sets up the course window's zoom: the scaled edges and their bases from
   the ROM table at $03:FC11 + x, their targets from $03:FB8E + y, the
   blend factor 0 with its limit 15, then the window table. */
void course_window_zoom_setup(uint16_t x, uint16_t y);

/* Stores v in the four level column bytes at $0F05 + x. */
void level_columns_set4(uint8_t v, uint16_t x);

/* The time-up select's flashing sprite position. */
void time_up_select_draw(void);
/* The time-up select's cursor, by its buttons. */
void time_up_cursor_step(void);

/* The records panel's states: opening, reopening, closing. */
void course_records_opening(void);
void course_records_reopen(void);
void course_records_closing(void);

/* The course screens' wipes of the scroll (the three counts). */
void course_screen_wipe_out(void);
void course_screen_wipe_in(void);

/* The preview's counts cleared and its table copied into place. */
void course_preview_reset(void);

/* The course screens' start states: each sets state 2 (Speed Trax also
   clears $10D8); the nothing routine leaves everything as it is. */
void course_screen_start_speed(void);
void course_screen_start_all(void);
void course_screen_start_battle(void);
void course_screen_start_stunt(void);
void course_screen_nothing(void);

/* The window's zoom out and in, one step each. */
void course_screen_zoom_out(void);
void course_screen_zoom_in(void);

/* The course screens' state 8. */
void course_screen_state8(void);

#endif
