/*
 * video.h: the picture, as the game describes it to whatever draws it.
 *
 * The game builds its screens from graphics in its own formats: tiles of
 * 2, 4 or 8 bits a pixel, maps of tile entries, 15-bit colours, a table
 * of sprites. Those are its data, and the port keeps them as they are;
 * what it does not keep is the machine they were sent to (registers, the
 * transfer channels, the beam's timing). Instead the game states what it
 * wants by meaning, here, and a platform turns the result into pixels
 * (video/render.c draws it in software for any platform that wants
 * that).
 *
 * A frame is the settings in force at its top, then changes at given
 * lines (a split screen's lower part, a sky's colours a line at a time,
 * window edges that move down the screen). Each call that changes a
 * setting takes effect from the line set by video_at_line (0 at the top
 * of a frame).
 */
#ifndef SRFX_VIDEO_H
#define SRFX_VIDEO_H

#include <stdint.h>

#define VIDEO_LINES     224
#define VIDEO_LAYERS    4               /* the tile layers; the sprites are a fifth */
#define VIDEO_SPRITES   128

/* ---- the graphics ---- */

/* The graphics store: 32 K words of tiles and maps, where the game puts
   them (a layer finds its tiles and its map by their word address). */
void video_store_write(uint16_t word, const uint8_t *data, unsigned bytes);  /* little-endian words */
void video_store_fill(uint16_t word, uint16_t value, unsigned words);
void video_store_write_bytes(uint16_t word, int high, const uint8_t *data, unsigned n);  /* one byte of each word */
const uint16_t *video_store(void);

/* The 256 colours (15 bits: blue, green, red, 5 bits each). */
void video_colours(uint8_t first, const uint8_t *data, unsigned n);  /* n colours, two bytes each */
const uint16_t *video_palette(void);

/* The sprites: the game's sprite table (128 entries of x, y, tile and
   attributes, then two bits each of x's ninth bit and size). */
void video_sprite_table(const uint8_t *table, unsigned first, unsigned bytes);
typedef struct {
    int16_t x;
    uint8_t y;
    uint16_t tile;                      /* 0-511 */
    uint8_t palette;                    /* 0-7 (colours 128 + 16 * palette on) */
    uint8_t priority;                   /* 0-3 */
    uint8_t flip_h, flip_v, large;
} VideoSprite;
void video_sprite_get(unsigned n, VideoSprite *s);

/* ---- the screen's settings, from the current line on ---- */

void video_at_line(unsigned line);

typedef struct {
    uint16_t map;                       /* its map's word address */
    uint8_t map_wide, map_tall;         /* a map of 2 x 1, 1 x 2 or 2 x 2 screens */
    uint16_t tiles;                     /* its tiles' word address */
    uint8_t large_tiles;                /* 16 x 16 */
    uint16_t scroll_x, scroll_y;
} VideoLayer;

typedef struct {
    uint8_t brightness;                 /* 0-15 */
    uint8_t blank;                      /* nothing shown */
    uint8_t mode;                       /* the layers' kind: 0-7 as the game sets it */
    uint8_t layer3_priority;            /* mode 1: layer 3 above all */
    VideoLayer layer[VIDEO_LAYERS];
    uint16_t sprite_tiles;              /* the sprites' tiles: word address, and the second half's gap */
    uint8_t sprite_gap;
    uint8_t sprite_sizes;               /* the small and large sizes, as the game picks them (0-5) */
    uint8_t main_layers, sub_layers;    /* bit n: layer n shown (bit 4 the sprites) */
    uint8_t window_left[2], window_right[2];
    uint8_t window_layers;              /* bit n: layer n clipped by the windows on the main screen */
    uint8_t window_sub_layers;
    uint8_t window_use[6];              /* each of the 4 layers, the sprites and the colour window: */
                                        /* bit 0 window 1 on, 1 inverted, 2 window 2 on, 3 inverted */
    uint8_t window_logic[6];            /* how the two combine: 0 or, 1 and, 2 xor, 3 xnor */
    uint8_t colour_clip, colour_prevent;/* where colours are cut to black / maths kept off: 0-3 */
    uint8_t colour_sub_screen;          /* add or subtract the sub screen (else the fixed colour) */
    uint8_t colour_direct;              /* mode 3 and 4: colours from the tiles */
    uint8_t colour_maths;               /* bit n: layer n mixed (bit 4 the sprites, 5 the backdrop) */
    uint8_t colour_subtract, colour_half;
    uint16_t fixed_colour;              /* 15 bits */
    uint8_t mosaic;                     /* size (bits 4-7), layers (0-3) */
} VideoScreen;

/* The settings from the current line on, to change in place. */
VideoScreen *video_screen(void);
/* A colour changed from the current line on (a gradient a line at a
   time). */
void video_colour_at_line(uint8_t n, uint16_t colour);

/* ---- the frame ---- */

/* A new frame: its settings start as the last one's ended. */
void video_frame_begin(void);
/* The settings in force at a line (for the platform's drawing). */
void video_screen_at(unsigned line, VideoScreen *s, uint16_t palette[256]);

/* ---- video/uploads.c: the game's graphics sent ---- */
void video_from_bank_7f(uint16_t n, uint16_t at, uint16_t word);
void video_from_gsu_ram(uint16_t n, uint16_t at, uint16_t word);
void video_rows_from_gsu_ram(uint16_t at, uint16_t word, uint8_t rows);
void video_wide_rows_from_gsu_ram(uint8_t rows, uint16_t at, uint16_t word);
uint16_t video_row_from_gsu_ram(uint16_t word);
void video_course_picture(void);
void video_gsu_picture(void);
void video_sprites_from(uint16_t at, uint16_t n);
void video_sprites_all(void);
void video_colours_from(uint16_t at, uint8_t first, uint16_t n);
void video_colours_all(void);

#endif
