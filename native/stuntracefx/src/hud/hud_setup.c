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

typedef struct { uint16_t at; uint8_t v; } HudByte;

static const HudByte hide_sprites[12] = {
    {0x01D1, 0xF0}, {0x01D5, 0xF0}, {0x01D9, 0xF0}, {0x01DD, 0xF0}, {0x01E1, 0xF0}, {0x01E5, 0xF0},
    {0x01E9, 0xF0}, {0x01ED, 0xF0}, {0x01F1, 0xF0}, {0x01F5, 0xF0}, {0x01F9, 0xF0}, {0x01FD, 0xF0},
};
static const HudByte best_time_sprites[30] = {
    {0x00F0, 0xAC}, {0x00F1, 0xB8}, {0x00F3, 0x36}, {0x00F4, 0xAC}, {0x00F5, 0xC0}, {0x00F7, 0x36},
    {0x00F8, 0xBC}, {0x00F9, 0xB8}, {0x00FB, 0x36}, {0x00FC, 0xBC}, {0x00FD, 0xC0}, {0x00FF, 0x36},
    {0x0160, 0xC4}, {0x0161, 0xB8}, {0x0163, 0x36}, {0x0164, 0xC4}, {0x0165, 0xC0}, {0x0167, 0x36},
    {0x0168, 0xD4}, {0x0169, 0xB8}, {0x016B, 0x36}, {0x016C, 0xD4}, {0x016D, 0xC0}, {0x016F, 0x36},
    {0x0170, 0xDC}, {0x0171, 0xB8}, {0x0173, 0x36}, {0x0174, 0xDC}, {0x0175, 0xC0}, {0x0177, 0x36},
};
static const HudByte no_time_sprites[40] = {
    {0x00F0, 0xAB}, {0x00F1, 0xBC}, {0x00F2, 0x3F}, {0x00F3, 0x36}, {0x00F4, 0xAB}, {0x00F5, 0xBC},
    {0x00F6, 0x3F}, {0x00F7, 0x36}, {0x00F8, 0xBB}, {0x00F9, 0xBC}, {0x00FA, 0x3F}, {0x00FB, 0x36},
    {0x00FC, 0xBB}, {0x00FD, 0xBC}, {0x00FE, 0x3F}, {0x00FF, 0x36}, {0x0160, 0xC3}, {0x0161, 0xBC},
    {0x0162, 0x3F}, {0x0163, 0x36}, {0x0164, 0xC3}, {0x0165, 0xBC}, {0x0166, 0x3F}, {0x0167, 0x36},
    {0x0168, 0xD5}, {0x0169, 0xBC}, {0x016A, 0x3F}, {0x016B, 0x36}, {0x0174, 0xD5}, {0x0175, 0xBC},
    {0x0176, 0x3F}, {0x0177, 0x36}, {0x0170, 0xDD}, {0x0171, 0xBC}, {0x0172, 0x3F}, {0x0173, 0x36},
    {0x0174, 0xDD}, {0x0175, 0xBC}, {0x0176, 0x3F}, {0x0177, 0x36},
};
static const HudByte flag_sprites[8] = {
    {0x0178, 0xB4}, {0x0179, 0xB8}, {0x017A, 0x14}, {0x017B, 0x36}, {0x017C, 0xCC}, {0x017D, 0xB8},
    {0x017E, 0x15}, {0x017F, 0x36},
};
static const HudByte stunt_record_sprites[12] = {
    {0x01D0, 0xB4}, {0x01D4, 0xCA}, {0x01D8, 0xD2}, {0x01DC, 0xD2}, {0x01E0, 0xDA}, {0x01E4, 0xDA},
    {0x01E8, 0xBA}, {0x01EC, 0xBA}, {0x01F0, 0xC2}, {0x01F4, 0xC2}, {0x01F8, 0xAC}, {0x01FC, 0xAC},
};
static const HudByte stunt_score_sprites[42] = {
    {0x00F0, 0xD0}, {0x00F1, 0xF0}, {0x00F2, 0x3F}, {0x00F3, 0x36}, {0x00F5, 0xF0}, {0x00F8, 0xD8},
    {0x00F9, 0xF0}, {0x00FA, 0x3F}, {0x00FB, 0x36}, {0x00FD, 0xF0}, {0x0160, 0xBD}, {0x0161, 0xB8},
    {0x0162, 0x00}, {0x0163, 0x36}, {0x0164, 0xBD}, {0x0165, 0xC0}, {0x0166, 0x01}, {0x0167, 0x36},
    {0x0168, 0xC5}, {0x0169, 0xB8}, {0x016A, 0x00}, {0x016B, 0x36}, {0x016C, 0xC5}, {0x016D, 0xC0},
    {0x016E, 0x01}, {0x016F, 0x36}, {0x0170, 0xDD}, {0x0171, 0xB8}, {0x0172, 0x08}, {0x0173, 0x36},
    {0x0174, 0xDD}, {0x0175, 0xC0}, {0x0176, 0x09}, {0x0177, 0x36}, {0x0178, 0xE5}, {0x0179, 0xB8},
    {0x017A, 0x00}, {0x017B, 0x36}, {0x017C, 0xE5}, {0x017D, 0xC0}, {0x017E, 0x01}, {0x017F, 0x36},
};
static const HudByte battle_alt_sprites[6] = {
    {0x00F8, 0xAA}, {0x00F9, 0xC0}, {0x00FB, 0x36}, {0x00F0, 0xE6}, {0x00F1, 0xC0}, {0x00F3, 0x36},
};
static const HudByte battle_p1_sprites[6] = {
    {0x00F8, 0xAA}, {0x00F9, 0xB8}, {0x00FB, 0x36}, {0x00FC, 0xAA}, {0x00FD, 0xC0}, {0x00FF, 0x36},
};
static const HudByte battle_p2_sprites[6] = {
    {0x00F0, 0xE6}, {0x00F1, 0xB8}, {0x00F3, 0x36}, {0x00F4, 0xE6}, {0x00F5, 0xC0}, {0x00F7, 0x36},
};

