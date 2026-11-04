/*
 * front.h: Stunt Race FX's front end on nativeshvc: its main (main.c), the
 * cartridge (cart.c), the touch controls (touch.c) and their pictures
 * (art.c), and the pause screen of the hosts with a keyboard (pause.c).
 * The console's calls the touch scheme makes are wrapped here (front_*) so
 * it can be checked on its own (touch_check.c).
 */
#ifndef SRFX_FRONT_H
#define SRFX_FRONT_H

#include <stdint.h>

#include "nshvc/nshvc.h"

/* ---- the console, as the touch scheme sees it ---- */
uint32_t front_frames(void);                    /* nshvc_frames */
void front_window(float *w, float *h);          /* nshvc_window_size, points */
float front_safe_top(void);                     /* nshvc_safe_area's top */
void front_buttons(uint16_t buttons);           /* nshvc_game_buttons(0, ...) */
int front_auto_accelerate(void);                /* the Remaster option */
/* the mode: 1 Enhanced, 0 Original (Tab, and the pause screen's button) */
int front_enhanced(void);
void front_set_enhanced(int on);
int front_overlay_image(const uint32_t *argb, int w, int h);       /* nshvc_overlay_image */
void front_overlay_place(int id, float cx, float cy, float scale, float angle, float alpha,
                         int visible);                             /* nshvc_overlay_place */

/* ---- the touch scheme (touch.c) ---- */
/* the touches that changed (the console's touch hook) */
void touch_input(const nshvc_touch *t, int n);
/* whether the player is driving now: the wheel and pedals, else the
   menus' swipes and taps; once a frame */
void touch_set_driving(int driving);
/* what to draw: the controls' places and states, in window points */
typedef struct {
    int driving, wheel, brake, boost;
    float wheel_x, wheel_y, wheel_angle;
    float brake_x, brake_y, boost_x, boost_y, pause_x, pause_y;
} TouchView;
void touch_view(TouchView *v);

/* ---- the controls' pictures (art.c) ---- */
/* made once and handed to the console's overlay; then each frame placed
   from touch_view */
void art_make(void);
void art_place(const TouchView *v);
/* the mode's badge (every host): made once, then placed each frame with
   its top-left corner at (left, top), in window points; shown for three
   seconds at launch and after each change of mode, then faded out */
void art_make_mode(void);
void art_place_mode(int enhanced, float left, float top);
/* the badge hidden (while paused) */
void art_hide_mode(void);

/* ---- the pause screen (pause.c): the hosts with a keyboard ---- */
/* the key hook's and the pointer hook's share: 1 taken */
int pause_key(int vk, int down);
int pause_pointer(float x, float y, int buttons);
/* once a frame: the keys laid out at the first, the pictures placed */
void pause_frame(void);
/* 1 while paused (the console's machine still) */
int pause_active(void);
/* where the mode's badge goes: right of the pause button, window points */
float pause_badge_left(void);
/* for a check (pause_check.c): what is at a panel point (a control's
   index in the panel's order, 100 RESUME, 101 DEFAULT KEYS, -1 none), the
   control asked for (-1 none), keys the front end keeps, a button's key
   changed as the panel changes it, the keys kept and laid out again, the
   pictures (0 pause, 1 play, 2 the panel) */
int pause_hit(float x, float y);
int pause_asking(void);
int pause_key_reserved(int vk);
void pause_rebind(int i, int vk);
void pause_keys_save(void);
void pause_keys_load(void);
uint32_t *pause_picture(int which, int *w, int *h);
/* the controller (pad_art.c), drawn from its vector drawing into px (w x h,
   ARGB straight alpha) at `density` pixels a point, k points a drawing
   pixel, the drawing's (500, 300) at (cx, cy) points */
void pad_draw(uint32_t *px, int w, int h, float density, float k, float cx, float cy);

/* ---- the cartridge (cart.c) and its Super FX (superfx_cart.c) ---- */
void cart_describe(nshvc_cart *cart);
/* the Super FX on the connector (cart.h) */
extern const struct nshvc_chip front_superfx;
/* a ROM read for the Super FX's ROM buffer's fill (with NSHVC_ROM_CHIP): a
   byte no asset holds reads 0 and sets front_rom_unheld */
#define FRONT_ROM_FETCH NSHVC_ROM_CHIP_OWN
extern int front_rom_unheld;

#endif
