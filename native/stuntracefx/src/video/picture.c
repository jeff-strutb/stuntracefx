/*
 * picture.c: the screen as the game has described it, handed to the engine
 * once per picture.
 */
#include <string.h>

#include "system/state.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

void menu_lines_send(void);

Picture pic;
MenuVideo menu_video;

void video_init(void)
{
    menu_video.tiles_a = nshvc_tiles_create(4, 1024, 1);
    menu_video.tiles_b = nshvc_tiles_create(4, 1024, 1);
    menu_video.tiles_text = nshvc_tiles_create(2, 512, 1);
    menu_video.sprites = nshvc_tiles_create(4, 512, 1);
    menu_video.page_0 = nshvc_map_create(32, 32);
    menu_video.pages_0_1 = nshvc_map_create(64, 32);
    menu_video.pages_1_2 = nshvc_map_create(64, 32);
    menu_video.pages_2_3 = nshvc_map_create(64, 32);
    menu_video.text = nshvc_map_create(32, 32);
    race_video_init();
    pic.blank = 1;
}

void menu_page_write(int page, const void *words)
{
    const uint16_t *w = (const uint16_t *)words;

    if (page == 0) {
        nshvc_map_write_snes(menu_video.page_0, 0, 0, 32, 32, w);
        nshvc_map_write_snes(menu_video.pages_0_1, 0, 0, 32, 32, w);
    } else if (page == 1) {
        nshvc_map_write_snes(menu_video.pages_0_1, 32, 0, 32, 32, w);
        nshvc_map_write_snes(menu_video.pages_1_2, 0, 0, 32, 32, w);
    } else if (page == 2) {
        nshvc_map_write_snes(menu_video.pages_1_2, 32, 0, 32, 32, w);
        nshvc_map_write_snes(menu_video.pages_2_3, 0, 0, 32, 32, w);
    } else {
        nshvc_map_write_snes(menu_video.pages_2_3, 32, 0, 32, 32, w);
    }
}

static void set_background(int n)
{
    const Background *b = &pic.bg[n];
    nshvc_layer l;

    memset(&l, 0, sizeof l);
    l.enabled_main = (pic.main_screen >> n) & 1;
    l.enabled_sub = (pic.sub_screen >> n) & 1;
    l.tiles = b->tiles;
    l.map = b->map;
    l.draw_list = b->draw_list;
    l.tile_size = 8;
    l.scroll_x = b->scroll_x;
    l.scroll_y = b->scroll_y;
    l.mosaic = 1;
    l.wrap = 1;
    nshvc_layer_set((uint8_t)n, &l);
}

/* ---- the race screen's lines ---- */

/* The rows the screen is lit on, by screen: one player one band, two players two. */
static void blank_lines_send(void)
{
    int32_t v[224];
    int r;

    for (r = 0; r < 224; r++) {
        if (pic.race.split)
            v[r] = !((r >= 15 && r <= 110) || (r >= 119 && r <= 214));
        else
            v[r] = !(r >= 17 && r <= 207);
    }
    nshvc_lines(0, 224, NSHVC_LINE_BLANK, v);
}

/* The gradient: colour 1 by row. One player: row 0's value held to row 31,
   then the second list row by row to row 158. Two players: the first band's
   start held, each half's list row by row. */
static void gradient_lines_send(void)
{
    int32_t v[224];
    int r;

    if (pic.race.split) {
        for (r = 0; r < 23; r++)
            v[r] = gradient_colour(0);
        for (; r < 103; r++)
            v[r] = gradient_colour((uint32_t)pic.race.gradient_start[0] + (uint32_t)(r - 23));
        for (; r < 127; r++)
            v[r] = gradient_colour(0);
        for (; r < 207; r++)
            v[r] = gradient_colour((uint32_t)pic.race.gradient_start[1] + (uint32_t)(r - 127));
        for (; r < 224; r++)
            v[r] = v[r - 1];
    } else {
        for (r = 0; r < 32; r++)
            v[r] = gradient_colour(0);
        for (; r < 159; r++)
            v[r] = gradient_colour((uint32_t)pic.race.gradient_start[0] + (uint32_t)(r - 32));
        for (; r < 224; r++)
            v[r] = v[r - 1];
    }
    nshvc_lines(0, 224, (nshvc_line_field)(NSHVC_LINE_PALETTE + 1), v);
}

