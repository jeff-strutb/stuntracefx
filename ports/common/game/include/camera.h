/*
 * camera.h: the race camera (race/camera.c).
 */
#ifndef SRFX_CAMERA_H
#define SRFX_CAMERA_H

#include <stdint.h>

/* One frame of the camera record at work RAM address `cam`, following its
   target object. `caller_y` is the Y register the caller leaves: read only
   when the record's bits 8 and 3 are both set. */
void camera_follow_step(uint16_t cam, uint16_t caller_y);

#endif
