/*
 * music.h: the game's music code on the engine: the sound driver run on the
 * audio thread, and what the game thread hands it.
 */
#ifndef STUNTRACEFX_MUSIC_H
#define STUNTRACEFX_MUSIC_H

#include <stdint.h>

#include "audio/driver.h"

/* game thread */
void music_ports(const uint8_t ports[4]);       /* what the game has told the driver */
void music_bank_set(const uint8_t image[0x10000], int battle);  /* a sound memory image, samples and all */
void music_bank_fill(SoundBank *b, const uint8_t image[0x10000]);       /* the same, into a bank of the caller's */
void music_stop(void);                          /* the driver leaves, and waits for its next bank */

/* audio thread */
void music_tick(void);
void music_pass(void);                          /* one 64-sample pass of the driver */

#endif
