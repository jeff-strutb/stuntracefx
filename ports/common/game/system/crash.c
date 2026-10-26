/*
 * crash.c: the stop screen. The original keeps the backdrop colour at
 * $1A2E and the code at $0717, rebuilds the screen as one layer in that
 * colour, scrolls it down until its scroll reaches the code and then stops
 * for good. Here the two are kept as the original keeps them, and the
 * platform shows the screen.
 */
#include <stdlib.h>
#include "crash.h"

#define stop_colour  W16(0x1A2E)
#define stop_code    W16(0x0717)

void (*game_stop_screen)(uint16_t colour, uint16_t code);

/* Keeps the colour and the code, then hands over to the platform's stop
   screen; with none set the program ends. */
static _Noreturn void game_stop(uint16_t colour, uint16_t code)
{
    stop_colour = colour;
    stop_code = code;
    if (game_stop_screen)
        game_stop_screen(colour, code);
    abort();
}

_Noreturn void crash_white(void)
{
    game_stop(0x7FFF, 0x00AA);
}

_Noreturn void crash_green(void)
{
    game_stop(0x03E0, 0x00E2);
}
