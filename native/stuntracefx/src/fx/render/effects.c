/*
 * effects.c: the Super FX code of asm/stuntracefx/render/effects.asm, written from it by
 * the generator.
 *
 * effects.asm: the Super FX's short-lived screen effects. Sixteen records of
 * six bytes at $2688: +$00 the time left (0: the record is free), +$01 a
 * byte, +$02 X and +$04 Y on the screen (words).
 * C: src/stuntracefx/render/effects.c.
 */

#include "fx/fx.h"

/*
 * @routine 01:C060 FxEffectsAge
 * WHAT IT DOES: Counts down the time of each effect in use and frees the
 * ones outside the screen less a margin of 8: X below $38 + 8 or not below
 * $3A - 8, Y below $3C + 8 or not below $3E - 8.
 * HOW: The margins into R8, R7, R4 and R5 (ADD #8, SUB #8). R6 = 8 times
 * the word at +$4A of the record at $247A, or 0 when that is negative
 * (nothing here uses it). A LOOP over the sixteen records, R13 = its
 * start ($C095), R3 stepping by 6: a record whose time (LDB, LOB) is not
 * 0 has it counted down (DEC, STB); then R2 = its byte, R9 = $0034
 * (unused), R1 = X, R2 = Y, and CMP against each margin; one outside has
 * its first word set to 0. WITH then TO is a MOVE, WITH then FROM a
 * MOVES. Returns by a MOVE of R11 to R15.
 * Entry: R11 = return address. Exit: R3 = $26E8; R12 = 0; R13 = $C095;
 * R8, R7, R4, R5 the margins; R6 as above; R0, R1, R2, R9 and the OV and
 * CY flags as the last record in use, or the last free record, left them;
 * S clear, Z set (LOOP).
 */
