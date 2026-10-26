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
/* Whether the steps are being shared out now (a race frame at 60) */
int rm_sharing(void);
/* Whether this tick completes one of the original's steps: every
   RM_TICKS-th tick at 60, always otherwise. The discrete work (the race's
   phases, countdowns that end in an event, the HUD's tallies, sound
   commands) runs on these ticks only, so it keeps the original's timing
   exactly. */
int rm_step_due(void);
/* the ticks run at 60 so far */
unsigned rm_tick_count(void);
/* rm_share of d * mul / RM_SHARE_ONE: for a change that is a fraction of
   a gap (a decay, a filter) the tick's own fraction, RM_DECAY(c) for a
   step's c = 1/2, 1/4, 1/8, 1/16 */
#define RM_SHARE_ONE 4096
int32_t rm_share_by(uint32_t field, int32_t d, int32_t mul);
/* (1 - (1 - c)^(1/6)) / c * 6 * 4096: the share of a step's change by a
   fraction c of a gap that leaves after six ticks what the step leaves */
#define RM_DECAY_2  5363            /* c = 1/2 */
#define RM_DECAY_4  4602            /* c = 1/4 */
#define RM_DECAY_8  4327            /* c = 1/8 */
#define RM_DECAY_16 4207            /* c = 1/16 */
/* rm_share for a signed word of the Super FX's RAM (bank $70) */
static inline uint16_t rm_share_fx(uint16_t addr, uint16_t d)
{
    return (uint16_t)rm_share(0x700000u | addr, (int16_t)d);
}
static inline uint16_t rm_share_fx_by(uint16_t addr, uint16_t d, int32_t mul)
{
    return (uint16_t)rm_share_by(0x700000u | addr, (int16_t)d, mul);
}
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
