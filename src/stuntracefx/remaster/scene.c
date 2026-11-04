/*
 * scene.c: the cartridge's GPU renderer's CPU side (scene.h). Stage 1: the
 * model interpreter's point commands natively, checked against the chip's
 * own run beside it.
 */
#include <stdio.h>
#include <stdlib.h>

#include "gsu.h"
#include "scene.h"
#include "remaster.h"

#define FX_POINT_OUT     0x001Eu
#define FX_POINT_SCALE   0x0030u
#define FX_POINT_SHIFT   0x0032u
#define FX_MATRIX_PACKED 0x0128u
#define FX_VIEW_MATRIX   0x0132u

/* MULT: the low bytes' signed product */
static uint16_t mult8(unsigned a, unsigned b) { return (uint16_t)((int8_t)a * (int8_t)b); }
/* FMULT then ROL (the carry FMULT leaves, the product's bit 15): bits 15 to
   30 of the signed product */
static uint16_t fmult_rol(uint16_t a, uint16_t b) { return (uint16_t)(((int32_t)(int16_t)a * (int16_t)b) >> 15); }

/* the packed matrix's sum (each term a byte of the matrix's high bytes
   times a point byte) to the point's word: doubled, its high byte, times
   the scale */
static uint16_t packed_out(uint16_t s, uint16_t scale) { return mult8((uint16_t)(s + s) >> 8, scale); }

int sc_points(const uint8_t *bytes, int n, int mirrored, uint16_t shift, uint16_t scale, const uint16_t m[9],
              int16_t *out)
{
    int e, j, k = 0;

    for (e = 0; e < n; e++) {
        const uint8_t *p = bytes + 3 * e;
        for (j = 0; j < 3; j++) {
            uint16_t xt, rest;
            if (shift < 3) {            /* FxModelPoints01 01:8AA3, mirrored 01:8B1A: the packed matrix */
                xt = mult8(m[j] >> 8, p[0]);
                rest = (uint16_t)(mult8(m[3 + j] >> 8, p[1]) + mult8(m[6 + j] >> 8, p[2]));
                out[k + j] = (int16_t)packed_out((uint16_t)(rest + xt), scale);
                if (mirrored)
                    out[k + 3 + j] = (int16_t)packed_out((uint16_t)(rest - xt), scale);
            } else {                    /* 01:8982, mirrored 01:8A03: the full matrix */
                xt = fmult_rol(mult8(p[0], scale), m[j]);
                rest = (uint16_t)(fmult_rol(mult8(p[1], scale), m[3 + j]) + fmult_rol(mult8(p[2], scale), m[6 + j]));
                out[k + j] = (int16_t)(uint16_t)(rest + xt);
                if (mirrored)
                    out[k + 3 + j] = (int16_t)(uint16_t)(rest - xt);
            }
        }
        k += mirrored ? 6 : 3;
    }
    return k / 3;
}

/* (a * b) >> 15, signed: FMULT then ROL */
static uint16_t mul15(uint16_t a, uint16_t b) { return fmult_rol(a, b); }

/* the sine of an angle (a turn $10000), the table at $00:983E (257 words,
   1.15) read between its entries by the angle's low byte */
static uint16_t sine(uint16_t angle)
{
    uint32_t at = 0x00983Eu + (uint32_t)(angle >> 8) * 2;
    uint16_t a = (uint16_t)(gsu_peek8(at) | gsu_peek8(at + 1) << 8);
    uint16_t b = (uint16_t)(gsu_peek8(at + 2) | gsu_peek8(at + 3) << 8);
    return (uint16_t)(a + mul15((uint16_t)((angle & 0xFF) << 7), (uint16_t)(b - a)));
}

void sc_matrix(const uint16_t angle[3], const uint8_t *frac, uint16_t flags, const uint16_t e[9], int shift,
               uint16_t out[9])
{
    uint16_t a[3], r[9], t;
    int i, j;

    for (i = 0; i < 3; i++)             /* the angles negated in place, a byte's turn (01:8661) */
        a[i] = (uint16_t)-(uint16_t)((angle[i] & 0xFF) << 8);
    if (frac && (frac[0] | frac[1] | frac[2]))
        for (i = 0; i < 3; i++)
            a[i] = (uint16_t)-(uint16_t)((angle[i] & 0xFF) << 8 | frac[i]);
    else if (!(angle[0] | angle[2]) && (angle[1] == 0 || angle[1] == 0x80)) {
        /* 01:8687 (the whole words tested): no turn, or a half turn about y
           (01:86E4): the view's matrix, its first and last rows negated for
           the half turn */
        int half = angle[1] == 0x80;
        for (i = 0; i < 9; i++)
            out[i] = half && (i < 3 || i >= 6) ? (uint16_t)-e[i] : e[i];
        if (flags & 0x2000)
            out[3] = out[4] = out[5] = 0;
        goto shifted;
    }
    {
        /* FxMatrixFromAngles01 (01:832F): $0020 z, $0022 x, $0024 y */
        uint16_t sz = sine(a[0]), cz = sine((uint16_t)(a[0] + 0x4000));
        uint16_t sx = sine(a[1]), cx = sine((uint16_t)(a[1] + 0x4000));
        uint16_t sy = sine(a[2]), cy = sine((uint16_t)(a[2] + 0x4000));
        uint16_t sx_cy = mul15(sx, cy), cx_cy = mul15(cx, cy), sx_sy = mul15(sx, sy), cx_sy = mul15(cx, sy);
        r[0] = (uint16_t)(mul15(sx_sy, sz) + cx_cy);
        r[1] = (uint16_t)(mul15(sx_cy, sz) - cx_sy);
        r[2] = mul15(sx, cz);
        r[3] = mul15(sy, cz);
        r[4] = mul15(cy, cz);
        r[5] = (uint16_t)-sz;
        r[6] = (uint16_t)(mul15(cx_sy, sz) - sx_cy);
        r[7] = (uint16_t)(mul15(cx_cy, sz) + sx_sy);
        r[8] = mul15(cx, cz);
    }
    t = r[1], r[1] = r[3], r[3] = t;    /* FxMatrixTranspose */
    t = r[2], r[2] = r[6], r[6] = t;
    t = r[5], r[5] = r[7], r[7] = t;
    if (flags & 0x2000)                 /* 01:8751: each vector's y gone */
        r[1] = r[4] = r[7] = 0;
    for (i = 0; i < 3; i++)             /* FxMatrixApplyVectors: each vector times the view's matrix */
        for (j = 0; j < 3; j++)
            out[3 * i + j] = (uint16_t)(mul15(r[3 * i], e[j]) + mul15(r[3 * i + 1], e[3 + j]) +
                                        mul15(r[3 * i + 2], e[6 + j]));
shifted:
    if (shift)                          /* a far object's matrix halved (remaster.h) */
        for (i = 0; i < 9; i++)
            out[i] = (uint16_t)((int16_t)out[i] >> shift);
}

