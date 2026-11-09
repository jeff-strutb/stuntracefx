/*
 * fx_course.c: the Super FX code of asm/stuntracefx/race/fx_course.asm, written from it by
 * the generator.
 *
 * fx_course.asm: the Super FX's reads of the course's segment table and the
 * lap count it keeps from them.
 *
 * The table is in ROM: its bank is the byte at $70:1094 and its address
 * the word at $70:1092. Its first word is the number of segments; 8-byte
 * entries follow (see render/table.asm). A car's Super FX record (R8) holds
 * its current segment at +$58, the last segment it was counted through at
 * +$86, and its laps at +$7A.
 * C: src/stuntracefx/race/fx_course.c.
 */

#include "fx/fx.h"

/*
 * @routine 04:C378 FxRecordHeadingAhead
 * WHAT IT DOES: The bearing (FxBearingTo) from record R8 to the segment
 * after the one nearest it (FxCourseNearest, FxCourseNextEntry): R0 = the
 * negated angle.
 * HOW: The position into R1-R3, then the three calls in a row; the return
 * address is kept on the R10 stack.
 * Entry: R8 = the record; R10 = stack; R11 = return address. Exit: as
 * FxBearingTo leaves them (R11 = $C39D when it did not divide); S, Z from
 * R10; the last RAM address R10.
 */
FxRecordHeadingAheadResult FxRecordHeadingAhead(uint16_t stale_entry_number, uint16_t stale_entry_ptr, uint16_t nearest_number, uint16_t record, uint16_t nearest_dist_x, uint16_t *stack)
{
    uint16_t y;
    uint16_t angle;
    uint16_t count;
    uint16_t entry;
    uint16_t half_x;
    uint16_t point_z;
    uint16_t words_1;
    uint16_t words_3;
    uint16_t number_1;
    uint16_t number_2;
    uint16_t rom_at_2;
    uint16_t entry_out;
    uint16_t rom_at_out;
    uint16_t entry_words_z;
    uint16_t last_dist_sum;
    uint16_t swap_mark_out;
    uint16_t angle_sign_xor_2;

    fx_spill(stack);
    half_x = fx_ldw((uint16_t)(record + FXREC_HALF_X));
    y = fx_ldw((uint16_t)(record + FXREC_Y));
    point_z = fx_ldw((uint16_t)(record + 0x0022));
    { FxCourseNearestResult r_ = FxCourseNearest(half_x, y, point_z, stale_entry_number, stale_entry_ptr, nearest_number, nearest_dist_x); number_1 = r_.number_1; entry_words_z = r_.entry_words_z; count = r_.count; entry = r_.entry; number_2 = r_.number_2; last_dist_sum = r_.last_dist_sum; rom_at_2 = r_.rom_at; };
    { FxCourseNextEntryResult r_ = FxCourseNextEntry(number_1, entry_words_z, count, entry, record, rom_at_2); words_1 = r_.words_1; words_3 = r_.words_3; entry_out = r_.entry_out; rom_at_out = r_.rom_at_out; };
    { FxBearingToResult r_ = FxBearingTo(words_1, words_3, entry_out, record, stack, rom_at_out); angle = r_.angle; angle_sign_xor_2 = r_.angle_sign_xor; swap_mark_out = r_.swap_mark_out; };
    fx_unspill(stack);
    return (FxRecordHeadingAheadResult){angle, angle_sign_xor_2, swap_mark_out, number_2, last_dist_sum, rom_at_out};
}


/*
 * @routine 04:C3B5 FxRecordSegment
 * WHAT IT DOES: FxCourseNearest for record R8's position.
 * HOW: The position (+$1E, +$20, +$22) into R1-R3, then the call; the
 * return address is kept on the R10 stack.
 * Entry: R8 = the record; R10 = stack; R11 = return address. Exit: as
 * FxCourseNearest; R11 = $C3D0; S, Z from R10; the last RAM address R10.
 */
FxRecordSegmentResult FxRecordSegment(uint16_t entry_scratch_a, uint16_t entry_scratch_b, uint16_t prev_best_number, uint16_t record, uint16_t prev_dist_sum, uint16_t *stack)
{
    uint16_t y;
    uint16_t count;
    uint16_t entry;
    uint16_t half_x;
    uint16_t point_z;
    uint16_t number_1;
    uint16_t rom_at_2;
    uint16_t entry_words_z;
    uint16_t last_dist_sum_2;

    fx_spill(stack);
    half_x = fx_ldw((uint16_t)(record + FXREC_HALF_X));
    y = fx_ldw((uint16_t)(record + FXREC_Y));
    point_z = fx_ldw((uint16_t)(record + 0x0022));
    { FxCourseNearestResult r_ = FxCourseNearest(half_x, y, point_z, entry_scratch_a, entry_scratch_b, prev_best_number, prev_dist_sum); number_1 = r_.number_1; entry_words_z = r_.entry_words_z; count = r_.count; entry = r_.entry; last_dist_sum_2 = r_.last_dist_sum; rom_at_2 = r_.rom_at; };
    fx_unspill(stack);
    return (FxRecordSegmentResult){number_1, entry_words_z, count, entry, last_dist_sum_2, rom_at_2};
}


/*
 * @routine 04:CC06 FxCourseNearest
 * WHAT IT DOES: Finds the segment table entry nearest the point (R1, R2,
 * R3) by |dx| + 4|dy| + |dz|: R0 = its number (from 1), R1-R3 = its first
 * three words, R5 its address. An entry whose sums overflow is passed
 * over. With no table (address 0) or no entries, R0 = 0.
 * HOW: A LOOP over the count with the ROM buffer (ROMBR = the table bank)
 * reading each entry's x, y, z; SUB with BVS for the overflow, NOT and INC
 * for the magnitudes; a sum below the best so far (CMP, BCS) takes its
 * place, R6 holding the best sum, R7 its number and R5 the address just
 * past its z. The routine runs under CACHE (the code cache only; nothing
 * the C has to do). The not-reached-yet path for a zero table address is
 * decoded from the ROM bytes and not reached in the traces so far.
 * Entry: R1, R2, R3 = the point; R11 = return address. Exit: R0 = R7 = the
 * number; R1-R3 = the entry's words; R4 = the count; R5 = the entry; R6 =
 * its sum ($7FFF when none was nearer); R9 from the last entry; R12 = 0;
 * R13 = $CC33; R14 = R5 + 6; ROMBR = the table bank; S, Z from R14, OV, CY
 * from R5's SUB; the last RAM address $1092. No entries: R0 = 0; R12 = 0;
 * R14 = table + 2; the flags from SUB R0.
 */
