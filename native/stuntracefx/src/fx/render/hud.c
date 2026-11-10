/*
 * hud.c: the Super FX programs in bank 07 that draw the 2D pieces of the
 * screen (asm/stuntracefx/render/hud.asm; C: src/stuntracefx/render/hud.c).
 *
 * The glyphs and digits, the banner's rising picture and the scaled sprites
 * take the piece they draw (hud.h) and hand each row to the GPU
 * (fx_glyph_row, fx_glyph_scaled_row). Their pictures are 2 bits a pixel in
 * the cartridge, 32 bytes a row; the low two bits of a byte are its leftmost
 * pixel, each value one of the piece's four colours.
 *
 * Not yet converted, still as the generator wrote them on the chip's RAM:
 * the tile converters and the noise fills below. (The instruction after a
 * branch or LOOP in them, its delay slot, always runs.)
 */

#include "fx/fx.h"
#include "data/race_tables.h"
#include "fx/render/hud.h"

/*
 * @routine 07:EE50 FxTiles2bppTo4bppHigh
 * WHAT IT DOES: Converts 96 2-bit tiles at $70:C000 into 4-bit tiles at
 * $70:D000: each tile's 16 bytes (two bit planes, 8 rows) become 32, the
 * upper two planes 0.
 * HOW: Bytes are copied with a 0 after each (8 a tile, LDB and STB), then 16
 * zero bytes; R4 counts the tiles down.
 * Entry: started by the S-CPU. Exit: R0 = 0; R1 = 0; R2 = $C300; R3 = $DC00;
 * R4 = 0; R12 = 0; R13 = $EE7E; RAM bank 0; the last RAM address $DBFF; CY
 * set, OV clear, S, Z from the tile count (Z set).
 */
