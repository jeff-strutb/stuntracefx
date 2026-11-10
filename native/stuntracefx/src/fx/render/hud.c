/*
 * hud.c: the Super FX code of asm/stuntracefx/render/hud.asm, written from it by
 * the generator.
 *
 * hud.asm: the Super FX programs in bank 07 that draw the 2D pieces of the
 * screen (glyphs and digits, the rising picture, scaled sprites, the noise
 * fill) into the Super FX's screen buffer. The S-CPU sets their inputs in
 * Super FX RAM and starts each one (gsu/programs.asm); each ends at a STOP.
 *
 * Bitmaps are 2 bits a pixel in ROM bank $15, 32 bytes a row; the low two
 * bits of a byte are its leftmost pixel and pick one of the four colour
 * bytes at $70:0200. The instruction after a branch or LOOP (its delay
 * slot) always runs.
 * C: src/stuntracefx/render/hud.c.
 */

#include "fx/fx.h"

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


/*
 * @routine 07:EEF5 FxPictureRiseSplit40
 * WHAT IT DOES: Draws the picture at $15:3D00 rising out of line $40, the
 * top $70:020C rows of it, in two pieces: picture columns 56-111 at x = $1C
 * and columns 0-39 at x = $54.
 * HOW: Each piece: y = $40 - height; rows of 14 (then 10) bitmap bytes, 32
 * bytes apart, each byte four pixels through the colour table at $70:0200;
 * RPIX after each piece.
 * Entry: started by the S-CPU; the colour bytes and the height set. Exit: R0
 * = the pixel at ($54, $40); R1 = R8 = $54; R2 = $40; R3 = 0; R6 = the last
 * byte rotated right 8 times through the carry; R11 = $0200; R12 = 0; R13 =
 * $EF81; R14 past the last row; ROM bank $15, RAM bank 0, plot options 0;
 * the last RAM address that of the last colour byte read.
 */
