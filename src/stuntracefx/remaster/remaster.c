/*
 * remaster.c: Frame rate 60 (remaster.h). The race's loop runs a game frame
 * each tick, 1/60 s: a race frame waits for its tick (the vertical blank's
 * start), then runs its work as native code (contract 2.0: the main CPU's
 * work takes no time), its Super FX programs with their own durations; at its end its screen goes to the display at
 * once (the game's upload stages in one go), and the simulation steps a
 * tick at a time. Not in the attract's demos ($10DA set), which replay the
 * original's buttons frame by frame. The option is taken at each frame's
 * end.
 */
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "machine.h"
#include "gsu.h"
#include "console_ext.h"
#include "remaster.h"
#include "rm_options.h"

static int rm_on;                       /* Frame rate 60 */
static int rm_pace;                     /* video frames a race frame: 1 at 60; 0 the original's */
static int rm_share_on;                 /* the steps shared out over RM_TICKS ticks */
static int rm_frame60;                  /* in a race frame at 60: from its tick to its step over */
/* each field's carried share (rm_share), by its address: work RAM, the
   Super FX's RAM; in 1/RM_SHARE_ONE of a unit */
static int32_t rm_carry_w[0x20000], rm_carry_s[0x10000];

int rm_tick60(void) { return rm_pace && rm_frame60; }

int rm_enhanced(void) { return rm_on; }

int rm_sharing(void) { return rm_share_on && rm_frame60; }

int rm_sharing_on(void) { return rm_share_on; }

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
static int rm_waiting;                  /* in a race frame's wait for its tick */

int rm_in_tick_wait(void) { return rm_waiting; }

unsigned rm_tick_count(void) { return rm_ticks; }

int rm_step_due_every(unsigned k)
{
    return !rm_sharing() || rm_ticks % k == k - 1;
}

static int rm_redraw;
static jmp_buf *redraw_out;             /* where a redraw that would wait leaves to */

int rm_redrawing(void) { return rm_redraw; }

/* A redraw (or an in-between frame) that reaches a wait for a frame (a
   race frame, a phase's own loop, a step over) is let go: the caller puts
   the state back and shows nothing new. */
static void redraw_wait(void)
{
    if (rm_redraw && redraw_out)
        longjmp(*redraw_out, 1);
}

/* Every wait of the game's (waits.h) comes here first: a redraw never waits */
void rm_wait_point(void)
{
    redraw_wait();
}

int rm_redraw_run(void (*draw)(void))
{
    jmp_buf here, *outer = redraw_out;
    int on = rm_redraw, ok;
    redraw_out = &here;
    rm_redraw = 1;
    if (!setjmp(here)) {
        draw();
        ok = 1;
    } else
        ok = 0;
    rm_redraw = on;
    redraw_out = outer;
    return ok;
}

int rm_step_due(void)
{
    return rm_redraw || !rm_sharing() || rm_ticks % RM_TICKS == RM_TICKS - 1;
}

/* the work RAM and the registers RaceSoundsAndHud started from on the last
   step's tick; the state a redraw puts back */
static uint8_t hud_w[0x20000], now_w[0x20000], now_s[0x2C00 + 0x400];
static Regs65816 hud_cpu;
static unsigned hud_tick;
static int hud_ok;
static unsigned hud_stopped;            /* the tick whose redraw reached the HUD's loop */
int rm_tween_start_side;

void RmRaceHud(void)
{
    if (!rm_tick60()) {
        hud_ok = 0;
        call_routine(0x07DAA7, 1);
        return;
    }
    if (rm_step_due()) {
        memcpy(hud_w, nshvc_wram(), sizeof hud_w);
        hud_cpu = cpu;
        hud_tick = rm_ticks;
        hud_ok = 1;
        call_routine(0x07DAA7, 1);
        return;
    }
    rm_hud_redraw();
}

void rm_hud_redraw(void) { rm_hud_redraw_in(1); }

static void hud_draw(void) { call_routine(0x07DAA7, 1); }

void rm_hud_redraw_in(int hold)
{
    uint8_t *w = nshvc_wram(), *s = nshvc_sram();
    Regs65816 cpu_now = cpu;
    RegsGSU gsu_now = gsu;
    if ((!hud_ok && !(rm_tween_start_side && hud_stopped == rm_ticks)) || rm_ticks - hud_tick >= RM_TICKS)
        return;
    /* the Super FX's RAM outside the two screen buffers ($2C00-$FBFF) kept
       too: only the pixels drawn stay */
    memcpy(now_w, w, sizeof now_w);
    memcpy(now_s, s, 0x2C00);
    memcpy(now_s + 0x2C00, s + 0xFC00, 0x400);
    memcpy(w, hud_w, sizeof hud_w);
    if (rm_tween_start_side) {          /* an in-between frame: the RAM the tick's redraw read, its end's */
        memcpy(s, rm_tween_end_sram(), 0x2C00);
        memcpy(s + 0xFC00, rm_tween_end_sram() + 0xFC00, 0x400);
    }
    cpu = hud_cpu;
    if (hold)
        nshvc_hold_interrupts(1);
    if (!rm_redraw_run(hud_draw))
        hud_ok = 0, hud_stopped = rm_ticks;  /* the HUD's own loop this step: no more redraws of it */
    if (hold)
        nshvc_hold_interrupts(0);
    memcpy(w, now_w, sizeof now_w);
    memcpy(s, now_s, 0x2C00);
    memcpy(s + 0xFC00, now_s + 0x2C00, 0x400);
    cpu = cpu_now;
    gsu = gsu_now;
}

void rm_race_frame(void)
{
    int n;
    redraw_wait();
    if (!rm_pace)
        return;
    rm_frame60 = 0;
    rm_waiting = 1;                     /* the frame hook in the wait: a clean point (tween.c) */
    for (n = 0; n < rm_pace; n++)       /* the tick */
        nshvc_wait_vblank();
    rm_waiting = 0;
    if (rm_share_on)                    /* counted after the wait: the frame hook in it shows the last */
        rm_ticks++;
    rm_frame60 = 1;
}

void rm_race_frame_paced(void)
{
    redraw_wait();
    if (!rm_pace)
        return;
    if (rm_share_on)
        rm_ticks++;
    rm_frame60 = 1;
}

void rm_step_over(void)
{
    redraw_wait();
    rm_frame60 = 0;
}

void rm_race_frame_end(void)
{
    rm_frame60 = 0;
}

/* At each frame's end (the front end calls it). RM_REF_PACE=N (a check,
   not an option): the original's whole steps, one each N video
   frames, to compare a run at 60 with the original at a steady pace. */
void rm_frame_end(void)
{
    static int ref = -1;
    if (ref < 0)
        ref = getenv("RM_REF_PACE") ? atoi(getenv("RM_REF_PACE")) : 0;
    rm_tween_frame_end();
    rm_on = rm_option(RM_OPT_FRAMERATE) && nshvc_peek(0x7E10DA) == 0;
    rm_pace = ref ? ref : rm_on;
    rm_share_on = rm_on && !ref;
}
