/*
 * state.h: the game's variables, by what they mean. Each field's comment
 * names the original's work RAM address, so that anyone converting code can
 * search here for an address before adding it again.
 *
 * Add a field under the section of the code that mostly writes it. A field
 * is one meaning: where the original read two bytes as a word, or used one
 * address for two things, say so in its comment and write what is meant.
 */
#ifndef STUNTRACEFX_STATE_H
#define STUNTRACEFX_STATE_H

#include <stdint.h>

typedef struct {
    /* ---- the session: boot, modes, what was chosen ---- */

    /* ---- menus and saves ---- */

    /* ---- the race: its flow, cars, laps and times ---- */

    /* ---- the objects ---- */

    /* ---- the picture: palettes, windows, tile maps, the HUD ---- */

    /* ---- sound ---- */

    uint8_t unused_keeps_the_struct_valid_until_fields_exist;
} Game;

extern Game g;

#endif