FxCourseNearestResult FxCourseNearest(uint16_t point_x, uint16_t point_y, uint16_t point_z, uint16_t prev_entry_count, uint16_t prev_entry_ptr, uint16_t prev_best_number, uint16_t prev_dist_sum)
{
    uint16_t dx;
    uint16_t dy;
    uint16_t dz;
    uint16_t abs_dx;
    uint16_t abs_dy;
    uint16_t abs_dz;
    uint16_t dist_x;
    uint16_t count_2;
    uint16_t count_3;
    uint16_t entry_x;
    uint16_t entry_y;
    uint16_t entry_z;
    uint16_t best_sum;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t abs_dy_x2;
    uint16_t abs_dy_x4;
    uint16_t best_x_low;
    uint16_t best_y_low;
    uint16_t best_y_ptr;
    uint16_t best_z_low;
    uint16_t best_z_ptr;
    uint16_t dist_total;
    uint16_t skip_z_ptr;
    uint16_t table_addr;
    uint16_t table_bank;
    uint16_t best_number;
    uint16_t entry_x_low;
    uint16_t entry_y_low;
    uint16_t entry_y_ptr;
    uint16_t entry_z_low;
    uint16_t entry_z_ptr;
    uint16_t entry_end_ptr;
    uint16_t best_entry_ptr;
    uint16_t count_high_ptr;
    uint16_t count_low_byte;
    uint16_t after_entry_ptr;
    uint16_t best_x_high_ptr;
    uint16_t best_y_high_ptr;
    uint16_t best_z_high_ptr;
    uint16_t entry_words_z_2;
    uint16_t best_after_z_ptr;
    uint16_t entry_x_high_ptr;
    uint16_t entry_y_high_ptr;
    uint16_t entry_z_high_ptr;
    uint16_t table_entry_total;

    table_bank = fx_ldw(0x1094);
    fx_romb(table_bank);
    table_addr = fx_ldw(0x1092);
    rom_at_2 = table_addr;
    fx_r14(rom_at_2);
    if (table_addr != 0) {
        count_low_byte = fx_getb();
        count_high_ptr = (uint16_t)(rom_at_2 + 1);
        fx_r14(count_high_ptr);
        table_entry_total = (uint16_t)((count_low_byte & 0xFF) | fx_getb() << 8);
        rom_at_2 = (uint16_t)(count_high_ptr + 1);
        fx_r14(rom_at_2);
        count_2 = table_entry_total;
        if (table_entry_total != 0) {
            best_after_z_ptr = 0;
            best_sum = 0x7FFF;
            count_3 = 0;
            best_number = 0;
            for (;;) {
                count_3 = (uint16_t)(count_3 + 1);
                entry_x_low = fx_getb();
                entry_x_high_ptr = (uint16_t)(rom_at_2 + 1);
                fx_r14(entry_x_high_ptr);
                entry_x = (uint16_t)((entry_x_low & 0xFF) | fx_getb() << 8);
                entry_y_ptr = (uint16_t)(entry_x_high_ptr + 1);
                fx_r14(entry_y_ptr);
                dx = (uint16_t)(entry_x - point_x);
                abs_dx = dx;
                if ((((entry_x ^ point_x) & (entry_x ^ (uint16_t)(entry_x - point_x - 0)) & 0x8000) != 0)) {
                    rom_at_3 = (uint16_t)(entry_y_ptr + 4);
                    fx_r14(rom_at_3);
                } else {
                    if (!(dx & 0x8000)) {
                    } else {
                        abs_dx = (uint16_t)((uint16_t)~abs_dx + 1);
                    }
                    dist_x = abs_dx;
                    entry_y_low = fx_getb();
                    entry_y_high_ptr = (uint16_t)(entry_y_ptr + 1);
                    fx_r14(entry_y_high_ptr);
                    entry_y = (uint16_t)((entry_y_low & 0xFF) | fx_getb() << 8);
                    entry_z_ptr = (uint16_t)(entry_y_high_ptr + 1);
                    fx_r14(entry_z_ptr);
                    dy = (uint16_t)(entry_y - point_y);
                    abs_dy = dy;
                    if (!(dy & 0x8000)) {
                    } else {
                        abs_dy = (uint16_t)((uint16_t)~abs_dy + 1);
                    }
                    abs_dy_x2 = (uint16_t)(abs_dy + abs_dy);
                    abs_dy_x4 = (uint16_t)(abs_dy_x2 + abs_dy_x2);
                    prev_dist_sum = (uint16_t)(dist_x + abs_dy_x4);
                    if (((~(dist_x ^ abs_dy_x4) & (abs_dy_x4 ^ (uint16_t)(dist_x + abs_dy_x4 + 0)) & 0x8000) != 0)) {
                        skip_z_ptr = (uint16_t)(entry_z_ptr + 1);
                        fx_r14(skip_z_ptr);
                        rom_at_3 = (uint16_t)(skip_z_ptr + 1);
                        fx_r14(rom_at_3);
                    } else {
                        entry_z_low = fx_getb();
                        entry_z_high_ptr = (uint16_t)(entry_z_ptr + 1);
                        fx_r14(entry_z_high_ptr);
                        entry_z = (uint16_t)((entry_z_low & 0xFF) | fx_getb() << 8);
                        rom_at_3 = (uint16_t)(entry_z_high_ptr + 1);
                        fx_r14(rom_at_3);
                        dz = (uint16_t)(entry_z - point_z);
                        abs_dz = dz;
                        if ((((entry_z ^ point_z) & (entry_z ^ (uint16_t)(entry_z - point_z - 0)) & 0x8000) != 0)) {
                        } else {
                            if (!(dz & 0x8000)) {
                            } else {
                                abs_dz = (uint16_t)((uint16_t)~abs_dz + 1);
                            }
                            dist_total = (uint16_t)(abs_dz + prev_dist_sum);
                            if (((~(abs_dz ^ prev_dist_sum) & (prev_dist_sum ^ (uint16_t)(abs_dz + prev_dist_sum + 0)) & 0x8000) != 0)) {
                            } else {
                                if (dist_total >= best_sum) {
                                } else {
                                    best_sum = dist_total;
                                    best_after_z_ptr = rom_at_3;
                                    best_number = count_3;
                                }
                            }
                        }
                    }
                }
                entry_end_ptr = (uint16_t)(rom_at_3 + 1);
                fx_r14(entry_end_ptr);
                count_2 = (uint16_t)(count_2 - 1);
                rom_at_2 = (uint16_t)(entry_end_ptr + 1);
                fx_r14(rom_at_2);
                if (count_2 != 0) {
                    continue;
                } else {
                    break;
                }
            }
            best_entry_ptr = (uint16_t)(best_after_z_ptr - 6);
            fx_r14(best_entry_ptr);
            best_x_low = fx_getb();
            best_x_high_ptr = (uint16_t)(best_entry_ptr + 1);
            fx_r14(best_x_high_ptr);
            fx_getb();
            best_y_ptr = (uint16_t)(best_x_high_ptr + 1);
            fx_r14(best_y_ptr);
            best_y_low = fx_getb();
            best_y_high_ptr = (uint16_t)(best_y_ptr + 1);
            fx_r14(best_y_high_ptr);
            fx_getb();
            best_z_ptr = (uint16_t)(best_y_high_ptr + 1);
            fx_r14(best_z_ptr);
            best_z_low = fx_getb();
            best_z_high_ptr = (uint16_t)(best_z_ptr + 1);
            fx_r14(best_z_high_ptr);
            entry_words_z_2 = (uint16_t)((best_z_low & 0xFF) | fx_getb() << 8);
            after_entry_ptr = (uint16_t)(best_z_high_ptr + 1);
            fx_r14(after_entry_ptr);
            return (FxCourseNearestResult){best_number, entry_words_z_2, count_3, best_entry_ptr, best_number, prev_dist_sum, after_entry_ptr};
        } else {
            return (FxCourseNearestResult){0, point_z, prev_entry_count, prev_entry_ptr, prev_best_number, prev_dist_sum, rom_at_2};
        }
    } else {
        return (FxCourseNearestResult){0, point_z, prev_entry_count, prev_entry_ptr, prev_best_number, prev_dist_sum, rom_at_2};
    }
}


