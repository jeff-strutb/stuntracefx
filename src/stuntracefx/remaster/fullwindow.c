/*
 * fullwindow.c: Enhanced mode's full-window race (fullwindow.h).
 *
 *   the screen   W x H console pixels from the window's aspect
 *                (nshvc_ext_screen): 256 x 224 at 4:3, wider at 224
 *                lines, or 256 across and taller with the original's
 *                lines its top rows (nshvc_ext_screen_origin);
 *   BG1          the Super FX's picture (the 3D and the HUD's pieces) and
 *                the panels, at the window's resolution: the canvas over
 *                the whole screen (nshvc_ext_canvas_screen); its window off;
 *   BG2          the hills, the console's: the game's map and characters
 *                on page 1 (bg2_prepare), the rows its bezel hid the band's
 *                own sky, at the game's scroll;
 *   the sprites  the console's, the HUD's placed at the screen's edges
 *                (sprites_place);
 *   the texels   (W k) x (H k), drawn by the redraw with the view over
 *                the whole screen (fw_view_words), handed to the canvas
 *                each frame of the race's wait (fw_picture);
 *   the uploads  the race's raster stage that blanks the screen's last
 *                lines for its uploads runs in the vertical blank instead
 *                and leaves the screen on, so every line shows the picture.
 *
 * The view: its centre W / 2 across and the original's (32 + 64 = line 96)
 * down, half the rows a taller screen adds lower, focal length 128, the
 * original's, whatever the size: the 3D at the console's scale, so the
 * console's hills turn with it, and the field of view grows with the
 * window across and down; all of it k times in texels.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__APPLE__)
#include <execinfo.h>
#include <mach-o/dyld.h>
#define FW_CENSUS_BT 1
#endif

#include "machine.h"
#include "hw.h"
#include "gsu.h"
#include "gsu_ops.h"
#include "console_ext.h"
#include "remaster.h"
#include "fullwindow.h"
#include "scene.h"
#include "widescreen.h"
#include "menuwindow.h"
#include "rm_options.h"

uint64_t nshvc_wait_until(int (*ready)(void *ctx), void *ctx);
uint32_t nshvc_frames(void);
uint64_t nshvc_cart_clock(void);
const uint16_t *nshvc_vram(void);
const uint16_t *nshvc_cgram(void);

int fw_drawing, fw_hud;

/* getenv, each name looked up once (the environment is the process's from
   its start; the checks' switches are asked on paths run per face and per
   span, where a host's getenv scans the environment each time) */
static const char *fw_env(const char *name)
{
    static struct { const char *name; const char *value; } kept[48];
    static int nkept;
    int i;
    for (i = 0; i < nkept; i++)
        if (kept[i].name == name || !strcmp(kept[i].name, name))
            return kept[i].value;
    if (nkept < 48) {
        kept[nkept].name = name;
        kept[nkept].value = getenv(name);
        return kept[nkept++].value;
    }
    return getenv(name);
}

static int on;                          /* the full-window race this frame */
static int seen;                        /* the race screen's stages ran this frame */
static int unseen = 2;                  /* frames since they last did */
static int W = 256, H = 224;            /* the screen's width and height in console pixels */
static int tw, th;                      /* the texels' size */
/* texels a console pixel, whole or not: SXN / SXD across and SYN / SYD
   down (the canvas's tw / W and th / H; the checks' pictures keep them
   while their texels' size changes) */
static int64_t SXN = 256, SXD = 256, SYN = 224, SYD = 224;
static uint8_t *tex;
static size_t tex_size;
static int handed;                      /* the canvas is the full window's */
/* the picture in the GPU's memory (fw_canvas_gpu): this picture's
   (gpu_pic), opened there (gpu_open), from a cleared screen (gpu_cleared) */
const FwCanvasGpu *fw_canvas_gpu;
static int gpu_pic, gpu_open, gpu_cleared;
#ifdef SRFX_NSHVC_HANDOFF
/* the hand-off (nshvc_handoff.h): this file's picture goes to the console as the
   GPU's own (hand_pic), the texels neither written nor read here; handed_now, one
   handed since the last frame hook */
const FwHandoff *fw_handoff;
int fw_tex_align = 1;
int fw_gpu_settled;
int fw_ground_handed;
static int handed_now;
static int hand_pic(void);
#define CANVAS_SCALE() rm_canvas_scale()
#else
#define CANVAS_SCALE() nshvc_ext_canvas_scale()
#endif
static int staged;                    /* this frame's upload stage ran in the full window */

/* the bitmap pictures (fullwindow.h): Enhanced's at k (bp: its shape's columns
   and rows of characters, bits a pixel, targets wanted and done, the one open;
   the game's view words while the frame program has k's; its centre in
   texels), a job of Original's (gx: its target valid, its shape), the screen's
   low-nibble rule (bit4) */
static int bp, bp_cols, bp_rows, bp_bpp, bp_targets, bp_done, bp_open_now;
static uint16_t bp_game[8];
static int bp_cx, bp_cy;
static int menu_due;
static int gx, gx_valid;
static FwBits gx_shape;
static FwJob job;
static uint32_t ram_mask = 0x1FFFF;
static int bit4;
static int scale_lost;                  /* tw, th and the scale theirs: the full window's made again when it comes back */
int fw_silent;                          /* (fullwindow.h) */
static int unplotted;
static int still_word = -1, still_tries;    /* the full window gone: the GPU's bitmap picture due at this VRAM word (fw_frame_end) */
int fw_unplotted(void) { return unplotted; }
int fw_bp_active(void) { return bp; }
int fw_nibble(void) { return bit4; }
/* Two players in the full window: one picture, each player's view on a rectangle
   of it (two_layout), drawn in their order. The screen's shape chooses: left and
   right once each column holds the original's 144 columns, else top and bottom,
   made again as the window changes. */
#define TWO_BOX_W 80                    /* a player's boxes' place (panels_two) */
#define TWO_BOX_H 96
#define TWO_SEAM 2                      /* console pixels between the players' views */
#define TWO_SIDE_W 288                  /* the screen's width from which they are side by side */
static int two;                         /* this frame's full window is two players' */
static int two_p;                       /* the player being drawn */
static int two_side, two_w, two_h, two_x[2], two_y[2];  /* each view on the screen, console pixels */
int fw_clip[4] = { 0, 0, -1, -1 };      /* (fullwindow.h) */
uint8_t fw_hi(void) { return two ? 0x20 : gsu.colr & 0xF0; }
uint8_t fw_face_colour(uint8_t c) { return two ? (c & 0x0F ? (uint8_t)(0x20 | (c & 0x0F)) : 0) : c; }
#define POR_NOW() (two ? fw_por() : gsu.por)
uint8_t fw_por(void) { return (uint8_t)(gsu.por | (bit4 ? 8 : 0)); }
void fw_ram_size(uint32_t size) { ram_mask = size - 1; }
const FwJob *fw_job(void) { return &job; }
void (*fw_bits_filler)(uint8_t *tex, uint8_t *ram, const FwBits *b, int pack) = fw_bits_cpu;

static void panels_draw(void);
static void sprites_write(void);
static void span_flush(void);
static void span_flat(int32_t y, int32_t x0, int32_t x1, uint8_t c);
static int page1_known;                 /* (bg2_prepare's) */
static void sprites_place(void);
static void screen_set(void);
static void takeover(void);

/* the view on the original's screen: its column 0 and row 0 (BG1's map
   names the view's character 0 at row 2, column 3, under the race's
   scroll), its size, and its centre line VIEW_Y + 64 */
#define VIEW_X 24
#define VIEW_Y 32
#define VIEW_W 208
#define VIEW_H 128
#define VIEW_CY 96

/* the view's centre line on this screen: the original's, the rows a taller
   screen adds shared above and below it */
static int view_cy(void);
/* FW_CCHECK's larger picture: texels added above the screen (its centre
   that much lower) */
static int cy_off;
static int view_cy(void) { return bp ? bp_cy : two ? two_y[two_p] + two_h / 2 : VIEW_CY + (H - 224) / 2; }

int fw_width(void) { return tw; }
int fw_height(void) { return th; }

/* a / b rounded down (b > 0) */
static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return q - (a % b != 0 && a < 0);
}

/* the first texel column of console column x, and the first texel row of
   console row y: console column x covers texel columns col_at(x) to
   col_at(x + 1) - 1 (SXN / SXD of them, whole or not), row y rows
   row_at(y) to row_at(y + 1) - 1 */
static int col_at_(int64_t x) { return (int)floor_div(x * SXN, SXD); }
static int row_at_(int64_t y) { return (int)floor_div(y * SYN, SYD); }
/* the same from tables over the screen's own columns and rows (0 to W and
   0 to H; 64-bit division is a library call on a 32-bit host), made again
   when the scale changes */
#define AT_MAX 1025
static int col_tab[AT_MAX], row_tab[AT_MAX];
static int64_t tab_key[4] = { -1, -1, -1, -1 };
static void at_tables(void)
{
    int i;
    if (tab_key[0] == SXN && tab_key[1] == SXD && tab_key[2] == SYN && tab_key[3] == SYD)
        return;
    tab_key[0] = SXN, tab_key[1] = SXD, tab_key[2] = SYN, tab_key[3] = SYD;
    for (i = 0; i < AT_MAX; i++)
        col_tab[i] = col_at_(i), row_tab[i] = row_at_(i);
}
static int col_at(int64_t x)
{
    at_tables();
    return x >= 0 && x < AT_MAX ? col_tab[x] : col_at_(x);
}
static int row_at(int64_t y)
{
    at_tables();
    return y >= 0 && y < AT_MAX ? row_tab[y] : row_at_(y);
}
int fw_col_at(int x) { return col_at(x); }
/* the view's centre column in texels: the screen's middle, or its player's */
static int view_cx(void) { return bp ? bp_cx : two ? col_at(two_x[two_p] + two_w / 2) : tw / 2; }
int fw_row_at(int y) { return row_at(y); }

/* the view's centre line in texels (kept: its inputs rarely change, and
   its 64-bit division is a library call on a 32-bit host) */
static float centre_f;                  /* (float)centre_row(), kept with it */
static int64_t centre_row(void)
{
    static int64_t at, key[4] = { -1, -1, -1, -1 };
    int64_t vc = view_cy();
    if (key[0] != vc || key[1] != SYN || key[2] != SYD || key[3] != cy_off) {
        key[0] = vc, key[1] = SYN, key[2] = SYD, key[3] = cy_off;
        at = (int64_t)row_at(vc) + cy_off;
        centre_f = (float)at;
    }
    return at;
}
static float centre_rowf(void) { centre_row(); return centre_f; }

/* a console pixel's texels set to c: its block, columns col_at(x) on and
   rows row_at(y) on */
static void block_set(int x, int y, uint8_t c)
{
    int x0 = col_at(x), x1 = col_at(x + 1), y0 = row_at(y), y1 = row_at(y + 1), j;
#ifdef SRFX_NSHVC_HANDOFF
    if (hand_pic()) {                   /* (Metal, Vulkan: no fw_canvas_gpu, the same) */
        for (j = y0; j < y1; j++)
            if (x1 > x0)
                span_flat(j, x0, x1 - 1, c);
        return;
    }
#endif
    if (gpu_pic || bp) {                /* drawn in its order with the rest, as spans */
        for (j = y0; j < y1; j++)
            if (x1 > x0)
                span_flat(j, x0, x1 - 1, c);
        return;
    }
    for (j = y0; j < y1; j++)
        memset(tex + (size_t)j * tw + x0, c, (size_t)(x1 - x0));
}

/* a console pixel's texel the console's picture takes: the one under its centre */
static uint8_t block_get(int x, int y)
{
    return tex[(size_t)floor_div((2 * (int64_t)y + 1) * SYN, 2 * SYD) * tw + floor_div((2 * (int64_t)x + 1) * SXN, 2 * SXD)];
}

int fw_on(void) { return on; }

void fw_race_stage(void) { seen = 1; }

/* ---- PLOT into the texels ---------------------------------------------- */

/* the colour PLOT writes, or -1 when it writes nothing: the screen's 8 bits
   a pixel (the race's), colour 0 (or a zero low nibble with the high one
   frozen) transparent unless the options say opaque */
static int plot_colour(void)
{
    uint8_t c = gsu.colr;

    if (two)                            /* its bitmap's characters: 4 bits, palette 2 (BG1's map), a 0 clear */
        return c & 0x0F ? 0x20 | (c & 0x0F) : -1;
    if (!(gsu.por & 0x01)) {
        if ((gsu.por & 0x08) || bit4) {
            if (!(c & 0x0F))
                return -1;
        } else if (!c) {
            return -1;
        }
    }
    return c;
}

/* a HUD piece's pixel (the original view's x, y) to the console pixel it
   shows at: the view's place on the original's screen, centred across */
static int checking;                    /* FW_CHECK's second pass: the original's view, in place */
static void check_alias(int32_t x, int32_t y, int c);
static int hud_x(int x) { return checking ? x : x + VIEW_X + (W - 256) / 2; }
static int hud_y(int y) { return checking ? y : y + VIEW_Y; }

/* The HUD anchored to the edges of its box: the screen's middle, at most
   16:9 wide (UI_W_MAX: 16:9 at 224 lines is 341 1/3), so that on a wider
   screen it does not sit at the far edges. A piece of the original's
   screen spanning columns x0 to x1 (the original's 256) moves, on a
   screen wider than 256, to the box's left edge when it lies left of the
   screen's middle, to its right edge when right of it, and stays centred
   when across it: the columns it is shifted by from the original's
   screen to this one */
#define UI_W_MAX 340

/* the window's safe area (nshvc_safe_area: a phone's notch, rounded
   corners, home bar) in console pixels, left, top, right, bottom; the
   HUD's edges kept inside it. FW_SAFE=l,t,r,b (points) sets it for a
   check. */
static int safe_px[4];

static void safe_area_update(void)
{
    float in[4] = {0, 0, 0, 0}, ww = 0, wh = 0, per;
    const char *e = fw_env("FW_SAFE");
    int i;

    nshvc_safe_area(&in[0], &in[1], &in[2], &in[3]);
    if (e)
        sscanf(e, "%f,%f,%f,%f", &in[0], &in[1], &in[2], &in[3]);
    nshvc_window_size(&ww, &wh);
    per = ww > 0 && wh > 0 ? (W / ww < H / wh ? W / ww : H / wh) : 0;
    for (i = 0; i < 4; i++)
        safe_px[i] = in[i] > 0 ? (int)ceilf(in[i] * per) : 0;
}

static int anchor_shift(int x0, int x1)
{
    int ox = (W - 256) / 2, margin = W > UI_W_MAX ? (W - UI_W_MAX) / 2 : 0;
    if (x1 < 128 - 8)
        return margin > safe_px[0] ? margin : safe_px[0];
    if (x0 > 128 + 8)
        return 2 * ox - (margin > safe_px[2] ? margin : safe_px[2]);
    return ox;
}

/* The same for the HUD inside the view, which the original drew against
   its bezel: a piece left of the screen's middle moves so that the view's
   left edge (column VIEW_X) is at the box's left edge, one right of it so
   that the view's right edge is at the box's right edge. In a window up to
   16:9 the box's edges are the window's, so a piece the bezel cut (the
   start lamps) is cut by the window's edge there, and what the bezel hid
   is off the screen. */
/* Two players: where the view's own (144 x 80) top left goes for a piece of it
   (lx0 to lx1, ly0 to ly1 on the view): by its middle, to the near side of its
   player's rectangle, or about the rectangle's middle */
/* Each box floating on its own, its digits and gauge with it (panels_two: all one
   picture on the GPU), its place on the original's (TWO_BOX_W x TWO_BOX_H from
   two_bx, two_by: DAMAGE, the map, the speed, TIME; the second player's map and speed
   the other way round) to its place on its player's rectangle, scaled together: top
   and bottom, down the rectangle's right side as the original's column; left and
   right, in a row along its bottom. two_q: the scale, 256ths: at most 0.6, the row
   within 70% of the rectangle's width, the column within 80% of its height and 30%
   of its width; no less than a source pixel a window pixel while that fits. */
static const int two_sx[2][4] = { { 0, 0, 32, 0 }, { 0, 48, 0, 0 } }, two_sy[4] = { 0, 32, 32, 64 };
static const int two_sw[4] = { 80, 32, 48, 80 };
/* the map's sprites run past its place, a row above and two below, over their
   neighbours': they have their own rows of the texture, from TWO_MAP_Y */
#define TWO_MAP_Y 100
#define TWO_MAP_TOP 31
#define TWO_MAP_H 35
#define TWO_BOX_ROW 32
#define TWO_BOX_GAP 2
#define TWO_ROW_W (240 + 3 * TWO_BOX_GAP)
static int two_q(void)
{
    int q = 154, a, fit, least = (int)(256 * SXD / SXN);
    if (two_side) {
        a = 256 * 7 * two_w / (10 * TWO_ROW_W);
        fit = 256 * two_w / TWO_ROW_W;
    } else {
        a = 256 * 8 * two_h / (10 * TWO_BOX_H);
        if (a > 256 * 3 * two_w / (10 * TWO_BOX_W))
            a = 256 * 3 * two_w / (10 * TWO_BOX_W);
        fit = 256 * two_h / TWO_BOX_H;
    }
    if (q > a)
        q = a;
    if (q < least)
        q = least;
    if (q > fit)
        q = fit;
    return q > 256 ? 256 : q;
}
/* n source pixels' texels across and down, at the scale */
static int two_tx(int n, int q) { return (int)((int64_t)n * q * SXN / (256 * SXD)); }
static int two_ty(int n, int q) { return (int)((int64_t)n * q * SYN / (256 * SYD)); }
/* the rows the boxes take at the rectangle's bottom, console pixels */
static int two_band(void) { return two_side ? (TWO_MAP_H * two_q() + 255) / 256 + TWO_BOX_GAP : 0; }
/* box b's texels */
static void two_box_place(int p, int b, int *x, int *y, int *w, int *h)
{
    int ml = two_x[p] == 0 ? safe_px[0] : 0, mr = two_x[p] + two_w == W ? safe_px[2] : 0;
    int mt = two_y[p] == 0 ? safe_px[1] : 0, mb = two_y[p] + two_h == H ? safe_px[3] : 0;
    int left = col_at(two_x[p] + ml), right = col_at(two_x[p] + two_w - mr), top = row_at(two_y[p] + mt), bottom = row_at(two_y[p] + two_h - mb);
    int q = two_q(), wide = two_tx(TWO_BOX_W, q), gap = two_tx(TWO_BOX_GAP, q);

    *w = two_tx(two_sw[b], q);
    *h = two_ty(b == 1 ? TWO_MAP_H : TWO_BOX_ROW, q);
    if (!two_side) {
        *x = right - wide + two_tx(two_sx[p][b], q);
        *y = top + two_ty(b == 1 ? TWO_MAP_TOP : two_sy[b], q);
        return;
    }
    *y = bottom - *h;
    *x = b == 0 ? left : b == 1 ? left + wide + gap : b == 2 ? right - wide - gap - *w : right - wide;
}
static void two_anchor(int lx0, int lx1, int ly0, int ly1, int *ox, int *oy)
{
    int p = two_p, cx = (lx0 + lx1) / 2, cy = (ly0 + ly1) / 2;
    int ml = two_x[p] == 0 ? safe_px[0] : 0, mr = two_x[p] + two_w == W ? safe_px[2] : 0;
    int mt = two_y[p] == 0 ? safe_px[1] : 0, mb = two_y[p] + two_h == H ? safe_px[3] : 0;

    *ox = cx < 64 ? two_x[p] + ml : cx > 80 ? two_x[p] + two_w - 144 - mr : two_x[p] + (two_w - 144) / 2;
    *oy = cy < 32 ? two_y[p] + mt : cy > 48 ? two_y[p] + two_h - 80 - mb : two_y[p] + (two_h - 80) / 2;
    if (two_side) {                     /* the bottom's pieces above the boxes' row */
        if (cy > 48)
            *oy -= two_band();
    } else {                            /* clear of the boxes' column, where a short rectangle would put it under them */
        int q = two_q(), bx = two_x[p] + two_w - mr - TWO_BOX_W * q / 256, by = two_y[p] + mt;
        if (*oy + ly0 < by + TWO_BOX_H * q / 256 && *oy + ly1 >= by && *ox + lx1 >= bx)
            *ox = bx - 2 - lx1;
    }
}

static int view_shift(int x0, int x1)
{
    int margin = W > UI_W_MAX ? (W - UI_W_MAX) / 2 : 0;
    if (x1 < 128 - 8)
        return (margin > safe_px[0] ? margin : safe_px[0]) - VIEW_X;
    if (x0 > 128 + 8)
        return W - (margin > safe_px[2] ? margin : safe_px[2]) - (VIEW_X + VIEW_W);
    return (W - 256) / 2;
}

/* whether the original's view showed a pixel of its screen (the bezel
   around it, its window, hid the rest) */
static int in_view(int x, int y)
{
    return x >= VIEW_X && x < VIEW_X + VIEW_W && y >= VIEW_Y && y < VIEW_Y + VIEW_H;
}

/* the same down a screen taller than 224 (the original's lines its top
   rows), by the piece's middle: above the view's centre line it stays,
   below it it goes to the bottom edge, about it (the race's messages
   there) it goes with the view's centre */
static int anchor_shift_y(int y0, int y1)
{
    int oy = H - 224, c = (y0 + y1) / 2;
    if (c < VIEW_CY - 16)
        return safe_px[1];
    if (c > VIEW_CY + 16)
        return oy - safe_px[3];
    return oy / 2;
}

/* The HUD's pieces as whole pieces: while the HUD draws, each Super FX
   program's pixels go to a layer of the original's screen (hud_px, the
   colour + 1; hud_id, the program's piece) and, as the HUD ends
   (fw_hud_end), onto the texels: the pieces near each other (a message
   one program a word) joined, each join by its columns' and rows' anchor
   (view_shift). Only the pixels the original's view showed (in_view) go
   onto the texels, and only they make a piece's place: the HUD parks
   pieces under the bezel before they slide in. */
#define HUD_PIECES 96
#define HUD_GAP 24                      /* pieces this near across (on rows they share) are one: a message's words */
static uint16_t hud_px[224][256];
static uint8_t hud_id[224][256];
static int hud_run, hud_x0, hud_x1, hud_y0, hud_y1, hud_vx0, hud_vx1, hud_vy0, hud_vy1;
/* each piece's pixels' columns and rows (hp_*), and those of its pixels in
   the view (hv_*; hv_x1 < 0: none) */
static int hud_n, hp_x0[HUD_PIECES], hp_x1[HUD_PIECES], hp_y0[HUD_PIECES], hp_y1[HUD_PIECES], hp_up[HUD_PIECES];
static int hv_x0[HUD_PIECES], hv_x1[HUD_PIECES], hv_y0[HUD_PIECES], hv_y1[HUD_PIECES];

static void gx_begin(void);
static void gx_end(void);

void fw_gsu_program(int begin)
{
    if (begin) {
        gx_begin();
        hud_run = fw_hud && !checking && !bp && hud_n < HUD_PIECES;
        hud_x0 = hud_y0 = hud_vx0 = hud_vy0 = 0x7FFF;
        hud_x1 = hud_y1 = hud_vx1 = hud_vy1 = -1;
        return;
    }
    gx_end();
    if (!hud_run)
        return;
    hud_run = 0;
    if (hud_x1 < 0)
        return;
    hp_x0[hud_n] = hud_x0;
    hp_x1[hud_n] = hud_x1;
    hp_y0[hud_n] = hud_y0;
    hp_y1[hud_n] = hud_y1;
    hv_x0[hud_n] = hud_vx0;
    hv_x1[hud_n] = hud_vx1;
    hv_y0[hud_n] = hud_vy0;
    hv_y1[hud_n] = hud_vy1;
    hp_up[hud_n] = hud_n;
    hud_n++;
}

static int hud_root(int i)
{
    while (hp_up[i] != i)
        i = hp_up[i] = hp_up[hp_up[i]];
    return i;
}

/* The HUD's bitmaps (fw_hud, its pieces being made): each kept with its
   piece, in the HUD's layer (the original's screen, the view's offset
   added: hud_px's), and drawn by hud_place with its piece's join's shift */
typedef struct {
    int lx, ly;                         /* the bitmap's origin on the layer (source pixel 0, 0) */
    int x0, y0, x1, y1;                 /* the pixels drawn, on the layer (inclusive) */
    uint16_t at, stepx, stepy;
    uint8_t bank, por, hi, pal[4];
    int piece;
} HudQuad;
static HudQuad *hq;
static size_t nhq, cap_hq;
static uint8_t hud_qd[224][256];        /* (a check) hud_px's pixel is a quad's too: the reference's alone */
static int span_ref_only;               /* (the spans' below) */
static uint8_t ram8(uint16_t a);
static int blit_quad;                   /* (a check) the blit being run has its quad */

/* a HUD bitmap with its join's shift (s, t: the layer's pixels to the
   screen's): its pixels in the view (on the layer), at the screen's pixel
   x + s, y + t, the texels block_set's: texel column c the screen's
   column ((c + 1) SXD - 1) / SXN, its source column that less s and the
   origin (then the 8.8 step), the rows the same (hud_quad, below) */
static void hud_quad(const HudQuad *h, int s, int t);

static void hud_place(void)
{
    int a, b, x, y, g0[HUD_PIECES], g1[HUD_PIECES], h0[HUD_PIECES], h1[HUD_PIECES], s[HUD_PIECES], t[HUD_PIECES];
    size_t q;

    span_flush();                       /* the 3D first, the HUD over it */
    for (a = 0; a < hud_n; a++)
        for (b = 0; b < a; b++)
            if (hv_x1[a] >= 0 && hv_x1[b] >= 0 && hv_y0[a] <= hv_y1[b] && hv_y0[b] <= hv_y1[a] &&
                hv_x0[a] <= hv_x1[b] + HUD_GAP && hv_x0[b] <= hv_x1[a] + HUD_GAP)
                hp_up[hud_root(a)] = hud_root(b);
    for (a = 0; a < hud_n; a++)
        g0[a] = h0[a] = 0x7FFF, g1[a] = h1[a] = -1;
    for (a = 0; a < hud_n; a++) {
        int g = hud_root(a);
        if (hv_x1[a] < 0)
            continue;
        if (hv_x0[a] < g0[g]) g0[g] = hv_x0[a];
        if (hv_x1[a] > g1[g]) g1[g] = hv_x1[a];
        if (hv_y0[a] < h0[g]) h0[g] = hv_y0[a];
        if (hv_y1[a] > h1[g]) h1[g] = hv_y1[a];
    }
    for (a = 0; a < hud_n; a++)
        if (hud_root(a) == a && g1[a] >= 0) {
            if (two) {                  /* (the layer's the view's place + VIEW_X, VIEW_Y) */
                int ox, oy;
                two_anchor(g0[a] - VIEW_X, g1[a] - VIEW_X, h0[a] - VIEW_Y, h1[a] - VIEW_Y, &ox, &oy);
                s[a] = ox - VIEW_X;
                t[a] = oy - VIEW_Y;
            } else {
                s[a] = view_shift(g0[a], g1[a]);
                t[a] = anchor_shift_y(h0[a], h1[a]);
            }
        }
    for (a = 0, q = 0; a < hud_n; a++) {
        int g = hud_root(a);
        if (hv_x1[a] < 0) {
            while (q < nhq && hq[q].piece <= a)
                q++;
            continue;
        }
        for (y = hv_y0[a]; y <= hv_y1[a]; y++)
            for (x = hv_x0[a]; x <= hv_x1[a]; x++) {
                if (!hud_px[y][x] || hud_id[y][x] != a)
                    continue;
                span_ref_only = hud_qd[y][x];   /* (a check) a quad's pixel: the reference's alone */
                block_set(x + s[g], y + t[g], (uint8_t)(hud_px[y][x] - 1));
            }
        span_ref_only = 0;
        for (; q < nhq && hq[q].piece <= a; q++)    /* its bitmaps, each a quad */
            if (hq[q].piece == a)
                hud_quad(&hq[q], s[g], t[g]);
    }
    nhq = 0;
    for (a = 0; a < hud_n; a++)
        for (y = hp_y0[a]; y <= hp_y1[a]; y++)
            for (x = hp_x0[a]; x <= hp_x1[a]; x++)
                hud_px[y][x] = 0, hud_qd[y][x] = 0;
    hud_n = 0;
}

static int cropping;                    /* FW_CCHECK's pictures: the 3D alone */

/* ---- the picture's spans ------------------------------------------------- */
/*
 * The 3D's pixels as spans, in the order they are drawn: the fill's edge
 * walks (the game's own rules, here) give each row of a polygon its first
 * and last column and, textured, its u and v; span_fill then writes the
 * pixels (fw_span_filler: the GPU's where the front end has one, else
 * span_fill_cpu, the reference). A flat span is a colour; a textured one
 * fetches each pixel's texel at ((v << 8 | u) & mask) + base in its bank,
 * u and v in 32.32 fixed point from the span's left end, through COLOR's
 * and PLOT's rules (the plot options, the colour's frozen high nibble).
 */
static FwSpan *spans;
static size_t nspans, cap_spans;
/* the polygons the spans hold whole (textured 3) and their edges, made
   again with each flush */
FwPoly *fw_polys;
FwEdge *fw_edges;
static size_t npolys, cap_polys, nedges, cap_edges;

/* ---- depth, for the water ------------------------------------------------- */
/* What is drawn now's 1 / depth across the screen (FwSpan's za, zx, zy):
   a face's (face_depth, by its draw-list place, made with its vertices),
   a sprite's (one depth, fw_depth_sprite), or none (0: the HUD, labels,
   lines). */
float fw_water[4];
static float cur_z[3], sprite_z[3];
static int label_on;               /* the label being drawn (fw_label_begin) */
static struct { float z[3]; uint32_t gen; } face_depth[0x8000];

static int sunk(float za, float zx, float zy, int32_t x, int32_t y)
{
    float z, w;
    if (fw_water[3] == 0 || (za == 0 && zx == 0 && zy == 0))
        return 0;
    z = fw_fmaf(zy, (float)y, fw_fmaf(zx, (float)x, za));
    w = fw_fmaf(fw_water[2], (float)y, fw_fmaf(fw_water[1], (float)x, fw_water[0]));
    return z > 0 && w > 0 && z < w;
}

static long cen_plots[4], cen_entries[3], cen_entries_gpu, cen_quads, cen_map_apart;  /* (FW_CENSUS's) */
/* a display-list face the GPU draws in its place (scene.h): its marker
   span; 1 when the CPU's spans of it are wanted beside it (the check's
   reference, the reference's alone until fw_gpu_face_done) */
static FwSpan *span_new(void);
static void quad_fill(uint8_t *out, int w, const FwSpan *p);
static FwSpan *quad_new(int32_t y0, int32_t rows, int32_t a, int32_t b, int64_t v, int64_t dv);
static int quads_on(void);
static int quad_made(void);
int fw_gpu_face(int id)
{
    FwSpan *m = span_new();
    if (m) {
        m->textured = 6;
        m->y = m->x0 = 0;
        m->x1 = id;
        m->pad1[0] = 0;
    }
    if (!fw_env("FW_GPUCHECK"))
        return 0;
    span_ref_only = 1;
    return 1;
}
void fw_gpu_face_done(void) { span_ref_only = 0; }

/* the spans being made are the CPU reference's alone (a model face the
   GPU draws from the display list, FW_GPUCHECK's reference): FwSpan's
   pad1[0] 1; a span with textured 6 is such a face for the GPU, x1 its
   place in the display list (fullwindow.h) */

static FwSpan *span_new(void)
{
    if (nspans == cap_spans) {
        size_t c = cap_spans ? cap_spans * 2 : 65536;
        FwSpan *n = realloc(spans, c * sizeof *n);
        if (!n)
            return NULL;
        spans = n;
        cap_spans = c;
    }
    {
        FwSpan *p = &spans[nspans++];
        int none = fw_hud || label_on;
        p->pad1[0] = (uint8_t)(span_ref_only | fw_ref_pass);
        p->za = none ? 0 : cur_z[0];
        p->zx = none ? 0 : cur_z[1];
        p->zy = none ? 0 : cur_z[2];
        return p;
    }
}

static void span_flat(int32_t y, int32_t x0, int32_t x1, uint8_t c)
{
    FwSpan *p;
    if (y < 0 || y >= th || x1 < x0)
        return;
    if (nspans) {                       /* the last one's run on: the same pixels as two */
        p = &spans[nspans - 1];
        if (!p->textured && p->y == y && p->colour == c && p->x1 + 1 == x0 && p->za == cur_z[0] && p->zx == cur_z[1] &&
            p->zy == cur_z[2] && !fw_hud && !label_on && p->pad1[0] == (span_ref_only | fw_ref_pass)) {
            p->x1 = x1;
            return;
        }
    }
    if (!(p = span_new()))
        return;
    p->y = y;
    p->x0 = x0;
    p->x1 = x1;
    p->colour = c;
    p->textured = 0;
}

/* the texel fetch, as the Super FX reads it (fw_span_filler's need the
   banks' bytes: fw_span_bank) */
static uint8_t *play_bank[256];         /* FW_SPANPLAY's recorded banks */
/* the panels' boxes as textures (panels_draw): banks $F0 to $F3, no
   bank of the Super FX's; their version moved on as they change */
static uint8_t panel_bank[8][0x10000];  /* ($F4 to $F7: two players' hills, two_hills) */
static uint32_t panel_version[8];
uint32_t fw_bank_version(uint8_t bank) { return bank >= 0xF0 && bank <= 0xF7 ? panel_version[bank & 7] : 0; }

uint8_t fw_texel(uint8_t bank, uint16_t at)
{
    if (play_bank[bank])
        return play_bank[bank][at];
    if (bank >= 0xF0 && bank <= 0xF7)
        return panel_bank[bank & 7][at];
    return gsu_rd8((uint32_t)bank << 16 | at);
}

/* COLOR from a texel byte, then PLOT's rule: the colour written, or -1 */
static int texel_colour(uint8_t v, uint8_t por, uint8_t hi)
{
    uint8_t c = por & 0x04 ? (uint8_t)(hi | v >> 4) : por & 0x08 ? (uint8_t)(hi | (v & 0x0F)) : v;
    if (!(por & 0x01)) {
        if (por & 0x08) {
            if (!(c & 0x0F))
                return -1;
        } else if (!c) {
            return -1;
        }
    }
    return c;
}

/* a perspective-correct span's texel coordinate: a / q, its whole part's
   low byte (0 where it is out of range: a pixel just past a face's edge) */
static unsigned persp_texel(float a, float q)
{
    float t = a / q;
    if (!(t > -1.0e9f && t < 1.0e9f))
        t = 0.0f;
    return (unsigned)(int)floorf(t) & 0xFF;
}

/* one effect's haze (FW_HAZE, the GPUs' the same): each entry's block of
   texels read from the picture as FxHazeSample left it, all before any is
   written, then written opaque in the shape's order */
static void haze_fill(uint8_t *out, int w, int h, const FwSpan *p)
{
    static uint8_t *from;
    static size_t from_len;
    size_t len = (size_t)w * h;
    int32_t X = (int32_t)p->xl, Y = p->pad0, r, c;
    uint16_t at = (uint16_t)(0xF92E + 4 * p->base);
    uint16_t shape = (uint16_t)(fw_texel(8, at) | fw_texel(8, (uint16_t)(at + 1)) << 8);
    uint16_t offs = (uint16_t)(fw_texel(8, (uint16_t)(at + 2)) | fw_texel(8, (uint16_t)(at + 3)) << 8);
    int wrap = p->pad1[2];

    if (from_len < len) {
        free(from);
        from = malloc(len);
        from_len = from ? len : 0;
        if (!from)
            return;
    }
    memcpy(from, out, len);
    for (r = 0; r < 16; r++) {
        int32_t sy = Y + (int8_t)fw_texel(8, (uint16_t)(offs + r)), ty = Y - 8 + r;
        for (c = 0; c < 16; c++) {
            int8_t b = (int8_t)fw_texel(8, (uint16_t)(shape + r * 16 + c));
            int32_t sx = X + b, tx = (X - 8 + c) & 255, ty8 = ty & 255, sxa = sx, sya = sy, u, v;
            int64_t c0, c1, r0, r1, s0, t0;
            if (!b)
                continue;
            if (wrap) {
                sxa &= 255;
                sya &= 255;
                if (sya >= h) {
                    sya -= h;
                    sxa += 8;
                }
                if (ty8 >= h) {
                    ty8 -= h;
                    tx += 8;
                }
            }
            c0 = floor_div(tx * p->u, p->du);
            c1 = floor_div((tx + 1) * p->u, p->du);
            r0 = floor_div(ty8 * p->v, p->dv);
            r1 = floor_div((ty8 + 1) * p->v, p->dv);
            s0 = floor_div(sxa * p->u, p->du);
            t0 = floor_div(sya * p->v, p->dv);
            for (v = 0; v < r1 - r0; v++)
                for (u = 0; u < c1 - c0; u++) {
                    int64_t xs = s0 + u, ys = t0 + v, xd = c0 + u, yd = r0 + v;
                    if (xd >= 0 && xd < w && yd >= 0 && yd < h)
                        out[yd * w + xd] = xs >= 0 && xs < w && ys >= 0 && ys < h ? from[ys * w + xs] : 0;
                }
        }
    }
}

/* a job's noise (FW_NOISE, the GPUs' the same) */
uint16_t fx_noise_step(uint16_t s);
static void noise_fill(uint8_t *out, int w, int h, const FwSpan *p)
{
    uint16_t s = p->base;
    uint64_t ramp = (uint64_t)p->u;
    int32_t cols = (int32_t)p->xl, rows = (int32_t)p->v, x, y;

    for (y = 0; y < rows; y++)
        for (x = 0; x < cols; x++) {
            uint8_t c;
            int px = x & 255, py = y & 255;
            s = fx_noise_step(s);
            c = (uint8_t)(ramp >> ((s & 7) * 8));
            if (p->por & 8 ? !(c & 15) : !c)
                continue;
            if (py >= h) {
                py -= h;
                px += 8;
            }
            if (px < w && py < h)
                out[(size_t)py * w + px] = c;
        }
}

