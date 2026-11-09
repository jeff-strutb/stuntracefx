/*
 * driver_music.c: the music sequencer, Nintendo's N-SPC engine as this game
 * carries it. Tracks of notes and voice commands step on a tempo; each voice
 * has its fades, vibrato and tremolo; what comes out are voice calls.
 * Runs on the audio thread: no waits, locks, allocation or files.
 */
#include <string.h>

#include "driver.h"

Driver d;

uint8_t bank_byte(uint16_t at) { return d.bank ? d.bank->bytes[at] : 0; }
uint16_t bank_word(uint16_t at) { return (uint16_t)(bank_byte(at) | bank_byte((uint16_t)(at + 1)) << 8); }

static uint8_t held_by_sfx(void) { return d.sfx_voices & d.current_bit; }

/* The pan curve; the volume of a side at a pan is read off it. */
/* (A pan past 20 reads on into the next two bytes of the original's table,
   which are the first command parameter counts.) */
static const uint8_t pan_curve[23] = {
    0x00, 0x01, 0x03, 0x07, 0x0D, 0x15, 0x1E, 0x29, 0x34, 0x42, 0x51,
    0x5E, 0x67, 0x6E, 0x73, 0x77, 0x7A, 0x7C, 0x7D, 0x7E, 0x7F, 0x01, 0x01
};

/* How many parameter bytes each voice command $E0-$FA has. */
static const uint8_t command_params[27] = {
    1, 1, 2, 3, 0, 1, 2, 1, 2, 1, 1, 3, 0, 1, 2, 3, 1, 3, 3, 0, 1, 3, 0, 3, 3, 3, 1
};

/* A semitone's pitch at the top octave: note 0 to the octave's end. */
static const uint16_t note_pitch[13] = {
    0x085F, 0x08DE, 0x0965, 0x09F4, 0x0A8C, 0x0B2C, 0x0BD6, 0x0C8B, 0x0D4A, 0x0E14, 0x0EEA, 0x0FCD, 0x10BE
};

static uint8_t track_read(Voice *v)
{
    uint8_t b = bank_byte(v->track);
    v->track = (uint16_t)(v->track + 1);
    return b;
}

/*
 * @routine 19:CA8B SpcFadeDelta
 * WHAT IT DOES: The step of a fade: a difference of up to a byte over a
 * number of steps, as an 8.8 value of the difference's sign.
 * HOW: The magnitude divided whole, then its remainder divided for the
 * fraction, and negated back. A difference that underflowed its byte (from
 * the unsigned subtraction before it) is the negative one.
 */
static uint16_t fade_delta(uint8_t to, uint8_t from, uint8_t steps)
{
    uint8_t diff = (uint8_t)(to - from), whole, fraction, remainder;
    int negative = to < from;
    uint16_t result;

    if (negative)
        diff = (uint8_t)(-diff);
    if (!steps) {
        /* the DSP's DIV by zero: quotient $FF, remainder as the hardware leaves it */
        whole = 0xFF;
        remainder = diff;
        fraction = 0xFF;
    } else {
        whole = (uint8_t)(diff / steps);
        remainder = (uint8_t)(diff % steps);
        fraction = (uint8_t)(((uint16_t)remainder << 8) / steps);
    }
    result = (uint16_t)(whole << 8 | fraction);
    return negative ? (uint16_t)(0 - result) : result;
}

/*
 * @routine 19:CAA7 SpcVolumeCalc
 * WHAT IT DOES: The voice's output volume: the tremolo factor times the
 * global scale, the note velocity and the voice volume, squared.
 */
static void volume_calc(Voice *v, uint8_t factor)
{
    uint8_t a = (uint8_t)(((uint16_t)factor * d.volume_scale) >> 8);

    a = (uint8_t)(((uint16_t)a * v->note_velocity) >> 8);
    a = (uint8_t)(((uint16_t)a * (v->volume >> 8)) >> 8);
    a = (uint8_t)(((uint16_t)a * a) >> 8);
    v->volume_out = a;
}

/* The tremolo's factor from its phase: a triangle times the depth, inverted. */
static void tremolo_apply(Voice *v, uint8_t phase)
{
    uint8_t a = (uint8_t)(phase << 1);
    uint8_t folded = (phase & 0x80) ? (uint8_t)~a : a;

    volume_calc(v, (uint8_t)~(uint8_t)(((uint16_t)folded * v->tremolo_depth) >> 8));
}

