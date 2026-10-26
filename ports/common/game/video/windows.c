/*
 * windows.c: the windows the race's frame shows: the window and colour
 * maths setups it switches between, the window table (the window values
 * used from line to line), and the animations that move them: the bobbing
 * and sweeping windows, the oscillating one, the slide, the opening and
 * closing bands and split, the layouts, and the fixed colour's bands.
 *
 * The window table is in bank $7F at $F85B, read a line at a time by the
 * frame (video/registers.c): entries of a line count and the address of
 * the window values for those lines ($F857 the normal ones, $FFD1-$FFDD
 * others), a count of 0 ending it. The fixed colour's table is at $7F:FF8B.
 * The animations' state is work RAM from $19A9 to $1A26; the tables they
 * read are with the code the original runs from work RAM (ram_code_word).
 */
#include "game.h"
#include "rom.h"
#include "video.h"

#define WT8(o)          W8(0x1F85Bu + (o))
#define WT16(o)         W16(0x1F85Bu + (o))
#define NORMAL          0xF857
#define window_setup    W8(0x19A9)      /* which setup: 0-8 by twos */
#define window_state    W8(0x19D8)      /* the bands' or split's animation; 0 none */
#define window_layout_none (W8(0x19BC) == 0xFF)

/* ---- the setups ---- */

static void colour_maths(VideoScreen *s, uint8_t layers, int subtract)
{
    s->colour_maths = layers;
    s->colour_half = 0;
    s->colour_subtract = (uint8_t)subtract;
}

/* each window_use: window 1 or 2 on, and inverted (video.h) */
enum { OUT1 = 1, ON1 = 2, OUT2 = 4, ON2 = 8 };

/* how the windows combine: by or, the sprites' by `sprites` */
static void window_logic_or(VideoScreen *s, int sprites)
{
    int l;

    for (l = 0; l < 4; l++)
        s->window_logic[l] = 0;
    s->window_logic[4] = (uint8_t)sprites;
    s->window_logic[5] = 0;
}

/* Layers 1 and 2 cut inside window 2 (with window 1's inversion kept, it
   being off); the colour window window 1 outside, the maths kept to it,
   the sub screen added on layers 1 and 2; layers 1-3 and the sprites
   cut on the main screen. */
static void windows_colour_add_bg12(void)
{
    VideoScreen *s = video_screen();

    s->window_use[0] = s->window_use[1] = OUT1 | ON2;
    s->window_use[4] = 0;
    s->window_use[5] = OUT1 | ON1;
    s->colour_clip = 0, s->colour_prevent = 2, s->colour_sub_screen = 1, s->colour_direct = 0;
    colour_maths(s, 0x03, 0);
    window_logic_or(s, 1);
    s->window_layers = 0x17;
}

/* No layer cut; window 1 for the colour alone, the maths kept inside it,
   the fixed colour mixed; layers 1 and 2 named for cutting. */
static void windows_colour_window_only(void)
{
    VideoScreen *s = video_screen();

    s->window_use[0] = s->window_use[1] = s->window_use[2] = s->window_use[3] = 0;
    s->window_use[4] = 0;
    s->window_use[5] = ON1;
    s->colour_clip = 0, s->colour_prevent = 1, s->colour_sub_screen = 0, s->colour_direct = 0;
    window_logic_or(s, 0);
    s->window_layers = 0x03;
}

/* Every layer, the sprites and the colour within both windows, the maths
   kept inside them, subtracting on everything; all cut on the main
   screen. */
static void windows_all_inverted(void)
{
    VideoScreen *s = video_screen();
    int l;

    colour_maths(s, 0x3F, 1);
    for (l = 0; l < 6; l++)
        s->window_use[l] = ON1 | ON2;
    s->colour_clip = 0, s->colour_prevent = 1, s->colour_sub_screen = 0, s->colour_direct = 0;
    window_logic_or(s, 0);
    s->window_layers = 0x1F;
}

void colour_maths_sub83_windowed(void)
{
    colour_maths(video_screen(), 0x03, 1);
    windows_colour_window_only();
}

