/*
 * oam.c: the work-RAM copy of the sprite table (OAM), copied to the PPU
 * each frame. Each sprite is four bytes: X, Y, tile, attributes. The high
 * table after the 512 bytes holds, for each sprite, its X bit 8 and its
 * size bit (two bits each).
 *
 * The routines that fill the table write through a data bank, as the
 * original does: a bank of $7E, $7F or $00 (or the first $2000 of any
 * ROM bank) is work RAM, a bank of $70 the game pak RAM, anything else is
 * ROM, which is never written.
 *
 * Not ported here (see oam.h): the sprite strips and bar gauges (hud/
 * sprite_bars.c), OamDirectStage (oam_direct.c), the pause stages (they
 * run handlers in race/pause.c), the overlay's and the pause's frames that
 * call race/results_screen.c and race/pause.c, and two routines nothing
 * in the game calls.
 */
#include "game.h"
#include "glue.h"
#include "oam.h"
#include "video.h"

#define PT_BANK 0x07                    /* the picture turn's data bank */
#define OV_BANK 0x04                    /* the overlay's data bank */

/* A byte written in a data bank, as the game writes it (see the note at
   the top of this file). */
static void put8(uint8_t bank, uint16_t at, uint8_t v)
{
    if (bank == 0x7E || bank == 0x7F)
        wram[(uint32_t)(bank - 0x7E) << 16 | at] = v;
    else if (bank == 0x70)
        pakram[at] = v;
    else if ((bank & 0x7F) < 0x40 && at < 0x2000)
        wram[at] = v;
}

static void put16(uint8_t bank, uint16_t at, uint16_t v)
{
    put8(bank, at, (uint8_t)v);
    put8(bank, (uint16_t)(at + 1), (uint8_t)(v >> 8));
}

/* A byte or word as the game reads it in a data bank. */
static uint8_t get8(uint8_t bank, uint16_t at)
{
    return bank_byte(bank, at);
}

static uint16_t get16(uint8_t bank, uint16_t at)
{
    return (uint16_t)(get8(bank, at) | get8(bank, (uint16_t)(at + 1)) << 8);
}

/* A byte of the cartridge's ROM at a 24-bit address (the tables). */
static uint8_t rom8(uint32_t addr)
{
    return *rom_at(addr);
}

/* Moves `count` sprites from table byte offset `offset` below the screen
   (Y = $F0). A count of 0 means 65536. */
void oam_hide_range(uint16_t offset, uint16_t count)
{
    do {
        oam_buffer[(uint16_t)(offset + 1)] = 0xF0;
        offset = (uint16_t)(offset + 4);
    } while (--count);
}

/* Copies sprites 36-46 (the table from byte $90, 44 bytes) to the OAM, at
   the OAM word address $48. Returns the last byte written. */
uint8_t oam_write_sprites_36_to_46(void)
{
    video_sprite_table(&oam_buffer[0x90], 0x90, 0x2C);
    return oam_buffer[0x90 + 0x2B];
}

/* Every sprite off screen at X = Y = $F0, and the high table cleared. */
void oam_buffer_clear(void)
{
    unsigned x;

    for (x = 0; x < 0x200; x += 4)
        OAM_POS(x) = 0xF0F0;
    for (x = 0; x < 0x20; x++)
        oam_buffer_high[x] = 0;
}

/* Every sprite's Y to $F0, and the high table to $AA: large sprites, X
   bit 8 clear. */
void oam_buffer_hide_large(void)
{
    unsigned x;

    for (x = 0; x < 0x200; x += 4)
        oam_buffer[x + 1] = 0xF0;
    for (x = 0; x < 0x20; x++)
        oam_buffer_high[x] = 0xAA;
}

void oam_buffer_hide_large_long(void)
{
    oam_buffer_hide_large();
}

/* Writes the list at $F883 + *y (in the data bank) into the table from
   *x: an $FE item hides its entry (Y = $FE); any other item is four bytes,
   X, Y, tile and attributes. The list ends at $FF; *x and *y step past
   what they used. */
void oam_from_list(uint8_t bank, uint16_t *x, uint16_t *y)
{
    uint8_t v;

    while ((v = get8(bank, (uint16_t)(0xF883 + *y))) != 0xFF) {
        if (v == 0xFE) {
            put8(bank, (uint16_t)(0x0376 + *x), v);
        } else {
            put8(bank, (uint16_t)(0x0375 + *x), v);
            put8(bank, (uint16_t)(0x0376 + *x), get8(bank, (uint16_t)(0xF884 + *y)));
            put8(bank, (uint16_t)(0x0377 + *x), get8(bank, (uint16_t)(0xF885 + *y)));
            put8(bank, (uint16_t)(0x0378 + *x), get8(bank, (uint16_t)(0xF886 + *y)));
            *y = (uint16_t)(*y + 3);
        }
        *y = (uint16_t)(*y + 1);
        *x = (uint16_t)(*x + 4);
    }
}

