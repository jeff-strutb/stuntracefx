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

#endif
