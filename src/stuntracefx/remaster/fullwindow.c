/*
 * fullwindow.c: Enhanced mode's full-window race (fullwindow.h).
 *
 *   the screen   W x 224 console pixels, W from the window's aspect
 *                (nshvc_ext_screen), the original's 256 at 4:3;
 *   BG1          on VRAM page 1: its map names a character for each of
 *                the screen's cells (row by row, (W / 8) x 28 of them,
 *                from page 1's word 0), which the canvas stands in for;
 *                both buffers' maps ($7800 with the characters at word 0,
 *                $7C00 with them at $2000) name the same, so the game's
 *                flips between them change nothing; its scroll 0, its
 *                window off;
 *   the texels   (W k) x (224 k), drawn by the redraw with the view over
 *                the whole screen (fw_view_words), handed to the canvas
 *                each frame of the race's wait (fw_picture);
 *   the uploads  the race's raster stage that blanks the screen's last
 *                lines for its uploads runs in the vertical blank instead
 *                and leaves the screen on, so every line shows the picture.
 *
 * The view: its centre W / 2 across and the original's (32 + 64 = line 96)
 * down, focal length 157.5 (the original's 104 / 128 half-width at 4:3)
 * whatever the width, so the field of view across grows with the window
 * and the one up and down stays; all of it k times in texels.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int W = 256, K = 1;              /* the screen's width, texels a console pixel */
static int tw, th;                      /* the texels' size */
static uint8_t *tex, *chars;
static size_t tex_size;
static int map_w;                       /* the width BG1's maps were written for */
static int handed;                      /* the canvas is the full window's */
static int staged;                      /* this frame's upload stage ran in the full window */

static void panels_draw(void);
static void sprites_write(int full);
static void backdrop_draw(void);

/* the view on the original's screen: its column 0 and row 0 (BG1's map
   names the view's character 0 at row 2, column 3, under the race's
   scroll), its size, and its centre line VIEW_Y + 64 */
#define VIEW_X 24
#define VIEW_Y 32
#define VIEW_W 208
#define VIEW_H 128
#define VIEW_CY 96

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

