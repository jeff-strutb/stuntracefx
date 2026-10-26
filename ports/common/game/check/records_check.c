/* old: save/records.c hud/lap_time.c audio/sound_queue.c audio/sound_effects.c video/oam.c

 * records_check.c: the port's saved-record routines (save/records.c) against
 * the decomp's, on random states: work RAM and game pak RAM random, then
 * shaped where the routine reads an index or a course (the realistic range
 * the game uses, named by each case). Each case runs the decomp's routine on
 * a copy and the port's on the state, and compares both RAMs but the direct
 * page.
 *
 *   check/check.sh records N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#include "machine.h"
#include "cpu_ops.h"
#undef cpu

#include "game.h"
#include "records.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(RecordsUnpackCourse) OLD(RecordsUnpackCourseTotals) OLD(RecordsUnpackCourseRunning)
OLD(RecordsLoadForCourse) OLD(RecordsLoadCourse) OLD(RecordsLoadCourseLong)
OLD(RecordsLoadSpecialCourse) OLD(RecordsLoadSpecialCourseLong)
OLD(PreviewRowDrawStunt) OLD(PreviewRowDrawAll)
OLD(StuntRecordTime) OLD(TimeUpRecordUpdate)
OLD(SaveRecordSumB) OLD(SaveSumsRecordsB) OLD(SaveRecordsCheckA) OLD(SaveRecordsCheckB)
OLD(SaveBlocksCheck) OLD(SaveTablesCheck) OLD(SaveBlocksValidate) OLD(SaveBlocksFourthBad)
OLD(SaveValidate) OLD(SaveUnlocksCheck)
OLD(RecordNameLetters) OLD(RecordNameLettersSmall) OLD(RecordNameLettersLarge) OLD(RecordLetterTile)
OLD(RecordTagCarSet) OLD(RecordTimeDigitsLarge) OLD(RecordTimeDigitsSmall) OLD(RecordScoreDigits)
OLD(RecordRowDraw) OLD(RecordRow1) OLD(RecordRow2) OLD(RecordRow3)
OLD(RecordRow1Time0) OLD(RecordRow2Time4) OLD(RecordRow3Time8)
OLD(RecordRowsGroupFirsts) OLD(RecordRowsFirstTimes)
OLD(RecordTimeCompare) OLD(RecordCompareTime0) OLD(RecordCompareTime1)
OLD(SpecialRecordsChecksum) OLD(SpecialRecordInsert)

static uint64_t rng = 0x510E527FADE682D1ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration;

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 20)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld: ", model_current, iteration);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* Random RAM, and the decomp's register file: data bank $07 (the tag table's
   bank, which the course loads set), direct page 0, the stack high. */
