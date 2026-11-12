/*
 * fx_weather.h: the race's rain and snow drawn at the window's resolution
 * and across the whole view (fx/fx_weather.c).
 */
#ifndef STUNTRACEFX_FX_WEATHER_H
#define STUNTRACEFX_FX_WEATHER_H

#include <stdint.h>

/* FxWeatherDraw's drawing of its drops (render/frame.c): begun with the
   weather (1 rain, 2 snow); then each drop the chip projects, given its
   place in the drops' space before this picture's move (x, y, z) and
   after it (nx, ny, nz); then ended. While it runs the chip's own plots of
   the drops draw nothing (fx_weather_drawing). */
void fx_weather_begin(int weather);
void fx_weather_drop(int16_t x, int16_t y, uint16_t z, int16_t nx, int16_t ny, uint16_t nz);
void fx_weather_end(void);
int fx_weather_drawing(void);

#endif
