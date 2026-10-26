/*
 * gsu_record.c: each object's record in the game pak RAM (bank $70), which
 * the renderer reads. An object's +$22 holds its record's address, 0 when
 * it has none. The routines here copy an object's state into its record
 * and back: its position (+$0C/+$0E/+$10 to the record's +$1E/+$20/+$22),
 * its heading, velocity and angles, and the flag and step words the Super
 * FX programs read. Also the contact list the car touches each frame, and
 * the records the course gives new objects.
 */
#include "game.h"
#include "objects.h"
#include "fixmath.h"
#include "glue.h"
#include "object_more.h"
#include "gsu_record.h"

/* A field of a record: the record at `rec` (in the game pak RAM), `off`. */
#define REC_W(rec, off) P16((uint16_t)((rec) + (off)))
#define REC_B(rec, off) P8((uint16_t)((rec) + (off)))

/* The object's parent (its +$06): the object it hangs from, 0 if none. */
#define obj_parent(o)   OBJ_W(o, 0x06)

/* Work RAM words the routines share. */
#define touch_next      W16(0x1F3C)     /* the contact list's next entry, a record */
#define touch_word      W16(0x1F3A)     /* the touched record's contact word */
#define touch_flags     W16(0x1F36)     /* the touched record's flags */
#define touch_sum       W16(0x1F38)     /* the contact flags, summed */
#define record_arg_0a   W16(0x1F4C)     /* the +$0A word a new record takes */
#define step_size       W16(0x1F2B)     /* the step record_step_6a adds */
#define bits_60_a       W16(0x1A24)
#define bits_60_b       W16(0x1A26)

/* The record a Super FX program makes, left by it in the game pak RAM. */
#define program_record P16(0x238E)

/* Sets the record's mode word (+$1A) to 2. Returns A: 2, or 0 with no
   record. */
uint16_t record_set_mode_2(uint16_t obj)
{
    uint16_t rec = obj_record(obj);

    if (!rec)
        return 0;
    REC_W(rec, 0x1A) = 0x0002;
    return 0x0002;
}

/* Sets the bits `bits` in the record's flags (+$16). Returns the new flags,
   0 with no record. */
uint16_t record_or_flags(uint16_t obj, uint16_t bits)
{
    uint16_t rec = obj_record(obj), w = 0;

    if (rec) {
        w = (uint16_t)(REC_W(rec, 0x16) | bits);
        REC_W(rec, 0x16) = w;
    }
    return w;
}

/* The record's flags: bit 4 set, bit 8 clear. Returns the new flags, 0 with
   no record. */
uint16_t record_flags_bit4_on_bit8_off(uint16_t obj)
{
    uint16_t rec = obj_record(obj), w = 0;

    if (rec) {
        w = (uint16_t)((REC_W(rec, 0x16) | 0x0010) & 0xFEFF);
        REC_W(rec, 0x16) = w;
    }
    return w;
}

/* Copies a position into the record (+$0C, +$0E, +$10). Returns the z
   value, 0 with no record. */
uint16_t record_set_position(uint16_t obj, uint16_t x, uint16_t y, uint16_t z)
{
    uint16_t rec = obj_record(obj);

    if (!rec)
        return 0;
    REC_W(rec, 0x0C) = x;
    REC_W(rec, 0x0E) = y;
    REC_W(rec, 0x10) = z;
    return z;
}

/* Marks the record's flags with bit 2 of +$1C. Returns the high byte of the
   new word; 0x00FF with no record (and the object's countdown is set to
   $FF). */
uint16_t record_mark_flag4(uint16_t obj)
{
    uint16_t rec = obj_record(obj), w;

    if (!rec) {
        obj_countdown(obj) = 0xFF;
        return 0x00FF;
    }
    w = (uint16_t)(REC_W(rec, 0x1C) | 0x0004);
    REC_W(rec, 0x1C) = w;
    return (uint16_t)(w & 0xFF00);
}

/* Sets the record's word at +$2A to `value` as its high byte, and keeps that
   word in the $1F36 word. Returns the word, 0 with no record. */
uint16_t record_set_2a_high(uint16_t obj, uint8_t value)
{
    uint16_t w = (uint16_t)(value << 8), rec = obj_record(obj);

    touch_flags = w;
    if (!rec)
        return 0;
    REC_W(rec, 0x2A) = w;
    return w;
}

/* The two records whose addresses are at $70:2476 and $70:2478 have their
   +$40 bits 5-6 kept, into $1A24 and $1A26. Returns the second value. */
uint16_t record_bits_60_pair(void)
{
    uint16_t rec, v;

    rec = P16(0x2476);
    v = REC_W(rec, 0x40) & 0x0060;
    bits_60_a = v;
    rec = P16(0x2478);
    v = REC_W(rec, 0x40) & 0x0060;
    bits_60_b = v;
    return v;
}

