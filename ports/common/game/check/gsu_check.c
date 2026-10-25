/* old: gsu/run.c gsu/params.c gsu/screen.c render/unused.c render/matrix.c render/arith.c render/angle.c math/fx_math.c gsu/fx_init.c
 *
 * gsu_check.c: the port's S-CPU side of the Super FX (gsu/glue.c) against
 * the decomp's, on random states. The decomp's S-CPU code starts the
 * decomp's Super FX programs through the bus model (model_gsu_start); the
 * work RAM, the game pak RAM and the Super FX's screen are compared, but
 * the poll counts the original keeps while it waits ($FE-$FF, $1F21-$1F22).
 *
 *   check/check.sh gsu N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define gsu old_gsu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#define gsu_rd8 old_gsu_rd8
#define gsu_wr8 old_gsu_wr8
#define gsu_plot old_gsu_plot
#define gsu_rpix old_gsu_rpix
#define gsu_code old_gsu_code
#define gsu_cache old_gsu_cache
#define gsu_cyc old_gsu_cyc
#define gsu_time old_gsu_time
#define gsu_buffer old_gsu_buffer
#include "machine.h"
#include "cpu_ops.h"
#include "gsu.h"
#undef cpu
#undef gsu

#include "game.h"
#include "render.h"
#include "glue.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(GsuRun) OLD(GsuWait) OLD(GsuParamsUpdate) OLD(GsuParamsFromDp) OLD(GsuParam216A) OLD(GsuParam216B)
OLD(GsuTableEntryTo1F4C) OLD(GsuTableEntryNext) OLD(GsuRecordsUpload) OLD(GsuScreenFlip) OLD(GsuScreenFlipMenu)
OLD(GsuScreenToggle) OLD(GsuBytes200_BFB7B6) OLD(GsuBytes200_0F060E) OLD(GsuBytes200_0F010B) OLD(GsuBytes200_BFB5BB)
OLD(GsuBytes200_BFBCBD) OLD(GsuBytes200_BFBAB9) OLD(GsuBytes200_BFB3BE) OLD(GsuBytes200_BFB2B3) OLD(GsuBytes200_BF12B6)
OLD(GsuBytes200_BF11BD) OLD(GsuBytes200_BF10B9) OLD(GsuBytes200_BFB7B6_D8C4) OLD(GsuBytes200_BFBAB9_D8E1)
OLD(GsuBytes200_BFB3BE_D8FE) OLD(GsuBytes200_BFBBBE) OLD(GsuBytes200_0F0405) OLD(GsuBytes200_0F090A)
OLD(GsuBytes200_0F050E) OLD(GsuBytes200_0F080E) OLD(GsuBytes200_BFBABD) OLD(GsuBytes200_0F0708) OLD(GsuBytes200_0F0D03)
OLD(FxClearScreenIfSet) OLD(FxClear6000IfSet) OLD(FxMatrixFromAnglesProgram) OLD(FxInit)

static uint64_t rng = 0x6A09E667F3BCC909ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

/* ---- the decomp's programs, by where they start ---- */
static const struct {
    uint32_t at;
    void (*run)(void);
} olds[] = {
    {0x01AECA, old_abi_FxClearScreenIfSet},
    {0x01AEE1, old_abi_FxClear6000IfSet},
    {0x018325, old_abi_FxMatrixFromAnglesProgram},
    {0x048800, old_abi_FxInit},
};

static void old_start(uint8_t bank, uint16_t start)
{
    uint32_t at = (uint32_t)(bank & 0x7F) << 16 | start;
    unsigned i;

    for (i = 0; i < sizeof olds / sizeof olds[0]; i++)
        if (olds[i].at == at) {
            old_gsu.r[10] = 0x0240;
            olds[i].run();
            return;
        }
    fprintf(stderr, "check: %s: the decomp started $%06X, which the check lacks\n", model_current, (unsigned)at);
    exit(2);
}

/* ---- running both ---- */
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

