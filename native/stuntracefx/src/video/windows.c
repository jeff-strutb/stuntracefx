/*
 * windows.c: the race's windows and colour maths: the bands of lines that set
 * the two windows' edges, the slides, sweeps and bobs that move them, the
 * fixed colour by line, and the five window and colour maths setups the
 * frames switch between.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "object/object.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

Windows windows;

/* The edge rows the band table's entries name by address: the normal edges,
   the named ones, the shapes built by WindowsInit and their copy. */
#define EDGE_SHAPES_ADDR 0xF883u
#define EDGE_COPY_ADDR 0xFB03u
#define EDGE_ROWS 160
static WindowEdges edge_shapes[EDGE_ROWS], edge_copy[EDGE_ROWS];

const WindowEdges *window_edges_at(uint16_t addr)
{
    switch (addr) {
    case 0xF857: return &windows.normal;
    case 0xFFD1: return &windows.ffd1;
    case 0xFFD5: return &windows.ffd5;
    case 0xFFD9: return &windows.ffd9;
    case 0xFFDD: return &windows.ffdd;
    default: break;
    }
    if (addr >= EDGE_SHAPES_ADDR && addr < EDGE_SHAPES_ADDR + EDGE_ROWS * 4)
        return &edge_shapes[(addr - EDGE_SHAPES_ADDR) / 4];
    if (addr >= EDGE_COPY_ADDR && addr < EDGE_COPY_ADDR + EDGE_ROWS * 4)
        return &edge_copy[(addr - EDGE_COPY_ADDR) / 4];
    return &windows.normal;
}

/* Set a band: `lines` of the original's count (bit 7 repeats a row per line),
   edges by address. */
static void band_set(int i, uint8_t lines, uint16_t edges_addr)
{
    windows.band[i].lines = lines;
    windows.band[i].edges = edges_addr;
}

static void band_end(int i)
{
    windows.band[i].lines = 0;
    windows.bands = (uint8_t)i;
}

/* ---- the table's fixed shapes ---- */

void WindowHdmaFFDD(void)
{
    band_set(0, 0x40, 0xFFDD);
    band_set(1, 0x40, 0xFFDD);
    band_set(2, 0x40, 0xFFDD);
    band_set(3, 0x3E, 0xFFDD);
    band_end(4);
}

void WindowHdmaFFD1(void)
{
    band_set(0, 0x20, 0xF857);
    band_set(1, 0x70, 0xFFD1);
    band_set(2, 0x10, 0xFFD1);
    band_set(3, 0x08, 0xF857);
    band_end(4);
}

/* Three bands, 16, 48 and 16 lines, at band `at`; the entries around them stay. */
static void window_bands_3(int at, uint16_t addr)
{
    band_set(at, 0x10, addr);
    band_set(at + 1, 0x30, addr);
    band_set(at + 2, 0x10, addr);
}

void WindowHdmaFFD5(void)
{
    window_bands_3(1, 0xFFD5);
}

void WindowHdmaFFD9(void)
{
    window_bands_3(5, 0xFFD9);
}

/* normal edges over the screen, unless the bands animation or a moving layout has it */
void WindowHdmaNormal(void)
{
    int i;

    if (windows.bands_state || windows.layout_moving == 0xFF)
        return;
    for (i = 0; i < 4; i++)
        band_set(i, 0x40, 0xF857);
    band_end(4);
}

void WindowHdmaBands(void)
{
    static const uint8_t lines[9] = {0x17, 0x10, 0x30, 0x10, 0x18, 0x10, 0x30, 0x10, 0x01};
    int i;

    if (windows.bands_state)
        return;
    for (i = 0; i < 9; i++)
        band_set(i, lines[i], 0xF857);
    band_end(9);
}

/* The layout's three bands; the middle's edges and the counts from the tables. */
static void window_hdma_from_table(void)
{
    uint16_t x = (uint16_t)windows.layout;

    band_set(0, ram_code_7e8((uint16_t)(0xE0B4 + x)), 0xF857);
    band_set(1, ram_code_7e8((uint16_t)(0xE0B5 + x)), ram_code_7e16((uint16_t)(0xE0D2 + x)));
    band_set(2, 0x30, 0xF857);
    band_end(3);
}

