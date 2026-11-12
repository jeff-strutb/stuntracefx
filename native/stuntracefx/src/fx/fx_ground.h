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
/* A course's ground faces (Night Cruise's khaki verges): whether a flat
   face of palette entry `entry` is one, and its colour at a corner `depth`
   from the eye on screen row `row` of picture `panel` (fx_draw.c draws such
   faces in their corners' colours). */
int fx_ground_verge(uint8_t entry);
uint32_t fx_ground_verge_colour(uint8_t entry, double depth, float row, int panel);

#endif
