/*
 * render.c: the picture drawn from the game's description of it (video.h),
 * a line at a time: the tile layers and the sprites, put in front of one
 * another by their priorities, cut by the windows, the main screen mixed
 * with the sub screen or the fixed colour, then the brightness. The result
 * is 15-bit colour (red in the low bits), for the platform to show.
 *
 * It draws what the game uses: the layer kinds of modes 0, 1 and 3, tiles
 * of 8 or 16, sprites of every size, both windows, colour maths. The game
 * never turns on mosaic, the high-resolution or interlaced modes, mode 7
 * or the per-tile scrolling of modes 2, 4 and 6, so they are not drawn.
 */
#include "video.h"

/* a pixel of one layer: its colour number (0 none) and its priority (the
   higher in front; 0 nothing) */
typedef struct {
    uint8_t colour, priority, group;
} Dot;

/* each mode's layers: bits a pixel (0 none), and the priorities of a
   tile's two levels; the sprites' four */
static const struct {
    uint8_t bits[VIDEO_LAYERS];
    uint8_t layer[VIDEO_LAYERS][2];
    uint8_t sprite[4];
} modes[2][8] = {
    {   /* layer 3 in its usual place */
        { {2, 2, 2, 2}, {{8, 11}, {7, 10}, {2, 5}, {1, 4}}, {3, 6, 9, 12} },
        { {4, 4, 2, 0}, {{6, 9}, {5, 8}, {1, 3}, {0, 0}}, {2, 4, 7, 10} },
        { {0}, {{0}}, {0} },
        { {8, 4, 0, 0}, {{3, 7}, {1, 5}, {0, 0}, {0, 0}}, {2, 4, 6, 8} },
    },
    {   /* mode 1 with layer 3 above all */
        { {2, 2, 2, 2}, {{8, 11}, {7, 10}, {2, 5}, {1, 4}}, {3, 6, 9, 12} },
        { {4, 4, 2, 0}, {{5, 8}, {4, 7}, {1, 10}, {0, 0}}, {2, 3, 6, 9} },
        { {0}, {{0}}, {0} },
        { {8, 4, 0, 0}, {{3, 7}, {1, 5}, {0, 0}, {0, 0}}, {2, 4, 6, 8} },
    },
};

/* the sprites' sizes: small and large, width and height, for each choice */
static const uint8_t sizes[8][2][2] = {
    {{8, 8}, {16, 16}}, {{8, 8}, {32, 32}}, {{8, 8}, {64, 64}}, {{16, 16}, {32, 32}},
    {{16, 16}, {64, 64}}, {{32, 32}, {64, 64}}, {{16, 32}, {32, 64}}, {{16, 32}, {32, 32}},
};

static void layer_line(const VideoScreen *s, int l, unsigned bits, const uint8_t prio[2],
                       unsigned line, Dot out[256])
{
    const uint16_t *store = video_store();
    const VideoLayer *L = &s->layer[l];
    unsigned big = L->large_tiles, shift = 3 + big;
    unsigned wmask = (256u << big << L->map_wide) - 1, hmask = (256u << big << L->map_tall) - 1;
    unsigned y = (line + 1 + L->scroll_y) & hmask;
    unsigned depth = bits == 2 ? 0 : bits == 4 ? 1 : 2;
    unsigned base = (s->mode == 0 ? (unsigned)l << 5 : 0);
    unsigned x;

    for (x = 0; x < 256; x++) {
        unsigned h = (x + L->scroll_x) & wmask;
        unsigned tx = h >> shift, ty = y >> shift;
        uint16_t at = (uint16_t)(L->map + (tx & 31) + ((ty & 31) << 5));
        uint16_t e, tile;
        unsigned row, col, bit, colour = 0, p;

        if (tx & 32)
            at = (uint16_t)(at + (L->map_wide ? 32 << 5 : 0));
        if (ty & 32)
            at = (uint16_t)(at + (L->map_tall ? 32 << (5 + L->map_wide) : 0));
        e = store[at & 0x7FFF];
        tile = e & 0x3FF;
        if (big && ((h & 8) != 0) != ((e >> 14) & 1))
            tile = (uint16_t)(tile + 1);
        if (big && ((y & 8) != 0) != (e >> 15))
            tile = (uint16_t)(tile + 16);
        tile = (uint16_t)((tile + (L->tiles >> (3 + depth))) & (0x7FFF >> (3 + depth)));
        row = (y & 7) ^ (e >> 15 ? 7 : 0);
        col = h & 7;
        bit = (e >> 14) & 1 ? col : 7 - col;
        for (p = 0; p < (1u << depth); p++) {
            uint16_t w = store[((tile << (3 + depth)) + row + 8 * p) & 0x7FFF];

            colour |= ((w >> bit) & 1) << (2 * p);
            colour |= ((w >> (8 + bit)) & 1) << (2 * p + 1);
        }
        out[x].group = (e >> 10) & 7;
        if (colour) {
            out[x].colour = (uint8_t)(base + (out[x].group << (2 << depth)) + colour);
            out[x].priority = prio[(e >> 13) & 1];
        } else
            out[x].colour = out[x].priority = 0;
    }
}