/* the layout for one screen or the split one */
static void window_hdma_layout(void)
{
    uint16_t x = (uint16_t)windows.layout;

    if (!g.pause_player) {
        band_set(0, ram_code_7e8((uint16_t)(0xE0C2 + x)), 0xF857);
        band_set(1, ram_code_7e8((uint16_t)(0xE0C3 + x)), ram_code_7e16((uint16_t)(0xE0D2 + x)));
        band_set(2, 0x10, 0xF857);
        band_end(3);
    } else {
        uint8_t n = ram_code_7e8((uint16_t)(0xE0CA + x));

        band_set(0, n, 0xF857);
        band_set(1, n, 0xF857);
        band_set(2, ram_code_7e8((uint16_t)(0xE0CB + x)), ram_code_7e16((uint16_t)(0xE0E0 + x)));
        band_set(3, 0x10, 0xF857);
        band_end(4);
    }
}

void WindowLayoutStepForScreen(void)
{
    uint16_t pos, limit, y = g.split_screen ? 2 : 0;
    uint8_t req;

    g.hdma_enable &= (uint8_t)~0x20;
    req = windows.layout_request;
    if (req == 0x01 || req == 0xFF) {
        windows.layout_moving = 0xFF;
        windows.layout_step = req == 0x01 ? 2 : -2;
    } else if (windows.layout_moving != 0xFF) {
        return;
    }
    windows.layout_request = 0;
    pos = (uint16_t)(windows.layout + windows.layout_step);
    if (pos == 0xFFFE) {
        windows.layout_moving = 0;
        return;
    }
    limit = ram_code_7e16((uint16_t)(0xDBC5 + y));
    if (pos != limit)
        windows.layout = (int16_t)pos;
    windows.setup = 0;
    memset(windows.colours, 0xE7, sizeof windows.colours);
    if (y == 0)
        window_hdma_from_table();
    else
        window_hdma_layout();
}

/* four bands around a line: the middle two normal, the outer two $FFDD */
static void window_hdma_at(uint8_t line)
{
    uint8_t h = (uint8_t)(windows.split_pos >> 8);

    band_set(0, (uint8_t)(h - 1 + windows.split_bottoms[0]), 0xFFDD);
    band_set(1, (uint8_t)(line + windows.split_bottoms[1]), 0xF857);
    band_set(2, (uint8_t)(line + windows.split_tops[1]), 0xF857);
    band_set(3, (uint8_t)(h + windows.split_tops[0]), 0xFFDD);
    band_end(4);
}

/* One normal band of 32 lines, then the entries of the list: count and the
   address of its edges, until a count of 0. */
static void window_hdma_from_list(uint8_t select)
{
    const uint8_t *p = ram_code_7e(ram_code_7e16((uint16_t)(0xD8DE + select)));
    int i = 1;

    band_set(0, 0x20, 0xF857);
    while (*p && i < WINDOW_BANDS_MAX - 1) {
        band_set(i++, p[0], (uint16_t)(p[1] | p[2] << 8));
        p += 3;
    }
    band_end(i);
}

/* A preset of three entries, at entries 5 on with the split lines set, else 1 on. */
static void window_hdma_preset(uint8_t index)
{
    const uint8_t *p;
    int i, at;

    if (g.pause_player) {
        p = ram_code_7e(ram_code_7e16((uint16_t)(0xDA1D + index)));
        at = 5;
    } else {
        p = ram_code_7e(ram_code_7e16((uint16_t)(0xD9C5 + index)));
        at = 1;
    }
    for (i = 0; i < 3; i++, p += 3)
        band_set(at + i, p[0], (uint16_t)(p[1] | p[2] << 8));
}

/* ---- the moving windows ---- */

