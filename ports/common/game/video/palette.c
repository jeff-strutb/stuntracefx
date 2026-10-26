/*
 * palette.c: the colours the frame changes as it is drawn, and the colour
 * effects stepped once a frame: the cycling colours, the kept colours that
 * pulse with a player's state, the player's colours flashing, the status
 * colour.
 *
 * The palette buffer is work RAM from $FD65 (colour 0 on); a few colours
 * are kept apart from $FF27 (colours $E1-$E3, then $E4-$E9 from $FF2D).
 * The effects' colour tables are with the code the original runs from
 * work RAM (ram_code_word).
 */
#include "game.h"
#include "rom.h"
#include "video.h"

static void colours_from(uint8_t first, uint32_t at, unsigned n)
{
    uint8_t data[512];
    unsigned i;

    for (i = 0; i < 2 * n; i++)
        data[i] = wram[(at + i) & 0x1FFFF];
    video_colours(first, data, n);
}

/* The animated colours $E1-$E3 when in use, else colour 0. */
void raster_colours(void)
{
    if (palette_animate)
        colours_from(0xE1, 0xFF27, 3);
    else
        colours_from(0x00, 0xFD65, 1);
}

/* When the animated colours are in use, colours 0-$2F and $B0-$BF from the
   buffer, then the animated ones; else colour 0. */
void raster_palette_upload(void)
{
    if (palette_animate) {
        colours_from(0x00, 0xFD65, 0x30);
        colours_from(0xB0, 0xFEC5, 0x10);
    }
    raster_colours();
}

/* The animated colours, then this frame's two cycling ones: $10-$11 on
   even frames, $12-$13 on odd, from the buffer at $FD85 + x. */
void raster_colours_cycling(uint16_t x)
{
    colours_from(0xE1, 0xFF27, 3);
    if (!(frame_count & 1))
        colours_from(0x10, 0xFD85u + x, 2);
    else
        colours_from(0x12, 0xFD89u + x, 2);
}

/* Colours $E4-$E9 from the buffer at $FF2D + x. */
void raster_colours_e4(uint16_t x)
{
    colours_from(0xE4, 0xFF2Du + x, 6);
}

/* Unless $0D22 is set, the colours from the start of their buffers. */
void raster_colours_unless_0d22(void)
{
    if (W8(0x0D22))
        return;
    raster_colours_e4(0);
    raster_colours_cycling(0);
}

/* One cycling colour: its phase (0-15) picks from its table of eight,
   back down the table for phases 8-15, and steps. */
static void cycle_colour(uint16_t phase_at, uint16_t table, uint16_t to)
{
    uint16_t phase = W16(phase_at);
    uint16_t i = phase >= 8 ? (uint16_t)(15 - phase) : phase;

    W16(to) = ram_code_word((uint16_t)(table + (uint16_t)(i << 1)));
    phase++;
    W16(phase_at) = phase >= 16 ? 0 : phase;
}

/* The cycling colours: two on even frames, one on odd. */
void palette_cycle(void)
{
    if (!(frame_count & 1)) {
        cycle_colour(0xFD5B, 0xB71B, 0xFD85);
        cycle_colour(0xFD5D, 0xB6FB, 0xFD87);
    } else
        cycle_colour(0xFD5F, 0xB70B, 0xFD89);
}

/* A 13-step cycle moved on by n. */
static uint8_t cycle13(uint8_t v, uint8_t n)
{
    v = (uint8_t)(v + n);
    if (v < 13)
        return v;
    return n == 1 ? 0 : (uint8_t)(v - 13);
}

/* Player y's kept colours: the third ($FF2B) cycles faster the longer
   the player's timer ($0E01 + y) has counted up, which it does while the
   player's flag ($0E05 + y) and bit 14 of $0D1E + x are set and $0D53 is
   clear; otherwise the timer counts back down through the same speeds,
   and at 0 the colour rests at $7E63. The first two ($FF27, $FF29) step
   together each time $0DFD + y, less $0DFB + y each frame, borrows. */
