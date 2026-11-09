/*
 * fx_draw.c: the Super FX's fill work as shapes for the GPU.
 *
 * The frame program queues its polygons in Super FX RAM from $08AA. Where
 * the original chip walked each polygon's edges and plotted its rows, here
 * each shape in the queue is sent to a draw list with the engine's exact
 * fill rule, so the GPU decides the pixels.
 */
#include <stdlib.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "system/state.h"
#include "video/video.h"

uint8_t fx_ram[0x20000];
FxState fxs;

/*
 * A picture is kept as its list of shapes, so that what is on the screen
 * can be put together from it whenever it is wanted: one player shows one
 * picture, two players show both at once in one layer.
 */
typedef struct {
    uint32_t first, count;
    uint32_t image;             /* 0: the vertices' colours */
    uint8_t lines;
} Shape;

typedef struct {
    nshvc_vertex *v;
    Shape *s;
    uint32_t nv, ns, cap_v, cap_s;
} Recording;

static nshvc_draw_list target;  /* a list drawn into directly (the checks) */
static uint32_t target_w, target_h;
static Recording recordings[2], *recording;
static int offset_x, offset_y;
static uint32_t generation[2], composed_generation[2];
static nshvc_draw_list screen_list;
static int composed_picture = -1, composed_split = -1;

void fx_draw_set_target(nshvc_draw_list list, uint32_t width, uint32_t height)
{
    target = list;
    target_w = width;
    target_h = height;
    recording = NULL;
    offset_x = offset_y = 0;
}

void fx_draw_init(void)
{
    screen_list = nshvc_draw_list_create();
}

/* Where each picture's pixel (x, y) goes on the screen. */
void fx_draw_begin(int picture)
{
    picture &= 1;
    target.id = 0;
    recording = &recordings[picture];
    if (!g.split_screen) {
        offset_x = 24;
        offset_y = 32;
    } else if (!picture) {
        offset_x = 16;
        offset_y = 23;
    } else {
        offset_x = 96;
        offset_y = 127;
    }
    generation[picture]++;
}

static void emit(const nshvc_vertex *v, uint32_t count, uint32_t image, int lines)
{
    Recording *r = recording;
    nshvc_material m;
    uint32_t i;

    if (!r) {
        if (!target.id)
            return;
        memset(&m, 0, sizeof m);
        m.image.id = image;
        if (lines)
            nshvc_draw_lines(target, v, count, image ? &m : NULL);
        else
            nshvc_draw_tris(target, v, count, image ? &m : NULL);
        return;
    }
    if (r->nv + count > r->cap_v) {
        r->cap_v = r->cap_v ? r->cap_v * 2 : 4096;
        while (r->nv + count > r->cap_v)
            r->cap_v *= 2;
        r->v = realloc(r->v, r->cap_v * sizeof *r->v);
    }
    if (r->ns == r->cap_s) {
        r->cap_s = r->cap_s ? r->cap_s * 2 : 1024;
        r->s = realloc(r->s, r->cap_s * sizeof *r->s);
    }
    if (!r->v || !r->s)
        abort();
    r->s[r->ns].first = r->nv;
    r->s[r->ns].count = count;
    r->s[r->ns].image = image;
    r->s[r->ns].lines = (uint8_t)lines;
    r->ns++;
    for (i = 0; i < count; i++) {
        r->v[r->nv + i] = v[i];
        r->v[r->nv + i].x += (float)offset_x;
        r->v[r->nv + i].y += (float)offset_y;
    }
    r->nv += count;
    generation[r - recordings]++;
}

static void compose_recording(const Recording *r)
{
    uint32_t i;
    nshvc_material m;

    memset(&m, 0, sizeof m);
    for (i = 0; i < r->ns; i++) {
        const Shape *s = &r->s[i];

        m.image.id = s->image;
        if (s->lines)
            nshvc_draw_lines(screen_list, r->v + s->first, s->count, s->image ? &m : NULL);
        else
            nshvc_draw_tris(screen_list, r->v + s->first, s->count, s->image ? &m : NULL);
    }
}

