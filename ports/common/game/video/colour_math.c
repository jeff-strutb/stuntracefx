/*
 * colour_math.c: the colour maths settings. The mode the raster interrupt
 * sends to the screen's colour maths register is kept at $0D47; the fixed
 * colour's step is at $7F:3408 and its intensity at $7F:340D.
 */
#include "game.h"
#include "rom.h"
#include "smallvideo.h"

#define colour_maths_mode   W8(0x0D47)      /* the mode the frame sends to the screen */
#define fixed_colour_step   W8(0x13408)     /* $7F:3408 */
#define fixed_colour_level  W8(0x1340D)     /* $7F:340D */

/* Subtract, half, on BG1 and BG2 and the backdrop: mode $A3. */
void colour_math_a3(void)
{
    colour_maths_mode = 0xA3;
}

/* The same, for callers in other banks. */
void colour_math_a3_long(void)
{
    colour_math_a3();
}

/* Sound command $04 to sound port 1, the mode $A7 and the fixed colour's
   step back to 0. */
void colour_math_a7_start(void)
{
    sound_queue_port1(0x04);
    colour_maths_mode = 0xA7;
    W16(0x13408) = 0x0000;
}

/* The fixed colour's intensity from the step, through the table at bank
   $03 $FE7B; it is kept at $7F:340D and returned. */
uint8_t fixed_colour_from_step(void)
{
    uint8_t v = *rom_at(0x03FE7Bu + fixed_colour_step);

    fixed_colour_level = v;
    return v;
}

/* No colour maths. */
void colour_math_off(void)
{
    colour_maths_mode = 0x00;
}

/* The same, for callers in other banks. */
void colour_math_off_long(void)
{
    colour_math_off();
}
