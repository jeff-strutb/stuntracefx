/*
 * records.c: the save file: reading it, the checks that make it valid, and
 * the records it keeps (best times, championship tables, Stunt Trax scores).
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "hud/hud.h"
#include "race/race.h"
#include "nshvc/nshvc.h"
#include "save/save.h"
#include "system/state.h"

SaveFile save;

static const char SAVE_NAME[] = "save";

void save_commit(void)
{
    nshvc_save_write(SAVE_NAME, &save, sizeof save);
}

/* Reads the file, or starts from an empty one, then applies the original's
   checks: whatever fails them is reset to its empty form. */
void save_load(void)
{
    if (nshvc_save_read(SAVE_NAME, &save, sizeof save) != (int)sizeof save)
        memset(&save, 0, sizeof save);
    SaveUnlocksCheck();
    save_commit();
}

/* An 8-bit sum with no carries between the bytes. */
static uint8_t sum8(const void *data, unsigned n)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t s = 0;

    while (n--)
        s = (uint8_t)(s + *p++);
    return s;
}

/*
 * @routine 07:ADCC SaveRecordSumA
 * WHAT IT DOES: The 8-bit sum of one Stunt Trax record's five bytes.
 * @routine 07:ADF9 SaveRecordSumB
 * WHAT IT DOES: The same for one of the special records.
 * (both become sum8 over the typed record)
 */

/*
 * @routine 07:ADDA SaveSumsRecordsB
 * WHAT IT DOES: Keeps the sums of the first three special records in their
 * sum bytes.
 */
void SaveSumsRecordsB(void)
{
    int i;

    for (i = 0; i < 3; i++)
        save.special_sum[i] = sum8(&save.special[i], sizeof save.special[i]);
    save_commit();
}

/* A record whose sum differs, or whose first byte is 0, is reset. */
static void check_bests(SaveBest *r, uint8_t *sums, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (sum8(&r[i], sizeof r[i]) == sums[i] && (r[i].time & 0x00FF))
            continue;
        r[i].time = 0xFFFF;
        r[i].name = 0;
        r[i].car = 0;
        sums[i] = 0xFE;
    }
}

/*
 * @routine 07:B0D1 SaveRecordsCheckA
 * WHAT IT DOES: Validates the four Stunt Trax records.
 * @routine 07:B097 SaveRecordsCheckB
 * WHAT IT DOES: Validates the three special records.
 * (check_bests, called from SaveValidate)
 */

/*
 * @routine 07:B190 SaveBlocksCheck
 * WHAT IT DOES: Validates blocks 12-15 (Stunt Trax scores): each is twelve
 * entries whose value's high byte must be below $41, with an 8-bit sum of
 * all 48 bytes equal to the block's sum byte. A block that fails becomes
 * twelve entries of value 0, name $FFFF, sum byte $E8 (which is their sum).
 */
static void save_blocks_check(void)
{
    int b, e;

    for (b = 12; b < 16; b++) {
        int bad = 0;

        for (e = 0; e < 12; e++)
            if ((save.blocks[b][e].value >> 8) >= 0x41)
                bad = 1;
        if (!bad && sum8(save.blocks[b], sizeof save.blocks[b]) == save.block_sum[b])
            continue;
        for (e = 0; e < 12; e++) {
            save.blocks[b][e].value = 0;
            save.blocks[b][e].name = 0xFFFF;
        }
        save.block_sum[b] = 0xE8;
    }
}

/*
 * @routine 07:B10B SaveTablesCheck
 * WHAT IT DOES: Validates the three championship tables, last first: twelve
 * times whose high byte must be $1F or more, nine name bytes, the 8-bit sum
 * of all 33 equal to the table's sum byte. A failing table becomes 24 bytes
 * of $FF and nine of 0, sum byte $E8 (which is their sum).
 */
