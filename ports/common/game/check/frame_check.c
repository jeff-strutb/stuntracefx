/* old: render/model.c render/raster.c render/polygon.c render/points.c render/fx_points.c render/project.c render/matrix.c render/arith.c render/angle.c math/fx_math.c render/frame.c render/effects.c render/text.c render/bitmap.c render/screen.c object/fx_record.c
 *
 * frame_check.c: the port's frame drawing (render/fx_frame.c, fx_text.c)
 * against the decomp's Super FX routines on random states: the weather,
 * the particles and their bursts, the object sprites, the screen effects
 * and the text, the paths the game's own calls seldom reach included.
 * Each runs on the same game pak RAM and plot state; the RAM is compared
 * (the Super FX's stack aside) once both sides' pixel caches are written
 * out.
 *
 *   check/check.sh frame N
 */
#include <signal.h>
#include <unistd.h>
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
OLD(FxWeatherDraw) OLD(FxParticlesDraw) OLD(FxEffectsAdd) OLD(FxObjectSprite) OLD(FxEffectsRun) OLD(FxTextDraw)
OLD(FxTextNotice)

static uint64_t rng = 0x5D2B7C94618E1F3Aull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static int16_t within(int lo, int hi)
{
    return (int16_t)(lo + (int)(rnd() % (unsigned)(hi - lo + 1)));
}

static uint8_t rom_copy[0x100000];
static uint8_t start_p[sizeof pakram], old_p[sizeof pakram];
static long cases, failed, iteration;
static volatile const char *side = "";

static void runaway(int sig)
{
    (void)sig;
    fprintf(stderr, "frame_check: %s, case %ld: %s ran away\n", model_current, iteration, side);
    _exit(3);
}

/* random RAM and registers, the game's screen, the view's edges */
static void fresh(void)
{
    static const uint8_t bases[2] = {0x0B, 0x25};
    static const uint8_t zero[8];
    size_t i;

    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    memset(&old_gsu, 0, sizeof old_gsu);
    for (i = 0; i < 16; i++)
        old_gsu.r[i] = (uint16_t)rnd();
    old_gsu.r[10] = 0x0240;
    old_gsu.r[11] = 0xFFF0;
    old_gsu.scmr = (rnd() & 1) ? 1 : 3;
    old_gsu.scbr = bases[rnd() & 1];
    old_gsu.colr = (uint8_t)rnd();
    old_gsu.por = (uint8_t)(rnd() & 0x0F);
    fx_screen.mode = old_gsu.scmr & 3;
    fx_screen.height = 0;
    fx_screen.base = old_gsu.scbr;
    fx_colour = old_gsu.colr;
    fx_plot_options = old_gsu.por;
    fx_pixels_set(0, 0xFFFF, 0, zero);
    fx_pixels_set(1, 0xFFFF, 0, zero);
    FXU(0x001C) = (uint16_t)(rnd() % 0x20);
    if (rnd() & 3) {
        fx_screen_centre_x = 108, fx_screen_centre_y = 64;
        fx_edges[0] = 0, fx_edges[1] = 216, fx_edges[2] = 0, fx_edges[3] = 128;
    } else {
        fx_screen_centre_x = within(40, 160), fx_screen_centre_y = within(30, 100);
        fx_edges[0] = within(0, 50), fx_edges[1] = within(150, 216);
        fx_edges[2] = within(0, 50), fx_edges[3] = within(80, 128);
    }
}

static void fail(const char *fmt, ...)
{
    va_list ap;

    if (++failed > 25)
        return;
    va_start(ap, fmt);
    fprintf(stderr, "%s, case %ld: ", model_current, iteration);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* the decomp's routine, its pixels written out; then the RAM put back for
   the port's */
static void old_run(void (*f)(void))
{
    memcpy(start_p, pakram, sizeof pakram);
    model_pixels_empty();
    side = "the decomp's routine";
    alarm(5);
    f();
    model_pixels_flush();
    side = "the port's";
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(pakram, start_p, sizeof pakram);
}

static void same_ram(void)
{
    size_t i;

    fx_pixels_flush();
    alarm(0);
    cases++;
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0)) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            if (getenv("ALLDIFF"))
                for (i++; i < sizeof pakram; i++)
                    if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0))
                        fprintf(stderr, "  $%04zX = %02X, the decomp's %02X (was %02X)\n", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
}

