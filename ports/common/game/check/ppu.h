/*
 * ppu.h: the checks' model of the video hardware the decomp writes (never
 * the port's): video RAM, the palette, the sprite memory, the registers,
 * and the transfer channels that feed them. The port's video state
 * (video.h) is compared with it.
 */
#ifndef SRFX_CHECK_PPU_H
#define SRFX_CHECK_PPU_H

#include <stdint.h>

typedef struct {
    uint8_t vram[0x10000];
    uint8_t cgram[0x200];
    uint8_t oam[0x220];
    uint8_t reg[0x100];                 /* $21xx as last written */
    uint8_t dma[0x80];                  /* $43xx */
    uint16_t vaddr, oaddr, oaddr_reload;
    uint16_t hofs[4], vofs[4];
    uint8_t ofs_latch, hofs_latch;
    uint8_t fixed[3];                   /* the fixed colour's red, green, blue */
    uint8_t vmain, cgaddr, cg_latch, cg_high, oam_latch;
    int cg_second;
} Ppu;

extern Ppu ppu;
void ppu_reset(void);
void ppu_write(uint16_t reg, uint8_t v);       /* $2100-$21FF, $4300-$437F, $420B */
/* the first difference between the model and the port's store, palette
   (its current line) and sprite table, described into `out`; 0 if none */
int ppu_differs(char *out, unsigned size);
/* the port's video state loaded from the model's (to start both alike) */
void ppu_to_port(void);
/* the screen's settings the registers make, by meaning */
#include "video.h"
void ppu_screen(VideoScreen *s);
/* the first difference between them and the port's current settings */
int ppu_screen_differs(char *out, unsigned size);

#endif
