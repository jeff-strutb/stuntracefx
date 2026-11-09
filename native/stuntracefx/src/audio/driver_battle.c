/*
 * driver_battle.c: the battle driver's game rules, the race driver's with two
 * players: each has a channel pair and a port for its engine, its pitch
 * ramps and its side of the stereo. Audio thread only.
 */
#include "driver.h"
#include "driver_sfx.h"

static const uint8_t command_filter[23] = {
    0x03, 0x10, 0x32, 0x2B, 0x34, 0x43, 0x44, 0x45, 0x77, 0x78, 0x79, 0x7A, 0x23, 0x3A, 0x3B, 0x3F,
    0x06, 0x22, 0x41, 0x40, 0x1F, 0x2A, 0x00
};
static const uint8_t command_blocked[25] = {
    0x02, 0x01, 0x04, 0x07, 0x0A, 0x0D, 0x25, 0x15, 0x17, 0x1E, 0x21, 0x18, 0x19, 0x1A, 0x09, 0x0C,
    0x0B, 0x2E, 0x4E, 0x2D, 0x29, 0x4D, 0x33, 0x5A, 0x00
};
static const uint8_t channel_exempt[9] = { 0x03, 0x10, 0x32, 0x30, 0x3C, 0x16, 0x20, 0x56, 0x00 };
static const uint8_t channel_order[7] = { 0x30, 0x3C, 0x38, 0x16, 0x20, 0x56, 0x00 };

static int listed(const uint8_t *list, uint8_t v)
{
    for (; *list; list++)
        if (*list == v)
            return 1;
    return 0;
}

/* A loop's stop: the bit cleared, and channel 5 stops if it plays the sound. */
static void stop_channel5(uint8_t loop_bit, uint8_t sound, uint8_t reason)
{
    s.loop_flags &= (uint8_t)~loop_bit;
    if (s.ch[5].command == sound)
        sfx_stop(5, reason);
}

/* @routine 1A:F148 SpcBStaggeredChecks: the checks spread over a 25-tick cycle. */
static void staggered_checks(void)
{
    uint8_t a = (uint8_t)(++s.stagger_step);

    switch (a) {
    case 6:
        if (s.game_flags & 0x20) {
            uint8_t c7 = s.ch[7].command, c6 = s.ch[6].command;

            if (c7 >= 0x77 && c7 <= 0x79)
                sfx_stop_1(7);
            if (c6 >= 0x77 && c6 <= 0x7A)
                sfx_stop_1(6);
        }
        return;
    case 9:
        if (s.loop_flags & 1) { sfx_queue_push(0x17); return; }
        stop_channel5(1, 0x17, 0x0C);
        return;
    case 0x0C:
        if (s.loop_flags & 2) { sfx_queue_push(0x97); return; }
        stop_channel5(2, 0x17, 0x1C);
        return;
    case 0x0F:
        if (!(s.loop_flags & 4)) { stop_channel5(4, 0x25, 0x0A); return; }
        if (!(s.loop_flags & 1) && !(s.loop_flags & 0x10)) { sfx_queue_push(0x25); return; }
        break;
    case 0x12:
        if (!(s.loop_flags & 4)) { stop_channel5(8, 0x25, 0x1A); return; }
        if (!(s.loop_flags & 2) && !(s.loop_flags & 0x20)) { sfx_queue_push(0x25); return; }
        break;
    case 0x15:
        if (s.loop_flags & 0x10) break;
        stop_channel5(0x10, 0x15, 0x0B);
        return;
    case 0x18:
        if (s.loop_flags & 0x20) break;
        stop_channel5(0x20, 0x15, 0x1B);
        return;
    default:
        if (a >= 0x19)
            s.stagger_step = 0;
        break;
    }
    if (d.song != 1)
        s.loop_flags = 0;
}

