/*
 * picture.h: what the screen shows. The game's code sets these fields as
 * the original's screens set theirs; video_draw hands them to the engine
 * once per picture. Tile sets and maps are engine objects by role.
 */
#ifndef STUNTRACEFX_PICTURE_H
#define STUNTRACEFX_PICTURE_H

#include <stdint.h>

#include "nshvc/nshvc.h"

/* on the main or sub screen: one bit a layer */
enum { SHOW_BG1 = 1, SHOW_BG2 = 2, SHOW_BG3 = 4, SHOW_SPRITES = 16 };

typedef struct {
    nshvc_tiles tiles;
    nshvc_map map;
    nshvc_draw_list draw_list;          /* in place of tiles and map: the 3D layer */
    int32_t scroll_x, scroll_y;
} Background;

/* What the race screen shows beyond the layers: per half and per line. */
typedef struct {
    uint8_t on;                         /* a race screen is up */
    uint8_t split;                      /* two halves, one above the other */
    uint8_t course_screen;              /* the course screen before a race */
    uint16_t gradient_start[2];         /* colour numbers into the gradient block */
    int32_t backdrop_x[2], backdrop_y[2]; /* BG2's scroll for each half */
    int32_t wide_x, wide_y;             /* the full window's hills: their scroll on the wide map */
    uint16_t sky_offset;                /* and the sky's: byte offset into the day and night lists at line 32 */
    uint8_t panel[2];                   /* the finished player's panel shows in the half */
    uint16_t colours_kept[2][3];        /* colours $E1-$E3 in each half */
    uint16_t colour_0[2];               /* colour 0 in each half */
    uint16_t fixed[2];                  /* the fixed colour in each half, in BGR */
} RacePicture;

typedef struct {
    uint8_t blank, brightness;
    uint8_t mode, bg3_priority;
    Background bg[3];
    uint8_t main_screen, sub_screen;
    nshvc_blend blend;
    nshvc_windows windows;
    uint16_t palette[256];
    uint16_t palette_saved[256];
    nshvc_sprite sprites[128];
    uint32_t sprite_count;
    uint8_t line_tables;                /* LINES_ bits */
    RacePicture race;
} Picture;

/* tables of per-line changes the menu screens turn on and off, by channel */
enum { LINES_TM_C = 0x10, LINES_TM_B = 0x20, LINES_WINDOW = 0x40, LINES_TM_A = 0x80 };

extern Picture pic;

/* The menu screens' tile sets and maps. */
typedef struct {
    nshvc_tiles tiles_a, tiles_b, tiles_text, sprites;  /* 0 and $2000 in the original's layout, BG3's, the sprites' */
    /* the maps: four 32 x 32 pages, seen one or two at a time */
    nshvc_map page_0, pages_0_1, pages_1_2, pages_2_3;
    nshvc_map text;
} MenuVideo;

/* a map page, 32 x 32 words, to every map that shows it */
void menu_page_write(int page, const void *words);

extern MenuVideo menu_video;

void video_init(void);
void video_draw(void);

#endif
