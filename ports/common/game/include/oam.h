/*
 * oam.h: the sprite table's work-RAM copy (video/oam.c) and the routines
 * that fill it: the hidden and off-screen sprites, the attribute bytes,
 * the palettes, the high table's size and X-high bits, the pieces of the
 * cars and the metasprites, and the overlay and picture turn of the pause.
 *
 * Routines that write through the data bank take it as `bank`, as the
 * original's do; the data bank's own bytes are work RAM for the banks
 * $00, $7E and $7F, the game's ROM elsewhere.
 */
#ifndef SRFX_OAM_H
#define SRFX_OAM_H

#include <stdint.h>

/* Moves `count` sprites from table byte offset `offset` below the screen
   (Y = $F0). A count of 0 means 65536. */
void oam_hide_range(uint16_t offset, uint16_t count);

/* Copies sprites 36-46 to the OAM. Returns the last byte written. */
uint8_t oam_write_sprites_36_to_46(void);

/* Every sprite off screen at X = Y = $F0 and the high table cleared. */
void oam_buffer_clear(void);
/* Every sprite's Y to $F0 and the high table to $AA (large, X bit 8 clear). */
void oam_buffer_hide_large(void);
void oam_buffer_hide_large_long(void);     /* the same, for callers in other banks */

/* Writes the sprite entries of the list at $F883 + *y until $FF, from
   sprite table offset *x; $FE hides an entry, anything else is four bytes. */
void oam_from_list(uint8_t bank, uint16_t *x, uint16_t *y);

/* Sprite 0's Y moved down by `lines`. Returns the new Y. */
uint8_t oam_sprite0_add_y(uint8_t bank, uint8_t lines);

/* The attribute byte of `count` sprites from entry `first` on set to attr.
   OamAttr36x4 = (first, 4, $36), OamAttr36x6 = (6, $36), OamAttr36x7 =
   (7, $36), OamAttr38x4 = (4, $38), OamAttr38x6 = (6, $38), OamAttr38x7 =
   (7, $38). */
void oam_attr_fill(uint8_t bank, uint16_t first, uint16_t count, uint8_t attr);

/* Sprites 3 and 9 (or 0-2) hidden at byte offset y. */
void oam_hide_entries_3_and_9(uint8_t bank, uint16_t y);
void oam_hide_entries_0_to_2(uint8_t bank, uint16_t y);

/* The palette bits (1-3) of `count` attribute bytes from *y on set to
   `bits`; *y steps past them. Returns the last attribute byte. */
uint8_t oam_set_palette(uint8_t bank, uint8_t bits, uint16_t count, uint16_t *y);

/* The three blocks of sprites (each two X bytes and three attribute bytes),
   left and right, as the original's routines name them: 59-63, 123-127 and
   52-56 (the block's table offset is $2C, $12C or $10). */
void oam_block_59_left(uint8_t bank);
void oam_block_123_left(uint8_t bank);
void oam_block_52_left(uint8_t bank);
void oam_block_59_right(uint8_t bank);
void oam_block_123_right(uint8_t bank);
void oam_block_52_right(uint8_t bank);

/* Eight sprites from the entry at *x: X from pos stepping $10, tiles from
   tile stepping 2. *x steps past them. */
void oam_column8(uint8_t bank, uint16_t *x, uint16_t pos, uint16_t tile);

/* The size and X-high bits of the high table: OamHighBits58 (bytes
   $16-$19, sprites 88-103) and OamHighBits18 (bytes $06-$09, sprites 24-39),
   each returning the last byte; the _park routines also set bytes $1C-$1F
   (sprites 112-127) or $0C-$0F (sprites 48-63) to $55, off the left edge. */
uint8_t oam_high_bits_58(uint8_t bank);
void oam_high_bits_58_park_112(uint8_t bank);
uint8_t oam_high_bits_18(uint8_t bank);
void oam_high_bits_18_park_48(uint8_t bank);

/* Palette bits of sprites, from the entries at $0D79, $0D7B and $0D75 and
   the bits in the tables at $04:E30F, $04:E26A, $04:E30B and $04:E313. */
