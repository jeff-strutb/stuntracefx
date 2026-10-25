/*
 * sprite_bars.c: the HUD's bar gauges, strips of sprites with a marker.
 *
 * A position is the sprite table's X and Y as one word, X in the low byte;
 * the game steps it as a word, so an X past 255 carries into Y, as the
 * original's does.
 */
#include "game.h"

uint8_t sprite_y_on_screen(uint8_t y, bool *hidden)
{
    *hidden = y >= 0xB0;
    return *hidden ? 0xF0 : y;
}

/* n sprites from `pos`, `step` apart, with the tile words from `tiles`. */
static void sprite_strip(const w16 *tiles, uint16_t pos, uint16_t step, unsigned n, uint16_t sprite)
{
    unsigned i;

    for (i = 0; i < n; i++) {
        OAM_POS(sprite) = pos;
        OAM_TILEW(sprite) = tiles[i];
        pos = (uint16_t)(pos + step);
        sprite = (uint16_t)(sprite + 4);
    }
}

/* The marker in the slot before the strip, at the value along it; past the
   first sprite's span it moves back by the span with the far tile. */
static void bar_marker(uint16_t value, uint16_t pos, uint16_t span, uint16_t far, uint16_t near, uint16_t sprite)
{
    if (value >= span) {
        OAM_POS(sprite) = (uint16_t)(value + pos - span);
        OAM_TILEW(sprite) = far;
    } else {
        OAM_POS(sprite) = (uint16_t)(value + pos);
        OAM_TILEW(sprite) = near;
    }
}

/* Four sprites 16 apart, the value 0-63 (more shows 63); the tile set is
   the value / 16 (eight bytes a set). */
void sprite_bar4(uint8_t value, uint16_t pos, uint16_t sprite)
{
    uint16_t v = value < 0x3F ? value : 0x3F;

    sprite_strip(&strip4_tiles[(v >> 1 & 0xF8) / 2], pos, 0x10, 4, (uint16_t)(sprite + 4));
    bar_marker(v, pos, 0x10, 0x3C20, 0x3622, sprite);
}

/* Six sprites 8 apart; the tile set is bits 3-5 of the value (twelve
   bytes a set, as the original indexes them: sixteen apart). */
void sprite_bar6(uint8_t value, uint16_t pos, uint16_t sprite)
{
    sprite_strip(&strip6_tiles[((value & 0x38) << 1) / 2], pos, 0x08, 6, (uint16_t)(sprite + 4));
    bar_marker(value, pos, 0x08, 0x3C1F, 0x3C1E, sprite);
}
