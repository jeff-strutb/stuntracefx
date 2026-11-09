/*
 * handlers.h: the object handlers, by the numbers the objects and the
 * course data store: the bank is the high byte of the record's +$1C. Handlers
 * named here without a bank are in bank $08.
 */
#ifndef STUNTRACEFX_HANDLERS_H
#define STUNTRACEFX_HANDLERS_H

enum {
    MODEL_COMMON = 0xA158,              /* the model the spawned effects use */

    H_CAR_FRAME = 0xB23D,
    H_CAR_POSE_STEP = 0xB2DE,
    H_SPAWNED_D460 = 0xD460,
    H_SPARKLE_START = 0xEA6B,
    H_SPARKLE_STEP = 0xEB07,
    H_TRAIL_START = 0xE919,
    H_TRAIL_STEP = 0xE934,
    H_ATTACHED_START = 0xEBA3,
    H_ATTACHED_STEP = 0xEBB9,
    H_TIMED_STEP = 0xEC8B,
    H_RELEASE = 0xF927,
    H_RECORD_MARK_STEP = 0xCDA4,
    H_SPIN_RECORD = 0xCDD0,
    H_SPIN_STEP = 0xCDDE,
    H_CAMERA_DRIFT = 0x91EB,
    H_CAMERA_RISE = 0x93D7,
    H_CAMERA_DRIFT_LONG = 0x92DF,
    H_CAMERA_ORBIT = 0x94A1,
    H_CAMERA_STILL = 0x951B
};

#endif