/* The window bands: a row's edges for each line of the table. */
static void window_lines_send(void)
{
    int32_t l1[224], r1[224], l2[224], r2[224];
    int band, line = 0;

    memset(l1, 0, sizeof l1);
    memset(r1, 0, sizeof r1);
    memset(l2, 0, sizeof l2);
    memset(r2, 0, sizeof r2);
    for (band = 0; band < WINDOW_BANDS_MAX && windows.band[band].lines && line < 224; band++) {
        uint8_t c = windows.band[band].lines;
        int repeat = c >= 0x81, n = repeat ? c - 0x80 : c, i;
        const WindowEdges *e = window_edges_at(windows.band[band].edges);

        for (i = 0; i < n && line < 224; i++, line++) {
            l1[line] = e->left1;
            r1[line] = e->right1;
            l2[line] = e->left2;
            r2[line] = e->right2;
            if (repeat)
                e = window_edges_at((uint16_t)(windows.band[band].edges + (uint16_t)(4 * (i + 1))));
        }
    }
    for (; line < 224; line++) {
        l1[line] = line ? l1[line - 1] : 0;
        r1[line] = line ? r1[line - 1] : 0;
        l2[line] = line ? l2[line - 1] : 0;
        r2[line] = line ? r2[line - 1] : 0;
    }
    nshvc_lines(0, 224, NSHVC_LINE_WIN1_LEFT, l1);
    nshvc_lines(0, 224, NSHVC_LINE_WIN1_RIGHT, r1);
    nshvc_lines(0, 224, NSHVC_LINE_WIN2_LEFT, l2);
    nshvc_lines(0, 224, NSHVC_LINE_WIN2_RIGHT, r2);
}

/* The fixed colour by line from its table: each entry a line count and a
   byte naming colour channels, added on top of what the rows above left. */
static void fixed_lines_send(uint16_t start)
{
    int32_t v[224];
    int i = 0, line = 0;
    int r = start & 31, gc = (start >> 5) & 31, b = (start >> 10) & 31;

    memset(v, 0, sizeof v);
    while (i + 1 < windows.level_table_bytes && windows.level_table[i] && line < 224) {
        int n = windows.level_table[i];
        uint8_t d = windows.level_table[i + 1];

        if (d & 0x80)
            b = d & 31;
        if (d & 0x40)
            gc = d & 31;
        if (d & 0x20)
            r = d & 31;
        while (n-- && line < 224)
            v[line++] = r | gc << 5 | b << 10;
        i += 2;
    }
    for (; line < 224; line++)
        v[line] = line ? v[line - 1] : (r | gc << 5 | b << 10);
    nshvc_lines(0, 224, NSHVC_LINE_FIXED, v);
}

/* The lower half of a two-player screen: from row 112 its backdrop, or the
   finished player's panel, its colours and fixed colour. */
static void split_half_send(void)
{
    int32_t y[224];
    int r, h;

    for (r = 0; r < 224; r++) {
        h = r >= 112 ? 1 : 0;
        if (pic.race.panel[h])
            y[r] = 512 + (pic.race.backdrop_y[h] & 0xFF);
        else
            y[r] = ((pic.race.backdrop_y[h] + r + 1) & 511) - (r + 1);
    }
    nshvc_lines(0, 224, (nshvc_line_field)(NSHVC_LINE_SCROLL_Y + 1), y);
    nshvc_line(0, (nshvc_line_field)(NSHVC_LINE_SCROLL_X + 1), pic.race.backdrop_x[0]);
    nshvc_line(112, (nshvc_line_field)(NSHVC_LINE_SCROLL_X + 1), pic.race.backdrop_x[1]);
    for (r = 0; r < 3; r++)
        nshvc_line(112, (nshvc_line_field)(NSHVC_LINE_PALETTE + 0xE1 + r), pic.race.colours_kept[1][r]);
    nshvc_line(112, (nshvc_line_field)(NSHVC_LINE_PALETTE + 0), pic.race.colour_0[1]);
    nshvc_line(112, NSHVC_LINE_FIXED, pic.race.fixed[1]);
}

static void race_lines_send(void)
{
    blank_lines_send();
    gradient_lines_send();
    window_lines_send();
    if (g.hdma_enable & 0x20)
        fixed_lines_send(pic.blend.fixed_bgr15);
    if (pic.race.split)
        split_half_send();
}

void video_draw(void)
{
    nshvc_screen s;
    int i;

    memset(&s, 0, sizeof s);
    s.width = 256;
    s.height = 224;
    s.brightness = pic.brightness;
    s.blank = pic.blank;
    nshvc_screen_set(&s);
    if (pic.blank)
        return;
    nshvc_mode_set(pic.mode, pic.bg3_priority, 0);
    for (i = 0; i < 3; i++)
        set_background(i);
    nshvc_windows_set(&pic.windows);
    nshvc_blend_set(&pic.blend);
    nshvc_palette_write(0, 256, pic.palette);
    nshvc_sprites_show((pic.main_screen >> 4) & 1, (pic.sub_screen >> 4) & 1);
    nshvc_sprites_set(pic.sprites, pic.sprite_count, 0);
    if (pic.line_tables)
        menu_lines_send();
    if (pic.race.on)
        race_lines_send();
}
