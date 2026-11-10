/*
 * menu_tables.h: the cartridge's tables the menus read, as typed data
 * (plan section 3, Data). Each table's place in the cartridge image is
 * written once, in menu_tables.c; the menus see what each holds.
 */
#ifndef STUNTRACEFX_DATA_MENU_TABLES_H
#define STUNTRACEFX_DATA_MENU_TABLES_H

#include <stdint.h>

/* --- the title and the selection ---------------------------------------- */

int8_t menu_sine(uint8_t angle);                        /* the signed sine, a byte an angle */
uint16_t menu_button_sequence(int step);                /* the title's button sequence: the button each step takes */
void menu_pair_animations(int pair, uint16_t *first, uint16_t *second);   /* for the objects of kinds 3 and 5 */
uint16_t menu_choice_repeat_model(int choice);          /* player 2's model when both players choose the same */
uint16_t menu_choice_step(int right, int choice);       /* the choice a left (0) or right (1) press steps to */
uint16_t menu_saved_choice(int saved);                  /* a choice as the save keeps it (2 bits) as the choice's word */
uint16_t menu_initial_tile(int letter);                 /* an initial's tile, A-Z */
uint8_t menu_car_setting_row(int choice, int combination, int k);  /* a car's setting row k (0-2) by the three settings' combination */
uint8_t menu_selection_row(int choice);                 /* player 1's choice's setting row */
uint8_t menu_selection_model(int choice, int row);      /* a racer's model by its choice and setting row */
uint16_t menu_setting_animation(int setting, int row);  /* the animation that shows a setting's value */
uint16_t menu_setting_object(int i);                    /* the kind of the object that shows setting i */

#endif
