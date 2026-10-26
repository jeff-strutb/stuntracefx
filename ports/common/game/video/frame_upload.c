/*
 * frame_upload.c: the uploads made while the screen is blanked between
 * frames, one stage a frame.
 *
 * In a race the Super FX draws its picture into one of two buffers in its
 * RAM while the last one is shown; the stages copy the finished buffer's
 * halves to the graphics store, with the rows of bank $7F that go with
 * them, and when one is complete point layer 1 at it and set layer 2's
 * scroll to match ($0D81 says which: -1 the first, 1 the second). The
 * split screen of the menus and the two-player screens has its own
 * stages. A stage with nothing to copy sends the sprites and the palette.
 */
#include <stdlib.h>

#include "game.h"
#include "video.h"

#define upload_stage    W16(0x0D7D)     /* the race screen's next stage */
#define split_stage     W8(0x0D23)      /* the split screen's */
#define buffer_finished W16(0x0D7F)     /* nonzero: the second buffer is the finished one */
#define frames_waiting  W8(0x0D28)      /* frames since the game's last; 0 when the sprites are sent */

static void layer_tiles(int l, uint16_t word)
{
    video_screen()->layer[l].tiles = word;
}

static void layer_map(int l, uint16_t word)
{
    VideoLayer *L = &video_screen()->layer[l];

    L->map = word;
    L->map_wide = L->map_tall = 0;
}

/* The whole palette, noted at $0D83. */
static void palette_upload(void)
{
    W16(0x0D83) = 0xFD65;
    video_colours_all();
}

/* The sprites, if a frame of the game's is waiting (and the HUD's panels
   are not moving), then the palette. */
static void sprites_and_palette(void)
{
    if (frames_waiting) {
        frames_waiting = 0;
        if (!W8(0x0D54))
            video_sprites_from(0x0375, 0x0220);
    }
    palette_upload();
}

/* A buffer complete: layer 1's tiles and map at it, layer 2 scrolled for
   it, the gradient's colours for it; the game's frame counted done. */
static void buffer_shown(uint16_t colours, uint8_t band, uint16_t tiles, uint16_t map, uint16_t shown)
{
    W16(0x1348D) = colours;
    layer_tiles(0, tiles);
    layer_tiles(1, 0x6000);
    layer_map(0, map);
    bg2_scroll_from_table(band);
    W16(0x0D81) = shown;
    W8(0x0000) = 0;
    frames_waiting--;
}

/* The rows of a picture (18 of them) from Super FX RAM when the finished
   buffer is the one `want` names; then the tiles of layers 1 and 2 at
   `tiles` (layer 1 by its low four bits) when given. */
static void picture_rows(int want, uint16_t next, uint16_t from, uint16_t to, uint8_t tiles)
{
    if (!buffer_finished != !want)
        return;
    upload_stage = next;
    video_rows_from_gsu_ram(from, to, 0x12);
    W8(0x0357) = 1;
    W8(0x0000) = 0;
    if (tiles) {
        layer_tiles(0, (uint16_t)((tiles & 0x0F) << 12));
        layer_tiles(1, (uint16_t)((tiles >> 4) << 12));
    }
}

/* 17 rows of 26 map entries from bank $7F ($93D7, rows $40 bytes apart)
   to the store from $5C00, a row of 32 apart; layer 2 scrolled for the
   top band. */
static void map_rows_to_5c00(void)
{
    int row;

    for (row = 0; row < 17; row++)
        video_store_write((uint16_t)(0x5C00 + 0x20 * row), &wram[0x193D7 + 0x40 * row], 52);
    bg2_scroll_from_table(0);
}

/* The split screen's picture from bank $7F ($9BD7) to `word`, layer 1's
   tiles at 0 and layer 2's at $7000, colour 0 the dark blue $1CC4. */
static void split_picture(uint16_t word)
{
    static const uint8_t blue[2] = {0xC4, 0x1C};

    video_from_bank_7f(0x1680, 0x9BD7, word);
    layer_tiles(0, 0x0000);
    layer_tiles(1, 0x7000);
    split_stage = 0x10;
    video_colours(0, blue, 1);
    W8(0xFD65) = blue[0], W8(0xFD66) = blue[1];
    palette_animate = 0;
}