/* The parent's record word at +$88, minus one. Also gives the parent. */
uint16_t parent_record_word_88(uint16_t obj, uint16_t *parent)
{
    uint16_t rec;

    *parent = obj_parent(obj);
    rec = obj_record(*parent);
    return (uint16_t)(REC_W(rec, 0x88) - 1);
}

/* The parent's record word at +$58. `clear` is set when neither bit 4 nor
   bit 10 of the record's +$42 is set. Also gives the parent. */
uint16_t parent_record_word_58(uint16_t obj, uint16_t *parent, bool *clear)
{
    uint16_t rec;

    *parent = obj_parent(obj);
    rec = obj_record(*parent);
    *clear = !(REC_W(rec, 0x42) & 0x0410);
    return REC_W(rec, 0x58);
}

/* Copies the object's velocity, eight times over, into its record. Returns
   the high byte of the last word, 0x00FF with no record. */
uint16_t record_take_velocity(uint16_t obj)
{
    uint16_t rec = obj_record(obj), v = 0;
    unsigned i;

    if (!rec)
        return 0x00FF;
    for (i = 0; i < 3; i++) {
        v = (uint16_t)(obj_vel(obj, i) << 3);
        REC_W(rec, 0x2E + 2 * i) = v;
    }
    return (uint16_t)(v & 0xFF00);
}

/* The object's velocity is an eighth of the record's of object `from`, each
   shifted right three times keeping its sign. Returns the Z velocity. */
uint16_t velocity_from_record_of(uint16_t obj, uint16_t from)
{
    uint16_t rec = obj_record(from), v = 0;
    unsigned i;

    for (i = 0; i < 3; i++) {
        v = (uint16_t)asr16((int16_t)REC_W(rec, 0x2E + 2 * i), 3);
        obj_vel(obj, i) = v;
    }
    return v;
}

/* The object's velocity is three quarters of the eighth of the record's
   velocity of the object named at its +$27: X and Z. Returns Z. */
uint16_t velocity_from_record_scaled(uint16_t obj)
{
    uint16_t rec = obj_record(OBJ_W(obj, 0x27)), v;

    v = (uint16_t)scale_3_4(asr16((int16_t)REC_W(rec, 0x2E), 3));
    OBJ_W(obj, 0x2E) = v;
    v = (uint16_t)scale_3_4(asr16((int16_t)REC_W(rec, 0x32), 3));
    OBJ_W(obj, 0x32) = v;
    return v;
}

/* The object's state into its record: +$53, +$55, +$57 to +$0C, +$0E, +$10;
   +$20 to +$14; the position to +$1E, +$20, +$22; its +$15 as the high byte
   of +$24; and the low 7 bits of +$5A to +$12. Returns that last value, 0
   with no record. */
uint16_t record_copy_from_object(uint16_t obj)
{
    static const struct { uint16_t from, to; } copy[7] = {
        {0x53, 0x0C}, {0x55, 0x0E}, {0x57, 0x10}, {0x20, 0x14},
        {0x0C, 0x1E}, {0x0E, 0x20}, {0x10, 0x22}};
    uint16_t rec = obj_record(obj), v;
    unsigned i;

    if (!rec)
        return 0;
    for (i = 0; i < 7; i++)
        REC_W(rec, copy[i].to) = OBJ_W(obj, copy[i].from);
    REC_W(rec, 0x24) = (uint16_t)(*OBJ(obj, 0x15) << 8);
    v = OBJ_W(obj, 0x5A) & 0x007F;
    REC_W(rec, 0x12) = v;
    return v;
}

/* The object's pose from its record: its position, then its angles (+$27,
   +$29 and +$25) and the record's +$40 to +$90. Clears bits 5 and 6 of the
   object's +$25 first; its countdown 1 becomes 2 and sets bit 10 of the
   record's flags, otherwise that bit is cleared. Returns the high byte of
   the record's +$22 over the +$25 byte. */
uint16_t object_pose_from_record(uint16_t obj)
{
    uint16_t rec, z;
    uint8_t s, v;

    *OBJ(obj, 0x25) &= 0xDF;
    *OBJ(obj, 0x25) &= 0xBF;
    s = obj_countdown(obj);
    if (s == 0x01)
        obj_countdown(obj) = 0x02;
    rec = obj_record(obj);
    if (s == 0x01)
        REC_W(rec, 0x16) = (uint16_t)(REC_W(rec, 0x16) | 0x0400);
    else
        REC_W(rec, 0x16) = (uint16_t)(REC_W(rec, 0x16) & 0xFBFF);
    REC_W(rec, 0x90) = REC_W(rec, 0x40);
    OBJ_W(obj, 0x0C) = REC_W(rec, 0x1E);
    OBJ_W(obj, 0x0E) = REC_W(rec, 0x20);
    z = REC_W(rec, 0x22);
    OBJ_W(obj, 0x10) = z;
    *OBJ(obj, 0x14) = REC_B(rec, 0x27);
    *OBJ(obj, 0x16) = REC_B(rec, 0x29);
    v = REC_B(rec, 0x25);
    *OBJ(obj, 0x15) = v;
    return (uint16_t)((z & 0xFF00) | v);
}

