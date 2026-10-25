/*
 * fx_sums.c: the vector sums of the object the Super FX is moving, and the
 * pushes added into them.
 */
#include "game.h"
#include "physics.h"

#define fx_push_flags       FXU(0x2480)     /* the flag bytes of the pushes applied */
#define fx_push_amount      FXU(0x2378)     /* the last push's amount, doubled */
#define fx_push_turn        FXU(0x247E)     /* the turn the pushes' directions take */
#define FX_PUSH_DIR         0x236Cu         /* the last push's direction, turned */
#define FX_MATRIX           0x00E4u         /* 3 x 3, column j, row i at +6j + 2i */
#define FX_PUSH_ROT         0x0132u         /* a push in the object's axes: x, y, z 8 bytes apart */
#define FX_PUSH_POINT       0x26F6u
#define FX_SPIN             0x233Au         /* the spin sums: z, x, y */

#define M(i)                FXW(FX_MATRIX + 2 * (i))

void fx_sums_clear(void)
{
    int i;

    for (i = 0; i < 6; i++)
        FXU(FX_WORLD_SUM + 2 * i) = 0;
}

/* The own-axes sum turned into world axes by the matrix and added to the
   world sum. */
void fx_sums_rotate_local(void)
{
    int16_t l[3];
    int i;

    for (i = 0; i < 3; i++)
        l[i] = FXW(FX_LOCAL_SUM + 2 * i);
    for (i = 0; i < 3; i++)
        FXW(FX_WORLD_SUM + 2 * i) = wrap16(FXW(FX_WORLD_SUM + 2 * i) + mul15(l[0], M(i)) + mul15(l[1], M(3 + i))
                                           + mul15(l[2], M(6 + i)));
}

/* The world sum, when not zero, added to the record: half of it to the
   position (+$1E), twice x, eight times y and twice z to the velocity
   (+$2E). */
void fx_sums_apply(uint16_t rec)
{
    static const int times[3] = {2, 8, 2};
    int16_t v[3];
    int i;

    for (i = 0; i < 3; i++)
        v[i] = FXW(FX_WORLD_SUM + 2 * i);
    if (!(v[0] | v[1] | v[2]))
        return;
    for (i = 0; i < 3; i++)
        FXW(rec + 0x1E + 2 * i) = wrap16(div2(v[i]) + FXW(rec + 0x1E + 2 * i));
    for (i = 0; i < 3; i++)
        FXW(rec + 0x2E + 2 * i) = wrap16(v[i] * times[i] + FXW(rec + 0x2E + 2 * i));
}

static void push_into(uint16_t sum, int16_t v[3])
{
    int i;

    for (i = 0; i < 3; i++) {
        if (v[i] >= 0 ? wrap16(v[i] - 5000) >= 0 : wrap16(v[i] + 5000) < 0)
            v[i] = v[i] >= 0 ? 5000 : -5000;
        FXW(sum + 2 * i) = wrap16(FXW(sum + 2 * i) + v[i]);
    }
}

void fx_sums_push_world(int16_t v[3])
{
    push_into(FX_WORLD_SUM, v);
}

void fx_sums_push_local(int16_t v[3])
{
    push_into(FX_LOCAL_SUM, v);
}

/* The push at *at (flags, amount, direction), *at moved past it: none
   (no flags, or flags above the low byte) or skipped (a negative amount,
   or $7D00) changes nothing; else its flags are noted, its direction is
   turned about the vertical by fx_push_turn and kept, and twice its amount
   along it goes into the world sum. */
uint16_t fx_push_apply(uint16_t *at)
{
    uint16_t p = *at, flags = FXU(p);
    int16_t amount, d[3];
    uint16_t turn;
    int i;

    *at = (uint16_t)(p + 10);
    if (!flags || (flags >> 8))
        return 8;
    amount = FXW(p + 2);
    if (amount < 0 || amount == 0x7D00)
        return 6;
    fx_push_flags |= flags;
    amount = wrap16(amount * 2);
    fx_push_amount = (uint16_t)amount;
    for (i = 0; i < 3; i++)
        d[i] = FXW(p + 4 + 2 * i);
    turn = fx_push_turn;
    if (turn == 0x40) {
        int16_t x = d[0];

        d[0] = wrap16(-d[2]);
        d[2] = x;
    } else if (turn == 0xC0) {
        int16_t x = d[0];

        d[0] = d[2];
        d[2] = wrap16(-x);
    } else if (turn == 0x80) {
        d[0] = wrap16(-d[0]);
        d[2] = wrap16(-d[2]);
    } else if (turn) {
        int16_t x = d[0], z = d[2];

        if (x | z) {
            fx_turn(&x, &z, (uint8_t)turn);
            x = wrap16(x * 4);
            z = wrap16(z * 4);
        }
        d[0] = z;
        d[2] = x;
    }
    for (i = 0; i < 3; i++)
        FXW(FX_PUSH_DIR + 2 * i) = d[i];
    for (i = 0; i < 3; i++)
        d[i] = fmult(amount, FXW(FX_PUSH_DIR + 2 * i));
    fx_sums_push_world(d);
    return 1;
}

