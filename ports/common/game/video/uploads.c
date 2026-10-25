/*
 * uploads.c: the graphics the game sends for the screen: blocks of tiles
 * and maps from work RAM and from the Super FX's RAM, the sprite table,
 * the palette.
 */
#include "game.h"
#include "video.h"

#define palette_buffer      0xFD65u     /* work RAM: the 256 colours as the game last built them */
#define gsu_picture_waiting W8(0x0D32)
#define gsu_buffer_select   W8(0x0D7F)

/* n bytes from `at` in a 64 KB bank (wrapping in it) to the store */
static void store_from(const uint8_t *bank, uint16_t at, unsigned n, uint16_t word)
{
    uint8_t buf[2];

    while (n >= 2) {
        buf[0] = bank[at], buf[1] = bank[(uint16_t)(at + 1)];
        video_store_write(word++, buf, 2);
        at = (uint16_t)(at + 2);
        n -= 2;
    }
    if (n) {
        buf[0] = bank[at];
        video_store_write(word, buf, 1);
    }
}

/* VramDmaFromBank7F: `n` bytes of work RAM's second bank from `at` to the
   store at `word` (0: 64 K). */
void video_from_bank_7f(uint16_t n, uint16_t at, uint16_t word)
{
    store_from(wram + 0x10000, at, n ? n : 0x10000, word);
}

/* VramDmaFromGsuRam: `n` bytes of the Super FX's RAM. */
void video_from_gsu_ram(uint16_t n, uint16_t at, uint16_t word)
{
    store_from(pakram, at, n ? n : 0x10000, word);
}

/* VramDmaRowsFromGsuRam: `rows` rows of $140 bytes ($200 apart in the
   Super FX's RAM), one after another in the store (0 rows: 256). */
void video_rows_from_gsu_ram(uint16_t at, uint16_t word, uint8_t rows)
{
    unsigned r = rows ? rows : 256;

    for (; r; r--, at = (uint16_t)(at + 0x0200), word = (uint16_t)(word + 0xA0))
        store_from(pakram, at, 0x0140, word);
}

/* VramDmaWideRowsFromGsuRam: rows of $2C0 bytes ($400 apart), `rows`
   counting down to -1 as a signed byte. */
void video_wide_rows_from_gsu_ram(uint8_t rows, uint16_t at, uint16_t word)
{
    do {
        store_from(pakram, at, 0x02C0, word);
        word = (uint16_t)(word + 0x0160);
        at = (uint16_t)(at + 0x0400);
    } while (!(--rows & 0x80));
}

/* VramWriteRowFromGsuRam: one row of 64 words from $3000 plus the offset
   in direct page $02, which then moves a row down ($200). */
uint16_t video_row_from_gsu_ram(uint16_t word)
{
    store_from(pakram, (uint16_t)(0x3000 + W16(0x0002)), 0x80, word);
    W16(0x0002) = (uint16_t)(W16(0x0002) + 0x0200);
    return W16(0x0002);
}

/* CoursePictureToVram: four rows of the course's picture as the Super FX
   drew it ($0E3D's place in the table at $7F:0B96, work RAM) to $6480,
   $6580, $64C0, $65C0. */
void video_course_picture(void)
{
    uint16_t w = W16(0x10B96 + 2 * W8(0x0E3D)), t = (uint16_t)(w & 0xFFFC);

    t = (uint16_t)((uint16_t)(t >> 8 | t << 8) << 1);
    W16(0x0002) = (uint16_t)(t + ((w & 3) << 7));
    video_row_from_gsu_ram(0x6480);
    video_row_from_gsu_ram(0x6580);
    video_row_from_gsu_ram(0x64C0);
    video_row_from_gsu_ram(0x65C0);
}

/* VramUploadGsuBuffer: a waiting Super FX picture ($C00 bytes, from the
   buffer drawn: $4000 or $D000) to $6600. */
void video_gsu_picture(void)
{
    if (!gsu_picture_waiting)
        return;
    store_from(pakram, gsu_buffer_select ? 0x4000 : 0xD000, 0x0C00, 0x6600);
    gsu_picture_waiting = 0;
}

/* OamDma, OamDmaAll: `n` bytes of the sprite table from work RAM `at`. */
void video_sprites_from(uint16_t at, uint16_t n)
{
    unsigned i, total = n ? n : 0x10000u;

    /* the table's 544 bytes take the first $200 places a pair at a time
       (an odd last byte of a pair is held back); the next $200 all reach
       its last 32 bytes, a byte at a time; then the start again */
    for (i = 0; i < total; i++) {
        unsigned a = i & 0x3FF;

        if (a >= 0x200)
            video_sprite_table(&wram[(uint16_t)(at + i)], 0x200 + (a & 0x1F), 1);
        else if ((a & 1) && i > 0) {
            uint8_t pair[2] = {wram[(uint16_t)(at + i - 1)], wram[(uint16_t)(at + i)]};

            video_sprite_table(pair, a - 1, 2);
        }
    }
}

void video_sprites_all(void)
{
    video_sprite_table(oam_buffer, 0, 544);
}

/* CgramDma, CgramDmaAll: `n` bytes of colours from work RAM `at`, from
   colour `first`. */
void video_colours_from(uint16_t at, uint8_t first, uint16_t n)
{
    unsigned i;

    for (i = 0; i + 1 < (n ? n : 0x10000u); i += 2) {
        uint8_t c[2] = {wram[(uint16_t)(at + i)], wram[(uint16_t)(at + i + 1)]};   /* (wrapping in bank $7E) */

        video_colours((uint8_t)(first + i / 2), c, 1);
    }
}

void video_colours_all(void)
{
    video_colours(0, &wram[palette_buffer], 256);
}