/* The record words at +$88 of the objects in the list at $7E:FFAE (its count
   is half the word at $7E:FFB8) copied to the list at $7E:FFA6. Returns the
   last word. */
uint16_t object_list_record_words_88(void)
{
    uint16_t n = W16(0xFFB8), count, c, x = 0, v = 0;

    count = (uint16_t)((n >> 1) | (n & 0x8000));
    c = count;
    do {
        uint16_t obj = W16((uint16_t)(0xFFAE + x));

        v = REC_W(obj_record(obj), 0x88);
        W16((uint16_t)(0xFFA6 + x)) = v;
        x = (uint16_t)(x + 2);
        c = (uint16_t)(c - 1);
    } while (c);
    return v;
}

/* While the $0D62 flag is set, the record's +$6A is raised by 1 (when its
   +$88 is 1) or 3, up to $180, unless bit 10 of its +$42 is set. `ran` is
   the flag. Returns the new word; 0 with the flag clear, or the +$42 word
   when the record is held. */
uint16_t record_step_6a(uint16_t obj, bool *ran)
{
    uint16_t rec, f, a, s;

    *ran = W8(0x0D62) != 0;
    if (!*ran)
        return 0;
    rec = obj_record(obj);
    f = REC_W(rec, 0x42);
    if (f & 0x0400)
        return f;
    step_size = 0x0001;
    if (REC_W(rec, 0x88) != 0x0001)
        step_size = 0x0003;
    a = REC_W(rec, 0x6A);
    s = (uint16_t)(a + step_size);
    if (!((uint16_t)(s - 0x0180) & 0x8000))
        s = 0x0180;
    REC_W(rec, 0x6A) = s;
    return s;
}

/* A record that is landing: while the $70:2482 word is set, the record of
   object `obj` has bit 6 of +$2C and bit 4 of +$8A set, and not bit 8 of
   +$40, it stops (+$2E, +$30, +$32, +$36, +$34, +$38 zeroed) and, unless
   +$82 is already $200, +$82 becomes $200 and +$40 loses bits 5-6 and gains
   bits 0 and 12. Returns the last value tested or written. */
uint16_t record_land(uint16_t obj)
{
    static const uint8_t stop[6] = {0x2E, 0x30, 0x32, 0x36, 0x34, 0x38};
    uint16_t rec, a;
    unsigned i;

    if (!(a = P16(0x2482)))
        return a;
    rec = obj_record(obj);
    if (!(a = REC_W(rec, 0x2C) & 0x0040))
        return a;
    if (!(a = REC_W(rec, 0x8A) & 0x0010))
        return a;
    if ((a = REC_W(rec, 0x40) & 0x0100))
        return a;
    for (i = 0; i < 6; i++)
        REC_W(rec, stop[i]) = 0x0000;
    a = REC_W(rec, 0x82);
    if (a == 0x0200)
        return a;
    REC_W(rec, 0x82) = 0x0200;
    a = (uint16_t)((REC_W(rec, 0x40) & 0xFF9F) | 0x1001);
    REC_W(rec, 0x40) = a;
    return a;
}

/* The bits `bits` set in the record's word at +$8A. Returns the new word. */
uint16_t record_flags_8a_set(uint16_t rec, uint8_t bits)
{
    uint16_t w = (uint16_t)(bits | REC_W(rec, 0x8A));

    REC_W(rec, 0x8A) = w;
    return w;
}

/* The bits `bits` cleared in the record's word at +$8A. The original keeps
   only the low byte of its mask, so the high byte of the word is cleared
   too. Returns the new word. */
uint16_t record_flags_8a_clear(uint16_t rec, uint8_t bits)
{
    uint16_t w = (uint16_t)((0x00FF ^ bits) & REC_W(rec, 0x8A));

    REC_W(rec, 0x8A) = w;
    return w;
}

/* Runs the Super FX program at bank $04 `program`, which makes the new record
   and leaves its address at $70:238E; then gives it to object `obj`: its +$0A
   is `word0a` (through $1F4C) and the rest is copied from the object. False
   when the pool had no room. */
static bool record_new(uint16_t obj, uint16_t program, uint16_t word0a)
{
    uint16_t rec;

    gsu_run(0x04, program);
    record_arg_0a = word0a;
    rec = program_record;
    obj_record(obj) = rec;
    if (!rec)
        return false;
    REC_W(rec, 0x0A) = record_arg_0a;
    record_copy_from_object(obj);
    return true;
}

/* A new record from the second pool (mode $0422). */
bool record_new_422(uint16_t obj)
{
    return record_new(obj, 0x8B64, 0x0000);
}