static void save_tables_check(void)
{
    int t, i;

    for (t = 2; t >= 0; t--) {
        int bad = 0;

        for (i = 0; i < 12; i++)
            if ((save.class_times[t][i] >> 8) < 0x1F)
                bad = 1;
        if (!bad && (uint8_t)(sum8(save.class_times[t], sizeof save.class_times[t])
                              + sum8(save.class_names[t], sizeof save.class_names[t])) == save.class_sum[t])
            continue;
        memset(save.class_times[t], 0xFF, sizeof save.class_times[t]);
        memset(save.class_names[t], 0, sizeof save.class_names[t]);
        save.class_sum[t] = 0xE8;
    }
}

/* An entry byte is good when it is $FF or in its range; `which` is 0-3. */
static int entry_byte_good(uint8_t v, int which)
{
    if (v == 0xFF)
        return 1;
    switch (which) {
    case 0: return (v & 0x7F) < 0x64;
    case 1: return (v & 0x1F) < 0x1E && (v & 0xE0) >= 0x20;
    case 2: return (v & 0x1F) < 0x1C;
    default: return 1;                  /* the original's test of the fourth can never fail */
    }
}

/*
 * @routine 07:B211 SaveBlocksValidate
 * WHAT IT DOES: Validates blocks 0-11: every entry byte must be $FF or in
 * range, and the 8-bit sum of the block's four lap bytes and its 48 bytes
 * must equal the block's sum byte. A failing block becomes all $FF, its lap
 * bytes too, sum byte $CC (which is their sum).
 */
static void save_blocks_validate(void)
{
    int b, i;

    for (b = 0; b < 12; b++) {
        const uint8_t *bytes = (const uint8_t *)save.blocks[b];
        int bad = 0;

        for (i = 0; i < 48; i++)
            if (!entry_byte_good(bytes[i], i & 3))
                bad = 1;
        if (!bad && (uint8_t)(sum8(save.laps[b], sizeof save.laps[b]) + sum8(bytes, 48)) == save.block_sum[b])
            continue;
        memset(save.blocks[b], 0xFF, sizeof save.blocks[b]);
        memset(save.laps[b], 0xFF, sizeof save.laps[b]);
        save.block_sum[b] = 0xCC;
    }
}

/*
 * @routine 07:B085 SaveValidate
 * WHAT IT DOES: Validates the whole save area, resetting whatever fails.
 */
void SaveValidate(void)
{
    save_tables_check();
    save_blocks_validate();
    save_blocks_check();
    check_bests(save.special, save.special_sum, 3);
    check_bests(save.stunt, save.stunt_sum, 4);
}

/* An 8-bit sum carrying from each add into the next, the first add with
   carry clear. */
static uint8_t sum_with_carries(const uint8_t *const *bytes, int n)
{
    unsigned s = 0, c = 0;
    int i;

    for (i = 0; i < n; i++) {
        s = (s & 0xFF) + *bytes[i] + c;
        c = s > 0xFF;
    }
    return (uint8_t)s;
}

/*
 * @routine 03:F02B SaveChecksum
 * WHAT IT DOES: Computes the save's checksum and stores it twice.
 * The sum covers nine of the ten bytes SaveUnlocksCheck reads back: it
 * leaves out $FC0F, as the original does.
 */
uint8_t SaveChecksum(void)
{
    const uint8_t *const order[9] = {
        &save.progress, &save.courses_open[0], &save.courses_open[1], &save.cars_mode01,
        &save.car_mode3, &save.cars_battle, &save.settings[0], &save.settings[1], &save.settings[2]};
    uint8_t s = sum_with_carries(order, 9);

    save.check[0] = s;
    save.check[1] = s;
    save_commit();
    return s;
}

/*
 * @routine 03:EFD2 SaveUnlocksCheck
 * WHAT IT DOES: Checks the unlock bytes ($FC06-$FC0F) against their
 * checksum; when it differs, clears them and the checksum. Then validates
 * the whole save.
 */
