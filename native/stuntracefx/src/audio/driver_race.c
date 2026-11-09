/*
 * driver_race.c: the race driver's game rules on top of the sequencer: the
 * ports from the game, the sound effect channels, the engine sounds, the
 * pitch ramps and the echo transition. Audio thread only.
 */
#include <string.h>

#include "driver.h"
#include "driver_sfx.h"

Sfx s;

/* The channels' voice bits */
static uint8_t voice_mask(int x) { return (uint8_t)(1u << (x & 7)); }

/*
 * @routine 19:D0AE SpcSoundQueuePush
 * WHAT IT DOES: Appends a sound to the zero-ended queue.
 */
void sfx_queue_push(uint8_t sound)
{
    int i = 0;

    while (s.queue[i])
        i++;
    s.queue[i] = sound;
}

/*
 * @routine 19:D5D6 SpcSfxStop
 * WHAT IT DOES: Requests the stop of a channel, when it is active.
 */
void sfx_stop(int x, uint8_t reason)
{
    if (s.ch[x].active)
        s.ch[x].stop_request = reason;
}

void sfx_stop_1(int x) { sfx_stop(x, 1); }

static void stop_if(int x, uint8_t loop_bit, uint8_t sound, uint8_t reason)
{
    s.loop_flags &= (uint8_t)~loop_bit;
    if (s.ch[x].command == sound)
        sfx_stop(x, reason);
}

/*
 * @routine 19:D71C SpcSfxStopRequest
 * WHAT IT DOES: A channel's queued and looping state cleared and, if active,
 * its fade-out started.
 */
void stop_request_fade(int x)
{
    Channel *c = &s.ch[x];

    c->loop_sound = 0;
    c->loop = 0;
    s.loop_flags = 0;
    if (c->active) {
        c->fade_count[0] = 1;
        c->fade_count[1] = 1;
    }
}

static void fade_out_start(void)
{
    if (!(d.flags & 1))
        d.flags |= 0x40;
}

/* Every channel stopped with a fade, and the music fade started. */
void stop_all(void)
{
    int x;

    s.loop_flags = 0;
    fade_out_start();
    for (x = 7; x >= 0; x--)
        stop_request_fade(x);
}

/* The driver's end: every key off, and it waits for its next upload. */
void driver_leave(void)
{
    int i;

    for (i = 0; i < DRIVER_VOICES; i++)
        nshvc_voice_key_off((uint32_t)i);
    d.bank = NULL;
}

/* ---- the sound data's small tables, as the driver holds them ---- */

static const uint8_t command_filter[25] = {
    0x03, 0x58, 0x32, 0x35, 0x2B, 0x34, 0x43, 0x44, 0x45, 0x01, 0x04, 0x07, 0x0A, 0x0D, 0x23, 0x3A,
    0x3B, 0x3F, 0x06, 0x22, 0x41, 0x40, 0x1F, 0x2A, 0x00
};
static const uint8_t command_blocked[6] = { 0x02, 0x25, 0x15, 0x17, 0x21, 0x00 };
static const uint8_t channel_exempt[14] = {
    0x03, 0x32, 0x37, 0x30, 0x3C, 0x16, 0x20, 0x54, 0x31, 0x57, 0x3D, 0x35, 0x39, 0x00
};
static const uint8_t channel_order[24] = {
    0x5A, 0x4D, 0x4E, 0x2D, 0x2E, 0x0B, 0x0C, 0x29, 0x33, 0x43, 0x30, 0x3C, 0x38, 0x3E, 0x56, 0x31,
    0x2C, 0x57, 0x3D, 0x16, 0x20, 0x35, 0x39, 0x00
};
static const uint8_t note_table[24] = {
    0x7C, 0x21, 0x7A, 0x23, 0x96, 0x25, 0xD2, 0x27, 0x30, 0x2A, 0xB2, 0x2C,
    0x5A, 0x2F, 0x2C, 0x32, 0x28, 0x35, 0x50, 0x38, 0xAA, 0x3B, 0x36, 0x3F
};
static const uint8_t special_sounds[16] = {
    0x07, 0x04, 0x01, 0x0A, 0x0D, 0x74, 0x73, 0x72, 0x75, 0x76, 0x17, 0x25, 0x21, 0x02, 0x58, 0x00
};

/* ---- the channels ---- */

uint16_t sfx_pointer(uint8_t sound)
{
    return bank_word((uint16_t)(BANK_SFX_TABLE + (uint16_t)(uint8_t)((sound - 1) * 2)));
}

/*
 * @routine 19:DABB SpcSfxRelease
 * WHAT IT DOES: Gives a voice back to the music: its sound effect bit
 * cleared, the music's instrument reloaded, its echo bit back and its volume
 * rewritten.
 */
void sfx_release(int x)
{
    Voice *v = &d.voice[x];
    uint8_t bit = voice_mask(x);

    d.sfx_voices &= (uint8_t)~bit;
    d.current = (uint8_t)x;
    d.current_bit = bit;
    driver_set_instrument(v, v->instrument);
    if (d.echo_enable & bit)
        d.echo_voices |= bit;
    else
        d.echo_voices &= (uint8_t)~bit;
    d.sfx_voices &= (uint8_t)~bit;
    d.current_bit |= bit;
    d.volume_dirty |= (uint8_t)(x + 1);
    d.current = (uint8_t)x;
    d.current_bit = bit;
    driver_volume_write(v);
}

/* Takes a voice from the music and keys it off. */
void sfx_take_voice(int x)
{
    d.sfx_voices |= voice_mask(x);
    d.key_off |= voice_mask(x);
}

/*
 * @routine 19:DCF2 SpcSfxEngineScale
 * WHAT IT DOES: In engine mode a volume is scaled by the engine speed from
 * port 3 ((speed * 3) / 2 + $80, the high byte of the product), to at most
 * $7F.
 */
static uint8_t engine_scale(const Channel *c, uint8_t volume)
{
    uint16_t factor;
    uint8_t speed, y;

    if (c->mode != 4)
        return volume;
    speed = (s.battle && c->side == 2) ? s.port_in[2] : s.port_in[3];
    if (!speed)
        speed = 1;
    factor = (uint16_t)(((uint16_t)speed * 3) / 2 + 0x80);
    y = (uint8_t)((volume * (factor & 0xFF)) >> 8);
    y = (uint8_t)(y + volume * (factor >> 8));
    return (y & 0x80) ? 0x7F : y;
}

/* The per-note pitch scale correction, as the sound data asks. */
static uint16_t pitch_scaled(uint16_t pitch, uint8_t scale)
{
    uint16_t extra;

    if ((pitch >> 8) >= 0x10) {
        uint8_t q = (uint8_t)(pitch / 0x43);

        extra = (uint16_t)((uint8_t)(((uint16_t)q * 4) / 0xFC) * scale);
    } else {
        uint8_t q = (uint8_t)(pitch / 0x11);
        uint8_t r = q ? (uint8_t)(0xFF / q) : 0xFF;

        extra = r ? (uint8_t)(scale / r) : 0xFF;
    }
    return (uint16_t)(pitch + extra);
}

/* A note number of the sound data to its pitch. */
static void sound_note(Channel *c, uint8_t note)
{
    uint8_t n = (uint8_t)(note - 0x80), octave = (uint8_t)(n / 12), semi = (uint8_t)(n % 12);
    uint16_t pitch = (uint16_t)(note_table[semi * 2] | note_table[semi * 2 + 1] << 8);
    uint8_t count = (uint8_t)(octave - 7);

    while (count) {
        uint8_t hi = (uint8_t)(pitch >> 8), lo = (uint8_t)pitch, carry = (uint8_t)(hi & 1);

        hi >>= 1;
        lo = (uint8_t)(((uint16_t)carry << 8 | lo) / 2);
        pitch = (uint16_t)(hi << 8 | lo);
        count++;
    }
    c->pitch = pitch;
    c->dsp_pitch = c->pitch_scale ? pitch_scaled(pitch, c->pitch_scale) : pitch;
}

