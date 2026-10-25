/* old: render/angle.c render/arith.c render/points.c render/fx_points.c render/table.c render/table_2702.c render/screen.c render/sort.c render/project.c render/matrix.c math/fx_math.c */
/*
 * render_check.c: the port's renderer geometry (render/fx_geom.c) against
 * the decomp's Super FX routines, on random states.
 *
 * The decomp's routine runs on the model's Super FX registers and the
 * game pak RAM (model.c), the port's function on the same RAM with its
 * arguments taken from those registers; the results carried in registers
 * are compared, and all of the game pak RAM but the Super FX's stack and
 * the clip routines' save words.
 *
 *   render_check ROM [N]
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define gsu old_gsu
#define gsu_rd8 old_gsu_rd8
#define gsu_wr8 old_gsu_wr8
#define gsu_plot old_gsu_plot
#define gsu_rpix old_gsu_rpix
#define gsu_code old_gsu_code
#define gsu_cache old_gsu_cache
#define gsu_cyc old_gsu_cyc
#define gsu_time old_gsu_time
#define gsu_buffer old_gsu_buffer
#include "gsu.h"
#undef gsu

#include "game.h"
#include "render.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(FxMatrixApply) OLD(FxMatrixApplyVectors) OLD(FxMatrixRotateProgram) OLD(FxMatrixRotateBackProgram)
OLD(FxMatrixFromAngles01) OLD(FxMatrixFromAnglesProgram) OLD(FxMatrixTranspose) OLD(FxAngleProgram)
OLD(FxLengthProgram) OLD(FxTableEntryWords) OLD(FxTable2702Program) OLD(FxScreenClear) OLD(FxProjectPoint)
OLD(FxProjectScaled) OLD(FxProjectVertex) OLD(FxSortObjectsProgram) OLD(FxPointsGather) OLD(FxModelPoints)
OLD(FxSegmentCrossZ) OLD(FxRatioClamp) OLD(FxDivideSigned)

static uint64_t rng = 0x8E1F3A5D2B7C9461ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static int16_t rand16(void)
{
    static const uint16_t e[] = {0, 1, 2, 0xFFFF, 0xFFFE, 0x7FFF, 0x8000, 0x8001, 0x0100, 0x00FF, 0x4000, 0xC000, 0x3FFF};

    return (int16_t)((rnd() & 3) ? (uint16_t)rnd() : e[rnd() % (sizeof e / sizeof e[0])]);
}

/* a value scaled down by a random number of bits, so small ones come up */
static int16_t small16(void)
{
    return (int16_t)(rand16() >> (rnd() % 16));
}

static uint8_t pool[1 << 20];
static uint8_t rom_copy[0x100000];

static void fresh(void)
{
    memcpy(pakram, pool + rnd() % (sizeof pool - sizeof pakram), sizeof pakram);
    memset(&old_gsu, 0, sizeof old_gsu);
    for (int i = 0; i < 16; i++)
        old_gsu.r[i] = (uint16_t)rnd();
    old_gsu.r[10] = 0x0240;                 /* the stack, where the programs keep it */
    old_gsu.rambr = 0;
}

/* the stack and the clip routines' save words: not compared */
static int scratch(size_t a)
{
    return (a >= 0x0240 && a < 0x0280) || (a >= 0x008A && a < 0x00A2);
}

static uint8_t start_p[sizeof pakram], old_p[sizeof pakram];
static long cases, failed;

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 25)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", model_current);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void old_run(void (*f)(void))
{
    memcpy(start_p, pakram, sizeof pakram);
    f();
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same_ram(void)
{
    size_t i;

    cases++;
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !scratch(i)) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
}

static void same(const char *what, uint16_t mine, uint16_t theirs)
{
    if (mine != theirs)
        fail("%s %04X, the decomp's %04X", what, mine, theirs);
}

/* a synthetic point script at $1F:F000 of the check's own ROM copy: points,
   mirrored points, jumps forward (plain and by the variant), then the end */