/* A new record from the first pool (mode $042D). */
bool record_new_42d(uint16_t obj)
{
    return record_new(obj, 0x8C95, 0x90E8);
}

/* A new record for a flying piece or a firework shell (+$0A = $907A). */
bool record_new_424_piece(uint16_t obj)
{
    return record_new(obj, 0x8C0A, 0x907A);
}

/* A new record from the first pool (mode $0424). */
bool record_new_424(uint16_t obj)
{
    return record_new(obj, 0x8C0A, 0x910A);
}

/* As record_new_424, with +$0A = $90E8. */
bool record_new_424_alt(uint16_t obj)
{
    return record_new(obj, 0x8C0A, 0x90E8);
}

/* A replay camera's follower's record. */
bool record_new_camera(uint16_t obj)
{
    return record_new(obj, 0x8C6C, 0x916D);
}

/* A new record from the first pool (mode $0426). */
bool record_new_426(uint16_t obj)
{
    return record_new(obj, 0x8B33, 0x910A);
}

/* Sums the contact flags in $1F38: the touched record's contact word (its
   lowest bit of 0-2, or the word itself, plus 4), 8 for its flags bit 0 and
   $80 for bit 11. Returns the sum. */
uint16_t contact_flags_sum(void)
{
    uint16_t c = touch_word, f;

    if (c) {
        if (c & 0x0001)
            c = 0x0001;
        else if (c & 0x0002)
            c = 0x0002;
        else if (c & 0x0004)
            c = 0x0004;
        touch_sum = (uint16_t)(c | 0x0004 | touch_sum);
    }
    f = touch_flags;
    if (f & 0x0001)
        touch_sum = (uint16_t)(touch_sum | 0x0008);
    if (f & 0x0800)
        touch_sum = (uint16_t)(touch_sum | 0x0080);
    return touch_sum;
}

/* The next touched record in the contact list from $1F3C (records linked
   through +$0E) whose +$10 has bit 1 set: its address is given, its contact
   word and flags are in $1F3A and $1F36 and the sum in $1F38. The list
   moves on past it. False when none is left. */
bool record_next_contact(uint16_t *touched)
{
    uint16_t e = touch_next, r;

    if (!e)
        return false;
    while (!(REC_W(e, 0x10) & 0x0002)) {
        e = REC_W(e, 0x0E);
        if (!e)
            return false;
    }
    r = REC_W(e, 0x0C);
    touch_next = REC_W(e, 0x0E);
    touch_word = 0;
    if (REC_W(r, 0x16) & 0x0002)
        touch_word = REC_W(r, 0x64);
    touch_flags = REC_W(r, 0x16);
    touch_sum = 0;
    contact_flags_sum();
    *touched = r;
    return true;
}

/* Sums up every contact of the object's record into $1F38: $10 when bit 0 of
   its +$1C is set ($80 more when bit 5 of $70:26F2 is too), bit 5 of its
   +$2C, then the contact words and flags of every touched record in its
   contact list (from +$18) with bit 1 of +$10 set. `last` is set to the
   last such entry. Returns the sum, 0 with no record. */
uint16_t record_contact_summary(uint16_t obj, uint16_t *last)
{
    uint16_t rec = obj_record(obj), e, r, f;

    if (!rec)
        return 0;
    touch_sum = 0;
    if (REC_W(rec, 0x1C) & 0x0001) {
        touch_sum = (uint16_t)(touch_sum | 0x0010);
        if (P16(0x26F2) & 0x0020)
            touch_sum = (uint16_t)(touch_sum | 0x0080);
    }
    touch_sum = (uint16_t)(touch_sum | (REC_W(rec, 0x2C) & 0x0020));
    touch_flags = 0;
    touch_word = 0;
    for (e = REC_W(rec, 0x18); e; e = REC_W(e, 0x0E)) {
        if (!(REC_W(e, 0x10) & 0x0002))
            continue;
        r = REC_W(e, 0x0C);
        f = REC_W(r, 0x16);
        if (f & 0x0002) {
            touch_word = (uint16_t)(touch_word | REC_W(r, 0x64));
            f = REC_W(r, 0x16);
        }
        touch_flags = (uint16_t)(touch_flags | f);
        *last = e;
    }
    return contact_flags_sum();
}

/* ---- the Super FX programs that take an amount and a record ---- */

/* The amount and the record the program at bank $04 reads from the game pak
   RAM ($70:2390 and $70:238E, see program_record). */
#define program_amount  P16(0x2390)

/* The screen mode GsuRun leaves in A: the shadow the S-CPU keeps. */
#define gsu_mode_out    W8(0x0374)

/* Runs the program at bank $04 `program` on the record `rec` (the record
   read from $70:238E). Flags the object at $0346 when `rec` is the record at
   $70:2476. Returns A: `rec`'s high byte over the screen mode. */