void FxTiles2bppTo4bppHigh(void)
{
    uint16_t count;
    uint16_t count_2;
    uint16_t src_byte;
    uint16_t byte_count;
    uint16_t bytes_left;
    uint16_t next_dst_ptr;
    uint16_t tile_dst_ptr;
    uint16_t tile_src_ptr;

    tile_src_ptr = 0xC000;
    tile_dst_ptr = 0xD000;
    fx_ramb(0);
    byte_count = 0x0060;
    for (;;) {
        count = 8;
        for (;;) {
            src_byte = fx_ldb(tile_src_ptr);
            fx_stb(tile_dst_ptr, (uint8_t)src_byte);
            tile_src_ptr = (uint16_t)(tile_src_ptr + 1);
            next_dst_ptr = (uint16_t)(tile_dst_ptr + 1);
            fx_stb(next_dst_ptr, (uint8_t)0);
            count = (uint16_t)(count - 1);
            tile_dst_ptr = (uint16_t)(next_dst_ptr + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        count_2 = 0x0010;
        for (;;) {
            fx_stb(tile_dst_ptr, (uint8_t)0);
            count_2 = (uint16_t)(count_2 - 1);
            tile_dst_ptr = (uint16_t)(tile_dst_ptr + 1);
            if (count_2 != 0) {
                continue;
            } else {
                break;
            }
        }
        bytes_left = (uint16_t)(byte_count - 1);
        byte_count = bytes_left;
        if (bytes_left != 0) {
            continue;
        } else {
            break;
        }
    }
    return;
}


/*
 * @routine 07:EE59 FxTiles2bppTo4bppLow
 * WHAT IT DOES: As FxTiles2bppTo4bppHigh, from $70:3000 into $70:4000.
 * HOW: As FxTiles2bppTo4bppHigh, with source $3000 and destination $4000.
 * Entry: started by the S-CPU. Exit: as FxTiles2bppTo4bppHigh, with R2 =
 * $3300, R3 = $4C00 and the last RAM address $4BFF.
 */
void FxTiles2bppTo4bppLow(void)
{
    uint16_t count;
    uint16_t count_2;
    uint16_t dst_ptr;
    uint16_t src_ptr;
    uint16_t plane_byte;
    uint16_t tiles_left;
    uint16_t dst_ptr_odd;
    uint16_t tiles_left_after;

    src_ptr = 0x3000;
    dst_ptr = 0x4000;
    fx_ramb(0);
    tiles_left = 0x0060;
    for (;;) {
        count = 8;
        for (;;) {
            plane_byte = fx_ldb(src_ptr);
            fx_stb(dst_ptr, (uint8_t)plane_byte);
            src_ptr = (uint16_t)(src_ptr + 1);
            dst_ptr_odd = (uint16_t)(dst_ptr + 1);
            fx_stb(dst_ptr_odd, (uint8_t)0);
            count = (uint16_t)(count - 1);
            dst_ptr = (uint16_t)(dst_ptr_odd + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        count_2 = 0x0010;
        for (;;) {
            fx_stb(dst_ptr, (uint8_t)0);
            count_2 = (uint16_t)(count_2 - 1);
            dst_ptr = (uint16_t)(dst_ptr + 1);
            if (count_2 != 0) {
                continue;
            } else {
                break;
            }
        }
        tiles_left_after = (uint16_t)(tiles_left - 1);
        tiles_left = tiles_left_after;
        if (tiles_left_after != 0) {
            continue;
        } else {
            break;
        }
    }
    return;
}


/*
 * @routine 07:EE88 FxNoiseFill
 * WHAT IT DOES: Fills the 16 x 128 pixels at the screen's top left with
 * random grey: each pixel's colour is one of the eight words at $70:3C00
 * (NoiseFillWithRamp writes $80, $90, ... $F0 there), picked by a random number
 * seeded from and saved back to $70:021C.
 * HOW: The generator (seed in R8): t = the seed with its low byte XORed into
 * its high byte, its bytes swapped; the new seed is (twice the old low byte
 * XOR t) >> 1, XOR $FF00; when the bit shifted out is set it is XORed with
 * $8180, and when not, a value of $AA55 becomes 0. Its low three bits pick
 * the colour. RPIX at the end writes the pixel caches out.
 * Entry: started by the S-CPU. Exit: R0 = the pixel at (0, 128); R1 = 0; R2
 * = 128; R3 = $FF00; R4 = $AA55; R5 = $8180; R6 = $3C00; R8 = the new seed;
 * R9 = 16; R10 = 0; R12 = 0; R13 = $EEBB; plot options 0; the colour of the
 * last pixel; the last RAM address $021C; CY set, OV clear, S, Z from the
 * RPIX.
 */
void FxNoiseFill(void)
{
    uint16_t count;
    uint16_t plot_x;
    uint16_t plot_y;
    uint16_t noise_mix;
    uint16_t row_count;
    uint16_t rows_left;
    uint16_t noise_next;
    uint16_t noise_seed;
    uint16_t fill_colour;
    uint16_t colour_index;
    uint16_t noise_feedback;
    uint16_t noise_low_byte;

    fx_cmode(0);
    row_count = 0x0080;
    noise_seed = fx_ldw(FX_NOISE_SEED);
    plot_x = 0;
    plot_y = 0;
    for (;;) {
        count = 0x0010;
        fx_noise_row(plot_x, plot_y, 0x0010, noise_seed, 0x3C00);
        for (;;) {
            noise_mix = (uint16_t)(((uint16_t)((uint16_t)(noise_seed & 0xFF) << 8 | (uint16_t)(noise_seed & 0xFF) >> 8) ^ noise_seed) << 8 | ((uint16_t)((uint16_t)(noise_seed & 0xFF) << 8 | (uint16_t)(noise_seed & 0xFF) >> 8) ^ noise_seed) >> 8);
            noise_low_byte = (uint16_t)((uint16_t)(noise_mix << 8 | noise_mix >> 8) & 0xFF);
            noise_feedback = ((uint16_t)(noise_low_byte + noise_low_byte) ^ noise_mix);
            noise_next = ((uint16_t)(noise_feedback >> 1) ^ 0xFF00);
            if (!(noise_feedback & 1)) {
                if ((uint16_t)(noise_next + 0x55AB) != 0) {
                    noise_seed = noise_next;
                } else {
                    noise_next = 0;
                    noise_seed = noise_next;
                }
            } else {
                noise_next = (noise_next ^ 0x8180);
                noise_seed = noise_next;
            }
            colour_index = (noise_next & 7);
            fill_colour = fx_ldw((uint16_t)((uint16_t)(colour_index + colour_index) + 0x3C00));
            fx_color((uint8_t)fill_colour);
            count = (uint16_t)(count - 1);
            plot_x = (uint16_t)(plot_x + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        plot_x = 0;
        rows_left = (uint16_t)(row_count - 1);
        row_count = rows_left;
        plot_y = (uint16_t)(plot_y + 1);
        if (rows_left != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)plot_x, (uint8_t)plot_y);
    fx_stw(FX_NOISE_SEED, noise_seed);
    return;
}


/*
 * @routine 07:EE94 FxNoiseFillScreen
 * WHAT IT DOES: FxNoiseFill over the whole screen: $70:003A pixels across
 * and $70:003E + 1 lines down.
 * HOW: The width and height into R9 and R10, then FxNoiseFill's body. No
 * script has started it yet.
 * Entry: started by the S-CPU. Exit: as FxNoiseFill, with R9 = the width and
 * R2 = the lines drawn.
 */
void FxNoiseFillScreen(void)
{
    uint16_t count;
    uint16_t plot_x;
    uint16_t plot_y;
    uint16_t last_row;
    uint16_t noise_mix;
    uint16_t row_count;
    uint16_t rows_left;
    uint16_t noise_next;
    uint16_t noise_seed;
    uint16_t fill_colour;
    uint16_t colour_index;
    uint16_t screen_width;
    uint16_t noise_feedback;
    uint16_t noise_low_byte;

    fx_cmode(0);
    screen_width = fx_ldw(0x003A);
    last_row = fx_ldw(0x003E);
    row_count = (uint16_t)(last_row + 1);
    noise_seed = fx_ldw(FX_NOISE_SEED);
    plot_x = 0;
    plot_y = 0;
    for (;;) {
        count = screen_width;
        fx_noise_row(plot_x, plot_y, screen_width, noise_seed, 0x3C00);
        for (;;) {
            noise_mix = (uint16_t)(((uint16_t)((uint16_t)(noise_seed & 0xFF) << 8 | (uint16_t)(noise_seed & 0xFF) >> 8) ^ noise_seed) << 8 | ((uint16_t)((uint16_t)(noise_seed & 0xFF) << 8 | (uint16_t)(noise_seed & 0xFF) >> 8) ^ noise_seed) >> 8);
            noise_low_byte = (uint16_t)((uint16_t)(noise_mix << 8 | noise_mix >> 8) & 0xFF);
            noise_feedback = ((uint16_t)(noise_low_byte + noise_low_byte) ^ noise_mix);
            noise_next = ((uint16_t)(noise_feedback >> 1) ^ 0xFF00);
            if (!(noise_feedback & 1)) {
                if ((uint16_t)(noise_next + 0x55AB) != 0) {
                    noise_seed = noise_next;
                } else {
                    noise_next = 0;
                    noise_seed = noise_next;
                }
            } else {
                noise_next = (noise_next ^ 0x8180);
                noise_seed = noise_next;
            }
            colour_index = (noise_next & 7);
            fill_colour = fx_ldw((uint16_t)((uint16_t)(colour_index + colour_index) + 0x3C00));
            fx_color((uint8_t)fill_colour);
            count = (uint16_t)(count - 1);
            plot_x = (uint16_t)(plot_x + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        plot_x = 0;
        rows_left = (uint16_t)(row_count - 1);
        row_count = rows_left;
        plot_y = (uint16_t)(plot_y + 1);
        if (rows_left != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)plot_x, (uint8_t)plot_y);
    fx_stw(FX_NOISE_SEED, noise_seed);
    return;
}

/* ---- the HUD's pieces: glyphs, the banner's picture and scaled sprites ---- */

/* where the pictures are (their banks, the banner) is data/race_tables' */
enum { GLYPH_ROW_BYTES = 32 /* a picture row's bytes in the cartridge, 4 pixels a byte */ };

/* `rows` rows of a picture, `width` pixels each, from `at` down, a line
   apart; the count runs as the chip's loop did (0 is 65536 rows). */
static void glyph_rows(uint16_t x, uint16_t y, uint16_t width, uint16_t rows, uint8_t bank, uint16_t at,
                       const uint8_t colours[4])
{
    do {
        fx_glyph_row(x, y, width, bank, at, colours);
        at = (uint16_t)(at + GLYPH_ROW_BYTES);
        y = (uint16_t)(y + 1);
    } while (--rows);
}

/*
 * @routine 07:F121 FxGlyph16RowsFromBottom
 * WHAT IT DOES: Draws the bottom p->h rows of the 16 x 32 glyph at the
 * piece's place and moves x on by p->w; glyph 0 draws nothing. With
 * FxGlyph16RowsFromTop it rolls a digit into its cell.
 */
void FxGlyph16RowsFromBottom(FxPiece *p)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + p->w);
    if (p->glyph)
        glyph_rows(x, p->y, 16, p->h, hud_glyphs_bank(), (uint16_t)(p->glyph + (uint16_t)(32 - p->h) * GLYPH_ROW_BYTES),
                   p->colours);
}

/*
 * @routine 07:F15B FxGlyph16RowsFromTop
 * WHAT IT DOES: Draws the top p->h rows of the 16 x 32 glyph at the bottom
 * of the 32-line cell at the piece's place and moves x on by p->w; glyph 0
 * draws nothing. With p->h = 32 it draws the whole glyph (digits).
 */
void FxGlyph16RowsFromTop(FxPiece *p)
{
    uint16_t x = p->x, rows = p->h, y = (uint16_t)(p->y - p->h + 32);

    p->x = (uint16_t)(x + p->w);
    if (p->glyph)
        glyph_rows(x, y, 16, rows, hud_glyphs_bank(), p->glyph, p->colours);
}

/* @routine 07:F280 FxGlyph8x8: the 8 x 8 glyph; x on by 8; glyph 0 draws nothing. */
void FxGlyph8x8(FxPiece *p)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + 8);
    if (p->glyph)
        glyph_rows(x, p->y, 8, 8, hud_glyphs_bank(), p->glyph, p->colours);
}

/* @routine 07:F329 FxGlyph8x16: the 8 x 16 glyph; x on by 8; glyph 0 draws nothing. */
void FxGlyph8x16(FxPiece *p)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + 8);
    if (p->glyph)
        glyph_rows(x, p->y, 8, 16, hud_glyphs_bank(), p->glyph, p->colours);
}

/* @routine 07:F3D2 FxGlyph24x32Bank04: the 24 x 32 glyph from bank $04; x on by 16; the glyph is not checked. */
void FxGlyph24x32Bank04(FxPiece *p)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + 16);
    glyph_rows(x, p->y, 24, 32, hud_places_bank(), p->glyph, p->colours);
}

