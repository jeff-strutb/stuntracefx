/*
 * screens.c: the menu screens from the logo to the course menu's end. Each
 * state's code runs in one update; where the original waited for the frame,
 * the update returns.
 */
#include <stdio.h>
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "data/unpack.h"
#include "fx/fx_draw.h"
#include "menu/actors.h"
#include "menu/menu.h"
#include "menu/menu_tables.h"
#include "menu/menu_window.h"
#include "race/race.h"
#include "save/save.h"
#include "system/game.h"
#include "system/state.h"
#include "video/picture.h"
#include "video/video.h"

MenuState menu;

/* tile sets only the course menu and the championship use */
static nshvc_tiles tiles_8bpp;          /* word 0 in mode 3 */
static nshvc_tiles tiles_menu;          /* word $3000: BG2's in the course menu */
static nshvc_tiles tiles_load;          /* word $7000 as 4bpp: the loading screen's */

static uint8_t picture[0x8000];
/* the animated pictures: the $7F:3FD7 and $7F:A7D7 copies of the original */
static uint8_t frames_sprites[0x6800];
static uint8_t frames_tiles[0x2000];
/* the championship's selection frames, from $04:EBCB */
static uint8_t champion_frames[0x800];

const uint8_t *menu_frame_data(uint16_t address)
{
    if (address >= 0xA7D7)
        return &frames_tiles[address - 0xA7D7];
    return &frames_sprites[address - 0x3FD7];
}

static uint32_t picture_unpack(uint8_t bank, uint16_t end)
{
    return unpack((uint32_t)bank << 16 | end, picture, sizeof picture);
}

static void tiles_from_picture(nshvc_tiles set, uint32_t first, uint32_t bytes, uint32_t per_tile)
{
    nshvc_tiles_write(set, first, picture, bytes / per_tile, NSHVC_TILES_PLANAR);
}

/* A menu page written, and kept for the granite's map at the window's size. */
static void page_write(int page, const void *words)
{
    menu_page_write(page, words);
    menu_window_page(page, (const uint16_t *)words);
}

/* A picture's map pages, 32 x 32 words each, from page `first` on. */
static void pages_from_picture(int first, uint32_t bytes)
{
    uint32_t i;

    for (i = 0; i < bytes / 0x800; i++)
        page_write(first + (int)i, picture + i * 0x800);
}

static void palette_load(uint8_t bank, uint16_t addr)
{
    int i;

    for (i = 0; i < 256; i++)
        pic.palette[i] = cart16(bank, (uint16_t)(addr + i * 2));
}

/*
 * @routine 0B:F4B2 MenuScrollReset
 * WHAT IT DOES: The layers' scrolls back to 0.
 */
static void menu_scroll_reset(void)
{
    int i;

    for (i = 0; i < 3; i++)
        pic.bg[i].scroll_x = pic.bg[i].scroll_y = 0;
    actors.scroll_x = actors.scroll_y = 0;
}

static void menu_common_mode(void)
{
    race_steps_end();                   /* no race steps drawn between behind this screen */
    race_screen_end();                  /* none of the race's layers left behind this screen */
    pic.mode = 1;
    pic.bg3_priority = 1;
    pic.bg[0].tiles = menu_video.tiles_a;
    pic.bg[0].map = menu_video.page_0;
    pic.bg[1].tiles = menu_video.tiles_b;
    pic.bg[1].map = menu_video.pages_1_2;
    pic.bg[2].tiles = menu_video.tiles_text;
    pic.bg[2].map = menu_video.text;
    pic.bg[2].draw_list.id = 0;
    menu_lines_set(MENU_LINES_TITLE, 0);
    memset(&pic.blend, 0, sizeof pic.blend);
    pic.blend.use_fixed = 1;
    memset(&pic.windows, 0, sizeof pic.windows);
    /* past the picture: the checks (BG3) */
    menu_window_screen(KEEP_BG1 | KEEP_BG2 | KEEP_SPRITES, 0);
}

/* The objects made and placed before the screen first shows. */
static void menu_objects_start(uint16_t list)
{
    actors_reset();
    actors_spawn_list(list);
    actors_run();
    actors_build_sprites();
}

/*
 * @routine 0B:F33C MenuScreenOpen0
 * WHAT IT DOES: Opens the first screen: the logo, drawn with sprites alone.
 */
static void logo_open(void)
{
    pic.blank = 1;
    pic.main_screen = pic.sub_screen = 0;
    frame_state.brightness = 0;
    menu_common_mode();
    sound_queue_reset();
    menu_scroll_reset();
    palette_load(0x1C, 0x989C);
    pic.palette[0] = 0;
    nshvc_tiles_write(menu_video.sprites, 0, cart_at(0x1C, 0xDC8C), 0x720 / 32, NSHVC_TILES_PLANAR);
    actors.fast_animation = 0;
    actors.exit_request = 0;
    menu_objects_start(0);
}

/*
 * @routine 0B:EE24 MenuScreenOpen6
 * WHAT IT DOES: Opens the title screen, with its music.
 */
