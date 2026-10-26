/*
 * pads.c: the two joypads. The platform sets the buttons held on each
 * before the frame's work (bit 15 B, 14 Y, 13 select, 12 start, 11 up, 10
 * down, 9 left, 8 right, 7 A, 6 X, 5 L, 4 R); the game keeps what was
 * held and what was newly pressed.
 */
#include "game.h"

uint16_t pads[2];

#define pad_held(n)         W16(0x0309 + 4 * (n))
#define pad_pressed(n)      W16(0x0311 + 2 * (n))
#define pad_pressed_kept(n) W16(0x0D1A + 2 * (n))   /* collected between the game's own frames */
#define pad_last(n)         W16(0x0D1E + 2 * (n))

/* Both pads: the buttons pressed since the last read, and those held. */
void joypad_read(void)
{
    int n;

    for (n = 0; n < 2; n++)
        pad_pressed(n) = pad_pressed_kept(n) = (uint16_t)((pads[n] ^ pad_held(n)) & pads[n]);
    pad_held(1) = pads[1];
    pad_held(0) = pads[0];
}

/* This frame's presses collected for game code that runs less often than
   the frame: each pad's presses since its last sample added to its kept
   ones; then, as the original does, both pads' presses taken against pad
   2's held buttons into pad 2's pressed (the second kept), and both held
   gaining what is held now. */
void joypad_accumulate(void)
{
    int n;

    for (n = 0; n < 2; n++) {
        pad_pressed_kept(n) |= (uint16_t)((pads[n] ^ pad_last(n)) & pads[n]);
        pad_last(n) = pads[n];
    }
    pad_pressed(1) = (uint16_t)((pads[1] ^ pad_held(1)) & pads[1]);
    pad_held(1) |= pads[1];
    pad_held(0) |= pads[0];
}

/* The frame's sound and pads, the presses also collected for the menus
   ($118B, $118D). */
void frame_sound_and_joypads(void)
{
    sound_ports_send();
    joypad_read();
    W16(0x118B) |= pad_pressed(0);
    W16(0x118D) |= pad_pressed(1);
}