static void slide_step(int up)
{
    uint8_t v = (uint8_t)(windows.slide_pos + (up ? 0xFF : 1)), stop = up ? 0x7F : 0x96;

    if (v == stop) {
        g.picture_effects &= 0xF9;
        return;
    }
    windows.slide_pos = v;
    WindowHdmaFFD1();
    windows.colours[1] = 0x40;
    windows.colours[0] = 0x20;
    windows.colours[2] = v;
}

/* The oscillator's value by the level, every `rate` steps moving by its step,
   turning round at $40 and $42. */
static void oscillator_step(int x)
{
    uint8_t in = windows.oscillator_input, level = (uint8_t)(in >> 5), v;

    if (in & 0x80) {
        windows.oscillator_level[x] = 0x80;
        level = 4;
    } else {
        if (level != windows.oscillator_level[x])
            windows.oscillator_count[x] = 0;
        windows.oscillator_level[x] = level;
        if (!level) {
            windows.oscillator_count[x] = 0;
            windows.oscillator[x] = 0x40;
            windows.oscillator_step[x] = 1;
            return;
        }
    }
    windows.oscillator_rate = ram_code_7e8((uint16_t)(0xD30D + level - 1));
    v = (uint8_t)(windows.oscillator_count[x] + 1);
    if (v != windows.oscillator_rate) {
        windows.oscillator_count[x] = v;
        return;
    }
    windows.oscillator_count[x] = 0;
    v = (uint8_t)(windows.oscillator[x] + windows.oscillator_step[x]);
    windows.oscillator[x] = v;
    if (v == 0x40 || v == 0x42)
        windows.oscillator_step[x] = (uint8_t)(0 - windows.oscillator_step[x]);
}

static void bands_for_screen(void)
{
    if (g.split_screen)
        WindowHdmaFFD5();
    else
        WindowHdmaFFD1();
}

static void oscillator_window(void)
{
    windows.oscillator_input = g.status_level[0];
    oscillator_step(0);
    windows.colours[1] = windows.oscillator[0];
    windows.colours[0] = 0x20;
    windows.colours[2] = 0x95;
    bands_for_screen();
}

/* Slide, else oscillate. */
void WindowSlideOrOscillate(void)
{
    if (windows.bands_state || windows.layout_moving == 0xFF)
        return;
    if (g.picture_effects & 0x06) {
        windows.setup = 2;
        if ((g.picture_effects & 0x06) == 0x02)
            slide_step(0);
        else
            slide_step(1);
        if (g.picture_effects & 0x06)
            return;
    }
    if (g.picture_effects & 0x08) {
        windows.setup = 2;
        oscillator_window();
    }
}

static void sweep_step(int x)
{
    uint8_t v = (uint8_t)(windows.sweep_halve[x] + 1);

    if (v != 2) {
        windows.sweep_halve[x] = v;
        return;
    }
    windows.sweep_steps++;
    if (windows.sweep_steps == 0x10) {
        windows.sweep_steps = 0;
        g.picture_effects &= 0xEF;
        windows.sweep_halve[0] = windows.sweep_halve[1] = 0;
        windows.sweep_step[0] = windows.sweep_step[1] = 0xFE;
        windows.sweep[0] = windows.sweep[1] = 0xF0;
        return;
    }
    windows.sweep_halve[x] = 0;
    v = (uint8_t)(windows.sweep[x] + windows.sweep_step[x]);
    windows.sweep[x] = v;
    if (v == 0xF2 || v == 0xE8)
        windows.sweep_step[x] = (uint8_t)(0 - windows.sweep_step[x]);
}

void WindowSweepIfOn(void)
{
    uint8_t v;

    if (windows.bands_state || windows.layout_moving == 0xFF || !(g.picture_effects & 0x10))
        return;
    windows.setup = 4;
    sweep_step(0);
    v = windows.sweep[0];
    if (v == 0xEE)
        return;
    windows.colours[0] = v;
    windows.colours[2] = v;
    windows.colours[1] = v;
    bands_for_screen();
}

/* ---- the fixed colour's bands ---- */

