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

#endif
