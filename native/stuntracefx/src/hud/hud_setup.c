/*
 * hud_setup.c: the race HUD's sprites and positions set up after its
 * graphics are loaded: the sprite lists, the HUD mode's own sprites (best
 * time, special time or score, the players' ranks), the HUD's positions; for
 * two players the split screen's sprites instead.
 *
 * Each sprite byte is written as an offset in the sprite table.
 */
#include "audio/sound.h"
#include "data/cart.h"
#include "fx/fx.h"
#include "hud/hud.h"
#include "menu/menu.h"
#include "race/race.h"
#include "save/save.h"
#include "system/chip.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

typedef struct { uint8_t sprite; uint8_t field; uint8_t v; } HudByte; /* a sprite's field (SpriteField) and its value */

static const HudByte hide_sprites[12] = {
    {0x74, SPRITE_Y, 0xF0}, {0x75, SPRITE_Y, 0xF0}, {0x76, SPRITE_Y, 0xF0}, {0x77, SPRITE_Y, 0xF0}, {0x78, SPRITE_Y, 0xF0},
    {0x79, SPRITE_Y, 0xF0}, {0x7A, SPRITE_Y, 0xF0}, {0x7B, SPRITE_Y, 0xF0}, {0x7C, SPRITE_Y, 0xF0}, {0x7D, SPRITE_Y, 0xF0},
    {0x7E, SPRITE_Y, 0xF0}, {0x7F, SPRITE_Y, 0xF0},
};
static const HudByte best_time_sprites[30] = {
    {0x3C, SPRITE_X, 0xAC}, {0x3C, SPRITE_Y, 0xB8}, {0x3C, SPRITE_ATTRIBUTES, 0x36}, {0x3D, SPRITE_X, 0xAC}, {0x3D, SPRITE_Y, 0xC0},
    {0x3D, SPRITE_ATTRIBUTES, 0x36}, {0x3E, SPRITE_X, 0xBC}, {0x3E, SPRITE_Y, 0xB8}, {0x3E, SPRITE_ATTRIBUTES, 0x36}, {0x3F, SPRITE_X, 0xBC},
    {0x3F, SPRITE_Y, 0xC0}, {0x3F, SPRITE_ATTRIBUTES, 0x36}, {0x58, SPRITE_X, 0xC4}, {0x58, SPRITE_Y, 0xB8}, {0x58, SPRITE_ATTRIBUTES, 0x36},
    {0x59, SPRITE_X, 0xC4}, {0x59, SPRITE_Y, 0xC0}, {0x59, SPRITE_ATTRIBUTES, 0x36}, {0x5A, SPRITE_X, 0xD4}, {0x5A, SPRITE_Y, 0xB8},
    {0x5A, SPRITE_ATTRIBUTES, 0x36}, {0x5B, SPRITE_X, 0xD4}, {0x5B, SPRITE_Y, 0xC0}, {0x5B, SPRITE_ATTRIBUTES, 0x36}, {0x5C, SPRITE_X, 0xDC},
    {0x5C, SPRITE_Y, 0xB8}, {0x5C, SPRITE_ATTRIBUTES, 0x36}, {0x5D, SPRITE_X, 0xDC}, {0x5D, SPRITE_Y, 0xC0}, {0x5D, SPRITE_ATTRIBUTES, 0x36},
};
static const HudByte no_time_sprites[40] = {
    {0x3C, SPRITE_X, 0xAB}, {0x3C, SPRITE_Y, 0xBC}, {0x3C, SPRITE_TILE, 0x3F}, {0x3C, SPRITE_ATTRIBUTES, 0x36}, {0x3D, SPRITE_X, 0xAB},
    {0x3D, SPRITE_Y, 0xBC}, {0x3D, SPRITE_TILE, 0x3F}, {0x3D, SPRITE_ATTRIBUTES, 0x36}, {0x3E, SPRITE_X, 0xBB}, {0x3E, SPRITE_Y, 0xBC},
    {0x3E, SPRITE_TILE, 0x3F}, {0x3E, SPRITE_ATTRIBUTES, 0x36}, {0x3F, SPRITE_X, 0xBB}, {0x3F, SPRITE_Y, 0xBC}, {0x3F, SPRITE_TILE, 0x3F},
    {0x3F, SPRITE_ATTRIBUTES, 0x36}, {0x58, SPRITE_X, 0xC3}, {0x58, SPRITE_Y, 0xBC}, {0x58, SPRITE_TILE, 0x3F}, {0x58, SPRITE_ATTRIBUTES, 0x36},
    {0x59, SPRITE_X, 0xC3}, {0x59, SPRITE_Y, 0xBC}, {0x59, SPRITE_TILE, 0x3F}, {0x59, SPRITE_ATTRIBUTES, 0x36}, {0x5A, SPRITE_X, 0xD5},
    {0x5A, SPRITE_Y, 0xBC}, {0x5A, SPRITE_TILE, 0x3F}, {0x5A, SPRITE_ATTRIBUTES, 0x36}, {0x5D, SPRITE_X, 0xD5}, {0x5D, SPRITE_Y, 0xBC},
    {0x5D, SPRITE_TILE, 0x3F}, {0x5D, SPRITE_ATTRIBUTES, 0x36}, {0x5C, SPRITE_X, 0xDD}, {0x5C, SPRITE_Y, 0xBC}, {0x5C, SPRITE_TILE, 0x3F},
    {0x5C, SPRITE_ATTRIBUTES, 0x36}, {0x5D, SPRITE_X, 0xDD}, {0x5D, SPRITE_Y, 0xBC}, {0x5D, SPRITE_TILE, 0x3F}, {0x5D, SPRITE_ATTRIBUTES, 0x36},
};
static const HudByte flag_sprites[8] = {
    {0x5E, SPRITE_X, 0xB4}, {0x5E, SPRITE_Y, 0xB8}, {0x5E, SPRITE_TILE, 0x14}, {0x5E, SPRITE_ATTRIBUTES, 0x36}, {0x5F, SPRITE_X, 0xCC},
    {0x5F, SPRITE_Y, 0xB8}, {0x5F, SPRITE_TILE, 0x15}, {0x5F, SPRITE_ATTRIBUTES, 0x36},
};
static const HudByte stunt_record_sprites[12] = {
    {0x74, SPRITE_X, 0xB4}, {0x75, SPRITE_X, 0xCA}, {0x76, SPRITE_X, 0xD2}, {0x77, SPRITE_X, 0xD2}, {0x78, SPRITE_X, 0xDA},
    {0x79, SPRITE_X, 0xDA}, {0x7A, SPRITE_X, 0xBA}, {0x7B, SPRITE_X, 0xBA}, {0x7C, SPRITE_X, 0xC2}, {0x7D, SPRITE_X, 0xC2},
    {0x7E, SPRITE_X, 0xAC}, {0x7F, SPRITE_X, 0xAC},
};
static const HudByte stunt_score_sprites[42] = {
    {0x3C, SPRITE_X, 0xD0}, {0x3C, SPRITE_Y, 0xF0}, {0x3C, SPRITE_TILE, 0x3F}, {0x3C, SPRITE_ATTRIBUTES, 0x36}, {0x3D, SPRITE_Y, 0xF0},
    {0x3E, SPRITE_X, 0xD8}, {0x3E, SPRITE_Y, 0xF0}, {0x3E, SPRITE_TILE, 0x3F}, {0x3E, SPRITE_ATTRIBUTES, 0x36}, {0x3F, SPRITE_Y, 0xF0},
    {0x58, SPRITE_X, 0xBD}, {0x58, SPRITE_Y, 0xB8}, {0x58, SPRITE_TILE, 0x00}, {0x58, SPRITE_ATTRIBUTES, 0x36}, {0x59, SPRITE_X, 0xBD},
    {0x59, SPRITE_Y, 0xC0}, {0x59, SPRITE_TILE, 0x01}, {0x59, SPRITE_ATTRIBUTES, 0x36}, {0x5A, SPRITE_X, 0xC5}, {0x5A, SPRITE_Y, 0xB8},
    {0x5A, SPRITE_TILE, 0x00}, {0x5A, SPRITE_ATTRIBUTES, 0x36}, {0x5B, SPRITE_X, 0xC5}, {0x5B, SPRITE_Y, 0xC0}, {0x5B, SPRITE_TILE, 0x01},
    {0x5B, SPRITE_ATTRIBUTES, 0x36}, {0x5C, SPRITE_X, 0xDD}, {0x5C, SPRITE_Y, 0xB8}, {0x5C, SPRITE_TILE, 0x08}, {0x5C, SPRITE_ATTRIBUTES, 0x36},
    {0x5D, SPRITE_X, 0xDD}, {0x5D, SPRITE_Y, 0xC0}, {0x5D, SPRITE_TILE, 0x09}, {0x5D, SPRITE_ATTRIBUTES, 0x36}, {0x5E, SPRITE_X, 0xE5},
    {0x5E, SPRITE_Y, 0xB8}, {0x5E, SPRITE_TILE, 0x00}, {0x5E, SPRITE_ATTRIBUTES, 0x36}, {0x5F, SPRITE_X, 0xE5}, {0x5F, SPRITE_Y, 0xC0},
    {0x5F, SPRITE_TILE, 0x01}, {0x5F, SPRITE_ATTRIBUTES, 0x36},
};
static const HudByte battle_alt_sprites[6] = {
    {0x3E, SPRITE_X, 0xAA}, {0x3E, SPRITE_Y, 0xC0}, {0x3E, SPRITE_ATTRIBUTES, 0x36}, {0x3C, SPRITE_X, 0xE6}, {0x3C, SPRITE_Y, 0xC0},
    {0x3C, SPRITE_ATTRIBUTES, 0x36},
};
static const HudByte battle_p1_sprites[6] = {
    {0x3E, SPRITE_X, 0xAA}, {0x3E, SPRITE_Y, 0xB8}, {0x3E, SPRITE_ATTRIBUTES, 0x36}, {0x3F, SPRITE_X, 0xAA}, {0x3F, SPRITE_Y, 0xC0},
    {0x3F, SPRITE_ATTRIBUTES, 0x36},
};
static const HudByte battle_p2_sprites[6] = {
    {0x3C, SPRITE_X, 0xE6}, {0x3C, SPRITE_Y, 0xB8}, {0x3C, SPRITE_ATTRIBUTES, 0x36}, {0x3D, SPRITE_X, 0xE6}, {0x3D, SPRITE_Y, 0xC0},
    {0x3D, SPRITE_ATTRIBUTES, 0x36},
};

