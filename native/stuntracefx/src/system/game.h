/*
 * game.h: the game's frame and its parts.
 */
#ifndef STUNTRACEFX_GAME_H
#define STUNTRACEFX_GAME_H

#include <stdint.h>

#include "nshvc/nshvc.h"

/* the frame's end: the fades, the sound and the counters */
typedef struct {
    uint16_t held[2], pressed[2];
    uint16_t pressed_copy[2];           /* $0D1A, $0D1C: the race's presses, folded in by the raster */
    uint16_t last[2];                   /* $0D1E, $0D20 */
    uint16_t menu_pressed[2];
    uint8_t brightness, brightness_target, half_rate;
    uint8_t frames;
} FrameState;

extern FrameState frame_state;

uint16_t frame_menu_pressed(void);              /* player 1's presses since the screen last cleared them */
void frame_menu_pressed_clear(int player);
void input_read(const nshvc_input *input);
void frame_end(void);

/* menu/screens.c */
void session_start(void);
void session_update(void);

#endif
