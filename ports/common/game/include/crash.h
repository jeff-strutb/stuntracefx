/*
 * crash.h: the stop screen, where the game ends when it finds itself in a
 * state it cannot go on from.
 */
#ifndef CRASH_H
#define CRASH_H
#include "game.h"

/* Shows the stop screen: the backdrop in `colour`, the screen's one layer
   scrolled down to `code`. The platform sets it; it must not return. */
extern void (*game_stop_screen)(uint16_t colour, uint16_t code);

/* The game stops with a white backdrop, code $AA (CrashWhite). */
_Noreturn void crash_white(void);
/* The game stops with a green backdrop, code $E2: no object was free for
   a script's object (CrashGreen). */
_Noreturn void crash_green(void);

#endif