void windows_colour_add3(void)
{
    colour_maths(video_screen(), 0x03, 0);
    windows_colour_window_only();
}

static void windows_colour_sub81(void)
{
    VideoScreen *s = video_screen();

    colour_maths(s, 0x01, 1);
    windows_colour_window_only();
    s->window_layers = 0x01;
}

/* The setup window_setup picks: 0 adding on layers 1 and 2, 2 subtracting
   on them, 4 adding, 6 subtracting on layer 1 alone, 8 (or another) all
   cut and subtracted. */
void windows_setup_select(void)
{
    switch (window_setup) {
    case 0: windows_colour_add_bg12(); break;
    case 2: colour_maths_sub83_windowed(); break;
    case 4: windows_colour_add3(); break;
    case 6: windows_colour_sub81(); break;
    default: windows_all_inverted();
    }
}

/* ---- the window table ---- */

static void table_bands(const uint8_t *lines, const uint16_t *values, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        WT8(3 * i) = lines[i];
        WT16(3 * i + 1) = values[i];
    }
    WT8(3 * n) = 0;
}

/* Four bands of the values at $FFDD. */
void window_table_ffdd(void)
{
    static const uint8_t lines[4] = {0x40, 0x40, 0x40, 0x3E};
    static const uint16_t values[4] = {0xFFDD, 0xFFDD, 0xFFDD, 0xFFDD};

    table_bands(lines, values, 4);
}

/* Normal, $FFD1 for 112 and 16 lines, normal. */
void window_table_ffd1(void)
{
    static const uint8_t lines[4] = {0x20, 0x70, 0x10, 0x08};
    static const uint16_t values[4] = {NORMAL, 0xFFD1, 0xFFD1, NORMAL};

    table_bands(lines, values, 4);
}

/* Bands 16, 48 and 16 lines long from entry `at`, at `values` (the end
   and the bands around them as they were). */
static void table_three(int at, uint16_t values)
{
    WT8(at) = 0x10, WT8(at + 3) = 0x30, WT8(at + 6) = 0x10;
    WT16(at + 1) = WT16(at + 4) = WT16(at + 7) = values;
}

void window_table_ffd5(void)
{
    table_three(3, 0xFFD5);
}

void window_table_ffd9(void)
{
    table_three(15, 0xFFD9);
}

/* Normal, the values layout $19B6 picks, normal for $30 lines. */
static void window_table_from_layout_list(void)
{
    uint16_t x = W16(0x19B6);

    WT16(1) = NORMAL, WT16(7) = NORMAL;
    WT16(4) = ram_code_word((uint16_t)(0xE0D2 + x));
    WT8(0) = ram_code_byte((uint16_t)(0xE0B4 + x));
    WT8(3) = ram_code_byte((uint16_t)(0xE0B5 + x));
    WT8(6) = 0x30;
    WT8(9) = 0;
}

/* Layout $19B6's lines and values: one screen, or the split screen with
   $0D2E set. */
static void window_table_layout(void)
{
    uint16_t x = W16(0x19B6);

    if (!W8(0x0D2E)) {
        WT16(1) = NORMAL, WT16(7) = NORMAL;
        WT16(4) = ram_code_word((uint16_t)(0xE0D2 + x));
        WT8(0) = ram_code_byte((uint16_t)(0xE0C2 + x));
        WT8(3) = ram_code_byte((uint16_t)(0xE0C3 + x));
        WT8(6) = 0x10;
        WT8(9) = 0;
    } else {
        uint8_t n = ram_code_byte((uint16_t)(0xE0CA + x));

        WT16(1) = NORMAL, WT16(4) = NORMAL, WT16(10) = NORMAL;
        WT16(7) = ram_code_word((uint16_t)(0xE0E0 + x));
        WT8(0) = n, WT8(3) = n;
        WT8(6) = ram_code_byte((uint16_t)(0xE0CB + x));
        WT8(9) = 0x10;
        WT8(12) = 0;
    }
}

/* The normal values over the whole screen, unless the bands animate or
   no layout is chosen. */