void fw_span_fill_cpu(uint8_t *out, int w, int h, const FwSpan *sp, size_t n)
{
    FW_CPU_FALLBACK("the spans' fill");
    size_t i;
    (void)h;
    for (i = 0; i < n; i++) {
        const FwSpan *p = &sp[i];
        uint8_t *o = out + (size_t)p->y * w;
        int32_t x;
        int deep = fw_water[3] != 0 && (p->za != 0 || p->zx != 0 || p->zy != 0);
        if (p->textured == 6)           /* a face the GPU draws (the reference's own spans are beside it) */
            continue;
        if (p->textured == FW_HAZE) {
            haze_fill(out, w, h, p);
            continue;
        }
        if (p->textured == FW_NOISE) {
            noise_fill(out, w, h, p);
            continue;
        }
        if (p->textured == FW_QUAD) {
            if (p->pad1[0] != 2)        /* (2: the GPU's alone, the reference's spans beside it) */
                quad_fill(out, w, p);
            continue;
        }
        if (!p->textured) {
            if (!deep)
                memset(o + p->x0, p->colour, (size_t)(p->x1 - p->x0 + 1));
            else
                for (x = p->x0; x <= p->x1; x++)
                    if (!sunk(p->za, p->zx, p->zy, x, p->y))
                        o[x] = p->colour;
            continue;
        }
        if (p->textured == 3) {         /* a polygon's rows, in its place */
            int32_t row;
            FwSpan r;
            for (row = p->y; row < p->x0; row++)
                if (fw_poly_row(&fw_polys[p->x1], fw_edges, row, w, &r))
                    fw_span_fill_cpu(out, w, h, &r, 1);
            continue;
        }
        if (p->textured == 2) {
            for (x = p->x0; x <= p->x1; x++) {
                float k = (float)(x - p->xl), q = fw_fmaf(p->pdq, k, p->pq);
                unsigned iu = persp_texel(fw_fmaf(p->pda, k, p->pa), q), iv = persp_texel(fw_fmaf(p->pdb, k, p->pb), q);
                int c = texel_colour(fw_texel(p->bank, (uint16_t)(((iv << 8 | iu) & p->mask) + p->base)), p->por, p->hi);
                if (c >= 0 && !(deep && sunk(p->za, p->zx, p->zy, x, p->y)))
                    o[x] = (uint8_t)c;
            }
            continue;
        }
        for (x = p->x0; x <= p->x1; x++) {
            int64_t k = x - p->xl;
            unsigned iu = (unsigned)((p->u + p->du * k) >> 32) & 0xFF, iv = (unsigned)((p->v + p->dv * k) >> 32) & 0xFF;
            int c = texel_colour(fw_texel(p->bank, (uint16_t)(((iv << 8 | iu) & p->mask) + p->base)), p->por, p->hi);
            if (c >= 0 && !(deep && sunk(p->za, p->zx, p->zy, x, p->y)))
                o[x] = (uint8_t)c;
        }
    }
}

void (*fw_span_filler)(uint8_t *out, int w, int h, const FwSpan *sp, size_t n) = fw_span_fill_cpu;
uint8_t *(*fw_texels_alloc)(size_t len);

/* ---- FW_SPANREC and FW_SPANPLAY (a check of a GPU's fill) ----
   FW_SPANREC=FILE: every FW_SPANREC_EVERY'th fill (10), to FW_SPANREC_MAX
   of them (30), written as it stands before the fill: the texels'
   size, the spans, polygons and edges, the water's plane, the texels,
   and every bank a textured span or polygon reads (64 KB each).
   FW_SPANPLAY=FILE (fw_span_play): each one filled by the CPU's
   reference and by fw_span_filler from those texels, the texels apart
   counted. A record's header: 'SPAN', the sizes of FwSpan, FwPoly and
   FwEdge, w, h, the spans', polygons', edges' and banks' counts. */
static void span_record(void)
{
    static FILE *f;
    static long fills, kept;
    static int every = -1, most;
    uint8_t used[256] = { 0 };
    uint32_t head[10];
    size_t i;
    unsigned b, nb = 0;
    if (every < 0) {
        const char *e = fw_env("FW_SPANREC");
        every = fw_env("FW_SPANREC_EVERY") ? atoi(fw_env("FW_SPANREC_EVERY")) : 10;
        most = fw_env("FW_SPANREC_MAX") ? atoi(fw_env("FW_SPANREC_MAX")) : 30;
        if (!e || every < 1 || !(f = fopen(e, "wb")))
            every = 0;
    }
    if (!every || kept >= most || fills++ % every)
        return;
    for (i = 0; i < nspans; i++)
        if (spans[i].textured == 1 || spans[i].textured == 2)
            used[spans[i].bank] = 1;
        else if (spans[i].textured == 3 && fw_polys[spans[i].x1].kind)
            used[fw_polys[spans[i].x1].bank] = 1;
    for (b = 0; b < 256; b++)
        nb += used[b];
    memcpy(head, "SPAN", 4);
    head[1] = sizeof(FwSpan);
    head[2] = sizeof(FwPoly);
    head[3] = sizeof(FwEdge);
    head[4] = (uint32_t)tw;
    head[5] = (uint32_t)th;
    head[6] = (uint32_t)nspans;
    head[7] = (uint32_t)npolys;
    head[8] = (uint32_t)nedges;
    head[9] = nb;
    fwrite(head, sizeof head, 1, f);
    fwrite(fw_water, sizeof fw_water, 1, f);
    fwrite(spans, sizeof *spans, nspans, f);
    fwrite(fw_polys, sizeof *fw_polys, npolys, f);
    fwrite(fw_edges, sizeof *fw_edges, nedges, f);
    fwrite(tex, 1, (size_t)tw * th, f);
    for (b = 0; b < 256; b++)
        if (used[b]) {
            static uint8_t bank[0x10000];
            unsigned a;
            for (a = 0; a < 0x10000; a++)
                bank[a] = fw_texel((uint8_t)b, (uint16_t)a);
            fputc((int)b, f);
            fwrite(bank, 1, sizeof bank, f);
        }
    fflush(f);
    kept++;
}

int fw_span_play(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t head[10];
    long records = 0, apart = 0, texels = 0;
    unsigned b;
    if (!f) {
        printf("spanplay: cannot read %s\n", path);
        return 2;
    }
    for (b = 0; b < 256; b++)
        play_bank[b] = NULL;
    while (fread(head, sizeof head, 1, f) == 1) {
        size_t w = head[4], h = head[5], n = head[6], np = head[7], ne = head[8], i;
        FwSpan *sp;
        uint8_t *a, *g;
        long here = 0;
        if (memcmp(head, "SPAN", 4) || head[1] != sizeof(FwSpan) || head[2] != sizeof(FwPoly) ||
            head[3] != sizeof(FwEdge)) {
            printf("spanplay: record %ld: not this build's layout\n", records);
            return 2;
        }
        sp = malloc(n * sizeof *sp + 1);
        fw_polys = malloc(np * sizeof *fw_polys + 1);
        fw_edges = malloc(ne * sizeof *fw_edges + 1);
        a = malloc(w * h);
        g = malloc(w * h);
        if (!sp || !fw_polys || !fw_edges || !a || !g || fread(fw_water, sizeof fw_water, 1, f) != 1 ||
            fread(sp, sizeof *sp, n, f) != n || fread(fw_polys, sizeof *fw_polys, np, f) != np ||
            fread(fw_edges, sizeof *fw_edges, ne, f) != ne || fread(a, 1, w * h, f) != w * h) {
            printf("spanplay: record %ld: short or no memory\n", records);
            return 2;
        }
        for (i = 0; i < head[9]; i++) {
            int k = fgetc(f);
            if (k < 0 || (!play_bank[k] && !(play_bank[k] = malloc(0x10000))) ||
                fread(play_bank[k], 1, 0x10000, f) != 0x10000) {
                printf("spanplay: record %ld: a bank short\n", records);
                return 2;
            }
        }
        memcpy(g, a, w * h);
        fw_span_fill_cpu(a, (int)w, (int)h, sp, n);
        fw_span_filler(g, (int)w, (int)h, sp, n);
        for (i = 0; i < w * h; i++)
            here += a[i] != g[i];
        printf("spanplay: record %ld: %dx%d, %d spans, %d polygons: %ld texels apart\n", records, (int)w, (int)h, (int)n,
               (int)np, here);
        fflush(stdout);
        apart += here;
        texels += (long)(w * h);
        records++;
        free(sp);
        free(fw_polys);
        free(fw_edges);
        free(a);
        free(g);
        for (b = 0; b < 256; b++) {
            free(play_bank[b]);
            play_bank[b] = NULL;
        }
    }
    printf("spanplay: %ld records, %ld texels, %ld apart\n", records, texels, apart);
    fflush(stdout);
    fclose(f);
    return apart != 0;
}

/* FW_WATERREC=FILE: every FW_SPANREC_EVERY'th water pass (10), to
   FW_SPANREC_MAX (30): 'WATR', sizeof(FwWaterJob), the job, the texels
   (w x h) and the rows' colours (2 h). FW_WATERPLAY=FILE
   (fw_water_play): each by the CPU's reference and by fw_water_filler,
   the texels apart and how far apart printed. */
static void water_record(const uint8_t *in, const FwWaterJob *j, const uint32_t *gap)
{
    static FILE *f;
    static long passes, kept;
    static int every = -1, most;
    uint32_t head[2];
    if (every < 0) {
        const char *e = fw_env("FW_WATERREC");
        every = fw_env("FW_SPANREC_EVERY") ? atoi(fw_env("FW_SPANREC_EVERY")) : 10;
        most = fw_env("FW_SPANREC_MAX") ? atoi(fw_env("FW_SPANREC_MAX")) : 30;
        if (!e || every < 1 || !(f = fopen(e, "wb")))
            every = 0;
    }
    if (!every || kept >= most || passes++ % every)
        return;
    memcpy(head, "WATR", 4);
    head[1] = sizeof *j;
    fwrite(head, sizeof head, 1, f);
    fwrite(j, sizeof *j, 1, f);
    fwrite(in, 1, (size_t)j->w * j->h, f);
    fwrite(gap, sizeof *gap, 2 * (size_t)j->h, f);
    fflush(f);
    kept++;
}

int fw_water_play(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t head[2];
    FwWaterJob j;
    long records = 0, apart = 0, texels = 0, hist[5] = { 0 };
    int worst = 0;
    double sum = 0;
    if (!f) {
        printf("waterplay: cannot read %s\n", path);
        return 2;
    }
    while (fread(head, sizeof head, 1, f) == 1) {
        size_t n, i;
        uint8_t *in;
        uint32_t *gap, *a, *g;
        long here = 0;
        if (memcmp(head, "WATR", 4) || head[1] != sizeof j || fread(&j, sizeof j, 1, f) != 1) {
            printf("waterplay: record %ld: not this build's layout\n", records);
            return 2;
        }
        n = (size_t)j.w * j.h;
        in = malloc(n);
        gap = malloc(2 * (size_t)j.h * sizeof *gap);
        a = malloc(n * 4);
        g = malloc(n * 4);
        if (!in || !gap || !a || !g || fread(in, 1, n, f) != n || fread(gap, sizeof *gap, 2 * (size_t)j.h, f) != 2 * (size_t)j.h) {
            printf("waterplay: record %ld: short or no memory\n", records);
            return 2;
        }
        fw_water_fill_cpu(in, a, &j, gap);
        fw_water_filler(in, g, &j, gap);
        for (i = 0; i < n; i++)
            if (a[i] != g[i]) {
                int s, d = 0;
                here++;
                if ((a[i] >> 24) && (g[i] >> 24))
                    for (s = 0; s < 24; s += 8) {
                        int e = abs((int)(a[i] >> s & 255) - (int)(g[i] >> s & 255));
                        if (e > d)
                            d = e;
                    }
                else
                    d = 256;            /* a colour against an index or transparency */
                hist[d <= 1 ? 0 : d <= 4 ? 1 : d <= 16 ? 2 : d <= 64 ? 3 : 4]++;
                if (d > worst)
                    worst = d;
                sum += d;
            }
        printf("waterplay: record %ld: %dx%d: %ld texels apart\n", records, j.w, j.h, here);
        fflush(stdout);
        apart += here;
        texels += (long)n;
        records++;
        free(in);
        free(gap);
        free(a);
        free(g);
    }
    printf("waterplay: %ld records, %ld texels, %ld apart (%.3f%%); the most a channel apart %d (256: not both colours), "
           "the mean %.2f; apart by 1 %ld, 2-4 %ld, 5-16 %ld, 17-64 %ld, more %ld\n",
           records, texels, apart, texels ? 100.0 * apart / texels : 0.0, worst, apart ? sum / apart : 0.0, hist[0], hist[1],
           hist[2], hist[3], hist[4]);
    fflush(stdout);
    fclose(f);
    return 0;
}

/* FW_QUADCHECK (with SC_GPU): each fill with quads made twice on the CPU,
   with the quads (the reference's own spans left out) and with the
   reference's spans (the quads left out), the texels apart counted: the
   quads' rule against the chip's loops, which must be 0 */
static long qc_fills, qc_apart, quads_made, quads_left, quads_kind[3];
static void quad_report(void)
{
    printf("quadcheck: %ld fills with quads, %ld quads (%ld small, %ld large, %ld not square), %ld texels apart, %ld loops "
           "not made quads\n", qc_fills, quads_made, quads_kind[0], quads_kind[1], quads_kind[2], qc_apart, quads_left);
    fflush(stdout);
}

static void quad_check(void)
{
    static uint8_t *a, *b;
    static FwSpan *q;
    static size_t len, qcap;
    size_t i, m = 0, n = (size_t)tw * th;
    int any = 0;
    for (i = 0; i < nspans && !any; i++)
        any = spans[i].textured == FW_QUAD;
    if (!any)
        return;
    if (len < n) {
        free(a);
        free(b);
        a = malloc(n);
        b = malloc(n);
        len = a && b ? n : 0;
    }
    if (qcap < nspans) {
        free(q);
        q = malloc(nspans * sizeof *q);
        qcap = q ? nspans : 0;
    }
    if (!len || !qcap)
        return;
    for (i = 0; i < nspans; i++)
        if (spans[i].pad1[0] != 1) {
            q[m] = spans[i];
            if (q[m].textured == FW_QUAD)
                q[m].pad1[0] = 0;
            m++;
        }
    memcpy(a, tex, n);
    memcpy(b, tex, n);
    fw_span_fill_cpu(a, tw, th, spans, nspans);
    fw_span_fill_cpu(b, tw, th, q, m);
    if (!qc_fills++)
        atexit(quad_report);
    for (i = 0; i < n; i++)
        qc_apart += a[i] != b[i];
}

/* the CPU's fill held to the fw_clip the GPUs' take: what it drew past the
   rectangle put back */
static void two_fill_cpu(void)
{
    static uint8_t *kept;
    static size_t kept_len;
    size_t len = (size_t)tw * th;
    int y;
    if (fw_clip[2] < 0) {
        fw_span_fill_cpu(tex, tw, th, spans, nspans);
        return;
    }
    if (kept_len < len) {
        free(kept);
        kept = malloc(len);
        kept_len = kept ? len : 0;
        if (!kept)
            return;
    }
    memcpy(kept, tex, len);
    fw_span_fill_cpu(tex, tw, th, spans, nspans);
    for (y = 0; y < th; y++) {
        uint8_t *t = tex + (size_t)y * tw;
        const uint8_t *k = kept + (size_t)y * tw;
        if (y < fw_clip[1] || y > fw_clip[3]) {
            memcpy(t, k, (size_t)tw);
            continue;
        }
        memcpy(t, k, (size_t)fw_clip[0]);
        memcpy(t + fw_clip[2] + 1, k + fw_clip[2] + 1, (size_t)(tw - 1 - fw_clip[2]));
    }
}

/* the spans drawn into the texels */
static void span_flush(void)
{
    if (gx && !gx_valid && nspans) {    /* a job's first drawing: the chip's bitplanes unpacked */
        fw_bits_filler(tex, nshvc_sram(), &gx_shape, 0);
        gx_valid = 1;
    }
    if (gx && !nspans)
        return;
    if (gpu_pic && !gpu_open) {         /* the GPU's picture opened: cleared, or the last one's to draw over */
#ifdef SRFX_NSHVC_HANDOFF
        /* (handed, no texels here to draw over: every setup of the race sets $01C4, so it is cleared) */
        if (fw_canvas_gpu->open(tw, th, gpu_cleared || hand_pic() || (bp && fw_chars_on()) ? NULL : tex))
#else
        if (fw_canvas_gpu->open(tw, th, gpu_cleared ? NULL : tex))
#endif
            gpu_open = 1;
        else {
            FW_CPU_FALLBACK("a picture with no GPU memory");
            gpu_pic = 0;                /* no GPU memory: the CPU's texels */
        }
    }
    if (nspans) {
        span_record();
        if (fw_env("FW_QUADCHECK"))
            quad_check();
        if (gpu_pic)
            fw_canvas_gpu->spans(spans, nspans);
        else if (two && fw_span_filler == fw_span_fill_cpu)
            two_fill_cpu();
        else
            fw_span_filler(tex, tw, th, spans, nspans);
    }
    nspans = npolys = nedges = 0;
}

/* the label being drawn (fw_label_begin): its position as FxTextNotice
   set it, and where that is in the texels */
static int label_x, label_y, label_tx, label_ty;

int fw_label_begin(uint16_t x, uint16_t y)
{
    int lw = col_at(0x11);              /* the original's room for it: $3A - $11 */
    if (!fw_view_active())
        return 0;
    label_x = x;
    label_y = y;
    /* x is half the side offset past the centre (tw / 2, the full
       window's $34), in console pixels; y the original's row on its
       128-row view, centre line 64 (a bitmap picture: the row itself, the
       centre its own) */
    {
        int cx = view_cx();
        label_tx = cx + (int)floor_div(((int)(int16_t)x - cx) * SXN, SXD);
    }
    if (label_tx < 0)
        label_tx = 0;
    if (label_tx > tw - 1 - lw)
        label_tx = tw - 1 - lw;
    label_ty = bp ? row_at((int16_t)y) : row_at(view_cy() + (int)(int16_t)y - (two ? 40 : 64)) + cy_off;
    label_on = 1;
    return 1;
}

void fw_label_end(void) { label_on = 0; }

int fw_sprite_rows(void) { return fw_drawing && !fw_hud && !checking && !label_on; }

/* the sprite being drawn is not put in the picture (a solid wheel is in its
   place, solids.c): its rows and plots run, their pixels left out */
static int sprite_hidden;
void fw_sprite_hide(int on) { sprite_hidden = on; }
int fw_sprite_hidden(void) { return sprite_hidden; }

/* the texels square: as many a console pixel across as down */
int fw_square(void) { return SXN * SYD == SYN * SXD; }

/* n console rows' texel rows, rounded toward zero */
int fw_rows(int n) { return (int)(n * SYN / SYD); }

static uint16_t ram16(uint16_t a);

void fw_sprite_draw(int32_t h)
{
    int32_t cx = (int16_t)ram16(0x00BA), cy = (int16_t)ram16(0x00BC), right = (int16_t)ram16(0x003A);
    int32_t bottom = (int16_t)ram16(0x003E), x0, y0, a, b, j, w;
    uint16_t mask = ram16(0x00D4), base = ram16(0x00A8), bank = ram16(0x00A6);
    int c8 = (mask & 0xFF) + 1, back = (mask & 0x8000) != 0;
    int64_t du;

    /* across: the size down times the texels a console pixel across over down, rounded */
    w = (int32_t)((h * SXN * SYD * 2 + SXD * SYN) / (2 * SXD * SYN));
    if (w < 1 || h < 1)
        return;
    x0 = cx - (w + 1) / 2;              /* the program's: the centre less half, rounded up */
    y0 = cy - (h + 1) / 2;
    a = x0 < 0 ? 0 : x0;
    b = x0 + w - 1 > right ? right : x0 + w - 1;
    if (b >= tw)
        b = tw - 1;
    if (a > b)
        return;
    du = ((int64_t)c8 << 32) / w;       /* a column's texels, from 0 */
    if (quads_on()) {                   /* one quad for the GPU: row j's texel row j c8 / h */
        int32_t last = y0 + h - 1;
        FwSpan *q;
        if (last > bottom - 1)
            last = bottom - 1;
        q = quad_new(y0, last - y0 + 1, a, b, 0, c8);
        if (q) {
            q->mask = (uint16_t)h;
            q->xl = x0;
            q->bank = (uint8_t)bank;
            q->hi = fw_hi();
            if (!back) {
                q->base = base;
                q->du = du;
            } else {
                q->base = (uint16_t)(base + c8 - 1 - 0xFF);
                q->u = ((int64_t)0xFF << 32) + (((int64_t)1 << 32) - 1);
                q->du = -du;
            }
            quads_kind[2]++;
        }
        if (quad_made())
            return;
    }
    for (j = 0; j < h; j++) {
        int32_t y = y0 + j;
        unsigned v;
        FwSpan *p;
        if (y < 0)
            continue;
        if (y > bottom - 1 || y >= th)
            break;
        v = (unsigned)((int64_t)j * c8 / h);   /* a row's texel row, from 0 */
        if (!(p = span_new()))
            break;
        memset(p, 0, sizeof *p);
        p->pad1[0] = (uint8_t)(span_ref_only | fw_ref_pass);
        p->za = sprite_z[0];
        p->y = y;
        p->x0 = a;
        p->x1 = b;
        p->xl = x0;
        p->bank = (uint8_t)bank;
        p->mask = 0x00FF;
        p->por = POR_NOW();
        p->hi = fw_hi();
        p->textured = 1;
        p->base = (uint16_t)(base + (v << 8));
        if (!back) {                    /* column k's texel floor(k du) */
            p->u = 0;
            p->du = du;
        } else {                        /* c8 - 1 less it, as fw_sprite_span's mirrored rows */
            p->base = (uint16_t)(p->base + c8 - 1 - 0xFF);
            p->u = ((int64_t)0xFF << 32) + (((int64_t)1 << 32) - 1);
            p->du = -du;
        }
    }
    fw_sprite_rows_done();
}

void fw_sprite_span(uint16_t y, uint16_t x0, uint16_t x1, uint16_t bank, uint16_t at, uint16_t frac,
                    uint16_t step, int back)
{
    FwSpan *p;
    int32_t a = x0, b = x1;
    if (sprite_hidden)
        return;
    if (y >= th || a >= tw)             /* fw_plot's: a column or row past the texels is not drawn */
        return;
    if (b >= tw)
        b = tw - 1;
    if (!(p = span_new()))
        return;
    memset(p, 0, sizeof *p);
    p->pad1[0] = (uint8_t)(span_ref_only | fw_ref_pass);
    p->za = sprite_z[0];                /* the sprite's one depth */
    p->y = y;
    p->x0 = a;
    p->x1 = b;
    p->xl = x0;
    p->bank = (uint8_t)bank;
    p->mask = 0x00FF;
    p->por = POR_NOW();
    p->hi = fw_hi();
    p->textured = 1;
    if (!back) {                        /* texel (frac + k step) >> 16 from at */
        p->base = at;
        p->u = (int64_t)frac << 16;
        p->du = (int64_t)step << 16;
    } else {                            /* at minus it: 255 less it from at - 255 */
        p->base = (uint16_t)(at - 0xFF);
        p->u = ((int64_t)0xFF << 32) + (((int64_t)1 << 32) - 1) - ((int64_t)frac << 16);
        p->du = -((int64_t)step << 16);
    }
    p->por = fw_por();
}

/* ---- screen quads (textured 7, fullwindow.h) ------------------------------ */

/* the quads drawn by the GPU: its display list's (SC_GPU), the 3D's sprites */
/* (FW_NOQUADS, a check: the sprites by the chip's loops, the GPU's faces kept) */
/* (a bitmap picture's programs plot at the bitmap's own places, fw_plot's; a
   job with dither on a 4-bit screen, none made, by the loop's plots) */
static int quads_on(void)
{
    return fw_drawing && sc_gpu_on() && !checking && !fw_hud && !label_on && !bp && !(gx && bit4 && (gsu.por & 2)) &&
           !fw_env("FW_NOQUADS");
}
/* the reference's own spans kept beside them (FW_GPUCHECK, FW_QUADCHECK) */
static int quads_both(void) { return fw_env("FW_GPUCHECK") || fw_env("FW_QUADCHECK"); }

/* a quad on rows y0 to y0 + rows - 1 (r from y0), columns a to b, put on
   the texels: rows and columns past them left out, its rows' accumulator
   moved on by the rows above */
static FwSpan *quad_new(int32_t y0, int32_t rows, int32_t a, int32_t b, int64_t v, int64_t dv)
{
    FwSpan *p;
    int32_t y1 = y0 + rows - 1;
    if (y0 < 0) {
        v += dv * -y0;
        y0 = 0;
    }
    if (y1 >= th)
        y1 = th - 1;
    if (a < 0)
        a = 0;
    if (b >= tw)
        b = tw - 1;
    if (y1 < y0 || b < a || !(p = span_new()))
        return NULL;
    memset(p, 0, sizeof *p);
    p->y = y0;
    p->pad0 = y1 - y0 + 1;
    p->x0 = a;
    p->x1 = b;
    p->v = v;
    p->dv = dv;
    p->za = cur_z[0];
    p->zx = cur_z[1];
    p->zy = cur_z[2];
    p->textured = FW_QUAD;
    p->pad1[0] = quads_both() ? 2 : 0;
    p->por = fw_por();
    p->hi = fw_hi();
    quads_made++;
    cen_quads++;
    if (gx)
        job.quad += (uint64_t)(b - a + 1) * (uint64_t)(y1 - y0 + 1);
    if (fw_env("SC_BREAK"))             /* (a planted break: the check must see it) */
        p->x1 += p->x1 > p->x0;
    return p;
}

/* the quad stands for the loop's pixels: the loop skipped, or its spans the
   reference's alone (a check) */
static int quad_made(void)
{
    if (!quads_both())
        return 1;
    span_ref_only = 1;
    return 0;
}

void fw_sprite_rows_done(void) { span_ref_only = 0; blit_quad = 0; }

/* 01:B1CB's rows: rows rows from y (those at or past the screen's bottom,
   $003E, or above row 0 left out: the loop's unsigned compare), each from
   column x, cols columns; column k's texel (k R9 & $FFFF) >> 8 (mirrored:
   (-k R9 & $FFFF) >> 8), row r's (r R9 & $FFFF) >> 8, added to R5 */
int fw_sprite_small(uint16_t y, uint16_t x, uint16_t cols, uint16_t rows, uint16_t base, uint16_t step, int back)
{
    int32_t bottom, last;
    int64_t s = (int64_t)step << 24;
    FwSpan *p;
    if (!quads_on())
        return 0;
    bottom = (int32_t)ram16(0x003E) - 1;
    if (sprite_hidden)                  /* (its pixels left out: none) */
        return 1;
    if (!cols || !rows) {               /* (the loop's count from 0: 65536) */
        quads_left++;
        return 0;
    }
    last = (int32_t)(int16_t)y + rows - 1;
    if (last > bottom)
        last = bottom;
    p = quad_new((int16_t)y, last - (int16_t)y + 1, x, (int32_t)x + cols - 1, 0, s);
    if (p) {
        p->xl = x;
        p->du = back ? -s : s;
        p->bank = (uint8_t)ram16(0x00A6);
        p->base = base;
        quads_kind[0]++;
    }
    return quad_made();
}

/* 01:B2ED's rows: row r (from y) on texel row ceil(r R9 / 65536) while it
   is below c8, to the screen's bottom; column k's texel (R11 + k R9) >>
   16, the first cols - 1 texels whole, the last to the screen's right
   ($003A); R5 on (mirrored: back) by each */
int fw_sprite_large(uint16_t y, uint16_t x, uint16_t cols, uint16_t frac, uint16_t step, uint16_t base, int back)
{
    int32_t bottom, right, last, kmax;
    int64_t M = 65536, kin, kall;
    uint16_t c8;
    FwSpan *p;
    if (!quads_on())
        return 0;
    bottom = (int32_t)(int16_t)ram16(0x003E) - 1;
    right = (int16_t)ram16(0x003A);
    c8 = ram16(0x00C8);
    if (sprite_hidden)
        return 1;
    if (!cols || !step || !c8) {
        quads_left++;
        return 0;
    }
    /* the rows: those whose texel row is below c8, to the bottom */
    last = (int32_t)(int16_t)y + (int32_t)(((int64_t)(c8 - 1) * M) / step);
    if (last > bottom)
        last = bottom;
    /* the columns: the first cols - 1 texels' (kin), then the last's to
       the carry (kall) or the right edge */
    kin = cols > 1 ? ((int64_t)(cols - 1) * M - frac + step - 1) / step : 0;
    kall = ((int64_t)cols * M - frac + step - 1) / step;
    kmax = (int32_t)(kall - 1 < right - (int32_t)x ? kall - 1 : right - (int32_t)x);
    if (kmax < kin - 1)
        kmax = (int32_t)(kin - 1);
    p = kmax >= 0 ? quad_new((int16_t)y, last - (int16_t)y + 1, x, (int32_t)x + kmax, M - 1, (int64_t)step) : NULL;
    if (p) {
        p->v <<= 16;                    /* (65535 + r R9) << 16: ceil(r R9 / 65536) at >> 32 */
        p->dv <<= 16;
        p->xl = x;
        p->bank = (uint8_t)ram16(0x00A6);
        if (!back) {
            p->base = base;
            p->u = (int64_t)frac << 16;
            p->du = (int64_t)step << 16;
        } else {                        /* R5 less the texel: 255 less it from R5 - 255 */
            p->base = (uint16_t)(base - 0xFF);
            p->u = ((int64_t)0xFF << 32) + (((int64_t)1 << 32) - 1) - ((int64_t)frac << 16);
            p->du = -((int64_t)step << 16);
        }
        quads_kind[1]++;
    }
    return quad_made();
}

/* a quad's texel column at x and row at r (fullwindow.h's rule) */
static int64_t quad_col(const FwSpan *p, int32_t x)
{
    int64_t a = p->u + p->du * (x - p->xl), s = p->pad1[1] & 8 ? a / (int64_t)fw_quad_word(&p->pq) : a >> 32;
    uint32_t m = fw_quad_word(&p->pdq);
    return m ? (s * m) >> 8 : s;
}
static int64_t quad_row(const FwSpan *p, int32_t r)
{
    int64_t a = p->v + p->dv * r, t = p->mask ? a / p->mask : a >> 32;
    uint32_t m = fw_quad_word(&p->pda);
    return m ? (t * m) >> 8 : t;
}
/* a quad's colour at column s, row t, or -1 (transparent) */
static int quad_colour(const FwSpan *p, int64_t s, int64_t t)
{
    if (p->pad1[1] & 2)
        return p->colour;
    if (p->pad1[1] & 4) {
        uint8_t b = fw_texel(p->bank, (uint16_t)(p->base + t * 32 + (s >> 2)));
        return texel_colour(p->pad1[3 + ((b >> 2 * (s & 3)) & 3)], p->por, p->hi);
    }
    return texel_colour(fw_texel(p->bank, (uint16_t)(p->base + ((t & 255) << 8) + (s & 255))), p->por, p->hi);
}

/* a line quad's pixel (x, y) on its line (fullwindow.h, bit 5) */
static int quad_on_line(const FwSpan *p, int32_t x, int32_t y)
{
    int ymaj = p->mask & 1, sx = p->mask & 2 ? -1 : 1, sy = p->mask & 4 ? -1 : 1;
    int64_t i = ymaj ? (int64_t)sy * (y - p->v) : (int64_t)sx * (x - p->xl), n, st;
    if (i < 0 || i > p->du)
        return 0;
    n = i * p->dv - p->u;
    st = n <= 0 ? 0 : (n + p->du - 1) / p->du;
    return ymaj ? x == p->xl + sx * st : y == p->v + sy * st;
}

/* a quad's texels into out (the reference's rule, fullwindow.h) */
static void quad_fill(uint8_t *out, int w, const FwSpan *p)
{
    int deep = fw_water[3] != 0 && (p->za != 0 || p->zx != 0 || p->zy != 0);
    int32_t r, x;
    for (r = 0; r < p->pad0; r++) {
        int32_t y = p->y + r;
        int64_t t = quad_row(p, r);
        uint8_t *o = out + (size_t)y * w;
        for (x = p->x0; x <= p->x1; x++) {
            int c;
            if (p->pad1[1] & 32) {
                if (!quad_on_line(p, x, y))
                    continue;
                c = p->colour;
            } else
                c = quad_colour(p, quad_col(p, x), t);
            if (c >= 0 && !(deep && sunk(p->za, p->zx, p->zy, x, y)))
                o[x] = (uint8_t)c;
        }
    }
}

/* ---- 2-bit bitmaps as quads (labels, the HUD) ----------------------------- */

/* a bitmap quad's fields: ROM bank and address, the palette's bytes, the
   plot options and colour, 8.8 steps (0: 1:1) */
static void bitmap_fields(FwSpan *p, uint8_t bank, uint16_t at, const uint8_t pal[4], uint16_t stepx, uint16_t stepy,
                          uint32_t cden)
{
    uint32_t mx = stepx, my = stepy;
    p->bank = bank;
    p->base = at;
    memcpy(p->pad1 + 3, pal, 4);
    p->pad1[1] = 4 | 8;
    memcpy(&p->pq, &cden, 4);
    memcpy(&p->pdq, &mx, 4);
    memcpy(&p->pda, &my, 4);
    p->por = fw_por();
    p->hi = fw_hi();
    p->za = p->zx = p->zy = 0;          /* no depth: the HUD's and labels' (span_new's) */
}

/* a bitmap's source pixel's colour (sx, sy: its column and row, the 8.8
   steps applied), or -1 */
static int bitmap_colour(uint8_t bank, uint16_t at, int64_t sx, int64_t sy, const uint8_t pal[4])
{
    uint8_t b = fw_texel(bank, (uint16_t)(at + sy * 32 + (sx >> 2)));
    uint8_t c = pal[(b >> 2 * (sx & 3)) & 3];
    return texel_colour(c, gsu.por, fw_hi());
}

static int bitmaps_on(void)
{
    return fw_drawing && sc_gpu_on() && !checking && !cropping && !fw_env("FW_NOQUADS") &&
           (label_on || (fw_hud && hud_run) || bp || gx);
}

/* the bitmap's pixels (dx, dy from the origin ox, oy; cols by rows from
   dx0, dy0) as a quad: a label's straight into the picture, the HUD's
   kept with its piece (its extents made as fw_plot's would be) */
static int bitmap_quad(int32_t ox, int32_t oy, int32_t dx0, int32_t dy0, int cols, int rows, uint8_t bank, uint16_t at,
                       uint16_t stepx, uint16_t stepy, uint16_t pal_at)
{
    uint8_t pal[4];
    int i, j;
    if (!bitmaps_on() || cols < 1 || rows < 1 || (rows > 0xFFFF && !bp && !gx))
        return 0;
    for (i = 0; i < 4; i++)
        pal[i] = ram8((uint16_t)(pal_at + i));
    if (label_on) {
        /* a label's pixel (dx, dy from the label's place) covers the
           texels col_at's way from (label_tx, label_ty) */
        int64_t DXo = ox - label_x, DYo = oy - label_y, DX0 = DXo + dx0, DY0 = DYo + dy0, last;
        int32_t c0, c1, r0, r1;
        FwSpan *p;
        if (DX0 < 0 || DY0 < 0 || DXo < 0 || DYo < 0)
            return 0;
        last = DX0 + cols - 1;          /* the blocks wholly on the texels (fw_plot's) */
        while (last >= DX0 && label_tx + floor_div((last + 1) * SXN, SXD) - 1 >= tw)
            last--;
        if (last < DX0)
            return quad_made();
        c0 = label_tx + (int32_t)floor_div(DX0 * SXN, SXD);
        c1 = label_tx + (int32_t)floor_div((last + 1) * SXN, SXD) - 1;
        r0 = label_ty + (int32_t)floor_div(DY0 * SYN, SYD);
        r1 = label_ty + (int32_t)floor_div((DY0 + rows) * SYN, SYD) - 1;
        p = quad_new(r0, r1 - r0 + 1, c0, c1, (int64_t)(r0 - label_ty + 1) * SYD - 1 - DYo * SYN, SYD);
        if (p) {
            p->xl = label_tx;
            p->u = SXD - 1 - DXo * SXN;
            p->du = SXD;
            p->mask = (uint16_t)SYN;
            bitmap_fields(p, bank, at, pal, stepx, stepy, (uint32_t)SXN);
        }
        return quad_made();
    }
    if (bp || gx) {
        /* a bitmap picture's or a job's: at the bitmap's own place, as PLOT
           puts it (the low bytes), each pixel its k x k texels: one quad for
           each piece the wraps leave, in the rows' order, a job's rows past
           the bitmap on their alias (x + 8, y less the height) */
        int32_t dy = dy0, left = rows;
        while (left > 0) {
            int32_t ly = (uint8_t)(oy + dy), n = left < 256 - ly ? left : 256 - ly, ty = ly, shift = 0, dx = dx0, lc = cols;
            if (gx && ly >= th) {
                ty = ly - th;
                shift = 8;
            } else if (gx && n > th - ly) {
                n = th - ly;
            }
            while (lc > 0) {
                int32_t lx = (uint8_t)(ox + dx), m = lc < 256 - lx ? lc : 256 - lx, tx = lx + shift;
                int32_t r0 = row_at(ty), c0 = col_at(tx);
                FwSpan *p = quad_new(r0, row_at(ty + n) - r0, c0, col_at(tx + m) - 1,
                                     (int64_t)(r0 + 1) * SYD - 1 - (int64_t)(ty - dy) * SYN, SYD);
                if (p) {
                    p->xl = 0;
                    p->u = SXD - 1 - (int64_t)(tx - dx) * SXN;
                    p->du = SXD;
                    p->mask = (uint16_t)SYN;
                    bitmap_fields(p, bank, at, pal, stepx, stepy, (uint32_t)SXN);
                }
                dx += m;
                lc -= m;
            }
            dy += n;
            left -= n;
        }
        return quad_made();
    }
    {
        /* the HUD: on the layer as fw_plot puts it (the low bytes, the
           view's offset); a bitmap across the layer's wrap is run */
        int lx0 = (uint8_t)(ox + dx0) + VIEW_X, ly0 = (uint8_t)(oy + dy0) + VIEW_Y;
        HudQuad *h;
        if ((uint8_t)(ox + dx0) + cols > 256 || (uint8_t)(oy + dy0) + rows > 256)
            return 0;
        if (nhq == cap_hq) {
            size_t c = cap_hq ? 2 * cap_hq : 256;
            HudQuad *n = realloc(hq, c * sizeof *n);
            if (!n)
                return 0;
            hq = n;
            cap_hq = c;
        }
        h = &hq[nhq++];
        h->lx = lx0 - dx0;
        h->ly = ly0 - dy0;
        h->x0 = lx0;
        h->y0 = ly0;
        h->x1 = lx0 + cols - 1;
        h->y1 = ly0 + rows - 1 < 223 ? ly0 + rows - 1 : 223;
        h->at = at;
        h->stepx = stepx;
        h->stepy = stepy;
        h->bank = bank;
        h->por = POR_NOW();
        h->hi = fw_hi();
        memcpy(h->pal, pal, 4);
        h->piece = hud_n;
        quads_made++;
        cen_quads++;
        /* the piece's extents, as its pixels would make them (fw_plot) */
        for (j = h->y0; j <= h->y1; j++)
            for (i = h->x0; i <= h->x1; i++) {
                int64_t sx = i - h->lx, sy = j - h->ly;
                if (stepx)
                    sx = (sx * stepx) >> 8;
                if (stepy)
                    sy = (sy * stepy) >> 8;
                if (bitmap_colour(bank, at, sx, sy, pal) < 0)
                    continue;
                if (i < hud_x0) hud_x0 = i;
                if (i > hud_x1) hud_x1 = i;
                if (j < hud_y0) hud_y0 = j;
                if (j > hud_y1) hud_y1 = j;
                if (in_view(i, j)) {
                    if (i < hud_vx0) hud_vx0 = i;
                    if (i > hud_vx1) hud_vx1 = i;
                    if (j < hud_vy0) hud_vy0 = j;
                    if (j > hud_vy1) hud_vy1 = j;
                }
            }
        if (!quads_both())
            return 1;
        blit_quad = 1;                  /* (a check) its pixels plotted too, the reference's */
        return 0;
    }
}

