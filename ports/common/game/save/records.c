/*
 * records.c: the saved records in the game pak RAM (bank $70): loading a
 * course's records for the course screens, unpacking them for the records
 * screens, drawing the records panel's characters, and recording the times
 * of a race.
 *
 * The saves: the time-up run's four records at $70:FC4A (five bytes each:
 * a packed time, a name word and the car; sums at $70:FC46); the course
 * records' packed times at $70:FC88 (twelve a course, 24 bytes) and the
 * best two of a normal course at $70:FFD0 (four bytes a course); the
 * course names at $70:FC6D (nine bytes a course); and the special courses'
 * entries at $70:FCD0 + course * 48 + car * 12 (three entries of a score
 * word and a name word, best first; sums at $70:FC33 + course).
 */
#include <stdbool.h>
#include "records.h"
#include "oam.h"

/* A records entry as the records panel reads it: a score word (for Stunt
   Trax, the score; for the other courses, a packed time) and a name word. */
typedef struct {
    uint16_t score, name;
} RecordEntry;

/* The sum of the five bytes at pak offset `at` + x, the index taken as a
   byte (it wraps at $FF, as the original's index does). */
static uint8_t record_sum5(uint16_t at, uint8_t x)
{
    uint8_t sum = 0;
    unsigned i;

    for (i = 0; i < 5; i++)
        sum = (uint8_t)(sum + P8(at + (uint8_t)(x + i)));
    return sum;
}

/* The sums of the four time-up records into $70:FC46-$FC49. */
static void save_record_sums_time_up(void)
{
    unsigned i;

    for (i = 0; i < 4; i++)
        P8(0xFC46 + i) = record_sum5(0xFC4A, (uint8_t)(i * 5));
}

/* $0FA3, the time-up run's lead: 0 equal to the record, 1 slower, $FF a
   new record. */
static void set_time_up_lead(uint8_t v)
{
    W8(0x0FA3) = v;
}

/* The pak offset of course c's entries: 48 bytes a course. */
static uint16_t course_entries_offset(uint16_t c)
{
    return (uint16_t)((3u * c) << 4);
}

/* Swaps the words at work RAM address a and a + 2. */
static void swap_words_at(uint16_t a)
{
    uint16_t t = W16(a);

    W16(a) = W16(a + 2);
    W16(a + 2) = t;
}

/* Loads a course's twelve entries from pak offset `from` (score word, name
   word, four bytes each) into the times at $0F2B, the name words at $0F43
   and the tags at $0F13 (from the ROM table at $07:B4EB), then sorts the
   three arrays together by the times: descending (a larger time first)
   or ascending. */
static void records_load_sorted(uint16_t from, bool descending)
{
    uint16_t x = from, at;
    unsigned k, y;

    for (y = 0; y < 12; y++, x = (uint16_t)(x + 4)) {
        W16(0x0F2B + 2 * y) = P16(0xFCD0 + x);
        W16(0x0F43 + 2 * y) = P16(0xFCD2 + x);
        W16(0x0F13 + 2 * y) = rom_word(0x07B4EB + 2 * y);
    }
    for (k = 11; k-- > 0;) {
        at = (uint16_t)(2 * k);
        for (;;) {
            uint16_t v = W16(0x0F2D + at), c = W16(0x0F2B + at);

            if (descending ? v <= c : v >= c)
                break;
            swap_words_at(0x0F2B + at);
            swap_words_at(0x0F13 + at);
            swap_words_at(0x0F43 + at);
            if (at >= 0x14)
                break;
            at = (uint16_t)(at + 2);
        }
    }
}

/* The player's place (1-3) among the first three tags, by the tag at
   $10C7; 0 if it is none of them. Stored at $10C9. */
static uint16_t records_player_place(void)
{
    uint16_t tag = W16(0x10C7), place = 0;
    int i;

    for (i = 0; i < 3; i++) {
        if (tag == W16(0x0F13 + 2 * i)) {
            place = (uint16_t)(i + 1);
            break;
        }
    }
    W16(0x10C9) = place;
    return place;
}

/* A packed time unpacked to BCD: hundredths, seconds, minutes at `at`. */
static void store_time_unpacked(uint16_t at, uint16_t packed)
{
    BcdTime t = time_unpack(packed);

    W8(at) = t.hund;
    W8(at + 1) = t.sec;
    W8(at + 2) = t.min;
}

/*
 * The records of a special course ($0C-$0F, the course at $0E3D) for the
 * course screens: loaded and sorted descending, then the player's place.
 * Other courses: nothing.
 */
void records_load_special_course(void)
{
    uint16_t c = W8(0x0E3D);

    if (c < 0x0C || c >= 0x10)
        return;
    records_load_sorted(course_entries_offset(c), true);
    records_player_place();
}

/* The caller's data bank is the original's concern only; the port reads
   the same records. */
void records_load_special_course_long(void)
{
    records_load_special_course();
}

/*
 * A normal course's records (below $0C) for the course screens: loaded and
 * sorted ascending; the player's place; then the course's two best
 * lap-time records ($70:FFD0 + course * 4) and the first of the sorted
 * times, each unpacked to BCD at $0F5B, $0F5E and $0F61.
 */
