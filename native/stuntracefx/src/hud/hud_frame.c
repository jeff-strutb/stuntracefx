/*
 * hud_frame.c: the HUD's part of a race frame: the digits written into
 * sprites already placed, the gauges, the map markers and the panels that
 * slide on and off the screen.
 *
 * Not converted, nothing reaches it: SlideOutWobble (7E:E190).
 */
#include "audio/sound.h"
#include "fx/fx.h"
#include "hud/hud.h"
#include "object/object.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"
#include "data/cart.h"
#include "video/video.h"

/* A digit's two tiles into two sprites stacked: tile 2d on top, 2d + 1 below. */
static void digit(int sprite, uint8_t d)
{
    OamByteSet((uint16_t)(sprite * 4 + 2), (uint8_t)(d << 1));
    OamByteSet((uint16_t)((sprite + 1) * 4 + 2), (uint8_t)((d << 1) + 1));
}

/*
 * @routine 7E:B00C HudDrawTime
 * WHAT IT DOES: Draws the race time with sprite digits, unless the HUD is
 * blanked; also counts down the HUD's hold timer.
 */
void HudDrawTime(void)
{
    int base;
    uint8_t v;

    if (g.hud_mode != 0x01) {
        if (g.hud_hold != 0) {
            g.hud_hold--;
            if (!g.hud_blank)
                return;
        }
    } else if (!g.hud_blank) {
        return;
    }
    base = g.time_sprite_base / 4;
    v = (uint8_t)g.clock_seconds;
    digit(base + 6, v & 0x0F);
    digit(base + 4, v >> 4);
    v = (uint8_t)(g.clock_fraction >> 8);
    digit(base + 2, v & 0x0F);
    digit(base, v >> 4);
    digit(base + 8, g.clock_minutes & 0x0F);
}

/* @routine 7E:E7D6 HudDrawDigits19EC: the two-digit BCD count, sprites 90 and 88. */
void HudDrawDigits19EC(void)
{
    if (g.hud_blank)
        return;
    digit(90, g.stunt_score_high & 0x0F);
    digit(88, g.stunt_score_high >> 4);
}

/* @routine 7E:E95E HudDrawNumber3: a byte as three digits at sprite `sprite`. */
void HudDrawNumber3(uint8_t n, int sprite)
{
    uint8_t count = 0;

    while (n >= 100) {
        n = (uint8_t)(n - 100);
        count++;
    }
    digit(sprite, count);
    count = 0;
    while (n >= 10) {
        n = (uint8_t)(n - 10);
        count++;
    }
    digit(sprite + 2, count);
    digit(sprite + 4, n);
}

/* @routine 7E:E7AE HudDrawValueA, E7BA HudDrawValueB, E7C6 HudDrawValueC */
void HudDrawValueA(void) { HudDrawNumber3(g.status_level[0], 0x0114 / 4); }
void HudDrawValueB(void) { HudDrawNumber3(g.status_level[0], 0x0028 / 4); }
void HudDrawValueC(void) { HudDrawNumber3(g.status_level[1], 0x0128 / 4); }

/* @routine 7E:E7FC HudDigitSprites62: a digit on sprites 62 and 63. */
void HudDigitSprites62(void)
{
    uint8_t d = g.time_override ? 0x01 : g.player_mode[0];

    OamByteSet(62 * 4 + 2, (uint8_t)(d << 1));
    OamByteSet(63 * 4 + 2, (uint8_t)((d << 1) + 1));
}

/* @routine 09:C072 HudCountUp19EC: an object's flag set, the count up, sound $3C. */
void HudCountUp19EC(Object *o)
{
    uint8_t v = g.stunt_score_high;

    o->flags25 |= 0x01;
    v = (uint8_t)(((v & 0x0F) == 9) ? ((v & 0xF0) + 0x10) : (v + 1));
    if ((v & 0xF0) == 0xA0)
        v = 0;
    g.stunt_score_high = v;
    sound_port0_push(0x3C);
}

/* @routine 09:C0AB HudCountMove19EECopy */
void HudCountMove19EECopy(void)
{
    g.stunt_score_add = (uint8_t)g.hits_bcd;
    g.hits_bcd = 0;
    g.byte_19f2 = 0x01;
}