/*
 * @routine 19:CB32 SpcVoiceVolumeLR
 * WHAT IT DOES: The voice's two volumes from its pan: the curve read at the
 * pan, and at 20 minus the pan for the other side, times the output volume,
 * each negated where the pan's flags invert that side.
 */
static int8_t side_volume(const Voice *v, uint16_t pan, int invert)
{
    uint8_t at = (uint8_t)(pan >> 8);
    uint8_t rise = (uint8_t)(pan_curve[at + 1] - pan_curve[at]);
    uint8_t curve = (uint8_t)(pan_curve[at] + (uint8_t)(((uint16_t)rise * (uint8_t)pan) >> 8));
    uint8_t out = (uint8_t)(((uint16_t)curve * v->volume_out) >> 8);

    return (int8_t)(invert ? (uint8_t)(-out) : out);
}

static void volume_send(const Voice *v, uint16_t pan)
{
    int i = d.current;
    int8_t l, r;

    if (held_by_sfx())
        return;
    l = side_volume(v, pan, v->pan_flags & 0x80);
    r = side_volume(v, (uint16_t)(0x1400 - pan), v->pan_flags & 0x40);
    if (l != d.sent_left[i] || r != d.sent_right[i]) {
        nshvc_voice_volume((uint32_t)i, l, r);
        d.sent_left[i] = l;
        d.sent_right[i] = r;
    }
}

/*
 * The note and fraction `pitch` to the DSP's pitch: the fine correction of
 * high and very low notes, the octave and interpolated semitone, then the
 * instrument's tuning.
 */
static void pitch_send(const Voice *v, uint16_t pitch)
{
    uint8_t note = (uint8_t)(pitch >> 8), octave, semitone, frac;
    uint16_t base, shifted;
    uint32_t product;

    if (note >= 0x34)
        pitch = (uint16_t)(pitch + (uint8_t)(note - 0x34));
    else if (note < 0x13)
        pitch = (uint16_t)(pitch + (uint16_t)(0xFF00u | (uint8_t)((uint8_t)(note - 0x13) << 1)));
    note = (uint8_t)(pitch >> 8);
    frac = (uint8_t)pitch;
    octave = (uint8_t)((uint8_t)(note << 1) / 0x18);
    semitone = (uint8_t)(((uint8_t)(note << 1) % 0x18) / 2);
    base = note_pitch[semitone];
    base = (uint16_t)(base + (((uint8_t)(note_pitch[semitone + 1] - base) * frac) >> 8));
    shifted = (uint16_t)(base << 1);
    while (octave != 6) {
        shifted >>= 1;
        octave++;
    }
    product = (uint32_t)v->pitch_scale * shifted;
    if (!held_by_sfx())
        nshvc_voice_pitch(d.current, (uint16_t)((product >> 8) & 0x3FFF));
}

/*
 * @routine 19:CA68 SpcSlideToNote
 * WHAT IT DOES: Starts the voice's pitch slide to a note over its steps.
 */
static void slide_to_note(Voice *v, uint8_t note)
{
    v->slide_target = note & 0x7F;
    v->slide_delta = fade_delta(v->slide_target, (uint8_t)(v->pitch >> 8), v->slide_steps);
}

/*
 * @routine 19:C84D SpcLoadInstrument
 * WHAT IT DOES: Loads an instrument into the voice: its sample and envelope
 * and its pitch scale. A sample number with bit 7 set means noise, at the
 * clock in its low five bits. Nothing for a voice a sound effect holds.
 */
void driver_set_instrument(Voice *v, uint8_t instrument)
{
    uint16_t at;
    uint8_t adsr1, adsr2, gain, number;
    uint32_t i = d.current;

    v->instrument = instrument;
    if (instrument & 0x80)
        instrument = (uint8_t)(instrument - 0xCA + d.percussion_base);
    at = (uint16_t)(BANK_INSTRUMENTS + (uint16_t)instrument * 6);
    if (held_by_sfx())
        return;
    number = bank_byte(at);
    adsr1 = bank_byte((uint16_t)(at + 1));
    adsr2 = bank_byte((uint16_t)(at + 2));
    gain = bank_byte((uint16_t)(at + 3));
    v->pitch_scale = (uint16_t)(bank_byte((uint16_t)(at + 4)) << 8 | bank_byte((uint16_t)(at + 5)));
    if (number & 0x80) {
        d.flg = (uint8_t)((d.flg & 0x20) | (number & 0x1F));
        d.noise_voices |= d.current_bit;
        v->source = 0;
    } else {
        d.noise_voices &= (uint8_t)~d.current_bit;
        v->source = number;
    }
    if (adsr1 & 0x80) {
        nshvc_voice_adsr(i, adsr1 & 15, (adsr1 >> 4) & 7, adsr2 >> 5, adsr2 & 31);
    } else if (gain & 0x80) {
        nshvc_voice_gain(i, (nshvc_gain_mode)((gain >> 5) & 7), gain & 31);
    } else {
        nshvc_voice_gain(i, NSHVC_GAIN_DIRECT, gain & 127);
    }
}