/* Sprite 0's Y moved down by `lines` (an 8-bit add). Returns the new Y. */
uint8_t oam_sprite0_add_y(uint8_t bank, uint8_t lines)
{
    uint8_t s = (uint8_t)(lines + get8(bank, 0x0376));

    put8(bank, 0x0376, s);
    return s;
}

/* The attribute byte of `count` sprites from entry `first` on: attr,
   every fourth byte of the table from $0378 on. */
void oam_attr_fill(uint8_t bank, uint16_t first, uint16_t count, uint8_t attr)
{
    uint16_t x = (uint16_t)(first << 2);

    do {
        put8(bank, (uint16_t)(0x0378 + x), attr);
        x = (uint16_t)(x + 4);
    } while (--count);
}

/* Sprites 3 and 9 hidden (X = 0, Y = $F0) at byte offset y. */
void oam_hide_entries_3_and_9(uint8_t bank, uint16_t y)
{
    put16(bank, (uint16_t)(0x0381 + y), 0xF000);
    put16(bank, (uint16_t)(0x0399 + y), 0xF000);
}

/* Sprites 0, 1 and 2 hidden at byte offset y. */
void oam_hide_entries_0_to_2(uint8_t bank, uint16_t y)
{
    put16(bank, (uint16_t)(0x0375 + y), 0xF000);
    put16(bank, (uint16_t)(0x0379 + y), 0xF000);
    put16(bank, (uint16_t)(0x037D + y), 0xF000);
}

/* The palette bits (1-3) of `count` attribute bytes from *y on set to
   `bits`; the other bits kept. *y steps past them. Returns the last byte. */
uint8_t oam_set_palette(uint8_t bank, uint8_t bits, uint16_t count, uint16_t *y)
{
    uint8_t v = 0;

    do {
        v = (uint8_t)(get8(bank, (uint16_t)(0x0378 + *y)) & 0xF1);
        v = (uint8_t)(v | bits);
        put8(bank, (uint16_t)(0x0378 + *y), v);
        *y = (uint16_t)(*y + 4);
    } while (--count);
    return v;
}

/* A block of sprites: the X bytes at the block's x0 and x0 + dx (the
   second and third of three), and the attributes: attr_a for the first
   two, attr_b for the last. The block's table at $0435 + x. */
static void oam_block(uint8_t bank, uint16_t x, uint8_t x0, uint8_t dx, uint8_t attr_a, uint8_t attr_b)
{
    put8(bank, (uint16_t)(0x0435 + x), x0);
    put8(bank, (uint16_t)(0x0439 + x), (uint8_t)(x0 + dx));
    put8(bank, (uint16_t)(0x0440 + x), attr_a);
    put8(bank, (uint16_t)(0x0444 + x), attr_a);
    put8(bank, (uint16_t)(0x0448 + x), attr_b);
}

/* Sprites 59-63 (OamBlock59Left): X $30, $52; attributes $0C, $0C, $06. */
void oam_block_59_left(uint8_t bank)
{
    oam_block(bank, 0x002C, 0x30, 0x22, 0x0C, 0x06);
}

/* Sprites 123-127 (OamBlock123Left): X $7E, $A0; attributes $0C, $0C, $06. */
void oam_block_123_left(uint8_t bank)
{
    oam_block(bank, 0x012C, 0x7E, 0x22, 0x0C, 0x06);
}

/* Sprites 52-56 (OamBlock52Left): X $50, $72; attributes $0C, $0C, $06. */
void oam_block_52_left(uint8_t bank)
{
    oam_block(bank, 0x0010, 0x50, 0x22, 0x0C, 0x06);
}

/* Sprites 59-63 (OamBlock59Right): X $5F, $79; attributes $06, $06, $0C. */
void oam_block_59_right(uint8_t bank)
{
    oam_block(bank, 0x002C, 0x5F, 0x1A, 0x06, 0x0C);
}

/* Sprites 123-127 (OamBlock123Right): X $AD, $C7; attributes $06, $06, $0C. */
void oam_block_123_right(uint8_t bank)
{
    oam_block(bank, 0x012C, 0xAD, 0x1A, 0x06, 0x0C);
}

/* Sprites 52-56 (OamBlock52Right): X $87, $A1; attributes $06, $06, $0C. */
void oam_block_52_right(uint8_t bank)
{
    oam_block(bank, 0x0010, 0x87, 0x1A, 0x06, 0x0C);
}

/* Eight sprites from the entry at *x: X from pos stepping $10, tiles from
   tile stepping 2 (the attributes not touched). *x steps past them. */
void oam_column8(uint8_t bank, uint16_t *x, uint16_t pos, uint16_t tile)
{
    unsigned i;

    for (i = 0; i < 8; i++) {
        put16(bank, (uint16_t)(0x0375 + *x), pos);
        pos = (uint16_t)(pos + 0x10);
        put16(bank, (uint16_t)(0x0377 + *x), tile);
        tile = (uint16_t)(tile + 2);
        *x = (uint16_t)(*x + 4);
    }
}

