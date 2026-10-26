/* old: video/oam.c gsu/run.c gsu/params.c gsu/screen.c gsu/programs.c render/unused.c render/matrix.c render/arith.c render/angle.c math/fx_math.c gsu/fx_init.c data/unpack.c render/sort.c render/hud.c render/points.c render/fx_points.c render/project.c render/table.c hud/lap_time.c audio/sound_queue.c system/misc.c
 *
 * oam_check.c: the port's sprite table fills (video/oam.c) against the
 * decomp's, on random states: the decomp's writes to the sprite hardware
 * go to the checks' model of it (ppu.c), the port's to its video state, and
 * the two are compared, with work RAM (from $0100 up, the direct page being
 * the routines' scratch) and the game pak RAM. The data bank is one the
 * routine runs in (work RAM, or the bank the table is read from). The
 * picture turn runs the Super FX programs: they run through the bus model
 * (model_gsu_start), as the game's own calls do.
 *
 *   check/check.sh oam N
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

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
#include "oam.h"
#include "render.h"
#include "video.h"
#include "model.h"
#include "ppu.h"
#include "rom.h"

#define OLD(name) void old_abi_##name(void);
OLD(OamHideRange) OLD(OamWriteSprites36To46) OLD(OamBufferClear) OLD(OamBufferHideLarge)
OLD(OamBufferHideLargeLong) OLD(OamFromList) OLD(OamSprite0AddY)
OLD(OamAttr36x4) OLD(OamAttr38x6) OLD(OamAttr38x7) OLD(OamAttr36x6) OLD(OamAttr38x4) OLD(OamAttr36x7)
OLD(OamHideEntries3And9) OLD(OamHideEntries0To2) OLD(OamSetPalette)
OLD(OamBlock59Left) OLD(OamBlock123Left) OLD(OamBlock52Left) OLD(OamBlock59Right) OLD(OamBlock123Right)
OLD(OamBlock52Right) OLD(OamColumn8) OLD(OamHighBits58) OLD(OamPaletteFromTableE30F)
OLD(OamPaletteFromTableE26A) OLD(OamPaletteFromTableE26AByMode) OLD(OamPalettesE28D) OLD(OamPaletteForD75)
OLD(OamHighBits18) OLD(OamHighBits58Park112) OLD(OamHighBits18Park48) OLD(OamHide36And96) OLD(OamHide75Up)
OLD(OamLargeColumns) OLD(OamPlaceRows75And112) OLD(MetaspriteDraw) OLD(FlashingMetaspriteDraw)
OLD(FlashingMetaspriteDraw0) OLD(FlashingMetaspriteDrawByMode) OLD(SpritePieces3)
OLD(SpritePiecesAtLeft) OLD(SpritePiecesAtRight) OLD(SpritePiecesAt46Y71) OLD(SpritePiecesAt46Y82)
OLD(SpritePiecesAt46Y93) OLD(SpritePiecesAt62Y71) OLD(SpritePiecesAt62Y83) OLD(SpritePiecesAt62Y95)
OLD(SpritePiecesCarAt51) OLD(SpritePiecesTwoCars) OLD(SpritePiecesTwoCarsTop) OLD(OamFillEvery4)
OLD(OamWordRow5) OLD(OamHideForHudMode) OLD(OverlaySpritesStep) OLD(OverlaySpritesState0)
OLD(PictureTurnStep) OLD(FxUnpackProgram) OLD(FxTiles2bppTo4bppHigh) OLD(FxTiles2bppTo4bppLow)

static uint64_t rng = 0x2545F4914F6CDD1Dull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static Ppu old_ppu;
static long cases, failed, skipped, iteration;

/* The decomp's Super FX programs by where they start (the picture turn's
   unpacker and its two tile programs). */
static const struct {
    uint32_t at;
    void (*run)(void);
} olds[] = {
    {0x01CF42, old_abi_FxUnpackProgram},
    {0x07EE50, old_abi_FxTiles2bppTo4bppHigh},
    {0x07EE59, old_abi_FxTiles2bppTo4bppLow},
};

