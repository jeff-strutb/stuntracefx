/*
 * menuwindow.c: Enhanced mode's menus in the whole window (menuwindow.h).
 *
 * The menus' screens (BG mode 1, everything but the race's 3D) on a
 * screen the window's shape (nshvc_ext_screen), the console's picture at
 * its centre exactly as the original's: past its 256 columns and 224
 * lines only the screen's background texture goes on. Every layer but the
 * background's is kept to the console's picture (nshvc_ext_layer_clip),
 * the sprites too: the panels' maps hold the screens they slide to, and
 * the sprites the game parks off its screen would come into view. The
 * background's layer (the one whose map is a repeating texture: the
 * granite, the title's checks) reads, outside the console's picture, a
 * map of that texture alone (nshvc_ext_layer_outside, the map passed as
 * data), so the window is filled
 * with it at its own scroll and the holes its map has for the screen's
 * contents (the 3D car's window) stay inside.
 *
 * The layers' registers are the game's last writes to them, which the
 * front end passes on from the console's write hook (mw_ppu_write).
 */
#include <stdlib.h>
#include <string.h>

#include "machine.h"
#include "hw.h"
#include "console_ext.h"
#include "remaster.h"
#include "rm_options.h"
#include "fullwindow.h"
#include "menuwindow.h"

const uint16_t *nshvc_vram(void);

/* nshvc_ext_layer_outside (nativeshvc 25a9be2): the background's texture
   past the console's picture; MW_LAYER_OUTSIDE=0 shows its own map there
   instead, wrapped */
#ifndef MW_LAYER_OUTSIDE
#define MW_LAYER_OUTSIDE 1
#endif

/* the game's last writes to the PPU's registers $2100-$213F */
static uint8_t reg[0x40];

void mw_ppu_write(uint32_t addr, uint8_t value)
{
    uint16_t a = (uint16_t)addr;
    uint8_t bank = (uint8_t)(addr >> 16);

    if ((bank & 0x7F) < 0x40 && a >= 0x2100 && a < 0x2140)
        reg[a - 0x2100] = value;
}

static int on;                          /* the menus in the whole window */
static int W = 256, H = 224;            /* the screen asked for */
static int bg = -1;                     /* the background's layer (0-2), or -1 */
static uint16_t filler_of[64 * 64];     /* the map the filler was made from */
static int filler_layer = -1, filler_sc = -1;
static int bg_pure;                     /* the background's map is its texture alone (no holes) */

/* the screen for the window: its aspect at the console's 8:7 pixels, the
   console's 256 x 224 at 4:3, wider or taller (even), to 1024 */
#define MW_MAX 1024

static void size_of_window(int *w, int *h)
{
    float ww = 0, wh = 0, a;

    *w = 256;
    *h = 224;
    nshvc_window_size(&ww, &wh);
    if (!(ww > 0 && wh > 0))
        return;
    a = (ww / wh) / (4.0f / 3.0f);
    if (a >= 1.0f)
        *w = (int)(256.0f * a / 2.0f + 0.5f) * 2;
    else
        *h = (int)(224.0f / a / 2.0f + 0.5f) * 2;
    if (*w > MW_MAX)
        *w = MW_MAX;
    if (*h > MW_MAX)
        *h = MW_MAX;
}

/* a layer's map (BGnSC: its word address and size) as cells, 64 x 64 at
   most: screens of 32 x 32, side by side then below */
static int map_cells(int layer, uint16_t *cells, int *mw, int *mh)
{
    const uint16_t *v = nshvc_vram();
    uint8_t sc = reg[0x07 + layer];
    unsigned base = (unsigned)(sc & 0xFC) << 8, r, c;

    *mw = sc & 1 ? 64 : 32;
    *mh = sc & 2 ? 64 : 32;
    for (r = 0; r < (unsigned)*mh; r++)
        for (c = 0; c < (unsigned)*mw; c++) {
            unsigned screen = (c >> 5) + (r >> 5) * (*mw >> 5);
            cells[r * 64 + c] = v[(base + screen * 0x400 + (r & 31) * 32 + (c & 31)) & 0x7FFF];
        }
    return sc;
}

/* a layer's words a character: mode 1 BG1-2 4 bpp and BG3 2, mode 3 BG1
   8 bpp and BG2 4; 0 for none */
static unsigned char_words(int layer)
{
    static const unsigned words[2][3] = {{16, 16, 8}, {32, 16, 0}};
    return words[(reg[0x05] & 7) == 3][layer];
}

/* whether a layer's character has any pixel */
static int char_shows(int layer, uint16_t e)
{
    const uint16_t *v = nshvc_vram();
    unsigned nba = layer < 2 ? (unsigned)(reg[0x0B] >> (4 * layer) & 15) : (unsigned)(reg[0x0C] & 15);
    unsigned words = char_words(layer), at = (nba << 12) + (e & 0x3FF) * words, i;

    for (i = 0; i < words; i++)
        if (v[(at + i) & 0x7FFF])
            return 1;
    return 0;
}

/* A layer's texture: the smallest repeat (px x py cells, each a power of
   two to 32) for which most of the map's cells are their class's most
   common one, the classes the cells at the same place in the repeat; the
   texture those most common cells, each of which must have pixels (a
   layer mostly empty is no background). Returns the share of
   the map that matches (0 none). */
