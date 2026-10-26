/*
 * tilemap.c: the BG tilemap buffers in work RAM, and the VRAM uploads that
 * go with them.
 *
 * A tilemap entry is a word: bits 0-9 the tile, 10-12 the palette, 13 the
 * priority, 14 and 15 the flips. The game builds its screens' maps in
 * buffers of bank $7F, a row of 32 entries ($40 bytes) at a time: the rows
 * of the Super FX's picture, the level's columns, the championship screen's
 * digits. The routines here fill those buffers, set the palette bits of
 * entries, and write the frame's rows and the picture's to VRAM.
 *
 * Where the game writes VRAM at the address its VMADDL holds, the address
 * is passed in and the address after the words is returned (see tilemap.h).
 * The game's VMAIN is $80 for these: a word goes to the store and the
 * address then moves on a word.
 */
#include "game.h"
#include "video.h"
#include "tilemap.h"
#include "object_more.h"
#include "rom.h"

/* The work RAM index of an address in a bank: $7E is the low 64 K of work
   RAM, $7F the high 64 K. */
static uint32_t wram_index(uint8_t bank, uint16_t at)
{
    return (uint32_t)(bank & 1) << 16 | at;
}

static void put_byte(uint8_t bank, uint16_t at, uint8_t v)
{
    wram[wram_index(bank, at)] = v;
}

static void put_word(uint8_t bank, uint16_t at, uint16_t v)
{
    *(w16 *)&wram[wram_index(bank, at)] = v;
}

/* A word as the game reads it in a bank (a ROM bank reads the cartridge). */
static uint16_t get_word(uint8_t bank, uint16_t at)
{
    return (uint16_t)(bank_byte(bank, at) | bank_byte(bank, (uint16_t)(at + 1)) << 8);
}

/* One word to VRAM at *vram; the address then moves on a word. */
static void vram_put(uint16_t *vram, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};

    video_store_write(*vram, b, 2);
    (*vram)++;
}

/* A row of 32 entries from the picture: the Super FX's picture at the
   offset a plus the word at $00F1 (the picture's row base), each entry
   with priority set, into the buffer at $93D7 + y. */
void tilemap_row_from_gsu_ram(uint8_t bank, uint16_t a, uint16_t y)
{
    uint16_t x = (uint16_t)(a + W16(0x00F1));
    unsigned i;

    /* the picture is at $70:3000 onward; the offsets the game uses stay
       below $C000, inside the bank */
    for (i = 0; i < 32; i++) {
        put_word(bank, (uint16_t)(0x93D7 + y), (uint16_t)(P16((uint16_t)(0x3000 + x)) | 0x2000));
        x = (uint16_t)(x + 2);
        y = (uint16_t)(y + 2);
    }
}

/* 16 rows of 32 words: row r runs from start + r, each word 16 more than
   the last. */
uint16_t vram_fill_gradient(uint16_t vram, uint16_t start)
{
    unsigned r, c;

    for (r = 0; r < 16; r++) {
        uint16_t a = (uint16_t)(start + r);

        for (c = 0; c < 32; c++, a = (uint16_t)(a + 0x10))
            vram_put(&vram, a);
    }
    return vram;
}

/* A 32-byte tile from the Super FX's RAM at the tile's offset (tile * 32
   plus the word at $00F1): 16 words to VRAM. */
uint16_t vram_tile_from_gsu_ram(uint16_t tile, uint16_t vram)
{
    uint16_t at = (uint16_t)((uint16_t)(tile << 5) + W16(0x00F1));

    video_from_gsu_ram(0x20, at, vram);
    return (uint16_t)(vram + 0x10);
}

/* The frame rows: a row of 32 words, then 35 words, each with the tile
   base; the 35 are $2010 + base in the HUD modes 1 and 3 when the byte at
   $10CD is 0, else $2002 + base. */
uint16_t vram_frame_rows(uint8_t bank, uint16_t base, uint16_t vram)
{
    uint8_t mode = bank_byte(bank, 0x0D2B);
    uint16_t second = ((mode == 1 || mode == 3) && bank_byte(bank, 0x10CD) == 0) ? 0x2010 : 0x2002;
    unsigned i;

    for (i = 0; i < 0x20; i++)
        vram_put(&vram, (uint16_t)(0x2001 + base));
    for (i = 0; i < 0x23; i++)
        vram_put(&vram, (uint16_t)(second + base));
    return vram;
}

/* The picture: 192 words of the Super FX's picture ($70:3000), each plus
   the tile base. */
