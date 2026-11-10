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
#include "data/race_tables.h"
#include "video/video.h"

/* A digit's two tiles into two sprites stacked: tile 2d on top, 2d + 1 below. */
static void digit(int sprite, uint8_t d)
{
    sprite_tile_set(sprite, (uint8_t)(d << 1));
    sprite_tile_set(sprite + 1, (uint8_t)((d << 1) + 1));
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
void HudStuntScoreDigits(void)
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
void HudDrawValueA(void) { HudDrawNumber3(g.status_level[0], 0x45); }
void HudDrawValueB(void) { HudDrawNumber3(g.status_level[0], 0x0A); }
void HudDrawValueC(void) { HudDrawNumber3(g.status_level[1], 0x4A); }

/* @routine 7E:E7FC HudDigitSprites62: a digit on sprites 62 and 63. */
void HudDigitSprites62(void)
{
    uint8_t d = g.time_override ? 0x01 : g.player_rank[0];

    sprite_tile_set(62, (uint8_t)(d << 1));
    sprite_tile_set(63, (uint8_t)((d << 1) + 1));
}

/* @routine 09:C072 HudCountUp19EC: an object's flag set, the count up, sound $3C. */
void StuntScoreUp(Object *o)
{
    uint8_t v = g.stunt_score_high;

    o->bonus_marker.state |= MARKER_SCORED;
    v = (uint8_t)(((v & 0x0F) == 9) ? ((v & 0xF0) + 0x10) : (v + 1));
    if ((v & 0xF0) == 0xA0)
        v = 0;
    g.stunt_score_high = v;
    sound_port0_push(0x3C);
}

/* @routine 09:C0AB HudCountMove19EECopy */
void HitsToScoreAdd(void)
{
    g.stunt_score_add = g.hits_bcd;     /* the original's 16-bit copy: the hundreds too */
    g.hits_bcd = 0;
}

/* @routine 09:C08E HudCountMove19EE: unless time-up is shown ($0E59): sound $35, the record's lift, the count moved. */
void HitsCollect(uint16_t rec)
{
    if (g.time_up_shown)
        return;
    sound_port0_push(0x35);
    GsuRecordBoostRaise(rec, 0x7F);
    GsuRecordRepair(rec, 0x7F);
    HitsToScoreAdd();
}

/*
 * @routine 7E:EE46 HudGaugesPanelA
 * WHAT IT DOES: Panel A's gauges: a four-sprite bar and a six-sprite bar.
 */
void HudGaugesPanelA(void)
{
    SpriteBar4WithMarker(g.gauge_one[1], (uint16_t)(g.gauge_place[3] << 8 | g.gauge_place[1]), 0x50);
    SpriteBar6WithMarker(g.kept_enable[1], (uint16_t)(g.gauge_place[7] << 8 | g.gauge_place[5]), 0x5B);
}

/*
 * @routine 7E:EE7B HudGaugesPanelB
 * WHAT IT DOES: When the race is ended or on the split screen, panel B's
 * gauges.
 */
void HudGaugesPanelB(void)
{
    if (!g.end_panel_in && !g.split_screen)
        return;
    SpriteBar4WithMarker(g.gauge_one[0], (uint16_t)(g.gauge_place[2] << 8 | g.gauge_place[0]),
                         g.split_screen ? 0x10 : 0x40);
    SpriteBar6WithMarker(g.kept_enable[0], (uint16_t)(g.gauge_place[6] << 8 | g.gauge_place[4]),
                         g.split_screen ? 0x1B : 0x4B);
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
 * low bits of its +$26; each next `step` sprites on (1 or -1) from sprite `n`.
 */
static void map_marker_sprites(uint8_t origin_x, uint8_t origin_y, int step, int n)
{
    for (int i = 0; objects.list[i]; i++) {
        const Object *o = obj_at(objects.list[i]);
        uint16_t v, sx;

        v = asr3(o->x);
        sx = (uint8_t)(((uint16_t)(v >> 8 | v << 8) + origin_x) & 0xFF);
        v = asr3((uint16_t)-o->z);
        v = (uint16_t)((uint16_t)(v + (uint16_t)(origin_y << 8)) & 0xFF00);
        sprite_x_set(n, (uint8_t)sx);
        sprite_y_set(n, (uint8_t)(v >> 8));
        v = ram_code_7f16((uint16_t)(0x0B46 + 2 * (o->list_kind & 0x07)));
        sprite_tile_set(n, (uint8_t)v);
        sprite_attributes_set(n, (uint8_t)(v >> 8));
        n += step;
    }
}

/* @routine 7E:EEF7 MapMarkersPanelA */
void MapMarkersPanelA(void)
{
    map_marker_sprites(g.hud_map_origin_x[1], g.hud_map_y_a, -1, 0x56);
}

/* @routine 7E:EF16 MapMarkersPanelB */
void MapMarkersPanelB(void)
{
    if (!g.end_panel_in && !g.split_screen)
        return;
    map_marker_sprites(g.hud_map_origin_x[0], g.hud_map_y_b, 1, g.split_screen ? 0x15 : 0x60);
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

/* The slide plus the panel's offset, into its sprites' X (the field at the
   start of each sprite's four bytes, where the original wrote it): sprites
   in the table below plus the panel's first sprite `base`. */
static void hud_panel_sprites_x(uint8_t x_offset, int base)
{
    static const int row[4][2] = {{39, 43}, {40, 44}, {41, 45}, {42, 46}};
    static const struct { uint8_t add; int clc; int sprite; } tail[3] = {
        {0x0A, 0, 36}, {0x18, 0, 37}, {0x26, 1, 38}};
    uint8_t v0 = (uint8_t)(g.hud_slide + x_offset), v = v0, x;

    for (int i = 0; i < 4; i++) {
        if (i)
            v = (uint8_t)(v + 0x10);
        x = SpriteYOnScreen(v);
        sprite_x_set(row[i][0] + base, x);
        sprite_x_set(row[i][1] + base, x);
    }
    /* The tail's first two sit a pixel further after a sprite at or past X
       $B0 (the original's add took the carry of the test before it); the
       third does not. */
    for (int i = 0; i < 3; i++) {
        int lower = !tail[i].clc && v >= 0xB0;

        v = (uint8_t)(v0 + tail[i].add + lower);
        sprite_x_set(tail[i].sprite + base, SpriteYOnScreen(v));
    }
}

static void hud_panel_slide(uint8_t x_offset, int base, int panel_a)
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
    hud_panel_sprites_x(x_offset, base);
}

/* @routine 7E:AE18 HudPanelASlide */
void HudPanelASlide(void) { hud_panel_slide(0xF8, 8, 1); }
/* @routine 7E:AE30 HudPanelBSlide */
void HudPanelBSlide(void) { hud_panel_slide(0x4A, 0x48, 0); }

/* Panel B's place for its two sprite pairs, $70-$73: into their X, where the original wrote it. */
static void hud_panel_b_x(void)
{
    uint8_t v = (uint8_t)(g.hud_panel_b_x - 0x0D);

    sprite_x_set(0x70, v);
    sprite_x_set(0x72, v);
    v = (uint8_t)(v + 0x10);
    sprite_x_set(0x71, v);
    sprite_x_set(0x73, v);
}

/* The map marker's place and the column of places after it, $FF once a
   step carries: into sprites $74-$7F's X, where the original wrote it. */
static void hud_marker_x(void)
{
    static const struct { uint8_t add; int sprite[2]; int n; } step[6] = {
        {0x08, {0x74, 0}, 1}, {0x06, {0x7A, 0x7B}, 2}, {0x08, {0x7C, 0x7D}, 2},
        {0x08, {0x75, 0}, 1}, {0x08, {0x76, 0x77}, 2}, {0x08, {0x78, 0x79}, 2}};
    unsigned sum = g.hud_marker_x;

    sprite_x_set(0x7E, g.hud_marker_x);
    sprite_x_set(0x7F, g.hud_marker_x);
    for (int i = 0; i < 6; i++) {
        unsigned r = (sum & 0xFF) + step[i].add;

        sum = r > 0xFF ? 0xFF : r;
        for (int j = 0; j < step[i].n; j++)
            sprite_x_set(step[i].sprite[j], (uint8_t)sum);
    }
}

/* Gauge B's X and the row of Xs after it: the first five skipped while
   negative, the rest carrying on. The Xs add up in 8 bits; where a step
   does not start afresh, an X after one that wrapped past 255 is a pixel
   further (the original's add took the carry the one before left). */
static void hud_gauge_b_x(void)
{
    static const struct { uint8_t add; int sub, clc, skip; int sprite[2]; int n; } step[11] = {
        {0x00, 0, 0, 1, {0x4B, 0}, 1}, {0x08, 1, 0, 1, {0x52, 0}, 1},
        {0x10, 0, 1, 1, {0x4C, 0}, 1}, {0x08, 0, 1, 1, {0x4D, 0}, 1},
        {0x08, 0, 1, 1, {0x4E, 0}, 1}, {0x08, 0, 0, 0, {0x50, 0}, 1},
        {0x08, 0, 0, 0, {0x51, 0x4F}, 2}, {0x08, 0, 0, 0, {0x53, 0}, 1},
        {0x04, 0, 0, 0, {0x54, 0}, 1}, {0x08, 0, 0, 0, {0x55, 0}, 1},
        {0x08, 0, 0, 0, {0x56, 0}, 1}};
    uint8_t v = g.gauge_place[4];
    int wrapped = 0;                    /* the X before went past 255 (or, after the subtract, did not go below 0) */

    for (int i = 0; i < 11; i++) {
        if (i) {
            if (step[i].sub) {
                wrapped = v >= step[i].add;
                v = (uint8_t)(v - step[i].add);
            } else {
                unsigned s = (unsigned)v + step[i].add + (step[i].clc ? 0u : (unsigned)wrapped);

                wrapped = s > 0xFF;
                v = (uint8_t)s;
            }
        }
        if (step[i].skip && (v & 0x80))
            continue;
        for (int j = 0; j < step[i].n; j++)
            sprite_x_set(step[i].sprite[j], v);
    }
}

/*
 * @routine 7E:AE5E HudPanelsStep
 * WHAT IT DOES: The HUD panels' stage: the map marker up to $AC, gauge B's
 * X right to $1F, panel B's X out to $2A, then slid in or out.
 */
void HudPanelsStep(void)
{
    uint8_t v;

    switch (g.start_light) {
    case 0x02:
        v = (uint8_t)(g.hud_marker_x - 0x04);
        if (v < 0xAC)
            v = 0xAC;
        g.hud_marker_x = v;
        hud_marker_x();
        break;
    case 0x04:
        v = (uint8_t)(g.gauge_place[4] + 0x04);
        if (!(v & 0x80) && v >= 0x1F)
            v = 0x1F;
        g.gauge_place[4] = v;
        hud_gauge_b_x();
        break;
    case 0x06:
        v = (uint8_t)(g.hud_panel_b_x + 0x04);
        if (!(v & 0x80) && v >= 0x2A) {
            g.hud_map_y_b = 0x7C;
            v = 0x2A;
        }
        g.hud_panel_b_x = v;
        hud_panel_b_x();
        break;
    case 0x08:
    case 0x0A:
        if (g.start_light == 0x08)
            HudSlideIn();
        else
            HudSlideOut();
        hud_panel_sprites_x(0, 0);
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
    int n = g.records_group_slot[5] + 1;    /* the six sprites after the first free one: their characters */
    uint16_t x;

    if (!g.course_class)
        return;
    x = g.course_class == 1 ? 0x0006 : 0x0000;
    for (int i = 0; i < 6; i++)
        sprite_tile_set(n + i, class_name_tile((uint16_t)(x + i)));   /* six tiles a class, the second class's six on */
}
