/*
 * remaster.h: the Remaster Options the game's C asks about (the runtime's
 * rm_options.c keeps them; main.c applies them). Off, the game is the original's, exactly.
 */
#ifndef SRFX_REMASTER_H
#define SRFX_REMASTER_H

/* Frame rate 60: a game frame each video frame, the simulation's steps a
   tick each (RM_TICKS of them to the original's one) */
int rm_tick60(void);
/* A race frame starts: at Frame rate 60 it waits for its tick and runs
   untimed, as native code, until rm_race_frame_end. */
void rm_race_frame(void);
void rm_race_frame_end(void);
/* the finished screen to the display at once: video/dma.c */
void RmRaceScreenShow(void);

#endif
