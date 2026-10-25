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
void gsu_sprites_fa5a_from_tables(void);
void gsu_sprites_fa5a_six(void);
void gsu_sprites_fa5a_six_left(void);
void gsu_sprites_fa5a_three_shifted(void);
int gsu_sprite_fa35_from_tables(void);
void gsu_glyphs_two_lines(uint16_t rows);
void gsu_glyph_rows_at_32(uint16_t rows);
void gsu_glyph_rows_at_56(uint16_t rows);
void gsu_glyphs_six(void);
void gsu_pieces_step_10cb(void);
void big_sprites_count_draw(void);
void big_sprites_row6(void);
void big_sprites_row5(void);
void big_sprites_row6b(void);
void big_sprites_nine(void);
void glyph_row_y60(int from);
void glyph_list_draw(void);
void glyph_sprite_draw(uint16_t k);
void glyph_sprites_init(void);
void glyph_list_message_and_time(uint16_t m);
void hud_hold_draw(void);
uint16_t object_length_and_angle(uint16_t obj);
uint16_t object_angle(uint16_t obj);
void glyph_word_at_1c28(void);
void glyph_grid_1a(void);
void glyph_grid_1d(void);
void glyph_slide_three(void);
void glyph_sprites_draw_all(void);
void hud_times_packed_draw(void);
void hud_lap_digits_draw(void);
void hud_lead_word_draw(void);
void hud_times_draw(void);
void hud_blank_draw(void);
void glyph_sprites_slide_out(void);
void glyph_sprites_slide_in(void);
void gsu_glyphs_four(int b);

#endif
