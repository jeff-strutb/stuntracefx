/*
 * fps.c: the pictures drawn a second, shown at the window's top left while
 * the game is worked on (Jeff, 2026-11-11: an FPS counter on screen, the
 * framerate unlocked, to see how the courses run while testing).
 *
 * Counted in draw: every picture the engine asks the game for, over each
 * half second of real time, shown as a whole number. The game draws a
 * picture each time the display shows one (the 2D and the 3D between
 * updates), not tied to the 60 a second its updates run at, so this is the
 * framerate as it is played, up to the display's refresh rate (the engine
 * draws no faster than the display shows).
 *
 * Drawn by the GPU as overlays over everything (nshvc_overlay_set): each
 * digit and the word FPS a small RGBA picture, made once for each size (a
 * 5 x 7 font, white with a dark edge, each font pixel DOT window pixels, the
 * dot from the window's height), placed a pixel for a pixel inside the safe
 * area. SRFX_FPS=0 in the environment leaves it off.
 */
#include <stdlib.h>
#include <string.h>

#include "nshvc/nshvc.h"
#include "system/fps.h"

enum {
    FIRST_OVERLAY = 4,      /* the word at 4, the digits from 5 (the touch controls take 0-3, the pause screen 12 on) */
    DIGITS = 4,             /* shown up to 9999 */
    DOT_MOST = 16,
    GLYPHS = 10             /* the digits; then the word */
};

#define DOTS_HIGH 270.0f    /* the window's height in dots: a 1080-pixel window, a dot of 4 */

static const uint8_t font[13][7] = {
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },
    { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 },       /* F */
    { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 },       /* P */
    { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E },       /* S */
};

/* each size's pictures: the ten digits, then the word; made when first shown */
static struct {
    nshvc_image glyph[GLYPHS + 1];
    int w[GLYPHS + 1], h;
} sizes[DOT_MOST + 1];

/* font character k's cell (x, y) lit; outside it, none */
static int lit(int k, int x, int y)
{
    return x >= 0 && x < 5 && y >= 0 && y < 7 && (font[k][y] >> (4 - x) & 1);
}

/* the characters chars[0..n-1] as one picture, a cell a character apart and
   one of edge round them, each cell dot x dot pixels: a lit cell white, a
   cell beside one (of the eight round it) dark */
static nshvc_image picture(const int *chars, int n, int dot, int *w, int *h)
{
    int cw = n * 6 - 1 + 2, ch = 7 + 2, x, y, px, py, i, k, dx, dy;
    uint32_t *t = (uint32_t *)calloc((size_t)(cw * dot) * (size_t)(ch * dot), 4);
    nshvc_image img = { 0 };

    *w = cw * dot;
    *h = ch * dot;
    if (!t)
        return img;
    for (y = 0; y < ch; y++)
        for (x = 0; x < cw; x++) {
            uint32_t c = 0;

            for (i = 0; i < n && !c; i++) {
                k = chars[i];
                if (lit(k, x - 1 - 6 * i, y - 1))
                    c = 0xFFFFFFFFu;
            }
            for (i = 0; i < n && !c; i++)
                for (dy = -1; dy <= 1 && !c; dy++)
                    for (dx = -1; dx <= 1 && !c; dx++)
                        if (lit(chars[i], x - 1 - 6 * i + dx, y - 1 + dy))
                            c = 0xD0000000u;
            for (py = 0; py < dot; py++)
                for (px = 0; px < dot; px++)
                    t[(size_t)(y * dot + py) * (size_t)(cw * dot) + (size_t)(x * dot + px)] = c;
        }
    img = nshvc_image_create((uint32_t)*w, (uint32_t)*h, NSHVC_IMAGE_RGBA);
    if (img.id)
        nshvc_image_write(img, t);
    free(t);
    return img;
}

static void size_make(int dot)
{
    static const int word[3] = { 10, 11, 12 };
    int k;

    for (k = 0; k < GLYPHS; k++)
        sizes[dot].glyph[k] = picture(&k, 1, dot, &sizes[dot].w[k], &sizes[dot].h);
    sizes[dot].glyph[GLYPHS] = picture(word, 3, dot, &sizes[dot].w[GLYPHS], &sizes[dot].h);
}

static void show(uint32_t id, nshvc_image img, float x, float y, float w, float h)
{
    static const nshvc_image none;
    float c[6] = { x, y, x + w, y, x, y + h };

    nshvc_overlay_set(id, img.id ? img : none, c, img.id ? 1.0f : 0.0f);
}

void fps_hide(void)
{
    static const nshvc_image none;
    static const float c[6] = { 0, 0, 0, 0, 0, 0 };
    uint32_t id;

    for (id = FIRST_OVERLAY; id <= FIRST_OVERLAY + DIGITS; id++)
        nshvc_overlay_set(id, none, c, 0);
}

void fps_draw(void)
{
    static int on = -1;
    static double from = -1;
    static unsigned count, shown;
    double now = nshvc_seconds();
    nshvc_display d;
    char text[8];
    int dot, n, i;
    float x, y;

    if (on < 0) {
        const char *s = getenv("SRFX_FPS");

        on = !(s && !strcmp(s, "0"));
    }
    if (!on)
        return;
    count++;
    if (from < 0) {
        from = now;
        count = 0;
    } else if (now - from >= 0.5) {
        shown = (unsigned)((double)count / (now - from) + 0.5);
        count = 0;
        from = now;
    }
    d = nshvc_display_info();
    if (d.height <= 0)
        return;
    dot = (int)((float)d.height / DOTS_HIGH + 0.5f);
    dot = dot < 2 ? 2 : dot > DOT_MOST ? DOT_MOST : dot;
    if (!sizes[dot].h)
        size_make(dot);
    if (shown > 9999)
        shown = 9999;
    n = 0;
    {
        unsigned v = shown;

        do {
            text[n++] = (char)('0' + v % 10);
            v /= 10;
        } while (v && n < DIGITS);
    }
    /* the digits, most significant first, then a space and the word */
    x = (float)(int)(d.safe_left + 2.0f * (float)dot);
    y = (float)(int)(d.safe_top + 2.0f * (float)dot);
    for (i = 0; i < DIGITS; i++) {
        uint32_t id = (uint32_t)(FIRST_OVERLAY + 1 + i);

        if (i < n) {
            int k = text[n - 1 - i] - '0';

            show(id, sizes[dot].glyph[k], x, y, (float)sizes[dot].w[k], (float)sizes[dot].h);
            x += (float)(sizes[dot].w[k] - dot);        /* the next's edge on this one's: a cell between */
        } else {
            nshvc_image none = { 0 };

            show(id, none, 0, 0, 0, 0);
        }
    }
    x += (float)(3 * dot);                              /* half a character's space */
    show(FIRST_OVERLAY, sizes[dot].glyph[GLYPHS], x, y, (float)sizes[dot].w[GLYPHS], (float)sizes[dot].h);
}