uint8_t oam_palette_from_table_e30f(uint8_t bank, uint16_t *y);
uint8_t oam_palette_from_table_e26a(uint8_t bank, uint16_t *y);
uint8_t oam_palette_from_table_e26a_by_mode(uint8_t bank, uint16_t *y);
uint8_t oam_palettes_e28d(uint8_t bank, uint16_t *y);
uint8_t oam_palette_for_d75(uint8_t bank, uint16_t *y);

/* Hides sprites 36-56 and 96-111 (and clears their high bits). */
void oam_hide_36_and_96(uint8_t bank);
/* Hides sprites 75 up, with the HUD mode's exceptions; sets $0E0C, $0E14. */
void oam_hide_75_up(uint8_t bank);
/* Three columns of eight large sprites (40-47, 96-103, 104-111). */
void oam_large_columns(uint8_t bank);
/* Y positions of sprites 75-86, 112-127 (the game's rows of cars). */
void oam_place_rows_75_and_112(uint8_t bank);

/* Draws metasprite `group` of the $04 table into the sprite table from
   sprite `slot`; *end gets the byte offset past its last piece. Returns the
   last piece's high table byte and fifth byte, as the original leaves A. */
uint16_t metasprite_draw(uint8_t bank, uint16_t group, uint16_t slot, uint16_t *end);

/* Flashing metasprites from the $1C table: metasprite 8 (FlashingMetaspriteDraw),
   0 (FlashingMetaspriteDraw0) and the mode's pick (8 or $14 by $0D4C), with
   the palettes cycled by the phase in $0D39. *x and *y take the original's
   X and Y after the draw. */
uint16_t flashing_metasprite_draw(uint8_t bank, uint16_t *x, uint16_t *y);
uint16_t flashing_metasprite_draw0(uint8_t bank, uint16_t *x, uint16_t *y);
uint16_t flashing_metasprite_draw_by_mode(uint8_t bank, uint16_t *x, uint16_t *y);

/* The three sprites of piece set `set` (a word index into the table at
   $1C:F09E) from sprite offset *y: each adds its offsets from $10BD, $10BF
   and $10C1 and takes its attributes, the palette bits from $10C5 when set.
   *x steps past the pieces, *set gets the set's word. Returns the last
   attribute byte. */
uint8_t sprite_pieces3(uint8_t bank, uint16_t *x, uint16_t *y, uint16_t *set);

/* The piece set `set` at screen (x, y) from sprite offset *slot. The
   callers' positions: at left X $5D, right $A5 (Y $7E); at 46 Y $71, $82,
   $93 (X $46); at 62 Y $71, $83, $95 (X $62). */
uint8_t sprite_pieces_at(uint8_t bank, uint16_t set, uint16_t x, uint16_t y, uint16_t *slot);

/* Car `car`'s three sprites with palette 0, at X $51 Y $87, from *slot. */
uint8_t sprite_piece_car(uint8_t bank, uint16_t car, uint16_t *slot);

/* The two players' cars: the piece selector steps every fourth frame
   (SpritePiecesTwoCars) or every eighth (SpritePiecesTwoCarsTop, higher on
   the screen); then the bytes $0581-$0582 are set to $20 and $58. */
void sprite_pieces_two_cars(uint8_t bank);
void sprite_pieces_two_cars_top(uint8_t bank);

/* Sets the Y byte of `count` sprites from table offset x to v (each fourth
   byte on). Returns the offset past them. */
uint16_t oam_fill_every4(uint8_t bank, uint8_t v, uint16_t x, uint16_t count);

/* Puts the five letters of the word ($D5AD in the data bank) as sprites
   90-94 at X $A5 by 8, Y $4E, tiles $C0 + letter, attributes $38. */
void oam_word_row5(uint8_t bank);

/* Hides the sprites the HUD mode leaves off (see the routine). */
void oam_hide_for_hud_mode(uint8_t bank);

/* The overlay's state and its sprites (bank $04): one frame. */
void overlay_sprites_step(void);
void overlay_sprites_state0(void);

/* One frame of the picture turn: waits for $0D32 to read 0 (the caller's
   frame), then steps the picture. Data bank $07. */
void picture_turn_step(void);

#endif
