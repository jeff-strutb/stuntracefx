/*
 * fullwindow.h: Enhanced mode's full-window race (remaster/fullwindow.c).
 *
 * In Enhanced mode a one-player race fills the window: the screen is as
 * wide as the window's aspect (nativeshvc's wider screen), BG1 is one
 * canvas (nativeshvc's character canvas) over all of it, and every picture
 * shown is the 3D drawn again (tween.c's redraw, the state kept and put
 * back) with the view over the whole screen at k times the console's
 * pixels (k the canvas's scale for the window): the view's words at W k x
 * 224 k, the focal length at k times, every PLOT into the cartridge's own
 * texels in 16-bit coordinates. The HUD's Super FX pieces are drawn there
 * too, at the console's pixel size (k x k texels a pixel), and the
 * panels' characters over the 3D. Off, everything is the original's.
 */
#ifndef SRFX_FULLWINDOW_H
#define SRFX_FULLWINDOW_H

#include <stdint.h>

/* the full-window race this frame (Enhanced, one player's race screen,
   the console's canvas) */
int fw_on(void);
/* a full-window picture being drawn (the redraw's PLOTs go to the texels);
   fw_hud while its HUD pieces are (the original view's pixels, placed) */
extern int fw_drawing, fw_hud;
/* the picture's size in texels, and k */
int fw_width(void);
int fw_height(void);
int fw_k(void);

/* PLOT and RPIX into the texels at (x, y), with the Super FX's colour and
   plot options (gsu.colr, gsu.por) as PLOT takes them */
void fw_plot(uint16_t x, uint16_t y);
uint8_t fw_rpix(uint16_t x, uint16_t y);

/* GsuParamsFromDp's eight words (the centre, the edges, the frustum
   planes) for the full-window view, given the six the game set: 1 when
   they are the one-player race's and a full-window picture is being
   drawn (out then set) */
int fw_view_words(const uint16_t w[6], uint16_t out[8]);
/* the focal length (view units to texels) and half-width for the
   full-window view (view.c's projection and cull read them) */
int fw_focal(void);
/* the full-window view is in force (its words set, not the HUD's pieces) */
int fw_view_active(void);
int fw_half_width(void);

/* the redraw's start (race_frame.c RmTweenDraw): the full-window view's
   words in the Super FX's RAM (the redraw puts the RAM back after) */
void fw_view_begin(void);
/* the HUD's pieces drawn into a full-window picture from here to the end
   (tween.c's redraw: the text program, RaceSoundsAndHud): the original's
   view words put back for them, their pixels placed (fw_hud) */
void fw_hud_begin(void);
void fw_hud_end(void);
/* the race screen's stages ran (video/dma.c), this frame */
void fw_race_stage(void);

/* a Super FX program begins (1) or ends (0) (the cartridge's board,
   front/superfx_cart.c): the HUD's pieces placed whole */
void fw_gsu_program(int begin);

/* FxDrawList's fill into the texels (raster.c, while fw_drawing) */
void fw_draw_list(void);
/* FxScreenClear's, while fw_drawing: the texels to colour 0 */
void fw_clear(void);

/* A picture j / RM_TICKS of the way through the step just run, drawn and
   handed to the canvas (remaster.c, at a race frame's wait); 0 when no
   picture could be drawn (the last one stays) */
int fw_picture(int j);
/* The race's upload stage (system/raster.c, 7E:A56B): the line of its IRQ
   (208, the original's; in the full window 240, a fallback, the stage run
   from the frame's hook instead: fw_vblank_uploads, at each frame's end,
   first); and at its end, in the full window, the sprites and BG1 set for
   it and the screen on again */
int fw_upload_line(void);
/* a race frame's wait over, its step to run (remaster.c): the work RAM the
   full window borrowed for its own HDMA given back */
void fw_before_step(void);
void fw_vblank_uploads(void);
void fw_upload_done(void);
/* at each frame's end: the console's screen and BG1 set for the mode */
void fw_frame_end(void);

#endif
