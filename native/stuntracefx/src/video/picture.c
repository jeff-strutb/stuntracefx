/*
 * picture.c: the screen as the game has described it, handed to the engine
 * once per picture.
 */
#include <string.h>

#include "fx/fx_draw.h"
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
static void fixed_line_values(uint16_t start, int32_t *v)
{
    int i = 0, line = 0;
    int r = start & 31, gc = (start >> 5) & 31, b = (start >> 10) & 31;

    memset(v, 0, 224 * sizeof *v);
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
}

static void fixed_lines_send(uint16_t start)
{
    int32_t v[224];

    fixed_line_values(start, v);
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
static void course_lines_send(int full, int origin_x)
{
    int32_t v[224], l1[224], r1[224];
    int r, at = 0, line = 0;
    int32_t scroll = g.course_scroll_top;

    if (!full) {                        /* in the whole window no line is blanked for the uploads */
        for (r = 0; r < 224; r++)
            v[r] = r >= 216;
        nshvc_lines(0, 224, NSHVC_LINE_BLANK, v);
    }
    nshvc_line(0, NSHVC_LINE_SPRITES_MAIN, 0);
    nshvc_line(55, NSHVC_LINE_SPRITES_MAIN, 1);
    for (r = 0; r < 55; r++)
        v[r] = scroll;
    scroll = (g.course_scroll_low & 0xFF) | (g.course_scroll_top & 0xFF00);
    for (; r < 224; r++)
        v[r] = scroll;
    if (g.line_effects.course_scroll) {
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
    if (g.line_effects.course_wipe) {
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
        for (r = 0; r < 224; r++) {     /* the window's edges are the screen's columns */
            l1[r] += origin_x;
            r1[r] += origin_x;
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
        int full = race_course_full_window();

        course_lines_send(full, full ? screen_shape()->origin_x : 0);
        return;
    }
    blank_lines_send();
    if (!one || g.line_effects.sky)
        gradient_lines_send();
    if (!one || g.line_effects.window_bands)
        window_lines_send();
    if (g.line_effects.fixed_bands)
        fixed_lines_send(pic.blend.fixed_bgr15);
    if (one && g.line_effects.results_bg1_off) {
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

/* The engine's origin stays at the screen's top left: the original's 256 x
   224 is placed here, in the layers' scrolls and the lines' numbers, so
   that every row of the screen can take its own sky. */

/* The sky (colour 1) on a screen row: the colour at the same height above
   the horizon, read from the day and night lists as a band reads them,
   from the band's start at the original's line 32; black while the
   tunnel's black list is up. */
static int32_t full_sky_at(int row, int origin_y)
{
    if (pic.race.gradient_start[0] >= GRADIENT_AT_BLACK)
        return 0;
    return (int32_t)sky_colour_at(pic.race.sky_offset / 2 + (row - origin_y) - 32);
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
    static int32_t rows[1024];
    nshvc_windows w = pic.windows;
    Background hills = pic.bg[1];
    static nshvc_sprite placed[128];
    int sky = g.line_effects.sky, r;

    race_hills_full();
    layer_send(0, &pic.bg[0]);
    hills.map = race_video.backdrop_wide;
    hills.scroll_x = pic.race.wide_x - sh->origin_x;
    hills.scroll_y = pic.race.wide_y - sh->origin_y;
    layer_send(1, &hills);
    layer_send(2, &pic.bg[2]);
    if (g.line_effects.window_bands)
        full_windows(&w, sh->width);
    nshvc_windows_set(&w);
    nshvc_blend_set(&pic.blend);
    memcpy(palette, pic.palette, sizeof palette);
    if (sky)
        palette[1] = (uint16_t)full_sky_at(0, sh->origin_y);
    nshvc_palette_write(0, 256, palette);
    nshvc_sprites_show((pic.main_screen >> 4) & 1, (pic.sub_screen >> 4) & 1);
    sprites_place_full(placed, pic.sprites, pic.sprite_count);
    nshvc_sprites_set(placed, pic.sprite_count, 0);
    if (sky) {
        for (r = 0; r < sh->height; r++)
            rows[r] = full_sky_at(r, sh->origin_y);
        nshvc_lines(0, (uint32_t)sh->height, (nshvc_line_field)(NSHVC_LINE_PALETTE + 1), rows);
    }
    if (g.line_effects.fixed_bands) {
        int32_t lines[224];

        fixed_line_values(pic.blend.fixed_bgr15, lines);
        for (r = 0; r < sh->height; r++) {
            int line = r - sh->origin_y;

            rows[r] = lines[line < 0 ? 0 : line > 223 ? 223 : line];
        }
        nshvc_lines(0, (uint32_t)sh->height, NSHVC_LINE_FIXED, rows);
    }
}

/* Two players in the whole window: the frame's layer off (the boxes are in
   the list), each player's sky, hills and 3D in the list on BG2, the
   sprites placed by player. Above and below, the second player's colours
   $E1-$E3, colour 0 and fixed colour from its rectangle's first row, as
   the original changed them at line 112. */
static void full_two_draw(const ScreenShape *sh)
{
    static nshvc_sprite placed[128];
    nshvc_windows w = pic.windows;
    Background none;
    int r;

    memset(&none, 0, sizeof none);
    race_two_player_list();
    layer_send(0, &none);
    layer_send(1, &pic.bg[1]);
    layer_send(2, &none);
    full_windows(&w, sh->width);
    nshvc_windows_set(&w);
    nshvc_blend_set(&pic.blend);
    nshvc_palette_write(0, 256, pic.palette);
    nshvc_sprites_show(1, 0);
    sprites_place_two(placed, pic.sprites, pic.sprite_count);
    nshvc_sprites_set(placed, pic.sprite_count, 0);
    if (!screen_two_side()) {
        int x, y, wd, ht;

        screen_two_rect(1, &x, &y, &wd, &ht);
        for (r = 0; r < 3; r++)
            nshvc_line((uint32_t)y, (nshvc_line_field)(NSHVC_LINE_PALETTE + 0xE1 + r), pic.race.colours_kept[1][r]);
        nshvc_line((uint32_t)y, (nshvc_line_field)(NSHVC_LINE_PALETTE + 0), pic.race.colour_0[1]);
        nshvc_line((uint32_t)y, NSHVC_LINE_FIXED, pic.race.fixed[1]);
    }
}

void video_draw(void)
{
    nshvc_screen s;
    const ScreenShape *sh;
    int i, full, menu_full, course_full;

    screen_update();
    sh = screen_shape();
    full = race_full_window() || race_two_full_window();
    menu_full = pic.fills_window && !pic.race.on;
    course_full = race_course_full_window();
    if (pic.race.on && pic.race.course_screen)
        pic.bg[0].draw_list = fx_draw_picture(0);   /* the static as this frame's loop drew it */
    memset(&s, 0, sizeof s);
    s.width = full || menu_full || course_full ? sh->width : 256;
    s.height = full || menu_full || course_full ? sh->height : 224;
    s.origin_x = menu_full || course_full ? sh->origin_x : 0;   /* the race places its own (full_window_draw) */
    s.origin_y = menu_full || course_full ? sh->origin_y : 0;
    s.brightness = pic.brightness;
    s.blank = pic.blank;
    nshvc_screen_set(&s);
    if (pic.blank)
        return;
    nshvc_mode_set(pic.mode, pic.bg3_priority, 0);
    {
        nshvc_limits l;

        memset(&l, 0, sizeof l);
        if (menu_full)
            memcpy(l.layer_clip, pic.layer_clip, sizeof l.layer_clip);
        if (course_full)
            l.layer_clip[1] = l.layer_clip[4] = 1;      /* the cards and sprites kept to the original's picture */
        nshvc_limits_set(&l);
    }
    if (full) {
        if (pic.race.split)
            full_two_draw(sh);
        else
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
    if (menu_full || course_full) {
        static nshvc_sprite moved[128];
        uint32_t k, n = pic.sprite_count > 128 ? 128 : pic.sprite_count;

        for (k = 0; k < n; k++) {
            moved[k] = pic.sprites[k];
            moved[k].x += sh->origin_x;
            moved[k].y += sh->origin_y;
        }
        nshvc_sprites_set(moved, n, 0);
    } else
        nshvc_sprites_set(pic.sprites, pic.sprite_count, 0);
    if (pic.line_tables)
        menu_lines_send();
    if (pic.race.on)
        race_lines_send();
}
