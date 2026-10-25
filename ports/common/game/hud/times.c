/*
 * times.c: race times as the HUD and the records keep them.
 *
 * A time is kept two ways: as BCD bytes (hundredths, seconds, minutes) while
 * racing and in the record tables, and packed in a word for the save:
 * hundredths in bits 0-6, seconds in bits 7-12, minutes in bits 13-15 (so
 * only minutes 0-7 survive packing).
 */
#include <string.h>
#include "game.h"

uint16_t time_pack(BcdTime t)
{
    return (uint16_t)((bcd_to_binary(t.hund) & 0x7F) | (bcd_to_binary(t.sec) << 7 & 0x1F80)
                      | (bcd_to_binary(t.min) & 7) << 13);
}

BcdTime time_unpack(uint16_t packed)
{
    BcdTime t;

    if (packed == 0xFFFF) {
        t.hund = t.sec = t.min = 0xFF;
        return t;
    }
    t.hund = (uint8_t)binary_to_bcd(packed & 0x7F);
    t.sec = (uint8_t)binary_to_bcd(packed >> 7 & 0x3F);
    t.min = (uint8_t)binary_to_bcd(packed >> 13);
    return t;
}

/* `to` += t in decimal, unless t is no time ($FF hundredths). The seconds
   take the hundredths' carry; past 99 they lose $60 and carry a minute,
   and from 60 they lose $60 again and carry another (as the original does
   it, so a sum past 159 seconds is not corrected further). */
void time_add(const BcdTime *t, BcdTime *to)
{
    bool c = false;
    uint8_t s;

    if (t->hund == 0xFF)
        return;
    to->hund = bcd_add8(t->hund, to->hund, &c);
    s = bcd_add8(t->sec, to->sec, &c);
    if (c) {
        bool one = false;

        s = bcd_sub8(s, 0x60, &c);
        to->min = bcd_add8(to->min, 0x01, &one);
    }
    to->sec = s;
    if (s >= 0x60) {
        bool one = false;

        c = true;
        to->sec = bcd_sub8(s, 0x60, &c);
        to->min = bcd_add8(to->min, 0x01, &one);
    }
    c = false;
    to->min = bcd_add8(t->min, to->min, &c);
}

/* Each group of four record times summed into its total; a group whose
   last time is none totals none. */
void time_totals_sum(void)
{
    int g, i;

    for (g = 0; g < 3; g++) {
        BcdTime sum = {0, 0, 0};

        for (i = 0; i < 4; i++)
            time_add(&record_times[g][i], &sum);
        if (record_times[g][0].min == 0xFF)
            sum.hund = sum.sec = sum.min = 0xFF;
        record_totals[g] = sum;
    }
}

/* Each group's first three times added into the time after each. */
void time_running_totals(void)
{
    int g, i;

    for (g = 0; g < 3; g++)
        for (i = 0; i < 3; i++)
            time_add(&record_times[g][i], &record_times[g][i + 1]);
}

/* Whether the race time is below place `place`'s record (the running total
   of its group: the group's fourth time), minutes first. */
bool time_below_record(int place)
{
    const BcdTime *r = &record_times[place][3];

    if (race_time.min != r->min)
        return race_time.min < r->min;
    if (race_time.sec != r->sec)
        return race_time.sec < r->sec;
    return race_time.hund < r->hund;
}

/* The checksum of class c's records: its times' bytes and its names'
   bytes, summed in a byte. */
static uint8_t records_sum(int c)
{
    const uint8_t *t = (const uint8_t *)save_records.times[c], *n = (const uint8_t *)save_records.names[c];
    uint8_t sum = 0;
    unsigned i;

    for (i = 0; i < sizeof save_records.times[c]; i++)
        sum = (uint8_t)(sum + t[i]);
    for (i = 0; i < sizeof save_records.names[c]; i++)
        sum = (uint8_t)(sum + n[i]);
    return sum;
}

/* The race's place in the class's records: below a place's record it takes
   that place, the places after it moving down a place (name and times),
   with the entered name and the car; its lap times go to the place packed
   and the class's checksum is remade. No place: player_tag $FF and
   record_place 0. Not for wram_0D24 = 3. */
void record_time_insert(void)
{
    int c = wram_0D4A, place, i;

    if (wram_0D24 == 3)
        return;
    for (place = 0; place < 3 && !time_below_record(place); place++)
        ;
    if (place == 3) {
        player_tag = 0xFF;
        record_place = 0;
        return;
    }
    for (i = 2; i > place; i--) {
        save_records.names[c][i] = save_records.names[c][i - 1];
        memcpy(save_records.times[c][i], save_records.times[c][i - 1], sizeof save_records.times[c][i]);
    }
    save_records.names[c][place].name[0] = entered_name[0];
    save_records.names[c][place].name[1] = entered_name[1];
    save_records.names[c][place].car = wram_0D89;
    record_place = (uint8_t)(place + 1);
    for (i = 0; i < 4; i++)
        save_records.times[c][place][i] = time_pack(lap_times[i]);
    save_table_sums[c] = records_sum(c);
}