/* Bytes of the fixed-colour table: a line count and a value per band. */
static void band_step(uint8_t *at, int8_t step, uint16_t limits)
{
    uint8_t v = (uint8_t)(*at + (uint8_t)step);

    if (v == ram_code_7e8(limits) || v < ram_code_7e8((uint16_t)(limits + 1)))
        return;
    *at = v;
}

static void level_step(void)
{
    uint16_t s = (uint16_t)(windows.level + (uint16_t)windows.level_step);
    uint8_t whole;
    int x;

    if (s == 0xE000 || s == 0xEF00)
        return;
    windows.level = s;
    whole = (uint8_t)(s >> 8);
    for (x = 0;; x += 2) {
        uint8_t v = ram_code_7e8((uint16_t)(0xD5B3 + x));

        if (v == 0xFF)
            break;
        if (v >= whole)
            windows.level_table[x + 3] = whole;
    }
}

/* Whether the vehicle of the player is flagged (bit 14 of its camera's mode) */
static int player_vehicle_flagged(void)
{
    const Object *car = obj_at(g.player_car);
    const Object *next = obj_at(car->link);

    return (camera_of(next->link)->mode & 0x4000) != 0;
}

void WindowSlideCheck(void)
{
    if (windows.bands_state || windows.layout_moving == 0xFF || !(g.picture_effects & 0x01))
        return;
    windows.colours[0] = windows.colours[1] = windows.colours[2] = 0xE0;
    if ((uint8_t)(windows.level >> 8) >= 0xE1)
        WindowHdmaFFD1();
    windows.setup = 6;
    level_step();
    if ((uint16_t)(windows.level + (uint16_t)windows.level_step) == 0xE000) {
        g.picture_effects &= 0xFE;
        g.hdma_enable &= (uint8_t)~0x20;
        return;
    }
    {
        uint16_t y = obj_at(g.player_car)->y;

        if (windows.player_y != y) {
            int down = windows.player_y >= y;

            windows.top_step = down ? -2 : 2;
            windows.low_step = down ? 1 : -1;
            band_step(&windows.top_band, windows.top_step, (uint16_t)(0xD5AA + windows.band_limits));
            {
                int flagged = player_vehicle_flagged();

                if (flagged) {
                    if (!windows.player_raised) {
                        windows.player_raised = 1;
                        windows.low_band = 0x40;
                    }
                } else if (windows.player_raised) {
                    windows.player_raised = 0;
                    windows.low_band = 0x20;
                }
                band_step(&windows.low_band, windows.low_step, (uint16_t)(0xD56E + (flagged ? 2 : 0)));
            }
        }
        windows.level_table[0] = windows.top_band;
        windows.level_table[0x20] = windows.low_band;
        windows.player_y = y;
        g.hdma_enable |= 0x20;
    }
}

/* ---- bobs ---- */

static void bob_step(int x)
{
    uint8_t v = (uint8_t)(g.bob_phase[x] + 1);

    if (v != 2) {
        g.bob_phase[x] = v;
        return;
    }
    v = (uint8_t)(g.bob_22[x] + 1);
    g.bob_22[x] = v;
    if (v == 0x0A) {
        g.bob_22[x] = 0;
        g.view_event[x] &= 0x7F;
        return;
    }
    g.bob_phase[x] = 0;
    v = (uint8_t)(g.bob_kind[x] + g.bob_speed[x]);
    g.bob_kind[x] = v;
    if (v == 0x1C || v == 0x16)
        g.bob_speed[x] = (uint8_t)(0 - g.bob_speed[x]);
}

static int bob_window(int x, int at)
{
    uint8_t v;

    bob_step(x);
    v = g.bob_kind[x];
    if (v == 0x18)
        return 0;
    windows.colours[at + 2] = (uint8_t)(v | 0x80);
    windows.colours[at + 1] = (uint8_t)((v | 0x40) - 2);
    windows.colours[at] = 0x20;
    return 1;
}

void ObjectLinkEventTrigger(void);
void ObjectLinkEventTrigger2(void);

