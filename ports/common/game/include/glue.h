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

/* gsu/programs.c */
void gsu_unpack_to(uint16_t in68, uint16_t in6a, uint16_t dest);
uint16_t gsu_object_length(uint16_t obj);
void gsu_vector_turn(void);
uint16_t gsu_vector_angle(void);
void gsu_view_matrix(void);
void view_matrix_update_and_save(void);
void view_update_and_sort(void);
void view_reset(void);
void gsu_noise_ramp(void);
void gsu_glyph_f329(uint16_t n);
void gsu_glyph_f329_sequence(uint16_t at);
void gsu_player_picture(int player);
void gsu_picture_rise(uint16_t y, int split);
void gsu_digit_pair_0e74(void);
void gsu_glyphs_three(void);
void gsu_glyphs_five(void);
int gsu_sprite_fa1b_from_tables(void);
void gsu_sprites_fa5a_three(void);

#endif