/* @routine 07:F441 FxGlyph24x16: the 24 x 16 glyph (the player marks); x on by 16; the glyph is not checked. */
void FxGlyph24x16(FxPiece *p)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + 16);
    glyph_rows(x, p->y, 24, 16, hud_glyphs_bank(), p->glyph, p->colours);
}

/* @routine 07:F5C9 FxGlyph16x16: the 16 x 16 glyph; x on by 16; the glyph is not checked. */
void FxGlyph16x16(FxPiece *p)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + 16);
    glyph_rows(x, p->y, 16, 16, hud_glyphs_bank(), p->glyph, p->colours);
}

/* @routine 07:F6DF FxGlyph16x32: the 16 x 32 glyph; x on by 16; glyph 0 draws nothing. */
void FxGlyph16x32(FxPiece *p)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + 16);
    if (p->glyph)
        glyph_rows(x, p->y, 16, 32, hud_glyphs_bank(), p->glyph, p->colours);
}

/* the banner's picture from `at`, `width` columns, its top `rows` rows rising out of `line` at `x` */
static void rise(uint16_t x, uint16_t line, uint16_t width, uint16_t at, uint16_t rows, const uint8_t colours[4])
{
    glyph_rows(x, (uint16_t)(line - rows), width, rows, hud_glyphs_bank(), at, colours);
}

