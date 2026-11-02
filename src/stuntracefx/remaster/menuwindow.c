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
const uint8_t *nshvc_oam(void);

/* nshvc_ext_layer_outside (nativeshvc 25a9be2): the background's texture
   past the console's picture; MW_LAYER_OUTSIDE=0 shows its own map there
   instead, wrapped */
#ifndef MW_LAYER_OUTSIDE
#define MW_LAYER_OUTSIDE 1
#endif

/* the game's last writes to the PPU's registers $2100-$213F */
static uint8_t reg[0x40];
/* BG1-4's scrolls as the game's writes set them (BGnHOFS/BGnVOFS, written
   twice through the PPU's shared latch) */
static uint16_t hofs[4], vofs[4];
static uint8_t ofs_latch;
/* every scroll write in the frame, hashed by layer (the raster interrupt
   sets a band of lines at a time: the frame's last write alone can stay
   the same while a band moves); cleared at each frame's end */
static uint32_t ofs_sig[4];
/* the DMA channels' registers ($4300-$437F) and HDMAEN ($420C) as the
   game wrote them: the menus scroll their panels a band of lines at a
   time by HDMA (MenuHdmaSetupB: channel 7 to BG2HOFS), which no write of
   the S-CPU's shows */
static uint8_t dma_reg[0x80], hdma_en;

void mw_ppu_write(uint32_t addr, uint8_t value)
{
    uint16_t a = (uint16_t)addr;
    uint8_t bank = (uint8_t)(addr >> 16);

    if ((bank & 0x7F) >= 0x40)
        return;
    if (a >= 0x4300 && a < 0x4380)
        dma_reg[a - 0x4300] = value;
    if (a == 0x420C)
        hdma_en = value;
    if (a < 0x2100 || a >= 0x2140)
        return;
    reg[a - 0x2100] = value;
    if (a >= 0x210D && a <= 0x2114) {
        int l = (a - 0x210D) >> 1;
        if (!((a - 0x210D) & 1))
            hofs[l] = (uint16_t)((value << 8 | (ofs_latch & ~7) | (hofs[l] >> 8 & 7)) & 0x3FF);
        else
            vofs[l] = (uint16_t)((value << 8 | ofs_latch) & 0x3FF);
        ofs_latch = value;
        ofs_sig[l] = (ofs_sig[l] ^ (uint32_t)(a << 8 | value)) * 16777619u;
    }
}

/* a signature of the scroll values each layer's HDMA writes this frame:
   every enabled channel whose register is a scroll (BGnHOFS/BGnVOFS), its
   table (direct or indirect) walked through the frame's lines in work RAM
   and its values hashed into its layer's */
