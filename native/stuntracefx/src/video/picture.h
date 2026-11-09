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
    int32_t scroll_x, scroll_y;
} Background;

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
} Picture;

extern Picture pic;

/* The menu screens' tile sets and maps. */
typedef struct {
    nshvc_tiles bg1_4, bg2_4, bg3_2, sprites;
    nshvc_map bg1, bg2, bg3;
} MenuVideo;

extern MenuVideo menu_video;

void video_init(void);
void video_draw(void);

#endif
