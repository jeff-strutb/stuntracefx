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
#include "fullwindow.h"
#include "menuwindow.h"

static int rm_on;                       /* Frame rate 60 */
static int rm_pace;                     /* video frames a race frame: 1 at 60; 0 the original's */
static int rm_share_on;                 /* the steps shared out over RM_TICKS ticks */
static int rm_frame60;                  /* in a race frame at 60: from its tick to its step over */
static int rm_whole;                    /* Enhanced's race: the original's whole steps, the 3D drawn between */
static int rm_wait_k;                   /* the video frames a race frame's wait has had */
/* each field's carried share (rm_share), by its address: work RAM, the
   Super FX's RAM; in 1/RM_SHARE_ONE of a unit */
static int32_t rm_carry_w[0x20000], rm_carry_s[0x10000];

int rm_tick60(void) { return rm_pace && rm_frame60; }

int rm_enhanced(void) { return rm_on; }

int rm_sharing(void) { return rm_share_on && rm_frame60 && !rm_whole; }

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

void rm_hud_state(int put_back)
{
    static int ok;
    static unsigned stopped;

    if (put_back) {
        hud_ok = ok;
        hud_stopped = stopped;
    } else {
        ok = hud_ok;
        stopped = hud_stopped;
    }
}

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

static void cpu_costs(int every);
static int drawing;                     /* drawing Enhanced's 3D between steps (whole_tween) */

/* In a race frame or a menus' 3D frame at 60: the main CPU's work there
   takes no time in Enhanced (cpu_costs) */
static void frame60(int on)
{
    rm_frame60 = on;
    cpu_costs(0);
}

/* A race frame. Enhanced (rm_share_on): the original's whole step each
   RM_TICKS video frames, the 3D drawn between its start and end in the
   frames between (rm_whole_tween): the physics are the original's, step
   for step, which a share of a step a tick cannot be (its dead zones and
   tire forces act on the state each tick reaches). */
void rm_race_frame(void)
{
    redraw_wait();
    if (!rm_pace)
        return;
    frame60(0);
    rm_whole = rm_share_on;
    rm_waiting = 1;                     /* the frame hook in the wait: a clean point (tween.c) */
    if (rm_whole && fw_on()) {          /* the full window: the step's own picture drawn again */
        drawing = 1;
        cpu_costs(0);
        fw_picture(0);
        fw_sky();
        drawing = 0;
        cpu_costs(0);
    }
    for (rm_wait_k = 0; rm_wait_k < (rm_whole ? RM_TICKS : rm_pace); rm_wait_k++)
        nshvc_wait_vblank();
    fw_before_step();                   /* the work RAM the full window borrowed, the game's again (then tween.c's) */
    rm_waiting = 0;
    rm_tween_backdrop_back();
    if (rm_share_on)                    /* counted after the wait: the frame hook in it shows the last */
        rm_ticks++;
    frame60(1);
}

int rm_whole_steps(void) { return rm_whole; }

/* Enhanced's race, from the frame hook in a race frame's wait: frames 1
   to RM_TICKS - 1 of the wait show the 3D drawn that far between the
   last step's start and end (its own picture is the start's, the next
   step's the end's), on the canvas in the shown buffer's place */
static void whole_tween(void)
{
    static uint8_t px[2][RM_TWEEN_BYTES];
    int j = rm_wait_k + 1, m;

    if (!rm_whole || !rm_waiting || j >= RM_TICKS)
        return;
    drawing = 1;                        /* the 3D's work, free as the race frame's */
    cpu_costs(0);
    if (fw_on()) {                      /* the full window's picture (fullwindow.c) */
        m = fw_picture(j);
        drawing = 0;
        cpu_costs(0);
        fw_sky_take();                  /* the gradient's bytes the game's as tween.c keeps them */
        if (m)
            rm_tween_backdrop_show();
        fw_sky();
        return;
    }
    m = rm_tween_buffers(j, RM_TICKS, px[0], px[1]);
    drawing = 0;
    cpu_costs(0);
    if (m == 3)
        rm_canvas_show_players(px[0], px[1]);
    else if (m)                         /* in the place of the buffer the step showed */
        rm_canvas_replace(px[m - 1]);
    if (m)
        rm_tween_backdrop_show();
}

void rm_race_frame_paced(void)
{
    redraw_wait();
    rm_whole = 0;
    if (!rm_pace)
        return;
    if (rm_share_on)
        rm_ticks++;
    frame60(1);
}

void rm_step_over(void)
{
    redraw_wait();
    fw_before_step();
    rm_tween_backdrop_back();
    frame60(0);
}

void rm_race_frame_end(void)
{
    if (rm_tick60())
        rm_tween_backdrop_late();
    frame60(0);
}

/* At each frame's end (the front end calls it). RM_REF_PACE=N (a check,
   not an option): the original's whole steps, one each N video
   frames, to compare a run at 60 with the original at a steady pace. */
static int in_irq;                      /* in the raster interrupt's handler */
static int costs_now = -1;              /* the costs as last set */

/* the main CPU's costs: off in Enhanced's race frames and menus' 3D
   frames, the work it makes processor-speed independent; on elsewhere
   (the boot, the menus, the sound driver's uploads and commands, whose
   handshakes keep the original's timing) and in the interrupt handler.
   Set each frame's end, otherwise only when they change: a set drops the
   console's carried part of a clock */
static void cpu_costs(int every)
{
    int on = in_irq || !rm_on || !(rm_frame60 || drawing);
    if (every || on != costs_now)
        nshvc_set_cpu_costs(on);
    costs_now = on;
}

void rm_irq_enter(void)
{
    in_irq++;
    cpu_costs(0);
}

void rm_irq_leave(void)
{
    in_irq--;
    cpu_costs(0);
}

void rm_frame_end(void)
{
    static int ref = -1;
    if (ref < 0)
        ref = getenv("RM_REF_PACE") ? atoi(getenv("RM_REF_PACE")) : 0;
    fw_prof_frame(1);
    fw_vblank_uploads();
    rm_tween_frame_end();
    rm_on = rm_option(RM_OPT_FRAMERATE) && nshvc_peek(0x7E10DA) == 0;
    rm_pace = ref ? ref : rm_on;
    rm_share_on = rm_on && !ref;
    cpu_costs(1);
    whole_tween();
    rm_canvas_frame_end();
    fw_frame_end();
    mw_frame_end();
    fw_prof_frame(0);
}
