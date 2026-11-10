/*
 * object_tables.h: the objects' data from the cartridge, as typed data
 * (plan section 3, Data). Where each table is in the cartridge is written
 * once, in object_tables.c; object/ reads them only through these.
 */
#ifndef STUNTRACEFX_OBJECT_TABLES_H
#define STUNTRACEFX_OBJECT_TABLES_H

#include <stdint.h>

#include "data/ref.h"

/* ---- the course scripts (object/course_script.c) ----
   A script is a run of entries, each its command byte, then its fields.
   `entry` is a reference to one: a script's start (race_tables'
   course_script, course_menu_script), then course_script_next. Reads keep
   the original's 16-bit address sums. */
CartRef course_script_next(CartRef entry, unsigned bytes);      /* the place `bytes` on */
CartRef course_script_restart(CartRef entry);                   /* a restart's: the first entry of the script's bank */
uint8_t course_script_byte(CartRef entry, unsigned field);
uint16_t course_script_word(CartRef entry, unsigned field);

/* ---- the model records: 28 bytes each, an object's script word names one ---- */
uint16_t model_record_touch(uint16_t record, int axis);         /* its touch box, half its size along x, y, z (axis 0-2) */

/* ---- bank $09's object data: entries (a child's, an attachment's) and
   lists (a car's attachments, its camera points), each named by the data
   by its offset, the field offsets object.h's DATA_* ---- */
uint16_t object_entry_word(uint16_t entry, unsigned field);
uint8_t object_list_byte(uint16_t at);
uint16_t object_list_word(uint16_t at);

/* ---- small tables ---- */
uint8_t facing_frame_32(uint8_t step);          /* a sprite's frame by its turn to the camera, in 32 steps */
uint8_t facing_frame_64(uint8_t step);          /* the same in 64 steps */
uint8_t jump_rise(uint16_t timer);              /* a car's rise by its jump timer (ObjectMoveForward) */
/* an eye's looks through a blink, by its step (1-4; the table's first word
   is not read): plain, half, shut, half, plain; to startled: plain, half,
   shut, half startled, startled; back to plain: startled, half startled,
   shut, half, plain; startled: startled, half startled, shut, half
   startled, startled */
typedef enum { EYE_BLINK_PLAIN, EYE_BLINK_TO_STARTLED, EYE_BLINK_TO_PLAIN, EYE_BLINK_STARTLED } EyeBlink;
uint16_t eye_blink_look(EyeBlink blink, uint8_t step);

/* ---- the camera paths: commands, each path named by its offset (object.h's PATH_*) ---- */
uint8_t path_byte(uint16_t at);
uint16_t path_word(uint16_t at);

/* ---- a course's track table, which the course script's end leaves for
   the Super FX (its reference back from the chip's RAM: cart_ref) ---- */
uint16_t track_table_last(CartRef table);                               /* its last entry's number */
void track_table_place(CartRef table, uint16_t entry, uint16_t out[3]); /* entry's (from 1) place */

/* ---- the trigger areas: rectangles of map cells, data in the block of
   code the original copies to work RAM; the data names an area by its
   place in that block ---- */
typedef uint16_t TriggerAreaRef;
typedef struct {
    uint8_t cols, rows;                         /* the rectangle's cells across and down */
    uint8_t left;                               /* its cells left of the window's origin */
    uint8_t top;                                /* its rows above it, a signed byte */
    const uint8_t *cells;                       /* cols * rows cell codes, a row at a time */
} TriggerArea;
typedef enum { TRIGGER_FIXED_SPLIT, TRIGGER_FIXED_SINGLE } TriggerFixed;
TriggerAreaRef trigger_area_for_height(uint16_t at);   /* by the view's height; at: a byte offset into its word table */
TriggerAreaRef trigger_area_fixed(TriggerFixed which); /* the split screen's (or with course flag bit 7) or the other */
TriggerArea trigger_area(TriggerAreaRef area);

/* ---- what the objects name in the cartridge, by meaning (plan section 3):
   each id resolves to the word a record keeps, the original's, which the
   Super FX's code and the recordings read ---- */