/* The size and X-high bits of sprites 88-103: $0E15 set to $F8 and the
   high table bytes $16-$19 ORed with $40, $55, $55, $15. Returns the last
   byte. */
uint8_t oam_high_bits_58(uint8_t bank)
{
    static const uint8_t bits[4] = {0x40, 0x55, 0x55, 0x15};
    uint8_t v = 0;
    unsigned i;

    put8(bank, 0x0E15, 0xF8);
    for (i = 0; i < 4; i++) {
        v = (uint8_t)(get8(bank, (uint16_t)(0x058B + i)) | bits[i]);
        put8(bank, (uint16_t)(0x058B + i), v);
    }
    return v;
}

/* oam_high_bits_58, then sprites 112-127 parked off the left edge (the high
   table bytes $1C-$1F = $55). */
void oam_high_bits_58_park_112(uint8_t bank)
{
    oam_high_bits_58(bank);
    put16(bank, 0x0591, 0x5555);
    put16(bank, 0x0593, 0x5555);
}

/* The same for sprites 24-39 ($0E14 and the high table bytes $06-$09). */
uint8_t oam_high_bits_18(uint8_t bank)
{
    static const uint8_t bits[4] = {0x40, 0x55, 0x55, 0x15};
    uint8_t v = 0;
    unsigned i;

    put8(bank, 0x0E14, 0xF8);
    for (i = 0; i < 4; i++) {
        v = (uint8_t)(get8(bank, (uint16_t)(0x057B + i)) | bits[i]);
        put8(bank, (uint16_t)(0x057B + i), v);
    }
    return v;
}

/* oam_high_bits_18, then sprites 48-63 parked off the left edge (the high
   table bytes $0C-$0F = $55). */
void oam_high_bits_18_park_48(uint8_t bank)
{
    oam_high_bits_18(bank);
    put16(bank, 0x0581, 0x5555);
    put16(bank, 0x0583, 0x5555);
}

/* 22 sprites from the entry $0D79 * 4 with the palette bits from the table
   at $04:E30F for $0D48's low byte. */
uint8_t oam_palette_from_table_e30f(uint8_t bank, uint16_t *y)
{
    *y = (uint16_t)(get16(bank, 0x0D79) << 2);
    return oam_set_palette(bank, rom8(0x04E30F + (get16(bank, 0x0D48) & 0x00FF)), 0x16, y);
}

/* 22 sprites from entry $0D7B + 13 with the bits $0A, or from the table at
   $04:E26A for ($10D8 - 1)'s low byte when $10D8 is set. */
uint8_t oam_palette_from_table_e26a(uint8_t bank, uint16_t *y)
{
    uint16_t n;
    uint8_t bits;

    *y = (uint16_t)((uint16_t)(get16(bank, 0x0D7B) + 0x0D) << 2);
    n = get16(bank, 0x10D8);
    bits = n ? rom8(0x04E26A + (uint8_t)(n - 1)) : 0x0A;
    return oam_set_palette(bank, bits, 0x16, y);
}

/* 18 sprites from the entry $0D79 * 4 with the bits from the table at
   $04:E26A for $0D48's low byte. */
uint8_t oam_palette_from_table_e26a_by_mode(uint8_t bank, uint16_t *y)
{
    *y = (uint16_t)(get16(bank, 0x0D79) << 2);
    return oam_set_palette(bank, rom8(0x04E26A + (get16(bank, 0x0D48) & 0x00FF)), 0x12, y);
}

/* Three runs: 18 sprites from entry $0D75 with the table at $04:E30B for
   $0D48's low byte; 6 sprites from entry $0D75 + $16 with the table at
   $04:E313 for $0D89's; 6 from $0D75 + $1C for $0D8A's. Returns the last. */
uint8_t oam_palettes_e28d(uint8_t bank, uint16_t *y)
{
    *y = (uint16_t)(get16(bank, 0x0D75) << 2);
    oam_set_palette(bank, rom8(0x04E30B + (get16(bank, 0x0D48) & 0x00FF)), 0x12, y);
    *y = (uint16_t)((uint16_t)(get16(bank, 0x0D75) + 0x16) << 2);
    oam_set_palette(bank, rom8(0x04E313 + (get16(bank, 0x0D89) & 0x00FF)), 0x06, y);
    *y = (uint16_t)((uint16_t)(get16(bank, 0x0D75) + 0x1C) << 2);
    return oam_set_palette(bank, rom8(0x04E313 + (get16(bank, 0x0D8A) & 0x00FF)), 0x06, y);
}

/* 22 sprites (the count the original passes, $16) from entry $0D75 + 13
   with the palette bits $0A. */
uint8_t oam_palette_for_d75(uint8_t bank, uint16_t *y)
{
    *y = (uint16_t)((uint16_t)(get16(bank, 0x0D75) + 0x0D) << 2);
    return oam_set_palette(bank, 0x0A, 0x0016, y);
}