/* The HUD's tables in bank 3: the sprite lists (four bytes an entry); for
   the split screen, its panel sprites' places and tiles (a word each, four
   bytes an entry), its map sprites' the same, their five size bytes, and
   its art block's two records (its transfer, for its length; where its
   packed data is). */
enum {
    HUD_SETUP_BANK = 0x03, SPRITE_LISTS = 0xF883,
    SPLIT_PANEL_SPRITES = 0xE7EA, SPLIT_MAP_SPRITES = 0xE80F, SPLIT_SPRITE_SIZES = 0xE80A,
    TO_SPLIT_ART = 0xF845, PACKED_SPLIT_ART = 0xF804
};

/* The sprite list's entries from `list` on into the sprites' X ninth bits
   and sizes (the original sends this list to its high table): each entry
   four bytes, sixteen sprites; $FF ends it, $FE skips to its second byte. */
static void oam_high_table_from_list(uint16_t list)
{
    int first = 0;
    uint8_t v;

    while ((v = cart8(HUD_SETUP_BANK, (uint16_t)(SPRITE_LISTS + list))) != 0xFF) {
        if (v == 0xFE) {
            sprite_high_set(first + 4, 0xFE);
        } else {
            sprite_high_set(first, v);
            sprite_high_set(first + 4, cart8(HUD_SETUP_BANK, (uint16_t)(SPRITE_LISTS + 1 + list)));
            sprite_high_set(first + 8, cart8(HUD_SETUP_BANK, (uint16_t)(SPRITE_LISTS + 2 + list)));
            sprite_high_set(first + 12, cart8(HUD_SETUP_BANK, (uint16_t)(SPRITE_LISTS + 3 + list)));
            list = (uint16_t)(list + 3);
        }
        list = (uint16_t)(list + 1);
        first += 16;
    }
}

