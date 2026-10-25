/*
 * fx_course.c: the course's segment table as the Super FX reads it, and
 * the laps it counts from it.
 *
 * The table is in ROM: its bank is the byte at $1094 and its address the
 * word at $1092; its first word is the number of segments, 8-byte entries
 * follow (x, y, z and a word). A car's record keeps its segment at +$58,
 * the last segment counted at +$86 and its laps at +$7A.
 */
#include "game.h"
#include "render.h"
#include "physics.h"

#define fx_table            FXU(0x1092)

static uint16_t tw(uint16_t at)
{
    uint8_t bank = pakram[0x1094] & 0x7F;

    return (uint16_t)(rom_at((uint32_t)bank << 16 | at)[0] | rom_at((uint32_t)bank << 16 | (uint16_t)(at + 1))[0] << 8);
}

/* the four words of the entry at `at` */
void fx_course_entry_read(uint16_t at, uint16_t w[4])
{
    int i;

    for (i = 0; i < 4; i++)
        w[i] = tw((uint16_t)(at + 2 * i));
}

/* The car counted through its segment: when +$58 is 1 to 4 on from +$86,
   or has wrapped from the last segment, +$86 takes it, and the laps go up
   at segment 1. */
void fx_lap_segment(uint16_t car)
{
    uint16_t seg = FXU(car + 0x58), counted = FXU(car + 0x86);
    int16_t step;

    if (seg == counted || !fx_table)
        return;
    step = wrap16(seg - counted);
    if (step < 0 ? counted != tw(fx_table) : wrap16(step - 5) >= 0)
        return;
    FXU(car + 0x86) = seg;
    if (seg == 1)
        FXU(car + 0x7A)++;
}

/* The entry nearest (x, y, z) by |dx| + 4|dy| + |dz| (an entry whose sums
   overflow passed over): its number from 1, its address and first three
   words; 0 with no table or no entries (then *at and w are left). With no
   entry nearer than $7FFF the words are read from $FFFA. */
uint16_t fx_course_nearest(int16_t x, int16_t y, int16_t z, uint16_t *at, uint16_t w[3])
{
    uint16_t count, n, e, best = 0x7FFF, best_n = 0, best_at = 0;

    if (!fx_table || !(count = tw(fx_table)))
        return 0;
    for (n = 1, e = (uint16_t)(fx_table + 2); n <= count; n++, e = (uint16_t)(e + 8)) {
        int32_t d;
        int16_t s, dy;

        d = (int32_t)(int16_t)tw(e) - x;
        if (d != wrap16(d))
            continue;
        s = (int16_t)d < 0 ? wrap16(-d) : (int16_t)d;
        dy = wrap16((int16_t)tw((uint16_t)(e + 2)) - y);
        dy = wrap16((dy < 0 ? wrap16(-dy) : dy) * 4);
        d = (int32_t)s + dy;
        if (d != wrap16(d))
            continue;
        s = (int16_t)d;
        d = (int32_t)(int16_t)tw((uint16_t)(e + 4)) - z;
        if (d != wrap16(d))
            continue;
        d = (int32_t)((int16_t)d < 0 ? wrap16(-d) : (int16_t)d) + s;
        if (d != wrap16(d))
            continue;
        if ((uint16_t)d < best) {
            best = (uint16_t)d;
            best_at = (uint16_t)(e + 6);
            best_n = n;
        }
    }
    *at = (uint16_t)(best_at - 6);
    for (n = 0; n < 3; n++)
        w[n] = tw((uint16_t)(*at + 2 * n));
    return best_n;
}

/* One segment on from number n at `at` (from the last to the first), or
   back for a car going the wrong way (+$70 bit 1). */
uint16_t fx_course_next_entry(uint16_t car, uint16_t n, uint16_t *at, uint16_t w[4])
{
    uint16_t count = tw(fx_table);

    if (FXU(car + 0x70) & 2) {
        if (--n) {
            *at = (uint16_t)(*at - 8);
        } else {
            n = count;
            *at = (uint16_t)(fx_table + 2 + (uint16_t)(count - 1) * 8);
        }
    } else if (n == count) {
        n = 1;
        *at = (uint16_t)(fx_table + 2);
    } else {
        n++;
        *at = (uint16_t)(*at + 8);
    }
    fx_course_entry_read(*at, w);
    return n;
}

