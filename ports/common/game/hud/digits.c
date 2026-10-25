/*
 * digits.c: numbers drawn as sprite digits. Each digit is two 8x8 sprites
 * stacked, tile 2d on top and 2d + 1 below; only the tiles of sprites
 * already placed are written. Sprites are named by their byte offset in the
 * sprite table, as the game names them.
 */
#include "game.h"

static void digit(uint16_t sprite, uint8_t d)
{
    OAM_TILE(sprite) = (uint8_t)(d << 1);
    OAM_TILE(sprite + 4) = (uint8_t)((d << 1) + 1);
}

/* The race time: hundredths, seconds and the minutes' last digit on the
   five sprite pairs from hud_time_sprite. Outside Stunt Trax the HUD's
   hold timer counts down first and the time is drawn once it has run out;
   otherwise (or while it runs) only when hud_blank is set. */
void hud_draw_time(void)
{
    uint16_t at = hud_time_sprite;

    if (hud_mode != 1 && hud_hold == 0) {
        /* drawn */
    } else {
        if (hud_mode != 1)
            hud_hold--;
        if (!hud_blank)
            return;
    }
    digit((uint16_t)(at + 0x18), race_time_seconds & 0x0F);
    digit((uint16_t)(at + 0x10), race_time_seconds >> 4);
    digit((uint16_t)(at + 0x08), race_time_fraction & 0x0F);
    digit(at, race_time_fraction >> 4);
    digit((uint16_t)(at + 0x20), race_time_minutes & 0x0F);
}

/* The BCD count, ones then tens, unless hud_blank is set. */
void hud_draw_count(void)
{
    if (hud_blank)
        return;
    digit(0x168, hud_digits_19ec & 0x0F);
    digit(0x160, hud_digits_19ec >> 4);
}

/* A byte as three digits on the sprite pairs from `sprite`. */
void hud_draw_number3(uint8_t n, uint16_t sprite)
{
    digit(sprite, n / 100);
    digit((uint16_t)(sprite + 8), n % 100 / 10);
    digit((uint16_t)(sprite + 16), n % 10);
}

void hud_draw_speed_a(void) { hud_draw_number3(speed_sample, 0x114); }
void hud_draw_speed_b(void) { hud_draw_number3(speed_sample, 0x028); }
void hud_draw_value_c(void) { hud_draw_number3(wram_0DDB, 0x128); }

/* Object `obj` marked counted (bit 0 of +$25), the count up one (BCD) and
   its sound. */
void hud_count_up(uint16_t obj)
{
    bool c = false;

    obj_25(obj) |= 0x01;
    hud_digits_19ec = bcd_add8(hud_digits_19ec, 0x01, &c);
    sound_queue(0x3C);
}

/* The digit on sprites 62 and 63: digit62_value, or 1 when overridden. */
void hud_digit_sprites_62(void)
{
    digit(62 * 4, digit62_override ? 0x01 : digit62_value);
}

/* The count at $19EE moved to $19F0, and $19F2 set. */
void hud_count_move_copy(void)
{
    wram_19F0 = wram_19EE;
    wram_19EE = 0;
    wram_19F2 = 1;
}
