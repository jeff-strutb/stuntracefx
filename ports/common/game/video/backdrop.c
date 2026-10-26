/*
 * backdrop.c: the race screen's backdrop (layer 2), scrolled with the view.
 * Each half of the screen (the half is $0D7F, 0 or 1) has its own scroll
 * pair: across at $0D92 and down at $0D8E, both per half, and the raster
 * value at $0D96. The view's angle is at $06CD and its heading at $06CF.
 */
#include "game.h"
#include "smallvideo.h"

#define half_select    W8(0x0D7F)           /* the half of the screen: 0 or 1 */
#define view_angle     W16(0x06CD)
#define view_heading   W16(0x06CF)
#define backdrop_across(x)  W16((uint16_t)(0x0D92 + (x)))
#define backdrop_down(x)    W16((uint16_t)(0x0D8E + (x)))
#define backdrop_raster(x)  W16((uint16_t)(0x0D96 + (x)))
#define BACKDROP_Y_BASE 0xE920u             /* the vertical base, a word per half, in the code the game runs from work RAM */

/* Whether the vehicle of player `player` (0, or any other value for player
   2) is raised: the vehicle's object links to the next object twice, and
   bit 14 of the word at the object's offset in bank $00 says so. */
uint16_t player_vehicle_raised(uint8_t player)
{
    uint16_t obj = W16(player ? 0x034A : 0x0346);

    obj = W16(0x204F + obj);
    obj = W16(0x204F + obj);
    return (W16(obj) & 0x4000) ? 0x0008 : 0x0000;
}

/* The scroll for the current half: across is the view's heading / 4; the
   raster value is the view's angle + $18 (+ 8 when the vehicle is raised),
   doubled, plus $EF99 and capped at $F257; down is that angle plus the
   half's base. Returns the down scroll. */
uint16_t backdrop_scroll_from_view(void)
{
    uint16_t x = (uint16_t)(half_select << 1);      /* the half's word offset */
    uint16_t h, t;
    uint32_t s;

    backdrop_across(x) = view_heading >> 2;
    h = (uint16_t)(player_vehicle_raised((uint8_t)x) + view_angle + 0x0018);
    s = (uint32_t)(uint16_t)(h << 1) + 0xEF99u + (h >> 15);
    t = (uint16_t)s;
    if (t >= 0xF257)
        t = 0xF257;
    backdrop_raster(x) = t;
    h = (uint16_t)(h + ram_code_word((uint16_t)(BACKDROP_Y_BASE + x)));
    backdrop_down(x) = h;
    return h;
}
