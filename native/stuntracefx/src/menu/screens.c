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

enum {
    STATE_SOUND_START, STATE_LOGO_OPEN, STATE_LOGO_SOUND, STATE_LOGO_SHOW, STATE_LOGO_RUN, STATE_MENUS
};

static int state;

static void palette_load(uint8_t bank, uint16_t addr)
{
    int i;

    for (i = 0; i < 256; i++)
        pic.palette[i] = cart16(bank, (uint16_t)(addr + i * 2));
}

static void menu_common_mode(void)
{
    pic.mode = 1;
    pic.bg3_priority = 1;
    pic.bg[0].tiles = menu_video.bg1_4;
    pic.bg[0].map = menu_video.bg1;
    pic.bg[1].tiles = menu_video.bg2_4;
    pic.bg[1].map = menu_video.bg2;
    pic.bg[2].tiles = menu_video.bg3_2;
    pic.bg[2].map = menu_video.bg3;
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
        if (actors.exit_request) {
            pic.blank = 1;
            state = STATE_MENUS;
        }
        break;
    default:
        break;
    }
    frame_end();
}
