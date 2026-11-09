/*
 * menu.h: the menu screens as the rest of the game calls them, and the
 * state their handlers share.
 */
#ifndef STUNTRACEFX_MENU_H
#define STUNTRACEFX_MENU_H

#include <stdint.h>

#include "menu/actors.h"

/* Screens that wait for the player are run a frame at a time: called once
   per update, they return nonzero on the update they finish. */
int MenuScreen0Run(void);
int CourseMenuRun(void);
int ChampionScreenRun(void);
void MenuCourseScreenSetup(void);       /* wanted by the race (two-player HUD setup) */

/* the race has ended: back to the course menu, or (0) to the start of the game */
void session_race_ended(int to_course_menu);

/* one handler for one object, once a frame */
void menu_actor_run(Actor *a);

/* the animated sprite frames unpacked for the course menu */
const uint8_t *menu_frame_data(uint16_t address);

/* what a menu screen's objects share, by the original's addresses */
typedef struct {
    uint16_t blink_counter;             /* $11DD: steps once per frame on 3D screens */
    uint16_t press_current;             /* $118F: the press being worked on */
    uint16_t press_list[2][8];          /* $1195, $11B5: presses queued a frame at a time */
    uint16_t press_count[2];            /* $1191, $1193, in entries */
    uint16_t mode_previous;             /* $18C5 */
    uint16_t row;                       /* $18EB: settings row 0-2 */
    uint16_t position;                  /* $18ED: initials position 0-2 */
    uint16_t countdown;                 /* $18F5 */
    uint16_t fixed_colour;              /* $18F9 */
    uint16_t animated_offset;           /* $18F7: the animated tiles' offset, $400 a frame */
    uint16_t slide_top, slide_low;      /* $18BB, $18BD: the positions the per-line table reads */
    uint16_t slide_top_next, slide_low_next; /* $18BF, $18C1: the same, written during the frame */
    uint16_t window_mode, window_size;  /* $1905, $1907 */
    uint16_t blink_count, blink_phase;  /* $1901, $18FF: colour $D9's blink */
    uint16_t hold_up, hold_down;        /* $190B, $190D */
    uint16_t leader_timer, leader_step; /* $190F, $1911: where the place marker's blink stands */
    uint16_t panel_rise;                /* $1921 */
    uint16_t panel_b1919, panel_b191b, panel_b191d, panel_b191f, panel_b1925, panel_b1927;
    uint8_t blink_on;                   /* $18B9 */
    uint8_t shown_3d;                   /* a handler showed a race frame this run */
} MenuState;

extern MenuState menu;

/* the per-line changes the current screen has switched on */
void menu_lines_send(void);

void SelectionTables18CB(void);
void SelectionCommit(void);
void SelectionSave(void);
void CarSelectTwoPlayers(void);
void MenuCursorFourWay(void);
void SpeedGaugeUpdate(uint16_t offset);
void SpeedGauge2Update(uint16_t offset);

/* the course screens' records pages */
void SpeedRecordsScreen(void);
void StuntRecordsScreen(void);
void BattleRecordsScreen(void);
void AllRecordsScreen(void);
void record_rows_first_times(uint8_t page_offset);
void record_rows_group_firsts(uint8_t page_offset);

#endif
