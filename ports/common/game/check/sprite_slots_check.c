/* old: video/sprite_slots.c
 *
 * sprite_slots_check.c: the port's sprite slots (video/sprite_slots.c)
 * against the decomp's, on random states: work RAM and game pak RAM random,
 * then the bytes the routines read held to the values the game uses (the
 * slot count, the palette flag, the levels). Each case runs the decomp's
 * routine on a copy and the port's on the state, and compares both RAMs
 * from $0100 up (the direct page is scratch the routines do not keep).
 *
 *   check/check.sh sprite_slots N
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
#include "sprite_slots.h"
#include "model.h"

#define OLD(name) void old_abi_##name(void);
OLD(SpriteSlotFirst) OLD(SpriteSlotFirstLong) OLD(SpriteSlot1Countdown)
OLD(SpriteSlot2Brighten) OLD(SpriteSlot2BrightenLong) OLD(SpriteSlot2Darken) OLD(SpriteSlot2DarkenLong)

static uint64_t rng = 0x2F1E9B7C4D3A5861ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

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

/* Random RAM, the data bank at work RAM, and the bytes the routines read
   held to the game's values: the slot count is $FF (slot 1 off) or $00 to
   $28; the palette flag is 0 or 1; slot 1's level steps by 2 from $1E; the
   levels of slot 2 are $00 to $1F. */
static void fresh(void)
{
    size_t k;

    for (k = 0; k < sizeof wram; k++)
        wram[k] = (uint8_t)rnd();
    for (k = 0; k < sizeof pakram; k++)
        pakram[k] = (uint8_t)rnd();
    memset(&old_cpu, 0, sizeof old_cpu);
    old_cpu.a = (uint16_t)rnd();
    old_cpu.x = (uint16_t)rnd();
    old_cpu.y = (uint16_t)rnd();
    old_cpu.db = 0x7E;
    old_cpu.p = (uint8_t)(rnd() & (P_N | P_V | P_Z | P_C | P_I | P_M | P_X));
    wram[0x08FB] = rnd() % 8 == 0 ? 0xFF : (uint8_t)(rnd() % 0x29);
    wram[0xFF65] = (uint8_t)(rnd() & 1);
    wram[0x05A4] = (uint8_t)((rnd() % 0x10) * 2);
    wram[0x05AC] = (uint8_t)(rnd() % 0x20);
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
        fprintf(stderr, "usage: sprite_slots_check ROM [N]\n");
        return 2;
    }
    model_load_rom(argv[1]);

    CASE("SpriteSlotFirst / SpriteSlotFirstLong") {
        int k = (int)(rnd() & 1);
        bool carry;

        fresh();
        carry = (old_cpu.p & P_C) != 0;
        old_run(k ? old_abi_SpriteSlotFirstLong : old_abi_SpriteSlotFirst);
        if (k)
            sprite_slot_first_long(carry);
        else
            sprite_slot_first(carry);
        same();
    }
    CASE("SpriteSlot1Countdown") {
        fresh();
        old_run(old_abi_SpriteSlot1Countdown);
        sprite_slot1_countdown();
        same();
    }
    CASE("SpriteSlot2Brighten / SpriteSlot2Darken (and the Long forms)") {
        int k = (int)(rnd() % 4);

        fresh();
        old_run(k == 0 ? old_abi_SpriteSlot2Brighten : k == 1 ? old_abi_SpriteSlot2BrightenLong
                : k == 2 ? old_abi_SpriteSlot2Darken : old_abi_SpriteSlot2DarkenLong);
        switch (k) {
        case 0: sprite_slot2_brighten(); break;
        case 1: sprite_slot2_brighten_long(); break;
        case 2: sprite_slot2_darken(); break;
        default: sprite_slot2_darken_long(); break;
        }
        same();
    }
    printf("sprite_slots_check: %ld cases, %ld differ\n", cases, failed);
    return failed != 0;
}