static void hdma_scrolls(uint32_t sig[4])
{
    static const uint8_t units[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    int c;

    memset(sig, 0, 4 * sizeof sig[0]);
    for (c = 0; c < 8; c++) {
        const uint8_t *r = dma_reg + c * 16;
        uint32_t at = (uint32_t)r[4] << 16 | (uint32_t)(r[3] << 8 | r[2]), h = 2166136261u;
        unsigned n = units[r[0] & 7], lines = 0, i, k;
        int indirect = r[0] & 0x40;

        if (!(hdma_en >> c & 1) || r[1] < 0x0D || r[1] > 0x14)
            continue;
        while (lines < 240) {
            uint8_t count = nshvc_peek(at);
            unsigned rows = count & 0x7F ? count & 0x7F : 128, take;
            uint32_t data;
            at = (at & 0xFF0000) | ((at + 1) & 0xFFFF);
            if (!count)
                break;
            take = count & 0x80 ? n * rows : n;     /* repeat: new data each line */
            if (indirect) {
                data = (uint32_t)r[7] << 16 | (uint32_t)(nshvc_peek(at) | nshvc_peek((at & 0xFF0000) | ((at + 1) & 0xFFFF)) << 8);
                at = (at & 0xFF0000) | ((at + 2) & 0xFFFF);
            } else {
                data = at;
                at = (at & 0xFF0000) | ((at + take) & 0xFFFF);
            }
            for (i = 0; i < take; i++)
                h = (h ^ nshvc_peek((data & 0xFF0000) | ((data + i) & 0xFFFF))) * 16777619u;
            h = (h ^ rows) * 16777619u;
            lines += rows;
        }
        k = (unsigned)(r[1] - 0x0D) >> 1;
        sig[k] = (sig[k] ^ h) * 16777619u;
    }
}

uint8_t mw_ppu_last(uint16_t addr)
{
    return addr >= 0x2100 && addr < 0x2140 ? reg[addr - 0x2100] : 0;
}

static int on;                          /* the menus in the whole window */
static int W = 256, H = 224;            /* the screen asked for */

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

/* One 32 x 32 screen of a layer's map (its cells from x0, y0): the
   smallest repeat (px x py cells, each a power of two to 32) for which
   most of its cells are their class's most common one, the classes the
   cells at the same place in the repeat; the texture those most common
   cells, each of which must have pixels (a layer mostly empty is no
   background). Returns the share of the screen that matches (0 none). */
static double screen_texture(int layer, const uint16_t *cells, int x0, int y0, uint16_t *tile, int *px, int *py)
{
    int x, y, i, j;
    double share;

    for (*py = 1; *py <= 32; *py *= 2)
        for (*px = 1; *px <= 32; *px *= 2) {
            long match = 0;
            int shows = 1;
            for (j = 0; j < *py; j++)
                for (i = 0; i < *px; i++) {
                    /* the class's most common cell: a vote over its cells */
                    static uint16_t seen[32 * 32];
                    static int count[32 * 32];
                    int n = 0, k, top = 0;
                    for (y = j; y < 32; y += *py)
                        for (x = i; x < 32; x += *px) {
                            uint16_t e = cells[(y0 + y) * 64 + x0 + x];
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
            share = (double)match / (32 * 32);
            if (share >= 0.6 && shows)
                return share;
        }
    return 0;
}

/* A layer's texture: the best of its map's screens (screen_texture). A
   map of two or four screens holds the screen shown and the ones the
   menus slide to (a panel coming in), so the texture is judged a screen
   at a time, not over the whole map, which would fall short of a texture
   while a panel waits in the other half. Returns the share of the best
   screen that matches (0 none). */
static double texture_of(int layer, uint16_t *tile, int *px, int *py)
{
    static uint16_t cells[64 * 64], t[32 * 32];
    int mw, mh, sx, sy, tx, ty;
    double best = 0, s;

    map_cells(layer, cells, &mw, &mh);
    for (sy = 0; sy < mh; sy += 32)
        for (sx = 0; sx < mw; sx += 32)
            if ((s = screen_texture(layer, cells, sx, sy, t, &tx, &ty)) > best) {
                best = s;
                memcpy(tile, t, sizeof t);
                *px = tx;
                *py = ty;
            }
    return best;
}

/* each background layer's texture as a 32 x 32 map, for
   nshvc_ext_layer_outside (copied there: no VRAM written, nothing on the
   game's bus): every layer shown whose map is a repeating texture gets
   one, not only the frontmost, as the menus cross-fade from one texture
   to another (the title's checks on BG2 into the granite on BG1) with
   colour math, which past the console's picture needs both */
static uint16_t filler[3][32 * 32];
static int filler_ok[3], filler_pure[3];
static int set_ok[3] = {-1, -1, -1};          /* what the console was last given */
static int set_clip = -1;
/* a layer that is no texture (a panel's) is kept to the console's picture
   while it rests, as its map holds the screens the menus slide to beside
   the one shown; while it slides (its scroll moved this frame or the last)
   it goes on past the picture, so a panel comes in from the window's edge
   and goes out at it rather than at the console's */
static int sliding[3], set_sliding[3] = {-1, -1, -1};
static uint16_t set_filler[3][32 * 32];

static void filler_make(int layer)
{
    static uint16_t tile[32 * 32];
    int px, py, r, c;
    double share = 0;

    if (char_words(layer) && (reg[0x2C] >> layer & 1))
        share = texture_of(layer, tile, &px, &py);
    filler_ok[layer] = share > 0;
    filler_pure[layer] = share >= 1.0;
    if (!filler_ok[layer])
        return;
    for (r = 0; r < 32; r++)
        for (c = 0; c < 32; c++)
            filler[layer][r * 32 + c] = tile[(r % py) * 32 + c % px];
}

/* the layers past the console's picture: a texture's layer reads its
   texture there (the outside map), or until nativeshvc has it its own map
   wrapped, which only a map of the texture alone shows right (the car
   window's hole would repeat there, the car in it); every other layer is
   kept to the picture. Passed on only when it changed. */
static void layers(int clip_on)
{
    int l, ok;

    for (l = 0; l < 5; l++) {
        int slide = l < 3 ? sliding[l] : l == 4 && (sliding[0] || sliding[1] || sliding[2]);
        /* sliding, a texture's layer too shows its own map there: the
           panel coming in on it with the texture (the granite and GAME
           SELECTION, both on BG1) */
        ok = l < 3 && clip_on && filler_ok[l] && !slide && (MW_LAYER_OUTSIDE || filler_pure[l]);
        if (l < 3 && ok == set_ok[l] && clip_on == set_clip && slide == set_sliding[l] &&
            (!ok || !memcmp(set_filler[l], filler[l], sizeof filler[l])))
            continue;
        nshvc_ext_layer_clip(l, clip_on && !ok && !slide);
#if MW_LAYER_OUTSIDE
        if (l < 4)
            nshvc_ext_layer_outside(l, ok ? filler[l] : NULL, 0);
#endif
        if (l < 3) {
            set_ok[l] = ok;
            set_sliding[l] = slide;
            memcpy(set_filler[l], filler[l], sizeof filler[l]);
        }
    }
    set_clip = clip_on;
}

/* the sprites while a panel slides: shown past the console's picture (the
   panels' letters and arrows are sprites), and one just past its right
   edge (9-bit X 256 on, which the console reads as off its left) moved a
   page right (nshvc_ext_sprite_page), so it comes in with its panel from
   the window's right edge; at rest every sprite is back on page 0 and
   kept to the picture, where the game parks the ones it hides */
static uint8_t sprite_paged[128];

static void sprite_pages(int sliding_now)
{
    const uint8_t *oam = nshvc_oam();
    int i, side = (W - 256) / 2;

    for (i = 0; i < 128; i++) {
        unsigned x = oam[i * 4] | (unsigned)(oam[512 + i / 4] >> (i % 4 * 2) & 1) << 8;
        int page = sliding_now && x >= 256 && x < 256u + (unsigned)side && oam[i * 4 + 1] < 224;
        if (page != sprite_paged[i]) {
            nshvc_ext_sprite_page(i, page, 0);
            sprite_paged[i] = (uint8_t)page;
        }
    }
}

void mw_frame_end(void)
{
    int want, w, h, l, stage;
    uint32_t sig[4];

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
        memset(filler_ok, 0, sizeof filler_ok);
        memset(sliding, 0, sizeof sliding);
        sprite_pages(0);
        layers(0);
        nshvc_ext_screen(256, 224);
        nshvc_ext_screen_origin(-1, -1);
        fw_screen_lost();
        on = 0;
    } else {
        size_of_window(&w, &h);
        if (!on || w != W || h != H) {
            nshvc_ext_screen(w, h);
            nshvc_ext_screen_origin(-1, -1);
            fw_screen_lost();
            W = w;
            H = h;
        }
        hdma_scrolls(sig);
        for (l = 0; l < 4; l++)
            sig[l] = (sig[l] ^ ofs_sig[l]) * 16777619u;
        for (l = 0; l < 3; l++) {
            static uint16_t last_h[3], last_v[3];
            static uint32_t last_sig[3];
            static int moved[3];
            int now = hofs[l] != last_h[l] || vofs[l] != last_v[l] || sig[l] != last_sig[l];
            last_sig[l] = sig[l];
            ofs_sig[l] = 0;
            sliding[l] = on && (now || moved[l]);
            moved[l] = now;
            last_h[l] = hofs[l];
            last_v[l] = vofs[l];
            filler_make(l);
        }
        sprite_pages(sliding[0] || sliding[1] || sliding[2]);
        layers(1);
        on = 1;
    }
    nshvc_set_cpu_costs(1);
    nshvc_hold_interrupts(0);
}