static void pitch_slide(Channel *c)
{
    if (c->slide_on) {
        uint16_t p = (uint16_t)(c->pitch + c->slide_delta);

        c->pitch = p;
        if (p & 0x8000)
            c->dsp_pitch = 0;
        else if ((p >> 8) >= 0x40)
            c->dsp_pitch = 0x3FFF;
        else
            c->dsp_pitch = p;
    }
}

static void volume_slide_side(Channel *c, int side, uint16_t delta)
{
    uint16_t v = (uint16_t)((uint16_t)(c->vol_int[side] << 8 | c->vol_frac[side]) + delta);

    c->vol_frac[side] = (uint8_t)v;
    c->vol_int[side] = (uint8_t)(v >> 8);
    if (v & 0x8000)
        c->vol[side] = (uint8_t)((v >> 8) >= 0xC0 ? 0 : 0x7F);
    else
        c->vol[side] = (uint8_t)(v >> 8);
}

/* A battle's volume command: engine mode scales both sides; otherwise side 1's
   sound plays on the left only and side 2's on the right. */
static void battle_volume(Channel *c, uint8_t left, uint8_t right)
{
    if (c->mode == 1) {
        c->vol[0] = c->vol_int[0] = engine_scale(c, left);
        c->vol[1] = c->vol_int[1] = engine_scale(c, right);
    } else if (c->side == 2) {
        c->vol[0] = c->vol_int[0] = 0;
        c->vol[1] = c->vol_int[1] = engine_scale(c, right);
    } else {
        c->vol[0] = c->vol_int[0] = engine_scale(c, left);
        c->vol[1] = c->vol_int[1] = 0;
    }
    c->vol_frac[0] = c->vol_frac[1] = 0;
}

/*
 * @routine 19:DB30 SpcSfxRead
 * WHAT IT DOES: The channel's sound data for this tick: its age counts (a
 * lifetime stops it); on its first tick its state is reset; while a note's
 * length runs the slides step; then bytes are read until the next length.
 */
static void sfx_read(int x)
{
    Channel *c = &s.ch[x];
    uint16_t at = c->ptr;
    uint8_t y;

    if (!c->command) {
        c->age = 0;
        return;
    }
    if (c->lifetime && (s.battle ? c->age == c->lifetime : !((uint8_t)(c->age - c->lifetime) & 0x80)))
        sfx_stop_1(x);
    if (c->age != 0xFF)
        c->age++;
    at = c->ptr;
    if (c->age == 0)
        return;
    if (c->age == 1) {
        c->read_pos = 0;
        c->slide_on = c->sweep_on = 0;
        c->key_state = 0;
        c->lifetime = 0;
        c->pitch_scale = 0;
        c->vol_frac[0] = c->vol_frac[1] = 0;
        c->fade_count[0] = c->fade_count[1] = 0;
        c->mode = 0;
        c->vol[0] = c->vol_int[0] = 0;
        c->vol[1] = c->vol_int[1] = 0;
        c->dsp_pitch = c->pitch = 0;
        c->adsr1 = 0x8F;
        c->adsr2 = 0xE0;
        d.echo_voices &= (uint8_t)~voice_mask(x);
        y = 1;
    } else {
        c->length = (uint8_t)(c->length - 1);
        if (c->length != 0 && c->length != 0xFF) {
            pitch_slide(c);
            if (s.battle)
                return;
            if (c->sweep_on) {
                volume_slide_side(c, 0, c->sweep[0]);
                volume_slide_side(c, 1, c->sweep[1]);
            }
            return;
        }
        y = (uint8_t)(c->read_pos + 1);
    }
    for (;;) {
        uint8_t a = bank_byte((uint16_t)(at + y));

        if (a == 0) {
            y++;
            continue;
        }
        if (!(a & 0x80)) {
            if (c->key_state == 0)
                d.key_off |= voice_mask(x);
            if (c->key_state == 0 || c->key_state == 1)
                c->key_request = 1;
            c->key_state = 0;
            c->length = a;
            c->held = a == 0x7F;
            c->read_pos = y;
            c->ptr = at;
            return;
        }
        if (a < 0xE0) {
            sound_note(c, a);
            y++;
            continue;
        }
        switch (a) {
        case 0xE0:              /* the end, unless held */
            if (!c->held)
                sfx_stop_1(x);
            return;
        case 0xE1: c->key_state = 1; break;
        case 0xE2: c->key_state = 2; break;
        case 0xE3:
            y++;
            c->number = bank_byte((uint16_t)(at + y));
            break;
        case 0xFE:              /* one volume for both sides */
            y++;
            if (s.battle) {
                battle_volume(c, bank_byte((uint16_t)(at + y)), bank_byte((uint16_t)(at + y)));
                break;
            }
            c->vol[0] = c->vol_int[0] = (c->mode == 1 || c->mode == 0 || c->mode > 2) ?
                engine_scale(c, bank_byte((uint16_t)(at + y))) : bank_byte((uint16_t)(at + y));
            c->vol[1] = c->vol_int[1] = c->vol[0];
            c->vol_frac[0] = c->vol_frac[1] = 0;
            break;
        case 0xE4:              /* the two volumes */
            y++;
            if (s.battle) {
                uint8_t left = bank_byte((uint16_t)(at + y));

                y++;
                battle_volume(c, left, bank_byte((uint16_t)(at + y)));
                break;
            }
            if (c->mode == 2) {
                c->vol[0] = c->vol_int[0] = bank_byte((uint16_t)(at + y));
                y++;
                c->vol[1] = c->vol_int[1] = bank_byte((uint16_t)(at + y));
            } else {
                c->vol[0] = c->vol_int[0] = engine_scale(c, bank_byte((uint16_t)(at + y)));
                y++;
                c->vol[1] = c->vol_int[1] = engine_scale(c, bank_byte((uint16_t)(at + y)));
            }
            c->vol_frac[0] = c->vol_frac[1] = 0;
            break;
        case 0xE5:
            y++;
            c->adsr1 = (uint8_t)((c->adsr1 & 0xF0) | (bank_byte((uint16_t)(at + y)) & 0x0F) | 0x80);
            break;
        case 0xE6:
            y++;
            c->adsr1 = (uint8_t)((c->adsr1 & 0x8F) | (uint8_t)((bank_byte((uint16_t)(at + y)) & 7) << 4) | 0x80);
            break;
        case 0xE7:
            y++;
            c->adsr2 = (uint8_t)((c->adsr2 & 0x1F) | (uint8_t)((bank_byte((uint16_t)(at + y)) & 7) << 5));
            break;
        case 0xE8:
            y++;
            c->adsr2 = (uint8_t)((c->adsr2 & 0xE0) | (bank_byte((uint16_t)(at + y)) & 0x1F));
            break;
        case 0xE9:
            c->slide_delta = (uint16_t)(bank_byte((uint16_t)(at + (uint8_t)(y + 1))) | bank_byte((uint16_t)(at + (uint8_t)(y + 2))) << 8);
            y = (uint8_t)(y + 2);
            c->slide_on = 1;
            break;
        case 0xEA:
            if (s.battle || !(s.effect_flags & 1))
                d.echo_voices |= voice_mask(x);
            break;
        case 0xEB:
            d.echo_voices &= (uint8_t)~voice_mask(x);
            break;
        case 0xEC: {
            uint8_t lo = bank_byte((uint16_t)(at + (uint8_t)(y + 1))), hi = bank_byte((uint16_t)(at + (uint8_t)(y + 2)));
            uint8_t i = 0xBC;

            y = (uint8_t)(y + 2);
            for (;;) {
                while (bank_byte((uint16_t)(BANK_SFX_TABLE + i)) != lo)
                    i = (uint8_t)(i + 2);
                i++;
                if (bank_byte((uint16_t)(BANK_SFX_TABLE + i)) == hi) {
                    sfx_start_data((uint8_t)((i >> 1) + 1), (uint16_t)(hi << 8 | lo));
                    break;
                }
                i++;
                if (!i)
                    break;
            }
            break;
        }
        case 0xED: {
            uint8_t b0 = bank_byte((uint16_t)(at + (uint8_t)(y + 1))), b1 = bank_byte((uint16_t)(at + (uint8_t)(y + 2)));
            uint8_t b2 = bank_byte((uint16_t)(at + (uint8_t)(y + 3))), b3 = bank_byte((uint16_t)(at + (uint8_t)(y + 4)));

            c->sweep[0] = (uint16_t)(b1 << 8 | b0);
            c->sweep[1] = (uint16_t)(b3 << 8 | b2);
            y = (uint8_t)(y + 4);
            c->sweep_on = 1;
            break;
        }
        case 0xEE:
            y++;
            c->pitch_scale = bank_byte((uint16_t)(at + y));
            break;
        case 0xF1: c->slide_on = 0; break;
        case 0xF2: c->sweep_on = 0; break;
        case 0xF3: {
            uint8_t lo = bank_byte((uint16_t)(at + (uint8_t)(y + 1))), hi = bank_byte((uint16_t)(at + (uint8_t)(y + 2)));

            at = (uint16_t)(hi << 8 | lo);
            y = 0;
            continue;
        }
        case 0xF8:
            y++;
            c->lifetime = bank_byte((uint16_t)(at + y));
            break;
        case 0xFB:
            d.key_off |= voice_mask(x);
            c->key_state = 2;
            break;
        case 0xFC:
            y++;
            c->mode = bank_byte((uint16_t)(at + y));
            break;
        case 0xFD:
            y++;
            s.noise_header = (uint8_t)(bank_byte((uint16_t)(at + y)) & 0xFE);
            if (!d.random)
                d.random = 1;
            break;
        default:                /* $EF, $F0, $F4-$F7, $F9, $FA, $FF: the read ends here; a battle sound ends */
            if (s.battle && !c->held)
                sfx_stop_1(x);
            return;
        }
        y++;
    }
}