/* models: the model table's records (28 bytes each) */
typedef enum {
    MODEL_NONE,                      /* none: drawn by its record or not at all */
    MODEL_FIREWORK_SPARK,            /* a firework shell's sparks */
    MODEL_LAMP_POST_LIGHT,           /* a lamp post's second light */
    MODEL_LAMP_TOWER,
    MODEL_LAMP_TOWER_LIGHT,          /* a lamp tower's second light */
    MODEL_COMMON,                    /* the sprite the effects use, their look choosing the picture */
    MODEL_BREAK_OFF_PIECE,           /* a piece a knock breaks off a car's child */
    MODEL_TRUCK_WHEEL,               /* the bonus truck's wheels */
    MODEL_BURST_PIECE,               /* a burst's four pieces, this and the next three */
    MODEL_PLAYER_AT_GO,              /* the player's car's object once the pace car's signal says go */
    MODEL_SIGNAL_LIGHT_COUNTING,     /* the signal's light while it counts down */
    MODEL_SIGNAL_LIGHT,              /* the pace car's signal light */
    MODEL_BLINKER,                   /* a blinker's light */
    MODEL_BALL,                      /* the sky, cannon and wave balls */
    MODEL_BREAKABLE_PIECE,           /* a breakable's pieces, two kinds */
    MODEL_BREAKABLE_PIECE_2,
    MODEL_FAN_LIT,                   /* a fan over the lit part of its turn */
    MODEL_WHEEL_LIT,                 /* a wheel the same */
    MODEL_SHARDS,                    /* ObjectShatter's six shards, this and the next five */
    MODEL_SHARDS_2,                  /* ObjectShatter2's six */
    MODEL_RAIL_RISING,
    MODEL_RAIL_DROPPING,
    MODEL_FLOATER,
    MODEL_FINALE_BREAKABLE,
    MODEL_BONUS_5,                   /* a bonus marker's figure at the player, by its bonus */
    MODEL_BONUS_2,
    MODEL_FIREWORK_SHELL,
    MODEL_SCORE_COUNTER_ON,          /* the score counter from the second lap */
    MODEL_CROWD,                     /* a lap's crowd */
    MODEL_RAISE_BALL,                /* a ball that raises a car's record (+$6A); a sky ball drops one */
    MODEL_LOWER_BALL,                /* one that lowers it (+$82) */
    MODEL_POP_MARKER,
} ObjectModel;
uint16_t model_word(ObjectModel model);
/* the model k records on: a run the code takes in order (a burst's four
   pieces, a shatter's six shards) */
uint16_t model_word_after(ObjectModel first, int k);
/* the part (0-5) a crashing car sheds, by its set (CarCrashParts: the
   F-Type's, the Coupe's, the 4WD's, the 2WD's) */
uint16_t crash_part_model_word(int set, int part);
uint16_t ring_part_model_word(int part);              /* the ring's four parts */
/* whether a record's model word names a place in the cartridge (a bare
   object's is 0) */
int model_word_in_cartridge(uint16_t word);

/* looks: an object's animation word (+$5D), where its pictures are listed */
typedef enum {
    LOOK_CAR_F_TYPE,                 /* a car's far looks, by its kind (the menu's order) */
    LOOK_CAR_COUPE,
    LOOK_CAR_4WD,
    LOOK_CAR_2WD,
    LOOK_FIREWORK_SPARK,
    LOOK_LANTERN,
    LOOK_LAMP_LIGHT,                 /* a lamp's light */
    LOOK_LAMP_LIGHT_RAISED,          /* a lamp's light raised */
    LOOK_STAR_SPARKLE,
    LOOK_BREAK_OFF_A,                /* the pieces' looks, by the routine that breaks them off */
    LOOK_SPARKLE,
    LOOK_EYE_PLAIN,                  /* a car's eyes: plain */
    LOOK_EYE_STARTLED,               /* startled: more white, the pupil small */
    LOOK_BREAK_OFF_H,
    LOOK_BREAK_OFF_G,
    LOOK_BREAK_OFF_E_FLAT,           /* ObjectBreakOffE's piece once it lies flat */
    LOOK_BREAK_OFF_E,
    LOOK_BREAK_OFF_D,
    LOOK_BREAK_OFF_B,
    LOOK_PUFF,
    LOOK_WHEEL,                      /* a car's tyres and the bonus truck's wheels */
} ObjectLook;
uint16_t look_word(ObjectLook look);
/* a look word's split screen form: the list after it, the two players' pictures */
uint16_t look_word_split(uint16_t look);
/* a tilted billboard's look: the word `tilt` bytes (a multiple of 4, -8 to
   8) from the middle of its list */
uint16_t look_word_tilted(uint16_t look, int8_t tilt);

/* the records objects name in their +$20 (an object's script word): its
   shape and offsets */
typedef enum {
    OBJECT_SCRIPT_SIGNAL,            /* the pace car's signal */
    OBJECT_SCRIPT_BLINKER,
    OBJECT_SCRIPT_MARKER,            /* the count marker's and the switch lamps' */
} ObjectScript;
uint16_t object_script_word(ObjectScript script);

/* the car scripts (an object's +$27 for its car record, FxCarNew); the
   first four in the menu's order */
typedef enum {
    CAR_F_TYPE, CAR_COUPE, CAR_4WD, CAR_2WD,
    CAR_RIVAL_F_TYPE, CAR_RIVAL_COUPE, CAR_RIVAL_4WD, CAR_RIVAL_2WD,
    CAR_BONUS_TRUCK
} CarScript;
uint16_t car_script_word(CarScript script);

#endif
