/*
 * game.h: the game's frame and its parts.
 */
#ifndef STUNTRACEFX_GAME_H
#define STUNTRACEFX_GAME_H

#include <stdint.h>

#include "nshvc/nshvc.h"

/* the pad's buttons, as the pad words hold them */
enum {
    PAD_R = 0x0010, PAD_L = 0x0020, PAD_X = 0x0040, PAD_A = 0x0080,
    PAD_RIGHT = 0x0100, PAD_LEFT = 0x0200, PAD_DOWN = 0x0400, PAD_UP = 0x0800,
    PAD_START = 0x1000, PAD_SELECT = 0x2000, PAD_Y = 0x4000, PAD_B = 0x8000
};

/* the frame's end: the fades, the sound and the counters */
typedef struct {
    uint16_t held[2], pressed[2];
    uint16_t pad_now[2];                /* the pads as read this update */
    uint16_t pressed_copy[2];           /* $0D1A, $0D1C: the race's presses, folded in by the raster */
    uint16_t menu_pressed[2];
    uint8_t brightness, brightness_target, half_rate;
    uint8_t frames;
} FrameState;

extern FrameState frame_state;

uint16_t frame_menu_pressed(void);              /* player 1's presses since the screen last cleared them */
void frame_menu_pressed_clear(int player);
void input_read(const nshvc_input *input);
void frame_end(void);
int system_stopped(void);                /* crash.c */
void JoypadAccumulate(void);
void JoypadRead(void);
void JoypadAutoAccelerate(void);        /* touch: player 1's gas held while driving (input/joypad.c) */

/* menu/screens.c */
void session_start(void);
void session_update(void);

#endif