/* @routine 1A:F1D2 SpcBGameStateTick: the battle's sound rules each tick. */
static void game_state_tick(void)
{
    if (s.song5_count != 0xFF)
        s.song5_count++;
    if (d.song_request == 5) {
        s.song5_count = 0;
        s.song5_scale = 0;
    }
    if ((s.song5_count & 0x80) && s.song5_scale != 0xFF)
        s.song5_scale++;
    if (d.song_request == 2)
        s.song3_delay = 1;
    if (s.song3_delay) {
        s.song3_delay++;
        if (s.song3_delay == 0xE6) {
            s.song3_delay = 0;
            d.song_request = 3;
        }
    }
    if (!(s.song5_count & 0x80) && s.command >= 0x11 && s.command <= 0x14)
        s.command = 0;
    if (s.command == 0x32) {
        s.loop_flags = 0;
        s.game_flags |= 0x60;
        sfx_stop_1(2);
        sfx_stop_1(1);
        sfx_stop_1(0);
    }
    if (s.command == 0x03 || s.command == 0x10) {
        sfx_stop_1(5);
        sfx_stop_1(4);
        s.loop_flags = 0;
        s.game_flags |= 0x60;
    }
    if (s.timer_4c) {
        s.timer_4c++;
        if (!((uint8_t)(s.timer_4c - 0x78) & 0x80)) {
            s.game_flags |= 0x20;
            s.timer_4c = 0;
        }
    }
    if ((s.ch[5].command == 0x17 || s.ch[5].command == 0x21) && (s.port_in[1] == 0x09 || s.port_in[1] == 0x0D))
        s.port_in[1] = 0;
    if (s.ch[7].command == 0x32 && s.ch[4].command == 0x38)
        sfx_stop_1(4);
}

static void command_4c(void)
{
    if (s.command != 0x4C)
        return;
    stop_request_fade(7);
    stop_request_fade(6);
    sfx_stop_1(5);
    sfx_stop_1(4);
    sfx_stop_1(3);
    s.timer_4c = 1;
    s.loop_flags = 0;
    driver_music_pause();
    s.game_flags |= 0x40;
}

/* @routine 1A:F2F1 SpcBCommandTimers */
static void command_timers(void)
{
    if (d.flags & 1)
        return;
    if (s.command == 0x3E)
        s.count_3e = (s.port_in[0] & 0x80) ? 0x81 : 1;
    if (s.count_3e)
        s.count_3e++;
    if ((s.count_3e & 0x7F) == 0x2A) {
        uint8_t t = (s.count_3e & 0x80) ? s.ch[6].command : s.ch[7].command;

        if (t == 0x78 || t == 0x7A)
            sfx_queue_push((uint8_t)(0x0F | (s.count_3e & 0x80)));
        else if (t == 0x77 || t == 0x79)
            sfx_queue_push((uint8_t)(0x2F | (s.count_3e & 0x80)));
        s.count_3e = 0;
    }
    if (s.command == 0x20 || s.command == 0x16) {
        uint8_t t = s.ch[7].command;

        d.track_flags |= s.command == 0x20 ? 0x10 : 0x20;
        s.command = (t == 1 || t == 7) ? 0x27 : 0x1B;
        s.count_20_16 = 1;
    }
    if (s.count_20_16)
        s.count_20_16++;
    if (s.count_20_16 == 0x11) {
        s.count_20_16 = 0;
        if (d.track_flags & 0x10) {
            d.track_flags &= (uint8_t)~0x10;
            sfx_queue_push(0x20);
        } else if (d.track_flags & 0x20) {
            d.track_flags &= (uint8_t)~0x20;
            sfx_queue_push(0x16);
        }
    }
}

/* The ramps: each player's two pitch offsets. */
static void ramp_up(uint8_t *steps, uint16_t *pitch, uint8_t limit, uint16_t step)
{
    if (*steps != limit) {
        (*steps)++;
        *pitch = (uint16_t)(*pitch + step);
    }
}

static void ramp_down(uint8_t *steps, uint16_t *pitch, uint16_t step, uint8_t *flags, uint8_t both)
{
    if (*steps) {
        (*steps)--;
        *pitch = (uint16_t)(*pitch - step);
    } else {
        *flags &= (uint8_t)~both;
        *pitch = 0;
    }
}

