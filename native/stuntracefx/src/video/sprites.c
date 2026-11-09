/*
 * sprites.c: the race's sprite table. `sprites` is what the game builds; what
 * is on screen is pic.sprites, which only OamDmaAll and OamDirectStage change.
 * Each sprite's tile counts from the race's art: the sprites' first page is
 * tile $200 on in the shared set, the second $300.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

nshvc_sprite sprites[128];

/* the tile set the table's tile numbers count in, and where its two pages
   start in it: set by each screen; the race's art by default */
static nshvc_tiles sprite_set;
static uint16_t page_first = 0x200, page_second = 0x300;

void sprites_use_tiles(nshvc_tiles set, uint16_t first_page, uint16_t second_page)
{
    int n;

    sprite_set = set;
    page_first = first_page;
    page_second = second_page;
    for (n = 0; n < 128; n++)
        sprites[n].tiles = set;
}

static int on_second_page(int n)
{
    return sprites[n].tile >= page_second;
}

/* A sprite's fields as the original's table bytes hold them. */
static void set_tile_byte(int n, uint8_t t)
{
    sprites[n].tile = (uint16_t)((on_second_page(n) ? page_second : page_first) + t);
    sprites[n].tiles = sprite_set;
}

static uint8_t tile_byte(int n)
{
    return (uint8_t)(sprites[n].tile - (on_second_page(n) ? page_second : page_first));
}

/* The attribute byte: name page, palette, priority, flips */
static void set_attr(int n, uint8_t a)
{
    uint8_t t = tile_byte(n);

    sprites[n].tile = (uint16_t)(((a & 1) ? page_second : page_first) + t);
    sprites[n].palette = (a >> 1) & 7;
    sprites[n].priority = (a >> 4) & 3;
    sprites[n].hflip = (a >> 6) & 1;
    sprites[n].vflip = (a >> 7) & 1;
    sprites[n].tiles = sprite_set;
}

static uint8_t attr_byte(int n)
{
    return (uint8_t)(on_second_page(n) | sprites[n].palette << 1 | sprites[n].priority << 4 |
                     sprites[n].hflip << 6 | sprites[n].vflip << 7);
}

/* The screen position as the table holds it: the X byte, the high X bit apart; Y a byte. */
static void set_x_byte(int n, uint8_t x)
{
    sprites[n].x = (sprites[n].x < 0) ? (int32_t)x - 256 : (int32_t)x;
}

static void set_x_high(int n, int high)
{
    uint8_t x = (uint8_t)sprites[n].x;

    sprites[n].x = high ? (int32_t)x - 256 : (int32_t)x;
}

static void set_y(int n, uint8_t y)
{
    sprites[n].y = y >= 240 ? (int32_t)y - 256 : (int32_t)y;
}

static uint8_t y_byte(int n)
{
    return (uint8_t)sprites[n].y;
}

/* 16 x 16 when large, 8 x 8 otherwise, the race's sprite sizes */
static void set_large(int n, int large)
{
    sprites[n].width = sprites[n].height = large ? 2 : 1;
}

/* A high table byte: four sprites' X-high and size bits, as the original wrote them. */
static void set_high_byte(int byte_index, uint8_t v)
{
    int i;

    for (i = 0; i < 4; i++) {
        int n = byte_index * 4 + i;

        set_x_high(n, (v >> (i * 2)) & 1);
        set_large(n, (v >> (i * 2 + 1)) & 1);
    }
}

static void or_high_byte(int byte_index, uint8_t v)
{
    int i;

    for (i = 0; i < 4; i++) {
        int n = byte_index * 4 + i;

        if ((v >> (i * 2)) & 1)
            set_x_high(n, 1);
        if ((v >> (i * 2 + 1)) & 1)
            set_large(n, 1);
    }
}

/* the whole word of a sprite: X, Y bytes */
static void set_pos_word(int n, uint16_t w)
{
    set_x_byte(n, (uint8_t)w);
    set_y(n, (uint8_t)(w >> 8));
}

