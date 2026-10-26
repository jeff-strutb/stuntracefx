/* old: video/backdrop.c video/colour_math.c video/records_7f.c save/checksum.c video/windows.c audio/sound_queue.c
 * ldflags: -Wl,--wrap=old_wr8
 *
 * smallvideo_check.c: the port's small video and save routines (backdrop.c,
 * colour_math.c, records_7f.c, save/checksum.c) against the decomp's, on
 * random states: work RAM and the game pak RAM, with the code copy and the
 * object table's links shaped as the game keeps them. The decomp's writes to
 * the video hardware go to the checks' model of it (ppu.c), the port's to its
 * video state, and the two are compared, as in raster_check.c.
 *
 * The model has no WRAM DMA (WMADD, WMDATA, channel 0 into work RAM): the
 * wrapper of old_wr8 below does it for the decomp's writes, as the hardware
 * does, and nothing else of the model is changed.
 *
 *   check/check.sh smallvideo N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#include "machine.h"
#include "cpu_ops.h"
#undef cpu

#include "game.h"
#include "objects.h"
#include "smallvideo.h"
#include "model.h"
#include "ppu.h"
#include "rom.h"

#define OLD(name) void old_abi_##name(void);
OLD(PlayerVehicleRaised) OLD(BackdropScrollFromView)
OLD(ColourMathA3) OLD(ColourMathA3Long) OLD(ColourMathA7Start) OLD(FixedColourFromStep)
OLD(ColourMathSub83Windowed) OLD(ColourMathOff) OLD(ColourMathOffLong)
OLD(Records7FFromF634) OLD(Records7FFromF170) OLD(SaveChecksum)

static uint64_t rng = 0x2545F4914F6CDD1Dull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration;

/* ---- the WRAM DMA the model lacks ---- */

static uint32_t wmadd;                  /* the work RAM address the DMA writes to (WMADDH:M:L) */

void __real_old_wr8(uint32_t a, uint8_t v);

/* The decomp's writes to the system area's registers: WMADDL, M and H
   ($2181-$2183), and channel 0's DMA from memory to WMDATA ($2180) when the
   enable ($420B) is written; the rest go to the model. */
void __wrap_old_wr8(uint32_t a, uint8_t v)
{
    uint16_t o = (uint16_t)a;

    if (((a >> 16) & 0x7F) < 0x40) {   /* the system area, in the banks below $40 */
        if (o == 0x2181) { wmadd = (wmadd & 0x1FF00u) | v; return; }
        if (o == 0x2182) { wmadd = (wmadd & 0x100FFu) | (uint32_t)v << 8; return; }
        if (o == 0x2183) { wmadd = (wmadd & 0x0FFFFu) | (uint32_t)(v & 1) << 16; return; }
        if (o == 0x420B && (v & 1)) {
            const uint8_t *d = ppu.dma;
            uint16_t src = (uint16_t)(d[2] | d[3] << 8);
            unsigned n = d[5] | d[6] << 8, i;

            if (d[0] != 0x00 || d[1] != 0x80) {
                fprintf(stderr, "check: a DMA the check does not model (mode %02X, B-bus %02X)\n", d[0], d[1]);
                exit(2);
            }
            if (!n)
                n = 0x10000;
            for (i = 0; i < n; i++)
                wram[(wmadd + i) & 0x1FFFF] = bank_byte(d[4], (uint16_t)(src + i));
            wmadd += n;
        }
    }
    __real_old_wr8(a, v);
}

/* ---- the states ---- */

static uint16_t record(int i)
{
    return (uint16_t)(1 + 0x62 * i);
}