static void fresh(void)
{
    size_t i;

    for (i = 0; i < sizeof wram; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = (rnd() & 1) ? 0x7E : 0x00;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
    memset(&old_gsu, 0, sizeof old_gsu);
    old_gsu.scbr = fx_screen.base = (uint8_t)rnd();
    fx_screen.mode = 0, fx_screen.height = 0;
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    model_pixels_empty();
    f();
    model_pixels_flush();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

static int counted(size_t i)
{
    return i == 0xFE || i == 0xFF || i == 0x1F21 || i == 0x1F22;
}

static void same_ram(void)
{
    size_t i;

    fx_pixels_flush();
    cases++;
    for (i = 0; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && !counted(i)) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
}

static void same_screen(void)
{
    if (fx_screen.base != old_gsu.scbr)
        fail("screen base %02X, the decomp's %02X", fx_screen.base, old_gsu.scbr);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;
    static const struct {
        void (*old)(void);
        uint8_t b[3];
    } bytes[] = {
        {old_abi_GsuBytes200_BFB7B6, {0xBF, 0xB7, 0xB6}}, {old_abi_GsuBytes200_0F060E, {0x0F, 0x06, 0x0E}},
        {old_abi_GsuBytes200_0F010B, {0x0F, 0x01, 0x0B}}, {old_abi_GsuBytes200_BFB5BB, {0xBF, 0xB5, 0xBB}},
        {old_abi_GsuBytes200_BFBCBD, {0xBF, 0xBC, 0xBD}}, {old_abi_GsuBytes200_BFBAB9, {0xBF, 0xBA, 0xB9}},
        {old_abi_GsuBytes200_BFB3BE, {0xBF, 0xB3, 0xBE}}, {old_abi_GsuBytes200_BFB2B3, {0xBF, 0xB2, 0xB3}},
        {old_abi_GsuBytes200_BF12B6, {0xBF, 0x12, 0xB6}}, {old_abi_GsuBytes200_BF11BD, {0xBF, 0x11, 0xBD}},
        {old_abi_GsuBytes200_BF10B9, {0xBF, 0x10, 0xB9}}, {old_abi_GsuBytes200_BFB7B6_D8C4, {0xBF, 0xB7, 0xB6}},
        {old_abi_GsuBytes200_BFBAB9_D8E1, {0xBF, 0xBA, 0xB9}}, {old_abi_GsuBytes200_BFB3BE_D8FE, {0xBF, 0xB3, 0xBE}},
        {old_abi_GsuBytes200_BFBBBE, {0xBF, 0xBB, 0xBE}}, {old_abi_GsuBytes200_0F0405, {0x0F, 0x04, 0x05}},
        {old_abi_GsuBytes200_0F090A, {0x0F, 0x09, 0x0A}}, {old_abi_GsuBytes200_0F050E, {0x0F, 0x05, 0x0E}},
        {old_abi_GsuBytes200_0F080E, {0x0F, 0x08, 0x0E}}, {old_abi_GsuBytes200_BFBABD, {0xBF, 0xBA, 0xBD}},
        {old_abi_GsuBytes200_0F0708, {0x0F, 0x07, 0x08}}, {old_abi_GsuBytes200_0F0D03, {0x0F, 0x0D, 0x03}},
    };

    if (argc < 2) {
        fprintf(stderr, "usage: gsu_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_gsu_start = old_start;

    CASE("GsuRun") {
        static const uint32_t at[4] = {0x01AECA, 0x01AEE1, 0x018325, 0x048800};
        int w = (int)(rnd() % 4);

        fresh();
        pakram[0x01C4] = (uint8_t)((rnd() & 1) ? rnd() : 0);
        old_cpu.p = (uint8_t)((old_cpu.p | P_M) & ~P_X);
        old_cpu.a = (uint16_t)((old_cpu.a & 0xFF00) | (at[w] >> 16));
        old_cpu.x = (uint16_t)at[w];
        old_run(old_abi_GsuRun);
        gsu_run((uint8_t)(at[w] >> 16), (uint16_t)at[w]);
        same_ram();
        if (fx_screen.mode != (W8(0x0374) & 3))
            fail("screen mode %d, the S-CPU's %d", fx_screen.mode, W8(0x0374) & 3);
    }
    CASE("GsuWait") {
        fresh();
        old_cpu.p |= P_X;
        old_run(old_abi_GsuWait);
        same_ram();
    }
    CASE("GsuParamsUpdate") {
        fresh();
        old_run(old_abi_GsuParamsUpdate);
        gsu_params_update();
        same_ram();
    }
    CASE("GsuParamsFromDp") {
        fresh();
        old_cpu.p |= P_M;
        old_run(old_abi_GsuParamsFromDp);
        gsu_params_from_dp();
        same_ram();
    }
    CASE("GsuParam216A / GsuParam216B") {
        int b = (int)(rnd() & 1);
        uint16_t k = (uint16_t)(rnd() % 10);

        fresh();
        old_cpu.db = 0x07;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_cpu.a = k;
        old_run(b ? old_abi_GsuParam216B : old_abi_GsuParam216A);
        gsu_param_216(b ? 0x8E52 : 0x8E3E, k);
        same_ram();
    }
    CASE("GsuTableEntryTo1F4C / GsuTableEntryNext") {
        int next = (int)(rnd() & 1);
        uint16_t k;

        fresh();
        pakram[0x1094] = (uint8_t)(1 + rnd() % 0x1F);
        FXU(0x1092) = (uint16_t)(0x8000 + (rnd() % 0x7000 & ~1u));
        k = (uint16_t)((rnd() & 3) || !next ? 1 + rnd() % 30 : rom_word((uint32_t)pakram[0x1094] << 16 | FXU(0x1092)));
        old_cpu.db = (uint8_t)((rnd() & 1) ? 0x7E : 0x00);
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_cpu.a = k;
        old_run(next ? old_abi_GsuTableEntryNext : old_abi_GsuTableEntryTo1F4C);
        if (next) {
            uint16_t v = gsu_table_entry_next(k);

            if (v != old_cpu.a)
                fail("entry %04X, the decomp's %04X", v, old_cpu.a);
        } else {
            gsu_table_entry_to_1f4c(k);
        }
        same_ram();
    }
    CASE("GsuBytes200_*") {
        int w = (int)(rnd() % (sizeof bytes / sizeof bytes[0]));

        fresh();
        old_run(bytes[w].old);
        gsu_inputs_200(bytes[w].b[0], bytes[w].b[1], bytes[w].b[2]);
        same_ram();
    }
    CASE("GsuRecordsUpload") {
        fresh();
        W16(0x0CEE) = (uint16_t)((rnd() & 7) ? rnd() % 40 : 0);
        old_run(old_abi_GsuRecordsUpload);
        gsu_records_upload();
        same_ram();
    }
    CASE("GsuScreenFlip / GsuScreenFlipMenu / GsuScreenToggle") {
        int w = (int)(rnd() % 3);
        void (*const f[3])(void) = {old_abi_GsuScreenFlip, old_abi_GsuScreenFlipMenu, old_abi_GsuScreenToggle};

        fresh();
        old_cpu.db = (uint8_t)(rnd() % 0x40);  /* a bank with both the registers and work RAM */
        if (rnd() & 1)
            W16(0x0D7F) = (uint16_t)(rnd() & 1);
        old_run(f[w]);
        if (w)
            gsu_screen_toggle();
        else
            gsu_screen_flip();
        same_ram();
        same_screen();
    }

    printf("gsu_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
