/*
 * render.h: the 3D renderer (render/), the Super FX's work.
 */
#ifndef SRFX_RENDER_H
#define SRFX_RENDER_H

#include <stdbool.h>
#include <stdint.h>

/* ---- render/fx_geom.c ---- */
void fx_matrix_apply(const int16_t v[3], int16_t out[3]);
void fx_matrix_rotate(const int16_t v[3], int16_t out[3]);
void fx_matrix_apply_vectors(uint16_t out);
void fx_matrix_rotate_program(void);
void fx_matrix_rotate_back_program(void);
void fx_matrix_from_angles_at(uint16_t out);
void fx_matrix_transpose_at(uint16_t at);
void fx_angle_program(void);
void fx_length_program(void);
uint16_t fx_table_entry_words(uint16_t entry);
void fx_table_2702_program(void);
void fx_screen_clear(void);
void fx_clear_screen_if_set(void);       /* programs the S-CPU starts (gsu/fx_run.c) */
void fx_clear_6000_if_set(void);
void fx_matrix_from_angles_program(void);
int fx_run(uint8_t bank, uint16_t start);
uint16_t fx_project_point(int16_t *x, int16_t *y, int16_t *z);
void fx_project_scaled(int16_t *x, int16_t *y, int16_t *z, int16_t *size);
uint16_t fx_project_vertex(int16_t *x, int16_t *y, int16_t z);
void fx_sort_objects_program(void);
uint16_t fx_points_gather(uint16_t count, uint8_t bank, uint16_t script);
uint16_t fx_model_points(uint8_t bank, uint16_t script, uint16_t *end);
int fx_segment_cross_z(int16_t a[3], int16_t b[3]);

/* ---- render/fx_clip.c ---- */
uint16_t fx_polygon_clip_top(void);
uint16_t fx_polygon_clip_bottom(void);
uint16_t fx_polygon_clip_right(void);
uint16_t fx_polygon_clip_left(void);
uint16_t fx_polygon_clip_near(int16_t near);
uint16_t fx_polygon_project(void);
uint16_t fx_polygon_near_clip(void);
uint16_t fx_polygon_draw(uint16_t outcodes, uint8_t bank, uint16_t script);
uint16_t fx_polygon_gather(uint16_t n, uint8_t bank, uint16_t script);
uint16_t fx_polygon_clip_screen(uint16_t n);

/* ---- render/fx_model.c ---- */
void fx_model_draw(void);
void fx_model_draw_flagged(void);
void fx_model_draw_with_flags(uint16_t flags);
void fx_sprite_material(uint8_t index);

/* ---- render/fx_screen.c ---- */
/* The screen as the S-CPU has set it up: its colour depth (SCMR's mode:
   0 2 bits, 1 and 2 4 bits, 3 8 bits), height (0 128 lines, 1 160, 2 192,
   3 the sprite layout) and base (SCBR, in kilobytes). */
typedef struct {
    uint8_t mode, height, base;
} FxScreen;
extern FxScreen fx_screen;
void fx_screen_put(const FxScreen *s, uint8_t x, uint8_t y, uint8_t colour);
uint8_t fx_screen_get(const FxScreen *s, uint8_t x, uint8_t y);
void fx_draw_list_program(void);
extern uint8_t fx_colour, fx_plot_options;
void fx_set_colour(uint8_t v);
void fx_plot(uint8_t x, uint8_t y);
void fx_plot_put(uint8_t x, uint8_t y, uint8_t c);
uint8_t fx_read_pixel(uint8_t x, uint8_t y);
void fx_pixels_flush(void);
void fx_pixels_set(int i, uint16_t row, uint8_t pending, const uint8_t colours[8]);

/* ---- render/fx_frame.c ---- */
void fx_frame_program(void);
void fx_objects_draw(void);
void fx_object_sprite(void);
void fx_effects_add(uint16_t kind, uint16_t life, uint16_t key, uint16_t count);
void fx_particles_draw(uint16_t key);
void fx_weather_draw(uint16_t kind, uint16_t drops);
void fx_effects_run(void);
void fx_text_notice(void);

/* ---- render/fx_text.c ---- */
uint16_t fx_draw_bitmap(uint16_t at, int w, int h, uint16_t x, uint16_t y, uint16_t palette);
void fx_text_draw(void);
void fx_text_program(void);

/* ---- render/fx_hud.c: programs the S-CPU starts, inputs at $0200-$021D ---- */
void fx_glyph_rows_from_bottom(void);
void fx_glyph_rows_from_top(void);
void fx_glyph_8x16(void);
void fx_glyph_8x8(void);
void fx_glyph_24x16(void);
void fx_glyph_16x16(void);
void fx_glyph_16x32(void);
void fx_glyph_24x32_bank04(void);
void fx_glyph_16x32_double(void);
void fx_picture_rise_split_40(void);
void fx_picture_rise_split_60(void);
void fx_picture_rise_40(void);
void fx_picture_rise_60(void);
void fx_scaled_sprite_13(void);
void fx_scaled_sprite_12(void);
void fx_scaled_sprite_eighth(void);
void fx_scaled_sprite_advance(void);
void fx_sprite_24x40(void);
void fx_sprite_16x32(void);
void fx_noise_fill(void);
void fx_noise_fill_screen(void);
void fx_tiles_2bpp_to_4bpp_high(void);
void fx_tiles_2bpp_to_4bpp_low(void);

#endif