/* @routine 07:F0A5 FxPictureRise40: the banner's first 56 columns rising out of line $40 at x $28. */
void FxPictureRise40(const uint8_t colours[4], uint16_t rows) { rise(0x28, 0x40, 56, hud_banner_picture(0), rows, colours); }

/* @routine 07:F0B4 FxPictureRise60: as FxPictureRise40, out of line $60 at x $4C. */
void FxPictureRise60(const uint8_t colours[4], uint16_t rows) { rise(0x4C, 0x60, 56, hud_banner_picture(0), rows, colours); }

/*
 * @routine 07:EEF5 FxPictureRiseSplit40
 * WHAT IT DOES: The banner rising out of line $40 in two pieces: its
 * columns 56-111 at x $1C, then its columns 0-39 at x $54.
 */
void FxPictureRiseSplit40(const uint8_t colours[4], uint16_t rows)
{
    rise(0x1C, 0x40, 56, hud_banner_picture(56), rows, colours);
    rise(0x54, 0x40, 40, hud_banner_picture(0), rows, colours);
}

/* @routine 07:EFCD FxPictureRiseSplit60: as FxPictureRiseSplit40, out of line $60, the pieces at x $34 and $6C. */
void FxPictureRiseSplit60(const uint8_t colours[4], uint16_t rows)
{
    rise(0x34, 0x60, 56, hud_banner_picture(56), rows, colours);
    rise(0x6C, 0x60, 40, hud_banner_picture(0), rows, colours);
}