/* Hides sprites 36-56 and 96-111, and clears the size and X-high bits of
   sprites 96-111 (high table bytes $18-$1B). */
void oam_hide_36_and_96(uint8_t bank)
{
    unsigned i;

    oam_hide_range(0x0090, 0x0015);
    oam_hide_range(0x0180, 0x0010);
    for (i = 0; i < 4; i++)
        put8(bank, (uint16_t)(0x058D + i), 0x00);
}

/* $0E0C and $0E14 to $F8, and sprites 75-127 hidden; with the HUD mode
   ($0D2B) set, sprites 87-95 are left alone. */
void oam_hide_75_up(uint8_t bank)
{
    put8(bank, 0x0E0C, 0xF8);
    put8(bank, 0x0E14, 0xF8);
    if (get8(bank, 0x0D2B)) {
        oam_hide_range(0x012C, 0x000C);
        oam_hide_range(0x0180, 0x0020);
    } else {
        oam_hide_range(0x012C, 0x0035);
    }
}

/* Three columns of eight large sprites: 40-47 at $4840 with tiles from
   $0660, 96-103 at $5840 from $0680, 104-111 at $6840 from $06A0. Their
   high table bytes (sizes) set to $AA. */
void oam_large_columns(uint8_t bank)
{
    static const uint16_t pos[3] = {0x4840, 0x5840, 0x6840};
    static const uint16_t tile[3] = {0x0660, 0x0680, 0x06A0};
    static const uint16_t first[3] = {0x00A0, 0x0180, 0x01A0};
    static const uint16_t high[6] = {0x057F, 0x0580, 0x058D, 0x058E, 0x058F, 0x0590};
    unsigned i;

    for (i = 0; i < 3; i++) {
        uint16_t x = first[i];

        oam_column8(bank, &x, pos[i], tile[i]);
    }
    for (i = 0; i < 6; i++)
        put8(bank, high[i], 0xAA);
}

/* Sprite Y positions: 75-86 at $93, 112-113 at $70, 114-115 at $80 and,
   unless $10CD is set, 116-127 at $8C or $94 (119, 121, 123, 125 and 127
   at $94). The high table bytes for sprites 40-47 and 96-111 cleared. */
void oam_place_rows_75_and_112(uint8_t bank)
{
    static const uint8_t at8c[7] = {116, 117, 118, 120, 122, 124, 126};
    static const uint8_t at94[5] = {119, 121, 123, 125, 127};
    static const uint16_t high[6] = {0x057F, 0x0580, 0x058D, 0x058E, 0x058F, 0x0590};
    unsigned i;

    if (!get8(bank, 0x10CD)) {
        for (i = 0; i < 7; i++)
            put8(bank, (uint16_t)(0x0376 + 4 * at8c[i]), 0x8C);
        for (i = 0; i < 5; i++)
            put8(bank, (uint16_t)(0x0376 + 4 * at94[i]), 0x94);
    }
    for (i = 75; i <= 86; i++)
        put8(bank, (uint16_t)(0x0376 + 4 * i), 0x93);
    put8(bank, (uint16_t)(0x0376 + 4 * 112), 0x70);
    put8(bank, (uint16_t)(0x0376 + 4 * 113), 0x70);
    put8(bank, (uint16_t)(0x0376 + 4 * 114), 0x80);
    put8(bank, (uint16_t)(0x0376 + 4 * 115), 0x80);
    for (i = 0; i < 6; i++)
        put8(bank, high[i], 0x00);
}

/* Draws metasprite `group` (a group table at $04:EDCB, 4 bytes each: the
   first piece, the piece count) into the sprite table from sprite `slot`:
   each 5-byte piece at $04:EE4B adds its X and Y offsets to the origin at
   $0D3B/$0D3C and takes its tile and attributes. A piece whose fifth byte
   has bit 0 clear is made small (its size bit cleared in the high table).
   The count runs as the original's byte count. *end gets the byte offset
   past the last piece. Returns the high byte from the group's first piece
   (or the last size mask) and the last fifth byte, as the original's A. */
