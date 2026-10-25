/*
 * fx_run.c: the Super FX programs the S-CPU starts, found by where they
 * start in ROM (the bank and address the S-CPU writes before it starts
 * the Super FX). The S-CPU hands a program its inputs only through the
 * game pak RAM; what the program leaves there, and in the screen's two
 * pixel caches, is its result.
 */
#include "game.h"
#include "render.h"
#include "physics.h"

/* $01:AECA, $01:AEE1: $1A00 words cleared from $2C00 or from $6000 when
   the byte $01C4 is set */
static void clear_if_set(uint16_t at)
{
    int i;

    if (!pakram[0x01C4])
        return;
    for (i = 0; i < 0x1A00; i++, at = (uint16_t)(at + 2))
        FXU(at) = 0;
}

void fx_clear_screen_if_set(void)
{
    clear_if_set(0x2C00);
}

void fx_clear_6000_if_set(void)
{
    clear_if_set(0x6000);
}

/* $01:8325: the matrix of the angles at $0020-$0024 into $00E4 */
void fx_matrix_from_angles_program(void)
{
    fx_matrix_from_angles_at(0x00E4);
}

static const struct {
    uint32_t at;
    void (*run)(void);
} programs[] = {
    {0x0180BC, fx_length_program},
    {0x0181E3, fx_angle_program},
    {0x018277, fx_matrix_rotate_back_program},
    {0x0182CE, fx_matrix_rotate_program},
    {0x018325, fx_matrix_from_angles_program},
    {0x01AB13, fx_frame_program},
    {0x01AECA, fx_clear_screen_if_set},
    {0x01AEE1, fx_clear_6000_if_set},
    {0x01AEF8, fx_sort_objects_program},
    {0x01BBC7, fx_timers_program},
    {0x01CF42, fx_unpack_program},
    {0x01D1EB, fx_table_2702_program},
    {0x01D307, fx_text_program},
    {0x048800, fx_init_program},
    {0x04888E, fx_pools_merge},
    {0x048B33, fx_record_new_426},
    {0x048B64, fx_record_new_422},
    {0x048C0A, fx_record_new_424},
    {0x048C3B, fx_record_new_426b},
    {0x048C6C, fx_record_new_camera},
    {0x048C95, fx_record_new_42d},
    {0x048CC6, fx_car_new},
    {0x048FCA, fx_physics_program},
    {0x04BF51, fx_record_raise_6a},
    {0x04BF75, fx_record_lower_82},
    {0x07EE50, fx_tiles_2bpp_to_4bpp_high},
    {0x07EE59, fx_tiles_2bpp_to_4bpp_low},
    {0x07EE88, fx_noise_fill},
    {0x07EE94, fx_noise_fill_screen},
    {0x07EEF5, fx_picture_rise_split_40},
    {0x07EFCD, fx_picture_rise_split_60},
    {0x07F0A5, fx_picture_rise_40},
    {0x07F0B4, fx_picture_rise_60},
    {0x07F121, fx_glyph_rows_from_bottom},
    {0x07F15B, fx_glyph_rows_from_top},
    {0x07F280, fx_glyph_8x8},
    {0x07F329, fx_glyph_8x16},
    {0x07F3D2, fx_glyph_24x32_bank04},
    {0x07F441, fx_glyph_24x16},
    {0x07F5C9, fx_glyph_16x16},
    {0x07F6DF, fx_glyph_16x32},
    {0x07F7F4, fx_glyph_16x32_double},
    {0x07FA1B, fx_scaled_sprite_13},
    {0x07FA35, fx_scaled_sprite_12},
    {0x07FA5A, fx_scaled_sprite_eighth},
    {0x07FA88, fx_sprite_24x40},
    {0x07FAB0, fx_sprite_16x32},
    {0x07FAF2, fx_scaled_sprite_advance},
};

/* The program at `bank`:`start` run to its end. Returns 0 for a place no
   program starts. */
int fx_run(uint8_t bank, uint16_t start)
{
    uint32_t at = (uint32_t)(bank & 0x7F) << 16 | start;
    unsigned i;

    for (i = 0; i < sizeof programs / sizeof programs[0]; i++)
        if (programs[i].at == at) {
            programs[i].run();
            return 1;
        }
    return 0;
}