/* The screen's layer: picture n, or with two players both. */
nshvc_draw_list fx_draw_picture(int picture)
{
    int split = g.split_screen != 0, two = split;

    picture &= 1;
    if (composed_picture != picture || composed_split != split
        || composed_generation[picture] != generation[picture]
        || (two && composed_generation[!picture] != generation[!picture])) {
        nshvc_draw_list_clear(screen_list, 256, 224);
        nshvc_draw_rule(screen_list, NSHVC_FILL_EXACT);
        race_frame_art_draw(screen_list);
        if (two) {
            compose_recording(&recordings[0]);
            compose_recording(&recordings[1]);
            composed_generation[0] = generation[0];
            composed_generation[1] = generation[1];
        } else {
            compose_recording(&recordings[picture]);
            composed_generation[picture] = generation[picture];
        }
        composed_picture = picture;
        composed_split = split;
    }
    return screen_list;
}

void fx_list_begin(void) {}
void fx_list_end(void) {}

void fx_op_clear(void)
{
    if (recording) {
        recording->nv = recording->ns = 0;
        generation[recording - recordings]++;
    } else if (target.id) {
        nshvc_draw_list_clear(target, target_w, target_h);
        nshvc_draw_rule(target, NSHVC_FILL_EXACT);
    }
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

static void blocks_flush(void);

/* The vertices of a textured quad over the pixels [x0, x1) x [y0, y1), with
   u = u0 + i du and v = v0 + j dv at the centre of column i and row j. The
   exact rule puts a whole-pixel vertex 1/64 and 1/128 past its place. */
static void quad(nshvc_image image, int x0, int x1, int y0, int y1, double u0, double du, double v0, double dv)
{
    nshvc_vertex v[6];
    nshvc_material m;
    double ul = u0 - (0.5 - 1.0 / 64) * du, ur = u0 + ((x1 - x0) - 0.5 + 1.0 / 64) * du;
    double vt = v0 - (0.5 - 1.0 / 128) * dv, vb = v0 + ((y1 - y0) - 0.5 + 1.0 / 128) * dv;

    if ((!target.id && !recording) || x1 <= x0 || y1 <= y0)
        return;
    blocks_flush();
    memset(&m, 0, sizeof m);
    m.image = image;
    memset(v, 0, sizeof v);
    v[0].x = (float)x0; v[0].y = (float)y0; v[0].u = (float)ul; v[0].v = (float)vt; v[0].colour = 1;
    v[1].x = (float)x1; v[1].y = (float)y0; v[1].u = (float)ur; v[1].v = (float)vt; v[1].colour = 1;
    v[2].x = (float)x1; v[2].y = (float)y1; v[2].u = (float)ur; v[2].v = (float)vb; v[2].colour = 1;
    v[3] = v[0];
    v[4] = v[2];
    v[5].x = (float)x0; v[5].y = (float)y1; v[5].u = (float)ul; v[5].v = (float)vb; v[5].colour = 1;
    emit(v, 6, m.image.id, 0);
}

void fx_flat_row(uint16_t x, uint16_t y, uint16_t n)
{
    nshvc_vertex v[6];
    uint8_t colour = fxs.colr;
    int i;

    if ((!target.id && !recording) || !colour)
        return;
    blocks_flush();
    memset(v, 0, sizeof v);
    v[0].x = x; v[0].y = y;
    v[1].x = (float)(x + n); v[1].y = y;
    v[2].x = (float)(x + n); v[2].y = (float)(y + 1);
    v[3] = v[0];
    v[4] = v[2];
    v[5].x = x; v[5].y = (float)(y + 1);
    for (i = 0; i < 6; i++)
        v[i].colour = colour;
    emit(v, 6, 0, 0);
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

/* ---- glyphs: two bits a pixel, through a table of four colours ---- */

typedef struct {
    uint8_t bank;
    uint16_t base;
    uint16_t width, rows, stride;
    uint8_t colours[4];
    nshvc_image image;
} GlyphKey;

#define KNOWN_GLYPHS 4096
#define GLYPH_ROWS 128
static GlyphKey glyph_known[KNOWN_GLYPHS];
static int glyph_count;

static nshvc_image glyph_image(uint8_t bank, uint16_t base, uint16_t width, uint16_t rows, uint16_t stride, const uint8_t colours[4])
{
    static uint8_t t[256 * 256];
    int k, x, y;

    rows = GLYPH_ROWS;
    for (k = 0; k < glyph_count; k++)
        if (glyph_known[k].bank == bank && glyph_known[k].base == base && glyph_known[k].width == width
            && glyph_known[k].rows == rows && glyph_known[k].stride == stride
            && !memcmp(glyph_known[k].colours, colours, 4))
            return glyph_known[k].image;
    for (y = 0; y < rows; y++)
        for (x = 0; x < width; x++) {
            uint8_t b = fx_rom_read(bank, (uint16_t)(base + y * stride + x / 4));

            t[y * width + x] = colours[(b >> (2 * (x & 3))) & 3];
        }
    if (glyph_count == KNOWN_GLYPHS)
        glyph_count = 0;
    glyph_known[glyph_count].bank = bank;
    glyph_known[glyph_count].base = base;
    glyph_known[glyph_count].width = width;
    glyph_known[glyph_count].rows = rows;
    glyph_known[glyph_count].stride = stride;
    memcpy(glyph_known[glyph_count].colours, colours, 4);
    glyph_known[glyph_count].image = nshvc_image_create(width, rows, NSHVC_IMAGE_INDEXED);
    nshvc_image_write(glyph_known[glyph_count].image, t);
    return glyph_known[glyph_count++].image;
}

/* The block being gathered from consecutive rows of a glyph. */
static struct {
    int open;
    uint8_t bank, colours[4];
    uint16_t x, y, width, rows, base, stride;
} block;

static void blocks_flush(void)
{
    if (block.open) {
        nshvc_image image = glyph_image(block.bank, block.base, block.width, block.rows, block.rows > 1 ? block.stride : 32, block.colours);

        block.open = 0;
        quad(image, block.x, block.x + block.width, block.y, block.y + block.rows, 0, 1, 0, 1);
    }
}

void fx_draw_flush(void) { blocks_flush(); }

void fx_bitmap_row(uint16_t x, uint16_t y, uint16_t n, uint16_t palette, uint16_t base)
{
    uint8_t colours[4];
    uint16_t next;
    int i;

    for (i = 0; i < 4; i++)
        colours[i] = fx_ram[(uint16_t)(palette + i)];
    if (block.open && block.x == x && block.width == n && block.bank == fxs.rombr
        && !memcmp(block.colours, colours, 4) && y == (uint16_t)(block.y + block.rows)) {
        next = (uint16_t)(base - block.base);
        if (block.rows == 1)
            block.stride = next;
        if (next == (uint16_t)(block.stride * block.rows)) {
            block.rows++;
            return;
        }
    }
    blocks_flush();
    block.open = 1;
    block.bank = fxs.rombr;
    memcpy(block.colours, colours, 4);
    block.x = x;
    block.y = y;
    block.width = n;
    block.rows = 1;
    block.base = base;
    block.stride = 0;
}

void fx_scaled_row_start(void) {}

/* One row of a glyph drawn scaled: pixel k shows column ((tu + k) du) / 256 of the row at base. */
void fx_scaled_row(uint16_t x, uint16_t y, uint16_t n, uint16_t tu, uint16_t du, uint16_t base, uint16_t palette)
{
    uint8_t colours[4];
    uint16_t start = (uint16_t)(fx_ram[0x0216] | fx_ram[0x0217] << 8);
    int i, row;
    nshvc_image image;

    if ((!target.id && !recording) || !n)
        return;
    blocks_flush();
    for (i = 0; i < 4; i++)
        colours[i] = fx_ram[(uint16_t)(palette + i)];
    row = (uint16_t)(base - start) / 32;
    image = glyph_image(fxs.rombr, start, 128, GLYPH_ROWS, 32, colours);
    quad(image, x, x + n, y, y + 1, (double)((uint32_t)tu * du) / 256.0, du / 256.0, row + 0.5, 0);
}

/* The static: rows from a square of noise, the original's own generator's,
   made once. */
static nshvc_image noise_image;

static uint16_t noise_step(uint16_t s)
{
    uint16_t t = (uint16_t)(s ^ (s << 8)), r;

    t = (uint16_t)(t >> 8 | t << 8);
    r = (uint16_t)(((uint16_t)((uint16_t)(s & 0xFF) << 1) ^ t) >> 1);
    r ^= 0xFF00;
    if ((((s & 0xFF) << 1) ^ t) & 1)
        r ^= 0x8180;
    else if (r == 0xAA55)
        r = 0;
    return r;
}

void fx_noise_row(uint16_t x, uint16_t y, uint16_t n, uint16_t seed, uint16_t ramp)
{
    (void)ramp;
    if (!noise_image.id) {
        static uint8_t t[256 * 256];
        uint16_t s = 0x1234;
        int i;

        for (i = 0; i < 256 * 256; i++) {
            s = noise_step(s);
            t[i] = (uint8_t)(0x80 + 0x10 * (s & 7));
        }
        noise_image = nshvc_image_create(256, 256, NSHVC_IMAGE_INDEXED);
        nshvc_image_write(noise_image, t);
    }
    quad(noise_image, x, x + n, y, y + 1, seed & 0xFF, 1, (seed >> 8) + 0.5, 0);
}

/* The rows of an object sprite are sent whole by fx_sprite_shrink and
   fx_sprite_magnify. */
void fx_row_open(uint8_t kind, uint16_t x, uint16_t y, uint16_t tu, uint16_t du, uint16_t tv, uint16_t base, uint16_t dv) {}
void fx_row_close(uint16_t x) {}

void fx_plot(uint8_t x, uint8_t y) { fx_flat_row(x, y, 1); }
uint8_t fx_rpix(uint8_t x, uint8_t y) { (void)x; (void)y; return 0; }

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
    if ((!target.id && !recording) || !colour)
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
            emit(q, 6, 0, 0);
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
        emit(v, (uint32_t)out, 0, 0);
}

static void line(uint16_t p)
{
    nshvc_vertex v[2];
    uint8_t colour = fx_ram[p];

    fxs.por = 0;
    fxs.colr = colour;
    if ((!target.id && !recording) || !colour)
        return;
    memset(v, 0, sizeof v);
    v[0].x = fx_ram[(uint16_t)(p + 1)];
    v[0].y = fx_ram[(uint16_t)(p + 3)];
    v[1].x = fx_ram[(uint16_t)(p + 5)];
    v[1].y = fx_ram[(uint16_t)(p + 7)];
    v[0].colour = v[1].colour = colour;
    emit(v, 2, 0, 1);
}

static void textured_polygon(uint16_t p, int n)
{
    nshvc_vertex v[8 * 3];
    nshvc_material m;
    uint8_t bank = fx_ram[p];
    uint16_t word = rd((uint16_t)(p + 1)), base = rd((uint16_t)(p + 3)), mask = rd((uint16_t)(p + 5));
    uint16_t at = (uint16_t)(p + 7);
    uint8_t colour = (uint8_t)word, options = (uint8_t)((word >> 8) & 0x1F);
    double u_off, v_off;
    int i, out = 0;

    fxs.por = options;
    fxs.colr = colour;
    if (!target.id && !recording)
        return;
    u_off = (int16_t)rd(0x00C0) / 256.0;
    v_off = (int16_t)rd(0x00C2) / 256.0;
    memset(&m, 0, sizeof m);
    m.image = texture(bank, base, mask, colour, options);
    memset(v, 0, sizeof v);
    for (i = 1; i + 1 < n && out + 3 <= 24; i++) {
        int t[3] = {0, i, i + 1}, j;

        for (j = 0; j < 3; j++) {
            v[out].x = fx_ram[(uint16_t)(at + t[j] * 8)];
            v[out].y = fx_ram[(uint16_t)(at + t[j] * 8 + 2)];
            v[out].u = (float)(fx_ram[(uint16_t)(at + t[j] * 8 + 5)] + u_off);
            v[out].v = (float)(fx_ram[(uint16_t)(at + t[j] * 8 + 7)] + v_off);
            v[out].colour = 1;
            out++;
        }
    }
    if (out)
        emit(v, (uint32_t)out, m.image.id, 0);
}

FxDrawListResult FxDrawList(uint16_t caller_list_end, uint16_t caller_write_ptr, uint16_t *stack, uint16_t rom_at)
{
    uint16_t list_end = fx_ldw(FX_LIST_END), at = QUEUE;

    if (list_end != QUEUE) {
        blocks_flush();
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