/* the bearing from the car to the segment after the one nearest it */
uint16_t fx_record_heading_ahead(uint16_t car)
{
    uint16_t at = 0, w[4] = {0}, n;

    n = fx_course_nearest(FXW(car + 0x1E), FXW(car + 0x20), FXW(car + 0x22), &at, w);
    fx_course_next_entry(car, n, &at, w);
    return fx_bearing_to(car, (int16_t)w[0], (int16_t)w[2]);
}

/* how far the car is from the segment after entry n: |dx| / 4 + |dz| / 4 */
int16_t fx_course_next_distance(uint16_t car, uint16_t n)
{
    uint16_t at = fx_table_entry_words(n), w[4];
    int16_t dx, dz;

    fx_course_next_entry(car, n, &at, w);
    dx = wrap16(FXW(car + 0x1E) - (int16_t)w[0]);
    dz = wrap16(FXW(car + 0x22) - (int16_t)w[2]);
    return wrap16(div2(div2(dz < 0 ? wrap16(-dz) : dz)) + div2(div2(dx < 0 ? wrap16(-dx) : dx)));
}

static void sweep_if_listed(uint16_t r)
{
    if (FXU(r + 0x18)) {
        fx_list_clear_bit0(r);
        fx_list_sweep(r);
    }
}

/* The car put back on its segment (+$58, unless 0): its point at $26F6
   ($A0 higher), its velocity to the point, its turns cleared, facing the
   next segment; then its parts' and its own contacts dropped. */
void fx_car_place_on_segment(uint16_t car)
{
    static const uint8_t zero[5] = {0x26, 0x28, 0x36, 0x34, 0x38};
    uint16_t n = FXU(car + 0x58), at, w[4], p;
    int i;

    if (!n)
        return;
    at = fx_table_entry_words(n);
    fx_course_entry_read(at, w);
    w[1] = (uint16_t)(w[1] - 0xA0);
    for (i = 0; i < 3; i++)
        FXU(0x26F6 + 2 * i) = w[i];
    for (i = 0; i < 3; i++)
        FXU(car + 0x2E + 2 * i) = (uint16_t)(w[i] - FXU(car + 0x1E + 2 * i));
    for (i = 0; i < 5; i++)
        FXU(car + zero[i]) = 0;
    fx_course_next_entry(car, n, &at, w);
    FXU(car + 0x24) = fx_bearing_to(car, (int16_t)w[0], (int16_t)w[2]);
    for (p = FXU(car + 0x3E); p; p = FXU(p + 0x34))
        sweep_if_listed(p);
    sweep_if_listed(car);
}

/* The rubber band: a pair of words (bank 4, from the table at $C4D1 by
   $26F4) by how far the car is ahead of the player (+$88): its count +$72
   up by one, and at the first word (or on wrapping) back to 0 with the car
   moved on to its next segment and put there; then its position moved by
   its velocity times the second word, and its parts put where it was. */
void fx_car_catch_up(uint16_t car)
{
    uint16_t pair = rom_word(0x040000u | (uint16_t)(0xC4D1 + 2 * pakram[0x26F4]));
    uint16_t player = FXU(0x2476), limit, scale, count, p;
    int16_t was[3];
    int i;

    pair = (uint16_t)(pair + (uint16_t)(FXU(car + 0x88) - FXU((uint16_t)(player + 0x88))) * 4);
    limit = (uint16_t)(rom_at(0x040000u | pair)[0] | rom_at(0x040000u | (uint16_t)(pair + 1))[0] << 8);
    scale = (uint16_t)(rom_at(0x040000u | (uint16_t)(pair + 2))[0] | rom_at(0x040000u | (uint16_t)(pair + 3))[0] << 8);
    count = (uint16_t)(FXU(car + 0x72) + 1);
    FXU(car + 0x72) = count;
    if (!count || wrap16(count - limit) >= 0) {
        if (count)
            FXU(car + 0x72) = 0;
        if (FXU(car + 0x58)) {
            uint16_t at = fx_table_entry_words(FXU(car + 0x58)), w[4];

            FXU(car + 0x58) = fx_course_next_entry(car, FXU(car + 0x58), &at, w);
            fx_car_place_on_segment(car);
        }
    }
    for (i = 0; i < 3; i++) {
        was[i] = FXW(car + 0x1E + 2 * i);
        FXW(car + 0x1E + 2 * i) = wrap16(fmult(FXW(car + 0x2E + 2 * i), (int16_t)scale) + was[i]);
    }
    for (p = FXU(car + 0x3E); p; p = FXU(p + 0x34))
        for (i = 0; i < 3; i++)
            FXW(p + 0x1E + 2 * i) = was[i];
}