static void title_open(void)
{
    uint32_t n;
    int i;

    pic.blank = 1;
    frame_state.brightness = 0;
    menu_common_mode();
    pic.blend.layers = 1;
    pic.sub_screen = SHOW_BG2;
    pic.blend.use_fixed = 0;
    sound_upload_begin();
    sound_upload_packed(0xF3B5, 0x1E);
    sound_upload_cart(0x1B, 0x85F8);
    sound_upload_cart(0x19, 0x9A98);
    sound_upload_end();
    sound_queue_reset();
    sound_set_port0(1);
    menu_scroll_reset();
    palette_load(0x1C, 0x989C);
    memcpy(pic.palette_saved, pic.palette, sizeof pic.palette);
    menu_palette_scale(0x40, 0x40, 1);
    menu_palette_scale(0x0C, 4, 32);
    tiles_from_picture(menu_video.sprites, 0, picture_unpack(0x1C, 0xDC38), 32);
    pages_from_picture(2, picture_unpack(0x1C, 0xC9DC));
    nshvc_tiles_write(menu_video.tiles_text, 0, cart_at(0x1C, 0xBE58), 0x40 / 16, NSHVC_TILES_PLANAR);
    tiles_from_picture(menu_video.tiles_a, 0, picture_unpack(0x1C, 0xBABC), 32);
    pages_from_picture(0, picture_unpack(0x1C, 0xBE58));
    tiles_from_picture(menu_video.tiles_b, 0, picture_unpack(0x1C, 0xC664), 32);
    pages_from_picture(1, picture_unpack(0x1C, 0xC818));
    n = picture_unpack(0x1C, 0xBECC);
    for (i = 0; n >= 0x800 && i < 1; i++)
        nshvc_map_write_snes(menu_video.text, 0, 0, 32, 32, (const uint16_t *)picture);
    frame_state.menu_pressed[0] = 0;
    actors.fast_animation = 0;
    actors.exit_request = 0;
    menu.blink_on = 0;
    menu.window_mode = 0;
    menu_objects_start(6);
}

/* --- leaving the title: the loader and the course menu's pictures ------ */

/*
 * @routine 0B:D14A MenuCrossFadeOut (its end)
 * WHAT IT DOES: The screen set up for the loader: mode 3, only BG2 on, BG2
 * on the loading tiles and the text map.
 */
void menu_load_screen_open(void)
{
    nshvc_tiles_write(tiles_load, 0, cart_at(0x0B, 0xD56C), 4, NSHVC_TILES_PLANAR);
    pic.mode = 3;
    pic.bg[1].tiles = tiles_load;
    pic.bg[1].map = menu_video.text;
    pic.main_screen = SHOW_BG2;
    menu_lines_set(MENU_LINES_TITLE, 0);
    menu_window_screen(KEEP_BG1 | KEEP_BG3 | KEEP_SPRITES, 0);    /* past the picture: the loading pattern */
}

/*
 * @routine 0B:F6EA MenuPictureInsetGrid
 * @routine 0B:F6B6 MenuPictureDC8CGrid0
 * @routine 0B:F6D0 MenuPictureDC8CGrid58
 * WHAT IT DOES: The course menu's BG1 map: the picture at $DC8C with its tile
 * numbers raised by $100 and an 11 x 13 block over it numbered from
 * `first_tile`, columns $0B apart.
 */
void menu_grid_load(uint16_t first_tile, int page)
{
    uint16_t w[0x400];
    int x, y;

    memset(w, 0, sizeof w);
    picture_unpack(0x1C, 0xDC8C);
    memcpy(w, picture, 0x700);
    for (y = 0; y < 28; y++)
        for (x = 0; x < 32; x++)
            w[y * 32 + x] = (uint16_t)(w[y * 32 + x] + 0x0100);
    if (page == 0) {
        page_write(1, w);
        page_write(0, w);
    }
    for (y = 12; y < 23; y++)
        for (x = 4; x < 17; x++)
            w[y * 32 + x] = (uint16_t)((first_tile + (y - 12) + (x - 4) * 0x0B) | 0x2000);
    page_write(page, w);
}

/*
 * @routine 0B:D1F7 MenuLoadPictureCC78 .. @routine 0B:D476 MenuLoadBlank1800
 * WHAT IT DOES: The course menu's graphics, one step a frame: BG2's tiles,
 * its map, BG1's map, the sprites, the animated pictures and blank tiles.
 */
void menu_load_step(Actor *a)
{
    uint32_t n;

    switch (a->count++) {
    case 0:                             /* D1F7 */
        tiles_from_picture(tiles_menu, 0, picture_unpack(0x17, 0xCC78), 32);
        break;
    case 1:                             /* D234 */
        n = picture_unpack(0x17, 0xCE78);
        pages_from_picture(2, n);
        break;
    case 2:                             /* D26F */
        menu_grid_load(0x0000, 0);
        break;
    case 3:                             /* D2C9, and the next three steps upload it */
        n = picture_unpack(0x17, 0xDD8C);
        tiles_from_picture(menu_video.sprites, 0, n < 0x4000 ? n : 0x4000, 32);
        break;
    case 4: case 5: case 6:             /* D304, D333, D362 */
        break;
    case 7:                             /* D391 */
        n = picture_unpack(0x1D, 0x9068);
        memcpy(frames_tiles, picture, sizeof frames_tiles);
        n = picture_unpack(0x17, 0xF8E8);
        memcpy(frames_sprites, picture, sizeof frames_sprites);
        (void)n;
        break;
    case 8: case 9: case 10: case 11:   /* D3D8 .. D476: blank tiles under the 3D picture */
        break;
    default:
        a->handler = HANDLER_SET_EXIT;
        break;
    }
}

/*
 * @routine 7F:0CDF AnimTilesQueueNext
 * WHAT IT DOES: The next $400-byte frame of the animated tiles, stepping
 * through eight frames: 16 tiles of 8 bits at tile $100 of mode 3's BG1.
 */
void menu_frames_vram_tick(void)
{
    menu.animated_offset = (uint16_t)((menu.animated_offset + 0x0400) & 0x1FFF);
    nshvc_tiles_write(tiles_8bpp, 0x100, &frames_tiles[menu.animated_offset], 16, NSHVC_TILES_PLANAR);
}

/*
 * @routine 7F:0DAC WindowOpenCloseStep
 * WHAT IT DOES: Steps the window over the screen: opening shrinks the size
 * by 4 a frame, closing grows it to $2C, then clears colour 0; each step
 * rewrites the window table's line counts. Mode 3, the championship's,
 * sets the panel's line counts.
 */