static void pitch_ramps(void)
{
    if (s.effect_flags & 0x04) ramp_up(&s.ramp_b_steps, &s.ramp_b_pitch, 0x14, 0x20);
    if (s.effect_flags & 0x08) ramp_down(&s.ramp_b_steps, &s.ramp_b_pitch, 0x20, &s.effect_flags, 0x0C);
    if (s.effect_flags & 0x10) ramp_up(&s.ramp_b2_steps, &s.ramp_b2_pitch, 0x14, 0x20);
    if (s.effect_flags & 0x20) ramp_down(&s.ramp_b2_steps, &s.ramp_b2_pitch, 0x20, &s.effect_flags, 0x30);
    if (d.flags & 0x04) ramp_up(&s.ramp_a_steps, &s.ramp_a_pitch, 0x19, 0x3C);
    if (d.flags & 0x08) ramp_down(&s.ramp_a_steps, &s.ramp_a_pitch, 0x3C, &d.flags, 0x0C);
    if (d.flags & 0x10) ramp_up(&s.ramp_a2_steps, &s.ramp_a2_pitch, 0x19, 0x3C);
    if (d.flags & 0x20) ramp_down(&s.ramp_a2_steps, &s.ramp_a2_pitch, 0x3C, &d.flags, 0x30);
}

/* After a pause a channel's saved sound restarts for its player. */
static void channel_resume(int x)
{
    Channel *c = &s.ch[x];

    if (c->paused_side != 1 && c->paused_side != 2)
        return;
    if (c->paused_side == 1)
        s.port_in[0] &= 0x7F;
    else
        s.port_in[0] |= 0x80;
    c->paused_side = 0;
    c->number = c->paused_number;
    if (c->paused) {
        uint8_t sound = c->paused;

        c->paused = 0;
        sfx_start_data(sound, sfx_pointer(sound));
    } else {
        sfx_stop_1(x);
    }
}

static void channel_pause(int x)
{
    Channel *c = &s.ch[x];

    if (c->loop_sound || c->loop == 2) {
        c->paused = c->loop_sound;
        c->paused_side = c->loop_side;
        c->loop_sound = 0;
        c->loop = 0;
        c->loop_side = 0;
    } else if (c->loop) {
        c->paused_side = c->side;
        c->paused_number = c->number;
        c->paused = c->command;
        c->loop = 0;
    }
}

/* A looping sound's request, unless the game has stopped them. */
static void loop_request(uint8_t loop_bit, uint8_t sound)
{
    if (!(s.game_flags & 0x40)) {
        s.loop_flags |= loop_bit;
        sfx_queue_push(sound);
    }
}

