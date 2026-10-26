/*
 * sprite_text.c: lines of text drawn as sprites.
 *
 * A line is a count byte, then one byte per character; a byte with bit 7
 * set is a space (the pen steps past it, with no sprite). The lines are
 * found through a word table in ROM at $07:8000, indexed by the line
 * number the game keeps at $0E3D (its low byte, times two).
 */
#include "game.h"
#include "sprite_text.h"

#define OAM_TABLE   0x0375u     /* the sprite table's work-RAM copy (state.h: oam_buffer) */
#define HIGH_TABLE  0x058Eu     /* the sprite table's high bytes for the small sprites */
#define LINE_NUMBER 0x0E3Du     /* the line's number, a word in the data bank */
#define LINE_TABLE  0x078000u   /* the table of line pointers, in ROM */

/* A byte written in a bank the way the game writes it: work RAM ($7E-$7F,
   and the first $2000 bytes of the ROM banks), the game pak RAM ($70).
   Anything else is not kept. */
static void bank_put(uint8_t bank, uint16_t at, uint8_t v)
{
    if (bank == 0x7E || bank == 0x7F)
        wram[(uint32_t)(bank - 0x7E) << 16 | at] = v;
    else if (bank == 0x70)
        pakram[at] = v;
    else if ((bank & 0x7F) < 0x40 && at < 0x2000)
        wram[at] = v;
}

/* Where the current line starts: the pointer in the table at the line's
   number (its low seven bits, times two). */
static uint16_t text_line_start(uint8_t bank)
{
    uint8_t number = bank_byte(bank, LINE_NUMBER);
    uint16_t index = (uint8_t)(number << 1);

    return rom_word(LINE_TABLE + index);
}

/* Draws the current line as sprites, one per character from the first
   sprite on, the pen starting at centre_x less four times the count
   (each character is eight wide). Gives the X past the line's text and
   the Y past the last sprite. A space steps the pen and the count but
   gets no sprite, and the end test comes only after a sprite, so a line
   that ends in a space runs on past its count. */
void text_line_sprites(uint8_t bank, const TextSprites *t, uint16_t *end_x, uint16_t *end_y)
{
    uint16_t x = text_line_start(bank);     /* at the count byte */
    uint16_t y = (uint16_t)(t->first_sprite << 2);   /* 4 bytes a sprite */
    uint8_t left = bank_byte(bank, x);      /* characters still to draw, spaces included */
    uint8_t pen = (uint8_t)(t->centre_x - (uint8_t)(left << 2));

    x++;
    for (;;) {
        uint8_t c = bank_byte(bank, x);

        if (c & 0x80) {                     /* a space: the pen steps, no sprite */
            x++;
            pen = (uint8_t)(pen + 8);
            left--;
            continue;
        }
        bank_put(bank, (uint16_t)(OAM_TABLE + 2 + y), (uint8_t)(c + t->tile_base));
        x++;
        bank_put(bank, (uint16_t)(OAM_TABLE + y), pen);
        pen = (uint8_t)(pen + 8);
        bank_put(bank, (uint16_t)(OAM_TABLE + 1 + y), t->y);
        bank_put(bank, (uint16_t)(OAM_TABLE + 3 + y), t->attr);
        y += 4;
        left--;
        if (!left)
            break;
    }
    *end_x = x;
    *end_y = y;
}

/* The same, for a caller in another bank: the data bank is $07. */
void text_line_sprites_far(const TextSprites *t, uint16_t *end_x, uint16_t *end_y)
{
    text_line_sprites(0x07, t, end_x, end_y);
}

/* The current line at (x, y): sprites from slot $70, attributes $36,
   tiles from 0; then the sprite table's high bytes for them cleared
   (the small sprites, X bit 8 clear), in the data bank. */
void sprite_text_at(uint8_t bank, uint8_t x, uint8_t y)
{
    TextSprites t = { 0x70, x, y, 0x00, 0x36 };
    uint16_t end_x, end_y;
    unsigned i;

    text_line_sprites_far(&t, &end_x, &end_y);
    for (i = 0; i < 8; i++)
        bank_put(bank, (uint16_t)(HIGH_TABLE + i), 0);
}

void sprite_text_at_89_5e(uint8_t bank)
{
    sprite_text_at(bank, 0x89, 0x5E);
}

void sprite_text_at_81_52(uint8_t bank)
{
    sprite_text_at(bank, 0x81, 0x52);
}

void sprite_text_at_81_52_copy_b(uint8_t bank)
{
    sprite_text_at_81_52(bank);
}

void sprite_text_at_89_52(uint8_t bank)
{
    sprite_text_at(bank, 0x89, 0x52);
}

void sprite_text_at_81_52_copy_c(uint8_t bank)
{
    sprite_text_at_81_52(bank);
}

void sprite_text_at_89_52_copy_b(uint8_t bank)
{
    sprite_text_at_89_52(bank);
}