/* @routine 09:C08E HudCountMove19EE: unless time-up is shown ($0E59): sound $35, the record's lift, the count moved. */
void HudCountMove19EE(uint16_t rec)
{
    if (g.time_up_shown)
        return;
    sound_port0_push(0x35);
    GsuRecordRaise6A(rec, 0x7F);
    GsuRecordLower82(rec, 0x7F);
    HudCountMove19EECopy();
}

/*
 * @routine 7E:EE46 HudGaugesPanelA
 * WHAT IT DOES: Panel A's gauges: a four-sprite bar and a six-sprite bar.
 */
void HudGaugesPanelA(void)
{
    SpriteBar4WithMarker(g.gauge_one[1], (uint16_t)(g.gauge_place[3] << 8 | g.gauge_place[1]), 0x0140);
    SpriteBar6WithMarker(g.kept_enable[1], (uint16_t)(g.gauge_place[7] << 8 | g.gauge_place[5]), 0x016C);
}

/*
 * @routine 7E:EE7B HudGaugesPanelB
 * WHAT IT DOES: When the race is ended or on the split screen, panel B's
 * gauges.
 */
void HudGaugesPanelB(void)
{
    if (!g.race_ended_d55 && !g.split_screen)
        return;
    SpriteBar4WithMarker(g.gauge_one[0], (uint16_t)(g.gauge_place[2] << 8 | g.gauge_place[0]),
                         g.split_screen ? 0x0040 : 0x0100);
    SpriteBar6WithMarker(g.kept_enable[0], (uint16_t)(g.gauge_place[6] << 8 | g.gauge_place[4]),
                         g.split_screen ? 0x006C : 0x012C);
}

/* An arithmetic shift right by 3. */
static uint16_t asr3(uint16_t v)
{
    for (int i = 0; i < 3; i++)
        v = (uint16_t)((v >> 1) | (v & 0x8000));
    return v;
}

/*
 * @routine 7F:0AD6 MapMarkerSprites
 * WHAT IT DOES: A map marker sprite for each object of the drawn list, at
 * its world position scaled down plus the map's origin, the tile by the
 * low bits of its +$26; each next at `step` bytes along the sprite table.
 */
static void map_marker_sprites(uint8_t origin_x, uint8_t origin_y, int16_t step, uint16_t slot)
{
    for (int i = 0; objects.list[i]; i++) {
        const Object *o = obj_at(objects.list[i]);
        uint16_t v, sx;

        v = asr3(o->x);
        sx = (uint8_t)(((uint16_t)(v >> 8 | v << 8) + origin_x) & 0xFF);
        v = asr3((uint16_t)-o->z);
        v = (uint16_t)((uint16_t)(v + (uint16_t)(origin_y << 8)) & 0xFF00);
        OamByteSet((uint16_t)(slot), (uint8_t)sx);
        OamByteSet((uint16_t)(slot + 1), (uint8_t)(v >> 8));
        v = ram_code_7f16((uint16_t)(0x0B46 + 2 * (o->byte26 & 0x07)));
        OamByteSet((uint16_t)(slot + 2), (uint8_t)v);
        OamByteSet((uint16_t)(slot + 3), (uint8_t)(v >> 8));
        slot = (uint16_t)(slot + step);
    }
}

/* @routine 7E:EEF7 MapMarkersPanelA */
void MapMarkersPanelA(void)
{
    map_marker_sprites(g.hud_map_origin_x[1], g.hud_map_y_a, -4, 0x0158);
}

/* @routine 7E:EF16 MapMarkersPanelB */
void MapMarkersPanelB(void)
{
    if (!g.race_ended_d55 && !g.split_screen)
        return;
    map_marker_sprites(g.hud_map_origin_x[0], g.hud_map_y_b, 4, g.split_screen ? 0x0054 : 0x0180);
}

/* @routine 7E:AECE HudSlideIn: the panel 4 further in, stopping at $10. */
void HudSlideIn(void)
{
    uint8_t r = (uint8_t)(g.hud_slide + 4);

    if (!(r & 0x80) && r >= 0x10)
        r = 0x10;
    g.hud_slide = r;
}

