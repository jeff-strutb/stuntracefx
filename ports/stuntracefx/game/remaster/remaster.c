/*
 * remaster.c: Frame rate 60 (remaster.h). The race's loop runs a game frame
 * each tick, 1/60 s: a race frame waits for its tick (the vertical blank's
 * start), then runs untimed, as native code (console_ext.h), its Super FX
 * programs to their STOPs; at its end its screen goes to the display at
 * once (the game's upload stages in one go), and the simulation steps a
 * tick at a time. Not in the attract's demos ($10DA set), which replay the
 * original's buttons frame by frame. The option is taken at each frame's
 * end.
 */
#include <stdlib.h>

#include "console_ext.h"
#include "remaster.h"
#include "rm_options.h"

static int rm_on;                       /* Frame rate 60 */
static int rm_pace;                     /* video frames a race frame: 1 at 60; 0 the original's */
static int rm_share_on;                 /* the steps shared out over RM_TICKS ticks */
/* each field's carried share (rm_share), by its address: work RAM, the
   Super FX's RAM; in 1/RM_SHARE_ONE of a unit */
static int32_t rm_carry_w[0x20000], rm_carry_s[0x10000];

int rm_tick60(void) { return rm_pace && nshvc_untimed(); }

int rm_sharing(void) { return rm_share_on && nshvc_untimed(); }

int32_t rm_share_by(uint32_t field, int32_t d, int32_t mul)
{
    int32_t *c;
    int64_t t, q, one = (int64_t)RM_SHARE_ONE * RM_TICKS;
    if (!rm_sharing())
        return d;
    c = (field >> 16 & 0xFE) == 0x70 ? &rm_carry_s[field & 0xFFFF]
                                      : &rm_carry_w[field & 0x1FFFF];
    t = (int64_t)d * mul + *c;
    q = t >= 0 ? t / one : -((one - 1 - t) / one);  /* floor */
    *c = (int32_t)(t - q * one);
    return (int32_t)q;
}

int32_t rm_share(uint32_t field, int32_t d)
{
    return rm_share_by(field, d, RM_SHARE_ONE);
}

static unsigned rm_ticks;               /* ticks run, sharing */

unsigned rm_tick_count(void) { return rm_ticks; }

int rm_step_due_every(unsigned k)
{
    return !rm_sharing() || rm_ticks % k == k - 1;
}

int rm_step_due(void)
{
    return !rm_sharing() || rm_ticks % RM_TICKS == RM_TICKS - 1;
}

void rm_race_frame(void)
{
    int n;
    if (!rm_pace)
        return;
    if (rm_share_on)
        rm_ticks++;
    nshvc_set_untimed(0);
    for (n = 0; n < rm_pace; n++)       /* the tick */
        nshvc_wait_vblank();
    nshvc_set_untimed(1);
}

void rm_race_frame_paced(void)
{
    if (!rm_pace)
        return;
    if (rm_share_on)
        rm_ticks++;
    nshvc_set_untimed(1);
}

void rm_step_over(void)
{
    nshvc_set_untimed(0);
}

void rm_race_frame_end(void)
{
    nshvc_set_untimed(0);
}

/* At each frame's end (the front end calls it). RM_REF_PACE=N (a check,
   not an option): the original's whole steps, untimed, one each N video
   frames, to compare a run at 60 with the original at a steady pace. */
void rm_frame_end(void)
{
    static int ref = -1;
    if (ref < 0)
        ref = getenv("RM_REF_PACE") ? atoi(getenv("RM_REF_PACE")) : 0;
    rm_on = rm_option(RM_OPT_FRAMERATE) && nshvc_peek(0x7E10DA) == 0;
    rm_pace = ref ? ref : rm_on;
    rm_share_on = rm_on && !ref;
}
