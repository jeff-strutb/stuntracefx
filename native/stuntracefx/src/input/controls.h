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
   keyboard, the touch screen and the first controller used; player 2: the
   next controller used where two play; every controller where one does (see
   controls.c). A controller's left stick always steers. */
void controls_read(const nshvc_input *in, uint16_t pad[2]);
/* the engine's controller (an index of nshvc_input.controller[]) that is
   player 0's or 1's, -1 none yet */
int controls_controller(int player);
/* Counts the times the owners were dropped (a controller connected or
   disconnected): what is kept of a player's controller, its last buttons,
   is then another's. */
uint32_t controls_owner_epoch(void);
/* controller c's buttons (NSHVC_CTL_*) as the game takes them: those it held
   as it appeared are its rest and count for nothing until let go */
uint16_t controls_controller_buttons(const nshvc_input *in, int c);

/* The race, each update it reads the player's controls (not in a demo, not
   paused): the touch screen drives (the wheel, the pedals, the gas) from
   the next update on, until an update passes without this. */
void controls_driving(void);

/* In draw: the touch screen's controls laid over the picture, once a finger
   has been on it. */
void controls_draw(void);
/* In draw, in its place: none of them shown (the pause screen's blur is
   being made from the pictures). */
void controls_hide(void);

/* The device player 1 last used. */
enum { CONTROLS_KEYBOARD, CONTROLS_CONTROLLER, CONTROLS_TOUCH };
int controls_source(void);

/* The bindings (the pause screen's): button is one PAD_* bit; a key by its
   nshvc_key (player 1's only: player 2 has no keys), a controller button by
   its one NSHVC_CTL_* bit, for player 0 or 1. */
void controls_bind_key(uint16_t button, int key);
void controls_bind_controller(int player, uint16_t button, uint16_t ctl);
/* what is bound to button for player 0 or 1, as the device names it ("" for
   none): player 1's on the device player 1 last used, player 2's on their
   controller */
const char *controls_name(int player, uint16_t button);

#endif