/* tile and attribute bytes */
static void set_tile_word(int n, uint16_t w)
{
    set_tile_byte(n, (uint8_t)w);
    set_attr(n, (uint8_t)(w >> 8));
}

/* ---- the table to the screen ---- */

void OamDmaAll(void)
{
    memcpy(pic.sprites, sprites, sizeof pic.sprites);
    pic.sprite_count = 128;
}

/* Bands of sprites written straight to the screen's table in the blank lines:
   stage $0D54 picks them; the data is the table as it stands. */
void OamDirectStage(void)
{
    static const struct { uint8_t first, count; } band[] = {{116, 12}, {75, 12}, {112, 4}, {36, 11}};

    if (g.hud_mode != 1 && g.hud_hold) {
        int n;

        for (n = 116; n < 128; n++) {       /* all four table bytes $F0 */
            pic.sprites[n] = sprites[n];
            pic.sprites[n].x = 0xF0;
            pic.sprites[n].y = 0xF0 - 256;
            pic.sprites[n].tile = (uint16_t)(page_first + 0xF0);
            pic.sprites[n].palette = 0;
            pic.sprites[n].priority = 3;
            pic.sprites[n].hflip = 1;
            pic.sprites[n].vflip = 1;
        }
        return;
    }
    switch (g.start_light) {
    case 0x02:
        if (g.hud_marker_y == 0xAC)
            g.start_light = (uint8_t)(g.start_light + 2);
        /* fall through */
    case 0x00:
        memcpy(&pic.sprites[band[0].first], &sprites[band[0].first], band[0].count * sizeof sprites[0]);
        break;
    case 0x04:
        if (g.gauge_place[4] == 0x1F)
            g.start_light = (uint8_t)(g.start_light + 2);
        memcpy(&pic.sprites[band[1].first], &sprites[band[1].first], band[1].count * sizeof sprites[0]);
        break;
    case 0x06:
        if (g.hud_panel_b_y == 0x2A) {
            g.race_ended_d55 = 0x2A;
            g.start_light = 0;
        }
        memcpy(&pic.sprites[band[2].first], &sprites[band[2].first], band[2].count * sizeof sprites[0]);
        break;
    case 0x08:
        if (g.hud_slide == 0x10)
            g.start_light = 0;
        memcpy(&pic.sprites[band[3].first], &sprites[band[3].first], band[3].count * sizeof sprites[0]);
        break;
    default:
        if (g.hud_slide == 0xD0)
            g.start_light = 0;
        memcpy(&pic.sprites[band[3].first], &sprites[band[3].first], band[3].count * sizeof sprites[0]);
        memcpy(&pic.sprites[band[0].first], &sprites[band[0].first], band[0].count * sizeof sprites[0]);
        break;
    }
}

/* ---- clearing and hiding ---- */

void OamHideRange(int first, int count)
{
    do {
        set_y(first, 0xF0);
        first++;
    } while (--count);
}

void OamBufferClear(void)
{
    int n;

    for (n = 0; n < 128; n++) {
        sprites[n].tiles = sprite_set;
        set_x_byte(n, 0xF0);
        set_y(n, 0xF0);
        set_x_high(n, 0);
        set_large(n, 0);
    }
}

void OamBufferHideLarge(void)
{
    int n;

    for (n = 0; n < 128; n++) {
        sprites[n].tiles = sprite_set;
        set_y(n, 0xF0);
        set_x_high(n, 0);
        set_large(n, 1);
    }
}

/* Entries from the list: $FE hides one, else four bytes of X, Y, tile, attributes. */
void OamFromList(uint8_t bank, int *slot, uint16_t *list)
{
    uint8_t v;

    while ((v = cart8(bank, (uint16_t)(0xF883 + *list))) != 0xFF) {
        if (v == 0xFE) {
            set_y(*slot, 0xFE);
        } else {
            set_x_byte(*slot, v);
            set_y(*slot, cart8(bank, (uint16_t)(0xF884 + *list)));
            set_tile_byte(*slot, cart8(bank, (uint16_t)(0xF885 + *list)));
            set_attr(*slot, cart8(bank, (uint16_t)(0xF886 + *list)));
            *list = (uint16_t)(*list + 3);
        }
        *list = (uint16_t)(*list + 1);
        (*slot)++;
    }
}

