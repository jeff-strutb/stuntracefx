/*
 * object_more.c: the object routines of object/object.c that objects.c does
 * not hold: the tables that give an object its place and heading, placing
 * the course's objects, the groups and triggers that wake them, the
 * sparkles, the trails and attached pieces, the billboard tilt, the puffs
 * and the timed objects.
 *
 * Object fields are named by offset (OBJ, OBJ_W); the bank-7E words the
 * original reads through the data bank are the same words here. The table
 * the original runs from work RAM ($7E:A2D9-F77F) is read where the cartridge
 * keeps it (ram_code_byte, ram_code_word).
 */
#include "game.h"
#include "objects.h"
#include "gsu_record.h"
#include "object_more.h"

/* The Super FX record of the object's +$22 (a game pak RAM address). */
#define rec_of(o)       OBJ_W(o, 0x22)

/* A byte of the data bank 7E: the code copy ($7E:A2D9-F77F) is the
   cartridge's bytes, every other byte is work RAM. */
static uint8_t data_byte(uint16_t a)
{
    return (a >= 0xA2D9 && a <= 0xF77F) ? ram_code_byte(a) : W8(a);
}

/* An arithmetic shift right by one (the sign bit stays). */
static uint16_t asr1(uint16_t v)
{
    return (uint16_t)((v >> 1) | (v & 0x8000));
}

/* The magnitude as EOR #$FFFF, INC does it ($8000 stays $8000). */
static uint16_t magnitude(uint16_t v)
{
    return (v & 0x8000) ? (uint16_t)(-v) : v;
}

/* Two's complement negation of a byte (EOR #$FF, INC A). */
static uint8_t neg8(uint8_t a)
{
    return (uint8_t)(~a + 1);
}

/* A rotation (rotate8) as the original runs it: the angle's sine and cosine
   are left in $0624 and $0625 (the tables' bytes at $08:96FE and $08:973E). */
static void rotate_dp(int8_t x, int8_t v, uint8_t angle, int16_t *x_out, int16_t *v_out)
{
    rotate8(x, v, angle, x_out, v_out);
    W8(0x0624) = (uint8_t)sin8(angle);
    W8(0x0625) = (uint8_t)cos8(angle);
}

/* The bit 14 of the word at the object two links on from the player's
   object ($0346), and when it is set $06CD goes up by 8 (and is returned).
   The links are bank 0 words here: the objects are below $2000, which is
   where the game's first records lie. */
uint16_t object_link_flag_4000_step(void)
{
    uint16_t x = W16(0x0346), a;

    x = OBJ_W(x, 0x4F);
    x = OBJ_W(x, 0x4F);
    a = W16(x) & 0x4000;
    if (!a)
        return a;
    a = (uint16_t)(W16(0x06CD) + 8);
    W16(0x06CD) = a;
    return a;
}

/* When A equals the word at $70:2476, bit 6 of the player's object's +$25
   is set. True when it matched. */
bool object_flag40_if_match(uint16_t a)
{
    if (a != P16(0x2476))
        return false;
    *OBJ(W16(0x0346), 0x25) |= 0x40;
    return true;
}

/* The byte of the table at $08:E8AF for (A + the object's +$15 + $1F4C) / 8. */
uint8_t object_table_e8af(uint16_t obj, uint8_t a)
{
    a = (uint8_t)(a + *OBJ(obj, 0x15));
    a = (uint8_t)(a + W8(0x1F4C));
    return rom_at(0x08E8AF + (a >> 3))[0];
}

/* Sets $1F5A to the byte of the table at $08:C056 for (the object's +$15 -
   $1F5A) / 4, and returns it. */
uint8_t object_table_c056(uint16_t obj)
{
    uint8_t f = *OBJ(obj, 0x15), s = W8(0x1F5A), d = (uint8_t)(f - s), v;

    v = rom_at(0x08C056 + (d >> 2))[0];
    W8(0x1F5A) = v;
    return v;
}

/* The X and Z velocities (+$2E, +$32) of the object are an eighth of the
   record's at +$2E and +$32, the record of the object named by +$27. Returns
   the Z velocity. */
uint16_t object_velocity_from_record(uint16_t obj)
{
    uint16_t rec = rec_of(OBJ_W(obj, 0x27)), v;

    v = asr1(asr1(asr1(P16((uint16_t)(0x2E + rec)))));
    OBJ_W(obj, 0x2E) = v;
    v = asr1(asr1(asr1(P16((uint16_t)(0x32 + rec)))));
    OBJ_W(obj, 0x32) = v;
    return v;
}

/* The object's +$5D from its kind: $9163, $9335 or $9507 for 1, 2 or 3, else
   $8F91. Returns it. */
uint16_t object_set_5d_by_kind(uint16_t obj, uint8_t kind)
{
    uint16_t v = kind == 1 ? 0x9163 : kind == 2 ? 0x9335 : kind == 3 ? 0x9507 : 0x8F91;

    OBJ_W(obj, 0x5D) = v;
    return v;
}

/* Sets up the object of list entry `list` (from 1) from the entry `entry`
   (from 1) of the 8-byte table at $7E:FF66: its +$27 from $1F2D, its
   position and +$15 from the table unless bit 0 of $1AEE is set, its +$49
   and +$4A from $1F2B and $1F4C. Returns the byte from $1F4C; the object
   in *obj. */