uint16_t vram_picture_rows(uint16_t base, uint16_t vram)
{
    unsigned x;

    for (x = 0; x < 0x180; x += 2)
        vram_put(&vram, (uint16_t)(P16((uint16_t)(0x3000 + x)) + base));
    return vram;
}

/* Four words copied from the bank $7F buffer at $BBD7 to the buffer at
   $93D7, at the offset *x; then *x moves on by $40 (one row). */
uint16_t tilemap_row_from_7fbbd7(uint16_t *x)
{
    unsigned i;

    for (i = 0; i < 4; i++)
        put_word(0x7F, (uint16_t)(0x93D7 + 2 * i + *x), get_word(0x7F, (uint16_t)(0xBBD7 + 2 * i + *x)));
    *x = (uint16_t)(*x + 0x40);
    return *x;
}

/* Four words copied from the table at $FB4E + *y in `bank` (in ROM when
   the bank is 2) to the buffer at $93D7 + *x; *y moves on by 8 and *x by
   $40. */
uint16_t tilemap_row_from_fb4e(uint8_t bank, uint16_t *x, uint16_t *y)
{
    unsigned i;

    for (i = 0; i < 4; i++)
        put_word(0x7F, (uint16_t)(0x93D7 + 2 * i + *x), get_word(bank, (uint16_t)(0xFB4E + 2 * i + *y)));
    *y = (uint16_t)(*y + 8);
    *x = (uint16_t)(*x + 0x40);
    return *x;
}

/* `count` entries from the buffer at $93D7 + x: each attribute byte keeps
   its bits other than the palette (2-4) and takes `bits`. The count is
   at least 1 (the game never gives none). */
uint16_t tilemap_palette_fill(uint16_t x, uint16_t count, uint8_t bits)
{
    do {
        uint16_t at = (uint16_t)(0x93D7 + x);
        uint8_t v = (uint8_t)((bank_byte(0x7F, at) & 0xE3) | bits);

        put_byte(0x7F, at, v);
        x = (uint16_t)(x + 2);
    } while (--count);
    return x;
}

/* Every fourth frame: the phase at $0DD5 steps (0 to 5), and the entries
   take its palette, from $07:CF2C when the HUD mode is 2 and $07:CF26
   otherwise. */
int tilemap_palette_cycle(uint8_t bank, uint16_t *x, uint16_t count)
{
    uint8_t phase, pal;
    uint16_t mode, table;

    if (bank_byte(bank, 0x0E3B) & 0x03)
        return 0;
    phase = (uint8_t)(bank_byte(bank, 0x0DD5) + 1);
    if (phase >= 6)
        phase = 0;
    put_byte(bank, 0x0DD5, phase);
    mode = get_word(bank, 0x0D2B);
    table = mode == 2 ? 0xCF2C : 0xCF26;
    pal = bank_byte(0x07, (uint16_t)(table + phase));
    *x = tilemap_palette_fill(*x, count, pal);
    return mode >= 2 ? 2 : 1;
}

void tilemap_palette_cycle_87(uint8_t bank)
{
    uint16_t x = 0x0087;

    tilemap_palette_cycle(bank, &x, 9);
}

void tilemap_palette_cycle_107(uint8_t bank)
{
    uint16_t x = 0x0107;

    tilemap_palette_cycle(bank, &x, 13);
}

void tilemap_palette_cycle_187(uint8_t bank)
{
    uint16_t x = 0x0187;

    tilemap_palette_cycle(bank, &x, 10);
}

/* The two framed panels: at $7800 the frame rows (base $02B0), the sky
   gradient from $2000 and at $7A40 the picture; at $7C00 the frame rows
   (base $01B0), the gradient from $2010 and at $7E40 the picture. */
void screen_frame_and_pictures(uint8_t bank)
{
    vram_fill_gradient(vram_frame_rows(bank, 0x02B0, 0x7800), 0x2000);
    vram_picture_rows(0x02B0, 0x7A40);
    vram_fill_gradient(vram_frame_rows(bank, 0x01B0, 0x7C00), 0x2010);
    vram_picture_rows(0x01B0, 0x7E40);
}

/* 1024 words from VRAM address 0, each a random tile $20-$3F: the random
   word at $0D18 steps (random_word_step) before each. The writes go
   through the data bank: bank 0 reaches the PPU (BG4's vertical scroll is
   the first half of the word sent to $2114, its second half VMAIN $80),
   and a work RAM bank keeps them in work RAM at $2114-$2118. */