static void fresh(void)
{
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.s = 0x01FF;
    old_cpu.db = 0x07;
    old_cpu.p = (uint8_t)((rnd() & (P_N | P_V | P_Z | P_C | P_I)) | (rnd() & 1 ? P_M : 0) | (rnd() & 1 ? P_X : 0));
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same(void)
{
    size_t i;

    cases++;
    for (i = 0x100; i < sizeof wram; i++)
        if (wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X", i, pakram[i], old_p[i]);
            return;
        }
}

/* A byte as binary coded decimal of a value below 100. */
static uint8_t bcd(uint32_t v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

/* A random packed time as the save keeps it, with its BCD parts: hundredths
   (0-99), seconds (0-59), minutes (0-9); written to $0E63, $0E64 and $0E66
   as the time-up run leaves them. */
static void random_time_up_time(void)
{
    W8(0x0E63) = bcd(rnd() % 100);
    W8(0x0E64) = bcd(rnd() % 60);
    W8(0x0E66) = bcd(rnd() % 10);
}

/* ---- the save: a valid save as the game keeps it, then sometimes corrupt ---- */

/* The rules of a block's entry byte, as the save's checks state them: $FF
   passes; byte 0 with its low seven bits below $64; byte 1 with its low five
   bits below $1E and its top three bits not all 0; byte 2 with its low five
   bits below $1C; byte 3 any. */
static int entry_byte_ok(uint8_t v, int j)
{
    if (v == 0xFF)
        return 1;
    switch (j) {
    case 0: return (v & 0x7F) < 0x64;
    case 1: return (v & 0x1F) < 0x1E && (v & 0xE0) != 0;
    case 2: return (v & 0x1F) < 0x1C;
    default: return 1;
    }
}

/* The 8-bit sum of n pak bytes from `at` (16-bit addresses, wrapping). */
static uint8_t pak_sum(unsigned at, unsigned n)
{
    uint8_t s = 0;
    unsigned i;

    for (i = 0; i < n; i++)
        s = (uint8_t)(s + pakram[(uint16_t)(at + i)]);
    return s;
}

/* The save shaped to pass its checks: the three tables (entries' second
   bytes $1F or more), the four and three records (first byte nonzero),
   blocks 0-11 (entry bytes passing entry_byte_ok, group bytes random),
   blocks 12-15 (entry second bytes below $41); then each sum byte is set to
   the sum its check makes. */
static void build_save(void)
{
    static const unsigned tfirst[3] = {0x00, 0x18, 0x30}, tsecond[3] = {0x00, 0x09, 0x12};
    unsigned k, i, j, b, x;

    for (k = 0; k < 3; k++) {
        for (i = 0; i < 12; i++) {
            pakram[0xFC88 + tfirst[k] + 2 * i] = (uint8_t)rnd();
            pakram[0xFC89 + tfirst[k] + 2 * i] = (uint8_t)(0x1F + rnd() % (0x100 - 0x1F));
        }
        for (j = 0; j < 9; j++)
            pakram[0xFC6D + tsecond[k] + j] = (uint8_t)rnd();
    }
    for (x = 0; x < 4; x++)
        pakram[0xFC4A + 5 * x] = (uint8_t)(1 + rnd() % 255);
    for (x = 0; x < 3; x++)
        pakram[0xFC5E + 5 * x] = (uint8_t)(1 + rnd() % 255);
    for (b = 0; b < 16; b++)
        for (i = 0; i < 12; i++)
            for (j = 0; j < 4; j++) {
                uint16_t at = (uint16_t)(0xFCD0 + 48 * b + 4 * i + j);
                uint8_t v;

                if (b < 12) {
                    do
                        v = (uint8_t)rnd();
                    while (!entry_byte_ok(v, (int)j));
                } else if (j == 1) {
                    v = (uint8_t)(rnd() % 0x41);
                } else {
                    v = (uint8_t)rnd();
                }
                pakram[at] = v;
            }
    for (b = 0; b < 12; b++)
        for (j = 0; j < 4; j++)
            pakram[0xFFD0 + 4 * b + j] = (uint8_t)rnd();

    for (k = 0; k < 3; k++) {
        uint8_t s = 0;

        for (i = 0; i < 12; i++) {
            s = (uint8_t)(s + pakram[0xFC88 + tfirst[k] + 2 * i]);
            s = (uint8_t)(s + pakram[0xFC89 + tfirst[k] + 2 * i]);
        }
        for (j = 0; j < 9; j++)
            s = (uint8_t)(s + pakram[0xFC6D + tsecond[k] + j]);
        pakram[0xFC2D + k] = s;
    }
    for (x = 0; x < 4; x++)
        pakram[0xFC46 + x] = pak_sum(0xFC4A + 5 * x, 5);
    for (x = 0; x < 3; x++)
        pakram[0xFC43 + x] = pak_sum(0xFC5E + 5 * x, 5);
    for (b = 0; b < 16; b++) {
        uint8_t s = b < 12 ? pak_sum(0xFFD0 + 4 * b, 4) : 0;

        s = (uint8_t)(s + pak_sum((uint16_t)(0xFCD0 + 48 * b), 48));
        pakram[0xFC33 + b] = s;
    }
}

/* A save as the game keeps it, and sometimes one a bad write has spoiled:
   one byte of it changed at random. */
static void fresh_save(void)
{
    fresh();
    old_cpu.db = 0x70;
    build_save();
    if (rnd() % 3 == 0) {
        unsigned at = 0xFC2D + rnd() % (0x10000 - 0xFC2D);

        pakram[at] ^= (uint8_t)(rnd() | 1);
    }
}

/* The unlock checksum of the save (the carry chain over its ten bytes). */
static uint8_t unlock_checksum_of_save(void)
{
    static const unsigned at[10] = {0xFC0B, 0xFC0F, 0xFC06, 0xFC07, 0xFC0C, 0xFC0D, 0xFC0E, 0xFC08, 0xFC09, 0xFC0A};
    unsigned s = pakram[at[0]], c = 0, i;

    for (i = 1; i < 10; i++) {
        s = s + pakram[at[i]] + c;
        c = s >> 8;
        s &= 0xFF;
    }
    return (uint8_t)s;
}

/* ---- the panel's routines: the data bank is work RAM ($7E), the sprite
   buffer and the record times random; the time index of a row is (page +
   time) & $FF, and some seconds bytes are $FF (no time) ---- */

static void fresh_w(void)
{
    fresh();
    old_cpu.db = 0x7E;
}

/* Random seconds bytes in the record times: $FF (no time) now and then. */
static void shape_times(void)
{
    unsigned k;

    for (k = 0; k < 48; k++)
        if (rnd() % 3 == 0)
            W8(0x0F77 + k) = 0xFF;
}

/* A records row's name entry and sprite, and the page's offset ($F7). The
   names' car bytes are the cars 0-3; the piece selector $10BB is 0-3 (its
   table's entries; the sprite check shapes it the same way). */
static void shape_row(uint8_t page)
{
    unsigned n;

    for (n = 0; n < 9; n++)
        W8(0x10CF + n) = (uint8_t)rnd();
    for (n = 0; n < 3; n++)
        W8(0x10CF + 3 * n + 2) = (uint8_t)(rnd() % 4);
    W8(0x10BB) = (uint8_t)(rnd() % 4);
    for (n = 0; n < 3; n++)
        W16(0x0D75 + 2 * n) = (uint16_t)(rnd() % 0x80);
    W8(0x00F7) = page;
    W8(0x00F8) = (uint8_t)rnd();
    shape_times();
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: records_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("RecordsLoadForCourse") {
        fresh();
        W8(0x0E3D) = (uint8_t)rnd();          /* any course: the three ranges */
        old_run(old_abi_RecordsLoadForCourse);
        records_load_for_course();
        same();
    }
    CASE("RecordsLoadSpecialCourse / RecordsLoadSpecialCourseLong") {
        int longform = (int)(rnd() & 1);

        fresh();
        W8(0x0E3D) = (uint8_t)(rnd() % 4 ? 0x0C + rnd() % 4 : rnd());   /* the special courses, now and then any */
        old_run(longform ? old_abi_RecordsLoadSpecialCourseLong : old_abi_RecordsLoadSpecialCourse);
        if (longform)
            records_load_special_course_long();
        else
            records_load_special_course();
        same();
    }
    CASE("RecordsLoadCourse / RecordsLoadCourseLong") {
        int longform = (int)(rnd() & 1);

        fresh();
        W8(0x0E3D) = (uint8_t)(rnd() % 0x0C);   /* a normal course: its lap-time records end at $FFFF */
        old_run(longform ? old_abi_RecordsLoadCourseLong : old_abi_RecordsLoadCourse);
        if (longform)
            records_load_course_long();
        else
            records_load_course();
        same();
    }
    CASE("RecordsUnpackCourse / RecordsUnpackCourseTotals / RecordsUnpackCourseRunning") {
        int k = (int)(rnd() % 3);

        fresh();
        W8(0x0D4A) = (uint8_t)(rnd() % 3);     /* the class: 0-2 */
        old_run(k == 0 ? old_abi_RecordsUnpackCourse : k == 1 ? old_abi_RecordsUnpackCourseTotals
                : old_abi_RecordsUnpackCourseRunning);
        if (k == 0)
            records_unpack_course();
        else if (k == 1)
            records_unpack_course_totals();
        else
            records_unpack_course_running();
        same();
    }
    CASE("PreviewRowDrawStunt / PreviewRowDrawAll") {
        int k = (int)(rnd() & 1);

        fresh();
        W8(0x0E3D) = (uint8_t)(rnd() % 0x10);  /* the courses $00-$0F */
        W8(0x0FA8) = (uint8_t)(rnd() % 4);     /* the row */
        W8(0x0FA9) = 0;
        W8(0x0FAA) = (uint8_t)(rnd() % 3);     /* the place */
        W8(0x0FAB) = 0;
        W8(0x0FA6) = (uint8_t)(rnd() % (k ? 8 : 7));  /* the character */
        W8(0x0FA7) = 0;
        if (rnd() % 4 == 0) {                  /* an empty entry: no time (all, $FFFF) or none (stunt, 0) */
            uint16_t x = (uint16_t)((((3u * W8(0x0E3D)) & 0xFF) << 4) + (W8(0x0FA8) * 3 << 2)
                                    + (W8(0x0FAA) << 2));
            P16(0xFCD0 + x) = k ? 0xFFFF : 0;
        }
        old_run(k ? old_abi_PreviewRowDrawAll : old_abi_PreviewRowDrawStunt);
        if (k)
            preview_row_draw_all();
        else
            preview_row_draw_stunt();
        same();
    }
    CASE("StuntRecordTime") {
        /* a course $0C-$0F, a car 0-3 (the car's entries are the 48 bytes of
           the course); the score's low byte is the BCD sum of two scores */
        fresh();
        W8(0x0E3D) = (uint8_t)(0x0C + rnd() % 4);
        W8(0x0D89) = (uint8_t)(rnd() % 4);
        W8(0x19EC) = (uint8_t)rnd();
        W8(0x0E70) = bcd(rnd() % 100);
        W8(0x19F0) = bcd(rnd() % 100);
        W8(0x0F11) = (uint8_t)rnd();
        W8(0x0F12) = (uint8_t)rnd();
        old_run(old_abi_StuntRecordTime);
        stunt_record_time();
        same();
    }
    CASE("TimeUpRecordUpdate") {
        uint16_t x;
        uint16_t t;
        BcdTime now;
        int mode = (int)(rnd() % 4);

        fresh();
        random_time_up_time();
        W8(0x0E3D) = (uint8_t)rnd();           /* the course: its record at (course - $17) * 5 */
        W8(0x0D89) = (uint8_t)rnd();
        W8(0x0F11) = (uint8_t)rnd();
        W8(0x0F12) = (uint8_t)rnd();
        now.hund = W8(0x0E63), now.sec = W8(0x0E64), now.min = W8(0x0E66);
        t = time_pack(now);
        x = (uint8_t)(5 * (W8(0x0E3D) - 0x17));
        /* the record: random, or equal to the time, or a little above or below */
        if (mode == 1)
            P16(0xFC4A + x) = t;
        else if (mode == 2)
            P16(0xFC4A + x) = (uint16_t)(t + 1);
        else if (mode == 3)
            P16(0xFC4A + x) = (uint16_t)(t - 1);
        old_run(old_abi_TimeUpRecordUpdate);
        time_up_record_update();
        same();
    }
    CASE("SaveRecordSumB") {
        uint8_t x = (uint8_t)rnd(), got;

        fresh_save();
        old_cpu.x = x;
        old_run(old_abi_SaveRecordSumB);
        got = save_record_sum_b(x);
        if ((uint8_t)old_cpu.a != got)
            fail("SaveRecordSumB (x %u) = %02X, the decomp's %02X", x, got, (uint8_t)old_cpu.a);
        same();
    }
    CASE("SaveSumsRecordsB") {
        uint8_t got;

        fresh_save();
        old_run(old_abi_SaveSumsRecordsB);
        got = save_sums_records_b();
        if ((uint8_t)old_cpu.a != got)
            fail("SaveSumsRecordsB returns %02X, the decomp's %02X", got, (uint8_t)old_cpu.a);
        same();
    }
    CASE("SaveRecordsCheckA / SaveRecordsCheckB") {
        int longform = (int)(rnd() & 1);
        uint8_t got;

        fresh_save();
        old_run(longform ? old_abi_SaveRecordsCheckB : old_abi_SaveRecordsCheckA);
        got = longform ? save_records_check_b() : save_records_check_a();
        if ((uint8_t)old_cpu.a != got)
            fail("SaveRecordsCheck%c returns %02X, the decomp's %02X", longform ? 'B' : 'A', got, (uint8_t)old_cpu.a);
        same();
    }
    CASE("SaveBlocksCheck") {
        fresh_save();
        old_run(old_abi_SaveBlocksCheck);
        save_blocks_check();
        same();
    }
    CASE("SaveTablesCheck") {
        fresh_save();
        old_run(old_abi_SaveTablesCheck);
        save_tables_check();
        same();
    }
    CASE("SaveBlocksValidate") {
        fresh_save();
        old_run(old_abi_SaveBlocksValidate);
        save_blocks_validate();
        same();
    }
    CASE("SaveBlocksFourthBad") {
        /* a block in $F3 (0-11) with its offset in $F5 (block * 48), checked from there */
        unsigned blk = rnd() % 12;

        fresh_save();
        W8(0x00F3) = (uint8_t)blk;
        W8(0x00F4) = 0;
        W8(0x00F5) = (uint8_t)(blk * 48);
        W8(0x00F6) = (uint8_t)((blk * 48) >> 8);
        old_run(old_abi_SaveBlocksFourthBad);
        save_blocks_fourth_bad((uint8_t)blk);
        same();
    }
    CASE("SaveValidate") {
        fresh_save();
        old_run(old_abi_SaveValidate);
        save_validate();
        same();
    }
    CASE("SaveUnlocksCheck") {
        fresh_save();
        /* the checksum at $FC14: the true one now and then, else a random byte */
        pakram[0xFC14] = rnd() % 2 ? unlock_checksum_of_save() : (uint8_t)rnd();
        pakram[0xFC15] = (uint8_t)rnd();
        old_run(old_abi_SaveUnlocksCheck);
        save_unlocks_check();
        same();
    }
    CASE("RecordNameLetters / RecordNameLettersSmall / RecordNameLettersLarge") {
        int k = (int)(rnd() % 3);
        uint16_t letters = (uint16_t)rnd(), y = (uint16_t)((rnd() % 0x100) * 4);

        fresh_w();
        old_cpu.a = letters;
        old_cpu.y = y;
        old_run(k == 0 ? old_abi_RecordNameLetters : k == 1 ? old_abi_RecordNameLettersSmall
                : old_abi_RecordNameLettersLarge);
        if (k == 0)
            record_name_letters(letters, y);
        else if (k == 1)
            record_name_letters_small(letters, y);
        else
            record_name_letters_large(letters, y);
        same();
    }
    CASE("RecordLetterTile") {
        uint16_t letter = (uint16_t)(rnd() % 32), got;

        fresh_w();
        old_cpu.a = letter;
        old_run(old_abi_RecordLetterTile);
        got = record_letter_tile(letter);
        if (old_cpu.a != got)
            fail("RecordLetterTile (letter %u) = %04X, the decomp's %04X", letter, got, old_cpu.a);
        same();
    }
    CASE("RecordTagCarSet") {
        uint16_t a = (uint16_t)rnd();
        uint8_t got;

        fresh_w();
        old_cpu.a = a;
        old_run(old_abi_RecordTagCarSet);
        got = record_tag_car_set((uint8_t)a);
        if ((uint8_t)old_cpu.a != got)
            fail("RecordTagCarSet (tag %u) = %02X, the decomp's %02X", (uint8_t)a, got, (uint8_t)old_cpu.a);
        same();
    }
    CASE("RecordTimeDigitsLarge / RecordTimeDigitsSmall") {
        int k = (int)(rnd() & 1);
        uint16_t packed = rnd() % 8 == 0 ? 0xFFFF : (uint16_t)rnd();
        uint16_t y = (uint16_t)((rnd() % 0x100) * 4);

        fresh_w();
        old_cpu.a = packed;
        old_cpu.y = y;
        old_run(k ? old_abi_RecordTimeDigitsSmall : old_abi_RecordTimeDigitsLarge);
        if (k)
            record_time_digits_small(packed, y);
        else
            record_time_digits_large(packed, y);
        same();
    }
    CASE("RecordScoreDigits") {
        uint16_t score = (uint16_t)rnd(), y = (uint16_t)((rnd() % 0x100) * 4);

        fresh_w();
        old_cpu.a = score;
        old_cpu.y = y;
        old_run(old_abi_RecordScoreDigits);
        record_score_digits(score, y);
        same();
    }
    CASE("RecordRowDraw") {
        uint8_t page = (uint8_t)rnd(), time = (uint8_t)rnd(), name0 = (uint8_t)rnd(),
                name1 = (uint8_t)rnd(), car = (uint8_t)(rnd() % 4);
        uint16_t sprite = (uint16_t)(rnd() % 0x80);
        uint8_t index = (uint8_t)(page + time);

        fresh_w();
        shape_row(page);
        W8(0x00F2) = time;
        W8(0x00F3) = name0;
        W8(0x00F4) = name1;
        W8(0x00F5) = car;
        W8(0x00F6) = (uint8_t)rnd();
        if (rnd() % 4 == 0)
            W8(0x0F77 + index) = 0xFF;
        old_cpu.y = sprite;
        old_run(old_abi_RecordRowDraw);
        record_row_draw(sprite, index, name0, name1, car);
        same();
    }
    CASE("RecordRow1 / RecordRow2 / RecordRow3") {
        unsigned n = (unsigned)(rnd() % 3);
        uint8_t page = (uint8_t)rnd(), time = (uint8_t)rnd();

        fresh_w();
        shape_row(page);
        W8(0x00F2) = time;
        old_run(n == 0 ? old_abi_RecordRow1 : n == 1 ? old_abi_RecordRow2 : old_abi_RecordRow3);
        if (n == 0)
            record_row1(page, time);
        else if (n == 1)
            record_row2(page, time);
        else
            record_row3(page, time);
        same();
    }
    CASE("RecordRow1Time0 / RecordRow2Time4 / RecordRow3Time8") {
        unsigned n = (unsigned)(rnd() % 3);
        uint8_t page = (uint8_t)rnd();

        fresh_w();
        shape_row(page);
        old_run(n == 0 ? old_abi_RecordRow1Time0 : n == 1 ? old_abi_RecordRow2Time4 : old_abi_RecordRow3Time8);
        if (n == 0)
            record_row1_time0(page);
        else if (n == 1)
            record_row2_time4(page);
        else
            record_row3_time8(page);
        same();
    }
    CASE("RecordRowsGroupFirsts / RecordRowsFirstTimes") {
        int k = (int)(rnd() & 1);
        uint8_t page = (uint8_t)rnd();

        fresh_w();
        shape_row(page);
        old_run(k ? old_abi_RecordRowsFirstTimes : old_abi_RecordRowsGroupFirsts);
        if (k)
            record_rows_first_times(page);
        else
            record_rows_group_firsts(page);
        same();
    }
    CASE("RecordTimeCompare") {
        uint16_t x = (uint16_t)(rnd() & 1 ? 3 : 0);
        uint8_t best[5];
        unsigned i;

        fresh_w();
        for (i = 0; i < 5; i++) {
            best[i] = (uint8_t)(rnd() % 16);       /* the best time's digits, $0F69-$0F6D */
            W8(0x0F69 + i) = best[i];
        }
        if (rnd() % 3 == 0) {                      /* the record equal to the best now and then */
            W8(0x0F5B + x) = (uint8_t)((best[3] << 4) | best[4]);
            W8(0x0F5C + x) = (uint8_t)((best[1] << 4) | best[2]);
            W8(0x0F5D + x) = (uint8_t)((W8(0x0F5D + x) & 0xF0) | best[0]);
        }
        W16(0x0F2B) = rnd() % 6 == 0 ? 0xFFFF : (uint16_t)rnd();
        W8(0x0E57) = rnd() % 3 == 0 ? 0 : (uint8_t)rnd();
        old_cpu.x = x;
        old_run(old_abi_RecordTimeCompare);
        record_time_compare(x);
        same();
    }
    CASE("RecordCompareTime0 / RecordCompareTime1") {
        static const uint16_t modes[4] = {0x0000, 0x0002, 0x0001, 0x0005};
        int k = (int)(rnd() & 1);

        fresh_w();
        W16(0x0D2B) = rnd() % 2 ? modes[rnd() % 4] : (uint16_t)rnd();
        W16(0x0F2B) = rnd() % 6 == 0 ? 0xFFFF : (uint16_t)rnd();
        W8(0x0E57) = rnd() % 3 == 0 ? 0 : (uint8_t)rnd();
        W8(0x0F5B) = (uint8_t)rnd();
        W8(0x0F5C) = (uint8_t)rnd();
        W8(0x0F5D) = (uint8_t)rnd();
        W8(0x0F5E) = (uint8_t)rnd();
        W8(0x0F5F) = (uint8_t)rnd();
        W8(0x0F60) = (uint8_t)rnd();
        old_run(k ? old_abi_RecordCompareTime1 : old_abi_RecordCompareTime0);
        if (k)
            record_compare_time1();
        else
            record_compare_time0();
        same();
    }
    CASE("SpecialRecordsChecksum") {
        uint8_t course = (uint8_t)(rnd() % 0x10);

        fresh_w();
        W8(0x0E3D) = course;
        W16(0x0D14) = (uint16_t)(48 * course);   /* the course's offset in the save */
        old_run(old_abi_SpecialRecordsChecksum);
        special_records_checksum();
        same();
    }
    CASE("SpecialRecordInsert") {
        static const uint16_t modes[3] = {0x0000, 0x0002, 0x0001};
        uint16_t t = rnd() % 8 == 0 ? 0xFFFF : (uint16_t)rnd();

        fresh_w();
        W16(0x0D2B) = modes[rnd() % 3];
        W16(0x0D48) = (uint16_t)rnd();
        W8(0x0E3D) = (uint8_t)(rnd() % 0x10);   /* the courses $00-$0F; $0C and up change nothing */
        W8(0x0D89) = (uint8_t)(rnd() % 6);      /* the cars 0-3; 4 and 5 change nothing */
        W16(0x0F11) = (uint16_t)rnd();
        old_cpu.a = t;
        old_run(old_abi_SpecialRecordInsert);
        special_record_insert(t);
        same();
    }
    printf("records_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
