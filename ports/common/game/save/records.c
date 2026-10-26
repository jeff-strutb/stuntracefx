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