/* @routine 1A:F656 SpcBPort1Command */
static void port1_command(void)
{
    uint8_t a = s.port_in[1] == s.port1_last ? 0 : s.port_in[1];
    int x;

    s.port1_last = s.port_in[1];
    if (a >= 0x3E)
        a = 0;
    switch (a) {
    case 0x05:
        stop_all();
        if (d.master >> 8)
            d.track_flags |= 0x80;
        else
            driver_leave();
        break;
    case 0x06:
        s.game_flags |= 1;
        stop_all();
        if (!(d.master >> 8))
            driver_leave();
        break;
    case 0x09: loop_request(0x04, 0x25); break;
    case 0x0A: stop_channel5(0x04, 0x25, a); break;
    case 0x0B: stop_channel5(0x10, 0x15, a); break;
    case 0x0C: stop_channel5(0x01, 0x17, a); break;
    case 0x0D: loop_request(0x10, 0x15); break;
    case 0x12: loop_request(0x01, 0x17); break;
    case 0x14: loop_request(0x08, 0xA5); break;
    case 0x15:                  /* the pause */
        if (d.flags & 1) {
            /* the original runs the save with the dispatch index as the channel,
               so it copies what stands at those addresses: channel 0's sweep flag
               into its loop state, and clears what it read */
            s.ch[0].loop = s.ch[0].sweep_on;
            s.ch[0].key_state = (uint8_t)s.echo_step;
            s.ch[0].sweep_on = 0;
            s.ch[0].vol_int[0] = 0;
            s.echo_step = 0;
            break;
        }
        s.loop_flags_saved = s.loop_flags;
        s.loop_flags = 0;
        d.flags |= 1;
        s.game_flags |= 0x40;
        sfx_queue_push(0x4A);
        for (x = 7; x >= 4; x--)
            channel_pause(x);
        for (x = 7; x >= 4; x--)
            sfx_stop_1(x);
        break;
    case 0x16:                  /* the resume */
        if (!(d.flags & 1))
            break;
        s.loop_flags = s.loop_flags_saved;
        sfx_queue_push(0x4B);
        d.flags &= (uint8_t)~1;
        s.game_flags &= (uint8_t)~0x40;
        for (x = 7; x >= 4; x--)
            channel_resume(x);
        sfx_stop_1(1);
        sfx_stop_1(0);
        break;
    case 0x18: loop_request(0x20, 0x95); break;
    case 0x19: loop_request(0x02, 0x97); break;
    case 0x1A: stop_channel5(0x08, 0x25, a); break;
    case 0x1B: stop_channel5(0x20, 0x15, a); break;
    case 0x1C: stop_channel5(0x02, 0x17, a); break;
    case 0x22:                  /* the music stops, from a pause */
        if (d.flags & 1) {
            sfx_queue_push(0x4C);
            d.flags &= (uint8_t)~1;
            for (x = 7; x >= 4; x--) {
                s.ch[x].paused = 0;
                s.ch[x].loop_sound = 0;
            }
            d.flags &= (uint8_t)~0x3C;
            driver_music_pause();
            s.ramp_a_pitch = 0;
            s.ramp_a2_pitch = 0;
        }
        break;
    case 0x23:
        if (!(d.flags & 4)) {
            d.flags = (uint8_t)((d.flags & ~8) | 4);
            s.ramp_a_pitch = 0x0200;
        }
        break;
    case 0x24:
        if (d.flags & 4) {
            d.flags = (uint8_t)((d.flags & ~4) | 8);
            s.ramp_a_pitch = (uint16_t)(s.ramp_a_pitch - 0x0200);
        }
        break;
    case 0x25:
        if (!(d.flags & 0x10)) {
            d.flags = (uint8_t)((d.flags & ~0x20) | 0x10);
            s.ramp_a2_pitch = 0x0200;
        }
        break;
    case 0x26:
        if (d.flags & 0x10) {
            d.flags = (uint8_t)((d.flags & ~0x10) | 0x20);
            s.ramp_a2_pitch = (uint16_t)(s.ramp_a2_pitch - 0x0200);
        }
        break;
    case 0x3A:
        if (!(s.effect_flags & 4)) {
            s.effect_flags = (uint8_t)((s.effect_flags & ~8) | 4);
            s.ramp_b_pitch = 0x0040;
        }
        break;
    case 0x3B:
        if (s.effect_flags & 4) {
            s.effect_flags = (uint8_t)((s.effect_flags & ~4) | 8);
            s.ramp_b_pitch = (uint16_t)(s.ramp_b_pitch - 0x0040);
        }
        break;
    case 0x3C:
        if (!(s.effect_flags & 0x10)) {
            s.effect_flags = (uint8_t)((s.effect_flags & ~0x20) | 0x10);
            s.ramp_b2_pitch = 0x0040;
        }
        break;
    case 0x3D:
        if (s.effect_flags & 0x10) {
            s.effect_flags = (uint8_t)((s.effect_flags & ~0x10) | 0x20);
            s.ramp_b2_pitch = (uint16_t)(s.ramp_b2_pitch - 0x0040);
        }
        break;
    default:
        if (a >= 0x32 && a <= 0x35)
            s.volume_add = (uint8_t)(a - 0x32);
        else if (a >= 0x36 && a <= 0x39)
            s.volume_add_4 = (uint8_t)(a - 0x36);
        break;
    }
}

static void channel_check(void)
{
    uint8_t sound = s.command;
    int y;

    if (!sound) {
        s.command_kept = 0;
        return;
    }
    if (listed(channel_exempt, sound))
        return;
    for (y = 0; channel_order[y]; y++) {
        uint8_t header = bank_byte(sfx_pointer(sound));
        int x = sfx_channel_for(sound, header), k;

        if (channel_order[y] != s.ch[x].command)
            continue;
        for (k = 0; channel_order[k] && channel_order[k] != sound; k++)
            ;
        if (channel_order[k] && !((uint8_t)(y - k) & 0x80))
            continue;
        s.command_kept = sound;
        s.command = 0;
        return;
    }
}