void fw_plot(uint16_t x, uint16_t y)
{
    int c = plot_colour(), i, j;

    if (c < 0)
        return;
    if (fw_hud) {
        int px = hud_x((uint8_t)x), py = hud_y((uint8_t)y);
        uint8_t *o;
        if (px < 0 || px >= W || py < 0 || py >= 224)
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
    if (fw_hud) {
        int px = hud_x((uint8_t)x), py = hud_y((uint8_t)y);
        if (px < 0 || px >= W || py < 0 || py >= 224)
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
    int i, f = fw_focal(), below = th - VIEW_CY * K;

    if (!fw_drawing || fw_hud)
        return 0;
    for (i = 0; i < 6; i++)
        if (w[i] != k_race1p[i])
            return 0;
    out[0] = (uint16_t)(tw / 2);        /* the centre */
    out[1] = (uint16_t)(VIEW_CY * K);
    out[2] = 0;                         /* the edges, as the original's: the right one
                                           its last column, the bottom one past its last row */
    out[3] = (uint16_t)(tw - 1);
    out[4] = 0;
    out[5] = (uint16_t)th;
    /* the frustum planes (view.c's rule) for the half-width and the larger
       of the parts above and below the centre */
    out[6] = (uint16_t)(((uint32_t)f << 14) / (uint32_t)(tw / 2));
    out[7] = (uint16_t)(((uint32_t)f << 13) / (uint32_t)(below > VIEW_CY * K ? below : VIEW_CY * K));
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

static uint16_t reciprocal(uint32_t n)
{
    uint32_t at = RECIPROCALS + 2u * n;
    if (n < 256)
        return (uint16_t)(gsu_rd8(at) | gsu_rd8(at + 1) << 8);
    return (uint16_t)(32768u / n);
}

/* an edge's slope in 24.8: dx times the reciprocal of its height, the
   product's bits from 7 up; past 255 rows the division it stands for */
static int32_t slope(int32_t dx, uint32_t rows)
{
    if (rows >= 256)
        return (int32_t)(((int64_t)dx * 256) / (int64_t)rows);
    return (int32_t)(((int64_t)dx * (int16_t)reciprocal(rows)) >> 7);
}

/* an edge's x stepped: set to 0 when it has gone just below it (to -8.0) */
static int32_t edge_step(int32_t x, int32_t step)
{
    x += step;
    if (x < 0 && x >= -0x800)
        x = 0;
    return x;
}

static int32_t vw(uint16_t a) { return (int16_t)ram16(a); }

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
    int32_t ymin = 0x7FFF, ymax = -0x8000, lrows, rrows, row, lx, rx, ls = 0, rs = 0;
    int i;
    gsu.por = 0;
    gsu_color(ram8(p));
    for (i = 0; i < n; i++) {                  /* the vertices copied below */
        int32_t y;
        ram16w(dst, ram16(src));
        dst = (uint16_t)(dst + 2);
        src = (uint16_t)(src + 2);
        ram16w(dst, ram16(src));
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
        int32_t from = lx, to, y;
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
            int32_t from = (rx + 0x7F) >> 8, to, y;
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
            int32_t l = lx >> 8, r = rx >> 8;
            if (r >= l)
                flat_row(l, r, row);
            lx = edge_step(lx, ls);
            lrows--;
            rx = edge_step(rx, rs);
            rrows--;
            row++;
        } while (lrows && rrows);
        if (lrows)
            continue;
        {
            int32_t from = (lx + 0x7F) >> 8, to, y;
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
    int32_t x;                  /* 24.8 */
    int32_t slope, rows;
    uint16_t at;
    uint16_t u, v, du, dv;      /* 8.8 */
} TexEdge;

/* A7D9 / A879: the next edge, forward (left) or back (right) */
static int tex_edge(TexEdge *e, int32_t row, int left)
{
    int32_t from, to, y;
    uint16_t u, v, u_prev, v_prev;
    to = (e->x + 0x7F) >> 8;
    u = (uint16_t)(e->u >> 8);
    v = (uint16_t)(e->v >> 8);
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
    return 1;
}

/* A662: a textured polygon of n vertices, its bank byte at p */
static uint16_t tex_polygon(uint16_t p, int n)
{
    uint8_t bank = ram8(p);
    uint16_t word = ram16((uint16_t)(p + 1)), base = ram16((uint16_t)(p + 3)), mask = ram16((uint16_t)(p + 5));
    uint16_t v = (uint16_t)(p + 7), size = (uint16_t)(8 * n), src = v, dst = (uint16_t)(v - size), top_at = 0;
    int32_t ymin = 0x7FFF, ymax = -0x8000, row;
    TexEdge l, r;
    int i;
    gsu.por = 0;
    gsu_color((uint8_t)word);
    gsu.por = (uint8_t)((word >> 8) & 0x1F);
    for (i = 0; i < n; i++) {                  /* u and v moved down a byte */
        int32_t y = vw((uint16_t)(src + 2));
        ram16w(dst, ram16(src));
        ram16w((uint16_t)(dst + 2), (uint16_t)y);
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
        while (count--) {
            int32_t xl = l.x >> 8, xr = r.x >> 8, x;
            uint32_t width = xr >= xl ? (uint32_t)(xr - xl + 1) : 1;
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
            if (row >= 0 && row < th) {
                uint8_t *o = tex + (size_t)row * tw;
                for (x = xl; x < xl + (int32_t)width; x++) {
                    uint16_t at = (uint16_t)((((tv & 0xFF00) | tu >> 8) & mask) + base);
                    int c;
                    tv = (uint16_t)(tv + dv);
                    tu = (uint16_t)(tu + du);
                    if (x < 0 || x >= tw)
                        continue;
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
}

/* ---- the picture to the console ------------------------------------------ */

/* the screen's width for the window: its aspect at 224 lines (256 at 4:3),
   a whole number of cells; at most 256 while a canvas holds at most 1,024
   8 bpp characters (nativeshvc 2.4, one page) */
static int width_for_window(void)
{
    float ww = 0, wh = 0;
    int w;

    nshvc_window_size(&ww, &wh);
    if (!(ww > 0 && wh > 0))
        return 256;
    w = (int)(256.0f * (ww / wh) / (4.0f / 3.0f) / 8.0f + 0.5f) * 8;
    if (w < 256)
        w = 256;
    if (w > 256)
        w = 256;
    return w;
}

static int size_for_window(void)
{
    int k = nshvc_ext_canvas_scale(), w = width_for_window();
    size_t need;

    if (k < 1)
        return 0;
    if (k != K || w != W || !tex) {
        K = k;
        W = w;
        tw = W * K;
        th = 224 * K;
        need = (size_t)tw * th;
        if (need > tex_size) {
            free(tex);
            free(chars);
            tex = malloc(need);
            chars = malloc(need);
            tex_size = tex && chars ? need : 0;
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

/* BG1's two maps in page 1: the picture's cell (c, r) names character
   r (W / 8) + c, at characters from word 0 ($7800) and from $2000 ($7C00),
   in map row r - 2 under the race's scroll; cells outside the screen
   character 0 */
static void maps_write(void)
{
    unsigned m, cells = (unsigned)W / 8, r, c;

    nshvc_ext_vram(128);
    nshvc_ext_vram_direct(1);
    for (m = 0; m < 2; m++) {
        unsigned base = m ? 256 : 0;
        wr8(VMAIN, 0x80);
        wr8(VMADDL, 0x00);
        wr8(VMADDH, m ? 0x7C : 0x78);
        for (r = 0; r < 32; r++)
            for (c = 0; c < 32; c++) {
                unsigned pr = (r + RACE_ROW0 / 8) & 31, n = pr < 28 && c < cells ? pr * cells + c : 0;
                uint16_t t = (uint16_t)(((n - base) & 0x3FF) | 0x2000);    /* priority 1, as the game's */
                wr8(VMDATAL, (uint8_t)t);
                wr8(VMDATAH, (uint8_t)(t >> 8));
            }
    }
    nshvc_ext_vram_direct(-1);
    map_w = W;
}

/* the texels as the canvas's characters, row by row, each 8k x 8k */
static void hand(void)
{
    unsigned cells = (unsigned)W / 8, n = cells * 28, i, r;
    size_t side = (size_t)8 * K;

    for (i = 0; i < n; i++) {
        const uint8_t *s = tex + (size_t)(i / cells) * side * tw + (size_t)(i % cells) * side;
        uint8_t *o = chars + (size_t)i * side * side;
        for (r = 0; r < side; r++)
            memcpy(o + r * side, s + r * tw, side);
    }
    rm_canvas_taken();
    nshvc_ext_canvas_chars(0, 0x0000, (int)n, chars, K);
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
    a = rm_tween_buffers(j, RM_TICKS, gsu_px[0], gsu_px[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    checking = 1;
    fw_drawing = 1;
    memset(tex, 0, (size_t)tw * th);
    b = rm_tween_buffers(j, RM_TICKS, skip[0], skip[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    fw_drawing = 0;
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
static long kc_pictures;
static double kc_sum, kc_worst[5];
static uint32_t kc_worst_frame[5];

static void kcheck_report(void)
{
    int i;
    fprintf(stderr, "fwkcheck: k %d, %ld pictures, %.2f%% of pixels differ on average; worst:", K, kc_pictures,
            kc_pictures ? 100.0 * kc_sum / kc_pictures : 0.0);
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
    if (!one && !(one = malloc((size_t)W * 224)))
        return;
    tex = one;
    K = 1;
    tw = W;
    th = 224;
    fw_drawing = 1;
    rm_hud_state(0);
    memcpy(bank71, nshvc_sram() + 0x10000, sizeof bank71);
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    rm_hud_state(1);
    memcpy(nshvc_sram() + 0x10000, bank71, sizeof bank71);
    fw_drawing = 0;
    fw_hud = 0;
    tex = tex_k;
    K = k;
    tw = tw_k;
    th = th_k;
    if (!m)
        return;
    if (!kc_pictures++)
        atexit(kcheck_report);
    for (y = 0; y < 224; y++)
        for (x = 0; x < W; x++)
            differ += one[y * W + x] != tex[(size_t)(y * K + K / 2) * tw + x * K + K / 2];
    part = (double)differ / (W * 224);
    kc_sum += part;
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
            fprintf(f, "P6\n%d 224\n255\n", W);
            for (y = 0; y < 224; y++)
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

int fw_picture(int j)
{
    static uint8_t scratch[2][RM_TWEEN_BYTES];
    int m;

    if (!on)
        return 0;
    if (j == 0) {                       /* the step's own screen stages may have copied the sprite table to OAM */
        nshvc_hold_interrupts(1);
        nshvc_set_cpu_costs(0);
        sprites_write(1);
        nshvc_set_cpu_costs(1);
        nshvc_hold_interrupts(0);
    }
    if (getenv("FW_CHECK"))
        check_picture(j);
    fw_drawing = 1;
    m = rm_tween_buffers(j, RM_TICKS, scratch[0], scratch[1]);
    fw_drawing = 0;
    fw_hud = 0;
    if (!m)
        return 0;
    if (getenv("FW_KCHECK"))            /* before the backdrop and panels: the 3D and HUD alone */
        kcheck_picture(j);
    backdrop_draw();
    panels_draw();
    hand();
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
   map holds nothing the original showed, its edge rows go on (the sky
   above, the ground's colour below). */
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
        int32_t y = (VIEW_CY * 256 + ((int32_t)Y * 2 + 1 - 2 * VIEW_CY * K) * 128 * 256 / (2 * f)) >> 8;
        unsigned row, cell, py;
        if (y < VIEW_Y)                 /* past the original view's top and bottom its edge rows go on */
            y = VIEW_Y;
        if (y > VIEW_Y + VIEW_H - 1)
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
            if (c)
                o[X] = (uint8_t)((e >> 10 & 7) * 16 + c);
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
   PANEL_TOP to PANEL_END, drawn over the 3D at the console's pixel size,
   less the bezel's background around the panels' boxes: the pixels of its
   colour (the band's left edge's) reached from the band's edges, the
   boxes' outlines stopping it */
#define PANEL_TOP 161
#define PANEL_END 208
#define PANEL_ROWS (PANEL_END - PANEL_TOP)

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
    for (y = 0; y < PANEL_ROWS; y++)
        for (x = 0; x < 256; x++)
            if (keep[y][x]) {
                uint8_t *o = tex + (size_t)(PANEL_TOP + y) * K * tw + (size_t)(x + (W - 256) / 2) * K;
                for (j = 0; j < K; j++, o += tw)
                    for (i = 0; i < K; i++)
                        o[i] = px[y][x];
            }
}

/* ---- the screen ------------------------------------------------------------ */

/* The sprites: the game's table (work RAM $0375: 512 bytes, then the 32
   of each sprite's X bit 8 and size) to OAM whole, in the vertical blank
   after the game's own copies of parts of it. In the full window the
   sprites the original's screen kept out of sight are hidden: the
   bezel's sides (characters $40-$47, palette 3), those wholly in the
   lines it blanked (above 16, from 208, and wrapped past 255 into the
   top), and those under the bezel (below priority 3, which BG1's
   priority-1 tiles cover, and wholly outside the view); the race's
   sprites are 8 and 16 pixels (OBSEL $03). */
#define OAM_TABLE 0x0375

static void sprites_write(int full)
{
    const uint8_t *o = nshvc_wram() + OAM_TABLE;
    int i;

    wr8(OAMADDL, 0x00);
    wr8(OAMADDH, 0x00);
    for (i = 0; i < 128; i++) {
        unsigned y = o[i * 4 + 1], h = (o[512 + i / 4] >> (2 * (i & 3)) & 2) ? 16 : 8;
        unsigned t = o[i * 4 + 2] | (o[i * 4 + 3] & 1) << 8, a = o[i * 4 + 3];
        int x = o[i * 4] | (o[512 + i / 4] >> (2 * (i & 3)) & 1) << 8;
        int hide;
        if (x >= 256)
            x -= 512;
        hide = full && (((t & 0x1F8) == 0x40 && (a & 0x0E) == 0x06) || y + h > 256 || y >= PANEL_END ||
                        y + h <= RACE_ROW0 ||
                        ((a & 0x30) != 0x30 && (x + (int)h <= VIEW_X || x >= VIEW_X + VIEW_W ||
                                                (int)(y + h) <= VIEW_Y || (int)y >= VIEW_Y + VIEW_H)));
        wr8(OAMDATA, o[i * 4]);
        wr8(OAMDATA, hide ? 0xF0 : (uint8_t)y);
        wr8(OAMDATA, o[i * 4 + 2]);
        wr8(OAMDATA, o[i * 4 + 3]);
    }
    for (i = 0; i < 32; i++)
        wr8(OAMDATA, o[512 + i]);
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
    return on ? 0xF0 : 0xD0;
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

void fw_upload_done(void)
{
    uint8_t *w = nshvc_wram();

    if (!on)
        return;
    /* the sprites, BG1 on page 1, the view's window off and the screen on
       again (the stage's blank was for its uploads); work the original
       never did, so it takes none of the S-CPU's time (an interrupt's
       accesses are timed, contract 2.2) */
    nshvc_set_cpu_costs(0);
    sprites_write(1);
    wr8(TM, 0x11);                      /* BG1 and the sprites: the backdrop is in BG1's texels */
    wr8(TMW, 0x00);
    nshvc_ext_layer_page(0, 1);
    wr8(INIDISP, w[0x0D61]);
    nshvc_set_cpu_costs(1);
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
    on = unseen <= 1 && rm_enhanced() && rm_option(RM_OPT_WIDESCREEN) && rm_canvas_ready() && !nshvc_wram()[0x0D62] && rm_whole_steps() &&
         rm_in_tick_wait() && size_for_window();
    /* the writes here are the full window's own, in no original's time:
       made with interrupts held, where the console's time does not pass
       (and only when there are any: a hold may move where a due interrupt
       is taken) */
    if (!(on && map_w != W) && !(was && !on)) {
        staged = 0;
        return;
    }
    nshvc_hold_interrupts(1);
    if (on && map_w != W)               /* VRAM's page 1 and BG1's maps there, for this width */
        maps_write();
    if (was && !on) {                   /* back to the original's screen */
        nshvc_ext_layer_page(0, 0);
        sprites_write(0);
        if (staged)
            wr8(TM, 0x13);              /* the race screen's, as the stage set it */
        if (handed)
            nshvc_ext_canvas_chars(0, 0, 0, NULL, 1);
        handed = 0;
    }
    nshvc_hold_interrupts(0);
    staged = 0;
}
