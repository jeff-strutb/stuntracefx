/*
 * slide.c: the HUD's panels sliding on and off, and the sprites that
 * follow them.
 *
 * Positions are bytes that wrap. A few of the original's adds take the
 * carry an earlier add or compare left instead of clearing it; those are
 * kept, and each says where its carry comes from.
 */
#include "game.h"

/* In by 4, stopping at $10 (a negative slide just moves). */
void hud_slide_in(void)
{
    uint8_t v = (uint8_t)(hud_slide + 4);

    if (!(v & 0x80) && v >= 0x10)
        v = 0x10;
    hud_slide = v;
}

/* Out by 4, once negative not below $D0. */
void hud_slide_out(void)
{
    uint8_t v = (uint8_t)(hud_slide - 4);

    if ((v & 0x80) && v < 0xD0)
        v = 0xD0;
    hud_slide = v;
}

static uint8_t adc8(uint8_t a, uint8_t m, bool *c)
{
    unsigned r = a + m + *c;

    *c = r > 0xFF;
    return (uint8_t)r;
}

/* A panel's sprites' Ys, the slide plus `dy`, its sprites from table
   offset `x`: four rows of two, $10 apart, then three more at +$0A, +$18
   and +$26, each hidden below the screen (sprite_y_on_screen). The first
   two of those three take the carry of the hiding test before them (set
   when that sprite was hidden). */
static void panel_sprites_y(uint8_t dy, uint16_t x)
{
    static const uint16_t row[4][2] = {{0x0411, 0x0421}, {0x0415, 0x0425}, {0x0419, 0x0429}, {0x041D, 0x042D}};
    static const struct { uint8_t add; uint16_t at; } more[3] = {{0x0A, 0x0405}, {0x18, 0x0409}, {0x26, 0x040D}};
    uint8_t y0 = (uint8_t)(hud_slide + dy), y;
    bool hidden = false;
    int i;

    for (i = 0; i < 4; i++) {
        y = sprite_y_on_screen((uint8_t)(y0 + 0x10 * i), &hidden);
        W8((uint16_t)(row[i][0] + x)) = y;
        W8((uint16_t)(row[i][1] + x)) = y;
    }
    for (i = 0; i < 3; i++) {
        bool c = i < 2 && hidden;

        y = sprite_y_on_screen(adc8(y0, more[i].add, &c), &hidden);
        W8((uint16_t)(more[i].at + x)) = y;
    }
}

/* By the panels' stage (wram_0D54): below 8 nothing; panel A slides in at
   8 or out above it (the stage restarting once the slide is at $D0), then
   either panel's sprites. */
static void panel_slide(uint8_t dy, uint16_t x, bool panel_a)
{
    if (wram_0D54 < 0x08)
        return;
    if (panel_a) {
        if (wram_0D54 == 0x08) {
            hud_slide_in();
        } else {
            if (hud_slide == 0xD0)
                wram_0D54 = 0;
            hud_slide_out();
        }
    }
    panel_sprites_y(dy, x);
}

void hud_panel_a_slide(void) { panel_slide(0xF8, 0x0020, true); }
void hud_panel_b_slide(void) { panel_slide(0x4A, 0x0120, false); }

/* Panel B's two sprite pairs at its Y less $0D, and $10 below. */
static void panel_b_sprites_y(void)
{
    uint8_t y = (uint8_t)(panel_b_y - 0x0D);

    W8(0x0535) = W8(0x053D) = y;
    W8(0x0539) = W8(0x0541) = (uint8_t)(y + 0x10);
}

/* The map marker's column: its Y, then six steps down, $FF from the first
   step that carries past 255. */
static void marker_sprites_y(void)
{
    static const struct { uint8_t add; uint16_t at[2]; int n; } step[6] = {
        {0x08, {0x0545, 0}, 1}, {0x06, {0x055D, 0x0561}, 2}, {0x08, {0x0565, 0x0569}, 2},
        {0x08, {0x0549, 0}, 1}, {0x08, {0x054D, 0x0551}, 2}, {0x08, {0x0555, 0x0559}, 2}};
    uint8_t y = wram_0E16;
    int i, j;

    W8(0x056D) = W8(0x0571) = y;
    for (i = 0; i < 6; i++) {
        bool c = false;

        y = adc8(y, step[i].add, &c);
        if (c)
            y = 0xFF;
        for (j = 0; j < step[i].n; j++)
            W8(step[i].at[j]) = y;
    }
}

/* Gauge B's row of Xs from its X: back 8, on $10, then 8 apart (one 4).
   The first five are left alone while negative. The adds from the sixth on
   carry from each other (the fifth's carry starts them). */
static void gauge_b_sprites_x(void)
{
    static const struct { uint8_t add; bool sub, clc, skip; uint16_t at[2]; int n; } step[11] = {
        {0x00, 0, 0, 1, {0x04A1, 0}, 1}, {0x08, 1, 0, 1, {0x04BD, 0}, 1},
        {0x10, 0, 1, 1, {0x04A5, 0}, 1}, {0x08, 0, 1, 1, {0x04A9, 0}, 1},
        {0x08, 0, 1, 1, {0x04AD, 0}, 1}, {0x08, 0, 0, 0, {0x04B5, 0}, 1},
        {0x08, 0, 0, 0, {0x04B9, 0x04B1}, 2}, {0x08, 0, 0, 0, {0x04C1, 0}, 1},
        {0x04, 0, 0, 0, {0x04C5, 0}, 1}, {0x08, 0, 0, 0, {0x04C9, 0}, 1},
        {0x08, 0, 0, 0, {0x04CD, 0}, 1}};
    uint8_t x = gauge_b6_x;
    bool c = false;
    int i, j;

    for (i = 0; i < 11; i++) {
        if (i) {
            if (step[i].sub) {
                c = x >= step[i].add;
                x = (uint8_t)(x - step[i].add);
            } else {
                if (step[i].clc)
                    c = false;
                x = adc8(x, step[i].add, &c);
            }
        }
        if (step[i].skip && (x & 0x80))
            continue;
        for (j = 0; j < step[i].n; j++)
            W8(step[i].at[j]) = x;
    }
}

/* The panels' stage: 2 the map marker up 4 (not above $AC) and its column;
   4 gauge B right 4 (to $1F at most, unless negative) and its row; 6 panel
   B down 4 (to $2A, wram_0E0C then $7C, unless negative); 8 and 10 the
   slide in or out and panel A's sprites. */
void hud_panels_step(void)
{
    uint8_t v;

    switch (wram_0D54) {
    case 0x02:
        v = (uint8_t)(wram_0E16 - 4);
        wram_0E16 = v < 0xAC ? 0xAC : v;
        marker_sprites_y();
        break;
    case 0x04:
        v = (uint8_t)(gauge_b6_x + 4);
        if (!(v & 0x80) && v >= 0x1F)
            v = 0x1F;
        gauge_b6_x = v;
        gauge_b_sprites_x();
        break;
    case 0x06:
        v = (uint8_t)(panel_b_y + 4);
        if (!(v & 0x80) && v >= 0x2A) {
            wram_0E0C = 0x7C;
            v = 0x2A;
        }
        panel_b_y = v;
        panel_b_sprites_y();
        break;
    case 0x08:
    case 0x0A:
        if (wram_0D54 == 0x08)
            hud_slide_in();
        else
            hud_slide_out();
        panel_sprites_y(0x00, 0x0000);
        break;
    }
}