void palette_kept_colours_step(uint16_t x, uint16_t y)
{
    enum { NONE, QUARTER, HALF, ONE, TWO } go = NONE;
    int up = !W8(0x0D53) && W8((uint16_t)(0x0E05 + y)) && (W16((uint16_t)(0x0D1E + x)) & 0x4000);
    uint8_t t = W8((uint16_t)(0x0E01 + y));
    unsigned v, d;

    if (up) {
        if (t == 0xFF)
            go = TWO;
        else {
            W8((uint16_t)(0x0E01 + y)) = ++t;
            go = t >= 0xB4 ? TWO : t >= 0x78 ? ONE : t >= 0x3C ? HALF : QUARTER;
        }
    } else if (!t)
        W16(0xFF2B) = 0x7E63;
    else {
        static const struct { uint8_t from, down, go; } bands[] = {
            {0x78, 5, TWO}, {0x3C, 4, ONE}, {0x1E, 3, HALF}, {0x00, 1, QUARTER},
        };
        int i = 0;

        while (t < bands[i].from)
            i++;
        W8((uint16_t)(0x0E01 + y)) = (uint8_t)(t - bands[i].down);
        go = bands[i].go;
    }
    if (go == QUARTER)
        go = frame_count & 3 ? NONE : ONE;
    else if (go == HALF)
        go = frame_count & 1 ? NONE : ONE;
    if (go == ONE || go == TWO) {
        uint8_t step = cycle13(W8((uint16_t)(0x0E03 + y)), go == TWO ? 2 : 1);

        W8((uint16_t)(0x0E03 + y)) = step;
        W16(0xFF2B) = ram_code_word((uint16_t)(0xB4E9 + (uint8_t)(step << 1)));
    }
    v = W8((uint16_t)(0x0DFD + y));
    d = W8((uint16_t)(0x0DFB + y));
    W8((uint16_t)(0x0DFD + y)) = (uint8_t)(v - d);
    if (v < d) {
        uint8_t step = cycle13(W8((uint16_t)(0x0DFF + y)), 1);

        W8((uint16_t)(0x0DFF + y)) = step;
        W16(0xFF27) = ram_code_word((uint16_t)(0xB505 + (uint8_t)(step << 1)));
        W16(0xFF29) = ram_code_word((uint16_t)(0xB521 + (uint8_t)(step << 1)));
    }
}

/* Player y's three colours into the palette at $01C8 + the player's
   offset ($0D83 + 2y): the set at $0E21, or for a level of 8 or more
   flashing with the set at $0E27, faster the higher the level (n calls
   of each, n = 8 less the top three bits of level + $60, held at $FF; the
   count at $0E1F + y). */
void player_colours_flash(uint8_t level, uint16_t y)
{
    uint16_t from = 0x0E21, at;
    uint8_t k;

    if (level >= 8)
        level = level + 0x60 > 0xFF ? 0xFF : (uint8_t)(level + 0x60);
    k = level >> 5;
    if (!k)
        W8((uint16_t)(0x0E1F + y)) = 0;
    else {
        uint8_t n = (uint8_t)(8 - k), c = W8((uint16_t)(0x0E1F + y));

        if (c >= (uint8_t)(n << 1))
            c = 0xFF;
        W8((uint16_t)(0x0E1F + y)) = ++c;
        if (c < n)
            from = 0x0E27;
    }
    at = W16((uint16_t)(0x0D83 + (uint16_t)(y << 1)));
    W16((uint16_t)(0x01C8 + at)) = W16(from);
    W16((uint16_t)(0x01CA + at)) = W16(from + 2);
    W16((uint16_t)(0x01CC + at)) = W16(from + 4);
}

/* Item y's status colour into $FD8B: none while its timer ($0DDA + y) is
   0; with flag $0400 and the player's flag ($0E05 + y), the four-frame
   cycle at $B5B9; with flag $0010 and the timer at $3C or more, that at
   $B5B1; otherwise $B5B1 at half the speed, changed on even frames only. */
void status_colour_pick(uint16_t y)
{
    uint8_t t = W8((uint16_t)(0x0DDA + y));
    uint16_t flags, frame = W16(0x0E3B), colour;

    if (!t)
        colour = 0;
    else {
        flags = (uint16_t)(W8((uint16_t)(0x0DF0 + y)) | W8((uint16_t)(0x0DF2 + y)) << 8);
        if ((flags & 0x0400) && W8((uint16_t)(0x0E05 + y)))
            colour = ram_code_word((uint16_t)(0xB5B9 + ((frame & 3) << 1)));
        else if ((flags & 0x0010) && t >= 0x3C)
            colour = ram_code_word((uint16_t)(0xB5B1 + ((frame & 3) << 1)));
        else if (frame & 1)
            return;
        else
            colour = ram_code_word((uint16_t)(0xB5B1 + (((frame >> 1) & 3) << 1)));
    }
    W16(0xFD8B) = colour;
}

/* The frame's colour effects for player 0, while the animated colours are
   in use. */
void palettes_step(void)
{
    if (!palette_animate)
        return;
    player_colours_flash(W8(0x0DDA), 0);
    status_colour_pick(0);
    palette_cycle();
    palette_kept_colours_step(0, 0);
}
