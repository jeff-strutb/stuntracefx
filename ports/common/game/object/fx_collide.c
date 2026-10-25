/*
 * fx_collide.c: two records touching: the push between them, whether
 * they stay apart, and the contacts each keeps of the other.
 *
 * A record's position is at +$1E/+$20/+$22, its velocity at +$2E/+$30/
 * +$32, its box's half sizes at +$0C/+$0E/+$10 and its mass at +$2A; +$16
 * and +$1A hold flags and masks. A contact ($12 bytes from the first pool,
 * on a record's list from +$18 through +$0E) has the other record at
 * +$0C, flags at +$10 (bit 0 seen this step, bit 2 keep the copy) and a
 * copy of the other's velocity x, z and position x, z at +$04-+$0A.
 */
#include "game.h"
#include "physics.h"

#define REC(r, f)           FXW((uint16_t)((r) + (f)))

/* The push (FX_PUSH) on record `rec` from the other record `other`, whose
   contact words are at `c`: along the unit vector from the other's place
   to this one's on the ground plane, its amount the closing speed (at
   least $04B0 when it is not negative) times the other's share of the two
   masses. */
void fx_collide_push(uint16_t c, uint16_t other, uint16_t rec)
{
    int16_t n[3], closing, share, amount;
    uint16_t sum;

    n[0] = wrap16(REC(rec, 0x1E) - REC(c, 8));
    n[1] = 0;
    n[2] = wrap16(REC(rec, 0x22) - REC(c, 0x0A));
    FXW(0x0020) = n[0];
    FXW(0x0022) = 0;
    FXW(0x0024) = n[2];
    fx_normalize(n);
    FXW(0x0068) = n[0];
    FXW(0x002C) = n[1];
    FXW(0x002E) = n[2];
    closing = wrap16(fmult(wrap16(2 * wrap16(REC(c, 4) - REC(rec, 0x2E))), FXW(0x0068))
                     + fmult(wrap16(2 * wrap16(REC(c, 6) - REC(rec, 0x32))), FXW(0x002E)));
    if (closing >= 0 && wrap16(0x04B0 - closing) >= 0)
        closing = 0x04B0;
    FXW(0x26F6) = closing;
    sum = (uint16_t)(REC(rec, 0x2A) + REC(other, 0x2A));
    share = (int16_t)(fx_divide_halved_if_large((uint32_t)(uint16_t)REC(other, 0x2A) << 16, &sum).quot >> 1);
    amount = fmult(wrap16(closing * 2), share);
    FXU(FX_PUSH) = amount >= 0;
    FXW(FX_PUSH + 2) = amount;
    FXW(FX_PUSH + 4) = FXW(0x0068);
    FXU(FX_PUSH + 6) = 0;
    FXW(FX_PUSH + 8) = FXW(0x002E);
}

/* Whether records a and b stay apart this step: their distance on the
   ground plane beyond their sizes less $294 (no less than 0) and how fast
   they close. */
int fx_records_apart(uint16_t a, uint16_t b)
{
    int16_t n[3], reach;

    FXW(0x0068) = wrap16(REC(a, 0x1E) - REC(b, 0x1E));
    FXW(0x002E) = wrap16(REC(a, 0x22) - REC(b, 0x22));
    n[0] = FXW(0x0068);
    n[1] = 0;
    n[2] = FXW(0x002E);
    fx_normalize(n);
    FXW(0x0068) = wrap16(fmult(wrap16(FXW(0x0068) * 2), n[0]) + fmult(wrap16(FXW(0x002E) * 2), n[2]));
    FXW(0x006A) = wrap16(-(fmult(div2(wrap16(REC(a, 0x2E) - REC(b, 0x2E))), n[0])
                           + fmult(div2(wrap16(REC(a, 0x32) - REC(b, 0x32))), n[2])));
    reach = wrap16(REC(a, 0x0C) + REC(b, 0x0C) - 0x0294);
    if (reach < 0)
        reach = 0;
    return wrap16(wrap16(reach + FXW(0x006A)) - FXW(0x0068)) < 0;
}

/* That `owner` touches `other`: its contact of the other found or made,
   the other's velocity and place copied into it (unless it keeps its
   copy), and marked seen. Nothing when the owner's +$16 has bit 7, or the
   pool has no room. */
void fx_contact_note(uint16_t owner, uint16_t other)
{
    uint16_t c;

    if (FXU(owner + 0x16) & 0x80)
        return;
    for (c = FXU(owner + 0x18); c; c = FXU(c + 0x0E))
        if (FXU(c + 0x0C) == other)
            break;
    if (!c) {
        c = fx_pool_block(0x1098, 0x12);
        if (!c)
            return;
        FXU(c + 0x0C) = other;
        FXU(c + 0x0E) = FXU(owner + 0x18);
        FXU(owner + 0x18) = c;
        FXU(c + 0x10) = 0;
    }
    if (!(FXU(c + 0x10) & 4)) {
        FXU(c + 4) = FXU(other + 0x2E);
        FXU(c + 6) = FXU(other + 0x32);
        FXU(c + 8) = FXU(other + 0x1E);
        FXU(c + 0x0A) = FXU(other + 0x22);
    }
    FXU(c + 0x10) |= 1;
}

/* |the other's position - mine| on one axis below the two sizes */
static int overlaps(uint16_t r, uint16_t size, uint16_t mine_size, uint16_t pos, int16_t mine)
{
    int16_t d = wrap16(REC(r, pos) - mine);

    if (d < 0)
        d = wrap16(-d);
    return wrap16(d - wrap16(REC(r, size) + FXW(mine_size))) < 0;
}

/* Record `rec` against every record after it on the list of all records:
   boxes that overlap (z, x, then y) note each other as contacts, unless
   both may stay apart (bit 1 of +$16) and do, or one's +$1A masks the
   other's +$16. Then its contacts not seen are dropped. */
void fx_record_touches(uint16_t rec)
{
    int16_t x = REC(rec, 0x1E), y = REC(rec, 0x20), z = REC(rec, 0x22);
    uint16_t r;
    int i;

    for (i = 0; i < 3; i++)
        FXU(0x236C + 2 * i) = FXU(rec + 0x0C + 2 * i);
    for (r = FXU(rec + 4); r; r = FXU(r + 4)) {
        if (!overlaps(r, 0x10, 0x2370, 0x22, z) || !overlaps(r, 0x0C, 0x236C, 0x1E, x)
            || !overlaps(r, 0x0E, 0x236E, 0x20, y))
            continue;
        if ((FXU(rec + 0x16) & FXU(r + 0x16) & 2) && fx_records_apart(rec, r))
            continue;
        if (!(FXU(rec + 0x1A) & FXU(r + 0x16)))
            fx_contact_note(rec, r);
        if (!(FXU(r + 0x1A) & FXU(rec + 0x16)))
            fx_contact_note(r, rec);
    }
    if (FXU(rec + 0x18))
        fx_list_sweep(rec);
}

void fx_all_touches(void)
{
    uint16_t r;

    for (r = FXU(0x2332); r; r = FXU(r + 4))
        fx_record_touches(r);
}
