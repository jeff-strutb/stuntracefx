/*
 * options.c: the game's options (plan stage 10), kept in the save folder
 * beside the save as "options": auto-accelerate, for driving on a touch
 * screen, on until the player turns it off; and the race's view, behind
 * the car (the game's own) or from above (race/overhead.c), as the player
 * last switched it; the FPS counter (system/fps.c), shown until the player
 * hides it; and full screen or a window, full screen until the player
 * chooses a window (Jeff, 2026-11-17: "full screen to be the default on
 * first launch when a user hasn't yet set their preference"). The frame
 * rate and the picture's shape are no options: the game is the Enhanced
 * game (decisions 4 and 5).
 */
#include <stdint.h>

#include "nshvc/nshvc.h"
#include "save/save.h"

static const char OPTIONS_NAME[] = "options";

typedef struct {
    uint8_t size;                       /* sizeof (OptionsFile); a file of the first layout (2: no view) is read too */
    uint8_t auto_accelerate;            /* 1: on a touch screen the gas is held while driving */
    uint8_t overhead;                   /* 1: the race seen from above */
    uint8_t fps;                        /* 1: the FPS counter shown */
    uint8_t full_screen;                /* 1: full screen, 0: a window */
} OptionsFile;

static OptionsFile options = { sizeof (OptionsFile), 1, 0, 1, 1 };
static int loaded;

static void options_load(void)
{
    OptionsFile f;

    int n;

    loaded = 1;
    n = nshvc_save_read(OPTIONS_NAME, &f, sizeof f);
    if (n >= 2 && n == f.size && f.size <= sizeof f) {
        options.auto_accelerate = f.auto_accelerate != 0;
        if (f.size >= 3)
            options.overhead = f.overhead != 0;
        if (f.size >= 4)
            options.fps = f.fps != 0;
        if (f.size >= 5)
            options.full_screen = f.full_screen != 0;
    }
}

static void options_write(void)
{
    nshvc_save_write(OPTIONS_NAME, &options, sizeof options);
}

int option_auto_accelerate(void)
{
    if (!loaded)
        options_load();
    return options.auto_accelerate;
}

void option_auto_accelerate_set(int on)
{
    if (!loaded)
        options_load();
    options.auto_accelerate = on != 0;
    options_write();
}

int option_overhead(void)
{
    if (!loaded)
        options_load();
    return options.overhead;
}

void option_overhead_set(int on)
{
    if (!loaded)
        options_load();
    options.overhead = on != 0;
    options_write();
}

int option_fps(void)
{
    if (!loaded)
        options_load();
    return options.fps;
}

void option_fps_set(int on)
{
    if (!loaded)
        options_load();
    options.fps = on != 0;
    options_write();
}

int option_full_screen(void)
{
    if (!loaded)
        options_load();
    return options.full_screen;
}

void option_full_screen_set(int on)
{
    if (!loaded)
        options_load();
    if (options.full_screen == (on != 0))
        return;
    options.full_screen = on != 0;
    options_write();
}