static void window_table(void)
{
    /* the mask table's runs 1-3: count, value; count, value; count */
    uint8_t *t = menu_table_byte(MT_PREVIEW_MASK);
    uint8_t c = (uint8_t)(0x2C - menu.window_size), d = (uint8_t)(menu.window_size << 1);

    t[2] = c;
    t[6] = c;
    if (d) {
        t[4] = d;
        t[5] = 0x00;
    } else {
        t[4] = 0x02;
        t[5] = 0x03;
        t[6] = (uint8_t)(t[6] - 2);
    }
}

static void window_open_close_step(void)
{
    if (menu.window_mode == 3) {
        uint8_t n = (uint8_t)(menu.panel_rise + 1);

        /* the panels' rows: the third run's line count in each table */
        *menu_table_byte(MT_CHAMPION_SCROLL_X + 6) = n;
        *menu_table_byte(MT_CHAMPION_SCROLL_Y + 6) = n;
        *menu_table_byte(MT_CHAMPION_MAIN + 4) = n;
        *menu_table_byte(MT_CHAMPION_WINDOW + 4) = n;
        menu.panel_top_scroll = (uint16_t)(0x0016 - menu.panel_rise + menu.panel_top_lowered);
        menu.panel_low_scroll = (uint16_t)(0x0045 - menu.panel_rise + menu.panel_low_start);
        return;
    }
    if (menu.window_mode == 1) {
        window_table();
        menu.window_size = (uint16_t)(menu.window_size - 4);
        if (menu.window_size & 0x8000) {
            menu.window_size = 0;
            menu.window_mode = 0;
        }
    } else if (menu.window_mode == 2) {
        window_table();
        menu.window_size = (uint16_t)(menu.window_size + 4);
        if (menu.window_size == 0x2C) {
            menu.window_mode = 0;
            pic.palette[0] = 0;
        }
    }
}

/* The race's picture is BG1's in the original: the engine shows a draw list
   as a layer of its own, so BG3 stands in for it and follows BG1's switches,
   windows and colour maths. */
static void menu_3d_layer_follow(void)
{
    /* the picture is put together from the race's last drawing when asked for */
    if (pic.bg[2].draw_list.id)
        pic.bg[2].draw_list = fx_draw_picture(0);
    pic.main_screen = (uint8_t)((pic.main_screen & ~SHOW_BG3) | ((pic.main_screen & SHOW_BG1) << 2));
    pic.sub_screen = (uint8_t)((pic.sub_screen & ~SHOW_BG3) | ((pic.sub_screen & SHOW_BG1) << 2));
    pic.windows.layer_use[2] = pic.windows.layer_use[0];
    pic.windows.mask_main[2] = pic.windows.mask_main[0];
    pic.windows.mask_sub[2] = pic.windows.mask_sub[0];
    pic.blend.layers = (uint8_t)((pic.blend.layers & ~4) | ((pic.blend.layers & 1) << 2));
}

/* What the menu's vertical blank code did, ordered as it did it. `stage20`:
   the course menu and championship stage, which also steps the animated
   tiles, queues the presses and takes the positions the objects wrote. */
static int stage20_on;                  /* the last frame ended under the menus' stage $20 */

/* The vertical blank before this frame's objects, under stage $20, queued
   the presses read at it. */
static void menu_frame_begin(void)
{
    int i;

    menu_window_frame_begin();
    if (!stage20_on)
        return;
    for (i = 0; i < 2; i++)
        if (menu.press_count[i] < 8)
            menu.press_list[i][menu.press_count[i]++] = frame_state.pressed[i];
}

static void menu_frame_end(int stage20)
{
    stage20_on = stage20;
    if (stage20) {
        menu_3d_layer_follow();
        menu_frames_vram_tick();
        menu.slide_top = menu.slide_top_next;
        menu.slide_low = menu.slide_low_next;
    }
    window_open_close_step();
    if (menu.blink_on) {
        const uint8_t *t = menu_table_byte(MT_BLINK_COLOURS);

        if (++menu.blink_count == 5) {
            menu.blink_count = 0;
            menu.blink_phase = (uint16_t)((menu.blink_phase + 1) & 1);
        }
        pic.palette[0xD9] = (uint16_t)(t[menu.blink_phase * 2] | t[menu.blink_phase * 2 + 1] << 8);
    }
    actors_rows_flush();
    menu_window_frame_end();
    frame_end();
}

/* --- the course menu's screens ----------------------------------------- */

/* Clears the menu's lists and state as a screen opens. */
static void menu_state_clear(void)
{
    menu.press_count[0] = menu.press_count[1] = 0;
    menu.press_current = 0;
    actors.exit_request = 0;
    actors.fast_animation = 0;
    actors.scroll_x = actors.scroll_y = 0;
    menu.window_mode = 0;
    menu.window_size = 0;
}

/* BG1 and BG2 for the course menu's full mode. */
static void preview_full_mode(void)
{
    race_screen_end();                  /* none of the race's layers left behind this screen */
    pic.mode = 3;
    pic.bg3_priority = 0;
    pic.bg[0].tiles = tiles_8bpp;
    pic.bg[0].map = menu_video.pages_0_1;
    pic.bg[1].tiles = tiles_menu;
    pic.bg[1].map = menu_video.pages_2_3;
    pic.bg[2].draw_list = fx_draw_picture(0);
    memset(&pic.blend, 0, sizeof pic.blend);
    pic.blend.use_fixed = 1;
    pic.bg[1].scroll_x = (int16_t)0xFF00;
    pic.windows.w[0].left = 0;
    pic.windows.w[0].right = 0xFF;
    memset(pic.windows.layer_use, 0, sizeof pic.windows.layer_use);
    pic.windows.layer_use[1] = NSHVC_WIN1 | NSHVC_WIN1_INVERT;
    pic.windows.layer_use[4] = NSHVC_WIN1 | NSHVC_WIN1_INVERT;
    memset(pic.windows.mask_main, 0, sizeof pic.windows.mask_main);
    pic.windows.mask_main[1] = 1;
    pic.windows.mask_main[4] = 1;
    pic.main_screen = SHOW_BG1 | SHOW_BG2 | SHOW_SPRITES;
    pic.sub_screen = 0;
    menu.slide_top = menu.slide_top_next = 0;
    menu.slide_low = menu.slide_low_next = 0x00A8;
    *menu_table_byte(MT_PREVIEW_WINDOW + 1) = 0xFF;   /* the first run's left edge: none */
    menu_lines_set(MENU_LINES_PREVIEW, 0x90);
    menu_window_screen(KEEP_BG2 | KEEP_BG3 | KEEP_SPRITES, 1);    /* past the picture: the granite */
}

