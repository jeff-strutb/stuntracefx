/*
 * glue.h: the S-CPU's side of the Super FX (gsu/glue.c, gsu/fx_run.c).
 */
#ifndef SRFX_GLUE_H
#define SRFX_GLUE_H

#include <stdint.h>

int fx_run(uint8_t bank, uint16_t start);
void gsu_run(uint8_t bank, uint16_t start);
void gsu_params_update(void);
void gsu_params_from_dp(void);
void gsu_param_216(uint16_t table, uint16_t n);
void gsu_table_entry_to_1f4c(uint16_t n);
uint16_t gsu_table_entry_next(uint16_t n);
void gsu_inputs_200(uint8_t b1, uint8_t b2, uint8_t b3);
void gsu_records_upload(void);
void gsu_screen_flip(void);
void gsu_screen_toggle(void);

#endif
