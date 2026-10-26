/*
 * oam_direct.c: sprites written to the sprite table between frames
 * straight from the game's copy, a band at a time, as the HUD's panels
 * slide.
 */
#include "game.h"
#include "video.h"

#define panel_stage W8(0x0D54)

static void sprites_copy(uint16_t at, uint16_t n)
{
    video_sprite_table(&wram[0x0375 + at], at, n);
}

/* The panel stage on by 2 when the byte at `at` is v. */
static void stage_next_if(uint16_t at, uint8_t v)
{
    if (W8(at) == v)
        panel_stage += 2;
}

static void stage_reset_if(uint16_t at, uint8_t v)
{
    if (W8(at) == v)
        panel_stage = 0;
}

/* Away from Stunt Trax with the HUD held, sprites 116-127 hidden;
   otherwise by the panel stage: 0 sprites 116-127; 2 the same, moving on
   when $0E16 is $AC; 4 sprites 75-86, moving on when $0E12 is $1F; 6
   sprites 112-115, the stage ending when panel B ($0E0B) reaches $2A; 8
   sprites 36-46, ending when the slide is $10; $0A those and sprites
   116-127, ending when it is $D0. */
void oam_direct_stage(void)
{
    if (hud_mode != 1 && hud_hold) {
        static const uint8_t hidden[0x30] = {
            0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
            0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
            0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
        };

        video_sprite_table(hidden, 0x1D0, sizeof hidden);
        return;
    }
    switch (panel_stage) {
    case 0x02:
        stage_next_if(0x0E16, 0xAC);
        /* fall through */
    case 0x00:
        sprites_copy(0x1D0, 0x30);
        break;
    case 0x04:
        stage_next_if(0x0E12, 0x1F);
        sprites_copy(0x12C, 0x30);
        break;
    case 0x06:
        if (panel_b_y == 0x2A) {
            panel_b_shown = 0x2A;
            panel_stage = 0;
        }
        sprites_copy(0x1C0, 0x10);
        break;
    case 0x08:
        stage_reset_if(0x0D57, 0x10);
        sprites_copy(0x090, 0x2C);
        break;
    default:
        stage_reset_if(0x0D57, 0xD0);
        sprites_copy(0x090, 0x2C);
        sprites_copy(0x1D0, 0x30);
    }
}