uint16_t object_place_from_table(uint8_t list, uint8_t entry, uint16_t *obj)
{
    uint16_t o = W16(0xFFAE + (uint32_t)(uint16_t)((list - 1) << 1));
    uint16_t t = (uint16_t)((uint16_t)(entry - 1) << 3), a, w = 0;
    int i;

    OBJ_W(o, 0x27) = W16(0x1F2D);
    a = W16(0x1AEE) & 0x0001;
    if (!a) {
        for (i = 0; i < 3; i++) {
            w = W16(0xFF66 + 2 * i + (uint32_t)t);
            OBJ_W(o, 0x0C + 2 * i) = w;
        }
        a = (uint16_t)((w & 0xFF00) | W8(0xFF6C + (uint32_t)t));
        *OBJ(o, 0x15) = (uint8_t)a;
    }
    a = (uint16_t)((a & 0xFF00) | W8(0x1F2B));
    *OBJ(o, 0x49) = (uint8_t)a;
    a = (uint16_t)((a & 0xFF00) | W8(0x1F4C));
    *OBJ(o, 0x4A) = (uint8_t)a;
    *obj = o;
    return a;
}

/* ObjectLinkEventTrigger for player 1: once ($0DA4 bit 7 set), when the
   object two links on from the player's has bit 14 set in its first word and
   $1A24 is set, clears $1A24, sets $0DA4 to $80 and starts the values at
   $1A1C-$1A22 ($1A, 0, $FE, 0). Returns A's low byte. */
uint8_t object_link_event_trigger(void)
{
    uint16_t x, y, a;

    if (W8(0x0DA4) & 0x80)
        return 0x80;
    x = W16(0x0346);
    y = OBJ_W(x, 0x4F);
    y = OBJ_W(y, 0x4F);
    a = W16(y) & 0x4000;
    if (!a)
        return 0x00;
    if (!(a = W16(0x1A24)))
        return 0x00;
    W16(0x1A24) = 0x0000;
    W8(0x0DA4) = 0x80;
    W8(0x1A1C) = 0x1A;
    W8(0x1A1E) = 0x00;
    W8(0x1A22) = 0x00;
    W8(0x1A20) = 0xFE;
    return 0xFE;
}

/* ObjectLinkEventTrigger for player 2 (with $1A26 and bob 1 at $1A1D). */
uint8_t object_link_event_trigger_2(void)
{
    uint16_t x, y, a;

    if (W8(0x0DA6) & 0x80)
        return 0x80;
    x = W16(0x034A);
    y = OBJ_W(x, 0x4F);
    y = OBJ_W(y, 0x4F);
    a = W16(y) & 0x4000;
    if (!a)
        return 0x00;
    if (!(a = W16(0x1A26)))
        return 0x00;
    W16(0x1A26) = 0x0000;
    W8(0x0DA6) = 0x80;
    W8(0x1A1D) = 0x1A;
    W8(0x1A1F) = 0x00;
    W8(0x1A23) = 0x00;
    W8(0x1A21) = 0xFE;
    return 0xFE;
}

/* A to $1A21: a byte when the registers are 8-bit (m8), else a word. */
void object_link_speed_set(uint16_t a, bool m8)
{
    if (m8)
        W8(0x1A21) = (uint8_t)a;
    else
        W16(0x1A21) = a;
}

/* Bob 1 starts: its kind $1A1D = kind, $1A1F and $1A23 cleared, then the
   speed set to $FE. */
void object_link_bob_start(uint8_t kind)
{
    W8(0x1A1D) = kind;
    W8(0x1A1F) = 0x00;
    W8(0x1A23) = 0x00;
    object_link_speed_set(0x00FE, true);
}

/* Sets up every object in the list at $7E:FFA6 (half the count at $FFB8 of
   them) with object_place_from_table: object n (from 1) takes table entry
   FFA6[n], and its $1F2B, $1F2D and $1F4C words come from the lists at
   $18D3, $18DB and $18E3. Returns the last byte. */
uint16_t object_place_all(void)
{
    uint16_t n = W16(0xFFB8), i = 0, a = 0, counter = 1, x, obj;

    n = asr1(n);
    do {
        W16(0x1F2B) = W16(0x18D3 + (uint32_t)i);
        W16(0x1F2D) = W16(0x18DB + (uint32_t)i);
        W16(0x1F4C) = W16(0x18E3 + (uint32_t)i);
        x = W16(0xFFA6 + (uint32_t)i);
        a = object_place_from_table((uint8_t)counter, (uint8_t)x, &obj);
        i = (uint16_t)(i + 2);
        counter = (uint16_t)(counter + 1);
        n = (uint16_t)(n - 1);
    } while (n);
    return a;
}

/* The object's counter +$49 steps up by one. While it has not passed $1F58
   its +$5F rises by $1F5A (false, *a the new +$5F); after that +$5F falls by
   $1F5A, and true is returned when the counter has reached $1F59 (*a the
   counter). */