/*
 * @routine 04:CCA4 FxCourseNextEntry
 * WHAT IT DOES: Steps car R8 on one segment: from number R0 at entry R5 to
 * the next (from the last to the first); R0 = R6 = the new number, R5 its
 * entry, R1-R4 its words. A car going the wrong way (bit 1 of +$70) steps
 * back instead (FxCoursePrevEntry).
 * HOW: The count is read through the ROM buffer (ROMBR = the table bank)
 * and compared with the number; the first entry is table + 2, otherwise R5
 * + 8. The wrong-way jump is decoded from the ROM bytes and not reached in
 * the traces so far.
 * Entry: R0 = number; R5 = its entry; R8 = the car's record; R11 = return
 * address. Exit: R0 = R6 = the number; R1-R4 = the words; R5 = the entry;
 * R14 = R5 + 8; ROMBR = the table bank; S, Z from R14; OV, CY from the
 * address ADD (or the CMP with the count); the last RAM address $1092.
 */
FxCourseNextEntryResult FxCourseNextEntry(uint16_t number, uint16_t prior_word3, uint16_t prior_word4, uint16_t entry, uint16_t car_record, uint16_t rom_at)
{
    uint16_t count_lo;
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t word1_lo;
    uint16_t word2_lo;
    uint16_t word3_lo;
    uint16_t word4_lo;
    uint16_t direction;
    uint16_t words_1_2;
    uint16_t words_3_2;
    uint16_t words_4_2;
    uint16_t table_bank;
    uint16_t table_head;
    uint16_t next_number;
    uint16_t word1_hi_ptr;
    uint16_t word2_hi_ptr;
    uint16_t word2_lo_ptr;
    uint16_t word3_hi_ptr;
    uint16_t word3_lo_ptr;
    uint16_t word4_hi_ptr;
    uint16_t word4_lo_ptr;
    uint16_t segment_count;
    uint16_t next_entry_end_ptr;

    table_bank = fx_ldw(0x1094);
    fx_romb(table_bank);
    direction = fx_ldw((uint16_t)(FXREC_DIRECTION + car_record));
    if ((2 & direction) == 0) {
        next_number = (uint16_t)(number + 1);
        rom_at_2 = fx_ldw(0x1092);
        fx_r14(rom_at_2);
        count_lo = fx_getb();
        fx_r14((uint16_t)(rom_at_2 + 1));
        segment_count = (uint16_t)((count_lo & 0xFF) | fx_getb() << 8);
        if (number != segment_count) {
            rom_at_3 = (uint16_t)(entry + 8);
            fx_r14(rom_at_3);
        } else {
            table_head = fx_ldw(0x1092);
            rom_at_3 = (uint16_t)(table_head + 2);
            fx_r14(rom_at_3);
            next_number = 1;
        }
        word1_lo = fx_getb();
        word1_hi_ptr = (uint16_t)(rom_at_3 + 1);
        fx_r14(word1_hi_ptr);
        words_1_2 = (uint16_t)((word1_lo & 0xFF) | fx_getb() << 8);
        word2_lo_ptr = (uint16_t)(word1_hi_ptr + 1);
        fx_r14(word2_lo_ptr);
        word2_lo = fx_getb();
        word2_hi_ptr = (uint16_t)(word2_lo_ptr + 1);
        fx_r14(word2_hi_ptr);
        fx_getb();
        word3_lo_ptr = (uint16_t)(word2_hi_ptr + 1);
        fx_r14(word3_lo_ptr);
        word3_lo = fx_getb();
        word3_hi_ptr = (uint16_t)(word3_lo_ptr + 1);
        fx_r14(word3_hi_ptr);
        words_3_2 = (uint16_t)((word3_lo & 0xFF) | fx_getb() << 8);
        word4_lo_ptr = (uint16_t)(word3_hi_ptr + 1);
        fx_r14(word4_lo_ptr);
        word4_lo = fx_getb();
        word4_hi_ptr = (uint16_t)(word4_lo_ptr + 1);
        fx_r14(word4_hi_ptr);
        words_4_2 = (uint16_t)((word4_lo & 0xFF) | fx_getb() << 8);
        next_entry_end_ptr = (uint16_t)(word4_hi_ptr + 1);
        fx_r14(next_entry_end_ptr);
        return (FxCourseNextEntryResult){next_number, words_1_2, words_3_2, words_4_2, rom_at_3, next_entry_end_ptr};
    } else {
        { FxCoursePrevEntryResult tr_ = FxCoursePrevEntry(number, entry); return (FxCourseNextEntryResult){tr_.number_1, tr_.words_1, tr_.words_3, tr_.words_4, tr_.entry_out, tr_.rom_at}; }
    }
}


