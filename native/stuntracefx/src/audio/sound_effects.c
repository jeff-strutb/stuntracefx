/*
 * sound_effects.c: sound commands the game sends for each player's car.
 *
 * Most effects have one command for player 1 and another for player 2; the
 * routines here pick by the player whose car is being processed. A car can
 * also hold one looping sound, remembered per player so it is only re-sent
 * when it changes.
 */
#include "audio/sound.h"
#include "data/cart.h"
#include "fx/fx.h"
#include "fx/fx_rt.h"
#include "system/state.h"
#include "video/video.h"

/* a word of Super FX RAM, as the S-CPU reads it */
#define FX_PLAYER_RECORD 0x2476         /* the player's car record */
#define FX_NEAR_LEVEL 0x2642            /* how far off the nearest car is, negative with none */
#define FX_RECORD_SCRIPT 0x66           /* in a car's record: its script */
#define FX_RECORD_DISTANCE 0x88         /* in a car's record */

static uint16_t fx_word(uint16_t at)
{
    return (uint16_t)(fx_ram[at] | fx_ram[at + 1] << 8);
}

static uint8_t held_sound[2];           /* $0E4F, $0E50: each player's held sound */
static uint8_t near_car_mark;           /* $0E1A: the near car's sound is playing */
static uint8_t sound_hold[2];           /* $0DD8: each player's held-by-buttons sound is on */
static uint16_t near_band_sent;         /* $115E: $31 less the band last sent on port 1 */

/* Start a held sound: announce it first if nothing was held, then send it. */
static void held_sound_set(uint8_t *held, uint8_t announce, uint8_t sound)
{
    if (*held == 0)
        SoundPort1QueuePush(announce);
    *held = sound;
    SoundPort1QueuePush(sound);
}

/* Hold sound p1 (player 1) or p2 (player 2) unless it is already held. */
static void player_hold_sound(uint8_t p1, uint8_t p2)
{
    if (g.current_player == 0) {
        if (held_sound[0] != p1)
            held_sound_set(&held_sound[0], 0x09, p1);
        return;
    }
    if (held_sound[1] != p2)
        held_sound_set(&held_sound[1], 0x14, p2);
}

/*
 * @routine 04:D463 PlayerHoldSound32
 * WHAT IT DOES: Makes the current player's car hold sound $32 (player 1) or
 * $36 (player 2): when the player held nothing it first sends $09 or $14 on
 * port 1, then records the sound and sends it.
 */
void PlayerHoldSound32(void)
{
    player_hold_sound(0x32, 0x36);
}

/* @routine 04:D47F PlayerHoldSound33: as PlayerHoldSound32, with $33 or $37. */
void PlayerHoldSound33(void)
{
    player_hold_sound(0x33, 0x37);
}

/*
 * @routine 04:D49B PlayerStopHeldSound
 * WHAT IT DOES: Stops the current player's held sound, if any: clears it and
 * sends $0A (player 1) or $1A (player 2) on port 1.
 */
void PlayerStopHeldSound(void)
{
    uint8_t *held = &held_sound[g.current_player != 0];

    if (*held == 0)
        return;
    *held = 0;
    SoundPort1QueuePush(g.current_player ? 0x1A : 0x0A);
}

/* @routine 04:D4FB PlayerHoldSound34: as PlayerHoldSound32, with $34 or $38. */
void PlayerHoldSound34(void)
{
    player_hold_sound(0x34, 0x38);
}

/* @routine 04:D517 PlayerHoldSound35: as PlayerHoldSound32, with $35 or $39. */
void PlayerHoldSound35(void)
{
    player_hold_sound(0x35, 0x39);
}

/*
 * @routine 04:D5A2 Sound0E1AStop
 * WHAT IT DOES: Ends the near car's sound: clears its mark and sends $08 on
 * port 1, if the mark was set.
 */
void Sound0E1AStop(void)
{
    if (near_car_mark == 0)
        return;
    near_car_mark = 0;
    SoundPort1QueuePush(0x08);
}

static void player_sound(uint8_t p1, uint8_t p2)
{
    SoundPort1QueuePush(g.current_player ? p2 : p1);
}

/* @routine 04:D5B3 PlayerSound23: sound $23 (player 1) or $25 (player 2) on port 1. */
void PlayerSound23(void)
{
    player_sound(0x23, 0x25);
}

/* @routine 04:D5CC PlayerSound24: sound $24 or $26 on port 1. */
void PlayerSound24(void)
{
    player_sound(0x24, 0x26);
}

