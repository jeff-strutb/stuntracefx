/*
 * fx_far.h: the race's view distance (plan stage 10): the objects past the
 * block of map cells the trigger list names, handed to the chip in its
 * RAM's second 64 KB by the draw list (object/draw_list.c) and drawn by
 * fx/fx_far.c before the near list, farthest first.
 */
#ifndef STUNTRACEFX_FX_FAR_H
#define STUNTRACEFX_FX_FAR_H

#include <stdint.h>

/* The far records at $1:0000 of the chip's RAM: the count, then each a
   draw record as the near ones at $0AAA (30 bytes), its place from the
   view in 32 bits (X, Y, Z at +FX_FAR_PLACE, 4 bytes each, low first) and
   whether its depth key is lifted by $1F40 (+FX_FAR_LIFT), as the near
   record's. The frame program takes them once: the count is cleared. */
enum {
    FX_FAR_BANK = 0x10000,
    FX_FAR_COUNT = 0x0000,
    FX_FAR_RECORDS = 0x0100,
    FX_FAR_SIZE = 0x30,
    FX_FAR_PLACE = 0x20,
    FX_FAR_LIFT = 0x2C,
    FX_FAR_MAX = 1024
};

/* The frame program's far objects, after the screen is cleared and before
   the near list (render/frame.c). */
void fx_far_pass(uint16_t *stack);
/* FxObjectsDraw's record as it takes it: a far one's halvings and depth
   for the drawing that follows. */
void fx_far_record(uint16_t record);
/* While a far object draws: the halvings of its place (0 when none, and
   for every near object), and its true depth in place of the chip's
   halved one (`depth` as it is otherwise). */
int fx_far_shift(void);
uint16_t fx_far_depth(uint16_t depth);
int fx_far_drawing(void);

#endif
