/*
 * player_stats.c: the race's player statistics (the speed sample and the
 * totals kept from it) and the phases of the race that run on the phase
 * word $0DD6 (the Stunt Trax panel, the time-up phase's first steps).
 */
#include "game.h"
#include "glue.h"
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
