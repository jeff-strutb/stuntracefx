/*
 * chip.h: the Super FX's work as the game's code calls it. Its programs are
 * ordinary functions in fx/; the inputs and results the main CPU exchanged
 * with them are words in the chip's RAM, read and written here.
 */
#ifndef STUNTRACEFX_CHIP_H
#define STUNTRACEFX_CHIP_H

#include <stdint.h>

#include "fx/fx.h"

/* The main CPU's view of the chip's RAM: plain words, little-endian. */
static inline uint16_t fx16(uint16_t a) { return (uint16_t)(fx_ram[a] | fx_ram[(uint16_t)(a + 1)] << 8); }
static inline void fx16_set(uint16_t a, uint16_t v)
{
    fx_ram[a] = (uint8_t)v;
    fx_ram[(uint16_t)(a + 1)] = (uint8_t)(v >> 8);
}
static inline uint8_t fx8(uint16_t a) { return fx_ram[a]; }
static inline void fx8_set(uint16_t a, uint8_t v) { fx_ram[a] = v; }

/* The chip's parameter words, named for what the main CPU puts there. */
enum {
    FXIN_VECTOR_X = 0x0068,             /* a vector's first word; also a program's input */
    FXIN_VECTOR_Y = 0x002C,
    FXIN_VECTOR_Z = 0x002E,
    FXIN_LENGTH_RESULT = 0x006A,        /* the length program's ($01:80BC) result */
    FXIN_ANGLE_RESULT = 0x0040,         /* the program $01:81E3's result */
    FXIN_ROTATED = 0x0026,              /* $01:82CE's three results */
    FXIN_ANGLES = 0x0020,               /* $01:8325's three angles */
    FXIN_VIEW_CENTRE_X = 0x0034,
    FXIN_VIEW_CENTRE_Y = 0x0036,
    FXIN_VIEW_LEFT = 0x0038,
    FXIN_VIEW_RIGHT = 0x003A,
    FXIN_VIEW_TOP = 0x003C,
    FXIN_VIEW_BOTTOM = 0x003E,
    FXIN_VIEW_SCALE_A = 0x0092,
    FXIN_VIEW_SCALE_B = 0x0094,
    FXIN_VIEW_POS = 0x00D8,             /* $00D8, $00DA, $00DC */
    FXIN_VIEW_ANGLES = 0x00DE,          /* $00DE, $00E0, $00E2 */
    FXIN_CAMERA_FLAGS = 0x2736,
    FXIN_RECORDS = 0x0AAA,              /* the frame's records, 30 bytes each */
    FXIN_RAMP = 0x3C00,                 /* the noise program's eight-word ramp */
    FXIN_EFFECTS_SEED = 0x01D0,         /* the renderer's effects random seed, $1234 once the view is set */
    FXIN_NOT_MENU = 0x108C,             /* 0 for the course menu's view, else the players */
    FXIN_TEXT_NOTICE = 0x2732,          /* 0 no notice, else draw the text */
    FXIN_PLAYER1_RECORD = 0x2476,       /* the record of player 1's car */
    FXIN_PLAYER2_RECORD = 0x2478,
    FXIN_VIEW_RECORD = 0x247A,          /* the record the view follows */
    FXIN_STEP_FRAMES = 0x108E,          /* the frames of the last step, for the renderer */
    FXIN_SHADE_BASE = 0x004E,           /* the faces' colour tables' base (the frame program's shade base) */
    FXIN_COURSE_MODELS = 0x0050,        /* the course's model table in bank 3 (from $03:F799 by course), the model program's */
};

/* The shade bases the screens use: the race's 3D, the HUD's pieces. */
enum { FX_SHADE_BASE_RACE = 0xA545, FX_SHADE_BASE_HUD = 0xA325 };

/* The chip's RAM the main CPU lays out: its two picture buffers (the frame
   program's screen start), and where a packed screen is unpacked. */
enum { FX_PICTURE_0 = 0x2C00, FX_PICTURE_1 = 0x9400, FX_PICTURE_0_CLEARED_END = 0x6000, FX_UNPACK_AREA = 0x3000,
       FX_RESULTS_PICTURE = 0x4000 /* the results' picture, unpacked for the screen's overlay */ };

/* The pause's turning picture: unpacked at FX_UNPACK_AREA (buffer 1) or
   FX_TURN_UNPACK_HIGH (buffer 0), made 4-bit at FX_TURN_PICTURE_LOW or
   FX_TURN_PICTURE_HIGH. */
enum { FX_TURN_UNPACK_HIGH = 0xC000, FX_TURN_PICTURE_LOW = 0x4000, FX_TURN_PICTURE_HIGH = 0xD000 };

/* The view's right and bottom edges, which the HUD's scaled sprites clip
   to. Only GsuParamsFromDp writes them (FXCAR_STEER_BIAS and FXCAR_PARTS,
   at the same numbers, are offsets in a car's record, not these words; the
   sound/fx session measured no job writing $0038-$003F). Interim: they go
   with the renderer's inputs as named fields (the frame inputs in
   FxShared). */
static inline uint16_t chip_view_right(void) { return fx16(FXIN_VIEW_RIGHT); }
static inline uint16_t chip_view_bottom(void) { return fx16(FXIN_VIEW_BOTTOM); }

void GsuParamsUpdate(void);
void GsuParamsFromDp(void);
void GsuRecordsUpload(void);

/* The programs the race frame runs. */
void FramePhysicsAndObjects(void);
void FramePicture(void);
void GsuScreenFlip(void);
void GsuScreenFlipMenu(void);
void GsuScreenToggle(void);
void ViewMatrixUpdateAndSave(void);
void ViewUpdateAndSort(void);
void ViewReset(void);
void ViewDefaults(void);
void NoiseFillWithRamp(void);
void ViewMatrixMake(void);
void ViewRotateVector(uint16_t x, uint16_t y, uint16_t z);
uint16_t VectorAngle(uint16_t x, uint16_t y);
uint16_t VectorLength(uint16_t x, uint16_t z);

#endif