uint8_t SpriteYOnScreen(uint8_t y)
{
    return y >= 0xB0 ? 0xF0 : y;
}

void OamSprite0AddY(uint8_t lines)
{
    set_y(0, (uint8_t)(y_byte(0) + lines));
}

/* ---- attribute runs ---- */

void oam_attr_fill(int first, int count, uint8_t attr)
{
    do {
        set_attr(first, attr);
        first++;
    } while (--count);
}

void OamAttr36x4(int first) { oam_attr_fill(first, 4, 0x36); }
void OamAttr36x6(int first) { oam_attr_fill(first, 6, 0x36); }
void OamAttr36x7(int first) { oam_attr_fill(first, 7, 0x36); }
void OamAttr38x4(int first) { oam_attr_fill(first, 4, 0x38); }
void OamAttr38x6(int first) { oam_attr_fill(first, 6, 0x38); }
void OamAttr38x7(int first) { oam_attr_fill(first, 7, 0x38); }

static void hide_at(int n)
{
    sprites[n].x = 0;
    set_y(n, 0xF0);
}

void OamHideEntries3And9(int first)
{
    hide_at(first + 3);
    hide_at(first + 9);
}

void OamHideEntries0To2(int first)
{
    hide_at(first);
    hide_at(first + 1);
    hide_at(first + 2);
}

void OamSetPalette(uint8_t bits, int count, int first)
{
    do {
        uint8_t a = (uint8_t)((attr_byte(first) & 0xF1) | bits);

        set_attr(first, a);
        first++;
    } while (--count);
}

void OamPaletteFromTableE30F(void)
{
    OamSetPalette(cart8(0x04, (uint16_t)(0xE30F + g.champ_stage)), 0x16, g.sprite_row_d79);
}

void OamPaletteFromTableE26A(void)
{
    uint16_t n = g.times_step;

    OamSetPalette(n ? cart8(0x04, (uint16_t)(0xE26A + ((n - 1) & 0xFF))) : 0x0A, 0x16, g.oam_first_free + 0x0D);
}

void OamPaletteFromTableE26AByMode(void)
{
    OamSetPalette(cart8(0x04, (uint16_t)(0xE26A + g.champ_stage)), 0x12, g.sprite_row_d79);
}

void OamPalettesE28D(void)
{
    OamSetPalette(cart8(0x04, (uint16_t)(0xE30B + g.champ_stage)), 0x12, g.sprite_row_d75);
    OamSetPalette(cart8(0x04, (uint16_t)(0xE313 + g.car[0])), 0x06, g.sprite_row_d75 + 0x16);
    OamSetPalette(cart8(0x04, (uint16_t)(0xE313 + g.car[1])), 0x06, g.sprite_row_d75 + 0x1C);
}

void OamPaletteForD75(void)
{
    OamSetPalette(0x0A, 0x16, g.sprite_row_d75 + 13);
}

/* ---- blocks and columns ---- */

static void oam_block(int first, uint8_t x0, uint8_t dx, uint8_t attr_a, uint8_t attr_b)
{
    set_x_byte(first, x0);
    set_x_byte(first + 1, (uint8_t)(x0 + dx));
    set_attr(first + 2, attr_a);
    set_attr(first + 3, attr_a);
    set_attr(first + 4, attr_b);
}

void OamBlock52Left(void) { oam_block(52, 0x50, 0x22, 0x0C, 0x06); }
void OamBlock52Right(void) { oam_block(52, 0x87, 0x1A, 0x06, 0x0C); }
void OamBlock59Left(void) { oam_block(59, 0x30, 0x22, 0x0C, 0x06); }
void OamBlock59Right(void) { oam_block(59, 0x5F, 0x1A, 0x06, 0x0C); }
void OamBlock123Left(void) { oam_block(123, 0x7E, 0x22, 0x0C, 0x06); }
void OamBlock123Right(void) { oam_block(123, 0xAD, 0x1A, 0x06, 0x0C); }