/*
 * @routine 19:C57F SpcNoteOn
 * WHAT IT DOES: Plays a note on the voice: percussion sets its instrument
 * first and plays as note $24; ties and rests do nothing; nor does a voice a
 * sound effect holds. The pitch is the note plus the transposes and tuning,
 * vibrato and tremolo restart, a pitch envelope starts if the voice has one.
 */
static void note_on(Voice *v, uint8_t note)
{
    if (note >= 0xCA) {
        driver_set_instrument(v, note);
        note = 0xA4;
    }
    if (note >= 0xC8 || held_by_sfx())
        return;
    v->pitch = (uint16_t)((uint8_t)((note & 0x7F) + d.transpose + v->voice_transpose) << 8 | v->tuning);
    v->vibrato_phase = (v->vibrato_fade_len & 1) ? 0x80 : 0;
    v->vibrato_wait = 0;
    v->vibrato_fade_step = 0;
    v->tremolo_phase = 0;
    v->tremolo_wait = 0;
    d.volume_dirty |= d.current_bit;
    d.key_on |= d.current_bit;
    v->slide_steps = v->env_steps;
    if (v->slide_steps) {
        v->slide_delay = v->env_delay;
        if (!v->env_dir)
            v->pitch = (uint16_t)((uint16_t)(uint8_t)((v->pitch >> 8) - v->env_semitones) << 8 | (v->pitch & 0xFF));
        slide_to_note(v, (uint8_t)(v->env_semitones + (v->pitch >> 8)));
    }
    pitch_send(v, v->pitch);
}

/* Echo delay: when it changes, the echo is silenced until the buffer has
   moved, so the settle count starts. */
void driver_set_echo_delay(uint8_t delay)
{
    d.echo_delay = delay;
    if (d.dsp_delay != delay) {
        uint8_t a = (uint8_t)~(d.dsp_delay & 0x0F);

        if (d.echo_delay_wait & 0x80)
            a = (uint8_t)(a + d.echo_delay_wait);
        d.echo_delay_wait = a;
        d.sent_echo_left = d.sent_echo_right = d.sent_feedback = 0;
        d.sent_echo_voices = 0;
        d.dsp_delay = delay;
        nshvc_echo(delay, 0, 0, 0, d.fir);
        nshvc_echo_writes(0);
        d.sent_flg |= 0x20;
    }
}

static void echo_setup_fir(uint8_t set)
{
    uint16_t at = (uint16_t)(BANK_FIR_SETS + (uint16_t)set * 8);
    int i;

    for (i = 0; i < 8; i++)
        d.fir[i] = (int8_t)bank_byte((uint16_t)(at + i));
    d.fir_dirty = 1;
}

/*
 * @routine 19:C82E SpcVcmdDispatch
 * WHAT IT DOES: Voice command $E0-$FA with its first parameter already read.
 */
