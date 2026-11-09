/*
 * driver.h: the sound driver's state, shared by its music sequencer
 * (driver_music.c) and the game's sound rules (driver_race.c,
 * driver_battle.c). All of it lives on the audio thread.
 *
 * The song, sample and table data keep their own format: a 64 KB bank in
 * which positions are 16-bit offsets, as the data itself stores them.
 */
#ifndef STUNTRACEFX_DRIVER_H
#define STUNTRACEFX_DRIVER_H

#include <stdint.h>

#include "nshvc/nshvc.h"

/* where the driver's tables sit in the bank */
enum {
    BANK_INSTRUMENTS = 0x0590,
    BANK_GATES = 0x0650,
    BANK_VELOCITIES = 0x0658,
    BANK_SONGS = 0x07F4,
    BANK_FIR_SETS = 0x3FAA,
    BANK_SAMPLE_DIRECTORY = 0x3F00
};

#define DRIVER_VOICES 8
#define DRIVER_SAMPLES 256

/* A song and its samples, prepared on the game thread and handed over whole. */
typedef struct {
    uint8_t bytes[0x10000];
    nshvc_sample samples[DRIVER_SAMPLES];
} SoundBank;

typedef struct {
    uint16_t track;             /* position of the voice's next command; high byte 0: no track */
    uint8_t note_timer, note_keyoff;
    uint8_t sub_count;
    uint8_t volume_fade, pan_fade;
    uint8_t slide_steps, slide_delay;
    uint8_t vibrato_wait, vibrato_depth;
    uint8_t tremolo_wait, tremolo_depth;
    uint8_t vibrato_fade_step;
    uint8_t note_length, note_gate, note_velocity, instrument;
    uint16_t pitch_scale;       /* the instrument's tuning, 8.8 */
    uint16_t sub_return, sub_start;
    uint8_t env_steps, env_delay, env_dir, env_semitones;
    uint8_t vibrato_phase, vibrato_rate, vibrato_delay, vibrato_fade_len;
    uint8_t vibrato_fade_delta, vibrato_target;
    uint8_t tremolo_phase, tremolo_rate, tremolo_delay;
    uint8_t voice_transpose;
    uint16_t volume, volume_delta;      /* 8.8 */
    uint8_t volume_target, volume_out;
    uint16_t pan, pan_delta;            /* 8.8 */
    uint8_t pan_target, pan_flags;
    uint16_t pitch, slide_delta;        /* note number, 8.8 */
    uint8_t slide_target, tuning;
    uint8_t source;             /* the sample the next key-on plays */
} Voice;

typedef struct {
    const SoundBank *bank;
    Voice voice[DRIVER_VOICES];

    uint8_t current;            /* the voice being worked */
    uint8_t current_bit;
    uint8_t sfx_voices;         /* voices a sound effect holds */
    uint8_t song_request, song, song_last;
    uint8_t flags, track_flags;
    uint16_t pattern;
    uint8_t pattern_loops, pattern_delay;
    uint8_t music_acc, sfx_acc; /* the tempo accumulators */
    uint8_t key_on, key_off;
    uint8_t noise_voices, pitch_mod_voices, echo_voices, echo_enable;
    uint8_t flg;
    uint8_t echo_delay_wait, echo_delay, echo_feedback;
    uint8_t transpose;
    uint16_t tempo, tempo_delta;
    uint8_t tempo_fade, tempo_target;
    uint16_t master, master_delta;
    uint8_t master_fade, master_target;
    uint8_t volume_dirty;
    uint8_t percussion_base;
    uint16_t echo_left, echo_right, echo_delta_left, echo_delta_right;
    uint8_t echo_fade, echo_target_left, echo_target_right;
    uint8_t volume_scale;
    uint16_t random;

    /* the working values the tick's steps pass along */
    uint16_t value;             /* a pitch or pan being moved */
    uint8_t negative;           /* bit 7: the delta's sign */
    uint8_t moved;              /* bit 7: the value moved between ticks */

    /* what the DSP was last told, so that only changes reach the engine */
    int8_t  sent_left[DRIVER_VOICES], sent_right[DRIVER_VOICES];
    uint8_t sent_flags;         /* noise, echo and modulation masks, sent as options */
    uint8_t sent_noise_voices, sent_echo_voices, sent_mod_voices, sent_flg;
    int8_t  sent_echo_left, sent_echo_right, sent_feedback;
    uint8_t sent_delay;
    int8_t  fir[8];
    uint8_t fir_dirty;
    uint8_t dsp_delay;          /* the echo delay the DSP has */
} Driver;

extern Driver d;

/* driver_music.c */
void driver_reset(void);
void driver_music_tick(void);
void driver_between_ticks(void);
void driver_output(void);
void driver_set_echo_delay(uint8_t delay);
void driver_set_instrument(Voice *v, uint8_t instrument);
void driver_music_pause(void);
void driver_keys_release(void);
uint8_t bank_byte(uint16_t at);
uint16_t bank_word(uint16_t at);

#endif
