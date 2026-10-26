/*
 * sprite_text.h: text drawn as sprites (video/sprite_text.c).
 *
 * A line of text is one sprite per character, in the sprite table from a
 * given slot on, centred on an X; the spaces in it move the pen without a
 * sprite. The sprite table's high bytes for the small text are cleared
 * after it.
 */
#ifndef SRFX_SPRITE_TEXT_H
#define SRFX_SPRITE_TEXT_H

#include <stdint.h>

/* Where a line's sprites go: the first sprite (its index in the sprite
   table), the X the line is centred on, the Y, the base added to each
   character's tile, and the attributes. */
typedef struct {
    uint8_t first_sprite;
    uint8_t centre_x;
    uint8_t y;
    uint8_t tile_base;
    uint8_t attr;
} TextSprites;

/* The text line the game's $0E3D names, read in the data bank, drawn as
   sprites. Gives the X past the line's text and the Y past the last
   sprite (4 bytes each). */
void text_line_sprites(uint8_t bank, const TextSprites *t, uint16_t *end_x, uint16_t *end_y);
/* The same, for a caller in another bank (the data bank $07). */
void text_line_sprites_far(const TextSprites *t, uint16_t *end_x, uint16_t *end_y);

/* The current line at X x, Y y: sprites from slot $70, attributes $36, tiles
   from 0, then the sprite table's high bytes $058E-$0595 cleared in the
   data bank. */
void sprite_text_at(uint8_t bank, uint8_t x, uint8_t y);

/* The entries that place the line at a fixed X and Y (the copies do the
   same as the entry they copy). */
void sprite_text_at_89_5e(uint8_t bank);        /* X $89, Y $5E */
void sprite_text_at_81_52(uint8_t bank);        /* X $81, Y $52 */
void sprite_text_at_81_52_copy_b(uint8_t bank); /* as sprite_text_at_81_52 */
void sprite_text_at_89_52(uint8_t bank);        /* X $89, Y $52 */
void sprite_text_at_81_52_copy_c(uint8_t bank); /* as sprite_text_at_81_52 */
void sprite_text_at_89_52_copy_b(uint8_t bank); /* as sprite_text_at_89_52 */

#endif
