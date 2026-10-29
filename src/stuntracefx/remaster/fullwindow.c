/*
 * fullwindow.c: Enhanced mode's full-window race (fullwindow.h).
 *
 *   the screen   W x H console pixels from the window's aspect
 *                (nshvc_ext_screen): 256 x 224 at 4:3, wider at 224
 *                lines, or 256 across and taller with the original's
 *                lines its top rows (nshvc_ext_screen_origin);
 *   BG1          on VRAM pages 1 and 2: its maps name a character for
 *                each of the screen's cells, which the canvas stands in
 *                for; both buffers' maps ($7800 with the characters at
 *                word 0, $7C00 with them at $2000) name the same, so the
 *                game's flips between them change nothing; its window
 *                off; BG1 alone shows, the backdrop, panels and sprites
 *                drawn into its texels;
 *   the texels   (W k) x (H k), drawn by the redraw with the view over
 *                the whole screen (fw_view_words), handed to the canvas
 *                each frame of the race's wait (fw_picture);
 *   the uploads  the race's raster stage that blanks the screen's last
 *                lines for its uploads runs in the vertical blank instead
 *                and leaves the screen on, so every line shows the picture.
 *
 * The view: its centre W / 2 across and the original's (32 + 64 = line 96)
 * down, half the rows a taller screen adds lower, focal length 157.5 (the
 * original's 104 / 128 half-width at 4:3) whatever the size, so the field
 * of view grows with the window across and down; all of it k times in
 * texels.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "machine.h"
#include "hw.h"
#include "gsu.h"
#include "gsu_ops.h"
#include "console_ext.h"
#include "remaster.h"
#include "fullwindow.h"
#include "rm_options.h"

uint64_t nshvc_wait_until(int (*ready)(void *ctx), void *ctx);
uint32_t nshvc_frames(void);
uint64_t nshvc_cart_clock(void);
const uint16_t *nshvc_vram(void);
const uint16_t *nshvc_cgram(void);

int fw_drawing, fw_hud;

static int on;                          /* the full-window race this frame */
static int seen;                        /* the race screen's stages ran this frame */
static int unseen = 2;                  /* frames since they last did */
static int W = 256, H = 224, K = 1;     /* the screen's width and height, texels a console pixel */
static int tw, th;                      /* the texels' size */
static uint8_t *tex, *chars;
/* each texel's layer as the console orders them: 0 nothing (the backdrop's
   colour or the sky's), 1 BG2 at priority 0, 2 BG2 at priority 1, 3 BG1
   (the 3D, the HUD, the panels) */
static uint8_t *lay;
static size_t tex_size;
static int map_w, map_h;                /* the size BG1's maps were written for */
static int handed;                      /* the canvas is the full window's */
static int staged;                      /* this frame's upload stage ran in the full window */

static void panels_draw(void);
static void sprites_write(void);
static void sprites_draw(void);
static void bg1_set(void);
static void takeover(void);
static void backdrop_draw(void);

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
static int view_cy(void) { return VIEW_CY + (H - 224) / 2; }
/* FW_CCHECK's larger picture: texels added above the screen (its centre
   that much lower) */
static int cy_off;

int fw_width(void) { return tw; }
int fw_height(void) { return th; }
int fw_k(void) { return K; }

int fw_on(void) { return on; }

void fw_race_stage(void) { seen = 1; }

/* ---- PLOT into the texels ---------------------------------------------- */

/* the colour PLOT writes, or -1 when it writes nothing: the screen's 8 bits
   a pixel (the race's), colour 0 (or a zero low nibble with the high one
   frozen) transparent unless the options say opaque */
