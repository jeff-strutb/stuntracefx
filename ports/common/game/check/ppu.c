/*
 * ppu.c: see ppu.h.
 */
#include <stdio.h>
#include <string.h>

#include "ppu.h"
#include "video.h"

uint8_t old_rd8(uint32_t a);

Ppu ppu;

void ppu_reset(void)
{
    memset(&ppu, 0, sizeof ppu);
}

static unsigned vstep(void)
{
    static const unsigned step[4] = {1, 32, 128, 128};

    return step[ppu.vmain & 3];
}

static void bbus(uint8_t r, uint8_t v)
{
    switch (r) {
    case 0x02:
        ppu.oaddr_reload = (uint16_t)((ppu.oaddr_reload & 0x100) | v);
        ppu.oaddr = (uint16_t)(ppu.oaddr_reload << 1);
        break;
    case 0x03:
        ppu.oaddr_reload = (uint16_t)((ppu.oaddr_reload & 0xFF) | (v & 1) << 8);
        ppu.oaddr = (uint16_t)(ppu.oaddr_reload << 1);
        break;
    case 0x04: {
        uint16_t a = (uint16_t)(ppu.oaddr & 0x3FF);

        if (a >= 0x200) {
            ppu.oam[0x200 + (a & 0x1F)] = v;
        } else if (!(a & 1)) {
            ppu.oam_latch = v;
        } else {
            ppu.oam[a - 1] = ppu.oam_latch;
            ppu.oam[a] = v;
        }
        ppu.oaddr = (uint16_t)((ppu.oaddr + 1) & 0x3FF);
        break;
    }
    case 0x15:
        ppu.vmain = v;
        break;
    case 0x16:
        ppu.vaddr = (uint16_t)((ppu.vaddr & 0xFF00) | v);
        break;
    case 0x17:
        ppu.vaddr = (uint16_t)((ppu.vaddr & 0x00FF) | v << 8);
        break;
    case 0x18:
        ppu.vram[(ppu.vaddr & 0x7FFF) * 2] = v;
        if (!(ppu.vmain & 0x80))
            ppu.vaddr = (uint16_t)(ppu.vaddr + vstep());
        break;
    case 0x19:
        ppu.vram[(ppu.vaddr & 0x7FFF) * 2 + 1] = v;
        if (ppu.vmain & 0x80)
            ppu.vaddr = (uint16_t)(ppu.vaddr + vstep());
        break;
    case 0x0D: case 0x0F: case 0x11: case 0x13: {   /* a layer's horizontal scroll: written twice */
        int l = (r - 0x0D) / 2;

        ppu.hofs[l] = (uint16_t)((v << 8 | (ppu.ofs_latch & ~7) | (ppu.hofs_latch & 7)) & 0x3FF);
        ppu.ofs_latch = v;
        ppu.hofs_latch = v;
        break;
    }
    case 0x0E: case 0x10: case 0x12: case 0x14: {
        int l = (r - 0x0E) / 2;

        ppu.vofs[l] = (uint16_t)((v << 8 | ppu.ofs_latch) & 0x3FF);
        ppu.ofs_latch = v;
        break;
    }
    case 0x32:
        if (v & 0x20) ppu.fixed[0] = v & 0x1F;
        if (v & 0x40) ppu.fixed[1] = v & 0x1F;
        if (v & 0x80) ppu.fixed[2] = v & 0x1F;
        break;
    case 0x21:
        ppu.cgaddr = v;
        ppu.cg_second = 0;
        break;
    case 0x22:
        if (!ppu.cg_second) {
            ppu.cg_latch = v;
        } else {
            ppu.cgram[ppu.cgaddr * 2] = ppu.cg_latch;
            ppu.cgram[ppu.cgaddr * 2 + 1] = (uint8_t)(v & 0x7F);
            ppu.cgaddr++;
        }
        ppu.cg_second = !ppu.cg_second;
        break;
    }
    ppu.reg[r] = v;
}

