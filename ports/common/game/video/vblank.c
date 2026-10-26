/*
 * vblank.c: the menus' and the course screens' work between frames: the
 * Super FX picture copied to the store a stage a frame, the window that
 * opens and closes over the screen, the queued uploads, a blinking colour,
 * the brightness eased toward its target; and the end of the frame.
 */
#include "game.h"
#include "video.h"

#define upload_stage    W16(0x0D7D)
#define screen_flags    W8(0x11D7)      /* bit 5: a picture completed this frame */
#define window_mode     W16(0x1905)     /* 0 idle, 1 opening, 2 closing, 3 set */
#define window_size     W16(0x1907)     /* 0-$2C, by 4 */
#define brightness_now  W8(0x11DA)
#define brightness_to   W8(0x11DB)

/* A stage of the picture: `rows` + 1 wide rows from Super FX RAM. */
static void picture_rows(uint16_t next, uint8_t rows_less_one, uint16_t from, uint16_t to)
{
    upload_stage = next;
    video_wide_rows_from_gsu_ram(rows_less_one, from, to);
}

/* The picture complete: layer 1's map at `map`. */
static void picture_done(uint16_t map)
{
    VideoLayer *l = &video_screen()->layer[0];

    W8(0x0357) = 1;
    l->map = map;
    l->map_wide = l->map_tall = 0;
    screen_flags |= 0x20;
}

/* The finished picture copied in three stages a frame each, from either
   buffer (stages 0-4 or 8-$C), the map then pointed at it. */
static void gsu_picture_upload_step(void)
{
    switch (upload_stage) {
    case 0x0: picture_rows(0x2, 3, 0x2C00, 0x0000); break;
    case 0x2: picture_rows(0x4, 3, 0x3C00, 0x0580); break;
    case 0x4: picture_rows(0x6, 4, 0x4C00, 0x0B00); picture_done(0x6000); break;
    case 0x8: picture_rows(0xA, 3, 0x5000, 0x1760); break;
    case 0xA: picture_rows(0xC, 3, 0x4000, 0x11E0); break;
    case 0xC: picture_rows(0x6, 4, 0x2C00, 0x0B00); picture_done(0x6400); break;
    }
}

/* The window's per-line table for its size: the bands above and below
   $2C - size lines, the middle size * 2 (or, at a multiple of 256, two
   entries of 2 and 3 lines, the band below two shorter). */
static void window_open_table(void)
{
    uint8_t c = (uint8_t)(0x2C - window_size), d = (uint8_t)(window_size << 1);

    W8(0x1134A) = c;
    W8(0x1134E) = c;
    if (d) {
        W8(0x1134C) = d;
        W8(0x1134D) = 0;
    } else {
        W8(0x1134C) = 2;
        W8(0x1134D) = 3;
        W8(0x1134E) = (uint8_t)(c - 2);
    }
}

/* The window over the screen: opening shrinks it by 4 a frame until it
   is gone, closing grows it to $2C and then darkens colour 0; mode 3 sets
   the bands of the layout at $1921-$1927. */
static void window_open_close_step(void)
{
    uint16_t t;

    switch (window_mode) {
    case 0:
        return;
    case 3: {
        uint8_t n = (uint8_t)(W8(0x1921) + 1);

        W8(0x112DB) = W8(0x112E8) = W8(0x112F6) = W8(0x11301) = n;
        W16(0x191B) = (uint16_t)(0x16 - W16(0x1921) + W16(0x1925));
        W16(0x191D) = (uint16_t)(0x45 - W16(0x1921) + W16(0x1927));
        return;
    }
    case 2:
        window_open_table();
        t = window_size = (uint16_t)(window_size + 4);
        if (t == 0x2C) {
            static const uint8_t black[2] = {0, 0};

            window_mode = 0;
            W16(0xFD65) = 0;
            video_colours(0, black, 1);
        }
        return;
    default:
        window_open_table();
        t = window_size = (uint16_t)(window_size - 4);
        if (t & 0x8000)
            window_mode = 0;
    }
}

/* Colour $D9 blinking between the two at $7F:0DA8, every fifth frame. */
static void blink_colour(void)
{
    uint16_t c = (uint16_t)(W16(0x1901) + 1);

    W16(0x1901) = c;
    if (c == 5) {
        W16(0x1901) = 0;
        W16(0x18FF) = (W16(0x18FF) + 1) & 1;
    }
    video_colours(0xD9, &wram[0x10DA8 + (uint16_t)(W16(0x18FF) << 1)], 1);
}

/* The brightness a step toward its target every other frame. */
static void half_rate_approach(void)
{
    uint8_t t = (W8(0x11DC) + 1) & 1;

    W8(0x11DC) = t;
    if (t || brightness_now == brightness_to)
        return;
    brightness_now = (uint8_t)(brightness_now < brightness_to ? brightness_now + 1 : brightness_now - 1);
}

void vblank_updates(void)
{
    W16(0x11D8) = 0;
    gsu_picture_upload_step();
    window_open_close_step();
    if (!(screen_flags & 0x20)) {
        frame_colours_if_pending();
        frame_transfers_if_pending();
        frame_sprites_if_pending();
    }
    frame_byte_writes_if_pending(0x00);
    if (W16(0x18B9))
        blink_colour();
    half_rate_approach();
    screen_flags &= 0xDF;
}

/* The end of a frame of the menus: the frame counters, a check of the
   pads for a Super Multitap when $1903 asks (the platforms have plain
   pads: none is found), and the brightness shown. */
void frame_count_and_ports_end(void)
{
    VideoScreen *s = video_screen();

    W8(0x0006)++;
    frame_count++;
    W8(0x0306)++;
    W16(0x18FB) = 0;
    if (W16(0x1903))
        W8(0x1182) = 0;
    s->brightness = brightness_now & 0x0F;
    s->blank = brightness_now >> 7;
}