/* A sprite's place (X low byte, Y high) and look (character low, attributes high), as words. */
static void sprite_place(int n, uint16_t w)
{
    sprite_x_set(n, (uint8_t)w);
    sprite_y_set(n, (uint8_t)(w >> 8));
}

static void sprite_look(int n, uint16_t w)
{
    sprite_tile_set(n, (uint8_t)w);
    sprite_attributes_set(n, (uint8_t)(w >> 8));
}

static void hud_bytes(const HudByte *t, unsigned n)
{
    for (unsigned i = 0; i < n; i++)
        sprite_field_set(t[i].sprite, (SpriteField)t[i].field, t[i].v);
}

#define HUD_BYTES(t) hud_bytes((t), sizeof(t) / sizeof((t)[0]))

/* A digit's two characters into sprites `n` and n + 1. */
static void digit_tiles(int n, uint8_t d)
{
    sprite_tile_set(n, (uint8_t)(d << 1));
    sprite_tile_set(n + 1, (uint8_t)((d << 1) + 1));
}

/* The five digits of a BCD time into the best time's sprites. */
static void time_tiles(uint8_t min, uint8_t sec, uint8_t hund)
{
    digit_tiles(0x3C, min & 0x0F);
    digit_tiles(0x3E, (uint8_t)(sec >> 4));
    digit_tiles(0x58, sec & 0x0F);
    digit_tiles(0x5A, (uint8_t)(hund >> 4));
    digit_tiles(0x5C, hund & 0x0F);
}