/*
 * @routine 04:CD05 FxCoursePrevEntry
 * WHAT IT DOES: Steps back one segment: from number R0 at entry R5 to the
 * one before (from the first to the last); R0 = R6 = the new number, R5 its
 * entry, R1-R4 its words.
 * HOW: The last entry is table + 2 + (count - 1) * 8 (LMULT by R6 = 8),
 * the count read through the ROM buffer; otherwise R5 - 8. It is reached
 * only by FxCourseNextEntry's jump for a car going the wrong way; decoded
 * from the ROM bytes and not reached in the traces so far.
 * Entry: R0 = number; R5 = its entry; R11 = return address. Exit: R0 = R6 =
 * the number; R1-R4 = the words; R5 = the entry; R14 = R5 + 8; ROMBR = the
 * table bank; S, Z from R14; OV, CY from the address SUB or ADD; the last
 * RAM address $1094 or $1092.
 */
FxCoursePrevEntryResult FxCoursePrevEntry(uint16_t number, uint16_t entry)
{
    uint16_t rom_at_2;
    uint16_t rom_at_3;
    uint16_t words_1_2;
    uint16_t words_3_2;
    uint16_t words_4_2;
    uint16_t table_addr;
    uint16_t table_bank;
    uint16_t entry_ptr_1;
    uint16_t entry_ptr_2;
    uint16_t entry_ptr_3;
    uint16_t entry_ptr_4;
    uint16_t entry_ptr_5;
    uint16_t entry_ptr_6;
    uint16_t entry_ptr_7;
    uint16_t prev_number;
    uint16_t entry_end_ptr;
    uint16_t segment_count;
    uint16_t count_low_byte;
    uint16_t number_minus_1;
    uint16_t word1_low_byte;
    uint16_t word2_low_byte;
    uint16_t word3_low_byte;
    uint16_t word4_low_byte;

    table_bank = fx_ldw(0x1094);
    fx_romb(table_bank);
    number_minus_1 = (uint16_t)(number - 1);
    prev_number = number_minus_1;
    if (number_minus_1 != 0) {
        rom_at_2 = (uint16_t)(entry - 8);
        fx_r14(rom_at_2);
    } else {
        rom_at_3 = fx_ldw(0x1092);
        fx_r14(rom_at_3);
        count_low_byte = fx_getb();
        fx_r14((uint16_t)(rom_at_3 + 1));
        segment_count = (uint16_t)((count_low_byte & 0xFF) | fx_getb() << 8);
        table_addr = fx_ldw(0x1092);
        rom_at_2 = (uint16_t)((uint16_t)((uint16_t)(uint32_t)((int32_t)(int16_t)(uint16_t)(segment_count - 1) * (int16_t)8) + table_addr) + 2);
        fx_r14(rom_at_2);
        prev_number = segment_count;
    }
    word1_low_byte = fx_getb();
    entry_ptr_1 = (uint16_t)(rom_at_2 + 1);
    fx_r14(entry_ptr_1);
    words_1_2 = (uint16_t)((word1_low_byte & 0xFF) | fx_getb() << 8);
    entry_ptr_2 = (uint16_t)(entry_ptr_1 + 1);
    fx_r14(entry_ptr_2);
    word2_low_byte = fx_getb();
    entry_ptr_3 = (uint16_t)(entry_ptr_2 + 1);
    fx_r14(entry_ptr_3);
    fx_getb();
    entry_ptr_4 = (uint16_t)(entry_ptr_3 + 1);
    fx_r14(entry_ptr_4);
    word3_low_byte = fx_getb();
    entry_ptr_5 = (uint16_t)(entry_ptr_4 + 1);
    fx_r14(entry_ptr_5);
    words_3_2 = (uint16_t)((word3_low_byte & 0xFF) | fx_getb() << 8);
    entry_ptr_6 = (uint16_t)(entry_ptr_5 + 1);
    fx_r14(entry_ptr_6);
    word4_low_byte = fx_getb();
    entry_ptr_7 = (uint16_t)(entry_ptr_6 + 1);
    fx_r14(entry_ptr_7);
    words_4_2 = (uint16_t)((word4_low_byte & 0xFF) | fx_getb() << 8);
    entry_end_ptr = (uint16_t)(entry_ptr_7 + 1);
    fx_r14(entry_end_ptr);
    return (FxCoursePrevEntryResult){prev_number, words_1_2, words_3_2, words_4_2, rom_at_2, entry_end_ptr};
}


/*
 * @routine 04:CD56 FxCourseEntryRead
 * WHAT IT DOES: Reads the four words of the segment table entry at R5 into
 * R1-R4, keeping R0.
 * HOW: R0 is pushed on the R10 stack, ROMB from the table bank byte, then
 * GETB and GETBH through R14 = R5 and pulled back.
 * Entry: R0 = any; R5 = entry address; R10 = stack; R11 = return address.
 * Exit: R1-R4 = the words; R14 = R5 + 8; ROMBR = the table bank; R0 as on
 * entry; the last RAM address R10; S, Z from R10.
 */