/* @routine 7E:AEE0 HudSlideOut */
void HudSlideOut(void)
{
    uint8_t v = (uint8_t)(g.hud_slide - 4);

    if ((v & 0x80) && v < 0xD0)
        v = 0xD0;
    g.hud_slide = v;
}

/* A sprite Y through SpriteYOnScreen; c is the carry it leaves. */
static uint8_t on_screen(uint8_t y, unsigned *c)
{
    *c = y >= 0xB0;
    return SpriteYOnScreen(y);
}

/* The slide plus the panel's offset as the panel's Y, into its sprites' Y
   bytes (`at` table offsets plus the panel's sprite base). */
static void hud_sprites_y(uint8_t y_offset, uint16_t base)
{
    static const uint16_t row[4][2] = {{0x009C, 0x00AC}, {0x00A0, 0x00B0}, {0x00A4, 0x00B4}, {0x00A8, 0x00B8}};
    static const struct { uint8_t add; int clc; uint16_t at; } tail[3] = {
        {0x0A, 0, 0x0090}, {0x18, 0, 0x0094}, {0x26, 1, 0x0098}};
    uint8_t v0 = (uint8_t)(g.hud_slide + y_offset), v = v0, y;
    unsigned c = 0;

    for (int i = 0; i < 4; i++) {
        if (i)
            v = (uint8_t)(v + 0x10);
        y = on_screen(v, &c);
        OamByteSet((uint16_t)(row[i][0] + base), y);
        OamByteSet((uint16_t)(row[i][1] + base), y);
    }
    for (int i = 0; i < 3; i++) {
        if (tail[i].clc)
            c = 0;
        y = on_screen((uint8_t)(v0 + tail[i].add + c), &c);
        OamByteSet((uint16_t)(tail[i].at + base), y);
    }
}

static void hud_panel_slide(uint8_t y_offset, uint16_t base, int panel_a)
{
    if (g.start_light < 0x08)
        return;
    if (panel_a) {
        if (g.start_light == 0x08) {
            HudSlideIn();
        } else {
            if (g.hud_slide == 0xD0)
                g.start_light = 0;
            HudSlideOut();
        }
    }
    hud_sprites_y(y_offset, base);
}

/* @routine 7E:AE18 HudPanelASlide */
void HudPanelASlide(void) { hud_panel_slide(0xF8, 0x0020, 1); }
/* @routine 7E:AE30 HudPanelBSlide */
void HudPanelBSlide(void) { hud_panel_slide(0x4A, 0x0120, 0); }

/* Panel B's Y for its two sprite pairs. */
static void hud_panel_b_y(void)
{
    uint8_t v = (uint8_t)(g.hud_panel_b_y - 0x0D);

    OamByteSet(0x01C0, v);
    OamByteSet(0x01C8, v);
    v = (uint8_t)(v + 0x10);
    OamByteSet(0x01C4, v);
    OamByteSet(0x01CC, v);
}

/* The map marker's Y and the column of Ys below it, $FF once a step carries. */
static void hud_marker_y(void)
{
    static const struct { uint8_t add; uint16_t at[2]; int n; } step[6] = {
        {0x08, {0x01D0, 0}, 1}, {0x06, {0x01E8, 0x01EC}, 2}, {0x08, {0x01F0, 0x01F4}, 2},
        {0x08, {0x01D4, 0}, 1}, {0x08, {0x01D8, 0x01DC}, 2}, {0x08, {0x01E0, 0x01E4}, 2}};
    unsigned sum = g.hud_marker_y;

    OamByteSet(0x01F8, g.hud_marker_y);
    OamByteSet(0x01FC, g.hud_marker_y);
    for (int i = 0; i < 6; i++) {
        unsigned r = (sum & 0xFF) + step[i].add;

        sum = r > 0xFF ? 0xFF : r;
        for (int j = 0; j < step[i].n; j++)
            OamByteSet(step[i].at[j], (uint8_t)sum);
    }
}

/* Gauge B's X and the row of Xs after it: the first five skipped while
   negative, the rest carrying on. */