/* @routine 04:D5E5 PlayerSound0D: sound $0D or $18 on port 1. */
void PlayerSound0D(void)
{
    player_sound(0x0D, 0x18);
}

/* @routine 04:D5FE PlayerSound0B: sound $0B or $1B on port 1. */
void PlayerSound0B(void)
{
    player_sound(0x0B, 0x1B);
}

/* @routine 04:D617 PlayerSound12: sound $12 or $19 on port 1. */
void PlayerSound12(void)
{
    player_sound(0x12, 0x19);
}

/* @routine 04:D630 PlayerSound0C: sound $0C or $1C on port 1. */
void PlayerSound0C(void)
{
    player_sound(0x0C, 0x1C);
}

/*
 * @routine 03:EDDF SoundFromTableD89
 * WHAT IT DOES: Queues, exactly as given, the port 0 command the table at
 * $03:EDEF holds for player 1's car; returns the command.
 */
uint8_t SoundFromTableD89(void)
{
    uint8_t command = cart8(0x03, (uint16_t)(0xEDEF + g.car[0]));

    SoundPort0QueuePushRaw(command);
    return command;
}

/*
 * @routine 0B:FAF9 SoundPairFromTable
 * WHAT IT DOES: Queues two port 0 commands from the table at $0B:FB18:
 * player 2's car's with bit 7 set, then player 1's; returns the second.
 */
uint8_t SoundPairFromTable(void)
{
    uint8_t command;

    SoundPort0QueuePushRaw((uint8_t)(cart8(0x0B, (uint16_t)(0xFB18 + g.car[1])) | 0x80));
    command = cart8(0x0B, (uint16_t)(0xFB18 + g.car[0]));
    SoundPort0QueuePushRaw(command);
    return command;
}

/*
 * @routine 08:D580 SoundAtRateForLevel
 * WHAT IT DOES: Queues port 0 command $23 at a rate set by the level: none
 * below $28, every fourth object frame up to $9F, every other from $A0.
 */
void SoundAtRateForLevel(uint16_t level)
{
    if ((uint16_t)(level - 0x28) & 0x8000)
        return;
    if (g.object_frames & ((uint16_t)(level - 0xA0) & 0x8000 ? 3 : 1))
        return;
    SoundPort0QueuePushRaw(0x23);
}

/*
 * @routine 04:D0BE SoundIfFlag40
 * WHAT IT DOES: When bit 6 of the flags is set, queues for the current
 * player the port 0 command the table at $04:D0D6 holds for the player's car.
 */
void SoundIfFlag40(uint16_t flags, uint8_t player)
{
    if (flags & 0x0040)
        SoundPort0QueuePush(cart8(0x04, (uint16_t)(0xD0D6 + g.car[player])));
}

/*
 * @routine 04:D082 SoundHoldByFlags
 * WHAT IT DOES: Starts or stops a held sound for a player: when bit 15 or 14
 * of the flags is set and the player's mark is clear, marks it and sends $3A
 * (player 1) or $3C (player 2) on port 1; when both are clear and the mark is
 * set, clears it and sends $3B or $3D.
 */
void SoundHoldByFlags(uint16_t flags, uint8_t player)
{
    uint8_t *mark = &sound_hold[player != 0];

    if (flags & 0xC000) {
        if (*mark)
            return;
        *mark = 0xFF;
        SoundPort1QueuePush(player ? 0x3C : 0x3A);
    } else {
        if (!*mark)
            return;
        *mark = 0;
        SoundPort1QueuePush(player ? 0x3D : 0x3B);
    }
}

/*
 * @routine 07:D6D1 PlayerSound35
 * WHAT IT DOES: Queues sound command $35 on port 0 for the current player.
 * (The original's test of the player's mode does not change the call.)
 */
void PlayerSound35(void)
{
    SoundPort0QueuePush(0x35);
}

static void engine_sounds(int which, uint8_t player, uint16_t pressed, uint16_t held)
{
    if (g.end_ready || g.player_phase[which] != 0x06)
        return;
    SoundIfFlag40(pressed, player);
    SoundHoldByFlags(held, player);
}

/*
 * @routine 04:D040 EngineSoundsPlayer2
 * WHAT IT DOES: Player 2's engine sounds: while the engine phase is 6 and
 * the engines are not muted, SoundIfFlag40 with the buttons newly pressed
 * and SoundHoldByFlags with the buttons held.
 */
