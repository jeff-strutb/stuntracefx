/* old: gsu/run.c gsu/params.c gsu/screen.c gsu/programs.c render/unused.c render/matrix.c render/arith.c render/angle.c math/fx_math.c gsu/fx_init.c data/unpack.c render/sort.c render/hud.c render/points.c render/fx_points.c render/project.c render/table.c hud/lap_time.c audio/sound_queue.c system/misc.c
 *
 * gsu_check.c: the port's S-CPU side of the Super FX (gsu/glue.c) against
 * the decomp's, on random states. The decomp's S-CPU code starts the
 * decomp's Super FX programs through the bus model (model_gsu_start); the
 * work RAM, the game pak RAM and the Super FX's screen are compared, but
 * the poll counts the original keeps while it waits ($FE-$FF, $1F21-$1F22).
 *
 *   check/check.sh gsu N
 */
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
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
OLD(FxUnpackProgram) OLD(FxLengthProgram) OLD(FxMatrixRotateProgram) OLD(FxAngleProgram) OLD(FxSortObjectsProgram)
OLD(FxNoiseFill) OLD(FxGlyph8x16) OLD(FxGlyph24x16) OLD(FxPictureRise40) OLD(FxPictureRise60) OLD(FxPictureRiseSplit40)
OLD(FxPictureRiseSplit60) OLD(FxGlyph16RowsFromTop) OLD(FxScaledSprite13) OLD(FxScaledSpriteEighth)
OLD(GsuCF42ToC000) OLD(GsuCF42To4000) OLD(GsuCF42To3000) OLD(ObjectGsu80BC) OLD(Gsu82CE) OLD(Gsu81E3) OLD(Gsu81E3Long)
OLD(Gsu8325) OLD(ViewMatrixUpdateAndSave) OLD(ViewReset) OLD(ViewUpdateAndGsuAEF8) OLD(GsuEE88Ramp) OLD(GsuF329Entry)
OLD(GsuF329Sequence) OLD(GsuF441Player1) OLD(GsuF441Player2) OLD(GsuF0A5OrF0B4) OLD(GsuEEF5OrEFCD) OLD(GsuDigitPair0E74)
OLD(GsuF15BThree) OLD(GsuF15BFive) OLD(GsuFA1BFromTables) OLD(GsuFA5AThree)
OLD(FxGlyph16RowsFromBottom) OLD(FxSprite24x40) OLD(FxScaledSprite12)
OLD(GsuFA5AFromTables) OLD(GsuFA5ASix) OLD(GsuFA5ASixLeft) OLD(GsuFA5AThreeShifted) OLD(GsuFA35FromTables) OLD(GsuF15BTwoLines)
OLD(GlyphRowsAt32) OLD(GlyphRowsAt56) OLD(HudSixGlyphs) OLD(GsuPiecesStep10CB) OLD(BigSpritesCountDraw) OLD(BigSpritesRow6)
OLD(BigSpritesRow5) OLD(GlyphRowY60From0) OLD(GlyphRowY60From1) OLD(GlyphRowY60From2) OLD(GlyphRowY60From3)
OLD(GlyphRowY60From4) OLD(GlyphRowY60From5) OLD(GlyphListDraw) OLD(GlyphSpriteDraw) OLD(GlyphSpritesInit)
OLD(GlyphListMessageAndTime) OLD(HudHoldDraw) OLD(ObjectGsu80BCThen81E3) OLD(ObjectGsu81E3)
OLD(FxGlyph8x8) OLD(FxGlyph24x32Bank04) OLD(FxGlyph16x32) OLD(FxSprite16x32) OLD(FxGlyph16x16)
OLD(GlyphWordAt1C28) OLD(GlyphGrid1A) OLD(GlyphGrid1D) OLD(GlyphSlideThree) OLD(GlyphSpritesDrawAll) OLD(HudTimesPackedDraw)
OLD(HudLapDigitsDraw) OLD(HudLeadWordDraw) OLD(HudTimesDraw) OLD(HudBlankDraw) OLD(GlyphSpritesSlideOut) OLD(GlyphSpritesSlideIn)
OLD(GsuF15BFourA) OLD(GsuF15BFourB) OLD(BigSpritesNine) OLD(BigSpritesRow6B) OLD(ViewDefaults)

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
    {0x01CF42, old_abi_FxUnpackProgram},
    {0x0180BC, old_abi_FxLengthProgram},
    {0x0182CE, old_abi_FxMatrixRotateProgram},
    {0x0181E3, old_abi_FxAngleProgram},
    {0x01AEF8, old_abi_FxSortObjectsProgram},
    {0x07EE88, old_abi_FxNoiseFill},
    {0x07F329, old_abi_FxGlyph8x16},
    {0x07F441, old_abi_FxGlyph24x16},
    {0x07F0A5, old_abi_FxPictureRise40},
    {0x07F0B4, old_abi_FxPictureRise60},
    {0x07EEF5, old_abi_FxPictureRiseSplit40},
    {0x07EFCD, old_abi_FxPictureRiseSplit60},
    {0x07F15B, old_abi_FxGlyph16RowsFromTop},
    {0x07FA1B, old_abi_FxScaledSprite13},
    {0x07FA5A, old_abi_FxScaledSpriteEighth},
    {0x07F121, old_abi_FxGlyph16RowsFromBottom},
    {0x07FA88, old_abi_FxSprite24x40},
    {0x07FA35, old_abi_FxScaledSprite12},
    {0x07F280, old_abi_FxGlyph8x8},
    {0x07F3D2, old_abi_FxGlyph24x32Bank04},
    {0x07F6DF, old_abi_FxGlyph16x32},
    {0x07FAB0, old_abi_FxSprite16x32},
    {0x07F5C9, old_abi_FxGlyph16x16},
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
    /* the game's screens: the race's at $2C00 (4 or 8 bits a pixel), the
       menus' at $9400 (4 bits) */
    old_gsu.scbr = fx_screen.base = (uint8_t)(rnd() & 1 ? 0x0B : 0x25);
    W8(0x0374) = fx_screen.base == 0x25 || (rnd() & 1) ? 0x01 : 0x03;
    fx_screen.mode = 0, fx_screen.height = 0;
    {
        static const uint8_t zero[8];

        fx_pixels_set(0, 0xFFFF, 0, zero);
        fx_pixels_set(1, 0xFFFF, 0, zero);
    }
}

