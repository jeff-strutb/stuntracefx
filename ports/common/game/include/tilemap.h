/*
 * tilemap.h: the tilemap buffers' fills and the VRAM uploads that go with
 * them (video/tilemap.c).
 *
 * Where a routine writes VRAM at the address the game's VMADDL holds, the
 * address is an argument and the routine returns the address after the
 * words it wrote. The game's VMAIN is $80 for these: each word goes to the
 * store and the address moves on a word.
 */
#ifndef SRFX_TILEMAP_H
#define SRFX_TILEMAP_H

#include <stdint.h>

/* A row of 32 tilemap words from the Super FX's picture (the offset a
   plus the word at $00F1), each with priority set, into bank `bank`'s
   buffer at $93D7 + y. */
void tilemap_row_from_gsu_ram(uint8_t bank, uint16_t a, uint16_t y);

/* 16 rows of 32 VRAM words from `vram`: row r runs from start + r, each word
   16 more than the last. Returns the next address. */
uint16_t vram_fill_gradient(uint16_t vram, uint16_t start);

/* One 32-byte tile (number `tile`, counted from the offset at $00F1) of the
   Super FX's RAM to VRAM at `vram`. Returns the next address. */
uint16_t vram_tile_from_gsu_ram(uint16_t tile, uint16_t vram);

/* The frame rows with tile base `base` ($02B0 or $01B0) to VRAM at `vram`:
   32 words of $2001 + base, then 35 of $2002 + base ($2010 + base when the
   HUD mode is 1 or 3 and the byte at $10CD is 0). Returns the next address. */
uint16_t vram_frame_rows(uint8_t bank, uint16_t base, uint16_t vram);

/* The picture (192 words of the Super FX's picture at $70:3000, each plus
   `base`) to VRAM at `vram`. Returns the next address. */
uint16_t vram_picture_rows(uint16_t base, uint16_t vram);

/* Four words from the bank $7F buffer at $BBD7 + *x to $93D7 + *x; *x moves
   on by $40. Returns the new *x. */
uint16_t tilemap_row_from_7fbbd7(uint16_t *x);

/* Four words from bank `bank` at $FB4E + *y to $93D7 + *x; *y moves on by 8,
   *x by $40. Returns the new *x. */
uint16_t tilemap_row_from_fb4e(uint8_t bank, uint16_t *x, uint16_t *y);

/* `count` buffer entries from offset x (bank $7F, $93D7): each keeps its
   attribute bits other than the palette (bits 2-4 of the high byte) and
   takes `bits`. Returns the offset past the last entry. */
uint16_t tilemap_palette_fill(uint16_t x, uint16_t count, uint8_t bits);

/* Every fourth frame, steps the palette phase ($0DD5) and gives `count`
   entries from *x the phase's palette (from the table at $07:CF2C when the
   HUD mode is 2, else $07:CF26). Returns 0 when skipped, else 1, or 2 when
   the mode is 2 or more. *x moves past the entries. */
int tilemap_palette_cycle(uint8_t bank, uint16_t *x, uint16_t count);

/* tilemap_palette_cycle over nine entries from $87, 13 from $107 and 10 from
   $187. */
void tilemap_palette_cycle_87(uint8_t bank);
void tilemap_palette_cycle_107(uint8_t bank);
void tilemap_palette_cycle_187(uint8_t bank);

/* The two framed picture panels written to VRAM (the frame rows, the sky
   gradient and the picture, for each panel). */
void screen_frame_and_pictures(uint8_t bank);

/* 1024 words of VRAM from address 0: random tiles $20-$3F from the random
   word at $0D18. Returns the last word written. */
uint16_t vram_noise_tiles(uint8_t bank);

/* Twelve 4-row columns of the level into the bank $7F buffer: each filled
   to its level from the ROM's row, the rest from the $7F:BBD7 rows. Returns
   the offset x ends at after the last column (after the fourth row of a
   full column). */
uint16_t tilemap_level_columns(void);

/* 32 entries from $93D7 (or $BBD7 when $0DA4 bit 14 is set) + *x to $E3D7 +
   *y, both pointers moving on by $40. Returns the last entry copied. */
uint16_t tilemap_row_to_e3d7(uint8_t bank, uint16_t *x, uint16_t *y);

/* The backdrop's scroll angle and its horizontal part from the view
   ($0664, $0666). Returns the value stored at $06CF. */
uint16_t backdrop_angles_from_view(uint8_t bank);

/* Palette on the entries of the strips at $87, $107 and $187 (bank $7F),
   and the one entry of each marked with another palette. */
void tilemap_row_87_palette5(void);
void tilemap_row_107_palette5(void);
void tilemap_row_187_palette5(void);
void tilemap_row_187_palette4(void);
void tilemap_row_207_palette4(void);
void tilemap_row_287_palette4(void);

/* The three digits at $1184-$1186 as tile words at $7F:9481-$9486. */
void digit_tiles_1184(void);

/* Keeps the three times at $0EE7 at $0EEA, clears $0E3F, moves the stage
   $0D59 on, sets $0D23 = 2, clears $19DF and starts the split window closing
   ($19D8 = 2). */
void times_keep_and_advance(uint8_t bank);

/* 17 rows of 26 entries from the buffer at $7F:93D7 (row r from offset
   $40 r) to VRAM from $5C00, one 32-entry row apart, then BG2's scroll from
   band 0. Returns the high byte of that band's vertical scroll. */
uint8_t tilemap_rows_to_5c00(uint8_t bank);

/* The backdrop's rows (BackdropRowsUpdate): the buffer picked by $0D7F gets
   17 rows of 32 entries from the backdrop map, and its scroll pair and
   $0D96 are set. Returns the map offset after the last row, or $20C0 after
   the fill that follows the map's end. */
uint16_t backdrop_rows_update(uint8_t bank);

#endif