/* The palette and its full colours for the course menu. */
static void preview_palette(void)
{
    palette_load(0x1C, 0xE3AC);
    pic.palette[0] = 0;
    memcpy(pic.palette_saved, pic.palette, sizeof pic.palette);
    menu_palette_scale(0x90, 8, 0x20);
    menu_palette_scale(0xD0, 8, 0x20);
}

/*
 * @routine 0B:F784 MenuScreenOpenPreview
 * WHAT IT DOES: Opens the menu screen with the course preview behind it, the
 * race started, the screen blank until its first frames.
 */
static void preview_open(void)
{
    pic.blank = 1;
    frame_state.brightness = 0;
    menu_state_clear();
    actors_reset();
    actors_spawn_list(0x10);
    menu_window_car_origin();
    RaceStart();
    actors_run();
    actors_build_sprites();
    menu_scroll_reset();
    preview_full_mode();
    pic.bg[0].scroll_x = 0x0100;        /* the second page */
    preview_palette();
    tiles_from_picture(tiles_menu, 0, picture_unpack(0x17, 0xCC78), 32);
    pages_from_picture(2, picture_unpack(0x17, 0xCE78));
    menu_grid_load(0x0000, 0);
    {
        uint32_t n = picture_unpack(0x17, 0xDD8C);

        tiles_from_picture(menu_video.sprites, 0, n < 0x4000 ? n : 0x4000, 32);
        picture_unpack(0x1D, 0x9068);
        memcpy(frames_tiles, picture, sizeof frames_tiles);
        picture_unpack(0x17, 0xF8E8);
        memcpy(frames_sprites, picture, sizeof frames_sprites);
    }
    sound_hold_a = 0;
    sound_queue_reset();
}

/*
 * @routine 0B:F09F MenuScreenOpenPreviewFade (its opening)
 * WHAT IT DOES: The first part of opening the course preview from the title:
 * no blanking; the objects made, the race started, the colours black and the
 * cross fade's mode set: BG1 of the new screen adds to the title's text.
 */
static void preview_fade_begin(void)
{
    int i;

    menu_state_clear();
    actors_reset();
    actors_spawn_list(0x10);
    memset(pic.palette, 0, 12 * 2);
    for (i = 0; i < 12; i++)
        pic.palette_saved[i] = cart16(0x1C, (uint16_t)(0xE3AC + i * 2));
    menu_window_car_origin();
    RaceStart();
    menu.slide_top = menu.slide_top_next = 0;
    menu.slide_low = menu.slide_low_next = 0x00A8;
    *menu_table_byte(MT_PREVIEW_WINDOW + 1) = 0xFF;   /* the first run's left edge: none */
    actors_run();
    actors_build_sprites();
    pic.bg[0].tiles = tiles_8bpp;
    pic.bg[0].map = menu_video.pages_0_1;
    pic.bg[1].tiles = tiles_load;
    pic.bg[2].draw_list = fx_draw_picture(0);
    memset(&pic.blend, 0, sizeof pic.blend);
    pic.blend.layers = 1;
    pic.blend.use_fixed = 0;
    pic.sub_screen = SHOW_BG2;
    /* past the picture: the granite and the loading pattern it fades from */
    menu_window_screen(KEEP_BG3 | KEEP_SPRITES, 1);
    memset(pic.windows.mask_main, 0, sizeof pic.windows.mask_main);
    memset(pic.windows.mask_sub, 0, sizeof pic.windows.mask_sub);
    pic.main_screen = SHOW_BG1 | SHOW_BG2;
    /* the second page (the original queued its two scroll bytes, applied
       newest first): the picture without the window's grid */
    pic.bg[0].scroll_x = 0x0100;
}

/* --- the flow, frame by frame ------------------------------------------ */

enum {
    S_SOUND_START, S_LOGO_OPEN, S_LOGO_SOUND, S_LOGO_SHOW, S_LOGO_RUN, S_BOOT_DONE,
    S_COURSE_START, S_TITLE_OPEN, S_TITLE_SHOW, S_TITLE_FADE, S_TITLE_RUN,
    S_TITLE_FADEOUT, S_DEMO, S_PREVIEW_OPEN, S_PREVIEW_SHOW, S_PREVIEW_READY,
    S_FADE_OPEN1, S_FADE_OPEN2, S_FADE_STEPS, S_FADE_FULL, S_FADE_SET, S_FADE_LAST,
    S_MENU_LOOP, S_MENU_FADEOUT, S_COURSE_DONE, S_RACE
};

static int state = S_SOUND_START;

/* The course menu's race steps every 4 updates, the original's usual pace;
   each step is told the updates since the last (RaceFrameStep). */
enum { MENU_STEP_FRAMES = 4 };
static int fade_level;
static int step_wait;
static int preview_mode_fade;

static void objects_frame(void)
{
    actors_run();
    actors_build_sprites();
}

/*
 * @routine 0B:AE0A MenuScreen0Run
 * WHAT IT DOES: The logo screen, opened again while buttons are held; then
 * the logo's end.
 * @routine 0B:AE47 MenuRegionCheck: the console's region is not modelled
 * here: the port runs as an NTSC console, so it passes.
 */
