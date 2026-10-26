/*
 * raster.c: the frame's stages, each run at a line of the picture: the
 * screen turned on and off around the part the game draws, the changes
 * of setting between a split screen's halves, and the frame's work (the
 * fade, the pads, the sound, the race clock and the HUD, the palettes and
 * windows, the uploads made while the screen is off).
 *
 * The stage to run next is $0D3F; each sets the next and the line it runs
 * at (raster_line, and raster_dot within that line: a stage that comes
 * late in its line makes its changes from the line after).
 */
#include <stdlib.h>

#include "game.h"
#include "video.h"

#define raster_stage_next   W8(0x0D3F)
#define screen_brightness   W8(0x0D61)  /* the brightness the screen comes on at (the fade's) */
#define sprites_dirty       W8(0x0D52)

uint16_t raster_line, raster_dot;       /* where the next stage runs */

/* The line a stage's changes take effect from. */
static unsigned raster_row(void)
{
    return raster_line + (raster_dot >= 0xC0);
}

static void next(uint8_t stage, uint16_t line)
{
    raster_stage_next = stage;
    raster_line = line;
}

static void screen_on_off(uint8_t v)
{
    VideoScreen *s = video_screen();

    s->brightness = v & 0x0F;
    s->blank = v >> 7;
}

static void frame_counts(void)
{
    frame_count++;
    W8(0x0306)++;
}

static void layer_map(int l, uint8_t v)
{
    VideoLayer *L = &video_screen()->layer[l];

    L->map = (uint16_t)((v & 0xFC) << 8);
    L->map_wide = v & 1;
    L->map_tall = (v >> 1) & 1;
}

static void layer_tiles_12(uint8_t v)
{
    VideoScreen *s = video_screen();

    s->layer[0].tiles = (uint16_t)((v & 0x0F) << 12);
    s->layer[1].tiles = (uint16_t)((v >> 4) << 12);
}

/* The window's colour (three bytes of the fixed colour's components) from
   `at`. */
static void window_colours(uint16_t at)
{
    int i;

    for (i = 0; i < 3; i++)
        video_register(0x32, W8(at + i));
}

/* Layer 2 for the split screen's lower half: scrolled by the sign of v
   (down `y` across `x`), its map at $7800 and the tiles of layers 1 and 2
   at $0000 and $7000. */
static void split_bg2(uint8_t v, uint16_t y_pos, uint16_t y_neg, uint8_t x)
{
    VideoLayer *l = &video_screen()->layer[1];

    l->scroll_y = (v & 0x80) ? y_neg : y_pos;
    l->scroll_x = (uint16_t)(0x100 | x);
    layer_map(1, 0x78);
    layer_tiles_12(0x70);
}

/* The menus' screen comes on (stage 0) and goes off (2), the pads and the
   sprites read while it is off. */
static void stage_menu_on(void)
{
    video_at_line(raster_row());
    screen_on_off(screen_brightness);
    next(0x02, 0xC8);
    frame_counts();
}

static void stage_menu_off(void)
{
    video_at_line(raster_row());
    screen_on_off(0x80);
    next(0x00, 0x17);
    sprites_dirty = 0xFF;
    video_line_tables_on(0x02);
    joypad_read();
    video_sprites_all();
    sound_ports_send();
}

/* The course screens' upper part (stage $10): layers 1 and 2, layer 2
   scrolled for it; the fade. */
static void stage_course_top(void)
{
    VideoScreen *s;

    video_at_line(raster_row());
    s = video_screen();
    s->main_layers = 0x13;
    s->layer[1].scroll_y = (uint16_t)((W8(0x0E34) << 8 | W8(0x0E37)) & 0x3FF);
    next(0x12, 0xD8);
    frame_counts();
    joypad_read();
    fade_step_fixed();
    sound_ports_send();
}

/* The course screens' lower part (stage $12): off while the course's
   picture goes up, then on with layers 1 and 2 at its own settings. */
static void stage_course_bottom(void)
{
    VideoScreen *s;
    uint8_t c;

    video_at_line(raster_row());
    screen_on_off(0x80);
    next(0x10, 0x37);
    video_line_tables_on(W8(0x0D45));
    video_register(0x31, W8(0x0D47));
    sprites_dirty = 0xFF;
    video_sprites_all();
    s = video_screen();
    s->layer[1].scroll_y = (uint16_t)((W8(0x0E34) << 8 | W8(0x0E33)) & 0x3FF);
    c = (uint8_t)(W8(0x1340D) | 0x80);
    video_register(0x32, c);
    c = (uint8_t)((c & 0x7F) | 0x40);
    video_register(0x32, c);
    video_register(0x32, (uint8_t)((c & 0x1F) | 0x20));
    s->main_layers = 0x03;
    video_from_bank_7f(0x1000, 0x93D7, 0x5000);
    if (W16(0x0D7F)) {
        W16(0x0D7F) = 0;
        video_from_gsu_ram(0x0800, 0x2C00, 0x0400);
        W16(0x0D81) = 0xFFFF;
    }
    window_table_7f();
    screen_on_off(screen_brightness);
}

/* A race on: the screen on at the fade's brightness, the frame's work. */
static void stage_race_on(void)
{
    video_at_line(raster_row());
    screen_on_off(screen_brightness);
    raster_dot = 0x30;
    next(0x14, 0xC0);
    frame_counts();
    sprites_dirty = 0xFF;
    fade_step();
    joypad_accumulate();
    sound_ports_send();
    race_clock_step();
    hud_time_sprite = 0x01D8;
    hud_draw_time();
    hud_panels_step();
    palettes_step();
}

