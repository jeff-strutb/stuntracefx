/*
 * options.c: the game's options (plan stage 10), kept in the save folder
 * beside the save as "options": auto-accelerate, for driving on a touch
 * screen, on until the player turns it off; and the race's view, behind
 * the car (the game's own) or from above (race/overhead.c), as the player
 * last switched it. The frame rate and the picture's shape are no options:
 * the game is the Enhanced game (decisions 4 and 5).
 */
#include <stdint.h>

#include "nshvc/nshvc.h"
#include "save/save.h"

static const char OPTIONS_NAME[] = "options";

typedef struct {
    uint8_t size;                       /* sizeof (OptionsFile); a file of the first layout (2: no view) is read too */
    uint8_t auto_accelerate;            /* 1: on a touch screen the gas is held while driving */
    uint8_t overhead;                   /* 1: the race seen from above */
} OptionsFile;

static OptionsFile options = { sizeof (OptionsFile), 1, 0 };
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
    }
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
    nshvc_save_write(OPTIONS_NAME, &options, sizeof options);
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
    nshvc_save_write(OPTIONS_NAME, &options, sizeof options);
}
