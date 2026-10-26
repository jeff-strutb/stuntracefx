/*
 * remaster.h: the Remaster Options the game's C asks about (the runtime's
 * rm_options.c keeps them; main.c applies them). Off, the game is the original's, exactly.
 */
#ifndef SRFX_REMASTER_H
#define SRFX_REMASTER_H

#include <stdint.h>

/* Frame rate 60: a game frame each video frame, the simulation's steps a
   tick each, RM_TICKS of them to the original's one: the original's race
   frames took 4 to 9 video frames, 6 on average */
#define RM_TICKS 6
int rm_tick60(void);
/* A change d the original makes to a field once a step, as a tick's share
   of it: the field's carry kept (by its address, `field`) so that
   RM_TICKS ticks change it by exactly d. d itself when the steps are the
   original's. */
int32_t rm_share(uint32_t field, int32_t d);
/* A race frame starts: at Frame rate 60 it waits for its tick and runs
   untimed, as native code, until rm_race_frame_end. */
void rm_race_frame(void);
void rm_race_frame_end(void);
/* The step is over though the race frame runs on: its code waits for
   the vertical blank itself (the pause, the end's fades) or leaves the
   race (the results and the screens they open). From here it runs frames
   of its own on the console's time; the next race frame is a tick again. */
void rm_step_over(void);
/* the finished screen to the display at once: video/dma.c */
void RmRaceScreenShow(void);

#endif