static void hud_quad(const HudQuad *h, int s, int t)
{
    int x0 = h->x0 > VIEW_X ? h->x0 : VIEW_X, x1 = h->x1 < VIEW_X + VIEW_W - 1 ? h->x1 : VIEW_X + VIEW_W - 1;
    int y0 = h->y0 > VIEW_Y ? h->y0 : VIEW_Y, y1 = h->y1 < VIEW_Y + VIEW_H - 1 ? h->y1 : VIEW_Y + VIEW_H - 1;
    int32_t c0, c1, r0, r1;
    FwSpan *p;
    if (x1 < x0 || y1 < y0)
        return;
    c0 = col_at(x0 + s);
    c1 = col_at(x1 + 1 + s) - 1;
    r0 = row_at(y0 + t);
    r1 = row_at(y1 + 1 + t) - 1;
    p = quad_new(r0, r1 - r0 + 1, c0, c1, (int64_t)(r0 + 1) * SYD - 1 - (int64_t)(t + h->ly) * SYN, SYD);
    if (!p)
        return;
    p->xl = 0;
    p->u = SXD - 1 - (int64_t)(s + h->lx) * SXN;
    p->du = SXD;
    p->mask = (uint16_t)SYN;
    bitmap_fields(p, h->bank, h->at, h->pal, h->stepx, h->stepy, (uint32_t)SXN);
    p->por = h->por;
    p->hi = h->hi;
}

/* ---- the primitives the GPU projects (scene.h ScPrim) ---- */
void fw_prim(const void *prim, int textured)
{
    int id = sc_list_prim((const ScPrim *)prim);
    FwSpan *p;
    if (id < 0 || !(p = span_new()))
        return;
    memset(p, 0, sizeof *p);
    p->textured = FW_QUAD;
    p->pad1[0] = 2;                     /* the GPU's alone */
    p->pad1[1] = (uint8_t)(64 | (textured ? 0 : 2));
    p->x0 = 1;                          /* no pixels until the GPU makes it */
    p->xl = id;
    p->bank = ((const ScPrim *)prim)->bank;
    quads_made++;
    cen_quads++;
}

void fw_prim_view(void *out)
{
    ScPrimView *v = (ScPrimView *)out;
    int64_t n, d;
    memset(v, 0, sizeof *v);
    v->tw = tw;
    v->th = th;
    v->cx = (int16_t)ram16(0x0034);
    v->cy = (int16_t)ram16(0x0036);
    v->left = (int16_t)ram16(0x0038);
    v->right = (int16_t)ram16(0x003A);
    v->top = (int16_t)ram16(0x003C);
    v->bottom = (int16_t)ram16(0x003E);
    v->rows_cap = fw_rows(0x00F0);
    v->square = fw_square();
    ws_focal_q(0, &n, &d);
    v->fxn = (int32_t)n;
    v->fxd = (int32_t)d;
    ws_focal_q(1, &n, &d);
    v->fyn = (int32_t)n;
    v->fyd = (int32_t)d;
    v->sxn = (int32_t)SXN;
    v->sxd = (int32_t)SXD;
    v->syn = (int32_t)SYN;
    v->syd = (int32_t)SYD;
}

int fw_plot_colour(void) { return plot_colour(); }

int fw_bitmap(uint16_t x, uint16_t y, uint8_t bank, uint16_t at, int rows, int bytes, uint16_t pal)
{
    /* (0 rows: the chip's LOOP counts 65536) */
    return bitmap_quad((int16_t)x, (int16_t)y, 0, 0, 4 * bytes, rows ? rows : 0x10000, bank, at, 0, 0, pal);
}

/* ---- the weather (FxWeatherDraw) as quads ---- */
int fw_plot_dots(uint16_t x, uint16_t y, int n)
{
    int c;
    FwSpan *p;
    if (!quads_on())
        return 0;
    if (sprite_hidden)                  /* (fw_plot's: none drawn) */
        return 1;
    c = plot_colour();
    if (c >= 0 && (p = quad_new((int16_t)y, n, (int16_t)x, (int16_t)x + n - 1, 0, 0))) {
        p->pad1[1] = 2;
        p->colour = (uint8_t)c;
        p->xl = (int16_t)x;
    }
    return quad_made();
}

int fw_plot_line(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t sx, uint16_t sy, uint16_t *xe, uint16_t *ye)
{
    int ymaj = dx < dy, c, isx = sx == 0xFFFF ? -1 : 1, isy = sy == 0xFFFF ? -1 : 1;
    int64_t dmaj = ymaj ? dy : dx, dmin = ymaj ? dx : dy, a0 = (dmaj - dmin) >> 1, n, st, x1, y1;
    FwSpan *p;
    if (!quads_on())
        return 0;
    /* its last pixel's place and the loop's end (one step past it) */
    n = dmaj * dmin - a0;
    st = n <= 0 ? 0 : (n + dmaj - 1) / dmaj;
    x1 = (int16_t)x + (ymaj ? isx * st : isx * dmaj);
    y1 = (int16_t)y + (ymaj ? isy * dmaj : isy * st);
    n = (dmaj + 1) * dmin - a0;
    st = n <= 0 ? 0 : (n + dmaj - 1) / dmaj;
    *xe = (uint16_t)((int16_t)x + (ymaj ? isx * st : isx * (dmaj + 1)));
    *ye = (uint16_t)((int16_t)y + (ymaj ? isy * (dmaj + 1) : isy * st));
    if (sprite_hidden)
        return 1;
    c = plot_colour();
    if (c >= 0) {
        int32_t xa = (int16_t)x < x1 ? (int16_t)x : (int32_t)x1, xb = (int16_t)x < x1 ? (int32_t)x1 : (int16_t)x;
        int32_t ya = (int16_t)y < y1 ? (int16_t)y : (int32_t)y1, yb = (int16_t)y < y1 ? (int32_t)y1 : (int16_t)y;
        if ((p = quad_new(ya, yb - ya + 1, xa, xb, 0, 0))) {
            p->pad1[1] = 2 | 32;
            p->colour = (uint8_t)c;
            p->xl = (int16_t)x;
            p->v = (int16_t)y;
            p->u = a0;
            p->du = dmaj;
            p->dv = dmin;
            p->mask = (uint16_t)(ymaj | (isx < 0 ? 2 : 0) | (isy < 0 ? 4 : 0));
        }
    }
    return quad_made();
}

int fw_bitmap_scaled(uint16_t ox, uint16_t oy, uint16_t x, uint16_t y, int cols, int rows, uint8_t bank, uint16_t at,
                     uint16_t stepx, uint16_t stepy, uint16_t pal)
{
    if (!stepx || !stepy) {             /* a step of 0: every pixel the source's first; (a step of 1 the same over the screen's 256) */
        if (!bp && !gx)
            return 0;
        stepx = stepx ? stepx : 1;
        stepy = stepy ? stepy : 1;
    }
    return bitmap_quad((int16_t)ox, (int16_t)oy, (int16_t)x - (int16_t)ox, (int16_t)y - (int16_t)oy, cols, rows, bank, at,
                       stepx, stepy, pal);
}

/* a job's row or single plot, one flat quad (PLOT's rule already taken: its
   colour opaque), not counted as a blit's pixels */
static void gx_flat(int32_t y, int32_t l, int32_t r, uint8_t c)
{
    uint64_t q = job.quad;
    FwSpan *p = quad_new(y, 1, l, r, 0, 0);
    if (p) {
        p->pad1[1] = 2;
        p->colour = c;
        p->xl = l;
    }
    job.quad = q;
}

/* FW_CENSUS (a measure, macOS): what the CPU still draws in the full
   window's pictures: each PLOT counted by where it comes from (its
   callers' addresses, less the image's slide: atos names them), by kind
   (the HUD's, a label's, the 3D's), and the draw list's entries the GPU
   does not draw from the display list, by type; printed at exit */
#define CEN_SITES 256
static struct { void *a[6]; long n[4]; } cen_site[CEN_SITES];
static void census_report(void)
{
    int i;
#ifdef FW_CENSUS_BT
    intptr_t slide = _dyld_get_image_vmaddr_slide(0);
#else
    intptr_t slide = 0;
#endif
    printf("census: plots: HUD %ld, label %ld, 3D %ld, left out %ld; draw-list entries the CPU's: flat %ld, line %ld, "
           "textured %ld; the GPU's (display list) %ld, of them not their face's %ld; quads %ld\n", cen_plots[0], cen_plots[1],
           cen_plots[2], cen_plots[3], cen_entries[0], cen_entries[1], cen_entries[2], cen_entries_gpu, cen_map_apart, cen_quads);
    for (i = 0; i < CEN_SITES && cen_site[i].a[0]; i++) {
        int k;
        printf("census site");
        for (k = 0; k < 6; k++)
            printf(" 0x%lx", (unsigned long)((intptr_t)cen_site[i].a[k] - slide));
        printf(": HUD %ld label %ld 3D %ld out %ld\n", cen_site[i].n[0], cen_site[i].n[1], cen_site[i].n[2], cen_site[i].n[3]);
    }
    fflush(stdout);
}
static void census_plot(int kind)
{
    static int on = -1;
    if (on < 0) {
        on = fw_env("FW_CENSUS") != NULL;
        if (on)
            atexit(census_report);
    }
    if (!on)
        return;
    cen_plots[kind]++;
#ifdef FW_CENSUS_BT
    {
        void *bt[9] = { 0 };
        int n = backtrace(bt, 9), i, k;
        for (i = 0; i < CEN_SITES; i++) {
            if (!cen_site[i].a[0])
                for (k = 0; k < 6; k++)
                    cen_site[i].a[k] = k + 1 < n ? bt[k + 1] : (void *)1;
            for (k = 0; k < 6; k++)
                if (cen_site[i].a[k] != (k + 1 < n ? bt[k + 1] : (void *)1))
                    break;
            if (k == 6) {
                cen_site[i].n[kind]++;
                break;
            }
        }
    }
#endif
}

void fw_plot(uint16_t x, uint16_t y)
{
    int c = plot_colour(), j;

    census_plot(c < 0 ? 3 : fw_hud ? 0 : label_on ? 1 : sprite_hidden && !checking ? 3 : 2);

    if (gx) {                           /* a job's, as gsu_plot's: the low bytes, dither on a 4-bit screen, its alias */
        int px = (uint8_t)x, py = (uint8_t)y;
        if (c < 0)
            return;
        if ((gsu.por & 2) && bit4) {
            if ((px ^ py) & 1)
                c >>= 4;
            c &= 0x0F;
        }
        if (py >= th) {
            py -= th;
            px += 8;
        }
        job.plots++;
        gx_flat(py, px, px, (uint8_t)c);
        return;
    }
    if (bp && !label_on) {              /* a bitmap picture's program: at its own place, k x k texels */
        int px = (uint8_t)x, py = (uint8_t)y;
        if (c >= 0 && px < bp_cols * 8 && py < bp_rows * 8)
            block_set(px, py, (uint8_t)c);
        return;
    }

    if (label_on && c >= 0 && !checking) {
        int64_t dx = (int)(int16_t)x - label_x, dy = (int)(int16_t)y - label_y;
        int px0 = label_tx + (int)floor_div(dx * SXN, SXD), px1 = label_tx + (int)floor_div((dx + 1) * SXN, SXD) - 1;
        int py0 = label_ty + (int)floor_div(dy * SYN, SYD), py1 = label_ty + (int)floor_div((dy + 1) * SYN, SYD);
        for (j = py0; j < py1; j++)
            if (px0 >= 0 && px1 < tw)
                span_flat(j, px0, px1, (uint8_t)c);
        return;
    }

    if (c < 0 || (fw_hud && cropping))
        return;
    if (fw_hud && hud_run) {
        int px = (uint8_t)x + VIEW_X, py = (uint8_t)y + VIEW_Y;
        if (py >= 224)
            return;
        hud_px[py][px] = (uint16_t)(c + 1);
        hud_id[py][px] = (uint8_t)hud_n;
        hud_qd[py][px] = (uint8_t)blit_quad;
        if (px < hud_x0) hud_x0 = px;
        if (px > hud_x1) hud_x1 = px;
        if (py < hud_y0) hud_y0 = py;
        if (py > hud_y1) hud_y1 = py;
        if (in_view(px, py)) {
            if (px < hud_vx0) hud_vx0 = px;
            if (px > hud_vx1) hud_vx1 = px;
            if (py < hud_vy0) hud_vy0 = py;
            if (py > hud_vy1) hud_vy1 = py;
        }
        return;
    }
    if (fw_hud) {
        int px = hud_x((uint8_t)x), py = hud_y((uint8_t)y);
        if (px < 0 || px >= W || py < 0 || py >= H)
            return;
        block_set(px, py, (uint8_t)c);
        return;
    }
    if (checking) {
        if (x < tw && y < th)
            tex[(size_t)y * tw + x] = (uint8_t)c;
        check_alias(x, y, c);
    } else if (sprite_hidden) {
        return;
    } else if (x < tw && y < th) {
        span_flat(y, x, x, (uint8_t)c);
    }
}

static void gx_sync(void);

uint8_t fw_rpix(uint16_t x, uint16_t y)
{
    if (gx) {                           /* a job's: what is drawn so far in the chip's RAM, read there */
        gx_sync();
        return gsu_rpix((uint8_t)x, (uint8_t)y);
    }
    if (bp)                             /* a pixel-cache flush whose value goes unread (the census) */
        return 0;
    if (fw_hud && hud_run) {
        int px = (uint8_t)x + VIEW_X, py = (uint8_t)y + VIEW_Y;
        return py < 224 && hud_px[py][px] ? (uint8_t)(hud_px[py][px] - 1) : 0;
    }
    if (fw_hud) {
        int px = hud_x((uint8_t)x), py = hud_y((uint8_t)y);
        if (px < 0 || px >= W || py < 0 || py >= H)
            return 0;
        if (gpu_pic) {
            span_flush();
            if (gpu_pic)
                return fw_canvas_gpu->read(col_at(px), row_at(py));
        }
        return tex[(size_t)row_at(py) * tw + col_at(px)];
    }
    span_flush();                       /* a read of the 3D: what is drawn so far */
    if (gpu_pic)
        return x < tw && y < th ? fw_canvas_gpu->read(x, y) : 0;
    return x < tw && y < th ? tex[(size_t)y * tw + x] : 0;
}

void gsu_plot16(uint16_t x, uint16_t y)
{
    if (fw_drawing)
        fw_plot(x, y);
    else if (!fw_silent)
        gsu_plot((uint8_t)x, (uint8_t)y);
}

uint8_t gsu_rpix16(uint16_t x, uint16_t y)
{
    return fw_drawing ? fw_rpix(x, y) : fw_silent ? 0 : gsu_rpix((uint8_t)x, (uint8_t)y);
}

static void clear_now(void);
void fw_clear(void)
{
    if (gx || two)                      /* a job's: FxScreenClear's own (fw_job_clear); two players': one, before the first (two_begin) */
        return;
    clear_now();
}

static void clear_now(void)
{
    nspans = npolys = nedges = 0;       /* drawn over: none of them shows */
#ifdef SRFX_NSHVC_HANDOFF
    if (hand_pic() || (bp && fw_chars_on())) {
        if (fw_handoff->clear)          /* (the GE: a canvas of its own, opened cleared below) */
            fw_handoff->clear(tw, th);
    } else
#endif
    memset(tex, 0, (size_t)tw * th);
    if (gpu_pic) {                    /* the GPU's picture opened again, cleared */
        gpu_cleared = 1;
        gpu_open = 0;
    }
}

/* ---- the view ------------------------------------------------------------ */

/* the one-player race's centre and edges, as ViewParamsByPlayers sets them */
static const uint16_t k_race1p[6] = {0x0068, 0x0040, 0x0000, 0x00CF, 0x0000, 0x0080};

/* the focal length across and down in texels, 128 console pixels each:
   num / den in lowest terms, 128 SXN / SXD and 128 SYN / SYD */
void fw_focal_q(int down, int64_t *num, int64_t *den)
{
    static int64_t key[2][2] = { { -1, -1 }, { -1, -1 } }, kn[2], kd[2];
    int64_t n = 128 * (down ? SYN : SXN), d = down ? SYD : SXD, a = n, b = d;
    down = down != 0;
    if (key[down][0] != n || key[down][1] != d) {   /* (kept: its divisions are library calls on a 32-bit host) */
        key[down][0] = n, key[down][1] = d;
        while (b) {
            int64_t r = a % b;
            a = b;
            b = r;
        }
        kn[down] = n / a;
        kd[down] = d / a;
    }
    *num = kn[down];
    *den = kd[down];
}
/* (kept, as centre_row: doubles are software on some hosts) */
static float focal_xf, focal_yf;        /* the same as floats (fw_focal_x's last, fw_focal_xf) */
double fw_focal_x(void)
{
    static double f;
    static int64_t n = -1, d = -1;
    if (n != SXN || d != SXD)
        n = SXN, d = SXD, f = 128.0 * SXN / SXD, focal_xf = (float)f;
    return f;
}
double fw_focal_y(void)
{
    static double f;
    static int64_t n = -1, d = -1;
    if (n != SYN || d != SYD)
        n = SYN, d = SYD, f = 128.0 * SYN / SYD, focal_yf = (float)f;
    return f;
}
/* (float)fw_focal_x() and (float)fw_focal_y(), with no conversion each
   time (a double's is software on some hosts) */
static float fw_focal_xf(void) { fw_focal_x(); return focal_xf; }
static float fw_focal_yf(void) { fw_focal_y(); return focal_yf; }
int fw_half_width(void) { return view_cx(); }
void fw_view_centre(float *cx, float *cy)
{
    *cx = (float)view_cx();
    *cy = centre_rowf();
}

int fw_view_words(const uint16_t w[6], uint16_t out[8])
{
    int i, cy = (int)centre_row(), below = th - cy;

    if (!fw_drawing || fw_hud || bp || gx)
        return 0;
    if (two) {                          /* its player's rectangle, the frustum planes for it */
        int x0 = col_at(two_x[two_p]), x1 = col_at(two_x[two_p] + two_w) - 1;
        int y0 = row_at(two_y[two_p]), y1 = row_at(two_y[two_p] + two_h);
        int cx = view_cx(), hw = cx - x0 > x1 + 1 - cx ? cx - x0 : x1 + 1 - cx, hh = cy - y0 > y1 - cy ? cy - y0 : y1 - cy;
        out[0] = (uint16_t)cx;
        out[1] = (uint16_t)cy;
        out[2] = (uint16_t)x0;
        out[3] = (uint16_t)x1;
        out[4] = (uint16_t)y0;
        out[5] = (uint16_t)y1;
        out[6] = (uint16_t)(((uint64_t)128 * SXN << 14) / ((uint64_t)SXD * (uint64_t)hw));
        out[7] = (uint16_t)(((uint64_t)128 * SYN << 13) / ((uint64_t)SYD * (uint64_t)hh));
        return 1;
    }
    for (i = 0; i < 6; i++)
        if (w[i] != k_race1p[i])
            return 0;
    out[0] = (uint16_t)(tw / 2);        /* the centre */
    out[1] = (uint16_t)cy;
    out[2] = 0;                         /* the edges, as the original's: the right one
                                           its last column, the bottom one past its last row */
    out[3] = (uint16_t)(tw - 1);
    out[4] = 0;
    out[5] = (uint16_t)th;
    /* the frustum planes (view.c's rule) for the half-width and the larger
       of the parts above and below the centre */
    out[6] = (uint16_t)(((uint64_t)128 * SXN << 14) / ((uint64_t)SXD * (uint64_t)(tw / 2)));
    out[7] = (uint16_t)(((uint64_t)128 * SYN << 13) / ((uint64_t)SYD * (uint64_t)(below > cy ? below : cy)));
    return 1;
}

/* the view's eight words in the Super FX's RAM, as GsuParamsFromDp puts
   them there (once, at the race's start), and the game's own kept while a
   full-window picture is drawn */
static const uint16_t view_at[8] = {0x0034, 0x0036, 0x0038, 0x003A, 0x003C, 0x003E, 0x0092, 0x0094};
static uint16_t view_game[8];
static int view_set;

int fw_view_active(void) { return fw_drawing && !fw_hud && view_set; }

void fw_view_begin(void)
{
    uint8_t *s = nshvc_sram();
    uint16_t full[8];
    int i;

    view_set = 0;
    if (!fw_drawing || checking || bp)
        return;
    two_p = 0;
    for (i = 0; i < 8; i++)
        view_game[i] = (uint16_t)(s[view_at[i]] | s[view_at[i] + 1] << 8);
    if (!fw_view_words(view_game, full))
        return;
    for (i = 0; i < 8; i++) {
        s[view_at[i]] = (uint8_t)full[i];
        s[view_at[i] + 1] = (uint8_t)(full[i] >> 8);
    }
    view_set = 1;
}

/* two players: the second's view, for its sort and picture */
void fw_two_next(void)
{
    uint8_t *s = nshvc_sram();
    uint16_t full[8];
    int i;
    if (!two || !fw_drawing)
        return;
    two_p = 1;
    if (!fw_view_words(view_game, full))
        return;
    for (i = 0; i < 8; i++) {
        s[view_at[i]] = (uint8_t)full[i];
        s[view_at[i] + 1] = (uint8_t)(full[i] >> 8);
    }
}

/* the game's words back, for the HUD's pieces (GsuPieceOffsetFromView
   reads the edges) */
void fw_hud_begin(void)
{
    uint8_t *s = nshvc_sram();
    int i;

    if (!fw_drawing)
        return;
    fw_hud = 1;
    if (view_set)
        for (i = 0; i < 8; i++) {
            s[view_at[i]] = (uint8_t)view_game[i];
            s[view_at[i] + 1] = (uint8_t)(view_game[i] >> 8);
        }
}

void fw_hud_end(void)
{
    if (fw_hud && hud_n)
        hud_place();
    fw_hud = 0;
}

/* ---- FxDrawList's fill at 16 bits ---------------------------------------- */
/*
 * The original's fill (raster.c, 01:A931; draw_list.c lifts it) with every
 * coordinate a whole word: the vertices' x and y words, the edges in 24.8,
 * the rows past 255. The rules are the original's: the list's entries, the
 * edge walks, the slopes from the ROM's reciprocals (32768 / n, which the
 * table holds, its division past 255 rows), an edge's clamp just below 0,
 * the textured rows' u and v steps, PLOT's colour rules. It works on the
 * list in the Super FX's RAM as FxDrawList does (each polygon's vertices
 * copied below its entry); the redraw puts the RAM back after.
 */
#define LIST_END    0x005Cu
#define DRAW_LIST   0x08AAu
#define RECIPROCALS 0x0094A0u           /* bank $00: 1/n words for n 0-255 */
#define TEX_U_OFF   0x00C0u
#define TEX_V_OFF   0x00C2u

static uint8_t ram8(uint16_t a) { return gsu_rd8(0x700000u | a); }
static uint16_t ram16(uint16_t a) { return (uint16_t)(ram8(a) | ram8((uint16_t)(a + 1)) << 8); }
static void ram8w(uint16_t a, uint8_t v) { gsu_wr8(0x700000u | a, v); }
static void ram16w(uint16_t a, uint16_t v) { ram8w(a, (uint8_t)v); ram8w((uint16_t)(a + 1), (uint8_t)(v >> 8)); }

/* the draw list's vertices the projection held, exact (below, vw) */
static int32_t sw_val[0x8000];
static uint32_t sw_gen[0x8000], list_gen = 1;

/* ---- the screen clip from the projection's exact results ------------------ */
/*
 * The projection holds a point's offset from the screen's centre to $3FFF
 * (FxRatioClamp keeps its direction from the centre), which the original's
 * 208-column view meets only beside the camera; at k texels a pixel the
 * same 16 bits are k times closer to the screen, so the screen clip's
 * edges towards a held point, a face's facing and a textured face's
 * texture would all change with k (a ceiling over the road lost at k = 4).
 * So in the full window each polygon's vertices are also kept in 64 bits
 * as the projection makes them with its hold scaled by k (project_exact),
 * and the screen clip (FxPolygonClipScreen), the model drawer's facing and
 * the fill of textured faces (which are never screen clipped) use those:
 * the picture is the same at every k. The vertices come from
 * FxPolygonGather (its projection loop's inputs) or the model drawer's
 * point table by index (the list it writes into the draw list); where the
 * projection's own vertex is within 16 texels of the scaled one it is used
 * as it stands, so a clipped face meets its unclipped neighbours.
 */
#define EXACT_MAX 32
#define NEAR_Z 1                        /* the full window's near plane, in view space */
static int64_t ex_x[EXACT_MAX], ex_y[EXACT_MAX];
static int32_t ex_vx[EXACT_MAX], ex_vy[EXACT_MAX], ex_vz[EXACT_MAX];  /* the model drawer's points in view space */
static int ex_n = -1;
static uint16_t ex_list;

void fw_exact_begin(uint16_t list)
{
    unsigned i;

    for (i = 0; i < EXACT_MAX * 4; i++)  /* the list's words: none held yet (a polygon dropped may have left some) */
        sw_gen[(uint16_t)(list + 2 * i) >> 1] = 0;
    ex_list = list;
    ex_n = fw_view_active() ? 0 : -1;
}

/* A point's place on the screen from its view-space point, k times the
   console's: the projection's rule, centre + X f / Z, with its hold scaled
   by k: past $3FFF k texels from the centre on its larger axis the point
   is drawn in, along its line from the centre, to that distance, as
   FxRatioClamp holds it at $3FFF in the console's pixels. So every k
   draws the same picture, k times as fine. */
static void project_exact(int64_t x, int64_t y, int64_t z, int64_t *sx, int64_t *sy)
{
    int64_t rx, ry, a, b;

    if (!z)
        z = 1;
    rx = x * 128 * SXN / (z * SXD);
    ry = y * 128 * SYN / (z * SYD);
    /* the larger in console pixels, |rx| SXD / SXN or |ry| SYD / SYN, held
       to $3FFF: compared over the common SXN SYN */
    a = llabs(rx) * SXD * SYN;
    b = llabs(ry) * SYD * SXN;
    if (a > b && a > 0x3FFFLL * SXN * SYN) {
        int64_t d = llabs(rx) * SXD;
        rx = rx * 0x3FFF * SXN / d;
        ry = ry * 0x3FFF * SXN / d;
    } else if (a <= b && b > 0x3FFFLL * SXN * SYN) {
        int64_t d = llabs(ry) * SYD;
        rx = rx * 0x3FFF * SYN / d;
        ry = ry * 0x3FFF * SYN / d;
    }
    *sx = view_cx() + rx;
    *sy = centre_row() + ry;
}

/* the 1 / depth across the screen of a face through n view-space points,
   at each pixel's centre: z[0] + z[1] x + z[2] y; a far object's (its
   place halved rm_far_shift times) its own depth's. 1 / depth is linear
   on the screen across a flat face, as a GPU interpolates it: through the
   points' own places and depths, the three in front of the eye farthest
   apart on the screen (the points are whole units, so a small face's
   plane in space can tilt far from them); with fewer than three in front,
   the plane through them in space. 0s when neither can be had. */
static long *depth_counts;
static void depth_report(void)
{
    int i;
    for (i = 0; i < 4; i++)
        fprintf(stderr, "fwdepth: faces of screen area class %d: %ld within 1%%, %ld within 10%%, %ld more\n", i,
                depth_counts[3 * i], depth_counts[3 * i + 1], depth_counts[3 * i + 2]);
}

static void depth_plane(const float (*P)[3], int n, float z[3])
{
    float fx = fw_focal_xf(), fy = fw_focal_yf(), ox = (float)view_cx(), oy = centre_rowf(), sc = (float)(1 << rm_far_shift);
    float X[EXACT_MAX + 8], Y[EXACT_MAX + 8], Q[EXACT_MAX + 8], best = 0, c[3] = { 0 }, d;
    int i, j, k, m = 0, a = 0, b = 1, e = 2;

    z[0] = z[1] = z[2] = 0;
    for (i = 0; i < n && m < EXACT_MAX + 8; i++)
        if (P[i][2] >= NEAR_Z) {
            X[m] = ox + fx * P[i][0] / P[i][2] - 0.5f;
            Y[m] = oy + fy * P[i][1] / P[i][2] - 0.5f;
            Q[m++] = 1.0f / (P[i][2] * sc);
        }
    for (i = 0; i < m; i++)
        for (j = i + 1; j < m; j++)
            for (k = j + 1; k < m; k++) {
                float ar = fabsf((X[j] - X[i]) * (Y[k] - Y[i]) - (X[k] - X[i]) * (Y[j] - Y[i]));
                if (ar > best)
                    best = ar, a = i, b = j, e = k;
            }
    if (best > 1e-6f) {                  /* through three on the screen */
        float x1 = X[b] - X[a], y1 = Y[b] - Y[a], q1 = Q[b] - Q[a], x2 = X[e] - X[a], y2 = Y[e] - Y[a], q2 = Q[e] - Q[a];
        float det = x1 * y2 - x2 * y1, gx = (q1 * y2 - q2 * y1) / det, gy = (x1 * q2 - x2 * q1) / det;
        z[1] = (float)gx;
        z[2] = (float)gy;
        z[0] = (float)(Q[a] - gx * X[a] - gy * Y[a]);
    } else if (m >= 1 && m == n) {      /* edge-on on the screen: its nearest point's */
        for (i = 1, d = Q[0]; i < m; i++)
            d = Q[i] > d ? Q[i] : d;
        z[0] = (float)d;
    } else {                            /* in space */
        for (i = 0, best = 0; i < n; i++)
            for (j = i + 1; j < n; j++)
                for (k = j + 1; k < n; k++) {
                    float u[3], v[3], w[3], mm;
                    int q;
                    for (q = 0; q < 3; q++)
                        u[q] = P[j][q] - P[i][q], v[q] = P[k][q] - P[i][q];
                    w[0] = u[1] * v[2] - u[2] * v[1];
                    w[1] = u[2] * v[0] - u[0] * v[2];
                    w[2] = u[0] * v[1] - u[1] * v[0];
                    mm = w[0] * w[0] + w[1] * w[1] + w[2] * w[2];
                    if (mm > best)
                        best = mm, a = i, c[0] = w[0], c[1] = w[1], c[2] = w[2];
                }
        if (best <= 0)
            return;
        d = (c[0] * P[a][0] + c[1] * P[a][1] + c[2] * P[a][2]) * sc;
        if (d == 0)
            return;
        z[1] = (float)(c[0] / (fx * d));
        z[2] = (float)(c[1] / (fy * d));
        z[0] = (float)((c[0] * (0.5f - ox) / fx + c[1] * (0.5f - oy) / fy + c[2]) / d);
    }
    if (fw_env("FW_DEPTHCHECK")) {      /* a check: the 1 / depth at each point's own place, by the face's size */
        static long cnt[4][3];          /* by screen area < 16, < 256, < 4096, more; error < 1 %, < 10 %, more */
        static int reg;
        float worst = 0, area = best;
        int sz;
        for (i = 0; i < m; i++) {
            float got = z[0] + z[1] * X[i] + z[2] * Y[i], e = fabsf(got - Q[i]) / Q[i];
            worst = e > worst ? e : worst;
        }
        sz = area < 32 ? 0 : area < 512 ? 1 : area < 8192 ? 2 : 3;
        cnt[sz][worst < 0.01f ? 0 : worst < 0.1f ? 1 : 2]++;
        if (!reg) {
            reg = 1;
            atexit(depth_report);
        }
        depth_counts = &cnt[0][0];
    }
}

/* the face whose vertices are being kept (ex_list): its depth from those
   in view space so far */
static void face_depth_kept(void)
{
    float P[EXACT_MAX][3];
    int i;
    if (ex_n < 3)
        return;
    for (i = 0; i < ex_n; i++)
        P[i][0] = ex_vx[i], P[i][1] = ex_vy[i], P[i][2] = ex_vz[i];
    depth_plane((const float (*)[3])P, ex_n, face_depth[ex_list >> 1].z);
    face_depth[ex_list >> 1].gen = list_gen;
}

void fw_exact_unmark(uint16_t list)
{
    unsigned i;

    for (i = 0; i < EXACT_MAX * 4; i++)
        sw_gen[(uint16_t)(list + 2 * i) >> 1] = 0;
}

void fw_exact_point(int16_t x, int16_t y, int16_t z)
{
    if (ex_n < 0 || ex_n >= EXACT_MAX) {
        ex_n = -1;
        return;
    }
    project_exact(x, y, z, &ex_x[ex_n], &ex_y[ex_n]);
    ex_vx[ex_n] = x;
    ex_vy[ex_n] = y;
    ex_vz[ex_n] = z;
    ex_n++;
    face_depth_kept();
}

/* the model drawer's point by index, exact */
static void model_view(uint16_t index, int32_t *x, int32_t *y, int32_t *z)
{
    uint16_t a = (uint16_t)(0x0340 + 6 * index);

    *x = (int16_t)(ram16(a) + ram16(0x0026));
    *y = (int16_t)(ram16((uint16_t)(a + 2)) + ram16(0x0028));
    *z = (int16_t)(ram16((uint16_t)(a + 4)) + ram16(0x002A));
    if (*z >= 0x100 && *z > 0x43FE)     /* the model drawer's far points: their depth held at $43FE */
        *z = 0x43FE;
}

static void model_point(uint16_t index, int64_t *sx, int64_t *sy)
{
    int32_t x, y, z;

    model_view(index, &x, &y, &z);
    project_exact(x, y, z, sx, sy);
}

void fw_exact_model_point(uint16_t index)
{
    if (ex_n < 0 || ex_n >= EXACT_MAX) {
        ex_n = -1;
        return;
    }
    model_point(index, &ex_x[ex_n], &ex_y[ex_n]);
    model_view(index, &ex_vx[ex_n], &ex_vy[ex_n], &ex_vz[ex_n]);
    ex_n++;
    face_depth_kept();
}

/* FxModelFacing's cross product of three projected points ($04E4 + 6 i),
   from the exact points: the facing of a face with a point the projection
   held would turn with it */
static uint16_t facing_at[3];
int64_t fw_facing_log[64];
int fw_facing_log_n;

void fw_facing_point(int slot, uint16_t addr)
{
    facing_at[slot] = addr;
}

void fw_facing_cross(uint16_t *hi, uint16_t *lo)
{
    int64_t x[3], y[3], c;
    int32_t vx[3], vy[3], vz[3];
    int i, behind = 0;

    if (!fw_view_active())
        return;
    for (i = 0; i < 3; i++) {
        model_point((uint16_t)((facing_at[i] - 0x04E4) / 6), &x[i], &y[i]);
        model_view((uint16_t)((facing_at[i] - 0x04E4) / 6), &vx[i], &vy[i], &vz[i]);
        behind |= vz[i] < NEAR_Z;
    }
    c = (x[1] - x[0]) * (y[2] - y[0]) - (y[1] - y[0]) * (x[2] - x[0]);
    if (behind) {
        /* a point behind the view: the projected cross product is f^2
           det(p0, p1, p2) / (z0 z1 z2), its sign det's times z0 z1 z2's
           (FxModelFacing then XORs it with the points' bytes, which mark
           those behind, z < 0, as FxProjectPoint does: the face's facing
           from the points themselves) */
        double d = (double)vx[0] * ((double)vy[1] * vz[2] - (double)vz[1] * vy[2]) -
                   (double)vy[0] * ((double)vx[1] * vz[2] - (double)vz[1] * vx[2]) +
                   (double)vz[0] * ((double)vx[1] * vy[2] - (double)vy[1] * vx[2]);
        int odd = (vz[0] < 0) ^ (vz[1] < 0) ^ (vz[2] < 0);
        c = d == 0 ? 0 : (d > 0) != odd ? 0x10000 : -0x10000;
    }
    if (fw_facing_log_n < 64)               /* the renderer's check (scene.h): each triple's cross as made */
        fw_facing_log[fw_facing_log_n++] = c;
    if (c > 0x7FFFFFFF)
        c = 0x7FFFFFFF;
    if (c < -0x7FFFFFFFLL - 1)
        c = -0x7FFFFFFFLL - 1;
    *hi = (uint16_t)((uint32_t)c >> 16);
    *lo = (uint16_t)c;
    gsu.s = (*hi & 0x8000) != 0;
    gsu.z = *hi == 0;
}

/* the polygon (or a line, 2 vertices) clipped to the view's edges, as
   FxPolygonClipScreen's: inside x from the left edge to the right one and
   y from the top to the bottom one, both ends included; a line's crossing
   with the bottom edge one row inside it */
static long exact_seen, exact_held, exact_off;

static void exact_report(void)
{
    fprintf(stderr, "fwexact: %ld vertices clipped, %ld held at $3FFF, %ld others off by more than 2\n", exact_seen,
            exact_held, exact_off);
}