static void voice_command(Voice *v, uint8_t command, uint8_t a)
{
    switch (command) {
    case 0xE0:
        driver_set_instrument(v, a);
        break;
    case 0xE1:
        v->pan_flags = a;
        v->pan = (uint16_t)((a & 0x1F) << 8);
        break;
    case 0xE2:
        v->pan_fade = a;
        v->pan_target = track_read(v);
        v->pan_delta = fade_delta(v->pan_target, (uint8_t)(v->pan >> 8), a);
        break;
    case 0xE3:
        v->vibrato_delay = a;
        v->vibrato_rate = track_read(v);
        a = track_read(v);
        /* fall through */
    case 0xE4:
        v->vibrato_depth = a;
        v->vibrato_target = a;
        v->vibrato_fade_len = 0;
        break;
    case 0xE5:
        d.master = (uint16_t)(a << 8);
        break;
    case 0xE6:
        d.master_fade = a;
        d.master_target = track_read(v);
        d.master_delta = fade_delta(d.master_target, (uint8_t)(d.master >> 8), a);
        break;
    case 0xE7:
        d.tempo = (uint16_t)(a << 8);
        break;
    case 0xE8:
        d.tempo_fade = a;
        d.tempo_target = track_read(v);
        d.tempo_delta = fade_delta(d.tempo_target, (uint8_t)(d.tempo >> 8), a);
        break;
    case 0xE9:
        d.transpose = a;
        break;
    case 0xEA:
        v->voice_transpose = a;
        break;
    case 0xEB:
        v->tremolo_delay = a;
        v->tremolo_rate = track_read(v);
        a = track_read(v);
        /* fall through */
    case 0xEC:
        v->tremolo_depth = a;
        break;
    case 0xED:
        v->volume = (uint16_t)(a << 8);
        break;
    case 0xEE:
        v->volume_fade = a;
        v->volume_target = track_read(v);
        v->volume_delta = fade_delta(v->volume_target, (uint8_t)(v->volume >> 8), a);
        break;
    case 0xEF: {
        uint8_t lo = a, hi = track_read(v);

        v->sub_start = (uint16_t)(hi << 8 | lo);
        v->sub_count = track_read(v);
        v->sub_return = v->track;
        v->track = v->sub_start;
        break;
    }
    case 0xF0:
        v->vibrato_fade_len = a;
        v->vibrato_fade_delta = a ? (uint8_t)(v->vibrato_depth / a) : 0xFF;
        break;
    case 0xF1:
    case 0xF2:
        v->env_dir = command == 0xF1;
        v->env_delay = a;
        v->env_steps = track_read(v);
        v->env_semitones = track_read(v);
        break;
    case 0xF3:
        v->env_steps = a;
        break;
    case 0xF4:
        v->tuning = a;
        break;
    case 0xF5:
        d.echo_enable = a;
        d.echo_voices = (uint8_t)((a & d.sfx_voices) ^ a);
        d.echo_left = (uint16_t)(track_read(v) << 8);
        d.echo_right = (uint16_t)(track_read(v) << 8);
        d.flg &= (uint8_t)~0x20;
        break;
    case 0xF6:
        d.echo_left = (uint16_t)(a << 8 | a);
        d.echo_right = (uint16_t)(a << 8 | a);
        d.flg |= 0x20;
        break;
    case 0xF7:
        driver_set_echo_delay(a);
        d.echo_feedback = track_read(v);
        echo_setup_fir(track_read(v));
        break;
    case 0xF8:
        d.echo_fade = a;
        d.echo_target_left = track_read(v);
        d.echo_delta_left = fade_delta(d.echo_target_left, (uint8_t)(d.echo_left >> 8), a);
        d.echo_target_right = track_read(v);
        d.echo_delta_right = fade_delta(d.echo_target_right, (uint8_t)(d.echo_right >> 8), a);
        break;
    case 0xF9:
        v->slide_delay = a;
        v->slide_steps = track_read(v);
        a = track_read(v);
        slide_to_note(v, (uint8_t)(a + d.transpose + v->voice_transpose));
        break;
    case 0xFA:
        d.percussion_base = a;
        break;
    default:
        break;
    }
}

/*
 * @routine 19:C6EC SpcMusicPause
 * WHAT IT DOES: Stops the music once: clears the song and its request and
 * keys off every voice no sound effect holds.
 */
void driver_keys_release(void)
{
    d.key_off |= (uint8_t)~d.sfx_voices;
}

void driver_music_pause(void)
{
    if (d.flags & 2)
        return;
    d.flags |= 2;
    d.song_request = 0;
    d.song = 0;
    driver_keys_release();
}

static void tracks_reset(void)
{
    int i;

    for (i = DRIVER_VOICES - 1; i >= 0; i--) {
        Voice *v = &d.voice[i];

        d.current = (uint8_t)i;
        d.current_bit = (uint8_t)(1u << i);
        v->volume = 0xFF00u | (v->volume & 0xFF);
        v->pan_flags = 0x0A;
        v->pan = 0x0A00;
        v->instrument = 0;
        v->tuning = 0;
        v->voice_transpose = 0;
        v->env_steps = 0;
        v->vibrato_depth = 0;
        v->tremolo_depth = 0;
    }
    d.current_bit = 0;
    d.master_fade = 0;
    d.echo_fade = 0;
    d.tempo_fade = 0;
    d.transpose = 0;
    d.pattern_loops = 0;
    d.percussion_base = 0;
    d.master = (uint16_t)(0xC0 << 8 | (d.master & 0xFF));
    d.tempo = (uint16_t)(0x20 << 8 | (d.tempo & 0xFF));
}

