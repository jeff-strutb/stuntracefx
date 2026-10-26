/* old: video/fade.c video/palette.c video/dma.c video/windows.c video/window_hdma.c video/colour_math.c object/object.c audio/sound_queue.c input/joypad.c race/time.c video/scroll.c video/gradient.c video/tilemap.c video/oam.c system/vblank_updates.c system/frame_updates.c system/approach.c system/raster.c hud/digits.c hud/slide.c
 *
 * raster_check.c: the port's frame work (what the original does in its
 * raster interrupt: the fade, the palettes, the windows, the uploads) against
 * the decomp's, on random states: the decomp's writes to the video
 * hardware go to the checks' model of it (ppu.c), the port's to its video
 * state, and the two are compared, with the work RAM and the game pak RAM.
 *
 *   check/check.sh raster N
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
#include "machine.h"
#include "cpu_ops.h"
#include "gsu.h"
#undef cpu
#undef gsu

#include "game.h"
#include "video.h"
#include "model.h"
#include "ppu.h"
#include "rom.h"

#define OLD(name) void old_abi_##name(void);
OLD(FadeStepFixed) OLD(FadeStep) OLD(RasterColours) OLD(RasterPaletteUpload) OLD(RasterColoursCycling)
OLD(RasterColoursE4) OLD(RasterColoursUnless0D22) OLD(PaletteCycle) OLD(PaletteKeptColoursStep)
OLD(PlayerColoursFlash) OLD(StatusColourPick) OLD(PalettesStep)
OLD(WindowsSetupSelect) OLD(WindowHdmaNormal) OLD(WindowHdmaBands) OLD(WindowLayoutStepForScreen) OLD(BobWindowsRun)
OLD(WindowSweepIfOn) OLD(WindowSlideOrOscillate) OLD(WindowBandsAnimate) OLD(WindowSplitAnimate) OLD(WindowSlideCheck)
OLD(WindowHdmaFFDD) OLD(WindowHdmaFFD1) OLD(WindowHdmaFFD5) OLD(WindowHdmaFFD9)
OLD(SoundPortsSend) OLD(JoypadRead) OLD(JoypadAccumulate) OLD(FrameSoundAndJoypads) OLD(RaceClockStep)
OLD(Bg2ScrollFromTable) OLD(GradientPointersToBank7F) OLD(NmiFrameUpload) OLD(OamDirectStage) OLD(VblankUpdates) OLD(FrameCountAndPortsEnd) OLD(WindowTable7F) OLD(RasterStage)

static uint64_t rng = 0x3C6EF372FE94F82Bull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static Ppu old_ppu;
static long cases, failed, iteration;
static int scratch_f9;                  /* the case's routine keeps counts in direct page $F9-$FA */

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
    ppu_reset();
    for (i = 0; i < sizeof ppu.vram; i++)
        ppu.vram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof ppu.cgram; i++)
        ppu.cgram[i] = (uint8_t)(i & 1 ? rnd() & 0x7F : rnd());
    for (i = 0; i < sizeof ppu.oam; i++)
        ppu.oam[i] = (uint8_t)rnd();
    for (i = 0; i < 0x34; i++)
        ppu.reg[i] = (uint8_t)rnd();
    for (i = 0; i < 4; i++)
        ppu.hofs[i] = (uint16_t)(rnd() & 0x3FF), ppu.vofs[i] = (uint16_t)(rnd() & 0x3FF);
    for (i = 0; i < 3; i++)
        ppu.fixed[i] = (uint8_t)(rnd() & 0x1F);
    ppu.hdmaen = (uint8_t)rnd();
    memcpy(&wram[0xA2D9], rom_at(0x028000), 0x54A6);  /* the code and tables the game runs from work RAM */
    ppu.vmain = 0x80;
    scratch_f9 = 0;
    for (i = 0; i < 4; i++) {
        model_sound_in[i] = sound_in[i] = (uint8_t)(rnd() & 1 ? 0 : rnd());
        model_sound_out[i] = sound_out[i] = (uint8_t)rnd();
    }
    for (i = 0; i < 2; i++)
        model_pads[i] = pads[i] = (uint16_t)(rnd() & 0xFFF0);
    model_vtime = raster_line = (uint16_t)(rnd() & 0xFF);
    model_htime = raster_dot = (uint16_t)(rnd() & 0xFF);                   /* the game's standing setting: words, a step of 1 */
    video_frame_begin();
    ppu_to_port();
    video_registers_reset();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x00;
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
    old_ppu = ppu;
}

