/*
 * tween.c: the in-between frames (remaster.h). A display faster than the
 * game's 60 ticks is shown pictures between two ticks. A race frame takes
 * its view and draw list from the state its tick starts from, and the next
 * frame will draw from the state this tick ends with. The state at the
 * tick's start is kept; a picture at num/den of the way is the nearer
 * tick's state (the start's before half way, else the end's: at 0/1 and
 * 1/1 the two frames' pictures exactly) with the positions and angles the
 * 3D draws from (the view's record, the objects', the Super FX records')
 * set between the two, the 3D drawn again (RmTweenDraw), and everything
 * put back. A field that moved further than any tick moves it (a new or
 * placed object) keeps the nearer tick's.
 */
#include <string.h>

#include "machine.h"
#include "gsu.h"
#include "console_ext.h"
#include "remaster.h"

void RmTweenDraw(void);
void RmTweenDrawTwoPlayers(void);

static uint8_t was_w[0x20000], was_s[0x10000], kept_w[0x20000], kept_s[0x10000];
static RegsGSU was_gsu;                     /* the Super FX's registers then (SCBR: the buffer it draws in) */
/* two players: the state each picture pair was drawn from, at its tick's
   RaceSoundsPlayer1 (remaster.h), for this tick and the last */
static uint8_t pic_w[2][0x20000], pic_s[2][0x10000];
static RegsGSU pic_gsu[2];
static unsigned pic_tick[2];
static int pic_ok[2], pic_step[2], pic_now;
int rm_tween_step;
static unsigned was_tick;                   /* the tick the kept start is of */
static int was_ok, ticked, shown_tick;      /* a race tick since the last frame's end; the frame shown was one */
static uint8_t done[0x10000 >> 1];          /* a Super FX word already set between */

#define FAR 0x0800                          /* further than a tick moves a thing */

void rm_tween_tick_start(void)
{
    memcpy(was_w, nshvc_wram(), sizeof was_w);
    memcpy(was_s, nshvc_sram(), sizeof was_s);
    was_gsu = gsu;
    was_tick = rm_tick_count();
    was_ok = 1;
    ticked = 1;
}

void rm_tween_pictures_drawn(void)
{
    pic_now ^= 1;
    memcpy(pic_w[pic_now], nshvc_wram(), sizeof pic_w[0]);
    memcpy(pic_s[pic_now], nshvc_sram(), sizeof pic_s[0]);
    pic_gsu[pic_now] = gsu;
    pic_tick[pic_now] = rm_tick_count();
    pic_step[pic_now] = rm_step_due();
    pic_ok[pic_now] = 1;
}

/* the state a picture is drawn from: its memory but the screen buffers,
   and the Super FX's registers */
static void draw_from(uint8_t *w, uint8_t *s, const uint8_t *from_w, const uint8_t *from_s, const RegsGSU *from_gsu)
{
    memcpy(w, from_w, 0x20000);
    memcpy(s, from_s, 0x2C00);
    memcpy(s + 0x2C00 + RM_TWEEN_BYTES, from_s + 0x2C00 + RM_TWEEN_BYTES, 0x9400 - 0x2C00 - RM_TWEEN_BYTES);
    memcpy(s + 0x9400 + RM_TWEEN_BYTES, from_s + 0x9400 + RM_TWEEN_BYTES, 0x10000 - 0x9400 - RM_TWEEN_BYTES);
    gsu = *from_gsu;
}

void rm_tween_frame_end(void)
{
    shown_tick = ticked;
    ticked = 0;
}

/* whether the last video frame's picture came from a tick of the race
   with its start kept */
int rm_tween_ready(void)
{
    /* only from a race frame's wait for its tick (the race frame done, the
       Super FX idle), and with the Super FX's ROM and RAM its own (SCMR
       bits 4 and 3), as the race frame leaves them: not while the NMI's
       upload has them part way through a DMA of a buffer (about 1 tick in
       100 at the vertical blank's start). An interrupt handler that leaves
       them alone (a two-player race's split screen IRQ, at the frame's
       end every frame) is no bar. */
    return was_ok && shown_tick && was_tick == rm_tick_count() && rm_sharing_on() && rm_in_tick_wait() &&
           (gsu.scmr & 0x18) == 0x18;
}

/* the word at a of `to` set num/den of the way from `was` to `now` */
static void between16(uint8_t *to, const uint8_t *now, const uint8_t *was, unsigned a, int num, int den, int angle)
{
    if (to == nshvc_sram()) {                       /* once a word */
        if (done[(a & 0xFFFF) >> 1])
            return;
        done[(a & 0xFFFF) >> 1] = 1;
    }
    int16_t n = (int16_t)(now[a] | now[a + 1] << 8), w = (int16_t)(was[a] | was[a + 1] << 8);
    int d = (int16_t)(uint16_t)(n - w);
    uint16_t v;

    if (!angle && (d > FAR || d < -FAR))
        return;
    v = (uint16_t)(w + d * num / den);
    to[a] = (uint8_t)v;
    to[a + 1] = (uint8_t)(v >> 8);
}

static void between8(uint8_t *to, const uint8_t *now, const uint8_t *was, unsigned a, int num, int den)
{
    int d = (int8_t)(uint8_t)(now[a] - was[a]);

    to[a] = (uint8_t)(was[a] + d * num / den);
}

static uint16_t word(const uint8_t *m, unsigned a)
{
    return (uint16_t)(m[a] | m[a + 1] << 8);
}