void records_load_course(void)
{
    uint16_t c = W8(0x0E3D), x;

    if (c < 0x0C)
        records_load_sorted(course_entries_offset(c), false);
    records_player_place();
    x = (uint16_t)(c << 2);
    store_time_unpacked(0x0F5B, P16(0xFFD0 + x));
    store_time_unpacked(0x0F5E, P16(0xFFD2 + x));
    store_time_unpacked(0x0F61, W16(0x0F2B));
}

void records_load_course_long(void)
{
    records_load_course();
}

/* The records of the course at $0E3D for the course screens: the special
   courses, the normal ones, nothing from $10 on. */
void records_load_for_course(void)
{
    uint8_t c = W8(0x0E3D);

    if (c < 0x0C)
        records_load_course();
    else if (c < 0x10)
        records_load_special_course();
}

/*
 * The course's records (class $0D4A) for the records screens: its twelve
 * times into record_times (each BCD: hundredths, seconds, minutes; from
 * $70:FC88 + class * 24), and its nine name bytes at $10CF (from
 * $70:FC6D + class * 9).
 */
void records_unpack_course(void)
{
    uint16_t x = (uint16_t)(((3u * W8(0x0D4A)) & 0xFF) << 3);
    uint8_t c = (uint8_t)(9 * W8(0x0D4A));
    unsigned i;

    for (i = 0; i < 12; i++)
        record_times[i / 4][i % 4] = time_unpack(P16(0xFC88 + x + 2 * i));
    for (i = 0; i < 9; i++)
        W8(0x10CF + i) = P8(0xFC6D + c + i);
}

void records_unpack_course_totals(void)
{
    records_unpack_course();
    time_totals_sum();
}

void records_unpack_course_running(void)
{
    records_unpack_course();
    time_running_totals();
}

/*
 * The records panel's character: the tilemap offset of its place. The
 * row's start (a ROM word at `row_table`, row $0FA8), + place ($0FAA) * 64,
 * + the character's column (a ROM byte at `char_table`, character $0FA6).
 */
static uint16_t records_char_offset(uint32_t row_table, uint32_t char_table)
{
    uint16_t f3 = rom_word(row_table + (uint16_t)(W16(0x0FA8) << 1));

    f3 = (uint16_t)(f3 + ((W16(0x0FAA) & 0xFF) << 6));
    f3 = (uint16_t)(f3 + (rom_word(char_table + W16(0x0FA6)) & 0xFF));
    return f3;
}

/*
 * The records panel's entry: place $0FAA of row $0FA8 of the course at
 * $0E3D. Its pak offset is ((course * 3, low byte) * 16) + row * 12 +
 * place * 4 from $70:FCD0.
 */
static RecordEntry records_entry_load(void)
{
    uint16_t x = (uint16_t)(((3u * W8(0x0E3D)) & 0xFF) << 4);
    uint16_t row = (uint16_t)(2 * (W16(0x0FA8) & 0xFF) + W16(0x0FA8));
    RecordEntry e;

    x = (uint16_t)(x + (uint16_t)(row << 2));
    x = (uint16_t)(x + (uint16_t)(W16(0x0FAA) << 2));
    e.name = P16(0xFCD2 + x);
    e.score = P16(0xFCD0 + x);
    return e;
}

/*
 * Stunt Trax's character (0-6, $0FA6): 0-1 the score's high byte as two
 * digits (a high byte of $40 or more shows the blanks $6D and $6E), 2-3
 * the high and low nibbles of the score word's low byte, and 4-6 the three
 * 5-bit letters of the name word, each + $0A.
 */
static uint16_t records_char_tile_stunt(RecordEntry e)
{
    uint8_t hi = (uint8_t)(e.score >> 8);

    switch (W16(0x0FA6)) {
    case 0:
        return hi >= 0x40 ? 0x006D : (uint16_t)((hi >> 4) & 0x0F);
    case 1:
        return hi >= 0x40 ? 0x006E : (uint16_t)(hi & 0x0F);
    case 2:
        return (uint16_t)((e.score >> 4) & 0x0F);
    case 3:
        return (uint16_t)(e.score & 0x0F);
    case 4:
        return (uint16_t)((e.name & 0x1F) + 0x0A);
    case 5:
        return (uint16_t)(((e.name >> 5) & 0x1F) + 0x0A);
    case 6:
        return (uint16_t)(((e.name >> 10) & 0x1F) + 0x0A);
    default:
        /* the characters past 6 do not occur; the original dispatches them
           through a jump table, which is not ported */
        return 0;
    }
}

/*
 * The other courses' character (0-7): 0 the minutes (bits 13-15 of the
 * packed time), 1-2 the seconds' tens and units (bits 7-12), 3-4 the
 * hundredths' tens and units (bits 0-6), each as a BCD digit; 5-7 the name
 * word's letters, each + $0A.
 */
static uint16_t records_char_tile_all(RecordEntry e)
{
    switch (W16(0x0FA6)) {
    case 0:
        return (uint16_t)((e.score >> 13) & 0x07);
    case 1:
        return (uint16_t)(binary_to_bcd((e.score >> 7) & 0x3F) >> 4);
    case 2:
        return (uint16_t)(binary_to_bcd((e.score >> 7) & 0x3F) & 0x0F);
    case 3:
        return (uint16_t)(binary_to_bcd(e.score & 0x7F) >> 4);
    case 4:
        return (uint16_t)(binary_to_bcd(e.score & 0x7F) & 0x0F);
    case 5:
        return (uint16_t)((e.name & 0x1F) + 0x0A);
    case 6:
        return (uint16_t)(((e.name >> 5) & 0x1F) + 0x0A);
    case 7:
        return (uint16_t)(((e.name >> 10) & 0x1F) + 0x0A);
    default:
        /* the characters past 7 do not occur; the original dispatches them
           through a jump table, which is not ported */
        return 0;
    }
}