/* The queue and the channels at the end of the tick. */
static void sound_tick_end(void)
{
    uint8_t sound;
    int i;

    if (s.command)
        s.hold_index = 0;
    else if (s.hold_index != 0xFF)
        s.hold_index++;
    if (!s.command && s.hold_index > 2 && s.queue[0]) {
        sound = s.queue[0];
        for (i = 1; i < 8; i++)
            s.queue[i - 1] = s.queue[i];
        if (sound & 0x80)
            s.port_in[0] |= 0x80;
        sound &= 0x7F;
        s.hold_index = 0;
    } else {
        sound = s.command;
        if (sound == s.hold_last || !(s.hold_last = sound, sound)) {
            channels_frame();
            return;
        }
    }
    {
        uint16_t data = sfx_pointer(sound);
        uint8_t header = bank_byte(data);
        int x = sfx_channel_for(sound, header);
        Channel *c = &s.ch[x];

        sfx_mode_apply(x, header);
        c->side = (s.port_in[0] & 0x80) ? 2 : 1;
        c->active = 1;
        c->ptr = data;
        c->fresh = sound;
        channels_frame();
        c->fresh = 0;
    }
}

/* @routine 1B:84C7 SpcBEngineVolume: each player's engine on one side. */
static void engine_volume(int x, int port, uint8_t divisor, uint8_t base)
{
    Channel *c = &s.ch[x];
    uint8_t a = (uint8_t)(base + spc_div_quotient((uint8_t)~s.port_in[2 + port], divisor));

    if (d.flags & (port ? 0x04 : 0x10))
        a = (uint8_t)(a + 4);   /* an ADC #3 with the compare's carry set */
    if (!(s.effect_flags & (port ? 0x04 : 0x10)))
        a = (uint8_t)(((uint16_t)a * 0xD0) >> 8);
    a = (uint8_t)(((uint16_t)a * s.song5_scale) >> 8);
    c->vol[port ? 0 : 1] = a;
    c->vol[port ? 1 : 0] = 0;
}

