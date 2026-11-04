/*
 * scene.c: the cartridge's GPU renderer's CPU side (scene.h). Stage 1: the
 * model interpreter's point commands natively, checked against the chip's
 * own run beside it.
 */
#include <stdio.h>
#include <stdlib.h>

#include "gsu.h"
#include "scene.h"

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

/* ---- SC_CHECK=geom ---- */
static int check = -1, brk;
static long cmds, points, apart, packed_apart, unchecked;
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
                    "%ld commands of kinds not checked\n",
            cmds, points, apart, max_off, packed_apart, unchecked);
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
