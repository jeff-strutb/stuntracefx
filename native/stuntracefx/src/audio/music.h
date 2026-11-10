/*
 * music.h: the game's music code on the engine: the sound driver run on the
 * audio thread, and what the game thread hands it.
 */
#ifndef STUNTRACEFX_MUSIC_H
#define STUNTRACEFX_MUSIC_H

#include <stdint.h>

#include "audio/driver.h"

/* game thread */
void music_push(int port, uint8_t value);       /* a command onto port 0's or 1's queue */
void music_queues_reset(void);                  /* both queues emptied, ports 2 and 3 to 0 */
void music_port0(uint8_t value);                /* a byte put straight on port 0 */
void music_frame(int held, uint8_t port2, uint8_t port3);     /* a frame's end */
void music_bank_set(const uint8_t image[0x10000], int battle);  /* a sound memory image, samples and all */
void music_bank_fill(SoundBank *b, const uint8_t image[0x10000]);       /* the same, into a bank of the caller's */
void music_stop(void);                          /* the driver leaves, and waits for its next bank */
void music_pause(int on);                       /* the pause screen: the sound silent and the driver held, or on again */

/* audio thread */
void music_tick(void);
void music_pass(void);                          /* one 64-sample pass of the driver */

#endif
