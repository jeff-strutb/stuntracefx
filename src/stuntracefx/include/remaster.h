/*
 * remaster.h: the Remaster Options the game's C asks about (game/remaster:
 * options.c keeps them, remaster.c applies them). Off, the game is the
 * original's, exactly.
 */
#ifndef SRFX_REMASTER_H
#define SRFX_REMASTER_H

#include <stdint.h>
#include "remaster.h"

/* Frame rate 60: a game frame each video frame, the simulation's steps a
   tick each, RM_TICKS of them to the original's one: the original's race
   frames took 4 to 9 video frames, 6 on average */
#define RM_TICKS 6
int rm_tick60(void);
/* Enhanced mode now (Frame rate 60 on, not in the attract's demos): the
   game's pace independent of the chips' speed, so the Super FX's programs
   take no time (superfx_cart.c) */
int rm_enhanced(void);
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
/* The race's HUD (RaceSoundsAndHud, 07:DAA7, in place of its call in
   RaceFrameRun): most of it is drawn into the Super FX's buffer, which the
   3D program clears each frame, by routines that also do its counting. At
   60 it runs on the step's tick as the original's frame did, the work RAM
   it started from kept; each other tick, and each in-between frame
   (tween.c), runs it again from that RAM into the buffer being drawn and
   puts everything back but the pixels (rm_hud_redraw), so the HUD looks
   as its step drew it on every frame shown. */
void RmRaceHud(void);
void rm_hud_redraw(void);
/* the same with the interrupts already held by the caller (tween.c) */
void rm_hud_redraw_in(int hold);
/* set by tween.c while it draws an in-between frame on the start's side
   (before half way): the HUD as the tick's own picture had it, drawn even
   when that picture's redraw reached the HUD's own loop (TIME UP), after
   which the redraws stop */
extern int rm_tween_start_side;
/* the Super FX's RAM at the tick's end, while tween.c draws: what the
   HUD's redraw reads, as the tick's own redraw after its objects did */
const uint8_t *rm_tween_end_sram(void);
/* Two players: the state at the tick's RaceSoundsPlayer1, after player
   1's picture and before the step's stats that player 2's picture shows
   (race_frame.c), kept for the tick and the last; and, while tween.c
   draws from one, whether that tick was a step's (RmTweenDrawTwoPlayers
   then runs RaceSoundsPlayer1 between the pictures) */
void rm_tween_pictures_drawn(void);
extern int rm_tween_step;
/* draw() run as a redraw (rm_redrawing holds): 0 when it reached a wait
   for a frame and was let go there, the state to be put back by the
   caller */
int rm_redraw_run(void (*draw)(void));
/* Whether a redraw is running: the step's work is done again (rm_step_due
   holds), the soft reset and engine sounds are not, nor anything that
   reaches the APU's ports */
int rm_redrawing(void);
/* every wait of the game's comes here first: a redraw (an in-between
   frame) that reaches a wait is let go (waits.h) */
void rm_wait_point(void);
/* rm_step_due for work whose original steps were k video frames: the
   menus' 3D screens (RaceFrameStep's) took 4 */
int rm_step_due_every(unsigned k);
#define RM_MENU_TICKS 4
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
#define RM_CAM_5_64 4236            /* c = 5/64 */
#define RM_CAM_5_128 4164           /* c = 5/128 */
#define RM_DECAY_32 4150            /* c = 1/32 */
#define RM_DECAY_64 4123            /* c = 1/64 */
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
/* rm_race_frame for a frame its caller paces itself (the menus' 3D
   screens wait for the vertical blank in their own loop): untimed, with
   no wait of its own */
void rm_race_frame_paced(void);
/* The step is over though the race frame runs on: its code waits for
   the vertical blank itself (the pause, the end's fades) or leaves the
   race (the results and the screens they open). From here it runs frames
   of its own on the console's time; the next race frame is a tick again. */
void rm_step_over(void);
/* the options taken, at each frame's end (the front end calls it); the
   main CPU's costs set for the mode (Enhanced: none, contract 2.2) */
void rm_frame_end(void);
/* The raster interrupt's handler runs (system/boot.c): the main CPU's
   costs on while it does, in Enhanced too, so its writes to the PPU land
   on the lines the original's did (the race screen's top and bottom, its
   windows); the game's frame work stays free */
void rm_irq_enter(void);
void rm_irq_leave(void);
/* the finished screen to the display at once: video/dma.c */
void RmRaceScreenShow(void);
/* Enhanced mode's picture (picture.c): count characters of planar data at
   bpp bits a pixel decoded to colour indices, each an 8 * scale square
   block, a pixel repeated scale x scale, into out (count * 64 * scale *
   scale bytes) */
void rm_picture_chars(const uint8_t *data, unsigned count, unsigned bpp, unsigned scale, uint8_t *out);
/* The console's canvas (nativeshvc 2.4) in BG1's characters' place
   (picture.c). rm_canvas_ready: Enhanced, and the console has one (the
   screen's uploads then leave the Super FX's picture out of VRAM).
   rm_canvas_show: count 8 bpp characters of Super FX RAM at src stand in
   for those at VRAM word `word`, at the window's resolution.
   rm_canvas_copy: an upload the canvas stands in for, rows of `bytes`
   from Super FX RAM at src, `stride` apart, kept as VRAM would hold them
   from word `word` on; rm_canvas_show_copied: count characters of bpp
   bits a pixel kept so from word `word` on the canvas.
   rm_canvas_narrow: the canvas kept for its characters from VRAM word
   `word`, count of them, VRAM's the rest. rm_canvas_off: VRAM's
   characters again. rm_canvas_kept: owner's uploads (the race
   screen's, the menus' 3D picture's) ran this frame; a frame without the
   uploads of the screen that handed the canvas turns it off. rm_canvas_frame_end: at each frame's end, the canvas off when the
   race screen has gone, handed again when the window's k changed. */