static uint16_t make_script(void)
{
    uint16_t at = 0xF000;
    uint8_t *p = rom_copy + 0x1F * 0x8000 + (at & 0x7FFF);
    int n = 0, parts = (int)(rnd() % 5), i, k;

    while (parts--) {
        int c = (int)(rnd() % 4);

        if (c < 2) {
            int count = (int)(rnd() % 6) + 1;

            p[n++] = c ? 0x38 : 0x04;
            p[n++] = (uint8_t)count;
            for (i = 0; i < count * 3; i++)
                p[n++] = (uint8_t)rnd();
        } else if (c == 2) {
            int skip = (int)(rnd() % 4);

            p[n++] = 0x20;
            p[n++] = (uint8_t)(skip + 1);
            p[n++] = 0;
            for (i = 0; i <= skip; i++)
                p[n++] = 0xEE;      /* skipped */
            n--;
        } else {
            p[n++] = 0x1C;
            p[n++] = 0xAA;
            for (k = 0; k < 4; k++) {   /* four jump words, each to just past the table */
                int left = (3 - k) * 2 + 1;

                p[n++] = (uint8_t)left;
                p[n++] = 0;
            }
            n--;
            p[n++] = 0;
        }
    }
    p[n++] = (uint8_t)(0x40 + rnd() % 0x30);    /* the end */
    return at;
}

