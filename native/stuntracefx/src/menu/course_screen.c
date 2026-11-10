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

static void oam_word(uint16_t offset, uint16_t w)
{
    OamByteSet(offset, (uint8_t)w);
    OamByteSet((uint16_t)(offset + 1), (uint8_t)(w >> 8));
}

/*
 * @routine 0B:FAE5 MenuOamFromList
 * WHAT IT DOES: Copies words from a list at $0B:FCA2 into the sprite table
 * from an offset, until a $FFFF word.
 */
static void oam_from_list(uint16_t offset, uint16_t list)
{
    uint16_t w;

    while ((w = cart16(MENU_BANK, (uint16_t)(SPRITE_LISTS + list))) != 0xFFFF) {
        oam_word(offset, w);
        offset = (uint16_t)(offset + 2);
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
    uint16_t w = ram_code_7f16((uint16_t)(0x0B96 + 2 * g.course));
    uint16_t t = (uint16_t)(w & 0xFFFC);
    uint16_t src = (uint16_t)((uint16_t)((uint16_t)(t >> 8 | t << 8) << 1) + ((w & 3) << 7));
    int i;

    for (i = 0; i < 4; i++, src = (uint16_t)(src + 0x0200))
        race_art_write(to[i], &fx_ram[0x3000 + src], 0x80);
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
    static const uint16_t groups[4][2] = {{0x0000, 0x0000}, {0x0200, 0x0154}, {0x0100, 0x00A8}, {0x0210, 0x0162}};
    uint16_t x;
    int i;

    SoundPairFromTable();
    for (x = 0; x != 0x0200; x = (uint16_t)(x + 2))
        oam_word(x, 0xE0E0);
    for (x = 0; x != 0x0020; x = (uint16_t)(x + 2))
        oam_word((uint16_t)(0x0200 + x), 0x5555);
    for (i = 0; i < 4; i++)
        oam_from_list(groups[i][0], groups[i][1]);
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