static uint16_t record_amount_go(uint16_t rec, uint16_t program)
{
    object_flag40_if_match(rec);
    gsu_run(0x04, program);
    return (uint16_t)((rec & 0xFF00) | gsu_mode_out);
}

/* The record and the amount (as the high byte) into the program's inputs,
   then the program. */
static uint16_t record_amount_run(uint16_t rec, uint8_t amount, uint16_t program)
{
    program_amount = (uint16_t)(amount << 8);
    program_record = rec;
    return record_amount_go(rec, program);
}

/* Lowers the record's +$82 by the amount A (Super FX program $04:BF75).
   Returns A: the record's high byte over the screen mode. */
uint16_t record_lower_82(uint16_t rec, uint8_t amount)
{
    return record_amount_run(rec, amount, 0xBF75);
}

/* Raises the record's +$6A by the amount A (Super FX program $04:BF51), unless
   bit 0 of +$40 is set. Returns 1 when it is set, else as record_lower_82. */
uint16_t record_raise_6a(uint16_t rec, uint8_t amount)
{
    program_amount = (uint16_t)(amount << 8);
    program_record = rec;
    if (REC_W(rec, 0x40) & 0x0001)
        return 0x0001;
    return record_amount_go(program_record, 0xBF51);
}

/* ---- the object's position and turn from its record ---- */

/* The object's turn from its record: +$5D = the record's +$29 byte + 8 + a
   lean step, +$18 and (with $0D62 set) +$19 from the $08:C056 table for the
   heading in $1F5A; the record's +$25 byte is the object's +$15. `rec` is
   the record, `obj` the object. Returns A: +$5D's high byte over the low
   byte of the last table value (0 when $0D62 is clear). */
uint16_t object_turn_from_record(uint16_t rec, uint16_t obj)
{
    uint8_t a, sum, t;
    uint16_t w;

    *OBJ(obj, 0x16) = REC_B(rec, 0x29);
    *OBJ(obj, 0x15) = REC_B(rec, 0x25);
    W8(0x1F5A) = (uint8_t)-W8(0x1A75);
    W8(0x1F2B) = W8(0x1A77);
    sum = (uint8_t)(*OBJ(obj, 0x16) + W8(0x1F2B));
    a = (uint8_t)((sum >> 1) | (sum & 0x80));
    if (a & 0x80)
        a = (uint8_t)-(uint8_t)((uint8_t)-a & 0xFC);
    a &= 0xFC;
    if (!((uint8_t)(a - 0x08) & 0x80))
        a = 0x08;
    if ((uint8_t)(a - 0xF8) & 0x80)
        a = 0xF8;
    W8(0x1F58) = a;
    t = (uint8_t)(*OBJ(obj, 0x15) - W8(0x1F5A));
    if (t & 0x80)
        t = (uint8_t)-t;
    if (!((uint8_t)(t - 0x40) & 0x80))
        W8(0x1F58) = (uint8_t)-W8(0x1F58);
    w = (W8(0x1F58) & 0x80) ? (uint16_t)(W8(0x1F58) | 0xFF00) : W8(0x1F58);
    w = (uint16_t)(w + 8 + OBJ_W(obj, 0x29));
    OBJ_W(obj, 0x5D) = w;
    object_table_c056(obj);
    *OBJ(obj, 0x18) = (uint8_t)(W8(0x1F5A) | 0x80);
    if (!W8(0x0D62))
        return (uint16_t)(w & 0xFF00);
    W8(0x1F5A) = (uint8_t)-W8(0x1AB0);
    object_table_c056(obj);
    a = (uint8_t)(W8(0x1F5A) | 0x80);
    *OBJ(obj, 0x19) = a;
    return (uint16_t)((w & 0xFF00) | a);
}

/* Sets the record's +$24 high byte from the object's heading (+$15) and its
   +$12 from +$5A's low 7 bits. */
static void record_heading_from(uint16_t obj, uint16_t rec)
{
    REC_W(rec, 0x24) = (uint16_t)(*OBJ(obj, 0x15) << 8);
    REC_W(rec, 0x12) = OBJ_W(obj, 0x5A) & 0x007F;
}

/* The end of the sync routines: $1F3C is the record's +$18 (`y`); when it is
   zero A is kept, else A's low byte becomes 1. Carry is clear. */
static uint16_t sync_tail(uint16_t a, uint16_t *y, bool *carry)
{
    *carry = false;
    *y = W16(0x1F3C);
    if (!*y)
        return a;
    return (uint16_t)((a & 0xFF00) | 0x0001);
}

/* No record: `y` is the object, $1F3C is zero, A is zero and carry is set. */
static uint16_t sync_none(uint16_t obj, uint16_t *y, bool *carry)
{
    *y = obj;
    W16(0x1F3C) = 0;
    *carry = true;
    return 0;
}

/* The start of the sync routines: the object is rebucketed and its record
   marked (record_mark_flag4). Returns the record, 0 for none. */
