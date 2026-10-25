/*
 * gauges.c: each HUD panel's two bar gauges, a four-sprite and a
 * six-sprite one, at the positions the panels keep.
 */
#include "game.h"

static uint16_t pos(uint8_t x, uint8_t y)
{
    return (uint16_t)(y << 8 | x);
}

void hud_gauges_panel_a(void)
{
    sprite_bar4(gauge_a4_value, pos(gauge_a4_x, gauge_a4_y), 0x140);
    sprite_bar6(gauge_a6_value, pos(gauge_a6_x, gauge_a6_y), 0x16C);
}

/* Only while panel B shows or the screen is split; split, its sprites are
   in the other half's slots. */
void hud_gauges_panel_b(void)
{
    if (!panel_b_shown && !wram_0D62)
        return;
    sprite_bar4(gauge_b4_value, pos(gauge_b4_x, gauge_b4_y), wram_0D62 ? 0x040 : 0x100);
    sprite_bar6(gauge_b6_value, pos(gauge_b6_x, gauge_b6_y), wram_0D62 ? 0x06C : 0x12C);
}

/* A six-sprite gauge's Y set and its twelve sprites' X high bits cleared
   in the high table. */
static void gauge6_place(uint8_t *y_of, uint8_t y, unsigned high)
{
    static const uint8_t keep[4] = {0xBF, 0xAA, 0xAA, 0xEA};
    unsigned i;

    *y_of = y;
    for (i = 0; i < 4; i++)
        oam_buffer_high[high + i] &= keep[i];
}

void gauge6_b_place(void) { gauge6_place(&gauge_b6_y, 0x5A, 0x06); }
void gauge6_a_place(void) { gauge6_place(&gauge_a6_y, 0xC2, 0x16); }
