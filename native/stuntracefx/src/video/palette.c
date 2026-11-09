/*
 * palette.c: the colours. `palettes.buffer` is what the game builds, pic.palette
 * what is on the screen; the original's uploads to CGRAM are the routines here
 * that copy between them. The colours in use while the animated colours run:
 * 0 to $2F and $B0 to $BF, the three "kept" ones $E1 to $E3, and each player's
 * flash at $E4 on.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

Palettes palettes;

/* The colours from `first`, `n` of them, as they are in the buffer. */
static void shown(int first, int n)
{
    memcpy(&pic.palette[first], &palettes.buffer[first], (size_t)n * sizeof pic.palette[0]);
}

/* The animated colours, else colour 0. */
void RasterColours(void)
{
    if (g.palette_animate)
        shown(0xE1, 3);
    else
        shown(0x00, 1);
}

void CgramDmaAll(void)
{
    shown(0, 256);
}

void palette_shown_all(void)
{
    shown(0, 256);
}

void CgramUploadAll(void)
{
    g.player_palette_offset[0] = 0;
    shown(0, 256);
}

/* While animating, colours 0-$2F and $B0-$BF too, then RasterColours. */
void RasterPaletteUpload(void)
{
    if (!g.palette_animate) {
        shown(0x00, 1);
        return;
    }
    shown(0x00, 0x30);
    shown(0xB0, 0x10);
    RasterColours();
}

/* The animated colours and two cycling ones ($10-$11 on even frames, $12-$13 on odd). */
static void raster_colours_cycling(void)
{
    shown(0xE1, 3);
    if (!(g.frame_count & 1))
        shown(0x10, 2);
    else
        shown(0x12, 2);
}

void RasterColoursUnless0D22(void)
{
    if (g.split_stage_on)
        return;
    shown(0xE4, 6);
    raster_colours_cycling();
}

/* One colour-cycling step: the phase runs 0 to 15 and folds back after 7. */
static void cycle_step(int which, uint16_t table, int colour)
{
    int phase = palettes.cycle_phase[which], i = phase >= 8 ? 15 - phase : phase;

    palettes.buffer[colour] = ram_code_7e16((uint16_t)(table + i * 2));
    palettes.cycle_phase[which] = (uint8_t)(phase + 1 >= 16 ? 0 : phase + 1);
}

void PaletteCycle(void)
{
    if (!(g.frame_count & 1)) {
        cycle_step(0, 0xB71B, 0x10);
        cycle_step(1, 0xB6FB, 0x11);
    } else {
        cycle_step(2, 0xB70B, 0x12);
    }
}

void PaletteLoad16(uint8_t bank, uint16_t src)
{
    int i;

    for (i = 0; i < 16; i++) {
        uint16_t c = cart16(bank, (uint16_t)(src + 2 * i));

        palettes.blended[i] = palettes.day[i] = palettes.buffer[i] = c;
    }
}

/* the bit table: whether a colour takes part in the blend toward night */
static int blend_bit(int colour)
{
    uint16_t bits = cart16(0x03, (uint16_t)(0xEF1F + ((colour >> 3) & 0x1E)));

    return (bits >> (colour & 15)) & 1;
}

void PaletteCourseLoad(int course)
{
    int i = course * 3, c;
    uint8_t bank = cart8(0x03, (uint16_t)(0xF64B + i));
    uint16_t src = cart16(0x03, (uint16_t)(0xF649 + i));

    for (c = 16; c < 256; c++) {
        uint16_t w = cart16(bank, (uint16_t)(src + 2 * c));

        palettes.blended[c] = palettes.day[c] = palettes.buffer[c] = w;
    }
    bank = cart8(0x03, (uint16_t)(0xF69F + i));
    src = cart16(0x03, (uint16_t)(0xF69D + i));
    for (c = 16; c < 256; c++)
        palettes.night[c] = cart16(bank, (uint16_t)(src + 2 * c));
    PaletteLoad16(cart8(0x03, (uint16_t)(0xF6F3 + i)), cart16(0x03, (uint16_t)(0xF6F1 + i)));
    bank = cart8(0x03, (uint16_t)(0xF747 + i));
    src = cart16(0x03, (uint16_t)(0xF745 + i));
    for (c = 0; c < 16; c++)
        palettes.night[c] = cart16(bank, (uint16_t)(src + 2 * c));
    for (c = 255; c >= 0; c--) {
        uint16_t w = palettes.day[c], d = palettes.night[c];

        if (blend_bit(c)) {
            if (w != d)
                palettes.night[c] = w;
        } else if (w == d) {
            palettes.night[c] = (uint16_t)((w & 0x001F) ? w - 1 : w + 1);
        }
    }
    {
        static const uint16_t hud[6] = {0x45CC, 0x2528, 0x1CE5, 0x739A, 0x1CE5, 0x2528};

        memcpy(palettes.hud_colours, hud, sizeof hud);
    }
    if (sound_hold_b)
        for (c = 0xB0; c < 0xC0; c++)
            palettes.buffer[c] = palettes.blended[c] = palettes.day[c] = palettes.night[c] = 0;
}