static double texture_of(int layer, uint16_t *tile, int *px, int *py)
{
    static uint16_t cells[64 * 64];
    int mw, mh, x, y, i, j;
    double best = 0;

    map_cells(layer, cells, &mw, &mh);
    for (*py = 1; *py <= 32; *py *= 2)
        for (*px = 1; *px <= 32; *px *= 2) {
            long match = 0;
            int shows = 1;
            for (j = 0; j < *py; j++)
                for (i = 0; i < *px; i++) {
                    /* the class's most common cell: a vote over its cells */
                    static uint16_t seen[64 * 64];
                    static int count[64 * 64];
                    int n = 0, k, top = 0;
                    for (y = j; y < mh; y += *py)
                        for (x = i; x < mw; x += *px) {
                            uint16_t e = cells[y * 64 + x];
                            for (k = 0; k < n && seen[k] != e; k++)
                                ;
                            if (k == n)
                                seen[n] = e, count[n++] = 0;
                            count[k]++;
                        }
                    for (k = 1; k < n; k++)
                        if (count[k] > count[top])
                            top = k;
                    tile[j * 32 + i] = seen[top];
                    match += count[top];
                    shows &= char_shows(layer, seen[top]);
                }
            best = (double)match / (mw * mh);
            if (best >= 0.6 && shows)
                return best;
        }
    return 0;
}

/* the background's texture as a 32 x 32 map, for nshvc_ext_layer_outside
   (copied there: no VRAM written, nothing on the game's bus) */
static uint16_t filler[32 * 32];
static int filler_ok;

static void filler_make(int layer)
{
    static uint16_t tile[32 * 32];
    int px, py, r, c;

    double share = texture_of(layer, tile, &px, &py);

    filler_ok = share > 0;
    bg_pure = share >= 1.0;
    if (!filler_ok)
        return;
    for (r = 0; r < 32; r++)
        for (c = 0; c < 32; c++)
            filler[r * 32 + c] = tile[(r % py) * 32 + c % px];
}

/* the background's layer: of those shown (TM), the one whose map is most
   a repeating texture; kept while it stays one */
static int background(void)
{
    static uint16_t tile[32 * 32];
    int l, px, py, pick = -1;
    double best = 0, s;

    if (bg >= 0 && char_words(bg) && (reg[0x2C] >> bg & 1) && texture_of(bg, tile, &px, &py) > 0)
        return bg;
    for (l = 0; l < 3; l++)
        if (char_words(l) && (reg[0x2C] >> l & 1) && (s = texture_of(l, tile, &px, &py)) > best)
            best = s, pick = l;
    return pick;
}

static void layers(int clip_on, int b)
{
    int l;

    /* the background past the console's picture: its texture (the outside
       map), or until nativeshvc has it its own map wrapped, which only a
       map of the texture alone shows right (the car window's hole would
       repeat there, the car in it); else kept to the picture too */
    for (l = 0; l < 5; l++)
        nshvc_ext_layer_clip(l, clip_on && (l != b || (!MW_LAYER_OUTSIDE && !bg_pure)));
#if MW_LAYER_OUTSIDE
    for (l = 0; l < 4; l++)
        nshvc_ext_layer_outside(l, clip_on && l == b && filler_ok ? filler : NULL, 0);
#endif
}

void mw_frame_end(void)
{
    static uint16_t cells[64 * 64];
    int want, w, h, b, mw, mh, sc, stage;

    /* the menus' screens: the raster stages of the vertical blank's work
       alone ($16 on), and the course grids' ($10 with BGMODE $0B); the
       race's, its intro's and the battles' stages keep the console's */
    stage = nshvc_wram()[0x0D3F];
    want = rm_enhanced() && rm_option(RM_OPT_WIDESCREEN) && !fw_on() &&
           (stage >= 0x16 || (stage == 0x10 && reg[0x05] == 0x0B)) && ((reg[0x05] & 7) == 1 || (reg[0x05] & 7) == 3);
    if (!want && !on)
        return;
    nshvc_hold_interrupts(1);
    nshvc_set_cpu_costs(0);
    if (!want) {                        /* the console's screen again */
        layers(0, -1);
        nshvc_ext_screen(256, 224);
        nshvc_ext_screen_origin(-1, -1);
        fw_screen_lost();
        on = 0;
        bg = filler_layer = filler_sc = -1;
    } else {
        size_of_window(&w, &h);
        if (!on || w != W || h != H) {
            nshvc_ext_screen(w, h);
            nshvc_ext_screen_origin(-1, -1);
            fw_screen_lost();
            W = w;
            H = h;
        }
        b = background();
        if (b >= 0) {
            sc = map_cells(b, cells, &mw, &mh);
            if (b != filler_layer || sc != filler_sc || memcmp(cells, filler_of, sizeof cells)) {
                filler_make(b);
                memcpy(filler_of, cells, sizeof cells);
                filler_layer = b;
                filler_sc = sc;
                layers(1, b);           /* the new texture */
            }
        }
        if (!on || b != bg)
            layers(1, b);
        bg = b;
        on = 1;
    }
    nshvc_set_cpu_costs(1);
    nshvc_hold_interrupts(0);
}