static void hud_gauge_b_x(void)
{
    static const struct { uint8_t add; int sub, clc, skip; uint16_t at[2]; int n; } step[11] = {
        {0x00, 0, 0, 1, {0x012C, 0}, 1}, {0x08, 1, 0, 1, {0x0148, 0}, 1},
        {0x10, 0, 1, 1, {0x0130, 0}, 1}, {0x08, 0, 1, 1, {0x0134, 0}, 1},
        {0x08, 0, 1, 1, {0x0138, 0}, 1}, {0x08, 0, 0, 0, {0x0140, 0}, 1},
        {0x08, 0, 0, 0, {0x0144, 0x013C}, 2}, {0x08, 0, 0, 0, {0x014C, 0}, 1},
        {0x04, 0, 0, 0, {0x0150, 0}, 1}, {0x08, 0, 0, 0, {0x0154, 0}, 1},
        {0x08, 0, 0, 0, {0x0158, 0}, 1}};
    uint8_t v = g.gauge_place[4];
    unsigned c = 0;

    for (int i = 0; i < 11; i++) {
        if (i) {
            if (step[i].sub) {
                unsigned d = (unsigned)v - step[i].add;

                c = d < 0x100;
                v = (uint8_t)d;
            } else {
                unsigned s;

                if (step[i].clc)
                    c = 0;
                s = (unsigned)v + step[i].add + c;
                c = s > 0xFF;
                v = (uint8_t)s;
            }
        }
        if (step[i].skip && (v & 0x80))
            continue;
        for (int j = 0; j < step[i].n; j++)
            OamByteSet(step[i].at[j], v);
    }
}

/*
 * @routine 7E:AE5E HudPanelsStep
 * WHAT IT DOES: The HUD panels' stage: the map marker up to $AC, gauge B's
 * X right to $1F, panel B's Y down to $2A, then slid in or out.
 */
void HudPanelsStep(void)
{
    uint8_t v;

    switch (g.start_light) {
    case 0x02:
        v = (uint8_t)(g.hud_marker_y - 0x04);
        if (v < 0xAC)
            v = 0xAC;
        g.hud_marker_y = v;
        hud_marker_y();
        break;
    case 0x04:
        v = (uint8_t)(g.gauge_place[4] + 0x04);
        if (!(v & 0x80) && v >= 0x1F)
            v = 0x1F;
        g.gauge_place[4] = v;
        hud_gauge_b_x();
        break;
    case 0x06:
        v = (uint8_t)(g.hud_panel_b_y + 0x04);
        if (!(v & 0x80) && v >= 0x2A) {
            g.hud_map_y_b = 0x7C;
            v = 0x2A;
        }
        g.hud_panel_b_y = v;
        hud_panel_b_y();
        break;
    case 0x08:
    case 0x0A:
        if (g.start_light == 0x08)
            HudSlideIn();
        else
            HudSlideOut();
        hud_sprites_y(0, 0);
        break;
    default:
        break;
    }
}

/*
 * @routine 7E:E0E8 SlideStep
 * WHAT IT DOES: Moves the sliding value toward its target and publishes
 * its high byte; ends the slide when the target is reached going down.
 */
void SlideStep(void)
{
    uint16_t v;

    if (!(g.picture_effects & 0x80))
        return;
    v = (g.slide_direction & 0x8000) ? (uint16_t)(g.slide_value - g.slide_step)
                                     : (uint16_t)(g.slide_value + g.slide_step);
    if (v == g.slide_target) {
        if (!(g.slide_direction & 0x8000))
            return;
        g.picture_effects &= 0x7F;
        palettes.target = 0xFFFF;
    } else {
        g.slide_value = v;
    }
    palettes.amount = (uint8_t)(g.slide_value >> 8);
}

/* @routine 02:EFA3 OamClassName: the class's name over six sprites. */
void OamClassName(void)
{
    uint16_t y = (uint16_t)(((g.oam_first_free + 1) * 4) + 2);
    uint16_t x;

    if (!g.course_class)
        return;
    x = g.course_class == 1 ? 0x0006 : 0x0000;
    for (int i = 0; i < 6; i++) {
        OamByteSet(y, cart8(0x02, (uint16_t)(0xEFD6 + x + i)));
        y = (uint16_t)(y + 4);
    }
}
