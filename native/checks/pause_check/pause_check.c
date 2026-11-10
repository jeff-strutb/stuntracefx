/*
 * pause_check.c: the pause screen's art outside the game, built from the
 * game's own pause_art.c and pad_art.c (build.sh).
 *
 *   pause_check pad SCALE OUT        the controller alone, a pixel of its
 *       drawing at SCALE pixels: 1000 SCALE x 600 SCALE, 0xAARRGGBB straight
 *       alpha, raw (check.py holds it against rsvg-convert's)
 *   pause_check panel DENSITY ITEM OUT.ppm   the overlays as the GPU lays them,
 *       ITEM lit (0-11 a control, 12 RESUME, 13 DEFAULT KEYS; -1 none), the
 *       tags with the engine's default keys, over a grey checkerboard, to look at
 *
 * Each says on stderr how long its drawing took.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pause/pause_art.h"

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* the image over the picture at its place, as an overlay (straight alpha) */
static void over(uint32_t *pic, int pw, int ph, const PauseImg *m, float density)
{
    int x, y, ox = (int)(m->x0 * density + 0.5f), oy = (int)(m->y0 * density + 0.5f);

    for (y = 0; y < m->h; y++)
        for (x = 0; x < m->w; x++) {
            uint32_t s = m->px[(size_t)y * (size_t)m->w + (size_t)x], *d;
            float a = (float)(s >> 24) / 255.0f;
            int k;
            uint32_t out = 0xFF000000u;

            if (x + ox < 0 || y + oy < 0 || x + ox >= pw || y + oy >= ph || !a)
                continue;
            d = &pic[(size_t)(y + oy) * (size_t)pw + (size_t)(x + ox)];
            for (k = 0; k < 3; k++) {
                float sv = (float)(s >> (16 - 8 * k) & 0xFF), dv = (float)(*d >> (16 - 8 * k) & 0xFF);
                out |= (uint32_t)(sv * a + dv * (1 - a) + 0.5f) << (16 - 8 * k);
            }
            *d = out;
        }
}

static const char *const k_default_names[PAUSE_CONTROLS] = { "Q", "UP", "LEFT", "RIGHT", "DOWN", "SHIFT",
                                                             "W", "S", "A", "X", "Z", "RETURN" };

int main(int argc, char **argv)
{
    double t0;

    if (argc == 4 && !strcmp(argv[1], "pad")) {
        float s = (float)atof(argv[2]);
        int w = (int)(1000 * s + 0.5f), h = (int)(600 * s + 0.5f);
        uint32_t *px = (uint32_t *)calloc((size_t)w * (size_t)h, 4);
        FILE *f;

        if (!px || s <= 0)
            return 1;
        t0 = now();
        pad_draw(px, w, h, 1, s, 500 * s, 300 * s);
        fprintf(stderr, "pad at %.4f: %d x %d in %.1f ms\n", s, w, h, (now() - t0) * 1e3);
        if (!(f = fopen(argv[3], "wb")) || fwrite(px, 4, (size_t)w * (size_t)h, f) != (size_t)w * (size_t)h)
            return 1;
        fclose(f);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "panel")) {
        float d = (float)atof(argv[2]), x, y, w, h;
        int item = atoi(argv[3]), pw = (int)(PAUSE_PW * d + 0.5f), ph = (int)(PAUSE_PH * d + 0.5f), i;
        uint32_t *pic = (uint32_t *)malloc((size_t)pw * (size_t)ph * 4);
        PauseImg m;
        FILE *f;

        if (!pic || d <= 0)
            return 1;
        for (i = 0; i < pw * ph; i++)
            pic[i] = ((i % pw / 16 + i / pw / 16) & 1) ? 0xFF606060u : 0xFF808080u;
        t0 = now();
        m = pause_img_new(0, 0, PAUSE_PW, PAUSE_PH, d);
        pause_draw_panel(&m);
        fprintf(stderr, "panel at %.2f: %d x %d in %.1f ms\n", d, m.w, m.h, (now() - t0) * 1e3);
        over(pic, pw, ph, &m, d);
        free(m.px);
        for (i = 0; i < PAUSE_CONTROLS; i++) {
            pause_tag_box(i, &x, &y, &w, &h);
            m = pause_img_new(x, y, w, h, d);
            pause_draw_tag(&m, i, k_default_names[i], 0);
            over(pic, pw, ph, &m, d);
            free(m.px);
        }
        if (item >= 0 && item < PAUSE_ITEMS) {
            pause_lit_box(item, &x, &y, &w, &h);
            m = pause_img_new(x, y, w, h, d);
            pause_draw_lit(&m, item);
            over(pic, pw, ph, &m, d);
            free(m.px);
        }
        pause_hint_box(&x, &y, &w, &h);
        m = pause_img_new(x, y, w, h, d);
        pause_draw_hint(&m, "CHOOSE A BUTTON TO CHANGE ITS KEY", 0xF2F2F2, "ESCAPE OR SELECT + START TO RESUME");
        over(pic, pw, ph, &m, d);
        free(m.px);
        if (!(f = fopen(argv[4], "wb")))
            return 1;
        fprintf(f, "P6\n%d %d\n255\n", pw, ph);
        for (i = 0; i < pw * ph; i++) {
            uint8_t rgb[3] = { (uint8_t)(pic[i] >> 16), (uint8_t)(pic[i] >> 8), (uint8_t)pic[i] };
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
        return 0;
    }
    fprintf(stderr, "pause_check pad SCALE OUT | panel DENSITY ITEM OUT.ppm\n");
    return 2;
}