int fw_exact_clip(void)
{
    static int64_t px[2][EXACT_MAX * 2], py[2][EXACT_MAX * 2];
    int n = gsu.r[0], e, i, m, s = 0;
    int64_t edge[4];
    uint16_t list;

    /* nothing read unless in the full window: the Super FX's accesses are
       timed in Original */
    if (!fw_view_active() || ex_n < 2 || n != ex_n)
        return 0;
    list = ram16(0x005A);
    if (list != ex_list)
        return 0;
    if (fw_env("FW_EXACTCHECK")) {      /* a check: the exact vertices against the projection's, where it did not hold them */
        static long seen, held, off;
        static int reported;
        for (i = 0; i < n; i++) {
            int64_t x = (int16_t)ram16((uint16_t)(list + 4 * i)), y = (int16_t)ram16((uint16_t)(list + 4 * i + 2));
            int64_t rx = x - view_cx(), ry = y - centre_row();
            seen++;
            if (rx >= 0x3FF0 || rx <= -0x3FF0 || ry >= 0x3FF0 || ry <= -0x3FF0) {
                held++;
                continue;
            }
            if (llabs(x - ex_x[i]) > 2 || llabs(y - ex_y[i]) > 2) {
                if (++off <= 10)
                    fprintf(stderr, "fwexact: vertex %d,%d against exact %lld,%lld\n", (int)x, (int)y, (long long)ex_x[i], (long long)ex_y[i]);
            }
        }
        if (!reported) {
            reported = 1;
            atexit(exact_report);
        }
        exact_seen = seen;
        exact_held = held;
        exact_off = off;
    }
    edge[0] = (int16_t)ram16(0x0038);
    edge[1] = (int16_t)ram16(0x003A);
    edge[2] = (int16_t)ram16(0x003C);
    edge[3] = (int16_t)ram16(0x003E);
    /* the projection's own vertex where it did not hold it (the far
       points' table rounds a few texels from the exact, and the clipped
       polygon must meet its neighbours), the exact one where it held or
       wrapped it */
    for (i = 0; i < n; i++) {
        int64_t x = (int16_t)ram16((uint16_t)(list + 4 * i)), y = (int16_t)ram16((uint16_t)(list + 4 * i + 2));
        int own = llabs(x - ex_x[i]) <= 16 && llabs(y - ex_y[i]) <= 16;
        px[0][i] = own ? x : ex_x[i];
        py[0][i] = own ? y : ex_y[i];
    }
    for (e = 0; e < 4 && n; e++) {
        int vertical = e < 2, high = e & 1;
        int64_t E = edge[e];
        m = 0;
        for (i = 0; i < (n == 2 ? 1 : n); i++) {
            int j = (i + 1) % n;
            int64_t ax = px[s][i], ay = py[s][i], bx = px[s][j], by = py[s][j];
            int64_t av = vertical ? ax : ay, bv = vertical ? bx : by;
            int ain = high ? av <= E : av >= E, bin = high ? bv <= E : bv >= E;
            int64_t at = n == 2 && e == 3 ? E - 1 : E;
            if (n == 2) {                   /* a line: both ends kept, an outside one moved to the crossing */
                int64_t q[2][2] = {{ax, ay}, {bx, by}};
                int k;
                if (!ain && !bin) {
                    m = 0;
                    break;
                }
                for (k = 0; k < 2; k++)
                    if (!(k ? bin : ain)) {
                        int64_t d = bv - av;
                        if (vertical) {
                            q[k][0] = at;
                            q[k][1] = ay + (E - av) * (by - ay) / d;
                        } else {
                            q[k][1] = at;
                            q[k][0] = ax + (E - av) * (bx - ax) / d;
                        }
                    }
                px[!s][0] = q[0][0]; py[!s][0] = q[0][1];
                px[!s][1] = q[1][0]; py[!s][1] = q[1][1];
                m = 2;
                break;
            }
            if (ain) {
                px[!s][m] = ax;
                py[!s][m++] = ay;
            }
            if (ain != bin && m < EXACT_MAX * 2) {   /* the crossing */
                int64_t d = bv - av;
                if (vertical) {
                    px[!s][m] = E;
                    py[!s][m++] = ay + (E - av) * (by - ay) / d;
                } else {
                    px[!s][m] = ax + (E - av) * (bx - ax) / d;
                    py[!s][m++] = E;
                }
            }
        }
        n = m;
        s = !s;
        if (n < 3 && !(n == 2 && ex_n == 2))
            n = 0;
    }
    for (i = 0; i < n; i++) {
        ram16w((uint16_t)(list + 4 * i), (uint16_t)px[s][i]);
        ram16w((uint16_t)(list + 4 * i + 2), (uint16_t)py[s][i]);
        sw_gen[(uint16_t)(list + 4 * i) >> 1] = sw_gen[(uint16_t)(list + 4 * i + 2) >> 1] = 0;
    }
    gsu.r[0] = (uint16_t)n;
    ram16w(0x0098, (uint16_t)n);
    ex_n = -1;
    return 1;
}


static uint16_t reciprocal(uint32_t n)
{
    uint32_t at = RECIPROCALS + 2u * n;
    if (n < 256)
        return (uint16_t)(gsu_rd8(at) | gsu_rd8(at + 1) << 8);
    return (uint16_t)(32768u / n);
}

/* an edge's slope in 24.8: dx times the reciprocal of its height, the
   product's bits from 7 up; past 255 rows the division it stands for */
static int64_t slope(int64_t dx, uint32_t rows)
{
    if (rows >= 256)
        return (dx * 256) / (int64_t)rows;
    return (dx * (int16_t)reciprocal(rows)) >> 7;
}

/* The full window's own pictures: an edge's x (24.8) at a row exactly,
   from its two vertices, from + (to - from) (row - y0) / rows, rounded
   down; and each edge from the vertex the last one ended at. The
   original's 24.8 slope, its fraction cut, and its next edge from where
   the last one's steps left it, hold for the short edges of its screen;
   an edge from a point the near plane's clip puts tens of thousands of
   rows away is off by a hundred texels or more at its end, by a
   different amount each picture, and the face's part on the screen
   moves and goes (a wall's sign beside the car). The check's pass (the
   original's view) keeps the original's. */
static int64_t edge_x(int64_t from, int64_t to, int32_t y0, int32_t rows, int32_t row)
{
    int64_t num = (to - from) * 256 * (int64_t)(row - y0), q;
    if (num > -0x7FFFFFFF && num < 0x7FFFFFFF) {   /* within 32 bits: the same quotient by a 32-bit division */
        int32_t n32 = (int32_t)num;
        q = n32 / rows;
        if (n32 % rows && n32 < 0)
            q--;
    } else {
        q = num / rows;
        if (num % rows && num < 0)
            q--;
    }
    return from * 256 + q;
}

/* an edge's x stepped: set to 0 when it has gone just below it (to -8.0) */
static int64_t edge_step(int64_t x, int64_t step)
{
    x += step;
    if (x < 0 && x >= -0x800)
        x = 0;
    return x;
}

/* ---- polygons whole: their rows made where they are filled -------------- */
/*
 * A polygon of the full window's own pictures (flat_polygon, tex_polygon)
 * is kept as its edges, and each of its rows made from them by
 * fw_poly_row where the spans are filled: on the GPU (the front end's,
 * each row a thread) or here. A row is the original's: its ends each
 * edge's x at the row (edge_x), a textured row from its left end's
 * column for the width the original takes, a flat one from left to right
 * when right is not left of left. u and v at a row's ends are each edge's
 * at the row, from its start's and its change over its rows, in 32.32;
 * the row's step their difference, the nearer turn, over its width.
 * Perspective-correct, u / z, v / z and 1 / z step evenly instead, along
 * each edge and across the row, in float, every multiply-add written as
 * one (the GPU's the same steps, so the two fill alike).
 */
static int64_t edge_uv(int32_t a, int32_t d, int32_t y0, int32_t n, int32_t row)
{
    return ((int64_t)a << 32) + ((int64_t)d << 32) * (row - y0) / n;
}

static uint16_t row_step(uint16_t r, uint16_t l, uint32_t n);

static const FwEdge *edge_at(const FwEdge *e, int32_t n, int32_t row)
{
    int32_t i;
    for (i = 0; i + 1 < n; i++)
        if (row < e[i].y0 + e[i].n)
            break;
    return &e[i];
}

/* an edge's u or v (a, d its start and change) and 1 / z at a row */
static float fwedge_persp(const FwEdge *e, int32_t a, int32_t d, int32_t row, float *q)
{
    float t = (float)(row - e->y0) / (float)e->n, a0 = e->qa * (float)a;
    float da = fw_fmaf(e->qb, (float)(a + d), -a0);
    *q = fw_fmaf(t, e->qb - e->qa, e->qa);
    return fw_fmaf(t, da, a0) / *q;
}

int fw_poly_row(const FwPoly *p, const FwEdge *edges, int32_t row, int32_t w, FwSpan *s)
{
    const FwEdge *l = edge_at(edges + p->le, p->ln, row), *r = edge_at(edges + p->re, p->rn, row);
    int64_t xl = edge_x(l->from, l->to, l->y0, l->n, row) >> 8, xr = edge_x(r->from, r->to, r->y0, r->n, row) >> 8;
    int64_t width, x0, x1, uL, uR, vL, vR, d;

    if (p->pad[0]) {                    /* the chip's: each edge's x stepped */
        xl = fw_chip_x(l->pad[0], l->pad[1], row - l->y0) >> 8;
        xr = fw_chip_x(r->pad[0], r->pad[1], row - r->y0) >> 8;
    }
    memset(s, 0, sizeof *s);
    s->y = row;
    s->za = p->za;
    s->zx = p->zx;
    s->zy = p->zy;
    if (!p->kind) {                     /* flat: left to right, kept to the screen */
        if (xr < xl)
            return 0;
        x0 = xl < 0 ? 0 : xl;
        x1 = xr >= w ? w - 1 : xr;
        if (x1 < x0)
            return 0;
        s->x0 = (int32_t)x0;
        s->x1 = (int32_t)x1;
        s->colour = p->colour;
        return 1;
    }
    width = xr >= xl ? (xr - xl + 1 > 0xFFFFFFF ? 0xFFFFFFF : xr - xl + 1) : 1;
    x0 = xl < 0 ? 0 : xl;
    x1 = xl + width < w ? xl + width : w;
    if (x0 >= x1)
        return 0;
    s->x0 = (int32_t)x0;
    s->x1 = (int32_t)(x1 - 1);
    s->xl = xl;
    s->bank = p->bank;
    s->base = p->base;
    s->mask = p->mask;
    s->por = p->por;
    s->hi = p->hi;
    s->textured = 1;
    if (p->pad[0]) {                    /* the chip's 8.8 u and v: each edge's by its steps, the row's by the reciprocal */
        uint32_t il = (uint32_t)(row - l->y0), ir = (uint32_t)(row - r->y0);
        uint16_t ul = (uint16_t)((uint32_t)l->pad[2] + il * (uint32_t)l->pad[3]);
        uint16_t ur = (uint16_t)((uint32_t)r->pad[2] + ir * (uint32_t)r->pad[3]);
        uint16_t vl = (uint16_t)(((uint32_t)l->pad[2] >> 16) + il * ((uint32_t)l->pad[3] >> 16));
        uint16_t vr = (uint16_t)(((uint32_t)r->pad[2] >> 16) + ir * ((uint32_t)r->pad[3] >> 16));
        s->u = (int64_t)(uint16_t)(ul + (uint16_t)p->ou) << 24;
        s->v = (int64_t)(uint16_t)(vl + (uint16_t)p->ov) << 24;
        s->du = (int64_t)row_step(ur, ul, (uint32_t)width) << 24;
        s->dv = (int64_t)row_step(vr, vl, (uint32_t)width) << 24;
        return 1;
    }
    uL = edge_uv(l->ua, l->dua, l->y0, l->n, row);
    uR = edge_uv(r->ua, r->dua, r->y0, r->n, row);
    vL = edge_uv(l->va, l->dva, l->y0, l->n, row);
    vR = edge_uv(r->va, r->dva, r->y0, r->n, row);
    d = uR - uL;                        /* the nearer turn: -128 to 127.99 */
    d = ((d + ((int64_t)128 << 32)) & (((int64_t)256 << 32) - 1)) - ((int64_t)128 << 32);
    s->du = d / width;
    d = vR - vL;
    d = ((d + ((int64_t)128 << 32)) & (((int64_t)256 << 32) - 1)) - ((int64_t)128 << 32);
    s->dv = d / width;
    s->u = uL + ((int64_t)p->ou << 24);
    s->v = vL + ((int64_t)p->ov << 24);
    if (p->persp) {
        float qL, qR, fuL = fwedge_persp(l, l->ua, l->dua, row, &qL), fuR = fwedge_persp(r, r->ua, r->dua, row, &qR);
        float fvL = fwedge_persp(l, l->va, l->dva, row, &qL), fvR = fwedge_persp(r, r->va, r->dva, row, &qR);
        float steps = width > 1 ? (float)(width - 1) : 1.0f, ou = (float)p->ou / 256.0f, ov = (float)p->ov / 256.0f;
        if (qL > 0 && qR > 0) {
            fuR = fw_fmaf(-256.0f, floorf((fuR - fuL + 128.0f) / 256.0f), fuR);   /* the nearer turn */
            fvR = fw_fmaf(-256.0f, floorf((fvR - fvL + 128.0f) / 256.0f), fvR);
            s->textured = 2;
            s->pq = qL;
            s->pdq = (qR - qL) / steps;
            s->pa = (fuL + ou) * qL;
            s->pda = fw_fmaf(fuR + ou, qR, -s->pa) / steps;
            s->pb = (fvL + ov) * qL;
            s->pdb = fw_fmaf(fvR + ov, qR, -s->pb) / steps;
        }
    }
    return 1;
}

static FwPoly *poly_new(void)
{
    if (npolys == cap_polys) {
        size_t c = cap_polys ? cap_polys * 2 : 4096;
        FwPoly *n = realloc(fw_polys, c * sizeof *n);
        if (!n)
            return NULL;
        fw_polys = n;
        cap_polys = c;
    }
    memset(&fw_polys[npolys], 0, sizeof *fw_polys);
    fw_polys[npolys].za = cur_z[0];
    fw_polys[npolys].zx = cur_z[1];
    fw_polys[npolys].zy = cur_z[2];
    return &fw_polys[npolys++];
}

static FwEdge *edge_new(void)
{
    if (nedges == cap_edges) {
        size_t c = cap_edges ? cap_edges * 2 : 16384;
        FwEdge *n = realloc(fw_edges, c * sizeof *n);
        if (!n)
            return NULL;
        fw_edges = n;
        cap_edges = c;
    }
    memset(&fw_edges[nedges], 0, sizeof *fw_edges);
    return &fw_edges[nedges++];
}

/* the polygon (its edges made) into the spans, its rows ya to yb - 1 kept
   to the screen; 0 when none are on it */
static int poly_put(FwPoly *p, int32_t ymin, int32_t yend)
{
    FwSpan *s;
    p->ya = ymin < 0 ? 0 : ymin;
    p->yb = yend > th ? th : yend;
    if (p->ya >= p->yb || !p->ln || !p->rn || !(s = span_new())) {
        npolys--;
        return 0;
    }
    memset(s, 0, sizeof *s);
    s->pad1[0] = (uint8_t)(span_ref_only | fw_ref_pass);
    s->y = p->ya;
    s->x0 = p->yb;
    s->x1 = (int32_t)(p - fw_polys);
    s->textured = 3;
    return 1;
}

/* The draw list's vertices the projection held, exact: a word's exact
   value by its address in the Super FX's RAM (sw_val), good for this list
   (sw_gen, the list's generation: a list drawn, every one gone) */

static int32_t vw(uint16_t a)
{
    if (gx)                             /* a job's: the chip takes the low byte (LDB) */
        return ram8(a);
    return sw_gen[a >> 1] == list_gen ? sw_val[a >> 1] : (int16_t)ram16(a);
}

/* a vertex word copied, its exact value with it (the fill's copies) */
static void wcopy(uint16_t dst, uint16_t src)
{
    if (gx) {                           /* the chip's copy: the low byte alone (STB) */
        ram8w(dst, ram8(src));
        return;
    }
    ram16w(dst, ram16(src));
    if (sw_gen[src >> 1] == list_gen) {
        sw_val[dst >> 1] = sw_val[src >> 1];
        sw_gen[dst >> 1] = list_gen;
    } else {
        sw_gen[dst >> 1] = 0;
    }
}

static void model_point(uint16_t index, int64_t *sx, int64_t *sy);

void fw_exact_vertex(uint16_t at, uint16_t index)
{
    int64_t x, y;
    int16_t x16, y16;

    if (!fw_view_active()) {
        sw_gen[at >> 1] = sw_gen[(uint16_t)(at + 2) >> 1] = 0;
        return;
    }
    model_point(index, &x, &y);
    x16 = (int16_t)ram16((uint16_t)(0x04E4 + 6 * index));
    y16 = (int16_t)ram16((uint16_t)(0x04E6 + 6 * index));
    if (llabs(x - x16) <= 16 && llabs(y - y16) <= 16) {     /* not held: the projection's own */
        sw_gen[at >> 1] = sw_gen[(uint16_t)(at + 2) >> 1] = 0;
        return;
    }
    sw_val[at >> 1] = (int32_t)x;
    sw_val[(uint16_t)(at + 2) >> 1] = (int32_t)y;
    sw_gen[at >> 1] = sw_gen[(uint16_t)(at + 2) >> 1] = list_gen;
}

/* A textured face's planes for its perspective-correct fill. For a face
   in view space on the plane n . P = d, the point a pixel shows is t r with
   r = ((sx - ox) / f, (sy - oy) / f, 1), t = d / (n . r), so 1 / z =
   (n / d) . r; and u, linear over the face, is g . P for the g its three
   points give (P rows, u's), so u / z = g . r: each a plane in r, which a
   row's pixels step along evenly. From the three of its points farthest
   apart; u and v as the edge walk takes them, each the nearest turn of the
   first point's. Good for the draw list it was made in (list_gen). */
typedef struct { double q[3], a[3], b[3]; uint32_t gen; } TexPlane;
/* FW_AFFINE=1 (a check): the affine fill, the original's, to compare */
static int fw_affine(void)
{
    static int on = -1;
    if (on < 0)
        on = fw_env("FW_AFFINE") != NULL;
    return on;
}
static TexPlane tex_planes[0x8000];

static void cross3(const double *a, const double *b, double *c)
{
    c[0] = a[1] * b[2] - a[2] * b[1];
    c[1] = a[2] * b[0] - a[0] * b[2];
    c[2] = a[0] * b[1] - a[1] * b[0];
}

static double dot3(const double *a, const double *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void fw_tex_plane(uint16_t list, uint16_t r3)
{
    TexPlane *t = &tex_planes[list >> 1];
    double P[EXACT_MAX][3], u[EXACT_MAX], v[EXACT_MAX], best = 0, e1[3], e2[3], c[3], d, det, bc[3], ca[3], ab[3];
    int n = ex_n, i, j, k, ia = 0, ib = 1, ic = 2;

    t->gen = 0;
    /* nothing read unless in the full window: the Super FX's accesses are
       timed in Original */
    if (!fw_view_active() || !(r3 & 0x8000) || n < 3 || list != ex_list)
        return;
    for (i = 0; i < n; i++) {
        P[i][0] = ex_vx[i];
        P[i][1] = ex_vy[i];
        P[i][2] = ex_vz[i];
        u[i] = ram16((uint16_t)(list + 8 * i + 4)) / 256.0;
        v[i] = ram16((uint16_t)(list + 8 * i + 6)) / 256.0;
    }
    for (i = 0; i < n; i++)             /* the three farthest apart: the largest triangle */
        for (j = i + 1; j < n; j++)
            for (k = j + 1; k < n; k++) {
                double a2;
                e1[0] = P[j][0] - P[i][0]; e1[1] = P[j][1] - P[i][1]; e1[2] = P[j][2] - P[i][2];
                e2[0] = P[k][0] - P[i][0]; e2[1] = P[k][1] - P[i][1]; e2[2] = P[k][2] - P[i][2];
                cross3(e1, e2, c);
                a2 = dot3(c, c);
                if (a2 > best) {
                    best = a2;
                    ia = i;
                    ib = j;
                    ic = k;
                }
            }
    if (best <= 0)
        return;
    e1[0] = P[ib][0] - P[ia][0]; e1[1] = P[ib][1] - P[ia][1]; e1[2] = P[ib][2] - P[ia][2];
    e2[0] = P[ic][0] - P[ia][0]; e2[1] = P[ic][1] - P[ia][1]; e2[2] = P[ic][2] - P[ia][2];
    cross3(e1, e2, c);
    d = dot3(c, P[ia]);
    cross3(P[ib], P[ic], bc);
    cross3(P[ic], P[ia], ca);
    cross3(P[ia], P[ib], ab);
    det = dot3(P[ia], bc);
    if (fabs(d) < 1e-9 * sqrt(best) || fabs(det) < 1e-9)
        return;                         /* seen edge on, or through the eye: the affine fill's */
    u[ib] = u[ia] + remainder(u[ib] - u[ia], 256.0);
    u[ic] = u[ia] + remainder(u[ic] - u[ia], 256.0);
    v[ib] = v[ia] + remainder(v[ib] - v[ia], 256.0);
    v[ic] = v[ia] + remainder(v[ic] - v[ia], 256.0);
    for (i = 0; i < 3; i++) {
        t->q[i] = c[i] / d;
        t->a[i] = (u[ia] * bc[i] + u[ib] * ca[i] + u[ic] * ab[i]) / det;
        t->b[i] = (v[ia] * bc[i] + v[ib] * ca[i] + v[ic] * ab[i]) / det;
    }
    t->gen = list_gen;
}

/* FxPolygonDraw's (polygon.c, 01:C339) in the full window: a textured face
   of the model drawer's whose points are not all on the screen, clipped
   exactly. The original clips it against the view's four planes and its
   near plane in 16-bit steps, which holds for the faces its 45-degree
   cull lets through; the full window keeps the objects beside the view
   too, whose faces reach behind it, where those steps go wrong. Here it
   is clipped at the near plane in view space (u and v carried along the
   edges in 3D, so true at the new points) and projected exactly, the
   screen's edges left to the fill (each row and column off the screen
   stepped at once, from the exact points in the side table). The list
   and count as FxPolygonProject leaves them. */
int fw_exact_tex_draw(void)
{
    static double cx[2][EXACT_MAX + 2], cy[2][EXACT_MAX + 2], cz[2][EXACT_MAX + 2], cu[2][EXACT_MAX + 2],
        cv[2][EXACT_MAX + 2];
    uint16_t list, n;
    int i, m = 0, left = 1, right = 1, above = 1, below = 1;
    double fx = fw_focal_x(), fy = fw_focal_y(), ox = view_cx(), oy = (double)centre_row();

    /* nothing read unless in the full window: the Super FX's accesses are
       timed in Original */
    if (!fw_view_active() || ex_n < 3)
        return 0;
    list = ram16(0x005A);
    n = ram16(0x0146);
    if (list != ex_list || n != ex_n)
        return 0;
    for (i = 0; i < (int)n; i++) {
        uint16_t at = (uint16_t)(list + 8 * i);
        cx[0][i] = ex_vx[i];
        cy[0][i] = ex_vy[i];
        cz[0][i] = ex_vz[i];
        cu[0][i] = ram16((uint16_t)(at + 4)) / 256.0;
        cv[0][i] = ram16((uint16_t)(at + 6)) / 256.0;
    }
    for (i = 0; i < (int)n; i++) {
        int j = (i + 1) % n, ain = cz[0][i] >= NEAR_Z, bin = cz[0][j] >= NEAR_Z;
        if (ain && m < EXACT_MAX + 2) {
            cx[1][m] = cx[0][i]; cy[1][m] = cy[0][i]; cz[1][m] = cz[0][i]; cu[1][m] = cu[0][i]; cv[1][m] = cv[0][i];
            m++;
        }
        if (ain != bin && m < EXACT_MAX + 2) {
            double t = (NEAR_Z - cz[0][i]) / (cz[0][j] - cz[0][i]);
            cx[1][m] = cx[0][i] + t * (cx[0][j] - cx[0][i]);
            cy[1][m] = cy[0][i] + t * (cy[0][j] - cy[0][i]);
            cz[1][m] = NEAR_Z;
            cu[1][m] = cu[0][i] + t * (cu[0][j] - cu[0][i]);
            cv[1][m] = cv[0][i] + t * (cv[0][j] - cv[0][i]);
            m++;
        }
    }
    if (m < 3 || m > EXACT_MAX)
        m = 0;
    for (i = 0; i < m; i++) {
        double sx = ox + cx[1][i] * fx / cz[1][i], sy = oy + cy[1][i] * fy / cz[1][i];
        int64_t X = (int64_t)floor(sx), Y = (int64_t)floor(sy);
        uint16_t at = (uint16_t)(list + 8 * i);
        if (X > 0x3FFFFFFF) X = 0x3FFFFFFF;
        if (X < -0x3FFFFFFF) X = -0x3FFFFFFF;
        if (Y > 0x3FFFFFFF) Y = 0x3FFFFFFF;
        if (Y < -0x3FFFFFFF) Y = -0x3FFFFFFF;
        left &= X < 0;
        right &= X >= tw;
        above &= Y < 0;
        below &= Y >= th;
        ram16w(at, (uint16_t)(X < -0x8000 ? -0x8000 : X > 0x7FFF ? 0x7FFF : X));
        ram16w((uint16_t)(at + 2), (uint16_t)(Y < -0x8000 ? -0x8000 : Y > 0x7FFF ? 0x7FFF : Y));
        ram16w((uint16_t)(at + 4), (uint16_t)(int)floor(cu[1][i] * 256.0 + 0.5));
        ram16w((uint16_t)(at + 6), (uint16_t)(int)floor(cv[1][i] * 256.0 + 0.5));
        sw_val[at >> 1] = (int32_t)X;
        sw_val[(uint16_t)(at + 2) >> 1] = (int32_t)Y;
        sw_gen[at >> 1] = sw_gen[(uint16_t)(at + 2) >> 1] = list_gen;
    }
    if (m && (left || right || above || below))     /* wholly off one side */
        m = 0;
    gsu.r[0] = (uint16_t)m;
    ram16w(0x0040, (uint16_t)m);
    ram16w(0x0146, (uint16_t)m);
    ram16w(0x014A, 0);
    ex_n = -1;
    return 1;
}

/* A flat convex face of the full window's own (solids.c): its points in view
   space clipped at the near plane, projected as fw_exact_tex_draw's are,
   kept to a band 4096 texels past the screen (so the edges' sums stay in
   range) and filled whole in colour c, as flat_polygon's faces are: its
   top vertex, the chain forward from it the left, back from it the right */
#define FACE_MAX 72
#define FACE_BAND 4096.0
static int face_clip(float (*in)[2], int n, float (*out)[2], int axis, float lim, int keep_below)
{
    int i, m = 0;
    for (i = 0; i < n && m < FACE_MAX; i++) {
        const float *a = in[i], *b = in[(i + 1) % n];
        int ain = keep_below ? a[axis] <= lim : a[axis] >= lim, bin = keep_below ? b[axis] <= lim : b[axis] >= lim;
        if (ain)
            out[m][0] = a[0], out[m][1] = a[1], m++;
        if (ain != bin && m < FACE_MAX) {
            float t = (lim - a[axis]) / (b[axis] - a[axis]);
            out[m][0] = a[0] + t * (b[0] - a[0]);
            out[m][1] = a[1] + t * (b[1] - a[1]);
            out[m][axis] = lim;
            m++;
        }
    }
    return m;
}

void fw_view_face(const float (*p)[3], int n, uint8_t c)
{
    static float v[FACE_MAX][3], s[2][FACE_MAX][2];
    int64_t X[FACE_MAX], Y[FACE_MAX];
    float fx = fw_focal_xf(), fy = fw_focal_yf(), ox = (float)view_cx(), oy = centre_rowf();
    int64_t area = 0;
    int i, m = 0, k, top = 0, at, next;
    int32_t lend, rend;
    FwPoly *pg;

    if (!fw_view_active() || checking || n < 3 || n > FACE_MAX - 8)
        return;
    {                                   /* its own depth while it is put in (a sprite's back after) */
        float z[3];
        depth_plane(p, n, z);
        cur_z[0] = z[0], cur_z[1] = z[1], cur_z[2] = z[2];
    }
    for (i = 0; i < n; i++) {           /* the near plane */
        const float *a = p[i], *b = p[(i + 1) % n];
        int ain = a[2] >= NEAR_Z, bin = b[2] >= NEAR_Z;
        if (ain)
            v[m][0] = a[0], v[m][1] = a[1], v[m][2] = a[2], m++;
        if (ain != bin) {
            float t = (NEAR_Z - a[2]) / (b[2] - a[2]);
            v[m][0] = a[0] + t * (b[0] - a[0]);
            v[m][1] = a[1] + t * (b[1] - a[1]);
            v[m][2] = NEAR_Z;
            m++;
        }
    }
    for (i = 0; i < m; i++) {
        s[0][i][0] = ox + v[i][0] * fx / v[i][2];
        s[0][i][1] = oy + v[i][1] * fy / v[i][2];
    }
    /* (the band's edges summed in float: exact, and no software double) */
    m = face_clip(s[0], m, s[1], 0, -(float)FACE_BAND, 0);
    m = face_clip(s[1], m, s[0], 0, (float)tw + (float)FACE_BAND, 1);
    m = face_clip(s[0], m, s[1], 1, -(float)FACE_BAND, 0);
    m = face_clip(s[1], m, s[0], 1, (float)th + (float)FACE_BAND, 1);
    for (i = k = 0; i < m; i++) {       /* to texels, a point the same as the one before dropped */
        /* (within the band: 32-bit conversions, not a library's 64-bit ones) */
        int64_t x = (int32_t)floorf(s[0][i][0]), y = (int32_t)floorf(s[0][i][1]);
        if (k && x == X[k - 1] && y == Y[k - 1])
            continue;
        X[k] = x;
        Y[k] = y;
        k++;
    }
    while (k > 1 && X[k - 1] == X[0] && Y[k - 1] == Y[0])
        k--;
    if (k < 3)
        return;
    for (i = 0; i < k; i++)
        area += X[i] * Y[(i + 1) % k] - X[(i + 1) % k] * Y[i];
    if (area == 0)
        return;
    if (area > 0)                       /* the forward chain the left: the points turned the other way */
        for (i = 0; i < k / 2; i++) {
            int64_t t = X[i];
            X[i] = X[k - 1 - i];
            X[k - 1 - i] = t;
            t = Y[i];
            Y[i] = Y[k - 1 - i];
            Y[k - 1 - i] = t;
        }
    for (i = 1; i < k; i++)
        if (Y[i] < Y[top])
            top = i;
    if (!(pg = poly_new()))
        return;
    pg->colour = fw_face_colour(c);
    pg->le = (int32_t)nedges;
    lend = (int32_t)Y[top];
    for (at = top, i = 0; i < k; i++, at = next) {      /* the left chain: forward from the top */
        FwEdge *e;
        next = (at + 1) % k;
        if (Y[next] < Y[at])
            break;
        if (Y[next] > Y[at] && (e = edge_new())) {
            e->from = X[at];
            e->to = X[next];
            e->y0 = (int32_t)Y[at];
            e->n = (int32_t)(Y[next] - Y[at]);
            pg->ln++;
            lend = (int32_t)Y[next];
        }
    }
    pg->re = (int32_t)nedges;
    rend = (int32_t)Y[top];
    for (at = top, i = 0; i < k; i++, at = next) {      /* the right chain: back from the top */
        FwEdge *e;
        next = (at + k - 1) % k;
        if (Y[next] < Y[at])
            break;
        if (Y[next] > Y[at] && (e = edge_new())) {
            e->from = X[at];
            e->to = X[next];
            e->y0 = (int32_t)Y[at];
            e->n = (int32_t)(Y[next] - Y[at]);
            pg->rn++;
            rend = (int32_t)Y[next];
        }
    }
    poly_put(pg, (int32_t)Y[top], lend < rend ? lend : rend);
}

/* FxPolygonGather's (raster.c, 01:98D7) in the full window: a face of the
   model drawer's with a point behind the view, near-clipped and projected.
   The original adds each point to the object's position in 16 bits and
   clips at depth 0, which holds for the objects its 45-degree cull keeps;
   the full window keeps the objects beside the view too, a long one's far
   points past 32767 deep wrapping to behind (a wall beside the car taken
   for a line, dropped a picture at a time). Here the points are summed in
   32 bits, clipped at the near plane in view space and projected exactly;
   the original runs as ever, its list, count and R0 replaced after. */
static int32_t ga_x[EXACT_MAX], ga_y[EXACT_MAX], ga_z[EXACT_MAX];
static int ga_n = -1;

int fw_exact_gather_begin(uint16_t count, uint16_t r14)
{
    int i;

    ga_n = -1;
    if (!fw_view_active() || count < 2 || count > EXACT_MAX)
        return 0;
    for (i = 0; i < count; i++) {
        uint16_t a;
        gsu_r14((uint16_t)(r14 + i));
        a = (uint16_t)(0x0340 + 6 * gsu_getb());
        ga_x[i] = (int16_t)ram16(a) + (int16_t)ram16(0x0026);
        ga_y[i] = (int16_t)ram16((uint16_t)(a + 2)) + (int16_t)ram16(0x0028);
        ga_z[i] = (int16_t)ram16((uint16_t)(a + 4)) + (int16_t)ram16(0x002A);
    }
    gsu_r14(r14);                       /* the original reads them again */
    ga_n = count;
    return 1;
}

void fw_exact_gather_end(void)
{
    double cx[EXACT_MAX + 2], cy[EXACT_MAX + 2], cz[EXACT_MAX + 2];
    double fx = fw_focal_x(), fy = fw_focal_y(), ox = view_cx(), oy = (double)centre_row();
    uint16_t list = ram16(0x005A);
    int i, m = 0, n = ga_n, lines = n == 2, in[4] = {0, 0, 0, 0};

    if (n < 0)
        return;
    ga_n = -1;
    if (n >= 3) {                       /* its depth, from its points in view space */
        float P[EXACT_MAX][3];
        for (i = 0; i < n; i++)
            P[i][0] = (float)ga_x[i], P[i][1] = (float)ga_y[i], P[i][2] = (float)ga_z[i];
        depth_plane((const float (*)[3])P, n, face_depth[list >> 1].z);
        face_depth[list >> 1].gen = list_gen;
    }
    for (i = 0; i < (lines ? 1 : n); i++) {
        int j = (i + 1) % n, ain = ga_z[i] >= NEAR_Z, bin = ga_z[j] >= NEAR_Z;
        double t = ain != bin ? (double)(NEAR_Z - ga_z[i]) / (ga_z[j] - ga_z[i]) : 0;
        if (lines) {                    /* a line: both ends kept, one behind moved to the crossing */
            if (!ain && !bin)
                break;
            cx[0] = ain ? ga_x[i] : ga_x[i] + t * (ga_x[j] - ga_x[i]);
            cy[0] = ain ? ga_y[i] : ga_y[i] + t * (ga_y[j] - ga_y[i]);
            cz[0] = ain ? ga_z[i] : NEAR_Z;
            cx[1] = bin ? ga_x[j] : ga_x[i] + t * (ga_x[j] - ga_x[i]);
            cy[1] = bin ? ga_y[j] : ga_y[i] + t * (ga_y[j] - ga_y[i]);
            cz[1] = bin ? ga_z[j] : NEAR_Z;
            m = 2;
            break;
        }
        if (ain && m < EXACT_MAX)
            cx[m] = ga_x[i], cy[m] = ga_y[i], cz[m++] = ga_z[i];
        if (ain != bin && m < EXACT_MAX) {
            cx[m] = ga_x[i] + t * (ga_x[j] - ga_x[i]);
            cy[m] = ga_y[i] + t * (ga_y[j] - ga_y[i]);
            cz[m++] = NEAR_Z;
        }
    }
    if (!lines && m < 3)
        m = 0;
    for (i = 0; i < m; i++) {
        double sx = ox + cx[i] * fx / cz[i], sy = oy + cy[i] * fy / cz[i];
        int64_t X = (int64_t)floor(sx), Y = (int64_t)floor(sy);
        uint16_t at = (uint16_t)(list + 4 * i);
        if (X > 0x3FFFFFFF) X = 0x3FFFFFFF;
        if (X < -0x3FFFFFFF) X = -0x3FFFFFFF;
        if (Y > 0x3FFFFFFF) Y = 0x3FFFFFFF;
        if (Y < -0x3FFFFFFF) Y = -0x3FFFFFFF;
        in[0] |= X >= 0;
        in[1] |= X <= tw - 1;
        in[2] |= Y >= 0;
        in[3] |= Y <= th;
        ram16w(at, (uint16_t)(X < -0x8000 ? -0x8000 : X > 0x7FFF ? 0x7FFF : X));
        ram16w((uint16_t)(at + 2), (uint16_t)(Y < -0x8000 ? -0x8000 : Y > 0x7FFF ? 0x7FFF : Y));
        ex_x[i] = X;
        ex_y[i] = Y;
    }
    if (!(in[0] && in[1] && in[2] && in[3]))   /* wholly past one edge */
        m = 0;
    ex_list = list;
    ex_n = m ? m : -1;
    ram16w(0x0040, (uint16_t)m);
    gsu.r[0] = (uint16_t)m;
}

/* FW_CHECK's second pass (the original's view in place): a pixel PLOT puts
   on a row from 128 to 255 lands, in the Super FX's 128-line layout, on
   row y - 128, 8 pixels to the right; the texels have no such alias, so
   the check makes it */
static void check_alias(int32_t x, int32_t y, int c)
{
    if (y >= 128 && y < 256 && x >= 0 && x + 8 < 256)
        tex[(size_t)(y - 128) * tw + x + 8] = (uint8_t)c;
}

static void flat_row(int32_t l, int32_t r, int32_t y)
{
    int32_t x;
    int c = plot_colour();
    uint8_t *o;
    if (gx && y >= th && y < 256) {     /* a job's row past the bitmap, on its alias (a thin polygon on the bottom edge) */
        y -= th;
        l += 8;
        r += 8;
    }
    if (c < 0 || y < 0 || y >= th)
        return;
    if (gx) {
        gx_flat(y, l, r, (uint8_t)c);
        return;
    }
    if (checking)
        for (x = l < 0 ? 0 : l; x <= r; x++)
            check_alias(x, y, c);
    if (checking && fw_env("FW_CHECKBREAK") && y == 64)  /* a planted break: one row left out */
        return;
    if (l < 0)
        l = 0;
    if (r >= tw)
        r = tw - 1;
    if (!checking) {
        span_flat(y, l, r, (uint8_t)c);
        return;
    }
    o = tex + (size_t)y * tw;
    for (x = l; x <= r; x++)
        o[x] = (uint8_t)c;
}

/* a job's flat edge by the chip's rules (FwEdge's pad, from its vertices'
   columns and rows): its slope from the reciprocal; where the next edge starts,
   as A9D0 and AA0D round it */
static int64_t edge_chip(FwEdge *f)
{
    int64_t x8 = f->from * 256, s = slope(f->to - f->from, (uint32_t)f->n);
    f->pad[0] = (int32_t)x8;
    f->pad[1] = (int32_t)s;
    return (fw_chip_x(x8, s, f->n) + 0x7F) >> 8;
}

/* what a job's polygon adds to its duration: its area by its vertices (a
   stride apart from v), its edges */
static void job_poly(uint16_t v, int stride, int n, const FwPoly *pg)
{
    int64_t a = 0;
    int i;
    for (i = 0; i < n; i++) {
        uint16_t p = (uint16_t)(v + i * stride), q = (uint16_t)(v + ((i + 1) % n) * stride);
        a += (int64_t)ram8(p) * ram8((uint16_t)(q + 2)) - (int64_t)ram8(q) * ram8((uint16_t)(p + 2));
    }
    a = (a < 0 ? -a : a) / 2;
    job.edges += (uint32_t)(pg->ln + pg->rn);
    if (pg->kind) {
        job.tex++;
        job.area_tex += (uint64_t)a;
    } else {
        job.flat++;
        job.area_flat += (uint64_t)a;
    }
}

/* A95C: a flat polygon of n vertices, its colour byte at p */
static uint16_t flat_polygon(uint16_t p, int n)
{
    uint16_t v = (uint16_t)(p + 1), size = (uint16_t)(4 * n), dst = (uint16_t)(v - size), top_at = 0, src = v;
    uint16_t left_at, right_at;
    int32_t ymin = 0x7FFFFFFF, ymax = -0x7FFFFFFF, lrows, rrows, row;
    int64_t lx, rx, ls = 0, rs = 0, lfrom = 0, lto = 0, rfrom = 0, rto = 0;     /* each edge's (edge_x) */
    int32_t ly0 = 0, ln = 1, ry0 = 0, rn = 1;
    int i;
    gsu.por = 0;
    gsu_color(ram8(p));
    for (i = 0; i < n; i++) {                  /* the vertices copied below */
        int32_t y;
        wcopy(dst, src);
        dst = (uint16_t)(dst + 2);
        src = (uint16_t)(src + 2);
        wcopy(dst, src);
        y = vw(src);
        if (y < ymin) {
            ymin = y;
            top_at = dst;
        }
        if (y >= ymax)
            ymax = y;
        dst = (uint16_t)(dst + 2);
        src = (uint16_t)(src + 2);
    }
    if (ymax - ymin - 2 < 0) {                 /* under two rows: one row */
        int32_t xmin = 0x7FFFFFFF, xmax = -0x7FFFFFFF;
        dst = v;
        for (i = 0; i < n; i++) {
            if (vw(dst) < xmin)
                xmin = vw(dst);
            if (vw(dst) >= xmax)
                xmax = vw(dst);
            dst = (uint16_t)(dst + 4);
        }
        if (xmax - xmin > 0) {
            if (gx)
                job.plots += (uint64_t)(xmax - xmin);
            flat_row(xmin, xmax - 1, ymin);
        }
        return (uint16_t)(v + size);
    }
    row = ymin;
    left_at = (uint16_t)(top_at - 2);
    right_at = (uint16_t)(left_at + size - 2);
    if (!checking) {                    /* whole: its edges, its rows made where it is filled */
        int c = plot_colour();
        int32_t lend = ymin, rend = ymin, y;
        int64_t from, to;
        FwPoly *pg;
        if (c < 0 || !(pg = poly_new()))
            return (uint16_t)(v + size);
        pg->colour = (uint8_t)c;
        pg->pad[0] = (uint8_t)gx;
        pg->le = (int32_t)nedges;
        from = vw(left_at);             /* the left chain: forward from the top */
        left_at = (uint16_t)(left_at + 4);
        for (;;) {
            FwEdge *f;
            int64_t next;
            to = vw(left_at);
            y = vw((uint16_t)(left_at + 2));
            left_at = (uint16_t)(left_at + 4);
            if (y - lend < 0)
                break;
            next = to;
            if (y - lend && (f = edge_new())) {
                f->from = from;
                f->to = to;
                f->y0 = lend;
                f->n = y - lend;
                pg->ln++;
                if (gx)
                    next = edge_chip(f);
                lend = y;
            }
            from = next;
            if (lend >= th)
                break;
        }
        pg->re = (int32_t)nedges;
        from = vw((uint16_t)(top_at - 2));      /* the right chain: back from the top */
        for (;;) {
            FwEdge *f;
            int64_t next;
            y = vw(right_at);
            right_at = (uint16_t)(right_at - 2);
            to = vw(right_at);
            right_at = (uint16_t)(right_at - 2);
            if (y - rend < 0)
                break;
            next = to;
            if (y - rend && (f = edge_new())) {
                f->from = from;
                f->to = to;
                f->y0 = rend;
                f->n = y - rend;
                pg->rn++;
                if (gx)
                    next = edge_chip(f);
                rend = y;
            }
            from = next;
            if (rend >= th)
                break;
        }
        if (gx)
            job_poly(v, 4, n, pg);
        poly_put(pg, ymin, lend < rend ? lend : rend);
        return (uint16_t)(v + size);
    }
    lx = vw(left_at);
    rx = lx * 256;
    rto = lx;
    left_at = (uint16_t)(left_at + 4);
    rrows = 0;
    {
        int64_t from = lx, to;
        int32_t y;
        for (;;) {
            to = vw(left_at);
            y = vw((uint16_t)(left_at + 2));
            left_at = (uint16_t)(left_at + 4);
            lrows = y - row;
            if (lrows < 0)
                return (uint16_t)(v + size);
            if (lrows)
                break;
            from = to;
        }
        ls = slope(to - from, (uint32_t)lrows);
        lx = from * 256;
        lfrom = from;
        lto = to;
        ly0 = row;
        ln = lrows;
    }
    for (;;) {
        if (!rrows) {
            int64_t from = checking ? (rx + 0x7F) >> 8 : rto, to;
            int32_t y;
            for (;;) {
                y = vw(right_at);
                right_at = (uint16_t)(right_at - 2);
                to = vw(right_at);
                right_at = (uint16_t)(right_at - 2);
                rrows = y - row;
                if (rrows < 0)
                    return (uint16_t)(v + size);
                if (rrows)
                    break;
                from = to;
            }
            rs = slope(to - from, (uint32_t)rrows);
            rx = from * 256;
            rfrom = from;
            rto = to;
            ry0 = row;
            rn = rrows;
        }
        do {
            int64_t l, r;
            if (!checking) {
                lx = edge_x(lfrom, lto, ly0, ln, row);
                rx = edge_x(rfrom, rto, ry0, rn, row);
            }
            l = lx >> 8;
            r = rx >> 8;
            if (row >= th)                  /* past the screen's last row: nothing more to draw */
                return (uint16_t)(v + size);
            if (!checking && row < -th && lrows > 1 && rrows > 1) {    /* rows far above the screen: stepped at once */
                int32_t k = -th - row;
                if (k > lrows - 1) k = lrows - 1;
                if (k > rrows - 1) k = rrows - 1;
                lx += ls * k;
                rx += rs * k;
                lrows -= k;
                rrows -= k;
                row += k;
                continue;
            }
            if (r >= l)
                flat_row(l < -0x7FFFFFFF ? -0x7FFFFFFF : (int32_t)l, r > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)r, row);
            lx = edge_step(lx, ls);
            lrows--;
            rx = edge_step(rx, rs);
            rrows--;
            row++;
        } while (lrows && rrows);
        if (lrows)
            continue;
        {
            int64_t from = checking ? (lx + 0x7F) >> 8 : lto, to;
            int32_t y;
            for (;;) {
                to = vw(left_at);
                y = vw((uint16_t)(left_at + 2));
                left_at = (uint16_t)(left_at + 4);
                lrows = y - row;
                if (lrows < 0)
                    return (uint16_t)(v + size);
                if (lrows)
                    break;
                from = to;
            }
            ls = slope(to - from, (uint32_t)lrows);
            lx = from * 256;
            lfrom = from;
            lto = to;
            ly0 = row;
            ln = lrows;
        }
    }
}