bool object_count_step_5f(uint16_t obj, uint8_t *a)
{
    uint8_t n, lim, v, d, r;

    *OBJ(obj, 0x49) = (uint8_t)(*OBJ(obj, 0x49) + 1);
    n = *OBJ(obj, 0x49);
    lim = W8(0x1F58);
    if (n == lim || ((uint8_t)(n - lim) & 0x80)) {
        v = *OBJ(obj, 0x5F);
        d = W8(0x1F5A);
        r = (uint8_t)(v + d);
        *OBJ(obj, 0x5F) = r;
        *a = r;
        return false;
    }
    v = *OBJ(obj, 0x5F);
    d = W8(0x1F5A);
    r = (uint8_t)(v - d);
    *OBJ(obj, 0x5F) = r;
    n = *OBJ(obj, 0x49);
    *a = n;
    r = (uint8_t)(n - W8(0x1F59));
    return r == 0;
}

/* Switches the object between near and far by its distance from the
   object at $1A64 (|dX| + |dZ| against $1194). Near: its +$24 bit 3 clear,
   +$5F zero, and its +$5D by its kind. Far: +$24 bit 3 set, +$3A zero, +$5F
   $32, +$5D = $99A3 + 2 * kind, and a direction above the low two bits of
   +$18. $1F2B keeps |dX|. Returns the +$5D value. */
uint16_t object_near_focus(uint16_t obj)
{
    uint16_t dx, dz, sum;
    uint8_t k;

    dx = magnitude((uint16_t)(W16(0x1A6C) - OBJ_W(obj, 0x0C)));
    W16(0x1F2B) = dx;
    dz = magnitude((uint16_t)(W16(0x1A70) - OBJ_W(obj, 0x10)));
    sum = (uint16_t)(dz + dx);
    if ((uint16_t)(sum - 0x1194) & 0x8000) {
        *OBJ(obj, 0x24) &= 0xF7;
        *OBJ(obj, 0x5F) = 0x00;
        return object_set_5d_by_kind(obj, *OBJ(obj, 0x49));
    }
    k = (uint8_t)(*OBJ(obj, 0x18) & 0x03);
    *OBJ(obj, 0x18) = k;
    k = (uint8_t)((((uint8_t)(W8(0x1A75) + *OBJ(obj, 0x15))) >> 3 & 0xFC) | 0x80 | k);
    *OBJ(obj, 0x18) = k;
    *OBJ(obj, 0x24) |= 0x08;
    *OBJ(obj, 0x3A) = 0x00;
    *OBJ(obj, 0x5F) = 0x32;
    sum = (uint16_t)((uint16_t)(*OBJ(obj, 0x49) << 1) + 0x99A3);
    OBJ_W(obj, 0x5D) = sum;
    return sum;
}

/* The object's bucket from its X and Z position (xz_grid_key); when it
   changed the object moves to the new bucket's list. Returns the bucket
   when unchanged, else the object. */
uint16_t object_rebucket(uint16_t obj)
{
    int16_t x = obj_pos_x(obj), z = obj_pos_z(obj);
    uint16_t key = xz_grid_key(&x, &z);

    if (key == OBJ_W(obj, 0x40))
        return key;
    object_bucket_remove(obj);
    OBJ_W(obj, 0x40) = key;
    object_bucket_insert(obj);
    return obj;
}

/* Counts the object's +$48 up to 8, copies it to $1F58 and, with bit 7 set,
   to +$5A. Returns the +$5A byte. */
uint8_t object_step_48_to_8(uint16_t obj)
{
    uint8_t v = *OBJ(obj, 0x48);

    if (v != 0x08)
        *OBJ(obj, 0x48) = (uint8_t)(*OBJ(obj, 0x48) + 1);
    W8(0x1F58) = *OBJ(obj, 0x48);
    v = (uint8_t)(W8(0x1F58) | 0x80);
    *OBJ(obj, 0x5A) = v;
    return v;
}

/* Adds 2 to the object's +$5D when $0D62 is set. */
void object_advance_5d_if_flag(uint16_t obj)
{
    if (W8(0x0D62))
        OBJ_W(obj, 0x5D) = (uint16_t)(OBJ_W(obj, 0x5D) + 2);
}

/* ObjectRelease for callers in other banks and the handler tables. */
void object_release_long(uint16_t obj)
{
    object_release(obj);
}

/* Points the linked object (+$4F) at the handler $08:addr and marks it
   done; returns the linked object in *y. */
static void set_linked_handler(uint16_t obj, uint16_t addr, uint16_t *y)
{
    uint16_t link = OBJ_W(obj, 0x4F);

    OBJ_W(link, 0x1A) = addr;
    *OBJ(link, 0x1C) = 0x08;
    *y = link;
}

/* The linked object's handler by the state byte at $0DD6 + *y, once per
   object (bit 3 of +$25). Each pick also runs GsuRecordWords88 unless bit 0
   of $1AEE is set, and marks the object done. Returns A (its high byte as
   given, its low byte the +$25 byte). */