void OamColumn8(int first, uint16_t pos, uint16_t tile)
{
    int i;

    for (i = 0; i < 8; i++) {
        set_pos_word(first + i, pos);
        set_tile_word(first + i, tile);
        pos = (uint16_t)(pos + 0x10);
        tile = (uint16_t)(tile + 2);
    }
}

void OamHighBits18(void)
{
    static const uint8_t bits[4] = {0x40, 0x55, 0x55, 0x15};
    int i;

    g.gauge_place[6] = 0xF8;
    for (i = 0; i < 4; i++)
        or_high_byte(6 + i, bits[i]);
}

void OamHighBits58(void)
{
    static const uint8_t bits[4] = {0x40, 0x55, 0x55, 0x15};
    int i;

    g.gauge_place[7] = 0xF8;
    for (i = 0; i < 4; i++)
        or_high_byte(22 + i, bits[i]);
}

void OamHighBits18Park48(void)
{
    OamHighBits18();
    set_high_byte(12, 0x55);
    set_high_byte(13, 0x55);
    set_high_byte(14, 0x55);
    set_high_byte(15, 0x55);
}

void OamHighBits58Park112(void)
{
    OamHighBits58();
    set_high_byte(28, 0x55);
    set_high_byte(29, 0x55);
    set_high_byte(30, 0x55);
    set_high_byte(31, 0x55);
}

void OamHide36And96(void)
{
    int i;

    OamHideRange(36, 0x15);
    OamHideRange(96, 0x10);
    for (i = 0; i < 4; i++)
        set_high_byte(0x18 + i, 0);
}

void OamHide75Up(void)
{
    g.hud_map_y_b = 0xF8;
    g.gauge_place[6] = 0xF8;
    if (g.hud_mode) {
        OamHideRange(75, 0x0C);
        OamHideRange(96, 0x20);
    } else {
        OamHideRange(75, 0x35);
    }
}

void OamLargeColumns(void)
{
    OamColumn8(40, 0x4840, 0x0660);
    OamColumn8(96, 0x5840, 0x0680);
    OamColumn8(104, 0x6840, 0x06A0);
    set_high_byte(10, 0xAA);
    set_high_byte(11, 0xAA);
    set_high_byte(24, 0xAA);
    set_high_byte(25, 0xAA);
    set_high_byte(26, 0xAA);
    set_high_byte(27, 0xAA);
}

void OamPlaceRows75And112(void)
{
    static const uint8_t at8c[7] = {116, 117, 118, 120, 122, 124, 126};
    static const uint8_t at94[5] = {119, 121, 123, 125, 127};
    int i;

    if (!g.race_skipped) {
        for (i = 0; i < 7; i++)
            set_y(at8c[i], 0x8C);
        for (i = 0; i < 5; i++)
            set_y(at94[i], 0x94);
    }
    for (i = 75; i <= 86; i++)
        set_y(i, 0x93);
    set_y(112, 0x70);
    set_y(113, 0x70);
    set_y(114, 0x80);
    set_y(115, 0x80);
    set_high_byte(10, 0);
    set_high_byte(11, 0);
    set_high_byte(24, 0);
    set_high_byte(25, 0);
    set_high_byte(26, 0);
    set_high_byte(27, 0);
}

void OamHideForHudMode(void)
{
    int i;

    g.hud_map_y_b = 0xE0;
    g.gauge_place[6] = 0xE0;
    for (i = 36; i < 36 + 0x15; i++)
        set_y(i, 0xE0);
    if (!g.hud_mode) {
        for (i = 75; i < 75 + 0x35; i++)
            set_y(i, 0xE0);
    } else if (g.hud_mode != 2) {
        for (i = 75; i < 75 + 0x0D; i++)
            set_y(i, 0xE0);
        for (i = 96; i < 96 + 0x20; i++)
            set_y(i, 0xE0);
    }
}

/* ---- strips and bars ---- */