static void song_start(void)
{
    d.song = d.song_request;
    d.song_request = 0;
    d.pattern = bank_word((uint16_t)(BANK_SONGS + (uint16_t)d.song * 2));
    d.pattern_delay = 2;
    driver_keys_release();
}

static uint16_t pattern_read(void)
{
    uint16_t w = bank_word(d.pattern);

    d.pattern = (uint16_t)(d.pattern + 2);
    return w;
}

/*
 * The song's next pattern: zero ends the song, a word with a zero high byte
 * is a loop count and target. Returns 0 when the song ended.
 */
static int pattern_next(void)
{
    for (;;) {
        uint16_t w = pattern_read();
        int i;

        if (!(w >> 8)) {
            uint8_t low = (uint8_t)w;

            if (!low) {
                driver_music_pause();
                return 0;
            }
            d.pattern_loops = (uint8_t)(d.pattern_loops - 1);
            if (d.pattern_loops & 0x80)
                d.pattern_loops = low;
            w = pattern_read();
            if (d.pattern_loops)
                d.pattern = w;
            continue;
        }
        for (i = 0; i < DRIVER_VOICES; i++)
            d.voice[i].track = bank_word((uint16_t)(w + i * 2));
        for (i = 0; i < DRIVER_VOICES; i++) {
            Voice *v = &d.voice[i];

            d.current = (uint8_t)i;
            d.current_bit = (uint8_t)(1u << i);
            if ((v->track >> 8) && !v->instrument)
                driver_set_instrument(v, 0);
            v->sub_count = 0;
            v->volume_fade = 0;
            v->pan_fade = 0;
            v->note_timer = 1;
        }
        d.current_bit = 0;
        return 1;
    }
}

/* Whether the note after this one is a tie: read ahead through commands
   and subroutine ends. */
static int next_is_tie(const Voice *v)
{
    uint8_t subs = v->sub_count;
    uint16_t at = v->track;
    uint8_t y = 0;

    for (;;) {
        uint8_t a = bank_byte((uint16_t)(at + y));

        if (a == 0) {
            if (!subs)
                return 0;
            subs--;
            at = subs ? v->sub_start : v->sub_return;
            y = 0;
            continue;
        }
        if (!(a & 0x80)) {
            do {
                y++;
                if (y & 0x80)
                    return 0;
                a = bank_byte((uint16_t)(at + y));
            } while (!(a & 0x80));
        }
        if (a == 0xC8)
            return 1;
        if (a == 0xEF) {
            uint8_t lo = bank_byte((uint16_t)(at + (uint8_t)(y + 1)));
            uint8_t hi = bank_byte((uint16_t)(at + (uint8_t)(y + 2)));

            at = (uint16_t)(hi << 8 | lo);
            y = 0;
            continue;
        }
        if (a < 0xE0)
            return 0;
        y = (uint8_t)(y + command_params[a - 0xE0] + 1);
    }
}

/* Adds an 8.8 delta to the value being worked, by the delta's sign kept in
   `negative` (negated when bit 7 is set). */
static void value_add(uint16_t magnitude)
{
    d.value = (uint16_t)(d.value + ((d.negative & 0x80) ? (uint16_t)(0 - magnitude) : magnitude));
}

/* The vibrato's offset to the working pitch, at phase A. */
static void vibrato_offset(const Voice *v, uint8_t phase)
{
    uint8_t a = (uint8_t)(phase << 1), y;
    uint16_t delta;

    d.negative = phase;
    a = (uint8_t)(a << 1);
    y = (phase & 0x40) ? (uint8_t)~a : a;
    if (v->vibrato_depth >= 0xF1)
        delta = (uint16_t)((v->vibrato_depth & 0x0F) * y);
    else
        delta = (uint8_t)(((uint16_t)v->vibrato_depth * y) >> 8);
    value_add(delta);
}

/*
 * @routine 19:CB70 SpcVoiceTick
 * WHAT IT DOES: The voice while a note holds: at the gate time, key-off
 * unless the next note is a tie; the pitch slide steps; vibrato moves the
 * pitch; the pitch is written when it changed.
 */