static uint16_t sync_start(uint16_t obj)
{
    object_rebucket(obj);
    record_mark_flag4(obj);
    return obj_record(obj);
}

/* The object takes its position and heading back from its record, and the
   record's bit 1 of +$1C becomes bit 1 of the object's +$24. `y` is the
   record's +$18 and `carry` is set when there is no record. */
uint16_t object_sync_from_record(uint16_t obj, uint16_t *y, bool *carry)
{
    uint16_t rec = sync_start(obj), a;

    *OBJ(obj, 0x24) &= 0xFD;
    if (!rec)
        return sync_none(obj, y, carry);
    W16(0x1F3C) = REC_W(rec, 0x18);
    record_heading_from(obj, rec);
    OBJ_W(obj, 0x0C) = REC_W(rec, 0x1E);
    OBJ_W(obj, 0x0E) = REC_W(rec, 0x20);
    OBJ_W(obj, 0x10) = REC_W(rec, 0x22);
    a = REC_W(rec, 0x1C) & 0x0002;
    if (a) {
        *OBJ(obj, 0x24) |= 0x02;
        a = *OBJ(obj, 0x24);
    }
    return sync_tail(a, y, carry);
}

/* As object_sync_from_record the other way: the object's position goes to
   its record (+$1E, +$20, +$22); +$24 is left alone. */
uint16_t object_sync_to_record(uint16_t obj, uint16_t *y, bool *carry)
{
    uint16_t rec = sync_start(obj), a;

    if (!rec)
        return sync_none(obj, y, carry);
    record_heading_from(obj, rec);
    W16(0x1F3C) = REC_W(rec, 0x18);
    REC_W(rec, 0x1E) = OBJ_W(obj, 0x0C);
    REC_W(rec, 0x20) = OBJ_W(obj, 0x0E);
    a = OBJ_W(obj, 0x10);
    REC_W(rec, 0x22) = a;
    return sync_tail(a, y, carry);
}

/* The object's heading and +$12 go to its record; nothing else moves. A is
   the record's +$18. */
uint16_t object_sync_angles(uint16_t obj, uint16_t *y, bool *carry)
{
    uint16_t rec = sync_start(obj), a;

    if (!rec)
        return sync_none(obj, y, carry);
    record_heading_from(obj, rec);
    a = REC_W(rec, 0x18);
    W16(0x1F3C) = a;
    return sync_tail(a, y, carry);
}

/* A falling object: as object_sync_from_record for an object whose record is
   there (its record is not tested), with its Y speed (+$30) added to the
   record's +$20 before it goes to the object's +$0E. */
uint16_t object_sync_from_record_fall(uint16_t obj, uint16_t *y)
{
    uint16_t rec, v;
    bool carry;

    object_rebucket(obj);
    record_mark_flag4(obj);
    rec = obj_record(obj);
    record_heading_from(obj, rec);
    W16(0x1F3C) = REC_W(rec, 0x18);
    OBJ_W(obj, 0x0C) = REC_W(rec, 0x1E);
    v = (uint16_t)(REC_W(rec, 0x20) + OBJ_W(obj, 0x30));
    OBJ_W(obj, 0x0E) = v;
    v = REC_W(rec, 0x22);
    OBJ_W(obj, 0x10) = v;
    return sync_tail(v, y, &carry);
}

/* The handler of a falling object: object_sync_from_record_fall. */
void object_sync_fall_step(uint16_t obj)
{
    uint16_t y;

    object_sync_from_record_fall(obj, &y);
}

/* The record's handler: rebuckets the object and marks its record. */
void object_record_mark_step(uint16_t obj)
{
    object_rebucket(obj);
    record_mark_flag4(obj);
}

/* The spinning object's handler: its heading turns by one; with a record,
   the record's +$24 takes the heading's low byte; then it follows its
   record. */
void object_spin_step(uint16_t obj)
{
    uint16_t rec, y;
    bool carry;

    *OBJ(obj, 0x15) = (uint8_t)(*OBJ(obj, 0x15) + 1);
    rec = obj_record(obj);
    if (rec)
        REC_W(rec, 0x24) = OBJ_W(obj, 0x15) & 0x00FF;
    object_sync_from_record(obj, &y, &carry);
}

/* The spinning object gets its record (mode 2, +$2A high byte $20) and takes
   its first step. */
void object_spin_record(uint16_t obj)
{
    record_new_424(obj);
    record_set_mode_2(obj);
    record_set_2a_high(obj, 0x20);
    object_spin_step(obj);
}

/* The handler of object `obj`: +$1A = `h`, +$1C = $08. */
static void set_handler_08(uint16_t obj, uint16_t h)
{
    OBJ_W(obj, 0x1A) = h;
    *OBJ(obj, 0x1C) = 0x08;
}

/* Sets up a spinning object: +$1D = $CDD0, +$1F = $08, the step handler, then
   its record. */