/* The tile's low byte at the tilemap buffer $7F:93D7 + off. */
static void records_char_store(uint16_t off, uint16_t tile)
{
    W8(0x193D7 + off) = (uint8_t)tile;
}

/* Stunt Trax's character of the records panel, drawn unless its entry's
   score is 0. */
void preview_row_draw_stunt(void)
{
    uint16_t off = records_char_offset(0x07D8D4, 0x07D8DC);
    RecordEntry e = records_entry_load();

    if (e.score == 0)
        return;
    records_char_store(off, records_char_tile_stunt(e));
}

/* The other courses' character of the records panel, drawn unless its
   entry's score is $FFFF (no time). */
void preview_row_draw_all(void)
{
    uint16_t off = records_char_offset(0x07D8E3, 0x07D8EB);
    RecordEntry e = records_entry_load();

    if (e.score == 0xFFFF)
        return;
    records_char_store(off, records_char_tile_all(e));
}

/* The time-up run's end: its time ($0E63 hundredths, $0E64 seconds, $0E66
   minutes, all BCD) as five digits at $0F69-$0F6D, then packed and set
   against the record of the course at $0E3D - $17 (five bytes each, from
   $70:FC4A). Lower than the record, it replaces the record with the name
   $0F11 and the car $0D89, then the sums are made again; higher, lead 1;
   equal, lead 0. The lead is kept at $0FA3. */
void time_up_record_update(void)
{
    BcdTime now = {W8(0x0E63), W8(0x0E64), W8(0x0E66)};
    uint8_t x = (uint8_t)(5 * (W8(0x0E3D) - 0x17));
    uint16_t t, r;

    W8(0x0F69) = now.min & 0x0F;
    W8(0x0F6A) = now.sec >> 4;
    W8(0x0F6B) = now.sec & 0x0F;
    W8(0x0F6C) = now.hund >> 4;
    W8(0x0F6D) = now.hund & 0x0F;

    t = time_pack(now);
    r = P16(0xFC4A + x);
    if (t == r) {
        set_time_up_lead(0x00);
        return;
    }
    if (t > r) {
        set_time_up_lead(0x01);
        return;
    }
    P16(0xFC4A + x) = t;
    P16(0xFC4C + x) = W16(0x0F11);
    P8(0xFC4E + x) = W8(0x0D89);
    set_time_up_lead(0xFF);
    save_record_sums_time_up();
}

/* The car's tag: $0D89 * 12 (low byte) plus `extra`, stored at $10C7. */
static void record_tag(uint16_t extra)
{
    uint16_t car12 = (uint16_t)((12u * W8(0x0D89)) & 0xFF);

    W16(0x10C7) = (uint16_t)(car12 + extra);
}

/* The pak entry word at offset x + from copied to x + to. */
static void special_move(uint16_t x, uint16_t from, uint16_t to)
{
    P16(0xFCD0 + x + to) = P16(0xFCD0 + x + from);
}

/*
 * Stunt Trax's score: the high byte $19EC, the low the BCD sum of $0E70
 * and $19F0. For courses $0C-$0F it goes into the car's three best (at
 * $70:FCD0 + course * 48 + car * 12: a score word, then its name word at
 * +2; the second at +4 and +6; the third at +8 and +10) if it beats one of
 * them: a score above the first moves the others down (the third is lost)
 * and tags the car (tag $10C7 = car * 12); above the second or the third
 * likewise, with the tags + 4 and + 8. The course's 48 entry bytes are
 * summed into $70:FC33 + course.
 *
 * Other courses: the original leaves through a jump to the score it pushed
 * as a return address. In play only courses $0C-$0F reach this, so for the
 * other courses the port does nothing.
 */
void stunt_record_time(void)
{
    bool carry = false;
    uint16_t score, x, base, course = W8(0x0E3D), car = W8(0x0D89);
    uint8_t sum = 0;
    unsigned i;

    score = (uint16_t)((W8(0x19EC) << 8) | bcd_add8(W8(0x0E70), W8(0x19F0), &carry));
    if (course < 0x0C || course >= 0x10)
        return;
    base = course_entries_offset(course);
    W16(0x0D14) = base;
    x = (uint16_t)(base + 12 * car);

    if (score > P16(0xFCD0 + x)) {
        record_tag(0);
        special_move(x, 4, 8);
        special_move(x, 6, 10);
        special_move(x, 0, 4);
        special_move(x, 2, 6);
        P16(0xFCD0 + x + 2) = W16(0x0F11);
        P16(0xFCD0 + x) = score;
    } else if (score > P16(0xFCD0 + x + 4)) {
        record_tag(4);
        special_move(x, 4, 8);
        special_move(x, 6, 10);
        P16(0xFCD0 + x + 4) = score;
        P16(0xFCD0 + x + 6) = W16(0x0F11);
    } else if (score > P16(0xFCD0 + x + 8)) {
        P16(0xFCD0 + x + 8) = score;
        record_tag(8);
        P16(0xFCD0 + x + 10) = W16(0x0F11);
    } else {
        return;
    }
    for (i = 0; i < 48; i++)
        sum = (uint8_t)(sum + P8(0xFCD0 + base + i));
    P8(0xFC33 + course) = sum;
}

