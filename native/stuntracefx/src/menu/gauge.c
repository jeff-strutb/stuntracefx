/*
 * gauge.c: the speed gauges: from a held button in the menus and the
 * attract, from the car's speed in a race.
 */
#include "data/cart.h"
#include "menu/menu.h"
#include "object/object.h"
#include "system/chip.h"
#include "system/game.h"
#include "system/state.h"

static uint16_t menu_gauge;             /* $117E */
static uint16_t gauge2 = 0;             /* $1180 */

/* The bytes of the gauge table for the kind of the vehicle object $0346,
   by the handler word at +$27 of it. */
static uint16_t gauge_table_index(uint16_t value)
{
    static const uint16_t kinds[5] = {0x81B9, 0x853D, 0x8411, 0x82E5, 0x8669};
    static const uint16_t offsets[5] = {0x0000, 0x0098, 0x0130, 0x01C8, 0x0260};
    uint16_t kind = obj_at(g.player_car)->link27;
    int i;

    for (i = 0; i < 5; i++)
        if (kind == kinds[i])
            return (uint16_t)(value + offsets[i]);
    return value;
}

/*
 * @routine 7F:11E9 GaugeFromSpeed
 * WHAT IT DOES: The speed at Super FX RAM $4A + offset (made positive and
 * halved when negative), capped and divided by 4, through the table at
 * $0B:B158; 0 stays 0.
 */
static uint16_t gauge_from_speed(uint16_t offset)
{
    uint16_t a;

    if (!offset)
        return 0;
    a = fx16((uint16_t)(0x4A + offset));
    if (a & 0x8000)
        a = (uint16_t)((uint16_t)(~a + 1) >> 1);
    if (a >= 0x0257)
        a = 0x0257;
    a = (uint16_t)(a >> 2);
    return cart8(0x0B, (uint16_t)(0xB158 + gauge_table_index(a)));
}

/*
 * @routine 7F:112F SpeedGaugeUpdate
 * @routine 7F:11AC MenuGaugeFromButtonB
 * @routine 7F:1177 MenuGaugeSlideToTarget
 * WHAT IT DOES: The first gauge, at $0E49: from the speed, sliding toward it
 * below phase 6; with $0D53 set it follows button B, down when released.
 */
void SpeedGaugeUpdate(uint16_t offset)
{
    uint16_t v;

    if (g.freeze) {
        if (g.player_phase[0] >= 6) {
            v = gauge_from_speed(offset);
        } else if (frame_state.held[0] & PAD_B) {
            menu_gauge = (uint16_t)(menu_gauge + 0x28);
            if (menu_gauge >= 0xFF)
                menu_gauge = 0xFF;
            v = menu_gauge;
        } else {
            if (menu_gauge) {
                menu_gauge = (uint16_t)(menu_gauge - 0x0C);
                if (menu_gauge & 0x8000)
                    menu_gauge = 0;
            }
            v = menu_gauge;
        }
    } else {
        v = gauge_from_speed(offset);
        if (g.player_phase[0] < 6 && !(menu_gauge & 0x8000) && menu_gauge != v) {
            int down = menu_gauge >= v;

            menu_gauge = (uint16_t)(down ? menu_gauge - 8 : menu_gauge + 4);
            if ((uint16_t)(menu_gauge - v) & 0x8000)
                menu_gauge = 0xFF00;
            else
                v = menu_gauge;
        } else if (g.player_phase[0] < 6 && !(menu_gauge & 0x8000)) {
            menu_gauge = 0xFF00;
        }
    }
    g.speed_gauge[0] = (uint8_t)v;
}

/*
 * @routine 7F:0BEF SpeedGauge2Update
 * WHAT IT DOES: The second gauge, at $0E47: with $0D53 set it follows pad
 * 2's B button; otherwise it steps 4 toward a target from the Super FX
 * values, showing the target once within 4.
 */
void SpeedGauge2Update(uint16_t offset)
{
    uint16_t t, v;

    if (g.freeze) {
        if (frame_state.held[1] & PAD_B) {
            gauge2 = (uint16_t)(gauge2 + 0x10);
            if (gauge2 >= 0x7F)
                gauge2 = 0x7F;
        } else if (gauge2) {
            gauge2 = (uint16_t)(gauge2 - 6);
            if (gauge2 & 0x8000)
                gauge2 = 0;
        }
        g.speed_gauge[1] = (uint8_t)gauge2;
        return;
    }
    if (!offset) {
        g.speed_gauge[1] = 0;
        return;
    }
    v = fx16((uint16_t)(0x4A + offset));
    t = (v & 0x8000) ? 0 : (uint16_t)(v >> 2);
    if ((uint16_t)(fx16((uint16_t)(0x44 + offset)) - 4) & 0x8000) {
        t = (uint16_t)((fx16((uint16_t)(0x50 + offset)) >> 3) + t);
    } else {
        t = (uint16_t)((v & 0x8000) | v >> 1);
        if (t & 0x8000)
            t = (uint16_t)(~t + 1);
    }
    if (t >= 0xFF)
        t = 0xFF;
    if (gauge2 & 0x8000) {
        g.speed_gauge[1] = (uint8_t)t;
        return;
    }
    if (gauge2 != t) {
        gauge2 = (uint16_t)(gauge2 > t ? gauge2 - 4 : gauge2 + 4);
        v = (uint16_t)(gauge2 - t + 4);
        if ((v & 0x8000) || v >= 8) {
            g.speed_gauge[1] = (uint8_t)gauge2;
            return;
        }
    }
    gauge2 = 0xFFFF;
    g.speed_gauge[1] = (uint8_t)t;
}