static void sprite_strip(int first, const uint8_t *table, uint16_t pos, int step, int n)
{
    int i;

    for (i = 0; i <= n; i++) {
        set_pos_word(first + i, pos);
        pos = (uint16_t)(pos + step);
        set_tile_word(first + i, (uint16_t)(table[2 * i] | table[2 * i + 1] << 8));
    }
}

void SpriteStrip4(uint16_t selector, int first, uint16_t pos)
{
    sprite_strip(first, ram_code_7f((uint16_t)(0x09CD + ((selector >> 1) & 0x00F8))), pos, 0x0010, 3);
}

void SpriteStrip6(uint16_t selector, int first, uint16_t pos)
{
    sprite_strip(first, ram_code_7f((uint16_t)(0x0A66 + ((selector & 0x0038) << 1))), pos, 0x0008, 5);
}

static void bar_marker(int slot, uint16_t v, uint16_t pos, uint16_t span, uint16_t tile_far, uint16_t tile_near)
{
    if (v >= span) {
        set_pos_word(slot, (uint16_t)(v + pos - span));
        set_tile_word(slot, tile_far);
    } else {
        set_pos_word(slot, (uint16_t)(v + pos));
        set_tile_word(slot, tile_near);
    }
}

void SpriteBar4WithMarker(uint8_t value, uint16_t pos, uint16_t slot)
{
    uint16_t v = value < 0x3F ? value : 0x3F;
    int n = slot / 4;

    SpriteStrip4(v, n + 1, pos);
    bar_marker(n, v, pos, 0x10, 0x3C20, 0x3622);
}

void SpriteBar6WithMarker(uint8_t value, uint16_t pos, uint16_t slot)
{
    int n = slot / 4;

    SpriteStrip6(value, n + 1, pos);
    bar_marker(n, value, pos, 0x08, 0x3C1F, 0x3C1E);
}

/* The bytes of the table as the original addressed them: 512 four to a
   sprite, then the 32 high bytes. */
uint8_t OamByte(uint16_t offset)
{
    if (offset >= 0x200) {
        int i, b = offset - 0x200;
        uint8_t v = 0;

        for (i = 0; i < 4; i++) {
            int n = b * 4 + i;

            if (sprites[n].x < 0)
                v |= (uint8_t)(1 << (i * 2));
            if (sprites[n].width > 1)
                v |= (uint8_t)(2 << (i * 2));
        }
        return v;
    }
    switch (offset & 3) {
    case 0: return (uint8_t)sprites[offset / 4].x;
    case 1: return y_byte(offset / 4);
    case 2: return tile_byte(offset / 4);
    default: return attr_byte(offset / 4);
    }
}

void OamBufferByteSet(uint16_t offset, uint8_t value)
{
    OamByteSet(offset, value);
}

/* A byte of the table as the original addressed it: 512 bytes four to a
   sprite, then the 32 high bytes. */
void OamByteSet(uint16_t offset, uint8_t value)
{
    if (offset >= 0x200) {
        set_high_byte(offset - 0x200, value);
        return;
    }
    switch (offset & 3) {
    case 0: set_x_byte(offset / 4, value); break;
    case 1: set_y(offset / 4, value); break;
    case 2: set_tile_byte(offset / 4, value); break;
    default: set_attr(offset / 4, value); break;
    }
}

/* ---- metasprites ---- */

int MetaspriteDraw(uint16_t group, int slot)
{
    uint16_t at = cart16(0x04, (uint16_t)(0xEDCB + group * 4));
    int n = (uint8_t)cart16(0x04, (uint16_t)(0xEDCD + group * 4)), count = n;

    do {
        set_x_byte(slot, (uint8_t)(cart8(0x04, (uint16_t)(0xEE4B + at)) + g.sprite_origin[0]));
        set_y(slot, (uint8_t)(cart8(0x04, (uint16_t)(0xEE4C + at)) + g.sprite_origin[1]));
        set_tile_byte(slot, cart8(0x04, (uint16_t)(0xEE4D + at)));
        set_attr(slot, cart8(0x04, (uint16_t)(0xEE4E + at)));
        if (!(cart8(0x04, (uint16_t)(0xEE4F + at)) & 1))
            set_large(slot, 0);
        at = (uint16_t)(at + 5);
        slot++;
    } while (--n);
    return count;
}

