/* old: video/sprite_text.c
 *
 * sprite_text_check.c: the port's text-as-sprites (video/sprite_text.c)
 * against the decomp's, on random states: work RAM and the game pak RAM
 * random, the line number (the word at $0E3D) one of the 128 lines the
 * ROM's table holds, the direct page's inputs of each routine set as the
 * routine reads them. The lines themselves are the real ROM's. Each case
 * runs the decomp's routine on a copy and the port's on the state, and
 * compares both RAMs but the direct page; the X and Y the line leaves are
 * compared too.
 *
 *   check/check.sh sprite_text N
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
#include "sprite_text.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(OamTextLine) OLD(OamTextLineLong)
OLD(SpriteTextAt895E) OLD(SpriteTextAt8152) OLD(SpriteTextAt8152B) OLD(SpriteTextAt8952)
OLD(SpriteTextAt8152C) OLD(SpriteTextAt8952B)

static uint64_t rng = 0x2F8A1C6D39B4E075ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

static uint8_t start_w[sizeof wram], start_p[sizeof pakram], old_w[sizeof wram], old_p[sizeof pakram];
static long cases, failed, iteration;

/* The data banks the routines are called with: the ones the game draws
   text from, and its work RAM. */
static const uint8_t banks[] = { 0x00, 0x02, 0x07, 0x7E };

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

/* Random RAM. The line number is one of the table's lines whose pointer
   lies in the ROM, below $FD00: the table's other entries point into work
   RAM or to addresses the bus does not answer, and a pointer near the
   bank's end runs on past it (a line of spaces, for one) into the direct
   page, which the decomp reads as text while it changes it. The game draws
   none of these. The number goes in $0E3D of bank $00 and $7E (the banks
   the routines read it in). */
static void fresh(void)
{
    size_t k;
    unsigned n;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    do
        n = rnd() & 0x7F;
    while (rom_word(0x078000u + 2 * n) < 0x8000 || rom_word(0x078000u + 2 * n) > 0xFD00);
    W8(0x0E3D) = (uint8_t)n;
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.p = P_M;                    /* X and Y 16-bit: the results whole */
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

static void same(void)
{
    size_t i;

    cases++;
    for (i = 0x100; i < sizeof wram; i++)
        if (wram[i] != old_w[i]) {
            fail("work RAM $%05zX = %02X, the decomp's %02X (was %02X)", i, wram[i], old_w[i], start_w[i]);
            return;
        }
    for (i = 0; i < sizeof pakram; i++)
        if (pakram[i] != old_p[i]) {
            fail("game pak RAM $%04zX = %02X, the decomp's %02X", i, pakram[i], old_p[i]);
            return;
        }
}

#define CASE(name) model_current = name; for (i = 0; i < n && (!getenv("ONLY") || !strcmp(getenv("ONLY"), name)) && (iteration = i, 1); i++)

int main(int argc, char **argv)
{
    long n = argc > 2 ? atol(argv[2]) : 2000, i;

    if (argc < 2) {
        fprintf(stderr, "usage: sprite_text_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("OamTextLine") {
        uint8_t bank = banks[rnd() % 4];
        TextSprites t;
        uint16_t ex, ey;

        fresh();
        t.first_sprite = (uint8_t)(rnd() & 0x7F);   /* the sprite slots: the table's 128 */
        t.centre_x = (uint8_t)rnd();
        t.y = (uint8_t)rnd();
        t.tile_base = (uint8_t)rnd();
        t.attr = (uint8_t)rnd();
        wram[0x00F2] = t.first_sprite;      /* the routine's inputs, direct page */
        wram[0x00F3] = t.centre_x;
        wram[0x00F4] = t.y;
        wram[0x00F1] = t.tile_base;
        wram[0x00F5] = t.attr;
        old_cpu.db = bank;
        old_run(old_abi_OamTextLine);
        text_line_sprites(bank, &t, &ex, &ey);
        if (ex != old_cpu.x || ey != old_cpu.y)
            fail("end X/Y $%04X/$%04X, the decomp's $%04X/$%04X (bank $%02X)", ex, ey,
                 (unsigned)old_cpu.x, (unsigned)old_cpu.y, bank);
        same();
    }
    CASE("OamTextLineLong") {
        TextSprites t;
        uint16_t ex, ey;

        fresh();
        t.first_sprite = (uint8_t)(rnd() & 0x7F);
        t.centre_x = (uint8_t)rnd();
        t.y = (uint8_t)rnd();
        t.tile_base = (uint8_t)rnd();
        t.attr = (uint8_t)rnd();
        wram[0x00F2] = t.first_sprite;
        wram[0x00F3] = t.centre_x;
        wram[0x00F4] = t.y;
        wram[0x00F1] = t.tile_base;
        wram[0x00F5] = t.attr;
        old_cpu.db = banks[rnd() % 4];      /* the routine sets bank $07 itself */
        old_run(old_abi_OamTextLineLong);
        text_line_sprites_far(&t, &ex, &ey);
        if (ex != old_cpu.x || ey != old_cpu.y)
            fail("end X/Y $%04X/$%04X, the decomp's $%04X/$%04X", ex, ey,
                 (unsigned)old_cpu.x, (unsigned)old_cpu.y);
        same();
    }
    CASE("SpriteTextAt895E / 8152 / 8952 and their copies") {
        int k = (int)(rnd() % 6);
        uint8_t bank = banks[rnd() % 4];

        fresh();
        old_cpu.db = bank;
        old_run(k == 0 ? old_abi_SpriteTextAt895E : k == 1 ? old_abi_SpriteTextAt8152
                : k == 2 ? old_abi_SpriteTextAt8152B : k == 3 ? old_abi_SpriteTextAt8952
                : k == 4 ? old_abi_SpriteTextAt8152C : old_abi_SpriteTextAt8952B);
        switch (k) {
        case 0: sprite_text_at_89_5e(bank); break;
        case 1: sprite_text_at_81_52(bank); break;
        case 2: sprite_text_at_81_52_copy_b(bank); break;
        case 3: sprite_text_at_89_52(bank); break;
        case 4: sprite_text_at_81_52_copy_c(bank); break;
        default: sprite_text_at_89_52_copy_b(bank); break;
        }
        same();
    }
    printf("sprite_text_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
