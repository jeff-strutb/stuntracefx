/*
 * fx_points.c: a collision model's points, the Super FX code of
 * asm/stuntracefx/render/fx_points.asm (C: src/stuntracefx/render/fx_points.c).
 *
 * A model's points are a byte script in the cartridge, unpacked into the
 * point table, three words (x, y, z) a point. Commands: 4, a count, then
 * that many points as three signed bytes; $38, a count, then that many
 * points each stored twice, as (x, y, z) and (-x, y, z); $20, a word added
 * to the cursor (a jump, counted from the word's second byte); $1C, a
 * skipped byte, then a jump through a table of such words, one for each of
 * the model's frames. Anything else ends the script. A count of 0 runs
 * 65,536 times, as the chip's LOOP ran it.
 *
 * Interim (plan section 8.2): the table stays in the chip's RAM at $2488
 * (FX_POINTS_W) in the original's layout, because FxCollideModel's face
 * walk (fx/object/fx_physics.c) reads it there; it moves with that walk
 * into FxStateBlock. That is this file's one place in the chip's RAM,
 * written only through put.
 */

#include "data/object_tables.h"
#include "fx/fx.h"
#include "fx/render/fx_points.h"

enum { POINTS = 0x04, MIRRORED_POINTS = 0x38, JUMP_BY_FRAME = 0x1C, JUMP = 0x20 };

/* one word of the table; the place after it */
static uint16_t put(uint16_t at, int16_t value)
{
    fx_ram[at] = (uint8_t)value;
    fx_ram[(uint16_t)(at + 1)] = (uint8_t)((uint16_t)value >> 8);
    return (uint16_t)(at + 2);
}

/*
 * @routine 04:A154 FxModelPoints
 * WHAT IT DOES: Unpacks a collision model's point script into the point
 * table, as described above; `frame` is the model's frame (its record's
 * FXREC_FRAME), which picks the jump of a $1C command.
 * HOW: A cursor through the script, each command byte signed. The chip
 * read the frame from $0044, where FxCollideModel had just put it, and
 * kept three registers on its stack; it left its cursor and ROM buffer
 * past the ending byte, which nothing after it reads.
 */
void FxModelPoints(CartRef script, uint16_t frame)
{
    uint16_t at = 0, out = FX_POINTS_W;

    for (;;) {
        int8_t command = (int8_t)model_points_byte(script, at);

        at = (uint16_t)(at + 1);
        if (command == POINTS || command == MIRRORED_POINTS) {
            uint16_t count = model_points_byte(script, at);

            at = (uint16_t)(at + 1);
            do {
                int8_t x = (int8_t)model_points_byte(script, at);
                int8_t y = (int8_t)model_points_byte(script, (uint16_t)(at + 1));
                int8_t z = (int8_t)model_points_byte(script, (uint16_t)(at + 2));

                at = (uint16_t)(at + 3);
                out = put(put(put(out, x), y), z);
                if (command == MIRRORED_POINTS)
                    out = put(put(put(out, (int16_t)-x), y), z);
            } while (--count);
        } else if (command == JUMP_BY_FRAME) {
            at = (uint16_t)(at + 1 + 2 * frame);         /* past the skipped byte, to the frame's word */
            at = (uint16_t)(at + 1 + model_points_word(script, at));
        } else if (command == JUMP) {
            at = (uint16_t)(at + 1 + model_points_word(script, at));
        } else {
            return;
        }
    }
}
