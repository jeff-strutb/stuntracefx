/*
 * obj_draw_list.h: the list of objects the Super FX draws each frame
 * (object/obj_draw_list.c).
 */
#ifndef SRFX_OBJ_DRAW_LIST_H
#define SRFX_OBJ_DRAW_LIST_H

#include <stdint.h>

/* Builds the draw records at $7E:F77F from the objects in the buckets the
   trigger list names, and counts them in $0CEE. */
void object_draw_list_build(void);

#endif
