/*
 * video.c: the picture as the game describes it (video.h): the graphics
 * store, the colours, the sprite table, and the screen's settings a line
 * at a time.
 */
#include <string.h>
#include "video.h"

static uint16_t store[0x8000];
static uint8_t sprites[544];
static VideoScreen screen[VIDEO_LINES];
static uint16_t colours[VIDEO_LINES][256];
static unsigned line;                   /* the line changes take effect from */

void video_store_write(uint16_t word, const uint8_t *data, unsigned bytes)
{
    unsigned i;

    for (i = 0; i + 1 < bytes; i += 2, word++)
        store[word & 0x7FFF] = (uint16_t)(data[i] | data[i + 1] << 8);
    if (i < bytes)                      /* an odd last byte: the low half */
        store[word & 0x7FFF] = (uint16_t)((store[word & 0x7FFF] & 0xFF00) | data[i]);
}

void video_store_fill(uint16_t word, uint16_t value, unsigned words)
{
    while (words--)
        store[word++ & 0x7FFF] = value;
}

void video_store_write_bytes(uint16_t word, int high, const uint8_t *data, unsigned n)
{
    unsigned i;

    for (i = 0; i < n; i++, word++) {
        uint16_t *w = &store[word & 0x7FFF];

        *w = high ? (uint16_t)((*w & 0x00FF) | data[i] << 8) : (uint16_t)((*w & 0xFF00) | data[i]);
    }
}

const uint16_t *video_store(void)
{
    return store;
}

/* the settings and colours carried down to the current line */
static void carry(unsigned to)
{
    while (line < to && line + 1 < VIDEO_LINES) {
        screen[line + 1] = screen[line];
        memcpy(colours[line + 1], colours[line], sizeof colours[0]);
        line++;
    }
}

void video_at_line(unsigned l)
{
    carry(l);
}

void video_colours(uint8_t first, const uint8_t *data, unsigned n)
{
    unsigned i;

    for (i = 0; i < n; i++)
        colours[line][(uint8_t)(first + i)] = (uint16_t)((data[2 * i] | data[2 * i + 1] << 8) & 0x7FFF);
}

void video_colour_at_line(uint8_t n, uint16_t colour)
{
    colours[line][n] = (uint16_t)(colour & 0x7FFF);
}

const uint16_t *video_palette(void)
{
    return colours[line];
}

void video_sprite_table(const uint8_t *table, unsigned first, unsigned bytes)
{
    unsigned i;

    for (i = 0; i < bytes; i++)
        sprites[(first + i) % sizeof sprites] = table[i];
}

void video_sprite_get(unsigned n, VideoSprite *s)
{
    const uint8_t *e = &sprites[4 * (n & 0x7F)];
    uint8_t hi = (uint8_t)(sprites[512 + (n & 0x7F) / 4] >> (2 * (n & 3)));

    s->x = (int16_t)(e[0] | (hi & 1) << 8);
    if (s->x >= 256)
        s->x = (int16_t)(s->x - 512);
    s->y = e[1];
    s->tile = (uint16_t)(e[2] | (e[3] & 1) << 8);
    s->palette = (e[3] >> 1) & 7;
    s->priority = (e[3] >> 4) & 3;
    s->flip_h = (e[3] >> 6) & 1;
    s->flip_v = (e[3] >> 7) & 1;
    s->large = (hi >> 1) & 1;
}

VideoScreen *video_screen(void)
{
    return &screen[line];
}

void video_frame_begin(void)
{
    carry(VIDEO_LINES - 1);
    screen[0] = screen[VIDEO_LINES - 1];
    memcpy(colours[0], colours[VIDEO_LINES - 1], sizeof colours[0]);
    line = 0;
}

void video_screen_at(unsigned l, VideoScreen *s, uint16_t palette[256])
{
    if (l > line)                       /* not changed since: the current line's */
        l = line;
    *s = screen[l];
    memcpy(palette, colours[l], sizeof colours[0]);
}
