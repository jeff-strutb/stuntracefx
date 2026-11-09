/*
 * driver_sfx.h: the race driver's sound effect channels and the game's sound
 * rules' state.
 */
#ifndef STUNTRACEFX_DRIVER_SFX_H
#define STUNTRACEFX_DRIVER_SFX_H

#include <stdint.h>

enum {
    BANK_SFX_TABLE = 0x0702,
    BANK_COUNTDOWN_A = 0xCB10,
    BANK_COUNTDOWN_B = 0xEE40,
    BANK_FINISH_PATTERN = 0xEE48,
    BANK_POSITION_PITCH = 0xEE20,
    BANK_POSITION_SIDES = 0xEE30
};

typedef struct {
    uint8_t command, state, active, fresh;
    uint8_t stop_request, key_request, age;
    uint8_t read_pos, length, held, key_state, lifetime;
    uint8_t number;             /* the sample */
    uint8_t adsr1, adsr2, sent_adsr1, sent_adsr2;
    uint8_t mode;
    uint16_t ptr;
    uint16_t pitch, dsp_pitch, slide_delta;
    uint8_t slide_on, pitch_scale, sweep_on;
    uint16_t sweep[2];
    /* left and right */
    uint8_t vol[2], vol_frac[2], vol_int[2];
    uint8_t fade_count[2], fade_vol[2], fade_step[2];
    uint8_t loop, loop_sound, paused, paused_number;
    uint8_t hold, hold_count;
    uint8_t side, loop_side, paused_side;     /* battle: 1 or 2, whose sound it is */
} Channel;

typedef struct {
    Channel ch[8];
    uint8_t port_latch[4], port_in[4], port_seen;
    uint8_t port1_last, port3_last;
    uint8_t command, command_kept, hold_last, hold_index, hold_time;
    uint8_t queue[9];
    uint8_t effect_flags, loop_flags, loop_flags_saved, game_flags;
    uint8_t stagger_step, song5_count, song5_scale, song3_delay;
    uint8_t tempo_value, tempo_value_saved, finish_count, finish_delay;
    uint8_t timer_4c, count_3e, count_20_16, count_37, random_delay, random_target;
    uint16_t fade_amount;
    uint8_t fade_start;
    uint8_t bend_count, bend_tempo_step, bend_transpose, bend_tempo;
    uint16_t bend_acc;
    uint8_t echo_step, echo_state, echo_saved[4], echo_mode_saved[4], engine_echo_scale;
    uint8_t ramp_a_steps, ramp_b_steps;
    uint16_t ramp_a_pitch, ramp_b_pitch;
    uint8_t volume_add, pan_scale;
    uint8_t rev_counter;
    uint8_t engine_step, engine_speed, engine_quotient, engine_quotient_hi, engine_overflow, engine_rising, engine_direction;
    uint16_t engine_table;
    uint8_t noise_header;
    /* the battle driver's second player: index 0 port 2, 1 port 3 */
    uint8_t battle;
    uint8_t volume_add_4;
    uint8_t ramp_a2_steps, ramp_b2_steps;
    uint16_t ramp_a2_pitch, ramp_b2_pitch;
    uint8_t bports_last[2], b_rising[2], b_quotient[2], b_overflow[2], b_step[2], b_speed[2], b_direction[2], b_rev[2];
    uint16_t b_table[2];
} Sfx;

extern Sfx s;

void race_sound_tick(void);
void battle_sound_tick(void);
void battle_special(int x);
void channels_frame(void);
void sfx_stop_1(int x);
void stop_request_fade(int x);
void stop_all(void);
void driver_leave(void);
void music_fade(void);
void port_command(void);
void tempo_bend(void);
uint16_t sfx_pointer(uint8_t sound);
int sfx_channel_for(uint8_t sound, uint8_t header);
void channel_resume_side(int x);
void sfx_queue_push(uint8_t sound);
void sfx_stop(int x, uint8_t reason);
void sfx_release(int x);
void sfx_take_voice(int x);
void sfx_mode_apply(int x, uint8_t header);
void sfx_start_data(uint8_t sound, uint16_t data);

void driver_volume_write(Voice *v);

#endif