void BobWindowsRun(void)
{
    if (windows.bands_state || windows.layout_moving == 0xFF)
        return;
    ObjectLinkEventTrigger();
    if (g.view_event[0] & 0x80) {
        g.hdma_enable &= (uint8_t)~0x20;
        windows.setup = 2;
        if (bob_window(0, 0))
            bands_for_screen();
    }
    if (!g.split_screen)
        return;
    ObjectLinkEventTrigger2();
    if (g.view_event[1] & 0x80) {
        g.hdma_enable &= (uint8_t)~0x20;
        windows.setup = 2;
        if (bob_window(1, 3))
            WindowHdmaFFD9();
    }
}

/* ---- the opening and closing animations ---- */

void WindowBandsAnimate(void)
{
    uint8_t s;

    if (windows.bands_kind || !windows.bands_state)
        return;
    s = windows.bands_state;
    if (s == 1 || s == 2) {
        int fast = g.split_screen != 0;

        if (s == 1) {
            windows.bands_step = 0x0080;
            windows.bands_pos = 0;
            windows.bands_end = fast ? 0x0780 : 0x0C80;
        } else {
            windows.bands_step = 0xFF80;
            windows.bands_pos = fast ? 0x0700 : 0x0C00;
            windows.bands_end = 0;
        }
        g.view_event[0] = 0;
        g.picture_effects = 0;
        windows.bands_state = 4;
    } else if (s != 4 && s != 5) {
        return;
    }
    g.hdma_enable &= (uint8_t)~0x20;
    windows.setup = 8;
    memset(windows.colours, windows.windows_colour, sizeof windows.colours);
    {
        uint16_t a = (uint16_t)(windows.bands_pos + windows.bands_step);

        if (a != windows.bands_end) {
            windows.bands_pos = a;
        } else {
            if (!(windows.bands_pos >> 8)) {
                windows.bands_state = 0;
                return;
            }
            windows.bands_state = 5;
        }
    }
    {
        uint8_t index = (uint8_t)((windows.bands_pos >> 8) << 1);

        if (g.split_screen)
            window_hdma_preset(index);
        else
            window_hdma_from_list(index);
    }
}

void WindowSplitAnimate(void)
{
    uint8_t s, v;

    if (windows.bands_kind != 1 || !windows.bands_state)
        return;
    s = windows.bands_state;
    if (s == 1) {
        windows.split_step = 0x0400;
        windows.split_pos = 0;
        windows.bands_end = 0x7400;
        windows.split_tops[0] = 0x0B;
        windows.split_tops[1] = 0xF5;
        windows.split_bottoms[0] = 0;
        windows.split_bottoms[1] = 0;
    } else if (s == 2) {
        windows.split_step = 0xFC00;
        windows.split_pos = 0x8000;
        windows.bands_end = 0x0800;
        windows.split_bottoms[0] = 0xF5;
        windows.split_bottoms[1] = 0x0B;
        windows.split_tops[0] = 0;
        windows.split_tops[1] = 0;
    } else if (s != 4 && s != 5) {
        return;
    }
    if (s == 1 || s == 2) {
        g.view_event[0] = 0;
        g.picture_effects = 0;
        windows.bands_state = 4;
    }
    g.hdma_enable &= (uint8_t)~0x20;
    windows.setup = 8;
    memset(windows.colours, windows.windows_colour, sizeof windows.colours);
    {
        uint16_t a = (uint16_t)(windows.split_pos + windows.split_step);

        if (a != windows.bands_end) {
            uint8_t h;

            windows.split_pos = a;
            h = (uint8_t)(a >> 8);
            v = (uint8_t)(((uint8_t)(0xFF - h - h)) >> 1);
            window_hdma_at(v);
            return;
        }
    }
    if (windows.split_pos >= 0x7000) {
        WindowHdmaFFDD();
        windows.bands_state = 5;
    } else {
        windows.bands_state = 0;
    }
}

/* The wipe that opens a course: its table of counts and edges rebuilt from
   the scaled edges, each band one step in on both sides. */