void window_table_normal(void)
{
    static const uint8_t lines[4] = {0x40, 0x40, 0x40, 0x40};
    static const uint16_t values[4] = {NORMAL, NORMAL, NORMAL, NORMAL};

    if (window_state || window_layout_none)
        return;
    table_bands(lines, values, 4);
}

/* Nine normal bands, unless the bands animate. */
void window_table_bands(void)
{
    static const uint8_t lines[9] = {0x17, 0x10, 0x30, 0x10, 0x18, 0x10, 0x30, 0x10, 0x01};
    static const uint16_t values[9] = {NORMAL, NORMAL, NORMAL, NORMAL, NORMAL, NORMAL, NORMAL, NORMAL, NORMAL};

    if (window_state)
        return;
    table_bands(lines, values, 9);
}

/* Four bands about `line`: $FFDD above and below, normal between; their
   sizes from the split's height $19DC and its edges $19E2-$19E5. */
static void window_table_at(uint8_t line)
{
    uint8_t h = W8(0x19DC);

    WT8(3) = (uint8_t)(line + W8(0x19E5));
    WT8(6) = (uint8_t)(line + W8(0x19E3));
    WT8(0) = (uint8_t)(h - 1 + W8(0x19E4));
    WT8(9) = (uint8_t)(h + W8(0x19E2));
    WT8(12) = 0;
    WT16(4) = NORMAL, WT16(7) = NORMAL;
    WT16(1) = 0xFFDD, WT16(10) = 0xFFDD;
}

/* A byte of bank $7E as the original reads it: its code's tables from the
   cartridge, else work RAM (some presets are kept there). */
static uint8_t bank_7e_byte(uint16_t at)
{
    return at >= 0xA2D9 && at < 0xA2D9 + 0x54A6 ? ram_code_byte(at) : W8(at);
}

/* 32 normal lines, then the entries of list `select` (to its end). */
static void window_table_from_list(uint8_t select)
{
    uint16_t at = ram_code_word((uint16_t)(0xD8DE + select)), x = 3;
    uint8_t v;

    WT8(0) = 0x20;
    WT16(1) = NORMAL;
    do {
        v = bank_7e_byte(at++);
        WT8(x++) = v;
    } while (v);
}

/* Preset `select`'s nine bytes from entry 1 (entry 5 with $0D2E set). */
static void window_table_preset(uint8_t select)
{
    uint16_t at, x;
    int i;

    if (W8(0x0D2E))
        at = ram_code_word((uint16_t)(0xDA1D + select)), x = 0x0F;
    else
        at = ram_code_word((uint16_t)(0xD9C5 + select)), x = 0x03;
    for (i = 0; i < 9; i++)
        WT8(x + i) = bank_7e_byte((uint16_t)(at + i));
    W8(0x19E1) = 9;
}

/* A request in $19BD to move the layout: 1 up by 2 a step, $FF down;
   while it moves ($19BC = $FF) each call steps $19B6 by $19B8, ending at
   -2 and held at the limit for `which` (0, or 2 on the menu's course
   screen), then resets the setup and rebuilds the table. */
static void window_layout_step(uint8_t which)
{
    uint8_t req = W8(0x19BD);
    uint16_t at;
    int i;

    if (req == 0x01 || req == 0xFF) {
        W8(0x19BC) = 0xFF;
        W16(0x19B8) = req == 0x01 ? 0x0002 : 0xFFFE;
        W16(0x19BA) = req == 0x01 ? 0x0001 : 0x0000;
    } else if (!window_layout_none)
        return;
    W8(0x19BD) = 0;
    at = (uint16_t)(W16(0x19B6) + W16(0x19B8));
    if (at == 0xFFFE) {
        W8(0x19BC) = 0;
        return;
    }
    if (at != ram_code_word((uint16_t)(0xDBC5 + which)))
        W16(0x19B6) = at;
    window_setup = 0;
    for (i = 0; i < 6; i++)
        W8(0x19AC + i) = 0xE7;
    if (which == 0)
        window_table_from_layout_list();
    else
        window_table_layout();
}