uint16_t metasprite_draw(uint8_t bank, uint16_t group, uint16_t slot, uint16_t *end)
{
    uint16_t x = (uint16_t)(group << 2), y = (uint16_t)(slot << 2), mask, hi, first;
    uint8_t n = (uint8_t)rom_word(0x04EDCD + x), lo = 0, v, o;

    first = rom_word(0x04EDCB + x);
    x = first;
    hi = (uint16_t)(x & 0xFF00);
    do {
        put8(bank, (uint16_t)(0x0375 + y), (uint8_t)(rom8(0x04EE4B + x) + get8(bank, 0x0D3B)));
        o = get8(bank, 0x0D3C);
        put8(bank, (uint16_t)(0x0376 + y), (uint8_t)(rom8(0x04EE4C + x) + o));
        put8(bank, (uint16_t)(0x0377 + y), rom8(0x04EE4D + x));
        put8(bank, (uint16_t)(0x0378 + y), rom8(0x04EE4E + x));
        lo = rom8(0x04EE4F + x);
        if (!(lo & 0x01)) {
            /* the size mask for the sprite's place: two bytes at $02:FA8D */
            mask = get16(bank, (uint16_t)(0xFA8D + ((y >> 2) & 0x0003)));
            hi = (uint16_t)(mask & 0xFF00);
            v = W8(0x0575 + (y >> 4));
            lo = (uint8_t)(v & (uint8_t)mask);
            W8(0x0575 + (y >> 4)) = lo;
        }
        x = (uint16_t)(x + 5);
        y = (uint16_t)(y + 4);
        n--;
    } while (n);
    *end = y;
    return (uint16_t)(hi | lo);
}

/* The palette bits of a flashing piece's attribute: phase 1 and phase 2
   swap the three palettes (bits 1-3) around, by which of bits 3 and 1 the
   piece's own attributes have set (bit 3, else bit 1, else neither). */
static uint8_t cycled_attr(uint8_t attr, uint8_t phase)
{
    static const uint8_t pal[2][3] = {{0x04, 0x08, 0x06}, {0x06, 0x04, 0x08}};
    unsigned k = (attr & 0x08) ? 0 : (attr & 0x02) ? 1 : 2;

    return (uint8_t)((attr & 0xF1) | pal[phase == 1][k]);
}

/* The flashing metasprite of set `set` (a word at $0D35) from the $1C
   tables: the high table reset to large sprites, then, when the position
   at $0D37/$0D38 is not 0, its pieces drawn from sprite 0 with the palettes
   cycled by the phase in $0D39 (stepped every fourth frame, 0 to 2). The
   group table at $1C:E5AC (first piece, count), the 5-byte pieces at
   $1C:E62C. When not drawn, *x is $20 and the result 0. */
static uint16_t flashing_draw(uint8_t bank, uint16_t set, uint16_t *x, uint16_t *y)
{
    uint16_t i, ph, idx, hi;
    uint8_t n, a = 0, phase;

    put16(bank, 0x0D35, set);
    for (i = 0; i < 0x20; i++)
        put16(bank, (uint16_t)(0x0575 + i), 0xAAAA);
    if (!get16(bank, 0x0D37)) {
        *x = 0x0020;
        return 0;
    }
    if (!(get16(bank, 0x0E3B) & 0x0003)) {
        ph = (uint16_t)(get16(bank, 0x0D39) + 1);
        put16(bank, 0x0D39, ph < 3 ? ph : 0);
    }
    idx = (uint16_t)(get16(bank, 0x0D35) << 2);
    n = (uint8_t)rom_word(0x1CE5AE + idx);
    *x = rom_word(0x1CE5AC + idx);
    *y = 0;
    hi = (uint16_t)(*x & 0xFF00);
    do {
        put8(bank, (uint16_t)(0x0375 + *y), (uint8_t)(rom8(0x1CE62C + *x) + get8(bank, 0x0D37)));
        put8(bank, (uint16_t)(0x0376 + *y), (uint8_t)(rom8(0x1CE62D + *x) + get8(bank, 0x0D38)));
        put8(bank, (uint16_t)(0x0377 + *y), rom8(0x1CE62E + *x));
        phase = get8(bank, 0x0D39);
        a = rom8(0x1CE62F + *x);
        put8(bank, (uint16_t)(0x0378 + *y), phase ? cycled_attr(a, phase) : a);
        a = rom8(0x1CE630 + *x);
        *x = (uint16_t)(*x + 5);
        *y = (uint16_t)(*y + 4);
        n--;
    } while (n);
    return (uint16_t)(hi | a);
}

/* FlashingMetaspriteDraw: metasprite 8 of the set. */
uint16_t flashing_metasprite_draw(uint8_t bank, uint16_t *x, uint16_t *y)
{
    return flashing_draw(bank, 0x0008, x, y);
}

/* FlashingMetaspriteDraw0: metasprite 0 of the set. */
uint16_t flashing_metasprite_draw0(uint8_t bank, uint16_t *x, uint16_t *y)
{
    return flashing_draw(bank, 0x0000, x, y);
}

/* FlashingMetaspriteDrawByMode: metasprite $14 when $0D4C's low byte is
   set, else $0E. */
uint16_t flashing_metasprite_draw_by_mode(uint8_t bank, uint16_t *x, uint16_t *y)
{
    uint16_t set = (get16(bank, 0x0D4C) & 0x00FF) ? 0x0014 : 0x000E;

    return flashing_draw(bank, set, x, y);
}

/* The three sprites of the piece set `set` (a word index into the table at
   $1C:F09E, read from the set's own offset plus $10BB's selector and
   $10C3): each piece adds its offsets from $10BD, $10BF and $10C1 and takes
   its attribute (with the palette bits from $10C5 when set). */