void object_spin_setup(uint16_t obj)
{
    OBJ_W(obj, 0x1D) = 0xCDD0;
    *OBJ(obj, 0x1F) = 0x08;
    set_handler_08(obj, 0xCDDE);
    object_spin_record(obj);
}

/* ---- the buttons: a record's button sequence and the car's controls ---- */

/* The button words: the buttons held this frame and last frame, and the
   sequence's frame counter and stage ($1A33, $1A35). */
#define ctl_input       W16(0x1AF0)
#define ctl_prev        W16(0x1AF2)
#define seq_count       W16(0x1A33)
#define seq_stage       W16(0x1A35)

/* Watches the buttons for the four-press sequence (see the decomp's notes):
   bit 6, then bit 7, then bit 14 within 5 frames of each, then bit 11 within
   12 to 16 frames. The full sequence plays sound $48 and gives the record bit
   13 of +$40, $180 at +$6A and $1000 at +$400. Too slow, too early or done:
   back to the start. Returns A. */
uint16_t record_button_sequence(uint16_t rec)
{
    static const uint16_t press[3] = {0x0040, 0x0080, 0x4000};
    uint16_t n, s, in;

    n = (uint16_t)(seq_count + 1);
    seq_count = n;
    s = seq_stage;
    if (s <= 2) {
        n = seq_count;
        if (!((uint16_t)(n - 5) & 0x8000))
            goto reset;
        in = ctl_input;
        if (!(in & press[s]))
            return in;
        seq_count = 0;
        seq_stage = (uint16_t)(seq_stage + 1);
        return in;
    }
    if (s == 3) {
        n = seq_count;
        if (!((uint16_t)(n - 0x11) & 0x8000))
            goto reset;
        in = ctl_input;
        if (!(in & 0x0800))
            return in;
        n = seq_count;
        if ((uint16_t)(n - 0x0C) & 0x8000)
            goto reset;
    }
    sound_queue(0x48);
    REC_W(rec, 0x40) |= 0x2000;
    REC_W(rec, 0x6A) = 0x0180;
    REC_W(rec, 0x400) = 0x1000;
    n = 0x1000;
reset:
    seq_count = 0;
    seq_stage = 0;
    return n;
}

/* The player's flag word: $1A60 for player 2 ($1A5D set), else $1A5E. */
static uint16_t player_word(void)
{
    return W8(0x1A5D) ? 0x1A60 : 0x1A5E;
}

/* Clears bit 4 of the player's flag word. */
static void player_flag_clear(void)
{
    uint16_t pw = player_word();

    if (W16(pw) & 0x0010)
        W16(pw) &= 0xFFEF;
}

/* Sets bits m1 and m2 (when not 0) of the +$25 flags of the object's linked
   object (+$4F). */
static void linked_flags(uint16_t obj, uint8_t m1, uint8_t m2)
{
    uint16_t link = OBJ_W(obj, 0x4F);

    *OBJ(link, 0x25) |= m1;
    if (m2)
        *OBJ(link, 0x25) |= m2;
}

/* A car's controls: the player's buttons (ctl_input, ctl_prev) into the car's
   record. A car with bit 2 of +$25 only marks its record (+$40 bit 9, and +$42
   kept to $D800 unless bit 12). Otherwise, unless the record's +$1C has bit 4:
   SELECT runs the button sequence and sets bit 0 of the linked object's +$25
   (and bit 1 with L and R held); in a two-player race the car's +$29 counts up
   to a random limit $6E-$71 ($1F2B), and is reset to $FFFF by B, Y, A or X once
   it is past 9; at the limit the record's +$64 gets bit 3 and the linked object
   bits 0 and 1. Then, with $0D53 clear and +$42 bit 12 clear, the record's
   +$40 loses bit 9, +$42 keeps only $D800, and the buttons held set +$42 bits:
   LEFT 0 (else RIGHT 1), L 2 (else R 3), A $2040 (and $20 below speed $32 at
   +$4A), Y $400 when +$6A is not 0 (else sound $5E and the player's flag $10,
   once), B $10; X newly pressed 7 when +$5E is 0 and +$40 bit 8 is clear.
   Without Y the player's flag $10 is cleared. With +$40 bit 8 set, DOWN takes
   $600 from +$26 and UP adds it. `obj` is the car; its record is read from its
   +$22. The data bank is $7E. */