static void fresh(void)
{
    size_t i;

    for (i = 0; i < sizeof wram; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    memcpy(&wram[0xA2D9], rom_at(0x028000), 0x54A6);  /* the code and tables the game runs from work RAM */
    ppu_reset();
    for (i = 0; i < 0x34; i++)
        ppu.reg[i] = (uint8_t)rnd();
    video_frame_begin();
    ppu_to_port();
    video_registers_reset();
    /* the objects' links (+$4F): each to another object (never none: the chain
       is followed twice, and the game's chains name objects). The vehicle
       routine ends its chain with a read in bank $00 at the link's offset, which
       is work RAM only below $2000, so the links stay in the first 52 records
       (offsets below $2000). The vehicles start from the same records. Set
       after the video state, which writes into work RAM. */
    for (i = 0; i < 0x135; i++)
        obj_link(record((int)i)) = record((int)(rnd() % 52));
    W16(0x0346) = record((int)(rnd() % 52));
    W16(0x034A) = record((int)(rnd() % 52));
    /* the sound queues as the game keeps them: heads in the ring, counts up to 16 */
    W8(0x0DCA) = (uint8_t)(rnd() & 0x0F);
    W8(0x0DCE) = (uint8_t)(rnd() % 17);
    W8(0x0DC8) = (uint8_t)(rnd() & 0x0F);
    W8(0x0DCC) = (uint8_t)(rnd() % 17);
    wmadd = 0;
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
}

static void old_run(void (*f)(void))
{
    memcpy(start_w, wram, sizeof wram);
    memcpy(start_p, pakram, sizeof pakram);
    f();
    memcpy(old_w, wram, sizeof wram);
    memcpy(old_p, pakram, sizeof pakram);
    memcpy(wram, start_w, sizeof wram);
    memcpy(pakram, start_p, sizeof pakram);
}

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

static void same(void)
{
    size_t i;
    char what[160];

    cases++;
    for (i = 0x100; i < sizeof wram; i++)
        if (wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    if (ppu_differs(what, sizeof what) || ppu_screen_differs(what, sizeof what))
        fail("%s", what);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: smallvideo_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("PlayerVehicleRaised") {
        uint8_t player = (uint8_t)rnd();
        uint16_t got;

        fresh();
        old_cpu.x = player;
        old_run(old_abi_PlayerVehicleRaised);
        got = player_vehicle_raised(player);
        if (old_cpu.a != got)
            fail("returned %04X, the decomp's %04X", got, (uint16_t)old_cpu.a);
        same();
    }
    CASE("BackdropScrollFromView") {
        uint16_t got;

        fresh();
        W16(0x0D7F) = (uint16_t)(rnd() & 1);       /* the half: 0 or 1 */
        old_run(old_abi_BackdropScrollFromView);
        got = backdrop_scroll_from_view();
        if (old_cpu.a != got)
            fail("returned %04X, the decomp's %04X", got, (uint16_t)old_cpu.a);
        same();
    }
    CASE("ColourMathA3 / ColourMathA3Long / ColourMathA7Start / ColourMathOff / ColourMathOffLong") {
        int k = (int)(rnd() % 5);

        fresh();
        old_run(k == 0 ? old_abi_ColourMathA3 : k == 1 ? old_abi_ColourMathA3Long : k == 2 ? old_abi_ColourMathA7Start
                : k == 3 ? old_abi_ColourMathOff : old_abi_ColourMathOffLong);
        switch (k) {
        case 0: colour_math_a3(); break;
        case 1: colour_math_a3_long(); break;
        case 2: colour_math_a7_start(); break;
        case 3: colour_math_off(); break;
        default: colour_math_off_long();
        }
        same();
    }
    CASE("ColourMathSub83Windowed") {
        fresh();
        old_run(old_abi_ColourMathSub83Windowed);
        colour_maths_sub83_windowed();
        same();
    }
    CASE("FixedColourFromStep") {
        uint8_t got;

        fresh();
        W8(0x13408) = (uint8_t)rnd();              /* the step: any of the table's 256 */
        old_run(old_abi_FixedColourFromStep);
        got = fixed_colour_from_step();
        if ((uint8_t)old_cpu.a != got)
            fail("returned %02X, the decomp's %02X", got, (uint8_t)old_cpu.a);
        same();
    }
    CASE("Records7FFromF634") {
        fresh();
        old_cpu.db = 0x09;                          /* the position list is in bank $09 */
        old_run(old_abi_Records7FFromF634);
        records_7f_from_f634(0x09);
        same();
    }
    CASE("Records7FFromF170") {
        uint8_t got;

        fresh();
        old_cpu.db = 0x09;
        old_run(old_abi_Records7FFromF170);
        got = records_7f_from_f170(0x09);
        if ((uint8_t)old_cpu.y != got)
            fail("returned %02X, the decomp's %02X", got, (uint8_t)old_cpu.y);
        same();
    }
    CASE("SaveChecksum") {
        uint8_t got;

        fresh();
        old_run(old_abi_SaveChecksum);
        got = save_checksum();
        if ((uint8_t)old_cpu.a != got)
            fail("returned %02X, the decomp's %02X", got, (uint8_t)old_cpu.a);
        same();
    }
    printf("smallvideo_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
