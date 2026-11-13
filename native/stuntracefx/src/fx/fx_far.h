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
   record's; +FX_FAR_PARKED 1: a parked tyre of a car in the near list,
   drawn among the near objects by depth, not behind them (its car's
   child, which ObjectChildFollow hides). The frame program takes them
   once: the count is cleared.
   Beside them the view's place in the world the records are from (X, Y,
   Z words at FX_FAR_VIEW), which the ground's surface is placed by. */
enum {
    FX_FAR_BANK = 0x10000,
    FX_FAR_COUNT = 0x0000,
    FX_FAR_VIEW = 0x0002,
    FX_FAR_RECORDS = 0x0100,
    FX_FAR_SIZE = 0x30,
    FX_FAR_PLACE = 0x20,
    FX_FAR_LIFT = 0x2C,
    FX_FAR_PARKED = 0x2D,
    FX_FAR_MAX = 1024
};

/* The frame program's far objects in view, after the screen is cleared
   (render/frame.c): turned to the view, kept for fx_far_parked. */
void fx_far_pass(void);
/* FxObjectsDraw's record as it takes it: a far one's halvings and depth
   for the drawing that follows. */
void fx_far_record(uint16_t record);
/* While a far object draws: the halvings of its place (0 when none, and
   for every near object), and its true depth in place of the chip's
   halved one (`depth` as it is otherwise); for a model's detail and haze,
   so less a view's extra distance (fx_view_depth). */
int fx_far_shift(void);
uint16_t fx_far_depth(uint16_t depth);
int fx_far_drawing(void);
/* The frame program's objects (render/frame.c): the near list with every
   far object and parked tyre among it, in one order by depth. */
void fx_far_parked(uint16_t *stack);
/* While a pass draws: the near record a slot holds (its $0AAA address), 0
   for a far record or a tyre; `record` as it is outside a pass. */
uint16_t fx_far_original(uint16_t record);
/* An object's place from the eye in the world's axes, whole, for its
   corners made exactly (fx_face.c): a far slot's from its far record (32
   bits, never halved), a near one's from the main CPU's draw record it was
   uploaded from (g.frame_records, the slot's index, its model the slot's),
   while the pass draws (fx_far_parked); *in_view 1 when the record is
   placed in view space
   (its flags' bit 7: then `rel` is that place). 0 when not known (an
   object drawn outside the race's lists: the ground's survey, a menu's). */
int fx_far_exact_place(uint16_t record, double rel[3], int *in_view);
/* While the pass draws: the car (draw_record_car)
   the slot `record` holds belongs to, 0 none; *tyre whether it is one of
   its tyres. */
uint16_t fx_far_car(uint16_t record, int *tyre);
/* While the pass draws: the object (draw_record_object) the slot `record`
   holds, 0 none. */
uint16_t fx_far_object(uint16_t record);

#endif