void SaveUnlocksCheck(void)
{
    const uint8_t *const order[10] = {
        &save.progress, &save.unused_0f, &save.courses_open[0], &save.courses_open[1], &save.cars_mode01,
        &save.car_mode3, &save.cars_battle, &save.settings[0], &save.settings[1], &save.settings[2]};

    if (sum_with_carries(order, 10) != save.check[0]) {
        save.progress = 0;
        save.unused_0f = 0;
        memset(save.courses_open, 0, sizeof save.courses_open);
        save.cars_mode01 = save.car_mode3 = save.cars_battle = 0;
        memset(save.settings, 0, sizeof save.settings);
        memset(save.check, 0, sizeof save.check);
    }
    SaveValidate();
}

/*
 * @routine 0B:D892 MenuEraseSave
 * WHAT IT DOES: Erases the save: clears all of it, then lets
 * SaveUnlocksCheck put back an empty, valid save.
 */
void SaveEraseAll(void)
{
    memset(&save, 0, sizeof save);
    SaveUnlocksCheck();
    save_commit();
}

/*
 * @routine 07:B518 RecordsUnpackCourse
 * WHAT IT DOES: Unpacks the records of class $0D4A for display: its twelve
 * packed times into three BCD bytes each (hundredths, seconds, minutes),
 * then its nine name bytes.
 */
void RecordsUnpackCourse(void)
{
    int i;

    for (i = 0; i < 12; i++) {
        uint16_t minutes, seconds, hundredths;

        TimeUnpackBcd(save.class_times[g.course_class][i], &minutes, &seconds, &hundredths);
        g.times_bcd[i][0] = (uint8_t)hundredths;
        g.times_bcd[i][1] = (uint8_t)seconds;
        g.times_bcd[i][2] = (uint8_t)minutes;
    }
    memcpy(g.names_bytes, save.class_names[g.course_class], sizeof g.names_bytes);
}

/*
 * @routine 07:B503 RecordsUnpackCourseTotals
 * WHAT IT DOES: Unpacks the records and sums each group of four times.
 */
void RecordsUnpackCourseTotals(void)
{
    RecordsUnpackCourse();
    TimeTotalsSum();
}

/*
 * @routine 07:B50A RecordsUnpackCourseRunning
 * WHAT IT DOES: Unpacks the records and makes each group of four times
 * running totals.
 */
void RecordsUnpackCourseRunning(void)
{
    RecordsUnpackCourse();
    TimeRunningTotals();
}

/* The course's twelve entries into g.record_times, g.record_names and
   g.record_tags, then sorted together: descending or ascending. */
static void records_load_sorted(int course, int descending)
{
    int i;

    for (i = 0; i < 12; i++) {
        g.record_times[i] = save.blocks[course][i].value;
        g.record_names[i] = save.blocks[course][i].name;
        g.record_tags[i] = cart16(0x07, (uint16_t)(0xB4EB + i * 2));
    }
    for (i = 10; i >= 0; i--) {
        int x = i;

        for (;;) {
            uint16_t v = g.record_times[x + 1], c = g.record_times[x], t;

            if (descending ? v <= c : v >= c)
                break;
            g.record_times[x + 1] = c;
            g.record_times[x] = v;
            t = g.record_tags[x + 1]; g.record_tags[x + 1] = g.record_tags[x]; g.record_tags[x] = t;
            t = g.record_names[x + 1]; g.record_names[x + 1] = g.record_names[x]; g.record_names[x] = t;
            if (x == 10)
                break;
            x++;
        }
    }
}

/* The place (1-3) of the tag among the first three, or 0. */
static void records_player_place(void)
{
    int i;

    g.record_place = 0;
    for (i = 0; i < 3; i++)
        if (g.record_tag == g.record_tags[i]) {
            g.record_place = (uint16_t)(i + 1);
            break;
        }
}