void car_controls_to_record(uint16_t obj)
{
    static const uint16_t clear_timer[4] = {0x8000, 0x4000, 0x0080, 0x0040};
    uint16_t rec, o29, lim, link;
    int i;

    W16(0x1F18) = obj;
    rec = obj_record(obj);
    if (*OBJ(obj, 0x25) & 0x04) {
        if (rec) {
            REC_W(rec, 0x40) |= 0x0200;
            if (!(REC_W(rec, 0x42) & 0x1000))
                REC_W(rec, 0x42) &= 0xD800;
        }
        return;
    }
    if (!rec || (REC_W(rec, 0x1C) & 0x0010))
        return;

    if (ctl_input & 0x2000) {
        record_button_sequence(rec);
        if (!(ctl_prev & 0x2000)) {
            link = OBJ_W(obj, 0x4F);
            *OBJ(link, 0x25) |= 0x01;
            if (W8(0x1AF0) & 0x20) {
                if (W8(0x1AF0) & 0x10)
                    *OBJ(link, 0x25) |= 0x02;
            }
        }
    }

    if (W8(0x0D62)) {
        lim = W16(0x1F2B) = (uint16_t)((random_step(&wram[0xE6]) & 0x03) + 0x6E);
        o29 = OBJ_W(obj, 0x29);
        if (!(o29 & 0x8000)) {
            if ((uint16_t)(o29 - lim) & 0x8000) {
                o29 = (uint16_t)(o29 + 1);
                OBJ_W(obj, 0x29) = o29;
                if (!((uint16_t)(o29 - 10) & 0x8000)) {
                    for (i = 0; i < 4; i++)
                        if (ctl_input & clear_timer[i])
                            break;
                    if (i < 4)
                        OBJ_W(obj, 0x29) = 0xFFFF;
                }
            } else if (!(REC_W(rec, 0x64) & 0x0008)) {
                REC_W(rec, 0x64) |= 0x0008;
                linked_flags(obj, 0x01, 0x02);
            }
        }
    }

    if (W8(0x0D53))
        return;
    REC_W(rec, 0x40) &= 0xFDFF;
    if (REC_W(rec, 0x42) & 0x1000)
        return;
    REC_W(rec, 0x42) &= 0xD800;
    if (ctl_input & 0x0200)
        REC_W(rec, 0x42) |= 0x0001;
    else if (ctl_input & 0x0100)
        REC_W(rec, 0x42) |= 0x0002;
    if (ctl_input & 0x0020)
        REC_W(rec, 0x42) |= 0x0004;
    else if (ctl_input & 0x0010)
        REC_W(rec, 0x42) |= 0x0008;
    if (ctl_input & 0x0080) {
        REC_W(rec, 0x42) |= 0x2040;
        if ((uint16_t)(REC_W(rec, 0x4A) - 0x32) & 0x8000)
            REC_W(rec, 0x42) |= 0x0020;
    }
    if (ctl_input & 0x4000) {
        if (REC_W(rec, 0x6A)) {
            REC_W(rec, 0x42) |= 0x0400;
            player_flag_clear();
        } else {
            uint16_t pw = player_word(), v = W16(pw);

            if (!(v & 0x0010)) {
                sound_queue(0x5E);
                v |= 0x0010;
                W16(pw) = v;
            }
        }
    } else {
        player_flag_clear();
    }
    if ((ctl_input & 0x0040) && !(ctl_prev & 0x0040)) {
        if (REC_W(rec, 0x5E) == 0 && !(REC_W(rec, 0x40) & 0x0100))
            REC_W(rec, 0x42) |= 0x0080;
    }
    if (ctl_input & 0x8000)
        REC_W(rec, 0x42) |= 0x0010;
    if (REC_W(rec, 0x40) & 0x0100) {
        if (ctl_input & 0x0400)
            REC_W(rec, 0x26) = (uint16_t)(REC_W(rec, 0x26) - 0x0600);
        if (ctl_input & 0x0800)
            REC_W(rec, 0x26) = (uint16_t)(REC_W(rec, 0x26) + 0x0600);
    }
}

/* One player's controls: the buttons held and last frame (at `held` and
   `prev`) into $1AF0 and $1AF2, the player into $1A5D, then the car's controls
   and the car's linked object handler. */
static void player_controls(uint16_t held, uint16_t prev, uint8_t player, uint16_t car)
{
    uint16_t obj, y = player;

    W16(0x1AF0) = W16(held);
    W16(0x1AF2) = W16(prev);
    W8(0x1A5D) = player;
    obj = W16(car);
    car_controls_to_record(obj);
    object_linked_handler_by_state(obj, &y, 0);
}

/* The players' controls for a race frame: player 1, and player 2 when $0D62
   is set, from the buttons held this frame ($0309, $030D) and last frame
   ($1F23, $1F25); then this frame's buttons become last frame's and $0309 and
   $030D are cleared. Returns player 2's buttons as read before the clear. */
uint16_t player_controls_frame(void)
{
    uint16_t a;

    player_controls(0x0309, 0x1F23, 0, 0x0346);
    if (W8(0x0D62))
        player_controls(0x030D, 0x1F25, 1, 0x034A);
    W16(0x1F23) = W16(0x0309);
    a = W16(0x030D);
    W16(0x1F25) = a;
    W16(0x0309) = 0;
    W16(0x030D) = 0;
    return a;
}