void PaletteDarkenFour(uint16_t request)
{
    int i;

    palettes.blend_request = request;
    for (i = 0; i < 4; i++)
        palettes.night[16 + i] = (uint16_t)(palettes.day[16 + i] - 1);
}

/* A part of a colour moved `t` thirty-seconds of the way toward another: the
   original's multiplier, its factor t * 8 by the signed 8-bit difference, the
   product's upper 16 bits added back. */
static int part_blend(int from, int to, int t)
{
    return (from + ((t * 8 * (int8_t)(to - from)) >> 8)) & 0x1F;
}

static uint16_t colour_blend(uint16_t from, uint16_t to, int t)
{
    int r = part_blend(from & 31, to & 31, t);
    int g5 = part_blend((from >> 5) & 31, (to >> 5) & 31, t);
    int b5 = part_blend((from >> 10) & 31, (to >> 10) & 31, t);

    return (uint16_t)(r | g5 << 5 | b5 << 10);
}

/* The kept colours' cycles: a position of 0-12, moved by one or two. */
static uint8_t cycle13(uint8_t v, int n)
{
    v = (uint8_t)(v + n);
    return v < 13 ? v : (uint8_t)(n == 1 ? 0 : v - 13);
}

enum { KEPT_NONE, KEPT_SKIP, KEPT_ONE, KEPT_TWO, KEPT_ODD };

/* The kept colours at $E1-$E3 for one player: the third by the player's speed
   and timer, the first two every time a countdown borrows. */
void PaletteKeptColoursStep(int player, int pad_index)
{
    int go = KEPT_NONE;
    uint8_t t;
    int rising = !g.freeze && g.kept_enable[player] && (g.pad_last[pad_index / 2] & 0x4000);

    t = g.kept_timer[player];
    if (rising) {
        if (t == 0xFF) {
            go = KEPT_TWO;
        } else {
            t++;
            g.kept_timer[player] = t;
            go = t >= 0xB4 ? KEPT_TWO : t >= 0x78 ? KEPT_ONE : t >= 0x3C ? KEPT_ODD : KEPT_SKIP;
        }
    } else if (t == 0) {
        palettes.buffer[0xE3] = 0x7E63;
    } else if (t >= 0x78) {
        g.kept_timer[player] = (uint8_t)(t - 5);
        go = KEPT_TWO;
    } else if (t >= 0x3C) {
        g.kept_timer[player] = (uint8_t)(t - 4);
        go = KEPT_ONE;
    } else if (t >= 0x1E) {
        g.kept_timer[player] = (uint8_t)(t - 3);
        go = KEPT_ODD;
    } else {
        g.kept_timer[player] = (uint8_t)(t - 1);
        go = KEPT_SKIP;
    }
    if (go == KEPT_SKIP)
        go = (g.frame_count & 3) ? KEPT_NONE : KEPT_ONE;
    else if (go == KEPT_ODD)
        go = (g.frame_count & 1) ? KEPT_NONE : KEPT_ONE;
    if (go == KEPT_ONE || go == KEPT_TWO) {
        uint8_t step = cycle13(g.kept_step[player], go == KEPT_TWO ? 2 : 1);

        g.kept_step[player] = step;
        palettes.buffer[0xE3] = ram_code_7e16((uint16_t)(0xB4E9 + step * 2));
    }
    {
        unsigned s = (unsigned)g.kept_count[player] - g.kept_rate[player];

        g.kept_count[player] = (uint8_t)s;
        if (s >= 0x100) {
            uint8_t step = cycle13(g.kept_cycle[player], 1);

            g.kept_cycle[player] = step;
            palettes.buffer[0xE1] = ram_code_7e16((uint16_t)(0xB505 + step * 2));
            palettes.buffer[0xE2] = ram_code_7e16((uint16_t)(0xB521 + step * 2));
        }
    }
}

/* The player's three colours: its normal set, or for a level of 8 or more,
   flashing between the normal and the bright set, faster the higher. */
void PlayerColoursFlash(uint8_t level, int player)
{
    const uint16_t *set = palettes.hud_colours;
    int k, at = 0xE4 + g.player_palette_offset[player];

    if (level >= 8) {
        unsigned s = (unsigned)level + 0x60;

        level = s > 0xFF ? 0xFF : (uint8_t)s;
    }
    k = level >> 5;
    if (!k) {
        g.flash_count[player] = 0;
    } else {
        int n = 8 - k;
        uint8_t c = g.flash_count[player];

        if (c >= n * 2)
            c = 0xFF;
        c++;
        g.flash_count[player] = c;
        if (c < n)
            set = palettes.hud_colours + 3;
    }
    palettes.buffer[at] = set[0];
    palettes.buffer[at + 1] = set[1];
    palettes.buffer[at + 2] = set[2];
}