void window_layout_step_for_screen(void)
{
    W8(0x0D45) &= 0xDF;
    window_layout_step(W8(0x0D62) ? 2 : 0);
}

/* ---- the moving windows ---- */

/* The window's values ($19AC-$19AE: $20, then the bob's place, as
   left and right edges) */
static void window_values(uint8_t a, uint8_t b, uint8_t c)
{
    W8(0x19AC) = a, W8(0x19AD) = b, W8(0x19AE) = c;
}

static void window_table_ffd1_or_ffd5(void)
{
    if (W8(0x0D62))
        window_table_ffd5();
    else
        window_table_ffd1();
}

/* Bob x (0 or 1) at half speed: every second call its count goes up; at
   10 it stops (bit 7 of $0DA4 or $0DA6 cleared), else its place moves by
   its speed, which turns round at $1C and $16. */
static void bob_step(uint8_t x)
{
    uint8_t v = (uint8_t)(W8(0x1A1E + x) + 1), c;

    if (v != 2) {
        W8(0x1A1E + x) = v;
        return;
    }
    c = (uint8_t)(W8(0x1A22 + x) + 1);
    W8(0x1A22 + x) = c;
    if (c == 10) {
        W8(0x1A22 + x) = 0;
        W8(x == 1 ? 0x0DA6 : 0x0DA4) &= 0x7F;
        return;
    }
    W8(0x1A1E + x) = 0;
    v = (uint8_t)(W8(0x1A1C + x) + W8(0x1A20 + x));
    W8(0x1A1C + x) = v;
    if (v == 0x1C || v == 0x16)
        W8(0x1A20 + x) = (uint8_t)-W8(0x1A20 + x);
}

/* Bob x's window from its place, unless that is $18; 1 if set. */
static int bob_window(uint8_t x, uint16_t at)
{
    uint8_t v;

    bob_step(x);
    v = W8(0x1A1C + x);
    if (v == 0x18)
        return 0;
    W8(at + 2) = (uint8_t)(v | 0x80);
    W8(at + 1) = (uint8_t)((v | 0x40) - 2);
    W8(at) = 0x20;
    return 1;
}

/* A player's bob starts when the object two links on from the player's
   (`player`) has bit 14 set and the event `pending` is waiting. */
static void link_event_trigger(uint16_t flag, uint16_t player, uint16_t pending, uint8_t bob)
{
    uint16_t x, y;

    if (W8(flag) & 0x80)
        return;
    x = W16(player);
    y = W16((uint16_t)(0x204F + x));
    y = W16(0x204Fu + y);
    if (!(W16(y) & 0x4000) || !W16(pending))
        return;
    W16(pending) = 0;
    W8(flag) = 0x80;
    W8(0x1A1C + bob) = 0x1A;
    W8(0x1A1E + bob) = 0;
    W8(0x1A22 + bob) = 0;
    W8(0x1A20 + bob) = 0xFE;
}

void bob_windows_run(void)
{
    int built = 0;

    if (window_state || window_layout_none)
        return;
    link_event_trigger(0x0DA4, 0x0346, 0x1A24, 0);
    if (W8(0x0DA4) & 0x80) {
        W8(0x0D45) &= 0xDF;
        window_setup = 2;
        if (bob_window(0, 0x19AC)) {
            window_table_ffd1_or_ffd5();
            built = 1;
        }
    }
    /* the original reads the menu's flag as a word after building bands */
    if (!(built ? W16(0x0D62) : W8(0x0D62)))
        return;
    link_event_trigger(0x0DA6, 0x034A, 0x1A26, 1);
    if (W8(0x0DA6) & 0x80) {
        W8(0x0D45) &= 0xDF;
        window_setup = 2;
        if (bob_window(1, 0x19AF))
            window_table_ffd9();
    }
}

/* The sweep x at half speed: its place moves by its speed, turning round
   at $F2 and $E8; the sixteenth step ends it. */