static uint8_t cycled_attr(uint8_t attr, uint8_t phase)
{
    static const uint8_t pal[2][3] = {{0x04, 0x08, 0x06}, {0x06, 0x04, 0x08}};
    unsigned k = (attr & 0x08) ? 0 : (attr & 0x02) ? 1 : 2;

    return (uint8_t)((attr & 0xF1) | pal[phase == 1][k]);
}

static void flashing_metasprite_draw(uint16_t set)
{
    int i, slot = 0, n;
    uint16_t at;

    g.metasprite_set = set;
    for (i = 0; i < 32; i++)
        set_high_byte(i, 0xAA);
    g.slot[0].first = 0xAA;             /* the word stores run one byte past the table */
    if (!g.flash_sprite_pos)
        return;
    if (!(g.frame_count & 3)) {
        uint16_t ph = (uint16_t)(g.flash_phase + 1);

        g.flash_phase = ph < 3 ? ph : 0;
    }
    at = cart16(0x1C, (uint16_t)(0xE5AC + g.metasprite_set * 4));
    n = (uint8_t)cart16(0x1C, (uint16_t)(0xE5AE + g.metasprite_set * 4));
    do {
        uint8_t attr;

        set_x_byte(slot, (uint8_t)(cart8(0x1C, (uint16_t)(0xE62C + at)) + (uint8_t)g.flash_sprite_pos));
        set_y(slot, (uint8_t)(cart8(0x1C, (uint16_t)(0xE62D + at)) + (uint8_t)(g.flash_sprite_pos >> 8)));
        set_tile_byte(slot, cart8(0x1C, (uint16_t)(0xE62E + at)));
        attr = cart8(0x1C, (uint16_t)(0xE62F + at));
        set_attr(slot, g.flash_phase ? cycled_attr(attr, (uint8_t)g.flash_phase) : attr);
        at = (uint16_t)(at + 5);
        slot++;
    } while (--n);
}

void FlashingMetaspriteDraw(void) { flashing_metasprite_draw(8); }
void FlashingMetaspriteDraw0(void) { flashing_metasprite_draw(0); }
void FlashingMetaspriteDrawByMode(void) { flashing_metasprite_draw(g.time_override ? 0x14 : 0x0E); }

/* ---- the three-piece cars and sprites ---- */

/* Three sprites of a piece set at the piece-set position */
static void sprite_pieces3(int slot)
{
    uint16_t i;
    uint8_t n;

    i = (uint16_t)(cart8(0x07, (uint16_t)(0xB981 + (g.piece_selector & 0xFF))) + g.piece_set);
    i = cart16(0x1C, (uint16_t)(0xF09E + ((uint16_t)(i << 2) & 0xFF)));
    for (n = 3; n; n--) {
        uint8_t a;

        set_x_byte(slot, (uint8_t)(g.piece_x + cart8(0x1C, (uint16_t)(0xF11E + i))));
        set_y(slot, (uint8_t)(g.piece_y + cart8(0x1C, (uint16_t)(0xF11F + i))));
        set_tile_byte(slot, (uint8_t)(g.piece_tile + cart8(0x1C, (uint16_t)(0xF120 + i))));
        a = cart8(0x1C, (uint16_t)(0xF121 + i));
        if (g.piece_palette)
            a = (uint8_t)((a & 0xF1) | g.piece_palette);
        set_attr(slot, a);
        i = (uint16_t)(i + 5);
        slot++;
    }
}

void SpritePieces3(uint16_t x, uint8_t y, uint16_t set, uint16_t palette, int slot)
{
    g.piece_set = set;
    g.piece_x = (uint8_t)x;
    g.piece_y = y;
    g.piece_palette = (uint8_t)palette;
    sprite_pieces3(slot);
}