/*
 * @routine 07:B329 RecordsLoadSpecialCourse
 * WHAT IT DOES: For courses $0C-$0F: loads and sorts the course's entries,
 * largest first, and finds the player's place.
 * @routine 07:B31B RecordsLoadSpecialCourseLong (the same, for a JSL caller)
 */
void RecordsLoadSpecialCourse(void)
{
    if (g.course < 0x0C || g.course >= 0x10)
        return;
    records_load_sorted(g.course, 1);
    records_player_place();
}

static void unpack_to(uint16_t packed, uint8_t to[3])
{
    uint16_t minutes, seconds, hundredths;

    TimeUnpackBcd(packed, &minutes, &seconds, &hundredths);
    to[0] = (uint8_t)hundredths;
    to[1] = (uint8_t)seconds;
    to[2] = (uint8_t)minutes;
}

/*
 * @routine 07:B400 RecordsLoadCourse
 * WHAT IT DOES: For a normal course (below $0C): loads its entries sorted
 * smallest first; for any course finds the player's place; then unpacks the
 * course's two best lap times and the first time to BCD.
 * @routine 07:B3F2 RecordsLoadCourseLong (the same, for a JSL caller)
 */
void RecordsLoadCourse(void)
{
    if (g.course < 0x0C)
        records_load_sorted(g.course, 0);
    records_player_place();
    unpack_to(g.course < 12 ? save.laps[g.course][0] : 0xFFFF, g.record_bcd[0]);
    unpack_to(g.course < 12 ? save.laps[g.course][1] : 0xFFFF, g.record_bcd[1]);
    unpack_to(g.record_times[0], g.record_bcd[2]);
    g.best_lap_time = g.record_times[0];
}

/*
 * @routine 07:B3D4 RecordsLoadForCourse
 * WHAT IT DOES: Loads the current course's records: RecordsLoadSpecialCourse
 * for courses $0C-$0F, RecordsLoadCourse below $0C, nothing above.
 */
void RecordsLoadForCourse(void)
{
    if (g.course < 0x0C)
        RecordsLoadCourse();
    else if (g.course < 0x10)
        RecordsLoadSpecialCourse();
}

/*
 * @routine 07:AFFC SpecialRecordsChecksum
 * WHAT IT DOES: The check sum of a course's records: the bytes of its four
 * lap bytes (0 for course $0C or more) and its 48 bytes, at the course's
 * sum byte.
 */
void SpecialRecordsChecksum(void)
{
    uint8_t s = sum8(save.blocks[g.course], sizeof save.blocks[g.course]);

    if (g.course < 0x0C)
        s = (uint8_t)(s + sum8(save.laps[g.course], sizeof save.laps[g.course]));
    save.block_sum[g.course] = s;
    save_commit();
}

/*
 * @routine 07:B077 RecordCarTimes12
 * WHAT IT DOES: The car times 12.
 */
uint16_t RecordCarTimes12(void)
{
    return (uint16_t)(g.car[0] * 12) & 0x00FF;
}

/*
 * @routine 07:B05E RecordTagCar0
 * WHAT IT DOES: Sets the tag to the car's first record's.
 * @routine 07:B063 RecordTagCar4
 * WHAT IT DOES: ... the second's.
 * @routine 07:B06C RecordTagCar8
 * WHAT IT DOES: ... the third's.
 */
void RecordTagCar0(void) { g.record_tag = RecordCarTimes12(); }
void RecordTagCar4(void) { g.record_tag = (uint16_t)(RecordCarTimes12() + 4); }
void RecordTagCar8(void) { g.record_tag = (uint16_t)(RecordCarTimes12() + 8); }

/*
 * @routine 07:AEF3 SpecialRecordInsert
 * WHAT IT DOES: Puts time into the course's three best times for the car,
 * entries the time and the player's name: below the first moves the others
 * down and, unless the time is still above another of the course's entries,
 * keeps the race's packed lap times; below the second or third likewise.
 * No place: the tag $FFFF. Then the course's check sum. With HUD mode 0, a
 * mark first; course $0C or more, or car 4 or more: nothing else.
 */