static int plot_colour(void)
{
    uint8_t c = gsu.colr;

    if (!(gsu.por & 0x01)) {
        if (gsu.por & 0x08) {
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
    const char *e = getenv("FW_SAFE");
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
   one program a word) joined, each join by its columns' and rows' anchor */
#define HUD_PIECES 96
#define HUD_GAP 24                      /* pieces this near across (on rows they share) are one: a message's words */
static uint16_t hud_px[224][256];
static uint8_t hud_id[224][256];
static int hud_run, hud_x0, hud_x1, hud_y0, hud_y1;
static int hud_n, hp_x0[HUD_PIECES], hp_x1[HUD_PIECES], hp_y0[HUD_PIECES], hp_y1[HUD_PIECES], hp_up[HUD_PIECES];

void fw_gsu_program(int begin)
{
    if (begin) {
        hud_run = fw_hud && !checking && hud_n < HUD_PIECES;
        hud_x0 = hud_y0 = 0x7FFF;
        hud_x1 = hud_y1 = -1;
        return;
    }
    if (!hud_run)
        return;
    hud_run = 0;
    if (hud_x1 < 0)
        return;
    hp_x0[hud_n] = hud_x0;
    hp_x1[hud_n] = hud_x1;
    hp_y0[hud_n] = hud_y0;
    hp_y1[hud_n] = hud_y1;
    hp_up[hud_n] = hud_n;
    hud_n++;
}

static int hud_root(int i)
{
    while (hp_up[i] != i)
        i = hp_up[i] = hp_up[hp_up[i]];
    return i;
}

static void hud_place(void)
{
    int a, b, x, y, i, j, g0[HUD_PIECES], g1[HUD_PIECES], h0[HUD_PIECES], h1[HUD_PIECES], s[HUD_PIECES], t[HUD_PIECES];

    for (a = 0; a < hud_n; a++)
        for (b = 0; b < a; b++)
            if (hp_y0[a] <= hp_y1[b] && hp_y0[b] <= hp_y1[a] && hp_x0[a] <= hp_x1[b] + HUD_GAP &&
                hp_x0[b] <= hp_x1[a] + HUD_GAP)
                hp_up[hud_root(a)] = hud_root(b);
    for (a = 0; a < hud_n; a++)
        g0[a] = h0[a] = 0x7FFF, g1[a] = h1[a] = -1;
    for (a = 0; a < hud_n; a++) {
        int g = hud_root(a);
        if (hp_x0[a] < g0[g]) g0[g] = hp_x0[a];
        if (hp_x1[a] > g1[g]) g1[g] = hp_x1[a];
        if (hp_y0[a] < h0[g]) h0[g] = hp_y0[a];
        if (hp_y1[a] > h1[g]) h1[g] = hp_y1[a];
    }
    for (a = 0; a < hud_n; a++)
        if (hud_root(a) == a) {
            s[a] = anchor_shift(g0[a], g1[a]);
            t[a] = anchor_shift_y(h0[a], h1[a]);
        }
    for (a = 0; a < hud_n; a++) {
        int g = hud_root(a);
        for (y = hp_y0[a]; y <= hp_y1[a]; y++)
            for (x = hp_x0[a]; x <= hp_x1[a]; x++) {
                uint8_t *o;
                if (!hud_px[y][x] || hud_id[y][x] != a)
                    continue;
                o = tex + (size_t)(y + t[g]) * K * tw + (size_t)(x + s[g]) * K;
                for (j = 0; j < K; j++, o += tw)
                    for (i = 0; i < K; i++)
                        o[i] = (uint8_t)(hud_px[y][x] - 1);
            }
    }
    for (a = 0; a < hud_n; a++)
        for (y = hp_y0[a]; y <= hp_y1[a]; y++)
            for (x = hp_x0[a]; x <= hp_x1[a]; x++)
                hud_px[y][x] = 0;
    hud_n = 0;
}

static int cropping;                    /* FW_CCHECK's pictures: the 3D alone */

void fw_plot(uint16_t x, uint16_t y)
{
    int c = plot_colour(), i, j;

    if (c < 0 || (fw_hud && cropping))
        return;
    if (fw_hud && hud_run) {
        int px = (uint8_t)x + VIEW_X, py = (uint8_t)y + VIEW_Y;
        if (py >= 224)
            return;
        hud_px[py][px] = (uint16_t)(c + 1);
        hud_id[py][px] = (uint8_t)hud_n;
        if (px < hud_x0) hud_x0 = px;
        if (px > hud_x1) hud_x1 = px;
        if (py < hud_y0) hud_y0 = py;
        if (py > hud_y1) hud_y1 = py;
        return;
    }
    if (fw_hud) {
        int px = hud_x((uint8_t)x), py = hud_y((uint8_t)y);
        uint8_t *o;
        if (px < 0 || px >= W || py < 0 || py >= H)
            return;
        o = tex + (size_t)py * K * tw + (size_t)px * K;
        for (j = 0; j < K; j++, o += tw)
            for (i = 0; i < K; i++)
                o[i] = (uint8_t)c;
        return;
    }
    if (x < tw && y < th)
        tex[(size_t)y * tw + x] = (uint8_t)c;
    if (checking)
        check_alias(x, y, c);
}

uint8_t fw_rpix(uint16_t x, uint16_t y)
{
    if (fw_hud && hud_run) {
        int px = (uint8_t)x + VIEW_X, py = (uint8_t)y + VIEW_Y;
        return py < 224 && hud_px[py][px] ? (uint8_t)(hud_px[py][px] - 1) : 0;
    }
    if (fw_hud) {
        int px = hud_x((uint8_t)x), py = hud_y((uint8_t)y);
        if (px < 0 || px >= W || py < 0 || py >= H)
            return 0;
        return tex[(size_t)py * K * tw + (size_t)px * K];
    }
    return x < tw && y < th ? tex[(size_t)y * tw + x] : 0;
}

void gsu_plot16(uint16_t x, uint16_t y)
{
    if (fw_drawing)
        fw_plot(x, y);
    else
        gsu_plot((uint8_t)x, (uint8_t)y);
}

uint8_t gsu_rpix16(uint16_t x, uint16_t y)
{
    return fw_drawing ? fw_rpix(x, y) : gsu_rpix((uint8_t)x, (uint8_t)y);
}

void fw_clear(void)
{
    memset(tex, 0, (size_t)tw * th);
}

/* ---- the view ------------------------------------------------------------ */

/* the one-player race's centre and edges, as ViewParamsByPlayers sets them */
static const uint16_t k_race1p[6] = {0x0068, 0x0040, 0x0000, 0x00CF, 0x0000, 0x0080};

int fw_focal(void) { return (315 * K + 1) / 2; }
int fw_half_width(void) { return tw / 2; }

int fw_view_words(const uint16_t w[6], uint16_t out[8])
{
    int i, f = fw_focal(), cy = view_cy() * K + cy_off, below = th - cy;

    if (!fw_drawing || fw_hud)
        return 0;
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
    out[6] = (uint16_t)(((uint32_t)f << 14) / (uint32_t)(tw / 2));
    out[7] = (uint16_t)(((uint32_t)f << 13) / (uint32_t)(below > cy ? below : cy));
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
    if (!fw_drawing || checking)
        return;
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
    int64_t f = fw_focal(), rx, ry, m, lim = 0x3FFFLL * K;

    if (!z)
        z = 1;
    rx = x * f / z;
    ry = y * f / z;
    m = llabs(rx) > llabs(ry) ? llabs(rx) : llabs(ry);
    if (m > lim) {
        rx = rx * lim / m;
        ry = ry * lim / m;
    }
    *sx = tw / 2 + rx;
    *sy = (int64_t)view_cy() * K + cy_off + ry;
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
    ex_n++;
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
}

/* FxModelFacing's cross product of three projected points ($04E4 + 6 i),
   from the exact points: the facing of a face with a point the projection
   held would turn with it */
static uint16_t facing_at[3];

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
    if (getenv("FW_EXACTCHECK")) {      /* a check: the exact vertices against the projection's, where it did not hold them */
        static long seen, held, off;
        static int reported;
        for (i = 0; i < n; i++) {
            int64_t x = (int16_t)ram16((uint16_t)(list + 4 * i)), y = (int16_t)ram16((uint16_t)(list + 4 * i + 2));
            int64_t rx = x - tw / 2, ry = y - (int64_t)view_cy() * K - cy_off;
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

/* an edge's x stepped: set to 0 when it has gone just below it (to -8.0) */
static int64_t edge_step(int64_t x, int64_t step)
{
    x += step;
    if (x < 0 && x >= -0x800)
        x = 0;
    return x;
}

/* The draw list's vertices the projection held, exact: a word's exact
   value by its address in the Super FX's RAM (sw_val), good for this list
   (sw_gen, the list's generation: a list drawn, every one gone) */

static int32_t vw(uint16_t a) { return sw_gen[a >> 1] == list_gen ? sw_val[a >> 1] : (int16_t)ram16(a); }

/* a vertex word copied, its exact value with it (the fill's copies) */
static void wcopy(uint16_t dst, uint16_t src)
{
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
    double f = fw_focal(), ox = tw / 2, oy = (double)view_cy() * K + cy_off;

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
        double sx = ox + cx[1][i] * f / cz[1][i], sy = oy + cy[1][i] * f / cz[1][i];
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
    double f = fw_focal(), ox = tw / 2, oy = (double)view_cy() * K + cy_off;
    uint16_t list = ram16(0x005A);
    int i, m = 0, n = ga_n, lines = n == 2, in[4] = {0, 0, 0, 0};

    if (n < 0)
        return;
    ga_n = -1;
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
        double sx = ox + cx[i] * f / cz[i], sy = oy + cy[i] * f / cz[i];
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
    if (c < 0 || y < 0 || y >= th)
        return;
    if (checking)
        for (x = l < 0 ? 0 : l; x <= r; x++)
            check_alias(x, y, c);
    if (checking && getenv("FW_CHECKBREAK") && y == 64)  /* a planted break: one row left out */
        return;
    if (l < 0)
        l = 0;
    if (r >= tw)
        r = tw - 1;
    o = tex + (size_t)y * tw;
    for (x = l; x <= r; x++)
        o[x] = (uint8_t)c;
}

/* A95C: a flat polygon of n vertices, its colour byte at p */
static uint16_t flat_polygon(uint16_t p, int n)
{
    uint16_t v = (uint16_t)(p + 1), size = (uint16_t)(4 * n), dst = (uint16_t)(v - size), top_at = 0, src = v;
    uint16_t left_at, right_at;
    int32_t ymin = 0x7FFFFFFF, ymax = -0x7FFFFFFF, lrows, rrows, row;
    int64_t lx, rx, ls = 0, rs = 0;
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
        if (xmax - xmin > 0)
            flat_row(xmin, xmax - 1, ymin);
        return (uint16_t)(v + size);
    }
    row = ymin;
    left_at = (uint16_t)(top_at - 2);
    right_at = (uint16_t)(left_at + size - 2);
    lx = vw(left_at);
    rx = lx * 256;
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
    }
    for (;;) {
        if (!rrows) {
            int64_t from = (rx + 0x7F) >> 8, to;
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
        }
        do {
            int64_t l = lx >> 8, r = rx >> 8;
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
            int64_t from = (lx + 0x7F) >> 8, to;
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
} TexEdge;

/* the textured fill at full precision: the original's edges and spans, its
   u and v without its 8.8 steps' rounding, which over the thousands of rows
   a polygon near the camera spans at k texels a pixel leaves the texture
   standing still; the check's pass (the original's view) keeps the 8.8 */
static int tex_precise(void) { return !checking; }

/* A7D9 / A879: the next edge, forward (left) or back (right) */
static int tex_edge(TexEdge *e, int32_t row, int left)
{
    int64_t from, to;
    int32_t y;
    uint16_t u, v, u_prev, v_prev;
    to = (e->x + 0x7F) >> 8;
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
    gsu.por = 0;
    gsu_color((uint8_t)word);
    gsu.por = (uint8_t)((word >> 8) & 0x1F);
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
    if (ymin == ymax)
        return (uint16_t)(v + size);
    {
        uint16_t t = (uint16_t)(top_at - 2);
        l.x = r.x = vw(t) * 256;
        l.u = r.u = (uint16_t)(ram8((uint16_t)(t + 4)) << 8);
        l.v = r.v = (uint16_t)(ram8((uint16_t)(t + 6)) << 8);
        l.vu = r.vu = ram8((uint16_t)(t + 4));
        l.vv = r.vv = ram8((uint16_t)(t + 6));
        l.at = (uint16_t)(t + 8);
        r.at = (uint16_t)(t + size);
    }
    l.rows = r.rows = 0;
    row = ymin;
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
            int64_t xl = l.x >> 8, xr = r.x >> 8, x, x0, x1;
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
                uint8_t *o = tex + (size_t)row * tw;
                double hdu = remainder(r.hu - l.hu, 256.0) / width, hdv = remainder(r.hv - l.hv, 256.0) / width;
                double hu0 = l.hu + ram16(TEX_U_OFF) / 256.0, hv0 = l.hv + ram16(TEX_V_OFF) / 256.0;
                x0 = xl < 0 ? 0 : xl;
                x1 = xl + (int64_t)width < tw ? xl + (int64_t)width : tw;
                for (x = x0; x < x1; x++) {
                    double fu = hu0 + hdu * (double)(x - xl), fv = hv0 + hdv * (double)(x - xl);
                    unsigned iu = (unsigned)((int64_t)floor(fu) & 0xFF), iv = (unsigned)((int64_t)floor(fv) & 0xFF);
                    uint16_t at = (uint16_t)(((iv << 8 | iu) & mask) + base);
                    int c;
                    gsu_color(gsu_rd8((uint32_t)bank << 16 | at));
                    if ((c = plot_colour()) >= 0)
                        o[x] = (uint8_t)c;
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
    return (uint16_t)(v + size);
}

void fw_draw_list(void)
{
    uint16_t at, end = ram16(LIST_END);

    if (end == DRAW_LIST)
        return;
    ram8w(end, 0);
    ram8w((uint16_t)(end + 1), 0);
    at = DRAW_LIST;
    for (;;) {
        int8_t type = (int8_t)ram8(at);
        at++;
        if (type >= 3)
            at = flat_polygon(at, type);
        else if (type == 2)
            at = line(at);
        else if (type == 0)
            break;
        else
            at = tex_polygon(at, (uint8_t)~type);
    }
    list_gen++;                         /* the list drawn: its exact vertices gone */
}

/* ---- the picture to the console ------------------------------------------ */

/* BG1 in the full window: its map at word 0 of VRAM pages 1 to 3, the
   characters from word 0 (BG12NBA's low nibble 0), a band of the
   picture's rows of cells read from each page (its page switched down the
   screen by HDMA channel 7 writing $21C0). Page 1's characters end at its
   top (character 1023, the most a map names), the others' begin after
   their map, so the canvas runs through them from page 1's first: page
   1's cells, then each page's whole 1,024 characters (its map's and those
   past its cells none of the picture's). On a 224-line screen the map is
   64 x 32 cells (BG1SC $01, 64 characters' words), rows 0-13 on page 1
   and 14-27 on page 2. On a taller one (256 across) the console's lines
   sit in its middle, from row T = (H - 224) / 2 (nshvc_ext_screen_origin):
   the rows above take line 1's page and those below line 224's, so page 1
   has the rows above and the lines' first ones, page 3 line 217's on and
   the rows below, page 2 those between, at most 31 rows of cells each
   with a 32 x 32 map (BG1SC $00, 32 characters' words). Under the race's
   scroll the picture's cell (c, r) is at map row r - T / 8 - 2 (wrapping
   in 32) and, the console's column 0 being the screen's (W - 256) / 2, at
   map column c - (W - 256) / 16. */
#define BG12_NBA 0x60                   /* BG1's characters from word 0, BG2's as the race's ($6000) */
#define PAGES_MAX 3
static int tall(void) { return H > 224; }
static int origin_row(void) { return (H - 224) / 2; }
static unsigned map_chars(void) { return tall() ? 32 : 64; }      /* the characters the map's words take */
static int npages;                      /* the pages the picture's rows are on */
static unsigned page_lo[PAGES_MAX + 1], page_hi[PAGES_MAX + 1];  /* each page's rows of cells, from lo to hi */

/* the pages' bands of rows of cells, and channel 7's table: page 1 for the
   lines to page 1's last row, page 2 to line 216, page 3 after (a count
   past 127 in two) */
static uint8_t page_table[10];

static void pages_lay(void)
{
    unsigned t = (unsigned)origin_row() / 8, a, n = 0, rest;

    if (!tall()) {
        npages = 2;
        page_lo[1] = 0, page_hi[1] = 14;
        page_lo[2] = 14, page_hi[2] = 28;
        a = 14;
    } else {
        a = 248 / 8 - t < 14 ? 248 / 8 - t : 14;     /* the lines' rows of cells on page 1 */
        npages = 3;
        page_lo[1] = 0, page_hi[1] = t + a;
        page_lo[2] = t + a, page_hi[2] = t + 27;
        page_lo[3] = t + 27, page_hi[3] = (unsigned)H / 8;
    }
    page_table[n++] = (uint8_t)(a * 8);
    page_table[n++] = 0x01;
    if (tall()) {
        for (rest = 216 - a * 8; rest; ) {
            unsigned c = rest > 112 ? 112 : rest;
            page_table[n++] = (uint8_t)c;
            page_table[n++] = 0x02;
            rest -= c;
        }
        page_table[n++] = 0x01;
        page_table[n++] = 0x03;
    } else {
        page_table[n++] = 0x01;
        page_table[n++] = 0x02;
    }
    page_table[n++] = 0x00;
    while (n < sizeof page_table)
        page_table[n++] = 0x00;
}

/* the screen's size for the window: its aspect at 224 lines (256 at 4:3,
   the console's pixels 8:7), whole pairs of cells across (the console's
   picture at its centre then starts on a cell), 256 to 512, the width of
   BG1's 64-cell map; narrower than 4:3, 256 across and taller, in whole
   pairs of cells (the console's lines then start on a cell): to 704
   lines, the rows above line 1 and below line 224 each one page's */
#define W_MAX 512
#define H_MAX 704

static void size_of_window(int *w, int *h)
{
    float ww = 0, wh = 0, a;

    *w = 256;
    *h = 224;
    nshvc_window_size(&ww, &wh);
    if (!(ww > 0 && wh > 0))
        return;
    a = (ww / wh) / (4.0f / 3.0f);
    if (a >= 1.0f) {
        *w = (int)(256.0f * a / 16.0f + 0.5f) * 16;
        if (*w > W_MAX)
            *w = W_MAX;
    } else {
        *h = (int)(224.0f / a / 16.0f + 0.5f) * 16;
        if (*h > H_MAX)
            *h = H_MAX;
    }
}

static int size_for_window(void)
{
    int k = nshvc_ext_canvas_scale(), w, h;
    size_t need;

    size_of_window(&w, &h);
    if (k < 1)
        return 0;
    if (k != K || w != W || h != H || !tex) {
        K = k;
        W = w;
        H = h;
        tw = W * K;
        th = H * K;
        pages_lay();
        need = (size_t)tw * th + (size_t)64 * 64 * K * K;
        if (need > tex_size) {
            free(tex);
            free(chars);
            free(lay);
            tex = malloc(need);
            chars = malloc(need);
            lay = malloc(need);
            tex_size = tex && chars && lay ? need : 0;
            if (!tex_size)
                return 0;
        }
        handed = 0;
    }
    return 1;
}

/* BG1's scroll in a race, as its setup leaves it (bank $03: $FFEF, -17,
   across 0): the picture's row y is the map's row y - 16 */
#define RACE_ROW0 16

static unsigned cells_across(void) { return (unsigned)W / 8; }
static unsigned cells_down(void) { return (unsigned)H / 8; }
static unsigned page1_first(void) { return 1024 - (page_hi[1] - page_lo[1]) * cells_across(); }

static void maps_write(void)
{
    unsigned p, half, mr, mc, cells = cells_across(), shift = (unsigned)(W - 256) / 16;
    unsigned across = tall() ? 32 : 64, t = (unsigned)origin_row() / 8;

    nshvc_ext_vram(npages == 3 ? 256 : 192);
    for (p = 1; p <= (unsigned)npages; p++) {
        nshvc_ext_vram_direct((int)p);
        wr8(VMAIN, 0x80);
        wr8(VMADDL, 0x00);
        wr8(VMADDH, 0x00);
        for (half = 0; half < across / 32; half++)  /* a 64-cell map: two 32-cell screens, one after the other */
            for (mr = 0; mr < 32; mr++)
                for (mc = half * 32; mc < half * 32 + 32; mc++) {
                    /* the page's row of cells at map row mr: its rows are fewer than 32 */
                    unsigned pr = page_lo[p] + ((mr + t + RACE_ROW0 / 8 - page_lo[p]) & 31), pc = (mc + shift) & 63, c = 0;
                    if (pc < cells && pr < page_hi[p])
                        c = p == 1 ? page1_first() + pr * cells + pc : map_chars() + (pr - page_lo[p]) * cells + pc;
                    c |= 0x2000;        /* priority 1, as the game's */
                    wr8(VMDATAL, (uint8_t)c);
                    wr8(VMDATAH, (uint8_t)(c >> 8));
                }
    }
    nshvc_ext_vram_direct(-1);
    map_w = W;
    map_h = H;
}

/* the texels as the canvas's characters in its order, each 8k x 8k */
static void hand(void)
{
    unsigned cells = cells_across(), first = page1_first(), gap = map_chars(), n, i, r, p;
    size_t side = (size_t)8 * K;

    /* page 1's cells, then each page from its character 0 to its last cell's */
    n = (1024 - first) + 1024 * (unsigned)(npages - 2) + gap + (page_hi[npages] - page_lo[npages]) * cells;
    for (i = 0; i < n; i++) {
        uint8_t *o = chars + (size_t)i * side * side;
        const uint8_t *s;
        unsigned c, cell;
        if (i < 1024 - first) {
            p = 1;
            cell = i;
        } else {
            p = 2 + (i - (1024 - first)) / 1024;
            c = (i - (1024 - first)) % 1024;
            if (c < gap || c - gap >= (page_hi[p] - page_lo[p]) * cells) {
                memset(o, 0, side * side);
                continue;
            }
            cell = page_lo[p] * cells + c - gap;
        }
        s = tex + (size_t)(cell / cells) * side * tw + (size_t)(cell % cells) * side;
        for (r = 0; r < side; r++)
            memcpy(o + r * side, s + r * tw, side);
    }
    rm_canvas_taken();
    nshvc_ext_canvas_chars(0, 0x8000u + first * 32u, (int)n, chars, K);
    if (!handed) {                      /* the first picture: the screen taken over with it, in the same blank */
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
    if (K != 1)
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
    fprintf(stderr, "fwkcheck: k %d, %ld pictures, %.2f%% of pixels differ on average, %ld past 10%%; worst:", K,
            kc_pictures, kc_pictures ? 100.0 * kc_sum / kc_pictures : 0.0, kc_over);
    for (i = 0; i < 5; i++)
        fprintf(stderr, " %.2f%% (frame %u)", 100.0 * kc_worst[i], kc_worst_frame[i]);
    fprintf(stderr, "\n");
}

static void kcheck_picture(int j)
{
    static uint8_t scratch[2][RM_TWEEN_BYTES], *one, bank71[0x10000];
    uint8_t *tex_k = tex;
    int k = K, tw_k = tw, th_k = th, x, y, i, m;
    long differ = 0;
    double part;

    if (K < 2)
        return;
    if (!one && !(one = malloc((size_t)W_MAX * H_MAX)))
        return;
    tex = one;
    K = 1;
    tw = W;
    th = H;
    fw_drawing = 1;
    rm_hud_state(0);
    memcpy(bank71, nshvc_sram() + 0x10000, sizeof bank71);
    list_gen++;
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    fw_drawing = 0;
    if (hud_n)                          /* a redraw let go before the HUD's end: its pieces */
        hud_place();
    fw_hud = 0;
    tex = tex_k;
    K = k;
    tw = tw_k;
    th = th_k;
    if (!m)
        return;
    if (!kc_pictures++)
        atexit(kcheck_report);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            differ += one[y * W + x] != tex[(size_t)(y * K + K / 2) * tw + x * K + K / 2];
    part = (double)differ / (W * H);
    kc_sum += part;
    kc_over += part > 0.10;
    if (getenv("FW_KDUMP") && nshvc_frames() == (uint32_t)atoi(getenv("FW_KDUMP"))) {
        /* the two as pictures, k = 1 then k's centres: FW_KDUMP's frame, to FW_KDUMPF.1.ppm and .k.ppm */
        const uint16_t *cg = nshvc_cgram();
        char path[512];
        int which;
        for (which = 0; which < 2; which++) {
            FILE *f;
            snprintf(path, sizeof path, "%s.%s.ppm", getenv("FW_KDUMPF"), which ? "k" : "1");
            if (!(f = fopen(path, "wb")))
                continue;
            fprintf(f, "P6\n%d %d\n255\n", W, H);
            for (y = 0; y < H; y++)
                for (x = 0; x < W; x++) {
                    uint16_t c = cg[which ? tex[(size_t)(y * K + K / 2) * tw + x * K + K / 2] : one[y * W + x]];
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
    int mg = atoi(getenv("FW_CCHECK")) * K, bw = tw + 2 * mg, bh = th + 2 * mg, x, y, i;
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
            differ += getenv("FW_CCOLOUR") ? a[(size_t)y * tw + x] != b[(size_t)(y + mg) * bw + x + mg]
                                           : !a[(size_t)y * tw + x] != !b[(size_t)(y + mg) * bw + x + mg];
    part = (double)differ / ((double)tw * th);
    cc_sum += part;
    cc_over += part > 0.005;
    if (getenv("FW_CDUMP") && nshvc_frames() == (uint32_t)atoi(getenv("FW_CDUMP"))) {
        const uint16_t *cg = nshvc_cgram();
        char path[512];
        int which;
        for (which = 0; which < 2; which++) {
            FILE *f;
            int ww = which ? bw : tw, hh = which ? bh : th;
            const uint8_t *p = which ? b : a;
            snprintf(path, sizeof path, "%s.%s.ppm", getenv("FW_CDUMPF"), which ? "big" : "screen");
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
enum { PR_REDRAW, PR_LAYERS, PR_BACKDROP, PR_PANELS, PR_SPRITES, PR_HAND, PR_N };
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
    if (!getenv("FW_PROF"))
        return;
    t = prof_now();
    if (stage >= 0)
        prof_ms[stage] += t - prof_t;
    prof_t = t;
    if (stage == PR_HAND && ++prof_pics == 120) {
        fprintf(stderr, "fwprof %dx%d k%d ms a picture: redraw %.2f layers %.2f backdrop %.2f panels %.2f sprites %.2f hand %.2f\n",
                W, H, K, prof_ms[0] / 120, prof_ms[1] / 120, prof_ms[2] / 120, prof_ms[3] / 120, prof_ms[4] / 120, prof_ms[5] / 120);
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

    if (!getenv("FW_PROF"))
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
        bg1_set();
        nshvc_set_cpu_costs(1);
        nshvc_hold_interrupts(0);
    }
    if (getenv("FW_CHECK"))
        check_picture(j);
    prof_mark(-1);
    fw_drawing = 1;
    list_gen++;
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    fw_drawing = 0;
    if (hud_n)                          /* a redraw let go before the HUD's end: its pieces */
        hud_place();
    fw_hud = 0;
    if (!m)
        return 0;
    if (getenv("FW_KCHECK"))            /* before the backdrop and panels: the 3D and HUD alone */
        kcheck_picture(j);
    if (getenv("FW_CCHECK"))
        ccheck_picture(j);
    prof_mark(PR_REDRAW);
    for (i = 0; i < (size_t)tw * th; i++)  /* the 3D and the HUD: BG1's */
        lay[i] = tex[i] ? 3 : 0;
    prof_mark(PR_LAYERS);
    backdrop_draw();
    prof_mark(PR_BACKDROP);
    panels_draw();
    prof_mark(PR_PANELS);
    sprites_draw();
    prof_mark(PR_SPRITES);
    hand();
    prof_mark(PR_HAND);
    return 1;
}

/* ---- the backdrop ------------------------------------------------------------ */

/* The backdrop (BG2: the hills, and the sky in colour 1, which the
   gradient's HDMA sets line by line) drawn under the 3D at the 3D's
   scale, where the 3D leaves colour 0, so it turns and pitches with it:
   texel (X, Y) shows the original's screen at the point the original's
   view (focal length 128) puts in the same direction, its BG2 as the
   redraw left it (rm_tween_backdrop: its map rows for the buffer shown and
   its scroll; the rest of the map VRAM's), a 4 bpp pixel as its colour
   index (palette * 16 + pixel, as BG1's 8 bpp texels index colours). BG2
   itself is then off. Past the original view's top and bottom, where its
   map holds nothing the original showed: the sky above (colour 1, which
   fw_sky sets line by line), its bottom row below (the ground's colour). */
#define BG2_MAP  0x5800                 /* BG2SC $5A: 32 x 64 at $5800 */
#define BG2_CHARS 0x6000                /* BG12NBA $6x */

static void backdrop_draw(void)
{
    static int32_t col_x[8192];
    const uint16_t *v = nshvc_vram();
    const uint8_t *rows;
    unsigned at, X, Y;
    uint16_t hofs, vofs;
    int32_t f = fw_focal();

    if (!rm_tween_backdrop(&rows, &at, &hofs, &vofs) || tw > 8192)
        return;
    for (X = 0; X < (unsigned)tw; X++)  /* the original's column, in 1/256ths of a pixel */
        col_x[X] = (VIEW_X + VIEW_W / 2) * 256 + ((int32_t)X * 2 + 1 - tw) * 128 * 256 / (2 * f);
    for (Y = 0; Y < (unsigned)th; Y++) {
        int32_t y = (VIEW_CY * 256 + ((int32_t)Y * 2 + 1 - 2 * view_cy() * K) * 128 * 256 / (2 * f)) >> 8;
        unsigned row, cell, py;
        if (y < VIEW_Y) {               /* above the original's view: the sky (colour 1, its line's) */
            uint8_t *o = tex + (size_t)Y * tw;
            for (X = 0; X < (unsigned)tw; X++)
                if (!o[X])
                    o[X] = 1;
            continue;
        }
        if (y > VIEW_Y + VIEW_H - 1)    /* below it its bottom row goes on (the ground's colour) */
            y = VIEW_Y + VIEW_H - 1;
        row = (unsigned)(y + 1 + vofs) & 511, cell = BG2_MAP + (row >> 3) * 32, py = row & 7;
        uint8_t *o = tex + (size_t)Y * tw;
        for (X = 0; X < (unsigned)tw; X++) {
            unsigned col, a, px, py2, c;
            uint16_t e, w0, w1;
            if (o[X])
                continue;
            col = (unsigned)((col_x[X] >> 8) + hofs) & 255;
            a = cell + (col >> 3);
            e = a >= at && a < at + 0x220 ? (uint16_t)(rows[(a - at) * 2] | rows[(a - at) * 2 + 1] << 8) : v[a];
            px = e & 0x4000 ? col & 7 : 7 - (col & 7);
            py2 = e & 0x8000 ? 7 - py : py;
            a = (BG2_CHARS + (e & 0x3FF) * 16 + py2) & 0x7FFF;
            w0 = v[a];
            w1 = v[(a + 8) & 0x7FFF];
            c = (w0 >> px & 1) | (w0 >> (px + 8) & 1) << 1 | (w1 >> px & 1) << 2 | (w1 >> (px + 8) & 1) << 3;
            if (c) {
                o[X] = (uint8_t)((e >> 10 & 7) * 16 + c);
                lay[(size_t)Y * tw + X] = e & 0x2000 ? 2 : 1;
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

static void panels_draw(void)
{
    static uint8_t px[PANEL_ROWS][256], keep[PANEL_ROWS][256];
    static int16_t stack[PANEL_ROWS * 256][2];
    int x, y, n = 0, i, j;
    uint8_t bg;

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
    for (y = 0; y < PANEL_ROWS; y++)
        for (x = 0; x < 256; x++) {
            int x0 = x, x1 = x, edge = 0, s, m = 0, q;
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
            if (nboxes < MAX_BOXES) {
                box_x0[nboxes] = x0;
                box_x1[nboxes] = x1;
                box_shift[nboxes++] = s;
            }
            for (q = 0; q < m; q++) {
                int a = part[q][0], b = part[q][1];
                size_t at = (size_t)(PANEL_TOP + b + H - 224 - safe_px[3]) * K * tw + (size_t)(a + s) * K;
                uint8_t *o = tex + at, *l = lay + at;   /* at: the band moved up by the safe area's bottom */
                for (j = 0; j < K; j++, o += tw, l += tw)
                    for (i = 0; i < K; i++)
                        o[i] = px[b][a], l[i] = 3;
            }
        }
}

/* ---- the screen ------------------------------------------------------------ */

/* The sprites: the game's table (work RAM $0375: 512 bytes, then the 32
   of each sprite's X bit 8 and size). In the full window they are drawn
   into the texels (sprites_draw) and the console's are off, so that they
   go anywhere on the screen, past its 256th row too, which the console's
   Y does not reach. The sprites the original's screen kept out of sight
   are left out: the bezel's sides (characters $40-$47, palette 3) and its
   start lamps once the race runs (lamp_hidden), those
   wholly in the lines it blanked (above 16, from 208, and wrapped past
   255 into the top), those off the console's 256 columns, and those under
   the bezel (below priority 3, which BG1's priority-1 tiles cover, and
   wholly outside the view). The race's sprites are 8 and 16 pixels, their
   characters from word $6000 (OBSEL $03). */
#define OAM_TABLE 0x0375
#define OBJ_CHARS 0x6000

/* the table to OAM as the game has it, the extended X bits off (the
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
        nshvc_ext_sprite_x(i, 0);
}

/* Into the texels as the console composes them: at each pixel the sprite
   of the lowest number with a pixel there (the console's order), then by
   its priority against the backgrounds (lay): 3 over everything, 2 under
   BG1 (its priority-1 tiles), 1 and 0 under BG2's priority-1 tiles too; a
   4 bpp pixel as colour 128 + its palette * 16 + the pixel. Those at
   priority 3 are the HUD's, each anchored as the HUD: in the panels' band
   with its box, else by the columns and rows of its piece, the sprites
   that touch it and those that touch them (a picture of several sprites
   moves whole); those below it are in the 3D's view, moved with it. */
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

static void sprites_draw(void)
{
    static uint16_t obj[H_MAX][W_MAX];  /* the sprites' layer: 0 none, else priority << 8 | colour, + 1 */
    const uint8_t *o = nshvc_wram() + OAM_TABLE;
    const uint16_t *v = nshvc_vram();
    int i, n = 0, x0 = W, x1 = -1, y0 = H, y1 = -1, x, y, a, b;
    int sx0[128], sy0[128], sz[128], up[128], px0[128], px1[128], py0[128], py1[128], shown[128];

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
                                              (int)(yy + h) <= VIEW_Y || (int)yy >= VIEW_Y + VIEW_H)));
        sx0[i] = xx;
        sy0[i] = (int)yy;
        sz[i] = (int)h;
        if (!shown[i])
            continue;
        for (b = 0; b < i; b++)         /* touching (overlapping or side by side), of the same kind */
            if (shown[b] && ((o[b * 4 + 3] & 0x30) == 0x30) == ((at & 0x30) == 0x30) && sx0[b] <= xx + (int)h && xx <= sx0[b] + sz[b] && sy0[b] <= (int)yy + (int)h &&
                (int)yy <= sy0[b] + sz[b])
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
    for (i = 127; i >= 0; i--) {
        unsigned y = (unsigned)sy0[i], h = (unsigned)sz[i];
        unsigned t = o[i * 4 + 2] | (o[i * 4 + 3] & 1) << 8, at = o[i * 4 + 3];
        int x = sx0[i], s, vs, r, c, g = spr_root(up, i);
        if (!shown[i])
            continue;
        if ((at & 0x30) != 0x30) {
            /* below priority 3, in the view: the 3D's (a rival's marker,
               what shows where the 3D leaves colour 0), its piece's middle
               moved as the 3D is: about the view's centre, at its scale */
            double sc = (double)fw_focal() / K / 128.0, cx = (px0[g] + px1[g] + 1) / 2.0, cy = (py0[g] + py1[g] + 1) / 2.0;
            s = (int)floor(W / 2.0 + (cx - (VIEW_X + VIEW_W / 2)) * sc - cx + 0.5);
            vs = (int)floor(view_cy() + (cy - VIEW_CY) * sc - cy + 0.5);
        } else {
            s = anchor_shift(px0[g], px1[g]);
            vs = anchor_shift_y(py0[g], py1[g]);
        }
        if ((int)y + (int)h > PANEL_TOP) {
            vs = H - 224 - safe_px[3];
            for (b = 0; b < nboxes; b++)
                if (x + (int)h / 2 >= box_x0[b] && x + (int)h / 2 <= box_x1[b])
                    s = box_shift[b];
        }
        x += s;
        for (r = 0; r < (int)h; r++) {
            int sy = (int)y + vs + r, ry = at & 0x80 ? (int)h - 1 - r : r;
            if (sy < 0 || sy >= H)
                continue;
            for (c = 0; c < (int)h; c++) {
                int sx = x + c, cx = at & 0x40 ? (int)h - 1 - c : c, bit = 7 - (cx & 7);
                unsigned ch = (t & 0x100) | ((t + (unsigned)(ry >> 3) * 16) & 0xF0) | ((t + (unsigned)(cx >> 3)) & 0x0F);
                unsigned w = (OBJ_CHARS + ch * 16 + (unsigned)(ry & 7)) & 0x7FFF, px;
                uint16_t w0 = v[w], w1 = v[(w + 8) & 0x7FFF];
                if (sx < 0 || sx >= W)
                    continue;
                px = (w0 >> bit & 1) | (w0 >> (bit + 8) & 1) << 1 | (w1 >> bit & 1) << 2 | (w1 >> (bit + 8) & 1) << 3;
                if (!px)
                    continue;
                obj[sy][sx] = (uint16_t)(((at >> 4 & 3) << 8 | (128 + (at >> 1 & 7) * 16 + px)) + 1);
                if (sx < x0) x0 = sx;
                if (sx > x1) x1 = sx;
                if (sy < y0) y0 = sy;
                if (sy > y1) y1 = sy;
                n++;
            }
        }
    }
    if (!n)
        return;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            unsigned e = obj[y][x];
            uint8_t *q, *ql;
            if (!e)
                continue;
            obj[y][x] = 0;
            e--;
            q = tex + (size_t)y * K * tw + (size_t)x * K;
            ql = lay + (size_t)y * K * tw + (size_t)x * K;
            /* the console's order in mode 3: priority 3 over all, 2 over
               BG2, 1 and 0 under BG2's priority-1 tiles; all under BG1's
               (priority 1) */
            for (b = 0; b < K; b++, q += tw, ql += tw)
                for (a = 0; a < K; a++)
                    if (ql[a] < ((e >> 8) == 3 ? 4 : (e >> 8) == 2 ? 3 : 2))
                        q[a] = (uint8_t)e;
        }
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

void abi_RasterStage(void);

void fw_vblank_uploads(void)
{
    Regs65816 kept = cpu;

    if (!on || !rm_in_tick_wait() || nshvc_wram()[0x0D3F] != 0x06)
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

/* BG1's page down the screen: HDMA channel 7, mode 0, to $21C0 from a
   table at work RAM $7F:FFF0 (past everything the game uses): page 1 for
   the lines of the picture's rows 0-13, then page 2. The table's bytes
   are the game's again (kept, put back) while the game's own code runs a
   step, and the channel's registers as the game left them when the full
   window ends. */
#define PAGE_TABLE 0x1FFF0u             /* in work RAM: $7F:FFF0 */
static uint8_t table_kept[sizeof page_table], ch7_kept[5];
static int table_in, ch7_set, screen_w = 256, screen_h = 224;

void fw_screen_lost(void) { screen_w = screen_h = 0; }

static void page_table_put(void)
{
    uint8_t *w = nshvc_wram();
    if (table_in)
        return;
    memcpy(table_kept, w + PAGE_TABLE, sizeof table_kept);
    memcpy(w + PAGE_TABLE, page_table, sizeof page_table);
    table_in = 1;
}

static void page_table_take(void)
{
    if (!table_in)
        return;
    memcpy(nshvc_wram() + PAGE_TABLE, table_kept, sizeof table_kept);
    table_in = 0;
}

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
    double f = (double)fw_focal() / K, y = origin_row() + (double)h - 1.0 + 0.5;
    int l = (int)floor(VIEW_CY + (y - view_cy()) * 128.0 / f);
    if (l < VIEW_Y)
        l = VIEW_Y;
    if (l > VIEW_Y + VIEW_H - 1)
        l = VIEW_Y + VIEW_H - 1;
    return l;
}

void fw_sky(void)
{
    uint8_t *w = nshvc_wram();
    unsigned n, e, i, line = 0, band = 0, room = 0;
    unsigned cnt[SKY_ENTRIES], addr[SKY_ENTRIES], first[SKY_ENTRIES];
    static uint16_t orig[256];          /* the original's colour each line */
    uint16_t last = 0;

    if (!on || !handed)
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

void fw_before_step(void)
{
    page_table_take();
    sky_take();
}

/* the full window's registers (costs off, the caller's): BG1's map and
   characters, its page's HDMA, the screen's width */
static void bg1_set(void)
{
    static const uint32_t ch7[5] = {DMAP7, BBAD7, A1T7L, A1T7L + 1, A1B7};
    static const uint8_t v[5] = {0x00, 0xC0, (uint8_t)PAGE_TABLE, (uint8_t)(PAGE_TABLE >> 8), 0x7F};
    int i;

    if (!ch7_set) {
        for (i = 0; i < 5; i++)
            ch7_kept[i] = rd8(ch7[i]);
        ch7_set = 1;
    }
    for (i = 0; i < 5; i++)
        wr8(ch7[i], v[i]);
    wr8(BG1SC, tall() ? 0x00 : 0x01);
    wr8(BG12NBA, BG12_NBA);
    page_table_put();
    if (screen_w != W || screen_h != H) {
        nshvc_ext_screen(W, H);
        nshvc_ext_screen_origin(-1, origin_row());
        screen_w = W;
        screen_h = H;
    }
}

/* the original's: BG1's map and characters for the buffer shown ($0D81: 1
   buffer B), channel 7's registers, the console's screen */
static void bg1_back(void)
{
    static const uint32_t ch7[5] = {DMAP7, BBAD7, A1T7L, A1T7L + 1, A1B7};
    const uint8_t *w = nshvc_wram();
    int b = (w[0x0D81] | w[0x0D82] << 8) == 0x0001, i;

    wr8(BG1SC, b ? 0x7C : 0x78);
    wr8(BG12NBA, b ? 0x62 : 0x60);
    if (ch7_set)
        for (i = 0; i < 5; i++)
            wr8(ch7[i], ch7_kept[i]);
    ch7_set = 0;
    page_table_take();
    sky_take();
    if (screen_w != 256 || screen_h != 224) {
        nshvc_ext_screen(256, 224);
        nshvc_ext_screen_origin(-1, -1);
        screen_w = 256;
        screen_h = 224;
    }
}

/* the screen the full window's: the sprites, BG1 on page 1, the view's
   window off and the screen on (the stage's blank was for its uploads);
   work the original never did, so it takes none of the S-CPU's time (an
   interrupt's accesses are timed, contract 2.2) */
static void takeover(void)
{
    uint8_t *w = nshvc_wram();

    nshvc_set_cpu_costs(0);
    wr8(TM, 0x01);                      /* BG1 alone: the backdrop and the sprites are in its texels */
    wr8(TMW, 0x00);
    bg1_set();
    wr8(HDMAEN, (uint8_t)(w[0x0D45] | 0x80));
    nshvc_ext_layer_page(0, 1);
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
    int was = on;


    /* the race screen's stages run once a frame (from the hook while the
       full window is on) */
    unseen = seen ? 0 : unseen + 1;
    seen = 0;
    /* only in a race frame's wait for its step (this hook runs in it every
       frame of the race): the pause, the end's fades and the screens
       before and after keep the original's screen and their own timing */
    safe_area_update();
    on = unseen <= 1 && rm_enhanced() && rm_option(RM_OPT_WIDESCREEN) && rm_canvas_ready() && !nshvc_wram()[0x0D62] && rm_whole_steps() &&
         rm_in_tick_wait() && size_for_window();
    /* the writes here are the full window's own, in no original's time:
       made with interrupts held, where the console's time does not pass
       (and only when there are any: a hold may move where a due interrupt
       is taken) */
    if (!(on && (map_w != W || map_h != H)) && !(was && !on)) {
        staged = 0;
        return;
    }
    nshvc_hold_interrupts(1);
    if (on && (map_w != W || map_h != H))  /* VRAM's pages 1 and 2 and BG1's maps there, for this size */
        maps_write();
    if (was && !on) {                   /* back to the original's screen */
        nshvc_set_cpu_costs(0);
        bg1_back();
        wr8(HDMAEN, nshvc_wram()[0x0D45]);
        nshvc_ext_layer_page(0, 0);
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
            rm_canvas_show(b ? 0x2200 : 0x0000, b ? 0x9400 : 0x2C00, 416);
        }
        handed = 0;
        nshvc_set_cpu_costs(1);
    }
    nshvc_hold_interrupts(0);
    staged = 0;
}
