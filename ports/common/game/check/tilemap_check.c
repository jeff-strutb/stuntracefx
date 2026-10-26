/* old: video/tilemap.c math/random.c
 *
 * tilemap_check.c: the port's tilemap buffers and uploads (video/tilemap.c)
 * against the decomp's, on random states: the decomp's writes to VRAM and
 * the screen registers go to the checks' model of the hardware (ppu.c), the
 * port's to its video state, and the two are compared, with work RAM (from
 * $0100 up, the direct page being the routines' scratch) and the game pak
 * RAM. Where a routine takes the VRAM address from the game's VMADDL, the
 * model's address after the decomp's writes must be the one the port
 * returns.
 *
 *   check/check.sh tilemap N
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
#include "tilemap.h"
#include "model.h"
#include "ppu.h"

#define OLD(name) void old_abi_##name(void);
OLD(TilemapRowFromGsuRam) OLD(VramDmaTileFromGsuRam) OLD(VramFillGradient)
OLD(VramFrameRows2B0) OLD(VramFrameRows1B0) OLD(VramPictureRows2B0) OLD(VramPictureRows1B0)
OLD(ScreenFrameAndPictures) OLD(VramNoiseTiles) OLD(TilemapRowFrom7FBBD7) OLD(TilemapRowFromFB4E)
OLD(TilemapPaletteFill) OLD(TilemapPaletteCycle) OLD(TilemapPaletteCycle87) OLD(TilemapPaletteCycle107)
OLD(TilemapPaletteCycle187) OLD(TilemapLevelColumns) OLD(TilemapRowToE3D7) OLD(BackdropAnglesFromView)
OLD(TilemapRow87Palette5) OLD(TilemapRow107Palette5) OLD(TilemapRow187Palette5) OLD(TilemapRow187Palette4)
OLD(TilemapRow207Palette4) OLD(TilemapRow287Palette4) OLD(DigitTiles1184) OLD(TimesKeepAndAdvance)

static uint64_t rng = 0x9E3779B97F4A7C15ull;

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

/* a byte or a word of work RAM in a bank ($7E, $7F, or bank 0's low 8 K) */
static void set_byte(uint8_t bank, uint16_t at, uint8_t v)
{
    wram[((uint32_t)(bank & 1) << 16) | at] = v;
}

static void set_word(uint8_t bank, uint16_t at, uint16_t v)
{
    set_byte(bank, at, (uint8_t)v);
    set_byte(bank, (uint16_t)(at + 1), (uint8_t)(v >> 8));
}

/* the data bank the routines are run in: the game's buffers are in $7E or
   $7F */
