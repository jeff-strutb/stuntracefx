/* old: race/camera.c gsu/run.c gsu/programs.c math/scale.c render/matrix.c render/arith.c render/angle.c math/fx_math.c system/misc.c
 *
 * camera_check.c: the port's race camera (race/camera.c) against the decomp's
 * CameraFollowStep, on random states: a camera record in work RAM, its
 * target an object of the table, the object and pak RAM random. The decomp's
 * Super FX view programs run through the bus model (model_gsu_start), as the
 * game's own calls do. Each case runs the decomp's routine on a copy and the
 * port's on the state, and compares both RAMs.
 *
 *   check/check.sh camera N
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
#include "glue.h"
#include "objects.h"
#include "camera.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(CameraFollowStep) OLD(FxMatrixFromAnglesProgram) OLD(FxMatrixRotateProgram)

static uint64_t rng = 0x2F8A6C1D04B39E57ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration;
static uint16_t cam, caller_y;

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

/* An object offset of the table: the records are $62 bytes from $0001. */
static uint16_t record(void)
{
    return (uint16_t)(1 + 0x62 * (rnd() % 0x135));
}

/* The decomp's Super FX programs the view calls, by the address the S-CPU
   starts them at. */
static void old_start(uint8_t bank, uint16_t start)
{
    static const struct {
        uint32_t at;
        void (*run)(void);
    } olds[] = {
        {0x018325, old_abi_FxMatrixFromAnglesProgram},
        {0x0182CE, old_abi_FxMatrixRotateProgram},
    };
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

/* Random RAM; the camera record and its target shaped the way the game
   keeps them. */
static void fresh(void)
{
    size_t k;
    uint16_t flags;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();

    /* the camera record in low work RAM, clear of the view and the camera's
       own words at $05FA, $1F2B, $1F33 */
    cam = (uint16_t)(0x1000 + rnd() % 0xD00);
    flags = (uint16_t)rnd();
    if (rnd() % 4)
        flags &= (uint16_t)~0x0002;     /* mostly the camera moves: bit 1 clear */
    W16(cam + 0x00) = flags;
    W16(cam + 0x02) = record();         /* the target: an object of the table */
    /* the Super FX record of each object: a record offset in pak RAM ($70:247A
       is the record in use; the records' words are read at $70:xxxx + it) */
    P16(0x247A) = (uint16_t)(rnd() % 0x4000);
    {
        int i;

        for (i = 0; i < 0x135; i++)
            W16(0x2022 + 1 + 0x62 * i) = (uint16_t)(rnd() % 0x4000);
    }
    caller_y = record();                /* the caller's Y: an object offset */

    memset(&old_cpu, 0, sizeof old_cpu);
    memset(&old_gsu, 0, sizeof old_gsu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = cam;
    old_cpu.y = caller_y;
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
    model_pixels_empty();
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    model_pixels_flush();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same(void)
{
    size_t i;

    cases++;
    /* $00FE-$00FF: the poll count the decomp's Super FX wait keeps there
       (gsu/run.c), machine detail the port does not have */
    for (i = 0; i < sizeof wram; i++)
        if (i != 0xFE && i != 0xFF && wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X); camera $%04X flags $%04X target $%04X, field %+ld",
                 i, wram[i], old_w[i], start_w[i], cam, (unsigned)W16(cam), (unsigned)W16(cam + 2),
                 (long)i - (long)cam);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X", i, pakram[i], old_p[i]);
            return;
        }
}

#define CASE(name) model_current = name; for (i = 0; i < n && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: camera_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_gsu_start = old_start;

    CASE("CameraFollowStep") {
        fresh();
        old_run(old_abi_CameraFollowStep);
        camera_follow_step(cam, caller_y);
        same();
    }
    printf("camera_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