/* ---- the save's checks: SaveValidate and what it runs ---- */

/* SaveRecordSumB: the sum of the five bytes of the record at pak $FC5E + x. */
uint8_t save_record_sum_b(uint8_t x)
{
    return record_sum5(0xFC5E, x);
}

/* SaveSumsRecordsB: the sums of the first three records at $FC5E go to the
   sum bytes $FC43-$FC45; returns the fourth record's sum. */
uint8_t save_sums_records_b(void)
{
    unsigned i;

    for (i = 0; i < 3; i++)
        P8(0xFC43 + i) = save_record_sum_b((uint8_t)(i * 5));
    return save_record_sum_b(15);
}

/* Checks `count` records of five bytes at pak `base` against their sum
   bytes at `sums`. A record whose sum differs, or whose first byte is 0, is
   reset to FF FF 00 00 00 with sum byte $FE. Returns the last record's first
   byte, or $FE when the last record was reset. */
static uint8_t records_check(uint16_t base, uint16_t sums, unsigned count)
{
    uint8_t x = 0, a = 0;
    unsigned y;

    for (y = 0; y < count; y++, x = (uint8_t)(x + 5)) {
        a = record_sum5(base, x);
        if (a == P8(sums + y)) {
            a = P8(base + x);
            if (a)
                continue;
        }
        P8(base + x) = 0xFF;
        P8(base + x + 1) = 0xFF;
        P8(base + x + 2) = 0x00;
        P8(base + x + 3) = 0x00;
        P8(base + x + 4) = 0x00;
        P8(sums + y) = 0xFE;
        a = 0xFE;
    }
    return a;
}

/* SaveRecordsCheckA: the four records at $FC4A against their sums at $FC46. */
uint8_t save_records_check_a(void)
{
    return records_check(0xFC4A, 0xFC46, 4);
}

/* SaveRecordsCheckB: the three records at $FC5E against their sums at $FC43. */
uint8_t save_records_check_b(void)
{
    return records_check(0xFC5E, 0xFC43, 3);
}

/*
 * SaveBlocksCheck: blocks 12-15 (48 bytes each, at pak $FCD0 + 48 * block,
 * which wraps past $FFFF into $0F10 onward). Each entry's second byte must
 * be below $41, and the 8-bit sum of the 48 bytes must equal the block's
 * sum byte at $FC33 + block. A block that fails is reset to twelve entries
 * of 00 00 FF FF, with sum byte $E8.
 */
void save_blocks_check(void)
{
    uint8_t blk;

    for (blk = 12; blk < 16; blk++) {
        uint16_t base = (uint16_t)(blk * 48), x;
        uint8_t sum = 0, v;
        unsigned i;
        bool bad = false;

        for (i = 0, x = base; i < 12; i++, x = (uint16_t)(x + 4)) {
            sum = (uint8_t)(sum + P8((uint16_t)(0xFCD0 + x)));
            v = P8((uint16_t)(0xFCD1 + x));
            if (v >= 0x41) {
                bad = true;
                break;
            }
            sum = (uint8_t)(sum + v + P8((uint16_t)(0xFCD2 + x)) + P8((uint16_t)(0xFCD3 + x)));
        }
        if (!bad && sum == P8(0xFC33 + blk))
            continue;
        for (i = 0, x = base; i < 12; i++, x = (uint16_t)(x + 4)) {
            P8((uint16_t)(0xFCD0 + x)) = 0x00;
            P8((uint16_t)(0xFCD1 + x)) = 0x00;
            P8((uint16_t)(0xFCD2 + x)) = 0xFF;
            P8((uint16_t)(0xFCD3 + x)) = 0xFF;
        }
        P8(0xFC33 + blk) = 0xE8;
    }
}

/*
 * SaveTablesCheck: the three save tables, last first. Table k is twelve
 * two-byte entries at pak $FC88 + 24k (each second byte $1F or more) and
 * nine bytes at $FC6D + 9k; the 8-bit sum of its 33 bytes must equal the
 * sum byte at $FC2D + k. A failing table is reset to 24 bytes of $FF and
 * nine of 0, with sum byte $E8.
 */
void save_tables_check(void)
{
    static const uint8_t first[3] = {0x00, 0x18, 0x30};
    static const uint8_t second[3] = {0x00, 0x09, 0x12};
    int k;

    for (k = 2; k >= 0; k--) {
        uint16_t x, idx;
        uint8_t sum = 0, v;
        unsigned i;
        bool bad = false;

        for (i = 0, x = first[k]; i < 12; i++, x = (uint16_t)(x + 2)) {
            sum = (uint8_t)(sum + P8((uint16_t)(0xFC88 + x)));
            v = P8((uint16_t)(0xFC89 + x));
            if (v < 0x1F) {
                bad = true;
                break;
            }
            sum = (uint8_t)(sum + v);
        }
        if (!bad) {
            for (i = 0, idx = second[k]; i < 9; i++, idx++)
                sum = (uint8_t)(sum + P8((uint16_t)(0xFC6D + idx)));
            if (P8(0xFC2D + k) == sum)
                continue;
        }
        for (i = 0, x = first[k]; i < 24; i++, x = (uint16_t)(x + 1))
            P8((uint16_t)(0xFC88 + x)) = 0xFF;
        for (i = 0, idx = second[k]; i < 9; i++, idx++)
            P8((uint16_t)(0xFC6D + idx)) = 0x00;
        P8(0xFC2D + k) = 0xE8;
    }
}

