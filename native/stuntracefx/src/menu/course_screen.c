/*
 * course_screen.c: the two-player race screen's set-up from the course
 * menu's side: its sprites, the course's small picture and the gauges' and
 * map's places.
 */
#include <stdint.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "data/unpack.h"
#include "menu/menu.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"
#include "video/video.h"

/* the sprite-table word lists, in bank $0B */
enum { MENU_BANK = 0x0B, SPRITE_LISTS = 0xFCA2 };

/* where the chip unpacks a picture in its RAM, and the course pictures'
   places in it by course (a table in the data block, as the original's) */
enum { CHIP_PICTURE = 0x3000, COURSE_PICTURE_PLACES = 0x0B96 };

/* A sprite's place (X low byte, Y high) or look (character low, attributes high) from a word. */
static void sprite_word(int n, int look, uint16_t w)
{
    if (look) {
        sprite_tile_set(n, (uint8_t)w);
        sprite_attributes_set(n, (uint8_t)(w >> 8));
    } else {
        sprite_x_set(n, (uint8_t)w);
        sprite_y_set(n, (uint8_t)(w >> 8));
    }
}

/*
 * @routine 0B:FAE5 MenuOamFromList
 * WHAT IT DOES: Copies words from a list at $0B:FCA2 into the sprites from
 * sprite `first`, a place then a look each, until a $FFFF word.
 */
static void sprites_from_list(int first, uint16_t list)
{
    uint16_t w;
    int k = 0;

    while ((w = cart16(MENU_BANK, (uint16_t)(SPRITE_LISTS + list))) != 0xFFFF) {
        sprite_word(first + k / 2, k & 1, w);
        k++;
        list = (uint16_t)(list + 2);
    }
}

/* The same into the sprites' X ninth bits and sizes from sprite `first`:
   each word eight sprites' (the original's high table, two bytes). */
static void sprite_highs_from_list(int first, uint16_t list)
{
    uint16_t w;

    while ((w = cart16(MENU_BANK, (uint16_t)(SPRITE_LISTS + list))) != 0xFFFF) {
        sprite_high_set(first, (uint8_t)w);
        sprite_high_set(first + 4, (uint8_t)(w >> 8));
        first += 8;
        list = (uint16_t)(list + 2);
    }
}

/*
 * @routine 7F:0B56 CoursePictureToVram
 * @routine 7F:0BCE VramWriteRowFromGsuRam
 * WHAT IT DOES: Four 64-word rows of the course's picture, as unpacked, to
 * the sprites' characters $48, $58, $4C and $5C (four each), from the
 * offset the table at $7F:0B96 gives for the course.
 */
static void course_picture_to_art(void)
{
    static const unsigned to[4] = { RACE_ART_SPRITES + 0x48, RACE_ART_SPRITES + 0x58, RACE_ART_SPRITES + 0x4C, RACE_ART_SPRITES + 0x5C };
    uint16_t w = ram_code_7f16((uint16_t)(COURSE_PICTURE_PLACES + 2 * g.course));
    uint16_t t = (uint16_t)(w & 0xFFFC);
    uint16_t src = (uint16_t)((uint16_t)((uint16_t)(t >> 8 | t << 8) << 1) + ((w & 3) << 7));
    int i;

    for (i = 0; i < 4; i++, src = (uint16_t)(src + 0x0200))
        race_art_write(to[i], &fx_ram[CHIP_PICTURE + src], 0x80);
}

/*
 * @routine 0B:FA24 MenuCourseScreenSetup
 * WHAT IT DOES: Sets up the two-player race screen: the two players' sounds,
 * every sprite hidden, four sprite groups from the lists at $0B:FCA2, the
 * table shown; the course's picture unpacked ($1D:D970) and its rows to the
 * art; sprites from the art at $6000; the raster stage 8, upload stage $0C,
 * the gauges' and map's places; then the gradient.
 */
void MenuCourseScreenSetup(void)
{
    int i;

    SoundPairFromTable();
    for (i = 0; i < 128; i++) {         /* every sprite at (224, 224), character $E0, attributes $E0 */
        sprite_word(i, 0, 0xE0E0);
        sprite_word(i, 1, 0xE0E0);
    }
    for (i = 0; i < 128; i += 4)        /* every X ninth bit set, every sprite small */
        sprite_high_set(i, 0x55);
    /* four sprite groups: the first half's from sprite 0, the second's from 64 */
    sprites_from_list(0, 0x0000);
    sprite_highs_from_list(0, 0x0154);
    sprites_from_list(64, 0x00A8);
    sprite_highs_from_list(64, 0x0162);
    OamDmaAll();
    unpack((uint32_t)0x1D << 16 | 0xD970, fx_ram + 0x3000, 0xC000);
    course_picture_to_art();
    sprites_use_tiles(race_video.tiles, 0x200, 0x300);
    g.raster_chain = RASTER_TWO_PLAYERS;    /* the original's stage $08 */
    g.fx_upload_stage = 0x000C;
    g.gauge_place[0] = 0xB0;            /* $0E0E */
    g.gauge_place[2] = 0x1C;            /* $0E10 */
    g.gauge_place[4] = 0x18;            /* $0E12 */
    g.gauge_place[6] = 0x5A;            /* $0E14 */
    g.hud_map_origin_x[0] = 0xB7;       /* $0E09 */
    g.hud_map_y_b = 0x3C;               /* $0E0C */
    g.gauge_place[1] = 0x10;            /* $0E0F */
    g.gauge_place[3] = 0x84;            /* $0E11 */
    g.gauge_place[5] = 0x68;            /* $0E13 */
    g.gauge_place[7] = 0xC2;            /* $0E15 */
    g.hud_map_origin_x[1] = 0x45;       /* $0E0A */
    g.hud_map_y_a = 0xA4;               /* $0E0D */
    GradientSetup();
}