/* AAA2: a line, stepped along its longer axis */
static uint16_t line(uint16_t p)
{
    int32_t x = vw((uint16_t)(p + 1)), y = vw((uint16_t)(p + 3));
    int32_t dx = vw((uint16_t)(p + 5)) - x, dy = vw((uint16_t)(p + 7)) - y;
    int32_t sx = 1, sy = 1, e, n;
    int carry;
    gsu.por = 0;
    gsu_color(ram8(p));
    if (dx < 0) { sx = -1; dx = -dx; }
    if (dy < 0) { sy = -1; dy = -dy; }
    if (gx) {                           /* a job's: one quad for the GPU (its steps fw_plot_line's), else plotted */
        uint16_t xe, ye;
        int made = fw_plot_line((uint16_t)x, (uint16_t)y, (uint16_t)dx, (uint16_t)dy, (uint16_t)sx, (uint16_t)sy, &xe, &ye);
        fw_sprite_rows_done();
        if (made)
            return (uint16_t)(p + 9);
    }
    if (dx >= dy) {
        n = dx + 1;
        e = (dx - dy) >> 1;
        carry = e >= dy;
        e -= dy;
        do {
            flat_row(x, x, y);
            if (!carry) { e += dx; y += sy; }
            x += sx;
            n--;
            carry = e >= dy;
            e -= dy;
        } while (n);
    } else {
        n = dy + 1;
        e = (dy - dx) >> 1;
        carry = e >= dx;
        e -= dx;
        do {
            flat_row(x, x, y);
            if (!carry) { e += dy; x += sx; }
            y += sy;
            n--;
            carry = e >= dx;
            e -= dx;
        } while (n);
    }
    return (uint16_t)(p + 9);
}

/* a u or v difference over a span of n pixels: the original's step (the
   difference's low byte as a signed word's high byte, times the
   reciprocal, FMULT and ROL); past 255 the division it stands for */
static uint16_t uv_step(uint16_t d, uint32_t n)
{
    int32_t s = (int16_t)(uint16_t)((d & 0xFF) << 8);
    uint32_t p;
    if (n >= 256)
        return (uint16_t)(s / (int32_t)n);
    p = (uint32_t)(s * (int16_t)reciprocal(n));
    return (uint16_t)((p >> 16) << 1 | (p >> 15 & 1));
}

/* the step across a row: (right - left) over the row's width */
static uint16_t row_step(uint16_t r, uint16_t l, uint32_t n)
{
    int32_t s = (int16_t)(uint16_t)(r - l);
    uint32_t p;
    if (n >= 256)
        return (uint16_t)(s / (int32_t)n);
    p = (uint32_t)(s * (int16_t)reciprocal(n));
    return (uint16_t)((p >> 16) << 1 | (p >> 15 & 1));
}

typedef struct {
    int64_t x;                  /* 24.8 */
    int64_t slope;
    int32_t rows;
    uint16_t at;
    uint16_t u, v, du, dv;      /* 8.8 */
    /* the same at full precision (the full window's own pictures): the
       texels at the row, their steps a row, the end vertex's */
    double hu, hv, hdu, hdv;
    uint8_t vu, vv;
    int64_t from, to;           /* its vertices' x, its first row and its rows (edge_x) */
    int32_t y0, n;
    /* perspective-correct (tex_persp): 1 / z at its two vertices, from the
       face's plane, and its u and v at its start and their changes to its
       end, as the original's walk takes them */
    double qa, qb, ua, va, dua, dva;
} TexEdge;

/* The face being filled has its planes (fw_tex_plane): its texels
   perspective-correct. Only its 1 / z is taken from them (the plane its
   points lie on); u and v are the original's at its vertices, carried
   along its edges and across each row as the original's walk does, with
   u / z, v / z and 1 / z stepping evenly instead of u and v. A plane of u
   and v fitted to three of a face's points would not do: a quad's four
   corners need not be one affine map of the texture (the 4WD's plate),
   so the texture would slide over it as the three chosen change. */
static const TexPlane *tex_persp;

static double plane_q(double sx, double sy)
{
    double rx = (sx - view_cx()) / fw_focal_x(), ry = (sy - (double)centre_row()) / fw_focal_y();
    return tex_persp->q[0] * rx + tex_persp->q[1] * ry + tex_persp->q[2];
}

/* an edge's u (or v: which 0 or 1), 1 / z at the row */
static void edge_persp(const TexEdge *e, int32_t row, double *u, double *v, double *q)
{
    double t = e->n ? (double)(row - e->y0) / e->n : 0, qa = e->qa, qb = e->qb;
    *q = qa + t * (qb - qa);
    *u = (qa * e->ua + t * (qb * (e->ua + e->dua) - qa * e->ua)) / *q;
    *v = (qa * e->va + t * (qb * (e->va + e->dva) - qa * e->va)) / *q;
}

/* the textured fill at full precision: the original's edges and spans, its
   u and v without its 8.8 steps' rounding, which over the thousands of rows
   a polygon near the camera spans at k texels a pixel leaves the texture
   standing still; the check's pass (the original's view) keeps the 8.8 */
static int tex_precise(void) { return !checking && !gx; }

/* A7D9 / A879: the next edge, forward (left) or back (right) */
static int tex_edge(TexEdge *e, int32_t row, int left)
{
    int64_t from, to;
    int32_t y;
    uint16_t u, v, u_prev, v_prev;
    to = tex_precise() ? e->to : (e->x + 0x7F) >> 8;
    u = (uint16_t)(e->u >> 8);
    v = (uint16_t)(e->v >> 8);
    if (tex_precise()) {                /* from the vertex the last edge ended at */
        u = e->vu;
        v = e->vv;
    }
    for (;;) {
        from = to;
        u_prev = u;
        v_prev = v;
        if (left) {
            to = vw(e->at);
            y = vw((uint16_t)(e->at + 2));
            u = ram8((uint16_t)(e->at + 4));
            v = ram8((uint16_t)(e->at + 6));
            e->at = (uint16_t)(e->at + 8);
        } else {
            v = ram8((uint16_t)(e->at - 2));
            u = ram8((uint16_t)(e->at - 4));
            y = vw((uint16_t)(e->at - 6));
            e->at = (uint16_t)(e->at - 8);
            to = vw(e->at);
        }
        e->rows = y - row;
        if (e->rows)
            break;
    }
    if (e->rows < 0)
        return 0;
    e->x = from * 256;
    e->from = from;
    e->to = to;
    e->y0 = row;
    e->n = e->rows;
    e->slope = slope(to - from, (uint32_t)e->rows);
    e->du = uv_step((uint16_t)(u - u_prev), (uint32_t)e->rows);
    e->u = (uint16_t)(u_prev << 8 | u_prev >> 8);
    e->dv = uv_step((uint16_t)(v - v_prev), (uint32_t)e->rows);
    e->v = (uint16_t)(v_prev << 8 | v_prev >> 8);
    e->hu = u_prev;
    e->hv = v_prev;
    e->hdu = (double)(int8_t)(uint8_t)(u - u_prev) / e->rows;
    e->hdv = (double)(int8_t)(uint8_t)(v - v_prev) / e->rows;
    e->vu = (uint8_t)u;
    e->vv = (uint8_t)v;
    if (tex_persp) {
        e->ua = u_prev;
        e->va = v_prev;
        e->dua = (int8_t)(uint8_t)(u - u_prev);
        e->dva = (int8_t)(uint8_t)(v - v_prev);
        e->qa = plane_q((double)from, (double)row);
        e->qb = plane_q((double)to, (double)(row + e->rows));
    }
    return 1;
}

/* A662: a textured polygon of n vertices, its bank byte at p */
static uint16_t tex_polygon(uint16_t p, int n)
{
    uint8_t bank = ram8(p);
    uint16_t word = ram16((uint16_t)(p + 1)), base = ram16((uint16_t)(p + 3)), mask = ram16((uint16_t)(p + 5));
    uint16_t v = (uint16_t)(p + 7), size = (uint16_t)(8 * n), src = v, dst = (uint16_t)(v - size), top_at = 0;
    int32_t ymin = 0x7FFFFFFF, ymax = -0x7FFFFFFF, row;
    TexEdge l, r;
    int i;
    /* the face's planes (fw_tex_plane): a perspective-correct fill */
    const TexPlane *pl = tex_planes[v >> 1].gen == list_gen && !fw_affine() && tex_precise() ? &tex_planes[v >> 1] : NULL;
    tex_persp = pl;
    gsu.por = 0;
    gsu_color((uint8_t)word);
    gsu.por = (uint8_t)((word >> 8) & 0x1F);
    if (gx) {                                  /* the RAM's words the chip's fill sets */
        ram16w(0x00A6, bank);
        ram16w(0x00A8, base);
        ram16w(0x00D4, mask);
    }
    for (i = 0; i < n; i++) {                  /* u and v moved down a byte */
        int32_t y = vw((uint16_t)(src + 2));
        wcopy(dst, src);
        wcopy((uint16_t)(dst + 2), (uint16_t)(src + 2));
        if (y < ymin) {
            ymin = y;
            top_at = (uint16_t)(dst + 2);
        }
        if (y >= ymax)
            ymax = y;
        ram8w((uint16_t)(src + 4), ram8((uint16_t)(src + 5)));
        ram8w((uint16_t)(dst + 4), ram8((uint16_t)(src + 5)));
        ram8w((uint16_t)(src + 6), ram8((uint16_t)(src + 7)));
        ram8w((uint16_t)(dst + 6), ram8((uint16_t)(src + 7)));
        src = (uint16_t)(src + 8);
        dst = (uint16_t)(dst + 8);
    }
    if (ymin == ymax) {
        tex_persp = NULL;
        return (uint16_t)(v + size);
    }
    {
        uint16_t t = (uint16_t)(top_at - 2);
        l.x = r.x = vw(t) * 256;
        l.to = r.to = vw(t);
        l.u = r.u = (uint16_t)(ram8((uint16_t)(t + 4)) << 8);
        l.v = r.v = (uint16_t)(ram8((uint16_t)(t + 6)) << 8);
        l.vu = r.vu = ram8((uint16_t)(t + 4));
        l.vv = r.vv = ram8((uint16_t)(t + 6));
        l.at = (uint16_t)(t + 8);
        r.at = (uint16_t)(t + size);
    }
    l.rows = r.rows = 0;
    row = ymin;
    if (tex_precise() || gx) {          /* whole: its edges, its rows made where it is filled */
        FwPoly *pg = poly_new();
        int32_t lend = ymin, rend = ymin;
        if (!pg) {
            tex_persp = NULL;
            return (uint16_t)(v + size);
        }
        pg->kind = 1;
        pg->persp = pl != NULL;
        pg->pad[0] = (uint8_t)gx;
        pg->bank = bank;
        pg->base = base;
        pg->mask = mask;
        pg->por = fw_por();
        pg->hi = fw_hi();
        pg->ou = ram16(TEX_U_OFF);
        pg->ov = ram16(TEX_V_OFF);
        for (i = 0; i < 2; i++) {
            TexEdge *e = i ? &r : &l;
            int32_t *end = i ? &rend : &lend;
            if (i)
                pg->re = (int32_t)nedges;
            else
                pg->le = (int32_t)nedges;
            while (tex_edge(e, *end, !i)) {
                FwEdge *f = edge_new();
                if (!f)
                    break;
                f->from = e->from;
                f->to = e->to;
                f->y0 = e->y0;
                f->n = e->n;
                f->ua = (int32_t)e->hu;
                f->va = (int32_t)e->hv;
                f->dua = (int8_t)(uint8_t)(e->vu - (uint8_t)e->hu);
                f->dva = (int8_t)(uint8_t)(e->vv - (uint8_t)e->hv);
                f->qa = (float)e->qa;
                f->qb = (float)e->qb;
                if (gx) {               /* the chip's: its 8.8 start and steps; on to where the next edge's start comes from */
                    f->pad[0] = (int32_t)e->x;
                    f->pad[1] = (int32_t)e->slope;
                    f->pad[2] = (int32_t)((uint32_t)e->u | (uint32_t)e->v << 16);
                    f->pad[3] = (int32_t)((uint32_t)e->du | (uint32_t)e->dv << 16);
                    e->x = fw_chip_x(e->x, e->slope, e->n);
                    e->u = (uint16_t)(e->u + e->du * (uint32_t)e->n);
                    e->v = (uint16_t)(e->v + e->dv * (uint32_t)e->n);
                }
                *end += e->n;
                if (i)
                    pg->rn++;
                else
                    pg->ln++;
                if (*end >= th)
                    break;
            }
        }
        if (gx)
            job_poly(v, 8, n, pg);
        if (poly_put(pg, ymin, lend < rend ? lend : rend)) {
            /* COLOR as the last row on the screen leaves it: its last pixel's texel */
            FwSpan last;
            for (row = pg->yb - 1; row >= pg->ya; row--)
                if (fw_poly_row(pg, fw_edges, row, tw, &last)) {
                    int64_t k = last.x1 - last.xl;
                    unsigned iu, iv;
                    if (last.textured == 2) {
                        float kf = (float)k, qk = fw_fmaf(last.pdq, kf, last.pq);
                        iu = persp_texel(fw_fmaf(last.pda, kf, last.pa), qk);
                        iv = persp_texel(fw_fmaf(last.pdb, kf, last.pb), qk);
                    } else {
                        iu = (unsigned)((last.u + last.du * k) >> 32) & 0xFF;
                        iv = (unsigned)((last.v + last.dv * k) >> 32) & 0xFF;
                    }
                    gsu_color(fw_texel(bank, (uint16_t)(((iv << 8 | iu) & mask) + base)));
                    break;
                }
        }
        tex_persp = NULL;
        return (uint16_t)(v + size);
    }
    for (;;) {
        int32_t count, left_rows, right_rows;
        if (!l.rows && !tex_edge(&l, row, 1))
            break;
        if (!r.rows && !tex_edge(&r, row, 0))
            break;
        if (l.rows >= r.rows) {
            left_rows = l.rows - r.rows;
            right_rows = 0;
            count = r.rows;
        } else {
            right_rows = r.rows - l.rows;
            left_rows = 0;
            count = l.rows;
        }
        if (row >= th)                      /* past the screen's last row */
            break;
        if (!checking && row < -th && count > 1) {  /* rows far above the screen: stepped at once */
            int32_t k = -th - row;
            if (k > count - 1)
                k = count - 1;
            l.x += l.slope * k;
            r.x += r.slope * k;
            l.u = (uint16_t)(l.u + l.du * k);
            r.u = (uint16_t)(r.u + r.du * k);
            l.v = (uint16_t)(l.v + l.dv * k);
            r.v = (uint16_t)(r.v + r.dv * k);
            l.hu += l.hdu * k;
            r.hu += r.hdu * k;
            l.hv += l.hdv * k;
            r.hv += r.hdv * k;
            row += k;
            count -= k;
        }
        while (count--) {
            int64_t xl, xr, x, x0, x1;
            if (tex_precise()) {
                l.x = edge_x(l.from, l.to, l.y0, l.n, row);
                r.x = edge_x(r.from, r.to, r.y0, r.n, row);
            }
            xl = l.x >> 8;
            xr = r.x >> 8;
            uint32_t width = xr >= xl ? (uint32_t)(xr - xl + 1 > 0xFFFFFFF ? 0xFFFFFFF : xr - xl + 1) : 1;
            uint16_t du, dv, tu, tv;
            uint16_t ur = r.u, ul = l.u, vr = r.v, vl = l.v;
            r.u = (uint16_t)(r.u + r.du);
            du = row_step(ur, ul, width);
            l.u = (uint16_t)(l.u + l.du);
            r.v = (uint16_t)(r.v + r.dv);
            dv = row_step(vr, vl, width);
            l.v = (uint16_t)(l.v + l.dv);
            tu = (uint16_t)(ul + ram16(TEX_U_OFF));
            tv = (uint16_t)(vl + ram16(TEX_V_OFF));
            if (tex_precise() && row >= 0 && row < th) {
                double hdu = remainder(r.hu - l.hu, 256.0) / width, hdv = remainder(r.hv - l.hv, 256.0) / width;
                double hu0 = l.hu + ram16(TEX_U_OFF) / 256.0, hv0 = l.hv + ram16(TEX_V_OFF) / 256.0;
                FwSpan *sp;
                x0 = xl < 0 ? 0 : xl;
                x1 = xl + (int64_t)width < tw ? xl + (int64_t)width : tw;
                if (x0 < x1 && (sp = span_new())) {
                    int64_t k = x1 - 1 - xl;
                    unsigned iu, iv;
                    memset(sp, 0, sizeof *sp);
                    sp->pad1[0] = (uint8_t)(span_ref_only | fw_ref_pass);
                    sp->y = row;
                    sp->x0 = (int32_t)x0;
                    sp->x1 = (int32_t)(x1 - 1);
                    sp->xl = xl;
                    sp->u = llround(hu0 * 4294967296.0);
                    sp->du = llround(hdu * 4294967296.0);
                    sp->v = llround(hv0 * 4294967296.0);
                    sp->dv = llround(hdv * 4294967296.0);
                    sp->bank = bank;
                    sp->base = base;
                    sp->mask = mask;
                    sp->por = POR_NOW();
                    sp->hi = fw_hi();
                    sp->textured = 1;
                    if (pl) {           /* perspective-correct: the row's ends from its edges (tex_persp) */
                        double ou = ram16(TEX_U_OFF) / 256.0, ov = ram16(TEX_V_OFF) / 256.0;
                        double uL, vL, qL, uR, vR, qR, steps = width > 1 ? (double)(width - 1) : 1.0;
                        edge_persp(&l, row, &uL, &vL, &qL);
                        edge_persp(&r, row, &uR, &vR, &qR);
                        if (qL > 0 && qR > 0) {
                            uR = uL + remainder(uR - uL, 256.0);    /* the nearer turn, as the row's step takes it */
                            vR = vL + remainder(vR - vL, 256.0);
                            sp->textured = 2;
                            sp->pq = (float)qL;
                            sp->pdq = (float)((qR - qL) / steps);
                            sp->pa = (float)((uL + ou) * qL);
                            sp->pda = (float)(((uR + ou) * qR - (uL + ou) * qL) / steps);
                            sp->pb = (float)((vL + ov) * qL);
                            sp->pdb = (float)(((vR + ov) * qR - (vL + ov) * qL) / steps);
                        }
                    }
                    /* COLOR's register as the span's last pixel leaves it */
                    if (sp->textured == 2) {
                        float kf = (float)k, qk = fw_fmaf(sp->pdq, kf, sp->pq);
                        iu = persp_texel(fw_fmaf(sp->pda, kf, sp->pa), qk);
                        iv = persp_texel(fw_fmaf(sp->pdb, kf, sp->pb), qk);
                    } else {
                        iu = (unsigned)((sp->u + sp->du * k) >> 32) & 0xFF;
                        iv = (unsigned)((sp->v + sp->dv * k) >> 32) & 0xFF;
                    }
                    gsu_color(fw_texel(bank, (uint16_t)(((iv << 8 | iu) & mask) + base)));
                }
            } else if (row >= 0 && row < th) {
                uint8_t *o = tex + (size_t)row * tw;
                /* the texels left of the screen stepped at once */
                x0 = xl < 0 ? 0 : xl;
                x1 = xl + (int64_t)width < tw ? xl + (int64_t)width : tw;
                tu = (uint16_t)(tu + du * (uint64_t)(x0 - xl));
                tv = (uint16_t)(tv + dv * (uint64_t)(x0 - xl));
                for (x = x0; x < x1; x++) {
                    uint16_t at = (uint16_t)((((tv & 0xFF00) | tu >> 8) & mask) + base);
                    int c;
                    tv = (uint16_t)(tv + dv);
                    tu = (uint16_t)(tu + du);
                    gsu_color(gsu_rd8((uint32_t)bank << 16 | at));
                    if ((c = plot_colour()) >= 0) {
                        o[x] = (uint8_t)c;
                        if (checking)
                            check_alias(x, row, c);
                    }
                }
            }
            l.x = edge_step(l.x, l.slope);
            r.x = edge_step(r.x, r.slope);
            l.hu += l.hdu;
            r.hu += r.hdu;
            l.hv += l.hdv;
            r.hv += r.hdv;
            row++;
        }
        l.rows = left_rows;
        r.rows = right_rows;
    }
    tex_persp = NULL;
    return (uint16_t)(v + size);
}

static void water_view(void);

/* the draw list's polygon whose vertices start at v: its depth, if its
   vertices were kept this list (0: a line, or none) */
static void draw_depth(uint16_t v)
{
    int k = v && face_depth[v >> 1].gen == list_gen;
    if (fw_env("FW_DEPTHCHECK") && v) {
        static long with, without;
        if (k) with++; else without++;
        if ((with + without) % 200000 == 0)
            fprintf(stderr, "fwdepth: polygons drawn with a depth %ld, without %ld\n", with, without);
    }
    cur_z[0] = k ? face_depth[v >> 1].z[0] : 0;
    cur_z[1] = k ? face_depth[v >> 1].z[1] : 0;
    cur_z[2] = k ? face_depth[v >> 1].z[2] : 0;
}

/* FxObjectSprite past its depth test (1, render/frame.c) and its end (0):
   the sprite's one depth, its place's ($002A, a far one's halved
   rm_far_shift times) */
void fw_depth_sprite(int on)
{
    int32_t z = on ? (int32_t)(int16_t)ram16(0x002A) * (1 << rm_far_shift) : 0;
    sprite_z[0] = z > 0 ? (float)(1.0 / z) : 0;
    sprite_z[1] = sprite_z[2] = 0;
    cur_z[0] = sprite_z[0];
    cur_z[1] = cur_z[2] = 0;
}

void fw_water_view(void) { water_view(); }

void fw_draw_list(void)
{
    uint16_t at, end = ram16(LIST_END);

    if (fw_drawing)
        water_view();

    if (end == DRAW_LIST)
        return;
    ram8w(end, 0);
    ram8w((uint16_t)(end + 1), 0);
    at = DRAW_LIST;
    for (;;) {
        int8_t type = (int8_t)ram8(at);
        int face = fw_drawing && sc_gpu_on() && !gx ? sc_face_for_entry(at) : -1;
        if (face >= 0 && getenv("SC_NEARREF")) {    /* (a check) a face with a near corner left to the reference */
            size_t nf;
            const ScFace *fc = sc_list_faces(&nf);
            if ((size_t)face < nf && fc[face].pad[0])
                face = -1;
        }
        at++;
        draw_depth(type >= 3 ? (uint16_t)(at + 1) : type < 0 ? (uint16_t)(at + 7) : 0);
        if (type && fw_env("FW_CENSUS")) {
            if (face >= 0) {
                /* the entry and its display-list face the same: a flat
                   one's colour (its second byte), a textured one's bank */
                size_t nf;
                const ScFace *fc = sc_list_faces(&nf);
                cen_entries_gpu++;
                if ((size_t)face < nf && type != 2 &&
                    (type >= 3 ? fc[face].textured || fc[face].colour != ram8(at)
                               : !fc[face].textured || fc[face].bank != ram8(at)))
                    cen_map_apart++;
            } else
                cen_entries[type >= 3 ? 0 : type == 2 ? 1 : 2]++;
        }
        if (type && face >= 0) {        /* a model face the GPU draws from the display list (scene.h) */
            FwSpan *m = span_new();
            if (m) {
                m->textured = 6;
                m->y = m->x0 = 0;
                m->x1 = face;
                m->pad1[0] = 0;
            }
            if (!fw_env("FW_GPUCHECK")) {   /* (with the check, the reference's spans too, the CPU's alone) */
                at = (uint16_t)(type >= 3 ? at + 1 + 4 * type : type == 2 ? at + 9 : at + 7 + 8 * (uint8_t)~type);
                continue;
            }
            span_ref_only = 1;
        }
        if (type >= 3)
            at = flat_polygon(at, type);
        else if (type == 2)
            at = line(at);
        else if (type == 0)
            break;
        else
            at = tex_polygon(at, (uint8_t)~type);
        span_ref_only = 0;
    }
    span_ref_only = 0;
    cur_z[0] = cur_z[1] = cur_z[2] = 0;
    list_gen++;                         /* the list drawn: its exact vertices gone */
    sc_entries_drawn();
}

/* ---- the picture to the console ------------------------------------------ */

/* BG1 in the full window: the canvas over the whole screen
   (nshvc_ext_canvas_screen), the texels in the screen's order, at priority
   1 as the race's BG1 tiles. On a screen taller than 224 the console's
   lines sit in its middle, from row T = (H - 224) / 2
   (nshvc_ext_screen_origin): the console's hills and the sky's lines then
   lie about the 3D's horizon as on the original's screen. */
static int tall(void) { return H > 224; }
static int origin_row(void) { return (H - 224) / 2; }

/* the screen's size for the window: its aspect at 224 lines (256 across
   at 4:3, the console's pixels 7:6 as nativeshvc presents them), the
   nearest even width (the original's 256 columns then centred on a whole
   column), the menus' rule (menuwindow.c), so the race and the menus share
   one screen; narrower than 4:3, 256 across and taller, the nearest even
   height; to 1024 each way. The console's pixels keep their shape: the
   screen is presented at its exact aspect, so a window whose shape no
   whole even width meets shows a gap of up to a column or a line or so at
   its edges, never a stretched picture. */
#define W_MAX 1024
#define H_MAX 1024

static void size_of_window(int *w, int *h)
{
    float ww = 0, wh = 0, a;

    *w = 256;
    *h = 224;
    nshvc_window_size(&ww, &wh);
    if (fw_env("FW_WINDOW"))            /* a check: the window's shape, width,height (headless has none of its own) */
        sscanf(fw_env("FW_WINDOW"), "%f,%f", &ww, &wh);
    if (!(ww > 0 && wh > 0))
        return;
    a = (ww / wh) / (4.0f / 3.0f);
    if (a >= 1.0f) {
        *w = (int)(256.0f * a / 2.0f + 0.5f) * 2;
        if (*w > W_MAX)
            *w = W_MAX;
    } else {
        *h = (int)(224.0f / a / 2.0f + 0.5f) * 2;
        if (*h > H_MAX)
            *h = H_MAX;
    }
}

static int size_for_window(void)
{
    int w, h, pw = 0, ph = 0;
    size_t need;

    /* a texel a window pixel: the window pixels the screen is presented
       over (contract 2.15, handed at k 0) */
    size_of_window(&w, &h);
    nshvc_ext_canvas_presented(&pw, &ph);
    if (pw < 1 || ph < 1)
        return 0;
#ifdef SRFX_NSHVC_HANDOFF
    if (fw_tex_align > 1 && sc_gpu_on())    /* (Vulkan's fill: else every odd window the CPU's) */
        pw = (pw + fw_tex_align - 1) & ~(fw_tex_align - 1);
#endif
    if (pw != tw || ph != th || w != W || h != H || !tex || scale_lost) {
        scale_lost = 0;
        W = w;
        H = h;
        tw = pw;
        th = ph;
        SXN = tw;
        SXD = W;
        SYN = th;
        SYD = H;
        need = (size_t)tw * th;
        if (need > tex_size) {
            if (fw_texels_alloc) {          /* the GPU's memory, which it fills */
                tex = fw_texels_alloc(need);
            } else {
                free(tex);
                tex = malloc(need);
            }
            tex_size = tex ? need : 0;
            if (!tex_size) {
                FW_CPU_FALLBACK("the full window with no GPU memory");
                return 0;
            }
        }
        handed = 0;
        page1_known = 0;                /* the screen anew: page 1 written whole again */
    }
    return 1;
}

/* BG1's scroll in a race, as its setup leaves it (bank $03: $FFEF, -17,
   across 0): the picture's row y is the map's row y - 16 */
#define RACE_ROW0 16

/* ---- the water -------------------------------------------------------------- */

/* Aqua Tunnel's water (course 1) in the Enhanced picture: a plane at world
   height WATER_Y (y down) under the road and over the glass tube, wherever
   nothing else is drawn, seen from either side. The original has no water
   height: the road outside never goes below -1807, the car in the tube
   stays between -49 and -146 (the player's record over the replayed race),
   and -1280 is where the track's records settle. Its art is the
   underwater backdrop's (BG2's map at $7F:93D7, characters from VRAM word
   $6000, palette 0): the nearest row of ripple cells, map rows 51-52, 64 x
   16 texels, laid on the plane at the size the original shows it, read
   off its most level underwater frame (the replay's 4724: the cells' top
   row on lines 1-17, 64 pixels across, meets the plane over 2148 across
   and 859 deep: 33.6 and 53.7 a texel). Its ripples take the strip's
   colours by distance, as its rows shade from near to the horizon
   (rows 51-60 over lines 1 to the horizon, line 59, on that frame: a
   pixel row's depth from the view there, WATER_DEPTH); the water between
   them is CGRAM colour 1, the line's band colour, as the strip's own. */
#define WATER_Y (-1280.0)
#define WATER_U 33.6
#define WATER_V 53.7

/* Each course's surface ($0E3D): none (Sky Ramp: its band is sky all the
   way down, the track meant to float), land or water by the ground colours
   of its band, at the height its scenery is planted at (the commonest
   height of its scenery's records over the courses' plays). Aqua Tunnel's
   water is the one the track is cut by and that is seen from under; the
   others are drawn only where nothing else is, so a road that dips below
   them is never left out (the track occludes the ground), and not while
   the eye is below them. */
enum { SURF_NONE, SURF_LAND, SURF_WATER, SURF_AQUA };
static const struct { uint8_t kind; int16_t y; } surface[0x20] = {
    [0x00] = { SURF_LAND, -1280 },      /* Easy Ride */
    [0x01] = { SURF_AQUA, -1280 },      /* Aqua Tunnel */
    [0x02] = { SURF_LAND, 0 },          /* Sunset Valley */
    [0x03] = { SURF_LAND, 0 },          /* Night Owl */
    [0x04] = { SURF_LAND, 0 },          /* King's Forest */
    [0x05] = { SURF_WATER, -1536 },     /* Sea Breeze */
    [0x06] = { SURF_LAND, 0 },          /* White Land */
    [0x07] = { SURF_LAND, -3840 },      /* Night Cruise */
    [0x08] = { SURF_WATER, 0 },         /* Lake Side */
    [0x09] = { SURF_LAND, 0 },          /* Big Ravine */
    [0x0A] = { SURF_NONE, 0 },          /* Sky Ramp */
    [0x0B] = { SURF_WATER, 3840 },      /* Harbor City */
    [0x0C] = { SURF_LAND, 0 },          /* Rock Field */
    [0x0D] = { SURF_LAND, 0 },          /* Up'n Down */
    [0x0E] = { SURF_LAND, 0 },          /* Ice Dance */
    [0x0F] = { SURF_WATER, 0 },         /* Blue Lake */
    [0x10] = { SURF_LAND, 0 },          /* Battle */
    [0x12] = { SURF_LAND, 0 },
    [0x13] = { SURF_LAND, 0 },
    [0x14] = { SURF_LAND, 0 },          /* the bonus courses */
    [0x15] = { SURF_LAND, 0 },
    [0x16] = { SURF_LAND, 0 },
};

static int surf_kind(void)
{
    unsigned c = nshvc_wram()[0x0E3D];
    return c < 0x20 ? surface[c].kind : SURF_NONE;
}

static double surf_y(void)
{
    unsigned c = nshvc_wram()[0x0E3D];
    return c < 0x20 ? surface[c].y : 0;
}
static const uint32_t water_depth[80] = {
    2152, 2180, 2208, 2236, 2266, 2296, 2327, 2359, 2392, 2426, 2461, 2497, 2534, 2572, 2611, 2651,
    2693, 2736, 2780, 2826, 2873, 2922, 2973, 3025, 3079, 3135, 3194, 3254, 3317, 3383, 3451, 3522,
    3595, 3672, 3753, 3837, 3924, 4016, 4112, 4213, 4319, 4431, 4548, 4672, 4803, 4941, 5088, 5243,
    5409, 5585, 5773, 5974, 6189, 6421, 6670, 6940, 7233, 7551, 7898, 8280, 8699, 9164, 9681, 10260,
    10912, 11653, 12503, 13485, 14635, 16000, 17645, 19668, 22214, 25518, 29976, 36321, 46074, 62987, 99520, 236952,
};
int16_t rm_view_place[3];
static int sky_lines(uint16_t *col, int n);
static double water_e[3][3], water_cam[3];
static int water_seen;                  /* a picture's view kept this frame */
static int water_due;                   /* a water picture waiting for its band (fw_sky) */
static uint32_t *tex32;
static size_t tex32_size;

