/*
 * game.h: the game's code as the port has it, area by area.
 */
#ifndef SRFX_GAME_H
#define SRFX_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "state.h"
#include "fixmath.h"
#include "rom.h"

/* ---- audio/sound_queue.c ---- */
void sound_queue(uint8_t command);          /* for the current player (bit 7: player 2) */
void sound_queue_raw(uint8_t command);
void sound_queue_port1(uint8_t command);

/* ---- video/sprite_bars.c ---- */
/* $F0 (hidden) for a Y of $B0 or more, below where sprites may show. */
uint8_t sprite_y_on_screen(uint8_t y, bool *hidden);
/* A bar gauge: a strip at `pos` from sprite offset `sprite` + 4 and its
   marker at `sprite`. */
void sprite_bar4(uint8_t value, uint16_t pos, uint16_t sprite);
void sprite_bar6(uint8_t value, uint16_t pos, uint16_t sprite);

/* ---- hud/digits.c ---- */
void fade_step_fixed(void);
void fade_step(void);
void raster_colours(void);
void raster_palette_upload(void);
void raster_colours_cycling(uint16_t x);
void raster_colours_e4(uint16_t x);
void raster_colours_unless_0d22(void);
void palette_cycle(void);
void palette_kept_colours_step(uint16_t x, uint16_t y);
void player_colours_flash(uint8_t level, uint16_t y);
void status_colour_pick(uint16_t y);
void palettes_step(void);
void hud_draw_time(void);
void hud_draw_count(void);
void hud_draw_number3(uint8_t n, uint16_t sprite);
void hud_draw_speed_a(void);
void hud_draw_speed_b(void);
void hud_draw_value_c(void);
void hud_count_up(uint16_t obj);
void hud_digit_sprites_62(void);
void hud_count_move_copy(void);

/* ---- hud/times.c ---- */
uint16_t time_pack(BcdTime t);
BcdTime time_unpack(uint16_t packed);       /* $FFFF (no time): $FF in each */
void time_add(const BcdTime *t, BcdTime *to);
void time_totals_sum(void);
void time_running_totals(void);
bool time_below_record(int place);
void record_time_insert(void);

/* ---- hud/slide.c ---- */
void hud_slide_in(void);
void hud_slide_out(void);
void hud_panel_a_slide(void);
void hud_panel_b_slide(void);
void hud_panels_step(void);

/* ---- hud/gauges.c ---- */
void hud_gauges_panel_a(void);
void hud_gauges_panel_b(void);
void gauge6_b_place(void);
void gauge6_a_place(void);

/* ---- hud/map.c ---- */
void map_markers(uint8_t origin_x, uint8_t origin_y, uint16_t sprite, int16_t step);
void map_markers_panel_a(void);
void map_markers_panel_b(void);

/* ---- data/fx_unpack.c ---- */
void fx_unpack_program(void);       /* the Super FX's unpacker, inputs at $002C-$006A, $00A2 */

/* ---- render/fx_init.c ---- */
void fx_init_program(void);         /* the Super FX's RAM set up at power on */

/* ---- hud/race_text.c ---- */
void oam_class_name(void);

#endif