FxCourseEntryReadResult FxCourseEntryRead(uint16_t entry_address, uint16_t *stack)
{
    uint16_t word1_lo;
    uint16_t word2_lo;
    uint16_t word3_lo;
    uint16_t word4_lo;
    uint16_t word2_ptr;
    uint16_t word3_ptr;
    uint16_t word4_ptr;
    uint16_t words_1_2;
    uint16_t words_2_2;
    uint16_t words_3_2;
    uint16_t words_4_2;
    uint16_t word1_hi_ptr;
    uint16_t word2_hi_ptr;
    uint16_t word3_hi_ptr;
    uint16_t word4_hi_ptr;
    uint16_t entry_end_ptr;
    uint16_t course_rom_bank;

    fx_spill(stack);
    course_rom_bank = fx_ldw(0x1094);
    fx_romb(course_rom_bank);
    fx_r14(entry_address);
    word1_lo = fx_getb();
    word1_hi_ptr = (uint16_t)(entry_address + 1);
    fx_r14(word1_hi_ptr);
    words_1_2 = (uint16_t)((word1_lo & 0xFF) | fx_getb() << 8);
    word2_ptr = (uint16_t)(word1_hi_ptr + 1);
    fx_r14(word2_ptr);
    word2_lo = fx_getb();
    word2_hi_ptr = (uint16_t)(word2_ptr + 1);
    fx_r14(word2_hi_ptr);
    words_2_2 = (uint16_t)((word2_lo & 0xFF) | fx_getb() << 8);
    word3_ptr = (uint16_t)(word2_hi_ptr + 1);
    fx_r14(word3_ptr);
    word3_lo = fx_getb();
    word3_hi_ptr = (uint16_t)(word3_ptr + 1);
    fx_r14(word3_hi_ptr);
    words_3_2 = (uint16_t)((word3_lo & 0xFF) | fx_getb() << 8);
    word4_ptr = (uint16_t)(word3_hi_ptr + 1);
    fx_r14(word4_ptr);
    word4_lo = fx_getb();
    word4_hi_ptr = (uint16_t)(word4_ptr + 1);
    fx_r14(word4_hi_ptr);
    words_4_2 = (uint16_t)((word4_lo & 0xFF) | fx_getb() << 8);
    entry_end_ptr = (uint16_t)(word4_hi_ptr + 1);
    fx_r14(entry_end_ptr);
    fx_unspill(stack);
    return (FxCourseEntryReadResult){words_1_2, words_2_2, words_3_2, words_4_2, entry_end_ptr};
}


/*
 * @routine 04:CDCA FxLapSegment
 * WHAT IT DOES: Counts car R8 through its current segment: when +$58 has
 * moved on from +$86 by 1 to 4 segments, or has wrapped from the last
 * segment (+$86 = the table's count) to a lower one, +$86 takes it; when
 * the segment counted is 1, the lap count +$7A goes up by one.
 * HOW: Returns early when the two are equal or the table address is 0. The
 * count is the table's first word (GETB, GETBH with ROMBR = the table
 * bank); the step forward is +$58 - +$86 compared with 5 by sign.
 * Entry: R8 = the car's record; R11 = return address. Exit: R3 = +$58; R1
 * = the old +$86; when they differed and the table is there: R14 = table +
 * 1, R4 = the count, R2 = 5 or 1; R0 and the flags from the last compare,
 * or from the INC of the laps. The last RAM address is the last field or
 * table word read or written.
 */
FxLapSegmentResult FxLapSegment(uint16_t car_record, uint16_t rom_at)
{
    uint16_t laps;
    uint16_t counted;
    uint16_t segment;
    uint16_t last_segment;
    uint16_t course_rom_bank;
    uint16_t last_segment_lo;
    uint16_t segment_advance;
    uint16_t course_table_ptr;

    segment = fx_ldw((uint16_t)(FXREC_SEGMENT + car_record));
    counted = fx_ldw((uint16_t)(FXREC_COUNTED + car_record));
    if (segment == counted) {
    } else {
        course_rom_bank = fx_ldw(0x1094);
        fx_romb(course_rom_bank);
        course_table_ptr = fx_ldw(0x1092);
        rom_at = course_table_ptr;
        fx_r14(rom_at);
        if (course_table_ptr == 0) {
        } else {
            last_segment_lo = fx_getb();
            rom_at = (uint16_t)(rom_at + 1);
            fx_r14(rom_at);
            last_segment = (uint16_t)((last_segment_lo & 0xFF) | fx_getb() << 8);
            segment_advance = (uint16_t)(segment - counted);
            if (!(segment_advance & 0x8000)) {
                if (!((uint16_t)(segment_advance - 5) & 0x8000)) {
                } else {
                    fx_stw((uint16_t)(FXREC_COUNTED + car_record), segment);
                    if (segment != 1) {
                    } else {
                        laps = fx_ldw((uint16_t)(FXREC_LAPS + car_record));
                        fx_sbk((uint16_t)(laps + 1));
                    }
                }
            } else {
                if (counted == last_segment) {
                    fx_stw((uint16_t)(FXREC_COUNTED + car_record), segment);
                    if (segment != 1) {
                    } else {
                        laps = fx_ldw((uint16_t)(FXREC_LAPS + car_record));
                        fx_sbk((uint16_t)(laps + 1));
                    }
                } else {
                }
            }
        }
    }
    return (FxLapSegmentResult){segment, rom_at};
}


/*
 * @routine 04:CED6 FxCourseNextDistance
 * WHAT IT DOES: Finds the segment after entry R0 for car R8 (FxTableEntryWords,
 * FxCourseNextEntry) and returns how far the car is from it: R0 = |car x
 * (+$1E) - R1| / 4 + |car z (+$22) - R3| / 4.
 * HOW: The return address is kept on the R10 stack; each difference is made
 * positive with NOT and INC and halved twice with DIV2.
 * Entry: R0 = entry; R8 = the car's record; R10 = stack; R11 = return
 * address. Exit: R0 = the distance; R4 = the x part; R1-R3, R5, R6 as
 * FxCourseNextEntry leaves them; R11 = $CEE4; OV, CY from the final add; S,
 * Z from R10; the last RAM address R10.
 */
FxCourseNextDistanceResult FxCourseNextDistance(uint16_t entry, uint16_t entry_words_z, uint16_t car_record, uint16_t *stack, uint16_t rom_at)
{
    uint16_t car_z;
    uint16_t dx_abs;
    uint16_t dz_abs;
    uint16_t half_x;
    uint16_t address;
    uint16_t words_1;
    uint16_t words_3;
    uint16_t dx_signed;
    uint16_t dz_signed;
    uint16_t entry_out;
    uint16_t entry_out_2;
    uint16_t rom_at_out_2;
    uint16_t dx_abs_quarter;

    fx_spill(stack);
    { FxTableEntryWordsResult r_ = FxTableEntryWords(entry); entry_out = r_.entry_out; address = r_.address; };
    { FxCourseNextEntryResult r_ = FxCourseNextEntry(entry, entry_words_z, entry_out, address, car_record, rom_at); words_1 = r_.words_1; words_3 = r_.words_3; entry_out_2 = r_.entry_out; rom_at_out_2 = r_.rom_at_out; };
    half_x = fx_ldw((uint16_t)(car_record + FXREC_HALF_X));
    dx_signed = (uint16_t)(half_x - words_1);
    dx_abs = dx_signed;
    if (!(dx_signed & 0x8000)) {
    } else {
        dx_abs = (uint16_t)((uint16_t)~dx_abs + 1);
    }
    dx_abs_quarter = fx_div2(fx_div2(dx_abs));
    car_z = fx_ldw((uint16_t)(car_record + 0x0022));
    dz_signed = (uint16_t)(car_z - words_3);
    dz_abs = dz_signed;
    if (!(dz_signed & 0x8000)) {
    } else {
        dz_abs = (uint16_t)((uint16_t)~dz_abs + 1);
    }
    fx_unspill(stack);
    return (FxCourseNextDistanceResult){(uint16_t)(fx_div2(fx_div2(dz_abs)) + dx_abs_quarter), dx_abs_quarter, entry_out_2, rom_at_out_2};
}