/* the B-bus registers a transfer mode writes, in turn */
static const uint8_t pattern[8][4] = {{0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
                                      {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1}};

static void dma_run(int c)
{
    uint8_t *d = &ppu.dma[c * 0x10];
    uint8_t mode = d[0], b = d[1], bank = d[4];
    uint16_t a = (uint16_t)(d[2] | d[3] << 8);
    unsigned n = (unsigned)(d[5] | d[6] << 8), k = 0;

    if (!n)
        n = 0x10000;
    if (mode & 0x80) {
        fprintf(stderr, "check: a transfer from the video registers, which the model lacks\n");
        return;
    }
    while (n--) {
        bbus((uint8_t)(b + pattern[mode & 7][k & 3]), old_rd8((uint32_t)bank << 16 | a));
        k++;
        if (!(mode & 0x08))
            a = (uint16_t)(mode & 0x10 ? a - 1 : a + 1);
    }
    d[2] = (uint8_t)a, d[3] = (uint8_t)(a >> 8);
    d[5] = d[6] = 0;
}

void ppu_write(uint16_t reg, uint8_t v)
{
    int c;

    if (reg >= 0x2100 && reg < 0x2200) {
        bbus((uint8_t)reg, v);
    } else if (reg >= 0x4300 && reg < 0x4380) {
        ppu.dma[reg - 0x4300] = v;
    } else if (reg == 0x420C) {
        ppu.hdmaen = v;
    } else if (reg == 0x420B) {
        for (c = 0; c < 8; c++)
            if (v >> c & 1)
                dma_run(c);
    }
}

int ppu_differs(char *out, unsigned size)
{
    const uint16_t *store = video_store(), *pal = video_palette();
    unsigned i;
    VideoSprite s;

    for (i = 0; i < 0x8000; i++) {
        uint16_t w = (uint16_t)(ppu.vram[2 * i] | ppu.vram[2 * i + 1] << 8);

        if (store[i] != w) {
            snprintf(out, size, "video word $%04X = %04X, the decomp's %04X", i, store[i], w);
            return 1;
        }
    }
    for (i = 0; i < 256; i++) {
        uint16_t c = (uint16_t)(ppu.cgram[2 * i] | ppu.cgram[2 * i + 1] << 8);

        if (pal[i] != c) {
            snprintf(out, size, "colour %u = %04X, the decomp's %04X", i, pal[i], c);
            return 1;
        }
    }
    for (i = 0; i < 128; i++) {
        uint8_t hi = (uint8_t)(ppu.oam[512 + i / 4] >> (2 * (i & 3))), *e = &ppu.oam[4 * i];
        int x = e[0] | (hi & 1) << 8;

        video_sprite_get(i, &s);
        if (s.x != (x >= 256 ? x - 512 : x) || s.y != e[1] || s.tile != (e[2] | (e[3] & 1) << 8)
            || s.palette != ((e[3] >> 1) & 7) || s.priority != ((e[3] >> 4) & 3) || s.flip_h != ((e[3] >> 6) & 1)
            || s.flip_v != (e[3] >> 7) || s.large != ((hi >> 1) & 1)) {
            snprintf(out, size, "sprite %u differs", i);
            return 1;
        }
    }
    return 0;
}

void ppu_to_port(void)
{
    video_line_tables_on(ppu.hdmaen);
    ppu_screen(video_screen());
    video_store_write(0, ppu.vram, sizeof ppu.vram);
    video_colours(0, ppu.cgram, 256);
    video_sprite_table(ppu.oam, 0, sizeof ppu.oam);
}

void ppu_screen(VideoScreen *s)
{
    const uint8_t *g = ppu.reg;
    int l;

    memset(s, 0, sizeof *s);
    s->brightness = g[0x00] & 0x0F;
    s->blank = g[0x00] >> 7;
    s->sprite_sizes = g[0x01] >> 5;
    s->sprite_gap = (g[0x01] >> 3) & 3;
    s->sprite_tiles = (uint16_t)((g[0x01] & 7) << 13);
    s->mode = g[0x05] & 7;
    s->layer3_priority = (g[0x05] >> 3) & 1;
    s->mosaic = g[0x06];
    for (l = 0; l < 4; l++) {
        VideoLayer *y = &s->layer[l];
        uint8_t sc = g[0x07 + l], nba = g[0x0B + l / 2];

        y->map = (uint16_t)((sc & 0xFC) << 8);
        y->map_wide = sc & 1;
        y->map_tall = (sc >> 1) & 1;
        y->tiles = (uint16_t)(((l & 1 ? nba >> 4 : nba) & 0x0F) << 12);
        y->large_tiles = (g[0x05] >> (4 + l)) & 1;
        y->scroll_x = ppu.hofs[l];
        y->scroll_y = ppu.vofs[l];
    }
    s->window_left[0] = g[0x26], s->window_right[0] = g[0x27];
    s->window_left[1] = g[0x28], s->window_right[1] = g[0x29];
    s->window_use[0] = g[0x23] & 0x0F, s->window_use[1] = g[0x23] >> 4;
    s->window_use[2] = g[0x24] & 0x0F, s->window_use[3] = g[0x24] >> 4;
    s->window_use[4] = g[0x25] & 0x0F, s->window_use[5] = g[0x25] >> 4;
    for (l = 0; l < 4; l++)
        s->window_logic[l] = (g[0x2A] >> (2 * l)) & 3;
    s->window_logic[4] = g[0x2B] & 3;
    s->window_logic[5] = (g[0x2B] >> 2) & 3;
    s->main_layers = g[0x2C] & 0x1F;
    s->sub_layers = g[0x2D] & 0x1F;
    s->window_layers = g[0x2E] & 0x1F;
    s->window_sub_layers = g[0x2F] & 0x1F;
    s->colour_clip = g[0x30] >> 6;
    s->colour_prevent = (g[0x30] >> 4) & 3;
    s->colour_sub_screen = (g[0x30] >> 1) & 1;
    s->colour_direct = g[0x30] & 1;
    s->colour_subtract = g[0x31] >> 7;
    s->colour_half = (g[0x31] >> 6) & 1;
    s->colour_maths = g[0x31] & 0x3F;
    s->fixed_colour = (uint16_t)(ppu.fixed[0] | ppu.fixed[1] << 5 | ppu.fixed[2] << 10);
}

int ppu_screens_differ(const VideoScreen *b, const VideoScreen *a, char *out, unsigned size)
{
#define F(f) if (a->f != b->f) { snprintf(out, size, #f " %u, the decomp's %u", (unsigned)b->f, (unsigned)a->f); return 1; }
    F(brightness) F(blank) F(mode) F(layer3_priority) F(sprite_tiles) F(sprite_gap) F(sprite_sizes)
    F(main_layers) F(sub_layers) F(window_layers) F(window_sub_layers) F(colour_clip) F(colour_prevent)
    F(colour_sub_screen) F(colour_direct) F(colour_maths) F(colour_subtract) F(colour_half) F(fixed_colour) F(mosaic)
    {
        int l;

        for (l = 0; l < 2; l++) {
            F(window_left[l]) F(window_right[l])
        }
        for (l = 0; l < 6; l++) {
            F(window_use[l]) F(window_logic[l])
        }
        for (l = 0; l < 4; l++) {
            F(layer[l].map) F(layer[l].map_wide) F(layer[l].map_tall) F(layer[l].tiles) F(layer[l].large_tiles)
            F(layer[l].scroll_x) F(layer[l].scroll_y)
        }
    }
#undef F
    return 0;
}

int ppu_screen_differs(char *out, unsigned size)
{
    VideoScreen a;

    ppu_screen(&a);
    if (video_line_tables() != ppu.hdmaen) {
        snprintf(out, size, "line tables %02X, the decomp's %02X", video_line_tables(), ppu.hdmaen);
        return 1;
    }
    return ppu_screens_differ(video_screen(), &a, out, size);
}

/* ---- the per-line channels, as the hardware runs them ---- */

static struct {
    uint16_t a2a, indirect;
    uint8_t counter, completed, transfer;
} hdma[8];

static int hdma_active(int c)
{
    return (ppu.hdmaen >> c & 1) && !hdma[c].completed;
}

static void hdma_reload(int c)
{
    uint8_t *d = &ppu.dma[c * 0x10], data = old_rd8((uint32_t)d[4] << 16 | hdma[c].a2a);

    if (hdma[c].counter & 0x7F)
        return;
    hdma[c].counter = data;
    hdma[c].a2a++;
    hdma[c].completed = hdma[c].counter == 0;
    hdma[c].transfer = !hdma[c].completed;
    if (d[0] & 0x40) {
        int k, finished = 1;

        data = old_rd8((uint32_t)d[4] << 16 | hdma[c].a2a++);
        hdma[c].indirect = (uint16_t)(data << 8);
        for (k = c + 1; k < 8; k++)
            if (hdma_active(k))
                finished = 0;
        if (hdma[c].completed && finished)
            return;
        data = old_rd8((uint32_t)d[4] << 16 | hdma[c].a2a++);
        hdma[c].indirect = (uint16_t)(data << 8 | hdma[c].indirect >> 8);
    }
}

void ppu_frame_lines(VideoScreen out[VIDEO_LINES], uint16_t colours[VIDEO_LINES][256])
{
    static const unsigned lengths[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    int c, line, i;

    for (c = 0; c < 8; c++)
        hdma[c].completed = 0, hdma[c].transfer = 0;
    for (c = 0; c < 8; c++) {
        hdma[c].transfer = 1;
        if (!(ppu.hdmaen >> c & 1))
            continue;
        hdma[c].a2a = (uint16_t)(ppu.dma[c * 0x10 + 2] | ppu.dma[c * 0x10 + 3] << 8);
        hdma[c].counter = 0;
        hdma_reload(c);
    }
    for (line = 0; line < VIDEO_LINES; line++) {
        for (c = 0; c < 8; c++) {
            uint8_t *d = &ppu.dma[c * 0x10];
            unsigned k;

            if (!hdma_active(c) || !hdma[c].transfer)
                continue;
            for (k = 0; k < lengths[d[0] & 7]; k++) {
                uint32_t at = d[0] & 0x40 ? (uint32_t)d[7] << 16 | hdma[c].indirect++ : (uint32_t)d[4] << 16 | hdma[c].a2a++;

                bbus((uint8_t)(d[1] + pattern[d[0] & 7][k]), old_rd8(at));
            }
        }
        ppu_screen(&out[line]);
        for (i = 0; i < 256; i++)
            colours[line][i] = (uint16_t)(ppu.cgram[2 * i] | ppu.cgram[2 * i + 1] << 8);
        for (c = 0; c < 8; c++) {
            if (!hdma_active(c))
                continue;
            hdma[c].counter--;
            hdma[c].transfer = hdma[c].counter >> 7;
            hdma_reload(c);
        }
    }
}