uint16_t vram_noise_tiles(uint8_t bank)
{
    uint16_t vram = 0, v = 0;
    unsigned i;

    if (bank == 0) {
        video_register(0x14, 0x00);
    } else {
        put_word(bank, 0x2114, 0x8000);
        put_word(bank, 0x2116, 0x0000);
    }
    for (i = 0; i < 0x400; i++) {
        uint16_t state = get_word(bank, 0x0D18);

        random_word_step(&state);
        put_word(bank, 0x0D18, state);
        v = (uint16_t)((state & 0x001F) | 0x0020);
        if (bank == 0)
            vram_put(&vram, v);
        else
            put_word(bank, 0x2118, v);
    }
    return v;
}

/* The level's columns: twelve of four rows each, from the table at
   $02:FB6E (the buffer offset of each column), filled to the column's
   level ($0F05 + i, more than 4 counts as 4) with rows from the ROM table
   at $02:FB4E, and the empty rows above them from $7F:BBD7. */
uint16_t tilemap_level_columns(void)
{
    uint16_t a = 0, x, y;
    unsigned i, k, n;

    for (i = 0; i < 12; i++) {
        x = get_word(0x02, (uint16_t)(0xFB6E + 2 * i));
        n = bank_byte(0x02, (uint16_t)(0x0F05 + i));
        if (n > 4)
            n = 4;
        y = (uint16_t)((4 - n) * 8);
        for (k = 0; k < n; k++)
            tilemap_row_from_fb4e(0x02, &x, &y);
        for (k = n; k < 4; k++)
            a = tilemap_row_from_7fbbd7(&x);
        if (n == 4)
            a = x;
    }
    return a;
}

/* A row of 32 entries from the buffer at $93D7 (or $BBD7 when $0DA4 bit 14
   is set) + *x in `bank` to $E3D7 + *y; both move on by $40. */
uint16_t tilemap_row_to_e3d7(uint8_t bank, uint16_t *x, uint16_t *y)
{
    uint16_t src = (get_word(0x00, 0x0DA4) & 0x4000) ? 0xBBD7 : 0x93D7, v = 0;
    unsigned n = 0x20;

    do {
        v = get_word(bank, (uint16_t)(src + *x));
        put_word(bank, (uint16_t)(0xE3D7 + *y), v);
        *x = (uint16_t)(*x + 2);
        *y = (uint16_t)(*y + 2);
    } while (--n);
    return v;
}

/* The backdrop's scroll: the angle at $06CD is minus the view's pitch
   ($0664 / 64, arithmetic), kept between -56 and 232, plus $98, modulo
   $200; $06CF is the view's heading ($0666 / 32) modulo $800. */
uint16_t backdrop_angles_from_view(uint8_t bank)
{
    int16_t v = asr16((int16_t)get_word(bank, 0x0664), 6);
    uint16_t w;

    v = (int16_t)-v;
    if (v >= 0x00E8)
        v = 0x00E8;
    if (v < (int16_t)0xFFC8)
        v = (int16_t)0xFFC8;
    put_word(bank, 0x06CD, (uint16_t)((uint16_t)(v + 0x0098) & 0x01FF));
    w = (uint16_t)asr16((int16_t)get_word(bank, 0x0666), 5) & 0x07FF;
    put_word(bank, 0x06CF, w);
    return w;
}

/* `count` entries from offset x take palette `bits`, then the entry at
   bank $7F's `mark` takes `mark_bits` as well (its other bits kept). */
static void palette_row_mark(uint16_t x, uint16_t count, uint8_t bits, uint16_t mark, uint8_t mark_bits)
{
    uint8_t v;

    tilemap_palette_fill(x, count, bits);
    v = (uint8_t)((bank_byte(0x7F, mark) & 0xE3) | mark_bits);
    put_byte(0x7F, mark, v);
}

void tilemap_row_87_palette5(void)
{
    palette_row_mark(0x0087, 9, 0x14, 0x9464, 0x10);
}

void tilemap_row_107_palette5(void)
{
    palette_row_mark(0x0107, 13, 0x14, 0x94EA, 0x10);
}

void tilemap_row_187_palette5(void)
{
    palette_row_mark(0x0187, 10, 0x14, 0x956A, 0x10);
}

void tilemap_row_187_palette4(void)
{
    palette_row_mark(0x0187, 9, 0x10, 0x9564, 0x00);
}

void tilemap_row_207_palette4(void)
{
    palette_row_mark(0x0207, 13, 0x10, 0x95EA, 0x00);
}

void tilemap_row_287_palette4(void)
{
    palette_row_mark(0x0287, 10, 0x10, 0x966A, 0x00);
}

/* The three digits (bytes at $1184-$1186) as tile words $CA + digit with
   priority set, at $7F:9481. */