/* the view the picture is drawn from (fw_draw_list, while drawing): $00E4's
   turn and the place the records are from */
static int water_here(void);

static void water_view(void)
{
    int i, j;
    if (bp || gx || two) {              /* the bitmap pictures have none: the console's own layers show through; two players: one surface a picture won't do */
        fw_water[3] = 0;
        return;
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++)
            water_e[i][j] = (int16_t)ram16((uint16_t)(0x00E4 + 6 * i + 2 * j)) / 32768.0;
        water_cam[i] = rm_view_place[i];
    }
    water_seen = 1;
    {                                   /* the water's 1 / depth along each pixel's ray: (E's row 1 . ray) / dy */
        double fx = fw_focal_x(), fy = fw_focal_y(), ox = tw / 2.0, oy = (double)centre_row(), dy = surf_y() - water_cam[1];
        int here = water_here() && dy != 0 && surf_kind() == SURF_AQUA;   /* the track cut only by Aqua Tunnel's */
        fw_water[0] = here ? (float)((water_e[1][0] * (0.5 - ox) / fx + water_e[1][1] * (0.5 - oy) / fy + water_e[1][2]) / dy) : 0;
        fw_water[1] = here ? (float)(water_e[1][0] / (fx * dy)) : 0;
        fw_water[2] = here ? (float)(water_e[1][1] / (fy * dy)) : 0;
        fw_water[3] = (float)here;
    }
}

/* A course's surface, out of a sealed tunnel (Aqua Tunnel's: the dark tunnel's
   gates set bit 3 of $0DA5 going in ($0A) and its exit slide clears it
   ($04); the glass tube's gate ($44) leaves it clear. Rock all round, no
   water: none drawn and nothing of the track left out for it. */
static int water_here(void)
{
    const uint8_t *w = nshvc_wram();
    int k = surf_kind();
    if (k == SURF_NONE || (w[0x0DA5] & 0x08) || !rm_enhanced())
        return 0;
    return k == SURF_AQUA || water_cam[1] < surf_y();   /* the others only from above */
}

/* a BG2 character's pixel (4 bits a pixel, its word $6000 + 16 n) */
static unsigned bg2_pixel(const uint16_t *v, unsigned entry, unsigned x, unsigned y)
{
    const uint16_t *t = v + 0x6000 + (entry & 0x3FF) * 16;
    unsigned c = 0, b;
    if (entry & 0x4000)
        x = 7 - x;
    if (entry & 0x8000)
        y = 7 - y;
    for (b = 0; b < 2; b++)
        c |= ((t[y] >> (7 - x + 8 * b)) & 1) << b | ((t[8 + y] >> (7 - x + 8 * b)) & 1) << (b + 2);
    return c;
}