uint16_t object_linked_handler_by_state(uint16_t obj, uint16_t *y, uint16_t a)
{
    uint8_t f = *OBJ(obj, 0x25), v, k;
    uint16_t rec, w;

    if (f & 0x08)
        return (uint16_t)((a & 0xFF00) | 0x08);
    v = W8((uint32_t)0x0DD6 + *y);
    if (v == 0x08) {
        rec = rec_of(obj);
        P16((uint16_t)(0x64 + rec)) |= 0x0008;
        P16((uint16_t)(0x70 + rec)) |= 0x0020;
        k = P8((uint16_t)(0x88 + rec));
        W8(0x1F58) = k;
        set_linked_handler(obj, k == 0x01 ? 0x91EB : k == 0x04 ? 0x93D7 : 0x92DF, y);
    } else if (v == 0x0C || v == 0x0A) {
        rec = rec_of(obj);
        w = P16((uint16_t)(0x42 + rec)) & 0xFBCF;
        P16((uint16_t)(0x42 + rec)) = w;
        if (!W8(0x0E59))
            return (uint16_t)(w & 0xFF00);
        set_linked_handler(obj, 0x94A1, y);
    } else {
        if (W8(0x19ED) != 0x08)
            return (uint16_t)((a & 0xFF00) | W8(0x19ED));
        set_linked_handler(obj, 0x951B, y);
    }
    a = 0x0000;
    if (!(W16(0x1AEE) & 0x0001))
        a = object_list_record_words_88();
    f = (uint8_t)(*OBJ(obj, 0x25) | 0x08);
    *OBJ(obj, 0x25) = f;
    return (uint16_t)((a & 0xFF00) | f);
}

/* Links object obj to `other`: obj's +$4F = other, other's +$06 = obj, and
   the block other's +$4F points at gets obj at +$02, kind 5 at +$00, its
   +$32 from the table at $09:8090 and its other fields cleared. The table
   entry is the word at $70:2396, plus 2 when other's +$1A is $8E2A, else
   plus 4, 8 or 6 for $0D2B = 1, 2 or 3. Returns the block. */
uint16_t object_attach_linked(uint16_t obj, uint16_t other)
{
    static const uint8_t clear[9] = {0x35, 0x37, 0x39, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30};
    uint16_t blk, kind, t, add = 0, m;
    unsigned i;

    OBJ_W(obj, 0x4F) = other;
    blk = OBJ_W(other, 0x4F);
    W16(0x1F4E) = blk;
    kind = OBJ_W(other, 0x1A);
    W16(0x1F4C) = kind;
    OBJ_W(other, 0x06) = obj;
    blk = W16(0x1F4E);
    W16((uint16_t)(blk + 0x02)) = obj;
    t = P16(0x2396);
    kind = W16(0x1F4C);
    if (kind == 0x8E2A) {
        add = 2;
    } else {
        m = W8(0x0D2B);
        if (m == 1)
            add = 4;
        else if (m == 2)
            add = 8;
        else if (m == 3)
            add = 6;
    }
    t = (uint16_t)(t + add);
    W16((uint16_t)(blk + 0x32)) = rom_word(0x098090u + t);
    for (i = 0; i < 9; i++)
        W16((uint16_t)(blk + clear[i])) = 0x0000;
    W16(blk) = 0x0005;
    W8((uint16_t)(blk + 0x34)) = 0x00;
    return blk;
}

/* Walks one bucket's chain from *y: each object's countdown +$36 is set to
   3 unless it is negative (for code $23, objects with +$24 bit 4 are left). */
static void groups_chain(uint16_t *y)
{
    do {
        if (!(W8(0x1F2D) == 0x23 && (*OBJ(*y, 0x24) & 0x10))) {
            if (!(*OBJ(*y, 0x36) & 0x80))
                *OBJ(*y, 0x36) = 0x03;
        }
        *y = OBJ_W(*y, 0x3C);
    } while (*y);
}

/* For each entry of the list at $1AF5 (code, bucket; 4 bytes each, ended by
   a zero code), the objects of the bucket its second word selects get their
   countdown set (groups_chain). Returns the position of the ending entry;
   *y is the last chain's end (0) when any chain was walked. */
uint16_t object_groups_activate(uint16_t *y)
{
    uint16_t pos, code, head;

    W16(0x1F2B) = 0x0000;
    for (;;) {
        pos = W16(0x1F2B);
        code = W16((uint32_t)0x1AF5 + pos);
        if (!code)
            return pos;
        W16(0x1F2D) = code;
        head = W16((uint32_t)0x1AF7 + pos);
        W16(0x1F2B) = (uint16_t)(pos + 4);
        head = W16(0x10000u + (head & 0x07FE));
        if (!head)
            continue;
        *y = head;
        groups_chain(y);
    }
}

/* The trigger list at $1AF5 for the cells of an area record: the record
   holds the columns (+0), rows (+1), first column (+2) and first row (+3),
   then the cells row by row. A cell inside the window ($1F01-$1F06) that is
   not $2E gets an entry: its code and the bucket $1F01 + column * 2 + row *
   $40. The list ends with a zero entry. Returns the list size in bytes. */
