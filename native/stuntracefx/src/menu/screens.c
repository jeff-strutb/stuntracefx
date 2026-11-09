/*
 * screens.c: the game from its start. Each state's code runs in one update;
 * where the original waited for the frame, the update returns.
 */
#include <string.h>

#include "audio/sound.h"
#include "data/cart.h"
#include "menu/actors.h"
#include "system/game.h"
#include "video/picture.h"
#include "data/unpack.h"

enum {
    STATE_SOUND_START, STATE_LOGO_OPEN, STATE_LOGO_SOUND, STATE_LOGO_SHOW, STATE_LOGO_RUN,
    STATE_TITLE_OPEN, STATE_TITLE_SHOW, STATE_TITLE_FADE, STATE_TITLE_RUN, STATE_NOT_CONVERTED
};

static int state;

static void palette_load(uint8_t bank, uint16_t addr)
{
    int i;

    for (i = 0; i < 256; i++)
        pic.palette[i] = cart16(bank, (uint16_t)(addr + i * 2));
}

/* The packed pictures: the data's own end address names one. */
static uint8_t picture[0x8000];

static uint32_t picture_unpack(uint8_t bank, uint16_t end)
{
    return unpack((uint32_t)bank << 16 | end, picture, sizeof picture);
}

static void tiles_from_picture(nshvc_tiles set, uint32_t first, uint32_t bytes, uint32_t per_tile)
{
    nshvc_tiles_write(set, first, picture, bytes / per_tile, NSHVC_TILES_PLANAR);
}

/* A picture's map pages, 32 x 32 words each, from page `first` on. */
static void pages_from_picture(int first, uint32_t bytes)
{
    uint32_t i;

    for (i = 0; i < bytes / 0x800; i++)
        menu_page_write(first + (int)i, picture + i * 0x800);
}

static void menu_common_mode(void)
{
    pic.mode = 1;
    pic.bg3_priority = 1;
    pic.bg[0].tiles = menu_video.tiles_a;
    pic.bg[0].map = menu_video.page_0;
    pic.bg[1].tiles = menu_video.tiles_b;
    pic.bg[1].map = menu_video.pages_1_2;
    pic.bg[2].tiles = menu_video.tiles_text;
    pic.bg[2].map = menu_video.text;
    pic.line_tables = 0;
    memset(&pic.blend, 0, sizeof pic.blend);
    pic.blend.use_fixed = 1;
    memset(&pic.windows, 0, sizeof pic.windows);
}

static void menu_scroll_reset(void)
{
    int i;

    for (i = 0; i < 3; i++)
        pic.bg[i].scroll_x = pic.bg[i].scroll_y = 0;
    actors.scroll_x = actors.scroll_y = 0;
}

/* The objects made and placed before the screen first shows. */
static void menu_objects_start(uint16_t list)
{
    actors_reset();
    actors_spawn_list(list);
    actors_run();
    actors_build_sprites();
}

/* The first screen: the logo, drawn with sprites alone. */
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

/* The title screen, with its music. */
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
    menu_objects_start(6);
}

void menu_load_start(Actor *a)
{
    (void)a;
    state = STATE_NOT_CONVERTED;
}

void session_start(void)
{
    state = STATE_SOUND_START;
    actors_load();
}

void session_update(void)
{
    switch (state) {
    case STATE_SOUND_START:
        sound_upload_begin();
        sound_upload_cart(0x19, 0xC27C);
        sound_upload_end();
        state = STATE_LOGO_OPEN;
        /* fall through */
    case STATE_LOGO_OPEN:
        logo_open();
        pic.blank = 0;
        state = STATE_LOGO_SOUND;
        break;
    case STATE_LOGO_SOUND:
        sound_port0_push(0x39);
        state = STATE_LOGO_SHOW;
        break;
    case STATE_LOGO_SHOW:
        frame_state.brightness = frame_state.brightness_target = 15;
        pic.main_screen = SHOW_SPRITES;
        state = STATE_LOGO_RUN;
        break;
    case STATE_LOGO_RUN:
        actors_run();
        actors_build_sprites();
        if (actors.exit_request)
            state = STATE_TITLE_OPEN;
        break;
    case STATE_TITLE_OPEN:
        title_open();
        pic.blank = 0;
        state = STATE_TITLE_SHOW;
        break;
    case STATE_TITLE_SHOW:
        pic.main_screen = SHOW_BG1 | SHOW_BG2 | SHOW_BG3 | SHOW_SPRITES;
        sound_hold_a = 0;
        state = STATE_TITLE_FADE;
        break;
    case STATE_TITLE_FADE:
        frame_state.brightness_target = 15;
        menu_idle = 0;
        state = STATE_TITLE_RUN;
        break;
    case STATE_TITLE_RUN: {
        Actor *a;

        actors_run();
        actors_build_sprites();
        a = actor_find(0x0012);
        if (a && a->handler == HANDLER_BUTTON_SEQUENCE && a->count == 0) {
            menu_idle++;
            if (menu_idle == 0x0400) {
                frame_state.brightness_target = 0;
                state = STATE_NOT_CONVERTED;
            }
        }
        frame_state.menu_pressed[0] = 0;
        frame_state.menu_pressed[1] = 0;
        if (actors.exit_request)
            state = STATE_NOT_CONVERTED;
        break;
    }
    default:
        break;
    }
    frame_end();
}