static void old_start(uint8_t bank, uint16_t start)
{
    uint32_t at = (uint32_t)(bank & 0x7F) << 16 | start;
    unsigned i;

    for (i = 0; i < sizeof olds / sizeof olds[0]; i++)
        if (olds[i].at == at) {
            old_gsu.r[10] = 0x0240;     /* the programs' stack, in the pak RAM */
            olds[i].run();
            return;
        }
    fprintf(stderr, "check: %s: the decomp started $%06X, which the check lacks\n", model_current, (unsigned)at);
    exit(2);
}

static sigjmp_buf escape;

static void runaway(int sig)
{
    (void)sig;
    siglongjmp(escape, 1);
}

/* a case that runs away (a loop of 65,536 on a random input) is skipped */
#define GUARD if (sigsetjmp(escape, 1)) { skipped++; alarm(0); continue; } alarm(5)

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

/* A data bank the routines run in: the game's work RAM banks (the ones
   its variables are kept in; $7F would be another part of the RAM). */
static uint8_t pick_bank(void)
{
    static const uint8_t banks[] = {0x00, 0x7E};

    return banks[rnd() % 2];
}

/* Random RAM and video state, the S-CPU's registers random (widths and
   flags as the routine's entry wants them, set by the case). */
static void fresh(uint8_t db)
{
    size_t i;

    for (i = 0; i < sizeof wram; i++)
        wram[i] = (uint8_t)rnd();
    for (i = 0; i < sizeof pakram; i++)
        pakram[i] = (uint8_t)rnd();
    ppu_reset();
    for (i = 0; i < sizeof ppu.oam; i++)
        ppu.oam[i] = (uint8_t)rnd();
    ppu_to_port();
    video_registers_reset();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = db;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I));
}

/* Runs the decomp's routine on the state; the RAM it leaves is kept as
   old_w and old_p, the state itself put back for the port's run. */
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
    old_ppu = ppu;
}

/* The poll counts the original keeps while it waits for a program
   ($FE-$FF, $1F21-$1F22), and the programs' stack in the pak RAM
   ($0240-$029F), are the Super FX code's own. */
static int counted(size_t i)
{
    return i == 0xFE || i == 0xFF || i == 0x1F21 || i == 0x1F22;
}

/* Work RAM from $0100 up and the pak RAM, and the sprite hardware: the
   model's sprite memory against the port's sprite table. */
