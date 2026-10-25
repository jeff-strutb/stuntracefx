/*
 * registers.c: the game's settings written as data: a register and a
 * value (the lists of writes the game queues for the next frame, the
 * tables in ROM, the per-line tables), read into the screen's settings by
 * meaning (video.h). A register written in two halves keeps its first
 * half until the second comes, as the game's data expects.
 */
#include "video.h"

static uint8_t scroll_latch, hscroll_latch, colour_latch;
static uint8_t colour_index;
static int colour_second;

/* the halves held cleared (as at power on) */
void video_registers_reset(void)
{
    scroll_latch = hscroll_latch = colour_latch = 0;
    colour_index = 0;
    colour_second = 0;
}

static void set_layer_map(VideoLayer *l, uint8_t v)
{
    l->map = (uint16_t)((v & 0xFC) << 8);
    l->map_wide = v & 1;
    l->map_tall = (v >> 1) & 1;
}

void video_register(uint8_t reg, uint8_t v)
{
    VideoScreen *s = video_screen();
    int l;

    switch (reg) {
    case 0x00:
        s->brightness = v & 0x0F;
        s->blank = v >> 7;
        break;
    case 0x01:
        s->sprite_sizes = v >> 5;
        s->sprite_gap = (v >> 3) & 3;
        s->sprite_tiles = (uint16_t)((v & 7) << 13);
        break;
    case 0x05:
        s->mode = v & 7;
        s->layer3_priority = (v >> 3) & 1;
        for (l = 0; l < 4; l++)
            s->layer[l].large_tiles = (v >> (4 + l)) & 1;
        break;
    case 0x06:
        s->mosaic = v;
        break;
    case 0x07: case 0x08: case 0x09: case 0x0A:
        set_layer_map(&s->layer[reg - 0x07], v);
        break;
    case 0x0B:
        s->layer[0].tiles = (uint16_t)((v & 0x0F) << 12);
        s->layer[1].tiles = (uint16_t)((v >> 4) << 12);
        break;
    case 0x0C:
        s->layer[2].tiles = (uint16_t)((v & 0x0F) << 12);
        s->layer[3].tiles = (uint16_t)((v >> 4) << 12);
        break;
    case 0x0D: case 0x0F: case 0x11: case 0x13:  /* across: this byte high, the last one's low bits */
        s->layer[(reg - 0x0D) / 2].scroll_x = (uint16_t)((v << 8 | (scroll_latch & ~7) | (hscroll_latch & 7)) & 0x3FF);
        scroll_latch = hscroll_latch = v;
        break;
    case 0x0E: case 0x10: case 0x12: case 0x14:  /* down: this byte high, the last one low */
        s->layer[(reg - 0x0E) / 2].scroll_y = (uint16_t)((v << 8 | scroll_latch) & 0x3FF);
        scroll_latch = v;
        break;
    case 0x21:
        colour_index = v;
        colour_second = 0;
        break;
    case 0x22:                                  /* a colour, low byte then high */
        if (colour_second)
            video_colour_at_line(colour_index++, (uint16_t)(colour_latch | v << 8));
        else
            colour_latch = v;
        colour_second = !colour_second;
        break;
    case 0x23:
        s->window_use[0] = v & 0x0F, s->window_use[1] = v >> 4;
        break;
    case 0x24:
        s->window_use[2] = v & 0x0F, s->window_use[3] = v >> 4;
        break;
    case 0x25:
        s->window_use[4] = v & 0x0F, s->window_use[5] = v >> 4;
        break;
    case 0x26: s->window_left[0] = v; break;
    case 0x27: s->window_right[0] = v; break;
    case 0x28: s->window_left[1] = v; break;
    case 0x29: s->window_right[1] = v; break;
    case 0x2A:
        for (l = 0; l < 4; l++)
            s->window_logic[l] = (v >> (2 * l)) & 3;
        break;
    case 0x2B:
        s->window_logic[4] = v & 3;
        s->window_logic[5] = (v >> 2) & 3;
        break;
    case 0x2C: s->main_layers = v & 0x1F; break;
    case 0x2D: s->sub_layers = v & 0x1F; break;
    case 0x2E: s->window_layers = v & 0x1F; break;
    case 0x2F: s->window_sub_layers = v & 0x1F; break;
    case 0x30:
        s->colour_clip = v >> 6;
        s->colour_prevent = (v >> 4) & 3;
        s->colour_sub_screen = (v >> 1) & 1;
        s->colour_direct = v & 1;
        break;
    case 0x31:
        s->colour_subtract = v >> 7;
        s->colour_half = (v >> 6) & 1;
        s->colour_maths = v & 0x3F;
        break;
    case 0x32:                                  /* the fixed colour: the components bits 5-7 pick */
        if (v & 0x20) s->fixed_colour = (uint16_t)((s->fixed_colour & ~0x001F) | (v & 0x1F));
        if (v & 0x40) s->fixed_colour = (uint16_t)((s->fixed_colour & ~0x03E0) | (v & 0x1F) << 5);
        if (v & 0x80) s->fixed_colour = (uint16_t)((s->fixed_colour & ~0x7C00) | (v & 0x1F) << 10);
        break;
    }
}

/* ---- the per-line tables ---- */

static uint8_t line_tables;

void video_line_tables_on(uint8_t mask)
{
    line_tables = mask;
}

uint8_t video_line_tables(void)
{
    return line_tables;
}