uint8_t sprite_pieces3(uint8_t bank, uint16_t *x, uint16_t *y, uint16_t *set)
{
    uint16_t i = rom8(0x07B981 + (get16(bank, 0x10BB) & 0x00FF));
    uint8_t attr = 0;
    unsigned n;

    i = (uint16_t)(i + get16(bank, 0x10C3));
    *x = *set = rom_word(0x1CF09E + ((uint16_t)(i << 2) & 0x00FF));
    for (n = 0; n < 3; n++) {
        uint8_t add = rom8(0x1CF120 + *x);

        put8(bank, (uint16_t)(0x0375 + *y), (uint8_t)(get8(bank, 0x10BD) + rom8(0x1CF11E + *x)));
        put8(bank, (uint16_t)(0x0376 + *y), (uint8_t)(get8(bank, 0x10BF) + rom8(0x1CF11F + *x)));
        put8(bank, (uint16_t)(0x0377 + *y), (uint8_t)(get8(bank, 0x10C1) + add));
        if (get8(bank, 0x10C5))
            attr = (uint8_t)((rom8(0x1CF121 + *x) & 0xF1) | get8(bank, 0x10C5));
        else
            attr = rom8(0x1CF121 + *x);
        put8(bank, (uint16_t)(0x0378 + *y), attr);
        *x = (uint16_t)(*x + 5);
        *y = (uint16_t)(*y + 4);
    }
    return attr;
}

/* The piece set `set` at screen (x, y): the set into $10C3 and the
   position into $10BD and $10BF, then sprite_pieces3 from *slot. */
uint8_t sprite_pieces_at(uint8_t bank, uint16_t set, uint16_t x, uint16_t y, uint16_t *slot)
{
    uint16_t px, pset;

    put16(bank, 0x10C3, set);
    put16(bank, 0x10BD, x);
    put16(bank, 0x10BF, y);
    return sprite_pieces3(bank, &px, slot, &pset);
}

/* Car `car`'s three sprites with palette 0, at X $51 and Y $87: the set
   worked out in $10C3 as (car * 3) & $0F, $10C5 cleared. */
uint8_t sprite_piece_car(uint8_t bank, uint16_t car, uint16_t *slot)
{
    uint16_t px, pset, v;

    put16(bank, 0x10C3, car);
    v = (uint16_t)(((uint16_t)(car << 1)) + get16(bank, 0x10C3)) & 0x000F;
    put16(bank, 0x10C3, v);
    put16(bank, 0x10C5, 0x0000);
    put16(bank, 0x10BD, 0x0051);
    put16(bank, 0x10BF, 0x0087);
    return sprite_pieces3(bank, &px, slot, &pset);
}

/* The cars: the selector $10BB steps when the frame count's low bits are
   all set (mask), through 0-3; the tile and palette base $30; Y at `y`;
   player 1's car (its selector: $0D89 * 3) at X `x1` into sprite offset
   $C0, player 2's ($0D8A * 3) at `x2` into $CC; then the bytes $20 and
   $58 at $0581-$0582. */
static void two_cars(uint8_t bank, uint8_t mask, uint8_t y, uint8_t x1, uint8_t x2)
{
    uint8_t v, n;
    uint16_t px, pset, slot;

    if ((get8(bank, 0x0E3B) & mask) == mask) {
        v = (uint8_t)(get8(bank, 0x10BB) + 1);
        if (v >= 0x04)
            v = 0x00;
        put8(bank, 0x10BB, v);
    }
    put8(bank, 0x10C1, 0x30);
    put8(bank, 0x10C5, 0x30);
    put8(bank, 0x10BF, y);
    put8(bank, 0x10BD, x1);
    n = get8(bank, 0x0D89);
    n = (uint8_t)((n << 1) + get8(bank, 0x0D89));
    put8(bank, 0x10C3, n);
    slot = 0x00C0;
    sprite_pieces3(bank, &px, &slot, &pset);
    n = get8(bank, 0x0D8A);
    n = (uint8_t)((n << 1) + get8(bank, 0x0D8A));
    put8(bank, 0x10C3, n);
    put8(bank, 0x10BD, x2);
    slot = 0x00CC;
    sprite_pieces3(bank, &px, &slot, &pset);
    put8(bank, 0x0581, 0x20);
    put8(bank, 0x0582, 0x58);
}

/* SpritePiecesTwoCars: the selector steps when the frame count's low two
   bits are 3; the cars at Y $A0, X $84 and $CC. */
void sprite_pieces_two_cars(uint8_t bank)
{
    two_cars(bank, 0x03, 0xA0, 0x84, 0xCC);
}

/* SpritePiecesTwoCarsTop: the selector steps when the low three bits are
   7; the cars higher up, at Y $38, X $34 and $7C. */
void sprite_pieces_two_cars_top(uint8_t bank)
{
    two_cars(bank, 0x07, 0x38, 0x34, 0x7C);
}

