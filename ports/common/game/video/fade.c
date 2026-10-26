/*
 * fade.c: the screen's fade, a step a frame.
 *
 * The level is a word whose high byte is the brightness the frame shows
 * ($0F00 full); bit 15 set is the screen blanked. While fade_out is 0 the
 * screen fades in, else out.
 */
#include "game.h"
#include "rom.h"

#define FADE_FULL 0x0F00

static void fade_in(uint16_t step)
{
    uint16_t level = (uint16_t)(fade_level + step);

    fade_level = level >= FADE_FULL ? FADE_FULL : level;
}

/* Out by $58; a level that would go below 0 blanks the screen, and a
   blanked one stays. */
static void fade_step_out(void)
{
    uint16_t level = fade_level;

    if (!(level & 0x8000))
        fade_level = level >= 0x58 ? (uint16_t)(level - 0x58) : 0x8000;
}

/* In by a fixed $98, or out. */
void fade_step_fixed(void)
{
    if (fade_out)
        fade_step_out();
    else
        fade_in(0x98);
}

/* In by the step for the current brightness (faster or slower as it
   rises: the table at $02:89C4, a word for each), or out. */
void fade_step(void)
{
    if (fade_out)
        fade_step_out();
    else
        fade_in(rom_word(0x0289C4 + (uint8_t)(fade_level >> 8 << 1)));
}
