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
    video_store_write(0, ppu.vram, sizeof ppu.vram);
    video_colours(0, ppu.cgram, 256);
    video_sprite_table(ppu.oam, 0, sizeof ppu.oam);
}
