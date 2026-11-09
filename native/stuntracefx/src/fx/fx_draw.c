/*
 * fx_draw.c: the Super FX's fill work as shapes for the GPU.
 *
 * The frame program queues its polygons in Super FX RAM from $08AA. Where
 * the original chip walked each polygon's edges and plotted its rows, here
 * each shape in the queue is sent to a draw list with the engine's exact
 * fill rule, so the GPU decides the pixels.
 */
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"

uint8_t fx_ram[0x20000];
FxState fxs;

static nshvc_draw_list target;
static uint32_t target_w, target_h;

void fx_draw_set_target(nshvc_draw_list list, uint32_t width, uint32_t height)
{
    target = list;
    target_w = width;
    target_h = height;
}

void fx_list_begin(void) {}
void fx_list_end(void) {}

void fx_op_clear(void)
{
    if (target.id)
        nshvc_draw_list_clear(target, target_w, target_h);
    if (target.id)
        nshvc_draw_rule(target, NSHVC_FILL_EXACT);
}

/* ---- the textures: a block of the cartridge seen as 256 x 256 texels ---- */

typedef struct {
    uint8_t bank, colour, options;
    uint16_t base, mask;
    nshvc_image image;
} TexKey;

#define KNOWN_TEX 512
static TexKey known[KNOWN_TEX];
static int known_count;

static nshvc_image texture(uint8_t bank, uint16_t base, uint16_t mask, uint8_t colour, uint8_t options)
{
    static uint8_t t[256 * 256];
    int k, x, y;

    for (k = 0; k < known_count; k++)
        if (known[k].bank == bank && known[k].base == base && known[k].mask == mask
            && known[k].colour == colour && known[k].options == options)
            return known[k].image;
    for (y = 0; y < 256; y++)
        for (x = 0; x < 256; x++) {
            uint8_t v = fx_rom_read(bank, (uint16_t)(((uint16_t)(y << 8 | x) & mask) + base)), c = v;

            if (options & 0x04)
                c = (uint8_t)((colour & 0xF0) | v >> 4);
            else if (options & 0x08)
                c = (uint8_t)((colour & 0xF0) | (v & 0x0F));
            if (!(options & 0x01) && ((options & 0x08) ? !(c & 0x0F) : !c))
                c = 0;
            t[y << 8 | x] = c;
        }
    if (known_count == KNOWN_TEX)
        known_count = 0;
    known[known_count].bank = bank;
    known[known_count].base = base;
    known[known_count].mask = mask;
    known[known_count].colour = colour;
    known[known_count].options = options;
    known[known_count].image = nshvc_image_create(256, 256, NSHVC_IMAGE_INDEXED);
    nshvc_image_write(known[known_count].image, t);
    return known[known_count++].image;
}

/* ---- quads ---- */

/* The vertices of a textured quad over the pixels [x0, x1) x [y0, y1), with
   u = u0 + i du and v = v0 + j dv at the centre of column i and row j. The
   exact rule puts a whole-pixel vertex 1/64 and 1/128 past its place. */
static void quad(nshvc_image image, int x0, int x1, int y0, int y1, double u0, double du, double v0, double dv)
{
    nshvc_vertex v[6];
    nshvc_material m;
    double ul = u0 - (0.5 - 1.0 / 64) * du, ur = u0 + ((x1 - x0) - 0.5 + 1.0 / 64) * du;
    double vt = v0 - (0.5 - 1.0 / 128) * dv, vb = v0 + ((y1 - y0) - 0.5 + 1.0 / 128) * dv;

    if (!target.id || x1 <= x0 || y1 <= y0)
        return;
    memset(&m, 0, sizeof m);
    m.image = image;
    memset(v, 0, sizeof v);
    v[0].x = (float)x0; v[0].y = (float)y0; v[0].u = (float)ul; v[0].v = (float)vt; v[0].colour = 1;
    v[1].x = (float)x1; v[1].y = (float)y0; v[1].u = (float)ur; v[1].v = (float)vt; v[1].colour = 1;
    v[2].x = (float)x1; v[2].y = (float)y1; v[2].u = (float)ur; v[2].v = (float)vb; v[2].colour = 1;
    v[3] = v[0];
    v[4] = v[2];
    v[5].x = (float)x0; v[5].y = (float)y1; v[5].u = (float)ul; v[5].v = (float)vb; v[5].colour = 1;
    nshvc_draw_tris(target, v, 6, &m);
}

void fx_flat_row(uint16_t x, uint16_t y, uint16_t n)
{
    nshvc_vertex v[6];
    uint8_t colour = fxs.colr;
    int i;

    if (!target.id || !colour)
        return;
    memset(v, 0, sizeof v);
    v[0].x = x; v[0].y = y;
    v[1].x = (float)(x + n); v[1].y = y;
    v[2].x = (float)(x + n); v[2].y = (float)(y + 1);
    v[3] = v[0];
    v[4] = v[2];
    v[5].x = x; v[5].y = (float)(y + 1);
    for (i = 0; i < 6; i++)
        v[i].colour = colour;
    nshvc_draw_tris(target, v, 6, NULL);
}

