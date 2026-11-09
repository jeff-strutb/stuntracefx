/*
 * lap_time.c: race times as the HUD and the records show them. A time is kept
 * packed in a word: hundredths in bits 0-6, seconds in bits 7-12, minutes in
 * bits 13-15; or as three BCD bytes: hundredths, seconds, minutes.
 */
#include <string.h>

#include "hud/hud.h"
#include "save/save.h"
#include "system/state.h"

/* @routine 07:ACD9 BinaryToBcd: a value below 10000 as four decimal digits. */
uint16_t BinaryToBcd(uint16_t v)
{
    uint16_t bcd = 0;

    while (v >= 1000) { v = (uint16_t)(v - 1000); bcd = (uint16_t)(bcd + 0x1000); }
    while (v >= 100) { v = (uint16_t)(v - 100); bcd = (uint16_t)(bcd + 0x0100); }
    while (v >= 10) { v = (uint16_t)(v - 10); bcd = (uint16_t)(bcd + 0x0010); }
    return (uint16_t)(bcd + v);
}

/* @routine 07:AC77 BcdToBinary */
static uint16_t BcdToBinary(uint16_t v)
{
    return (uint16_t)((v >> 12) * 1000 + ((v >> 8) & 0x0F) * 100 + ((v >> 4) & 0x0F) * 10 + (v & 0x0F));
}

/* @routine 07:AC46 TimeUnpackBcd, 07:AC42 TimeUnpackBcdLong: $FFFF, no time, gives $FFFF in all three. */
void TimeUnpackBcd(uint16_t packed, uint16_t *min, uint16_t *sec, uint16_t *hund)
{
    if (packed == 0xFFFF) {
        *min = *sec = *hund = 0xFFFF;
        return;
    }
    *hund = BinaryToBcd(packed & 0x007F);
    *sec = BinaryToBcd((packed >> 7) & 0x003F);
    *min = BinaryToBcd(packed >> 13);
}

/* @routine 07:AC13 TimePackBcd */
uint16_t TimePackBcd(uint8_t hundredths, uint8_t seconds, uint8_t minutes)
{
    return (uint16_t)((BcdToBinary(hundredths) & 0x007F)
                      | ((BcdToBinary(seconds) << 7) & 0x1F80)
                      | ((BcdToBinary(minutes) & 0x0007) << 13));
}

/* The decimal add of the processor's decimal mode. */
static uint8_t decimal_add(uint8_t a, uint8_t b, int *carry)
{
    int low = (a & 0x0F) + (b & 0x0F) + *carry;
    int sum;

    if (low > 0x09)
        low += 0x06;
    sum = (a & 0xF0) + (b & 0xF0) + (low > 0x0F ? 0x10 : 0) + (low & 0x0F);
    if (sum > 0x9F)
        sum += 0x60;
    *carry = sum > 0xFF;
    return (uint8_t)sum;
}

/* A value less 60 seconds, decimal. */
static uint8_t less_sixty(uint8_t v)
{
    int high = (v & 0xF0) - 0x60;

    if (high < 0)
        high -= 0x60;
    return (uint8_t)(high | (v & 0x0F));
}

/*
 * @routine 07:B75B TimeAddToTotal, 07:B796 TimeAddToNext
 * WHAT IT DOES: Adds a time to another, unless it is no time ($FF): seconds
 * past 59 carry into the minutes.
 */
static void time_add(const uint8_t *time, uint8_t *to)
{
    int carry = 0;
    uint8_t v;

    if (time[0] == 0xFF)
        return;
    to[0] = decimal_add(time[0], to[0], &carry);
    v = decimal_add(time[1], to[1], &carry);
    if (carry) {
        v = less_sixty(v);
        carry = 0;
        to[2] = decimal_add(to[2], 0x01, &carry);
    }
    to[1] = v;
    if (v >= 0x60) {
        to[1] = less_sixty(v);
        carry = 0;
        to[2] = decimal_add(to[2], 0x01, &carry);
    }
    carry = 0;
    to[2] = decimal_add(time[2], to[2], &carry);
}

/* @routine 07:B67E TimeTotalsSum: each group of four times into its total. */
void TimeTotalsSum(void)
{
    int group, i;

    for (group = 0; group < 3; group++) {
        uint8_t *total = g.times_total[group];

        total[0] = total[1] = total[2] = 0;
        for (i = 0; i < 4; i++)
            time_add(g.times_bcd[group * 4 + i], total);
        if (g.times_bcd[group * 4][2] == 0xFF)
            total[0] = total[1] = total[2] = 0xFF;
    }
}

/* @routine 07:B727 TimeRunningTotals: the first three of each four added into the next. */
void TimeRunningTotals(void)
{
    int group, i;

    for (group = 0; group < 3; group++)
        for (i = 0; i < 3; i++)
            time_add(g.times_bcd[group * 4 + i], g.times_bcd[group * 4 + i + 1]);
}

/* @routine 07:B964 TimeBelowRecord: is the race's time below a record's? */
static int time_below(const uint8_t *record)
{
    int i;

    for (i = 2; i >= 0; i--)
        if (g.race_time[i] != record[i])
            return g.race_time[i] < record[i];
    return 0;
}

/* @routine 07:B90C RecordShiftFrom0, 07:B91C RecordShiftFrom1 and the slot copies: a run moved down a place. */
static void record_move_down(uint8_t class, int place)
{
    memcpy(&save.class_names[class][3 * (place + 1)], &save.class_names[class][3 * place], 3);
    memcpy(&save.class_times[class][4 * (place + 1)], &save.class_times[class][4 * place], 8);
}

/* @routine 07:B8DD RecordNameStore0, 07:B8E1 RecordNameStore3, 07:B8E5 RecordNameStore6 */
static void record_name_store(uint8_t class, int place)
{
    uint8_t *e = &save.class_names[class][3 * place];

    e[0] = (uint8_t)g.player_name;
    e[1] = (uint8_t)(g.player_name >> 8);
    e[2] = g.car[0];
}

/*
 * @routine 07:B7DE RecordTimeInsert
 * WHAT IT DOES: Unless this is the extra run, places the championship run's
 * time in its class's three best: the name, the car and the four lap times
 * go in, the runs below move down, and the class's checksum is remade.
 */
void RecordTimeInsert(void)
{
    uint8_t class = g.course_class, sum = 0;
    const uint8_t *bytes;
    int place, i;

    if (g.byte_0d24 == 0x03)
        return;
    if (time_below(g.times_bcd[3])) {
        record_move_down(class, 1);
        record_move_down(class, 0);
        place = 0;
    } else if (time_below(g.times_bcd[7])) {
        record_move_down(class, 1);
        place = 1;
    } else if (time_below(g.times_bcd[11])) {
        place = 2;
    } else {
        g.record_tag = (uint16_t)(g.record_tag | 0x00FF);
        g.leader_place = 0;
        return;
    }
    record_name_store(class, place);
    g.leader_place = (uint8_t)(place + 1);
    for (i = 0; i < 4; i++)
        save.class_times[class][4 * place + i] =
            TimePackBcd(g.best_times[i][0], g.best_times[i][1], g.best_times[i][2]);
    bytes = (const uint8_t *)save.class_times[class];
    for (i = 0; i < 24; i++)
        sum = (uint8_t)(sum + bytes[i]);
    for (i = 0; i < 9; i++)
        sum = (uint8_t)(sum + save.class_names[class][i]);
    save.class_sum[class] = sum;
    save_commit();
}
