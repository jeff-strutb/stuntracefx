/*
 * fx_math.c: the Super FX's arithmetic: divide, square root, normalising a
 * vector, the angle of a vector, turning one, and rotation matrices.
 *
 * The Super FX multiplies signed 16 x 16 into 32 bits; its "1.15" products
 * keep bits 15-30 (FMULT and a rotate left). The 8-bit sine tables are used
 * as the byte times 256.
 */
#include "fixmath.h"
#include "rom.h"

#define ATAN_TABLE 0x009E40u   /* angle words by tangent / 64, 257 entries */

FxDiv fx_divide(uint32_t n, uint16_t d)
{
    uint16_t hi = (uint16_t)(n >> 16), lo = (uint16_t)n;
    FxDiv o;
    int i;

    /* the pair shifted left one bit ahead of each round: the remainder's
       top bit falls out there, and the last shift is taken back at the end */
    hi = (uint16_t)(hi << 1 | lo >> 15);
    lo = (uint16_t)(lo << 1);
    for (i = 0; i < 16; i++) {
        unsigned q = hi >= d, top = lo >> 15;

        if (q)
            hi = (uint16_t)(hi - d);
        lo = (uint16_t)(lo << 1 | q);
        hi = (uint16_t)(hi << 1 | top);
    }
    o.quot = lo;
    o.rem = (uint16_t)(hi >> 1);
    return o;
}

FxDiv fx_divide_halved_if_large(uint32_t n, uint16_t *d)
{
    if ((n & 0x80000000u) || (*d & 0x8000)) {
        *d = (uint16_t)(*d >> 1);
        n >>= 1;
    }
    return fx_divide(n, *d);
}

/* Two bits at a time into the remainder (kept to 16 bits), the root's next
   bit set when the remainder exceeds twice the root so far. */
FxSqrt fx_sqrt(uint32_t n)
{
    uint16_t root = 0, rem = 0;
    int i;

    for (i = 0; i < 16; i++) {
        uint16_t trial;

        rem = (uint16_t)(rem << 2 | n >> 30);
        n <<= 2;
        root = (uint16_t)(root << 1);
        trial = (uint16_t)(root << 1);
        if (trial < rem) {
            rem = (uint16_t)(rem - trial - 1);
            root++;
        }
    }
    return (FxSqrt){root, rem};
}

/* The squared length as 32 bits, its root, and $7FFF scaled up by the
   root's top bit number k divided by the root; each component times that
   quotient as 32 bits, shifted back down by k. */
void fx_normalize(int16_t v[3])
{
    uint32_t sum = 0;
    uint16_t root, d;
    FxDiv q;
    int i, k = 0;

    for (i = 0; i < 3; i++)
        sum += (uint32_t)((int32_t)v[i] * v[i]);
    if (!sum)
        return;
    root = fx_sqrt(sum).root;
    while (root >> (k + 1))
        k++;
    d = root;
    q = fx_divide_halved_if_large((uint32_t)0x7FFF << k, &d);
    for (i = 0; i < 3; i++)
        v[i] = wrap16(asr32((int32_t)v[i] * (int16_t)q.quot, k));
}

/* The smaller magnitude over the larger, times $4000, indexes the table
   (64 steps to an entry); the angle is the entry when |y| is the larger,
   else a quarter turn less it, then signed by the quadrant. The magnitudes
   are compared signed (so |-32768| counts as the smaller). */
uint16_t fx_angle_of(int16_t x, int16_t y)
{
    uint16_t ax = (uint16_t)(x < 0 ? -x : x), ay = (uint16_t)(y < 0 ? -y : y), a;

    if (ay == 0) {
        a = 0x4000;
    } else if (ay == ax) {
        a = 0x2000;
    } else {
        int y_larger = (int16_t)ay > (int16_t)ax;
        uint16_t small = y_larger ? ax : ay, large = y_larger ? ay : ax;
        FxDiv q = fx_divide((uint32_t)small << 14, large);

        a = rom_word(ATAN_TABLE + ((q.quot >> 5) & ~1u));
        if (!y_larger)
            a = (uint16_t)(0x4000 - a);
    }
    if ((x ^ y) < 0)
        a = (uint16_t)-a;
    if (y < 0)
        a = (uint16_t)(a + 0x8000);
    return a;
}

void fx_angle_vector(uint8_t angle, int16_t length, int16_t *x, int16_t *z)
{
    int16_t s = (int16_t)(uint16_t)(sin8(angle) << 8), c = (int16_t)(uint16_t)(cos8(angle) << 8);

    *x = wrap16((int32_t)(uint16_t)-(uint16_t)asr32((int32_t)s * length, 16) << 4);
    *z = wrap16((int32_t)(uint16_t)asr32((int32_t)c * length, 16) << 4);
}

void fx_turn(int16_t *x, int16_t *z, uint8_t angle)
{
    int32_t s = sin8(angle) * 256, c = cos8(angle) * 256;
    uint32_t sx = (uint32_t)(s * *x), cz = (uint32_t)(c * *z);
    uint32_t sz = (uint32_t)(s * *z), cx = (uint32_t)(c * *x);
    uint16_t hi = (uint16_t)((cx >> 16) - (sz >> 16) - ((uint16_t)sz < (uint16_t)cx));

    *x = asr16((int16_t)(uint16_t)((sx + cz) >> 16), 1);
    *z = asr16((int16_t)hi, 1);
}

void fx_matrix_transpose(int16_t m[9])
{
    static const int pairs[3][2] = {{1, 3}, {2, 6}, {5, 7}};
    int i;

    for (i = 0; i < 3; i++) {
        int16_t t = m[pairs[i][0]];

        m[pairs[i][0]] = m[pairs[i][1]];
        m[pairs[i][1]] = t;
    }
}

/* A 1.15 product: bits 15-30 of the signed product. */
static int16_t mul15(int16_t a, int16_t b)
{
    return wrap16(asr32((int32_t)a * b, 15));
}

void fx_matrix_from_angles(const uint16_t angles[3], int16_t m[9])
{
    int16_t s0 = fx_sin16(angles[0]), c0 = fx_cos16(angles[0]);
    int16_t s1 = fx_sin16(angles[1]), c1 = fx_cos16(angles[1]);
    int16_t s2 = fx_sin16(angles[2]), c2 = fx_cos16(angles[2]);
    int16_t s1c2 = mul15(s1, c2), c1c2 = mul15(c1, c2), s1s2 = mul15(s1, s2), c1s2 = mul15(c1, s2);

    m[0] = wrap16(mul15(s1s2, s0) + c1c2);
    m[1] = wrap16(mul15(s1c2, s0) - c1s2);
    m[2] = mul15(s1, c0);
    m[3] = mul15(s2, c0);
    m[4] = mul15(c2, c0);
    m[5] = wrap16(-s0);
    m[6] = wrap16(mul15(c1s2, s0) - s1c2);
    m[7] = wrap16(mul15(c1c2, s0) + s1s2);
    m[8] = mul15(c1, c0);
}
