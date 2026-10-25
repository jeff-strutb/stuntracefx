/*
 * race_text.c: words the race screen spells with sprites, a letter tile
 * per sprite.
 */
#include "game.h"

#define CLASS_NAMES 0x02EFD6u      /* "MASTER" and "EXPERT", six tiles each */

/* The class's name on the six sprites after sprite wram_0D7B: "EXPERT" for
   class 1, "MASTER" for any other but 0, nothing for 0. */
void oam_class_name(void)
{
    uint16_t at = (uint16_t)((uint16_t)(wram_0D7B + 1) << 2);
    const uint8_t *name;
    int i;

    if (!wram_0D4A)
        return;
    name = rom_at(CLASS_NAMES + (wram_0D4A == 1 ? 6 : 0));
    for (i = 0; i < 6; i++)
        W8((uint16_t)(0x0377 + at + 4 * i)) = name[i];
}