/* The chip's signed halving (DIV2): -1 halves to 0. */
static uint16_t half(uint16_t v) { return v == 0xFFFF ? 0 : (uint16_t)((int16_t)v >> 1); }

/* the picture row `off` lines into a sprite: off * step / 256 by the chip's
   fixed-point multiply (off's bytes swapped, times step, the high word) */
static uint16_t source_row(uint16_t off, uint16_t step)
{
    return (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(off << 8 | off >> 8) * (int16_t)step) >> 16);
}

/*
 * The body the scaled sprites share: the sprite `w` x `h` at (x, y) from
 * p->glyph, clipped to the view's right and bottom edges (a sprite starting
 * left of or above the screen is cut to what shows), each row sent once
 * with its picture row and p->u across. The row count runs as the chip's
 * loop did (0 is 65536 rows).
 */
static void scaled_sprite(const FxPiece *p, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t right,
                          uint16_t bottom)
{
    uint16_t left, row_y;

    if (x & 0x8000) {
        w = (uint16_t)(x + w);
        if (w & 0x8000)
            return;
        left = 0;
    } else {
        if (x >= right)
            return;
        if ((uint16_t)(x + w) >= right)
            w = (uint16_t)(right - x);
        left = x;
    }
    if (y & 0x8000) {
        h = (uint16_t)(y + h);
        if (h & 0x8000)
            return;
        row_y = 0;
    } else {
        if (y >= bottom)
            return;
        if ((uint16_t)(y + h) >= bottom)
            h = (uint16_t)(bottom - y);
        row_y = y;
    }
    do {
        uint16_t off = (uint16_t)(row_y - y), at = p->glyph;

        if (off)
            at = (uint16_t)(at + (uint16_t)(source_row(off, p->v) * GLYPH_ROW_BYTES));
        fx_scaled_row_start();
        fx_glyph_scaled_row(left, row_y, w, (uint16_t)(left - x), p->u, hud_glyphs_bank(), p->glyph, at, p->colours);
        row_y = (uint16_t)(row_y + 1);
    } while (--h);
}