/* The sprite list's entries from `list` on, four bytes each, into the high
   table of sizes: $FF ends it, $FE is a hidden entry. */
static void oam_high_table_from_list(uint16_t list)
{
    uint16_t at = 0x0200;
    uint8_t v;

    while ((v = cart8(0x03, (uint16_t)(0xF883 + list))) != 0xFF) {
        if (v == 0xFE) {
            OamByteSet((uint16_t)(at + 1), 0xFE);
        } else {
            OamByteSet(at, v);
            OamByteSet((uint16_t)(at + 1), cart8(0x03, (uint16_t)(0xF884 + list)));
            OamByteSet((uint16_t)(at + 2), cart8(0x03, (uint16_t)(0xF885 + list)));
            OamByteSet((uint16_t)(at + 3), cart8(0x03, (uint16_t)(0xF886 + list)));
            list = (uint16_t)(list + 3);
        }
        list = (uint16_t)(list + 1);
        at = (uint16_t)(at + 4);
    }
}

static void oam_word_set(uint16_t at, uint16_t w)
{
    OamByteSet(at, (uint8_t)w);
    OamByteSet((uint16_t)(at + 1), (uint8_t)(w >> 8));
}

static void hud_bytes(const HudByte *t, unsigned n)
{
    for (unsigned i = 0; i < n; i++)
        OamByteSet(t[i].at, t[i].v);
}

#define HUD_BYTES(t) hud_bytes((t), sizeof(t) / sizeof((t)[0]))

/* A digit's two tiles into the sprites at `at` and at + 4. */
static void digit_tiles(uint16_t at, uint8_t d)
{
    OamByteSet(at, (uint8_t)(d << 1));
    OamByteSet((uint16_t)(at + 4), (uint8_t)((d << 1) + 1));
}