static uint16_t triggers_from_area(uint16_t rec)
{
    uint16_t x = 0, y = rec, v, w, n, a5, z94, c2, n4;

    v = (uint16_t)(data_byte((uint16_t)(rec + 3)) << 8);
    z94 = asr1(asr1(v));
    W16(0x05F2) = data_byte((uint16_t)(rec + 1));
    a5 = (uint16_t)((uint16_t)(0 - data_byte((uint16_t)(rec + 2))) << 1);
    W16(0x1F2B) = data_byte(rec);
    n = W16(0x05F2);
    do {
        c2 = a5;
        n4 = W16(0x1F2B);
        do {
            v = c2;
            w = W16(0x1F03);
            if (!((uint16_t)(v + w) & 0xFFC0)) {
                v = z94;
                w = W16(0x1F05);
                if (!((uint16_t)(v + w) & 0xF800)) {
                    v = data_byte((uint16_t)(y + 4));
                    if (v != 0x002E) {
                        W16((uint16_t)(0x1AF5 + x)) = v;
                        v = (uint16_t)(W16(0x1F01) + c2 + z94);
                        W16((uint16_t)(0x1AF7 + x)) = v;
                        x = (uint16_t)(x + 4);
                    }
                }
            }
            y = (uint16_t)(y + 1);
            c2 = (uint16_t)(c2 + 2);
            n4 = (uint16_t)(n4 - 1);
        } while (n4);
        z94 = (uint16_t)(z94 - 0x0040);
        n = (uint16_t)(W16(0x05F2) - 1);
        W16(0x05F2) = n;
    } while (n);
    W16((uint16_t)(0x1AF5 + x)) = 0x0000;
    W16((uint16_t)(0x1AF7 + x)) = 0x0000;
    return x;
}

/* The window and bucket origin: three words from src to $1F01-$1F05. */
static void triggers_window(uint16_t src)
{
    unsigned i;

    for (i = 0; i < 6; i += 2)
        W16(0x1F01 + i) = W16((uint32_t)src + i);
}

/* The trigger list for the area the view's Y ($0667) selects, the window
   from $1EF5-$1EFA. */
void bucket_triggers_from_view(void)
{
    uint16_t x = (uint16_t)((((W8(0x0667)) >> 3) + 1) & 0xFE);
    uint16_t rec = ram_code_word((uint16_t)(0xC61D + x));

    triggers_window(0x1EF5);
    triggers_from_area(rec);
}

/* The trigger list for the fixed area: $C661 when $0D62 is set or bit 7 of
   $1AEC is, else $C696; the window from $1EFB-$1F00. */
void bucket_triggers_fixed_area(void)
{
    uint16_t rec;

    triggers_window(0x1EFB);
    if (W8(0x0D62) || (W8(0x1AEC) & 0x80))
        rec = 0xC661;
    else
        rec = 0xC696;
    triggers_from_area(rec);
}

/* A new object (kind $A158) with handler $08:EA6B at object obj's position,
   obj as its +$06, and put in its bucket. */
static void spawn_sparkle(uint16_t obj)
{
    uint16_t y;

    W16(0x1F2B) = 0xA158;
    if (!object_spawn(&y))
        return;
    OBJ_W(y, 0x1A) = 0xEA6B;
    *OBJ(y, 0x1C) = 0x08;
    OBJ_W(y, 0x06) = obj;
    object_copy_position(obj, y);
    object_rebucket(y);
}

/* Two sparkles at the object's position (spawn_sparkle). */
void object_spawn_sparkle_pair(uint16_t obj)
{
    spawn_sparkle(obj);
    spawn_sparkle(obj);
}

/* The chance of a new object with handler $08:D460 at the object's
   position: a random byte of $B2 or more (random_step at $E6). */
void object_spawn_chance_d460(uint16_t obj)
{
    uint16_t y;

    if (random_step(&wram[0xE6]) < 0xB2)
        return;
    W16(0x1F2B) = 0xA158;
    if (!object_spawn(&y))
        return;
    OBJ_W(y, 0x1A) = 0xD460;
    *OBJ(y, 0x1C) = 0x08;
    object_copy_position(obj, y);
    OBJ_W(y, 0x27) = obj;
}

/* Unless $0D62 is set, a new object with handler $08:D460 at the object's
   position, obj as its +$27. */
void object_spawn_d460_unless_flag(uint16_t obj)
{
    uint16_t y;

    if (W8(0x0D62))
        return;
    W16(0x1F2B) = 0xA158;
    if (!object_spawn(&y))
        return;
    OBJ_W(y, 0x1A) = 0xD460;
    *OBJ(y, 0x1C) = 0x08;
    object_copy_position(obj, y);
    OBJ_W(y, 0x27) = obj;
}

/* Adds (a random byte & mask) to the word at obj + off. */
static void add_random(uint16_t obj, uint16_t off, uint8_t mask)
{
    uint8_t r = random_step(&wram[0xE6]);

    OBJ_W(obj, off) = (uint16_t)(OBJ_W(obj, off) + (uint16_t)(r & mask));
}

/* The sparkle's first frame: bit 3 of +$24, +$5D = $9998, +$18 a random 0-3
   with bit 7, handler $08:EB07, +$5F = $0A, count +$48 = 5, and a random
   acceleration (+$27 and +$2B get -$3F to +$40, +$29 becomes minus 0 to
   $3F); then its first step. */
void object_sparkle_start(uint16_t obj)
{
    uint8_t v;

    *OBJ(obj, 0x24) |= 0x08;
    OBJ_W(obj, 0x5D) = 0x9998;
    v = random_step(&wram[0xE6]) & 0x03;
    W8(0x1F58) = v;
    *OBJ(obj, 0x18) = (uint8_t)(W8(0x1F58) | 0x80);
    OBJ_W(obj, 0x1A) = 0xEB07;
    *OBJ(obj, 0x1C) = 0x08;
    *OBJ(obj, 0x5F) = 0x0A;
    *OBJ(obj, 0x48) = 0x05;
    add_random(obj, 0x27, 0x7F);
    add_random(obj, 0x29, 0x3F);
    add_random(obj, 0x2B, 0x7F);
    OBJ_W(obj, 0x27) = (uint16_t)(OBJ_W(obj, 0x27) - 0x003F);
    OBJ_W(obj, 0x29) = (uint16_t)(-OBJ_W(obj, 0x29));
    OBJ_W(obj, 0x2B) = (uint16_t)(OBJ_W(obj, 0x2B) - 0x003F);
    object_sparkle_step(obj);
}