void WindowTable7F(void)
{
    uint8_t *t = windows.wipe_table;
    uint8_t a;

    windows.wipe_width = (uint8_t)(windows.wipe_b - windows.wipe_a);
    windows.wipe_middle = (uint8_t)((windows.wipe_width >> 1) + windows.wipe_a);
    a = (uint8_t)(windows.wipe_top >> 1);
    t[0x00] = a;
    t[0x03] = (uint8_t)(a + 1);
    a = windows.wipe_left;
    t[0x10] = a;
    t[0x13] = a;
    a++;
    t[0x0D] = a;
    t[0x16] = a;
    a++;
    t[0x0A] = a;
    t[0x19] = a;
    a++;
    t[0x07] = a;
    t[0x1C] = a;
    a = windows.wipe_right;
    t[0x11] = a;
    t[0x14] = a;
    a--;
    t[0x0E] = a;
    t[0x17] = a;
    a--;
    t[0x0B] = a;
    t[0x1A] = a;
    a--;
    t[0x08] = a;
    t[0x1D] = a;
    a = (uint8_t)(windows.wipe_bottom - windows.wipe_top);
    a = (uint8_t)(a - 6 - (windows.wipe_bottom >= windows.wipe_top ? 0 : 1));
    a >>= 1;
    t[0x0F] = a;
    t[0x12] = a;
}

/* One step of the wipe: the colour to $1F, the four edges moving out; when
   two have stopped the fade starts. */
void WindowEdgesStep(void)
{
    uint8_t stops = 0, opposite;
    unsigned s;
    int left_by;

    if (windows.fixed_level < 0x1F)
        windows.fixed_level++;
    opposite = (uint8_t)(0 - windows.wipe_right);
    if (opposite == windows.wipe_left) {
        s = windows.wipe_right + 2u;
        left_by = 2;
    } else if (opposite < windows.wipe_left) {
        s = windows.wipe_right + 2u;
        left_by = 3;
    } else {
        s = windows.wipe_right + 3u;
        left_by = 2;
    }
    if (s > 0xFF) {
        stops++;
        s = 0xFF;
    }
    windows.wipe_right = (uint8_t)s;
    if (windows.wipe_left < left_by) {
        stops++;
        windows.wipe_left = 1;
    } else {
        windows.wipe_left = (uint8_t)(windows.wipe_left - left_by);
    }
    {
        uint8_t r = (uint8_t)(windows.wipe_bottom + 1);

        if (r >= 0xE0) {
            stops++;
            r = 0xE0;
        }
        windows.wipe_bottom = r;
    }
    if (windows.wipe_top < 3) {
        stops++;
        windows.wipe_top = 1;
    } else {
        windows.wipe_top = (uint8_t)(windows.wipe_top - 3);
    }
    if (stops >= 2) {
        g.fade_level = 0x80;
        g.fade_out = 1;
    }
}

/* ---- the setups ---- */

/* The window choices of the setups below, for each layer; the masks are the
   original's TMW bits. */
static void masks_main(uint8_t bits)
{
    int i;

    for (i = 0; i < 5; i++) {
        pic.windows.mask_main[i] = (bits >> i) & 1;
        pic.windows.mask_sub[i] = 0;
    }
}

void WindowsColourAddBg12(void)
{
    memset(pic.windows.layer_use, 0, sizeof pic.windows.layer_use);
    pic.windows.layer_use[0] = NSHVC_WIN1_INVERT | NSHVC_WIN2;
    pic.windows.layer_use[1] = NSHVC_WIN1_INVERT | NSHVC_WIN2;
    pic.windows.layer_use[4] = NSHVC_WIN_AND;
    pic.windows.layer_use[5] = NSHVC_WIN1 | NSHVC_WIN1_INVERT;
    pic.blend.use_fixed = 0;
    pic.blend.clip = 0;
    pic.blend.prevent = 2;
    pic.blend.subtract = 0;
    pic.blend.half = 0;
    pic.blend.layers = 0x03;
    masks_main(0x17);
}

void WindowsColourWindowOnly(void)
{
    memset(pic.windows.layer_use, 0, sizeof pic.windows.layer_use);
    pic.windows.layer_use[5] = NSHVC_WIN1;
    pic.blend.use_fixed = 1;
    pic.blend.clip = 0;
    pic.blend.prevent = 1;
    masks_main(0x03);
}

