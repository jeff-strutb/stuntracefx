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

static void layer_send(int n, const Background *b)
{
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

static void set_background(int n)
{
    layer_send(n, &pic.bg[n]);
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

/* The lower half of a two-player screen: from row 112 its colours and fixed
   colour (its backdrop is in the draw list); the sprites from row 15's column 239. */
static void split_half_send(void)
{
    int r;

    nshvc_line(0, NSHVC_LINE_SPRITES_MAIN, 0);
    nshvc_line_from(15, 239, NSHVC_LINE_SPRITES_MAIN, 1);
    for (r = 0; r < 3; r++)
        nshvc_line(112, (nshvc_line_field)(NSHVC_LINE_PALETTE + 0xE1 + r), pic.race.colours_kept[1][r]);
    nshvc_line(112, (nshvc_line_field)(NSHVC_LINE_PALETTE + 0), pic.race.colour_0[1]);
    nshvc_line(112, NSHVC_LINE_FIXED, pic.race.fixed[1]);
}

/* The course screen: rows to $D8 lit; BG2's scroll in bands, by its table
   when the table is on; the sprites from row 55; window 1 from the wipe table. */
static void course_lines_send(void)
{
    int32_t v[224], l1[224], r1[224];
    int r, at = 0, line = 0;
    int32_t scroll = g.course_scroll_top;

    for (r = 0; r < 224; r++)
        v[r] = r >= 216;
    nshvc_lines(0, 224, NSHVC_LINE_BLANK, v);
    nshvc_line(0, NSHVC_LINE_SPRITES_MAIN, 0);
    nshvc_line(55, NSHVC_LINE_SPRITES_MAIN, 1);
    for (r = 0; r < 55; r++)
        v[r] = scroll;
    scroll = (g.course_scroll_low & 0xFF) | (g.course_scroll_top & 0xFF00);
    for (; r < 224; r++)
        v[r] = scroll;
    if (g.hdma_enable & 0x04) {
        const uint8_t *t = g.course_scroll_table;

        while (t[at] && line < 224) {
            int32_t value = t[at + 1] | t[at + 2] << 8;
            int n = t[at];

            for (r = line; r < line + n && r < 224; r++)
                v[r] = value;
            line += n;
            at += 3;
        }
        for (r = line; line && r < 224; r++)
            v[r] = v[line - 1];
    }
    nshvc_lines(0, 224, (nshvc_line_field)(NSHVC_LINE_SCROLL_Y + 1), v);
    if (g.hdma_enable & 0x02) {
        const uint8_t *t = windows.wipe_table;

        at = 0;
        line = 0;
        while (at + 2 < (int)sizeof windows.wipe_table && t[at] && line < 224) {
            int n = t[at];

            for (r = line; r < line + n && r < 224; r++) {
                l1[r] = t[at + 1];
                r1[r] = t[at + 2];
            }
            line += n;
            at += 3;
        }
        for (r = line; r < 224; r++) {
            l1[r] = line ? l1[line - 1] : 1;
            r1[r] = line ? r1[line - 1] : 0;
        }
        nshvc_lines(0, 224, NSHVC_LINE_WIN1_LEFT, l1);
        nshvc_lines(0, 224, NSHVC_LINE_WIN1_RIGHT, r1);
    }
}

/* One player, the per-line tables the original turned on in $0D45: BG1 off
   in the middle rows (channel 1, the table RaceHudSetup builds: $20 lines
   $13, $80 lines $12, then $13), the gradient (channels 2 and 3), the window
   bands (4), the fixed colour's bands (5). Two players have them all on. */
static void race_lines_send(void)
{
    int one = !pic.race.split;

    if (pic.race.course_screen) {
        course_lines_send();
        return;
    }
    blank_lines_send();
    if (!one || (g.hdma_enable & 0x0C) == 0x0C)
        gradient_lines_send();
    if (!one || (g.hdma_enable & 0x10))
        window_lines_send();
    if (g.hdma_enable & 0x20)
        fixed_lines_send(pic.blend.fixed_bgr15);
    if (one && (g.hdma_enable & 0x02)) {
        nshvc_line(0, NSHVC_LINE_MAIN + 0, 1);
        nshvc_line(32, NSHVC_LINE_MAIN + 0, 0);
        nshvc_line(160, NSHVC_LINE_MAIN + 0, 1);
    }
    if (one && g.results_page_shown) {
        /* BG2's map is the 32 x 32 page at $5C00: the backdrop map's second
           page, wrapping at 256 lines and columns */
        int32_t y[224];
        int r, sy = pic.race.backdrop_y[0];

        for (r = 0; r < 224; r++)
            y[r] = 256 + ((sy + r + 1) & 255) - (r + 1);
        nshvc_lines(0, 224, (nshvc_line_field)(NSHVC_LINE_SCROLL_Y + 1), y);
        nshvc_line(0, (nshvc_line_field)(NSHVC_LINE_SCROLL_X + 1), pic.race.backdrop_x[0] & 255);
    }
    if (!one)
        split_half_send();
}

/* ---- the one-player race in the whole window ---- */

/* The sky (colour 1) on a line of the original's numbering: the gradient's
   list by line through the view, its top line's colour above the view and
   its last line's below, so each line has the colour the original showed at
   the same height above the horizon. */
static int32_t full_sky_at(int line)
{
    if (line < 32)
        line = 32;
    if (line > 158)
        line = 158;
    return (int32_t)gradient_colour((uint32_t)pic.race.gradient_start[0] + (uint32_t)(line - 32));
}

/* Every line from the origin down; the rows above it take the frame's
   colour 1, which video_draw sets to the top line's. */
static void full_sky_lines(int lines)
{
    static int32_t v[1024];
    int r;

    for (r = 0; r < lines; r++)
        v[r] = full_sky_at(r);
    nshvc_lines(0, (uint32_t)lines, (nshvc_line_field)(NSHVC_LINE_PALETTE + 1), v);
}

/* Whether each window of the band table covers any of the screen: the
   original's windows were the view's opening (or nothing), and with no
   bezel the view is the whole screen, so a window of the view covers
   everything and one of nothing covers nothing. */
static void full_windows(nshvc_windows *w, int width)
{
    int band, line = 0, cover[2] = { 0, 0 }, i;

    for (band = 0; band < WINDOW_BANDS_MAX && windows.band[band].lines && line < 224; band++) {
        uint8_t c = windows.band[band].lines;
        int repeat = c >= 0x81, n = repeat ? c - 0x80 : c, k;
        const WindowEdges *e = window_edges_at(windows.band[band].edges);

        for (k = 0; k < n && line < 224; k++, line++) {
            cover[0] |= e->left1 <= e->right1;
            cover[1] |= e->left2 <= e->right2;
            if (repeat)
                e = window_edges_at((uint16_t)(windows.band[band].edges + (uint16_t)(4 * (k + 1))));
        }
    }
    for (i = 0; i < 2; i++) {
        w->w[i].left = cover[i] ? 0 : 1;
        w->w[i].right = cover[i] ? width - 1 : 0;
    }
}

static void full_window_draw(const ScreenShape *sh)
{
    static uint16_t palette[256];
    nshvc_windows w = pic.windows;
    Background hills = pic.bg[1];
    static nshvc_sprite placed[128];
    int sky = (g.hdma_enable & 0x0C) == 0x0C;

    layer_send(0, &pic.bg[0]);
    hills.map = race_video.backdrop_wide;
    hills.scroll_x = pic.race.wide_x;
    hills.scroll_y = pic.race.wide_y;
    layer_send(1, &hills);
    layer_send(2, &pic.bg[2]);
    if (g.hdma_enable & 0x10)
        full_windows(&w, sh->width);
    nshvc_windows_set(&w);
    nshvc_blend_set(&pic.blend);
    memcpy(palette, pic.palette, sizeof palette);
    if (sky)
        palette[1] = (uint16_t)full_sky_at(0);
    nshvc_palette_write(0, 256, palette);
    nshvc_sprites_show((pic.main_screen >> 4) & 1, (pic.sub_screen >> 4) & 1);
    sprites_place_full(placed, pic.sprites, pic.sprite_count);
    nshvc_sprites_set(placed, pic.sprite_count, 0);
    if (sky)
        full_sky_lines(sh->height - sh->origin_y);
    if (g.hdma_enable & 0x20)
        fixed_lines_send(pic.blend.fixed_bgr15);
}

void video_draw(void)
{
    nshvc_screen s;
    const ScreenShape *sh;
    int i, full;

    screen_update();
    sh = screen_shape();
    full = race_full_window();
    memset(&s, 0, sizeof s);
    s.width = full ? sh->width : 256;
    s.height = full ? sh->height : 224;
    s.origin_x = full ? sh->origin_x : 0;
    s.origin_y = full ? sh->origin_y : 0;
    s.brightness = pic.brightness;
    s.blank = pic.blank;
    nshvc_screen_set(&s);
    if (pic.blank)
        return;
    nshvc_mode_set(pic.mode, pic.bg3_priority, 0);
    if (full) {
        full_window_draw(sh);
        return;
    }
    if (pic.race.on && pic.race.split && !pic.race.course_screen)
        race_two_player_list();
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