/* The five digits of a BCD time into the best time's sprites. */
static void time_tiles(uint8_t min, uint8_t sec, uint8_t hund)
{
    digit_tiles(0x00F2, min & 0x0F);
    digit_tiles(0x00FA, (uint8_t)(sec >> 4));
    digit_tiles(0x0162, sec & 0x0F);
    digit_tiles(0x016A, (uint8_t)(hund >> 4));
    digit_tiles(0x0172, hund & 0x0F);
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

/* @routine 03:DEFD RecordOffsetAllCourses: (course - $14) * 5. */
uint16_t RecordOffsetAllCourses(void)
{
    return (uint8_t)((g.course - 0x14) * 5);
}

static void no_time(void) { HUD_BYTES(no_time_sprites); }
static void flags(void) { HUD_BYTES(flag_sprites); }

/* HUD mode 3: returns 1 to go on as mode 0. */
static int mode_all(void)
{
    if (g.race_skipped)
        return 1;
    g.hud_marker_y = 0xEC;
    palettes.target = 0x7FFF;
    if (g.time_override) {
        uint16_t t = fx16((uint16_t)(0xFC5E + RecordOffsetAllCourses()));

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
        g.hud_marker_y = 0xAC;
        HUD_BYTES(stunt_record_sprites);
        t = fx16((uint16_t)(0xFC4A + (uint16_t)((g.course - 0x17) * 5)));
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

static void player_tiles(uint8_t n, uint16_t at)
{
    OamByteSet(at, (uint8_t)(n << 1));
    OamByteSet((uint16_t)(at + 4), (uint8_t)((n << 1) + 1));
}

/* HUD modes 0 (Speed Trax) and 2 (Battle Trax). */
static void mode_speed_battle(int speed)
{
    if (speed) {
        g.hud_marker_y = 0xEC;
    }
    if (g.race_skipped) {
        OamByteSet(0x00FA, 0x3F);
        OamByteSet(0x00F2, 0x3F);
        HUD_BYTES(battle_alt_sprites);
        race_hud_sprites_hide();
    } else {
        player_tiles(g.player_rank[0], 0x00FA);
        HUD_BYTES(battle_p1_sprites);
        player_tiles(g.tries_left, 0x00F2);
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
    OamFromList(0x03, &slot, &list);
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
    g.hud_panel_b_y = 0x00;
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
    uint16_t x, v;

    fx16_set(FXIN_SHADE_BASE, FX_SHADE_BASE_RACE);
    g.fx_screen_mode = 0x01;
    fx16_set(GSU_WORD_1C4, 2);
    MenuCourseScreenSetup();
    PaletteCourseLoad(g.course);
    x = 0x00BC;
    for (uint16_t y = 0; y < 0x0020; y = (uint16_t)(y + 4), x = (uint16_t)(x + 4)) {
        v = (uint16_t)(cart16(0x03, (uint16_t)(0xE7EA + y)) - 0x0800);
        oam_word_set(x, v);
        v = (uint16_t)(v + 0x6850);
        oam_word_set((uint16_t)(x + 0x100), v);
        v = cart16(0x03, (uint16_t)(0xE7EC + y));
        oam_word_set((uint16_t)(x + 2), v);
        oam_word_set((uint16_t)(x + 0x102), v);
    }
    static const uint16_t blank_pos[6] = {0x00B0, 0x00B4, 0x00B8, 0x01B0, 0x01B4, 0x01B8};
    static const uint16_t blank_tile[6] = {0x00B2, 0x00B6, 0x00BA, 0x01B2, 0x01B6, 0x01BA};

    for (int i = 0; i < 6; i++) {
        oam_word_set(blank_pos[i], 0x0000);
        oam_word_set(blank_tile[i], 0x26A0);
    }
    x = 0x00DC;
    for (uint16_t y = 0; y < 0x0024; y = (uint16_t)(y + 4), x = (uint16_t)(x + 4)) {
        oam_word_set(x, (uint16_t)(cart16(0x03, (uint16_t)(0xE80F + y)) + 0x2C28));
        oam_word_set((uint16_t)(x + 2), cart16(0x03, (uint16_t)(0xE811 + y)));
    }
    x = 0x01DC;
    for (uint16_t y = 0; y < 0x0024; y = (uint16_t)(y + 4), x = (uint16_t)(x + 4)) {
        oam_word_set(x, (uint16_t)(cart16(0x03, (uint16_t)(0xE80F + y)) + 0x9476));
        oam_word_set((uint16_t)(x + 2), cart16(0x03, (uint16_t)(0xE811 + y)));
    }
    for (int i = 0; i < 5; i++) {
        uint8_t s = cart8(0x03, (uint16_t)(0xE80A + i));

        OamByteSet((uint16_t)(0x0200 + 0x0B + i), s);
        OamByteSet((uint16_t)(0x0210 + 0x0B + i), s);
    }
    race_unpacked_to_art(0xF845, 0xF804);
    RaceVarsReset();
}