int MenuScreen0Run(void)
{
    switch (state) {
    case S_SOUND_START:
        sound_upload_begin();
        sound_upload_cart(0x19, 0xC27C);
        sound_upload_end();
        state = S_LOGO_OPEN;
        /* fall through */
    case S_LOGO_OPEN:
        logo_open();
        pic.blank = 0;
        state = S_LOGO_SOUND;
        break;
    case S_LOGO_SOUND:
        sound_port0_push(0x39);
        state = S_LOGO_SHOW;
        break;
    case S_LOGO_SHOW:
        frame_state.brightness = frame_state.brightness_target = 15;
        pic.main_screen = SHOW_SPRITES;
        state = S_LOGO_RUN;
        break;
    case S_LOGO_RUN:
        objects_frame();
        if (actors.exit_request) {
            g.demo_number = 3;
            g.course_menu_seen = 0;
            state = S_BOOT_DONE;
        }
        break;
    default:
        return 1;
    }
    return state == S_BOOT_DONE;
}

/*
 * @routine 0B:AF10 TitleScreenLoop
 * WHAT IT DOES: The title screen's frames: while the sequence object is
 * idle the count goes up, and at $400 the screen fades out and an attract
 * demo runs; the menu's exit leaves.
 */
static void title_run(void)
{
    Actor *a;

    objects_frame();
    a = actor_find(0x0012);
    if (a && a->handler == HANDLER_BUTTON_SEQUENCE && a->count == 0) {
        menu_idle++;
        if (menu_idle == 0x0400) {
            frame_state.brightness_target = 0;
            state = S_TITLE_FADEOUT;
        }
    }
    frame_state.menu_pressed[0] = 0;
    frame_state.menu_pressed[1] = 0;
    if (actors.exit_request && state == S_TITLE_RUN) {
        preview_mode_fade = 1;
        state = S_FADE_OPEN1;
    }
}

/*
 * @routine 03:B3DA CourseMenuRun
 * WHAT IT DOES: The course menu before a race: the title screen first when
 * it has not been shown, then the course preview, faded in when the title
 * led straight to it; the selection kept in the save at the end.
 */