static void same(void)
{
    size_t i;
    char what[160];

    cases++;
    fx_pixels_flush();
    alarm(0);
    for (i = 0x100; i < sizeof wram; i++)
        if (wram[i] != old_w[i] && !counted(i)) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i] && !(i >= 0x0240 && i < 0x02A0)) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X (was %02X)", i, pakram[i], old_p[i], start_p[i]);
            return;
        }
    if (ppu_differs(what, sizeof what))
        fail("%s", what);
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: oam_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);
    model_gsu_start = old_start;
    signal(SIGALRM, runaway);

    CASE("OamHideRange") {
        uint16_t offset = (uint16_t)(rnd() % 0x200), count = (uint16_t)(1 + rnd() % 0x30);

        fresh(pick_bank());
        old_cpu.x = offset, old_cpu.y = count;
        old_run(old_abi_OamHideRange);
        oam_hide_range(offset, count);
        same();
    }
    CASE("OamWriteSprites36To46") {
        fresh(pick_bank());
        old_run(old_abi_OamWriteSprites36To46);
        oam_write_sprites_36_to_46();
        same();
    }
    CASE("OamBufferClear") {
        fresh(pick_bank());
        old_run(old_abi_OamBufferClear);
        oam_buffer_clear();
        same();
    }
    CASE("OamBufferHideLarge / Long") {
        int w = (int)(rnd() % 2);

        fresh(pick_bank());
        if (w) {
            old_run(old_abi_OamBufferHideLargeLong);
            oam_buffer_hide_large_long();
        } else {
            old_run(old_abi_OamBufferHideLarge);
            oam_buffer_hide_large();
        }
        same();
    }
    CASE("OamFromList") {
        uint16_t x = (uint16_t)(rnd() % 0x1F0), y = (uint16_t)(rnd() % 0x40);

        fresh(0x07);                    /* the list is in the game's bank $07 */
        old_cpu.x = x, old_cpu.y = y;
        old_run(old_abi_OamFromList);
        oam_from_list(0x07, &x, &y);
        same();
    }
    CASE("OamSprite0AddY") {
        uint8_t bank = pick_bank(), lines = (uint8_t)rnd();

        fresh(bank);
        old_cpu.a = lines;
        old_run(old_abi_OamSprite0AddY);
        oam_sprite0_add_y(bank, lines);
        same();
    }
    CASE("OamAttr36x4 / 38x6 / 38x7 / 36x6 / 38x4 / 36x7") {
        static const struct { void (*f)(void); uint16_t count; uint8_t attr; } r[] = {
            {old_abi_OamAttr36x4, 4, 0x36}, {old_abi_OamAttr38x6, 6, 0x38}, {old_abi_OamAttr38x7, 7, 0x38},
            {old_abi_OamAttr36x6, 6, 0x36}, {old_abi_OamAttr38x4, 4, 0x38}, {old_abi_OamAttr36x7, 7, 0x36}};
        int w = (int)(rnd() % 6);
        uint8_t bank = pick_bank();
        uint16_t first = (uint16_t)(rnd() % 0x80);

        fresh(bank);
        old_cpu.a = first;
        old_run(r[w].f);
        oam_attr_fill(bank, first, r[w].count, r[w].attr);
        same();
    }
    CASE("OamHideEntries3And9 / 0To2") {
        int w = (int)(rnd() % 2);
        uint8_t bank = pick_bank();
        uint16_t y = (uint16_t)(rnd() % 0x1E0);

        fresh(bank);
        old_cpu.y = y;
        if (w) {
            old_run(old_abi_OamHideEntries0To2);
            oam_hide_entries_0_to_2(bank, y);
        } else {
            old_run(old_abi_OamHideEntries3And9);
            oam_hide_entries_3_and_9(bank, y);
        }
        same();
    }
    CASE("OamSetPalette") {
        uint8_t bank = pick_bank(), bits = (uint8_t)rnd();
        uint16_t count = (uint16_t)(1 + rnd() % 0x30), y = (uint16_t)(rnd() % 0x180);

        fresh(bank);
        old_cpu.a = bits, old_cpu.x = count, old_cpu.y = y;
        old_run(old_abi_OamSetPalette);
        oam_set_palette(bank, bits, count, &y);
        same();
    }
    CASE("OamBlock59Left / 123Left / 52Left / 59Right / 123Right / 52Right") {
        static void (*const f[6])(void) = {old_abi_OamBlock59Left, old_abi_OamBlock123Left, old_abi_OamBlock52Left,
                                           old_abi_OamBlock59Right, old_abi_OamBlock123Right, old_abi_OamBlock52Right};
        int w = (int)(rnd() % 6);
        uint8_t bank = pick_bank();

        fresh(bank);
        old_run(f[w]);
        switch (w) {
        case 0: oam_block_59_left(bank); break;
        case 1: oam_block_123_left(bank); break;
        case 2: oam_block_52_left(bank); break;
        case 3: oam_block_59_right(bank); break;
        case 4: oam_block_123_right(bank); break;
        default: oam_block_52_right(bank); break;
        }
        same();
    }
    CASE("OamColumn8") {
        uint8_t bank = pick_bank();
        uint16_t x = (uint16_t)(rnd() % 0x1C0), pos = (uint16_t)rnd(), tile = (uint16_t)rnd();

        fresh(bank);
        old_cpu.x = x;
        W16(0x00F1) = pos;              /* the column's inputs, in direct page */
        W16(0x00F3) = tile;
        old_run(old_abi_OamColumn8);
        oam_column8(bank, &x, pos, tile);
        same();
    }
    CASE("OamHighBits58 / OamHighBits18 / Park112 / Park48") {
        int w = (int)(rnd() % 4);
        uint8_t bank = pick_bank();
        static void (*const f[4])(void) = {old_abi_OamHighBits58, old_abi_OamHighBits18,
                                           old_abi_OamHighBits58Park112, old_abi_OamHighBits18Park48};

        fresh(bank);
        old_run(f[w]);
        switch (w) {
        case 0: oam_high_bits_58(bank); break;
        case 1: oam_high_bits_18(bank); break;
        case 2: oam_high_bits_58_park_112(bank); break;
        default: oam_high_bits_18_park_48(bank); break;
        }
        same();
    }
    CASE("OamPaletteFromTableE30F / E26A / E26AByMode / D75 / E28D") {
        int w = (int)(rnd() % 5);
        uint8_t bank = pick_bank();
        uint16_t y;
        static void (*const f[5])(void) = {old_abi_OamPaletteFromTableE30F, old_abi_OamPaletteFromTableE26A,
                                           old_abi_OamPaletteFromTableE26AByMode, old_abi_OamPaletteForD75,
                                           old_abi_OamPalettesE28D};

        fresh(bank);
        /* the sprite entries the game keeps here, small: */
        W16(0x0D79) = (uint16_t)(rnd() % 0x70);
        W16(0x0D7B) = (uint16_t)(rnd() % 0x70);
        W16(0x0D75) = (uint16_t)(rnd() % 0x70);
        W16(0x10D8) = (uint16_t)(rnd() & 1 ? 0 : rnd() % 0x40);
        W8(0x0D48) = (uint8_t)rnd();
        W8(0x0D89) = (uint8_t)rnd();
        W8(0x0D8A) = (uint8_t)rnd();
        old_run(f[w]);
        switch (w) {
        case 0: oam_palette_from_table_e30f(bank, &y); break;
        case 1: oam_palette_from_table_e26a(bank, &y); break;
        case 2: oam_palette_from_table_e26a_by_mode(bank, &y); break;
        case 3: oam_palette_for_d75(bank, &y); break;
        default: oam_palettes_e28d(bank, &y); break;
        }
        same();
    }
    CASE("OamHide36And96") {
        uint8_t bank = pick_bank();

        fresh(bank);
        old_run(old_abi_OamHide36And96);
        oam_hide_36_and_96(bank);
        same();
    }
    CASE("OamHide75Up") {
        uint8_t bank = pick_bank();

        fresh(bank);
        W8(0x0D2B) = (uint8_t)(rnd() & 1 ? 0 : rnd() & 3);
        old_run(old_abi_OamHide75Up);
        oam_hide_75_up(bank);
        same();
    }
    CASE("OamLargeColumns") {
        uint8_t bank = pick_bank();

        fresh(bank);
        old_run(old_abi_OamLargeColumns);
        oam_large_columns(bank);
        same();
    }
    CASE("OamPlaceRows75And112") {
        uint8_t bank = pick_bank();

        fresh(bank);
        W8(0x10CD) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        old_run(old_abi_OamPlaceRows75And112);
        oam_place_rows_75_and_112(bank);
        same();
    }
    CASE("MetaspriteDraw") {
        uint8_t bank = pick_bank();
        uint16_t group = (uint16_t)(rnd() % 0x20), slot = (uint16_t)(rnd() % 0x80), end;
        uint16_t a;

        fresh(bank);
        W8(0x0D3B) = (uint8_t)rnd();
        W8(0x0D3C) = (uint8_t)rnd();
        old_cpu.x = group, old_cpu.y = slot;
        old_cpu.p &= (uint8_t)~P_X;
        old_run(old_abi_MetaspriteDraw);
        a = metasprite_draw(bank, group, slot, &end);
        if (a != old_cpu.a || end != old_cpu.y)
            fail("A %04X and Y %04X, the decomp's %04X %04X", a, end, (unsigned)old_cpu.a, (unsigned)old_cpu.y);
        same();
    }
    CASE("FlashingMetaspriteDraw / 0 / ByMode") {
        int w = (int)(rnd() % 3);
        uint8_t bank = pick_bank();
        uint16_t x, y, a;
        static void (*const f[3])(void) = {old_abi_FlashingMetaspriteDraw, old_abi_FlashingMetaspriteDraw0,
                                           old_abi_FlashingMetaspriteDrawByMode};

        fresh(bank);
        W16(0x0D37) = (uint16_t)(rnd() & 3 ? rnd() : 0);      /* the position, mostly set */
        W16(0x0D39) = (uint16_t)(rnd() % 3);
        W16(0x0E3B) = (uint16_t)rnd();
        W16(0x0D4C) = (uint16_t)rnd();
        old_cpu.p &= (uint8_t)~P_X;
        x = old_cpu.x, y = old_cpu.y;   /* the X and Y a not-drawn frame leaves */
        old_run(f[w]);
        switch (w) {
        case 0: a = flashing_metasprite_draw(bank, &x, &y); break;
        case 1: a = flashing_metasprite_draw0(bank, &x, &y); break;
        default: a = flashing_metasprite_draw_by_mode(bank, &x, &y); break;
        }
        if (a != old_cpu.a || x != old_cpu.x || y != old_cpu.y)
            fail("A %04X X %04X Y %04X, the decomp's %04X %04X %04X", a, x, y,
                 (unsigned)old_cpu.a, (unsigned)old_cpu.x, (unsigned)old_cpu.y);
        same();
    }
    CASE("SpritePieces3") {
        uint8_t bank = pick_bank();
        uint16_t slot = (uint16_t)(rnd() % 0x1E0), set, x;
        uint8_t attr;

        fresh(bank);
        W16(0x10BB) = (uint16_t)(rnd() % 4);    /* the selector: 0-3 (its table) */
        W16(0x10C3) = (uint16_t)(rnd() % 9);    /* the set: 0-8 with it, the table's sets */
        W8(0x10BD) = (uint8_t)rnd();
        W8(0x10BF) = (uint8_t)rnd();
        W8(0x10C1) = (uint8_t)rnd();
        W8(0x10C5) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        old_cpu.y = slot;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        old_run(old_abi_SpritePieces3);
        attr = sprite_pieces3(bank, &x, &slot, &set);
        if ((old_cpu.a & 0xFF) != attr || slot != old_cpu.y)
            fail("attribute %02X, Y %04X, the decomp's %04X %04X", attr, slot,
                 (unsigned)(old_cpu.a & 0xFF), (unsigned)old_cpu.y);
        same();
    }
    CASE("SpritePiecesAt (left, right, 46 and 62 rows), CarAt51") {
        static const struct { void (*f)(void); uint16_t x, y; } r[] = {
            {old_abi_SpritePiecesAtLeft, 0x5D, 0x7E}, {old_abi_SpritePiecesAtRight, 0xA5, 0x7E},
            {old_abi_SpritePiecesAt46Y71, 0x46, 0x71}, {old_abi_SpritePiecesAt46Y82, 0x46, 0x82},
            {old_abi_SpritePiecesAt46Y93, 0x46, 0x93}, {old_abi_SpritePiecesAt62Y71, 0x62, 0x71},
            {old_abi_SpritePiecesAt62Y83, 0x62, 0x83}, {old_abi_SpritePiecesAt62Y95, 0x62, 0x95}};
        int w = (int)(rnd() % 9);
        uint8_t bank = pick_bank();
        uint16_t slot = (uint16_t)(rnd() % 0x1E0), set = (uint16_t)rnd();
        uint8_t attr;

        fresh(bank);
        W16(0x10BB) = (uint16_t)(rnd() % 4);
        W8(0x10C5) = (uint8_t)(rnd() & 1 ? 0 : rnd());
        W8(0x10C1) = (uint8_t)rnd();
        set = (uint16_t)(rnd() % 9);
        old_cpu.a = set, old_cpu.y = slot;
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        if (w == 8)
            set = (uint16_t)(rnd() % 4), old_cpu.a = set;   /* a car: 0-3 */
        if (w < 8) {
            old_run(r[w].f);
            attr = sprite_pieces_at(bank, set, r[w].x, r[w].y, &slot);
        } else {
            old_run(old_abi_SpritePiecesCarAt51);
            attr = sprite_piece_car(bank, set, &slot);
        }
        if ((old_cpu.a & 0xFF) != attr || slot != old_cpu.y)
            fail("attribute %02X, Y %04X, the decomp's %04X %04X", attr, slot,
                 (unsigned)(old_cpu.a & 0xFF), (unsigned)old_cpu.y);
        same();
    }
    CASE("SpritePiecesTwoCars / Top") {
        int w = (int)(rnd() % 2);
        uint8_t bank = pick_bank();

        fresh(bank);
        W16(0x0E3B) = (uint16_t)rnd();
        W8(0x10BB) = (uint8_t)(rnd() % 4);
        W8(0x0D89) = (uint8_t)(rnd() % 4);
        W8(0x0D8A) = (uint8_t)(rnd() % 4);
        old_cpu.p &= (uint8_t)~(P_M | P_X);
        if (w) {
            old_run(old_abi_SpritePiecesTwoCarsTop);
            sprite_pieces_two_cars_top(bank);
        } else {
            old_run(old_abi_SpritePiecesTwoCars);
            sprite_pieces_two_cars(bank);
        }
        same();
    }
    CASE("OamFillEvery4") {
        uint8_t bank = pick_bank(), v = (uint8_t)rnd();
        uint16_t x = (uint16_t)(rnd() % 0x1C0), count = (uint16_t)(1 + rnd() % 0x20);

        fresh(bank);
        old_cpu.a = v, old_cpu.x = x, old_cpu.y = count;
        old_cpu.p &= (uint8_t)~P_X;
        old_run(old_abi_OamFillEvery4);
        oam_fill_every4(bank, v, x, count);
        same();
    }
    CASE("OamWordRow5") {
        fresh(0x07);                    /* the word's table is in bank $07 */
        old_run(old_abi_OamWordRow5);
        oam_word_row5(0x07);
        same();
    }
    CASE("OamHideForHudMode") {
        uint8_t bank = pick_bank();

        fresh(bank);
        W8(0x0D2B) = (uint8_t)(rnd() % 3);
        old_run(old_abi_OamHideForHudMode);
        oam_hide_for_hud_mode(bank);
        same();
    }
    CASE("OverlaySpritesStep") {
        fresh(pick_bank());
        W8(0x10A2) = (uint8_t)(rnd() % 8);                  /* the overlay's state */
        W16(0x10A4) = (uint16_t)(rnd() & 1 ? rnd() : (rnd() % 0x300) - 0x180);
        W16(0x10A0) = (uint16_t)(rnd() & 3 ? rom_word(0x04E066 + 2 * (rnd() % 3)) : rnd());
        W16(0x10A8) = (uint16_t)(rnd() & 1 ? 0 : 0x0040);
        W8(0x0E3B) = (uint8_t)rnd();
        old_run(old_abi_OverlaySpritesStep);
        overlay_sprites_step();
        same();
    }
    CASE("OverlaySpritesState0") {
        uint8_t bank = pick_bank();

        fresh(bank);
        old_cpu.s = 0x1F00;
        wram[0x1F01] = (uint8_t)rnd();  /* the data bank and P the routine pulls */
        wram[0x1F02] = (uint8_t)rnd();
        W8(0x0E3B) = (uint8_t)rnd();
        old_run(old_abi_OverlaySpritesState0);
        overlay_sprites_state0();
        same();
    }

    CASE("PictureTurnStep") {
        fresh(0x07);
        W8(0x0D32) = 0;                 /* the vertical blank's wait is over */
        W8(0x0D31) = (uint8_t)(rnd() % 4 ? 0 : 1 + rnd() % 5);
        W16(0x0D1E) = (uint16_t)rnd();  /* the buttons */
        W16(0x0309) = (uint16_t)(rnd() & 1 ? 0x2000 : rnd());
        W16(0x0D33) = (uint16_t)(rnd() % 8);
        W16(0x0D7F) = (uint16_t)(rnd() & 1 ? 0 : 1);
        GUARD;
        old_run(old_abi_PictureTurnStep);
        picture_turn_step();
        same();
    }

    printf("%ld cases, %ld differ, %ld skipped\n", cases, failed, skipped);
    return failed != 0;
}