void SpecialRecordInsert(uint16_t time)
{
    SaveEntry *e;

    if (g.hud_mode == 0)
        g.word_0f6e[g.champ_stage] = (uint16_t)((g.champ_stage * 2) & 0x00FF);
    if (g.course >= 0x0C || g.car[0] >= 4)
        return;
    e = &save.blocks[g.course][g.car[0] * 3];
    if (time < e[0].value) {
        int i;
        int keep = 1;

        RecordTagCar0();
        e[2] = e[1];
        e[1] = e[0];
        e[0].value = time;
        e[0].name = g.player_name;
        for (i = 0; i < 12; i++) {
            if (&save.blocks[g.course][i] == e)
                continue;
            if (time >= save.blocks[g.course][i].value) {
                keep = 0;
                break;
            }
        }
        if (keep) {
            save.laps[g.course][1] = TimePackBcd(g.lap_clock[3], g.lap_clock[4], g.lap_clock[5]);
            save.laps[g.course][0] = TimePackBcd(g.lap_clock[0], g.lap_clock[1], g.lap_clock[2]);
        }
    } else if (time < e[1].value) {
        RecordTagCar4();
        e[2] = e[1];
        e[1].value = time;
        e[1].name = g.player_name;
    } else if (time < e[2].value) {
        e[2].value = time;
        RecordTagCar8();
        e[2].name = g.player_name;
    } else {
        g.record_tag = 0xFFFF;
        return;
    }
    SpecialRecordsChecksum();
}

/*
 * @routine 07:ADA3 SaveRecordSumsTimeUp
 * WHAT IT DOES: The sums of the four Stunt Trax records into their sum
 * bytes.
 */
void SaveRecordSumsTimeUp(void)
{
    int i;

    for (i = 0; i < 4; i++)
        save.stunt_sum[i] = sum8(&save.stunt[i], sizeof save.stunt[i]);
    save_commit();
}

/*
 * @routine 07:AD1B TimeUpRecordUpdate
 * WHAT IT DOES: The time-up run's end: its time as five digits, then packed
 * and set against the record for the course ($0E3D - $17): lower, it becomes
 * the record, $0FA3 = $FF and the sums made again; higher, $0FA3 = 1; equal,
 * $0FA3 = 0.
 */
void TimeUpRecordUpdate(void)
{
    SaveBest *r = &save.stunt[(uint8_t)(g.course - 0x17)];
    uint16_t t;

    g.best_digits[0] = g.clock_minutes & 0x0F;
    g.best_digits[1] = (uint8_t)(g.clock_seconds >> 4) & 0x0F;
    g.best_digits[2] = g.clock_seconds & 0x0F;
    g.best_digits[3] = (uint8_t)(g.clock_fraction >> 12) & 0x0F;
    g.best_digits[4] = (g.clock_fraction >> 8) & 0x0F;
    t = TimePackBcd((uint8_t)(g.clock_fraction >> 8), (uint8_t)g.clock_seconds, (uint8_t)g.clock_minutes);
    if (t == r->time) {
        g.lead = 0x00;
        return;
    }
    if (t > r->time) {
        g.lead = 0x01;
        return;
    }
    r->time = t;
    r->name = g.player_name;
    r->car = (uint8_t)g.car[0];
    g.lead = 0xFF;
    SaveRecordSumsTimeUp();
}

/* A BCD add of two bytes, the carry out lost. */
static uint8_t bcd_add(uint8_t a, uint8_t b)
{
    unsigned lo = (a & 0x0F) + (b & 0x0F);
    unsigned hi = (a >> 4) + (b >> 4);

    if (lo > 9) {
        lo -= 10;
        hi++;
    }
    if (hi > 9)
        hi -= 10;
    return (uint8_t)(hi << 4 | lo);
}