/* A sparkle's frame: takes its parent's position, moves, adds its
   acceleration to its velocity; when its count +$48 is 0 it is released,
   otherwise counted down and rebucketed. */
void object_sparkle_step(uint16_t obj)
{
    uint16_t parent = OBJ_W(obj, 0x06);
    unsigned i;

    object_copy_position(parent, obj);
    object_apply_velocity(obj);
    for (i = 0; i < 3; i++)
        OBJ_W(obj, 0x2E + 2 * i) = (uint16_t)(OBJ_W(obj, 0x2E + 2 * i) + OBJ_W(obj, 0x27 + 2 * i));
    if (!*OBJ(obj, 0x48)) {
        object_release_long(obj);
        return;
    }
    *OBJ(obj, 0x48) = (uint8_t)(*OBJ(obj, 0x48) - 1);
    object_rebucket(obj);
}

/* A puff: counts a hit ($1A28 and the decimal counter at $19EE-$19EF), then
   makes an object of kind $A158 at obj's position, 500 units higher, with
   +$5D = $9BB4, +$29 = $FE0C, bit 3 of +$24, +$3A = 0, +$5F = $14 and +$0A
   = 7. Nothing is made without a free object. */
void object_spawn_puff(uint16_t obj)
{
    uint16_t y;
    bool c = false;

    W8(0x1A28) = (uint8_t)(W8(0x1A28) + 1);
    W8(0x19EE) = bcd_add8(W8(0x19EE), 0x01, &c);
    W8(0x19EF) = bcd_add8(W8(0x19EF), 0x00, &c);
    W16(0x1F2B) = 0xA158;
    if (!object_spawn(&y))
        return;
    OBJ_W(y, 0x5D) = 0x9BB4;
    OBJ_W(y, 0x29) = 0xFE0C;
    *OBJ(y, 0x24) |= 0x08;
    *OBJ(y, 0x3A) = 0x00;
    *OBJ(y, 0x5F) = 0x14;
    object_copy_position(obj, y);
    OBJ_W(y, 0x0E) = (uint16_t)(OBJ_W(y, 0x0E) + 0xFE0C);
    *OBJ(y, 0x0A) = 0x07;
}

/* Object obj placed at its parent (+$27)'s position plus an offset turned by
   the parent's angles +$14 (RotateXZFrom08) and +$15 (RotateXZ) and doubled.
   A trail's offset is (0, -10, +$32 at $1F58), an attached piece's is
   (0, +$30, +$32) kept at $1F59/$1F5A; a trail's +$32 then moves by -5. Then
   rebucketed. The vector is (x, y, z) = (0, y, z) as the original keeps it. */
static void parent_place(uint16_t obj, bool attached)
{
    uint16_t parent = OBJ_W(obj, 0x27);
    uint8_t vy, vz;
    int16_t xa, za, xb, zb;

    if (attached) {
        W8(0x1F59) = *OBJ(obj, 0x30);
        W8(0x1F5A) = *OBJ(obj, 0x32);
    } else {
        W8(0x1F58) = *OBJ(obj, 0x32);
    }
    vy = attached ? W8(0x1F59) : 0xF6;
    vz = W8(attached ? 0x1F5A : 0x1F58);
    rotate_dp((int8_t)vy, (int8_t)vz, *OBJ(parent, 0x14), &xa, &za);
    rotate_dp(0, (int8_t)(uint8_t)za, neg8(*OBJ(parent, 0x15)), &xb, &zb);
    W16(0x05F2) = (uint16_t)((uint16_t)zb << 1);
    OBJ_W(obj, 0x0C) = (uint16_t)(OBJ_W(parent, 0x0C) + (uint16_t)((uint16_t)xb << 1));
    OBJ_W(obj, 0x0E) = (uint16_t)(OBJ_W(parent, 0x0E) + (uint16_t)((uint16_t)xa << 1));
    OBJ_W(obj, 0x10) = (uint16_t)(OBJ_W(parent, 0x10) + (uint16_t)((uint16_t)zb << 1));
    if (!attached)
        *OBJ(obj, 0x32) = (uint8_t)(*OBJ(obj, 0x32) + 0xFB);
    object_rebucket(obj);
}

/* A trail object's new frame: its count +$4B = $0C, handler $08:E934, +$32 =
   $D8, then placed by its parent. */
void object_trail_start(uint16_t obj)
{
    *OBJ(obj, 0x4B) = 0x0C;
    OBJ_W(obj, 0x1A) = 0xE934;
    *OBJ(obj, 0x1C) = 0x08;
    *OBJ(obj, 0x32) = 0xD8;
    parent_place(obj, false);
}

/* A trail object's frame: sets bit 2 of +$24 and clears +$4A, +$48 and
   +$49; when its count +$4B is 0 it is released, otherwise counted down and
   placed at its parent. */