static void split_screen_stage(void)
{
    switch (split_stage) {
    case 0x00:
        break;
    case 0x02:
        map_rows_to_5c00();
        /* fall through */
    case 0x08:
        video_sprites_from(0x0375, 0x0220);
        bg2_scroll_from_table(0);
        palette_upload();
        break;
    case 0x04:                          /* the pictures from Super FX RAM */
        video_from_gsu_ram(0x0A00, 0x4000, 0x6C00);
        video_from_gsu_ram(0x0140, 0x4600, 0x6240);
        video_from_gsu_ram(0x0140, 0x4800, 0x6340);
        split_stage = 0;
        break;
    case 0x0A:
        video_gsu_picture();
        video_sprites_from(0x0375, 0x0220);
        /* fall through */
    case 0x06:
        palette_upload();
        break;
    case 0x0C:
        split_picture(0x0000);
        break;
    case 0x0E:
        split_picture(0x0B40);
        break;
    case 0x10:
        video_from_bank_7f(0x0800, 0x93D7, 0x7800);
        break;
    case 0x12:
        video_from_bank_7f(0x0C00, W16(0x10A0), 0x6600);
        split_stage = 0x02;
        break;
    default:
        abort();                        /* no other stage is set */
    }
}

/* The stage's uploads: the split screen's while $0D22 is set, else the
   race screen's. */
void nmi_frame_upload(void)
{
    if (W8(0x0D22)) {
        split_screen_stage();
        return;
    }
    switch (upload_stage) {
    case 0x00:                          /* a buffer's first half, when it is the finished one */
    case 0x06:
        if (!buffer_finished == (upload_stage == 0x00)) {
            sprites_and_palette();
            break;
        }
        if (upload_stage == 0x00) {
            upload_stage = 0x02;
            video_from_gsu_ram(0x2200, 0x2C00, 0x0000);
            video_from_bank_7f(0x0200, 0xE3D7, 0x5BC0);
        } else {
            upload_stage = 0x08;
            video_from_gsu_ram(0x2200, 0xB800, 0x3400);
            video_from_bank_7f(0x0200, 0xE817, 0x5DE0);
        }
        break;
    case 0x02:
        upload_stage = 0x04;
        video_from_gsu_ram(0x2200, 0x4E00, 0x1100);
        video_from_bank_7f(0x0240, 0xE5D7, 0x5CC0);
        W8(0x0357) = 1;
        break;
    case 0x04:
        upload_stage = 0x06;
        video_from_gsu_ram(0x2400, 0x7000, 0x2200);
        buffer_shown(W16(0x0D9A), 0, 0x0000, 0x7800, 0xFFFF);
        break;
    case 0x08:
        upload_stage = 0x0A;
        video_from_gsu_ram(0x2200, 0xDA00, 0x4500);
        video_from_bank_7f(0x0240, 0xEA17, 0x5EE0);
        W8(0x0357) = 1;
        break;
    case 0x0A:
        upload_stage = 0x00;
        video_from_gsu_ram(0x2400, 0x9400, 0x2200);
        buffer_shown(W16(0x0D9C), 2, 0x2000, 0x7C00, 0x0001);
        break;
    case 0x0C: picture_rows(1, 0x10, 0x2C00, 0x0000, 0); break;
    case 0x10: picture_rows(0, 0x14, 0x9400, 0x0B40, 0x40); break;
    case 0x14: picture_rows(1, 0x18, 0x2C00, 0x2000, 0); break;
    case 0x18: picture_rows(0, 0x0C, 0x9400, 0x2B40, 0x42); break;
    case 0x0E: case 0x12: case 0x16: case 0x1A:
        break;
    case 0x1E:                          /* the second buffer's first half, as stage 6 */
        if (buffer_finished) {
            sprites_and_palette();
            break;
        }
        upload_stage = 0x08;
        video_from_gsu_ram(0x2200, 0xB800, 0x3400);
        video_from_bank_7f(0x0200, 0xE817, 0x5DE0);
        break;
    case 0x20:
        split_screen_stage();
        break;
    default:
        abort();                        /* no other stage is set */
    }
}
