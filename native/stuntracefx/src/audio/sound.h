/*
 * sound.h: the game's side of the sound: the command queues, what the
 * driver is told each frame, its data's loading and the sounds the game asks
 * for. The driver itself runs on the audio thread (music.h).
 */
#ifndef STUNTRACEFX_SOUND_H
#define STUNTRACEFX_SOUND_H

#include <stdint.h>

/* ---- the queues (sound.c) ---- */

void sound_init(void);
void SoundReset(void);
void SoundPort0QueuePush(uint8_t command);      /* with player 2's mark */
void SoundPort0QueuePushRaw(uint8_t command);   /* exactly as given */
void SoundPort1QueuePush(uint8_t command);
void SoundPort0QueuePushRawDecimal(uint8_t command);   /* the same as Raw; until time_bonus.c calls Raw itself */
void sound_set_port0(uint8_t value);            /* a byte the driver is told at once */
void sound_frame(void);                         /* once a frame (the original's SoundPortsSend) */

/* the names the earlier screens use */
static inline void sound_queue_reset(void) { SoundReset(); }
static inline void sound_port0_push(uint8_t command) { SoundPort0QueuePush(command); }
static inline void sound_port0_push_raw(uint8_t command) { SoundPort0QueuePushRaw(command); }
static inline void sound_port1_push(uint8_t command) { SoundPort1QueuePush(command); }

extern uint8_t sound_muted;                     /* $0E4E: nothing is queued while set */
extern uint8_t sound_hold_a, sound_hold_b;      /* $10DA, $10DC: the frame's sends are held; also pick the sets loaded */
extern uint8_t sound_alt_set_loaded;            /* $10DE: the other set has been loaded (the soft reset clears it) */
/* Ports 2 and 3 get the bytes at $0E47 and $0E49 each frame, g.speed_gauge[1]
   and [0], and 0 while $0E4B (g.end_ready) is set. */

/* ---- loading (sound.c) ----
   The block lists build one image of the driver's memory. A load is
   sound_upload_begin, any of the uploads, sound_upload_end; the driver starts
   on the image at the end. */

void sound_upload_begin(void);                  /* the driver leaves for new data */
void sound_upload_cart(uint8_t bank, uint16_t addr);    /* ApuUpload */
void sound_upload_packed(uint16_t which, uint8_t bank);
void sound_upload_end(void);                    /* the driver starts on the image */

/* each of these between sound_upload_begin and sound_upload_end */
void SoundDataLoad(uint16_t which, uint8_t input);
void MusicLoadForCourse(uint8_t music);

/* whole loads: each brackets its uploads itself */
void ApuUploadBank19(void);
void CourseSoundLoad(void);
void RaceLoadSound(void);                       /* the sound part of RaceLoad */
void RaceModeSetupPickSound(void);              /* $0D8C and $0D8B from the first racer's handler */
void RaceModeSetupBattleSound(void);            /* the battle driver, for mode 2 */
void SoftResetApu(void);
void SoftResetToBootWarmSound(void);            /* the sound part of SoftResetToBootWarm */

/* ---- the sounds the game asks for (sound_effects.c) ---- */

void PlayerHoldSound32(void);
void PlayerHoldSound33(void);
void PlayerHoldSound34(void);
void PlayerHoldSound35(void);
void PlayerStopHeldSound(void);
void Sound0E1AStop(void);
void PlayerSound23(void);
void PlayerSound24(void);
void PlayerSound0D(void);
void PlayerSound0B(void);
void PlayerSound12(void);
void PlayerSound0C(void);
void PlayerSound35(void);
uint8_t SoundFromTableD89(void);
uint8_t SoundPairFromTable(void);
void SoundAtRateForLevel(uint16_t level);
void SoundIfFlag40(uint16_t flags, uint8_t player);
void SoundHoldByFlags(uint16_t flags, uint8_t player);
void EngineSoundsPlayer1(uint8_t player, uint16_t pressed, uint16_t held);
void EngineSoundsPlayer2(uint8_t player, uint16_t pressed, uint16_t held);
uint8_t HandlerSoundIndex(uint16_t handler);
void SoundIndicesFromHandlers(void);
void SoundNearCarOnce(void);
void NearCarSoundUpdate(void);
void WindowSlideUpSound(void);

#endif
