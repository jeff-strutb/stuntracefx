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
uint16_t model_record_offset(uint16_t record, int axis);        /* its offsets x, y, z (axis 0-2) */

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

#endif