int rm_canvas_ready(void);
void rm_canvas_show(unsigned word, uint16_t src, unsigned count);
/* the same from bytes in buffer order; and a two-player set (360 4 bpp
   characters) from the players' buffers, 18 columns of $140 bytes $200
   apart each, in the shown set's place */
void rm_canvas_show_bytes(unsigned word, const uint8_t *src, unsigned count);
void rm_canvas_show_players(const uint8_t *p1, const uint8_t *p2);
/* a one-player picture (416 8 bpp characters) in place of the race
   buffer the canvas shows */
void rm_canvas_replace(const uint8_t *src);
void rm_canvas_copy(unsigned word, uint16_t src, unsigned bytes, unsigned stride, unsigned rows);
void rm_canvas_show_copied(unsigned word, unsigned count, unsigned bpp);
/* the same a step late, for a two-player race in whole steps (a step's
   pictures are its end's, so the frames between show the way from the
   last step's to this one's): the last set's pictures in this one's
   place, this one's kept for the next */
void rm_canvas_show_copied_late(unsigned word, unsigned count, unsigned bpp);
/* rm_canvas_show_copied unless the canvas already shows just that */
void rm_canvas_show_copied_once(unsigned word, unsigned count, unsigned bpp);
void rm_canvas_narrow(unsigned word, unsigned count);
void rm_canvas_off(void);
enum { RM_CANVAS_RACE = 1, RM_CANVAS_MENU = 2 };
void rm_canvas_kept(int owner);
void rm_canvas_frame_end(void);
/* Enhanced's view distance (object/draw_list.c, render/frame.c): the
   far records in the Super FX's second bank ($71): the count, then each
   a draw record (30 bytes, as the near ones at $7E:F77F) and its place
   from the view in 32 bits (X, Y, Z at +RM_FAR_PLACE). The Super FX's
   frame program takes them, turns them to the view, and draws those in
   view behind the near ones, each farther than 16 bits hold scaled down
   by rm_far_shift halvings (its place, its model's matrix, its sprite's
   size), which the drawing reads while it draws that object. */
#define RM_FAR_COUNT   0x0000u
#define RM_FAR_RECORDS 0x0100u
#define RM_FAR_SIZE    0x30u
#define RM_FAR_PLACE   0x20u
#define RM_FAR_LIFT    0x2Cu        /* 1: the key lifted by $1F40, as a near record's */
#define RM_FAR_MAX     0x0136u
#define RM_FAR_KEEP    0x8000u      /* the first bank's work RAM kept while the far ones draw */
extern int rm_far_shift;
extern uint16_t rm_far_depth;    /* its true depth, at most $7FFF: its shade and its model by distance */
void rm_far_draw(void);

/* the menus' 3D picture to the display at once: system/vblank_updates.c */
void RmMenuPictureShow(void);

/* The in-between frames (game/remaster/tween.c): the state at a race tick's
   start kept (one player or two) (RaceFrameRun); at each frame's end whether the
   frame shown was such a tick; and the 3D drawn again at num/den of the way
   from the tick's start to its end, the state put back after (the
   runtime shows what it drew in place of the shown buffer's bytes in
   VRAM; untimed, interrupts held) */
void rm_tween_tick_start(void);
void rm_tween_frame_end(void);
int rm_tween_ready(void);
#define RM_TWEEN_BYTES 0x6800    /* a race screen buffer: 208 x 128 pixels as characters */
/* draws it into the buffer the Super FX draws in next, gives its bytes and
   the start of the buffer shown ($2C00 or $9400); 0 when not ready */
int rm_tween(int num, int den, uint8_t *pixels, uint16_t *shown);
/* the same for one or two players: the buffers drawn into at_2c00 (the
   buffer at $2C00) and at_9400 ($9400), RM_TWEEN_BYTES each; the mask of
   those drawn (1 $2C00, 2 $9400; a two-player race draws both, player 1
   in the buffer $021A gives at the tick's start), 0 when not ready */
int rm_tween_buffers(int num, int den, uint8_t *at_2c00, uint8_t *at_9400);
/* the backdrop (BG2) as the last rm_tween_buffers drew it, shown with its
   picture; and the game's own back before the next step */
void rm_tween_backdrop_show(void);
void rm_tween_backdrop_back(void);
/* two players: the last step's backdrop on the step's own frame, as its
   pictures are (at the race frame's end) */
void rm_tween_backdrop_late(void);
/* whether the option shares the steps out (Frame rate 60, not the
   reference pace) */
int rm_sharing_on(void);
/* Enhanced's race: the original's whole steps, the 3D drawn between them
   (remaster.c) */
int rm_whole_steps(void);
/* whether the S-CPU is in a race frame's wait for its tick (the frame
   hook's clean point for an in-between draw) */
int rm_in_tick_wait(void);

#endif