/* @routine 07:FA1B FxScaledSprite13: (p->w / 2) * 13 / 16 across, p->h / 16 down; glyph 0 draws nothing. */
void FxScaledSprite13(const FxPiece *p, uint16_t right, uint16_t bottom)
{
    uint16_t w = (uint16_t)((uint16_t)((uint16_t)((int8_t)half(p->w) * (int8_t)13) << 4) >> 8);
    uint16_t h = (uint16_t)((uint16_t)(p->h << 4) >> 8);

    if (p->glyph)
        scaled_sprite(p, p->x, p->y, w, h, right, bottom);
}

/* @routine 07:FA35 FxScaledSprite12: (p->w / 2) * 12 / 16 across, p->h / 16 down; glyph 0 draws nothing. */
void FxScaledSprite12(const FxPiece *p, uint16_t right, uint16_t bottom)
{
    uint16_t w = (uint16_t)((uint16_t)((uint16_t)((int8_t)half(p->w) * (int8_t)12) << 4) >> 8);
    uint16_t h = (uint16_t)((uint16_t)(p->h << 4) >> 8);

    if (p->glyph)
        scaled_sprite(p, p->x, p->y, w, h, right, bottom);
}

/* @routine 07:FA5A FxScaledSpriteEighth: p->w / 32 across, p->h / 32 down; glyph 0 draws nothing. */
void FxScaledSpriteEighth(const FxPiece *p, uint16_t right, uint16_t bottom)
{
    uint16_t w = (uint16_t)((uint16_t)(p->w << 3) >> 8), h = (uint16_t)((uint16_t)(p->h << 3) >> 8);

    if (p->glyph)
        scaled_sprite(p, p->x, p->y, w, h, right, bottom);
}

/* @routine 07:FAF2 FxScaledSpriteAdvance: p->w / 16 across, p->h / 8 down, x on by its width; glyph 0 draws nothing. */
void FxScaledSpriteAdvance(FxPiece *p, uint16_t right, uint16_t bottom)
{
    uint16_t x = p->x, w = (uint16_t)((uint16_t)(p->w << 4) >> 8), h = (uint16_t)((uint16_t)(p->h << 5) >> 8);

    p->x = (uint16_t)(x + w);
    if (p->glyph)
        scaled_sprite(p, x, p->y, w, h, right, bottom);
}

/* @routine 07:FA88 FxSprite24x40: 24 x 40 through p->u and p->v, x on by 24; glyph 0 draws nothing. */
void FxSprite24x40(FxPiece *p, uint16_t right, uint16_t bottom)
{
    uint16_t x = p->x;

    p->x = (uint16_t)(x + 24);
    if (p->glyph)
        scaled_sprite(p, x, p->y, 24, 40, right, bottom);
}

/*
 * @routine 07:FAB0 FxSprite16x32
 * WHAT IT DOES: Sets both steps to 1 ($100); then with x from -15 to $D0
 * draws the 16 x 32 sprite unscaled and moves x on by 16. From $D1 to
 * $FFF0 the original's branch runs on into FxScaledSpriteAdvance (an IWT in
 * its delay slot takes the bytes at the target), which this does too.
 * Glyph 0 draws nothing.
 */
void FxSprite16x32(FxPiece *p, uint16_t right, uint16_t bottom)
{
    uint16_t x = p->x;

    p->u = p->v = 0x0100;
    if (x > 0x00D0 && x <= 0xFFF0) {
        FxScaledSpriteAdvance(p, right, bottom);
        return;
    }
    p->x = (uint16_t)(x + 16);
    if (p->glyph)
        scaled_sprite(p, x, p->y, 16, 32, right, bottom);
}