static sigjmp_buf escape;
static long skipped;

static void runaway(int sig)
{
    (void)sig;
    siglongjmp(escape, 1);
}

/* a case either side runs away in (a loop of 65,536 on a random input)
   skipped */
#define GUARD if (sigsetjmp(escape, 1)) { skipped++; alarm(0); continue; } alarm(5)

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

static int scratch;                 /* the case's routines count in direct page $F1-$F4 */

static int counted(size_t i)
{
    return i == 0xFE || i == 0xFF || i == 0x1F21 || i == 0x1F22 || (scratch && i >= 0xF1 && i <= 0xF4)
           || (scratch == 2 && i == 0x64);
}

static void same_ram(void)
{
    size_t i;

    alarm(0);
    fx_pixels_flush();
    cases++;
    for (i = 0; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && !counted(i)) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0)) {   /* the programs' R10 stack */
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
    signal(SIGALRM, runaway);

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

    CASE("GsuCF42ToC000 / To4000 / To3000") {
        static const uint16_t dest[3] = {0xC000, 0x4000, 0x3000};
        void (*const f[3])(void) = {old_abi_GsuCF42ToC000, old_abi_GsuCF42To4000, old_abi_GsuCF42To3000};
        int w = (int)(rnd() % 3);
        static const uint16_t streams[][2] = {   /* the game's own (from its recorded unpacks) */
            {0x8C0C, 0x17}, {0x9068, 0x1D}, {0x9500, 0x1D}, {0x9664, 0x1D}, {0x97D4, 0x1D}, {0x989C, 0x1C},
            {0x993C, 0x1D}, {0x99A8, 0x17}, {0xB6D4, 0x1D}, {0xBABC, 0x1C}, {0xC3F4, 0x1D}, {0xC770, 0x18},
            {0xC9DC, 0x1C}, {0xD970, 0x1D}, {0xDC38, 0x1C}, {0xE1ED, 0x1F}, {0xE7CC, 0x18}, {0xEF88, 0x18},
            {0xF6ED, 0x1F}, {0xF884, 0x1D}, {0xFA39, 0x1F}, {0xFB99, 0x1F}, {0xFFB8, 0x18}};
        int k = (int)(rnd() % (sizeof streams / sizeof streams[0]));
        uint16_t a = streams[k][0], x = (uint16_t)(streams[k][1] | (rnd() & 0xFF00));

        fresh();
        GUARD;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_cpu.a = a, old_cpu.x = x;
        old_run(f[w]);
        gsu_unpack_to(a, x, dest[w]);
        same_ram();
    }
    CASE("ObjectGsu80BC") {
        uint16_t obj = (uint16_t)(rnd() % 0x80 * 2), v;

        fresh();
        GUARD;
        old_cpu.db = 0x7E;                  /* the objects are in work RAM's bank 7E */
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_cpu.x = obj;
        old_run(old_abi_ObjectGsu80BC);
        v = gsu_object_length(obj);
        same_ram();
        if (v != old_cpu.a)
            fail("A %04X, the decomp's %04X", v, old_cpu.a);
    }
    CASE("Gsu82CE / Gsu81E3 / Gsu8325 / ViewMatrixUpdateAndSave / ViewReset / ViewUpdateAndGsuAEF8") {
        int w = (int)(rnd() % 7);
        void (*const f[7])(void) = {old_abi_Gsu82CE, old_abi_Gsu81E3, old_abi_Gsu81E3Long, old_abi_Gsu8325,
                                    old_abi_ViewMatrixUpdateAndSave, old_abi_ViewReset, old_abi_ViewUpdateAndGsuAEF8};
        uint16_t v = 0;

        fresh();
        GUARD;
        {                                   /* the objects to sort: shapes in ROM, as the game's */
            int k;

            fx_slot_count = (uint8_t)(rnd() % 14);
            for (k = 0; k < 14; k++)
                FXU(FX_SLOTS + FX_SLOT_SIZE * k + 0x08) |= 0x8000;
        }
        old_run(f[w]);
        switch (w) {
        case 0: gsu_vector_turn(); break;
        case 1: case 2: v = gsu_vector_angle(); break;
        case 3: gsu_view_matrix(); break;
        case 4: view_matrix_update_and_save(); break;
        case 5: view_reset(); break;
        case 6: view_update_and_sort(); break;
        }
        same_ram();
        if ((w == 1 || w == 2) && v != old_cpu.a)
            fail("A %04X, the decomp's %04X", v, old_cpu.a);
    }
    CASE("GsuEE88Ramp") {
        fresh();
        GUARD;
        old_run(old_abi_GsuEE88Ramp);
        gsu_noise_ramp();
        same_ram();
    }
    CASE("HUD glyphs and pictures") {
        int w = (int)(rnd() % 12);
        uint16_t a = (uint16_t)rnd(), y = (uint16_t)(0x0300 + rnd() % 0x1C00);   /* digits in work RAM, past the direct page */
        void (*const f[12])(void) = {old_abi_GsuF329Entry, old_abi_GsuF329Sequence, old_abi_GsuF441Player1,
                                     old_abi_GsuF441Player2, old_abi_GsuF0A5OrF0B4, old_abi_GsuEEF5OrEFCD,
                                     old_abi_GsuDigitPair0E74, old_abi_GsuF15BThree, old_abi_GsuF15BFive,
                                     old_abi_GsuFA1BFromTables, old_abi_GsuFA5AThree, old_abi_GsuF329Entry};

        fresh();
        GUARD;
        old_cpu.db = 0x07;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        if (w == 4 || w == 5) {
            old_cpu.p |= P_X;
            a = (uint16_t)(rnd() % 0xC0);
        }
        if (rnd() & 1)
            W8(0x0D62) = 0;
        W16(0x0D6B) = (uint16_t)(rnd() % 6 * 2);
        W16(0x0D6D) = (uint16_t)(rnd() % 0xE0);
        W16(0x0D6F) = (uint16_t)(rnd() % 0xA0);
        old_cpu.a = a, old_cpu.y = y;
        old_run(f[w]);
        switch (w) {
        case 0: case 11: gsu_glyph_f329(a); break;
        case 1: gsu_glyph_f329_sequence(y); break;
        case 2: gsu_player_picture(0); break;
        case 3: gsu_player_picture(1); break;
        case 4: gsu_picture_rise(a, 0); break;
        case 5: gsu_picture_rise(a, 1); break;
        case 6: gsu_digit_pair_0e74(); break;
        case 7: gsu_glyphs_three(); break;
        case 8: gsu_glyphs_five(); break;
        case 9: gsu_sprite_fa1b_from_tables(); break;
        case 10: gsu_sprites_fa5a_three(); break;
        }
        same_ram();
    }

    CASE("HUD sprites, glyph rows and lists") {
        int w = (int)(rnd() % 26);
        uint16_t a = (uint16_t)(rnd() % 0x40), k = (uint16_t)(rnd() % 16);
        void (*const f[26])(void) = {old_abi_GsuFA5AFromTables, old_abi_GsuFA5ASix, old_abi_GsuFA5ASixLeft,
            old_abi_GsuFA5AThreeShifted, old_abi_GsuFA35FromTables, old_abi_GsuF15BTwoLines, old_abi_GlyphRowsAt32,
            old_abi_GlyphRowsAt56, old_abi_HudSixGlyphs, old_abi_GsuPiecesStep10CB, old_abi_BigSpritesCountDraw,
            old_abi_BigSpritesRow6, old_abi_BigSpritesRow5, old_abi_GlyphRowY60From0, old_abi_GlyphRowY60From1,
            old_abi_GlyphRowY60From2, old_abi_GlyphRowY60From3, old_abi_GlyphRowY60From4, old_abi_GlyphRowY60From5,
            old_abi_GlyphListDraw, old_abi_GlyphSpriteDraw, old_abi_GlyphSpritesInit, old_abi_GlyphListMessageAndTime,
            old_abi_HudHoldDraw, old_abi_GsuFA5AFromTables, old_abi_GsuFA5ASix};
        int e;

        fresh();
        GUARD;
        scratch = 1;
        old_cpu.db = (w >= 13 && w <= 19) || w == 21 || w == 22 ? 0x04 : 0x07;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        W16(0x0D6B) = (uint16_t)(rnd() % 6 * 2);
        W16(0x0D6D) = (uint16_t)(rnd() % 0xE0);
        W16(0x0D6F) = (uint16_t)(rnd() % 0xA0);
        if (rnd() & 1)
            W8(0x0E5C) = 0;
        if (rnd() & 1)
            W16(0x0E5D) = 0;
        W8(0x1160) = (uint8_t)((rnd() & 3) ? rnd() % 13 : rnd());
        if (rnd() & 1)
            W16(0x10CB) = (uint16_t)(rnd() % 0x18);
        W16(0x10E4) = (uint16_t)(rnd() % 12);
        W16(0x10E6) = (uint16_t)(rnd() % 20);
        for (e = 0; e < 40; e += 2)
            if (rnd() & 1)
                W16(0x110E + e) = 0;
        old_cpu.a = a;
        old_cpu.x = w == 20 ? (uint16_t)(2 * k) : (uint16_t)(2 * (rnd() % 10));
        k = old_cpu.x;
        old_run(f[w]);
        switch (w) {
        case 0: case 24: gsu_sprites_fa5a_from_tables(); break;
        case 1: case 25: gsu_sprites_fa5a_six(); break;
        case 2: gsu_sprites_fa5a_six_left(); break;
        case 3: gsu_sprites_fa5a_three_shifted(); break;
        case 4: gsu_sprite_fa35_from_tables(); break;
        case 5: gsu_glyphs_two_lines(a); break;
        case 6: gsu_glyph_rows_at_32(a); break;
        case 7: gsu_glyph_rows_at_56(a); break;
        case 8: gsu_glyphs_six(); break;
        case 9: gsu_pieces_step_10cb(); break;
        case 10: big_sprites_count_draw(); break;
        case 11: big_sprites_row6(); break;
        case 12: big_sprites_row5(); break;
        case 13: case 14: case 15: case 16: case 17: case 18: glyph_row_y60(w - 13); break;
        case 19: glyph_list_draw(); break;
        case 20: glyph_sprite_draw((uint16_t)(k / 2)); break;
        case 21: glyph_sprites_init(); break;
        case 22: glyph_list_message_and_time((uint16_t)(k / 2)); break;
        case 23: hud_hold_draw(); break;
        }
        same_ram();
        scratch = 0;
    }
    CASE("ObjectGsu80BCThen81E3 / ObjectGsu81E3") {
        int w = (int)(rnd() & 1);
        uint16_t obj = (uint16_t)(rnd() % 0x80 * 2), v;

        fresh();
        GUARD;
        old_cpu.db = 0x7E;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_cpu.x = obj;
        old_run(w ? old_abi_ObjectGsu81E3 : old_abi_ObjectGsu80BCThen81E3);
        v = w ? object_angle(obj) : object_length_and_angle(obj);
        same_ram();
        if (v != old_cpu.a)
            fail("A %04X, the decomp's %04X", v, old_cpu.a);
    }

    CASE("HUD words, grids, times and slides") {
        int w = (int)(rnd() % 17), e;
        void (*const f[17])(void) = {old_abi_GlyphWordAt1C28, old_abi_GlyphGrid1A, old_abi_GlyphGrid1D,
            old_abi_GlyphSlideThree, old_abi_GlyphSpritesDrawAll, old_abi_HudTimesPackedDraw, old_abi_HudLapDigitsDraw,
            old_abi_HudLeadWordDraw, old_abi_HudTimesDraw, old_abi_HudBlankDraw, old_abi_GlyphSpritesSlideOut,
            old_abi_GlyphSpritesSlideIn, old_abi_GsuF15BFourA, old_abi_GsuF15BFourB, old_abi_BigSpritesNine,
            old_abi_BigSpritesRow6B, old_abi_HudTimesDraw};

        fresh();
        GUARD;
        scratch = 1;
        old_cpu.db = 0x07;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        for (e = 0; e < 48; e += 2) {           /* the glyph sprites on the screen */
            W16(0x0FB0 + e) = (uint16_t)(rnd() % 0xE0);
            W16(0x0FE0 + e) = (uint16_t)(rnd() % 0xA0);
        }
        for (e = 0; e < 6; e += 2) {
            W16(0x1040 + e) = (uint16_t)(rnd() % 0x20);
            W16(0x1070 + e) = (uint16_t)(rnd() % 0x40);
            if (rnd() & 1)
                W16(0x1010 + e) = (uint16_t)(0x8000 + 6 * (rnd() % 3));
        }
        if (w == 3 && (rnd() & 1))             /* a slide landing on its target */
            for (e = 0; e < 6; e += 2)
                W16(0x1070 + e) = (uint16_t)(W16(0x0FB0 + e) - W16(0x1040 + e));
        if (w == 11 && (rnd() & 1)) {           /* both words arriving */
            W16(0x0FB0) = (uint16_t)(0x0034 + rnd() % 0x17);
            W16(0x0FB2) = (uint16_t)(0x0024 - rnd() % 0x17);
            W8(0x0E55) = 0x10;
            current_player = (uint8_t)(rnd() & 1);
            sound_muted = 0;
            sound_port0_count = (uint8_t)(rnd() % 8);
            sound_port0_head = (uint8_t)(rnd() & 0x0F);
        } else if (w == 10 || w == 11) {        /* the two words about their ends */
            W16(0x0FB0) = (uint16_t)((rnd() & 1) ? 0xFF98 + rnd() % 0x30 : 0x0034 + rnd() % 0x30);
            W16(0x0FB2) = (uint16_t)((rnd() & 1) ? 0x00D0 - rnd() % 0x30 : 0x0024 - rnd() % 0x30);
            W8(0x0E55) = (uint8_t)((rnd() & 1) ? 0x10 : rnd() % 3);
        }
        if (rnd() & 1) W16(0x0FAE) = 0;
        if (rnd() & 1) W8(0x10CD) = 0;
        if (rnd() & 1) W16(0x0E5D) = 0x0001;
        if (rnd() & 1) W16(0x0D2B) = 3;
        if (rnd() & 1) W16(0x0D4C) = 0;
        if (rnd() & 3) W16(0x0F2B) = (rnd() & 1) ? 0xFFFF : (uint16_t)rnd();
        if (rnd() & 1) W8(0x0E57) = 0;
        W8(0x0FA3) = (uint8_t)((rnd() % 3) - 1);
        if (rnd() & 1) W16(0x1163) = 0xFFFF;
        if (rnd() & 1) W16(0x1161) = 0xFFFF;
        for (e = 0; e < 10; e++)
            W8(0x0F64 + e) = (uint8_t)((rnd() & 3) ? rnd() % 10 : rnd());
        if (rnd() & 1) W8(0x0E5C) = 0;
        if (rnd() & 1) W16(0x1F71) = (uint16_t)(rnd() % 4);
        old_run(f[w]);
        switch (w) {
        case 0: glyph_word_at_1c28(); break;
        case 1: glyph_grid_1a(); break;
        case 2: glyph_grid_1d(); break;
        case 3: glyph_slide_three(); break;
        case 4: glyph_sprites_draw_all(); break;
        case 5: hud_times_packed_draw(); break;
        case 6: hud_lap_digits_draw(); break;
        case 7: hud_lead_word_draw(); break;
        case 8: case 16: hud_times_draw(); break;
        case 9: hud_blank_draw(); break;
        case 10: glyph_sprites_slide_out(); break;
        case 11: glyph_sprites_slide_in(); break;
        case 12: gsu_glyphs_four(0); break;
        case 13: gsu_glyphs_four(1); break;
        case 14: big_sprites_nine(); break;
        case 15: big_sprites_row6b(); break;
        }
        same_ram();
        scratch = 0;
    }

    CASE("ViewDefaults") {
        fresh();
        GUARD;
        scratch = 2;                            /* ReadTable9022's count in direct page $64 */
        old_cpu.db = 0x03;
        old_run(old_abi_ViewDefaults);
        view_defaults();
        same_ram();
        scratch = 0;
    }

    printf("gsu_check: %ld cases, %ld differ, %ld skipped (ran away)\n", cases, failed, skipped);
    return failed != 0;
}
