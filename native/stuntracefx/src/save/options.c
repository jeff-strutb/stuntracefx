/*
 * options.c: the game's options (plan stage 10), kept in the save folder
 * beside the save as "options". The one option: auto-accelerate, for
 * driving on a touch screen, on until the player turns it off. The frame
 * rate and the picture's shape are no options: the game is the Enhanced
 * game (decisions 4 and 5).
 */
#include <stdint.h>

#include "nshvc/nshvc.h"
#include "save/save.h"

static const char OPTIONS_NAME[] = "options";

typedef struct {
    uint8_t size;                       /* sizeof (OptionsFile): a file of another layout is not read */
    uint8_t auto_accelerate;            /* 1: on a touch screen the gas is held while driving */
} OptionsFile;

static OptionsFile options = { sizeof (OptionsFile), 1 };
static int loaded;

static void options_load(void)
{
    OptionsFile f;

    loaded = 1;
    if (nshvc_save_read(OPTIONS_NAME, &f, sizeof f) == (int)sizeof f && f.size == sizeof f)
        options.auto_accelerate = f.auto_accelerate != 0;
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
