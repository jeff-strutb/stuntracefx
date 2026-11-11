/*
 * fx_ground.h: the race's ground and water as a surface (fx/fx_ground.c).
 */
#ifndef STUNTRACEFX_FX_GROUND_H
#define STUNTRACEFX_FX_GROUND_H

/* The surface of the course into the picture being drawn: the frame
   program's first drawing after the screen clear (render/frame.c). */
void fx_ground_draw(void);
/* A view-space depth as the land's depth in the draw list: in (0, 0.998),
   nearer smaller, one over the depth linear across the screen. */
float fx_ground_depth_z(double depth);
/* The tunnels' rock (fx_ground.c): each object's drawing, as it begins
   (fx_far_record, its halvings `shift`), and the end of the one before and
   of each list of objects. */
void fx_ground_object_begin(uint16_t record, int shift);
void fx_ground_object_end(void);

#endif