static uint8_t pick_bank(void)
{
    return (rnd() & 1) ? 0x7E : 0x7F;
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
    ppu.vmain = 0x80;                   /* the game's standing setting: words, a step of 1 */
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

static void expect(int ok, const char *what)
{
    if (!ok)
        fail("%s", what);
}

/* work RAM from $0100 up and the pak RAM, the PPU's model against the
   port's video state */
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
        fprintf(stderr, "usage: tilemap_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("TilemapRowFromGsuRam") {
        uint8_t bank = pick_bank();
        uint16_t a = (uint16_t)(rnd() % 0x4000), y = (uint16_t)(rnd() % 0x400);

        fresh();
        W16(0x00F1) = (uint16_t)(rnd() % 0x4000);     /* the picture's row base: the offsets stay in the bank */
        old_cpu.db = bank;
        old_cpu.a = a;
        old_cpu.y = y;
        old_run(old_abi_TilemapRowFromGsuRam);
        tilemap_row_from_gsu_ram(bank, a, y);
        same();
    }

    CASE("VramDmaTileFromGsuRam") {
        uint16_t tile = (uint16_t)(rnd() % 0x400), vram = (uint16_t)(rnd() & 0x7FFF);

        fresh();
        W16(0x00F1) = (uint16_t)(rnd() % 0x4000);
        old_cpu.a = tile;
        old_cpu.x = vram;
        old_run(old_abi_VramDmaTileFromGsuRam);
        {
            uint16_t next = vram_tile_from_gsu_ram(tile, vram);

            expect(next == ppu.vaddr, "the VRAM address after the tile differs");
        }
        same();
    }

    CASE("VramFillGradient") {
        uint16_t start = (uint16_t)rnd(), vram = (uint16_t)(rnd() & 0x7FFF);

        fresh();
        ppu.vaddr = vram;
        old_cpu.a = start;
        old_run(old_abi_VramFillGradient);
        expect(vram_fill_gradient(vram, start) == ppu.vaddr, "the VRAM address after the gradient differs");
        same();
    }

    CASE("VramFrameRows2B0 / VramFrameRows1B0") {
        uint8_t bank = pick_bank();
        uint16_t base = (rnd() & 1) ? 0x02B0 : 0x01B0, vram = (uint16_t)(rnd() & 0x7FFF);

        fresh();
        set_byte(bank, 0x0D2B, (uint8_t)(rnd() % 5));       /* the HUD mode */
        set_byte(bank, 0x10CD, (uint8_t)(rnd() % 3 ? 0 : rnd()));
        ppu.vaddr = vram;
        old_cpu.db = bank;
        old_run(base == 0x02B0 ? old_abi_VramFrameRows2B0 : old_abi_VramFrameRows1B0);
        expect(vram_frame_rows(bank, base, vram) == ppu.vaddr, "the VRAM address after the frame rows differs");
        same();
    }

    CASE("VramPictureRows2B0 / VramPictureRows1B0") {
        uint16_t base = (rnd() & 1) ? 0x02B0 : 0x01B0, vram = (uint16_t)(rnd() & 0x7FFF);

        fresh();
        ppu.vaddr = vram;
        old_run(base == 0x02B0 ? old_abi_VramPictureRows2B0 : old_abi_VramPictureRows1B0);
        expect(vram_picture_rows(base, vram) == ppu.vaddr, "the VRAM address after the picture differs");
        same();
    }

    CASE("ScreenFrameAndPictures") {
        uint8_t bank = pick_bank();

        fresh();
        set_byte(bank, 0x0D2B, (uint8_t)(rnd() % 5));
        set_byte(bank, 0x10CD, (uint8_t)(rnd() % 3 ? 0 : rnd()));
        old_cpu.db = bank;
        old_run(old_abi_ScreenFrameAndPictures);
        screen_frame_and_pictures(bank);
        same();
    }

    CASE("VramNoiseTiles") {
        uint8_t bank = (uint8_t)(rnd() % 3 == 0 ? 0x00 : pick_bank());   /* the PPU at bank 0, else work RAM */

        fresh();
        old_cpu.db = bank;
        old_run(old_abi_VramNoiseTiles);
        expect(vram_noise_tiles(bank) == old_cpu.a, "the last word differs");
        same();
    }

    CASE("TilemapRowFrom7FBBD7") {
        uint16_t x = (uint16_t)(rnd() % 0x800), x2;

        fresh();
        old_cpu.x = x;
        old_run(old_abi_TilemapRowFrom7FBBD7);
        x2 = x;
        expect(tilemap_row_from_7fbbd7(&x2) == old_cpu.x && x2 == old_cpu.x, "the offset differs");
        same();
    }

    CASE("TilemapRowFromFB4E") {
        uint8_t bank = (rnd() & 1) ? 0x02 : 0x7F;     /* the level's table in ROM, or work RAM */
        uint16_t x = (uint16_t)(rnd() % 0x800), y = (uint16_t)(rnd() % 0x40), x2, y2;

        fresh();
        old_cpu.db = bank;
        old_cpu.x = x;
        old_cpu.y = y;
        old_run(old_abi_TilemapRowFromFB4E);
        x2 = x, y2 = y;
        expect(tilemap_row_from_fb4e(bank, &x2, &y2) == old_cpu.x && x2 == old_cpu.x && y2 == old_cpu.y,
               "the offsets differ");
        same();
    }

    CASE("TilemapPaletteFill") {
        uint16_t x = (uint16_t)(rnd() % 0x400), count = (uint16_t)(1 + rnd() % 20);
        uint8_t bits = (uint8_t)rnd();

        fresh();
        set_byte(0x00, 0x00F1, bits);            /* the palette bits come in $F1 */
        old_cpu.x = x;
        old_cpu.y = count;
        old_run(old_abi_TilemapPaletteFill);
        expect(tilemap_palette_fill(x, count, bits) == old_cpu.x, "the offset past the entries differs");
        same();
    }

    CASE("TilemapPaletteCycle") {
        uint8_t bank = pick_bank();
        uint16_t x = (uint16_t)(rnd() % 0x400), count = (uint16_t)(1 + rnd() % 20), x2;
        uint8_t frame = (uint8_t)rnd();

        if (rnd() % 4)
            frame &= 0xFC;                       /* mostly the frames the cycle runs on */
        fresh();
        set_byte(bank, 0x0E3B, frame);
        set_byte(bank, 0x0D2B, (uint8_t)(rnd() % 4));                      /* the HUD mode */
        old_cpu.db = bank;
        old_cpu.x = x;
        old_cpu.y = count;
        old_run(old_abi_TilemapPaletteCycle);
        x2 = x;
        tilemap_palette_cycle(bank, &x2, count);
        expect(x2 == old_cpu.x, "the offset past the entries differs");
        same();
    }

    CASE("TilemapPaletteCycle87 / 107 / 187") {
        uint8_t bank = pick_bank();
        int w = (int)(rnd() % 3);

        uint8_t frame = (uint8_t)rnd();

        if (rnd() % 4)
            frame &= 0xFC;
        fresh();
        set_byte(bank, 0x0E3B, frame);
        set_byte(bank, 0x0D2B, (uint8_t)(rnd() % 4));
        old_cpu.db = bank;
        old_run(w == 0 ? old_abi_TilemapPaletteCycle87 : w == 1 ? old_abi_TilemapPaletteCycle107 : old_abi_TilemapPaletteCycle187);
        if (w == 0)
            tilemap_palette_cycle_87(bank);
        else if (w == 1)
            tilemap_palette_cycle_107(bank);
        else
            tilemap_palette_cycle_187(bank);
        same();
    }

    CASE("TilemapRow strips with palette marks") {
        static void (*const old[6])(void) = {old_abi_TilemapRow87Palette5, old_abi_TilemapRow107Palette5,
            old_abi_TilemapRow187Palette5, old_abi_TilemapRow187Palette4, old_abi_TilemapRow207Palette4,
            old_abi_TilemapRow287Palette4};
        static void (*const mine[6])(void) = {tilemap_row_87_palette5, tilemap_row_107_palette5,
            tilemap_row_187_palette5, tilemap_row_187_palette4, tilemap_row_207_palette4,
            tilemap_row_287_palette4};
        int w = (int)(rnd() % 6);

        fresh();
        old_run(old[w]);
        mine[w]();
        same();
    }

    CASE("TilemapLevelColumns") {
        unsigned k;

        fresh();
        for (k = 0; k < 12; k++)
            set_byte(0x00, (uint16_t)(0x0F05 + k), (uint8_t)(rnd() % 6));   /* the levels 0-4 (5 counts as 4) */
        old_run(old_abi_TilemapLevelColumns);
        expect(tilemap_level_columns() == old_cpu.a, "the last offset differs");
        same();
    }

    CASE("TilemapRowToE3D7") {
        uint8_t bank = pick_bank();
        uint16_t x = (uint16_t)((rnd() % 0x200) * 2), y = (uint16_t)((rnd() % 0x200) * 2), x2, y2;

        fresh();
        old_cpu.db = bank;
        old_cpu.x = x;
        old_cpu.y = y;
        old_run(old_abi_TilemapRowToE3D7);
        x2 = x, y2 = y;
        tilemap_row_to_e3d7(bank, &x2, &y2);
        expect(x2 == old_cpu.x && y2 == old_cpu.y, "the offsets differ");
        same();
    }

    CASE("BackdropAnglesFromView") {
        uint8_t bank = pick_bank();

        fresh();
        set_word(bank, 0x0664, (uint16_t)rnd());
        set_word(bank, 0x0666, (uint16_t)rnd());
        old_cpu.db = bank;
        old_run(old_abi_BackdropAnglesFromView);
        expect(backdrop_angles_from_view(bank) == old_cpu.a, "the heading differs");
        same();
    }

    CASE("DigitTiles1184") {
        unsigned k;

        fresh();
        for (k = 0; k < 3; k++)
            set_byte(0x00, (uint16_t)(0x1184 + k), (uint8_t)(rnd() % 10));
        old_run(old_abi_DigitTiles1184);
        digit_tiles_1184();
        same();
    }

    CASE("TimesKeepAndAdvance") {
        uint8_t bank = pick_bank();

        fresh();
        old_cpu.db = bank;
        old_run(old_abi_TimesKeepAndAdvance);
        times_keep_and_advance(bank);
        same();
    }

    printf("tilemap_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