static void sweep_step(uint8_t x)
{
    uint8_t v = (uint8_t)(W8(0x1A17 + x) + 1), c;

    if (v != 2) {
        W8(0x1A17 + x) = v;
        return;
    }
    c = (uint8_t)(W8(0x1A1B) + 1);
    W8(0x1A1B) = c;
    if (c == 0x10) {
        W8(0x0DA5) &= 0xEF;
        W8(0x1A17) = W8(0x1A18) = W8(0x1A1B) = 0;
        W8(0x1A19) = W8(0x1A1A) = 0xFE;
        W8(0x1A15) = W8(0x1A16) = 0xF0;
        return;
    }
    W8(0x1A17 + x) = 0;
    v = (uint8_t)(W8(0x1A15 + x) + W8(0x1A19 + x));
    W8(0x1A15 + x) = v;
    if (v == 0xF2 || v == 0xE8)
        W8(0x1A19 + x) = (uint8_t)-W8(0x1A19 + x);
}

void window_sweep_if_on(void)
{
    uint8_t v;

    if (window_state || window_layout_none || !(W8(0x0DA5) & 0x10))
        return;
    window_setup = 4;
    sweep_step(0);
    v = W8(0x1A15);
    if (v == 0xEE)
        return;
    window_values(v, v, v);
    window_table_ffd1_or_ffd5();
}

/* Oscillator x at the level in bits 5-7 of $1A11 (4 while bit 7 is
   set): a new level restarts its count; level 0 rests it at $40; else
   each time its count reaches the level's rate its place moves by its
   speed, turning round at $40 and $42. */
static void oscillator_step(uint8_t x)
{
    uint8_t level, c, v;

    if (W8(0x1A11) & 0x80) {
        W8(0x1A0E + x) = 0x80;
        level = 4;
    } else {
        level = W8(0x1A11) >> 5;
        if (level != W8(0x1A0E + x))
            W8(0x1A0A + x) = 0;
        W8(0x1A0E + x) = level;
        if (!level) {
            W8(0x1A0A + x) = 0;
            W8(0x1A08 + x) = 0x40;
            W8(0x1A0C + x) = 1;
            return;
        }
    }
    W8(0x1A10) = ram_code_byte((uint16_t)(0xD30D + (uint8_t)(level - 1)));
    c = (uint8_t)(W8(0x1A0A + x) + 1);
    if (c != W8(0x1A10)) {
        W8(0x1A0A + x) = c;
        return;
    }
    W8(0x1A0A + x) = 0;
    v = (uint8_t)(W8(0x1A08 + x) + W8(0x1A0C + x));
    W8(0x1A08 + x) = v;
    if (v == 0x40 || v == 0x42)
        W8(0x1A0C + x) = (uint8_t)-W8(0x1A0C + x);
}

/* The slide's place moved by `step`; at `stop` it ends. 1 if it moved. */
static int window_slide(uint8_t step, uint8_t stop)
{
    uint8_t v = (uint8_t)(W8(0x1A14) + step);

    if (v == stop) {
        W8(0x0DA5) &= 0xF9;
        return 0;
    }
    W8(0x1A14) = v;
    window_table_ffd1();
    window_values(0x20, 0x40, v);
    return 1;
}

/* The slide as bits 1-2 of $0DA5 ask: 2 down to $96, 4 or 6 up to $7F. */
static void window_slide_step(void)
{
    uint8_t v = W8(0x0DA5) & 6;

    if (v == 2)
        window_slide(0x01, 0x96);
    else if (v)
        window_slide(0xFF, 0x7F);
}

void window_slide_or_oscillate(void)
{
    if (window_state || window_layout_none)
        return;
    if (W8(0x0DA5) & 6) {
        window_setup = 2;
        window_slide_step();
        if (W8(0x0DA5) & 6)
            return;
    }
    if (W8(0x0DA5) & 8) {
        window_setup = 2;
        W8(0x1A11) = W8(0x0DDA);
        oscillator_step(0);
        window_values(0x20, W8(0x1A08), 0x95);
        window_table_ffd1_or_ffd5();
    }
}