/*
 * @routine 04:C595 FxCarCatchUp
 * WHAT IT DOES: The rubber band for car R8: picks a pair of words from the
 * table for $26F4 (at $04:C4D1, pointing into bank 4) by how far the car
 * is ahead of the player (+$88 against the player's, $2476). The car's
 * count +$72 goes up by one; when it reaches the first word (or wraps
 * from $FFFF to 0) it restarts at 0 and the car is moved on to its next
 * segment entry (FxTableEntryWords, FxCourseNextEntry, into +$58) and put
 * there (FxCarPlaceOnSegment).
 * Then its position +$1E/+$20/+$22 gets its velocity +$2E/+$30/+$32
 * times the second word (FMULT) added, and every object linked from +$3E
 * (through +$34) is put at the same position.
 * HOW: ROM bank 4 for the GETBs; the second word is kept on the R10 stack
 * with the return address. The 0 entry jump is not reached in the traces
 * so far.
 * Entry: R8 = the car's record; R10 = stack; R11 = return address. Exit:
 * R1-R3 the new position; R6 = 0 (the end of the links); R14 past the
 * pair; S, Z from R10; the last RAM address R10.
 */
FxCarCatchUpResult FxCarCatchUp(uint16_t prior_word4, uint16_t entry, uint16_t segment_scratch_a, uint16_t car_record, uint16_t segment_scratch_b, uint16_t *stack)
{
    uint16_t y;
    uint16_t rank;
    uint16_t car_z;
    uint16_t parts;
    uint16_t half_x;
    uint16_t rank_2;
    uint16_t address;
    uint16_t segment;
    uint16_t number_1;
    uint16_t rank_gap;
    uint16_t words_4_2;
    uint16_t rom_at_out;
    uint16_t velocity_x;
    uint16_t velocity_y;
    uint16_t velocity_z;
    uint16_t drive_level;
    uint16_t entry_out_2;
    uint16_t entry_out_3;
    uint16_t band_ptr_low;
    uint16_t rom_at_out_2;
    uint16_t catchup_count;
    uint16_t player_record;
    uint16_t restart_limit;
    uint16_t band_pair_addr;
    uint16_t band_rom_ptr_1;
    uint16_t band_rom_ptr_2;
    uint16_t band_rom_ptr_3;
    uint16_t position_scale;
    uint16_t band_table_addr;
    uint16_t drive_level_low;
    uint16_t linked_part_ptr;
    uint16_t rank_gap_doubled;
    uint16_t restart_limit_low;
    uint16_t next_catchup_count;
    uint16_t position_scale_low;
    uint16_t band_pair_table_ptr;
    uint16_t restored_position_scale;

    fx_spill(stack);
    fx_romb(4);
    fx_r14(0xC4D1);
    drive_level = fx_ldw(FX_DRIVE_LEVEL);
    drive_level_low = (uint16_t)(drive_level & 0xFF);
    band_table_addr = (uint16_t)((uint16_t)(drive_level_low + drive_level_low) - 0x3B2F);
    fx_r14(band_table_addr);
    band_ptr_low = fx_getb();
    fx_r14((uint16_t)(band_table_addr + 1));
    band_pair_table_ptr = (uint16_t)((band_ptr_low & 0xFF) | fx_getb() << 8);
    fx_r14(band_pair_table_ptr);
    player_record = fx_ldw(0x2476);
    rank = fx_ldw((uint16_t)(player_record + FXREC_RANK));
    rank_2 = fx_ldw((uint16_t)(car_record + FXREC_RANK));
    rank_gap = (uint16_t)(rank_2 - rank);
    rank_gap_doubled = (uint16_t)(rank_gap + rank_gap);
    band_pair_addr = (uint16_t)((uint16_t)(band_pair_table_ptr + rank_gap_doubled) + rank_gap_doubled);
    fx_r14(band_pair_addr);
    restart_limit_low = fx_getb();
    band_rom_ptr_1 = (uint16_t)(band_pair_addr + 1);
    fx_r14(band_rom_ptr_1);
    restart_limit = (uint16_t)((restart_limit_low & 0xFF) | fx_getb() << 8);
    band_rom_ptr_2 = (uint16_t)(band_rom_ptr_1 + 1);
    fx_r14(band_rom_ptr_2);
    position_scale_low = fx_getb();
    band_rom_ptr_3 = (uint16_t)(band_rom_ptr_2 + 1);
    fx_r14(band_rom_ptr_3);
    position_scale = (uint16_t)((position_scale_low & 0xFF) | fx_getb() << 8);
    rom_at_out = (uint16_t)(band_rom_ptr_3 + 1);
    fx_r14(rom_at_out);
    fx_push(stack, position_scale);
    catchup_count = fx_ldw((uint16_t)(car_record + 0x0072));
    next_catchup_count = (uint16_t)(catchup_count + 1);
    fx_sbk(next_catchup_count);
    if (next_catchup_count == 0) {
        segment = fx_ldw((uint16_t)(car_record + FXREC_SEGMENT));
        if (segment != 0) {
            { FxTableEntryWordsResult r_ = FxTableEntryWords(segment); entry_out_2 = r_.entry_out; address = r_.address; };
            { FxCourseNextEntryResult r_ = FxCourseNextEntry(segment, restart_limit, entry_out_2, address, car_record, rom_at_out); number_1 = r_.number_1; words_4_2 = r_.words_4; entry_out_3 = r_.entry_out; rom_at_out_2 = r_.rom_at_out; };
            fx_stw((uint16_t)(car_record + FXREC_SEGMENT), number_1);
            { FxCarPlaceOnSegmentResult r_ = FxCarPlaceOnSegment(words_4_2, entry_out_3, segment_scratch_a, car_record, segment_scratch_b, stack, rom_at_out_2); prior_word4 = r_.angle_sign_xor; entry = r_.swap_mark_out; segment_scratch_a = r_.last_next_rec; segment_scratch_b = r_.last_size_addr; rom_at_out = r_.rom_at_out; };
        } else {
        }
    } else {
        if (!((uint16_t)(next_catchup_count - restart_limit) & 0x8000)) {
            fx_sbk(0);
            segment = fx_ldw((uint16_t)(car_record + FXREC_SEGMENT));
            if (segment != 0) {
                { FxTableEntryWordsResult r_ = FxTableEntryWords(segment); entry_out_2 = r_.entry_out; address = r_.address; };
                { FxCourseNextEntryResult r_ = FxCourseNextEntry(segment, restart_limit, entry_out_2, address, car_record, rom_at_out); number_1 = r_.number_1; words_4_2 = r_.words_4; entry_out_3 = r_.entry_out; rom_at_out_2 = r_.rom_at_out; };
                fx_stw((uint16_t)(car_record + FXREC_SEGMENT), number_1);
                { FxCarPlaceOnSegmentResult r_ = FxCarPlaceOnSegment(words_4_2, entry_out_3, segment_scratch_a, car_record, segment_scratch_b, stack, rom_at_out_2); prior_word4 = r_.angle_sign_xor; entry = r_.swap_mark_out; segment_scratch_a = r_.last_next_rec; segment_scratch_b = r_.last_size_addr; rom_at_out = r_.rom_at_out; };
            } else {
            }
        } else {
        }
    }
    restored_position_scale = fx_pull(stack);
    velocity_x = fx_ldw((uint16_t)(car_record + 0x002E));
    half_x = fx_ldw((uint16_t)(car_record + FXREC_HALF_X));
    fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)velocity_x * (int16_t)restored_position_scale) >> 16) + half_x));
    velocity_y = fx_ldw((uint16_t)(car_record + 0x0030));
    y = fx_ldw((uint16_t)(car_record + FXREC_Y));
    fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)velocity_y * (int16_t)restored_position_scale) >> 16) + y));
    velocity_z = fx_ldw((uint16_t)(car_record + 0x0032));
    car_z = fx_ldw((uint16_t)(car_record + 0x0022));
    fx_sbk((uint16_t)((uint16_t)((uint32_t)((int32_t)(int16_t)velocity_z * (int16_t)restored_position_scale) >> 16) + car_z));
    parts = fx_ldw((uint16_t)(car_record + FXCAR_PARTS));
    for (;;) {
        linked_part_ptr = parts;
        if (linked_part_ptr == 0) {
            break;
        } else {
            fx_stw((uint16_t)(linked_part_ptr + FXREC_HALF_X), half_x);
            fx_stw((uint16_t)(linked_part_ptr + FXREC_Y), y);
            fx_stw((uint16_t)(linked_part_ptr + 0x0022), car_z);
            parts = fx_ldw((uint16_t)(linked_part_ptr + 0x0034));
            continue;
        }
    }
    fx_unspill(stack);
    return (FxCarCatchUpResult){prior_word4, entry, segment_scratch_a, segment_scratch_b, rom_at_out};
}