/* Sets the Y byte of `count` sprites from table offset x, every fourth
   byte on, to v. Returns the offset past them. */
uint16_t oam_fill_every4(uint8_t bank, uint8_t v, uint16_t x, uint16_t count)
{
    do {
        put8(bank, (uint16_t)(0x0376 + x), v);
        x = (uint16_t)(x + 4);
    } while (--count);
    return x;
}

/* The five letters of the word in the data bank at $D5AD as sprites 90-94:
   X from $A5 by 8, Y $4E, tile $C0 + letter, attributes $38. */
void oam_word_row5(uint8_t bank)
{
    uint8_t x = 0xA5;
    unsigned k;

    for (k = 0; k < 5; k++) {
        uint16_t at = (uint16_t)(0x04E1 + 4 * k);

        put8(bank, at, x);
        put8(bank, (uint16_t)(at + 1), 0x4E);
        put8(bank, (uint16_t)(at + 2), (uint8_t)(get8(bank, (uint16_t)(0xD5AD + k)) + 0xC0));
        put8(bank, (uint16_t)(at + 3), 0x38);
        x = (uint8_t)(x + 8);
    }
}

/* Sprites 36-56 off the screen at Y $E0, and the HUD mode ($0D2B) hides
   more: mode 0 sprites 75-127; mode 2 no more; other modes 75-87 and
   96-127. $0E0C and $0E14 set to $E0 too. */
void oam_hide_for_hud_mode(uint8_t bank)
{
    uint8_t v;

    put8(bank, 0x0E0C, 0xE0);
    put8(bank, 0x0E14, 0xE0);
    oam_fill_every4(bank, 0xE0, 0x0090, 0x0015);
    v = get8(bank, 0x0D2B);
    if (!v) {
        oam_fill_every4(bank, 0xE0, 0x012C, 0x0035);
    } else if (v != 0x02) {
        oam_fill_every4(bank, 0xE0, 0x012C, 0x000D);
        oam_fill_every4(bank, 0xE0, 0x0180, 0x0020);
    }
}

/* The overlay's start (state 0): at (154, 106), plain, state $10A2 = 1. */
static void overlay_state0(void)
{
    put16(OV_BANK, 0x10A4, 0x009A);
    put16(OV_BANK, 0x10A6, 0x006A);
    put16(OV_BANK, 0x10A8, 0x0000);
    put16(OV_BANK, 0x10A2, 0x0001);
}

/* The overlay's twelve sprites (36-47), drawn from the table at $04:E1B6
   (X, Y, tile; the second set of twelve at $E1DA when $10A8 is set): off
   screen when the X is negative or past $EF; Y bobbing up one and the tile
   stepped while bit 2 of the frame count is clear; the attributes $10 with
   the palette from the picture at $10A0 ($04:E066, $E068, $E06A), ORed with
   $10A8. The high table bytes $09-$0B (sprites 36-47 large) set to $AA. */
static void overlay_draw(void)
{
    uint8_t bob, step, v, attr;
    uint16_t a, pic, x, y;

    if (get8(OV_BANK, 0x0E3B) & 0x04) {
        bob = 0x00;
        step = 0x00;
    } else {
        bob = 0x01;
        step = 0x08;
    }
    pic = get16(OV_BANK, 0x10A0);
    a = pic == rom_word(0x04E066) ? 0x0000 : pic == rom_word(0x04E068) ? 0x0002
        : pic == rom_word(0x04E06A) ? 0x0004 : 0x0008;
    attr = (uint8_t)(a | get8(OV_BANK, 0x10A8));
    x = get8(OV_BANK, 0x10A8) ? 0x0024 : 0x0000;
    for (y = 0x0090; y < 0x00C0; y = (uint16_t)(y + 4), x = (uint16_t)(x + 3)) {
        a = (uint16_t)(rom8(0x04E1B6 + x) + get16(OV_BANK, 0x10A4));
        if ((a & 0x8000) || a >= 0x00F0) {
            put8(OV_BANK, (uint16_t)(0x0376 + y), 0xF0);
        } else {
            put8(OV_BANK, (uint16_t)(0x0375 + y), (uint8_t)a);
            v = (uint8_t)(rom8(0x04E1B7 + x) + get8(OV_BANK, 0x10A6));
            v = (uint8_t)(v - bob);
            put8(OV_BANK, (uint16_t)(0x0376 + y), v);
            put8(OV_BANK, (uint16_t)(0x0377 + y), (uint8_t)(rom8(0x04E1B8 + x) + step));
            put8(OV_BANK, (uint16_t)(0x0378 + y), (uint8_t)(0x10 | attr));
        }
    }
    put8(OV_BANK, 0x057E, 0xAA);
    put8(OV_BANK, 0x057F, 0xAA);
    put8(OV_BANK, 0x0580, 0xAA);
}

/* OverlaySpritesStep: the overlay's state $10A2 (through its table at
   $04:E056) moves it, and its sprites are drawn. States 1 and 7 hold it. */