/* The animations' common part: setup 8 with the values $19E0. */
static void window_cover(void)
{
    uint8_t v = W8(0x19E0);
    int i;

    W8(0x0D45) &= 0xDF;
    window_setup = 8;
    for (i = 0; i < 6; i++)
        W8(0x19AC + i) = v;
}

/* The bands opening (state 1) or closing (2), then moving (4, 5): $19D4
   by $19D6 to $19D9, the table rebuilt for entry $19D5 each step. */
void window_bands_animate(void)
{
    uint8_t s;
    uint16_t a;

    if (W8(0x19DF) || !(s = window_state))
        return;
    if (s == 1 || s == 2) {
        int menu = W8(0x0D62) != 0;

        if (s == 1) {
            W16(0x19D6) = 0x0080;
            W16(0x19D4) = 0;
            W16(0x19D9) = menu ? 0x0780 : 0x0C80;
        } else {
            W16(0x19D6) = 0xFF80;
            W16(0x19D4) = menu ? 0x0700 : 0x0C00;
            W16(0x19D9) = 0;
        }
        W16(0x0DA4) = 0;
        window_state = 4;
    } else if (s != 4 && s != 5)
        return;
    window_cover();
    a = (uint16_t)(W16(0x19D4) + W16(0x19D6));
    if (a != W16(0x19D9))
        W16(0x19D4) = a;
    else if (!W8(0x19D5)) {
        window_state = 0;
        return;
    } else
        window_state = 5;
    if (W8(0x0D62))
        window_table_preset((uint8_t)(W8(0x19D5) << 1));
    else
        window_table_from_list((uint8_t)(W8(0x19D5) << 1));
}

/* The split opening (state 1) or closing (2), then moving (4, 5): $19DB
   by $19DD to $19D9, the bands about the line its height leaves; at the
   end the full bands (state 5) when open, none (0) when closed. */
void window_split_animate(void)
{
    uint8_t s, h;
    uint16_t a;

    if (W8(0x19DF) != 1 || !(s = window_state))
        return;
    if (s == 1 || s == 2) {
        if (s == 1) {
            W16(0x19DD) = 0x0400, W16(0x19DB) = 0x0000, W16(0x19D9) = 0x7400;
            W16(0x19E2) = 0xF50B, W16(0x19E4) = 0;
        } else {
            W16(0x19DD) = 0xFC00, W16(0x19DB) = 0x8000, W16(0x19D9) = 0x0800;
            W16(0x19E4) = 0x0BF5, W16(0x19E2) = 0;
        }
        W16(0x0DA4) = 0;
        window_state = 4;
    } else if (s != 4 && s != 5)
        return;
    window_cover();
    a = (uint16_t)(W16(0x19DB) + W16(0x19DD));
    if (a != W16(0x19D9)) {
        W16(0x19DB) = a;
        h = W8(0x19DC);
        window_table_at((uint8_t)((uint8_t)(0xFF - h - h) >> 1));
    } else if (W16(0x19DB) >= 0x7000) {
        window_table_ffdd();
        window_state = 5;
    } else
        window_state = 0;
}

/* ---- the fixed colour's bands ---- */

/* The band count at `at` moved by the step at `step`, unless that gives
   the first limit of the pair at `limits` or falls below the second. */
static void band_step(uint16_t at, uint16_t step, uint16_t limits)
{
    uint8_t v = (uint8_t)(W8(at) + W8(step));

    if (v == ram_code_byte(limits) || v < ram_code_byte((uint16_t)(limits + 1)))
        return;
    W8(at) = v;
}

/* The fixed colour's level $19C3 (its whole part $E0-$EF) by $19C5,
   unless that gives $E000 or $EF00; each band of the table at or above
   the new whole part comes down to it. */
static void coldata_level_step(void)
{
    uint16_t a = (uint16_t)(W16(0x19C3) + W16(0x19C5));
    uint8_t x, v;

    if (a == 0xE000 || a == 0xEF00)
        return;
    W16(0x19C3) = a;
    for (x = 0; (v = ram_code_byte((uint16_t)(0xD5B3 + x))) != 0xFF; x = (uint8_t)(x + 2))
        if (v >= W8(0x19C4))
            W8(0x1FF8Eu + x) = W8(0x19C4);
}

