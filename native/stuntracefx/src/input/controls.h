/*
 * controls.h: the game's controls (controls.c). The engine reports each
 * device as it is (keys, controllers, the pointer, every finger); the game
 * decides what each does, here: the pad bits (system/game.h PAD_*) its
 * logic reads, for players 1 and 2.
 */
#ifndef STUNTRACEFX_CONTROLS_H
#define STUNTRACEFX_CONTROLS_H

#include <stdint.h>

#include "nshvc/nshvc.h"

/* Each update, first: both players' pads from the devices. Player 1: the
   keyboard, the first controller and the touch screen; player 2: the second
   controller. A controller's left stick always steers. */
void controls_read(const nshvc_input *in, uint16_t pad[2]);

/* The race, each update it reads the player's controls (not in a demo, not
   paused): the touch screen drives (the wheel, the pedals, the gas) from
   the next update on, until an update passes without this. */
void controls_driving(void);

/* In draw: the touch screen's controls laid over the picture, once a finger
   has been on it. */
void controls_draw(void);

/* The device player 1 last used. */
enum { CONTROLS_KEYBOARD, CONTROLS_CONTROLLER, CONTROLS_TOUCH };
int controls_source(void);

/* Player 1's bindings (the pause screen's): button is one PAD_* bit; a key
   by its nshvc_key, a controller button by its one NSHVC_CTL_* bit. */
void controls_bind_key(uint16_t button, int key);
void controls_bind_controller(uint16_t button, uint16_t ctl);
/* what is bound to button on the device player 1 last used, as that device
   names it ("" for none) */
const char *controls_name(uint16_t button);

#endif