/* The nineteen pushes from FX_PUSHES. */
uint16_t fx_pushes_apply(void)
{
    uint16_t at = FX_PUSHES;
    int i;

    fx_push_flags = 0;
    for (i = 0; i < 19; i++)
        fx_push_apply(&at);
    return fx_push_flags;
}

/* the high word of twice a * [p] - b * [q] (32 bits) */
static int16_t cross(int16_t a, uint16_t p, int16_t b, uint16_t q)
{
    uint32_t d = (uint32_t)((int32_t)a * FXW(p)) - (uint32_t)((int32_t)b * FXW(q));

    return (int16_t)(uint16_t)((d << 1) >> 16);
}

/* sum over j of mul15(v[j], the matrix word at m + step * j) */
static int16_t row(const int16_t v[3], uint16_t m, uint16_t step)
{
    return wrap16(mul15(v[0], FXW(m)) + mul15(v[1], FXW(m + step)) + mul15(v[2], FXW(m + 2 * step)));
}

/* A push at a point of the object (in its own axes): the world sum scaled
   (by 4, 2, 1, 1/2 or 1/4, by the variant FX_PUSH_AT's offset picks: 0,
   2, 4, 6, 8; 10 is 4 and a twist), turned into the object's axes, and
   the point (times 16) crossed with it added four times to the spin
   sums. */
void fx_push_at(uint16_t which, const int16_t point[3])
{
    static const int shift[6] = {2, 1, 0, -1, -2, 2};
    int16_t p[3], s[3];
    int i, k;

    for (i = 0; i < 3; i++) {
        int16_t v = FXW(FX_WORLD_SUM + 2 * i);

        for (k = 0; k < shift[which / 2]; k++)
            v = wrap16(v * 2);
        for (k = 0; k > shift[which / 2]; k--)
            v = div2(v);
        FXW(FX_PUSH_ROT + 8 * i) = v;
    }
    for (i = 0; i < 3; i++)
        p[i] = wrap16(point[i] * 16);
    FXU(0x0134) = 0;
    for (i = 0; i < 3; i++)
        FXW(FX_PUSH_POINT + 2 * i) = p[i];
    for (i = 0; i < 3; i++)
        s[i] = FXW(FX_PUSH_ROT + 8 * i);
    for (i = 0; i < 3; i++)
        FXW(FX_PUSH_ROT + 8 * i) = row(s, (uint16_t)(FX_MATRIX + 6 * i), 2);
    FXW(0x0068) = cross(p[2], FX_PUSH_ROT + 8, p[1], FX_PUSH_ROT + 16);
    FXW(0x002C) = cross(p[0], FX_PUSH_ROT + 16, p[2], FX_PUSH_ROT);
    FXW(0x002E) = cross(p[1], FX_PUSH_ROT, p[0], FX_PUSH_ROT + 8);
    FXW(FX_SPIN + 2) = wrap16(FXW(0x0068) * 4 + FXW(FX_SPIN + 2));
    FXW(FX_SPIN + 4) = wrap16(FXW(0x002E) * 4 + FXW(FX_SPIN + 4));
    FXW(FX_SPIN) = wrap16(FXW(0x002C) * 4 + FXW(FX_SPIN));
    if (which == 10)
        FXW(FX_SPIN + 2) = wrap16(FXW(0x0068) * 8 + FXW(FX_SPIN + 2));
}

/* each of v times half the world sum's word, added to the hit point */
static void hit_add(const int16_t v[3])
{
    int i;

    for (i = 0; i < 3; i++)
        FXW(FX_HIT_POINT + 2 * i) = wrap16(FXW(FX_HIT_POINT + 2 * i) + fmult(v[i], div2(FXW(FX_WORLD_SUM + 2 * i))));
}

static int16_t magnitude(int16_t v)
{
    return v < 0 ? wrap16(-v) : v;
}

/* v scaled by `scale` (the low word of the product), made positive, into
   the hit point; a zero v counts as $7FFF each way. */
void fx_hit_add_scaled(int16_t v[3], int16_t scale)
{
    int i;

    if (!(v[0] | v[1] | v[2])) {
        v[0] = v[1] = v[2] = 0x7FFF;
    } else {
        for (i = 0; i < 3; i++)
            v[i] = magnitude((int16_t)(uint16_t)((int32_t)v[i] * scale));
    }
    hit_add(v);
}

/* as fx_hit_add_scaled with v made a unit vector */
void fx_hit_add_unit(int16_t v[3])
{
    int i;

    if (!(v[0] | v[1] | v[2])) {
        v[0] = v[1] = v[2] = 0x7FFF;
    } else {
        fx_normalize(v);
        for (i = 0; i < 3; i++)
            v[i] = magnitude(v[i]);
    }
    hit_add(v);
}

/* the world sum times scale as 8.8 */
void fx_sum_scale(int16_t scale)
{
    int i;

    for (i = 0; i < 3; i++)
        FXW(FX_WORLD_SUM + 2 * i) = (int16_t)(uint16_t)(((int32_t)FXW(FX_WORLD_SUM + 2 * i) * scale) >> 8);
}