static void pieces_at(uint16_t set, uint8_t x, uint8_t y, int slot)
{
    g.piece_set = set;
    g.piece_x = x;
    g.piece_y = y;
    sprite_pieces3(slot);
}

void SpritePiecesAtLeft(uint16_t set, int slot) { pieces_at(set, 0x81 - 0x24, 0x89 - 0x0B, slot); }
void SpritePiecesAtRight(uint16_t set, int slot) { pieces_at(set, 0x81 + 0x24, 0x89 - 0x0B, slot); }
void SpritePiecesAt46Y71(uint16_t set, int slot) { pieces_at(set, 0x81 - 0x3B, 0x89 - 0x18, slot); }
void SpritePiecesAt46Y82(uint16_t set, int slot) { pieces_at(set, 0x81 - 0x3B, 0x89 - 0x07, slot); }
void SpritePiecesAt46Y93(uint16_t set, int slot) { pieces_at(set, 0x81 - 0x3B, 0x89 + 0x0A, slot); }
void SpritePiecesAt62Y71(uint16_t set, int slot) { pieces_at(set, 0x89 - 0x27, 0x89 - 0x18, slot); }
void SpritePiecesAt62Y83(uint16_t set, int slot) { pieces_at(set, 0x89 - 0x27, 0x89 - 0x06, slot); }
void SpritePiecesAt62Y95(uint16_t set, int slot) { pieces_at(set, 0x89 - 0x27, 0x89 + 0x0C, slot); }

void SpritePiecesCarAt51(uint16_t car, int slot)
{
    g.piece_palette = 0;
    pieces_at((uint16_t)((car * 3) & 0x0F), 0x81 - 0x30, 0x89 - 0x02, slot);
}

static void two_cars(uint8_t step_mask, uint8_t y, uint8_t x1, uint8_t x2)
{
    if ((g.frame_count & step_mask) == step_mask) {
        uint8_t v = (uint8_t)(g.piece_selector + 1);

        g.piece_selector = v >= 4 ? 0 : v;
    }
    g.piece_tile = 0x30;
    g.piece_palette = 0x30;
    g.piece_y = y;
    pieces_at((uint16_t)(g.car[0] * 3), x1, y, 48);
    pieces_at((uint16_t)(g.car[1] * 3), x2, y, 51);
    set_high_byte(0x0C, 0x20);
    set_high_byte(0x0D, 0x58);
}

void SpritePiecesTwoCars(void) { two_cars(3, 0xA0, 0x84, 0xCC); }
void SpritePiecesTwoCarsTop(void) { two_cars(7, 0x38, 0x34, 0x7C); }

/* ---- the overlay of twelve large sprites ---- */

void OverlaySpritesStep(void)
{
    uint8_t s = g.overlay_state, bob, step;
    uint16_t a;
    int i;

    switch (s) {
    case 0:
        g.overlay_x = 0x009A;
        g.overlay_y = 0x006A;
        g.overlay_attr = 0;
        g.overlay_state = 1;
        break;
    case 2:
        a = (uint16_t)(g.overlay_x - 9);
        g.overlay_x = a;
        if ((a & 0x8000) && a < 0xFFB0)
            g.overlay_state = 1;
        break;
    case 3:
        g.overlay_x = 0xFE00;
        g.overlay_attr = 0x40;
        g.overlay_state = 4;
        break;
    case 4:
        a = (uint16_t)(g.overlay_x + 2);
        g.overlay_x = a;
        if (!(a & 0x8000) && a >= 0x0020) {
            g.overlay_x = 0x0020;
            g.overlay_state = 7;
        }
        break;
    case 5:
        g.overlay_x = 0x0100;
        g.overlay_attr = 0;
        g.overlay_state = 6;
        break;
    case 6:
        a = (uint16_t)(g.overlay_x - 3);
        g.overlay_x = a;
        if (a < 0x009A) {
            g.overlay_x = 0x009A;
            g.overlay_state = 7;
        }
        break;
    default:
        break;
    }
    if (g.frame_count & 4) {
        bob = 0;
        step = 0;
    } else {
        bob = 1;
        step = 8;
    }
    {
        uint16_t p = g.overlay_picture;
        uint8_t pal = p == cart16(0x04, 0xE066) ? 0 : p == cart16(0x04, 0xE068) ? 2 : p == cart16(0x04, 0xE06A) ? 4 : 8;
        unsigned at = g.overlay_attr ? 0x24 : 0;

        for (i = 0; i < 12; i++, at += 3) {
            uint16_t x = (uint16_t)(cart8(0x04, (uint16_t)(0xE1B6 + at)) + g.overlay_x);

            if ((x & 0x8000) || x >= 0xF0) {
                set_y(36 + i, 0xF0);
            } else {
                set_x_byte(36 + i, (uint8_t)x);
                set_y(36 + i, (uint8_t)(cart8(0x04, (uint16_t)(0xE1B7 + at)) + g.overlay_y - bob));
                set_tile_byte(36 + i, (uint8_t)(cart8(0x04, (uint16_t)(0xE1B8 + at)) + step));
                set_attr(36 + i, (uint8_t)(0x10 | pal | g.overlay_attr));
            }
        }
    }
    set_high_byte(9, 0xAA);
    set_high_byte(10, 0xAA);
    set_high_byte(11, 0xAA);
}