/* @routine 1B:8303 SpcBSfxSpecial */
void battle_special(int x)
{
    static const uint8_t divisor[4] = { 0x0C, 0x13, 0x55, 0x7F };
    static const uint8_t base[4] = { 0x23, 0x1A, 0x17, 0x17 };
    static const uint8_t multiple_hi[4] = { 0x0C, 0x0B, 0x0E, 0x07 };
    static const uint8_t multiple_lo[4] = { 0x1B, 0x1E, 0x1F, 0x12 };
    Channel *c = &s.ch[x];
    uint8_t command = c->command;
    int index = command == 0x79 ? 0 : command == 0x78 ? 1 : command == 0x77 ? 2 : command == 0x7A ? 3 : -1;

    if (index >= 0) {
        int port;
        uint8_t speed;
        uint16_t pitch, table;

        if (x != 7 && x != 6)
            return;
        port = x == 7 ? 1 : 0;
        engine_volume(x, port, divisor[index], base[index]);
        speed = s.port_in[2 + port];
        if (speed != s.bports_last[port]) {
            uint8_t diff, div, overflow, quotient;
            uint16_t numerator;

            s.b_step[port] = 0;
            s.b_speed[port] = speed;
            diff = (uint8_t)(speed - s.bports_last[port]);
            if (diff & 0x80) {
                diff = (uint8_t)(0 - diff);
                s.b_direction[port] = 2;
            } else {
                s.b_direction[port] = 1;
            }
            div = s.b_rising[port] ? s.b_rising[port] : 1;
            numerator = (uint16_t)(diff << 8);
            overflow = diff >= div;
            quotient = spc_div(numerator, div);
            s.b_quotient[port] = quotient;
            s.b_overflow[port] = overflow;
            s.b_rising[port] = 0;
        } else {
            if (!speed) {
                s.b_quotient[port] = 0;
                s.b_overflow[port] = 0;
                s.b_table[port] = 0;
            } else if ((speed & 0x80) && speed >= 0xF0) {
                s.b_table[port] = (uint16_t)(s.b_table[port] + 3);
            } else {
                s.b_table[port] = 0;
            }
            if (s.b_rising[port] >= 0x0A) {
                uint16_t q = (uint16_t)((s.b_overflow[port] << 8 | s.b_quotient[port]) - 1);

                if (q & 0x8000)
                    q = 0;
                s.b_quotient[port] = (uint8_t)q;
                s.b_overflow[port] = (uint8_t)(q >> 8);
            }
            if (s.b_rising[port] != 0xFF)
                s.b_rising[port]++;
            {
                uint16_t value = s.b_rising[port] == 1 ? (uint16_t)(speed << 8) : (uint16_t)(s.b_speed[port] << 8 | s.b_step[port]);
                uint16_t step = (uint16_t)(s.b_overflow[port] << 8 | s.b_quotient[port]);

                if (s.b_direction[port] == 2) {
                    if (value >= step) {
                        value = (uint16_t)(value - step);
                        s.b_step[port] = (uint8_t)value;
                        s.b_speed[port] = (uint8_t)(value >> 8);
                    }
                } else if ((uint32_t)value + step < 0x10000u) {
                    value = (uint16_t)(value + step);
                    s.b_step[port] = (uint8_t)value;
                    s.b_speed[port] = (uint8_t)(value >> 8);
                }
            }
        }
        pitch = (uint16_t)((uint16_t)multiple_lo[index] * s.b_speed[port] + (uint16_t)(multiple_hi[index] << 8));
        table = s.b_table[port];
        pitch = (uint16_t)(pitch + (port ? s.ramp_a_pitch : s.ramp_a2_pitch) + (port ? s.ramp_b_pitch : s.ramp_b2_pitch) + table);
        pitch = engine_pitch_top(pitch);
        s.bports_last[port] = speed;
        c->dsp_pitch = pitch;
    } else if (command == 0x17 || command == 0x25) {
        uint8_t divide = command == 0x17 ? 0x14 : 0x1E, add = command == 0x17 ? 0x0A : 0x03, request = command == 0x17 ? 2 : 1;
        int port;

        if (x != 5 && x != 4)
            return;
        port = x == 5 ? 1 : 0;
        {
            uint8_t limit = (uint8_t)(spc_div_quotient((uint8_t)~s.port_in[2 + port], divide) + add);

            s.b_rev[port]++;
            if ((uint8_t)(limit - s.b_rev[port]) & 0x80) {
                c->key_request = request;
                s.b_rev[port] = (uint8_t)(request - 1);
            }
        }
    }
}

/* @routine 1A:F095 SpcBSfxFrame */
void battle_sound_tick(void)
{
    if (s.port_latch[0] != s.port_seen) {
        s.port_seen = s.port_latch[0];
        s.port_in[0] = s.port_latch[0];
    } else {
        s.port_in[0] = 0;
    }
    s.port_in[1] = s.port_copy[1] = s.port_latch[1];
    s.port_in[2] = s.port_copy[2] = s.port_latch[2];
    s.port_in[3] = s.port_copy[3] = s.port_latch[3];
    if (d.flags & 2)
        d.flags &= (uint8_t)~3;
    if (s.port_in[0] == 0x88)
        s.port_in[0] = 0x15;
    music_fade();
    port_command();
    if ((s.game_flags & 0x40) && s.ch[5].command != 0x43)
        sfx_stop_1(5);
    if (listed(command_blocked, s.command))
        s.command = 0;
    channel_check();
    tempo_bend();
    staggered_checks();
    game_state_tick();
    pitch_ramps();
    if (d.song != 1 && !listed(command_filter, s.command))
        s.command = 0;
    command_4c();
    command_timers();
    {
        int x;

        for (x = 0; x < 7; x++)
            if (s.ch[x].number == 0x23)
                break;
        if (x < 7) {
            int i;

            for (i = 0; i < 28; i++) {
                uint8_t hi = (uint8_t)(d.random >> 8), lo = (uint8_t)d.random;
                uint8_t in = (uint8_t)(((hi >> 6) ^ (hi >> 5)) & 1);

                d.random = (uint16_t)((uint16_t)(hi << 1 | lo >> 7) << 8 | (uint8_t)(lo << 1 | in));
            }
        }
    }
    if (d.song != 1 && !((uint8_t)(s.port_in[1] - 7) & 0x80))
        s.port_in[1] = 0;
    port1_command();
    sound_tick_end();
}