#define CASE(name) model_current = name; for (i = 0; i < n; i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 20000, i;
    size_t k;

    if (argc < 2) {
        fprintf(stderr, "usage: render_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    memcpy(rom_copy, srfx_rom_image, sizeof rom_copy);
    srfx_rom_image = rom_copy;
    for (k = 0; k < sizeof pool; k++)
        pool[k] = (uint8_t)rnd();

    CASE("FxMatrixApply") {
        int16_t v[3], r[3];
        uint16_t out = (uint16_t)(0x3000 + (rnd() & 0x1FFE));

        fresh();
        for (k = 0; k < 3; k++)
            v[k] = (int16_t)(old_gsu.r[1 + k] = (uint16_t)rand16());
        old_gsu.r[9] = out;
        old_run(old_abi_FxMatrixApply);
        fx_matrix_apply(v, r);
        for (k = 0; k < 3; k++)
            FXW(out + 2 * k) = r[k];
        same_ram();
    }
    CASE("FxMatrixApplyVectors") {
        uint16_t out = (uint16_t)(0x3000 + (rnd() & 0x1FFE));

        fresh();
        old_gsu.r[9] = out;
        old_run(old_abi_FxMatrixApplyVectors);
        fx_matrix_apply_vectors(out);
        same_ram();
    }
    CASE("FxMatrixRotateProgram / FxMatrixRotateBackProgram") {
        int back = (int)(rnd() & 1);

        fresh();
        old_run(back ? old_abi_FxMatrixRotateBackProgram : old_abi_FxMatrixRotateProgram);
        back ? fx_matrix_rotate_back_program() : fx_matrix_rotate_program();
        same_ram();
    }
    CASE("FxMatrixFromAngles01 / FxMatrixFromAnglesProgram") {
        int prog = (int)(rnd() & 1);
        uint16_t out = prog ? 0x00E4 : (uint16_t)(0x3000 + (rnd() & 0x1FFE));

        fresh();
        old_gsu.r[9] = out;
        old_run(prog ? old_abi_FxMatrixFromAnglesProgram : old_abi_FxMatrixFromAngles01);
        fx_matrix_from_angles_at(out);
        same_ram();
    }
    CASE("FxMatrixTranspose") {
        uint16_t at = (uint16_t)(0x3000 + (rnd() & 0x1FFE));

        fresh();
        old_gsu.r[9] = at;
        old_run(old_abi_FxMatrixTranspose);
        fx_matrix_transpose_at(at);
        same_ram();
    }
    CASE("FxAngleProgram") {
        fresh();
        if (rnd() & 1)
            fx_angle_in_x = (rnd() & 1) ? fx_angle_in_y : (int16_t)-fx_angle_in_y;
        if (!(rnd() & 7))
            fx_angle_in_x = 0;
        old_run(old_abi_FxAngleProgram);
        fx_angle_program();
        same_ram();
    }
    CASE("FxLengthProgram") {
        fresh();
        old_run(old_abi_FxLengthProgram);
        fx_length_program();
        same_ram();
    }
    CASE("FxTableEntryWords") {
        fresh();
        old_run(old_abi_FxTableEntryWords);
        cases++;
        same("words", fx_table_entry_words(old_gsu.r[0]), old_gsu.r[5]);
    }
    CASE("FxTable2702Program") {
        fresh();
        old_run(old_abi_FxTable2702Program);
        fx_table_2702_program();
        same_ram();
    }
    CASE("FxScreenClear") {
        if (i >= 200)
            break;
        fresh();
        fx_screen_start &= 0xFFFE;
        old_run(old_abi_FxScreenClear);
        fx_screen_clear();
        same_ram();
    }
    CASE("FxProjectPoint") {
        int16_t x, y, z;
        uint16_t code;

        fresh();
        x = (int16_t)(old_gsu.r[1] = (uint16_t)small16());
        y = (int16_t)(old_gsu.r[2] = (uint16_t)small16());
        z = (int16_t)(old_gsu.r[3] = (uint16_t)small16());
        old_run(old_abi_FxProjectPoint);
        code = fx_project_point(&x, &y, &z);
        same_ram();
        same("x", (uint16_t)x, old_gsu.r[1]);
        same("y", (uint16_t)y, old_gsu.r[2]);
        same("depth", (uint16_t)z, old_gsu.r[3]);
        same("code", code, old_gsu.r[0]);
        same("outcode", code & 0xFF, old_gsu.r[5]);
    }
    CASE("FxProjectScaled") {
        int16_t x, y, z, size;

        fresh();
        x = (int16_t)(old_gsu.r[1] = (uint16_t)small16());
        y = (int16_t)(old_gsu.r[2] = (uint16_t)small16());
        z = (int16_t)(old_gsu.r[3] = (uint16_t)(1 + rnd() % 0x7FFF));
        size = (int16_t)(old_gsu.r[5] = (uint16_t)small16());
        old_run(old_abi_FxProjectScaled);
        fx_project_scaled(&x, &y, &z, &size);
        same_ram();
        same("x", (uint16_t)x, old_gsu.r[1]);
        same("y", (uint16_t)y, old_gsu.r[2]);
        same("depth", (uint16_t)z, old_gsu.r[3]);
        same("size", (uint16_t)size, old_gsu.r[5]);
    }
    CASE("FxProjectVertex") {
        int16_t x, y, z;
        uint16_t code;

        fresh();
        x = (int16_t)(old_gsu.r[1] = (uint16_t)small16());
        y = (int16_t)(old_gsu.r[2] = (uint16_t)small16());
        z = (int16_t)(old_gsu.r[6] = (uint16_t)(1 + rnd() % 0x7FFF));
        if (rnd() & 1)
            z = (int16_t)(old_gsu.r[6] = (uint16_t)(1 + rnd() % 0x200));
        old_gsu.rombr = 0x14;                   /* the caller's: the scale table's bank */
        old_run(old_abi_FxProjectVertex);
        code = fx_project_vertex(&x, &y, z);
        same_ram();
        same("x", (uint16_t)x, old_gsu.r[1]);
        same("y", (uint16_t)y, old_gsu.r[2]);
        same("code", code, old_gsu.r[0]);
    }
    CASE("FxSortObjectsProgram") {
        int slots = (int)(rnd() % 14), s;

        fresh();
        fx_slot_count = (uint8_t)slots;
        for (s = 0; s < slots; s++) {
            uint16_t at = (uint16_t)(FX_SLOTS + s * FX_SLOT_SIZE);

            FXU(at + 0x08) = (uint16_t)(0x8000 + (rnd() & 0x7FE0));
            FXW(at + 0x10) = small16();
            FXW(at + 0x12) = small16();
            FXW(at + 0x14) = small16();
            FXW(at + 0x02) = small16();
            if (rnd() & 1) {                     /* ties: the same key from the same parts */
                FXU(at + 0x08) = 0x8000;
                pakram[at + 0x07] = 0x80;
                FXW(at + 0x14) = (int16_t)(rnd() % 3);
                FXW(at + 0x02) = (int16_t)(rnd() % 3);
            }
        }
        old_run(old_abi_FxSortObjectsProgram);
        fx_sort_objects_program();
        same_ram();
    }
    CASE("FxPointsGather") {
        uint16_t count = (uint16_t)((rnd() & 7) ? 1 + rnd() % 24 : 250 + rnd() % 30), script = (uint16_t)(0x8000 + (rnd() & 0x7FC0)), end;
        uint8_t bank = (uint8_t)(rnd() % 0x20);

        fresh();
        fx_pairs_end = (uint16_t)(0x4000 + (rnd() & 0x0FFE));
        old_gsu.r[9] = count;
        old_gsu.r[14] = script;
        old_gsu.rombr = bank;
        old_gsu.romdr = rom_at((uint32_t)bank << 16 | script)[0];
        old_run(old_abi_FxPointsGather);
        end = fx_points_gather(count, bank, script);
        same_ram();
        same("script", end, old_gsu.r[14]);
    }
    CASE("FxModelPoints") {
        uint16_t script = make_script(), end, last;

        fresh();
        fx_model_variant = (uint16_t)(rnd() & 3);
        old_gsu.r[9] = script;
        old_gsu.rombr = 0x1F;
        old_run(old_abi_FxModelPoints);
        last = fx_model_points(0x1F, script, &end);
        same_ram();
        same("list end", last, old_gsu.r[9]);
        same("script end", end, old_gsu.r[14]);
    }
    CASE("FxRatioClamp") {
        int16_t x, y;
        uint16_t d = (uint16_t)(1 + rnd() % 0x400);

        fresh();
        /* around the $3FFF limit: x / 2 * 256 / d near $3FFF */
        x = (int16_t)(old_gsu.r[1] = (uint16_t)((rnd() & 1) ? small16()
                                                : (int16_t)(((uint32_t)0x3FFF * d / 256 + rnd() % 3 - 1) * 2 + (rnd() & 1))));
        y = (int16_t)(old_gsu.r[2] = (uint16_t)small16());
        old_gsu.r[3] = d;
        old_run(old_abi_FxRatioClamp);
        fx_ratio_clamp(&x, &y, d);
        cases++;
        same("x", (uint16_t)x, old_gsu.r[1]);
        same("y", (uint16_t)y, old_gsu.r[2]);
    }
    CASE("FxDivideSigned") {
        uint32_t num;
        uint16_t d;
        FxDiv q;

        fresh();
        old_gsu.r[5] = (uint16_t)small16();
        old_gsu.r[6] = d = (uint16_t)small16();
        num = (uint32_t)old_gsu.r[5] << 16 | old_gsu.r[4];
        old_run(old_abi_FxDivideSigned);
        q = fx_divide_signed(num, d);
        cases++;
        same("quotient", q.quot, old_gsu.r[4]);
        same("remainder", q.rem, old_gsu.r[0]);
    }
    CASE("FxSegmentCrossZ") {
        int16_t a[3], b[3];
        int r;

        fresh();
        for (k = 0; k < 3; k++) {
            a[k] = (int16_t)(old_gsu.r[1 + k] = (uint16_t)small16());
            b[k] = (int16_t)(old_gsu.r[4 + k] = (uint16_t)small16());
        }
        old_run(old_abi_FxSegmentCrossZ);
        r = fx_segment_cross_z(a, b);
        same_ram();
        same("result", (uint16_t)r, old_gsu.r[0]);
        if (r == 1)
            for (k = 0; k < 3; k++) {
                same("first end", (uint16_t)a[k], old_gsu.r[1 + k]);
                same("second end", (uint16_t)b[k], old_gsu.r[4 + k]);
            }
    }

    printf("render_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
