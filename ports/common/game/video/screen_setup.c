/*
 * screen_setup.c: the picture's settings when a screen is set up: the PPU
 * put to a known state (screen_ppu_reset), and the race screen's layers,
 * windows and colour maths with the HDMA tables that move the windows and
 * the BG2 scroll a line at a time (race_screen_setup). The settings are the
 * picture's (video.h); the work RAM and game pak RAM words written are the
 * game's own.
 */
#include "game.h"
#include "render.h"
#include "screen_setup.h"
#include "video.h"

#define WRAM7F(a)   wram[0x10000u + (a)]    /* bank $7F, low address a */

/* A layer's map from its register ($2107-$210A): the word address, and the
   screens the map spans (bit 0 wide, bit 1 tall). */
static void set_layer_map(VideoLayer *l, uint8_t v)
{
    l->map = (uint16_t)((v & 0xFC) << 8);
    l->map_wide = v & 1;
    l->map_tall = (v >> 1) & 1;
}

/* The PPU put to its known state. The timers, interrupts and the DMA
   enables are the machine's timing, not the picture: left out. */
void screen_ppu_reset(uint16_t x)
{
    static const uint8_t black[2] = {0x00, 0x00};
    VideoScreen *s = video_screen();
    int l;

    /* BGMODE $03: mode 3, no layer 3 priority, 8 x 8 tiles; MOSAIC $00 */
    s->mode = 3;
    s->layer3_priority = 0;
    for (l = 0; l < VIDEO_LAYERS; l++)
        s->layer[l].large_tiles = 0;
    s->mosaic = 0;

    /* OBSEL $03: small and large sprites as size set 0, tiles at $6000 */
    s->sprite_sizes = 0;
    s->sprite_gap = 0;
    s->sprite_tiles = 0x6000;

    /* BG1SC $7A and BG2SC $5A: the maps; BG12NBA $60: BG1 characters at 0,
       BG2 characters at $6000 */
    set_layer_map(&s->layer[0], 0x7A);
    set_layer_map(&s->layer[1], 0x5A);
    s->layer[0].tiles = 0x0000;
    s->layer[1].tiles = 0x6000;

    /* the sub screen, the window masks and logic, and the colour maths
       cleared: TS, TMW, TSW, W12SEL, W34SEL, WOBJSEL, WBGLOG, WOBJLOG, CGWSEL,
       CGADSUB all $00 */
    s->sub_layers = 0;
    s->window_layers = 0;
    s->window_sub_layers = 0;
    for (l = 0; l < 6; l++) {
        s->window_use[l] = 0;
        s->window_logic[l] = 0;
    }
    s->colour_clip = 0;
    s->colour_prevent = 0;
    s->colour_sub_screen = 0;
    s->colour_direct = 0;
    s->colour_subtract = 0;
    s->colour_half = 0;
    s->colour_maths = 0;

    /* COLDATA: the fixed colour's data from X's low byte; SETINI from its
       high byte, then cleared (the port keeps no SETINI state) */
    video_register(0x32, (uint8_t)x);
    video_register(0x33, (uint8_t)(x >> 8));
    video_register(0x33, 0x00);

    /* colour 0 black */
    video_colours(0, black, 1);

    /* HDMA off: no table runs */
    video_line_tables_on(0);

    /* the Super FX's screen base: SCBR $0B, in kilobytes */
    fx_screen.base = 0x0B;

    /* the buffer start: $70:021A (game pak RAM) and direct page $48 */
    P16(0x021A) = 0x2C00;
    W16(0x48) = 0x2C00;
}

/* The race screen. */
void race_screen_setup(void)
{
    static const uint16_t ones[10] = {0x3411, 0x3414, 0x3416, 0x3419, 0x341C, 0x3425, 0x3428, 0x342B, 0x342E, 0x342F};
    static const uint16_t zeros[4] = {0x3412, 0x3415, 0x3430, 0x3431};
    static const uint8_t black[2] = {0x00, 0x00};
    VideoScreen *s = video_screen();
    VideoLineTable window_bands = {
        .mode = 1, .indirect = 0, .target = 0x26, .bank = 0x7F, .indirect_bank = 0, .at = 0x3410
    };
    VideoLineTable bg2_scroll = {
        .mode = 2, .indirect = 0, .target = 0x10, .bank = 0x00, .indirect_bank = 0, .at = 0x10AA
    };
    unsigned i, reg;

    /* work RAM: the window flags, the sound and screen words */
    W16(0x0D7F) = 0x0000;
    W16(0x0D81) = 0xFFFF;
    for (i = 0; i < 10; i++)
        WRAM7F(ones[i]) = 0x01;
    for (i = 0; i < 4; i++)
        WRAM7F(zeros[i]) = 0x00;
    W8(0x0D47) = 0x00;
    W8(0x0D45) = 0x02;
    W8(0x0E91) = 0xF0;
    W8(0x0E92) = 0xF0;

    /* the BG2 scroll table at $00:10AA: $38 lines of the word at $0E33, then
       $4F lines of $0E37 (the word there, low then high), then three single
       lines of it and the end */
    W8(0x10AA) = 0x38;
    W8(0x10AB) = W8(0x0E33);
    W8(0x10AC) = W8(0x0E34);
    W8(0x10AD) = 0x4F;
    for (i = 0; i < 4; i++) {
        W8(0x10AE + 3 * i) = W8(0x0E37);
        W8(0x10AF + 3 * i) = W8(0x0E38);
        W8(0x10B0 + 3 * i) = i < 3 ? 0x01 : 0x00;
    }

    /* HDMA: channel 1 from $7F:3410 to WH0 and WH1 (the window bands), channel
       2 from $00:10AA to BG2VOFS */
    video_line_table(1, &window_bands);
    video_line_table(2, &bg2_scroll);

    /* the windows and colour maths: WOBJSEL $30 (sprites' window off, colour
       window 1 inverted and on), W34SEL $03 (BG3 window 1 inverted and on),
       TMW $07 (BG1-BG3 windowed), CGWSEL $20 (colour prevent 2) */
    s->window_use[4] = 0x0;
    s->window_use[5] = 0x3;
    s->window_use[2] = 0x3;
    s->window_use[3] = 0x0;
    s->window_layers = 0x07;
    s->colour_clip = 0;
    s->colour_prevent = 2;
    s->colour_sub_screen = 0;
    s->colour_direct = 0;

    /* BGMODE $0B: mode 3 with layer 3 on top, 8 x 8 tiles; OBSEL $03 as
       above */
    s->mode = 3;
    s->layer3_priority = 1;
    for (i = 0; i < VIDEO_LAYERS; i++)
        s->layer[i].large_tiles = 0;
    s->sprite_sizes = 0;
    s->sprite_gap = 0;
    s->sprite_tiles = 0x6000;

    /* the maps: BG1SC $00, BG2SC $52; characters BG12NBA $20: BG1 at 0, BG2
       at $2000; TM $12: BG2 and the sprites on the main screen */
    set_layer_map(&s->layer[0], 0x00);
    set_layer_map(&s->layer[1], 0x52);
    s->layer[0].tiles = 0x0000;
    s->layer[1].tiles = 0x2000;
    s->main_layers = 0x12;

    /* colour 1 black */
    video_colours(1, black, 1);

    /* every layer's scroll at 0, written as the registers are (BG1HOFS,
       BG1VOFS, ... BG3VOFS, each twice) so the port's scroll latches follow */
    for (reg = 0x0D; reg <= 0x12; reg++) {
        video_register((uint8_t)reg, 0x00);
        video_register((uint8_t)reg, 0x00);
    }
}