void EngineSoundsPlayer2(uint8_t player, uint16_t pressed, uint16_t held)
{
    engine_sounds(1, player, pressed, held);
}

/* @routine 04:D061 EngineSoundsPlayer1: as EngineSoundsPlayer2, for player 1. */
void EngineSoundsPlayer1(uint8_t player, uint16_t pressed, uint16_t held)
{
    engine_sounds(0, player, pressed, held);
}

/*
 * @routine 03:EC43 HandlerSoundIndex
 * WHAT IT DOES: Maps a selection handler (the word the menu keeps for a
 * racer) to its sound index: $81B9 to 0, $853D to 1, $8411 to 2, $82E5 to 3,
 * any other to 4.
 */
uint8_t HandlerSoundIndex(uint16_t handler)
{
    static const uint16_t known[4] = {0x81B9, 0x853D, 0x8411, 0x82E5};
    uint8_t i;

    for (i = 0; i < 4; i++)
        if (handler == known[i])
            return i;
    return 4;
}

/*
 * @routine 03:DCEF SoundIndicesFromHandlers
 * WHAT IT DOES: Sets the cars' sound indices from the two selection
 * handlers: $0D89 from the first, also $0D8B when a hold is set, and $0D8A
 * from the second.
 */
void SoundIndicesFromHandlers(void)
{
    g.car[0] = HandlerSoundIndex(g.selection_handler[0]);
    if (sound_hold_a || sound_hold_b)
        g.car_index_kept = g.car[0];
    g.car[1] = HandlerSoundIndex(g.selection_handler[1]);
}

/*
 * @routine 04:D533 SoundNearCarOnce
 * WHAT IT DOES: The first time it runs (mark $0E1A clear), marks it, sends
 * $1D on port 1 and, unless the race is frozen, a sound for the car nearest
 * the player when that car's record word $88 is not below the player's: $1D,
 * $1F or $1E on port 0 for its script $876E, $8AA8 or $8990.
 */
void SoundNearCarOnce(void)
{
    uint16_t player_record, near_record, script;

    if (near_car_mark)
        return;
    near_car_mark = 0xFF;
    SoundPort1QueuePush(0x1D);
    if (g.freeze)
        return;
    player_record = fx_word(FX_PLAYER_RECORD);
    near_record = fx_word(FX_NEAR_CAR);
    if (fx_word((uint16_t)(FX_RECORD_DISTANCE + near_record)) < fx_word((uint16_t)(FX_RECORD_DISTANCE + player_record)))
        return;
    script = fx_word((uint16_t)(FX_RECORD_SCRIPT + near_record));
    if (script == 0x876E)
        SoundPort0QueuePush(0x1D);
    else if (script == 0x8AA8)
        SoundPort0QueuePush(0x1F);
    else if (script == 0x8990)
        SoundPort0QueuePush(0x1E);
}

/*
 * @routine 07:EDC1 NearCarSoundUpdate
 * WHAT IT DOES: Unless the engines are muted, follows the car nearest the
 * player with sound: packs its level and side into port 2's byte; when its
 * distance band changes, sends $31 less the band on port 1, clears the
 * no-nearest-car mark and runs SoundNearCarOnce. With no nearest car (level
 * negative) it sets that mark and stops the near car's sound.
 */
void NearCarSoundUpdate(void)
{
    uint16_t level, band;

    if (g.end_ready)
        return;
    level = fx_word(FX_NEAR_LEVEL);
    if (level & 0x8000) {
        g.near_car_none = 1;
        Sound0E1AStop();
        return;
    }
    g.speed_gauge[1] = (uint8_t)(level >> 4 | fx_word(FX_NEAR_SIDE) << 4);     /* $0E47, port 2's byte */
    band = (uint16_t)(0x0031 - fx_word(FX_NEAR_BAND));
    if (band == near_band_sent)
        return;
    near_band_sent = band;
    SoundPort1QueuePush((uint8_t)band);
    g.near_car_none = 0;
    SoundNearCarOnce();
}

/*
 * @routine 04:DA20 WindowSlideUpSound
 * WHAT IT DOES: Sends sound $10 on port 1 while the window slide is going up
 * (bit 2 of $0DA5), then sets $19D8 (window lines off) if it is clear.
 */
void WindowSlideUpSound(void)
{
    if (g.picture_effects & 0x04)
        SoundPort1QueuePush(0x10);
    if (!windows.bands_state)
        windows.bands_state = 1;
}
