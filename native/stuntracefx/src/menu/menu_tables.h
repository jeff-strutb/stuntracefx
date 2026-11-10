/*
 * menu_tables.h: the menu screens' per-line tables: which set the screen
 * uses and which of its channels are on, as the screen's own code says.
 */
#ifndef STUNTRACEFX_MENU_TABLES_H
#define STUNTRACEFX_MENU_TABLES_H

#include "menu/actors.h"

enum { MENU_LINES_TITLE, MENU_LINES_PREVIEW, MENU_LINES_CHAMPION };

/* The menus' tables by line, where the cartridge's data block holds them
   (menu_tables_at(): laid out as the original's $7F:0800-$3FFF, so its
   places are the data's own addresses, named once here). Each table is runs
   of a line count and a value: one byte, two, or (the indirect ones) the
   address of a word that holds it. */
enum {
    MT_BLINK_COLOURS = 0x0DA8,          /* colour $D9's two blink colours */
    MT_CHAMPION_SCROLL_X = 0x12D5,      /* the championship: BG2's horizontal scroll (2-byte runs) */
    MT_CHAMPION_SCROLL_Y = 0x12E2,      /* BG2's vertical scroll (indirect runs) */
    MT_CHAMPION_MAIN = 0x12F2,          /* the main screen's layers */
    MT_CHAMPION_WINDOW = 0x12FD,        /* window 1's right edge */
    MT_CHAMPION_WINDOW_B = 0x1308,      /* window 1's right edge, the second set */
    MT_TITLE_MAIN_A = 0x1315,           /* the title: the main screen's layers, three sets */
    MT_TITLE_MAIN_B = 0x131A,
    MT_TITLE_MAIN_C = 0x131F,
    MT_TITLE_WINDOW = 0x1324,           /* window 1's edges (2-byte runs) */
    MT_PREVIEW_SCROLL_X = 0x133A,       /* the course menu: BG2's horizontal scroll (indirect runs) */
    MT_PREVIEW_COLOUR = 0x1341,         /* the colour maths' prevent and clip */
    MT_PREVIEW_MASK = 0x1348,           /* the layers the windows mask */
    MT_PREVIEW_WINDOW = 0x1353          /* window 1's left edge */
};

/* a byte of the tables, by its place */
uint8_t *menu_table_byte(uint16_t at);

/* `channels`: the per-line tables on, one bit each (bits 3-7) */
void menu_lines_set(int set, uint8_t channels);

/* the course menu's graphics, loaded a step a frame after the title */
void menu_load_screen_open(void);
void menu_load_step(Actor *a);
void menu_grid_load(uint16_t first_tile, int page);
void menu_frames_vram_tick(void);

/* one frame of the championship's selection animation, in BG2's tiles */
void menu_champion_frames_tile(uint16_t step);

#endif
