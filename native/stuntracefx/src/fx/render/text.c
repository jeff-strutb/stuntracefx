/*
 * text.c: the Super FX code of asm/stuntracefx/render/text.asm, written from it by
 * the generator.
 *
 * text.asm: the Super FX drawing a string of text and pictures into the
 * screen it renders: glyphs from '0' up, $0D a new line, 0 the end, and the
 * commands $F5-$FF (position, palette, font, space, half line, pictures).
 * C: src/stuntracefx/render/text.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:D12F FxTextNotice
 * WHAT IT DOES: When bit 0 of $273C is set and there is a nearest car
 * ($2648) that is neither $2476 (the player) nor $2478, and its $264C is
 * not negative, draws its label (the string from the table at $01:D1CB,
 * by the word at $96 + the car, in ROM bank 1) at Y $68 and X = half of
 * $264A plus the screen centre ($34), held to 0..$3A - $11; $2732 is
 * cleared after.
 * HOW: Pushes R11 on the R10 stack; CMPs and MOVES for the tests; DIV2,
 * then BPL and CMP for the bounds; ROMB 1, GETB and GETBH for the string's
 * address; FxTextDraw. Returns by pulling R15.
 * Entry: R10 = stack; R11 = return address. Exit: as FxTextDraw, R0 = 0;
 * or, when nothing is drawn, R0 and R1 as the last test left them; S and
 * Z from R10; the last RAM address R10.
 */
FxTextNoticeResult FxTextNotice(uint16_t prev_text_arg, uint16_t *stack)
{
    uint16_t end_x;
    uint16_t near_x;
    uint16_t near_z;
    uint16_t text_x;
    uint16_t near_car;
    uint16_t text_x_max;
    uint16_t text_x_raw;
    uint16_t view_right;
    uint16_t message_ptr;
    uint16_t notice_flags;
    uint16_t view_center_x;
    uint16_t message_ptr_low;
    uint16_t car_message_index;
    uint16_t message_table_addr;

    fx_spill(stack);
    notice_flags = fx_ldw(0x273C);
    end_x = 1;
    if ((notice_flags & end_x) != 0) {
        near_car = fx_ldw(FX_NEAR_CAR);
        if (near_car != 0) {
            end_x = fx_ldw(0x2476);
            if (near_car != end_x) {
                end_x = fx_ldw(GSU_MATCH_2478);
                if (near_car != end_x) {
                    prev_text_arg = near_car;
                    near_x = fx_ldw(FX_NEAR_X);
                    if (!(near_x & 0x8000)) {
                        near_z = fx_ldw(FX_NEAR_Z);
                        view_center_x = fx_ldw(0x0034);
                        text_x_raw = (uint16_t)(fx_div2(near_z) + view_center_x);
                        text_x = text_x_raw;
                        if (!(text_x_raw & 0x8000)) {
                        } else {
                            text_x = 0;
                        }
                        view_right = fx_ldw(0x003A);
                        text_x_max = (uint16_t)(view_right - 0x0011);
                        if ((uint16_t)(text_x - text_x_max) & 0x8000) {
                        } else {
                            text_x = text_x_max;
                        }
                        fx_stw(FX_TEXT_X, text_x);
                        fx_stw(FX_TEXT_Y, 0x0068);
                        fx_stw(FX_TEXT_BANK, 1);
                        fx_romb(1);
                        car_message_index = fx_ldw((uint16_t)(prev_text_arg + 0x0096));
                        fx_r14(0xD1CB);
                        message_table_addr = (uint16_t)((uint16_t)(car_message_index + car_message_index) - 0x2E35);
                        fx_r14(message_table_addr);
                        message_ptr_low = fx_getb();
                        fx_r14((uint16_t)(message_table_addr + 1));
                        message_ptr = (uint16_t)((message_ptr_low & 0xFF) | fx_getb() << 8);
                        fx_stw(0x2732, message_ptr);
                        { FxTextDrawResult r_ = FxTextDraw(text_x_max, stack); end_x = r_.end_x; prev_text_arg = r_.end_y; };
                        fx_stw(0x2732, 0);
                    } else {
                    }
                } else {
                }
            } else {
            }
        } else {
        }
    } else {
    }
    fx_unspill(stack);
    return (FxTextNoticeResult){end_x, prev_text_arg};
}


