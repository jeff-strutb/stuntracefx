/*
 * fx_motion.c: a record moved by its velocity.
 */
#include "game.h"
#include "physics.h"

/* The record's velocity (+$2E) over 8 added to its position (+$1E), over
   16 when +$1C has bit 3; an object of kind 6 also adds its rates (+$34)
   to the three words at +$24 (halved with the bit). Each division is by
   DIV2s. */
void fx_record_move(uint16_t rec, uint16_t kind)
{
    int halved = FXU(rec + 0x1C) & 8, i, k;

    for (i = 0; i < (kind == 6 ? 6 : 3); i++) {
        int16_t v = FXW(rec + 0x2E + 2 * i);

        for (k = 0; k < (i < 3 ? 3 : 0) + (halved ? 1 : 0); k++)
            v = div2(v);
        FXW(rec + 0x1E + 2 * i) = wrap16(v + FXW(rec + 0x1E + 2 * i));
    }
}