/*
 * @routine 04:C65A FxCarPlaceOnSegment
 * WHAT IT DOES: Puts car R8 back on its segment entry +$58 (unless that is
 * 0): reads the entry (FxTableEntryWords, FxCourseEntryRead), keeps its
 * point at $26F6/$26F8/$26FA with y lowered by $A0, sets the car's
 * velocity +$2E/+$30/+$32 to point minus position (+$1E/+$20/+$22), clears
 * +$26, +$28, +$36, +$34 and +$38, steps to the next entry
 * (FxCourseNextEntry) and turns the car to face it (FxBearingTo into
 * +$24); then, for each object linked from +$3E (through +$34) and for
 * the car itself, when its word +$18 is set, FxListClearBit0 and
 * FxListSweep.
 * HOW: The return address, the entry number and R8 are kept on the R10
 * stack. The 0 entry exit and the sweep inside the loop are not reached in
 * the traces so far.
 * Entry: R8 = the car's record; R10 = stack; R11 = return address. Exit:
 * R8 restored; R6 = the car; R11 = $C726 or as before the last sweep; S, Z
 * from R10; the last RAM address R10.
 */
FxCarPlaceOnSegmentResult FxCarPlaceOnSegment(uint16_t prior_word4, uint16_t swap_mark, uint16_t in_scratch_7, uint16_t car_record, uint16_t in_scratch_9, uint16_t *stack, uint16_t rom_at)
{
    uint16_t y;
    uint16_t list;
    uint16_t angle;
    uint16_t car_z;
    uint16_t parts;
    uint16_t half_x;
    uint16_t list_2;
    uint16_t number;
    uint16_t address;
    uint16_t segment;
    uint16_t words_1;
    uint16_t words_2;
    uint16_t words_3;
    uint16_t rom_at_2;
    uint16_t car_vel_z;
    uint16_t entry_out;
    uint16_t words_1_2;
    uint16_t words_3_2;
    uint16_t part_record;
    uint16_t record_holding;
    uint16_t entry_point_y_lowered;

    fx_spill(stack);
    segment = fx_ldw((uint16_t)(car_record + FXREC_SEGMENT));
    if (segment != 0) {
        { FxTableEntryWordsResult r_ = FxTableEntryWords(segment); address = r_.address; };
        { FxCourseEntryReadResult r_ = FxCourseEntryRead(address, stack); words_1 = r_.words_1; words_2 = r_.words_2; words_3 = r_.words_3; rom_at_2 = r_.rom_at; };
        fx_push(stack, segment);
        entry_point_y_lowered = (uint16_t)(words_2 - 0x00A0);
        fx_stw(0x26F6, words_1);
        fx_stw(FX_HAZE_X8, entry_point_y_lowered);
        fx_stw(FX_ME_Z, words_3);
        half_x = fx_ldw((uint16_t)(car_record + FXREC_HALF_X));
        fx_stw((uint16_t)(car_record + 0x002E), (uint16_t)((uint16_t)~(uint16_t)(half_x - words_1) + 1));
        y = fx_ldw((uint16_t)(car_record + FXREC_Y));
        fx_stw((uint16_t)(car_record + 0x0030), (uint16_t)((uint16_t)~(uint16_t)(y - entry_point_y_lowered) + 1));
        car_z = fx_ldw((uint16_t)(car_record + 0x0022));
        car_vel_z = (uint16_t)((uint16_t)~(uint16_t)(car_z - words_3) + 1);
        fx_stw((uint16_t)(car_record + 0x0032), car_vel_z);
        fx_stw((uint16_t)(car_record + 0x0026), 0);
        fx_stw((uint16_t)(car_record + FXCAR_ROLL), 0);
        fx_stw((uint16_t)(car_record + FXPART_WORLD), 0);
        fx_stw((uint16_t)(car_record + 0x0034), 0);
        fx_stw((uint16_t)(car_record + 0x0038), 0);
        number = fx_pull(stack);
        { FxCourseNextEntryResult r_ = FxCourseNextEntry(number, words_3, car_vel_z, address, car_record, rom_at_2); words_1_2 = r_.words_1; words_3_2 = r_.words_3; entry_out = r_.entry_out; rom_at = r_.rom_at_out; };
        { FxBearingToResult r_ = FxBearingTo(words_1_2, words_3_2, entry_out, car_record, stack, rom_at); angle = r_.angle; prior_word4 = r_.angle_sign_xor; swap_mark = r_.swap_mark_out; };
        fx_stw((uint16_t)(car_record + FXCAR_PITCH), angle);
        fx_push(stack, car_record);
        parts = fx_ldw((uint16_t)(car_record + FXCAR_PARTS));
        for (;;) {
            part_record = parts;
            if (part_record == 0) {
                break;
            } else {
                list = fx_ldw((uint16_t)(part_record + FXREC_LIST));
                if (list == 0) {
                } else {
                    FxListClearBit0(part_record);
                    { FxListSweepResult r_ = FxListSweep(part_record, in_scratch_7, in_scratch_9, stack); in_scratch_7 = r_.last_next_rec; in_scratch_9 = r_.last_size_addr; };
                }
                parts = fx_ldw((uint16_t)(part_record + 0x0034));
                continue;
            }
        }
        record_holding = fx_pull(stack);
        fx_push(stack, record_holding);
        list_2 = fx_ldw((uint16_t)(record_holding + FXREC_LIST));
        if (list_2 == 0) {
        } else {
            FxListClearBit0(record_holding);
            { FxListSweepResult r_ = FxListSweep(record_holding, in_scratch_7, in_scratch_9, stack); in_scratch_7 = r_.last_next_rec; in_scratch_9 = r_.last_size_addr; };
        }
        fx_unspill(stack);
    } else {
    }
    fx_unspill(stack);
    return (FxCarPlaceOnSegmentResult){prior_word4, swap_mark, in_scratch_7, in_scratch_9, rom_at};
}