/*
 * @routine 01:D307 FxTextProgram
 * WHAT IT DOES: The program the S-CPU starts to draw the string at $2732:
 * FxTextDraw with the R10 stack at $0240.
 * HOW: IWT R10, LINK, the jump, then STOP.
 * Entry: none. Exit: as FxTextDraw, R10 = $0240.
 */
void FxTextProgram(uint16_t text_ptr)
{
    uint16_t stack_top;
    uint16_t *stack = &stack_top;

    *stack = 0x0240;
    FxTextDraw(text_ptr, stack);
    return;
}


/*
 * @routine 01:D311 FxTextDraw
 * WHAT IT DOES: Draws the string at the address in $2732 (ROM bank $2734)
 * from the position at $2738/$273A: glyphs advance X by 20 (10 in the small
 * font), $0D goes back to the start X a line down; $FF sets the position
 * (two bytes), $FE the palette (one byte: $2702 + 4 * it), $FD the font
 * (one byte: 0 big, 32 lines, else small, 16 lines), $FC a space (8, 4 in
 * the small font), $F5 half a line down; $FB, $FA, $F9, $F8, $F7 and $F6
 * draw pictures from bank $15 ($3700 120 x 32, $3B00 120 x 16, $1810 64 x
 * 16, $3700 and $3A00 120 x 24, $3D1C 16 x 24). Other bytes below '0' only
 * advance X. Nothing is drawn when $2732 is 0.
 * HOW: Pushes R11 on the R10 stack. RAMB 0, CMODE 0, R9 = $2702. R8 walks
 * the string: each byte is read by setting R14 to it in the string's bank
 * (ROMB from $2734 each time), GETB, SEX. Commands are CMPs against bytes
 * IBT sign-extends; their bytes are read through R14. A glyph's bitmap
 * offset comes from ROM bank 1 ($F4D2 big, $F52E small) and is drawn by
 * FxDrawBitmap16x32 or FxDrawBitmap8x16. At the end RPIX writes out the
 * pixel caches. Returns by pulling R15.
 * Entry: R10 = stack; R11 = return address. Exit: R0 the pixel RPIX read
 * at (R1, R2); R4, R2 the position reached; R7 the line start X; R5, R6
 * the font; R8 past the string; R9 the palette; the rest as the last
 * drawing left them; S and Z from R10; the last RAM address R10. R14 and
 * the ROM buffer move as on the Super FX.
 */