/*
 * One entry byte of SaveBlocksValidate: $FF passes; otherwise the byte fails
 * by its position: byte 0 with its low seven bits $64 or more; byte 1 with
 * its low five bits $1E or more, or its top three bits 0; byte 2 with its
 * low five bits $1C or more. Byte 3's test (low two bits $38 or more) can
 * never be true.
 */
static bool entry_byte_bad(uint8_t v, unsigned which)
{
    if (v == 0xFF)
        return false;
    switch (which) {
    case 0: return (v & 0x7F) >= 0x64;
    case 1: return (v & 0x1F) >= 0x1E || (v & 0xE0) == 0x00;
    case 2: return (v & 0x1F) >= 0x1C;
    default: return (v & 0x03) >= 0x38;
    }
}

/* A block that fails (SaveBlocksValidate): $1FFE = $11; its 48 bytes at pak
   $FCD0 + base = $FF; its four group bytes at $FFD0 + 4 * block = $FF; its
   sum byte $CC. The block is then checked again. */
static void block_reset(uint8_t blk, uint16_t base)
{
    unsigned i, j;

    W8(0x1FFE) = 0x11;
    for (i = 0; i < 48; i++)
        P8((uint16_t)(0xFCD0 + base + i)) = 0xFF;
    for (j = 0; j < 4; j++)
        P8((uint16_t)(0xFFD0 + 4 * blk + j)) = 0xFF;
    P8(0xFC33 + blk) = 0xCC;
}

/* SaveBlocksValidate's loop from block `blk`: each block's check is the sum
   of its four group bytes at $FFD0 + 4 * block plus its 48 entry bytes,
   which must equal its sum byte at $FC33 + block, and every entry byte must
   pass entry_byte_bad. A failing block is reset (block_reset) and checked
   again, from the top of the loop; `reset_first` makes the first pass a
   reset (SaveBlocksFourthBad). Blocks run to 11. */
static void blocks_validate_from(uint8_t blk, bool reset_first)
{
    for (;;) {
        uint16_t base = (uint16_t)(blk * 48), x;
        uint8_t sum = 0, v;
        unsigned i, j;
        bool bad = reset_first;

        reset_first = false;
        if (!bad) {
            for (j = 0; j < 4; j++)
                sum = (uint8_t)(sum + P8((uint16_t)(0xFFD0 + 4 * blk + j)));
            for (i = 0, x = base; i < 12 && !bad; i++, x = (uint16_t)(x + 4)) {
                for (j = 0; j < 4; j++) {
                    v = P8((uint16_t)(0xFCD0 + x + j));
                    if (entry_byte_bad(v, j)) {
                        bad = true;
                        break;
                    }
                    sum = (uint8_t)(sum + v);
                }
            }
            if (!bad && sum != P8(0xFC33 + blk))
                bad = true;
        }
        if (bad) {
            block_reset(blk, base);
            continue;
        }
        blk++;
        if (blk >= 12)
            return;
    }
}

/* SaveBlocksValidate: blocks 0-11. */
void save_blocks_validate(void)
{
    blocks_validate_from(0, false);
}

/* SaveBlocksFourthBad: a fourth entry byte's failure (which cannot happen)
   resets block `blk` as a failure does, then the rest of the validation runs
   from it. */
void save_blocks_fourth_bad(uint8_t blk)
{
    blocks_validate_from(blk, true);
}

/* SaveValidate: the whole save: the tables, blocks 0-11, blocks 12-15, and
   the two record sets. */
void save_validate(void)
{
    save_tables_check();
    save_blocks_validate();
    save_blocks_check();
    save_records_check_b();
    save_records_check_a();
}

/* The unlock bytes of the save, in the order their checksum adds them. */
static const uint16_t unlock_bytes[10] = {
    0xFC0B, 0xFC0F, 0xFC06, 0xFC07, 0xFC0C, 0xFC0D, 0xFC0E, 0xFC08, 0xFC09, 0xFC0A};

/* The unlock checksum: an 8-bit add chain, each add taking the carry of the
   one before it. */
static uint8_t unlock_checksum(void)
{
    unsigned s = P8(unlock_bytes[0]), c = 0, i;

    for (i = 1; i < 10; i++) {
        s = s + P8(unlock_bytes[i]) + c;
        c = s >> 8;
        s &= 0xFF;
    }
    return (uint8_t)s;
}

/*
 * SaveUnlocksCheck: when the unlock bytes ($FC06-$FC0F) fail their checksum
 * at $FC14, they and the checksum word ($FC14-$FC15) are cleared; then the
 * whole save is validated (save_validate).
 */