int CourseMenuRun(void)
{
    switch (state) {
    case S_BOOT_DONE:
    case S_COURSE_START:
        g.draw_side = 0;
        g.menu_mode = 0;
        if (!g.course_menu_seen) {
            state = S_TITLE_OPEN;
        } else {
            preview_mode_fade = 0;
            state = S_PREVIEW_OPEN;
        }
        break;
    case S_TITLE_OPEN:
        title_open();
        pic.blank = 0;
        state = S_TITLE_SHOW;
        break;
    case S_TITLE_SHOW:
        pic.main_screen = SHOW_BG1 | SHOW_BG2 | SHOW_BG3 | SHOW_SPRITES;
        sound_hold_a = 0;
        state = S_TITLE_FADE;
        break;
    case S_TITLE_FADE:
        frame_state.brightness_target = 15;
        menu_idle = 0;
        state = S_TITLE_RUN;
        break;
    case S_TITLE_RUN:
        title_run();
        break;
    case S_TITLE_FADEOUT:
        objects_frame();
        if (frame_state.brightness == frame_state.brightness_target) {
            g.demo_number = (uint16_t)((g.demo_number + 1) & 3);
            if (g.demo_number == 3)
                g.demo_number = 0;
            sound_hold_a = 0xFF;        /* $10DA: the demo keeps the title's music */
            state = S_DEMO;
        }
        break;
    case S_DEMO: {
        int r = DemoRun(g.demo_number);

        if (r == 0)
            return 0;
        if (r > 0) {                    /* a button pressed: back to the course menu */
            preview_mode_fade = 0;
            state = S_PREVIEW_OPEN;
        } else {                        /* BootWarm */
            state = S_COURSE_START;
        }
        return 0;
    }
    case S_PREVIEW_OPEN:
        preview_open();
        pic.blank = 0;
        state = S_PREVIEW_SHOW;
        break;
    case S_PREVIEW_SHOW:
        state = S_PREVIEW_READY;
        break;
    case S_PREVIEW_READY:
        objects_frame();
        frame_state.brightness_target = 15;
        state = S_MENU_LOOP;
        break;

    /* the route from the title, cross-fading */
    case S_FADE_OPEN1:
        preview_fade_begin();
        state = S_FADE_OPEN2;
        break;
    case S_FADE_OPEN2:
        fade_level = 0x20;
        state = S_FADE_STEPS;
        break;
    case S_FADE_STEPS: {
        int i;

        for (i = 0; i < 8; i++) {
            /* colours 0-7 by (32 - level) / 32, colours 12-15 by level / 32 */
        }
        menu_palette_scale(0, 8, (uint16_t)(0x20 - fade_level));
        menu_palette_scale(0x0C, 4, (uint16_t)fade_level);
        fade_level -= 2;
        if (fade_level < 0)
            state = S_FADE_FULL;
        break;
    }
    case S_FADE_FULL:
        preview_full_mode();
        preview_palette();
        state = S_FADE_SET;
        break;
    case S_FADE_SET:
        objects_frame();
        state = S_FADE_LAST;
        break;
    case S_FADE_LAST:
        frame_state.brightness = frame_state.brightness_target = 15;
        state = S_MENU_LOOP;
        break;

    /* the menu's loop, shared by both openings */
    case S_MENU_LOOP:
        if (step_wait > 0) {
            step_wait--;
            break;
        }
        actors.fast_animation = 0;
        menu.blink_counter++;
        menu.shown_3d = 0;
        objects_frame();
        frame_state.menu_pressed[0] = 0;
        frame_state.menu_pressed[1] = 0;
        if (menu.shown_3d)
            step_wait = MENU_STEP_FRAMES - 1;
        if (actors.exit_request) {
            sound_port1_push(0x04);
            frame_state.brightness_target = 0;
            state = S_MENU_FADEOUT;
        }
        break;
    case S_MENU_FADEOUT:
        objects_frame();
        if (frame_state.brightness == frame_state.brightness_target)
            state = S_COURSE_DONE;
        break;
    case S_COURSE_DONE:
        menu_window_screen(0, 0);
        fx_draw_set_origin(0, 0);
        g.hud_mode = (uint8_t)g.menu_mode;
        g.course_menu_seen = 1;
        if (g.menu_mode == 2)
            SelectionCommit();
        else
            SelectionSave();
        for (int i = 0; i < 3; i++) {
            g.car_settings[i] = (uint8_t)g.settings_row[i];
            save.settings[i] = (uint8_t)g.settings_row[i];
        }
        SaveChecksum();
        pic.blank = 1;
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* --- selection results, kept in the save ------------------------------- */

static uint16_t handler_table_a(uint16_t i) { return cart16(0x03, (uint16_t)(0xB6F1 + i * 2)); }
static uint16_t handler_table_b(uint16_t i) { return cart16(0x03, (uint16_t)(0xB704 + i * 2)); }

/*
 * @routine 03:B548 SelectionCommit
 * WHAT IT DOES: Records the two players' choices: their handler addresses,
 * both packed as a byte into the save with the checksum renewed, and the
 * derived values.
 */
void SelectionCommit(void)
{
    uint16_t v;

    g.selection_handler[0] = handler_table_a(g.choice_p1);    /* $18DB */
    g.selection_handler[1] = handler_table_a(g.choice_p2);    /* $18DD */
    v = (uint16_t)((cart16(0x03, (uint16_t)(0xB540 + ((g.choice_p2 & 3) << 1))) << 4)
                   | cart16(0x03, (uint16_t)(0xB540 + ((g.choice_p1 & 3) << 1))));
    save.cars_battle = (uint8_t)v;
    SaveChecksum();
    g.selection_out[0] = 0;
    g.selection_out[1] = 0;
    if (g.choice_p1 == g.choice_p2)
        g.selection_out[1] = cart16(0x03, (uint16_t)(0xB464 + g.choice_p2 * 2));
    g.selection_derived[0] = cart16(0x03, (uint16_t)(0xB46C + ((g.choice_p1 << 2) + g.selection_out[0]) * 2));
    g.selection_derived[1] = cart16(0x03, (uint16_t)(0xB46C + ((g.choice_p2 << 2) + g.selection_out[1]) * 2));
}

/*
 * @routine 03:B48C SelectionSave
 * WHAT IT DOES: Records the choices: the handler addresses, the values
 * derived from them, and the choice packed into the save by the mode.
 */
void SelectionSave(void)
{
    uint16_t entry = cart16(0x03, (uint16_t)(0xB540 + ((g.choice_p1 & 3) << 1)));

    g.selection_handler[0] = handler_table_a(g.choice_p1);
    g.selection_handler[1] = handler_table_b(g.choices[0]);
    g.selection_handler[2] = handler_table_b(g.choices[1]);
    g.selection_handler[3] = handler_table_b(g.choices[2]);
    SelectionTables18CB();
    if (g.menu_mode == 3)
        save.car_mode3 = (uint8_t)cart16(0x03, (uint16_t)(0xB540 + ((g.choice_p1 & 3) << 1)));
    else if (g.menu_mode == 1)
        save.cars_mode01 = (uint8_t)((save.cars_mode01 & 0x03) | (entry << 4));
    else
        save.cars_mode01 = (uint8_t)((save.cars_mode01 & 0x30) | entry);
    SaveChecksum();
}

/*
 * @routine 07:C0CA CarSelectTwoPlayers
 * WHAT IT DOES: The two players' car choices at a two-player screen: Right
 * or Left pressed on a pad steps its car (3 only in HUD mode 3 or with bit 2
 * of the progress), each with sound $45; then both are recorded.
 * (The race screen in race/pause calls it; it writes the record here.)
 */
static void car_step(uint8_t *car, uint16_t pressed)
{
    int four;

    if (pressed & PAD_RIGHT) {
        sound_port0_push(0x45);
        *car = (uint8_t)(*car + 1);
        if (*car == 3) {
            four = g.hud_mode == 3 || (save.progress & 0x04);
            if (!four)
                *car = 0;
        } else if (*car > 3) {
            *car = 0;
        }
    } else if (pressed & PAD_LEFT) {
        sound_port0_push(0x45);
        *car = (uint8_t)(*car - 1);
        if (*car & 0x80) {
            four = g.hud_mode == 3 || (save.progress & 0x04);
            *car = four ? 3 : 2;
        }
    }
}

void CarSelectTwoPlayers(void)
{
    static const uint8_t map_car[4] = {2, 0, 1, 3};

    car_step(&g.car[0], frame_state.pressed[0]);
    car_step(&g.car[1], frame_state.pressed[1]);
    g.choice_p2 = map_car[g.car[1] & 3];
    SelectionCommit();
    g.choice_p1 = map_car[g.car[0] & 3];
    SelectionCommit();
}

/*
 * @routine 02:EA53 MenuCursorFourWay
 * WHAT IT DOES: Moves the course screen's cursor by the pad's buttons; each
 * move plays sound $45. (race/course.c owns the screen; it calls this.)
 */
void MenuCursorFourWay(void)
{
    uint16_t pad = frame_state.pressed[0];
    uint8_t v = g.time_override;

    if (v) {
        if (pad & PAD_RIGHT) {
            g.champ_stage = 2;
            g.time_override = 0;
            sound_port0_push(0x45);
        } else if (pad & PAD_LEFT) {
            g.champ_stage = 1;
            g.time_override = 0;
            sound_port0_push(0x45);
        }
    } else {
        switch (g.champ_stage) {
        case 0:
            if (pad & PAD_RIGHT) { g.champ_stage++; sound_port0_push(0x45); }
            break;
        case 1:
            if (pad & PAD_LEFT) { g.champ_stage--; sound_port0_push(0x45); }
            else if (pad & PAD_RIGHT) { g.time_override = (uint8_t)(g.time_override - 1); sound_port0_push(0x45); }
            break;
        case 2:
            if (pad & PAD_RIGHT) { g.champ_stage++; sound_port0_push(0x45); }
            else if (pad & PAD_LEFT) { g.time_override = (uint8_t)(g.time_override - 1); sound_port0_push(0x45); }
            break;
        default:
            if (pad & PAD_LEFT) { g.champ_stage--; sound_port0_push(0x45); }
            break;
        }
    }
    if (g.time_override) {
        g.flash_sprite_pos = (uint16_t)((g.flash_sprite_pos & 0xFF00) | 0x88);
    } else {
        g.cursor_column_x = cart8(0x02, (uint16_t)(0xEB49 + g.champ_stage));
        g.flash_sprite_pos = (uint16_t)((g.flash_sprite_pos & 0xFF00) | g.cursor_column_x);
    }
}

/* --- the championship's closing screen --------------------------------- */

enum { C_OPEN, C_WAIT_SHOW, C_RUN, C_FADE, C_DONE };
static int champion_state = C_OPEN;

/*
 * @routine 0B:DF7C (its frames) MenuSelectionFramesCycle
 * WHAT IT DOES: One frame of the selection's picture, for the class, on
 * four of BG2's tiles.
 */
void menu_champion_frames_tile(uint16_t step)
{
    uint16_t off = (uint16_t)(cart16(0x0B, (uint16_t)(0xDFCB + step * 2))
                              + cart16(0x0B, (uint16_t)(0xDFD3 + (g.choice_p1 & 3) * 2)));

    nshvc_tiles_write(menu_video.tiles_a, 0x15C, &champion_frames[off], 4, NSHVC_TILES_PLANAR);
}

/* A cell's attributes in the picture: `word` over the tile number. */
static void cell_attributes(uint8_t *map, uint16_t at, uint16_t word)
{
    uint16_t off[4];
    int i;

    off[0] = at;
    off[1] = (uint16_t)(at + 2);
    off[2] = (uint16_t)(at + 0x40);
    off[3] = (uint16_t)(at + 0x42);
    for (i = 0; i < 4; i++) {
        uint16_t v = (uint16_t)(map[off[i]] | map[off[i] + 1] << 8);

        v = (uint16_t)((v & 0x01FF) | word);
        map[off[i]] = (uint8_t)v;
        map[off[i] + 1] = (uint8_t)(v >> 8);
    }
}

/*
 * @routine 0B:EA3B ChampionScreenOpen
 * WHAT IT DOES: Opens the championship's closing screen with the display
 * off: mode 3, the sound, the win recorded in the save, the pictures drawn,
 * the panel coloured for the class, and the screen's objects made.
 */
static void champion_open(void)
{
    uint32_t n;
    uint8_t *map;
    int x, rows, rowi;

    pic.blank = 1;
    frame_state.brightness = 0;
    race_steps_end();                   /* no race steps drawn between behind this screen */
    race_screen_end();                  /* none of the race's layers left behind this screen */
    menu_window_screen(KEEP_BG2 | KEEP_BG3 | KEEP_SPRITES, 0);    /* past the picture: the granite, a whole page of it */
    pic.mode = 3;
    pic.bg3_priority = 0;
    pic.bg[2].draw_list.id = 0;
    pic.bg[0].tiles = tiles_8bpp;
    pic.bg[0].map = menu_video.page_0;
    pic.bg[1].tiles = menu_video.tiles_a;
    pic.bg[1].map = menu_video.pages_1_2;
    memset(&pic.blend, 0, sizeof pic.blend);
    pic.blend.use_fixed = 1;
    pic.sub_screen = 0;
    memset(&pic.windows, 0, sizeof pic.windows);
    pic.windows.mask_main[4] = 1;
    pic.windows.layer_use[4] = NSHVC_WIN1 | NSHVC_WIN1_INVERT;
    pic.windows.w[0].left = 0;
    pic.windows.w[0].right = 0;
    menu_lines_set(MENU_LINES_CHAMPION, 0);

    sound_upload_begin();
    sound_upload_packed(0xF884, 0x1D);
    sound_upload_cart(0x1B, 0x85F8);
    sound_upload_cart(0x19, 0x9A98);
    sound_upload_end();
    sound_queue_reset();
    sound_set_port0(1);
    menu_scroll_reset();

    if (g.player_phase[0] == 8 && g.course_class <= 2)
        save.progress = (uint8_t)(save.progress | (1 << g.course_class));
    SaveChecksum();

    palette_load(0x04, 0xEBCB);
    memcpy(pic.palette_saved, pic.palette, sizeof pic.palette);

    /* the table entries the window step and the blink change, as they start */
    actors_load();
    *menu_table_byte(MT_CHAMPION_WINDOW_B + 5) = 0;     /* the third run's value */
    *menu_table_byte(MT_CHAMPION_WINDOW_B + 7) = 0;     /* the fourth run's */

    picture_unpack(0x1D, 0x9068);
    memcpy(frames_tiles, picture, sizeof frames_tiles);
    n = picture_unpack(0x1C, 0xDC8C);
    {
        uint16_t w[0x400];

        memcpy(w, picture, n < sizeof w ? n : sizeof w);
        for (x = 0; x < 0x400; x++)
            w[x] = (uint16_t)(w[x] + 0x0100);
        page_write(0, w);
    }
    tiles_from_picture(menu_video.sprites, 0, picture_unpack(0x18, 0xC770), 32);
    tiles_from_picture(tiles_8bpp, 0x100, picture_unpack(0x18, 0x998C), 64);
    tiles_from_picture(menu_video.tiles_a, 0, picture_unpack(0x1C, 0x989C), 32);

    n = picture_unpack(0x04, 0xE87B);
    map = picture;
    {
        uint16_t tbl_off = (uint16_t)(g.course_class << 4);

        /* the four panel entries, coloured for the class */
        for (rowi = 3, rows = 0; rowi >= 0; rowi--, rows++) {
            uint16_t base = cart16(0x0B, (uint16_t)(0xED9F + tbl_off));
            uint16_t attr = cart16(0x0B, (uint16_t)(0xED9F + 2 + tbl_off));
            uint16_t at = cart16(0x0B, (uint16_t)(0xEDCF + 8 + rowi * 2));
            uint16_t cells = cart16(0x0B, (uint16_t)(0xEDCF + rowi * 2));
            int y, c;

            for (y = 0; y <= 4; y++) {
                for (c = 0; c <= 0x0B; c++) {
                    uint16_t a2 = (uint16_t)(at + c * 2);
                    uint16_t v = (uint16_t)(map[a2] | map[a2 + 1] << 8);

                    v = (uint16_t)((v & 0x03FF) | attr);
                    map[a2] = (uint8_t)v;
                    map[a2 + 1] = (uint8_t)(v >> 8);
                }
                at = (uint16_t)(at + 0x40);
            }
            {
                uint16_t v = (uint16_t)(base | attr);
                int i;

                for (i = 0; i < 4; i++) {
                    uint16_t u = v;

                    map[cells + i * 2] = (uint8_t)u;
                    map[cells + i * 2 + 1] = (uint8_t)(u >> 8);
                    u = (uint16_t)(u + 0x10);
                    map[cells + i * 2 + 0x40] = (uint8_t)u;
                    map[cells + i * 2 + 0x41] = (uint8_t)(u >> 8);
                    u = (uint16_t)(u + 0x10);
                    map[cells + i * 2 + 0x80] = (uint8_t)u;
                    map[cells + i * 2 + 0x81] = (uint8_t)(u >> 8);
                    v++;
                }
            }
            tbl_off = (uint16_t)(tbl_off + 4);
        }
    }
    cell_attributes(map, 0x0628, cart16(0x0B, (uint16_t)(0xEDDF + (g.choice_p1 & 3) * 2)));
    cell_attributes(map, 0x052E, cart16(0x0B, (uint16_t)(0xEDDF + (g.choice_p1 & 3) * 2)));
    for (x = 0; x < 2; x++)
        page_write(1 + x, map + x * 0x800);

    picture_unpack(0x04, 0xEBCB);
    memcpy(champion_frames, picture, sizeof champion_frames);

    menu_state_clear();
    menu.window_mode = 3;
    menu.panel_rise = 0;
    menu.panel_top_lowered = 0;
    menu.panel_class_scroll = (uint16_t)(g.course_class << 5);
    menu.panel_low_start = g.player_phase[0] == 8 ? 0x0000 : 0x0020;
    menu.panel_still_scroll = 0;
    actors.scroll_y = 0x0066;
    actors_reset();
    actors_spawn_list(0x0036);
    actors_run();
    actors_build_sprites();
    menu_lines_set(MENU_LINES_CHAMPION, 0xF0);
    pic.main_screen = 0;
}

/*
 * @routine 0B:ADD4 ChampionScreenRun
 * WHAT IT DOES: The championship's closing screen until the menu asks to
 * leave.
 */
int ChampionScreenRun(void)
{
    menu_frame_begin();
    switch (champion_state) {
    case C_OPEN:
        champion_open();
        pic.blank = 0;
        champion_state = C_WAIT_SHOW;
        break;
    case C_WAIT_SHOW:
        pic.main_screen = SHOW_BG1 | SHOW_BG2;
        frame_state.brightness_target = 15;
        champion_state = C_RUN;
        break;
    case C_RUN:
        objects_frame();
        frame_state.menu_pressed[0] = 0;
        frame_state.menu_pressed[1] = 0;
        if (actors.exit_request) {
            frame_state.brightness_target = 0;
            champion_state = C_FADE;
        }
        break;
    case C_FADE:
        objects_frame();
        if (frame_state.brightness == frame_state.brightness_target)
            champion_state = C_DONE;
        break;
    default:
        champion_state = C_OPEN;
        return 1;
    }
    menu_frame_end(1);
    return 0;
}

/* --- the frame's end, and until the boot's state machine takes over ---- */

void menu_video_more_init(void)
{
    tiles_8bpp = nshvc_tiles_create(8, 1024, 1);
    tiles_menu = nshvc_tiles_create(4, 512, 1);
    tiles_load = nshvc_tiles_create(4, 64, 1);
}

/* What the menu's stage does with the frame: which vblank stage is on. */
int menu_on_stage20(void)
{
    return state == S_MENU_LOOP || state == S_MENU_FADEOUT || (state >= S_FADE_OPEN2 && state <= S_FADE_LAST);
}

void session_start(void)
{
    state = S_SOUND_START;
    actors_load();
    menu_video_more_init();
    save_load();
}

static int after_race = S_COURSE_START;

void session_race_ended(int to_course_menu)
{
    after_race = to_course_menu ? S_COURSE_START : S_SOUND_START;
}

/* Logo, title, course menu, the race, and back. The race's flow and the
   demos end their own frames. */
void session_update(void)
{
    if (state == S_RACE) {
        stage20_on = 0;
        if (race_session_update())
            state = after_race;
        return;
    }
    menu_frame_begin();
    if (state <= S_BOOT_DONE && !MenuScreen0Run())
        goto done;
    if (state >= S_BOOT_DONE) {
        if (CourseMenuRun()) {
            after_race = S_COURSE_START;
            race_session_begin();
            state = S_RACE;
            return;
        }
        if (state == S_DEMO)
            return;
    }
done:
    menu_frame_end(menu_on_stage20());
}
