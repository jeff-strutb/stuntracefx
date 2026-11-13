/*
 * perf.h: where the time of an update and of a picture goes (perf.c), to
 * find what to make faster. SRFX_PROFILE=1 in the environment prints it once
 * a second; otherwise nothing is kept.
 */
#ifndef STUNTRACEFX_PERF_H
#define STUNTRACEFX_PERF_H

#include <stdint.h>

/* the stages of a picture, as the engine's profiling zones
   (nshvc_profile_begin with one of these, nshvc_profile_end after) */
extern const char perf_tween_state[];   /* the state kept and the in-between state made, and put back */
extern const char perf_frame[];         /* the race's 3D: the frame program, each panel */
extern const char perf_ground[];        /* the ground and water (fx_ground.c), within it */
extern const char perf_far[];           /* the objects past the near block turned to the view (fx_far.c), within it */
extern const char perf_near[];          /* every object drawn (fx_far_parked), within it */
extern const char perf_compose[];       /* the 3D's recordings into the screen's list */
extern const char perf_video[];         /* the 2D: the screen, its layers and lists (the compose within it) */

/* main.c, around the game's update and its draw */
void perf_update_begin(void);
void perf_update_end(void);
void perf_draw_begin(void);
void perf_draw_end(void);
/* a draw call of n vertices into a draw list */
void perf_draw_call(uint32_t vertices);

#endif