/* The colour at $13 that shows a player's status. */
void StatusColourPick(int item)
{
    uint16_t flags, colour;
    uint8_t level = g.status_level[item];

    if (level == 0) {
        colour = 0;
    } else {
        flags = g.status_flags[item];
        if ((flags & 0x0400) && g.kept_enable[item]) {
            colour = ram_code_7e16((uint16_t)(0xB5B9 + (g.frame_count & 3) * 2));
        } else if ((flags & 0x0010) && level >= 0x3C) {
            colour = ram_code_7e16((uint16_t)(0xB5B1 + (g.frame_count & 3) * 2));
        } else {
            if (g.frame_count & 1)
                return;
            colour = ram_code_7e16((uint16_t)(0xB5B1 + ((g.frame_count >> 1) & 3) * 2));
        }
    }
    palettes.buffer[0x13] = colour;
}

void PalettesStep(void)
{
    if (!g.palette_animate)
        return;
    PlayerColoursFlash(g.status_level[0], 0);
    StatusColourPick(0);
    PaletteCycle();
    PaletteKeptColoursStep(0, 0);
}

/* The weather's value eased toward its goal (bit 5 of the high flag byte, not
   with bit 7), its whole part shown in `weather`. */
void FadeValueStep(void)
{
    uint16_t w;

    if ((g.picture_effects & 0x80) || !(g.picture_effects & 0x20))
        return;
    w = (uint16_t)(palettes.weather_value + palettes.weather_step);
    if (w != palettes.weather_goal)
        palettes.weather_value = w;
    else
        g.picture_effects &= (uint8_t)~0x20;
    palettes.weather = (uint16_t)(palettes.weather_value >> 8);
}

/* Called each race frame; the count that starts it ($0CFA) is written only by
   code the game never runs, so it has nothing to do. */
void PaletteFadeStep(void)
{
}

/* The courses that cycle colours 12 to 15 (course 3 in the buffer, 7 in the
   night set, $0B only colour 15's low byte). */
void CoursePaletteCycle(void)
{
    static const uint16_t c3[4] = {0xE45F, 0xE48B, 0xE4B7, 0xE4E3};
    static const uint16_t c7[4] = {0xE50F, 0xE53B, 0xE4B7, 0xE4E3};
    static const uint16_t fixed7[4] = {0x14A7, 0x1086, 0x15FF, 0x269F};
    int c = g.course, i;
    uint8_t s;

    if (c != 3 && c != 7 && c != 0x0B)
        return;
    s = (uint8_t)(palettes.course_cycle + 1);
    if (s >= 0x16)
        s = 0;
    palettes.course_cycle = s;
    s = (uint8_t)(s * 2);
    if (c == 3) {
        for (i = 0; i < 4; i++)
            palettes.buffer[12 + i] = cart16(0x04, (uint16_t)(c3[i] + s));
    } else if (c == 7) {
        for (i = 0; i < 4; i++) {
            palettes.day[12 + i] = fixed7[i];
            palettes.night[12 + i] = cart16(0x04, (uint16_t)(c7[i] + s));
        }
    } else {
        uint8_t v = cart8(0x04, (uint16_t)((palettes.weather >= 0x18 ? 0xE433 : 0xE407) + s));

        palettes.buffer[15] = (uint16_t)((palettes.buffer[15] & 0xFF00) | v);
        palettes.day[15] = (uint16_t)((palettes.day[15] & 0xFF00) | v);
        palettes.night[15] = (uint16_t)((palettes.night[15] & 0xFF00) | v);
    }
}

/* ---- the race's colour work each frame ---- */

/* The fog's amount toward the target, moved by the request. */
static void amount_step(void)
{
    uint16_t r = palettes.blend_request;

    if (!r)
        return;
    if (r & 0x8000) {
        uint16_t a = (uint16_t)(palettes.amount + 2);

        palettes.target = r & 0x7FFF;
        palettes.amount = a >= 0x1F ? 0x1F : a;
        return;
    }
    if (g.course == 6 || g.course == 4) {
        uint16_t least = g.course == 6 ? 0x0A : 0x0F, a = (uint16_t)(palettes.amount - 3);

        if (a >= least) {
            palettes.amount = a;
        } else {
            palettes.amount = least;
            palettes.blend_request = 0;
        }
        return;
    }
    if ((uint16_t)(g.fade_level << 8 | g.fade_low) != 0x0F00 || !palettes.amount)
        return;
    if (palettes.amount >= 3) {
        palettes.amount = (uint16_t)(palettes.amount - 3);
    } else {
        palettes.amount = 0;
        palettes.target = 0xFFFF;
        palettes.blend_request = 0;
    }
}