static void voice_tick(Voice *v)
{
    int moved = 0;

    if (v->note_keyoff) {
        v->note_keyoff--;
        if ((v->note_keyoff == 0 || v->note_timer == 2) && !next_is_tie(v)) {
            if (!held_by_sfx())
                nshvc_voice_key_off(d.current);
        }
    }
    if (v->slide_steps) {
        if (v->slide_delay) {
            v->slide_delay--;
        } else {
            moved = 1;
            v->slide_steps--;
            if (v->slide_steps)
                v->pitch = (uint16_t)(v->pitch + v->slide_delta);
            else
                v->pitch = (uint16_t)(v->slide_target << 8 | v->tuning);
        }
    }
    d.value = v->pitch;
    if (v->vibrato_depth) {
        if (v->vibrato_delay == v->vibrato_wait) {
            if (v->vibrato_fade_step == v->vibrato_fade_len) {
                v->vibrato_depth = v->vibrato_target;
            } else {
                v->vibrato_depth = (uint8_t)((v->vibrato_fade_step ? v->vibrato_depth : 0) + v->vibrato_fade_delta);
                v->vibrato_fade_step++;
            }
            v->vibrato_phase = (uint8_t)(v->vibrato_phase + v->vibrato_rate);
            vibrato_offset(v, v->vibrato_phase);
            pitch_send(v, d.value);
            return;
        }
        v->vibrato_wait++;
    }
    if (moved)
        pitch_send(v, d.value);
}

/*
 * @routine 19:CAA7 SpcVoiceEffects
 * WHAT IT DOES: The voice's per-tick modulation: the volume fade steps,
 * tremolo moves the volume, the pan fade steps; then volume and pan go to
 * the DSP when either changed.
 */
static void voice_effects(Voice *v)
{
    if (v->volume_fade) {
        d.volume_dirty |= d.current_bit;
        v->volume_fade--;
        if (v->volume_fade)
            v->volume = (uint16_t)(v->volume + v->volume_delta);
        else
            v->volume = (uint16_t)(v->volume_target << 8);
    }
    if (v->tremolo_depth && v->tremolo_delay == v->tremolo_wait) {
        d.volume_dirty |= d.current_bit;
        if ((v->tremolo_phase & 0x80) && v->tremolo_depth == 0xFF)
            v->tremolo_phase = 0x80;
        else
            v->tremolo_phase = (uint8_t)(v->tremolo_phase + v->tremolo_rate);
        tremolo_apply(v, v->tremolo_phase);
    } else {
        if (v->tremolo_depth)
            v->tremolo_wait++;
        volume_calc(v, 0xFF);
    }
    if (v->pan_fade) {
        d.volume_dirty |= d.current_bit;
        v->pan_fade--;
        if (v->pan_fade)
            v->pan = (uint16_t)(v->pan + v->pan_delta);
        else
            v->pan = (uint16_t)(v->pan_target << 8);
    }
    if (d.current_bit & d.volume_dirty)
        volume_send(v, v->pan);
}

/* The next track byte a slide command, when no slide runs. */
static void slide_check(Voice *v)
{
    if (!v->slide_steps && bank_byte(v->track) == 0xF9) {
        uint8_t a;

        v->track = (uint16_t)(v->track + 1);
        a = track_read(v);
        voice_command(v, 0xF9, a);
    }
}

/* One voice at its music tick. Returns 0 when the pattern changed under it
   and the tick must start again. */
static int voice_step(Voice *v)
{
    uint8_t a;

    v->note_timer--;
    if (v->note_timer) {
        voice_tick(v);
        slide_check(v);
        return 1;
    }
    for (;;) {
        a = track_read(v);
        if (!a) {
            if (!v->sub_count)
                return 0;
            /* the subroutine repeats, or returns */
            v->track = v->sub_start;
            v->sub_count--;
            if (!v->sub_count)
                v->track = v->sub_return;
            continue;
        }
        if (d.track_flags & 1) {
            d.track_flags &= (uint8_t)~1;
            return 0;
        }
        if (!(a & 0x80)) {
            v->note_length = a;
            a = track_read(v);
            if (!(a & 0x80)) {
                v->note_gate = bank_byte((uint16_t)(BANK_GATES + ((a >> 4) & 7)));
                v->note_velocity = bank_byte((uint16_t)(BANK_VELOCITIES + (a & 15)));
                a = track_read(v);
            }
        }
        if (a >= 0xE0) {
            uint8_t param = 0;

            if (command_params[a - 0xE0])
                param = track_read(v);
            voice_command(v, a, param);
            continue;
        }
        break;
    }
    note_on(v, a);
    v->note_timer = v->note_length;
    {
        uint8_t k = (uint8_t)(((uint16_t)v->note_length * v->note_gate) >> 8);

        v->note_keyoff = k ? k : 1;
    }
    slide_check(v);
    return 1;
}