void overlay_sprites_step(void)
{
    uint8_t s = get8(OV_BANK, 0x10A2);
    uint16_t a;

    switch (rom_word(0x04E056 + (uint8_t)(s << 1))) {
    case 0xE06E:                        /* state 0: set up */
        overlay_state0();
        break;
    case 0xE088:                        /* state 2: slide left 9 a frame until below -80 */
        a = (uint16_t)(get16(OV_BANK, 0x10A4) - 9);
        put16(OV_BANK, 0x10A4, a);
        if ((a & 0x8000) && a < 0xFFB0)
            put16(OV_BANK, 0x10A2, 0x0001);
        break;
    case 0xE0A3:                        /* state 3: to x = -512, $10A8 = $40 */
        put16(OV_BANK, 0x10A4, 0xFE00);
        put16(OV_BANK, 0x10A8, 0x0040);
        put16(OV_BANK, 0x10A2, (uint16_t)(get16(OV_BANK, 0x10A2) + 1));
        break;
    case 0xE0B6:                        /* state 4: slide right 2 a frame to x = 32 */
        a = (uint16_t)(get16(OV_BANK, 0x10A4) + 2);
        put16(OV_BANK, 0x10A4, a);
        if (!((a & 0x8000) || a < 0x0020)) {
            put16(OV_BANK, 0x10A4, 0x0020);
            put16(OV_BANK, 0x10A2, 0x0007);
        }
        break;
    case 0xE0D7:                        /* state 5: to x = 256, plain */
        put16(OV_BANK, 0x10A4, 0x0100);
        put16(OV_BANK, 0x10A8, 0x0000);
        put16(OV_BANK, 0x10A2, (uint16_t)(get16(OV_BANK, 0x10A2) + 1));
        break;
    case 0xE0E7:                        /* state 6: slide left 3 a frame to x = 154 */
        a = (uint16_t)(get16(OV_BANK, 0x10A4) - 3);
        put16(OV_BANK, 0x10A4, a);
        if (a < 0x009A) {
            put16(OV_BANK, 0x10A4, 0x009A);
            put16(OV_BANK, 0x10A2, 0x0007);
        }
        break;
    default:                            /* states 1 and 7: hold */
        break;
    }
    overlay_draw();
}

/* OverlaySpritesState0: state 0 entered from its table: set up, then draw. */
void overlay_sprites_state0(void)
{
    overlay_state0();
    overlay_draw();
}

/* One frame of the picture turn: the delay in $0D31 counts down first;
   otherwise the delay is set from the buttons in $0D1E (X 3, Y 2, L 6,
   R 8, none 4), the picture number $0D33 moves back (Select held, $0309
   bit 13; 0 wraps to 7) or on, and the table at $07:C648 (three bytes each:
   the source word, the bank byte) is drawn: GsuCF42To3000 with $0D7F
   nonzero, else GsuCF42ToC000, then Super FX program $07:EE59 or $07:EE50.
   A table word of $8000 or more ends the search; below it is a picture
   number to go to instead. $0D32 is decremented at the end.
   The wait for $0D32 to read 0 (the vertical blank's poll) is the caller's
   frame: the port starts once $0D32 reads 0. */
void picture_turn_step(void)
{
    uint16_t m, k, t, a, y;
    uint8_t x;

    if (get8(PT_BANK, 0x0D31)) {
        put8(PT_BANK, 0x0D31, (uint8_t)(get8(PT_BANK, 0x0D31) - 1));
        return;
    }
    m = get16(PT_BANK, 0x0D1E);
    k = (m & 0x0040) ? 3 : (m & 0x4000) ? 2 : (m & 0x0020) ? 6 : (m & 0x0010) ? 8 : 4;
    put8(PT_BANK, 0x0D31, (uint8_t)k);
    if (get16(PT_BANK, 0x0309) & 0x2000) {
        t = (uint16_t)(get16(PT_BANK, 0x0D33) - 1);
        if (t & 0x8000)
            t = 0x0007;
    } else {
        t = (uint16_t)(get16(PT_BANK, 0x0D33) + 1);
    }
    for (;;) {
        put16(PT_BANK, 0x0D33, t);
        y = (uint16_t)((uint16_t)(t << 1) + get16(PT_BANK, 0x0D33));
        x = (uint8_t)get16(PT_BANK, (uint16_t)(0xC64A + y));
        a = get16(PT_BANK, (uint16_t)(0xC648 + y));
        if (a >= 0x8000)
            break;
        t = a;
    }
    if (get16(PT_BANK, 0x0D7F)) {
        gsu_unpack_to(a, x, 0x3000);
        gsu_run(0x07, 0xEE59);
    } else {
        gsu_unpack_to(a, x, 0xC000);
        gsu_run(0x07, 0xEE50);
    }
    put8(PT_BANK, 0x0D32, (uint8_t)(get8(PT_BANK, 0x0D32) - 1));
}