/* One gradient band rebuilt from the day and night lists at `offset` bytes:
   a repeated colour in both lists repeats the last result. */
static void gradient_band_rebuild(void)
{
    uint16_t start, end, row, offset;
    int first = 1;

    palettes.band = (uint16_t)(palettes.band + 1 >= 3 ? 0 : palettes.band + 1);
    start = ram_code_7e16((uint16_t)(0xE69C + palettes.band * 2));
    end = (uint16_t)(start + 0x100);
    palettes.band_end = end;
    gradient.source[g.fx_buffer] = (uint16_t)(start / 2);
    offset = gradient.view_offset[0];
    for (row = start; row < end; row = (uint16_t)(row + 2), offset = (uint16_t)(offset + 2)) {
        uint16_t day = gradient.day[offset / 2], night = gradient.night[offset / 2], c;

        if (!first && day == gradient.day[offset / 2 - 1] && night == gradient.night[offset / 2 - 1]) {
            c = gradient.band[row / 2 - 1];
        } else {
            c = colour_blend(day, night, palettes.weather);
            if (!(palettes.target & 0x8000))
                c = colour_blend(c, palettes.target, palettes.amount);
        }
        gradient.band[row / 2] = c;
        first = 0;
    }
}

/* The course colours from `first`: each that differs between the two sets is
   made again from them, then taken toward the target. */
static void colours_from_sets(int first)
{
    int x;

    for (x = first; x < 256; x++) {
        uint16_t d = (uint16_t)(palettes.day[x] - palettes.night[x]);

        if (!d)
            continue;
        if (d == 1 || d == 0xFFFF || !palettes.weather)
            palettes.blended[x] = palettes.day[x];
        else if (palettes.weather == 0x1F)
            palettes.blended[x] = palettes.night[x];
        else
            palettes.blended[x] = colour_blend(palettes.day[x], palettes.night[x], palettes.weather);
        palettes.buffer[x] = colour_blend(palettes.blended[x], palettes.target, palettes.amount);
    }
}

/* The buffer when the blend or the target have moved; 1 when nothing has. */
static int buffer_from_blend(void)
{
    int x;

    if (palettes.weather != palettes.weather_shown) {
        if (!(palettes.target & 0x8000)) {
            colours_from_sets(2);
            return 0;
        }
        for (x = 2; x < 256; x++) {
            uint16_t d = (uint16_t)(palettes.day[x] - palettes.night[x]);

            if (d && d != 1 && d != 0xFFFF) {
                palettes.blended[x] = colour_blend(palettes.day[x], palettes.night[x], palettes.weather);
                palettes.buffer[x] = palettes.blended[x];
            }
        }
        return 0;
    }
    if (palettes.target & 0x8000) {
        if (!(palettes.target_shown & 0x8000))
            memcpy(&palettes.buffer[2], &palettes.blended[2], 254 * sizeof palettes.buffer[0]);
        return 0;
    }
    if (palettes.target == palettes.target_shown && palettes.amount == palettes.amount_shown)
        return 1;
    for (x = 2; x < 256; x++)
        if (palettes.day[x] != palettes.night[x])
            palettes.buffer[x] = colour_blend(palettes.blended[x], palettes.target, palettes.amount);
    return 0;
}

void HudColourStep(void)
{
    static const uint16_t hud[6] = {0x45CC, 0x2528, 0x1CE5, 0x739A, 0x1CE5, 0x2528};
    int i, x;

    amount_step();
    if (g.picture_effects & 0x08) {
        palettes.gradient_was_off = 1;
        gradient.source[g.fx_buffer] = GRADIENT_AT_BLACK;
        memset(&palettes.buffer[2], 0, 14 * sizeof palettes.buffer[0]);
        return;
    }
    if (palettes.gradient_was_off) {
        palettes.target_shown = palettes.target;
        palettes.gradient_was_off = 0;
        colours_from_sets(2);
    } else {
        gradient_band_rebuild();
        if (buffer_from_blend())
            return;
    }
    for (i = 0; i < 6; i++) {
        uint16_t t = (palettes.target & 0x8000) ? hud[i] : palettes.target;

        palettes.hud_colours[i] = colour_blend(hud[i], t, palettes.amount);
    }
    if (g.course == 7 && !palettes.blend_request)
        for (x = 12; x < 16; x++) {
            palettes.blended[x] = colour_blend(palettes.day[x], palettes.night[x], palettes.weather);
            palettes.buffer[x] = palettes.blended[x];
        }
    palettes.weather_shown = palettes.weather;
    palettes.amount_shown = palettes.amount;
    palettes.target_shown = palettes.target;
}
