/*
 * front.h: Stunt Race FX's front end on nativeshvc: its main (main.c), the
 * cartridge (cart.c), the touch controls (touch.c) and their pictures
 * (art.c). The console's calls it makes are wrapped here (front_*) so the
 * touch scheme can be checked on its own (touch_check.c).
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

/* ---- the cartridge (cart.c) and its Super FX (superfx_cart.c) ---- */
void cart_describe(nshvc_cart *cart);
/* the Super FX on the connector (cart.h) */
extern const struct nshvc_chip front_superfx;
/* a ROM read for the Super FX's ROM buffer's fill (with NSHVC_ROM_CHIP): a
   byte no asset holds reads 0 and sets front_rom_unheld */
#define FRONT_ROM_FETCH NSHVC_ROM_CHIP_OWN
extern int front_rom_unheld;

#endif