void save_unlocks_check(void)
{
    unsigned i;

    if (unlock_checksum() != P8(0xFC14)) {
        for (i = 0; i < 10; i++)
            P8(unlock_bytes[i]) = 0x00;
        P8(0xFC14) = 0x00;
        P8(0xFC15) = 0x00;
    }
    save_validate();
}

/* ---- the records panel's characters, rows, compares and special courses ---- */

/* RecordNameLetters: the three 5-bit letters of `letters` (bits 0-4, 5-9,
   10-14), each + $0A, into the tile bytes at y + $039F, + 4 and + 8. */
void record_name_letters(uint16_t letters, uint16_t y)
{
    unsigned i;
    static const uint8_t shift[3] = {0, 5, 10};

    for (i = 0; i < 3; i++)
        W8((uint16_t)(0x039F + 4 * i + y)) = (uint8_t)(((letters >> shift[i]) & 0x1F) + 0x0A);
}

/* RecordNameLettersSmall: as record_name_letters, from y + $03B3. */
void record_name_letters_small(uint16_t letters, uint16_t y)
{
    unsigned i;
    static const uint8_t shift[3] = {0, 5, 10};

    for (i = 0; i < 3; i++)
        W8((uint16_t)(0x03B3 + 4 * i + y)) = (uint8_t)(((letters >> shift[i]) & 0x1F) + 0x0A);
}

/* RecordTagCarSet: the car's piece set for a record's tag t (its time
   offset, 0-$2F): 0 below $0C, 3 below $18, 6 below $24, else 9. */
uint8_t record_tag_car_set(uint8_t t)
{
    if (t >= 0x24)
        return 0x09;
    if (t >= 0x18)
        return 0x06;
    if (t >= 0x0C)
        return 0x03;
    return 0x00;
}

/* A large digit d (0-15) at sprite offset y: its top tile $50 + d at `at`
   and its bottom tile $60 + d four bytes on. */
static void large_digit(uint16_t at, uint16_t y, uint8_t d)
{
    W8((uint16_t)(at + y)) = (uint8_t)(d + 0x50);
    W8((uint16_t)(at + 4 + y)) = (uint8_t)(d + 0x60);
}

/* RecordTimeDigitsLarge: the packed time as five large digits: minutes at
   y + $0387, seconds at $0397 and $039F, hundredths at $03AF and $03B7. */
void record_time_digits_large(uint16_t packed, uint16_t y)
{
    BcdTime t = time_unpack(packed);

    large_digit(0x0387, y, t.min & 0x0F);
    large_digit(0x0397, y, t.sec >> 4);
    large_digit(0x039F, y, t.sec & 0x0F);
    large_digit(0x03AF, y, t.hund >> 4);
    large_digit(0x03B7, y, t.hund & 0x0F);
}

/* RecordTimeDigitsSmall: the packed time as five small digits: minutes at
   y + $0383, seconds at $038B and $038F, hundredths at $0397 and $039B. */
void record_time_digits_small(uint16_t packed, uint16_t y)
{
    BcdTime t = time_unpack(packed);

    W8((uint16_t)(0x0383 + y)) = t.min & 0x0F;
    W8((uint16_t)(0x038B + y)) = t.sec >> 4;
    W8((uint16_t)(0x038F + y)) = t.sec & 0x0F;
    W8((uint16_t)(0x0397 + y)) = t.hund >> 4;
    W8((uint16_t)(0x039B + y)) = t.hund & 0x0F;
}

/*
 * RecordScoreDigits: a Stunt Trax score. The low byte's low nibble at
 * y + $03A3 and its high nibble at $039F. Below $4000 the high byte's two
 * nibbles at $038B and $0387, and $F0 at $0382 and $039A. From $4000 on,
 * the word PERFECT in seven tile words from y + $0383, four bytes apart.
 */
void record_score_digits(uint16_t score, uint16_t y)
{
    static const uint16_t perfect[7] = {0x3619, 0x360E, 0x361B, 0x360F, 0x360E, 0x360C, 0x361D};
    unsigned i;

    W8((uint16_t)(0x03A3 + y)) = (uint8_t)(score & 0x0F);
    W8((uint16_t)(0x039F + y)) = (uint8_t)((score & 0xFF) >> 4);
    if (score >= 0x4000) {
        for (i = 0; i < 7; i++)
            W16((uint16_t)(0x0383 + 4 * i + y)) = perfect[i];
        return;
    }
    W8((uint16_t)(0x038B + y)) = (uint8_t)((score >> 8) & 0x0F);
    W8((uint16_t)(0x0387 + y)) = (uint8_t)((score >> 8) >> 4);
    W8((uint16_t)(0x0382 + y)) = 0xF0;
    W8((uint16_t)(0x039A + y)) = 0xF0;
}

/* RecordLetterTile: the top tile of large letter `letter` (0-31, + $0A so
   the letters follow the ten digits): $50 + the low four bits, + $20 for
   each of bits 4-5 doubled (the sheet's rows of 16). */
uint16_t record_letter_tile(uint16_t letter)
{
    uint16_t v = (uint16_t)(letter + 0x0A);

    return (uint16_t)(((v & 0x0F) | ((v & 0x30) << 1)) + 0x50);
}

/* A large letter at sprite offset y: its top tile at `at`, its bottom tile
   (+ $10) four bytes on. */
