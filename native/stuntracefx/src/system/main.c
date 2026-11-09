/*
 * main.c: the game on the engine. The engine owns the loop and calls these.
 */
#include <string.h>

#include "nshvc/nshvc.h"

static void init(void)
{
}

static void update(const nshvc_input *input)
{
    (void)input;
}

static void draw(float between)
{
    nshvc_screen screen;

    (void)between;
    memset(&screen, 0, sizeof screen);
    screen.width = 256;
    screen.height = 224;
    screen.blank = 1;
    nshvc_screen_set(&screen);
}

int main(int argc, char **argv)
{
    static nshvc_game game;

    game.title = "Stunt Race FX";
    game.id = "stunt-race-fx";
    game.update_hz = 60;
    game.init = init;
    game.update = update;
    game.draw = draw;
    return nshvc_run(&game, argc, argv);
}
