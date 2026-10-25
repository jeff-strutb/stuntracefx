/*
 * trig.c: the sine tables in the ROM.
 *
 * The S-CPU's table at $00:96FE holds 320 signed bytes, the sine of each
 * 1/256 of a turn times 128; the cosine is the same table 64 entries on.
 * The Super FX's at $00:983E holds 257 words, the sine of each 1/256 of a
 * turn in 1.15 fixed point, and is interpolated between entries.
 */
#include "fixmath.h"
#include "rom.h"

#define SIN8_TABLE  0x0096FEu
#define SIN16_TABLE 0x00983Eu

int8_t sin8(uint8_t angle)
{
    return (int8_t)rom_at(SIN8_TABLE)[angle];
}

int8_t cos8(uint8_t angle)
{
    return (int8_t)rom_at(SIN8_TABLE)[angle + 64];
}

/* The entry for the high byte and the step to the next, the step times the
   low byte / 256 (rounded toward minus infinity) added. */
int16_t fx_sin16(uint16_t angle)
{
    unsigned i = angle >> 8;
    int16_t s0 = (int16_t)rom_word(SIN16_TABLE + 2 * i);
    int16_t step = wrap16(rom_word(SIN16_TABLE + 2 * i + 2) - s0);

    return wrap16(s0 + asr32((int32_t)(angle & 0xFF) * step, 8));
}

int16_t fx_cos16(uint16_t angle)
{
    return fx_sin16((uint16_t)(angle + 0x4000));
}
