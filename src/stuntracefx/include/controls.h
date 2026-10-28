/*
 * controls.h: the game's own controls, beside the joypads the console reads.
 */
#ifndef SRFX_CONTROLS_H
#define SRFX_CONTROLS_H

/* The race reads player `player`'s pad for its car (object/gsu_record.c),
   once a race frame: the game's front end takes it that the player is
   driving (a touch screen is then a wheel and pedals). No time and nothing
   of the game's state. */
void port_controls_read(int player);

#endif