uint16_t FxEffectsAge(uint16_t entry_y)
{
    uint16_t count;
    uint16_t speed;
    uint16_t x_ptr;
    uint16_t y_ptr;
    uint16_t byte_ptr;
    uint16_t effect_x;
    uint16_t time_left;
    uint16_t top_limit;
    uint16_t effect_ptr;
    uint16_t left_limit;
    uint16_t player_car;
    uint16_t screen_top;
    uint16_t right_limit;
    uint16_t screen_left;
    uint16_t bottom_limit;
    uint16_t screen_right;
    uint16_t effect_y_copy;
    uint16_t screen_bottom;
    uint16_t time_left_byte;
    uint16_t record_start_ptr;

    screen_left = fx_ldw(0x0038);
    left_limit = (uint16_t)(screen_left + 8);
    screen_right = fx_ldw(0x003A);
    right_limit = (uint16_t)(screen_right - 8);
    screen_top = fx_ldw(0x003C);
    top_limit = (uint16_t)(screen_top + 8);
    screen_bottom = fx_ldw(0x003E);
    bottom_limit = (uint16_t)(screen_bottom - 8);
    player_car = fx_ldw(0x247A);
    speed = fx_ldw((uint16_t)(player_car + FXREC_SPEED));
    if (speed & 0x8000) {
    } else {
    }
    effect_ptr = 0x2688;
    count = 0x0010;
    for (;;) {
        time_left_byte = fx_ldb(effect_ptr);
        time_left = (uint16_t)(time_left_byte & 0xFF);
        if (time_left == 0) {
            effect_ptr = (uint16_t)(effect_ptr + 6);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            fx_stb(effect_ptr, (uint8_t)(uint16_t)(time_left - 1));
            byte_ptr = (uint16_t)(effect_ptr + 1);
            fx_ldb(byte_ptr);
            x_ptr = (uint16_t)(byte_ptr + 1);
            fx_ldw(0x0034);
            effect_x = fx_ldw(x_ptr);
            y_ptr = (uint16_t)(x_ptr + 2);
            entry_y = fx_ldw(y_ptr);
            effect_ptr = (uint16_t)(y_ptr + 2);
            if ((uint16_t)(effect_x - left_limit) & 0x8000) {
                record_start_ptr = (uint16_t)(effect_ptr - 6);
                fx_stw(record_start_ptr, 0);
                effect_ptr = (uint16_t)(record_start_ptr + 6);
            } else {
                if (!((uint16_t)(effect_x - right_limit) & 0x8000)) {
                    record_start_ptr = (uint16_t)(effect_ptr - 6);
                    fx_stw(record_start_ptr, 0);
                    effect_ptr = (uint16_t)(record_start_ptr + 6);
                } else {
                    effect_y_copy = entry_y;
                    if ((uint16_t)(effect_y_copy - top_limit) & 0x8000) {
                        record_start_ptr = (uint16_t)(effect_ptr - 6);
                        fx_stw(record_start_ptr, 0);
                        effect_ptr = (uint16_t)(record_start_ptr + 6);
                    } else {
                        if ((uint16_t)(effect_y_copy - bottom_limit) & 0x8000) {
                        } else {
                            record_start_ptr = (uint16_t)(effect_ptr - 6);
                            fx_stw(record_start_ptr, 0);
                            effect_ptr = (uint16_t)(record_start_ptr + 6);
                        }
                    }
                }
            }
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    return entry_y;
}


/*
 * @routine 01:BF6D FxEffectsRun
 * WHAT IT DOES: With bit 14 of $2736 set, starts new effects (as many as
 * the tied object's speed, +$4A of the record at $247A, over 8, at most
 * $26EE; none when it is negative), ages them and draws them.
 * HOW: Pushes R11 on the R10 stack; three DIV2s; CMP against $26EE keeps
 * the smaller. FxEffectsSpawn (when there are any to start),
 * FxEffectsAge and FxEffectsDraw. Returns by pulling R15.
 * Entry: R10 = stack; R11 = return address. Exit: as FxEffectsDraw, or
 * with the bit clear R0 = 0, R1 = $2736, Z set by the AND then S and Z
 * from R10; the last RAM address R10.
 */
uint16_t FxEffectsRun(uint16_t entry_plot_result, uint16_t *stack)
{
    int rC;
    uint16_t speed;
    uint16_t effect_max;
    uint16_t spawn_count;
    uint16_t effects_flags;
    uint16_t last_effect_y;
    uint16_t spawn_count_copy;
    uint16_t player_car_record;

    fx_spill(stack);
    effects_flags = fx_ldw(0x2736);
    if ((0x4000 & effects_flags) == 0) {
    } else {
        spawn_count = 0;
        player_car_record = fx_ldw(0x247A);
        rC = ((uint32_t)0x004A + player_car_record > 0xFFFF);
        speed = fx_ldw((uint16_t)(player_car_record + FXREC_SPEED));
        if (speed & 0x8000) {
        } else {
            spawn_count = fx_div2(fx_div2(fx_div2(speed)));
            effect_max = fx_ldw(FX_EFFECT_MAX);
            rC = (effect_max >= spawn_count);
            if (!((uint16_t)(effect_max - spawn_count) & 0x8000)) {
            } else {
                spawn_count = effect_max;
            }
        }
        spawn_count_copy = spawn_count;
        if (spawn_count_copy == 0) {
        } else {
            spawn_count = FxEffectsSpawn(spawn_count_copy, spawn_count, rC);
        }
        last_effect_y = FxEffectsAge(spawn_count);
        entry_plot_result = FxEffectsDraw(last_effect_y, stack);
    }
    fx_unspill(stack);
    return entry_plot_result;
}


/*
 * @routine 01:BFB7 FxHazeSample
 * WHAT IT DOES: Reads the pixels of a 16 x 16 shape at (R1, R2), each row
 * moved down or up by its own offset, and lists the ones the shape covers
 * at $0342 with where each goes back: X - 8 + its column, Y - 8 + its row
 * (the pixel word, then those two bytes). $0340 counts them.
 * HOW: The shape for the time R0 is found at $08:F92E + 4 * R0: a word
 * pointing to sixteen signed row offsets and one pointing to the shape's
 * 256 bytes (ROM bank 8). CACHE; sixteen rows (R7, BNE), each with a LOOP
 * of sixteen columns (R12, R13 = $C008): a shape byte (GETB, SEX) of 0
 * skips the pixel; otherwise its value moves X and RPIX reads the pixel
 * at that X and the row's Y. R5 and R6 step the return position (INC in
 * the delay slots). The count, (R11 - $0342) / 4 by two DIV2s, is added
 * to $0340 by SBK. Returns by pulling R15.
 * Entry: R0 = the time; R1 = X, R2 = Y; R10 = stack; R11 = return address.
 * Exit: R11 past the list; R7 = R12 = 0; R1 = $0340; R2 its old value; R0
 * = the new count; R3 = X, R4 = the last row's Y, R5, R6 past the last
 * return position, R8 past the row offsets, R9 = Y; R14 past the shape; the
 * ROM bank 8; S and Z from R10; the last RAM address R10. R14 and the ROM
 * buffer move as on the Super FX.
 */
uint16_t FxHazeSample(uint16_t time, uint16_t x, uint16_t y, uint16_t *stack)
{
    uint16_t count;
    uint16_t rom_at;
    uint16_t haze_x8;
    uint16_t col_byte;
    uint16_t cols_ptr;
    uint16_t rows_ptr;
    uint16_t sample_x;
    uint16_t rows_left;
    uint16_t col_offset;
    uint16_t row_offset;
    uint16_t old_value_2;
    uint16_t cols_ptr_low;
    uint16_t pixel_colour;
    uint16_t row_screen_y;
    uint16_t rows_ptr_low;
    uint16_t sample_row_y;
    uint16_t time_doubled;
    uint16_t sample_y_slot;
    uint16_t rows_left_next;
    uint16_t sample_out_ptr;
    uint16_t rows_entry_addr;
    uint16_t sample_row_slot;
    uint16_t pattern_entry_addr;
    uint16_t rows_entry_hi_addr;
    uint16_t pattern_entry_hi_addr;

    fx_spill(stack);
    fx_stw(0x26F6, x);
    fx_stw(FX_HAZE_X8, (uint16_t)(x - 8));
    sample_row_y = (uint16_t)(y - 8);
    time_doubled = (uint16_t)(time + time);
    fx_romb(8);
    fx_r14(0xF92E);
    pattern_entry_addr = (uint16_t)((uint16_t)(time_doubled + time_doubled) - 0x06D2);
    fx_r14(pattern_entry_addr);
    cols_ptr_low = fx_getb();
    pattern_entry_hi_addr = (uint16_t)(pattern_entry_addr + 1);
    fx_r14(pattern_entry_hi_addr);
    cols_ptr = (uint16_t)((cols_ptr_low & 0xFF) | fx_getb() << 8);
    rows_entry_addr = (uint16_t)(pattern_entry_hi_addr + 1);
    fx_r14(rows_entry_addr);
    rows_ptr_low = fx_getb();
    rows_entry_hi_addr = (uint16_t)(rows_entry_addr + 1);
    fx_r14(rows_entry_hi_addr);
    rows_ptr = (uint16_t)((rows_ptr_low & 0xFF) | fx_getb() << 8);
    fx_r14((uint16_t)(rows_entry_hi_addr + 1));
    rom_at = cols_ptr;
    fx_r14(rom_at);
    rows_left = 0x0010;
    sample_out_ptr = 0x0342;
    for (;;) {
        fx_r14(rows_ptr);
        row_offset = (uint16_t)(int16_t)(int8_t)fx_getb();
        rows_ptr = (uint16_t)(rows_ptr + 1);
        row_screen_y = (uint16_t)(row_offset + y);
        fx_r14(rom_at);
        sample_x = fx_ldw(0x26F6);
        haze_x8 = fx_ldw(FX_HAZE_X8);
        count = 0x0010;
        for (;;) {
            col_byte = fx_getb();
            col_offset = (uint16_t)(int16_t)(int8_t)col_byte;
            rom_at = (uint16_t)(rom_at + 1);
            fx_r14(rom_at);
            if (col_offset == 0) {
            } else {
                pixel_colour = fx_rpix((uint8_t)(uint16_t)(col_offset + sample_x), (uint8_t)row_screen_y);
                fx_stw(sample_out_ptr, pixel_colour);
                sample_y_slot = (uint16_t)(sample_out_ptr + 2);
                fx_stb(sample_y_slot, (uint8_t)haze_x8);
                sample_row_slot = (uint16_t)(sample_y_slot + 1);
                fx_stb(sample_row_slot, (uint8_t)sample_row_y);
                sample_out_ptr = (uint16_t)(sample_row_slot + 1);
            }
            count = (uint16_t)(count - 1);
            haze_x8 = (uint16_t)(haze_x8 + 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
        rows_left_next = (uint16_t)(rows_left - 1);
        rows_left = rows_left_next;
        sample_row_y = (uint16_t)(sample_row_y + 1);
        if (rows_left_next != 0) {
            continue;
        } else {
            break;
        }
    }
    old_value_2 = fx_ldw(0x0340);
    fx_sbk((uint16_t)(fx_div2(fx_div2((uint16_t)(sample_out_ptr - 0x0342))) + old_value_2));
    fx_unspill(stack);
    return old_value_2;
}


/*
 * @routine 01:C039 FxHazePlot
 * WHAT IT DOES: Plots back the pixels FxHazeSample listed, each at its
 * return position, so the shape's patch of screen is drawn again moved:
 * a shimmer.
 * HOW: The count from $0340; none returns at once. CMODE 1 (colour 0 is
 * plotted); CACHE; a LOOP (R13 = $C04B) of COLOR from the pixel word, R1
 * and R2 from the two bytes (LDB) and PLOT in the delay slot. Returns
 * with MOVE R15,R11.
 * Entry: R11 = return address. Exit: R3 past the list; R1 one past the
 * last X (PLOT steps it), R2 the last Y; R12 = 0; R13 = $C04B; R0 the
 * last pixel word; the plot options 1; flags from the LOOP; the last RAM
 * address the last Y's.
 */
uint16_t FxHazePlot(uint16_t entry_plot_y)
{
    uint16_t count;
    uint16_t haze_x;
    uint16_t haze_count;
    uint16_t haze_colour;
    uint16_t haze_x_addr;
    uint16_t haze_y_addr;
    uint16_t haze_table_ptr;

    haze_count = fx_ldw(0x0340);
    haze_table_ptr = (uint16_t)(0 + 0x0342);
    count = haze_count;
    if (haze_count == 0) {
    } else {
        fx_cmode(1);
        for (;;) {
            haze_colour = fx_ldw(haze_table_ptr);
            haze_x_addr = (uint16_t)(haze_table_ptr + 2);
            fx_color((uint8_t)haze_colour);
            haze_x = fx_ldb(haze_x_addr);
            haze_y_addr = (uint16_t)(haze_x_addr + 1);
            entry_plot_y = fx_ldb(haze_y_addr);
            haze_table_ptr = (uint16_t)(haze_y_addr + 1);
            count = (uint16_t)(count - 1);
            fx_plot((uint8_t)haze_x, (uint8_t)entry_plot_y);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    return entry_plot_y;
}


/*
 * @routine 01:C130 FxEffectsSpawn
 * WHAT IT DOES: Starts an effect in each free one of the first R0 effect
 * records: time 8, its byte 1 or 2 at random, and a random X and Y (the
 * low and high bytes of a random word).
 * HOW: The seed from $01D0 in R4; a LOOP of R0 records from $2688 (R13 =
 * $C13A), each taken by LDB, LOB; a record in use is skipped with ADD #6.
 * Each random number is one random step (SWAP, ROR, ADD, ADC). The seed
 * goes back to $01D0; returns with MOVE R15,R11.
 * Entry: R0 = the number of records to look at; R11 = return address.
 * Exit: R4 = the seed; R1 past the last record looked at; R12 = 0; R13 =
 * $C13A; R0, R2, R3 as the last record left them; S, Z from the LOOP
 * (Z set), OV and CY from the last ADD or ADC; the last RAM address $01D0.
 */
uint16_t FxEffectsSpawn(uint16_t number_records, uint16_t incoming_x, int rC)
{
    uint16_t count;
    uint16_t x_ptr;
    uint16_t y_ptr;
    uint16_t kind_ptr;
    uint16_t seed_sum;
    uint16_t effect_ptr;
    uint16_t seed_sum_2;
    uint16_t effect_time;
    uint16_t random_seed;
    uint16_t random_word;
    uint16_t seed_shifted;
    uint16_t seed_2_shifted;
    int seed_sum_carry;
    int seed_2_sum_carry;

    random_seed = fx_ldw(0x01D0);
    effect_ptr = 0x2688;
    count = number_records;
    for (;;) {
        effect_time = fx_ldb(effect_ptr);
        if ((uint16_t)(effect_time & 0xFF) != 0) {
            rC = ((uint32_t)effect_ptr + 6 > 0xFFFF);
            effect_ptr = (uint16_t)(effect_ptr + 6);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        } else {
            fx_stb(effect_ptr, (uint8_t)8);
            kind_ptr = (uint16_t)(effect_ptr + 1);
            seed_shifted = (uint16_t)((uint16_t)(random_seed << 8 | random_seed >> 8) >> 1 | rC << 15);
            seed_sum = (uint16_t)(seed_shifted + random_seed);
            seed_sum_carry = ((uint32_t)seed_shifted + random_seed > 0xFFFF);
            seed_sum_2 = (uint16_t)(seed_sum + random_seed + seed_sum_carry);
            fx_stb(kind_ptr, (uint8_t)(uint16_t)((seed_sum & 1) + 1));
            x_ptr = (uint16_t)(kind_ptr + 1);
            seed_2_shifted = (uint16_t)((uint16_t)(seed_sum_2 << 8 | seed_sum_2 >> 8) >> 1 | ((uint32_t)seed_sum + random_seed + seed_sum_carry > 0xFFFF) << 15);
            random_word = (uint16_t)(seed_2_shifted + seed_sum_2);
            seed_2_sum_carry = ((uint32_t)seed_2_shifted + seed_sum_2 > 0xFFFF);
            rC = ((uint32_t)random_word + seed_sum_2 + seed_2_sum_carry > 0xFFFF);
            random_seed = (uint16_t)(random_word + seed_sum_2 + seed_2_sum_carry);
            incoming_x = (uint16_t)(random_word & 0xFF);
            fx_stw(x_ptr, incoming_x);
            y_ptr = (uint16_t)(x_ptr + 2);
            fx_stw(y_ptr, (uint16_t)(random_word >> 8));
            effect_ptr = (uint16_t)(y_ptr + 2);
            count = (uint16_t)(count - 1);
            if (count != 0) {
                continue;
            } else {
                break;
            }
        }
    }
    fx_stw(0x01D0, random_seed);
    return incoming_x;
}


/*
 * @routine 01:C175 FxEffectsDraw
 * WHAT IT DOES: Draws each effect in use: FxHazeSample at its X and Y
 * with its time, then FxHazePlot.
 * HOW: Pushes R11 on the R10 stack; sixteen records from $2688 (R4
 * counting, BNE), R4 and R3 kept on the R10 stack around each. For a
 * record in use (its time by LDB, LOB, MOVES) $0340 is cleared first.
 * Returns by pulling R15.
 * Entry: R10 = stack; R11 = return address. Exit: R3 = $26E8, R4 = 0;
 * the rest as the last effect drawn leaves them; S and Z from R10; the
 * last RAM address R10.
 */
uint16_t FxEffectsDraw(uint16_t entry_plot_result, uint16_t *stack)
{
    uint16_t x;
    uint16_t y;
    uint16_t old_value;
    uint16_t effect_ptr;
    uint16_t effect_kind;
    uint16_t effects_left;
    uint16_t effect_x_addr;
    uint16_t effect_kind_byte;
    uint16_t effects_left_dec;
    uint16_t effect_ptr_restored;
    uint16_t effects_left_restored;

    fx_spill(stack);
    effect_ptr = 0x2688;
    effects_left = 0x0010;
    for (;;) {
        fx_push(stack, effects_left);
        fx_push(stack, effect_ptr);
        effect_kind_byte = fx_ldb(effect_ptr);
        effect_kind = (uint16_t)(effect_kind_byte & 0xFF);
        if (effect_kind == 0) {
        } else {
            fx_stw(0x0340, 0);
            effect_x_addr = (uint16_t)(effect_ptr + 2);
            x = fx_ldw(effect_x_addr);
            y = fx_ldw((uint16_t)(effect_x_addr + 2));
            old_value = FxHazeSample(effect_kind, x, y, stack);
            entry_plot_result = FxHazePlot(old_value);
        }
        effect_ptr_restored = fx_pull(stack);
        effects_left_restored = fx_pull(stack);
        effect_ptr = (uint16_t)(effect_ptr_restored + 6);
        effects_left_dec = (uint16_t)(effects_left_restored - 1);
        effects_left = effects_left_dec;
        if (effects_left_dec != 0) {
            continue;
        } else {
            break;
        }
    }
    fx_unspill(stack);
    return entry_plot_result;
}