/*
 * @routine 07:AE07 StuntRecordTime
 * WHAT IT DOES: Stunt Trax's score against the course's three best for the
 * car: the score is $19EC as the high byte and the BCD sum of $0E70 and
 * $19F0 as the low; a higher score goes in at its place, the lower ones
 * moving down, and the course's 48 bytes are summed into its sum byte.
 * Only courses $0C-$0F reach it in play (see the dead-code list).
 */
void StuntRecordTime(void)
{
    uint16_t score = (uint16_t)(g.stunt_score_high << 8 | bcd_add(g.time_left, g.stunt_score_add));
    SaveEntry *e;

    if (g.course < 0x0C || g.course >= 0x10)
        return;
    e = &save.blocks[g.course][g.car[0] * 3];
    if (score > e[0].value) {
        RecordTagCar0();
        e[2] = e[1];
        e[1] = e[0];
        e[0].value = score;
        e[0].name = g.player_name;
    } else if (score > e[1].value) {
        RecordTagCar4();
        e[2] = e[1];
        e[1].value = score;
        e[1].name = g.player_name;
    } else if (score > e[2].value) {
        e[2].value = score;
        RecordTagCar8();
        e[2].name = g.player_name;
    } else {
        return;
    }
    SpecialRecordsChecksum();
}

/*
 * @routine 07:D6E8 RecordTimeCompare
 * WHAT IT DOES: Compares the record time (BCD hundredths, seconds, minutes)
 * with the best time's digits, most significant first: better $FF, worse or
 * no best time 1, equal 0; then sound $35 on port 0 unless muted.
 */
void RecordTimeCompare(uint16_t record)
{
    const uint8_t *t = g.record_bcd[record / 3];
    uint8_t digits[5];
    int i;

    g.lead = 0x00;
    if (g.record_times[0] == 0xFFFF) {
        g.lead = 0x01;
    } else {
        digits[0] = t[2] & 0x0F;
        digits[1] = t[1] >> 4;
        digits[2] = t[1] & 0x0F;
        digits[3] = t[0] >> 4;
        digits[4] = t[0] & 0x0F;
        for (i = 0; i < 5; i++) {
            if (digits[i] < g.best_digits[i]) {
                g.lead = 0xFF;
                break;
            }
            if (digits[i] != g.best_digits[i]) {
                g.lead = 0x01;
                break;
            }
        }
    }
    if (!g.lap_shown)
        sound_port0_push(0x35);
}

static void record_compare_by_mode(uint16_t record)
{
    if (g.hud_mode == 0)
        sound_port0_push(0x35);
    else if (g.hud_mode == 2)
        PlayerSound35();
    else
        RecordTimeCompare(record);
}

/*
 * @routine 07:D62C RecordCompareTime0
 * WHAT IT DOES: By mode: 0 plays sound $35, 2 PlayerSound35, others compare
 * record time 0 with the best (RecordTimeCompare).
 * @routine 07:D64F RecordCompareTime1
 * WHAT IT DOES: As RecordCompareTime0 for record time 1.
 */
void RecordCompareTime0(void) { record_compare_by_mode(0); }
void RecordCompareTime1(void) { record_compare_by_mode(3); }

/* The records panel of the course screens (race/course): four rows of three
   saved entries, drawn one character a frame by the caller into the tilemap
   buffer it holds. */

/* The tilemap offset of a character: the row's start, plus place * 64 for
   the place, plus the character's column. */
static uint16_t records_char_offset(uint16_t row_table, uint16_t char_table, int row, int place, int character)
{
    return (uint16_t)(cart16(0x07, (uint16_t)(row_table + row * 2)) + place * 64
                      + cart8(0x07, (uint16_t)(char_table + character)));
}