/* @routine 03:DE39 RaceHudSpritesHide: sprites 116 to 127. */
static void race_hud_sprites_hide(void) { HUD_BYTES(hide_sprites); }

/* @routine 03:DF61 HudBestTimeSprites */
static void hud_best_time_sprites(void) { HUD_BYTES(best_time_sprites); }

/* @routine 03:DF10 HudBestTimeDigits: a packed time as the best time. */
void HudBestTimeDigits(uint16_t packed)
{
    uint16_t min, sec, hund;

    TimeUnpackBcd(packed, &min, &sec, &hund);
    time_tiles((uint8_t)min, (uint8_t)sec, (uint8_t)hund);
    hud_best_time_sprites();
}

static void no_time(void) { HUD_BYTES(no_time_sprites); }
static void flags(void) { HUD_BYTES(flag_sprites); }

/* HUD mode 3: returns 1 to go on as mode 0. */
static int mode_all(void)
{
    if (g.race_skipped)
        return 1;
    g.hud_marker_x = 0xEC;
    palettes.target = 0x7FFF;
    if (g.time_override) {
        /* @routine 03:DEFD RecordOffsetAllCourses ((course - $14) * 5) is
           this index: the override runs courses $14-$16 */
        uint16_t t = save.special[g.course - 0x14].time;

        if (t == 0xFFFF)
            no_time();
        else
            HudBestTimeDigits(t);
        flags();
        return 0;
    }
    if (g.record_times[0] == 0xFFFF) {
        no_time();
        flags();
        return 0;
    }
    time_tiles(g.record_bcd[2][2], g.record_bcd[2][1], g.record_bcd[2][0]);
    hud_best_time_sprites();
    flags();
    return 0;
}

/* HUD mode 1 (Stunt Trax). */
static void mode_stunt(void)
{
    if (g.hud_blank) {
        uint16_t t;

        palettes.target = 0x7FFF;
        g.hud_marker_x = 0xAC;
        HUD_BYTES(stunt_record_sprites);
        t = save.stunt[g.course - 0x17].time;  /* Stunt Trax: courses $17-$1A */
        if (t == 0xFFFF) {
            no_time();
        } else {
            uint16_t min, sec, hund;

            TimeUnpackBcd(t, &min, &sec, &hund);
            time_tiles((uint8_t)min, (uint8_t)sec, (uint8_t)hund);
            hud_best_time_sprites();
        }
        flags();
        return;
    }
    race_hud_sprites_hide();
    HUD_BYTES(stunt_score_sprites);
    palettes.target = 0x2127;
}

/* a number's two characters into sprites `sprite` and sprite + 1 */
static void player_tiles(uint8_t n, int sprite)
{
    sprite_tile_set(sprite, (uint8_t)(n << 1));
    sprite_tile_set(sprite + 1, (uint8_t)((n << 1) + 1));
}

/* HUD modes 0 (Speed Trax) and 2 (Battle Trax). */
static void mode_speed_battle(int speed)
{
    if (speed) {
        g.hud_marker_x = 0xEC;
    }
    if (g.race_skipped) {
        sprite_tile_set(0x3E, 0x3F);
        sprite_tile_set(0x3C, 0x3F);
        HUD_BYTES(battle_alt_sprites);
        race_hud_sprites_hide();
    } else {
        player_tiles(g.player_rank[0], 0x3E);
        HUD_BYTES(battle_p1_sprites);
        player_tiles(g.tries_left, 0x3C);
        HUD_BYTES(battle_p2_sprites);
        if (g.time_override) {
            palettes.target = 0x3947;
            return;
        }
    }
    palettes.target = 0x7FFF;
}

/*
 * @routine 03:DECA RaceHudSetup
 * WHAT IT DOES: The one-player race HUD after its graphics: the sprite
 * table filled from its lists, the mode's sprites, the scrolls, the HUD's
 * positions and the raster chain; the race's variables reset. The per-line
 * gradient tables the original built here are the video side's.
 */