/*
 * @routine 19:C6FB SpcMusicFrame
 * WHAT IT DOES: One tick of the music: a new song request starts its song;
 * the pattern list advances when the tracks end; each track's commands are
 * read; then the tempo, echo and master fades step and every voice's fades
 * and modulation are applied.
 */
void driver_music_tick(void)
{
    int i;

    if (d.flags & 1) {
        driver_keys_release();
        return;
    }
    if (d.song_request && d.song_request != d.song_last) {
        d.song_last = d.song_request;
        if (d.song_request) {
            song_start();
            return;
        }
    }
    d.song_last = d.song_request;
    d.song_request = 0;
    if (!d.song)
        return;
    if (d.pattern_delay) {
        d.pattern_delay--;
        if (d.pattern_delay) {
            tracks_reset();
            return;
        }
        if (!pattern_next())
            return;
    }
restart:
    d.volume_dirty = 0;
    for (i = 0; i < DRIVER_VOICES; i++) {
        Voice *v = &d.voice[i];

        d.current = (uint8_t)i;
        d.current_bit = (uint8_t)(1u << i);
        if (!(v->track >> 8))
            continue;
        if (!voice_step(v)) {
            if (!pattern_next())
                return;
            goto restart;
        }
    }
    if (d.tempo_fade) {
        d.tempo_fade--;
        d.tempo = d.tempo_fade ? (uint16_t)(d.tempo + d.tempo_delta) : (uint16_t)(d.tempo_target << 8);
    }
    if (d.echo_fade) {
        d.echo_fade--;
        if (d.echo_fade) {
            d.echo_left = (uint16_t)(d.echo_left + d.echo_delta_left);
            d.echo_right = (uint16_t)(d.echo_right + d.echo_delta_right);
        } else {
            d.echo_left = (uint16_t)(d.echo_target_left << 8);
            d.echo_right = (uint16_t)(d.echo_target_right << 8);
        }
    }
    if (d.master_fade) {
        d.master_fade--;
        d.master = d.master_fade ? (uint16_t)(d.master + d.master_delta) : (uint16_t)(d.master_target << 8);
        d.volume_dirty = 0xFF;
    }
    for (i = 0; i < DRIVER_VOICES; i++) {
        Voice *v = &d.voice[i];

        d.current = (uint8_t)i;
        d.current_bit = (uint8_t)(1u << i);
        if (v->track >> 8)
            voice_effects(v);
    }
}

/* A delta over the part of a tick elapsed, onto the working value. */
static void value_add_scaled(uint16_t delta)
{
    uint16_t magnitude, scaled;

    d.moved |= 0x80;
    d.negative = (uint8_t)(delta >> 8);
    magnitude = (d.negative & 0x80) ? (uint16_t)(0 - delta) : delta;
    scaled = (uint16_t)((((uint16_t)(uint8_t)magnitude * d.music_acc) >> 8) + (uint16_t)(magnitude >> 8) * d.music_acc);
    value_add(scaled);
}

/*
 * @routine 19:CC62 SpcVoiceBetweenTicks
 * WHAT IT DOES: The voice's smoothing between music ticks: tremolo, the pan
 * fade, the pitch slide and vibrato moved by the part of a tick elapsed, and
 * the volume and pitch written when they moved.
 */
static void voice_between_ticks(Voice *v)
{
    d.moved = 0;
    if (v->tremolo_depth && v->tremolo_delay == v->tremolo_wait) {
        d.moved |= 0x80;
        tremolo_apply(v, (uint8_t)(v->tremolo_phase + (uint8_t)(((uint16_t)v->tremolo_rate * d.music_acc) >> 8)));
    }
    d.value = v->pan;
    if (v->pan_fade)
        value_add_scaled(v->pan_delta);
    if (d.moved & 0x80)
        volume_send(v, d.value);
    d.moved = 0;
    d.value = v->pitch;
    if (v->slide_steps && !v->slide_delay)
        value_add_scaled(v->slide_delta);
    if (v->vibrato_depth && v->vibrato_delay == v->vibrato_wait) {
        vibrato_offset(v, (uint8_t)(v->vibrato_phase + (uint8_t)(((uint16_t)v->vibrato_rate * d.music_acc) >> 8)));
        pitch_send(v, d.value);
        return;
    }
    if (d.moved & 0x80)
        pitch_send(v, d.value);
}