/* the view's record of the car at `slot`: two links (+$4F) on from it */
static void between_view(uint8_t *w, unsigned slot, int num, int den)
{
    unsigned rec = word(w, slot), i;
    rec = word(w, 0x204F + rec);
    rec = word(w, 0x204F + rec);
    if (rec >= 0x1FE0)
        return;
    for (i = 0; i < 3; i++)
        between16(w, kept_w, was_w, rec + 0x08 + 2 * i, num, den, 0);
    for (i = 0; i < 3; i++)
        between16(w, kept_w, was_w, rec + 0x0E + 2 * i, num, den, 1);
}

const uint8_t *rm_tween_end_sram(void) { return kept_s; }

int rm_tween_buffers(int num, int den, uint8_t *at_2c00, uint8_t *at_9400)
{
    uint8_t *w = nshvc_wram(), *s = nshvc_sram();
    Regs65816 cpu_kept = cpu;
    RegsGSU gsu_kept = gsu;
    int i, n, drew, two = w[0x0D62] != 0, mask;
    unsigned k, rec;

    if (!rm_tween_ready())
        return 0;
    memcpy(kept_w, w, sizeof kept_w);
    memcpy(kept_s, s, sizeof kept_s);
    memset(done, 0, sizeof done);
    /* the state of the nearer tick (what is drawn, its frames, its looks,
       the Super FX's registers): the start's for the first half of the
       way, else the end's; its positions and angles set between the two.
       The screen buffers stay as they are. Two players: the start is the
       last tick's pictures, the end this tick's, each drawn from the state
       at its tick's RaceSoundsPlayer1. */
    if (two) {
        int side = 2 * num < den ? pic_now ^ 1 : pic_now;
        if (!pic_ok[side] || pic_tick[side] != (2 * num < den ? was_tick - 1 : was_tick))
            return 0;
        draw_from(w, s, pic_w[side], pic_s[side], &pic_gsu[side]);
        rm_tween_step = pic_step[side];
    } else if (2 * num < den) {
        draw_from(w, s, was_w, was_s, &was_gsu);
        /* one player: the text program's string ($2732: its address,
           bank, position at $2738/$273A, tested by RaceFrameRun) and the
           nearest car's label (FxTextNotice: $273C bit 0, the car $2648,
           its offset in the view's axes $264A/$264C) are drawn after the
           tick's objects have run (their handlers, FxNearestCar), so a
           tick's picture has the end's */
        memcpy(s + 0x2648, kept_s + 0x2648, 6);
        memcpy(s + 0x2732, kept_s + 0x2732, 12);
    }

    /* the views' records: player 1's car ($0346), and player 2's ($034A)
       in a two-player race */
    between_view(w, 0x0346, num, den);
    if (two)
        between_view(w, 0x034A, num, den);
    /* the objects: positions +$0C/+$0E/+$10, angles +$14/+$15/+$16 */
    for (k = 0; k < 0x136; k++) {
        unsigned o = 0x2000 + 1 + k * 0x62;
        for (i = 0; i < 3; i++)
            between16(w, kept_w, was_w, o + 0x0C + 2 * i, num, den, 0);
        for (i = 0; i < 3; i++)
            between8(w, kept_w, was_w, o + 0x14 + i, num, den);
    }
    /* the Super FX records (the list at $2332, by +$04) and a car's parts
       (+$3E, by +$34): positions +$1E/+$20/+$22, angles +$24/+$26/+$28 */
    for (rec = word(s, 0x2332), n = 0; rec && n < 96; rec = word(s, rec + 0x04), n++) {
        unsigned part;
        int m;
        for (i = 0; i < 3; i++)
            between16(s, kept_s, was_s, rec + 0x1E + 2 * i, num, den, 0);
        for (i = 0; i < 3; i++)
            between16(s, kept_s, was_s, rec + 0x24 + 2 * i, num, den, 1);
        for (part = word(s, rec + 0x3E), m = 0; part >= 0x1000 && part < 0x2400 && m < 8; part = word(s, part + 0x34), m++)
            for (i = 0; i < 3; i++)
                between16(s, kept_s, was_s, part + 0x1E + 2 * i, num, den, 0);
    }

    nshvc_hold_interrupts(1);
    rm_tween_start_side = 2 * num < den;
    drew = rm_redraw_run(two ? RmTweenDrawTwoPlayers : RmTweenDraw);
    rm_tween_start_side = 0;
    rm_tween_step = 0;  /* a redraw: no port, no wait (remaster.h) */
    nshvc_hold_interrupts(0);
    /* one player: the buffer drawn in (its start at $021A, the one the
       Super FX draws in next); two players: both, one player's each */
    mask = !drew ? 0 : two ? 3 : word(s, 0x021A) == 0x2C00 ? 1 : 2;
    if (mask & 1)
        memcpy(at_2c00, s + 0x2C00, RM_TWEEN_BYTES);
    if (mask & 2)
        memcpy(at_9400, s + 0x9400, RM_TWEEN_BYTES);

    memcpy(w, kept_w, sizeof kept_w);
    memcpy(s, kept_s, sizeof kept_s);
    cpu = cpu_kept;
    gsu = gsu_kept;
    return mask;
}

int rm_tween(int num, int den, uint8_t *pixels, uint16_t *shown)
{
    int mask;

    if (nshvc_wram()[0x0D62])           /* two players draw two: rm_tween_buffers */
        return 0;
    mask = rm_tween_buffers(num, den, pixels, pixels);
    *shown = mask == 1 ? 0x9400 : 0x2C00;
    return mask != 0;
}