void RaceHudSetup(void)
{
    int slot = 0;
    uint16_t list = 0;

    (void)SoundFromTableD89();
    fx16_set(FXIN_SHADE_BASE, FX_SHADE_BASE_HUD);
    OamBufferClear();
    OamFromList(HUD_SETUP_BANK, &slot, &list);
    oam_high_table_from_list(0x01AA);
    switch (g.hud_mode) {
    case 3:
        if (mode_all())
            mode_speed_battle(1);
        break;
    case 1:
        mode_stunt();
        break;
    case 0:
        mode_speed_battle(1);
        break;
    default:
        mode_speed_battle(0);
        break;
    }
    OamDmaAll();
    pic.bg[0].scroll_x = 0;
    pic.bg[0].scroll_y = (int16_t)0xFFEF;
    pic.bg[1].scroll_x = 0;
    pic.bg[1].scroll_y = 0x0080;
    g.raster_chain = RASTER_ONE_PLAYER;
    g.gauge_place[4] = 0xC4;
    g.gauge_place[6] = 0x93;
    g.hud_map_origin_x[0] = 0x2A;
    g.hud_map_y_b = 0x7A;
    g.hud_panel_b_x = 0x00;
    g.hud_slide = 0xE0;
    g.gauge_place[0] = 0x50;
    g.gauge_place[2] = 0xB8;
    g.fx_upload_stage = 0;
    g.fx_screen_mode = 0x03;
    fx16_set(GSU_WORD_1C4, 1);
    RaceVarsReset();
}

/*
 * @routine 03:E6FA RaceHudSetupTwoPlayers
 * WHAT IT DOES: The two-player race HUD: the course screen's set-up and the
 * split screen's sprites into both halves.
 */
void RaceHudSetupTwoPlayers(void)
{
    uint16_t v;

    fx16_set(FXIN_SHADE_BASE, FX_SHADE_BASE_RACE);
    g.fx_screen_mode = 0x01;
    fx16_set(GSU_WORD_1C4, 2);
    MenuCourseScreenSetup();
    PaletteCourseLoad(g.course);
    /* the panels' eight sprites: the top half's from $2F, the bottom's from $6F */
    for (int k = 0; k < 8; k++) {
        uint16_t y = (uint16_t)(4 * k);

        v = (uint16_t)(cart16(HUD_SETUP_BANK, (uint16_t)(SPLIT_PANEL_SPRITES + y)) - 0x0800);
        sprite_place(0x2F + k, v);
        v = (uint16_t)(v + 0x6850);
        sprite_place(0x6F + k, v);
        v = cart16(HUD_SETUP_BANK, (uint16_t)(SPLIT_PANEL_SPRITES + 2 + y));
        sprite_look(0x2F + k, v);
        sprite_look(0x6F + k, v);
    }
    /* the result lines' sprites, blank: $2C-$2E and $6C-$6E */
    for (int i = 0; i < 3; i++) {
        sprite_place(0x2C + i, 0x0000);
        sprite_place(0x6C + i, 0x0000);
        sprite_look(0x2C + i, 0x26A0);
        sprite_look(0x6C + i, 0x26A0);
    }
    /* the maps' nine sprites: the top half's from $37, the bottom's from $77 */
    for (int k = 0; k < 9; k++) {
        uint16_t y = (uint16_t)(4 * k);

        sprite_place(0x37 + k, (uint16_t)(cart16(HUD_SETUP_BANK, (uint16_t)(SPLIT_MAP_SPRITES + y)) + 0x2C28));
        sprite_look(0x37 + k, cart16(HUD_SETUP_BANK, (uint16_t)(SPLIT_MAP_SPRITES + 2 + y)));
    }
    for (int k = 0; k < 9; k++) {
        uint16_t y = (uint16_t)(4 * k);

        sprite_place(0x77 + k, (uint16_t)(cart16(HUD_SETUP_BANK, (uint16_t)(SPLIT_MAP_SPRITES + y)) + 0x9476));
        sprite_look(0x77 + k, cart16(HUD_SETUP_BANK, (uint16_t)(SPLIT_MAP_SPRITES + 2 + y)));
    }
    /* their sizes and X ninth bits: sprites $2C-$3F and $6C-$7F, four a byte */
    for (int i = 0; i < 5; i++) {
        uint8_t s = cart8(HUD_SETUP_BANK, (uint16_t)(SPLIT_SPRITE_SIZES + i));

        sprite_high_set(0x2C + 4 * i, s);
        sprite_high_set(0x6C + 4 * i, s);
    }
    /* the split screen's sprite art: the sprites' characters $A0 on */
    race_art_write(RACE_ART_SPRITES + 0x0A0, fx_ram + FX_UNPACK_AREA, race_block_unpack(TO_SPLIT_ART, PACKED_SPLIT_ART));
    RaceVarsReset();
}