/* ---- text ---- */

/* A line of text as sprites: from `slot`, centred on x, tiles counted from
   tile_base; a byte with bit 7 moves the pen without a sprite. */
void OamTextLine(int slot, uint8_t tile_base, uint8_t x, uint8_t y, uint8_t attr)
{
    uint16_t at = cart16(0x07, (uint16_t)(0x8000 + (uint8_t)(g.course * 2)));
    uint8_t n = cart8(0x07, at);
    uint8_t pen = (uint8_t)(x - (uint8_t)(n * 4));

    at++;
    for (;;) {
        uint8_t c = cart8(0x07, at);

        at++;
        if (c & 0x80) {
            pen = (uint8_t)(pen + 8);
            n--;
            continue;
        }
        set_tile_byte(slot, (uint8_t)(c + tile_base));
        set_x_byte(slot, pen);
        set_y(slot, y);
        set_attr(slot, attr);
        slot++;
        pen = (uint8_t)(pen + 8);
        if (!--n)
            break;
    }
}

static void sprite_text_line_at(uint8_t x, uint8_t y)
{
    int i;

    OamTextLine(0x70, 0, x, y, 0x36);
    for (i = 0; i < 7; i++)
        set_high_byte(0x19 + i, 0);
    g.slot[0].first = 0;                /* the eighth byte falls on the record after the table */
}

void SpriteTextAt895E(void) { sprite_text_line_at(0x89, 0x5E); }
void SpriteTextAt8152(void) { sprite_text_line_at(0x81, 0x52); }
void SpriteTextAt8952(void) { sprite_text_line_at(0x89, 0x52); }

/* ---- the eight records after the table: effects on the screen's colours ---- */

/* The second record's countdown. */
void SpriteSlot1Countdown(void)
{
    if (g.slot_count == 0xFF) {
        g.slot_mask &= (uint8_t)~0x02;
        return;
    }
    g.slot_mask |= 0x02;
    g.slot[1].unused4 = 0;
    g.slot[1].unused5 = 2;
    g.slot[1].unused6 = 0xA7;
    g.slot[1].first = (uint8_t)(g.slot[1].value | 0xE0);
    if (!g.slot_count) {
        if (g.slot1_finished) {
            if (!g.slot[1].value) {
                g.slot_count--;
                return;
            }
            g.slot[1].value = (uint8_t)(g.slot[1].value - 2);
            return;
        }
        g.slot1_finished = 1;
    }
    g.slot[1].unused4 = 0;
    g.slot[1].unused6 = 0xE7;
    g.slot[1].first = 0xFF;
    g.slot_count--;
}

/* The first record in use, an index 1 on, or 0 for none. */
int SpriteSlotFirst(void)
{
    int i;

    for (i = 0; i < 8; i++)
        if (g.slot_mask & (1 << i))
            return i + 1;
    return 0;
}