void object_trail_step(uint16_t obj)
{
    uint8_t v;

    *OBJ(obj, 0x24) = (uint8_t)(*OBJ(obj, 0x24) | 0x04);
    *OBJ(obj, 0x4A) = 0x00;
    *OBJ(obj, 0x48) = 0x00;
    *OBJ(obj, 0x49) = 0x00;
    v = *OBJ(obj, 0x4B);
    if (!v) {
        object_release_long(obj);
        return;
    }
    *OBJ(obj, 0x4B) = (uint8_t)(v - 1);
    parent_place(obj, false);
}

/* A new trail object (kind $A158) with handler $08:E919 and its parent. */
void object_spawn_trail(uint16_t obj)
{
    uint16_t y;

    if (W8(0x0D62))
        return;
    W16(0x1F2B) = 0xA158;
    if (!object_spawn(&y))
        return;
    *OBJ(y, 0x24) |= 0x04;
    *OBJ(y, 0x4A) = 0x03;
    *OBJ(y, 0x48) = 0x14;
    *OBJ(y, 0x49) = 0x0A;
    OBJ_W(y, 0x1A) = 0xE919;
    *OBJ(y, 0x1C) = 0x08;
    OBJ_W(y, 0x27) = obj;
}

/* A new piece attached to its parent (kind $A158, handler $08:EBA3), with
   the offset $1F59/$1F5A as its +$30/+$32. */
void object_spawn_attached(uint16_t obj)
{
    uint16_t y;

    if (W8(0x0D62))
        return;
    W16(0x1F2B) = 0xA158;
    if (!object_spawn(&y))
        return;
    *OBJ(y, 0x24) |= 0x04;
    *OBJ(y, 0x4A) = 0x02;
    *OBJ(y, 0x48) = 0x05;
    *OBJ(y, 0x49) = 0x0A;
    OBJ_W(y, 0x1A) = 0xEBA3;
    *OBJ(y, 0x1C) = 0x08;
    OBJ_W(y, 0x27) = obj;
    *OBJ(y, 0x30) = W8(0x1F59);
    *OBJ(y, 0x32) = W8(0x1F5A);
}

/* An attached piece's first frame: count +$4B = $0C, handler $08:EBB9, then
   placed by its parent. */
void object_attached_start(uint16_t obj)
{
    *OBJ(obj, 0x4B) = 0x0C;
    OBJ_W(obj, 0x1A) = 0xEBB9;
    *OBJ(obj, 0x1C) = 0x08;
    parent_place(obj, true);
}

/* An attached piece's frame: as object_trail_step, placed with its offset
   kept and no drift. */
void object_attached_step(uint16_t obj)
{
    uint8_t v;

    *OBJ(obj, 0x24) = (uint8_t)(*OBJ(obj, 0x24) | 0x04);
    *OBJ(obj, 0x4A) = 0x00;
    *OBJ(obj, 0x48) = 0x00;
    *OBJ(obj, 0x49) = 0x00;
    v = *OBJ(obj, 0x4B);
    if (!v) {
        object_release_long(obj);
        return;
    }
    *OBJ(obj, 0x4B) = (uint8_t)(v - 1);
    parent_place(obj, true);
}

/* Puts object obj on the object parent: unless bit 0 of obj's +$25 is set
   (then +$18 = +$19 = $8C), $1F4C = obj's +$48 plus the high byte of the
   parent's record word +$3A * 4, and obj's +$18 and +$19 become the table
   bytes (object_table_e8af) for $1A75 and $1AB0 with bit 7 set. Then obj's
   position is the parent's plus obj's offset (+$2E, +$30, +$32) turned by the
   parent's angles +$16 (RotateXY), +$14 (RotateXZFrom08) and +$15 (RotateXZ)
   and doubled. */
void object_place_on_parent(uint16_t parent, uint16_t obj)
{
    static const uint16_t from_x[2] = {0x1A64, 0x1A9F};
    uint16_t rec;
    uint8_t hi = 0, v;
    unsigned i;
    int16_t r0x, r0y, r1x, r1z, r2x, r2z;
    uint8_t vx, vy, vz;

    if (*OBJ(obj, 0x25) & 0x01) {
        *OBJ(obj, 0x18) = 0x8C;
        *OBJ(obj, 0x19) = 0x8C;
    } else {
        rec = rec_of(parent);
        if (rec)
            hi = (uint8_t)((uint16_t)(P16((uint16_t)(0x3A + rec)) << 2) >> 8);
        v = (uint8_t)(hi + *OBJ(obj, 0x48));
        W8(0x1F4C) = v;
        for (i = 0; i < 2; i++) {
            uint8_t a = W8((uint32_t)from_x[i] + 0x11);

            v = object_table_e8af(parent, a);
            *OBJ(obj, 0x18 + i) = (uint8_t)(v | 0x80);
        }
    }
    for (i = 0; i < 3; i++)
        W8(0x1F58 + i) = *OBJ(obj, 0x2E + 2 * i);
    vx = W8(0x1F58);
    vy = W8(0x1F59);
    vz = W8(0x1F5A);
    rotate_dp((int8_t)vx, (int8_t)vy, *OBJ(parent, 0x16), &r0x, &r0y);
    rotate_dp((int8_t)(uint8_t)r0y, (int8_t)vz, *OBJ(parent, 0x14), &r1x, &r1z);
    rotate_dp((int8_t)(uint8_t)r0x, (int8_t)(uint8_t)r1z, neg8(*OBJ(parent, 0x15)), &r2x, &r2z);
    W16(0x05F2) = (uint16_t)((uint16_t)r2z << 1);
    OBJ_W(obj, 0x0C) = (uint16_t)(OBJ_W(parent, 0x0C) + (uint16_t)((uint16_t)r2x << 1));
    OBJ_W(obj, 0x0E) = (uint16_t)(OBJ_W(parent, 0x0E) + (uint16_t)((uint16_t)r1x << 1));
    OBJ_W(obj, 0x10) = (uint16_t)(OBJ_W(parent, 0x10) + (uint16_t)((uint16_t)r2z << 1));
}