static uint32_t rgb8(uint16_t bgr)
{
    unsigned r = bgr & 31, g = bgr >> 5 & 31, b = bgr >> 10 & 31;
    return 0xFF000000u | (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

/* the ripple texels in [0, u) x [0, v) of the cells laid without end */
static int32_t ripples_to(const FwWaterJob *j, int32_t u, int32_t v)
{
    int32_t qu = u >= 0 ? u / 64 : -((63 - u) / 64), qv = v >= 0 ? v / 16 : -((15 - v) / 16);
    int32_t ru = u - 64 * qu, rv = v - 16 * qv;
    return qu * qv * j->sat[16 * 65 + 64] + qu * j->sat[rv * 65 + 64] + qv * j->sat[16 * 65 + ru] + j->sat[rv * 65 + ru];
}

static int32_t ripples_in(const FwWaterJob *j, int32_t u0, int32_t u1, int32_t v0, int32_t v1)
{
    return ripples_to(j, u1 + 1, v1 + 1) - ripples_to(j, u0, v1 + 1) - ripples_to(j, u1 + 1, v0) + ripples_to(j, u0, v0);
}

/* one texel of the water pass, as the GPU's computes it: each float step
   written out (fmaf, IEEE's divide, floorf) */
static uint32_t water_texel(const uint8_t *in, const FwWaterJob *j, const uint32_t *gap, int32_t x, int32_t y)
{
    const float *e = j->e;
    float fx = (float)x + 0.5f, fy = (float)y + 0.5f, vx, vy, vx1, vy1, wy, t, t1, t2, u, v, du, dv, cover, fu, fv;
    uint32_t c = in[(size_t)y * (size_t)j->w + (size_t)x], g = gap[y], rip, out = 0xFF000000u;
    int32_t lo, hi, u0, u1, v0, v1, s;

    if (c)                              /* drawn (before the water: what is past it was left out) */
        return c;
    vx = (fx - j->ox) / j->fx;
    vy = (fy - j->oy) / j->fy;
    wy = fw_fmaf(e[4], vx, fw_fmaf(e[5], vy, e[6]));
    t = j->dy / wy;
    if (!(wy != 0) || !(t > 0))         /* no surface this way: under it, deeper water; over it, the sky */
        return j->under ? g : 0;
    u = fw_fmaf(t, fw_fmaf(e[0], vx, fw_fmaf(e[1], vy, e[2])), j->cam[0]) / j->wu;
    v = fw_fmaf(t, fw_fmaf(e[8], vx, fw_fmaf(e[9], vy, e[10])), j->cam[2]) / j->wv;
    vx1 = (fx + 1.0f - j->ox) / j->fx;  /* the pixel's footprint: the next column's and line's hits */
    vy1 = (fy + 1.0f - j->oy) / j->fy;
    t1 = j->dy / fw_fmaf(e[4], vx1, fw_fmaf(e[5], vy, e[6]));
    t2 = j->dy / fw_fmaf(e[4], vx, fw_fmaf(e[5], vy1, e[6]));
    if (t1 > 0 && t2 > 0) {
        float ux = fw_fmaf(t1, fw_fmaf(e[0], vx1, fw_fmaf(e[1], vy, e[2])), j->cam[0]) / j->wu - u;
        float vx_ = fw_fmaf(t1, fw_fmaf(e[8], vx1, fw_fmaf(e[9], vy, e[10])), j->cam[2]) / j->wv - v;
        float uy = fw_fmaf(t2, fw_fmaf(e[0], vx, fw_fmaf(e[1], vy1, e[2])), j->cam[0]) / j->wu - u;
        float vy_ = fw_fmaf(t2, fw_fmaf(e[8], vx, fw_fmaf(e[9], vy1, e[10])), j->cam[2]) / j->wv - v;
        du = fmaxf(fabsf(ux), fabsf(uy));
        dv = fmaxf(fabsf(vx_), fabsf(vy_));
    } else {
        du = 64.0f;
        dv = 16.0f;
    }
    du = fminf(du, 64.0f);              /* a whole cell across is all of them */
    dv = fminf(dv, 16.0f);
    fu = u - 64.0f * floorf(u / 64.0f);
    fv = v - 16.0f * floorf(v / 16.0f);
    if (j->point) {
        u0 = u1 = (int32_t)floorf(fu);
        v0 = v1 = (int32_t)floorf(fv);
    } else {
        u0 = (int32_t)floorf(fu - du * 0.5f);
        u1 = (int32_t)floorf(fu + du * 0.5f);
        v0 = (int32_t)floorf(fv - dv * 0.5f);
        v1 = (int32_t)floorf(fv + dv * 0.5f);
    }
    cover = (float)ripples_in(j, u0, u1, v0, v1) / (float)((u1 - u0 + 1) * (v1 - v0 + 1));
    for (lo = 0, hi = 79; lo < hi;) {   /* the first strip row as deep as t (the view's depth) */
        int32_t m = (lo + hi) / 2;
        if (j->depth[m] >= t)
            hi = m;
        else
            lo = m + 1;
    }
    rip = j->land ? gap[j->h + y] : j->ripple[lo];
    if (!(cover > 0))
        return g;
    if (g == 1)                         /* the check's: colour 1 left to the console */
        return cover >= 0.5f ? rip : g;
    if (cover >= 1)
        return rip;
    for (s = 0; s < 24; s += 8) {
        float a = (float)(g >> s & 255), b = (float)(rip >> s & 255);
        out |= (uint32_t)floorf(fw_fmaf(b - a, cover, a) + 0.5f) << s;
    }
    return out;
}

void fw_water_fill_cpu(const uint8_t *in, uint32_t *out, const FwWaterJob *j, const uint32_t *gap)
{
    FW_CPU_FALLBACK("the ground's fill");
    int32_t x, y;
    for (y = 0; y < j->h; y++)
        for (x = 0; x < j->w; x++)
            out[(size_t)y * (size_t)j->w + (size_t)x] = water_texel(in, j, gap, x, y);
}

void (*fw_water_filler)(const uint8_t *in, uint32_t *out, const FwWaterJob *j, const uint32_t *gap) = fw_water_fill_cpu;
uint32_t *(*fw_texels32_alloc)(size_t len);

/* The other courses' surface: no art of the game's is ground, so its
   marks are drawn in its own band's shades (a nearer line's, SHADE_LINES
   on), on cells the size of Aqua Tunnel's: land a tuft (a 3-wide V) in
   two of each three 8 x 4 spots, at a place hashed from the spot; water a
   glint (a 4-wide dash) in one of each two. */
#define SHADE_LINES 24

static int tuft(unsigned x, unsigned y)
{
    unsigned cx = x / 8, cy = y / 4, h = (cx * 2654435761u) ^ (cy * 40503u), ox = h % 5, oy = (h >> 8) % 2;
    unsigned lx = x % 8, ly = y % 4;
    int on = (ly == oy + 1 && (lx == ox || lx == ox + 2)) || (ly == oy && lx == ox + 1);
    return on && (h >> 16) % 3 != 0 ? 2 : 0;
}

static int glint(unsigned x, unsigned y)
{
    unsigned cx = x / 8, cy = y / 4, h = (cx * 2654435761u) ^ (cy * 40503u), ox = h % 4, oy = (h >> 8) % 4;
    return y % 4 == oy && x % 8 >= ox && x % 8 < ox + 4 && (h >> 16) % 2 ? 2 : 0;
}

/* the 32-bit texels' memory, for tw x th */
static int tex32_room(void)
{
    size_t need = (size_t)tw * th;

    if (need > tex32_size) {
        if (fw_texels32_alloc) {        /* the GPU's memory, which it fills */
            tex32 = fw_texels32_alloc(need * sizeof *tex32);
        } else {
            free(tex32);
            tex32 = malloc(need * sizeof *tex32);
        }
        tex32_size = tex32 ? need : 0;
        if (!tex32)
            return 0;
    }
    return 1;
}

/* the texels with the water where nothing is drawn, 32 bits each
   (FW_WATERCHECK: idx, the water between the ripples left to the
   console's colour 1, or band, this file's; either with the cells read
   texel by texel, for the two to be compared) */
static const uint32_t *water_pass(void)
{
    static FwWaterJob job;
    static uint32_t *gap;
    static size_t gap_rows;
    uint8_t cell[16][64];
    const char *chk = fw_env("FW_WATERCHECK");
    int index_gap = chk && !strcmp(chk, "idx");
    const uint8_t *w = nshvc_wram();
    const uint16_t *v = nshvc_vram();
    unsigned r, x, y, i, k;

#ifdef SRFX_NSHVC_HANDOFF
    if (!(gpu_pic && hand_pic()))       /* (the GE's, handed: its canvas holds them) */
#endif
    if (!tex32_room())
        return NULL;
    if ((size_t)th > gap_rows) {
        free(gap);
        gap = malloc(2 * (size_t)th * sizeof *gap);
        gap_rows = gap ? (size_t)th : 0;
        if (!gap)
            return NULL;
    }
    job.w = tw;
    job.h = th;
    job.ox = (float)(tw / 2.0);
    job.oy = (float)(double)centre_row();
    job.fx = (float)fw_focal_x();
    job.fy = (float)fw_focal_y();
    job.dy = (float)(surf_y() - water_cam[1]);
    job.land = surf_kind() != SURF_AQUA;
    job.under = job.dy < 0;
    job.point = chk != NULL;
    job.wu = (float)WATER_U;
    job.wv = (float)WATER_V;
    for (i = 0; i < 3; i++) {
        for (k = 0; k < 3; k++)
            job.e[4 * i + k] = (float)water_e[i][k];
        job.e[4 * i + 3] = 0;
        job.cam[i] = (float)water_cam[i];
    }
    for (y = 0; y < 16; y++)            /* the cells: Aqua Tunnel's its backdrop's map rows 51-52, 8 entries */
        for (x = 0; x < 64; x++) {
            unsigned at = 0x193D7 + (51 + y / 8) * 0x140 + 2 * (x / 8);
            cell[y][x] = job.land ? (uint8_t)(surf_kind() == SURF_WATER ? glint(x, y) : tuft(x, y))
                                  : (uint8_t)bg2_pixel(v, w[at] | w[at + 1] << 8, x & 7, y & 7);
        }
    for (r = 0; r < 80; r++) {          /* each strip pixel row's ripple colour: its commonest of 2-15 */
        unsigned n[16] = { 0 }, best = 2, c, at;
        uint16_t bgr;
        for (x = 0; x < 64; x++) {
            at = 0x193D7 + (51 + r / 8) * 0x140 + 2 * (x / 8);
            n[bg2_pixel(v, w[at] | w[at + 1] << 8, x & 7, r & 7)]++;
        }
        for (c = 2; c < 16; c++)
            if (n[c] > n[best])
                best = c;
        bgr = (uint16_t)(w[0x139D7 + 2 * best] | w[0x139D8 + 2 * best] << 8);   /* the underwater palette's (HudColourStep's copy) */
        job.ripple[r] = rgb8(bgr);
        job.depth[r] = (float)water_depth[r];
    }
    for (y = 0; y <= 16; y++)           /* the cells' ripple texels counted from the corner */
        for (x = 0; x <= 64; x++)
            job.sat[y * 65 + x] = !x || !y ? 0
                : job.sat[(y - 1) * 65 + x] + job.sat[y * 65 + x - 1] - job.sat[(y - 1) * 65 + x - 1] + (cell[y - 1][x - 1] >= 2);
    {                                   /* each row's water between the ripples: its line's colour 1, as the console gives it */
        static uint16_t line[1024];
        int n = H + SHADE_LINES < 1024 ? H + SHADE_LINES : 1024;
        int have = !index_gap && sky_lines(line, n);
        for (y = 0; y < (unsigned)th; y++) {
            unsigned l = (unsigned)floor_div((int64_t)y * SYD, SYN);   /* the console line texel row y is on */
            gap[y] = have && l < 1024 ? rgb8(line[l]) : 1;
            gap[th + y] = have ? rgb8(line[(int)l + SHADE_LINES < n ? l + SHADE_LINES : (unsigned)n - 1]) : 1;
        }
    }
    if (gpu_pic) {                      /* the GPU's picture: its ground drawn there, then out */
        if (!gpu_open)
            span_flush();
    }
    if (gpu_pic) {
#ifdef SRFX_NSHVC_HANDOFF
        fw_ground_handed = hand_pic();
#endif
        fw_canvas_gpu->ground(&job, gap);
#ifdef SRFX_NSHVC_HANDOFF
        if (hand_pic()) {              /* none of it read back: the mark says it has its ground */
            static const uint32_t ground_mark = 0;
            fw_canvas_gpu->close(NULL, NULL);
            gpu_pic = gpu_open = 0;
            return &ground_mark;
        }
#endif
        fw_canvas_gpu->close(tex32, tex);
        gpu_pic = gpu_open = 0;
        water_record(tex, &job, gap);
        return tex32;
    }
    water_record(tex, &job, gap);
    fw_water_filler(tex, tex32, &job, gap);
    return tex32;
}

/* the finished picture to the console: t32 its 32 bits with the ground, or the
   indices alone */
static void picture_out(const uint32_t *t32)
{
#ifdef SRFX_NSHVC_HANDOFF
    if (hand_pic()) {
        if ((!t32 && !fw_canvas_gpu && !tex32_room()) || !fw_handoff->picture(tw, th, t32 != NULL))
            fw_cpu_fallback("the picture's hand-off");
        handed_now = 1;
        return;
    }
#endif
    if (t32)
        nshvc_ext_canvas_screen32(0, t32, tw, th, 0, 1, 0);
    else
        nshvc_ext_canvas_screen(0, tex, tw, th, 0, 1, 0);
}

/* ---- two players' pictures ------------------------------------------------- */
/* Each player's rectangle (fullwindow.h fw_clip's while it draws) from the
   screen's size. */
static void two_layout(void)
{
    two_side = W >= TWO_SIDE_W;
    if (two_side) {
        two_w = (W - TWO_SEAM) / 2;
        two_h = H;
        two_x[0] = 0;
        two_x[1] = W - two_w;
        two_y[0] = two_y[1] = 0;
    } else {
        two_w = W;
        two_h = (H - TWO_SEAM) / 2;
        two_x[0] = two_x[1] = 0;
        two_y[0] = 0;
        two_y[1] = H - two_h;
    }
}

/* where each player's view is on the original's screen, and its centre (the
   backdrop's and sky's place about it) */
static const int two_vx[2] = { 16, 96 }, two_vy[2] = { 24, 128 };

/* a texture of the banks ($F0 up, 256 x 256, 0 not drawn) on the screen: console
   columns c0 to c1 and rows r0 to r1, column c its column c - s and row r its row
   r - top */
static void bank_quad(int bank, int c0, int c1, int r0, int r1, int s, int top)
{
    int32_t t0 = row_at(r0), t1 = row_at(r1 + 1) - 1;
    FwSpan *p = quad_new(t0, t1 - t0 + 1, col_at(c0), col_at(c1 + 1) - 1, (int64_t)(t0 + 1) * SYD - 1 - (int64_t)top * SYN, SYD);
    uint32_t den = (uint32_t)SXN, zero = 0;
    if (!p)
        return;
    p->xl = 0;
    p->u = SXD - 1 - (int64_t)s * SXN;
    p->du = SXD;
    p->mask = (uint16_t)SYN;
    p->bank = (uint8_t)bank;
    p->base = 0;
    p->pad1[1] = 8;
    memcpy(&p->pq, &den, 4);
    memcpy(&p->pdq, &zero, 4);
    memcpy(&p->pda, &zero, 4);
    p->por = 0;
    p->hi = 0;
    p->za = p->zx = p->zy = 0;
}

/* a part of a texture (sw x sh from sx, sy) over w x h texels at x, y */
static void bank_quad_scaled(int bank, int sx, int sy, int sw, int sh, int x, int y, int w, int h)
{
    FwSpan *p;
    uint32_t den = (uint32_t)w, zero = 0;
    if (w < 1 || h < 1 || !(p = quad_new(y, h, x, x + w - 1, (int64_t)sy * h, sh)))
        return;
    p->xl = x;
    p->u = (int64_t)sx * w;
    p->du = sw;
    p->mask = (uint16_t)h;
    p->bank = (uint8_t)bank;
    p->base = 0;
    p->pad1[1] = 8;
    memcpy(&p->pq, &den, 4);
    memcpy(&p->pdq, &zero, 4);
    memcpy(&p->pda, &zero, 4);
    p->por = 0;
    p->hi = 0;
    p->za = p->zx = p->zy = 0;
}

/* The hills: BG2's map as the two players' race has it ($3C00, 64 x 64, characters
   from $4000) as four textures, 512 x 512, each pixel its CGRAM index; the sky's
   colour 1 and colour 0 not drawn (two_sky's behind them). Written when they differ. */
#define TWO_BG2_MAP 0x3C00
#define TWO_BG2_CHARS 0x4000
static void two_hills(void)
{
    static uint8_t img[4][0x10000];
    const uint16_t *v = nshvc_vram();
    int r, c, y, x, k;

    memset(img, 0, sizeof img);
    for (r = 0; r < 64; r++)
        for (c = 0; c < 64; c++) {
            /* the map: the screens 32 x 32, left then right, top then bottom */
            uint16_t e = v[TWO_BG2_MAP + (r >= 32 ? 0x800 : 0) + (c >= 32 ? 0x400 : 0) + (r & 31) * 32 + (c & 31)];
            const uint16_t *ch = v + ((TWO_BG2_CHARS + (e & 0x3FF) * 16) & 0x7FFF);
            int pal = e >> 10 & 7;
            for (y = 0; y < 8; y++) {
                int py = e & 0x8000 ? 7 - y : y, sy = r * 8 + y;
                uint16_t w0 = ch[py], w1 = ch[8 + py];
                for (x = 0; x < 8; x++) {
                    int px = e & 0x4000 ? x : 7 - x, sx = c * 8 + x;
                    int n = (w0 >> px & 1) | (w0 >> (8 + px) & 1) << 1 | (w1 >> px & 1) << 2 | (w1 >> (8 + px) & 1) << 3;
                    if (n && !(n == 1 && !pal))
                        img[(sy >> 8) * 2 + (sx >> 8)][(sy & 255) * 256 + (sx & 255)] = (uint8_t)(pal << 4 | n);
                }
            }
        }
    for (k = 0; k < 4; k++)
        if (memcmp(img[k], panel_bank[4 + k], 0x10000)) {
            memcpy(panel_bank[4 + k], img[k], 0x10000);
            panel_version[4 + k]++;
        }
}

/* the hills on player p's rectangle: the original's column and line about its
   view's centre at the rectangle's, at its scroll ($0D92, $0D8E: this player's as the
   backdrop's angles left them), a quad for each texture's part */
static void two_hills_quads(int p)
{
    const uint8_t *w = nshvc_wram();
    int X = w[0x0D92 + 2 * p] | w[0x0D93 + 2 * p] << 8, Y = w[0x0D8E + 2 * p] | w[0x0D8F + 2 * p] << 8;
    int sx0 = X + two_vx[p] + 72 - two_w / 2, sy0 = Y + 1 + two_vy[p] + 40 - two_h / 2, r = 0;

    while (r < two_h) {
        int sy = (sy0 + r) & 511, n = 256 - (sy & 255), c = 0;
        if (n > two_h - r)
            n = two_h - r;
        while (c < two_w) {
            int sx = (sx0 + c) & 511, m = 256 - (sx & 255);
            if (m > two_w - c)
                m = two_w - c;
            bank_quad(0xF4 + (sy >> 8) * 2 + (sx >> 8), two_x[p] + c, two_x[p] + c + m - 1, two_y[p] + r, two_y[p] + r + n - 1,
                      two_x[p] + c - (sx & 255), two_y[p] + r - (sy & 255));
            c += m;
        }
        r += n;
    }
}

/* each player's sky a texel row (two_gap[p * th + y]): the original's colour 1 at
   the line the same height above its view's centre, from its 80-line band
   (GradientPointersToBank7F's address at $0D96 + 2p, bank $7F), the band's ends held */
static uint32_t *two_gap;
static size_t two_gap_rows;
static void two_sky_rows(int p)
{
    const uint8_t *w = nshvc_wram();
    unsigned at = w[0x0D96 + 2 * p] | w[0x0D97 + 2 * p] << 8;
    int y;
    if ((size_t)th > two_gap_rows) {
        free(two_gap);
        two_gap = malloc(2 * (size_t)th * sizeof *two_gap);
        two_gap_rows = two_gap ? (size_t)th : 0;
        if (!two_gap)
            return;
    }
    for (y = 0; y < th; y++) {
        int i = 40 + (int)floor_div((int64_t)y * SYD, SYN) - (two_y[p] + two_h / 2);
        unsigned a;
        if (i < 0)
            i = 0;
        if (i > 79)
            i = 79;
        a = 0x10000 + ((at + 2 * (unsigned)i) & 0xFFFF);
        two_gap[(size_t)p * th + y] = rgb8((uint16_t)(w[a] | w[a + 1] << 8));
    }
}

/* the frame program of a player begins (01:AB13): the last player's drawn, this
   one's rectangle held to, its backdrop first */
static void two_begin(void)
{
    span_flush();
    if (!two_p)
        clear_now();
    fw_clip[0] = col_at(two_x[two_p]);
    fw_clip[1] = row_at(two_y[two_p]);
    fw_clip[2] = col_at(two_x[two_p] + two_w) - 1;
    fw_clip[3] = row_at(two_y[two_p] + two_h) - 1;
    sc_picture_begin();
    if (!two_p)
        two_hills();
    two_hills_quads(two_p);
    two_sky_rows(two_p);
}

/* the picture's 32 bits: its indices, and where nothing is drawn its player's sky
   (black between the two) */
static const uint32_t *two_sky(void)
{
    int split = two_side ? col_at(two_x[1]) : row_at(two_y[1]);
    int seam = two_side ? col_at(two_x[0] + two_w) : row_at(two_y[0] + two_h);

    if (!two_gap || !tex32_room())
        return NULL;
    fw_sky_filler(tex, tex32, tw, th, two_gap, two_side, split, seam);
    return tex32;
}

/* the texels to the canvas, the screen's picture */
static void hand(void)
{
    const uint32_t *t32 = NULL;

    span_flush();                       /* the panels' and the HUD's pieces (block_set) */
    if (two && (t32 = two_sky()) != NULL) {
        rm_canvas_taken();
        picture_out(t32);
        if (!handed) {
            nshvc_hold_interrupts(1);
            takeover();
            nshvc_hold_interrupts(0);
        }
        handed = 1;
        return;
    }
    if (water_seen && water_here()) {   /* the water's colours from the band this picture is shown with: fw_sky's, after it */
        water_seen = 0;
        if (handed) {
            water_due = 1;
            return;
        }
        t32 = water_pass();
    }
    water_seen = 0;
    if (gpu_pic && !t32) {              /* the GPU's picture out: its indices */
#ifdef SRFX_NSHVC_HANDOFF
        fw_canvas_gpu->close(NULL, hand_pic() ? NULL : tex);
#else
        fw_canvas_gpu->close(NULL, tex);
#endif
        gpu_pic = gpu_open = 0;
    }
    rm_canvas_taken();
    picture_out(t32);
    if (!handed) {                    /* the first picture: the screen taken over with it, in the same blank */
        nshvc_hold_interrupts(1);
        takeover();
        nshvc_hold_interrupts(0);
    }
    handed = 1;
}

/* FW_CHECK=1 (a check, at k = 1): each picture drawn twice more from the
   same state, by the Super FX into its buffer and into the texels with the
   original's view in place, and every pixel of the 208 x 128 compared:
   the 16-bit fill and PLOT against the Super FX's own (reported at exit) */
static long check_pictures, check_pixels, check_differ;

static void check_report(void)
{
    fprintf(stderr, "fwcheck: %ld pictures, %ld pixels, %ld differ\n", check_pictures, check_pixels, check_differ);
}

static uint8_t gsu_pixel(const uint8_t *buf, unsigned x, unsigned y)
{
    const uint8_t *c = buf + ((x >> 3) * 16 + (y >> 3)) * 64;
    uint8_t v = 0;
    unsigned n;
    for (n = 0; n < 8; n++)
        v |= (uint8_t)(((c[(n >> 1) * 16 + (y & 7) * 2 + (n & 1)] >> (7 - (x & 7))) & 1) << n);
    return v;
}

static void check_picture(int j)
{
    static uint8_t gsu_px[2][RM_TWEEN_BYTES], skip[2][RM_TWEEN_BYTES];
    int a, b;
    unsigned x, y;

    static uint8_t bank71[0x10000];
    if (tw != W || th != H)
        return;
    /* each pass from the same state: the HUD redraw's own, and the Super
       FX's RAM's second bank (the far records, which the far pass uses up),
       which the redraw does not put back */
    rm_hud_state(0);
    memcpy(bank71, nshvc_sram() + 0x10000, sizeof bank71);
    list_gen++;
    a = rm_tween_buffers(j, RM_TICKS, gsu_px[0], gsu_px[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    checking = 1;
    fw_drawing = 1;
    memset(tex, 0, (size_t)tw * th);
    list_gen++;
    b = rm_tween_buffers(j, RM_TICKS, skip[0], skip[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    fw_drawing = 0;
    span_flush();
    if (hud_n)                          /* a redraw let go before the HUD's end: its pieces */
        hud_place();
    fw_hud = 0;
    checking = 0;
    if (!a || a != b)
        return;
    if (!check_pictures++)
        atexit(check_report);
    for (y = 0; y < 128; y++)
        for (x = 0; x < 208; x++) {
            uint8_t g = gsu_pixel(gsu_px[a - 1], x, y), t = tex[y * (unsigned)tw + x];
            check_pixels++;
            if (g != t && ++check_differ <= 20)
                fprintf(stderr, "fwcheck: picture %ld (%d/6) at %u,%u: Super FX %02X, texels %02X\n", check_pictures, j, x, y, g, t);
        }
}

/* FW_KCHECK=1 (a check, at k > 1): each picture's 3D and HUD drawn again
   at k = 1 and the k picture's texel at each console pixel's centre held
   against it. Edges land a texel apart, so a few in a hundred differ; an
   object lost or misplaced at k shows as a picture far past that. Reported
   at exit: the pictures, the mean and the worst (frame, part differing). */
static long kc_pictures, kc_over;
static double kc_sum, kc_worst[5];
static uint32_t kc_worst_frame[5];

static void kcheck_report(void)
{
    int i;
    fprintf(stderr, "fwkcheck: %dx%d texels, %ld pictures, %.2f%% of pixels differ on average, %ld past 10%%; worst:", tw, th,
            kc_pictures, kc_pictures ? 100.0 * kc_sum / kc_pictures : 0.0, kc_over);
    for (i = 0; i < 5; i++)
        fprintf(stderr, " %.2f%% (frame %u)", 100.0 * kc_worst[i], kc_worst_frame[i]);
    fprintf(stderr, "\n");
}

static void kcheck_picture(int j)
{
    static uint8_t scratch[2][RM_TWEEN_BYTES], *one, bank71[0x10000];
    uint8_t *tex_k = tex;
    int tw_k = tw, th_k = th, x, y, i, m;
    long differ = 0;
    double part;

    if (tw == W && th == H)
        return;
    if (!one && !(one = malloc((size_t)W_MAX * H_MAX)))
        return;
    tex = one;
    SXN = SXD = tw = W;
    SYN = SYD = H;
    th = H;
    fw_drawing = 1;
    rm_hud_state(0);
    memcpy(bank71, nshvc_sram() + 0x10000, sizeof bank71);
    list_gen++;
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    fw_drawing = 0;
    span_flush();
    if (hud_n)                          /* a redraw let go before the HUD's end: its pieces */
        hud_place();
    fw_hud = 0;
    tex = tex_k;
    SXN = tw = tw_k;
    SYN = th_k;
    th = th_k;
    if (!m)
        return;
    if (!kc_pictures++)
        atexit(kcheck_report);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            differ += one[y * W + x] != block_get(x, y);
    part = (double)differ / (W * H);
    kc_sum += part;
    kc_over += part > 0.10;
    if (fw_env("FW_KDUMP") && nshvc_frames() == (uint32_t)atoi(fw_env("FW_KDUMP"))) {
        /* the two as pictures, k = 1 then k's centres: FW_KDUMP's frame, to FW_KDUMPF.1.ppm and .k.ppm */
        const uint16_t *cg = nshvc_cgram();
        char path[512];
        int which;
        for (which = 0; which < 2; which++) {
            FILE *f;
            snprintf(path, sizeof path, "%s.%s.ppm", fw_env("FW_KDUMPF"), which ? "k" : "1");
            if (!(f = fopen(path, "wb")))
                continue;
            fprintf(f, "P6\n%d %d\n255\n", W, H);
            for (y = 0; y < H; y++)
                for (x = 0; x < W; x++) {
                    uint16_t c = cg[which ? block_get(x, y) : one[y * W + x]];
                    fputc((c & 31) << 3, f);
                    fputc((c >> 5 & 31) << 3, f);
                    fputc((c >> 10 & 31) << 3, f);
                }
            fclose(f);
        }
    }
    for (i = 0; i < 5; i++)
        if (part > kc_worst[i]) {
            memmove(kc_worst + i + 1, kc_worst + i, (4 - i) * sizeof kc_worst[0]);
            memmove(kc_worst_frame + i + 1, kc_worst_frame + i, (4 - i) * sizeof kc_worst_frame[0]);
            kc_worst[i] = part;
            kc_worst_frame[i] = nshvc_frames();
            break;
        }
}

/* FW_CCHECK=M (a check): each picture's 3D drawn twice more from the same
   state, at this screen and at one M console pixels larger on every side
   with the same projection (the centre M lower and across), and the first
   compared with the larger one's middle, where each draws anything (any
   colour with FW_CCOLOUR; an affine texture shifts where a face is cut
   differently): an object or face the game
   leaves out near the screen's edges, which the larger one draws, shows
   as a difference (reported at exit; FW_CDUMPF + FW_CDUMP: a frame's
   pictures as PPMs) */
static long cc_pictures, cc_over;
static double cc_sum, cc_worst[5];
static uint32_t cc_worst_frame[5];

static void ccheck_report(void)
{
    fprintf(stderr, "fwccheck: %ld pictures, %.3f%% of texels differ on average, %ld past 0.5%%; worst: "
            "%.3f%% (frame %u) %.3f%% (frame %u) %.3f%% (frame %u) %.3f%% (frame %u) %.3f%% (frame %u)\n",
            cc_pictures, cc_pictures ? 100.0 * cc_sum / cc_pictures : 0.0, cc_over,
            100 * cc_worst[0], cc_worst_frame[0], 100 * cc_worst[1], cc_worst_frame[1], 100 * cc_worst[2],
            cc_worst_frame[2], 100 * cc_worst[3], cc_worst_frame[3], 100 * cc_worst[4], cc_worst_frame[4]);
}

static int ccheck_draw(int j, uint8_t *buf, int w, int h, int off)
{
    static uint8_t scratch[2][RM_TWEEN_BYTES], bank71[0x10000];
    uint8_t *tex_k = tex;
    int tw_k = tw, th_k = th, m;

    tex = buf;
    tw = w;
    th = h;
    cy_off = off;
    cropping = 1;
    memset(tex, 0, (size_t)tw * th);
    fw_drawing = 1;
    rm_hud_state(0);
    memcpy(bank71, nshvc_sram() + 0x10000, sizeof bank71);
    list_gen++;
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    fw_drawing = 0;
    span_flush();
    if (hud_n)                          /* a redraw let go before the HUD's end: its pieces */
        hud_place();
    fw_hud = 0;
    cropping = 0;
    cy_off = 0;
    tex = tex_k;
    tw = tw_k;
    th = th_k;
    return m;
}

static void ccheck_picture(int j)
{
    static uint8_t *a, *b;
    static size_t a_size, b_size;
    int mg = atoi(fw_env("FW_CCHECK")) * (int)((tw + W - 1) / W), bw = tw + 2 * mg, bh = th + 2 * mg, x, y, i;
    long differ = 0;
    double part;

    if (mg <= 0 || bw > 16384)
        return;
    if ((size_t)tw * th > a_size) {
        free(a);
        a = malloc(a_size = (size_t)tw * th);
    }
    if ((size_t)bw * bh > b_size) {
        free(b);
        b = malloc(b_size = (size_t)bw * bh);
    }
    if (!a || !b)
        return;
    if (!ccheck_draw(j, a, tw, th, 0) || !ccheck_draw(j, b, bw, bh, mg))
        return;
    if (!cc_pictures++)
        atexit(ccheck_report);
    for (y = 0; y < th; y++)
        for (x = 0; x < tw; x++)
            differ += fw_env("FW_CCOLOUR") ? a[(size_t)y * tw + x] != b[(size_t)(y + mg) * bw + x + mg]
                                           : !a[(size_t)y * tw + x] != !b[(size_t)(y + mg) * bw + x + mg];
    part = (double)differ / ((double)tw * th);
    cc_sum += part;
    cc_over += part > 0.005;
    if (fw_env("FW_CDUMP") && nshvc_frames() == (uint32_t)atoi(fw_env("FW_CDUMP"))) {
        const uint16_t *cg = nshvc_cgram();
        char path[512];
        int which;
        for (which = 0; which < 2; which++) {
            FILE *f;
            int ww = which ? bw : tw, hh = which ? bh : th;
            const uint8_t *p = which ? b : a;
            snprintf(path, sizeof path, "%s.%s.ppm", fw_env("FW_CDUMPF"), which ? "big" : "screen");
            if (!(f = fopen(path, "wb")))
                continue;
            fprintf(f, "P6\n%d %d\n255\n", ww, hh);
            for (y = 0; y < hh; y++)
                for (x = 0; x < ww; x++) {
                    uint16_t c = cg[p[(size_t)y * ww + x]];
                    fputc((c & 31) << 3, f);
                    fputc((c >> 5 & 31) << 3, f);
                    fputc((c >> 10 & 31) << 3, f);
                }
            fclose(f);
        }
    }
    for (i = 0; i < 5; i++)
        if (part > cc_worst[i]) {
            memmove(cc_worst + i + 1, cc_worst + i, (4 - i) * sizeof cc_worst[0]);
            memmove(cc_worst_frame + i + 1, cc_worst_frame + i, (4 - i) * sizeof cc_worst_frame[0]);
            cc_worst[i] = part;
            cc_worst_frame[i] = nshvc_frames();
            break;
        }
}

/* FW_PROF=1 (a check): each stage of the full window's pictures timed, the
   mean milliseconds a picture written to stderr each 120 */
enum { PR_REDRAW, PR_PANELS, PR_SPRITES, PR_HAND, PR_N };
static double prof_ms[PR_N];
static long prof_pics;
static double prof_t;

static double prof_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void prof_mark(int stage)
{
    double t;
    if (!fw_env("FW_PROF"))
        return;
    t = prof_now();
    if (stage >= 0)
        prof_ms[stage] += t - prof_t;
    prof_t = t;
    if (stage == PR_HAND && ++prof_pics == 120) {
        fprintf(stderr, "fwprof %dx%d texels ms a picture: redraw %.2f panels %.2f sprites %.2f hand %.2f\n",
                tw, th, prof_ms[0] / 120, prof_ms[1] / 120, prof_ms[2] / 120, prof_ms[3] / 120);
        memset(prof_ms, 0, sizeof prof_ms);
        prof_pics = 0;
    }
}

/* FW_PROF's frames: the time from one frame's end to the next's (the
   whole frame, the console's run and the present included) and the frame
   end's own work (rm_frame_end: the pictures drawn there), the mean and
   the most, each 120 frames */
void fw_prof_frame(int begin)
{
    static double last, start, gap_sum, gap_max, work_sum, work_max;
    static long n;
    double t;

    if (!fw_env("FW_PROF"))
        return;
    t = prof_now();
    if (begin) {
        if (last > 0) {
            double g = t - last;
            gap_sum += g;
            if (g > gap_max)
                gap_max = g;
        }
        last = start = t;
        return;
    }
    t -= start;
    work_sum += t;
    if (t > work_max)
        work_max = t;
    if (++n == 120) {
        fprintf(stderr, "fwprof frames: %.2f ms apart (most %.2f), the frame end's work %.2f (most %.2f), full window %d\n",
                gap_sum / 120, gap_max, work_sum / 120, work_max, on);
        gap_sum = gap_max = work_sum = work_max = 0;
        n = 0;
    }
}

int fw_picture(int j)
{
    static uint8_t scratch[2][RM_TWEEN_BYTES];
    int m;
    size_t i;

    if (!on)
        return 0;
    if (j == 0 && handed) {             /* the step's own screen stages may have copied the sprite table to OAM
                                           and flipped BG1's map and characters */
        nshvc_hold_interrupts(1);
        nshvc_set_cpu_costs(0);
        screen_set();
        nshvc_set_cpu_costs(1);
        nshvc_hold_interrupts(0);
    }
    if (fw_env("FW_CHECK"))
        check_picture(j);
    prof_mark(-1);
    fw_water[3] = 0;                    /* no water until this picture's view says so (water_view) */
    bit4 = two;                         /* (its bitmap's 4 bits a pixel) */
    if (gpu_pic && gpu_open)            /* (a last picture never handed: its texels kept) */
#ifdef SRFX_NSHVC_HANDOFF
        fw_canvas_gpu->close(NULL, hand_pic() ? NULL : tex);
#else
        fw_canvas_gpu->close(NULL, tex);
#endif
    gpu_pic = fw_canvas_gpu &&!fw_env("FW_CHECK") && !fw_env("FW_KCHECK") && !fw_env("FW_CCHECK") && !fw_env("FW_WATERCHECK");
    gpu_open = gpu_cleared = 0;
    fw_drawing = 1;
    list_gen++;
    sc_picture_begin();                 /* the GPU's display list (scene.h) */
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    fw_drawing = 0;
#ifdef SRFX_NSHVC_HANDOFF
    /* a redraw refused, nothing drawn: the canvas not opened, the GE's last picture
       standing whole in its buffer, to be handed again (fw_frame_end) */
    if (!m && !nspans && !gpu_open && !hud_n && hand_pic()) {
        fw_hud = 0;
        return 0;
    }
#endif
    span_flush();
    if (hud_n)                          /* a redraw let go before the HUD's end: its pieces */
        hud_place();
    fw_hud = 0;
    bit4 = 0;
    fw_clip[2] = fw_clip[3] = -1;       /* the panels, over the whole picture */
    if (!m)
        return 0;
    if (fw_env("FW_KCHECK"))            /* before the backdrop and panels: the 3D and HUD alone */
        kcheck_picture(j);
    if (fw_env("FW_CCHECK"))
        ccheck_picture(j);
    prof_mark(PR_REDRAW);
    panels_draw();
    prof_mark(PR_PANELS);
    nshvc_hold_interrupts(1);
    nshvc_set_cpu_costs(0);
    sprites_place();                    /* the panels' boxes found: the sprites in them with them */
    nshvc_set_cpu_costs(1);
    nshvc_hold_interrupts(0);
    prof_mark(PR_SPRITES);
    hand();
    prof_mark(PR_HAND);
    return 1;
}

/* ---- the bitmap pictures (fullwindow.h) ------------------------------------- */

/* the texels' memory for this many, as size_for_window keeps it */
static int tex_room(size_t need)
{
    if (need > tex_size) {
        if (fw_texels_alloc) {
            tex = fw_texels_alloc(need);
        } else {
            free(tex);
            tex = malloc(need);
        }
        tex_size = tex ? need : 0;
    }
    return tex != NULL && tex_size >= need;
}

/* the switches a check of the full window's turns these off with */
static int bp_checks(void)
{
    static int off = -1;
    if (off < 0)
        off = getenv("SC_CHECK") || getenv("FW_NOQUADS") || getenv("FW_QUADCHECK") || getenv("FW_GPUCHECK") ||
              getenv("FW_CHECK") || getenv("FW_KCHECK") || getenv("FW_CCHECK") || getenv("FW_WATERCHECK");
    return off;
}

/* a GPU to draw them on: its fill in place, or its canvas */
static int gpu_here(void) { return fw_span_filler != fw_span_fill_cpu || fw_canvas_gpu != NULL; }

#ifdef SRFX_NSHVC_HANDOFF
/* the full window's picture handed as the GPU's own: SC_GPU, the contract there,
   no check's (a check reads the texels) */
static int hand_pic(void)
{
    static int recording = -1;
    if (recording < 0)
        recording = fw_env("FW_SPANREC") || fw_env("FW_WATERREC");
    return on && !bp && !gx && !checking && !recording && fw_handoff && sc_gpu_on() && !bp_checks() && fw_handoff->ready();
}

int fw_chars_on(void)
{
    return fw_handoff && fw_handoff->chars_cut && sc_gpu_on() && !bp_checks() && fw_handoff->ready();
}

void fw_cpu_fallback(const char *what)
{
    static int check = -1;
    if (!fw_gpu_settled || !sc_gpu_on())
        return;
    if (check < 0)
        check = bp_checks() || fw_env("FW_CPUFILL") || fw_env("FW_SPANPLAY") || fw_env("FW_WATERPLAY");
    if (!check)
        nshvc_fatal("SC_GPU: %s would go to the CPU", what);
}
#endif

int fw_bp_on(void) { return sc_gpu_on() && !on && !bp_checks() && gpu_here() && rm_canvas_ready(); }

/* a bitmap picture begins: its shape times k, the texels and the canvas's room */
static int bp_open(int cols, int rows, int bpp, int targets)
{
    int k = CANVAS_SCALE();
    if (k < 1 || !tex_room((size_t)cols * 8 * k * rows * 8 * k) ||
        !rm_canvas_gpu_begin((unsigned)(cols * rows * targets), (unsigned)bpp, k)) {
        if (k >= 1)
            FW_CPU_FALLBACK("a bitmap picture with no memory");
        return 0;
    }
    SXN = SYN = k;
    SXD = SYD = 1;
    tw = cols * 8 * k;
    th = rows * 8 * k;
    scale_lost = 1;
    view_set = 0;
    bp = 1;
    bit4 = bpp < 8;
    bp_cols = cols;
    bp_rows = rows;
    bp_bpp = bpp;
    bp_targets = targets;
    bp_done = bp_open_now = 0;
    nspans = npolys = nedges = 0;
    gpu_pic = fw_canvas_gpu != NULL;
    gpu_open = gpu_cleared = 0;
    fw_water[3] = 0;
    return 1;
}

/* the target drawn: into the texels, cut into its characters */
static void bp_finish(void)
{
    if (!bp_open_now)
        return;
    bp_open_now = 0;
    span_flush();
    if (gpu_pic && gpu_open)
#ifdef SRFX_NSHVC_HANDOFF
        fw_canvas_gpu->close(NULL, fw_chars_on() ? NULL : tex);
#else
        fw_canvas_gpu->close(NULL, tex);
#endif
    gpu_open = gpu_cleared = 0;
    rm_canvas_gpu_cut((unsigned)(bp_done * bp_cols * bp_rows), (unsigned)bp_cols, (unsigned)bp_rows, tex, tw,
                      bp_bpp < 8 ? 0x0Fu : 0xFFu);
    bp_done++;
}

int fw_bp_frame(int begin)
{
    uint8_t *s;
    int i;
    if (two && fw_drawing && begin) {
        two_begin();
        return 0;
    }
    if (!bp)
        return 0;
    s = nshvc_sram();
    if (!begin) {                       /* the game's words back, for what the programs after it read */
        for (i = 0; i < 8; i++) {
            s[view_at[i]] = (uint8_t)bp_game[i];
            s[view_at[i] + 1] = (uint8_t)(bp_game[i] >> 8);
        }
        view_set = 0;
        return 1;
    }
    bp_finish();
    sc_picture_begin();
    for (i = 0; i < 8; i++)
        bp_game[i] = (uint16_t)(s[view_at[i]] | s[view_at[i] + 1] << 8);
    {
        /* the game's view times k: the right edge its last column, the bottom
           one past its last row; the planes' slopes the same */
        int32_t k = (int32_t)SXN, full[8];
        full[0] = (int16_t)bp_game[0] * k;
        full[1] = (int16_t)bp_game[1] * k;
        full[2] = (int16_t)bp_game[2] * k;
        full[3] = ((int16_t)bp_game[3] + 1) * k - 1;
        full[4] = (int16_t)bp_game[4] * k;
        full[5] = (int16_t)bp_game[5] * k;
        full[6] = bp_game[6];
        full[7] = bp_game[7];
        bp_cx = full[0];
        bp_cy = (int16_t)bp_game[1];
        for (i = 0; i < 8; i++) {
            s[view_at[i]] = (uint8_t)full[i];
            s[view_at[i] + 1] = (uint8_t)(full[i] >> 8);
        }
    }
    view_set = 1;
    bp_open_now = 1;
    return 1;
}

int fw_bp_picture(int j)
{
    static uint8_t scratch[2][RM_TWEEN_BYTES];
    const uint8_t *w = nshvc_wram();
    int two = w[0x0D62] != 0, m;

    if (!(two ? bp_open(18, 10, 4, 2) : bp_open(26, 16, 8, 1)))
        return 0;
    fw_drawing = 1;
    list_gen++;
    sc_picture_begin();
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    fw_drawing = 0;
    fw_hud = 0;
    if (m)
        bp_finish();
    bp_open_now = 0;
    bp = bit4 = 0;
    view_set = 0;
    gpu_pic = gpu_open = 0;
    if (!m || bp_done != bp_targets)
        return 0;
    rm_canvas_gpu_show(two ? -1 : (w[0x0D81] | w[0x0D82] << 8) == 0x0001 ? 0x2200 : 0x0000);
    still_word = -1;
    return m;
}

void fw_menu_picture(int on_) { menu_due = on_ && fw_bp_on() && !fw_drawing; }

/* The menus' 3D frame: no redraw of a tween's (its step is a video frame's):
   its program's list made natively as it starts, the chip's RAM and registers
   put back for the chip's own run beside it (fw_silent, the picture drawn), the
   picture kept for the flip. */
void fw_menu_frame(void)
{
    static uint8_t keep[0x20000];
    RegsGSU kept;
    uint8_t *s;
    size_t size = (size_t)ram_mask + 1;

    if (!menu_due)
        return;
    menu_due = 0;
    unplotted = 0;
    if (!bp_open(13, 11, 8, 1))
        return;
    s = nshvc_sram();
    if (size > sizeof keep)
        size = sizeof keep;
    memcpy(keep, s, size);
    kept = gsu;
    fw_drawing = 1;
    list_gen++;
    sc_picture_begin();
    sc_frame_native();
    fw_drawing = 0;
    bp_finish();
    bp = bit4 = 0;
    view_set = 0;
    gpu_pic = gpu_open = 0;
    memcpy(s, keep, size);
    gsu = kept;
    if (bp_done == bp_targets) {
        rm_canvas_gpu_ready();
        fw_silent = unplotted = 1;
    }
}

int fw_haze(uint16_t x, uint16_t y, uint8_t time)
{
    FwSpan *p;
    if (!fw_drawing || checking || cropping || !sc_gpu_on() || !(p = span_new()))
        return 0;
    memset(p, 0, sizeof *p);
    p->textured = FW_HAZE;
    p->x0 = 1;
    p->pad0 = (int16_t)y;
    p->xl = (int16_t)x;
    p->base = time;
    p->u = SXN;
    p->du = SXD;
    p->v = SYN;
    p->dv = SYD;
    p->pad1[2] = (uint8_t)gx;
    if (gx)
        job.plots += 256;
    return 1;
}

int fw_noise(uint16_t cols, uint16_t rows, uint16_t *seed, uint16_t ramp, uint16_t *last)
{
    FwSpan *p;
    uint64_t bytes = 0;
    uint16_t s = *seed;
    uint32_t i, n = (uint32_t)cols * rows;

    if (!gx || !n || !(p = span_new()))
        return 0;
    for (i = 0; i < 8; i++)
        bytes |= (uint64_t)gsu_peek8(gsu_ram((uint16_t)(ramp + 2 * i))) << (i * 8);
    memset(p, 0, sizeof *p);
    p->textured = FW_NOISE;
    p->x0 = 1;
    p->xl = cols;
    p->v = rows;
    p->base = s;
    p->u = (int64_t)bytes;
    p->por = (uint8_t)(bit4 ? 8 : 0);
    job.quad += n;
    for (i = 0; i < n; i++)             /* the generator's own state, the game's: no pixel of it */
        s = fx_noise_step(s);
    *seed = s;
    i = (uint32_t)(s & 7) * 2;
    *last = (uint16_t)(gsu_peek8(gsu_ram((uint16_t)(ramp + i))) | gsu_peek8(gsu_ram((uint16_t)(ramp + i + 1))) << 8);
    return 1;
}

int fw_job_clear(uint16_t at, uint32_t bytes)
{
    uint8_t *ram;
    uint32_t a, off, char_bytes, col_chars, cn, n, i;

    if (!gx)
        return 0;
    a = gsu_ram(at) & gx_shape.mask;
    char_bytes = (uint32_t)gx_shape.bpp * 8;
    col_chars = (uint32_t)gx_shape.h / 8;
    off = a - gx_shape.base;
    if (gx_shape.ht == 3 || a < gx_shape.base || off % char_bytes || bytes % char_bytes) {
        /* (no such start or layout in the game, GsuScreenFlip and SCMR's writes:) the chip's stores over what is drawn */
        gx_sync();
        gx_valid = 0;
        return 0;
    }
    ram = nshvc_sram();
    cn = off / char_bytes;
    n = bytes / char_bytes;
    while (n) {
        uint32_t cx = cn / col_chars, cy = cn % col_chars, cols = 1, run = col_chars - cy;
        if (cx >= (uint32_t)gx_shape.w / 8) {
            /* past the target: no PLOT reaches these bytes, the RAM's alone */
            uint32_t from = gx_shape.base + cn * char_bytes, count = n * char_bytes;
            for (i = 0; i < count; i++)
                ram[(from + i) & gx_shape.mask] = 0;
            break;
        }
        if (cy == 0 && n >= col_chars) {    /* whole columns, one quad */
            cols = n / col_chars;
            if (cx + cols > (uint32_t)gx_shape.w / 8)
                cols = (uint32_t)gx_shape.w / 8 - cx;
        } else if (run > n) {
            run = n;
        }
        {
            uint64_t q = job.quad;
            FwSpan *p = quad_new((int32_t)cy * 8, (int32_t)run * 8, (int32_t)cx * 8, (int32_t)(cx + cols) * 8 - 1, 0, 0);
            if (p) {
                p->pad1[1] = 2;
                p->colour = 0;
                p->xl = (int32_t)cx * 8;
            }
            job.quad = q;
        }
        cn += cols * run;
        n -= cols * run;
    }
    job.stores += bytes;
    return 1;
}

/* the frame hook, the full window having given way: the GPU's bitmap picture
   in its place, drawn from the step's start (j 0) as it stands; every byte and
   register put back (rm_tween_buffers). A few frames tried: it is refused while
   an upload has the chip's RAM part way. */
static void still_try(void)
{
    if (still_word < 0)
        return;
    if (on || !fw_bp_on() || ++still_tries > 8) {
        still_word = -1;
        return;
    }
    nshvc_set_cpu_costs(0);
    rm_tween_still = 1;
    fw_bp_picture(0);                   /* (still_word -1 when it stands) */
    rm_tween_still = 0;
    nshvc_set_cpu_costs(1);
}

/* ---- a job of Original's (and of Enhanced's, its bitmap the one shown) ---- */
static void gx_begin(void)
{
    /* no access of the chip's: the registers as they stand, only under SC_GPU */
    int md, ht;
    static int no = -1;
    job.on = 0;
    if (fw_drawing || !sc_gpu_on())
        return;
#ifdef SRFX_NSHVC_HANDOFF
    /* (not kept: on Metal and Vulkan the GPU opens at the first frame hook) */
    if (no < 0)
        no = bp_checks();
    if (no)
        return;
    if (!gpu_here()) {
        FW_CPU_FALLBACK("a job");
        return;
    }
#else
    if (no < 0)
        no = bp_checks() || !gpu_here();
    if (no)
        return;
#endif
    if (rm_enhanced()) {
        if (menu_due)                   /* fw_menu_frame's to say, as it starts */
            return;
        /* a bitmap nobody shows: the full window's up (race_flip, its giving way:
           still_try), or a step's with the GPU's bitmap picture on the canvas */
        if (on || (rm_tick60() && rm_whole_steps() && rm_canvas_ready() &&
                   rm_canvas_gpu_up(nshvc_wram()[0x0D62] ? 360 : 416))) {
            fw_silent = unplotted = 1;
            return;
        }
    }
    md = gsu.scmr & 3;
    ht = ((gsu.scmr >> 2) & 1) | ((gsu.scmr >> 4) & 2);
    gx_shape.w = 264;
    gx_shape.h = ht == 0 ? 128 : ht == 1 ? 160 : ht == 2 ? 192 : 256;
    gx_shape.bpp = 2 << (md - (md >> 1));
    gx_shape.ht = ht;
    gx_shape.base = (uint32_t)gsu.scbr << 10;
    gx_shape.mask = ram_mask;
    if (!tex_room((size_t)gx_shape.w * gx_shape.h)) {
        FW_CPU_FALLBACK("a job with no memory");
        unplotted = 0;
        return;
    }
    tw = gx_shape.w;
    th = gx_shape.h;
    SXN = SXD = SYN = SYD = 1;
    scale_lost = 1;
    view_set = 0;
    memset(&job, 0, sizeof job);
    job.on = 1;
    gx = 1;
    gx_valid = 0;
    bit4 = md != 3;
    nspans = npolys = nedges = 0;
    gpu_pic = fw_canvas_gpu != NULL;
    gpu_open = gpu_cleared = 0;
    fw_water[3] = 0;
    fw_drawing = 1;
}

/* what is drawn so far into the chip's RAM */
static void gx_sync(void)
{
    if (!nspans)
        return;
    unplotted = 0;
    span_flush();
    if (gpu_pic && gpu_open) {
        fw_canvas_gpu->close(NULL, tex);
        gpu_open = 0;
    }
    fw_bits_filler(tex, nshvc_sram(), &gx_shape, 1);
}

static void gx_end(void)
{
    fw_silent = 0;
    if (!gx)
        return;
    gx_sync();
    gx = 0;
    bit4 = 0;
    fw_drawing = 0;
    gpu_pic = gpu_open = 0;
}

/* The chip's bitplanes and the linear target (w x h), the address as PLOT's
   (superfx.c char_addr): pack, each character from the target; else each
   pixel of the target from its character. The kernels' the same. */
void fw_bits_cpu(uint8_t *t, uint8_t *ram, const FwBits *b, int pack)
{
    int cx, cy, y, x, n, bytes = b->bpp * 8;
    for (cx = 0; cx < b->w / 8; cx++)
        for (cy = 0; cy < b->h / 8; cy++) {
            uint32_t cn, X = (uint32_t)cx * 8, Y = (uint32_t)cy * 8, at;
            switch (b->ht) {
            case 0: cn = (uint32_t)cx * 16 + (uint32_t)cy; break;
            case 1: cn = (uint32_t)cx * 20 + (uint32_t)cy; break;
            case 2: cn = (uint32_t)cx * 24 + (uint32_t)cy; break;
            default: cn = ((Y & 0x80u) << 2) + ((X & 0x80u) << 1) + ((Y & 0x78u) << 1) + ((X & 0x78u) >> 3); break;
            }
            at = b->base + cn * (uint32_t)bytes;
            for (y = 0; y < 8; y++) {
                uint8_t *row = t + (size_t)(cy * 8 + y) * b->w + cx * 8;
                if (pack)
                    for (n = 0; n < b->bpp; n++) {
                        uint8_t d = 0;
                        for (x = 0; x < 8; x++)
                            d |= (uint8_t)(((row[x] >> n) & 1) << (7 - x));
                        ram[(at + (uint32_t)((n >> 1) * 16 + y * 2 + (n & 1))) & b->mask] = d;
                    }
                else
                    for (x = 0; x < 8; x++) {
                        uint8_t v = 0;
                        for (n = 0; n < b->bpp; n++)
                            v |= (uint8_t)(((ram[(at + (uint32_t)((n >> 1) * 16 + y * 2 + (n & 1))) & b->mask] >> (7 - x)) & 1) << n);
                        row[x] = v;
                    }
            }
        }
}

/* ---- the panels ------------------------------------------------------------ */

/* the original's BG1 pixel at (x, y) of the picture: its map and
   characters as the last flip set them ($0D81: -1 buffer A, map $7800 and
   characters from word 0; 1 buffer B, $7C00 and $2000), under the race's
   scroll (RACE_ROW0) */
static uint8_t panel_pixel(unsigned x, unsigned line)
{
    const uint16_t *v = nshvc_vram();
    const uint8_t *w = nshvc_wram();
    int b = (w[0x0D81] | w[0x0D82] << 8) == 0x0001;
    unsigned map = b ? 0x7C00 : 0x7800, base = b ? 0x2000 : 0x0000;
    unsigned row = (line - RACE_ROW0) & 0xFF, n;
    uint16_t e = v[map + (row >> 3) * 32 + (x >> 3)];
    unsigned px = x & 7, py = row & 7, c = 0;
    const uint16_t *ch;

    if (e & 0x4000)
        px = 7 - px;
    if (e & 0x8000)
        py = 7 - py;
    ch = v + ((base + (e & 0x3FF) * 32) & 0x7FFF);
    for (n = 0; n < 8; n++) {
        uint16_t word = ch[(n >> 1) * 8 + py];
        c |= (unsigned)((word >> ((n & 1) * 8 + 7 - px)) & 1) << n;
    }
    return (uint8_t)c;
}

/* The panels: the band of the original's screen under the view, lines
   PANEL_TOP to PANEL_END, drawn over the 3D at the console's pixel size
   (at the bottom edge of a taller screen),
   less the bezel's background around the panels' boxes: the pixels of its
   colour (the band's left edge's) reached from the band's edges, the
   boxes' outlines stopping it */
#define PANEL_TOP 161
#define PANEL_END 208
#define PANEL_ROWS (PANEL_END - PANEL_TOP)
/* the boxes found, their columns and shift, for the sprites in them */
#define MAX_BOXES 16
static int nboxes, box_x0[MAX_BOXES], box_x1[MAX_BOXES], box_shift[MAX_BOXES];
/* each box's rows, and whether its pixels are in its texture (panel_bank:
   box k in bank $F0 + k / 5 from (k % 5) * PANEL_ROWS rows, 256 bytes a
   row, its pixels' colours, 0 elsewhere) */
static int box_y0[MAX_BOXES], box_y1[MAX_BOXES], box_tex[MAX_BOXES];

/* the panels' quads for the GPU (SC_GPU), the picture's own (not a
   check's second pass) */
static int fw_drawing_quads(void) { return sc_gpu_on() && !checking && !cropping && !fw_env("FW_NOQUADS"); }

/* what the band's pixels are made from: each line's map entries and their
   characters in the buffer the flip shows (a hash of them); the boxes
   made again only when it changes */
static uint64_t panel_key(void)
{
    const uint16_t *v = nshvc_vram();
    const uint8_t *w = nshvc_wram();
    int b = (w[0x0D81] | w[0x0D82] << 8) == 0x0001;
    unsigned map = b ? 0x7C00 : 0x7800, base = b ? 0x2000 : 0x0000, r, x, k;
    uint64_t h = 1469598103934665603ull;   /* (the content, whichever buffer holds it) */
    for (r = ((PANEL_TOP - RACE_ROW0) & 0xFF) >> 3; r <= ((PANEL_END - 1 - RACE_ROW0) & 0xFF) >> 3; r++)
        for (x = 0; x < 32; x++) {
            uint16_t e = v[map + r * 32 + x];
            const uint16_t *ch = v + ((base + (e & 0x3FF) * 32) & 0x7FFF);
            h = (h ^ e) * 1099511628211ull;
            for (k = 0; k < 32; k++)
                h = (h ^ ch[k]) * 1099511628211ull;
        }
    return h;
}

/* a box as a quad for the GPU (SC_GPU): its texture at the screen's
   place block_set's (a + s, the band's line + top) */
static void box_quad(int k, int top)
{
    int s = box_shift[k];
    int32_t c0 = col_at(box_x0[k] + s), c1 = col_at(box_x1[k] + 1 + s) - 1;
    int32_t r0 = row_at(top + box_y0[k]), r1 = row_at(top + box_y1[k] + 1) - 1;
    FwSpan *p = quad_new(r0, r1 - r0 + 1, c0, c1, (int64_t)(r0 + 1) * SYD - 1 - (int64_t)top * SYN, SYD);
    uint32_t den = (uint32_t)SXN, zero = 0;
    if (!p)
        return;
    p->xl = 0;
    p->u = SXD - 1 - (int64_t)s * SXN;
    p->du = SXD;
    p->mask = (uint16_t)SYN;
    p->bank = (uint8_t)(0xF0 + k / 5);
    p->base = (uint16_t)((k % 5) * PANEL_ROWS * 256);
    p->pad1[1] = 8;
    memcpy(&p->pq, &den, 4);
    memcpy(&p->pdq, &zero, 4);
    memcpy(&p->pda, &zero, 4);
    p->por = 0;                         /* colour 0 not drawn: a box's own pixels alone */
    p->hi = 0;
}

/* Two players' boxes: each player's DAMAGE, map, speed and time as the original's
   screen has them beside its view, BG1's map $1C00 (a view's rows 2 to 13 of it, 12
   rows of 10 characters, 96 x 80 pixels; the second's rows 15 to 26), less the
   bezel's colour reached from their edges, a texture ($F0, $F1), each box at its
   own place (two_box_place). */
static const int two_bx[2] = { 168, 8 }, two_by[2] = { 16, 120 };

static uint8_t two_pixel(unsigned x, unsigned y)
{
    const uint16_t *v = nshvc_vram();
    uint16_t e = v[0x1C00 + (y >> 3) * 32 + (x >> 3)];
    unsigned px = e & 0x4000 ? 7 - (x & 7) : x & 7, py = e & 0x8000 ? 7 - (y & 7) : y & 7, n, c = 0;
    const uint16_t *ch = v + ((((unsigned)mw_ppu_last(0x210B) & 0x0F) * 0x1000 + (e & 0x3FFu) * 16) & 0x7FFF);

    for (n = 0; n < 4; n++)
        c |= (unsigned)(ch[(n >> 1) * 8 + py] >> ((n & 1) * 8 + 7 - px) & 1) << n;
    return c ? (uint8_t)((e >> 10 & 7) << 4 | c) : 0;
}

/* the sprites in player p's boxes (the HUD's, priority 3: its digits, gauge and map)
   into their picture, scaled with it, each pixel its CGRAM index ($80 up), the first
   sprite on top as the console's (OBSEL $03: characters from $6000) */
static void two_box_sprites(int p, uint8_t *img)
{
    const uint8_t *o = nshvc_wram() + 0x0375;
    const uint16_t *v = nshvc_vram();
    int i;

    for (i = p ? 127 : 63; i >= (p ? 64 : 0); i--) {
        unsigned yy = o[i * 4 + 1], at = o[i * 4 + 3], bits = o[512 + i / 4] >> (2 * (i & 3));
        unsigned name = o[i * 4 + 2] | (at & 1) << 8;
        int h = bits & 2 ? 16 : 8, xx = o[i * 4] | (bits & 1) << 8, tx, ty, mapped;
        if (xx >= 256)
            xx -= 512;
        {                               /* (its middle in the map's place: the map's rows of the texture) */
            int mx = xx + h / 2 - two_bx[p], my = (int)yy + h / 2 - two_by[p];
            mapped = mx >= two_sx[p][1] && mx < two_sx[p][1] + two_sw[1] && my >= two_sy[1] && my < two_sy[1] + TWO_BOX_ROW;
        }
        if ((at & 0x30) != 0x30 || yy >= 224 || yy + (unsigned)h > 256 || xx + h / 2 < two_bx[p] || xx + h / 2 >= two_bx[p] + TWO_BOX_W)
            continue;
        for (ty = 0; ty < h; ty++)
            for (tx = 0; tx < h; tx++) {
                int sx = at & 0x40 ? h - 1 - tx : tx, sy = at & 0x80 ? h - 1 - ty : ty;
                int lx = xx + tx - two_bx[p], ly = (int)yy + ty - two_by[p], px = 7 - (sx & 7), r = sy & 7, n;
                unsigned ch = (name & 0x100) | ((name + (unsigned)(sy >> 3) * 16) & 0xF0) | ((name + (unsigned)(sx >> 3)) & 0x0F);
                const uint16_t *c = v + ((0x6000 + ch * 16) & 0x7FFF);
                if (lx < 0 || lx >= TWO_BOX_W || ly < 0 || ly >= TWO_BOX_H || (mapped && (ly < TWO_MAP_TOP || ly >= TWO_MAP_TOP + TWO_MAP_H)))
                    continue;
                n = (c[r] >> px & 1) | (c[r] >> (8 + px) & 1) << 1 | (c[8 + r] >> px & 1) << 2 | (c[8 + r] >> (8 + px) & 1) << 3;
                if (n)
                    img[(mapped ? ly - TWO_MAP_TOP + TWO_MAP_Y : ly) * 256 + lx] = (uint8_t)(0x80 | (at >> 1 & 7) << 4 | n);
            }
    }
}

static void panels_two(void)
{
    static uint8_t img[0x10000], keep[TWO_BOX_H][TWO_BOX_W];
    static int16_t stack[TWO_BOX_H * TWO_BOX_W][2];
    uint8_t bg = two_pixel(21 * 8 + 4, 7 * 8 + 4);  /* the bezel's fill: its character under the map (map row 7, column 21) */
    int p, x, y, n, k, dx, dy;

    for (p = 0; p < 2; p++) {
        n = 0;
        memset(img, 0, sizeof img);
        for (y = 0; y < TWO_BOX_H; y++)
            for (x = 0; x < TWO_BOX_W; x++) {
                uint8_t c = two_pixel((unsigned)(two_bx[p] + x), (unsigned)(two_by[p] + y));
                img[y * 256 + x] = c;
                keep[y][x] = c != 0 && y && y != TWO_BOX_H - 1;   /* (its first and last rows the bezel's rim) */
            }
        /* floating, as one player's: only the boxes kept, the fill gone wherever it
           is reached from outside their outlines (the map's whole place is fill) */
        for (y = 0; y < TWO_BOX_H; y++)
            for (x = 0; x < TWO_BOX_W; x++)
                if ((!x || x == TWO_BOX_W - 1 || y == 1 || y == TWO_BOX_H - 2 ||
                     ((p ? x / 8 >= 6 : x / 8 < 4) && y / 8 >= 4 && y / 8 < 8)) &&
                    img[y * 256 + x] == bg && keep[y][x]) {
                    keep[y][x] = 0;
                    stack[n][0] = (int16_t)x;
                    stack[n++][1] = (int16_t)y;
                }
        while (n) {
            static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            n--;
            x = stack[n][0];
            y = stack[n][1];
            for (k = 0; k < 4; k++) {
                int a = x + d[k][0], b = y + d[k][1];
                if (a < 0 || a >= TWO_BOX_W || b < 0 || b >= TWO_BOX_H || !keep[b][a] || img[b * 256 + a] != bg)
                    continue;
                keep[b][a] = 0;
                stack[n][0] = (int16_t)a;
                stack[n++][1] = (int16_t)b;
            }
        }
        for (y = 0; y < TWO_BOX_H; y++)
            for (x = 0; x < TWO_BOX_W; x++)
                if (!keep[y][x])
                    img[y * 256 + x] = 0;
        two_box_sprites(p, img);
        if (memcmp(img, panel_bank[p], sizeof img)) {
            memcpy(panel_bank[p], img, sizeof img);
            panel_version[p]++;
        }
        for (k = 0; k < 4; k++) {
            int bw, bh;
            two_box_place(p, k, &dx, &dy, &bw, &bh);
            bank_quad_scaled(0xF0 + p, two_sx[p][k], k == 1 ? TWO_MAP_Y : two_sy[k], two_sw[k], k == 1 ? TWO_MAP_H : TWO_BOX_ROW, dx, dy, bw, bh);
        }
    }
}

static void panels_draw(void)
{
    static uint8_t px[PANEL_ROWS][256], keep[PANEL_ROWS][256];
    static int16_t stack[PANEL_ROWS * 256][2];
    static uint64_t key_was;
    static int known;
    int x, y, n = 0, top = PANEL_TOP + H - 224 - safe_px[3], k, quads = fw_drawing_quads(), both = quads && quads_both();
    uint64_t key;
    uint8_t bg;

    if (two) {
        panels_two();
        return;
    }
    /* the boxes as before while the band is (the GPU's: their textures and
       quads; the reference's, a check's, its rows again) */
    key = quads ? panel_key() : 0;
    if (quads && known && key == key_was && !both) {
        for (k = 0; k < nboxes; k++) {
            box_shift[k] = anchor_shift(box_x0[k], box_x1[k]);
            if (box_tex[k])
                box_quad(k, top);
        }
        return;
    }
    known = quads;
    key_was = key;
    for (y = 0; y < PANEL_ROWS; y++)
        for (x = 0; x < 256; x++) {
            px[y][x] = panel_pixel((unsigned)x, (unsigned)(PANEL_TOP + y));
            keep[y][x] = px[y][x] != 0;
        }
    bg = px[PANEL_ROWS / 2][0];
    for (y = 0; y < PANEL_ROWS; y++)
        for (x = 0; x < 256; x++)
            if ((x == 0 || x == 255 || y == 0 || y == PANEL_ROWS - 1) && px[y][x] == bg && keep[y][x]) {
                keep[y][x] = 0;
                stack[n][0] = (int16_t)x;
                stack[n++][1] = (int16_t)y;
            }
    while (n) {
        static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        int k;
        n--;
        x = stack[n][0];
        y = stack[n][1];
        for (k = 0; k < 4; k++) {
            int a = x + d[k][0], b = y + d[k][1];
            if (a < 0 || a > 255 || b < 0 || b >= PANEL_ROWS || !keep[b][a] || px[b][a] != bg)
                continue;
            keep[b][a] = 0;
            stack[n][0] = (int16_t)a;
            stack[n++][1] = (int16_t)b;
        }
    }
    /* each box (the kept pixels joined) by its columns' anchor; what
       reaches the band's sides is the bezel's (its outline under the view),
       not a box */
    nboxes = 0;
    if (quads)
        for (k = 0; k < 4; k++) {
            memset(panel_bank[k], 0, sizeof panel_bank[k]);
            panel_version[k]++;
        }
    for (y = 0; y < PANEL_ROWS; y++)
        for (x = 0; x < 256; x++) {
            int x0 = x, x1 = x, y0 = y, y1 = y, edge = 0, s, m = 0, q;
            static int16_t part[PANEL_ROWS * 256][2];
            if (keep[y][x] != 1)
                continue;
            keep[y][x] = 2;
            stack[0][0] = (int16_t)x;
            stack[0][1] = (int16_t)y;
            n = 1;
            while (n) {
                static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                int k, a0, b0;
                n--;
                a0 = stack[n][0];
                b0 = stack[n][1];
                part[m][0] = (int16_t)a0;
                part[m++][1] = (int16_t)b0;
                if (a0 < x0) x0 = a0;
                if (a0 > x1) x1 = a0;
                if (b0 < y0) y0 = b0;
                if (b0 > y1) y1 = b0;
                if (a0 == 0 || a0 == 255)
                    edge = 1;
                for (k = 0; k < 4; k++) {
                    int a = a0 + d[k][0], b = b0 + d[k][1];
                    if (a < 0 || a > 255 || b < 0 || b >= PANEL_ROWS || keep[b][a] != 1)
                        continue;
                    keep[b][a] = 2;
                    stack[n][0] = (int16_t)a;
                    stack[n++][1] = (int16_t)b;
                }
            }
            if (edge)
                continue;
            s = anchor_shift(x0, x1);
            k = -1;
            if (nboxes < MAX_BOXES) {
                k = nboxes++;
                box_x0[k] = x0;
                box_x1[k] = x1;
                box_y0[k] = y0;
                box_y1[k] = y1;
                box_shift[k] = s;
                box_tex[k] = quads;
                if (quads) {            /* its pixels into its texture, a quad for the GPU */
                    uint8_t *t = panel_bank[k / 5] + (k % 5) * PANEL_ROWS * 256;
                    for (q = 0; q < m; q++)
                        t[part[q][1] * 256 + part[q][0]] = px[part[q][1]][part[q][0]];
                    box_quad(k, top);
                    if (!both)
                        continue;
                }
            }
            /* the box's rows: the CPU's (a check's reference beside its
               quad; a box past MAX_BOXES, no quad, its own) */
            span_ref_only = both && k >= 0;
            for (q = 0; q < m; q++) {
                int a = part[q][0], b = part[q][1];
                /* the band moved up by the safe area's bottom */
                block_set(a + s, top + b, px[b][a]);
            }
            span_ref_only = 0;
        }
}

/* ---- the screen ------------------------------------------------------------ */

/* The sprites: the game's table (work RAM $0375: 512 bytes, then the 32
   of each sprite's X bit 8 and size). In the full window the console draws
   them, by its own order and priorities against BG1 and BG2, each placed
   on the larger screen (sprites_place: OAM, and nshvc_ext_sprite_page for
   the columns and rows past the console's). The sprites the original's screen kept out of sight
   are left out: the bezel's sides (characters $40-$47, palette 3) and its
   start lamps once the race runs (lamp_hidden), those
   wholly in the lines it blanked (above 16, from 208, and wrapped past
   255 into the top), those off the console's 256 columns, and those under
   the bezel (below priority 3, which BG1's priority-1 tiles cover, and
   wholly outside the view). The race's sprites are 8 and 16 pixels, their
   characters from word $6000 (OBSEL $03). */
#define OAM_TABLE 0x0375
#define OBJ_CHARS 0x6000

/* the table to OAM as the game has it, every sprite's pages 0 (the
   original's screen again) */
static void sprites_write(void)
{
    const uint8_t *o = nshvc_wram() + OAM_TABLE;
    int i;

    wr8(OAMADDL, 0x00);
    wr8(OAMADDH, 0x00);
    for (i = 0; i < 544; i++)
        wr8(OAMDATA, o[i]);
    for (i = 0; i < 128; i++)
        nshvc_ext_sprite_page(i, 0, 0);
}

/* Placed as the HUD is: those at priority 3 are the HUD's, each anchored:
   in the panels' band with its box, else by the columns and rows of its
   piece, the sprites that touch it and those that touch them (a picture of
   several sprites moves whole); those below it are the 3D's, in its view,
   which is the original's projection about the screen's middle, so they
   keep their place on the console's picture. Hidden ones go 32 pages of
   rows down, past any screen. */
/* The bezel's start lamps (characters $A0-$AF, palette 3: the lights and
   their housings at the view's top corners), which the original's bezel
   kept all race: in the full window only while the start runs (player 1's
   race phase $0DD6 below 6, the race's) */
static int lamp_hidden(unsigned t, unsigned at)
{
    return (t & 0x1F0) == 0xA0 && (at & 0x0E) == 0x06 && nshvc_wram()[0x0DD6] >= 0x06;
}

static int spr_root(int *up, int i)
{
    while (up[i] != i)
        i = up[i] = up[up[i]];
    return i;
}

/* Two players' (the original's: the first 64 the first player's, on lines to 111,
   the rest the second's): at priority 3, in a player's boxes with them, else by the
   view's own piece (two_anchor: a picture of several sprites moves whole); below it,
   the 3D's, about the rectangle's middle, those in the boxes' place left out. */
static void sprites_two(void)
{
    const uint8_t *o = nshvc_wram() + OAM_TABLE;
    int i, b, ox = (W - 256) / 2, oy = origin_row(), kept = two_p;
    int sx0[128], sy0[128], sz[128], up[128], px0[128], px1[128], py0[128], py1[128], shown[128], box[128];
    uint8_t high[32];

    for (i = 0; i < 128; i++) {
        unsigned yy = o[i * 4 + 1], h = (o[512 + i / 4] >> (2 * (i & 3)) & 2) ? 16 : 8, at = o[i * 4 + 3];
        int xx = o[i * 4] | (o[512 + i / 4] >> (2 * (i & 3)) & 1) << 8, p = i >= 64, cx;
        if (xx >= 256)
            xx -= 512;
        cx = xx + (int)h / 2;
        up[i] = i;
        box[i] = cx >= two_bx[p] && cx < two_bx[p] + TWO_BOX_W;
        shown[i] = !(yy + h > 256 || yy >= 224 || xx + (int)h <= 0 || xx >= 256 ||
                     (int)yy + (int)h <= two_by[p] || (int)yy >= two_by[p] + TWO_BOX_H ||
                     box[i]);           /* (the boxes' are in their picture, panels_two) */
        sx0[i] = xx;
        sy0[i] = (int)yy;
        sz[i] = (int)h;
        if (!shown[i])
            continue;
        for (b = p ? 64 : 0; b < i; b++)
            if (shown[b] && ((o[b * 4 + 3] & 0x30) == 0x30) == ((at & 0x30) == 0x30) && box[b] == box[i] &&
                sx0[b] <= xx + (int)h && xx <= sx0[b] + sz[b] && sy0[b] <= (int)yy + (int)h && (int)yy <= sy0[b] + sz[b])
                up[spr_root(up, b)] = spr_root(up, i);
    }
    for (i = 0; i < 128; i++)
        px0[i] = py0[i] = 0x7FFF, px1[i] = py1[i] = -0x7FFF;
    for (i = 0; i < 128; i++)
        if (shown[i]) {
            int g = spr_root(up, i);
            if (sx0[i] < px0[g]) px0[g] = sx0[i];
            if (sx0[i] + sz[i] - 1 > px1[g]) px1[g] = sx0[i] + sz[i] - 1;
            if (sy0[i] < py0[g]) py0[g] = sy0[i];
            if (sy0[i] + sz[i] - 1 > py1[g]) py1[g] = sy0[i] + sz[i] - 1;
        }
    memcpy(high, o + 512, sizeof high);
    wr8(OAMADDL, 0x00);
    wr8(OAMADDH, 0x00);
    for (i = 0; i < 128; i++) {
        int g = spr_root(up, i), p = i >= 64, x = 0, y = 0xF0, xp = 0, yp = 32, dx, dy;
        unsigned at = o[i * 4 + 3];
        if (shown[i]) {
            two_p = p;
            if ((at & 0x30) == 0x30) {
                two_anchor(px0[g] - two_vx[p], px1[g] - two_vx[p], py0[g] - two_vy[p], py1[g] - two_vy[p], &dx, &dy);
                dx += sx0[i] - two_vx[p];
                dy += sy0[i] - two_vy[p];
            } else {
                dx = two_x[p] + two_w / 2 + sx0[i] - two_vx[p] - 72;
                dy = two_y[p] + two_h / 2 + sy0[i] - two_vy[p] - 40;
            }
            x = dx - ox;
            y = dy - oy;
            xp = x >= 256 ? (x - 256) / 512 + 1 : x < -256 ? -((-256 - x + 511) / 512) : 0;
            x -= 512 * xp;
            yp = y >= 0 ? y / 256 : -((255 - y) / 256);
            y -= 256 * yp;
        }
        high[i / 4] = (uint8_t)((high[i / 4] & ~(1u << (2 * (i & 3)))) | ((unsigned)(x >> 8) & 1) << (2 * (i & 3)));
        nshvc_ext_sprite_page(i, xp, yp);
        wr8(OAMDATA, (uint8_t)x);
        wr8(OAMDATA, (uint8_t)y);
        wr8(OAMDATA, o[i * 4 + 2]);
        wr8(OAMDATA, o[i * 4 + 3]);
    }
    for (i = 0; i < 32; i++)
        wr8(OAMDATA, high[i]);
    two_p = kept;
}

static void sprites_place(void)
{
    const uint8_t *o = nshvc_wram() + OAM_TABLE;
    int i, b, ox = (W - 256) / 2, oy = origin_row();
    int sx0[128], sy0[128], sz[128], up[128], px0[128], px1[128], py0[128], py1[128], shown[128];
    uint8_t high[32];

    if (two) {
        sprites_two();
        return;
    }
    for (i = 0; i < 128; i++) {         /* the sprites shown, and their pieces */
        unsigned yy = o[i * 4 + 1], h = (o[512 + i / 4] >> (2 * (i & 3)) & 2) ? 16 : 8;
        unsigned t = o[i * 4 + 2] | (o[i * 4 + 3] & 1) << 8, at = o[i * 4 + 3];
        int xx = o[i * 4] | (o[512 + i / 4] >> (2 * (i & 3)) & 1) << 8;
        if (xx >= 256)
            xx -= 512;
        up[i] = i;
        shown[i] = !(((t & 0x1F8) == 0x40 && (at & 0x0E) == 0x06) || lamp_hidden(t, at) || yy + h > 256 || yy >= PANEL_END ||
                     xx + (int)h <= 0 || xx >= 256 || yy + h <= RACE_ROW0 ||
                     ((at & 0x30) != 0x30 && (xx + (int)h <= VIEW_X || xx >= VIEW_X + VIEW_W ||
                                              (int)(yy + h) <= VIEW_Y || (int)yy >= VIEW_Y + VIEW_H)) ||
                     /* the HUD's on the view's rows under the bezel: the
                        original's view window hid them (parked to slide in) */
                     ((at & 0x30) == 0x30 && (int)yy >= VIEW_Y && (int)(yy + h) <= VIEW_Y + VIEW_H &&
                      (xx + (int)h <= VIEW_X || xx >= VIEW_X + VIEW_W)));
        sx0[i] = xx;
        sy0[i] = (int)yy;
        sz[i] = (int)h;
        if (!shown[i])
            continue;
        for (b = 0; b < i; b++)         /* touching (overlapping or side by side), of the same kind */
            if (shown[b] && ((o[b * 4 + 3] & 0x30) == 0x30) == ((at & 0x30) == 0x30) && sx0[b] <= xx + (int)h &&
                xx <= sx0[b] + sz[b] && sy0[b] <= (int)yy + (int)h && (int)yy <= sy0[b] + sz[b])
                up[spr_root(up, b)] = spr_root(up, i);
    }
    for (i = 0; i < 128; i++)
        px0[i] = py0[i] = 0x7FFF, px1[i] = py1[i] = -0x7FFF;
    for (i = 0; i < 128; i++)
        if (shown[i]) {
            int g = spr_root(up, i);
            if (sx0[i] < px0[g]) px0[g] = sx0[i];
            if (sx0[i] + sz[i] - 1 > px1[g]) px1[g] = sx0[i] + sz[i] - 1;
            if (sy0[i] < py0[g]) py0[g] = sy0[i];
            if (sy0[i] + sz[i] - 1 > py1[g]) py1[g] = sy0[i] + sz[i] - 1;
        }
    memcpy(high, o + 512, sizeof high);
    wr8(OAMADDL, 0x00);
    wr8(OAMADDH, 0x00);
    for (i = 0; i < 128; i++) {
        int g = spr_root(up, i), s = ox, vs = oy, x, y, xp = 0, yp = 32;
        unsigned at = o[i * 4 + 3], h = (unsigned)sz[i];
        if (shown[i]) {
            if ((at & 0x30) == 0x30) {  /* the HUD's: to the screen's edges, the view's by its own (view_shift) */
                s = sy0[i] + (int)h > PANEL_TOP ? anchor_shift(px0[g], px1[g]) : view_shift(px0[g], px1[g]);
                vs = anchor_shift_y(py0[g], py1[g]);
                if (sy0[i] + (int)h > PANEL_TOP) {
                    vs = H - 224 - safe_px[3];
                    for (b = 0; b < nboxes; b++)
                        if (sx0[i] + (int)h / 2 >= box_x0[b] && sx0[i] + (int)h / 2 <= box_x1[b])
                            s = box_shift[b];
                }
            }
            /* from the screen's place to the console picture's: its column 0
               at ox, its line 1 at oy; the pages past its 9-bit X and its
               256 rows */
            x = sx0[i] + s - ox;
            y = sy0[i] + vs - oy;
            xp = x >= 256 ? (x - 256) / 512 + 1 : x < -256 ? -((-256 - x + 511) / 512) : 0;
            x -= 512 * xp;
            yp = y >= 0 ? y / 256 : -((255 - y) / 256);
            y -= 256 * yp;
        } else {
            x = 0;
            y = 0xF0;
        }
        high[i / 4] = (uint8_t)((high[i / 4] & ~(1u << (2 * (i & 3)))) | ((unsigned)(x >> 8) & 1) << (2 * (i & 3)));
        nshvc_ext_sprite_page(i, xp, yp);
        wr8(OAMDATA, (uint8_t)x);
        wr8(OAMDATA, (uint8_t)y);
        wr8(OAMDATA, o[i * 4 + 2]);
        wr8(OAMDATA, o[i * 4 + 3]);
    }
    for (i = 0; i < 32; i++)
        wr8(OAMDATA, high[i]);
}

/* The race's upload stage (system/raster.c, 7E:A56B: the frame's uploads in
   a blank it makes from line 208, which in the original went on to line 16
   of the next frame) runs in the full window from the frame's hook at the
   vertical blank's start, before the step goes on: interrupts held and the
   S-CPU's accesses free, so the console's time stands still there
   (rm_time_still). In the original it ran before the vertical blank and
   the step after it, and so here, at no cost to any other timing, and no
   line of the screen is blanked. Its IRQ is set for line 240, which the
   stage moves on (to line 16) before it comes: it is taken only when the
   hook did not run the stage (the S-CPU not in the race's wait). */
static int time_still;

int rm_time_still(void) { return time_still; }

int fw_upload_line(void)
{
    return on && handed ? 0xF0 : 0xD0;
}

/* The two players' screen's stages (system/raster.c): the blank between the halves
   is the original's bezel's, none on the full window's screen; and its upload stage's
   line, as fw_upload_line's. */
uint8_t fw_split_blank(uint8_t game) { return on && handed && two ? nshvc_wram()[0x0D61] : game; }
int fw_split_line(void) { return on && handed && two ? 0xF0 : 0xD7; }

void abi_RasterStage(void);

void fw_vblank_uploads(void)
{
    Regs65816 kept = cpu;

    if (!on || !rm_in_tick_wait() || nshvc_wram()[0x0D3F] != (two ? 0x0E : 0x06))
        return;
    nshvc_hold_interrupts(1);
    nshvc_set_cpu_costs(0);
    time_still = 1;
    /* as RasterIrq enters it: 8-bit registers, data bank 0 */
    cpu.p = (uint8_t)(cpu.p | P_I | P_M | P_X);
    cpu.a &= 0xFF00;
    cpu.x &= 0x00FF;
    cpu.y &= 0x00FF;
    cpu.db = 0x00;
    abi_RasterStage();
    time_still = 0;
    cpu = kept;
    nshvc_set_cpu_costs(1);
    nshvc_hold_interrupts(0);
}

static int screen_w = 256, screen_h = 224;

void fw_screen_lost(void) { screen_w = screen_h = 0; page1_known = 0; }

/* The sky: colour 1 a line, by HDMA channels 2 (CGADD from the table at
   $7F:3497) and 3 (CGDATA, indirect, from the table at $7F:3489: entries
   of a line count, one colour for those lines or, with bit 7, a colour
   each, and the colours' address in bank $7F; GradientSetup's four, $17
   lines of one, $50 of one each from the picture's band, one of three at
   $7F:EC57 + $100 n, its address at $7F:348D, $18 of one, $50 each; other
   courses' their own). In the full window the HDMA is exactly the
   original's (its transfers take the S-CPU's time): its entries, their
   counts and modes, the lines they cover; only their addresses point
   elsewhere, to colours in the bands (the original's read first), that give each line
   the colour the original showed at the line the same height above the
   horizon in its view (above the view its top line's, below it its
   bottom line's; an entry of one colour that of its last line). Past the
   table's last line the console holds its last colour, as the original's
   did. The bytes are the game's again while a step runs, and before
   tween.c's in-between band goes in (fw_sky_take), so that its kept copy
   and this file's are the game's; at the step this file's are put back
   first and tween.c's after. */
#define SKY_TABLE 0x13489u              /* $7F:3489, CGDATA's table */
#define SKY_BANDS 0x1EC57u              /* $7F:EC57, the three bands */
#define SKY_ENTRIES 8
static uint8_t sky_kept[3 * SKY_ENTRIES + 1 + 0x300];
static int sky_in;

void fw_sky_take(void);

static void sky_take(void)
{
    uint8_t *w = nshvc_wram();
    if (!sky_in)
        return;
    memcpy(w + SKY_TABLE, sky_kept, 3 * SKY_ENTRIES + 1);
    memcpy(w + SKY_BANDS, sky_kept + 3 * SKY_ENTRIES + 1, 0x300);
    sky_in = 0;
}

/* the original's line for the full window's line h (its row h - 1): the
   line the original's view puts at the same height above the horizon,
   held to the view's lines */
static int sky_line(int h)
{
    double y = origin_row() + (double)h - 1.0 + 0.5;
    int l = (int)floor(VIEW_CY + (y - view_cy()));
    if (l < VIEW_Y)
        l = VIEW_Y;
    if (l > VIEW_Y + VIEW_H - 1)
        l = VIEW_Y + VIEW_H - 1;
    return l;
}

static void sky_make(void);

/* the band for the picture just drawn, then a water picture handed with
   its colours from it */
/* (the reference; a GPU's the same) */
void fw_sky_fill_cpu(const uint8_t *in, uint32_t *out, int w, int h, const uint32_t *gap, int side, int split, int seam)
{
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int at = side ? x : y;
            uint8_t c = in[(size_t)y * w + x];
            out[(size_t)y * w + x] = c ? c : at >= seam && at < split ? 0xFF000000u : gap[(at >= split ? (size_t)h : 0) + y];
        }
}
void (*fw_sky_filler)(const uint8_t *in, uint32_t *out, int w, int h, const uint32_t *gap, int side, int split, int seam) = fw_sky_fill_cpu;

void fw_sky(void)
{
    const uint32_t *t32;

    sky_make();
    if (!water_due)
        return;
    water_due = 0;
    if ((t32 = water_pass())) {
        rm_canvas_taken();
        picture_out(t32);
    }
}

static void sky_make(void)
{
    uint8_t *w = nshvc_wram();
    unsigned n, e, i, line = 0, band = 0, room = 0;
    unsigned cnt[SKY_ENTRIES], addr[SKY_ENTRIES], first[SKY_ENTRIES];
    static uint16_t orig[256];          /* the original's colour each line */
    uint16_t last = 0;

    if (!on || !handed || two)
        return;
    sky_take();                         /* the game's (or tween.c's) table to read */
    for (n = 0; n < SKY_ENTRIES && w[SKY_TABLE + 3 * n]; n++) {
        cnt[n] = w[SKY_TABLE + 3 * n];
        addr[n] = w[SKY_TABLE + 3 * n + 1] | w[SKY_TABLE + 3 * n + 2] << 8;
        first[n] = line;
        line += (cnt[n] & 0x7F) ? (cnt[n] & 0x7F) : 0x80;
    }
    if (!n || n == SKY_ENTRIES || line > 256)
        return;                         /* not a gradient's table */
    /* the original's colours */
    for (e = 0; e < n; e++) {
        unsigned lines = (cnt[e] & 0x7F) ? (cnt[e] & 0x7F) : 0x80, each = cnt[e] & 0x80 ? lines : 1, b;
        if (addr[e] < 0xEC57 || addr[e] + 2 * each > 0xEF57)
            return;                     /* colours outside the bands */
        for (i = 0; i < lines; i++) {
            b = 0x10000 + addr[e] + 2 * (each > 1 ? i : 0);
            orig[first[e] + i] = (uint16_t)(w[b] | w[b + 1] << 8);
        }
    }
    for (i = line; i < 256; i++)        /* the last held past the table */
        orig[i] = orig[line - 1];
    memcpy(sky_kept, w + SKY_TABLE, 3 * SKY_ENTRIES + 1);
    memcpy(sky_kept + 3 * SKY_ENTRIES + 1, w + SKY_BANDS, 0x300);
    sky_in = 1;
    /* each entry's colours in the bands, one after another, none across
       a band's end */
    for (e = 0, i = 0; e < n; e++) {
        unsigned lines = (cnt[e] & 0x7F) ? (cnt[e] & 0x7F) : 0x80, each = cnt[e] & 0x80 ? lines : 1, a, k;
        if (2 * each > room) {
            if (band == 3) {
                sky_take();
                return;
            }
            room = 0x100;
            i = band++ * 0x100;
        }
        a = 0xEC57 + i;
        for (k = 0; k < each; k++) {
            last = orig[sky_line((int)(first[e] + (each > 1 ? k : lines - 1)))];
            w[0x10000 + a + 2 * k] = (uint8_t)last;
            w[0x10000 + a + 2 * k + 1] = (uint8_t)(last >> 8);
        }
        w[SKY_TABLE + 3 * e + 1] = (uint8_t)a;
        w[SKY_TABLE + 3 * e + 2] = (uint8_t)(a >> 8);
        i += 2 * each;
        room -= 2 * each;
    }
}

void fw_sky_take(void)
{
    sky_take();
}

/* colour 1 on each of the screen's first n lines from the gradient's
   table as sky_make left it (the HDMA's: line h from its entry's colours
   in turn, an entry of one colour the same on all its lines, past the
   table's end its last held); 0 when the table is not this file's */
static int sky_lines(uint16_t *col, int n)
{
    const uint8_t *w = nshvc_wram();
    int e, L = 0, k;
    uint16_t last = 0;

    if (!sky_in)
        return 0;
    for (e = 0; e < SKY_ENTRIES && w[SKY_TABLE + 3 * e] && L < n; e++) {
        unsigned cnt = w[SKY_TABLE + 3 * e], addr = w[SKY_TABLE + 3 * e + 1] | w[SKY_TABLE + 3 * e + 2] << 8;
        int lines = (cnt & 0x7F) ? (int)(cnt & 0x7F) : 0x80;
        for (k = 0; k < lines && L < n; k++) {
            unsigned b = 0x10000 + addr + 2 * (cnt & 0x80 ? (unsigned)k : 0);
            last = (uint16_t)(w[b] | w[b + 1] << 8);
            col[L++] = last;
        }
    }
    while (L < n)
        col[L++] = last;
    return 1;
}

void fw_before_step(void)
{
    sky_take();
}

/* the full window's screen (costs off, the caller's): its size and the
   console's picture's place on it, VRAM to 128 KB (page 1, the hills') */
static void screen_set(void)
{
    static int vram;

    if (!vram) {
        nshvc_ext_vram(128);
        vram = 1;
    }
    if (screen_w != W || screen_h != H) {
#ifdef SRFX_NSHVC_HANDOFF
        rm_canvas_screen(W, H);
#endif
        nshvc_ext_screen(W, H);
        nshvc_ext_screen_origin(-1, origin_row());
        screen_w = W;
        screen_h = H;
    }
}

/* the console's screen again */
static void screen_back(void)
{
    sky_take();
    if (screen_w != 256 || screen_h != 224) {
#ifdef SRFX_NSHVC_HANDOFF
        rm_canvas_screen(256, 224);
#endif
        nshvc_ext_screen(256, 224);
        nshvc_ext_screen_origin(-1, -1);
        screen_w = 256;
        screen_h = 224;
    }
}

/* The hills (BG2) on VRAM page 1 for the console to draw, as the game has
   them on page 0 (its map $5800, 32 x 64, characters from $6000), less what
   the original's bezel and blank lines hid: the map's rows the view's
   scroll shows (the two buffers' bands, $0D8E + 2 n) kept, the rest (the
   pause's words, the HUD's pieces) the band's own sky, a character of
   colour 1 alone, which the gradient sets line by line: the sky above the
   hills, the ground's colours below, as opaque as the original's sky (the
   sprites it hides stay hidden). The game's map holds the 32 columns of the
   backdrop its 256 lines show (from the backdrop map at $7F:93D7, a turn
   128 columns: rm_backdrop_view), which a wider screen would show again
   and again; page 1's map is 64 x 64 (BG2SC $4B, at $4800, while the full
   window is on) and past the console's picture BG2 reads a map 128 x 64
   (nshvc_ext_layer_outside, contract 2.10), a turn across: the shown
   buffer's rows the backdrop's own columns either side of the picture,
   whole at any width, and its rows above and below the band the
   backdrop's own rows (bg2_backdrop), whole at any height. Written each frame, held, outside the game's time,
   as the in-between frames' rows are (tween.c). */
#define BG2_MAP_AT 0x5800
#define BG2_CHARS_AT 0x6000
#define BG2_WIDE_AT 0x4800
#define BG2_WIDE_SC 0x4B                /* $4800, 64 x 64 */

/* whether a 4 bpp character's 64 pixels are all colour 1 */
static int all_colour1(const uint16_t *v, unsigned ch)
{
    unsigned r, at = (BG2_CHARS_AT + ch * 16) & 0x7FFF;
    for (r = 0; r < 8; r++)
        if (v[(at + r) & 0x7FFF] != 0x00FF || v[(at + 8 + r) & 0x7FFF] != 0)
            return 0;
    return 1;
}

/* buffer sel's rows (map rows 30-46 for buffer A, 47-63 for B, 17 of
   them) across a map `cols` wide (64, 128): column c the backdrop's entry
   at the layer's x c * 8 taken within cols * 4 of the console's picture's
   middle,
   the game's row (from byte x, its first entry the map's column 0) run on
   either way round the turn; past the map's end ($2800) $20C0, as the
   game fills. The map's other rows the backdrop's rows above and below
   the band, up to 32 each way round the map's 64: the hills the
   original's view cut at its top and bottom lines whole on a screen that
   shows past them; above the backdrop's first row the band's sky */
static void bg2_backdrop(uint16_t *map, unsigned cols, unsigned sel)
{
    const uint8_t *w = nshvc_wram();
    uint16_t x, src, hofs;
    unsigned first = sel ? 47 : 30, col, r, c;
    int mid;

    if (!rm_backdrop_view(sel, &x, &src, &hofs))
        return;
    col = (x % 0x140) / 2;
    mid = 128 + (int16_t)hofs;
    for (r = 0; r < 64; r++) {
        int i = (int)((r - first + 32) & 63) - 32, at = (int)x + i * 0x140;
        if (at < 0)
            continue;
        for (c = 0; c < cols; c++) {
            int lx = (int)c * 8, e, half = (int)cols * 4;
            while (lx < mid - half)
                lx += 2 * half;
            while (lx >= mid + half)
                lx -= 2 * half;
            e = lx >= 0 ? lx / 8 : -((7 - lx) / 8);
            map[r * cols + c] = at >= 0x2800 ? 0x20C0
                : (uint16_t)(w[0x10000 + ((src + at - col * 2 + (((int)col + e) & 127) * 2) & 0xFFFF)] |
                             w[0x10000 + ((src + at - col * 2 + (((int)col + e) & 127) * 2 + 1) & 0xFFFF)] << 8);
        }
    }
}

static int bg2sc_kept = -1;             /* the game's BG2SC while the full window has its own */
/* page 1's hills as bg2_prepare last wrote them (only it writes there):
   a word written again only when it changes; not known (page1_known 0)
   until it has written it all since the full window's screen was made
   (its size, size_for_window) or came back (fw_frame_end) */
static uint16_t page1_map[64 * 64], page1_chars[0x2000];
static int page1_known;

/* words n of page 1 from word address at, from want, through the data
   port: only the runs that differ from have (all when !page1_known) */
static void page1_write(unsigned at, const uint16_t *want, uint16_t *have, unsigned n)
{
    unsigned i = 0, j;
    while (i < n) {
        if (page1_known && want[i] == have[i]) {
            i++;
            continue;
        }
        for (j = i; j < n && !(page1_known && want[j] == have[j]); j++)
            ;
        wr8(VMADDL, (uint8_t)(at + i));
        wr8(VMADDH, (uint8_t)((at + i) >> 8));
        for (; i < j; i++) {
            wr8(VMDATAL, (uint8_t)want[i]);
            wr8(VMDATAH, (uint8_t)(want[i] >> 8));
            have[i] = want[i];
        }
    }
}

static void bg2_prepare(void)
{
    static uint16_t map[64 * 64], out[128 * 64];
    const uint16_t *v = nshvc_vram();
    const uint8_t *w = nshvc_wram();
    uint8_t keep[64] = {0};
    unsigned i, r, sel, sky = 0xFFFF;

    sel = (w[0x0D81] | w[0x0D82] << 8) == 0x0001;   /* the rows the view's lines show, the shown buffer's
                                                       (the page is made again after each flip: the frame's end,
                                                       fw_backdrop_update): past its view the other's band */
    {
        unsigned vofs = (unsigned)(w[0x0D8E + 2 * sel] | w[0x0D8F + 2 * sel] << 8);
        for (r = VIEW_Y; r < VIEW_Y + VIEW_H; r++)
            keep[((r + vofs) & 511) >> 3] = 1;
    }
    for (r = 0; r < 64 && sky == 0xFFFF; r++)   /* the bands' sky: an entry whose character is colour 1 alone */
        for (i = 0; i < 32 && keep[r]; i++)
            if (all_colour1(v, v[BG2_MAP_AT + r * 32 + i] & 0x3FF)) {
                sky = v[BG2_MAP_AT + r * 32 + i];
                break;
            }
    for (r = 0; r < 64; r++)
        for (i = 0; i < 64; i++)
            map[r * 64 + i] = keep[r] || sky == 0xFFFF ? v[BG2_MAP_AT + r * 32 + (i & 31)] : (uint16_t)sky;
    for (r = 0; r < 64; r++)
        for (i = 0; i < 128; i++)
            out[r * 128 + i] = map[r * 64 + (i & 63)];
    bg2_backdrop(map, 64, sel);
    bg2_backdrop(out, 128, sel);
    nshvc_ext_layer_outside(1, out, 5);
    nshvc_ext_vram_direct(1);
    wr8(VMAIN, 0x80);
    {
        static uint16_t page[64 * 64];
        for (i = 0; i < 64 * 64; i++) { /* the map: the screens 32 x 32, left then right, top then bottom */
            unsigned at = (i >> 10 & 1) * 32 + (i >> 11) * 32 * 64 + (i >> 5 & 31) * 64 + (i & 31);
            page[i] = map[at];
        }
        page1_write(BG2_WIDE_AT, page, page1_map, 64 * 64);
    }
    {                                   /* the characters as page 0 has them: those the two maps name (none
                                           other is shown) */
        static uint8_t used[0x2000 / 16];
        unsigned c, k;
        memset(used, 0, sizeof used);
        for (k = 0; k < 64 * 64; k++)
            if ((c = map[k] & 0x3FF) < 0x2000 / 16)
                used[c] = 1;
        for (k = 0; k < 128 * 64; k++)
            if ((c = out[k] & 0x3FF) < 0x2000 / 16)
                used[c] = 1;
        for (c = 0; c < 0x2000 / 16; c++)
            if (used[c] || !page1_known)
                page1_write(BG2_CHARS_AT + c * 16, v + BG2_CHARS_AT + c * 16, page1_chars + c * 16, 16);
    }
    page1_known = 1;
    wr8(VMADDL, (uint8_t)(BG2_CHARS_AT + 0x2000));  /* the address as writing it all leaves it */
    wr8(VMADDH, (uint8_t)((BG2_CHARS_AT + 0x2000) >> 8));
    nshvc_ext_vram_direct(-1);
}

/* the hills' page made again now, for the buffer the step's flip (its
   uploads at the frame's start) shows and the step's own picture's
   backdrop (remaster.c rm_race_frame), before the frame's lines, which
   would show the page the last frame's end made */
void fw_backdrop_update(void)
{
    if (!on || !handed || two)
        return;
    nshvc_hold_interrupts(1);
    nshvc_set_cpu_costs(0);
    bg2_prepare();
    nshvc_set_cpu_costs(1);
    nshvc_hold_interrupts(0);
}

/* CGWSEL for the full window. Its two fields (bits 7-6, clip to black;
   5-4, colour maths prevented) act inside or outside the colour window
   (WOBJSEL bits 7-4: windows 2 and 1 on and inverted, WOBJLOG bits 3-2
   their logic), which the race makes of window 1 alone: the bezel's
   opening or the view's, or nothing (its HDMA table's lines all left past
   right), inverted or not. In the full window the bezel is gone and the
   view is the whole screen, so a colour window of the view, or of
   everything, is everywhere, and one of nothing, or outside the view, is
   nowhere: each field "inside" (10) or "outside" (01) is then always (11)
   or never (00), as that makes it: the crash's red over the view is over
   the whole screen (the panels' boxes, BG1's too, with it), and the
   tunnel's subtraction inside an empty window 1 (window setup 2,
   ColourMathSub83Windowed, its slide ended) nowhere, as the original
   shows it. A window that covers any of the screen counts as the view's;
   one this cannot read keeps the view's rule. */
static int cgwsel_game = -1;            /* the game's, put back when the full window goes */
static int wobjsel_game = -1;           /* the same for WOBJSEL */
static int wobjsel_ours = -1;           /* the one the full window last wrote */

/* The sprites kept to the HUD's box (window 2, which the race's sprites
   and colour window do not use, at the box's columns, the sprites masked
   outside it): a HUD sprite across the view's edge (the start lamps') is
   cut at the box's edge as the bezel cut it, where on a screen past 16:9
   or inside a safe area that edge is not the screen's (view_shift) */
static void sprites_box(void)
{
    int margin = W > UI_W_MAX && !two ? (W - UI_W_MAX) / 2 : 0;   /* (two players' HUD is on its rectangles, the whole screen's) */
    int l = margin > safe_px[0] ? margin : safe_px[0], r = margin > safe_px[2] ? margin : safe_px[2];

    if (wobjsel_game < 0 || mw_ppu_last(0x2125) != wobjsel_ours)   /* the game's, or one it wrote since */
        wobjsel_game = mw_ppu_last(0x2125);
    nshvc_ext_window_columns(2, l, W - 1 - r);
    wobjsel_ours = (wobjsel_game & 0xF0) | 0x0C;   /* the sprites' window 2 on, inverted */
    wr8(WOBJSEL, (uint8_t)wobjsel_ours);
}

static uint8_t cgwsel_full(uint8_t c)
{
    int w1 = 1, w2 = 1, sel = wobjsel_game >= 0 ? wobjsel_game : mw_ppu_last(0x2125), logic = mw_ppu_last(0x212B) >> 2 & 3;
    int on1 = sel >> 5 & 1, on2 = sel >> 7 & 1, a, b, all;
    uint8_t field[4];

    if (mw_windows_cover(&w1, &w2) < 0)
        w1 = w2 = 1;
    a = (w1 ^ (sel >> 4 & 1)) & 1;      /* each window over the full window: the view everywhere, inverted or not */
    b = (w2 ^ (sel >> 6 & 1)) & 1;
    if (on1 && on2)
        all = logic == 0 ? a | b : logic == 1 ? a & b : logic == 2 ? a ^ b : !(a ^ b);
    else
        all = on1 ? a : on2 ? b : 0;    /* no window: the colour window is nowhere */
    field[0] = 0;
    field[1] = all ? 0 : 3;             /* outside it */
    field[2] = all ? 3 : 0;             /* inside it */
    field[3] = 3;
    return (uint8_t)((c & 0x0F) | field[c >> 6 & 3] << 6 | field[c >> 4 & 3] << 4);
}

/* the screen the full window's: the sprites (in the HUD's box,
   sprites_box), BG1 on page 1, the view's window off and the screen on (the stage's blank was for its uploads);
   work the original never did, so it takes none of the S-CPU's time (an
   interrupt's accesses are timed, contract 2.2) */
static void takeover(void)
{
    uint8_t *w = nshvc_wram();

    nshvc_set_cpu_costs(0);
    sprites_place();
    if (two) {
        wr8(BGMODE, 0x03);              /* the canvas's texels whole CGRAM entries (at Mode 1's 4 bits they would be cut) */
        nshvc_ext_sprite_limits(0);     /* the players' sprites share their lines side by side */
    }
    wr8(TM, two ? 0x11 : 0x13);         /* BG1, BG2 (the hills, bg2_prepare; two players': two_hills, in the picture) and the sprites, as the race's */
    sprites_box();
    wr8(TMW, 0x10);                     /* the sprites' window alone */
    cgwsel_game = mw_ppu_last(0x2130);  /* the stage's, just set (WindowsSetupSelect) */
    wr8(CGWSEL, cgwsel_full((uint8_t)cgwsel_game));
    screen_set();
    nshvc_ext_sprite_wrap(0);           /* Y page 0's sprites nowhere above line 1 (those above it on page -1) */
    if (!two) {
        nshvc_ext_layer_page(1, 1);     /* BG2 on page 1: its map at $4800, 64 x 64, characters from $6000 (bg2_prepare) */
        if (bg2sc_kept < 0)
            bg2sc_kept = mw_ppu_last(0x2108);
        wr8(BG2SC, BG2_WIDE_SC);
    }
    wr8(INIDISP, w[0x0D61]);
    nshvc_set_cpu_costs(1);
}

void fw_upload_done(void)
{
    if (!on || !handed)                 /* until its first picture is on the canvas, the original's screen */
        return;
    takeover();
    staged = 1;
}

void fw_frame_end(void)
{
    int was = on, two_was;

#ifdef SRFX_NSHVC_HANDOFF
    if (handed && !handed_now && hand_pic() && fw_handoff->again)   /* a frame with no new picture: the last again */
        fw_handoff->again(tw, th);
    handed_now = 0;
#endif

    /* the race screen's stages run once a frame (from the hook while the
       full window is on) */
    unseen = seen ? 0 : unseen + 1;
    seen = 0;
    /* only in a race frame's wait for its step (this hook runs in it every
       frame of the race): the pause, the end's fades and the screens
       before and after keep the original's screen and their own timing */
    safe_area_update();
    {
        int players = nshvc_wram()[0x0D62] != 0;
        on = unseen <= 1 && rm_enhanced() && rm_option(RM_OPT_WIDESCREEN) && rm_canvas_ready() &&
             rm_whole_steps() && rm_in_tick_wait() && size_for_window();
        if (on && players != two)       /* (screen_set's own for either, its registers taken again) */
            handed = 0;
        two_was = two;
        two = on && players;
        if (two)
            two_layout();
    }
    /* the writes here are the full window's own, in no original's time:
       made with interrupts held, where the console's time does not pass
       (and only when there are any: a hold may move where a due interrupt
       is taken) */
    if (on && handed && !two) {         /* the hills' page, as the game's VRAM now has them */
        nshvc_hold_interrupts(1);
        nshvc_set_cpu_costs(0);
        bg2_prepare();
        nshvc_set_cpu_costs(1);
        nshvc_hold_interrupts(0);
    }
    if (!(was && !on)) {
        staged = 0;
        still_try();
        return;
    }
    nshvc_hold_interrupts(1);
    {                                   /* back to the original's screen */
        nshvc_set_cpu_costs(0);
        screen_back();
        nshvc_ext_sprite_wrap(1);
        nshvc_ext_layer_page(1, 0);
        if (bg2sc_kept >= 0)            /* the game's map for BG2 again */
            wr8(BG2SC, (uint8_t)bg2sc_kept);
        if (cgwsel_game >= 0)           /* and its colour window */
            wr8(CGWSEL, (uint8_t)cgwsel_game);
        cgwsel_game = -1;
        nshvc_ext_window_columns(2, -1, 0);
        if (wobjsel_game >= 0)          /* and its sprites' windows */
            wr8(WOBJSEL, (uint8_t)wobjsel_game);
        wobjsel_game = wobjsel_ours = -1;
        nshvc_ext_layer_outside(1, NULL, 0);
        bg2sc_kept = -1;
        sprites_write();
        if (staged)
            wr8(TM, 0x13);              /* the race screen's, as the stage set it */
        if (handed) {
            /* the buffer shown ($0D81: 1 buffer B), from the Super FX's
               RAM, on picture.c's canvas in its VRAM place, as the race's
               own Enhanced picture would have it: VRAM's copies of it were
               left out, and picture.c gives the characters back to VRAM as
               the copies of the next pictures land */
            int b = (nshvc_wram()[0x0D81] | nshvc_wram()[0x0D82] << 8) == 0x0001;
            nshvc_ext_canvas_chars(0, 0, 0, NULL, 1);
            if (two_was) {
            wr8(BGMODE, 0x01);
            nshvc_ext_sprite_limits(1);
        }
        if (two_was && !(fw_bp_on() && unplotted)) {   /* two players: their bitmaps as the copies left them */
                rm_canvas_show_copied((mw_ppu_last(0x210B) & 0x0F) ? 0x2000 : 0x0000, 360, 4);
            } else if (fw_bp_on() && unplotted) {  /* SC_GPU: no bitmap of the chip's: the GPU's, below */
                still_word = b ? 0x2200 : 0x0000;
                still_tries = 0;
            } else
                rm_canvas_show(b ? 0x2200 : 0x0000, b ? 0x9400 : 0x2C00, 416);
        }
        handed = 0;
        page1_known = 0;                /* the original's screen again: page 1 written whole when it comes back */
        nshvc_set_cpu_costs(1);
    }
    nshvc_hold_interrupts(0);
    staged = 0;
    still_try();
}
