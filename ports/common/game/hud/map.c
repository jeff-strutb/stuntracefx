/*
 * map.c: the course map's markers, a sprite for each object in the list,
 * at its world position scaled to the map: x / 2048 and -z / 2048 (each
 * rounded toward minus infinity, kept to a byte) from the map's origin.
 */
#include "game.h"

void map_markers(uint8_t origin_x, uint8_t origin_y, uint16_t sprite, int16_t step)
{
    const w16 *o;

    for (o = object_list; *o; o++) {
        uint8_t x = (uint8_t)(asr16(obj_pos_x(*o), 11) + origin_x);
        uint8_t y = (uint8_t)(asr16((int16_t)(uint16_t)-(uint16_t)obj_pos_z(*o), 11) + origin_y);

        W16((uint16_t)(0x0375 + sprite)) = (uint16_t)(y << 8 | x);
        W16((uint16_t)(0x0377 + sprite)) = map_marker_tiles[obj_26(*o) & 7];
        sprite = (uint16_t)(sprite + step);
    }
}

void map_markers_panel_a(void)
{
    map_markers(map_a_x, map_a_y, 0x158, -4);
}

/* Only while panel B shows or the screen is split; split, upward from the
   other half's slots. */
void map_markers_panel_b(void)
{
    if (!panel_b_shown && !wram_0D62)
        return;
    map_markers(map_b_x, map_b_y, wram_0D62 ? 0x054 : 0x180, 4);
}