void WindowsAllInverted(void)
{
    int i;

    for (i = 0; i < 5; i++)
        pic.windows.layer_use[i] = NSHVC_WIN1;
    pic.windows.layer_use[5] = NSHVC_WIN1;
    pic.blend.use_fixed = 1;
    pic.blend.clip = 0;
    pic.blend.prevent = 1;
    pic.blend.subtract = 1;
    pic.blend.half = 0;
    pic.blend.layers = 0x3F;
    masks_main(0x1F);
}

/* Setups 2 and 6, which colour maths they use with the colour window. */
static void setup_colour_only(uint8_t subtract, uint8_t layers)
{
    WindowsColourWindowOnly();
    pic.blend.subtract = subtract;
    pic.blend.half = 0;
    pic.blend.layers = layers;
}

void WindowsColourSub81(void)
{
    memset(pic.windows.layer_use, 0, sizeof pic.windows.layer_use);
    pic.windows.layer_use[5] = NSHVC_WIN1;
    pic.blend.use_fixed = 1;
    pic.blend.clip = 0;
    pic.blend.prevent = 1;
    pic.blend.subtract = 1;
    pic.blend.half = 0;
    pic.blend.layers = 0x01;
    masks_main(0x01);
}

void WindowsSetupSelect(void)
{
    switch (windows.setup) {
    case 0: WindowsColourAddBg12(); break;
    case 2: setup_colour_only(1, 0x03); break;
    case 4: setup_colour_only(0, 0x03); break;
    case 6: WindowsColourSub81(); break;
    default: WindowsAllInverted(); break;
    }
}

/* ---- the colour maths and fixed colour ---- */

void ColourMathA3(void)
{
    windows.maths = 0xA3;
}

void ColourMathA7Start(void)
{
    sound_port1_push(0x04);
    windows.maths = 0xA7;
    windows.fixed_step = 0;
}

void ColourMathOff(void)
{
    windows.maths = 0;
}

uint8_t FixedColourFromStep(void)
{
    windows.fixed_level = cart8(0x03, (uint16_t)(0xFE7B + windows.fixed_step));
    return windows.fixed_level;
}

/* ---- the setup of a race ---- */

/* The edge rows from one list of pairs and its mirror image: each pair is
   the first window's left and the second window's left, mirrored about $80
   for the right edges. */
static void edges_from_pair(uint16_t pair, int row)
{
    uint8_t v0 = cart8(0x09, pair), v1 = cart8(0x09, (uint16_t)(pair + 1));
    WindowEdges *e = &edge_shapes[row];

    e->left1 = v0;
    e->right1 = (uint8_t)(0x100 - v0);
    e->left2 = v1;
    e->right2 = v1 == 0x81 ? 0x80 : (uint8_t)(0x100 - v1);
}

static uint16_t edges_from_list(uint16_t sel)
{
    int row = cart16(0x09, (uint16_t)(0xF155 + sel)) / 4, n = 0;
    uint16_t x = cart16(0x09, (uint16_t)(0xF98B + sel));

    for (;;) {
        edges_from_pair(x, row++);
        x = (uint16_t)(x + 2);
        n++;
        if (cart8(0x09, x) == 0xFF)
            break;
    }
    x = (uint16_t)(x - 2);
    while (n--) {
        edges_from_pair(x, row++);
        x = (uint16_t)(x - 2);
    }
    return (uint16_t)(sel + 2);
}

static void edges_from_rom(uint16_t rom, WindowEdges *to)
{
    to->left1 = cart8(0x09, rom);
    to->right1 = cart8(0x09, (uint16_t)(rom + 1));
    to->left2 = cart8(0x09, (uint16_t)(rom + 2));
    to->right2 = cart8(0x09, (uint16_t)(rom + 3));
}