/* ---- SC_CHECK=geom ---- */
static int check = -1, brk;
static long cmds, points, apart, packed_apart, unchecked, slots, matrix_apart;
static uint16_t slot_m[9];
static int slot_on;
static int max_off;
static struct {
    int on, mirrored;
    uint32_t at;
    uint16_t out;
} pend;

static uint8_t peek(uint32_t a) { return gsu_peek8(a); }
static uint16_t ram16(uint16_t a) { return (uint16_t)(peek(0x700000u | a) | peek(0x700000u | (uint16_t)(a + 1)) << 8); }

static void report(void)
{
    fprintf(stderr, "scene geom: %ld point commands, %ld points, %ld words apart (largest %d), %ld packed words apart, "
                    "%ld commands of kinds not checked; %ld slots' matrices, %ld words apart\n",
            cmds, points, apart, max_off, packed_apart, unchecked, slots, matrix_apart);
}

static void check_points(void)
{
    static uint8_t bytes[3 * 256];
    static int16_t mine[6 * 256];
    uint16_t m[9], shift = ram16(FX_POINT_SHIFT), scale = ram16(FX_POINT_SCALE), out;
    uint32_t bank = pend.at & 0xFF0000u;
    int n = peek(bank | (uint16_t)(pend.at + 1)), i, k;

    for (i = 0; i < 9; i++)
        m[i] = ram16((uint16_t)(FX_VIEW_MATRIX + 2 * i));
    if (slot_on) {                      /* the slot's matrix natively, against the chip's */
        slot_on = 0;
        slots++;
        if (brk)
            slot_m[4] ^= 1;
        for (i = 0; i < 9; i++)
            matrix_apart += slot_m[i] != m[i];
    }
    for (i = 0; i < 3 * n; i++)
        bytes[i] = peek(bank | (uint16_t)(pend.at + 2 + i));
    k = sc_points(bytes, n, pend.mirrored, shift, scale, m, mine);
    if (brk)
        mine[0] ^= 1;
    cmds++;
    points += k;
    for (i = 0; i < 3 * k; i++) {
        int16_t chip = (int16_t)ram16((uint16_t)(pend.out + 2 * i));
        int d = abs(chip - mine[i]);
        if (d) {
            apart++;
            if (d > max_off)
                max_off = d;
        }
    }
    out = ram16(FX_POINT_OUT);
    if (out != (uint16_t)(pend.out + 6 * k))
        apart++;
    if (shift < 3) {                    /* the packed matrix the chip built (01:87F2): the high bytes, two to a word */
        static const int lo[5] = { 0, 2, 4, 6, 8 }, hi[5] = { 1, 3, 5, 7, -1 };
        for (i = 0; i < 5; i++) {
            uint16_t want = (uint16_t)((m[lo[i]] >> 8) | (hi[i] < 0 ? 0 : (m[hi[i]] >> 8) << 8));
            if (ram16((uint16_t)(FX_MATRIX_PACKED + 2 * i)) != want)
                packed_apart++;
        }
    }
}

void sc_model_begin(void)
{
    uint16_t angle[3], e[9];
    int i;

    if (check <= 0)
        return;
    for (i = 0; i < 3; i++)
        angle[i] = ram16((uint16_t)(0x0020 + 2 * i));
    for (i = 0; i < 9; i++)
        e[i] = ram16((uint16_t)(0x00E4 + 2 * i));
    sc_matrix(angle, rm_angle_frac, ram16(0x0054), e, rm_far_shift, slot_m);
    slot_on = 1;
}

void sc_command(unsigned index, uint8_t bank, uint16_t at)
{
    if (check < 0) {
        const char *c = getenv("SC_CHECK");
        check = c && c[0] == 'g';
        brk = getenv("SC_BREAK") != NULL;
        if (check)
            atexit(report);
    }
    if (!check)
        return;
    if (pend.on)
        check_points();
    pend.on = index == 0x04 / 4 || index == 0x38 / 4;
    if (index == 0x08 / 4 || index == 0x4C / 4 || index == 0x34 / 4)
        unchecked++;
    if (pend.on) {
        pend.mirrored = index == 0x38 / 4;
        pend.at = (uint32_t)bank << 16 | at;
        pend.out = ram16(FX_POINT_OUT);
    }
}