static void large_letter(uint16_t at, uint16_t y, uint16_t letter)
{
    uint8_t t = (uint8_t)record_letter_tile(letter);

    W8((uint16_t)(at + y)) = t;
    W8((uint16_t)(at + 4 + y)) = (uint8_t)(t + 0x10);
}

/* RecordNameLettersLarge: the three letters as large letters, at y + $03BF,
   $03C7 and $03CF. */
void record_name_letters_large(uint16_t letters, uint16_t y)
{
    large_letter(0x03BF, y, letters & 0x1F);
    large_letter(0x03C7, y, (letters >> 5) & 0x1F);
    large_letter(0x03CF, y, (letters >> 10) & 0x1F);
}

/*
 * RecordRowDraw: one records row from sprite number `sprite` (its offset
 * y = sprite * 4). `index` is the time's byte offset in the record times
 * (page + time, low byte); the name entry is its two letter bytes (name0,
 * name1) and its car byte. The seconds byte at the index being $FF hides
 * the row's first three sprites. Otherwise the three time bytes become digit
 * tiles, the car's three sprites are drawn (position $10BD = $57 and $10BF
 * set by the caller, the piece set from $10BB and $10C3), and the name's
 * letters go four bytes on from y.
 */
void record_row_draw(uint16_t sprite, uint8_t index, uint8_t name0, uint8_t name1, uint8_t car)
{
    uint16_t y = (uint16_t)(sprite << 2), x = index, ty = y, set;
    unsigned i;
    static const uint16_t hi[3] = {0x039B, 0x038F, 0x0383}, lo[3] = {0x039F, 0x0393, 0x0387};

    if (W8(0x0F77 + x) == 0xFF) {
        W8((uint16_t)(0x0376 + y)) = 0xF0;
        W8((uint16_t)(0x037A + y)) = 0xF0;
        W8((uint16_t)(0x037E + y)) = 0xF0;
        return;
    }
    for (i = 0; i < 3; i++) {
        W8((uint16_t)(hi[i] + y)) = (uint8_t)(W8(0x0F76 + i + x) >> 4);
        W8((uint16_t)(lo[i] + y)) = (uint8_t)(W8(0x0F76 + i + x) & 0x0F);
    }
    W16(0x10C3) = (uint16_t)((3 * car) & 0x0F);
    W16(0x10BD) = 0x0057;
    W16(0x10C5) = 0x0000;
    sprite_pieces3(0x7E, &x, &ty, &set);
    record_name_letters((uint16_t)(name0 | (name1 << 8)), (uint16_t)(y + 4));
}

/* Row n (0-2): its name entry ($10CF + n * 3), its screen Y ($7C + n * $11)
   at $10BF, then the row drawn from the sprite in $0D75 + n * 2 with the
   time index (page + time). */
static void record_row(unsigned n, uint8_t page, uint8_t time)
{
    uint8_t index = (uint8_t)(page + time);

    W8(0x10BF) = (uint8_t)(0x7C + n * 0x11);
    record_row_draw(W16(0x0D75 + 2 * n), index, W8(0x10CF + 3 * n), W8(0x10CF + 3 * n + 1),
                    W8(0x10CF + 3 * n + 2));
}

/* RecordRow1 / RecordRow2 / RecordRow3: a records row. `page` is the page's
   offset and `time` the time's offset in the group. */
void record_row1(uint8_t page, uint8_t time)
{
    record_row(0, page, time);
}

void record_row2(uint8_t page, uint8_t time)
{
    record_row(1, page, time);
}

void record_row3(uint8_t page, uint8_t time)
{
    record_row(2, page, time);
}

/* RecordRow1Time0, RecordRow2Time4, RecordRow3Time8: the first time of each
   group (offsets 0, $0C and $18). */
void record_row1_time0(uint8_t page)
{
    record_row(0, page, 0x00);
}

void record_row2_time4(uint8_t page)
{
    record_row(1, page, 0x0C);
}

void record_row3_time8(uint8_t page)
{
    record_row(2, page, 0x18);
}

/* RecordRowsGroupFirsts: the three rows with each group's first time. */
void record_rows_group_firsts(uint8_t page)
{
    record_row1_time0(page);
    record_row2_time4(page);
    record_row3_time8(page);
}

/* RecordRowsFirstTimes: the three rows with the times 0, 1 and 2 (offsets
   0, 3 and 6). */
void record_rows_first_times(uint8_t page)
{
    unsigned n;

    for (n = 0; n < 3; n++)
        record_row(n, page, (uint8_t)(n * 3));
}

/* $0FA3 takes the compare's result `r`; then sound $35 on port 0 unless the
   flag at $0E57 is set. */
static void compare_result(uint8_t r)
{
    W8(0x0FA3) = r;
    if (W8(0x0E57) == 0)
        sound_queue(0x35);
}

/*
 * RecordTimeCompare: the record time at $0F5B + x (BCD, its digits at
 * $0F64-$0F68 as minutes, seconds, hundredths) against the best time's
 * digits at $0F69-$0F6D, most significant first: $0FA3 = $FF when it is
 * better, 1 when it is worse or when there is no best time ($0F2B = $FFFF),
 * 0 when equal.
 */