static void same(void)
{
    size_t i;
    char what[160];

    cases++;
    for (i = 0; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && !(scratch_f9 && i >= 0xF9 && i <= 0xFC)) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    if (raster_line != model_vtime || raster_dot != model_htime) {
        fail("raster line %u dot %u, the decomp's %u %u", raster_line, raster_dot, model_vtime, model_htime);
        return;
    }
    for (i = 0; i < 4; i++)
        if (sound_out[i] != model_sound_out[i]) {
            fail("sound port %zu = %02X, the decomp's %02X", i, sound_out[i], model_sound_out[i]);
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
        fprintf(stderr, "usage: raster_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("FadeStepFixed / FadeStep") {
        int fixed = (int)(rnd() & 1);

        fresh();
        fade_out = (uint8_t)(rnd() & 1 ? 0 : rnd());
        fade_level = (uint16_t)(rnd() & 1 ? rnd() & 0x0FFF : rnd());
        old_cpu.p |= P_M | P_X;
        old_run(fixed ? old_abi_FadeStepFixed : old_abi_FadeStep);
        if (fixed)
            fade_step_fixed();
        else
            fade_step();
        same();
    }
    CASE("RasterColours / RasterPaletteUpload / RasterColoursCycling / RasterColoursE4 / RasterColoursUnless0D22") {
        int k = (int)(rnd() % 5);
        uint16_t x = (uint16_t)(rnd() & 1 ? rnd() & 0x3F : rnd());

        fresh();
        palette_animate = (uint8_t)(rnd() & 1 ? 0 : rnd());
        W8(0x0D22) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        old_cpu.p |= P_M;
        if (k == 1)
            old_cpu.p |= P_X;
        else if (k == 2 || k == 3)
            old_cpu.p &= (uint8_t)~P_X;
        old_cpu.x = x;
        old_run(k == 0 ? old_abi_RasterColours : k == 1 ? old_abi_RasterPaletteUpload : k == 2 ? old_abi_RasterColoursCycling
                : k == 3 ? old_abi_RasterColoursE4 : old_abi_RasterColoursUnless0D22);
        switch (k) {
        case 0: raster_colours(); break;
        case 1: raster_palette_upload(); break;
        case 2: raster_colours_cycling(x); break;
        case 3: raster_colours_e4(x); break;
        default: raster_colours_unless_0d22();
        }
        same();
    }
    CASE("PaletteCycle") {
        fresh();
        W16(0xFD5B) = (uint16_t)(rnd() & 15), W16(0xFD5D) = (uint16_t)(rnd() & 15), W16(0xFD5F) = (uint16_t)(rnd() & 15);
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_run(old_abi_PaletteCycle);
        palette_cycle();
        same();
    }
    CASE("PaletteKeptColoursStep / PlayerColoursFlash / StatusColourPick / PalettesStep") {
        int k = (int)(rnd() % 4);
        uint16_t x = (uint16_t)(rnd() & 6), y = (uint16_t)(rnd() & 1);
        uint8_t level = (uint8_t)(rnd() & 1 ? rnd() & 0x0F : rnd());

        fresh();
        scratch_f9 = 1;
        palette_animate = (uint8_t)(rnd() & 3 ? rnd() | 1 : 0);
        if (rnd() & 1)
            W8(0x0D53) = 0, W8(0x0E05 + y) |= 1, W16(0x0D1E + x) |= 0x4000;
        if (rnd() & 1)
            W8(0x0DDA + y) = (uint8_t)(rnd() & 1 ? 0 : rnd() & 0x7F);
        W16(0x0D83) = (uint16_t)(rnd() & 0x1FE), W16(0x0D85) = (uint16_t)(rnd() & 0x1FE);
        W16(0xFD5B) = (uint16_t)(rnd() & 15), W16(0xFD5D) = (uint16_t)(rnd() & 15), W16(0xFD5F) = (uint16_t)(rnd() & 15);
        old_cpu.p |= P_M;
        old_cpu.p = k == 0 ? (uint8_t)(old_cpu.p | P_X) : (uint8_t)(old_cpu.p & ~P_X);
        old_cpu.x = x, old_cpu.y = y;
        old_cpu.a = (uint16_t)((rnd() & 0xFF00) | level);
        old_run(k == 0 ? old_abi_PaletteKeptColoursStep : k == 1 ? old_abi_PlayerColoursFlash
                : k == 2 ? old_abi_StatusColourPick : old_abi_PalettesStep);
        switch (k) {
        case 0: palette_kept_colours_step(x, y); break;
        case 1: player_colours_flash(level, y); break;
        case 2: status_colour_pick(y); break;
        default: palettes_step();
        }
        same();
    }
    CASE("the windows") {
        static void (*const olds[])(void) = {
            old_abi_WindowsSetupSelect, old_abi_WindowHdmaNormal, old_abi_WindowHdmaBands, old_abi_WindowLayoutStepForScreen,
            old_abi_BobWindowsRun, old_abi_WindowSweepIfOn, old_abi_WindowSlideOrOscillate, old_abi_WindowBandsAnimate,
            old_abi_WindowSplitAnimate, old_abi_WindowSlideCheck, old_abi_WindowHdmaFFDD, old_abi_WindowHdmaFFD1,
            old_abi_WindowHdmaFFD5, old_abi_WindowHdmaFFD9,
        };
        static void (*const news[])(void) = {
            windows_setup_select, window_table_normal, window_table_bands, window_layout_step_for_screen,
            bob_windows_run, window_sweep_if_on, window_slide_or_oscillate, window_bands_animate,
            window_split_animate, window_slide_check, window_table_ffdd, window_table_ffd1,
            window_table_ffd5, window_table_ffd9,
        };
        static const char *const names[] = {
            "WindowsSetupSelect", "WindowHdmaNormal", "WindowHdmaBands", "WindowLayoutStepForScreen", "BobWindowsRun",
            "WindowSweepIfOn", "WindowSlideOrOscillate", "WindowBandsAnimate", "WindowSplitAnimate", "WindowSlideCheck",
            "WindowHdmaFFDD", "WindowHdmaFFD1", "WindowHdmaFFD5", "WindowHdmaFFD9",
        };
        int k = (int)(rnd() % 14), j;
        uint16_t o;

        model_current = names[k];

        fresh();
        W8(0x19A9) = (uint8_t)(rnd() & 1 ? (rnd() % 5) * 2 : rnd());
        W8(0x19D8) = (uint8_t)(rnd() & 1 ? 0 : rnd() & 3 ? rnd() % 7 : rnd());
        W8(0x19BC) = (uint8_t)(rnd() & 3 ? rnd() : 0xFF);
        W8(0x19DF) = (uint8_t)(rnd() & 1 ? rnd() & 1 : rnd());
        W8(0x0D62) = (uint8_t)(rnd() & 1);
        W8(0x0D2E) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        W16(0x19B6) = (uint16_t)(rnd() % 12 * 2);
        W8(0x19BD) = (uint8_t)(rnd() & 1 ? (rnd() & 1 ? 1 : 0xFF) : rnd());
        W16(0x19B8) = (uint16_t)(rnd() & 1 ? 2 : 0xFFFE);
        if (rnd() & 1)                  /* the kept colours' timers counting up */
            W8(0x0D53) = 0, W8(0x0E05) |= 1, W8(0x0E06) |= 1;
        W8(0x19D5) = (uint8_t)(rnd() % 6);
        if (rnd() & 1)
            W16(0x19D9) = (uint16_t)(W16(0x19D4) + W16(0x19D6));
        if (rnd() & 1)
            W16(0x19D9) = (uint16_t)(W16(0x19DB) + W16(0x19DD));
        for (j = 0; j < 2; j++) {
            uint16_t obj = (uint16_t)(rnd() & 0x7FE), l1 = (uint16_t)(0x0300 + (rnd() & 0x7FE)), l2 = (uint16_t)(0x0300 + (rnd() & 0x7FE));

            W16(j ? 0x034A : 0x0346) = obj;
            W16(0x204F + obj) = l1;
            W16(0x204F + l1) = l2;
        }
        W8(0x19D1) = (uint8_t)(rnd() % 4 * 2);
        W8(0x02) = (uint8_t)(rnd() % 4 * 2);
        W16(0x19C3) = (uint16_t)(0xE000 + (rnd() & 0x0FFF));
        W16(0x19C5) = (uint16_t)(rnd() & 1 ? rnd() & 0xFF : -(rnd() & 0xFF));
        if (rnd() & 1)
            W16(0x19C5) = (uint16_t)(0xE000 - W16(0x19C3));
        for (o = 0x1A08; o < 0x1A24; o++)
            if (rnd() & 1)
                W8(o) = (uint8_t)(rnd() & 3);
        if (rnd() & 1) {                /* the moving windows near where they turn */
            for (j = 0; j < 2; j++) {
                W8(0x1A1C + j) = (uint8_t)(0x14 + rnd() % 10), W8(0x1A20 + j) = (uint8_t)(rnd() & 1 ? 2 : 0xFE);
                W8(0x1A15 + j) = (uint8_t)(0xE6 + rnd() % 14), W8(0x1A19 + j) = (uint8_t)(rnd() & 1 ? 2 : 0xFE);
                W8(0x1A08 + j) = (uint8_t)(0x3E + rnd() % 6), W8(0x1A0C + j) = (uint8_t)(rnd() & 1 ? 1 : 0xFF);
                W8(0x1A1E + j) = W8(0x1A17 + j) = 1;
            }
            W8(0x0DDA) = (uint8_t)((1 + rnd() % 3) << 5);
            W8(0x1A0E) = W8(0x0DDA) >> 5;
            W8(0x1A0A) = (uint8_t)(ram_code_byte((uint16_t)(0xD30D + W8(0x1A0E) - 1)) - 1);
        }
        old_cpu.p = (uint8_t)(old_cpu.p | (rnd() & (P_M | P_X)));
        old_run(olds[k]);
        news[k]();
        same();
        model_current = "the windows";
    }
    CASE("SoundPortsSend / JoypadRead / JoypadAccumulate / FrameSoundAndJoypads") {
        int k = (int)(rnd() % 4);

        fresh();
        if (rnd() & 1)
            W8(0x10E2) = W8(0x10E3) = 0;
        if (rnd() & 1)
            W8(0x10DA) = W8(0x10DC) = 0;
        W8(0x0DCC) = (uint8_t)(rnd() % 17), W8(0x0DCE) = (uint8_t)(rnd() % 17);
        W8(0x0DC8) = (uint8_t)(rnd() & 15), W8(0x0DCA) = (uint8_t)(rnd() & 15);
        old_cpu.p |= P_X;
        old_run(k == 0 ? old_abi_SoundPortsSend : k == 1 ? old_abi_JoypadRead : k == 2 ? old_abi_JoypadAccumulate
                : old_abi_FrameSoundAndJoypads);
        switch (k) {
        case 0: sound_ports_send(); break;
        case 1: joypad_read(); break;
        case 2: joypad_accumulate(); break;
        default: frame_sound_and_joypads();
        }
        same();
    }
    CASE("RaceClockStep") {
        static const uint16_t near[] = {0x0059, 0x0058, 0x0009, 0x0000, 0x0001, 0x9999, 0x9990};

        fresh();
        W8(0x0E5B) = (uint8_t)(rnd() & 3 ? 1 : 0);
        if (rnd() & 1) {
            W16(0x0E62) = near[rnd() % 7], W16(0x0E64) = near[rnd() % 7], W16(0x0E66) = near[rnd() % 7];
            W16(0x0E68) = near[rnd() % 7], W16(0x0E70) = near[rnd() % 7];
            W16(0x0D29) = (uint16_t)(rnd() & 1 ? 0x0167 : 0x0333);
        }
        old_run(old_abi_RaceClockStep);
        race_clock_step();
        same();
    }
    CASE("Bg2ScrollFromTable / GradientPointersToBank7F") {
        int wide = (int)(rnd() & 1);
        uint16_t band = (uint16_t)(rnd() & (wide ? 0x3F : 0xFF));

        fresh();
        old_cpu.p |= P_M;
        old_cpu.p = wide ? (uint8_t)(old_cpu.p & ~P_X) : (uint8_t)(old_cpu.p | P_X);
        old_cpu.x = (uint16_t)(wide ? band : (rnd() & 0xFF00) | band);
        if (rnd() & 1) {
            old_run(old_abi_Bg2ScrollFromTable);
            bg2_scroll_from_table(band);
        } else {
            old_run(old_abi_GradientPointersToBank7F);
            gradient_pointers_to_bank_7f();
        }
        same();
    }
    CASE("NmiFrameUpload") {
        static const uint8_t race[] = {0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1E, 0x20};

        fresh();
        scratch_f9 = 1;
        W8(0x0D22) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        W16(0x0D7D) = race[rnd() % sizeof race];
        W8(0x0D23) = (uint8_t)(rnd() % 10 * 2);
        W16(0x0D7F) = (uint16_t)(rnd() & 1 ? 0 : rnd());
        W16(0x10A0) = (uint16_t)(0x8000 + (rnd() & 0x3FFF));
        old_run(old_abi_NmiFrameUpload);
        nmi_frame_upload();
        same();
    }
    CASE("OamDirectStage") {
        fresh();
        W8(0x0D2B) = (uint8_t)(rnd() & 1 ? 1 : rnd() & 3);
        W16(0x0E5D) = (uint16_t)(rnd() & 1 ? 0 : rnd());
        W8(0x0D54) = (uint8_t)(rnd() % 6 * 2);
        W8(0x0E16) = (uint8_t)(rnd() & 1 ? 0xAC : rnd());
        W8(0x0E12) = (uint8_t)(rnd() & 1 ? 0x1F : rnd());
        W8(0x0E0B) = (uint8_t)(rnd() & 1 ? 0x2A : rnd());
        W8(0x0D57) = (uint8_t)(rnd() & 1 ? (rnd() & 1 ? 0x10 : 0xD0) : rnd());
        old_run(old_abi_OamDirectStage);
        oam_direct_stage();
        same();
    }
    CASE("VblankUpdates / FrameCountAndPortsEnd / WindowTable7F") {
        int k = (int)(rnd() % 3);

        fresh();
        scratch_f9 = 1;
        W16(0x0D7D) = (uint16_t)(rnd() & 1 ? rnd() % 7 * 2 : rnd() & 0xFF);
        W16(0x1905) = (uint16_t)(rnd() & 1 ? rnd() % 4 : rnd());
        W16(0x1907) = (uint16_t)(rnd() & 1 ? rnd() % 12 * 4 : rnd());
        W8(0x11D7) = (uint8_t)rnd();
        W8(0x11D7) &= (uint8_t)~0x0F;   /* no queued uploads: their own checks are video_check's */
        W16(0x18B9) = (uint16_t)(rnd() & 1 ? 0 : rnd());
        W16(0x1901) = (uint16_t)(rnd() % 6);
        W16(0x1903) = (uint16_t)(rnd() & 1 ? 0 : rnd());
        W16(0x18FF) = (uint16_t)(rnd() & 1);
        old_cpu.p &= (uint8_t)~P_M;
        old_cpu.p |= P_X;
        old_run(k == 0 ? old_abi_VblankUpdates : k == 1 ? old_abi_FrameCountAndPortsEnd : old_abi_WindowTable7F);
        switch (k) {
        case 0: vblank_updates(); break;
        case 1: frame_count_and_ports_end(); break;
        default: window_table_7f();
        }
        same();
    }
    CASE("RasterStage") {
        static const uint8_t stages[] = {0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18,
                                         0x1A, 0x1C, 0x1E, 0x20};

        fresh();
        scratch_f9 = 1;
        W8(0x0D3F) = stages[rnd() % sizeof stages];
        /* the states the stages' callees are checked from above */
        W16(0x0D7D) = (uint16_t)(rnd() % 14 * 2);
        if (W16(0x0D7D) == 0x1C)
            W16(0x0D7D) = 0x1E;
        W8(0x0D23) = (uint8_t)(rnd() % 10 * 2);
        W8(0x0D22) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        W16(0x10A0) = (uint16_t)(0x8000 + (rnd() & 0x3FFF));
        W8(0x0D54) = (uint8_t)(rnd() % 6 * 2);
        W8(0x0D2B) = (uint8_t)(rnd() & 3);
        W16(0x0D83) = (uint16_t)(rnd() & 0x1FE), W16(0x0D85) = (uint16_t)(rnd() & 0x1FE);
        W16(0xFD5B) = (uint16_t)(rnd() & 15), W16(0xFD5D) = (uint16_t)(rnd() & 15), W16(0xFD5F) = (uint16_t)(rnd() & 15);
        W8(0x19A9) = (uint8_t)((rnd() % 5) * 2);
        W8(0x19D5) = (uint8_t)(rnd() % 6);
        W16(0x19B6) = (uint16_t)(rnd() % 12 * 2);
        W16(0x19B8) = (uint16_t)(rnd() & 1 ? 2 : 0xFFFE);
        if (rnd() & 1)                  /* the kept colours' timers counting up */
            W8(0x0D53) = 0, W8(0x0E05) |= 1, W8(0x0E06) |= 1;
        W8(0x19D1) = (uint8_t)(rnd() % 4 * 2);
        W8(0x02) = (uint8_t)(rnd() % 4 * 2);
        W16(0x1905) = (uint16_t)(rnd() % 4);
        W16(0x18FF) = (uint16_t)(rnd() & 1);
        W8(0x11D7) &= (uint8_t)~0x0F;
        W16(0x16AF) = 0;                /* no transfers queued but the animated tiles' */
        W16(0x1191) = (uint16_t)(rnd() % 9 * 2), W16(0x1193) = (uint16_t)(rnd() % 9 * 2);
        {
            int j;

            for (j = 0; j < 2; j++) {
                uint16_t obj = (uint16_t)(rnd() & 0x7FE), l1 = (uint16_t)(0x0300 + (rnd() & 0x7FE)), l2 = (uint16_t)(0x0300 + (rnd() & 0x7FE));

                W16(j ? 0x034A : 0x0346) = obj;
                W16(0x204F + obj) = l1;
                W16(0x204F + l1) = l2;
            }
        }
        old_cpu.db = 0x00;
        {
            static char name[40];

            snprintf(name, sizeof name, "RasterStage %02X", W8(0x0D3F));
            model_current = name;
        }
        old_run(old_abi_RasterStage);
        raster_stage();
        same();
        model_current = "RasterStage";
    }
    printf("raster_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