/* The engine sounds: the volume from the speed on port 3. */
static uint8_t engine_damp(uint8_t a)
{
    return (s.effect_flags & 4) ? a : (uint8_t)(((uint16_t)a * 0xD0) >> 8);
}

static void engine_volume(int x, uint8_t divisor, uint8_t floor_value)
{
    Channel *c = &s.ch[x];
    uint8_t a = spc_div_quotient((uint8_t)~s.port_in[3], divisor);

    a = (uint8_t)(floor_value + a);
    if (s.effect_flags & 1)
        a = (uint8_t)(((uint16_t)a * (uint8_t)(s.engine_echo_scale - 1)) >> 8);
    if (d.flags & 4)
        a = (uint8_t)(a + 3);
    a = engine_damp(a);
    a = (uint8_t)(((uint16_t)a * s.song5_scale) >> 8);
    if (s.game_flags & 0x10)
        a = (uint8_t)(((uint16_t)a * 0xC0) >> 8);
    c->vol[0] = c->vol[1] = a;
}

static void engine_pitch(int x, uint8_t index)
{
    static const uint8_t divisor[10] = { 0x0C, 0x13, 0x55, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t base[10] = { 0x23, 0x1A, 0x17, 0x17, 0x16, 0x0D, 0x11, 0x11, 0x14, 0x20 };
    static const uint8_t multiple_hi[10] = { 0x0C, 0x0B, 0x0E, 0x07, 0x06, 0x0C, 0x0B, 0x0E, 0x07, 0x06 };
    static const uint8_t multiple_lo[10] = { 0x1B, 0x1E, 0x1F, 0x12, 0x0F, 0x1B, 0x1E, 0x1F, 0x12, 0x0F };
    Channel *c = &s.ch[x];
    uint16_t pitch;

    engine_volume(x, divisor[index], base[index]);
    if (s.port3_last != s.port_in[3]) {
        uint8_t diff, quotient, overflow = 0;
        uint16_t numerator;

        s.engine_step = 0;
        s.engine_speed = s.port_in[3];
        diff = (uint8_t)(s.port_in[3] - s.port3_last);
        if (diff & 0x80) {
            diff = (uint8_t)(0 - diff);
            s.engine_direction = 2;
        } else {
            s.engine_direction = 1;
        }
        {
            uint8_t div = s.engine_rising ? s.engine_rising : 1;

            numerator = (uint16_t)(diff << 8);
            overflow = (uint8_t)(numerator >> 8) >= div;
            quotient = spc_div(numerator, div);
        }
        s.engine_quotient = quotient;
        s.engine_overflow = overflow;
        s.engine_rising = 0;
    } else {
        if (!s.port_in[3]) {
            s.engine_quotient = 0;
            s.engine_quotient_hi = 0;
            s.engine_table = 0;
        } else if ((s.port_in[3] & 0x80) && s.port_in[3] >= 0xF0) {
            s.engine_table = (uint16_t)(s.engine_table + 3);
        } else {
            s.engine_table = 0;
        }
        if (s.engine_rising >= 0x0A) {
            uint16_t q = (uint16_t)((s.engine_overflow << 8 | s.engine_quotient) - 1);

            if (q & 0x8000)
                q = 0;
            s.engine_quotient = (uint8_t)q;
            s.engine_overflow = (uint8_t)(q >> 8);
        }
        if (s.engine_rising != 0xFF)
            s.engine_rising++;
        {
            uint16_t value;
            uint16_t step = (uint16_t)(s.engine_overflow << 8 | s.engine_quotient);

            if (s.engine_rising == 1)
                value = (uint16_t)(s.port_in[3] << 8);
            else
                value = (uint16_t)(s.engine_speed << 8 | s.engine_step);
            if (s.engine_direction == 2) {
                if (value >= step) {
                    value = (uint16_t)(value - step);
                    s.engine_step = (uint8_t)value;
                    s.engine_speed = (uint8_t)(value >> 8);
                }
            } else {
                uint32_t sum = (uint32_t)value + step;

                if (!(sum >> 16)) {
                    s.engine_step = (uint8_t)sum;
                    s.engine_speed = (uint8_t)(sum >> 8);
                }
            }
        }
    }
    pitch = (uint16_t)(((uint16_t)multiple_lo[index] * s.engine_speed) + (uint16_t)(multiple_hi[index] << 8));
    pitch = (uint16_t)(pitch + s.ramp_a_pitch + s.ramp_b_pitch + s.engine_table);
    if ((pitch >> 8) >= 0x3F)
        pitch = (uint16_t)(0x3F00 | (pitch & 0xFF));
    s.port3_last = s.port_in[3];
    c->dsp_pitch = pitch;
}

/* The rev pulse: restarts the channel's note at the speed's rate. */
static void rev_timer(int x, uint8_t divisor, uint8_t base, uint8_t request)
{
    uint8_t limit = (uint8_t)(spc_div_quotient((uint8_t)~s.port_in[3], divisor) + base);

    s.rev_counter++;
    if ((uint8_t)(limit - s.rev_counter) & 0x80) {
        s.ch[x].key_request = request;
        s.rev_counter = 1;
    }
}

/* Port 2's positional sound: pitch and sides from the distance and bearing. */
static uint8_t pan_scale(uint8_t a)
{
    uint8_t q = (uint8_t)(((uint16_t)a * (uint8_t)(s.pan_scale + 1)) / 0x15);

    return (uint8_t)(q - (s.port_in[2] & 0x0F));
}

static void position_sound(int x)
{
    Channel *c = &s.ch[x];
    uint8_t p = s.port_in[2], a = (p & 7), up = p & 8;
    uint16_t offset, base = c->pitch;

    if (!up)
        a = (uint8_t)(a ^ 7);
    offset = bank_word((uint16_t)(BANK_POSITION_PITCH + a * 2));
    c->dsp_pitch = up ? (uint16_t)(base + offset) : (uint16_t)(base - offset);
    a = (uint8_t)(p >> 4);
    if (p & 0x80) {
        a = (uint8_t)((a & 0x0F) ^ 0x0F);
        c->vol[1] = pan_scale(bank_byte((uint16_t)(BANK_POSITION_SIDES + a * 2)));
        c->vol[0] = pan_scale(bank_byte((uint16_t)(BANK_POSITION_SIDES + a * 2 + 1)));
    } else {
        c->vol[0] = pan_scale(bank_byte((uint16_t)(BANK_POSITION_SIDES + a * 2)));
        c->vol[1] = pan_scale(bank_byte((uint16_t)(BANK_POSITION_SIDES + a * 2 + 1)));
    }
}

/*
 * @routine 19:E059 SpcSfxSpecial
 * WHAT IT DOES: The per-tick work of the sounds that follow the game: the
 * engine sounds' pitch and volume from the car's speed on port 3, the rev
 * pulses and the positional sounds.
 */
static void sfx_special(int x)
{
    Channel *c = &s.ch[x];
    int i;

    if (s.battle) {
        battle_special(x);
        return;
    }
    for (i = 0; special_sounds[i]; i++)
        if (special_sounds[i] == c->command)
            break;
    if (!special_sounds[i])
        return;
    if (i < 10) {
        engine_pitch(x, (uint8_t)i);
    } else if (i == 10) {           /* $17 */
        rev_timer(x, 0x14, 0x0A, 2);
    } else if (i == 11) {           /* $25 */
        rev_timer(x, 0x1E, 0x03, 1);
    } else if (i == 12) {           /* $21 */
        rev_timer(x, 0x28, 0x03, 2);
        if (c->key_request == 2)
            s.effect_flags ^= 0x40;
        c->dsp_pitch = (s.effect_flags & 0x40) ? 0x0110 : 0x0180;
    } else {                        /* $02, $58 */
        position_sound(x);
    }
}

/*
 * @routine 19:DFC0 SpcSfxFadeVolume
 * WHAT IT DOES: A side's volume while the channel fades out: the start volume
 * and per-step drop taken at the first step, interpolated down over $3C
 * steps; at the end the channel stops (or the driver leaves, by flags).
 */
static uint8_t fade_volume(int x, int side)
{
    Channel *c = &s.ch[x];
    uint8_t count = c->fade_count[side];
    uint16_t volume = (uint16_t)(c->vol[side] << 8 | c->vol_frac[side]);

    if (count == 0x3C) {
        if ((d.track_flags & 0x80) || (s.game_flags & 1)) {
            driver_leave();
            return 0;
        }
        sfx_stop_1(x);
        c->fade_count[side] = 0xFF;
        return 0;
    }
    if (count == 0xFF) {
        c->fade_count[side] = 0;
        return 0;
    }
    if (count == 1) {
        uint16_t step = (uint16_t)(volume / 0x3C);

        c->fade_vol[side] = (uint8_t)step;
        c->fade_step[side] = (uint8_t)(step >> 8);
    }
    c->fade_count[side] = (uint8_t)(count + 1);
    {
        uint8_t n = (uint8_t)(c->fade_count[side] - 1);
        uint32_t product = ((uint32_t)c->fade_step[side] << 8 | c->fade_vol[side]) * n;
        uint16_t drop = (uint16_t)product;

        return (uint8_t)((uint16_t)(volume - drop) >> 8);
    }
}

static uint8_t volume_adjust(const Channel *c, uint8_t a)
{
    if (a && c->command == 0x25) {
        if (s.battle) {
            int x = (int)(c - s.ch);

            if (x == 5)
                return (uint8_t)(a + (uint8_t)(s.volume_add * 4));
            if (x == 4)
                return (uint8_t)(a + (uint8_t)(s.volume_add_4 * 4));
            return a;
        }
        return (uint8_t)(a + (uint8_t)(s.volume_add * 4));
    }
    return a;
}

/* The channel's voice settings to the voice, every tick. */
static void channel_output(int x)
{
    Channel *c = &s.ch[x];
    uint8_t l, r;

    l = (c->active && c->fade_count[0]) ? fade_volume(x, 0) : c->vol[0];
    l = volume_adjust(c, l);
    r = (c->active && c->fade_count[1]) ? fade_volume(x, 1) : c->vol[1];
    r = volume_adjust(c, r);
    if ((int8_t)l != d.sent_left[x] || (int8_t)r != d.sent_right[x]) {
        nshvc_voice_volume((uint32_t)x, (int8_t)l, (int8_t)r);
        d.sent_left[x] = (int8_t)l;
        d.sent_right[x] = (int8_t)r;
    }
    nshvc_voice_pitch((uint32_t)x, (uint16_t)(c->dsp_pitch & 0x3FFF));
    d.voice[x].source = c->number;
    if (c->adsr1 != c->sent_adsr1 || c->adsr2 != c->sent_adsr2) {
        nshvc_voice_adsr((uint32_t)x, c->adsr1 & 15, (c->adsr1 >> 4) & 7, c->adsr2 >> 5, c->adsr2 & 31);
        c->sent_adsr1 = c->adsr1;
        c->sent_adsr2 = c->adsr2;
    }
}

static void channel_read(int x)
{
    sfx_read(x);
    sfx_special(x);
    channel_output(x);
}

/*
 * @routine 19:D982 SpcSfxChannelStep
 * WHAT IT DOES: The channel's state machine: a new sound starts with its
 * voice taken from the music; playing, it is keyed on and read; stop
 * requests and the end of a sound release the voice.
 */
static void channel_begin(int x, uint8_t sound)
{
    Channel *c = &s.ch[x];

    c->command = sound;
    c->state = 1;
    c->age = 0;
    c->stop_request = 0;
    sfx_take_voice(x);
}

/* The channel a sound takes. In a battle a sound with header bit 6 takes the
   channel below when the second player's bit is set on port 0, and the
   sounds $77-$7A take their sample from the player. */
int sfx_channel_for(uint8_t sound, uint8_t header)
{
    int x = header & 15;

    if (s.battle) {
        if ((header & 0x40) && (s.port_in[0] & 0x80))
            x--;
        if (sound >= 0x77 && sound <= 0x7A)
            s.ch[x & 7].number = (s.port_in[0] & 0x80) ? 0x20 : 0x1C;
    }
    return x & 7;
}

void sfx_start_data(uint8_t sound, uint16_t data)
{
    uint8_t header = bank_byte(data);
    int x;
    Channel *c;

    /* header modes 2 and 3 keep a channel's sound */
    if (header < 8 && (s.ch[header].mode == 2 || s.ch[header].mode == 3))
        return;
    x = sfx_channel_for(sound, header);
    c = &s.ch[x];
    sfx_mode_apply(x, header);
    if (s.battle)
        c->side = (s.port_in[0] & 0x80) ? 2 : 1;
    c->ptr = data;
    c->active = 1;
    channel_begin(x, sound);
}

static void stop_accepted(int x, int keep_loop)
{
    Channel *c = &s.ch[x];

    c->stop_request = 0;
    if (keep_loop) {
        if (c->loop_sound) {
            c->loop_sound = 0;
            channel_read(x);
            return;
        }
        if (!c->loop) {
            channel_read(x);
            return;
        }
    } else if (c->loop_sound) {
        if (!s.battle)
            sfx_queue_push(c->loop_sound);
        else if (c->loop_side == 1)
            sfx_queue_push(c->loop_sound | 0x80);
        else if (c->loop_side == 2)
            sfx_queue_push(c->loop_sound & 0x7F);
    }
    c->state = 4;
    d.key_off |= voice_mask(x);
    c->loop = 0;
    c->loop_sound = 0;
    c->age = 0;
    c->number = 0;
    c->mode = 0;
    c->side = 0;
    c->loop_side = 0;
}

static void channel_step(int x)
{
    Channel *c = &s.ch[x];
    uint8_t reason;

    switch (c->state) {
    case 0:
        channel_begin(x, c->fresh);
        return;
    case 1:
        goto key_on;
    case 2:
        c->key_request = 0;
        /* fall through */
    case 3:
        if (c->fresh) {
            channel_begin(x, c->fresh);
            return;
        }
        c->state = 3;
        if (c->key_request == 1)
            goto key_on;
        if (c->key_request) {
            c->age = 0;
            goto key_on;
        }
        break;
    default:
        if (c->fresh) {
            channel_begin(x, c->fresh);
            return;
        }
        c->command = 0;
        c->state = 0;
        c->active = 0;
        sfx_release(x);
        return;
    }
    reason = c->stop_request;
    if (!reason) {
        channel_read(x);
        return;
    }
    if (reason == 1) {
        stop_accepted(x, 0);
        return;
    }
    if (s.battle) {
        if (((reason == 0x0A || reason == 0x1A) && c->command == 0x25) ||
            ((reason == 0x0B || reason == 0x1B) && c->command == 0x15) ||
            ((reason == 0x0C || reason == 0x1C) && c->command == 0x17)) {
            stop_accepted(x, 1);
            return;
        }
        c->stop_request = 0;
        channel_read(x);
        return;
    }
    if (reason == 7 ||
        (reason == 8 && c->command == 0x02) ||
        (reason == 0x0A && c->command == 0x25) ||
        (reason == 0x0B && c->command == 0x15) ||
        (reason == 0x0C && c->command == 0x17) ||
        (reason == 0x0E && c->command == 0x21)) {
        stop_accepted(x, 1);
        return;
    }
    c->stop_request = 0;
    channel_read(x);
    return;
key_on:
    if (c->fresh) {
        channel_begin(x, c->fresh);
        return;
    }
    c->state = 2;
    d.key_on |= voice_mask(x);
    c->key_request = 0;
    channel_read(x);
}

void channels_frame(void)
{
    int x;

    for (x = 0; x < 8; x++)
        if (s.ch[x].active)
            channel_step(x);
}

/* ---- the game's rules each tick ---- */

/*
 * @routine 19:CDB9 SpcChannelGuards
 * WHAT IT DOES: With game flag 6, stops channel 5 unless it plays sound $43.
 */
static void channel_guards(void)
{
    if ((s.game_flags & 0x40) && s.ch[5].command != 0x43)
        sfx_stop_1(5);
}

/* The tracks a command checks: channel 7's sound is one of the music's. */
static int track_sound(uint8_t a) { return a == 1 || a == 4 || a == 7 || a == 0x0A || a == 0x0D; }

/*
 * @routine 19:CE06 SpcStaggeredChecks
 * WHAT IT DOES: A counter through 0-$16 each tick, and at six steps of it one
 * check: restarting a looping sound or queueing one. Outside song 1 the loop
 * flags are cleared.
 */
static void staggered_checks(void)
{
    uint8_t a = (uint8_t)(++s.stagger_step);

    switch (a) {
    case 3:
        if ((s.game_flags & 8) && s.ch[4].command == 2) {
            stop_if(4, 0x40, 2, 8);
            return;
        }
        break;
    case 9:
        if (s.game_flags & 0x20) {
            if (track_sound(s.ch[7].command))
                sfx_stop_1(7);
            if (s.ch[6].command >= 0x72 && s.ch[6].command <= 0x76)
                sfx_stop_1(6);
        }
        return;
    case 0x0C:
        if (s.loop_flags & 1) {
            sfx_queue_push(0x17);
            return;
        }
        stop_if(5, 1, 0x17, 0x0C);
        return;
    case 0x0F:
        if (!(s.loop_flags & 4)) {
            stop_if(5, 4, 0x25, 0x0A);
            return;
        }
        if (!(s.loop_flags & 1) && !(s.loop_flags & 0x80) && !(s.loop_flags & 0x10)) {
            sfx_queue_push(0x25);
            return;
        }
        break;
    case 0x12:
        if (!(s.loop_flags & 0x10)) {
            stop_if(5, 0x10, 0x15, 0x0B);
            return;
        }
        break;
    case 0x15:
        if (s.loop_flags & 0x80) {
            sfx_queue_push(0x21);
            return;
        }
        stop_if(5, 0x80, 0x21, 0x0E);
        return;
    default:
        if (!((uint8_t)(a - 0x16) & 0x80))
            s.stagger_step = 0;
        break;
    }
    if (d.song != 1)
        s.loop_flags = 0;
}

/*
 * @routine 19:CE82 SpcGameStateTick
 * WHAT IT DOES: The game state's sound rules each tick: countdown flags, the
 * delays after song requests, the finish music, and the port commands that
 * stop channels and pause the music.
 */
static void game_state_tick(void)
{
    if (bank_byte(BANK_COUNTDOWN_B) == 0x98 && (s.game_flags & 0x10))
        s.game_flags |= 0x80;
    if (bank_byte(BANK_COUNTDOWN_A) == 0x14) {
        s.hold_time = 0x1E;
        s.game_flags |= 0x10;
    } else {
        s.hold_time = 0x04;
        s.game_flags &= (uint8_t)~0x10;
    }
    if (s.song5_count != 0xFF)
        s.song5_count++;
    if (d.song_request == 5) {
        s.song5_count = 0;
        s.song5_scale = 0;
    }
    if ((s.song5_count & 0x80) && s.song5_scale != 0xFF)
        s.song5_scale++;
    if (s.command == 0x35)
        s.song5_scale = 0xFF;
    if (d.song_request == 2)
        s.song3_delay = 1;
    if (s.song3_delay) {
        s.song3_delay++;
        if (s.song3_delay == 0xE6) {
            s.song3_delay = 0;
            d.song_request = 3;
        }
    }
    switch ((uint8_t)(d.tempo >> 8)) {
    case 0x1B: s.tempo_value = 0x73; break;
    case 0x1D: s.tempo_value = 0x6A; break;
    case 0x1E: s.tempo_value = 0x67; break;
    case 0x29: s.tempo_value = 0x92; break;
    case 0x2A: s.tempo_value = 0x93; break;
    default: s.tempo_value = 0x8E; break;
    }
    if (s.ch[7].command != 0x0D) {
        if (!s.finish_count) {
            if (s.port_in[1] == 0x1D)
                s.finish_count = 1;
        } else {
            int go_on = 1;

            if (s.finish_delay) {
                s.finish_delay--;
                if (s.finish_delay == 4) {
                    d.master = (uint16_t)(s.tempo_value_saved << 8 | (d.master & 0xFF));
                    sfx_release(0);
                    sfx_release(1);
                    go_on = 0;
                } else if (s.finish_delay == 1) {
                    d.transpose++;
                    d.pattern = BANK_FINISH_PATTERN;
                    d.track_flags |= 1;
                    go_on = 0;
                }
            }
            if (go_on && s.command == 0x30) {
                s.finish_count++;
                if (s.finish_count == 3) {
                    d.pattern = BANK_FINISH_PATTERN;
                    d.track_flags |= 1;
                    s.finish_delay = s.tempo_value;
                    sfx_take_voice(0);
                    sfx_take_voice(1);
                    s.tempo_value_saved = (uint8_t)(d.master >> 8);
                    d.master = (uint16_t)(0xBE00 | (d.master & 0xFF));
                }
            }
        }
    }
    if (s.port_in[1] == 0x1D && s.ch[7].command == 0x0D)
        s.port_in[1] = 0;
    if (d.song_request == 1 && s.finish_count && s.ch[7].command != 0x0D)
        sfx_queue_push(2);
    if (s.song5_count == 0xF0 && s.finish_count && s.ch[7].command != 0x0D && d.song == 5)
        sfx_queue_push(0x24);
    if (s.command == 0x39) {
        s.loop_flags = 0;
        s.game_flags |= 8 | 0x40;
        s.timer_4c = 1;
        stop_request_fade(7);
        stop_request_fade(6);
        /* the original calls the stop with whatever reason earlier code left; nothing it can leave is a reason that stops */
        s.loop_flags &= (uint8_t)~0x40;
    }
    if (s.command == 0x32) {
        s.game_flags |= 0x20;
        sfx_stop_1(4);
        s.loop_flags = 0;
        d.song = 0;
        driver_music_pause();
        s.game_flags |= 8 | 0x40;
        sfx_stop_1(2);
        sfx_stop_1(1);
        sfx_stop_1(0);
    }
    if (s.command == 3) {
        s.game_flags |= 0x20;
        sfx_stop_1(5);
        sfx_stop_1(4);
        s.game_flags |= 8;
        s.loop_flags = 0;
        s.game_flags |= 0x40;
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
    s.game_flags |= 8;
    s.loop_flags = 0;
    driver_music_pause();
    s.game_flags |= 0x40;
}

/*
 * @routine 19:D0C0 SpcCommandTimers
 * WHAT IT DOES: Commands that start delays and queue a sound when their
 * delay runs out.
 */
static void command_timers(void)
{
    if (d.flags & 1)
        return;
    if (s.command == 0x3E)
        s.count_3e = 1;
    if (s.count_3e)
        s.count_3e++;
    if (s.count_3e == 0x2A) {
        uint8_t t = s.ch[7].command;

        if (t == 1 || t == 7)
            sfx_queue_push(0x2F);
        else if (t == 4 || t == 0x0A)
            sfx_queue_push(0x0F);
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
    if (s.command == 0x37)
        s.count_37++;
    if (s.command == 0x30) {
        if (s.count_37 == 0x0A)
            sfx_queue_push(0x43);
        s.count_37 = 0;
    }
    if (!s.random_delay) {
        if (s.command == 0x29) {
            s.random_delay = 1;
            /* the ADC's carry is the compare's above: set when the command equals $29 */
            s.random_target = (uint8_t)((d.random & 0x3F) + 0x1E + 1);
        }
    } else {
        uint8_t was = s.random_delay;

        s.random_delay++;
        if (was == s.random_target) {
            sfx_queue_push(0x29);
            s.command = 0x29;
            s.random_delay = 0;
            s.random_target = 0;
        }
    }
}

void tempo_bend(void)
{
    /* the bend's start and end by command */
    if (s.command == 0x2C) {
        if (d.song == 1) {
            d.track_flags |= 4;
            d.track_flags &= (uint8_t)~8;
        }
    } else if (s.bend_count && (s.command == 0x30 || s.command == 0x3C)) {
        d.track_flags &= (uint8_t)~4;
        d.track_flags |= 8;
    }
    if (d.track_flags & 4) {
        if (s.bend_count == 0xFE)
            return;
        if (!s.bend_count) {
            s.bend_acc = (uint16_t)(0x04 << 8 | (s.bend_acc & 0xFF));
            s.bend_transpose = (uint8_t)(d.tempo >> 8);
            s.bend_tempo = d.transpose;
            s.bend_tempo_step = (uint8_t)(((uint16_t)d.transpose * 0x7F) / 0xFE);
        }
        s.bend_count++;
        d.transpose = (uint8_t)(s.bend_tempo - (uint8_t)(s.bend_count / 0x15));
        s.bend_acc = (uint16_t)(s.bend_acc + s.bend_tempo_step);
        d.tempo = (uint16_t)((uint8_t)(s.bend_transpose - (s.bend_acc >> 8)) << 8 | (d.tempo & 0xFF));
    } else if (d.track_flags & 8) {
        d.track_flags &= (uint8_t)~0xC;
        s.bend_count = 0;
        s.bend_acc = 0;
        s.bend_tempo_step = 0;
        d.transpose = s.bend_tempo;
        d.tempo = (uint16_t)(s.bend_transpose << 8 | (d.tempo & 0xFF));
    }
}

/* The echo transition between its modes. */
static void echo_step_change(void)
{
    if (s.echo_state == 1) {
        s.echo_step++;
        if (s.echo_step == 0x1E) {
            if (!(s.effect_flags & 1))
                s.echo_state = 2;
        } else if (s.echo_step == 0x3C) {
            s.echo_state = 2;
        }
    } else if (s.echo_state == 2) {
        if (s.effect_flags & 1) {
            if (s.echo_step != 0x3C)
                s.echo_state = 1;
        } else if (s.effect_flags & 2) {
            if (s.echo_step != 0x1E)
                s.echo_state = 3;
        } else {
            s.echo_state = 3;
        }
    } else {
        s.echo_step--;
        if (s.echo_step == 0x3C && (s.effect_flags & 2))
            s.echo_state = 2;
    }
}

static void echo_transition(void)
{
    if (d.flags & 1)
        return;
    if (!s.echo_step) {
        if (s.echo_state == 1) {
            s.echo_saved[0] = d.echo_voices;
            s.echo_saved[1] = d.echo_feedback;
            s.echo_saved[2] = (uint8_t)(d.echo_left >> 8);
            s.echo_saved[3] = (uint8_t)(d.echo_right >> 8);
            s.echo_step = 1;
        } else if (s.echo_state == 3) {
            d.echo_voices = s.echo_saved[0];
            d.echo_feedback = s.echo_saved[1];
            d.echo_left = (uint16_t)(s.echo_saved[2] << 8 | (d.echo_left & 0xFF));
            d.echo_right = (uint16_t)(s.echo_saved[3] << 8 | (d.echo_right & 0xFF));
            s.echo_state = 0;
        } else if (s.effect_flags & 3) {
            s.echo_state = 1;
        }
        return;
    }
    d.echo_voices = 0xC0;
    echo_step_change();
    s.engine_echo_scale = (uint8_t)~(uint8_t)(((uint16_t)s.echo_step * 0xB0) >> 8);
    d.echo_feedback = (uint8_t)(((uint16_t)(uint8_t)(0x64 - s.echo_saved[1]) * s.echo_step) / 0x3C + s.echo_saved[1]);
    {
        uint8_t v = (uint8_t)(((uint16_t)(uint8_t)(0x64 - s.echo_saved[2]) * s.echo_step) / 0x3C + s.echo_saved[2]);

        d.echo_left = (uint16_t)(v << 8 | (d.echo_left & 0xFF));
        d.echo_right = (uint16_t)(v << 8 | (d.echo_right & 0xFF));
    }
}

static void pitch_ramps(void)
{
    if ((s.effect_flags & 4) && s.ramp_b_steps != 0x14) {
        s.ramp_b_steps++;
        s.ramp_b_pitch = (uint16_t)(s.ramp_b_pitch + 0x20);
    }
    if (s.effect_flags & 8) {
        if (s.ramp_b_steps) {
            s.ramp_b_steps--;
            s.ramp_b_pitch = (uint16_t)(s.ramp_b_pitch - 0x20);
        } else {
            s.effect_flags &= (uint8_t)~0xC;
            s.ramp_b_pitch = 0;
        }
    }
    if ((d.flags & 4) && s.ramp_a_steps != 0x19) {
        s.ramp_a_steps++;
        s.ramp_a_pitch = (uint16_t)(s.ramp_a_pitch + 0x3C);
    }
    if (d.flags & 8) {
        if (s.ramp_a_steps) {
            s.ramp_a_steps--;
            s.ramp_a_pitch = (uint16_t)(s.ramp_a_pitch - 0x3C);
        } else {
            d.flags &= (uint8_t)~0xC;
            s.ramp_a_pitch = 0;
        }
    }
}

/*
 * @routine 19:D42F SpcPortCommand
 * WHAT IT DOES: The port 0 command: 6 and up is a sound command, 1-5 a song
 * request, which also clears the fade state.
 */
void port_command(void)
{
    uint8_t a = s.port_in[0] & 0x7F;

    s.command = 0;
    if (!a)
        return;
    if (a >= 6) {
        s.command = (uint8_t)(a - 5);
        return;
    }
    if (d.flags & 1)
        return;
    d.song_request = s.port_in[0];
    s.fade_amount = 0;
    s.bend_acc = 0;
    s.bend_count = 0;
    d.flags &= (uint8_t)~0x43;
    d.track_flags &= (uint8_t)~0x0C;
}

/* After a pause, a channel's saved sound restarts. */
static void channel_resume(int x)
{
    Channel *c = &s.ch[x];

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

    if (c->loop_sound) {
        c->paused = c->loop_sound;
        c->loop_sound = 0;
        c->loop = 0;
    } else if (c->loop == 2) {
        c->paused = c->loop_sound;
        c->loop_sound = 0;
        c->loop = 0;
    } else if (c->loop) {
        c->paused_number = c->number;
        c->paused = c->command;
        c->loop = 0;
    }
}

/* A looping sound's request, unless the game has stopped them. */
static void loop_start(uint8_t loop_bit, uint8_t sound)
{
    if (!(s.game_flags & 0x40)) {
        s.loop_flags |= loop_bit;
        sfx_queue_push(sound);
    }
}

/*
 * @routine 19:D469 SpcPort1Command
 * WHAT IT DOES: A new port 1 command runs its handler: looping sounds, the
 * ramps, the echo transition, pause, resume and music fades.
 */
static void port1_command(void)
{
    uint8_t a = s.port_in[1] == s.port1_last ? 0 : s.port_in[1];
    int x;

    s.port1_last = s.port_in[1];
    if (a >= 0x3E)
        a = 0;
    switch (a) {
    case 5:                     /* the driver reloads, at once or after its fade */
        stop_all();
        if (d.master >> 8)
            d.track_flags |= 0x80;
        else
            driver_leave();
        break;
    case 6:                     /* the driver leaves for the boot loader */
        s.game_flags |= 1;
        stop_all();
        if (d.master >> 8)
            s.game_flags |= 1;
        else
            driver_leave();
        break;
    case 0x08: stop_if(4, 0x40, 0x02, a); break;
    case 0x09: loop_start(0x04, 0x25); break;
    case 0x0A: stop_if(5, 0x04, 0x25, a); break;
    case 0x0B: stop_if(5, 0x10, 0x15, a); break;
    case 0x0C: stop_if(5, 0x01, 0x17, a); break;
    case 0x0D: loop_start(0x10, 0x15); break;
    case 0x0E: stop_if(5, 0x80, 0x21, a); break;
    case 0x0F: s.effect_flags = (uint8_t)((s.effect_flags & ~2) | 1); break;
    case 0x10: s.effect_flags &= (uint8_t)~3; break;
    case 0x11: s.effect_flags = (uint8_t)((s.effect_flags & ~1) | 2); break;
    case 0x12: loop_start(0x01, 0x17); break;
    case 0x13: loop_start(0x80, 0x21); break;
    case 0x15:                  /* the pause */
        if (d.flags & 1)
            break;
        s.loop_flags_saved = s.loop_flags;
        s.loop_flags = 0;
        if (s.echo_state) {
            d.flags |= 0x80;
            s.game_flags |= 0x40;
            s.echo_mode_saved[0] = d.echo_voices;
            s.echo_mode_saved[1] = d.echo_feedback;
            s.echo_mode_saved[2] = (uint8_t)(d.echo_left >> 8);
            s.echo_mode_saved[3] = (uint8_t)(d.echo_right >> 8);
            d.echo_feedback = 0x50;
            d.echo_left = (uint16_t)(0x11 << 8 | (d.echo_left & 0xFF));
            d.echo_right = (uint16_t)(0x11 << 8 | (d.echo_right & 0xFF));
        }
        d.flags |= 1;
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
        if (d.flags & 0x80) {
            d.flags &= (uint8_t)~0x80;
            d.echo_voices = s.echo_mode_saved[0];
            d.echo_feedback = s.echo_mode_saved[1];
            d.echo_left = (uint16_t)(s.echo_mode_saved[2] << 8 | (d.echo_left & 0xFF));
            d.echo_right = (uint16_t)(s.echo_mode_saved[3] << 8 | (d.echo_right & 0xFF));
        }
        sfx_queue_push(0x4B);
        d.flags &= (uint8_t)~1;
        s.game_flags &= (uint8_t)~0x40;
        for (x = 7; x >= 4; x--)
            channel_resume(x);
        sfx_stop_1(1);
        sfx_stop_1(0);
        break;
    case 0x1D:
        if (!(s.game_flags & 8)) {
            s.loop_flags |= 0x40;
            sfx_queue_push(0x02);
        }
        break;
    case 0x22:                  /* the music stops, from a pause */
        if (d.flags & 1) {
            sfx_queue_push(0x4C);
            d.flags &= (uint8_t)~1;
            for (x = 7; x >= 4; x--) {
                s.ch[x].paused = 0;
                s.ch[x].loop_sound = 0;
            }
            driver_music_pause();
            d.flags &= (uint8_t)~0xC;
            s.ramp_a_pitch = 0;
        }
        break;
    case 0x23:                  /* ramp A starts */
        if (!(d.flags & 4)) {
            d.flags = (uint8_t)((d.flags & ~8) | 4);
            s.ramp_a_pitch = 0x0200;
        }
        break;
    case 0x24:                  /* its end */
        if (d.flags & 4) {
            d.flags = (uint8_t)((d.flags & ~4) | 8);
            s.ramp_a_pitch = (uint16_t)(s.ramp_a_pitch - 0x0200);
        }
        break;
    case 0x3A:                  /* ramp B starts */
        if (!(s.effect_flags & 4)) {
            s.effect_flags = (uint8_t)((s.effect_flags & ~8) | 4);
            s.ramp_b_pitch = 0x0040;
        }
        break;
    case 0x3B:                  /* its end */
        if (s.effect_flags & 4) {
            s.effect_flags = (uint8_t)((s.effect_flags & ~4) | 8);
            s.ramp_b_pitch = (uint16_t)(s.ramp_b_pitch - 0x0040);
        }
        break;
    default:
        if (a >= 0x2A && a <= 0x31)
            s.pan_scale = (uint8_t)(a - 0x2A);
        else if (a >= 0x32 && a <= 0x35)
            s.volume_add = (uint8_t)(a - 0x32);
        break;
    }
}

/*
 * @routine 19:D7B6 SpcCommandChannelCheck
 * WHAT IT DOES: A command not in the exempt list is checked against the
 * priority list: when an earlier-listed sound already plays on its channel,
 * the command is dropped.
 */
static void channel_check(void)
{
    uint8_t sound = s.command;
    int y;

    if (!sound) {
        s.command_kept = 0;
        return;
    }
    for (y = 0; channel_exempt[y]; y++)
        if (channel_exempt[y] == sound)
            return;
    for (y = 0; channel_order[y]; y++) {
        uint8_t header = bank_byte(sfx_pointer(sound));
        int x = header & 7, k;

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

static void command_blocked_check(void)
{
    int i;

    for (i = 0; command_blocked[i]; i++)
        if (command_blocked[i] == s.command) {
            s.command = 0;
            return;
        }
}

static void command_filter_check(void)
{
    int i;

    if (d.song == 1)
        return;
    for (i = 0; command_filter[i]; i++)
        if (command_filter[i] == s.command)
            return;
    s.command = 0;
}

/*
 * @routine 19:D81C SpcSfxHolds
 * WHAT IT DOES: Each channel's hold counter counts up; a new command whose
 * channel is held is dropped, and sound $1E starts a hold on its channel,
 * which ends when its count reaches the hold's time.
 */
static void sfx_holds(void)
{
    int x;

    for (x = 0; x < 8; x++)
        if (s.ch[x].hold_count)
            s.ch[x].hold_count++;
    if (s.command) {
        uint8_t sound = s.command;

        x = bank_byte(sfx_pointer(sound)) & 7;
        if (s.ch[x].hold)
            s.command = 0;
        if (sound == 0x1E) {
            s.ch[x].hold = sound;
            s.ch[x].hold_count = 1;
        }
    } else {
        for (x = 0; x < 8; x++)
            if (s.ch[x].hold == 0x1E && s.ch[x].hold_count == s.hold_time) {
                s.ch[x].hold_count = 0;
                s.ch[x].hold = 0;
            }
    }
}

/*
 * @routine 19:D92F SpcSfxMode
 * WHAT IT DOES: The header's mode bits for a channel: bit 7 marks a looping
 * sound (1, or 2 with bit 5, which drops a queued one); otherwise a looping
 * channel keeps its sound to restart.
 */
void sfx_mode_apply(int x, uint8_t header)
{
    Channel *c = &s.ch[x];

    if (header & 0x80) {
        if (header & 0x20) {
            if (!s.battle)
                c->loop_sound = 0;
            c->loop = 2;
        } else {
            c->loop = 1;
        }
    } else if (c->loop == 1 && !c->loop_sound) {
        c->loop_sound = c->command;
        c->loop_side = c->side;
    }
}

/* The queue and the channels at the end of the sound tick. */
static void sound_tick_end(void)
{
    uint8_t sound;
    int i;

    if (s.command)
        sfx_queue_push(s.command);
    s.command = 0;
    if (s.hold_index != 0xFF)
        s.hold_index++;
    if (s.hold_index > 2 && s.queue[0]) {
        sound = s.queue[0];
        for (i = 1; i < 8; i++)
            s.queue[i - 1] = s.queue[i];
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
        c->active = 1;
        c->ptr = data;
        c->fresh = sound;
        channels_frame();
        c->fresh = 0;
    }
}

/*
 * @routine 19:F20E SpcNoiseSample
 * WHAT IT DOES: While channels 1-7 play sample $23 the noise generator steps
 * 28 times a tick. (The original also writes those bytes into the sample in
 * sound memory while it plays. Samples are made on the game thread and do
 * not change, so the sample is a long noise made once at the same rates; the
 * generator's steps are kept for the random delays that read it.)
 */
static void noise_tick(void)
{
    int x, i;

    for (x = 7; x >= 1; x--)
        if (s.ch[x].number == 0x23)
            break;
    if (x < 1)
        return;
    for (i = 0; i < 28; i++) {
        uint8_t hi = (uint8_t)(d.random >> 8), lo = (uint8_t)d.random;
        uint8_t in = (uint8_t)(((hi >> 6) ^ (hi >> 5)) & 1);

        d.random = (uint16_t)((uint16_t)(hi << 1 | lo >> 7) << 8 | (uint8_t)(lo << 1 | in));
    }
}

/*
 * @routine 19:D73A SpcMusicFade
 * WHAT IT DOES: The music fade: while it runs the volume scale falls; at the
 * bottom the fade ends and, by flags, the driver leaves or the music pauses.
 * Without a fade the scale follows the master volume.
 */
void music_fade(void)
{
    if (!(d.flags & 0x40)) {
        s.fade_start = (uint8_t)(d.master >> 8);
        d.volume_scale = s.fade_start;
        return;
    }
    s.fade_amount = (uint16_t)(s.fade_amount + 0x02F0);
    {
        uint8_t scale = (uint8_t)(((uint16_t)(s.fade_start << 8) - s.fade_amount) >> 8);

        d.volume_scale = scale;
        if (scale > 3)
            return;
    }
    s.fade_amount = 0;
    d.flags &= (uint8_t)~0x40;
    if ((d.track_flags & 0x80) || (s.game_flags & 1))
        driver_leave();
    else
        driver_music_pause();
}

/*
 * @routine 19:CD3A SpcSfxFrame
 * WHAT IT DOES: The game's sound tick: the ports read, then the tick's game
 * sound work in order and the sound effect channels.
 */
void race_sound_tick(void)
{
    if (s.port_latch[0] != s.port_seen) {
        s.port_seen = s.port_latch[0];
        s.port_in[0] = s.port_latch[0];
    } else {
        s.port_in[0] = 0;
    }
    s.port_in[1] = s.port_latch[1];
    s.port_in[2] = s.port_latch[2];
    s.port_in[3] = s.port_latch[3];
    if (d.flags & 2)
        d.flags &= (uint8_t)~3;
    if ((s.game_flags & 0x80) && s.port_in[1] == 9)
        s.port_in[1] = 0x12;
    if ((s.game_flags & 0x80) && s.port_in[1] == 0x0A)
        s.port_in[1] = 0x0C;
    echo_transition();
    music_fade();
    port_command();
    channel_guards();
    command_blocked_check();
    channel_check();
    tempo_bend();
    staggered_checks();
    game_state_tick();
    sfx_holds();
    pitch_ramps();
    command_filter_check();
    command_4c();
    command_timers();
    noise_tick();
    if (d.song != 1 && !((uint8_t)(s.port_in[1] - 7) & 0x80))
        s.port_in[1] = 0;
    port1_command();
    sound_tick_end();
}