void record_time_compare(uint16_t x)
{
    uint8_t v, m;
    unsigned i;

    if (W16(0x0F2B) == 0xFFFF) {
        compare_result(0x01);
        return;
    }
    v = W8(0x0F5B + x);
    W8(0x0F68) = v & 0x0F;
    W8(0x0F67) = v >> 4;
    v = W8(0x0F5C + x);
    W8(0x0F66) = v & 0x0F;
    W8(0x0F65) = v >> 4;
    v = W8(0x0F5D + x);
    W8(0x0F64) = v & 0x0F;
    for (i = 0; i < 5; i++) {
        v = W8(0x0F64 + i);
        m = W8(0x0F69 + i);
        if (v < m) {
            compare_result(0xFF);
            return;
        }
        if (v != m) {
            compare_result(0x01);
            return;
        }
    }
    compare_result(0x00);
}

/* By the mode at $0D2B: 0 queues sound $35 on port 0; 2 queues it too (the
   player's sound, the same call); any other mode compares the record time at
   `x` (record_time_compare). */
static void record_compare_by_mode(uint16_t x)
{
    uint16_t mode = W16(0x0D2B);

    if (mode == 0x0000 || mode == 0x0002)
        sound_queue(0x35);
    else
        record_time_compare(x);
}

/* RecordCompareTime0 / RecordCompareTime1: record time 0 (offset 0) or 1
   (offset 3) against the best time, by the mode. */
void record_compare_time0(void)
{
    record_compare_by_mode(0x0000);
}

void record_compare_time1(void)
{
    record_compare_by_mode(0x0003);
}

/*
 * SpecialRecordsChecksum: the sum byte of a special course's records: the
 * sum (mod 256) of its four packed lap times at $70:FFD0 + course * 4 (none
 * for course $0C or more) and its 48 bytes from $70:FCD0 + $0D14, stored at
 * $70:FC33 + course.
 */
void special_records_checksum(void)
{
    uint8_t course = W8(0x0E3D), sum = 0;
    uint16_t base = W16(0x0D14);
    unsigned i;

    if (course < 0x0C)
        for (i = 0; i < 4; i++)
            sum = (uint8_t)(sum + P8(0xFFD0 + 4 * course + i));
    for (i = 0; i < 48; i++)
        sum = (uint8_t)(sum + P8((uint16_t)(0xFCD0 + base + i)));
    P8(0xFC33 + course) = sum;
}

/* One lap's packed time (TimePackBcd) from the three BCD bytes at work RAM
   `at` (hundredths, seconds, minutes). */
static uint16_t special_pack(uint16_t at)
{
    BcdTime t = {W8(at), W8((uint16_t)(at + 1)), W8((uint16_t)(at + 2))};

    return time_pack(t);
}

/*
 * SpecialRecordInsert: the packed time t as a special course's record for
 * car $0D89, in the course's entries at $70:FCD0 + course * 48 + car * 12
 * (each a score word and a name word $0F11, best first). t below the first
 * moves the others down (the third is lost) and tags the car's first place;
 * then, unless t is still above another of the course's entries, the lap
 * times ($0EF0 and $0EF3, packed) are kept at $70:FFD0 + course * 4. t below
 * the second or the third likewise (tags 4 and 8). Not below the third: the
 * tag $10C7 = $FFFF and nothing else. The checksum follows each change. The
 * mode at $0D2B = 0 sets the word at $0F6E + the low byte of $0D48 * 2 to
 * that offset. A course $0C or more, or a car 4 or more, changes no entries.
 */
void special_record_insert(uint16_t t)
{
    uint16_t course, car, base, x, n, at;

    if (W16(0x0D2B) == 0x0000) {
        uint16_t v = (uint16_t)((uint16_t)(W16(0x0D48) << 1) & 0x00FF);

        W16(0x0F6E + v) = v;
    }
    course = W8(0x0E3D);
    if (course >= 0x0C)
        return;
    base = (uint16_t)(course * 48);
    W16(0x0D14) = base;
    car = W8(0x0D89);
    if (car >= 4)
        return;
    x = (uint16_t)(base + 12 * car);

    if (t < P16(0xFCD0 + x)) {
        record_tag(0);
        special_move(x, 4, 8);
        special_move(x, 6, 10);
        special_move(x, 0, 4);
        special_move(x, 2, 6);
        P16(0xFCD0 + x + 2) = W16(0x0F11);
        P16(0xFCD0 + x) = t;
        for (n = 0, at = base; n < 12; n++, at = (uint16_t)(at + 4)) {
            if (at != x && t >= P16(0xFCD0 + at)) {
                special_records_checksum();
                return;
            }
        }
        P16(0xFFD0 + 4 * course) = special_pack(0x0EF0);
        P16(0xFFD2 + 4 * course) = special_pack(0x0EF3);
        special_records_checksum();
        return;
    }
    if (t < P16(0xFCD0 + x + 4)) {
        record_tag(4);
        special_move(x, 4, 8);
        special_move(x, 6, 10);
        P16(0xFCD0 + x + 4) = t;
        P16(0xFCD0 + x + 6) = W16(0x0F11);
        special_records_checksum();
        return;
    }
    if (t >= P16(0xFCD0 + x + 8)) {
        W16(0x10C7) = 0xFFFF;
        return;
    }
    P16(0xFCD0 + x + 8) = t;
    record_tag(8);
    P16(0xFCD0 + x + 10) = W16(0x0F11);
    special_records_checksum();
}