void digit_tiles_1184(void)
{
    unsigned i;

    for (i = 0; i < 3; i++)
        put_word(0x7F, (uint16_t)(0x9481 + 2 * i), (uint16_t)((W8(0x1184 + i) + 0x00CA) | 0x2000));
}

/* The three times at $0EE7 are kept at $0EEA; the stage at $0D59 moves on
   and the split window starts closing. */
void times_keep_and_advance(uint8_t bank)
{
    unsigned i;

    for (i = 0; i < 3; i++)
        put_byte(bank, (uint16_t)(0x0EEA + i), bank_byte(bank, (uint16_t)(0x0EE7 + i)));
    put_byte(bank, 0x0E3F, 0x00);
    put_byte(bank, 0x0D59, (uint8_t)(bank_byte(bank, 0x0D59) + 1));
    put_byte(bank, 0x0D23, 0x02);
    put_byte(bank, 0x19DF, 0x00);
    put_byte(bank, 0x19D8, 0x02);
}

/* 17 rows of 26 entries from the buffer at $7F:93D7 (row r from offset $40
   r) to VRAM from $5C00, one 32-entry row apart; each row's address goes to
   VMADDL first. Then BG2's scroll from band 0. Returns the high byte of that
   band's vertical scroll, as the game leaves A. In a work RAM bank the
   VMAIN, VMADDL and VMDATA writes land in that bank's work RAM. */
uint8_t tilemap_rows_to_5c00(uint8_t bank)
{
    unsigned r, i;

    if (bank != 0)
        put_byte(bank, 0x2115, 0x80);
    for (r = 0; r < 17; r++) {
        uint16_t row = (uint16_t)(0x5C00 + 0x20 * r), vram = row;
        uint16_t off = (uint16_t)(0x40 * r);

        if (bank != 0)
            put_word(bank, 0x2116, row);
        for (i = 0; i < 26; i++) {
            uint16_t v = get_word(0x7F, (uint16_t)(0x93D7 + off + 2 * i));

            if (bank == 0)
                vram_put(&vram, v);
            else
                put_word(bank, 0x2118, v);
        }
    }
    bg2_scroll_from_table(0);
    return bank_byte(0x00, 0x0D8F);
}

/* The backdrop's rows. The S-CPU's multiplier gives the map row: $0140 (the
   M7A operand the game sets) times the row (at most 63), low word. The buffer
   picked by $0D7F (0 or 1) gets its scroll pair and $0D96 (twice the angle
   at $06CD plus $42, at most $300). Then 17 rows of 32 entries are copied
   from the map at $7F:93D7 by tilemap_row_to_e3d7, each row $100 further on
   in the map; once the map offset reaches $2800 the rest of the buffer is
   filled with $20C0. Returns the map offset after the last row, or $20C0 after
   a fill. */
uint16_t backdrop_rows_update(uint8_t bank)
{
    uint16_t sel = get_word(0x00, 0x0D7F), x = (uint16_t)(sel << 1), y, h, s, row, a = 0, end;
    unsigned rows = 17;

    object_link_flag_4000_step();
    h = get_word(0x00, 0x06CD);
    s = (uint16_t)((h << 1) + 0x42 + (h >> 15));
    put_word(bank, 0x0D96, s < 0x0300 ? s : 0x0300);
    put_word(bank, (uint16_t)(0x0D8E + x), (uint16_t)((h & 0x0007) - 0x20 + ram_code_word((uint16_t)(0xE811 + x))));
    put_word(bank, (uint16_t)(0x0D92 + x), (uint16_t)(((get_word(bank, 0x06CF) >> 1) & 0x0007) - 0x18));
    row = (uint16_t)(((uint16_t)(get_word(bank, 0x06CD) + get_word(bank, 0x0DA2))) >> 3) & 0x003F;
    x = (uint16_t)(((get_word(bank, 0x06CF) >> 3) & 0x00FE) + (uint16_t)(0x0140 * row));
    sel = get_word(0x00, 0x0D7F);
    y = sel ? 0x0440 : 0x0000;
    end = sel ? 0x0880 : 0x0440;
    for (;;) {
        if (x >= 0x2800) {
            a = 0x20C0;
            do {
                put_word(0x7F, (uint16_t)(0xE3D7 + y), a);
                y = (uint16_t)(y + 2);
            } while (y < end);
            break;
        }
        tilemap_row_to_e3d7(0x7F, &x, &y);
        x = (uint16_t)(x + 0x0100);
        a = x;
        if (--rows == 0)
            break;
    }
    return a;
}