/* the sprites on a line: the first 32 found in the table, of which up to
   34 tiles' widths are fetched (the later sprites' first), the earlier in
   front */
static void sprite_line(const VideoScreen *s, const uint8_t prio[4], unsigned line, Dot out[256])
{
    const uint16_t *store = video_store();
    struct { uint16_t x; uint8_t priority, palette, flip; uint16_t lo, hi; } tiles[34];
    uint8_t found[32];
    unsigned nfound = 0, ntiles = 0, n, i, t, x;

    for (x = 0; x < 256; x++)
        out[x].colour = out[x].priority = 0;
    for (n = 0; n < VIDEO_SPRITES && nfound < 32; n++) {
        VideoSprite sp;
        unsigned w, h, sx;

        video_sprite_get(n, &sp);
        w = sizes[s->sprite_sizes][sp.large][0];
        h = sizes[s->sprite_sizes][sp.large][1];
        sx = (unsigned)sp.x & 511;
        if (sx > 256 && sx + w - 1 < 512)
            continue;
        if ((line >= sp.y && line < sp.y + h) || (sp.y + h >= 256 && line < ((sp.y + h) & 255)))
            found[nfound++] = (uint8_t)n;
    }
    for (i = nfound; i-- > 0;) {
        VideoSprite sp;
        unsigned w, h, y, cols;
        uint16_t base;

        video_sprite_get(found[i], &sp);
        w = sizes[s->sprite_sizes][sp.large][0];
        h = sizes[s->sprite_sizes][sp.large][1];
        y = (line - sp.y) & 0xFF;
        if (sp.flip_v) {
            if (w == h)
                y = h - 1 - y;
            else if (y < w)
                y = w - 1 - y;
            else
                y = w + (w - 1) - (y - w);
        }
        y &= 255;
        base = s->sprite_tiles;
        if (sp.tile & 0x100)
            base = (uint16_t)(base + ((1 + s->sprite_gap) << 12));
        cols = w >> 3;
        for (t = 0; t < cols; t++) {
            unsigned sx = ((unsigned)sp.x + (t << 3)) & 511;
            unsigned mx = sp.flip_h ? cols - 1 - t : t;
            unsigned chr = (((sp.tile >> 4) + (y >> 3)) & 15) << 4 | (((sp.tile & 15) + mx) & 15);
            uint16_t at;

            if (((unsigned)sp.x & 511) != 256 && sx >= 256 && sx + 7 < 512)
                continue;
            if (ntiles == 34)
                break;
            at = (uint16_t)((((uint16_t)(base + (chr << 4))) & 0xFFF0) + (y & 7));
            tiles[ntiles].x = (uint16_t)sx;
            tiles[ntiles].priority = sp.priority;
            tiles[ntiles].palette = (uint8_t)(128 + (sp.palette << 4));
            tiles[ntiles].flip = sp.flip_h;
            tiles[ntiles].lo = store[at & 0x7FFF];
            tiles[ntiles].hi = store[(at + 8) & 0x7FFF];
            ntiles++;
        }
    }
    for (x = 0; x < 256; x++)
        for (t = 0; t < ntiles; t++) {
            int px = (int)x - (int)(tiles[t].x >= 256 ? tiles[t].x - 512 : tiles[t].x);
            unsigned b, colour;

            if (px & ~7)
                continue;
            b = tiles[t].flip ? (unsigned)px : 7 - (unsigned)px;
            colour = ((tiles[t].lo >> b) & 1) | ((tiles[t].lo >> (b + 7)) & 2)
                   | ((tiles[t].hi >> b) & 1) << 2 | ((tiles[t].hi >> (b + 7)) & 2) << 2;
            if (colour) {
                out[x].colour = (uint8_t)(tiles[t].palette + colour);
                out[x].priority = prio[tiles[t].priority];
            }
        }
}

/* whether a window setting holds at x: the two windows, each on or not and
   inverted or not (bit 0 window 1 inverted, 1 on, 2 window 2 inverted, 3
   on), combined by `logic` */
static int windowed(const VideoScreen *s, unsigned use, unsigned logic, unsigned x)
{
    int one = (x >= s->window_left[0] && x <= s->window_right[0]) ^ (use & 1);
    int two = (x >= s->window_left[1] && x <= s->window_right[1]) ^ ((use >> 2) & 1);

    if (!(use & 2))
        return (use & 8) && two;
    if (!(use & 8))
        return one;
    switch (logic) {
    case 0: return one | two;
    case 1: return one & two;
    default: return (one ^ two) == (int)(3 - logic);
    }
}