void FxPictureRiseSplit40(void)
{
    uint16_t count;
    uint16_t rom_at;
    uint16_t count_2;
    uint16_t rom_at_2;
    uint16_t rise_rows;
    uint16_t left_draw_y;
    uint16_t glyph_byte_1;
    uint16_t glyph_byte_2;
    uint16_t left_pixel_x;
    uint16_t right_draw_y;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_color_5;
    uint16_t pixel_color_6;
    uint16_t pixel_color_7;
    uint16_t pixel_color_8;
    uint16_t pixel_index_1;
    uint16_t pixel_index_2;
    uint16_t pixel_index_3;
    uint16_t pixel_index_4;
    uint16_t right_pixel_x;
    uint16_t left_row_count;
    uint16_t left_rows_left;
    uint16_t next_pixel_x_1;
    uint16_t next_pixel_x_2;
    uint16_t next_pixel_x_3;
    uint16_t next_pixel_x_4;
    uint16_t next_pixel_x_5;
    uint16_t next_pixel_x_6;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t shifted_bits_4;
    uint16_t shifted_bits_5;
    uint16_t shifted_bits_6;
    uint16_t right_row_count;
    uint16_t right_rows_left;
    uint16_t rise_rows_right;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    rise_rows = fx_ldw(0x020C);
    left_draw_y = (uint16_t)((uint16_t)(0 - rise_rows) + 0x0040);
    left_pixel_x = 0x001C;
    rom_at = 0x3D0E;
    fx_r14(rom_at);
    count = rise_rows;
    for (;;) {
        fx_bitmap_row(left_pixel_x, left_draw_y, 56, 0x0200, rom_at);
        left_row_count = 0x000E;
        for (;;) {
            glyph_byte_1 = fx_getb();
            pixel_index_1 = (glyph_byte_1 & 3);
            pixel_color_1 = fx_ldb((uint16_t)(pixel_index_1 + 0x0200));
            fx_color((uint8_t)pixel_color_1);
            next_pixel_x_1 = (uint16_t)(left_pixel_x + 1);
            shifted_bits_1 = (uint16_t)(glyph_byte_1 >> 1 | ((uint32_t)pixel_index_1 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (glyph_byte_1 & 1) << 15);
            rom_at = (uint16_t)(rom_at + 1);
            fx_r14(rom_at);
            shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
            pixel_index_2 = (shifted_bits_2 & 3);
            pixel_color_2 = fx_ldb((uint16_t)(pixel_index_2 + 0x0200));
            fx_color((uint8_t)pixel_color_2);
            next_pixel_x_2 = (uint16_t)(next_pixel_x_1 + 1);
            pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_3);
            next_pixel_x_3 = (uint16_t)(next_pixel_x_2 + 1);
            pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)pixel_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_4);
            left_pixel_x = (uint16_t)(next_pixel_x_3 + 1);
            left_rows_left = (uint16_t)(left_row_count - 1);
            left_row_count = left_rows_left;
            if (left_rows_left != 0) {
                continue;
            } else {
                break;
            }
        }
        rom_at = (uint16_t)(rom_at + 0x0012);
        fx_r14(rom_at);
        left_pixel_x = 0x001C;
        count = (uint16_t)(count - 1);
        left_draw_y = (uint16_t)(left_draw_y + 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)left_pixel_x, (uint8_t)left_draw_y);
    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    rise_rows_right = fx_ldw(0x020C);
    right_pixel_x = 0x0054;
    rom_at_2 = 0x3D00;
    fx_r14(rom_at_2);
    right_draw_y = (uint16_t)((uint16_t)(0 - rise_rows_right) + 0x0040);
    count_2 = rise_rows_right;
    for (;;) {
        fx_bitmap_row(right_pixel_x, right_draw_y, 40, 0x0200, rom_at_2);
        right_row_count = 0x000A;
        for (;;) {
            glyph_byte_2 = fx_getb();
            pixel_index_3 = (glyph_byte_2 & 3);
            pixel_color_5 = fx_ldb((uint16_t)(pixel_index_3 + 0x0200));
            fx_color((uint8_t)pixel_color_5);
            next_pixel_x_4 = (uint16_t)(right_pixel_x + 1);
            shifted_bits_4 = (uint16_t)(glyph_byte_2 >> 1 | ((uint32_t)pixel_index_3 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_5 = (uint16_t)(shifted_bits_4 >> 1 | (glyph_byte_2 & 1) << 15);
            rom_at_2 = (uint16_t)(rom_at_2 + 1);
            fx_r14(rom_at_2);
            shifted_bits_6 = (uint16_t)((uint16_t)(shifted_bits_5 >> 1 | (shifted_bits_4 & 1) << 15) >> 1 | (shifted_bits_5 & 1) << 15);
            pixel_index_4 = (shifted_bits_5 & 3);
            pixel_color_6 = fx_ldb((uint16_t)(pixel_index_4 + 0x0200));
            fx_color((uint8_t)pixel_color_6);
            next_pixel_x_5 = (uint16_t)(next_pixel_x_4 + 1);
            pixel_color_7 = fx_ldb((uint16_t)((shifted_bits_6 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_7);
            next_pixel_x_6 = (uint16_t)(next_pixel_x_5 + 1);
            pixel_color_8 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_6 >> 1 | ((uint32_t)pixel_index_4 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_6 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            right_pixel_x = (uint16_t)(next_pixel_x_6 + 1);
            right_rows_left = (uint16_t)(right_row_count - 1);
            right_row_count = right_rows_left;
            if (right_rows_left != 0) {
                continue;
            } else {
                break;
            }
        }
        rom_at_2 = (uint16_t)(rom_at_2 + 0x0016);
        fx_r14(rom_at_2);
        right_pixel_x = 0x0054;
        count_2 = (uint16_t)(count_2 - 1);
        right_draw_y = (uint16_t)(right_draw_y + 1);
        if (count_2 != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)right_pixel_x, (uint8_t)right_draw_y);
    return;
}


/*
 * @routine 07:EFCD FxPictureRiseSplit60
 * WHAT IT DOES: As FxPictureRiseSplit40, rising out of line $60, the pieces
 * at x = $34 and $6C.
 * HOW: As FxPictureRiseSplit40.
 * Entry: as FxPictureRiseSplit40. Exit: as FxPictureRiseSplit40, with R0 =
 * the pixel at ($6C, $60), R1 = R8 = $6C, R2 = $60 and R13 = $F059.
 */
void FxPictureRiseSplit60(void)
{
    uint16_t count;
    uint16_t rom_at;
    uint16_t count_2;
    uint16_t rom_at_2;
    uint16_t pixel_x_1;
    uint16_t pixel_x_2;
    uint16_t pixel_x_3;
    uint16_t pixel_x_4;
    uint16_t pixel_x_5;
    uint16_t pixel_x_6;
    uint16_t rise_rows;
    uint16_t left_row_y;
    uint16_t src_byte_1;
    uint16_t src_byte_2;
    uint16_t right_row_y;
    uint16_t rise_rows_2;
    uint16_t bytes_left_1;
    uint16_t bytes_left_2;
    uint16_t left_start_x;
    uint16_t color_index_1;
    uint16_t color_index_2;
    uint16_t color_index_3;
    uint16_t color_index_4;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_color_5;
    uint16_t pixel_color_6;
    uint16_t pixel_color_7;
    uint16_t pixel_color_8;
    uint16_t right_start_x;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t shifted_bits_4;
    uint16_t shifted_bits_5;
    uint16_t shifted_bits_6;
    uint16_t left_bytes_left;
    uint16_t right_bytes_left;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    rise_rows = fx_ldw(0x020C);
    left_row_y = (uint16_t)((uint16_t)(0 - rise_rows) + 0x0060);
    left_start_x = 0x0034;
    rom_at = 0x3D0E;
    fx_r14(rom_at);
    count = rise_rows;
    for (;;) {
        fx_bitmap_row(left_start_x, left_row_y, 56, 0x0200, rom_at);
        left_bytes_left = 0x000E;
        for (;;) {
            src_byte_1 = fx_getb();
            color_index_1 = (src_byte_1 & 3);
            pixel_color_1 = fx_ldb((uint16_t)(color_index_1 + 0x0200));
            fx_color((uint8_t)pixel_color_1);
            pixel_x_1 = (uint16_t)(left_start_x + 1);
            shifted_bits_1 = (uint16_t)(src_byte_1 >> 1 | ((uint32_t)color_index_1 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (src_byte_1 & 1) << 15);
            rom_at = (uint16_t)(rom_at + 1);
            fx_r14(rom_at);
            shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
            color_index_2 = (shifted_bits_2 & 3);
            pixel_color_2 = fx_ldb((uint16_t)(color_index_2 + 0x0200));
            fx_color((uint8_t)pixel_color_2);
            pixel_x_2 = (uint16_t)(pixel_x_1 + 1);
            pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_3);
            pixel_x_3 = (uint16_t)(pixel_x_2 + 1);
            pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)color_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_4);
            left_start_x = (uint16_t)(pixel_x_3 + 1);
            bytes_left_1 = (uint16_t)(left_bytes_left - 1);
            left_bytes_left = bytes_left_1;
            if (bytes_left_1 != 0) {
                continue;
            } else {
                break;
            }
        }
        rom_at = (uint16_t)(rom_at + 0x0012);
        fx_r14(rom_at);
        left_start_x = 0x0034;
        count = (uint16_t)(count - 1);
        left_row_y = (uint16_t)(left_row_y + 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)left_start_x, (uint8_t)left_row_y);
    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    rise_rows_2 = fx_ldw(0x020C);
    right_start_x = 0x006C;
    rom_at_2 = 0x3D00;
    fx_r14(rom_at_2);
    right_row_y = (uint16_t)((uint16_t)(0 - rise_rows_2) + 0x0060);
    count_2 = rise_rows_2;
    for (;;) {
        fx_bitmap_row(right_start_x, right_row_y, 40, 0x0200, rom_at_2);
        right_bytes_left = 0x000A;
        for (;;) {
            src_byte_2 = fx_getb();
            color_index_3 = (src_byte_2 & 3);
            pixel_color_5 = fx_ldb((uint16_t)(color_index_3 + 0x0200));
            fx_color((uint8_t)pixel_color_5);
            pixel_x_4 = (uint16_t)(right_start_x + 1);
            shifted_bits_4 = (uint16_t)(src_byte_2 >> 1 | ((uint32_t)color_index_3 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_5 = (uint16_t)(shifted_bits_4 >> 1 | (src_byte_2 & 1) << 15);
            rom_at_2 = (uint16_t)(rom_at_2 + 1);
            fx_r14(rom_at_2);
            shifted_bits_6 = (uint16_t)((uint16_t)(shifted_bits_5 >> 1 | (shifted_bits_4 & 1) << 15) >> 1 | (shifted_bits_5 & 1) << 15);
            color_index_4 = (shifted_bits_5 & 3);
            pixel_color_6 = fx_ldb((uint16_t)(color_index_4 + 0x0200));
            fx_color((uint8_t)pixel_color_6);
            pixel_x_5 = (uint16_t)(pixel_x_4 + 1);
            pixel_color_7 = fx_ldb((uint16_t)((shifted_bits_6 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_7);
            pixel_x_6 = (uint16_t)(pixel_x_5 + 1);
            pixel_color_8 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_6 >> 1 | ((uint32_t)color_index_4 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_6 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            right_start_x = (uint16_t)(pixel_x_6 + 1);
            bytes_left_2 = (uint16_t)(right_bytes_left - 1);
            right_bytes_left = bytes_left_2;
            if (bytes_left_2 != 0) {
                continue;
            } else {
                break;
            }
        }
        rom_at_2 = (uint16_t)(rom_at_2 + 0x0016);
        fx_r14(rom_at_2);
        right_start_x = 0x006C;
        count_2 = (uint16_t)(count_2 - 1);
        right_row_y = (uint16_t)(right_row_y + 1);
        if (count_2 != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)right_start_x, (uint8_t)right_row_y);
    return;
}


/*
 * @routine 07:F0A5 FxPictureRise40
 * WHAT IT DOES: Draws the picture at $15:3D00 rising out of line $40, the
 * top $70:020C rows of its first 56 columns, at x = $28.
 * HOW: y = $40 - height; rows of 14 bitmap bytes, 32 bytes apart, each byte
 * four pixels through the colour table at $70:0200; RPIX at the end.
 * Entry: started by the S-CPU; the colour bytes and the height set. Exit: R0
 * = the pixel at ($28, $40); R1 = R8 = $28; R2 = $40; R3 = 0; R6 = the last
 * byte rotated right 8 times through the carry; R11 = $0200; R12 = 0; R13 =
 * $F0D5; R14 past the last row; ROM bank $15, RAM bank 0, plot options 0;
 * the last RAM address that of the last colour byte read.
 */
void FxPictureRise40(void)
{
    uint16_t count;
    uint16_t row_y;
    uint16_t rom_at;
    uint16_t pixel_x_2;
    uint16_t pixel_x_9;
    uint16_t pixel_x_15;
    uint16_t pixel_x_17;
    uint16_t pixel_color_8;
    uint16_t pixel_index_7;
    uint16_t picture_height;
    uint16_t pixel_color_14;
    uint16_t pixel_color_16;
    uint16_t pixel_color_18;
    uint16_t pixel_index_13;
    uint16_t packed_pixels_6;
    uint16_t bytes_left_after;
    uint16_t shifted_pixels_10;
    uint16_t shifted_pixels_11;
    uint16_t shifted_pixels_12;
    uint16_t bytes_per_row_left;

    picture_height = fx_ldw(0x020C);
    row_y = (uint16_t)((uint16_t)(0 - picture_height) + 0x0040);
    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    pixel_x_2 = 0x0028;
    rom_at = 0x3D00;
    fx_r14(rom_at);
    count = picture_height;
    for (;;) {
        fx_bitmap_row(pixel_x_2, row_y, 56, 0x0200, rom_at);
        bytes_per_row_left = 0x000E;
        for (;;) {
            packed_pixels_6 = fx_getb();
            pixel_index_7 = (packed_pixels_6 & 3);
            pixel_color_8 = fx_ldb((uint16_t)(pixel_index_7 + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            pixel_x_9 = (uint16_t)(pixel_x_2 + 1);
            shifted_pixels_10 = (uint16_t)(packed_pixels_6 >> 1 | ((uint32_t)pixel_index_7 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_11 = (uint16_t)(shifted_pixels_10 >> 1 | (packed_pixels_6 & 1) << 15);
            rom_at = (uint16_t)(rom_at + 1);
            fx_r14(rom_at);
            shifted_pixels_12 = (uint16_t)((uint16_t)(shifted_pixels_11 >> 1 | (shifted_pixels_10 & 1) << 15) >> 1 | (shifted_pixels_11 & 1) << 15);
            pixel_index_13 = (shifted_pixels_11 & 3);
            pixel_color_14 = fx_ldb((uint16_t)(pixel_index_13 + 0x0200));
            fx_color((uint8_t)pixel_color_14);
            pixel_x_15 = (uint16_t)(pixel_x_9 + 1);
            pixel_color_16 = fx_ldb((uint16_t)((shifted_pixels_12 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_16);
            pixel_x_17 = (uint16_t)(pixel_x_15 + 1);
            pixel_color_18 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_12 >> 1 | ((uint32_t)pixel_index_13 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_12 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_18);
            pixel_x_2 = (uint16_t)(pixel_x_17 + 1);
            bytes_left_after = (uint16_t)(bytes_per_row_left - 1);
            bytes_per_row_left = bytes_left_after;
            if (bytes_left_after != 0) {
                continue;
            } else {
                break;
            }
        }
        rom_at = (uint16_t)(rom_at + 0x0012);
        fx_r14(rom_at);
        pixel_x_2 = 0x0028;
        count = (uint16_t)(count - 1);
        row_y = (uint16_t)(row_y + 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)pixel_x_2, (uint8_t)row_y);
    return;
}


/*
 * @routine 07:F0B4 FxPictureRise60
 * WHAT IT DOES: As FxPictureRise40, rising out of line $60, at x = $4C.
 * HOW: As FxPictureRise40.
 * Entry: as FxPictureRise40. Exit: as FxPictureRise40, with R0 = the pixel
 * at ($4C, $60), R1 = R8 = $4C and R2 = $60.
 */
void FxPictureRise60(void)
{
    uint16_t count;
    uint16_t row_y;
    uint16_t rom_at;
    uint16_t start_x;
    uint16_t pixel_x_1;
    uint16_t pixel_x_2;
    uint16_t pixel_x_3;
    uint16_t rise_rows;
    uint16_t src_byte_1;
    uint16_t bytes_left_1;
    uint16_t color_index_1;
    uint16_t color_index_2;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t bytes_left_init;

    rise_rows = fx_ldw(0x020C);
    row_y = (uint16_t)((uint16_t)(0 - rise_rows) + 0x0060);
    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    start_x = 0x004C;
    rom_at = 0x3D00;
    fx_r14(rom_at);
    count = rise_rows;
    for (;;) {
        fx_bitmap_row(start_x, row_y, 56, 0x0200, rom_at);
        bytes_left_init = 0x000E;
        for (;;) {
            src_byte_1 = fx_getb();
            color_index_1 = (src_byte_1 & 3);
            pixel_color_1 = fx_ldb((uint16_t)(color_index_1 + 0x0200));
            fx_color((uint8_t)pixel_color_1);
            pixel_x_1 = (uint16_t)(start_x + 1);
            shifted_bits_1 = (uint16_t)(src_byte_1 >> 1 | ((uint32_t)color_index_1 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (src_byte_1 & 1) << 15);
            rom_at = (uint16_t)(rom_at + 1);
            fx_r14(rom_at);
            shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
            color_index_2 = (shifted_bits_2 & 3);
            pixel_color_2 = fx_ldb((uint16_t)(color_index_2 + 0x0200));
            fx_color((uint8_t)pixel_color_2);
            pixel_x_2 = (uint16_t)(pixel_x_1 + 1);
            pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_3);
            pixel_x_3 = (uint16_t)(pixel_x_2 + 1);
            pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)color_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_4);
            start_x = (uint16_t)(pixel_x_3 + 1);
            bytes_left_1 = (uint16_t)(bytes_left_init - 1);
            bytes_left_init = bytes_left_1;
            if (bytes_left_1 != 0) {
                continue;
            } else {
                break;
            }
        }
        rom_at = (uint16_t)(rom_at + 0x0012);
        fx_r14(rom_at);
        start_x = 0x004C;
        count = (uint16_t)(count - 1);
        row_y = (uint16_t)(row_y + 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)start_x, (uint8_t)row_y);
    return;
}


/*
 * @routine 07:F121 FxGlyph16RowsFromBottom
 * WHAT IT DOES: Draws the bottom $70:0210 rows of the 16 x 32 glyph at
 * $70:0216 (in bank $15) at ($70:020A, $70:020C), and moves x on by
 * $70:020E; nothing is drawn when the glyph address is 0. With
 * FxGlyph16RowsFromTop it rolls a digit into its cell.
 * HOW: The glyph's start moves down 32 - rows rows; each row is four bitmap
 * bytes through the colour table at $70:0200, with an RPIX after each byte.
 * Entry: started by the S-CPU. Exit: when drawn, R0 = $1C; R1 = R8 = x; R2 =
 * y + rows; R3 = rows; R6 = the last byte rotated right 8 times through the
 * carry; R11 = $0200; R12 = 0; R13 = $F191; R14 past the last row; ROM bank
 * $15, RAM bank 0, plot options 0; the last RAM address that of the last
 * colour byte read. When not drawn: R0 = x + the advance; R1 = R8 = x; R14 =
 * 0; the last RAM address $0216; OV, S clear, Z set.
 */
void FxGlyph16RowsFromBottom(void)
{
    uint16_t count;
    uint16_t cell_y;
    uint16_t in_216;
    uint16_t rom_at;
    uint16_t glyph_x;
    uint16_t row_count;
    uint16_t glyph_byte_1;
    uint16_t glyph_byte_2;
    uint16_t glyph_byte_3;
    uint16_t glyph_byte_4;
    uint16_t glyph_advance;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_color_5;
    uint16_t pixel_color_6;
    uint16_t pixel_color_7;
    uint16_t pixel_color_8;
    uint16_t pixel_color_9;
    uint16_t pixel_index_1;
    uint16_t pixel_index_2;
    uint16_t pixel_index_3;
    uint16_t pixel_index_4;
    uint16_t pixel_index_5;
    uint16_t pixel_index_6;
    uint16_t pixel_index_7;
    uint16_t pixel_index_8;
    uint16_t pixel_x_start;
    uint16_t next_pixel_x_1;
    uint16_t next_pixel_x_2;
    uint16_t next_pixel_x_3;
    uint16_t next_pixel_x_4;
    uint16_t next_pixel_x_5;
    uint16_t next_pixel_x_6;
    uint16_t next_pixel_x_7;
    uint16_t next_pixel_x_8;
    uint16_t next_pixel_x_9;
    uint16_t next_src_ptr_1;
    uint16_t next_src_ptr_2;
    uint16_t next_src_ptr_3;
    uint16_t next_src_ptr_4;
    uint16_t pixel_color_10;
    uint16_t pixel_color_11;
    uint16_t pixel_color_12;
    uint16_t pixel_color_13;
    uint16_t pixel_color_14;
    uint16_t pixel_color_15;
    uint16_t pixel_color_16;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t shifted_bits_4;
    uint16_t shifted_bits_5;
    uint16_t shifted_bits_6;
    uint16_t shifted_bits_7;
    uint16_t shifted_bits_8;
    uint16_t shifted_bits_9;
    uint16_t next_pixel_x_10;
    uint16_t next_pixel_x_11;
    uint16_t next_pixel_x_12;
    uint16_t next_pixel_x_13;
    uint16_t next_pixel_x_14;
    uint16_t next_pixel_x_15;
    uint16_t rows_skipped_x2;
    uint16_t rows_skipped_x4;
    uint16_t rows_skipped_x8;
    uint16_t shifted_bits_10;
    uint16_t shifted_bits_11;
    uint16_t shifted_bits_12;
    uint16_t neg_rows_plus_32;
    uint16_t rows_skipped_x16;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    glyph_advance = fx_ldw(0x020E);
    glyph_x = fx_ldw(0x020A);
    pixel_x_start = glyph_x;
    fx_sbk((uint16_t)(glyph_advance + glyph_x));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    fx_r14(in_216);
    if (in_216 != 0) {
        cell_y = fx_ldw(0x020C);
        row_count = fx_ldw(0x0210);
        neg_rows_plus_32 = (uint16_t)((uint16_t)(0 - row_count) + 0x0020);
        rows_skipped_x2 = (uint16_t)(neg_rows_plus_32 + neg_rows_plus_32);
        rows_skipped_x4 = (uint16_t)(rows_skipped_x2 + rows_skipped_x2);
        rows_skipped_x8 = (uint16_t)(rows_skipped_x4 + rows_skipped_x4);
        rows_skipped_x16 = (uint16_t)(rows_skipped_x8 + rows_skipped_x8);
        rom_at = (uint16_t)((uint16_t)(rows_skipped_x16 + rows_skipped_x16) + in_216);
        fx_r14(rom_at);
        count = row_count;
        for (;;) {
            fx_bitmap_row(pixel_x_start, cell_y, 16, 0x0200, rom_at);
            glyph_byte_1 = fx_getb();
            pixel_index_1 = (glyph_byte_1 & 3);
            pixel_color_1 = fx_ldb((uint16_t)(pixel_index_1 + 0x0200));
            fx_color((uint8_t)pixel_color_1);
            next_pixel_x_1 = (uint16_t)(pixel_x_start + 1);
            shifted_bits_1 = (uint16_t)(glyph_byte_1 >> 1 | ((uint32_t)pixel_index_1 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (glyph_byte_1 & 1) << 15);
            next_src_ptr_1 = (uint16_t)(rom_at + 1);
            fx_r14(next_src_ptr_1);
            shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
            pixel_index_2 = (shifted_bits_2 & 3);
            pixel_color_2 = fx_ldb((uint16_t)(pixel_index_2 + 0x0200));
            fx_color((uint8_t)pixel_color_2);
            next_pixel_x_2 = (uint16_t)(next_pixel_x_1 + 1);
            pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_3);
            next_pixel_x_3 = (uint16_t)(next_pixel_x_2 + 1);
            pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)pixel_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_4);
            next_pixel_x_4 = (uint16_t)(next_pixel_x_3 + 1);
            fx_rpix((uint8_t)next_pixel_x_4, (uint8_t)cell_y);
            glyph_byte_2 = fx_getb();
            pixel_index_3 = (glyph_byte_2 & 3);
            pixel_color_5 = fx_ldb((uint16_t)(pixel_index_3 + 0x0200));
            fx_color((uint8_t)pixel_color_5);
            next_pixel_x_5 = (uint16_t)(next_pixel_x_4 + 1);
            shifted_bits_4 = (uint16_t)(glyph_byte_2 >> 1 | ((uint32_t)pixel_index_3 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_5 = (uint16_t)(shifted_bits_4 >> 1 | (glyph_byte_2 & 1) << 15);
            next_src_ptr_2 = (uint16_t)(next_src_ptr_1 + 1);
            fx_r14(next_src_ptr_2);
            shifted_bits_6 = (uint16_t)((uint16_t)(shifted_bits_5 >> 1 | (shifted_bits_4 & 1) << 15) >> 1 | (shifted_bits_5 & 1) << 15);
            pixel_index_4 = (shifted_bits_5 & 3);
            pixel_color_6 = fx_ldb((uint16_t)(pixel_index_4 + 0x0200));
            fx_color((uint8_t)pixel_color_6);
            next_pixel_x_6 = (uint16_t)(next_pixel_x_5 + 1);
            pixel_color_7 = fx_ldb((uint16_t)((shifted_bits_6 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_7);
            next_pixel_x_7 = (uint16_t)(next_pixel_x_6 + 1);
            pixel_color_8 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_6 >> 1 | ((uint32_t)pixel_index_4 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_6 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            next_pixel_x_8 = (uint16_t)(next_pixel_x_7 + 1);
            fx_rpix((uint8_t)next_pixel_x_8, (uint8_t)cell_y);
            glyph_byte_3 = fx_getb();
            pixel_index_5 = (glyph_byte_3 & 3);
            pixel_color_9 = fx_ldb((uint16_t)(pixel_index_5 + 0x0200));
            fx_color((uint8_t)pixel_color_9);
            next_pixel_x_9 = (uint16_t)(next_pixel_x_8 + 1);
            shifted_bits_7 = (uint16_t)(glyph_byte_3 >> 1 | ((uint32_t)pixel_index_5 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_8 = (uint16_t)(shifted_bits_7 >> 1 | (glyph_byte_3 & 1) << 15);
            next_src_ptr_3 = (uint16_t)(next_src_ptr_2 + 1);
            fx_r14(next_src_ptr_3);
            shifted_bits_9 = (uint16_t)((uint16_t)(shifted_bits_8 >> 1 | (shifted_bits_7 & 1) << 15) >> 1 | (shifted_bits_8 & 1) << 15);
            pixel_index_6 = (shifted_bits_8 & 3);
            pixel_color_10 = fx_ldb((uint16_t)(pixel_index_6 + 0x0200));
            fx_color((uint8_t)pixel_color_10);
            next_pixel_x_10 = (uint16_t)(next_pixel_x_9 + 1);
            pixel_color_11 = fx_ldb((uint16_t)((shifted_bits_9 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_11);
            next_pixel_x_11 = (uint16_t)(next_pixel_x_10 + 1);
            pixel_color_12 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_9 >> 1 | ((uint32_t)pixel_index_6 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_9 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_12);
            next_pixel_x_12 = (uint16_t)(next_pixel_x_11 + 1);
            fx_rpix((uint8_t)next_pixel_x_12, (uint8_t)cell_y);
            glyph_byte_4 = fx_getb();
            pixel_index_7 = (glyph_byte_4 & 3);
            pixel_color_13 = fx_ldb((uint16_t)(pixel_index_7 + 0x0200));
            fx_color((uint8_t)pixel_color_13);
            next_pixel_x_13 = (uint16_t)(next_pixel_x_12 + 1);
            shifted_bits_10 = (uint16_t)(glyph_byte_4 >> 1 | ((uint32_t)pixel_index_7 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_11 = (uint16_t)(shifted_bits_10 >> 1 | (glyph_byte_4 & 1) << 15);
            next_src_ptr_4 = (uint16_t)(next_src_ptr_3 + 1);
            fx_r14(next_src_ptr_4);
            shifted_bits_12 = (uint16_t)((uint16_t)(shifted_bits_11 >> 1 | (shifted_bits_10 & 1) << 15) >> 1 | (shifted_bits_11 & 1) << 15);
            pixel_index_8 = (shifted_bits_11 & 3);
            pixel_color_14 = fx_ldb((uint16_t)(pixel_index_8 + 0x0200));
            fx_color((uint8_t)pixel_color_14);
            next_pixel_x_14 = (uint16_t)(next_pixel_x_13 + 1);
            pixel_color_15 = fx_ldb((uint16_t)((shifted_bits_12 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_15);
            next_pixel_x_15 = (uint16_t)(next_pixel_x_14 + 1);
            pixel_color_16 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_12 >> 1 | ((uint32_t)pixel_index_8 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_12 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_16);
            fx_rpix((uint8_t)(uint16_t)(next_pixel_x_15 + 1), (uint8_t)cell_y);
            rom_at = (uint16_t)(next_src_ptr_4 + 0x001C);
            fx_r14(rom_at);
            pixel_x_start = glyph_x;
            count = (uint16_t)(count - 1);
            cell_y = (uint16_t)(cell_y + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        return;
    } else {
        return;
    }
}


/*
 * @routine 07:F15B FxGlyph16RowsFromTop
 * WHAT IT DOES: Draws the top $70:0210 rows of the 16 x 32 glyph at $70:0216
 * (in bank $15) at the bottom of the 32-line cell at ($70:020A, $70:020C),
 * and moves x on by $70:020E; nothing is drawn when the glyph address is 0.
 * With $0210 = 32 it draws the whole glyph (digits).
 * HOW: y = cell y + 32 - rows; each row is four bitmap bytes through the
 * colour table at $70:0200, with an RPIX after each byte.
 * Entry: started by the S-CPU. Exit: when drawn, as FxGlyph16RowsFromBottom,
 * with R2 = cell y + 32. When not drawn: R0 = x + the advance; R1 = R8 = x;
 * R2 = cell y + 32 - rows; R3 = rows; R11 = $0200; R14 = 0; the last RAM
 * address $0216; OV, S clear, Z set.
 */
void FxGlyph16RowsFromTop(void)
{
    uint16_t count;
    uint16_t cell_y;
    uint16_t draw_y;
    uint16_t in_216;
    uint16_t rom_at;
    uint16_t glyph_x;
    uint16_t row_count;
    uint16_t glyph_byte_1;
    uint16_t glyph_byte_2;
    uint16_t glyph_byte_3;
    uint16_t glyph_byte_4;
    uint16_t glyph_advance;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_color_5;
    uint16_t pixel_color_6;
    uint16_t pixel_color_7;
    uint16_t pixel_color_8;
    uint16_t pixel_color_9;
    uint16_t pixel_index_1;
    uint16_t pixel_index_2;
    uint16_t pixel_index_3;
    uint16_t pixel_index_4;
    uint16_t pixel_index_5;
    uint16_t pixel_index_6;
    uint16_t pixel_index_7;
    uint16_t pixel_index_8;
    uint16_t pixel_x_start;
    uint16_t next_pixel_x_1;
    uint16_t next_pixel_x_2;
    uint16_t next_pixel_x_3;
    uint16_t next_pixel_x_4;
    uint16_t next_pixel_x_5;
    uint16_t next_pixel_x_6;
    uint16_t next_pixel_x_7;
    uint16_t next_pixel_x_8;
    uint16_t next_pixel_x_9;
    uint16_t next_src_ptr_1;
    uint16_t next_src_ptr_2;
    uint16_t next_src_ptr_3;
    uint16_t next_src_ptr_4;
    uint16_t pixel_color_10;
    uint16_t pixel_color_11;
    uint16_t pixel_color_12;
    uint16_t pixel_color_13;
    uint16_t pixel_color_14;
    uint16_t pixel_color_15;
    uint16_t pixel_color_16;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t shifted_bits_4;
    uint16_t shifted_bits_5;
    uint16_t shifted_bits_6;
    uint16_t shifted_bits_7;
    uint16_t shifted_bits_8;
    uint16_t shifted_bits_9;
    uint16_t next_pixel_x_10;
    uint16_t next_pixel_x_11;
    uint16_t next_pixel_x_12;
    uint16_t next_pixel_x_13;
    uint16_t next_pixel_x_14;
    uint16_t next_pixel_x_15;
    uint16_t shifted_bits_10;
    uint16_t shifted_bits_11;
    uint16_t shifted_bits_12;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    row_count = fx_ldw(0x0210);
    cell_y = fx_ldw(0x020C);
    draw_y = (uint16_t)((uint16_t)(cell_y - row_count) + 0x0020);
    glyph_advance = fx_ldw(0x020E);
    glyph_x = fx_ldw(0x020A);
    pixel_x_start = glyph_x;
    fx_sbk((uint16_t)(glyph_advance + glyph_x));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    rom_at = in_216;
    fx_r14(rom_at);
    if (in_216 != 0) {
        count = row_count;
        for (;;) {
            fx_bitmap_row(pixel_x_start, draw_y, 16, 0x0200, rom_at);
            glyph_byte_1 = fx_getb();
            pixel_index_1 = (glyph_byte_1 & 3);
            pixel_color_1 = fx_ldb((uint16_t)(pixel_index_1 + 0x0200));
            fx_color((uint8_t)pixel_color_1);
            next_pixel_x_1 = (uint16_t)(pixel_x_start + 1);
            shifted_bits_1 = (uint16_t)(glyph_byte_1 >> 1 | ((uint32_t)pixel_index_1 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (glyph_byte_1 & 1) << 15);
            next_src_ptr_1 = (uint16_t)(rom_at + 1);
            fx_r14(next_src_ptr_1);
            shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
            pixel_index_2 = (shifted_bits_2 & 3);
            pixel_color_2 = fx_ldb((uint16_t)(pixel_index_2 + 0x0200));
            fx_color((uint8_t)pixel_color_2);
            next_pixel_x_2 = (uint16_t)(next_pixel_x_1 + 1);
            pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_3);
            next_pixel_x_3 = (uint16_t)(next_pixel_x_2 + 1);
            pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)pixel_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_4);
            next_pixel_x_4 = (uint16_t)(next_pixel_x_3 + 1);
            fx_rpix((uint8_t)next_pixel_x_4, (uint8_t)draw_y);
            glyph_byte_2 = fx_getb();
            pixel_index_3 = (glyph_byte_2 & 3);
            pixel_color_5 = fx_ldb((uint16_t)(pixel_index_3 + 0x0200));
            fx_color((uint8_t)pixel_color_5);
            next_pixel_x_5 = (uint16_t)(next_pixel_x_4 + 1);
            shifted_bits_4 = (uint16_t)(glyph_byte_2 >> 1 | ((uint32_t)pixel_index_3 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_5 = (uint16_t)(shifted_bits_4 >> 1 | (glyph_byte_2 & 1) << 15);
            next_src_ptr_2 = (uint16_t)(next_src_ptr_1 + 1);
            fx_r14(next_src_ptr_2);
            shifted_bits_6 = (uint16_t)((uint16_t)(shifted_bits_5 >> 1 | (shifted_bits_4 & 1) << 15) >> 1 | (shifted_bits_5 & 1) << 15);
            pixel_index_4 = (shifted_bits_5 & 3);
            pixel_color_6 = fx_ldb((uint16_t)(pixel_index_4 + 0x0200));
            fx_color((uint8_t)pixel_color_6);
            next_pixel_x_6 = (uint16_t)(next_pixel_x_5 + 1);
            pixel_color_7 = fx_ldb((uint16_t)((shifted_bits_6 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_7);
            next_pixel_x_7 = (uint16_t)(next_pixel_x_6 + 1);
            pixel_color_8 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_6 >> 1 | ((uint32_t)pixel_index_4 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_6 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            next_pixel_x_8 = (uint16_t)(next_pixel_x_7 + 1);
            fx_rpix((uint8_t)next_pixel_x_8, (uint8_t)draw_y);
            glyph_byte_3 = fx_getb();
            pixel_index_5 = (glyph_byte_3 & 3);
            pixel_color_9 = fx_ldb((uint16_t)(pixel_index_5 + 0x0200));
            fx_color((uint8_t)pixel_color_9);
            next_pixel_x_9 = (uint16_t)(next_pixel_x_8 + 1);
            shifted_bits_7 = (uint16_t)(glyph_byte_3 >> 1 | ((uint32_t)pixel_index_5 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_8 = (uint16_t)(shifted_bits_7 >> 1 | (glyph_byte_3 & 1) << 15);
            next_src_ptr_3 = (uint16_t)(next_src_ptr_2 + 1);
            fx_r14(next_src_ptr_3);
            shifted_bits_9 = (uint16_t)((uint16_t)(shifted_bits_8 >> 1 | (shifted_bits_7 & 1) << 15) >> 1 | (shifted_bits_8 & 1) << 15);
            pixel_index_6 = (shifted_bits_8 & 3);
            pixel_color_10 = fx_ldb((uint16_t)(pixel_index_6 + 0x0200));
            fx_color((uint8_t)pixel_color_10);
            next_pixel_x_10 = (uint16_t)(next_pixel_x_9 + 1);
            pixel_color_11 = fx_ldb((uint16_t)((shifted_bits_9 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_11);
            next_pixel_x_11 = (uint16_t)(next_pixel_x_10 + 1);
            pixel_color_12 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_9 >> 1 | ((uint32_t)pixel_index_6 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_9 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_12);
            next_pixel_x_12 = (uint16_t)(next_pixel_x_11 + 1);
            fx_rpix((uint8_t)next_pixel_x_12, (uint8_t)draw_y);
            glyph_byte_4 = fx_getb();
            pixel_index_7 = (glyph_byte_4 & 3);
            pixel_color_13 = fx_ldb((uint16_t)(pixel_index_7 + 0x0200));
            fx_color((uint8_t)pixel_color_13);
            next_pixel_x_13 = (uint16_t)(next_pixel_x_12 + 1);
            shifted_bits_10 = (uint16_t)(glyph_byte_4 >> 1 | ((uint32_t)pixel_index_7 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_11 = (uint16_t)(shifted_bits_10 >> 1 | (glyph_byte_4 & 1) << 15);
            next_src_ptr_4 = (uint16_t)(next_src_ptr_3 + 1);
            fx_r14(next_src_ptr_4);
            shifted_bits_12 = (uint16_t)((uint16_t)(shifted_bits_11 >> 1 | (shifted_bits_10 & 1) << 15) >> 1 | (shifted_bits_11 & 1) << 15);
            pixel_index_8 = (shifted_bits_11 & 3);
            pixel_color_14 = fx_ldb((uint16_t)(pixel_index_8 + 0x0200));
            fx_color((uint8_t)pixel_color_14);
            next_pixel_x_14 = (uint16_t)(next_pixel_x_13 + 1);
            pixel_color_15 = fx_ldb((uint16_t)((shifted_bits_12 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_15);
            next_pixel_x_15 = (uint16_t)(next_pixel_x_14 + 1);
            pixel_color_16 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_12 >> 1 | ((uint32_t)pixel_index_8 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_12 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_16);
            fx_rpix((uint8_t)(uint16_t)(next_pixel_x_15 + 1), (uint8_t)draw_y);
            rom_at = (uint16_t)(next_src_ptr_4 + 0x001C);
            fx_r14(rom_at);
            pixel_x_start = glyph_x;
            count = (uint16_t)(count - 1);
            draw_y = (uint16_t)(draw_y + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        return;
    } else {
        return;
    }
}


/*
 * @routine 07:F280 FxGlyph8x8
 * WHAT IT DOES: As FxGlyph8x16, 8 rows.
 * HOW: As FxGlyph8x16. No script has started it yet.
 * Entry: started by the S-CPU. Exit: as FxGlyph8x16, with R2 = y + 8 and R13
 * = $F2AC.
 */
void FxGlyph8x8(void)
{
    uint16_t count;
    uint16_t draw_x;
    uint16_t draw_y;
    uint16_t in_216;
    uint16_t rom_at;
    uint16_t pixel_x;
    uint16_t next_x_0;
    uint16_t next_x_1;
    uint16_t next_x_2;
    uint16_t next_x_3;
    uint16_t next_x_4;
    uint16_t next_x_5;
    uint16_t next_x_6;
    uint16_t packed_byte_0;
    uint16_t packed_byte_1;
    uint16_t pixel_index_0;
    uint16_t pixel_index_1;
    uint16_t pixel_index_4;
    uint16_t pixel_index_5;
    uint16_t next_rom_ptr_0;
    uint16_t next_rom_ptr_1;
    uint16_t palette_color_0;
    uint16_t palette_color_1;
    uint16_t palette_color_2;
    uint16_t palette_color_3;
    uint16_t palette_color_4;
    uint16_t palette_color_5;
    uint16_t palette_color_6;
    uint16_t palette_color_7;
    uint16_t bits_shifted_four_0;
    uint16_t bits_shifted_four_1;
    uint16_t bits_shifted_once_0;
    uint16_t bits_shifted_once_1;
    uint16_t bits_shifted_twice_0;
    uint16_t bits_shifted_twice_1;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    draw_y = fx_ldw(0x020C);
    draw_x = fx_ldw(0x020A);
    pixel_x = draw_x;
    fx_sbk((uint16_t)(draw_x + 8));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    rom_at = in_216;
    fx_r14(rom_at);
    if (in_216 != 0) {
        count = 8;
        for (;;) {
            fx_bitmap_row(pixel_x, draw_y, 8, 0x0200, rom_at);
            packed_byte_0 = fx_getb();
            pixel_index_0 = (packed_byte_0 & 3);
            palette_color_0 = fx_ldb((uint16_t)(pixel_index_0 + 0x0200));
            fx_color((uint8_t)palette_color_0);
            next_x_0 = (uint16_t)(pixel_x + 1);
            bits_shifted_once_0 = (uint16_t)(packed_byte_0 >> 1 | ((uint32_t)pixel_index_0 + 0x0200 > 0xFFFF) << 15);
            bits_shifted_twice_0 = (uint16_t)(bits_shifted_once_0 >> 1 | (packed_byte_0 & 1) << 15);
            next_rom_ptr_0 = (uint16_t)(rom_at + 1);
            fx_r14(next_rom_ptr_0);
            bits_shifted_four_0 = (uint16_t)((uint16_t)(bits_shifted_twice_0 >> 1 | (bits_shifted_once_0 & 1) << 15) >> 1 | (bits_shifted_twice_0 & 1) << 15);
            pixel_index_1 = (bits_shifted_twice_0 & 3);
            palette_color_1 = fx_ldb((uint16_t)(pixel_index_1 + 0x0200));
            fx_color((uint8_t)palette_color_1);
            next_x_1 = (uint16_t)(next_x_0 + 1);
            palette_color_2 = fx_ldb((uint16_t)((bits_shifted_four_0 & 3) + 0x0200));
            fx_color((uint8_t)palette_color_2);
            next_x_2 = (uint16_t)(next_x_1 + 1);
            palette_color_3 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(bits_shifted_four_0 >> 1 | ((uint32_t)pixel_index_1 + 0x0200 > 0xFFFF) << 15) >> 1 | (bits_shifted_four_0 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)palette_color_3);
            next_x_3 = (uint16_t)(next_x_2 + 1);
            fx_rpix((uint8_t)next_x_3, (uint8_t)draw_y);
            packed_byte_1 = fx_getb();
            pixel_index_4 = (packed_byte_1 & 3);
            palette_color_4 = fx_ldb((uint16_t)(pixel_index_4 + 0x0200));
            fx_color((uint8_t)palette_color_4);
            next_x_4 = (uint16_t)(next_x_3 + 1);
            bits_shifted_once_1 = (uint16_t)(packed_byte_1 >> 1 | ((uint32_t)pixel_index_4 + 0x0200 > 0xFFFF) << 15);
            bits_shifted_twice_1 = (uint16_t)(bits_shifted_once_1 >> 1 | (packed_byte_1 & 1) << 15);
            next_rom_ptr_1 = (uint16_t)(next_rom_ptr_0 + 1);
            fx_r14(next_rom_ptr_1);
            bits_shifted_four_1 = (uint16_t)((uint16_t)(bits_shifted_twice_1 >> 1 | (bits_shifted_once_1 & 1) << 15) >> 1 | (bits_shifted_twice_1 & 1) << 15);
            pixel_index_5 = (bits_shifted_twice_1 & 3);
            palette_color_5 = fx_ldb((uint16_t)(pixel_index_5 + 0x0200));
            fx_color((uint8_t)palette_color_5);
            next_x_5 = (uint16_t)(next_x_4 + 1);
            palette_color_6 = fx_ldb((uint16_t)((bits_shifted_four_1 & 3) + 0x0200));
            fx_color((uint8_t)palette_color_6);
            next_x_6 = (uint16_t)(next_x_5 + 1);
            palette_color_7 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(bits_shifted_four_1 >> 1 | ((uint32_t)pixel_index_5 + 0x0200 > 0xFFFF) << 15) >> 1 | (bits_shifted_four_1 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)palette_color_7);
            fx_rpix((uint8_t)(uint16_t)(next_x_6 + 1), (uint8_t)draw_y);
            rom_at = (uint16_t)(next_rom_ptr_1 + 0x001E);
            fx_r14(rom_at);
            pixel_x = draw_x;
            count = (uint16_t)(count - 1);
            draw_y = (uint16_t)(draw_y + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        return;
    } else {
        return;
    }
}


/*
 * @routine 07:F329 FxGlyph8x16
 * WHAT IT DOES: Draws the 8 x 16 glyph at $70:0216 (in bank $15) at
 * ($70:020A, $70:020C) and moves x on by 8; nothing is drawn when the glyph
 * address is 0.
 * HOW: 16 rows of two bitmap bytes through the colour table at $70:0200,
 * with an RPIX after each byte. The address-0 exit has not been reached by
 * any script yet.
 * Entry: started by the S-CPU. Exit: when drawn, R0 = $1E; R1 = R8 = x; R2 =
 * y + 16; R6 = the last byte rotated right 8 times through the carry; R11 =
 * $0200; R12 = 0; R13 = $F355; R14 past the last row; ROM bank $15, RAM bank
 * 0, plot options 0; the last RAM address that of the last colour byte read.
 */
void FxGlyph8x16(void)
{
    uint16_t count;
    uint16_t in_216;
    uint16_t rom_at;
    uint16_t glyph_x;
    uint16_t glyph_y;
    uint16_t glyph_byte_1;
    uint16_t glyph_byte_2;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_color_5;
    uint16_t pixel_color_6;
    uint16_t pixel_color_7;
    uint16_t pixel_color_8;
    uint16_t pixel_index_1;
    uint16_t pixel_index_2;
    uint16_t pixel_index_3;
    uint16_t pixel_index_4;
    uint16_t pixel_x_start;
    uint16_t next_pixel_x_1;
    uint16_t next_pixel_x_2;
    uint16_t next_pixel_x_3;
    uint16_t next_pixel_x_4;
    uint16_t next_pixel_x_5;
    uint16_t next_pixel_x_6;
    uint16_t next_pixel_x_7;
    uint16_t next_src_ptr_1;
    uint16_t next_src_ptr_2;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t shifted_bits_4;
    uint16_t shifted_bits_5;
    uint16_t shifted_bits_6;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    glyph_y = fx_ldw(0x020C);
    glyph_x = fx_ldw(0x020A);
    pixel_x_start = glyph_x;
    fx_sbk((uint16_t)(glyph_x + 8));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    rom_at = in_216;
    fx_r14(rom_at);
    if (in_216 != 0) {
        count = 0x0010;
        for (;;) {
            fx_bitmap_row(pixel_x_start, glyph_y, 8, 0x0200, rom_at);
            glyph_byte_1 = fx_getb();
            pixel_index_1 = (glyph_byte_1 & 3);
            pixel_color_1 = fx_ldb((uint16_t)(pixel_index_1 + 0x0200));
            fx_color((uint8_t)pixel_color_1);
            next_pixel_x_1 = (uint16_t)(pixel_x_start + 1);
            shifted_bits_1 = (uint16_t)(glyph_byte_1 >> 1 | ((uint32_t)pixel_index_1 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (glyph_byte_1 & 1) << 15);
            next_src_ptr_1 = (uint16_t)(rom_at + 1);
            fx_r14(next_src_ptr_1);
            shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
            pixel_index_2 = (shifted_bits_2 & 3);
            pixel_color_2 = fx_ldb((uint16_t)(pixel_index_2 + 0x0200));
            fx_color((uint8_t)pixel_color_2);
            next_pixel_x_2 = (uint16_t)(next_pixel_x_1 + 1);
            pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_3);
            next_pixel_x_3 = (uint16_t)(next_pixel_x_2 + 1);
            pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)pixel_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_4);
            next_pixel_x_4 = (uint16_t)(next_pixel_x_3 + 1);
            fx_rpix((uint8_t)next_pixel_x_4, (uint8_t)glyph_y);
            glyph_byte_2 = fx_getb();
            pixel_index_3 = (glyph_byte_2 & 3);
            pixel_color_5 = fx_ldb((uint16_t)(pixel_index_3 + 0x0200));
            fx_color((uint8_t)pixel_color_5);
            next_pixel_x_5 = (uint16_t)(next_pixel_x_4 + 1);
            shifted_bits_4 = (uint16_t)(glyph_byte_2 >> 1 | ((uint32_t)pixel_index_3 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_5 = (uint16_t)(shifted_bits_4 >> 1 | (glyph_byte_2 & 1) << 15);
            next_src_ptr_2 = (uint16_t)(next_src_ptr_1 + 1);
            fx_r14(next_src_ptr_2);
            shifted_bits_6 = (uint16_t)((uint16_t)(shifted_bits_5 >> 1 | (shifted_bits_4 & 1) << 15) >> 1 | (shifted_bits_5 & 1) << 15);
            pixel_index_4 = (shifted_bits_5 & 3);
            pixel_color_6 = fx_ldb((uint16_t)(pixel_index_4 + 0x0200));
            fx_color((uint8_t)pixel_color_6);
            next_pixel_x_6 = (uint16_t)(next_pixel_x_5 + 1);
            pixel_color_7 = fx_ldb((uint16_t)((shifted_bits_6 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_7);
            next_pixel_x_7 = (uint16_t)(next_pixel_x_6 + 1);
            pixel_color_8 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_6 >> 1 | ((uint32_t)pixel_index_4 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_6 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            fx_rpix((uint8_t)(uint16_t)(next_pixel_x_7 + 1), (uint8_t)glyph_y);
            rom_at = (uint16_t)(next_src_ptr_2 + 0x001E);
            fx_r14(rom_at);
            pixel_x_start = glyph_x;
            count = (uint16_t)(count - 1);
            glyph_y = (uint16_t)(glyph_y + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        return;
    } else {
        return;
    }
}


/*
 * @routine 07:F3D2 FxGlyph24x32Bank04
 * WHAT IT DOES: Draws the 24 x 32 glyph at $70:0216 in ROM bank $04 at
 * ($70:020A, $70:020C) and moves x on by 16.
 * HOW: 32 rows of six bitmap bytes (R3 counts them) through the colour table
 * at $70:0200; one RPIX at the end. The glyph address is not checked. No
 * script has started it yet.
 * Entry: started by the S-CPU. Exit: R0 = the pixel at (x, y + 32); R1 = R8
 * = x; R2 = y + 32; R3 = 0; R11 = $0200; R12 = 0; R13 = $F3F7; R14 past the
 * last row; ROM bank $04, RAM bank 0, plot options 0.
 */
void FxGlyph24x32Bank04(void)
{
    uint16_t count;
    uint16_t in_216;
    uint16_t glyph_x;
    uint16_t glyph_y;
    uint16_t bytes_left;
    uint16_t glyph_byte;
    uint16_t bytes_per_row;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_index_1;
    uint16_t pixel_index_2;
    uint16_t pixel_x_start;
    uint16_t next_pixel_x_1;
    uint16_t next_pixel_x_2;
    uint16_t next_pixel_x_3;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;

    fx_romb(4);
    fx_ramb(0);
    fx_cmode(0);
    glyph_y = fx_ldw(0x020C);
    glyph_x = fx_ldw(0x020A);
    pixel_x_start = glyph_x;
    fx_sbk((uint16_t)(glyph_x + 0x0010));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    count = 0x0020;
    for (;;) {
        bytes_per_row = 6;
        for (;;) {
            glyph_byte = fx_getb();
            pixel_index_1 = (glyph_byte & 3);
            pixel_color_1 = fx_ldb((uint16_t)(pixel_index_1 + 0x0200));
            fx_color((uint8_t)pixel_color_1);
            next_pixel_x_1 = (uint16_t)(pixel_x_start + 1);
            shifted_bits_1 = (uint16_t)(glyph_byte >> 1 | ((uint32_t)pixel_index_1 + 0x0200 > 0xFFFF) << 15);
            shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (glyph_byte & 1) << 15);
            in_216 = (uint16_t)(in_216 + 1);
            fx_r14(in_216);
            shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
            pixel_index_2 = (shifted_bits_2 & 3);
            pixel_color_2 = fx_ldb((uint16_t)(pixel_index_2 + 0x0200));
            fx_color((uint8_t)pixel_color_2);
            next_pixel_x_2 = (uint16_t)(next_pixel_x_1 + 1);
            pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_3);
            next_pixel_x_3 = (uint16_t)(next_pixel_x_2 + 1);
            pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)pixel_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_4);
            pixel_x_start = (uint16_t)(next_pixel_x_3 + 1);
            bytes_left = (uint16_t)(bytes_per_row - 1);
            bytes_per_row = bytes_left;
            if (bytes_left != 0) {
                continue;
            } else {
                break;
            }
        }
        in_216 = (uint16_t)(in_216 + 0x001A);
        fx_r14(in_216);
        pixel_x_start = glyph_x;
        count = (uint16_t)(count - 1);
        glyph_y = (uint16_t)(glyph_y + 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)pixel_x_start, (uint8_t)glyph_y);
    return;
}


/*
 * @routine 07:F441 FxGlyph24x16
 * WHAT IT DOES: Draws the 24 x 16 glyph at $70:0216 (in bank $15) at
 * ($70:020A, $70:020C) and moves x on by 16 (the player marks).
 * HOW: 16 rows of six bitmap bytes through the colour table at $70:0200,
 * with an RPIX after each byte and one more at the end. The glyph address is
 * not checked.
 * Entry: started by the S-CPU. Exit: R0 = the pixel at (x, y + 16); R1 = R8
 * = x; R2 = y + 16; R6 = the last byte rotated right 8 times through the
 * carry; R11 = $0200; R12 = 0; R13 = $F466; R14 past the last row; ROM bank
 * $15, RAM bank 0, plot options 0; the last RAM address that of the last
 * colour byte read.
 */
void FxGlyph24x16(void)
{
    uint16_t count;
    uint16_t in_216;
    uint16_t glyph_x;
    uint16_t glyph_y;
    uint16_t pixel_x_1;
    uint16_t pixel_x_2;
    uint16_t pixel_x_3;
    uint16_t pixel_x_4;
    uint16_t pixel_x_5;
    uint16_t pixel_x_6;
    uint16_t pixel_x_7;
    uint16_t pixel_x_8;
    uint16_t pixel_x_9;
    uint16_t pixel_x_10;
    uint16_t pixel_x_11;
    uint16_t pixel_x_12;
    uint16_t pixel_x_13;
    uint16_t pixel_x_14;
    uint16_t pixel_x_15;
    uint16_t pixel_x_16;
    uint16_t pixel_x_17;
    uint16_t pixel_x_18;
    uint16_t pixel_x_19;
    uint16_t pixel_x_20;
    uint16_t pixel_x_21;
    uint16_t pixel_x_22;
    uint16_t pixel_x_23;
    uint16_t pixel_x_24;
    uint16_t src_byte_1;
    uint16_t src_byte_2;
    uint16_t src_byte_3;
    uint16_t src_byte_4;
    uint16_t src_byte_5;
    uint16_t src_byte_6;
    uint16_t color_index_1;
    uint16_t color_index_2;
    uint16_t color_index_3;
    uint16_t color_index_4;
    uint16_t color_index_5;
    uint16_t color_index_6;
    uint16_t color_index_7;
    uint16_t color_index_8;
    uint16_t color_index_9;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_color_5;
    uint16_t pixel_color_6;
    uint16_t pixel_color_7;
    uint16_t pixel_color_8;
    uint16_t pixel_color_9;
    uint16_t color_index_10;
    uint16_t color_index_11;
    uint16_t color_index_12;
    uint16_t pixel_color_10;
    uint16_t pixel_color_11;
    uint16_t pixel_color_12;
    uint16_t pixel_color_13;
    uint16_t pixel_color_14;
    uint16_t pixel_color_15;
    uint16_t pixel_color_16;
    uint16_t pixel_color_17;
    uint16_t pixel_color_18;
    uint16_t pixel_color_19;
    uint16_t pixel_color_20;
    uint16_t pixel_color_21;
    uint16_t pixel_color_22;
    uint16_t pixel_color_23;
    uint16_t pixel_color_24;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t shifted_bits_4;
    uint16_t shifted_bits_5;
    uint16_t shifted_bits_6;
    uint16_t shifted_bits_7;
    uint16_t shifted_bits_8;
    uint16_t shifted_bits_9;
    uint16_t shifted_bits_10;
    uint16_t shifted_bits_11;
    uint16_t shifted_bits_12;
    uint16_t shifted_bits_13;
    uint16_t shifted_bits_14;
    uint16_t shifted_bits_15;
    uint16_t shifted_bits_16;
    uint16_t shifted_bits_17;
    uint16_t shifted_bits_18;
    uint16_t next_glyph_ptr_1;
    uint16_t next_glyph_ptr_2;
    uint16_t next_glyph_ptr_3;
    uint16_t next_glyph_ptr_4;
    uint16_t next_glyph_ptr_5;
    uint16_t next_glyph_ptr_6;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    glyph_y = fx_ldw(0x020C);
    glyph_x = fx_ldw(0x020A);
    pixel_x_1 = glyph_x;
    fx_sbk((uint16_t)(glyph_x + 0x0010));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    count = 0x0010;
    for (;;) {
        fx_bitmap_row(pixel_x_1, glyph_y, 24, 0x0200, in_216);
        src_byte_1 = fx_getb();
        color_index_1 = (src_byte_1 & 3);
        pixel_color_1 = fx_ldb((uint16_t)(color_index_1 + 0x0200));
        fx_color((uint8_t)pixel_color_1);
        pixel_x_2 = (uint16_t)(pixel_x_1 + 1);
        shifted_bits_1 = (uint16_t)(src_byte_1 >> 1 | ((uint32_t)color_index_1 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (src_byte_1 & 1) << 15);
        next_glyph_ptr_1 = (uint16_t)(in_216 + 1);
        fx_r14(next_glyph_ptr_1);
        shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
        color_index_2 = (shifted_bits_2 & 3);
        pixel_color_2 = fx_ldb((uint16_t)(color_index_2 + 0x0200));
        fx_color((uint8_t)pixel_color_2);
        pixel_x_3 = (uint16_t)(pixel_x_2 + 1);
        pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_3);
        pixel_x_4 = (uint16_t)(pixel_x_3 + 1);
        pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)color_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_4);
        pixel_x_5 = (uint16_t)(pixel_x_4 + 1);
        fx_rpix((uint8_t)pixel_x_5, (uint8_t)glyph_y);
        src_byte_2 = fx_getb();
        color_index_3 = (src_byte_2 & 3);
        pixel_color_5 = fx_ldb((uint16_t)(color_index_3 + 0x0200));
        fx_color((uint8_t)pixel_color_5);
        pixel_x_6 = (uint16_t)(pixel_x_5 + 1);
        shifted_bits_4 = (uint16_t)(src_byte_2 >> 1 | ((uint32_t)color_index_3 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_5 = (uint16_t)(shifted_bits_4 >> 1 | (src_byte_2 & 1) << 15);
        next_glyph_ptr_2 = (uint16_t)(next_glyph_ptr_1 + 1);
        fx_r14(next_glyph_ptr_2);
        shifted_bits_6 = (uint16_t)((uint16_t)(shifted_bits_5 >> 1 | (shifted_bits_4 & 1) << 15) >> 1 | (shifted_bits_5 & 1) << 15);
        color_index_4 = (shifted_bits_5 & 3);
        pixel_color_6 = fx_ldb((uint16_t)(color_index_4 + 0x0200));
        fx_color((uint8_t)pixel_color_6);
        pixel_x_7 = (uint16_t)(pixel_x_6 + 1);
        pixel_color_7 = fx_ldb((uint16_t)((shifted_bits_6 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_7);
        pixel_x_8 = (uint16_t)(pixel_x_7 + 1);
        pixel_color_8 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_6 >> 1 | ((uint32_t)color_index_4 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_6 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_8);
        pixel_x_9 = (uint16_t)(pixel_x_8 + 1);
        fx_rpix((uint8_t)pixel_x_9, (uint8_t)glyph_y);
        src_byte_3 = fx_getb();
        color_index_5 = (src_byte_3 & 3);
        pixel_color_9 = fx_ldb((uint16_t)(color_index_5 + 0x0200));
        fx_color((uint8_t)pixel_color_9);
        pixel_x_10 = (uint16_t)(pixel_x_9 + 1);
        shifted_bits_7 = (uint16_t)(src_byte_3 >> 1 | ((uint32_t)color_index_5 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_8 = (uint16_t)(shifted_bits_7 >> 1 | (src_byte_3 & 1) << 15);
        next_glyph_ptr_3 = (uint16_t)(next_glyph_ptr_2 + 1);
        fx_r14(next_glyph_ptr_3);
        shifted_bits_9 = (uint16_t)((uint16_t)(shifted_bits_8 >> 1 | (shifted_bits_7 & 1) << 15) >> 1 | (shifted_bits_8 & 1) << 15);
        color_index_6 = (shifted_bits_8 & 3);
        pixel_color_10 = fx_ldb((uint16_t)(color_index_6 + 0x0200));
        fx_color((uint8_t)pixel_color_10);
        pixel_x_11 = (uint16_t)(pixel_x_10 + 1);
        pixel_color_11 = fx_ldb((uint16_t)((shifted_bits_9 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_11);
        pixel_x_12 = (uint16_t)(pixel_x_11 + 1);
        pixel_color_12 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_9 >> 1 | ((uint32_t)color_index_6 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_9 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_12);
        pixel_x_13 = (uint16_t)(pixel_x_12 + 1);
        fx_rpix((uint8_t)pixel_x_13, (uint8_t)glyph_y);
        src_byte_4 = fx_getb();
        color_index_7 = (src_byte_4 & 3);
        pixel_color_13 = fx_ldb((uint16_t)(color_index_7 + 0x0200));
        fx_color((uint8_t)pixel_color_13);
        pixel_x_14 = (uint16_t)(pixel_x_13 + 1);
        shifted_bits_10 = (uint16_t)(src_byte_4 >> 1 | ((uint32_t)color_index_7 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_11 = (uint16_t)(shifted_bits_10 >> 1 | (src_byte_4 & 1) << 15);
        next_glyph_ptr_4 = (uint16_t)(next_glyph_ptr_3 + 1);
        fx_r14(next_glyph_ptr_4);
        shifted_bits_12 = (uint16_t)((uint16_t)(shifted_bits_11 >> 1 | (shifted_bits_10 & 1) << 15) >> 1 | (shifted_bits_11 & 1) << 15);
        color_index_8 = (shifted_bits_11 & 3);
        pixel_color_14 = fx_ldb((uint16_t)(color_index_8 + 0x0200));
        fx_color((uint8_t)pixel_color_14);
        pixel_x_15 = (uint16_t)(pixel_x_14 + 1);
        pixel_color_15 = fx_ldb((uint16_t)((shifted_bits_12 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_15);
        pixel_x_16 = (uint16_t)(pixel_x_15 + 1);
        pixel_color_16 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_12 >> 1 | ((uint32_t)color_index_8 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_12 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_16);
        pixel_x_17 = (uint16_t)(pixel_x_16 + 1);
        fx_rpix((uint8_t)pixel_x_17, (uint8_t)glyph_y);
        src_byte_5 = fx_getb();
        color_index_9 = (src_byte_5 & 3);
        pixel_color_17 = fx_ldb((uint16_t)(color_index_9 + 0x0200));
        fx_color((uint8_t)pixel_color_17);
        pixel_x_18 = (uint16_t)(pixel_x_17 + 1);
        shifted_bits_13 = (uint16_t)(src_byte_5 >> 1 | ((uint32_t)color_index_9 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_14 = (uint16_t)(shifted_bits_13 >> 1 | (src_byte_5 & 1) << 15);
        next_glyph_ptr_5 = (uint16_t)(next_glyph_ptr_4 + 1);
        fx_r14(next_glyph_ptr_5);
        shifted_bits_15 = (uint16_t)((uint16_t)(shifted_bits_14 >> 1 | (shifted_bits_13 & 1) << 15) >> 1 | (shifted_bits_14 & 1) << 15);
        color_index_10 = (shifted_bits_14 & 3);
        pixel_color_18 = fx_ldb((uint16_t)(color_index_10 + 0x0200));
        fx_color((uint8_t)pixel_color_18);
        pixel_x_19 = (uint16_t)(pixel_x_18 + 1);
        pixel_color_19 = fx_ldb((uint16_t)((shifted_bits_15 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_19);
        pixel_x_20 = (uint16_t)(pixel_x_19 + 1);
        pixel_color_20 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_15 >> 1 | ((uint32_t)color_index_10 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_15 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_20);
        pixel_x_21 = (uint16_t)(pixel_x_20 + 1);
        fx_rpix((uint8_t)pixel_x_21, (uint8_t)glyph_y);
        src_byte_6 = fx_getb();
        color_index_11 = (src_byte_6 & 3);
        pixel_color_21 = fx_ldb((uint16_t)(color_index_11 + 0x0200));
        fx_color((uint8_t)pixel_color_21);
        pixel_x_22 = (uint16_t)(pixel_x_21 + 1);
        shifted_bits_16 = (uint16_t)(src_byte_6 >> 1 | ((uint32_t)color_index_11 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_17 = (uint16_t)(shifted_bits_16 >> 1 | (src_byte_6 & 1) << 15);
        next_glyph_ptr_6 = (uint16_t)(next_glyph_ptr_5 + 1);
        fx_r14(next_glyph_ptr_6);
        shifted_bits_18 = (uint16_t)((uint16_t)(shifted_bits_17 >> 1 | (shifted_bits_16 & 1) << 15) >> 1 | (shifted_bits_17 & 1) << 15);
        color_index_12 = (shifted_bits_17 & 3);
        pixel_color_22 = fx_ldb((uint16_t)(color_index_12 + 0x0200));
        fx_color((uint8_t)pixel_color_22);
        pixel_x_23 = (uint16_t)(pixel_x_22 + 1);
        pixel_color_23 = fx_ldb((uint16_t)((shifted_bits_18 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_23);
        pixel_x_24 = (uint16_t)(pixel_x_23 + 1);
        pixel_color_24 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_18 >> 1 | ((uint32_t)color_index_12 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_18 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_24);
        fx_rpix((uint8_t)(uint16_t)(pixel_x_24 + 1), (uint8_t)glyph_y);
        in_216 = (uint16_t)(next_glyph_ptr_6 + 0x001A);
        fx_r14(in_216);
        pixel_x_1 = glyph_x;
        count = (uint16_t)(count - 1);
        glyph_y = (uint16_t)(glyph_y + 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)pixel_x_1, (uint8_t)glyph_y);
    return;
}


/*
 * @routine 07:F5C9 FxGlyph16x16
 * WHAT IT DOES: Draws the 16 x 16 glyph at $70:0216 (in bank $15) at
 * ($70:020A, $70:020C) and moves x on by 16.
 * HOW: 16 rows of four bitmap bytes through the colour table at $70:0200,
 * with an RPIX after each byte and one more at the end. The glyph address is
 * not checked. No script has started it yet.
 * Entry: started by the S-CPU. Exit: as FxGlyph24x16, with R13 = $F5EE.
 */
void FxGlyph16x16(void)
{
    uint16_t count;
    uint16_t in_216;
    uint16_t glyph_x;
    uint16_t glyph_y;
    uint16_t pixel_x_1;
    uint16_t pixel_x_2;
    uint16_t pixel_x_3;
    uint16_t pixel_x_4;
    uint16_t pixel_x_5;
    uint16_t pixel_x_6;
    uint16_t pixel_x_7;
    uint16_t pixel_x_8;
    uint16_t pixel_x_9;
    uint16_t pixel_x_10;
    uint16_t pixel_x_11;
    uint16_t pixel_x_12;
    uint16_t pixel_x_13;
    uint16_t pixel_x_14;
    uint16_t pixel_x_15;
    uint16_t pixel_x_16;
    uint16_t src_byte_1;
    uint16_t src_byte_2;
    uint16_t src_byte_3;
    uint16_t src_byte_4;
    uint16_t color_index_1;
    uint16_t color_index_2;
    uint16_t color_index_3;
    uint16_t color_index_4;
    uint16_t color_index_5;
    uint16_t color_index_6;
    uint16_t color_index_7;
    uint16_t color_index_8;
    uint16_t pixel_color_1;
    uint16_t pixel_color_2;
    uint16_t pixel_color_3;
    uint16_t pixel_color_4;
    uint16_t pixel_color_5;
    uint16_t pixel_color_6;
    uint16_t pixel_color_7;
    uint16_t pixel_color_8;
    uint16_t pixel_color_9;
    uint16_t pixel_color_10;
    uint16_t pixel_color_11;
    uint16_t pixel_color_12;
    uint16_t pixel_color_13;
    uint16_t pixel_color_14;
    uint16_t pixel_color_15;
    uint16_t pixel_color_16;
    uint16_t shifted_bits_1;
    uint16_t shifted_bits_2;
    uint16_t shifted_bits_3;
    uint16_t shifted_bits_4;
    uint16_t shifted_bits_5;
    uint16_t shifted_bits_6;
    uint16_t shifted_bits_7;
    uint16_t shifted_bits_8;
    uint16_t shifted_bits_9;
    uint16_t shifted_bits_10;
    uint16_t shifted_bits_11;
    uint16_t shifted_bits_12;
    uint16_t next_glyph_ptr_1;
    uint16_t next_glyph_ptr_2;
    uint16_t next_glyph_ptr_3;
    uint16_t next_glyph_ptr_4;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    glyph_y = fx_ldw(0x020C);
    glyph_x = fx_ldw(0x020A);
    pixel_x_1 = glyph_x;
    fx_sbk((uint16_t)(glyph_x + 0x0010));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    count = 0x0010;
    for (;;) {
        fx_bitmap_row(pixel_x_1, glyph_y, 16, 0x0200, in_216);
        src_byte_1 = fx_getb();
        color_index_1 = (src_byte_1 & 3);
        pixel_color_1 = fx_ldb((uint16_t)(color_index_1 + 0x0200));
        fx_color((uint8_t)pixel_color_1);
        pixel_x_2 = (uint16_t)(pixel_x_1 + 1);
        shifted_bits_1 = (uint16_t)(src_byte_1 >> 1 | ((uint32_t)color_index_1 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_2 = (uint16_t)(shifted_bits_1 >> 1 | (src_byte_1 & 1) << 15);
        next_glyph_ptr_1 = (uint16_t)(in_216 + 1);
        fx_r14(next_glyph_ptr_1);
        shifted_bits_3 = (uint16_t)((uint16_t)(shifted_bits_2 >> 1 | (shifted_bits_1 & 1) << 15) >> 1 | (shifted_bits_2 & 1) << 15);
        color_index_2 = (shifted_bits_2 & 3);
        pixel_color_2 = fx_ldb((uint16_t)(color_index_2 + 0x0200));
        fx_color((uint8_t)pixel_color_2);
        pixel_x_3 = (uint16_t)(pixel_x_2 + 1);
        pixel_color_3 = fx_ldb((uint16_t)((shifted_bits_3 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_3);
        pixel_x_4 = (uint16_t)(pixel_x_3 + 1);
        pixel_color_4 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_3 >> 1 | ((uint32_t)color_index_2 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_3 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_4);
        pixel_x_5 = (uint16_t)(pixel_x_4 + 1);
        fx_rpix((uint8_t)pixel_x_5, (uint8_t)glyph_y);
        src_byte_2 = fx_getb();
        color_index_3 = (src_byte_2 & 3);
        pixel_color_5 = fx_ldb((uint16_t)(color_index_3 + 0x0200));
        fx_color((uint8_t)pixel_color_5);
        pixel_x_6 = (uint16_t)(pixel_x_5 + 1);
        shifted_bits_4 = (uint16_t)(src_byte_2 >> 1 | ((uint32_t)color_index_3 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_5 = (uint16_t)(shifted_bits_4 >> 1 | (src_byte_2 & 1) << 15);
        next_glyph_ptr_2 = (uint16_t)(next_glyph_ptr_1 + 1);
        fx_r14(next_glyph_ptr_2);
        shifted_bits_6 = (uint16_t)((uint16_t)(shifted_bits_5 >> 1 | (shifted_bits_4 & 1) << 15) >> 1 | (shifted_bits_5 & 1) << 15);
        color_index_4 = (shifted_bits_5 & 3);
        pixel_color_6 = fx_ldb((uint16_t)(color_index_4 + 0x0200));
        fx_color((uint8_t)pixel_color_6);
        pixel_x_7 = (uint16_t)(pixel_x_6 + 1);
        pixel_color_7 = fx_ldb((uint16_t)((shifted_bits_6 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_7);
        pixel_x_8 = (uint16_t)(pixel_x_7 + 1);
        pixel_color_8 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_6 >> 1 | ((uint32_t)color_index_4 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_6 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_8);
        pixel_x_9 = (uint16_t)(pixel_x_8 + 1);
        fx_rpix((uint8_t)pixel_x_9, (uint8_t)glyph_y);
        src_byte_3 = fx_getb();
        color_index_5 = (src_byte_3 & 3);
        pixel_color_9 = fx_ldb((uint16_t)(color_index_5 + 0x0200));
        fx_color((uint8_t)pixel_color_9);
        pixel_x_10 = (uint16_t)(pixel_x_9 + 1);
        shifted_bits_7 = (uint16_t)(src_byte_3 >> 1 | ((uint32_t)color_index_5 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_8 = (uint16_t)(shifted_bits_7 >> 1 | (src_byte_3 & 1) << 15);
        next_glyph_ptr_3 = (uint16_t)(next_glyph_ptr_2 + 1);
        fx_r14(next_glyph_ptr_3);
        shifted_bits_9 = (uint16_t)((uint16_t)(shifted_bits_8 >> 1 | (shifted_bits_7 & 1) << 15) >> 1 | (shifted_bits_8 & 1) << 15);
        color_index_6 = (shifted_bits_8 & 3);
        pixel_color_10 = fx_ldb((uint16_t)(color_index_6 + 0x0200));
        fx_color((uint8_t)pixel_color_10);
        pixel_x_11 = (uint16_t)(pixel_x_10 + 1);
        pixel_color_11 = fx_ldb((uint16_t)((shifted_bits_9 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_11);
        pixel_x_12 = (uint16_t)(pixel_x_11 + 1);
        pixel_color_12 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_9 >> 1 | ((uint32_t)color_index_6 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_9 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_12);
        pixel_x_13 = (uint16_t)(pixel_x_12 + 1);
        fx_rpix((uint8_t)pixel_x_13, (uint8_t)glyph_y);
        src_byte_4 = fx_getb();
        color_index_7 = (src_byte_4 & 3);
        pixel_color_13 = fx_ldb((uint16_t)(color_index_7 + 0x0200));
        fx_color((uint8_t)pixel_color_13);
        pixel_x_14 = (uint16_t)(pixel_x_13 + 1);
        shifted_bits_10 = (uint16_t)(src_byte_4 >> 1 | ((uint32_t)color_index_7 + 0x0200 > 0xFFFF) << 15);
        shifted_bits_11 = (uint16_t)(shifted_bits_10 >> 1 | (src_byte_4 & 1) << 15);
        next_glyph_ptr_4 = (uint16_t)(next_glyph_ptr_3 + 1);
        fx_r14(next_glyph_ptr_4);
        shifted_bits_12 = (uint16_t)((uint16_t)(shifted_bits_11 >> 1 | (shifted_bits_10 & 1) << 15) >> 1 | (shifted_bits_11 & 1) << 15);
        color_index_8 = (shifted_bits_11 & 3);
        pixel_color_14 = fx_ldb((uint16_t)(color_index_8 + 0x0200));
        fx_color((uint8_t)pixel_color_14);
        pixel_x_15 = (uint16_t)(pixel_x_14 + 1);
        pixel_color_15 = fx_ldb((uint16_t)((shifted_bits_12 & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_15);
        pixel_x_16 = (uint16_t)(pixel_x_15 + 1);
        pixel_color_16 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_bits_12 >> 1 | ((uint32_t)color_index_8 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_bits_12 & 1) << 15) & 3) + 0x0200));
        fx_color((uint8_t)pixel_color_16);
        fx_rpix((uint8_t)(uint16_t)(pixel_x_16 + 1), (uint8_t)glyph_y);
        in_216 = (uint16_t)(next_glyph_ptr_4 + 0x001C);
        fx_r14(in_216);
        pixel_x_1 = glyph_x;
        count = (uint16_t)(count - 1);
        glyph_y = (uint16_t)(glyph_y + 1);
        if (count != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_rpix((uint8_t)pixel_x_1, (uint8_t)glyph_y);
    return;
}


/*
 * @routine 07:F6DF FxGlyph16x32
 * WHAT IT DOES: Draws the 16 x 32 glyph at $70:0216 (in bank $15) at
 * ($70:020A, $70:020C) and moves x on by 16; nothing is drawn when the glyph
 * address is 0.
 * HOW: 32 rows of four bitmap bytes through the colour table at $70:0200;
 * one RPIX at the end. The address-0 exit has not been reached by any script
 * yet.
 * Entry: started by the S-CPU. Exit: when drawn, R0 = the pixel at (x, y +
 * 32); R1 = R8 = x; R2 = y + 32; R6 = the last byte rotated right 8 times
 * through the carry; R11 = $0200; R12 = 0; R13 = $F70B; R14 past the last
 * row; ROM bank $15, RAM bank 0, plot options 0; the last RAM address that
 * of the last colour byte read.
 */
void FxGlyph16x32(void)
{
    uint16_t count;
    uint16_t row_y;
    uint16_t in_216;
    uint16_t left_x;
    uint16_t rom_at;
    uint16_t pixel_x_2;
    uint16_t pixel_x_9;
    uint16_t pixel_x_16;
    uint16_t pixel_x_18;
    uint16_t pixel_x_20;
    uint16_t pixel_x_24;
    uint16_t pixel_x_31;
    uint16_t pixel_x_33;
    uint16_t pixel_x_35;
    uint16_t pixel_x_39;
    uint16_t pixel_x_46;
    uint16_t pixel_x_48;
    uint16_t pixel_x_50;
    uint16_t pixel_x_54;
    uint16_t pixel_x_61;
    uint16_t rom_ptr_12;
    uint16_t rom_ptr_27;
    uint16_t rom_ptr_42;
    uint16_t rom_ptr_57;
    uint16_t pixel_color_8;
    uint16_t pixel_index_7;
    uint16_t pixel_color_15;
    uint16_t pixel_color_17;
    uint16_t pixel_color_19;
    uint16_t pixel_color_23;
    uint16_t pixel_color_30;
    uint16_t pixel_color_32;
    uint16_t pixel_color_34;
    uint16_t pixel_color_38;
    uint16_t pixel_color_45;
    uint16_t pixel_color_47;
    uint16_t pixel_color_49;
    uint16_t pixel_color_53;
    uint16_t pixel_color_60;
    uint16_t pixel_color_62;
    uint16_t pixel_color_63;
    uint16_t pixel_index_14;
    uint16_t pixel_index_22;
    uint16_t pixel_index_29;
    uint16_t pixel_index_37;
    uint16_t pixel_index_44;
    uint16_t pixel_index_52;
    uint16_t pixel_index_59;
    uint16_t packed_pixels_6;
    uint16_t packed_pixels_21;
    uint16_t packed_pixels_36;
    uint16_t packed_pixels_51;
    uint16_t shifted_pixels_10;
    uint16_t shifted_pixels_11;
    uint16_t shifted_pixels_13;
    uint16_t shifted_pixels_25;
    uint16_t shifted_pixels_26;
    uint16_t shifted_pixels_28;
    uint16_t shifted_pixels_40;
    uint16_t shifted_pixels_41;
    uint16_t shifted_pixels_43;
    uint16_t shifted_pixels_55;
    uint16_t shifted_pixels_56;
    uint16_t shifted_pixels_58;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    row_y = fx_ldw(0x020C);
    left_x = fx_ldw(0x020A);
    pixel_x_2 = left_x;
    fx_sbk((uint16_t)(left_x + 0x0010));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    rom_at = in_216;
    fx_r14(rom_at);
    if (in_216 != 0) {
        count = 0x0020;
        for (;;) {
            fx_bitmap_row(pixel_x_2, row_y, 16, 0x0200, rom_at);
            packed_pixels_6 = fx_getb();
            pixel_index_7 = (packed_pixels_6 & 3);
            pixel_color_8 = fx_ldb((uint16_t)(pixel_index_7 + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            pixel_x_9 = (uint16_t)(pixel_x_2 + 1);
            shifted_pixels_10 = (uint16_t)(packed_pixels_6 >> 1 | ((uint32_t)pixel_index_7 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_11 = (uint16_t)(shifted_pixels_10 >> 1 | (packed_pixels_6 & 1) << 15);
            rom_ptr_12 = (uint16_t)(rom_at + 1);
            fx_r14(rom_ptr_12);
            shifted_pixels_13 = (uint16_t)((uint16_t)(shifted_pixels_11 >> 1 | (shifted_pixels_10 & 1) << 15) >> 1 | (shifted_pixels_11 & 1) << 15);
            pixel_index_14 = (shifted_pixels_11 & 3);
            pixel_color_15 = fx_ldb((uint16_t)(pixel_index_14 + 0x0200));
            fx_color((uint8_t)pixel_color_15);
            pixel_x_16 = (uint16_t)(pixel_x_9 + 1);
            pixel_color_17 = fx_ldb((uint16_t)((shifted_pixels_13 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_17);
            pixel_x_18 = (uint16_t)(pixel_x_16 + 1);
            pixel_color_19 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_13 >> 1 | ((uint32_t)pixel_index_14 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_13 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_19);
            pixel_x_20 = (uint16_t)(pixel_x_18 + 1);
            packed_pixels_21 = fx_getb();
            pixel_index_22 = (packed_pixels_21 & 3);
            pixel_color_23 = fx_ldb((uint16_t)(pixel_index_22 + 0x0200));
            fx_color((uint8_t)pixel_color_23);
            pixel_x_24 = (uint16_t)(pixel_x_20 + 1);
            shifted_pixels_25 = (uint16_t)(packed_pixels_21 >> 1 | ((uint32_t)pixel_index_22 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_26 = (uint16_t)(shifted_pixels_25 >> 1 | (packed_pixels_21 & 1) << 15);
            rom_ptr_27 = (uint16_t)(rom_ptr_12 + 1);
            fx_r14(rom_ptr_27);
            shifted_pixels_28 = (uint16_t)((uint16_t)(shifted_pixels_26 >> 1 | (shifted_pixels_25 & 1) << 15) >> 1 | (shifted_pixels_26 & 1) << 15);
            pixel_index_29 = (shifted_pixels_26 & 3);
            pixel_color_30 = fx_ldb((uint16_t)(pixel_index_29 + 0x0200));
            fx_color((uint8_t)pixel_color_30);
            pixel_x_31 = (uint16_t)(pixel_x_24 + 1);
            pixel_color_32 = fx_ldb((uint16_t)((shifted_pixels_28 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_32);
            pixel_x_33 = (uint16_t)(pixel_x_31 + 1);
            pixel_color_34 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_28 >> 1 | ((uint32_t)pixel_index_29 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_28 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_34);
            pixel_x_35 = (uint16_t)(pixel_x_33 + 1);
            packed_pixels_36 = fx_getb();
            pixel_index_37 = (packed_pixels_36 & 3);
            pixel_color_38 = fx_ldb((uint16_t)(pixel_index_37 + 0x0200));
            fx_color((uint8_t)pixel_color_38);
            pixel_x_39 = (uint16_t)(pixel_x_35 + 1);
            shifted_pixels_40 = (uint16_t)(packed_pixels_36 >> 1 | ((uint32_t)pixel_index_37 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_41 = (uint16_t)(shifted_pixels_40 >> 1 | (packed_pixels_36 & 1) << 15);
            rom_ptr_42 = (uint16_t)(rom_ptr_27 + 1);
            fx_r14(rom_ptr_42);
            shifted_pixels_43 = (uint16_t)((uint16_t)(shifted_pixels_41 >> 1 | (shifted_pixels_40 & 1) << 15) >> 1 | (shifted_pixels_41 & 1) << 15);
            pixel_index_44 = (shifted_pixels_41 & 3);
            pixel_color_45 = fx_ldb((uint16_t)(pixel_index_44 + 0x0200));
            fx_color((uint8_t)pixel_color_45);
            pixel_x_46 = (uint16_t)(pixel_x_39 + 1);
            pixel_color_47 = fx_ldb((uint16_t)((shifted_pixels_43 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_47);
            pixel_x_48 = (uint16_t)(pixel_x_46 + 1);
            pixel_color_49 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_43 >> 1 | ((uint32_t)pixel_index_44 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_43 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_49);
            pixel_x_50 = (uint16_t)(pixel_x_48 + 1);
            packed_pixels_51 = fx_getb();
            pixel_index_52 = (packed_pixels_51 & 3);
            pixel_color_53 = fx_ldb((uint16_t)(pixel_index_52 + 0x0200));
            fx_color((uint8_t)pixel_color_53);
            pixel_x_54 = (uint16_t)(pixel_x_50 + 1);
            shifted_pixels_55 = (uint16_t)(packed_pixels_51 >> 1 | ((uint32_t)pixel_index_52 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_56 = (uint16_t)(shifted_pixels_55 >> 1 | (packed_pixels_51 & 1) << 15);
            rom_ptr_57 = (uint16_t)(rom_ptr_42 + 1);
            fx_r14(rom_ptr_57);
            shifted_pixels_58 = (uint16_t)((uint16_t)(shifted_pixels_56 >> 1 | (shifted_pixels_55 & 1) << 15) >> 1 | (shifted_pixels_56 & 1) << 15);
            pixel_index_59 = (shifted_pixels_56 & 3);
            pixel_color_60 = fx_ldb((uint16_t)(pixel_index_59 + 0x0200));
            fx_color((uint8_t)pixel_color_60);
            pixel_x_61 = (uint16_t)(pixel_x_54 + 1);
            pixel_color_62 = fx_ldb((uint16_t)((shifted_pixels_58 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_62);
            pixel_color_63 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_58 >> 1 | ((uint32_t)pixel_index_59 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_58 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_63);
            rom_at = (uint16_t)(rom_ptr_57 + 0x001C);
            fx_r14(rom_at);
            pixel_x_2 = left_x;
            count = (uint16_t)(count - 1);
            row_y = (uint16_t)(row_y + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        fx_rpix((uint8_t)pixel_x_2, (uint8_t)row_y);
        return;
    } else {
        return;
    }
}


/*
 * @routine 07:F7F4 FxGlyph16x32Double
 * WHAT IT DOES: Draws the 16 x 32 glyph at $70:0216 (in bank $15) at double
 * size (32 x 64) at ($70:020A, $70:020C) and moves x on by 32; nothing is
 * drawn when the glyph address is 0.
 * HOW: Each of the 32 rows twice (R14 back 4 bytes between), each byte's
 * pixels plotted twice, with an RPIX after each byte. No script has started
 * it yet.
 * Entry: started by the S-CPU. Exit: when drawn, R0 = $1C; R1 = R8 = x; R2 =
 * y + 64; R11 = $0200; R12 = 0; R13 = $F820; R14 past the last row; ROM bank
 * $15, RAM bank 0, plot options 0.
 */
void FxGlyph16x32Double(void)
{
    uint16_t count;
    uint16_t row_y;
    uint16_t in_216;
    uint16_t left_x;
    uint16_t rom_at;
    uint16_t pixel_x_2;
    uint16_t pixel_x_9;
    uint16_t pixel_x_10;
    uint16_t pixel_x_17;
    uint16_t pixel_x_18;
    uint16_t pixel_x_20;
    uint16_t pixel_x_21;
    uint16_t pixel_x_23;
    uint16_t pixel_x_24;
    uint16_t pixel_x_28;
    uint16_t pixel_x_29;
    uint16_t pixel_x_36;
    uint16_t pixel_x_37;
    uint16_t pixel_x_39;
    uint16_t pixel_x_40;
    uint16_t pixel_x_42;
    uint16_t pixel_x_43;
    uint16_t pixel_x_47;
    uint16_t pixel_x_48;
    uint16_t pixel_x_55;
    uint16_t pixel_x_56;
    uint16_t pixel_x_58;
    uint16_t pixel_x_59;
    uint16_t pixel_x_61;
    uint16_t pixel_x_62;
    uint16_t pixel_x_66;
    uint16_t pixel_x_67;
    uint16_t pixel_x_74;
    uint16_t pixel_x_75;
    uint16_t pixel_x_77;
    uint16_t pixel_x_78;
    uint16_t pixel_x_80;
    uint16_t pixel_x_86;
    uint16_t pixel_x_87;
    uint16_t pixel_x_94;
    uint16_t pixel_x_95;
    uint16_t pixel_x_97;
    uint16_t pixel_x_98;
    uint16_t rom_ptr_13;
    uint16_t rom_ptr_32;
    uint16_t rom_ptr_51;
    uint16_t rom_ptr_70;
    uint16_t rom_ptr_90;
    uint16_t pixel_x_100;
    uint16_t pixel_x_101;
    uint16_t pixel_x_105;
    uint16_t pixel_x_106;
    uint16_t pixel_x_113;
    uint16_t pixel_x_114;
    uint16_t pixel_x_116;
    uint16_t pixel_x_117;
    uint16_t pixel_x_119;
    uint16_t pixel_x_120;
    uint16_t pixel_x_124;
    uint16_t pixel_x_125;
    uint16_t pixel_x_132;
    uint16_t pixel_x_133;
    uint16_t pixel_x_135;
    uint16_t pixel_x_136;
    uint16_t pixel_x_138;
    uint16_t pixel_x_139;
    uint16_t pixel_x_143;
    uint16_t pixel_x_144;
    uint16_t pixel_x_151;
    uint16_t pixel_x_152;
    uint16_t pixel_x_154;
    uint16_t pixel_x_155;
    uint16_t pixel_x_157;
    uint16_t rom_ptr_109;
    uint16_t rom_ptr_128;
    uint16_t rom_ptr_147;
    uint16_t second_row_y;
    uint16_t pixel_color_8;
    uint16_t pixel_index_7;
    uint16_t pixel_color_16;
    uint16_t pixel_color_19;
    uint16_t pixel_color_22;
    uint16_t pixel_color_27;
    uint16_t pixel_color_35;
    uint16_t pixel_color_38;
    uint16_t pixel_color_41;
    uint16_t pixel_color_46;
    uint16_t pixel_color_54;
    uint16_t pixel_color_57;
    uint16_t pixel_color_60;
    uint16_t pixel_color_65;
    uint16_t pixel_color_73;
    uint16_t pixel_color_76;
    uint16_t pixel_color_79;
    uint16_t pixel_color_85;
    uint16_t pixel_color_93;
    uint16_t pixel_color_96;
    uint16_t pixel_color_99;
    uint16_t pixel_index_15;
    uint16_t pixel_index_26;
    uint16_t pixel_index_34;
    uint16_t pixel_index_45;
    uint16_t pixel_index_53;
    uint16_t pixel_index_64;
    uint16_t pixel_index_72;
    uint16_t pixel_index_84;
    uint16_t pixel_index_92;
    uint16_t packed_pixels_6;
    uint16_t pixel_color_104;
    uint16_t pixel_color_112;
    uint16_t pixel_color_115;
    uint16_t pixel_color_118;
    uint16_t pixel_color_123;
    uint16_t pixel_color_131;
    uint16_t pixel_color_134;
    uint16_t pixel_color_137;
    uint16_t pixel_color_142;
    uint16_t pixel_color_150;
    uint16_t pixel_color_153;
    uint16_t pixel_color_156;
    uint16_t pixel_index_103;
    uint16_t pixel_index_111;
    uint16_t pixel_index_122;
    uint16_t pixel_index_130;
    uint16_t pixel_index_141;
    uint16_t pixel_index_149;
    uint16_t rewound_rom_ptr;
    uint16_t packed_pixels_25;
    uint16_t packed_pixels_44;
    uint16_t packed_pixels_63;
    uint16_t packed_pixels_83;
    uint16_t packed_pixels_102;
    uint16_t packed_pixels_121;
    uint16_t packed_pixels_140;
    uint16_t shifted_pixels_11;
    uint16_t shifted_pixels_12;
    uint16_t shifted_pixels_14;
    uint16_t shifted_pixels_30;
    uint16_t shifted_pixels_31;
    uint16_t shifted_pixels_33;
    uint16_t shifted_pixels_49;
    uint16_t shifted_pixels_50;
    uint16_t shifted_pixels_52;
    uint16_t shifted_pixels_68;
    uint16_t shifted_pixels_69;
    uint16_t shifted_pixels_71;
    uint16_t shifted_pixels_88;
    uint16_t shifted_pixels_89;
    uint16_t shifted_pixels_91;
    uint16_t shifted_pixels_107;
    uint16_t shifted_pixels_108;
    uint16_t shifted_pixels_110;
    uint16_t shifted_pixels_126;
    uint16_t shifted_pixels_127;
    uint16_t shifted_pixels_129;
    uint16_t shifted_pixels_145;
    uint16_t shifted_pixels_146;
    uint16_t shifted_pixels_148;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    row_y = fx_ldw(0x020C);
    left_x = fx_ldw(0x020A);
    pixel_x_2 = left_x;
    fx_sbk((uint16_t)(left_x + 0x0020));
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    rom_at = in_216;
    fx_r14(rom_at);
    if (in_216 != 0) {
        count = 0x0020;
        for (;;) {
            packed_pixels_6 = fx_getb();
            pixel_index_7 = (packed_pixels_6 & 3);
            pixel_color_8 = fx_ldb((uint16_t)(pixel_index_7 + 0x0200));
            fx_color((uint8_t)pixel_color_8);
            pixel_x_9 = (uint16_t)(pixel_x_2 + 1);
            pixel_x_10 = (uint16_t)(pixel_x_9 + 1);
            shifted_pixels_11 = (uint16_t)(packed_pixels_6 >> 1 | ((uint32_t)pixel_index_7 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_12 = (uint16_t)(shifted_pixels_11 >> 1 | (packed_pixels_6 & 1) << 15);
            rom_ptr_13 = (uint16_t)(rom_at + 1);
            fx_r14(rom_ptr_13);
            shifted_pixels_14 = (uint16_t)((uint16_t)(shifted_pixels_12 >> 1 | (shifted_pixels_11 & 1) << 15) >> 1 | (shifted_pixels_12 & 1) << 15);
            pixel_index_15 = (shifted_pixels_12 & 3);
            pixel_color_16 = fx_ldb((uint16_t)(pixel_index_15 + 0x0200));
            fx_color((uint8_t)pixel_color_16);
            pixel_x_17 = (uint16_t)(pixel_x_10 + 1);
            pixel_x_18 = (uint16_t)(pixel_x_17 + 1);
            pixel_color_19 = fx_ldb((uint16_t)((shifted_pixels_14 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_19);
            pixel_x_20 = (uint16_t)(pixel_x_18 + 1);
            pixel_x_21 = (uint16_t)(pixel_x_20 + 1);
            pixel_color_22 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_14 >> 1 | ((uint32_t)pixel_index_15 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_14 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_22);
            pixel_x_23 = (uint16_t)(pixel_x_21 + 1);
            pixel_x_24 = (uint16_t)(pixel_x_23 + 1);
            fx_rpix((uint8_t)pixel_x_24, (uint8_t)row_y);
            packed_pixels_25 = fx_getb();
            pixel_index_26 = (packed_pixels_25 & 3);
            pixel_color_27 = fx_ldb((uint16_t)(pixel_index_26 + 0x0200));
            fx_color((uint8_t)pixel_color_27);
            pixel_x_28 = (uint16_t)(pixel_x_24 + 1);
            pixel_x_29 = (uint16_t)(pixel_x_28 + 1);
            shifted_pixels_30 = (uint16_t)(packed_pixels_25 >> 1 | ((uint32_t)pixel_index_26 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_31 = (uint16_t)(shifted_pixels_30 >> 1 | (packed_pixels_25 & 1) << 15);
            rom_ptr_32 = (uint16_t)(rom_ptr_13 + 1);
            fx_r14(rom_ptr_32);
            shifted_pixels_33 = (uint16_t)((uint16_t)(shifted_pixels_31 >> 1 | (shifted_pixels_30 & 1) << 15) >> 1 | (shifted_pixels_31 & 1) << 15);
            pixel_index_34 = (shifted_pixels_31 & 3);
            pixel_color_35 = fx_ldb((uint16_t)(pixel_index_34 + 0x0200));
            fx_color((uint8_t)pixel_color_35);
            pixel_x_36 = (uint16_t)(pixel_x_29 + 1);
            pixel_x_37 = (uint16_t)(pixel_x_36 + 1);
            pixel_color_38 = fx_ldb((uint16_t)((shifted_pixels_33 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_38);
            pixel_x_39 = (uint16_t)(pixel_x_37 + 1);
            pixel_x_40 = (uint16_t)(pixel_x_39 + 1);
            pixel_color_41 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_33 >> 1 | ((uint32_t)pixel_index_34 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_33 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_41);
            pixel_x_42 = (uint16_t)(pixel_x_40 + 1);
            pixel_x_43 = (uint16_t)(pixel_x_42 + 1);
            fx_rpix((uint8_t)pixel_x_43, (uint8_t)row_y);
            packed_pixels_44 = fx_getb();
            pixel_index_45 = (packed_pixels_44 & 3);
            pixel_color_46 = fx_ldb((uint16_t)(pixel_index_45 + 0x0200));
            fx_color((uint8_t)pixel_color_46);
            pixel_x_47 = (uint16_t)(pixel_x_43 + 1);
            pixel_x_48 = (uint16_t)(pixel_x_47 + 1);
            shifted_pixels_49 = (uint16_t)(packed_pixels_44 >> 1 | ((uint32_t)pixel_index_45 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_50 = (uint16_t)(shifted_pixels_49 >> 1 | (packed_pixels_44 & 1) << 15);
            rom_ptr_51 = (uint16_t)(rom_ptr_32 + 1);
            fx_r14(rom_ptr_51);
            shifted_pixels_52 = (uint16_t)((uint16_t)(shifted_pixels_50 >> 1 | (shifted_pixels_49 & 1) << 15) >> 1 | (shifted_pixels_50 & 1) << 15);
            pixel_index_53 = (shifted_pixels_50 & 3);
            pixel_color_54 = fx_ldb((uint16_t)(pixel_index_53 + 0x0200));
            fx_color((uint8_t)pixel_color_54);
            pixel_x_55 = (uint16_t)(pixel_x_48 + 1);
            pixel_x_56 = (uint16_t)(pixel_x_55 + 1);
            pixel_color_57 = fx_ldb((uint16_t)((shifted_pixels_52 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_57);
            pixel_x_58 = (uint16_t)(pixel_x_56 + 1);
            pixel_x_59 = (uint16_t)(pixel_x_58 + 1);
            pixel_color_60 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_52 >> 1 | ((uint32_t)pixel_index_53 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_52 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_60);
            pixel_x_61 = (uint16_t)(pixel_x_59 + 1);
            pixel_x_62 = (uint16_t)(pixel_x_61 + 1);
            fx_rpix((uint8_t)pixel_x_62, (uint8_t)row_y);
            packed_pixels_63 = fx_getb();
            pixel_index_64 = (packed_pixels_63 & 3);
            pixel_color_65 = fx_ldb((uint16_t)(pixel_index_64 + 0x0200));
            fx_color((uint8_t)pixel_color_65);
            pixel_x_66 = (uint16_t)(pixel_x_62 + 1);
            pixel_x_67 = (uint16_t)(pixel_x_66 + 1);
            shifted_pixels_68 = (uint16_t)(packed_pixels_63 >> 1 | ((uint32_t)pixel_index_64 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_69 = (uint16_t)(shifted_pixels_68 >> 1 | (packed_pixels_63 & 1) << 15);
            rom_ptr_70 = (uint16_t)(rom_ptr_51 + 1);
            fx_r14(rom_ptr_70);
            shifted_pixels_71 = (uint16_t)((uint16_t)(shifted_pixels_69 >> 1 | (shifted_pixels_68 & 1) << 15) >> 1 | (shifted_pixels_69 & 1) << 15);
            pixel_index_72 = (shifted_pixels_69 & 3);
            pixel_color_73 = fx_ldb((uint16_t)(pixel_index_72 + 0x0200));
            fx_color((uint8_t)pixel_color_73);
            pixel_x_74 = (uint16_t)(pixel_x_67 + 1);
            pixel_x_75 = (uint16_t)(pixel_x_74 + 1);
            pixel_color_76 = fx_ldb((uint16_t)((shifted_pixels_71 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_76);
            pixel_x_77 = (uint16_t)(pixel_x_75 + 1);
            pixel_x_78 = (uint16_t)(pixel_x_77 + 1);
            pixel_color_79 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_71 >> 1 | ((uint32_t)pixel_index_72 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_71 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_79);
            pixel_x_80 = (uint16_t)(pixel_x_78 + 1);
            fx_rpix((uint8_t)(uint16_t)(pixel_x_80 + 1), (uint8_t)row_y);
            rewound_rom_ptr = (uint16_t)(rom_ptr_70 - 4);
            fx_r14(rewound_rom_ptr);
            second_row_y = (uint16_t)(row_y + 1);
            packed_pixels_83 = fx_getb();
            pixel_index_84 = (packed_pixels_83 & 3);
            pixel_color_85 = fx_ldb((uint16_t)(pixel_index_84 + 0x0200));
            fx_color((uint8_t)pixel_color_85);
            pixel_x_86 = (uint16_t)(left_x + 1);
            pixel_x_87 = (uint16_t)(pixel_x_86 + 1);
            shifted_pixels_88 = (uint16_t)(packed_pixels_83 >> 1 | ((uint32_t)pixel_index_84 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_89 = (uint16_t)(shifted_pixels_88 >> 1 | (packed_pixels_83 & 1) << 15);
            rom_ptr_90 = (uint16_t)(rewound_rom_ptr + 1);
            fx_r14(rom_ptr_90);
            shifted_pixels_91 = (uint16_t)((uint16_t)(shifted_pixels_89 >> 1 | (shifted_pixels_88 & 1) << 15) >> 1 | (shifted_pixels_89 & 1) << 15);
            pixel_index_92 = (shifted_pixels_89 & 3);
            pixel_color_93 = fx_ldb((uint16_t)(pixel_index_92 + 0x0200));
            fx_color((uint8_t)pixel_color_93);
            pixel_x_94 = (uint16_t)(pixel_x_87 + 1);
            pixel_x_95 = (uint16_t)(pixel_x_94 + 1);
            pixel_color_96 = fx_ldb((uint16_t)((shifted_pixels_91 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_96);
            pixel_x_97 = (uint16_t)(pixel_x_95 + 1);
            pixel_x_98 = (uint16_t)(pixel_x_97 + 1);
            pixel_color_99 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_91 >> 1 | ((uint32_t)pixel_index_92 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_91 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_99);
            pixel_x_100 = (uint16_t)(pixel_x_98 + 1);
            pixel_x_101 = (uint16_t)(pixel_x_100 + 1);
            fx_rpix((uint8_t)pixel_x_101, (uint8_t)second_row_y);
            packed_pixels_102 = fx_getb();
            pixel_index_103 = (packed_pixels_102 & 3);
            pixel_color_104 = fx_ldb((uint16_t)(pixel_index_103 + 0x0200));
            fx_color((uint8_t)pixel_color_104);
            pixel_x_105 = (uint16_t)(pixel_x_101 + 1);
            pixel_x_106 = (uint16_t)(pixel_x_105 + 1);
            shifted_pixels_107 = (uint16_t)(packed_pixels_102 >> 1 | ((uint32_t)pixel_index_103 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_108 = (uint16_t)(shifted_pixels_107 >> 1 | (packed_pixels_102 & 1) << 15);
            rom_ptr_109 = (uint16_t)(rom_ptr_90 + 1);
            fx_r14(rom_ptr_109);
            shifted_pixels_110 = (uint16_t)((uint16_t)(shifted_pixels_108 >> 1 | (shifted_pixels_107 & 1) << 15) >> 1 | (shifted_pixels_108 & 1) << 15);
            pixel_index_111 = (shifted_pixels_108 & 3);
            pixel_color_112 = fx_ldb((uint16_t)(pixel_index_111 + 0x0200));
            fx_color((uint8_t)pixel_color_112);
            pixel_x_113 = (uint16_t)(pixel_x_106 + 1);
            pixel_x_114 = (uint16_t)(pixel_x_113 + 1);
            pixel_color_115 = fx_ldb((uint16_t)((shifted_pixels_110 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_115);
            pixel_x_116 = (uint16_t)(pixel_x_114 + 1);
            pixel_x_117 = (uint16_t)(pixel_x_116 + 1);
            pixel_color_118 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_110 >> 1 | ((uint32_t)pixel_index_111 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_110 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_118);
            pixel_x_119 = (uint16_t)(pixel_x_117 + 1);
            pixel_x_120 = (uint16_t)(pixel_x_119 + 1);
            fx_rpix((uint8_t)pixel_x_120, (uint8_t)second_row_y);
            packed_pixels_121 = fx_getb();
            pixel_index_122 = (packed_pixels_121 & 3);
            pixel_color_123 = fx_ldb((uint16_t)(pixel_index_122 + 0x0200));
            fx_color((uint8_t)pixel_color_123);
            pixel_x_124 = (uint16_t)(pixel_x_120 + 1);
            pixel_x_125 = (uint16_t)(pixel_x_124 + 1);
            shifted_pixels_126 = (uint16_t)(packed_pixels_121 >> 1 | ((uint32_t)pixel_index_122 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_127 = (uint16_t)(shifted_pixels_126 >> 1 | (packed_pixels_121 & 1) << 15);
            rom_ptr_128 = (uint16_t)(rom_ptr_109 + 1);
            fx_r14(rom_ptr_128);
            shifted_pixels_129 = (uint16_t)((uint16_t)(shifted_pixels_127 >> 1 | (shifted_pixels_126 & 1) << 15) >> 1 | (shifted_pixels_127 & 1) << 15);
            pixel_index_130 = (shifted_pixels_127 & 3);
            pixel_color_131 = fx_ldb((uint16_t)(pixel_index_130 + 0x0200));
            fx_color((uint8_t)pixel_color_131);
            pixel_x_132 = (uint16_t)(pixel_x_125 + 1);
            pixel_x_133 = (uint16_t)(pixel_x_132 + 1);
            pixel_color_134 = fx_ldb((uint16_t)((shifted_pixels_129 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_134);
            pixel_x_135 = (uint16_t)(pixel_x_133 + 1);
            pixel_x_136 = (uint16_t)(pixel_x_135 + 1);
            pixel_color_137 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_129 >> 1 | ((uint32_t)pixel_index_130 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_129 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_137);
            pixel_x_138 = (uint16_t)(pixel_x_136 + 1);
            pixel_x_139 = (uint16_t)(pixel_x_138 + 1);
            fx_rpix((uint8_t)pixel_x_139, (uint8_t)second_row_y);
            packed_pixels_140 = fx_getb();
            pixel_index_141 = (packed_pixels_140 & 3);
            pixel_color_142 = fx_ldb((uint16_t)(pixel_index_141 + 0x0200));
            fx_color((uint8_t)pixel_color_142);
            pixel_x_143 = (uint16_t)(pixel_x_139 + 1);
            pixel_x_144 = (uint16_t)(pixel_x_143 + 1);
            shifted_pixels_145 = (uint16_t)(packed_pixels_140 >> 1 | ((uint32_t)pixel_index_141 + 0x0200 > 0xFFFF) << 15);
            shifted_pixels_146 = (uint16_t)(shifted_pixels_145 >> 1 | (packed_pixels_140 & 1) << 15);
            rom_ptr_147 = (uint16_t)(rom_ptr_128 + 1);
            fx_r14(rom_ptr_147);
            shifted_pixels_148 = (uint16_t)((uint16_t)(shifted_pixels_146 >> 1 | (shifted_pixels_145 & 1) << 15) >> 1 | (shifted_pixels_146 & 1) << 15);
            pixel_index_149 = (shifted_pixels_146 & 3);
            pixel_color_150 = fx_ldb((uint16_t)(pixel_index_149 + 0x0200));
            fx_color((uint8_t)pixel_color_150);
            pixel_x_151 = (uint16_t)(pixel_x_144 + 1);
            pixel_x_152 = (uint16_t)(pixel_x_151 + 1);
            pixel_color_153 = fx_ldb((uint16_t)((shifted_pixels_148 & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_153);
            pixel_x_154 = (uint16_t)(pixel_x_152 + 1);
            pixel_x_155 = (uint16_t)(pixel_x_154 + 1);
            pixel_color_156 = fx_ldb((uint16_t)(((uint16_t)((uint16_t)(shifted_pixels_148 >> 1 | ((uint32_t)pixel_index_149 + 0x0200 > 0xFFFF) << 15) >> 1 | (shifted_pixels_148 & 1) << 15) & 3) + 0x0200));
            fx_color((uint8_t)pixel_color_156);
            pixel_x_157 = (uint16_t)(pixel_x_155 + 1);
            fx_rpix((uint8_t)(uint16_t)(pixel_x_157 + 1), (uint8_t)second_row_y);
            rom_at = (uint16_t)(rom_ptr_147 + 0x001C);
            fx_r14(rom_at);
            pixel_x_2 = left_x;
            count = (uint16_t)(count - 1);
            row_y = (uint16_t)(second_row_y + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        return;
    } else {
        return;
    }
}


/*
 * @routine 07:FA1B FxScaledSprite13
 * WHAT IT DOES: Draws a scaled sprite at ($70:020A, $70:020C), (size across
 * / 2) * 13 / 16 pixels wide and (size down) / 16 high, the sizes at
 * $70:020E and $70:0210.
 * HOW: Width by DIV2 and MULT #13 and four doublings, the high byte; height
 * by four doublings, the high byte; then the shared scaled-sprite body (see
 * scaled_sprite: clipped to the screen, the source stepped by $70:0212 and
 * $70:0214, RPIX after each row and at the end).
 * Entry: started by the S-CPU. Exit: R1 = R10 = the left edge; R2 = the line
 * below the sprite; R3, R4 = the steps; R5 = the width; R7 = 0; R8, R9 = x,
 * y; R11 = $0200; R12 = 0; R13 = $FB7D; R0 = the pixel at the left edge of
 * that line; ROM bank $15, RAM bank 0, plot options 0. A sprite clipped away
 * or with bitmap 0 stops early with the registers as they are then.
 */
void FxScaledSprite13(uint16_t entry_row_y)
{
    int run_lFB50 = 0;
    uint16_t count;
    uint16_t row_y;
    uint16_t in_216;
    uint16_t in_216_2;
    uint16_t sprite_x;
    uint16_t sprite_y;
    uint16_t src_byte;
    uint16_t width_x2;
    uint16_t width_x4;
    uint16_t width_x8;
    uint16_t height_x2;
    uint16_t height_x4;
    uint16_t height_x8;
    uint16_t rows_left;
    uint16_t size_down;
    uint16_t clip_right;
    uint16_t src_row_x2;
    uint16_t src_row_x4;
    uint16_t src_row_x8;
    uint16_t src_step_x;
    uint16_t src_step_y;
    uint16_t clip_bottom;
    uint16_t pixel_color;
    uint16_t size_across;
    uint16_t src_row_x16;
    uint16_t row_offset_y;
    uint16_t reached_traces;
    uint16_t src_col_offset;
    uint16_t src_row_offset;
    uint16_t reached_traces_2;
    uint16_t reached_traces_3;
    uint16_t reached_traces_4;
    uint16_t shift_count_prev;
    uint16_t pixel_shift_count;
    uint16_t bottom_clipped_rows;
    uint16_t half_across_times_13;

    size_across = fx_ldw(0x020E);
    size_down = fx_ldw(0x0210);
    sprite_x = fx_ldw(0x020A);
    half_across_times_13 = (uint16_t)((int8_t)fx_div2(size_across) * (int8_t)0x000D);
    width_x2 = (uint16_t)(half_across_times_13 + half_across_times_13);
    width_x4 = (uint16_t)(width_x2 + width_x2);
    width_x8 = (uint16_t)(width_x4 + width_x4);
    reached_traces = (uint16_t)((uint16_t)(width_x8 + width_x8) >> 8);
    sprite_y = fx_ldw(0x020C);
    height_x2 = (uint16_t)(size_down + size_down);
    height_x4 = (uint16_t)(height_x2 + height_x2);
    height_x8 = (uint16_t)(height_x4 + height_x4);
    reached_traces_2 = (uint16_t)((uint16_t)(height_x8 + height_x8) >> 8);
    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    fx_r14(in_216);
    if (in_216 == 0) {
        return;
    } else {
        clip_right = fx_ldw(0x003A);
        if (sprite_x & 0x8000) {
            reached_traces_3 = (uint16_t)(sprite_x + reached_traces);
            reached_traces = reached_traces_3;
            if (reached_traces_3 & 0x8000) {
                return;
            } else {
                reached_traces_4 = 0;
                run_lFB50 = 1;
            }
        } else {
            if (sprite_x >= clip_right) {
                return;
            } else {
                if ((uint16_t)(sprite_x + reached_traces) < clip_right) {
                } else {
                    reached_traces = (uint16_t)(clip_right - sprite_x);
                }
                reached_traces_4 = sprite_x;
                run_lFB50 = 1;
            }
        }
        clip_bottom = fx_ldw(0x003E);
        if (sprite_y & 0x8000) {
            bottom_clipped_rows = (uint16_t)(sprite_y + reached_traces_2);
            reached_traces_2 = bottom_clipped_rows;
            if (bottom_clipped_rows & 0x8000) {
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)entry_row_y);
            } else {
                row_y = 0;
                src_step_x = fx_ldw(0x0212);
                src_step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset_y = (uint16_t)(row_y - sprite_y);
                        if (row_offset_y == 0) {
                        } else {
                            src_row_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset_y << 8 | row_offset_y >> 8) * (int16_t)src_step_y) >> 16);
                            src_row_x2 = (uint16_t)(src_row_offset + src_row_offset);
                            src_row_x4 = (uint16_t)(src_row_x2 + src_row_x2);
                            src_row_x8 = (uint16_t)(src_row_x4 + src_row_x4);
                            src_row_x16 = (uint16_t)(src_row_x8 + src_row_x8);
                            in_216_2 = (uint16_t)((uint16_t)(src_row_x16 + src_row_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), src_step_x, in_216_2, 0x0200);
                        src_col_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)src_step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)src_col_offset >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (src_col_offset & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
            }
            return;
        } else {
            if (sprite_y >= clip_bottom) {
                return;
            } else {
                if ((uint16_t)(sprite_y + reached_traces_2) < clip_bottom) {
                    row_y = sprite_y;
                } else {
                    reached_traces_2 = (uint16_t)(clip_bottom - sprite_y);
                    row_y = sprite_y;
                }
                src_step_x = fx_ldw(0x0212);
                src_step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset_y = (uint16_t)(row_y - sprite_y);
                        if (row_offset_y == 0) {
                        } else {
                            src_row_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset_y << 8 | row_offset_y >> 8) * (int16_t)src_step_y) >> 16);
                            src_row_x2 = (uint16_t)(src_row_offset + src_row_offset);
                            src_row_x4 = (uint16_t)(src_row_x2 + src_row_x2);
                            src_row_x8 = (uint16_t)(src_row_x4 + src_row_x4);
                            src_row_x16 = (uint16_t)(src_row_x8 + src_row_x8);
                            in_216_2 = (uint16_t)((uint16_t)(src_row_x16 + src_row_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), src_step_x, in_216_2, 0x0200);
                        src_col_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)src_step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)src_col_offset >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (src_col_offset & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                return;
            }
        }
    }
}


/*
 * @routine 07:FA35 FxScaledSprite12
 * WHAT IT DOES: As FxScaledSprite13, (size across / 2) * 12 / 16 pixels
 * wide.
 * HOW: As FxScaledSprite13, with MULT #12. No script has started it yet.
 * Entry: started by the S-CPU. Exit: as FxScaledSprite13.
 */
void FxScaledSprite12(uint16_t entry_row_y)
{
    int run_lFB50 = 0;
    uint16_t count;
    uint16_t row_y;
    uint16_t in_216;
    uint16_t rows_x2;
    uint16_t rows_x4;
    uint16_t rows_x8;
    uint16_t in_216_2;
    uint16_t sprite_x;
    uint16_t sprite_y;
    uint16_t src_byte;
    uint16_t width_x2;
    uint16_t width_x4;
    uint16_t width_x8;
    uint16_t rows_left;
    uint16_t clip_x_max;
    uint16_t clip_y_max;
    uint16_t row_offset;
    uint16_t row_src_hi;
    uint16_t row_src_x2;
    uint16_t row_src_x4;
    uint16_t row_src_x8;
    uint16_t pixel_color;
    uint16_t row_src_x16;
    uint16_t x_scale_step;
    uint16_t y_scale_step;
    uint16_t col_src_index;
    uint16_t reached_traces;
    uint16_t sprite_rows_src;
    uint16_t reached_traces_2;
    uint16_t reached_traces_3;
    uint16_t reached_traces_4;
    uint16_t shift_count_prev;
    uint16_t sprite_width_src;
    uint16_t pixel_shift_count;
    uint16_t top_clipped_height;
    uint16_t half_width_times_12;

    sprite_width_src = fx_ldw(0x020E);
    sprite_rows_src = fx_ldw(0x0210);
    sprite_x = fx_ldw(0x020A);
    half_width_times_12 = (uint16_t)((int8_t)fx_div2(sprite_width_src) * (int8_t)0x000C);
    width_x2 = (uint16_t)(half_width_times_12 + half_width_times_12);
    width_x4 = (uint16_t)(width_x2 + width_x2);
    width_x8 = (uint16_t)(width_x4 + width_x4);
    reached_traces = (uint16_t)((uint16_t)(width_x8 + width_x8) >> 8);
    sprite_y = fx_ldw(0x020C);
    rows_x2 = (uint16_t)(sprite_rows_src + sprite_rows_src);
    rows_x4 = (uint16_t)(rows_x2 + rows_x2);
    rows_x8 = (uint16_t)(rows_x4 + rows_x4);
    reached_traces_2 = (uint16_t)((uint16_t)(rows_x8 + rows_x8) >> 8);
    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    fx_r14(in_216);
    if (in_216 == 0) {
        return;
    } else {
        clip_x_max = fx_ldw(0x003A);
        if (sprite_x & 0x8000) {
            reached_traces_3 = (uint16_t)(sprite_x + reached_traces);
            reached_traces = reached_traces_3;
            if (reached_traces_3 & 0x8000) {
                return;
            } else {
                reached_traces_4 = 0;
                run_lFB50 = 1;
            }
        } else {
            if (sprite_x >= clip_x_max) {
                return;
            } else {
                if ((uint16_t)(sprite_x + reached_traces) < clip_x_max) {
                } else {
                    reached_traces = (uint16_t)(clip_x_max - sprite_x);
                }
                reached_traces_4 = sprite_x;
                run_lFB50 = 1;
            }
        }
        clip_y_max = fx_ldw(0x003E);
        if (sprite_y & 0x8000) {
            top_clipped_height = (uint16_t)(sprite_y + reached_traces_2);
            reached_traces_2 = top_clipped_height;
            if (top_clipped_height & 0x8000) {
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)entry_row_y);
            } else {
                row_y = 0;
                x_scale_step = fx_ldw(0x0212);
                y_scale_step = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(row_y - sprite_y);
                        if (row_offset == 0) {
                        } else {
                            row_src_hi = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)y_scale_step) >> 16);
                            row_src_x2 = (uint16_t)(row_src_hi + row_src_hi);
                            row_src_x4 = (uint16_t)(row_src_x2 + row_src_x2);
                            row_src_x8 = (uint16_t)(row_src_x4 + row_src_x4);
                            row_src_x16 = (uint16_t)(row_src_x8 + row_src_x8);
                            in_216_2 = (uint16_t)((uint16_t)(row_src_x16 + row_src_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), x_scale_step, in_216_2, 0x0200);
                        col_src_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)x_scale_step) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)col_src_index >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (col_src_index & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
            }
            return;
        } else {
            if (sprite_y >= clip_y_max) {
                return;
            } else {
                if ((uint16_t)(sprite_y + reached_traces_2) < clip_y_max) {
                    row_y = sprite_y;
                } else {
                    reached_traces_2 = (uint16_t)(clip_y_max - sprite_y);
                    row_y = sprite_y;
                }
                x_scale_step = fx_ldw(0x0212);
                y_scale_step = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(row_y - sprite_y);
                        if (row_offset == 0) {
                        } else {
                            row_src_hi = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)y_scale_step) >> 16);
                            row_src_x2 = (uint16_t)(row_src_hi + row_src_hi);
                            row_src_x4 = (uint16_t)(row_src_x2 + row_src_x2);
                            row_src_x8 = (uint16_t)(row_src_x4 + row_src_x4);
                            row_src_x16 = (uint16_t)(row_src_x8 + row_src_x8);
                            in_216_2 = (uint16_t)((uint16_t)(row_src_x16 + row_src_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), x_scale_step, in_216_2, 0x0200);
                        col_src_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)x_scale_step) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)col_src_index >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (col_src_index & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                return;
            }
        }
    }
}


/*
 * @routine 07:FA5A FxScaledSpriteEighth
 * WHAT IT DOES: Draws a scaled sprite at ($70:020A, $70:020C), (size across)
 * / 32 pixels wide and (size down) / 32 high, the sizes at $70:020E and
 * $70:0210.
 * HOW: Each size by three doublings, the high byte; then the shared
 * scaled-sprite body (see FxScaledSprite13).
 * Entry: started by the S-CPU. Exit: as FxScaledSprite13.
 */
void FxScaledSpriteEighth(uint16_t entry_row_y)
{
    int run_lFB50 = 0;
    uint16_t count;
    uint16_t row_y;
    uint16_t draw_x;
    uint16_t draw_y;
    uint16_t in_216;
    uint16_t step_x;
    uint16_t step_y;
    uint16_t in_216_2;
    uint16_t rows_left;
    uint16_t size_down;
    uint16_t row_offset;
    uint16_t size_across;
    uint16_t source_byte;
    uint16_t clip_right_x;
    uint16_t down_doubled;
    uint16_t clip_bottom_y;
    uint16_t palette_color;
    uint16_t source_row_x2;
    uint16_t source_row_x4;
    uint16_t source_row_x8;
    uint16_t across_doubled;
    uint16_t reached_traces;
    uint16_t source_col_pos;
    uint16_t source_row_x16;
    uint16_t down_quadrupled;
    uint16_t reached_traces_2;
    uint16_t reached_traces_3;
    uint16_t reached_traces_4;
    uint16_t source_row_index;
    uint16_t across_quadrupled;
    uint16_t clipped_row_count;
    uint16_t pixel_shift_count;
    uint16_t shift_count_before;

    size_across = fx_ldw(0x020E);
    size_down = fx_ldw(0x0210);
    draw_x = fx_ldw(0x020A);
    across_doubled = (uint16_t)(size_across + size_across);
    across_quadrupled = (uint16_t)(across_doubled + across_doubled);
    reached_traces = (uint16_t)((uint16_t)(across_quadrupled + across_quadrupled) >> 8);
    draw_y = fx_ldw(0x020C);
    down_doubled = (uint16_t)(size_down + size_down);
    down_quadrupled = (uint16_t)(down_doubled + down_doubled);
    reached_traces_2 = (uint16_t)((uint16_t)(down_quadrupled + down_quadrupled) >> 8);
    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    fx_r14(in_216);
    if (in_216 == 0) {
        return;
    } else {
        clip_right_x = fx_ldw(0x003A);
        if (draw_x & 0x8000) {
            reached_traces_3 = (uint16_t)(draw_x + reached_traces);
            reached_traces = reached_traces_3;
            if (reached_traces_3 & 0x8000) {
                return;
            } else {
                reached_traces_4 = 0;
                run_lFB50 = 1;
            }
        } else {
            if (draw_x >= clip_right_x) {
                return;
            } else {
                if ((uint16_t)(draw_x + reached_traces) < clip_right_x) {
                } else {
                    reached_traces = (uint16_t)(clip_right_x - draw_x);
                }
                reached_traces_4 = draw_x;
                run_lFB50 = 1;
            }
        }
        clip_bottom_y = fx_ldw(0x003E);
        if (draw_y & 0x8000) {
            clipped_row_count = (uint16_t)(draw_y + reached_traces_2);
            reached_traces_2 = clipped_row_count;
            if (clipped_row_count & 0x8000) {
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)entry_row_y);
            } else {
                row_y = 0;
                step_x = fx_ldw(0x0212);
                step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(row_y - draw_y);
                        if (row_offset == 0) {
                        } else {
                            source_row_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)step_y) >> 16);
                            source_row_x2 = (uint16_t)(source_row_index + source_row_index);
                            source_row_x4 = (uint16_t)(source_row_x2 + source_row_x2);
                            source_row_x8 = (uint16_t)(source_row_x4 + source_row_x4);
                            source_row_x16 = (uint16_t)(source_row_x8 + source_row_x8);
                            in_216_2 = (uint16_t)((uint16_t)(source_row_x16 + source_row_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - draw_x), step_x, in_216_2, 0x0200);
                        source_col_pos = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - draw_x) << 8 | (uint16_t)(reached_traces_4 - draw_x) >> 8) * (int16_t)step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)source_col_pos >> 1) >> 1) + in_216_2));
                        source_byte = fx_getb();
                        pixel_shift_count = (source_col_pos & 3);
                        for (;;) {
                            shift_count_before = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_before - 1);
                            if (shift_count_before == 0) {
                                break;
                            } else {
                                source_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)source_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        palette_color = fx_ldb((uint16_t)((source_byte & 3) + 0x0200));
                        fx_color((uint8_t)palette_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
            }
            return;
        } else {
            if (draw_y >= clip_bottom_y) {
                return;
            } else {
                if ((uint16_t)(draw_y + reached_traces_2) < clip_bottom_y) {
                    row_y = draw_y;
                } else {
                    reached_traces_2 = (uint16_t)(clip_bottom_y - draw_y);
                    row_y = draw_y;
                }
                step_x = fx_ldw(0x0212);
                step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(row_y - draw_y);
                        if (row_offset == 0) {
                        } else {
                            source_row_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)step_y) >> 16);
                            source_row_x2 = (uint16_t)(source_row_index + source_row_index);
                            source_row_x4 = (uint16_t)(source_row_x2 + source_row_x2);
                            source_row_x8 = (uint16_t)(source_row_x4 + source_row_x4);
                            source_row_x16 = (uint16_t)(source_row_x8 + source_row_x8);
                            in_216_2 = (uint16_t)((uint16_t)(source_row_x16 + source_row_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - draw_x), step_x, in_216_2, 0x0200);
                        source_col_pos = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - draw_x) << 8 | (uint16_t)(reached_traces_4 - draw_x) >> 8) * (int16_t)step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)source_col_pos >> 1) >> 1) + in_216_2));
                        source_byte = fx_getb();
                        pixel_shift_count = (source_col_pos & 3);
                        for (;;) {
                            shift_count_before = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_before - 1);
                            if (shift_count_before == 0) {
                                break;
                            } else {
                                source_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)source_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        palette_color = fx_ldb((uint16_t)((source_byte & 3) + 0x0200));
                        fx_color((uint8_t)palette_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                return;
            }
        }
    }
}


/*
 * @routine 07:FA88 FxSprite24x40
 * WHAT IT DOES: Draws a 24 x 40 sprite at ($70:020A, $70:020C), stepping
 * through the source by $70:0212 and $70:0214, and moves x on by 24.
 * HOW: Width $18 and height $28, then the shared scaled-sprite body (see
 * FxScaledSprite13).
 * Entry: started by the S-CPU. Exit: as FxScaledSprite13.
 */
void FxSprite24x40(uint16_t entry_row_y)
{
    int run_lFB50 = 0;
    uint16_t count;
    uint16_t row_y;
    uint16_t in_216;
    uint16_t in_216_2;
    uint16_t sprite_x;
    uint16_t sprite_y;
    uint16_t src_byte;
    uint16_t rows_left;
    uint16_t clip_right;
    uint16_t src_row_x2;
    uint16_t src_row_x4;
    uint16_t src_row_x8;
    uint16_t src_step_x;
    uint16_t src_step_y;
    uint16_t clip_bottom;
    uint16_t pixel_color;
    uint16_t src_row_x16;
    uint16_t row_offset_y;
    uint16_t reached_traces;
    uint16_t src_col_offset;
    uint16_t src_row_offset;
    uint16_t reached_traces_2;
    uint16_t reached_traces_3;
    uint16_t reached_traces_4;
    uint16_t shift_count_prev;
    uint16_t pixel_shift_count;
    uint16_t bottom_clipped_rows;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    sprite_x = fx_ldw(0x020A);
    fx_sbk((uint16_t)(sprite_x + 0x0018));
    reached_traces = 0x0018;
    sprite_y = fx_ldw(0x020C);
    reached_traces_2 = 0x0028;
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    fx_r14(in_216);
    if (in_216 == 0) {
        return;
    } else {
        clip_right = fx_ldw(0x003A);
        if (sprite_x & 0x8000) {
            reached_traces_3 = (uint16_t)(sprite_x + reached_traces);
            reached_traces = reached_traces_3;
            if (reached_traces_3 & 0x8000) {
                return;
            } else {
                reached_traces_4 = 0;
                run_lFB50 = 1;
            }
        } else {
            if (sprite_x >= clip_right) {
                return;
            } else {
                if ((uint16_t)(sprite_x + reached_traces) < clip_right) {
                } else {
                    reached_traces = (uint16_t)(clip_right - sprite_x);
                }
                reached_traces_4 = sprite_x;
                run_lFB50 = 1;
            }
        }
        clip_bottom = fx_ldw(0x003E);
        if (sprite_y & 0x8000) {
            bottom_clipped_rows = (uint16_t)(sprite_y + reached_traces_2);
            reached_traces_2 = bottom_clipped_rows;
            if (bottom_clipped_rows & 0x8000) {
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)entry_row_y);
            } else {
                row_y = 0;
                src_step_x = fx_ldw(0x0212);
                src_step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset_y = (uint16_t)(row_y - sprite_y);
                        if (row_offset_y == 0) {
                        } else {
                            src_row_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset_y << 8 | row_offset_y >> 8) * (int16_t)src_step_y) >> 16);
                            src_row_x2 = (uint16_t)(src_row_offset + src_row_offset);
                            src_row_x4 = (uint16_t)(src_row_x2 + src_row_x2);
                            src_row_x8 = (uint16_t)(src_row_x4 + src_row_x4);
                            src_row_x16 = (uint16_t)(src_row_x8 + src_row_x8);
                            in_216_2 = (uint16_t)((uint16_t)(src_row_x16 + src_row_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), src_step_x, in_216_2, 0x0200);
                        src_col_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)src_step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)src_col_offset >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (src_col_offset & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
            }
            return;
        } else {
            if (sprite_y >= clip_bottom) {
                return;
            } else {
                if ((uint16_t)(sprite_y + reached_traces_2) < clip_bottom) {
                    row_y = sprite_y;
                } else {
                    reached_traces_2 = (uint16_t)(clip_bottom - sprite_y);
                    row_y = sprite_y;
                }
                src_step_x = fx_ldw(0x0212);
                src_step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset_y = (uint16_t)(row_y - sprite_y);
                        if (row_offset_y == 0) {
                        } else {
                            src_row_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset_y << 8 | row_offset_y >> 8) * (int16_t)src_step_y) >> 16);
                            src_row_x2 = (uint16_t)(src_row_offset + src_row_offset);
                            src_row_x4 = (uint16_t)(src_row_x2 + src_row_x2);
                            src_row_x8 = (uint16_t)(src_row_x4 + src_row_x4);
                            src_row_x16 = (uint16_t)(src_row_x8 + src_row_x8);
                            in_216_2 = (uint16_t)((uint16_t)(src_row_x16 + src_row_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), src_step_x, in_216_2, 0x0200);
                        src_col_offset = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)src_step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)src_col_offset >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (src_col_offset & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                return;
            }
        }
    }
}


/*
 * @routine 07:FAB0 FxSprite16x32
 * WHAT IT DOES: Draws the 16 x 32 sprite at $70:0216 at ($70:020A, $70:020C)
 * unscaled (both steps at $70:0212 and $70:0214 set to $100) and moves x on
 * by 16, when x is between -15 and $D0.
 * HOW: The steps stored, then two unsigned compares of x; then the shared
 * scaled-sprite body. When x is $D0 or more the branch away has an IWT in
 * its delay slot, which takes its operand from the two bytes at the target
 * (STOP, NOP: R0 = $0100) and runs on into FxScaledSpriteAdvance (x above
 * $D0); the C does the same. No script has started it yet.
 * Entry: started by the S-CPU. Exit: as FxScaledSprite13.
 */
void FxSprite16x32(uint16_t entry_row_y)
{
    int run_lFB50 = 0;
    uint16_t r0_2;
    uint16_t count;
    uint16_t r11_0;
    uint16_t row_y;
    uint16_t in_216;
    uint16_t r_t_17;
    uint16_t r_t_21;
    uint16_t in_216_2;
    uint16_t sprite_x;
    uint16_t sprite_y;
    uint16_t src_byte;
    uint16_t rows_left;
    uint16_t size_down;
    uint16_t clip_x_max;
    uint16_t clip_y_max;
    uint16_t row_offset;
    uint16_t row_src_hi;
    uint16_t row_src_x2;
    uint16_t row_src_x4;
    uint16_t row_src_x8;
    uint16_t pixel_color;
    uint16_t row_src_x16;
    uint16_t size_across;
    uint16_t size_down_x2;
    uint16_t size_down_x4;
    uint16_t size_down_x8;
    uint16_t x_scale_step;
    uint16_t y_scale_step;
    uint16_t col_src_index;
    uint16_t size_down_x16;
    uint16_t reached_traces;
    uint16_t size_across_x2;
    uint16_t size_across_x4;
    uint16_t size_across_x8;
    uint16_t reached_traces_2;
    uint16_t reached_traces_3;
    uint16_t reached_traces_4;
    uint16_t shift_count_prev;
    uint16_t pixel_shift_count;
    uint16_t top_clipped_height;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    fx_stw(0x0212, 0x0100);
    fx_stw(0x0214, 0x0100);
    r11_0 = 0x0200;
    sprite_x = fx_ldw(0x020A);
    if (0xFFF0 < sprite_x) {
        r0_2 = 0x0010;
        fx_sbk((uint16_t)(r0_2 + sprite_x));
        reached_traces = 0x0010;
        sprite_y = fx_ldw(0x020C);
        reached_traces_2 = 0x0020;
    } else {
        if (0x00D0 < sprite_x) {
            fx_romb(0x0015);
            fx_ramb(0);
            fx_cmode(0);
            r11_0 = 0x0200;
            size_across = fx_ldw(0x020E);
            size_down = fx_ldw(0x0210);
            sprite_x = fx_ldw(0x020A);
            size_across_x2 = (uint16_t)(size_across + size_across);
            size_across_x4 = (uint16_t)(size_across_x2 + size_across_x2);
            size_across_x8 = (uint16_t)(size_across_x4 + size_across_x4);
            reached_traces = (uint16_t)((uint16_t)(size_across_x8 + size_across_x8) >> 8);
            fx_sbk((uint16_t)(reached_traces + sprite_x));
            sprite_y = fx_ldw(0x020C);
            size_down_x2 = (uint16_t)(size_down + size_down);
            size_down_x4 = (uint16_t)(size_down_x2 + size_down_x2);
            size_down_x8 = (uint16_t)(size_down_x4 + size_down_x4);
            size_down_x16 = (uint16_t)(size_down_x8 + size_down_x8);
            reached_traces_2 = (uint16_t)((uint16_t)(size_down_x16 + size_down_x16) >> 8);
        } else {
            r0_2 = 0x0010;
            fx_sbk((uint16_t)(r0_2 + sprite_x));
            reached_traces = 0x0010;
            sprite_y = fx_ldw(0x020C);
            reached_traces_2 = 0x0020;
        }
    }
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    fx_r14(in_216);
    if (in_216 == 0) {
        return;
    } else {
        clip_x_max = fx_ldw(0x003A);
        r_t_17 = sprite_x;
        if (r_t_17 & 0x8000) {
            reached_traces_3 = (uint16_t)(r_t_17 + reached_traces);
            reached_traces = reached_traces_3;
            if (reached_traces_3 & 0x8000) {
                return;
            } else {
                reached_traces_4 = 0;
                run_lFB50 = 1;
            }
        } else {
            if (r_t_17 >= clip_x_max) {
                return;
            } else {
                if ((uint16_t)(r_t_17 + reached_traces) < clip_x_max) {
                } else {
                    reached_traces = (uint16_t)(clip_x_max - r_t_17);
                }
                reached_traces_4 = r_t_17;
                run_lFB50 = 1;
            }
        }
        clip_y_max = fx_ldw(0x003E);
        r_t_21 = sprite_y;
        if (r_t_21 & 0x8000) {
            top_clipped_height = (uint16_t)(r_t_21 + reached_traces_2);
            reached_traces_2 = top_clipped_height;
            if (top_clipped_height & 0x8000) {
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)entry_row_y);
            } else {
                row_y = 0;
                x_scale_step = fx_ldw(0x0212);
                y_scale_step = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(row_y - r_t_21);
                        if (row_offset == 0) {
                        } else {
                            row_src_hi = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)y_scale_step) >> 16);
                            row_src_x2 = (uint16_t)(row_src_hi + row_src_hi);
                            row_src_x4 = (uint16_t)(row_src_x2 + row_src_x2);
                            row_src_x8 = (uint16_t)(row_src_x4 + row_src_x4);
                            row_src_x16 = (uint16_t)(row_src_x8 + row_src_x8);
                            in_216_2 = (uint16_t)((uint16_t)(row_src_x16 + row_src_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - r_t_17), x_scale_step, in_216_2, r11_0);
                        col_src_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - r_t_17) << 8 | (uint16_t)(reached_traces_4 - r_t_17) >> 8) * (int16_t)x_scale_step) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)col_src_index >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (col_src_index & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + r11_0));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
            }
            return;
        } else {
            if (r_t_21 >= clip_y_max) {
                return;
            } else {
                if ((uint16_t)(r_t_21 + reached_traces_2) < clip_y_max) {
                    row_y = r_t_21;
                } else {
                    reached_traces_2 = (uint16_t)(clip_y_max - r_t_21);
                    row_y = r_t_21;
                }
                x_scale_step = fx_ldw(0x0212);
                y_scale_step = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(row_y - r_t_21);
                        if (row_offset == 0) {
                        } else {
                            row_src_hi = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)y_scale_step) >> 16);
                            row_src_x2 = (uint16_t)(row_src_hi + row_src_hi);
                            row_src_x4 = (uint16_t)(row_src_x2 + row_src_x2);
                            row_src_x8 = (uint16_t)(row_src_x4 + row_src_x4);
                            row_src_x16 = (uint16_t)(row_src_x8 + row_src_x8);
                            in_216_2 = (uint16_t)((uint16_t)(row_src_x16 + row_src_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, row_y, reached_traces, (uint16_t)(reached_traces_4 - r_t_17), x_scale_step, in_216_2, r11_0);
                        col_src_index = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - r_t_17) << 8 | (uint16_t)(reached_traces_4 - r_t_17) >> 8) * (int16_t)x_scale_step) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)col_src_index >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (col_src_index & 3);
                        for (;;) {
                            shift_count_prev = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_prev - 1);
                            if (shift_count_prev == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + r11_0));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    row_y = (uint16_t)(row_y + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)row_y);
                return;
            }
        }
    }
}


/*
 * @routine 07:FAF2 FxScaledSpriteAdvance
 * WHAT IT DOES: Draws a scaled sprite at ($70:020A, $70:020C), (size across)
 * / 16 pixels wide and (size down) / 8 high, the sizes at $70:020E and
 * $70:0210, and moves x on by its width.
 * HOW: Width by four doublings and height by five, each the high byte; the
 * width added to $020A; then the shared scaled-sprite body (see
 * FxScaledSprite13).
 * Entry: started by the S-CPU. Exit: as FxScaledSprite13.
 */
void FxScaledSpriteAdvance(uint16_t rpix_y)
{
    int run_lFB50 = 0;
    uint16_t count;
    uint16_t in_216;
    uint16_t in_216_2;
    uint16_t sprite_x;
    uint16_t sprite_y;
    uint16_t src_byte;
    uint16_t rows_left;
    uint16_t size_down;
    uint16_t row_offset;
    uint16_t row_scaled;
    uint16_t pixel_color;
    uint16_t size_across;
    uint16_t clip_right_x;
    uint16_t scale_step_x;
    uint16_t scale_step_y;
    uint16_t size_down_x2;
    uint16_t size_down_x4;
    uint16_t size_down_x8;
    uint16_t clip_bottom_y;
    uint16_t column_scaled;
    uint16_t first_src_row;
    uint16_t row_scaled_x2;
    uint16_t row_scaled_x4;
    uint16_t row_scaled_x8;
    uint16_t size_down_x16;
    uint16_t clipped_height;
    uint16_t reached_traces;
    uint16_t row_scaled_x16;
    uint16_t size_across_x2;
    uint16_t size_across_x4;
    uint16_t size_across_x8;
    uint16_t reached_traces_2;
    uint16_t reached_traces_3;
    uint16_t reached_traces_4;
    uint16_t pixel_shift_count;
    uint16_t shift_count_before;

    fx_romb(0x0015);
    fx_ramb(0);
    fx_cmode(0);
    size_across = fx_ldw(0x020E);
    size_down = fx_ldw(0x0210);
    sprite_x = fx_ldw(0x020A);
    size_across_x2 = (uint16_t)(size_across + size_across);
    size_across_x4 = (uint16_t)(size_across_x2 + size_across_x2);
    size_across_x8 = (uint16_t)(size_across_x4 + size_across_x4);
    reached_traces = (uint16_t)((uint16_t)(size_across_x8 + size_across_x8) >> 8);
    fx_sbk((uint16_t)(reached_traces + sprite_x));
    sprite_y = fx_ldw(0x020C);
    size_down_x2 = (uint16_t)(size_down + size_down);
    size_down_x4 = (uint16_t)(size_down_x2 + size_down_x2);
    size_down_x8 = (uint16_t)(size_down_x4 + size_down_x4);
    size_down_x16 = (uint16_t)(size_down_x8 + size_down_x8);
    reached_traces_2 = (uint16_t)((uint16_t)(size_down_x16 + size_down_x16) >> 8);
    in_216 = fx_ldw(GSU_IN_216);
    fx_r14(in_216);
    fx_r14(in_216);
    if (in_216 == 0) {
        return;
    } else {
        clip_right_x = fx_ldw(0x003A);
        if (sprite_x & 0x8000) {
            reached_traces_3 = (uint16_t)(sprite_x + reached_traces);
            reached_traces = reached_traces_3;
            if (reached_traces_3 & 0x8000) {
                return;
            } else {
                reached_traces_4 = 0;
                run_lFB50 = 1;
            }
        } else {
            if (sprite_x >= clip_right_x) {
                return;
            } else {
                if ((uint16_t)(sprite_x + reached_traces) < clip_right_x) {
                } else {
                    reached_traces = (uint16_t)(clip_right_x - sprite_x);
                }
                reached_traces_4 = sprite_x;
                run_lFB50 = 1;
            }
        }
        clip_bottom_y = fx_ldw(0x003E);
        if (sprite_y & 0x8000) {
            clipped_height = (uint16_t)(sprite_y + reached_traces_2);
            reached_traces_2 = clipped_height;
            if (clipped_height & 0x8000) {
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)rpix_y);
            } else {
                first_src_row = 0;
                scale_step_x = fx_ldw(0x0212);
                scale_step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(first_src_row - sprite_y);
                        if (row_offset == 0) {
                        } else {
                            row_scaled = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)scale_step_y) >> 16);
                            row_scaled_x2 = (uint16_t)(row_scaled + row_scaled);
                            row_scaled_x4 = (uint16_t)(row_scaled_x2 + row_scaled_x2);
                            row_scaled_x8 = (uint16_t)(row_scaled_x4 + row_scaled_x4);
                            row_scaled_x16 = (uint16_t)(row_scaled_x8 + row_scaled_x8);
                            in_216_2 = (uint16_t)((uint16_t)(row_scaled_x16 + row_scaled_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, first_src_row, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), scale_step_x, in_216_2, 0x0200);
                        column_scaled = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)scale_step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)column_scaled >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (column_scaled & 3);
                        for (;;) {
                            shift_count_before = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_before - 1);
                            if (shift_count_before == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)first_src_row);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    first_src_row = (uint16_t)(first_src_row + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)first_src_row);
            }
            return;
        } else {
            if (sprite_y >= clip_bottom_y) {
                return;
            } else {
                if ((uint16_t)(sprite_y + reached_traces_2) < clip_bottom_y) {
                    first_src_row = sprite_y;
                } else {
                    reached_traces_2 = (uint16_t)(clip_bottom_y - sprite_y);
                    first_src_row = sprite_y;
                }
                scale_step_x = fx_ldw(0x0212);
                scale_step_y = fx_ldw(0x0214);
                for (;;) {
                    fx_scaled_row_start();
                    count = reached_traces;
                    in_216_2 = fx_ldw(GSU_IN_216);
                    fx_r14(in_216_2);
                    for (;;) {
                        row_offset = (uint16_t)(first_src_row - sprite_y);
                        if (row_offset == 0) {
                        } else {
                            row_scaled = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(row_offset << 8 | row_offset >> 8) * (int16_t)scale_step_y) >> 16);
                            row_scaled_x2 = (uint16_t)(row_scaled + row_scaled);
                            row_scaled_x4 = (uint16_t)(row_scaled_x2 + row_scaled_x2);
                            row_scaled_x8 = (uint16_t)(row_scaled_x4 + row_scaled_x4);
                            row_scaled_x16 = (uint16_t)(row_scaled_x8 + row_scaled_x8);
                            in_216_2 = (uint16_t)((uint16_t)(row_scaled_x16 + row_scaled_x16) + in_216_2);
                            fx_r14(in_216_2);
                        }
                        fx_scaled_row(reached_traces_4, first_src_row, reached_traces, (uint16_t)(reached_traces_4 - sprite_x), scale_step_x, in_216_2, 0x0200);
                        column_scaled = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)((uint16_t)(reached_traces_4 - sprite_x) << 8 | (uint16_t)(reached_traces_4 - sprite_x) >> 8) * (int16_t)scale_step_x) >> 16);
                        fx_r14((uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)column_scaled >> 1) >> 1) + in_216_2));
                        src_byte = fx_getb();
                        pixel_shift_count = (column_scaled & 3);
                        for (;;) {
                            shift_count_before = pixel_shift_count;
                            pixel_shift_count = (uint16_t)(shift_count_before - 1);
                            if (shift_count_before == 0) {
                                break;
                            } else {
                                src_byte = (uint16_t)((int16_t)(uint16_t)((int16_t)src_byte >> 1) >> 1);
                                continue;
                            }
                        }
                        pixel_color = fx_ldb((uint16_t)((src_byte & 3) + 0x0200));
                        fx_color((uint8_t)pixel_color);
                        reached_traces_4 = (uint16_t)(reached_traces_4 + 1);
                        count = (uint16_t)(count - 1);
                        if (count != 0) {
                            in_216_2 = fx_ldw(GSU_IN_216);
                            fx_r14(in_216_2);
                            continue;
                        } else {
                            break;
                        }
                    }
                    fx_rpix((uint8_t)reached_traces_4, (uint8_t)first_src_row);
                    reached_traces_4 = (uint16_t)(reached_traces_4 - reached_traces);
                    rows_left = (uint16_t)(reached_traces_2 - 1);
                    reached_traces_2 = rows_left;
                    first_src_row = (uint16_t)(first_src_row + 1);
                    if (rows_left != 0) {
                        continue;
                    } else {
                        break;
                    }
                }
                fx_rpix((uint8_t)reached_traces_4, (uint8_t)first_src_row);
                return;
            }
        }
    }
}

