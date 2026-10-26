/*
 * sprite_slots.h: the sprite slots (video/sprite_slots.c), the records
 * after the work-RAM sprite table that the race's sprites are drawn from.
 * Each routine takes what it needs and returns what its caller uses; the
 * work they do is in work RAM.
 */
#ifndef SRFX_SPRITE_SLOTS_H
#define SRFX_SPRITE_SLOTS_H

#include <stdbool.h>
#include <stdint.h>

/* The first slot in use, by the lowest bit of the mask; its address is
   stored as the first slot. carry is the carry on entry: it goes into the
   first shift. Returns the address, or 0 when no slot is in use. */
uint16_t sprite_slot_first(bool carry);
uint16_t sprite_slot_first_long(bool carry);   /* the same, for callers in other banks */

/* Runs slot 1's countdown one step; returns the last value loaded. */
uint8_t sprite_slot1_countdown(void);

/* Shows slot 2 and brightens it a step; returns its new level. */
uint8_t sprite_slot2_brighten(void);
uint8_t sprite_slot2_brighten_long(void);

/* Shows slot 2 and darkens it a step; returns its new level, or the new
   mask when the slot has gone dark. */
uint8_t sprite_slot2_darken(void);
uint8_t sprite_slot2_darken_long(void);

#endif