static uint16_t add(unsigned x, unsigned y, int half)
{
    unsigned sum, carry;

    if (half)
        return (uint16_t)((x + y - ((x ^ y) & 0x0421)) >> 1);
    sum = x + y;
    carry = (sum - ((x ^ y) & 0x0421)) & 0x8420;
    return (uint16_t)((sum - carry) | (carry - (carry >> 5)));
}

static uint16_t subtract(unsigned x, unsigned y, int half)
{
    unsigned diff = x - y + 0x8420;
    unsigned borrow = (diff - ((x ^ y) & 0x8420)) & 0x8420;
    unsigned r = (diff - borrow) & (borrow - (borrow >> 5));

    return (uint16_t)(half ? (r & 0x7BDE) >> 1 : r & 0x7FFF);
}

static uint16_t direct_colour(uint8_t c, uint8_t g)
{
    return (uint16_t)((c << 7 & 0x6000) + (g << 10 & 0x1000) + (c << 4 & 0x0380)
                      + (g << 5 & 0x0040) + (c << 2 & 0x001C) + (g << 1 & 0x0002));
}

void video_render_line(unsigned line, uint16_t out[256])
{
    static Dot dots[VIDEO_LAYERS + 1][256];
    VideoScreen s;
    uint16_t pal[256];
    unsigned l, x, k;
    int third;

    video_screen_at(line, &s, pal);
    if (s.blank || s.mode > 3 || s.mode == 2) {
        for (x = 0; x < 256; x++)
            out[x] = 0;
        return;
    }
    third = s.mode == 1 && s.layer3_priority;
    for (l = 0; l < VIDEO_LAYERS; l++)
        if (modes[third][s.mode].bits[l] && ((s.main_layers | s.sub_layers) >> l & 1))
            layer_line(&s, (int)l, modes[third][s.mode].bits[l], modes[third][s.mode].layer[l], line, dots[l]);
        else
            for (x = 0; x < 256; x++)
                dots[l][x].colour = dots[l][x].priority = 0;
    if ((s.main_layers | s.sub_layers) & 0x10)
        sprite_line(&s, modes[third][s.mode].sprite, line, dots[4]);
    else
        for (x = 0; x < 256; x++)
            dots[4][x].colour = dots[4][x].priority = 0;

    for (x = 0; x < 256; x++) {
        unsigned main_p = 0, sub_p = 0, main_k = 5;
        uint16_t main_c = pal[0], sub_c = pal[0], c;
        int maths, shown, clip;

        for (k = 0; k < 5; k++) {
            const Dot *d = &dots[k][x];
            int cut;
            uint16_t colour;

            if (!d->priority)
                continue;
            cut = windowed(&s, s.window_use[k], s.window_logic[k], x);
            colour = k == 0 && s.colour_direct && s.mode == 3 ? direct_colour(d->colour, d->group) : pal[d->colour];
            if ((s.main_layers >> k & 1) && !(cut && (s.window_layers >> k & 1)) && d->priority > main_p)
                main_p = d->priority, main_c = colour, main_k = k;
            if ((s.sub_layers >> k & 1) && !(cut && (s.window_sub_layers >> k & 1)) && d->priority > sub_p)
                sub_p = d->priority, sub_c = colour;
        }
        if (main_k == 4)
            maths = (s.colour_maths >> 4 & 1) && dots[4][x].colour >= 192;
        else
            maths = s.colour_maths >> main_k & 1;     /* the backdrop: bit 5 */
        {
            int value = windowed(&s, s.window_use[5], s.window_logic[5], x);
            const int pick[4] = { 1, value, !value, 0 };

            shown = pick[s.colour_clip];
            if (!pick[s.colour_prevent])
                maths = 0;
        }
        clip = !shown;
        if (!maths)
            c = clip ? 0 : main_c;
        else {
            int sub = s.colour_sub_screen && sub_p;
            uint16_t other = sub ? sub_c : s.fixed_colour;
            int half = s.colour_half && !clip && (!s.colour_sub_screen || sub_p);

            c = s.colour_subtract ? subtract(clip ? 0 : main_c, other, half) : add(clip ? 0 : main_c, other, half);
        }
        if (s.brightness != 15) {
            unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;

            r = (r * s.brightness * 2 + 15) / 30;
            g = (g * s.brightness * 2 + 15) / 30;
            b = (b * s.brightness * 2 + 15) / 30;
            c = (uint16_t)(b << 10 | g << 5 | r);
        }
        out[x] = c;
    }
}

void video_render(uint16_t out[VIDEO_LINES][256])
{
    unsigned line;

    for (line = 0; line < VIDEO_LINES; line++)
        video_render_line(line, out[line]);
}