/* A sprite smaller than its texture: size x size pixels, the texel at
   column k and row j being (k step / 256, j step / 256). Returns the row
   count of the original's loop. */
uint16_t fx_sprite_shrink(uint16_t size, uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask)
{
    nshvc_image image = texture((uint8_t)bank, base, 0xFFFF, fxs.colr, fxs.por);
    int x = (int16_t)left, y = (int16_t)top, count = size, skip = 0, bottom = fx_ldw(0x003E);
    int clip_w = (int16_t)fx_ldw(FX_SPRITE_CLIP_W), j0 = 0, j1 = size;
    double du = step / 256.0, u0;
    uint16_t skipped_accum = 0;
    int mirror = (mask & 0x8000) != 0;

    if (x < 0) {
        skip = -x;
        if (skip >= count)
            return 0;
        skipped_accum = (uint16_t)(skip * step);
        count -= skip;
        x = 0;
    }
    if (clip_w - x < 0)
        count += clip_w - x + 1;
    if ((int)top < 0 || (int16_t)top < 0)
        j0 = -(int16_t)top;
    if (y + j1 > bottom)
        j1 = bottom - y;
    if (j0 < j1 && count > 0) {
        if (!mirror) {
            u0 = (skipped_accum >> 8);
            quad(image, x, x + count, y + j0, y + j1, u0, du, j0 * du, du);
        } else {
            /* the original steps the column down through a 16-bit sum whose
               high byte is added to the last texel's address: every column
               but the first reads the next row of the block */
            double c0 = c8 - 1;

            if (skip == 0) {
                quad(image, x, x + 1, y + j0, y + j1, c0, 0, j0 * du, du);
                quad(image, x + 1, x + count, y + j0, y + j1, c0 - du, -du, j0 * du + 1, du);
            } else {
                quad(image, x, x + count, y + j0, y + j1, c0 - (double)(uint16_t)(0 - skipped_accum) / 256.0 + 256, -du, j0 * du, du);
            }
        }
    }
    return (uint16_t)(size * step);
}

/* A sprite larger than its texture: each texel covers about 65536 / step
   pixels. Returns the row error of the original's loop. */
uint16_t fx_sprite_magnify(uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask)
{
    nshvc_image image = texture((uint8_t)bank, base, 0xFFFF, fxs.colr, fxs.por);
    int x = (int16_t)left, y = (int16_t)top, right = fx_ldw(0x003A), bottom = fx_ldw(0x003E);
    uint32_t visible = (uint32_t)((uint32_t)step * (uint32_t)(uint16_t)(right - x)) >> 16;
    int columns = c8 < visible ? c8 : (int)visible, skipped = 0, rows, j0 = 0, j1, width;
    uint32_t frac = 0;
    double du = step / 65536.0, u0, v0;
    int mirror = (mask & 0x8000) != 0;

    if (x < 0) {
        uint32_t p = (uint32_t)step * (uint32_t)(-x);

        frac = p & 0xFFFF;
        skipped = (int)(p >> 16);
        columns -= skipped;
        x = 0;
    }
    if (columns <= 0 || !step)
        return 0;
    rows = (int)((uint32_t)(c8 - 1) * 65536u / step) + 1;
    j1 = rows;
    if (y + j1 > bottom)
        j1 = bottom - y;
    if (j1 < 0)
        j1 = 0;
    if (y < 0)
        j0 = -y;
    width = (int)(((uint64_t)columns * 65536u - frac + step - 1) / step);
    if (x + width > right + 1)
        width = right + 1 - x;
    if (j0 < j1 && width > 0) {
        v0 = (j0 * (double)step + 65535.0) / 65536.0;
        if (!mirror) {
            u0 = skipped + frac / 65536.0;
            quad(image, x, x + width, y + j0, y + j1, u0, du, v0, du);
        } else {
            u0 = c8 - 1 - skipped - frac / 65536.0 + (1.0 - 1.0 / 65536);
            quad(image, x, x + width, y + j0, y + j1, u0, -du, v0, du);
        }
    }
    return (uint16_t)(0 - (uint32_t)j1 * step);
}

/* ---- the queue ---- */

enum { QUEUE = 0x08AA };

static uint16_t rd(uint16_t a) { return (uint16_t)(fx_ram[a] | fx_ram[(uint16_t)(a + 1)] << 8); }