FxTextDrawResult FxTextDraw(uint16_t entry_x, uint16_t *stack)
{
    uint16_t rom_at;
    uint16_t text_x;
    uint16_t start_x;
    uint16_t end_row_y;
    uint16_t text_bank;
    uint16_t big_offset;
    uint16_t next_row_y;
    uint16_t script_ptr;
    uint16_t small_font;
    uint16_t coord_x_ptr;
    uint16_t coord_y_ptr;
    uint16_t glyph_index;
    uint16_t line_height;
    uint16_t palette_ptr;
    uint16_t script_byte;
    uint16_t script_char;
    uint16_t space_width;
    uint16_t bottom_row_y;
    uint16_t char_advance;
    uint16_t small_offset;
    uint16_t big_offset_lo;
    uint16_t big_bitmap_ptr;
    uint16_t big_table_addr;
    uint16_t font_size_byte;
    uint16_t glyph_index_x2;
    uint16_t palette_number;
    int is_command_code;
    uint16_t small_offset_lo;
    uint16_t text_script_ptr;
    uint16_t after_coords_ptr;
    uint16_t bitmap_ptr_16x24;
    uint16_t bitmap_ptr_64x16;
    uint16_t small_bitmap_ptr;
    uint16_t small_table_addr;
    uint16_t bitmap_ptr_120x16;
    uint16_t bitmap_ptr_120x32;
    uint16_t palette_number_x2;
    uint16_t bitmap_ptr_120x24_a;
    uint16_t bitmap_ptr_120x24_b;
    uint16_t small_font_for_glyph;
    uint16_t small_font_for_space;
    uint16_t small_font_for_advance;

    fx_spill(stack);
    text_x = fx_ldw(FX_TEXT_X);
    bottom_row_y = fx_ldw(FX_TEXT_Y);
    small_font = 0;
    line_height = 0x0020;
    fx_ramb(0);
    fx_cmode(0);
    palette_ptr = 0x2702;
    text_script_ptr = fx_ldw(0x2732);
    script_ptr = text_script_ptr;
    if (text_script_ptr != 0) {
        start_x = text_x;
        for (;;) {
            text_bank = fx_ldw(FX_TEXT_BANK);
            fx_romb((uint16_t)(text_bank & 0xFF));
            rom_at = script_ptr;
            fx_r14(rom_at);
            script_byte = fx_getb();
            script_ptr = (uint16_t)(script_ptr + 1);
            script_char = (uint16_t)(int16_t)(int8_t)script_byte;
            is_command_code = ((script_char & 0x8000) != 0);
            if (script_char != 0) {
                if (is_command_code) {
                    coord_x_ptr = (uint16_t)(rom_at + 1);
                    fx_r14(coord_x_ptr);
                    if ((uint16_t)(script_char + 1) != 0) {
                        if ((uint16_t)(script_char + 2) != 0) {
                            if ((uint16_t)(script_char + 3) != 0) {
                                if ((uint16_t)(script_char + 4) != 0) {
                                    if ((uint16_t)(script_char + 0x000B) != 0) {
                                        if ((uint16_t)(script_char + 5) != 0) {
                                            if ((uint16_t)(script_char + 6) != 0) {
                                                if ((uint16_t)(script_char + 7) != 0) {
                                                    if ((uint16_t)(script_char + 8) != 0) {
                                                        if ((uint16_t)(script_char + 9) != 0) {
                                                            if ((uint16_t)(script_char + 0x000A) != 0) {
                                                                if (script_char != 0x000D) {
                                                                    glyph_index = (uint16_t)(script_char - 0x0030);
                                                                    if (!(glyph_index & 0x8000)) {
                                                                        fx_romb(1);
                                                                        glyph_index_x2 = (uint16_t)(glyph_index + glyph_index);
                                                                        small_font_for_glyph = small_font;
                                                                        small_font = small_font_for_glyph;
                                                                        if (small_font_for_glyph != 0) {
                                                                            fx_r14(0xF52E);
                                                                            small_table_addr = (uint16_t)(glyph_index_x2 - 0x0AD2);
                                                                            fx_r14(small_table_addr);
                                                                            small_offset_lo = fx_getb();
                                                                            fx_r14((uint16_t)(small_table_addr + 1));
                                                                            small_offset = (uint16_t)((small_offset_lo & 0xFF) | fx_getb() << 8);
                                                                            fx_romb(0x0015);
                                                                            fx_r14(0x8000);
                                                                            small_bitmap_ptr = (uint16_t)(small_offset - 0x8000);
                                                                            fx_r14(small_bitmap_ptr);
                                                                            { FxDrawBitmap8x16Result r_ = FxDrawBitmap8x16(bottom_row_y, start_x, palette_ptr, small_bitmap_ptr); entry_x = r_.right_x; next_row_y = r_.next_row_y; };
                                                                            bottom_row_y = (uint16_t)(next_row_y - line_height);
                                                                        } else {
                                                                            fx_r14(0xF4D2);
                                                                            big_table_addr = (uint16_t)(glyph_index_x2 - 0x0B2E);
                                                                            fx_r14(big_table_addr);
                                                                            big_offset_lo = fx_getb();
                                                                            fx_r14((uint16_t)(big_table_addr + 1));
                                                                            big_offset = (uint16_t)((big_offset_lo & 0xFF) | fx_getb() << 8);
                                                                            fx_romb(0x0015);
                                                                            fx_r14(0x8000);
                                                                            big_bitmap_ptr = (uint16_t)(big_offset - 0x8000);
                                                                            fx_r14(big_bitmap_ptr);
                                                                            { FxDrawBitmap16x32Result r_ = FxDrawBitmap16x32(bottom_row_y, start_x, palette_ptr, big_bitmap_ptr); entry_x = r_.end_x; end_row_y = r_.end_row_y; };
                                                                            bottom_row_y = (uint16_t)(end_row_y - line_height);
                                                                        }
                                                                    } else {
                                                                    }
                                                                    char_advance = 0x000A;
                                                                    small_font_for_advance = small_font;
                                                                    small_font = small_font_for_advance;
                                                                    if (small_font_for_advance != 0) {
                                                                    } else {
                                                                        char_advance = 0x0014;
                                                                    }
                                                                    start_x = (uint16_t)(start_x + char_advance);
                                                                    continue;
                                                                } else {
                                                                    start_x = text_x;
                                                                    bottom_row_y = (uint16_t)(bottom_row_y + line_height);
                                                                    continue;
                                                                }
                                                            } else {
                                                                fx_romb(0x0015);
                                                                fx_r14(0x8000);
                                                                bitmap_ptr_16x24 = (uint16_t)(0 - 0x42E4);
                                                                fx_r14(bitmap_ptr_16x24);
                                                                { FxDrawBitmap16x24Result r_ = FxDrawBitmap16x24(bottom_row_y, start_x, palette_ptr, bitmap_ptr_16x24); entry_x = r_.end_x; bottom_row_y = r_.next_y; };
                                                                continue;
                                                            }
                                                        } else {
                                                            fx_romb(0x0015);
                                                            fx_r14(0x8000);
                                                            bitmap_ptr_120x24_a = (uint16_t)(0 - 0x4600);
                                                            fx_r14(bitmap_ptr_120x24_a);
                                                            { FxDrawBitmap120x24Result r_ = FxDrawBitmap120x24(bottom_row_y, start_x, palette_ptr, bitmap_ptr_120x24_a); entry_x = r_.end_x; bottom_row_y = r_.end_row_y; };
                                                            continue;
                                                        }
                                                    } else {
                                                        fx_romb(0x0015);
                                                        fx_r14(0x8000);
                                                        bitmap_ptr_120x24_b = (uint16_t)(0 - 0x4900);
                                                        fx_r14(bitmap_ptr_120x24_b);
                                                        { FxDrawBitmap120x24Result r_ = FxDrawBitmap120x24(bottom_row_y, start_x, palette_ptr, bitmap_ptr_120x24_b); entry_x = r_.end_x; bottom_row_y = r_.end_row_y; };
                                                        continue;
                                                    }
                                                } else {
                                                    fx_romb(0x0015);
                                                    fx_r14(0x8000);
                                                    bitmap_ptr_64x16 = (uint16_t)(0 - 0x67F0);
                                                    fx_r14(bitmap_ptr_64x16);
                                                    { FxDrawBitmap64x16Result r_ = FxDrawBitmap64x16(bottom_row_y, start_x, palette_ptr, bitmap_ptr_64x16); entry_x = r_.right_x; bottom_row_y = r_.final_row_y; };
                                                    continue;
                                                }
                                            } else {
                                                fx_romb(0x0015);
                                                fx_r14(0x8000);
                                                bitmap_ptr_120x16 = (uint16_t)(0 - 0x4500);
                                                fx_r14(bitmap_ptr_120x16);
                                                { FxDrawBitmap120x16Result r_ = FxDrawBitmap120x16(bottom_row_y, start_x, palette_ptr, bitmap_ptr_120x16); entry_x = r_.right_x; bottom_row_y = r_.next_row_y; };
                                                continue;
                                            }
                                        } else {
                                            fx_romb(0x0015);
                                            fx_r14(0x8000);
                                            bitmap_ptr_120x32 = (uint16_t)(0 - 0x4900);
                                            fx_r14(bitmap_ptr_120x32);
                                            { FxDrawBitmap120x32Result r_ = FxDrawBitmap120x32(bottom_row_y, start_x, palette_ptr, bitmap_ptr_120x32); entry_x = r_.right_x; bottom_row_y = r_.bottom_row_y; };
                                            continue;
                                        }
                                    } else {
                                        start_x = text_x;
                                        bottom_row_y = (uint16_t)(bottom_row_y + fx_div2(line_height));
                                        continue;
                                    }
                                } else {
                                    space_width = 4;
                                    small_font_for_space = small_font;
                                    small_font = small_font_for_space;
                                    if (small_font_for_space != 0) {
                                    } else {
                                        space_width = 8;
                                    }
                                    start_x = (uint16_t)(start_x + space_width);
                                    continue;
                                }
                            } else {
                                font_size_byte = fx_getb();
                                script_ptr = (uint16_t)(script_ptr + 1);
                                if (font_size_byte == 0) {
                                    line_height = 0x0020;
                                    small_font = 0;
                                    continue;
                                } else {
                                    line_height = 0x0010;
                                    small_font = 1;
                                    continue;
                                }
                            }
                        } else {
                            palette_number = fx_getb();
                            script_ptr = (uint16_t)(script_ptr + 1);
                            palette_number_x2 = (uint16_t)(palette_number + palette_number);
                            palette_ptr = (uint16_t)((uint16_t)(palette_number_x2 + palette_number_x2) + 0x2702);
                            continue;
                        }
                    } else {
                        text_x = fx_getb();
                        coord_y_ptr = (uint16_t)(coord_x_ptr + 1);
                        fx_r14(coord_y_ptr);
                        bottom_row_y = fx_getb();
                        after_coords_ptr = (uint16_t)(coord_y_ptr + 1);
                        fx_r14(after_coords_ptr);
                        start_x = text_x;
                        script_ptr = after_coords_ptr;
                        continue;
                    }
                } else {
                    if (script_char != 0x000D) {
                        glyph_index = (uint16_t)(script_char - 0x0030);
                        if (!(glyph_index & 0x8000)) {
                            fx_romb(1);
                            glyph_index_x2 = (uint16_t)(glyph_index + glyph_index);
                            small_font_for_glyph = small_font;
                            small_font = small_font_for_glyph;
                            if (small_font_for_glyph != 0) {
                                fx_r14(0xF52E);
                                small_table_addr = (uint16_t)(glyph_index_x2 - 0x0AD2);
                                fx_r14(small_table_addr);
                                small_offset_lo = fx_getb();
                                fx_r14((uint16_t)(small_table_addr + 1));
                                small_offset = (uint16_t)((small_offset_lo & 0xFF) | fx_getb() << 8);
                                fx_romb(0x0015);
                                fx_r14(0x8000);
                                small_bitmap_ptr = (uint16_t)(small_offset - 0x8000);
                                fx_r14(small_bitmap_ptr);
                                { FxDrawBitmap8x16Result r_ = FxDrawBitmap8x16(bottom_row_y, start_x, palette_ptr, small_bitmap_ptr); entry_x = r_.right_x; next_row_y = r_.next_row_y; };
                                bottom_row_y = (uint16_t)(next_row_y - line_height);
                            } else {
                                fx_r14(0xF4D2);
                                big_table_addr = (uint16_t)(glyph_index_x2 - 0x0B2E);
                                fx_r14(big_table_addr);
                                big_offset_lo = fx_getb();
                                fx_r14((uint16_t)(big_table_addr + 1));
                                big_offset = (uint16_t)((big_offset_lo & 0xFF) | fx_getb() << 8);
                                fx_romb(0x0015);
                                fx_r14(0x8000);
                                big_bitmap_ptr = (uint16_t)(big_offset - 0x8000);
                                fx_r14(big_bitmap_ptr);
                                { FxDrawBitmap16x32Result r_ = FxDrawBitmap16x32(bottom_row_y, start_x, palette_ptr, big_bitmap_ptr); entry_x = r_.end_x; end_row_y = r_.end_row_y; };
                                bottom_row_y = (uint16_t)(end_row_y - line_height);
                            }
                        } else {
                        }
                        char_advance = 0x000A;
                        small_font_for_advance = small_font;
                        small_font = small_font_for_advance;
                        if (small_font_for_advance != 0) {
                        } else {
                            char_advance = 0x0014;
                        }
                        start_x = (uint16_t)(start_x + char_advance);
                        continue;
                    } else {
                        start_x = text_x;
                        bottom_row_y = (uint16_t)(bottom_row_y + line_height);
                        continue;
                    }
                }
            } else {
                break;
            }
        }
    } else {
    }
    fx_rpix((uint8_t)entry_x, (uint8_t)bottom_row_y);
    fx_unspill(stack);
    return (FxTextDrawResult){entry_x, bottom_row_y};
}

