/*
 * player_stats.c: the race's player statistics (the speed sample and the
 * totals kept from it) and the phases of the race that run on the phase
 * word $0DD6 (the Stunt Trax panel, the time-up phase's first steps).
 */
#include "game.h"
#include "glue.h"
#include "gsu_record.h"
#include "player_stats.h"
#include "smallvideo.h"

/* A speed sample for player `player`: |speed| * $3C / 256, kept to $FF,
   stored at $0DDA + player. The player's total is a 32-bit sum at $0DDC
   (player 0) or $0DE0 (player 1), its count at $0DE4 or $0DE6. */
uint16_t speed_sample_add(uint16_t player, uint16_t speed)
{
    uint16_t mag = speed >= 0x8000 ? (uint16_t)(0x10000u - speed) : speed;
    uint32_t product = (uint32_t)mag * 0x3C;
    uint16_t m = (uint16_t)(product >> 8);
    uint8_t v = m < 0x00FF ? (uint8_t)m : 0xFF;
    uint16_t sum = player == 0 ? 0x0DDC : 0x0DE0;
    uint16_t cnt = player == 0 ? 0x0DE4 : 0x0DE6;
    uint32_t s;
    uint16_t lo, hi;
    uint32_t h;

    W8(0x0DDA + player) = v;
    s = (uint32_t)v + W16(sum);
    lo = (uint16_t)s;
    W16(sum) = lo;
    hi = W16(sum + 2);
    h = (uint32_t)hi + (s >> 16);
    if (h > 0xFFFF) {
        /* the total is kept at its limit; player 1's high word is not
           touched (its low word is set twice, as the original does) */
        W16(sum) = 0xFFFF;
        W16(player == 0 ? sum + 2 : sum) = 0xFFFF;
        return 0xFFFF;
    }
    W16(sum + 2) = (uint16_t)h;
    W16(cnt) = (uint16_t)(W16(cnt) + 1);
    return (uint16_t)h;
}

/* A byte and a word of a car's record (in the game pak RAM) at `off`. */
static uint8_t rec_byte(uint16_t rec, uint16_t off)
{
    return P8((uint16_t)(rec + off));
}

static uint16_t rec_word(uint16_t rec, uint16_t off)
{
    return P16((uint16_t)(rec + off));
}

/* The figures of player `player` from its car's record at `rec`, unless $0E57
   is set: the record's bytes at $40-$43 and $4E go to the player's slots at
   $0DEC, $0DEE, $0DF0, $0DF2 and $0DF6; the record's speed (+$4A) is sampled;
   the record's +$82 / 8 is the gauge's value $0DF9 and its position (kept to
   $3F, then 2n + $30) $0DFB; the record's +$6A / 8 goes to $0E05, +$7A to
   $0DE8 and the low nibble of +$88 to $0DF4. The record's bits $40 are kept
   (record_bits_60_pair) in every case. */
void player_stats_from_record(uint16_t player, uint16_t rec)
{
    uint8_t v;

    if (!W8(0x0E57)) {
        W8(0x0DEC + player) = rec_byte(rec, 0x40);
        W8(0x0DEE + player) = rec_byte(rec, 0x41);
        W8(0x0DF0 + player) = rec_byte(rec, 0x42);
        W8(0x0DF2 + player) = rec_byte(rec, 0x43);
        W8(0x0DF6 + player) = rec_byte(rec, 0x4E);
        v = (uint8_t)(rec_word(rec, 0x82) >> 3);
        W8(0x0DF9 + player) = v;
        if (v >= 0x3F)
            v = 0x3F;
        W8(0x0DFB + player) = (uint8_t)((v << 1) + 0x30);
        speed_sample_add(player, rec_word(rec, 0x4A));
        W8(0x0E05 + player) = (uint8_t)(rec_word(rec, 0x6A) >> 3);
        W8(0x0DE8 + player) = rec_byte(rec, 0x7A);
        W8(0x0DF4 + player) = rec_byte(rec, 0x88) & 0x0F;
    }
    record_bits_60_pair();
}

/* Player 0's figures from its record at $2476, then $0E5F = $FF. */
void player_stats_and_palette_animate(void)
{
    player_stats_from_record(0, P16(0x2476));
    W8(0x0E5F) = 0xFF;
}

/* The stunt courses ($0C-$0F, in $0E3D) each set their bit in the save's
   byte $FC06 (1, 2, 4, 8), and the save's checksum is made again. */
void stunt_course_mark(void)
{
    uint8_t bit;

    switch (W8(0x0E3D)) {
    case 0x0C: bit = 0x01; break;
    case 0x0D: bit = 0x02; break;
    case 0x0E: bit = 0x04; break;
    case 0x0F: bit = 0x08; break;
    default: return;
    }
    P8(0xFC06) = (uint8_t)(P8(0xFC06) | bit);
    save_checksum();
}

/* The panel's line for the count: the byte at $07:E115 + count. */
static uint16_t stunt_count_row(void)
{
    return *rom_at(0x07E115u + W8(0x0E3F));
}

/* The count goes down a step; at 0 the panel's state $0E41 is cleared. */
static void stunt_count_down(void)
{
    uint8_t v = (uint8_t)(W8(0x0E3F) - 1);

    W8(0x0E3F) = v;
    if (!v)
        W8(0x0E41) = 0x00;
}

/* Stunt Trax panel state 2: two lines of glyphs for the count, then the
   count goes down. */
void stunt_panel_count(void)
{
    gsu_glyphs_two_lines(stunt_count_row());
    stunt_count_down();
}

/* Stunt Trax panel state 4: the line at y $28 for the count, then the count
   goes down. */
void stunt_panel_show(void)
{
    gsu_glyph_rows_at_32(stunt_count_row());
    stunt_count_down();
}

/* Time-up phase 0: the first two glyph sprites placed ($0FB0 = $00DA, $0FB2
   = $FF9E, $0FE0 = $0014, $0FE2 = $0040), and the phase word $0DD6 stepped
   twice. */
void time_up_phase_start(void)
{
    W16(0x0FB0) = 0x00DA;
    W16(0x0FE0) = 0x0014;
    W16(0x0FB2) = 0xFF9E;
    W16(0x0FE2) = 0x0040;
    W16(0x0DD6) = (uint16_t)(W16(0x0DD6) + 2);
}

/* Time-up phase 2: the glyph sprites slide in. */
void time_up_phase_slide_in(void)
{
    glyph_sprites_slide_in();
}

/* Time-up phase 4: the glyph sprites slide out. */
void time_up_phase_slide_out(void)
{
    glyph_sprites_slide_out();
}