/*
 * @routine 04:C72F FxCarVelocityFromSegment
 * WHAT IT DOES: Takes the saved R11 back off the R10 stack and, unless
 * car R8's word +$58 (its entry) is 0, sets the car's velocity
 * +$2E/+$30/+$32 to a vector along its heading +$24 whose length is
 * twice word 4 of the entry (FxTableEntryWords, FxCourseEntryRead)
 * times the factor for $26F4 from the table at $04:C781 (FMULT). It
 * aims a fallen car along the course.
 * HOW: Entered by LINK from 04:C451 (FxCarFollowCourse), but it never
 * returns to that R11: it pulls the R11 04:C3D5 saved and returns
 * through the word under it, two levels up, into its caller's caller.
 * Its C is the physics program's from here, to that return (the jump
 * with the R10 stack two words below its entry, where the checks end
 * such a call too). FxAngleVector makes the vector.
 * Entry: R8 = the car's record; R10 = stack (FxCarFollowCourse's saved
 * R11 on top of the return address).
 * Exit: R11 = $C76D (or the saved R11 for a 0 entry); R1-R3 the vector;
 * R10 two words below its entry; S, Z from R10; the last RAM address
 * R10.
 */
FxCarVelocityFromSegmentResult FxCarVelocityFromSegment(uint16_t in_scratch_4, uint16_t in_scratch_5, uint16_t last_scan_value, uint16_t car_record, uint16_t last_scan_aux, uint16_t *stack, uint16_t rom_at)
{
    uint16_t pitch;
    uint16_t segment;
    uint16_t words_4;
    uint16_t vector_x;
    uint16_t vector_y;
    uint16_t vector_z;
    uint16_t drive_level;
    uint16_t drive_factor;
    uint16_t drive_factor_lo;
    uint16_t drive_factor_ptr;
    uint16_t course_followed_2;
    uint16_t drive_level_index;

    fx_unspill(stack);
    segment = fx_ldw((uint16_t)(car_record + FXREC_SEGMENT));
    course_followed_2 = segment;
    if (segment != 0) {
        { FxTableEntryWordsResult r_ = FxTableEntryWords(course_followed_2); in_scratch_5 = r_.address; };
        { FxCourseEntryReadResult r_ = FxCourseEntryRead(in_scratch_5, stack); words_4 = r_.words_4; };
        fx_romb(4);
        drive_level = fx_ldw(FX_DRIVE_LEVEL);
        drive_level_index = (uint16_t)(drive_level & 0xFF);
        fx_r14(0xC781);
        drive_factor_ptr = (uint16_t)((uint16_t)(drive_level_index + drive_level_index) - 0x387F);
        fx_r14(drive_factor_ptr);
        drive_factor_lo = fx_getb();
        fx_r14((uint16_t)(drive_factor_ptr + 1));
        drive_factor = (uint16_t)((drive_factor_lo & 0xFF) | fx_getb() << 8);
        in_scratch_4 = (uint16_t)((uint32_t)((int32_t)(int16_t)(uint16_t)(words_4 + words_4) * (int16_t)drive_factor) >> 16);
        pitch = fx_ldw((uint16_t)(car_record + FXCAR_PITCH));
        { FxAngleVectorResult r_ = FxAngleVector(pitch, in_scratch_4); vector_x = r_.vector_x; vector_y = r_.vector_y; vector_z = r_.vector_z; rom_at = r_.rom_at; };
        fx_stw((uint16_t)(car_record + 0x002E), vector_x);
        fx_stw((uint16_t)(car_record + 0x0030), vector_y);
        course_followed_2 = (uint16_t)(car_record + 0x0032);
        fx_stw(course_followed_2, vector_z);
    } else {
    }
    fx_unspill(stack);
    return (FxCarVelocityFromSegmentResult){course_followed_2, in_scratch_4, in_scratch_5, last_scan_value, car_record, last_scan_aux, rom_at, 2};
}