void driver_between_ticks(void)
{
    int i;

    if (!d.song)
        return;
    for (i = 0; i < DRIVER_VOICES; i++) {
        Voice *v = &d.voice[i];

        d.current = (uint8_t)i;
        d.current_bit = (uint8_t)(1u << i);
        if (v->track >> 8)
            voice_between_ticks(v);
    }
}

/*
 * The DSP writes the main loop makes each pass: key-offs and key-ons, noise
 * and modulation masks, the flags, then the echo settings (those only when
 * the echo delay has settled).
 */
void driver_output(void)
{
    int i, settled = d.echo_delay_wait == d.echo_delay && !(d.echo_delay_wait & 0x80);
    int flg_ok = !(d.echo_delay_wait & 0x80);

    for (i = 0; i < DRIVER_VOICES; i++)
        if (d.key_off & (1u << i))
            nshvc_voice_key_off((uint32_t)i);
    if (d.noise_voices != d.sent_noise_voices || d.pitch_mod_voices != d.sent_mod_voices ||
        (settled && d.echo_voices != d.sent_echo_voices)) {
        if (settled)
            d.sent_echo_voices = d.echo_voices;
        d.sent_noise_voices = d.noise_voices;
        d.sent_mod_voices = d.pitch_mod_voices;
        for (i = 0; i < DRIVER_VOICES; i++)
            nshvc_voice_options((uint32_t)i, (d.noise_voices >> i) & 1, (d.sent_echo_voices >> i) & 1, (d.pitch_mod_voices >> i) & 1);
    }
    for (i = 0; i < DRIVER_VOICES; i++)
        if ((d.key_on & (1u << i)) && d.bank)
            nshvc_voice_key_on((uint32_t)i, d.bank->samples[d.voice[i].source]);
    if (flg_ok && d.flg != d.sent_flg) {
        if ((d.flg ^ d.sent_flg) & 0x1F)
            nshvc_sound_noise_rate(d.flg & 0x1F);
        if ((d.flg ^ d.sent_flg) & 0x20)
            nshvc_echo_writes(!(d.flg & 0x20));
        d.sent_flg = d.flg;
    }
    if (settled) {
        int8_t l = (int8_t)(d.echo_left >> 8), r = (int8_t)(d.echo_right >> 8), f = (int8_t)d.echo_feedback;

        if (l != d.sent_echo_left || r != d.sent_echo_right || f != d.sent_feedback || d.sent_delay != d.echo_delay || d.fir_dirty) {
            nshvc_echo(d.echo_delay, f, l, r, d.fir);
            d.sent_echo_left = l;
            d.sent_echo_right = r;
            d.sent_feedback = f;
            d.sent_delay = d.echo_delay;
            d.fir_dirty = 0;
        }
    }
    d.key_on = 0;
    d.key_off = 0;
    /* the noise generator's feedback, stepped every pass */
    {
        uint8_t lo = (uint8_t)d.random, hi = (uint8_t)(d.random >> 8);
        uint8_t carry = !(((lo ^ hi) >> 1) & 1);
        uint8_t next_lo = (uint8_t)(lo >> 1 | carry << 7), next_hi = (uint8_t)(hi >> 1 | (lo & 1) << 7);

        d.random = (uint16_t)(next_hi << 8 | next_lo);
    }
}

/*
 * @routine 19:C48C SpcReset
 * WHAT IT DOES: The driver's start: everything cleared, each voice's pitch
 * high byte 1, the master volume $60 each side, tempo $10, every voice keyed
 * off.
 */
void driver_reset(void)
{
    int i;

    {
        const SoundBank *b = d.bank;

        memset(&d, 0, sizeof d);
        d.bank = b;
    }
    d.echo_delay = 1;
    d.dsp_delay = 0;
    driver_set_echo_delay(1);
    for (i = 0; i < DRIVER_VOICES; i++)
        d.voice[i].pitch = 0x0100;
    nshvc_sound_volume(0x60, 0x60);
    d.tempo = 0x1000;
    d.key_off = 0xFF;
    d.sent_flg = 0;
}