static void flat_polygon(uint16_t p, int n)
{
    nshvc_vertex v[8 * 3];
    uint8_t colour = fx_ram[p];
    uint16_t at = (uint16_t)(p + 1);
    int i, ymin = 999, ymax = -1, xmin = 999, xmax = -1, out = 0;

    for (i = 0; i < n; i++) {
        int x = fx_ram[(uint16_t)(at + i * 4)], y = fx_ram[(uint16_t)(at + i * 4 + 2)];

        if (y < ymin)
            ymin = y;
        if (y > ymax)
            ymax = y;
        if (x < xmin)
            xmin = x;
        if (x > xmax)
            xmax = x;
    }
    fxs.por = 0;
    fxs.colr = colour;
    if (!target.id || !colour)
        return;
    if (ymax - ymin < 2) {
        /* the original's single row: from the smallest x up to the largest */
        if (xmax > xmin) {
            nshvc_vertex q[6];

            memset(q, 0, sizeof q);
            q[0].x = (float)xmin; q[0].y = (float)ymin;
            q[1].x = (float)xmax; q[1].y = (float)ymin;
            q[2].x = (float)xmax; q[2].y = (float)ymin + 1;
            q[3] = q[0]; q[4] = q[2];
            q[5].x = (float)xmin; q[5].y = (float)ymin + 1;
            for (i = 0; i < 6; i++)
                q[i].colour = colour;
            nshvc_draw_tris(target, q, 6, NULL);
        }
        return;
    }
    memset(v, 0, sizeof v);
    for (i = 1; i + 1 < n && out + 3 <= 24; i++) {
        int t[3] = {0, i, i + 1}, j;

        for (j = 0; j < 3; j++) {
            v[out].x = fx_ram[(uint16_t)(at + t[j] * 4)];
            v[out].y = fx_ram[(uint16_t)(at + t[j] * 4 + 2)];
            v[out].colour = colour;
            out++;
        }
    }
    if (out)
        nshvc_draw_tris(target, v, (uint32_t)out, NULL);
}

static void line(uint16_t p)
{
    nshvc_vertex v[2];
    uint8_t colour = fx_ram[p];

    fxs.por = 0;
    fxs.colr = colour;
    if (!target.id || !colour)
        return;
    memset(v, 0, sizeof v);
    v[0].x = fx_ram[(uint16_t)(p + 1)];
    v[0].y = fx_ram[(uint16_t)(p + 3)];
    v[1].x = fx_ram[(uint16_t)(p + 5)];
    v[1].y = fx_ram[(uint16_t)(p + 7)];
    v[0].colour = v[1].colour = colour;
    nshvc_draw_lines(target, v, 2, NULL);
}

static void textured_polygon(uint16_t p, int n)
{
    nshvc_vertex v[8 * 3];
    nshvc_material m;
    uint8_t bank = fx_ram[p];
    uint16_t word = rd((uint16_t)(p + 1)), base = rd((uint16_t)(p + 3)), mask = rd((uint16_t)(p + 5));
    uint16_t at = (uint16_t)(p + 7);
    uint8_t colour = (uint8_t)word, options = (uint8_t)((word >> 8) & 0x1F);
    int i, out = 0;

    fxs.por = options;
    fxs.colr = colour;
    if (!target.id)
        return;
    memset(&m, 0, sizeof m);
    m.image = texture(bank, base, mask, colour, options);
    memset(v, 0, sizeof v);
    for (i = 1; i + 1 < n && out + 3 <= 24; i++) {
        int t[3] = {0, i, i + 1}, j;

        for (j = 0; j < 3; j++) {
            v[out].x = fx_ram[(uint16_t)(at + t[j] * 8)];
            v[out].y = fx_ram[(uint16_t)(at + t[j] * 8 + 2)];
            v[out].u = fx_ram[(uint16_t)(at + t[j] * 8 + 4)];
            v[out].v = fx_ram[(uint16_t)(at + t[j] * 8 + 6)];
            v[out].colour = 1;
            out++;
        }
    }
    if (out)
        nshvc_draw_tris(target, v, (uint32_t)out, &m);
}

FxDrawListResult FxDrawList(uint16_t caller_list_end, uint16_t caller_write_ptr, uint16_t *stack, uint16_t rom_at)
{
    uint16_t list_end = fx_ldw(FX_LIST_END), at = QUEUE;

    if (list_end != QUEUE) {
        fx_spill(stack);
        fx_stw(0x005E, *stack);
        fx_stw(list_end, 0);
        fx_stw(FX_LIST_END, QUEUE);
        for (;;) {
            int8_t type = (int8_t)fx_ram[at];

            at = (uint16_t)(at + 1);
            if (type >= 3) {
                flat_polygon(at, type);
                at = (uint16_t)(at + 1 + 4 * type);
            } else if (type == 2) {
                line(at);
                at = (uint16_t)(at + 9);
            } else if (type == 0) {
                break;
            } else {
                int n = (uint8_t)~type;

                textured_polygon(at, n);
                at = (uint16_t)(at + 7 + 8 * n);
            }
        }
        fx_stw(FX_LIST_END, QUEUE);
        fx_romb(fx_ldw(0x001C));
        *stack = fx_ldw(0x005E);
        fx_unspill(stack);
    }
    return (FxDrawListResult){caller_list_end, caller_write_ptr, rom_at};
}