/*
 * @routine 07:D8F3 RecordsEntryLoad
 * WHAT IT DOES: The saved entry (row * 3 + place) of the course.
 */
static const SaveEntry *records_entry(int row, int place)
{
    return &save.blocks[g.course][row * 3 + place];
}

/*
 * @routine 07:D795 RecordsCharTileStunt
 * WHAT IT DOES: The tile of a Stunt Trax records character: 0-1 the score's
 * high byte digits (blank tiles $6D, $6E from $40 up), 2-3 the digits of the
 * low byte, 4-6 the three letters of the name (five bits each, + $0A).
 */
static uint16_t stunt_char_tile(const SaveEntry *e, int character)
{
    uint8_t high = (uint8_t)(e->value >> 8);

    switch (character) {
    case 0: return high >= 0x40 ? 0x6D : (uint16_t)(high >> 4);
    case 1: return high >= 0x40 ? 0x6E : (uint16_t)(high & 0x0F);
    case 2: return (uint16_t)(e->value >> 4) & 0x000F;
    case 3: return e->value & 0x000F;
    case 4: return (uint16_t)((e->name & 0x1F) + 0x0A);
    case 5: return (uint16_t)(((e->name >> 5) & 0x1F) + 0x0A);
    default: return (uint16_t)(((e->name >> 10) & 0x1F) + 0x0A);
    }
}

/*
 * @routine 07:D7FF RecordsCharTileAll
 * WHAT IT DOES: As RecordsCharTileStunt for the other courses, whose entries
 * hold a packed time: 0 the minutes, 1-2 the seconds' tens and units, 3-4
 * the hundredths', 5-7 the three letters.
 */
static uint16_t time_char_tile(const SaveEntry *e, int character)
{
    switch (character) {
    case 0: return BinaryToBcd(e->value >> 13) & 0x000F;
    case 1: return BinaryToBcd((e->value >> 7) & 0x3F) >> 4;
    case 2: return BinaryToBcd((e->value >> 7) & 0x3F) & 0x000F;
    case 3: return BinaryToBcd(e->value & 0x7F) >> 4;
    case 4: return BinaryToBcd(e->value & 0x7F) & 0x000F;
    case 5: return (uint16_t)((e->name & 0x1F) + 0x0A);
    case 6: return (uint16_t)(((e->name >> 5) & 0x1F) + 0x0A);
    default: return (uint16_t)(((e->name >> 10) & 0x1F) + 0x0A);
    }
}

/*
 * @routine 07:D87A RecordsCharOffsetStunt
 * @routine 07:D768 RecordsCharDrawStunt
 * @routine 07:C663 PreviewRowDrawStunt
 * WHAT IT DOES: Draws the character of the Stunt Trax records panel: its
 * tile, when the entry's first word is not 0, at its place in the map
 * buffer.
 */
void RecordsCharDrawStunt(void)
{
    uint16_t off = records_char_offset(0xD8D4, 0xD8DC, g.panel_row, g.panel_place, g.panel_char);
    const SaveEntry *e = records_entry(g.panel_row, g.panel_place);

    if (e->value)
        RecordsPanelTile(off, (uint8_t)stunt_char_tile(e, g.panel_char));
}

/*
 * @routine 07:D8A7 RecordsCharOffsetAll
 * @routine 07:D77D RecordsCharDrawAll
 * @routine 07:C673 PreviewRowDrawAll
 * WHAT IT DOES: As RecordsCharDrawStunt for the other courses; an entry is
 * empty when its first word is $FFFF.
 */
void RecordsCharDrawAll(void)
{
    uint16_t off = records_char_offset(0xD8E3, 0xD8EB, g.panel_row, g.panel_place, g.panel_char);
    const SaveEntry *e = records_entry(g.panel_row, g.panel_place);

    if (e->value != 0xFFFF)
        RecordsPanelTile(off, (uint8_t)time_char_tile(e, g.panel_char));
}