void WindowsInit(void)
{
    uint16_t sel, w;
    int i;

    edges_from_rom(0xF999, &windows.normal);
    edges_from_rom(0xF99D, &windows.ffd1);
    edges_from_rom(0xF9A1, &windows.ffd5);
    edges_from_rom(0xF9A5, &windows.ffd9);
    edges_from_rom(0xF9A9, &windows.ffdd);
    for (i = 0; i < 16; i += 4) {
        uint8_t c = cart8(0x09, (uint16_t)(0xF163 + i));

        band_set(i / 4, c, (uint16_t)(cart8(0x09, (uint16_t)(0xF164 + i)) | cart8(0x09, (uint16_t)(0xF165 + i)) << 8));
    }
    for (sel = 0; sel != 0x000E; )
        sel = edges_from_list(sel);
    for (i = 0; (w = cart16(0x09, (uint16_t)(0xFB74 + i))) != 0xFFFF; i += 2) {
        windows.level_table[i] = (uint8_t)w;
        windows.level_table[i + 1] = (uint8_t)(w >> 8);
    }
    windows.level_table_bytes = (uint8_t)i;
    if (g.split_screen) {
        for (i = 0; i < EDGE_ROWS; i++) {
            WindowEdges *e = &edge_shapes[i];

            e->left1 = (uint8_t)(e->left1 + 0xD8);
            e->right1 = (uint8_t)(e->right1 + 0xD8);
            e->left2 = (uint8_t)(e->left2 + 0xD8);
            e->right2 = (uint8_t)(e->right2 + 0xD8);
            edge_copy[i].left1 = (uint8_t)(e->left1 + 0x50);
            edge_copy[i].right1 = (uint8_t)(e->right1 + 0x50);
            edge_copy[i].left2 = (uint8_t)(e->left2 + 0x50);
            edge_copy[i].right2 = (uint8_t)(e->right2 + 0x50);
        }
    }
    windows.layout = -2;
    windows.layout_step = -2;
    windows.layout_moving = 0;
    windows.layout_request = 0;
    windows.setup = 0;
    memset(windows.oscillator_count, 0, sizeof windows.oscillator_count);
    memset(windows.sweep_halve, 0, sizeof windows.sweep_halve);
    windows.oscillator_step[0] = windows.oscillator_step[1] = 1;
    windows.oscillator[0] = windows.oscillator[1] = 0x40;
    windows.sweep_step[0] = windows.sweep_step[1] = 0xFE;
    windows.sweep[0] = windows.sweep[1] = 0xF0;
    memset(windows.colours, 0, sizeof windows.colours);
    memset(windows.oscillator_level, 0, sizeof windows.oscillator_level);
    windows.sweep_steps = 0;
    g.bob_kind[0] = g.bob_kind[1] = 0x1A;
    memset(g.bob_phase, 0, sizeof g.bob_phase);
    memset(g.bob_22, 0, sizeof g.bob_22);
    g.bob_speed[0] = g.bob_speed[1] = 0xFE;
    if (g.hud_mode == 0)
        g.byte_19e6 = cart8(0x09, (uint16_t)(cart16(0x09, (uint16_t)(0xF115 + 2 * g.course_class)) + g.champ_stage));
    palettes.weather_value = 0;
    i = (uint8_t)(g.car[0] * 2);
    windows.band_limits = (uint8_t)i;
    windows.top_band = cart8(0x09, (uint16_t)(0xF14D + i));
    windows.player_raised = 0;
    windows.low_band = 0x17;
    windows.level_step = 0x0020;
    windows.level = 0xE020;
    windows.bands_end = 0;
    windows.slide_pos = 0x80;
    windows.bands_state = 0;
    windows.windows_colour = 0xFE;
    windows.bands_kind = 0;
    windows.split_pos = windows.split_step = 0;
    g.flag_19ed = 0;
    g.byte_19e8 = g.byte_19e9 = g.byte_19ea = g.byte_19ec = g.byte_19f2 = 0;
    g.hits_bcd = 0;
    g.byte_19f0 = 0;
    g.byte_1a06 = 0;
    g.byte_1a29 = 0;
    g.byte_1a07 = 0;
    g.byte_1a04 = 0xFF;
}