/* The lower band's count: its limits wider while the object two links on
   from the player's has bit 14 set (each change restarting it, at $40
   when set, $20 when clear). */
static void coldata_low_band_step(void)
{
    uint16_t y = W16((uint16_t)(0x204F + W16(0x0346)));
    uint8_t pair = 0;

    y = W16(0x204Fu + y);
    if (W16(y) & 0x4000) {
        if (!W8(0x19D3))
            W8(0x19D3) = 1, W8(0x19CF) = 0x40;
        pair = W8(0x02);
    } else if (W8(0x19D3))
        W8(0x19D3) = 0, W8(0x19CF) = 0x20;
    band_step(0x19CF, 0x19CD, (uint16_t)(0xD56E + pair));
}

/* The sliding window: while one is under way (bit 0 of $0DA5), setup 6
   with the values $E0, the bands of $FFD1 once the level is $E1 or more,
   the level stepped; at its end the slide stops; otherwise the bands
   follow the player's height ($200E), up or down as it moved. */
void window_slide_check(void)
{
    uint16_t m;

    if (window_state || window_layout_none || !(W8(0x0DA5) & 1))
        return;
    window_values(0xE0, 0xE0, 0xE0);
    if (W8(0x19C4) >= 0xE1)
        window_table_ffd1();
    window_setup = 6;
    coldata_level_step();
    if ((uint16_t)(W16(0x19C3) + W16(0x19C5)) == 0xE000) {
        W8(0x0DA5) &= 0xFE;
        W8(0x0D45) &= 0xDF;
        return;
    }
    m = W16(0x200Eu + W16(0x0346));
    if (W16(0x19C9) != m) {
        int down = W16(0x19C9) >= m;

        W8(0x19C7) = down ? 0xFE : 0x02;
        W8(0x19CD) = down ? 0x01 : 0xFF;
        band_step(0x19CB, 0x19C7, (uint16_t)(0xD5AA + W8(0x19D1)));
        coldata_low_band_step();
    }
    W8(0x1FF8B) = W8(0x19CB);
    W8(0x1FFAB) = W8(0x19CF);
    W16(0x19C9) = W16(0x200Eu + W16(0x0346));
    W8(0x0D45) |= 0x20;
}

/* The split window's bands in bank $7F from its four scaled edges ($3433
   left, $3435 top, $3437 right, $3439 bottom, $3434 and $3438 the
   width's ends): the width and its middle, the top line, the left edges
   stepping in a line a band and the right ones out, mirrored, and the
   middle bands' height ((bottom - top - 6) / 2, a borrow taking one
   more). */
void window_table_7f(void)
{
    uint8_t a = (uint8_t)(W8(0x13438) - W8(0x13434)), b;
    int i;
    static const uint16_t left[4][2] = {{0x3420, 0x3423}, {0x341D, 0x3426}, {0x341A, 0x3429}, {0x3417, 0x342C}};
    static const uint16_t right[4][2] = {{0x3421, 0x3424}, {0x341E, 0x3427}, {0x341B, 0x342A}, {0x3418, 0x342D}};

    W8(0x13402) = a;
    W8(0x13406) = (uint8_t)((a >> 1) + W8(0x13434));
    a = W8(0x13435) >> 1;
    W8(0x13410) = a;
    W8(0x13413) = (uint8_t)(a + 1);
    for (i = 0, a = W8(0x13433); i < 4; i++, a++)
        W8(0x10000u + left[i][0]) = W8(0x10000u + left[i][1]) = a;
    for (i = 0, a = W8(0x13437); i < 4; i++, a--)
        W8(0x10000u + right[i][0]) = W8(0x10000u + right[i][1]) = a;
    b = W8(0x13435);
    a = (uint8_t)(W8(0x13439) - b - 6 - (W8(0x13439) < b));
    W8(0x1341F) = W8(0x13422) = a >> 1;
}
