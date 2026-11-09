/*
 * picture.c: the screen as the game has described it, handed to the engine
 * once per picture.
 */
#include <string.h>

#include "video/picture.h"

uint8_t *menu_tables_at(void);

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

/* The screen's per-line tables, in channel order as the hardware ran them:
   each table is a list of line counts and values that end on a count of 0
   and writes only at the start of each run, so what it wrote lasts. */
static void tm_table_apply(uint8_t tm[225], uint16_t offset)
{
    const uint8_t *t = menu_tables_at() + offset;
    int v = 0;

    while (*t && v < 225) {
        int n = *t++ & 0x7F;

        for (int i = 0; i < n && v < 225; i++, v++)
            if (i == 0)
                tm[v] = *t;
            else if (t[-1] & 0x80)
                tm[v] = t[i];
            else
                tm[v] = tm[v - 1];
        t += (t[-1] & 0x80) ? n : 1;
    }
    for (; v < 225; v++)
        tm[v] = tm[v - 1];
}

static void line_tables_send(void)
{
    uint8_t tm[225];
    int v, l;

    memset(tm, pic.main_screen, sizeof tm);
    if (pic.line_tables & LINES_TM_C)
        tm_table_apply(tm, 0x131F - 0x0800);
    if (pic.line_tables & LINES_TM_B)
        tm_table_apply(tm, 0x131A - 0x0800);
    if (pic.line_tables & LINES_TM_A)
        tm_table_apply(tm, 0x1315 - 0x0800);
    for (l = 0; l < 4; l++)
        nshvc_line(0, (nshvc_line_field)(NSHVC_LINE_MAIN + l), (tm[1] >> l) & 1);
    nshvc_line(0, NSHVC_LINE_SPRITES_MAIN, (tm[1] >> 4) & 1);
    for (v = 2; v < 225; v++) {
        if (tm[v] == tm[v - 1])
            continue;
        for (l = 0; l < 4; l++)
            nshvc_line((uint32_t)(v - 1), (nshvc_line_field)(NSHVC_LINE_MAIN + l), (tm[v] >> l) & 1);
        nshvc_line((uint32_t)(v - 1), NSHVC_LINE_SPRITES_MAIN, (tm[v] >> 4) & 1);
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
    l.tile_size = 8;
    l.scroll_x = b->scroll_x;
    l.scroll_y = b->scroll_y;
    l.mosaic = 1;
    l.wrap = 1;
    nshvc_layer_set((uint8_t)n, &l);
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
        line_tables_send();
}
