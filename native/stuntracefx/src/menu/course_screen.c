/*
 * course_screen.c: the two-player race screen's set-up from the course
 * menu's side: its sprites, the course's small picture and the gauges' and
 * map's places.
 */
#include <stdint.h>

#include "audio/sound.h"
#include "data/menu_tables.h"
#include "menu/menu.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"
#include "video/video.h"

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
static void sprites_from_list(int first, MenuSpriteList list)
{
    uint16_t w;
    int k = 0;

    while ((w = menu_course_sprite_word(list, k)) != 0xFFFF) {
        sprite_word(first + k / 2, k & 1, w);
        k++;
    }
}

/* The same into the sprites' X ninth bits and sizes from sprite `first`:
   each word eight sprites' (the original's high table, two bytes). */
static void sprite_highs_from_list(int first, MenuSpriteList list)
{
    uint16_t w;
    int k = 0;

    while ((w = menu_course_sprite_word(list, k++)) != 0xFFFF) {
        sprite_high_set(first, (uint8_t)w);
        sprite_high_set(first + 4, (uint8_t)(w >> 8));
        first += 8;
    }
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
    sprites_from_list(0, MENU_COURSE_SPRITES_0);
    sprite_highs_from_list(0, MENU_COURSE_SPRITES_0_SIZES);
    sprites_from_list(64, MENU_COURSE_SPRITES_64);
    sprite_highs_from_list(64, MENU_COURSE_SPRITES_64_SIZES);
    OamDmaAll();
    menu_picture_unpack(MENU_PIC_COURSE_PICTURES, fx_ram + FX_UNPACK_AREA, 0xC000);
    race_course_picture_write();
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