/* A race's windows stepped, near its bottom (no change on screen). */
static void stage_race_windows(void)
{
    next(0x06, 0xD0);
    window_table_normal();
    window_layout_step_for_screen();
    window_slide_or_oscillate();
    window_sweep_if_on();
    window_slide_check();
    bob_windows_run();
    window_bands_animate();
    window_split_animate();
}

/* A race off at its bottom: the uploads, the sprite bands, the colours,
   then the settings the next frame starts with. */
static void stage_race_off(void)
{
    VideoScreen *s;

    video_at_line(raster_row());
    screen_on_off(0x80);
    nmi_frame_upload();
    oam_direct_stage();
    raster_colours_unless_0d22();
    raster_dot = 0xE0;
    next(0x04, 0x10);
    s = video_screen();
    s->main_layers = 0x13;
    video_line_tables_on(W8(0x0D45));
    window_colours(0x19AC);
    windows_setup_select();
}

/* Two players: the upper half on (stage 8), with the frame's work. */
static void stage_two_top_on(void)
{
    video_at_line(raster_row());
    screen_on_off(screen_brightness);
    next(0x0A, 0x6F);
    raster_dot = 0x10;
    frame_counts();
    sprites_dirty = 0xFF;
    fade_step();
    race_clock_step();
    video_screen()->main_layers = 0x13;
    hud_time_sprite = 0x0100;
    hud_draw_time();
    hud_panel_a_slide();
    hud_panel_b_slide();
    sound_ports_send();
    palette_kept_colours_step(0x02, 0x01);
}

/* Two players: off between the halves (stage $0A), the lower half's
   window colours and layer 2. */
static void stage_two_gap(void)
{
    uint8_t v;

    video_at_line(raster_row());
    screen_on_off(0x80);
    next(0x0C, 0x77);
    window_colours(0x19AF);
    if ((v = W8(0x0D5D))) {
        split_bg2(v, 0x180, 0x000, 0xA0);
        return;
    }
    if (W8(0x0D5C))
        layer_tiles_12(0x40);
    layer_map(1, 0x3F);
    bg2_scroll_from_table(0x02);
    raster_colours();
}

/* Two players: the lower half on (stage $0C). */
static void stage_two_bottom_on(void)
{
    video_at_line(raster_row());
    screen_on_off(screen_brightness);
    next(0x0E, 0xD7);
    hud_time_sprite = 0x0000;
    hud_draw_time();
    palette_kept_colours_step(0x00, 0x00);
    window_split_animate();
}

/* Two players: off at the bottom (stage $0E): the uploads and the frame's
   work, then the upper half's settings. */
static void stage_two_off(void)
{
    VideoScreen *s;
    uint8_t v;

    video_at_line(raster_row());
    screen_on_off(0x80);
    nmi_frame_upload();
    video_sprites_all();
    window_layout_step_for_screen();
    raster_palette_upload();
    joypad_accumulate();
    window_table_bands();
    window_layout_step_for_screen();
    bob_windows_run();
    window_bands_animate();
    if ((v = W8(0x0D5C)))
        split_bg2(v, 0x1E8, 0x068, 0xF0);
    else {
        if (W8(0x0D5D))
            layer_tiles_12(0x40);
        bg2_scroll_from_table(0x00);
        layer_map(0, 0x1C);
        layer_map(1, 0x3F);
    }
    s = video_screen();
    s->mode = 1;
    s->layer3_priority = 0;
    s->layer[0].large_tiles = s->layer[1].large_tiles = s->layer[2].large_tiles = s->layer[3].large_tiles = 0;
    s->main_layers = 0x03;
    video_line_tables_on(0x1C);
    window_colours(0x19AC);
    windows_setup_select();
    next(0x08, 0x0F);
    gradient_pointers_to_bank_7f();
}

/* The menus' frame end (stages $1E and $20), with the screen off. */
static void stage_menu_end(int anim)
{
    video_at_line(raster_row());
    screen_on_off(0x80);
    if (anim)
        frame_anim_tiles_next();
    vblank_updates();
    frame_sound_and_joypads();
    if (anim)
        frame_menu_end();
    else
        frame_count_and_ports_end();
    raster_line = 0xE0;
}

/* The stage $0D3F asks for. */
void raster_stage(void)
{
    switch (raster_stage_next) {
    case 0x00: stage_menu_on(); break;
    case 0x02: stage_menu_off(); break;
    case 0x04: stage_race_on(); break;
    case 0x06: stage_race_off(); break;
    case 0x08: stage_two_top_on(); break;
    case 0x0A: stage_two_gap(); break;
    case 0x0C: stage_two_bottom_on(); break;
    case 0x0E: stage_two_off(); break;
    case 0x10: stage_course_top(); break;
    case 0x12: stage_course_bottom(); break;
    case 0x14: stage_race_windows(); break;
    case 0x16: case 0x18: case 0x1A: break;
    case 0x1C: raster_line = 0x10; break;
    case 0x1E: stage_menu_end(0); break;
    case 0x20: stage_menu_end(1); break;
    default: abort();                   /* no other stage is set */
    }
}
