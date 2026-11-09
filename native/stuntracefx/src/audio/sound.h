/*
 * sound.h: the game's side of the sound: the command queues, what the
 * driver is told each frame, and its data's loading.
 */
#ifndef STUNTRACEFX_SOUND_H
#define STUNTRACEFX_SOUND_H

#include <stdint.h>

void sound_init(void);
void sound_queue_reset(void);
void sound_port0_push(uint8_t command);
void sound_port0_push_raw(uint8_t command);     /* without player 2's mark */
void sound_port1_push(uint8_t command);
void sound_set_port0(uint8_t value);            /* a byte the driver is told at once */
void sound_frame(void);                         /* once a frame */
void sound_upload_begin(void);                  /* the driver leaves for new data */
void sound_upload_cart(uint8_t bank, uint16_t addr);
void sound_upload_packed(uint16_t which, uint8_t bank);
void sound_upload_end(void);                    /* the driver starts on it */

extern uint8_t sound_muted, sound_player2;
extern uint8_t sound_hold_a, sound_hold_b;
extern uint8_t sound_port2, sound_port3, sound_mute_engines;

#endif
