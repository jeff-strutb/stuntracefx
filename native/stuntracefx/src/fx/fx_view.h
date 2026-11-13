/*
 * fx_view.h: a picture drawn from another camera than the game's (the
 * race's overhead view, race/overhead.c): what the drawing takes from it
 * besides the place, angles and window words the chip already reads.
 *
 * A picture between steps sets it before its frame program and clears it
 * after (fx/fx_tween.c); with none set every function here gives the
 * game's own drawing, so the step's own pictures, the course menu's race
 * and two players' halves are as they were.
 */
#ifndef STUNTRACEFX_FX_VIEW_H
#define STUNTRACEFX_FX_VIEW_H

#include <stdint.h>

/* The chip's light, in view space as a 1.15 vector (FxModelDraw): the
   game's own, at a level view's (0, $393E, $727C). */
enum { FX_LIGHT_GAME_X = 0x0000, FX_LIGHT_GAME_Y = 0x393E, FX_LIGHT_GAME_Z = 0x727C };

typedef struct {
    /* The 3D magnified about the view's centre on the screen: the same as a
       focal length of 128 * zoom (the chip projects at 128), with the
       chip's window words cut to the part of the view the screen shows. */
    float zoom;
    /* the light in this view's space, 1.15 words */
    int16_t light[3];
    /* an object's depth times this for its model's detail and its haze
       tier: the view's target at the game camera's distance from it, so
       what it shows has the detail and haze the game's view gives its car */
    float depth_scale;
    /* the view's place in the world, whole: it may lie past the 16 bits
       the game's places wrap at (g's view place is its low 16 bits, which
       a near record's place from it, in 16 bits, needs no more of) */
    int32_t eye[3];
} FxView;

void fx_view_set(const FxView *v);
void fx_view_clear(void);
int fx_view_on(void);
float fx_view_zoom(void);
/* the light's word on axis 0, 1, 2 (X, Y, Z), as FxModelDraw stores it */
uint16_t fx_view_light(int axis);
/* the light as a vector of doubles (unit length, as the chip's words) */
void fx_view_light_vector(double l[3]);
/* a depth for a model's detail and haze tier */
uint16_t fx_view_depth(uint16_t depth);
/* while a view is set: 1 and its place in the world, whole; else 0 */
int fx_view_eye(int32_t eye[3]);

#endif