/* the object the view follows, with its speed */
static void followed(void)
{
    FXU(0x247A) = 0x3000;
    FXW(0x304A) = (rnd() & 3) ? within(-0x40, 0x600) : (int16_t)rnd();
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 20000, i;
    int k;

    if (argc < 2) {
        fprintf(stderr, "usage: frame_check ROM [N]\n");
        return 2;
    }
    signal(SIGALRM, runaway);
    model_load_rom(argv[1]);
    memcpy(rom_copy, srfx_rom_image, sizeof rom_copy);
    srfx_rom_image = rom_copy;

    CASE("FxWeatherDraw") {
        uint16_t kind = (uint16_t)(rnd() % 2 + 1), drops = (rnd() & 1) ? 0x50 : (uint16_t)(rnd() % 81);

        fresh();
        followed();
        FXU(0x2B94) = (rnd() % 8) ? kind : (uint16_t)(rnd() % 4);
        FXU(0x00E0) = (uint16_t)rnd();
        FXU(0x2B8E) = (rnd() & 1) ? FXU(0x00E0) + within(-0x400, 0x400) : (uint16_t)rnd();
        FXW(0x2B90) = within(-0x40, 0x40);
        for (k = 0; k < 80; k++) {
            uint16_t p = (uint16_t)(0x2864 + 10 * k);

            pakram[p] = (uint8_t)((rnd() & 3) == 0 ? 0 : (rnd() & 1) ? within(-63, -1) : within(1, 0x64));
            if (getenv("WMODE"))
                pakram[p] = (uint8_t)(atoi(getenv("WMODE")) == 0 ? -5 : atoi(getenv("WMODE")) == 1 ? 0 : within(1, 0x64));
            FXW(p + 2) = within(-0x1FF, 0x1FF);
            FXW(p + 4) = within(-0xFF, 0xFF);
            FXW(p + 6) = (rnd() & 7) ? within(0, 0x1FE) : within(-0x40, 0x240);
            /* where it was drawn last: on the screen, or 0 */
            FXU(p + 8) = (rnd() & 1) ? 0 : (uint16_t)(within(0, fx_edges[1] - 1) | within(0, fx_edges[3] - 1) << 8);
        }
        old_gsu.r[0] = kind;
        old_gsu.r[1] = drops;
        model_trace = getenv("TRACE") && atol(getenv("TRACE")) == i;
        old_run(old_abi_FxWeatherDraw);
        model_trace = 0;
        fx_weather_draw(kind, drops);
        same_ram();
    }

    CASE("FxParticlesDraw") {
        uint16_t key = (uint16_t)rnd();

        fresh();
        fx_vector_out[0] = within(-0x300, 0x300);
        fx_vector_out[1] = within(-0x300, 0x300);
        fx_vector_out[2] = (rnd() & 3) ? within(0x80, 0x3000) : (int16_t)rnd();
        for (k = 0; k < 20; k++) {
            uint16_t p = (uint16_t)(0x273E + 14 * k);

            if (rnd() & 1)
                FXU(p + 12) = key;
            pakram[p + 1] = (uint8_t)rnd();
            FXW(p + 6) = within(-0x200, 0x200);
            FXW(p + 8) = within(-0x200, 0x200);
            FXW(p + 10) = within(-0x400, 0x400);
        }
        old_gsu.r[2] = key;
        old_run(old_abi_FxParticlesDraw);
        fx_particles_draw(key);
        same_ram();
    }

    CASE("FxEffectsAdd") {
        uint16_t kind = (uint16_t)(rnd() % 9), life = (uint16_t)rnd(), key = (uint16_t)rnd(), count = (uint16_t)(rnd() % 26);

        fresh();
        for (k = 0; k < 20; k++)
            if (rnd() & 1)
                pakram[0x273E + 14 * k] = 0;
        old_gsu.r[0] = kind, old_gsu.r[1] = life, old_gsu.r[2] = key, old_gsu.r[3] = count;
        old_run(old_abi_FxEffectsAdd);
        fx_effects_add(kind, life, key, count);
        same_ram();
    }

    CASE("FxObjectSprite") {
        fresh();
        fx_vector_out[0] = within(-0x600, 0x600);
        fx_vector_out[1] = within(-0x400, 0x400);
        fx_vector_out[2] = (rnd() & 7) ? within(0x40, 0x3000) : (int16_t)rnd();
        FXW(0x00CE) = within(-0x80, 0x7F);
        FXU(0x0032) = (uint16_t)(rnd() % 5);
        FXU(0x00AA) = (uint16_t)(rnd() % 0x40);
        FXU(0x00D6) = (uint16_t)(rnd() & 0xFF);
        FXU(0x004A) = (rnd() & 1) ? 0x8D30 : 0x8F91;
        FXU(0x0054) = (uint16_t)(rnd() & 0x3000);
        FXU(0x00A6) = (uint16_t)(rnd() % 0x20);
        FXU(0x00A8) = (uint16_t)(0x8000 | rnd());
        FXU(0x00D4) = (uint16_t)((rnd() & 0x8000) | (rnd() & 1 ? 0x3F : 0x7F));
        if (getenv("SHOW") && atol(getenv("SHOW")) == i)
            fprintf(stderr, "x %d y %d z %d CE %d shift %u AA %u D4 %04X\n", fx_vector_out[0], fx_vector_out[1], fx_vector_out[2],
                    FXW(0x00CE), FXU(0x0032), FXU(0x00AA), FXU(0x00D4));
        old_run(old_abi_FxObjectSprite);
        fx_object_sprite();
        same_ram();
    }

    CASE("FxEffectsRun") {
        fresh();
        followed();
        FXU(0x2736) = (uint16_t)(rnd() | 0x4000) & ((rnd() & 7) ? 0xFFFF : 0xBFFF);
        FXU(0x26EE) = (rnd() & 1) ? 0x10 : (uint16_t)(rnd() % 0x14);
        for (k = 0; k < 16; k++) {
            uint16_t p = (uint16_t)(0x2688 + 6 * k);

            pakram[p] = (rnd() & 1) ? 0 : (uint8_t)(rnd() % 9);
            pakram[p + 1] = (uint8_t)(rnd() % 3);
            FXW(p + 2) = within(-10, 240);
            FXW(p + 4) = within(-10, 140);
        }
        FXU(0x0340) = 0;
        old_run(old_abi_FxEffectsRun);
        fx_effects_run();
        same_ram();
    }

    CASE("FxTextDraw") {
        static const uint8_t codes[] = {0x0D, 0xFF, 0xFE, 0xFD, 0xFC, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0x20, 0x90};
        uint8_t *s = rom_copy + 0x1F * 0x8000 + 0x7000;
        int len = (int)(rnd() % 12), j = 0;

        fresh();
        while (len-- > 0) {
            uint8_t c = (rnd() & 1) ? (uint8_t)(0x30 + rnd() % 0x2B) : codes[rnd() % sizeof codes];

            s[j++] = c;
            if (c == 0xFF) {
                s[j++] = (uint8_t)(rnd() % 220);
                s[j++] = (uint8_t)(rnd() % 140);
            } else if (c == 0xFE) {
                s[j++] = (uint8_t)(rnd() % 4);
            } else if (c == 0xFD) {
                s[j++] = (uint8_t)(rnd() % 2);
            }
        }
        s[j] = 0;
        FXU(0x2732) = (rnd() % 16) ? 0xF000 : 0;
        FXU(0x2734) = 0x1F;
        FXU(0x2738) = (uint16_t)(rnd() % 200);
        FXU(0x273A) = (uint16_t)(rnd() % 120);
        old_run(old_abi_FxTextDraw);
        fx_text_draw();
        same_ram();
    }

    CASE("FxTextNotice") {
        fresh();
        FXU(0x273C) = (uint16_t)rnd();
        FXU(0x2648) = (rnd() & 7) ? 0x3100 : 0;
        FXU(0x2476) = (rnd() & 7) ? 0x3200 : 0x3100;
        FXU(0x2478) = (rnd() & 7) ? 0x3300 : 0x3100;
        FXW(0x264C) = (int16_t)rnd();
        FXW(0x264A) = within(-0x200, 0x200);
        FXU(0x3196) = (uint16_t)(rnd() % 8);
        old_run(old_abi_FxTextNotice);
        fx_text_notice();
        same_ram();
    }

    printf("frame_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