/* The object's X and Z forward by its speed: the table byte at $08:B32B for
   its record's word +$5E (the record at +$22) is turned by the object's
   angles +$16 (RotateXY), +$14 (RotateXZFrom08) and +$15 (RotateXZ), and four
   times the result is added to its position. An object of 0 does nothing. */
void object_move_forward(uint16_t obj)
{
    uint16_t idx;
    uint8_t speed;
    int16_t x1, v1, x2, z2, x3, z3;

    if (!obj)
        return;
    idx = P16((uint16_t)(0x5E + rec_of(obj)));
    speed = rom_at(0x08B32B + idx)[0];
    rotate_dp(0, (int8_t)speed, *OBJ(obj, 0x16), &x1, &v1);
    rotate_dp((int8_t)(uint8_t)v1, 0, *OBJ(obj, 0x14), &x2, &z2);
    rotate_dp((int8_t)(uint8_t)x1, (int8_t)(uint8_t)z2, neg8(*OBJ(obj, 0x15)), &x3, &z3);
    W16(0x05F2) = (uint16_t)((uint16_t)z3 << 2);
    OBJ_W(obj, 0x0C) = (uint16_t)(OBJ_W(obj, 0x0C) + (uint16_t)((uint16_t)x3 << 2));
    OBJ_W(obj, 0x0E) = (uint16_t)(OBJ_W(obj, 0x0E) + (uint16_t)((uint16_t)x2 << 2));
    OBJ_W(obj, 0x10) = (uint16_t)(OBJ_W(obj, 0x10) + (uint16_t)((uint16_t)z3 << 2));
}

/* The billboard tilt of object y's picture as the camera $1A64 sees it: half
   its pitch +$16 plus the camera's +$13, rounded to 4 and held to -8..8,
   negated when its heading +$15 is $40 or more from the camera's turn; +$5D
   = that plus 8 plus its +$29; then the table bytes (object_table_c056) and
   +$18 = the turn with bit 7 set; with two players ($0D62) the second camera
   ($1A9F) the same into +$19. */
void object_billboard_tilt(uint16_t y)
{
    uint8_t a, c;

    W8(0x1F5A) = neg8(W8(0x1A64 + 0x11));
    W8(0x1F2B) = W8(0x1A64 + 0x13);
    a = (uint8_t)(*OBJ(y, 0x16) + W8(0x1F2B));
    c = a >= 0x80;
    a = (uint8_t)((a >> 1) | (c ? 0x80 : 0x00));
    if (a & 0x80) {
        a = neg8(a);
        a &= 0xFC;
        a = neg8(a);
    }
    a &= 0xFC;
    if (!((uint8_t)(a - 0x08) & 0x80))
        a = 0x08;
    if ((uint8_t)(a - 0xF8) & 0x80)
        a = 0xF8;
    W8(0x1F58) = a;
    a = (uint8_t)(*OBJ(y, 0x15) - W8(0x1F5A));
    if (a & 0x80)
        a = neg8(a);
    if (!((uint8_t)(a - 0x40) & 0x80)) {
        a = neg8(W8(0x1F58));
        W8(0x1F58) = a;
    }
    OBJ_W(y, 0x5D) = (uint16_t)((uint16_t)(int16_t)(int8_t)W8(0x1F58) + 0x0008 + OBJ_W(y, 0x29));
    a = object_table_c056(y);
    *OBJ(y, 0x18) = (uint8_t)(W8(0x1F5A) | 0x80);
    (void)a;
    if (!W8(0x0D62))
        return;
    W8(0x1F5A) = neg8(W8(0x1A9F + 0x11));
    object_table_c056(y);
    *OBJ(y, 0x19) = (uint8_t)(W8(0x1F5A) | 0x80);
}

/* A timed object's first frame: handler $08:EC8B, count +$4B = $14, then its
   first step. */
void object_timed_setup(uint16_t obj)
{
    OBJ_W(obj, 0x1A) = 0xEC8B;
    *OBJ(obj, 0x1C) = 0x08;
    *OBJ(obj, 0x4B) = 0x14;
    object_timed_step(obj);
}

/* A timed object's frame: rebucketed; when its count +$4B is 0 it is
   released, otherwise the count goes down by one. */
void object_timed_step(uint16_t obj)
{
    uint8_t v;

    object_rebucket(obj);
    v = *OBJ(obj, 0x4B);
    if (!v) {
        object_release_long(obj);
        return;
    }
    *OBJ(obj, 0x4B) = (uint8_t)(v - 1);
}
