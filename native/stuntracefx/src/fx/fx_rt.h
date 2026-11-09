/*
 * fx_rt.h: what the Super FX logic reads and writes: its RAM, the cartridge
 * through a cursor, and the plot state its drawing routines set.
 */
#ifndef STUNTRACEFX_FX_RT_H
#define STUNTRACEFX_FX_RT_H

#include <stdint.h>

#include "data/cart.h"

extern uint8_t fx_ram[0x20000];

typedef struct {
    uint16_t ramaddr;
    uint8_t rambr, rombr, romdr, colr, por, scmr;
} FxState;

extern FxState fxs;

static inline uint32_t fx_ram_at(uint16_t a) { return (uint32_t)(fxs.rambr & 1) << 16 | a; }

static inline uint16_t fx_ldw(uint16_t a)
{
    fxs.ramaddr = a;
    return (uint16_t)(fx_ram[fx_ram_at(a)] | fx_ram[fx_ram_at((uint16_t)(a ^ 1))] << 8);
}

static inline uint8_t fx_ldb(uint16_t a)
{
    fxs.ramaddr = a;
    return fx_ram[fx_ram_at(a)];
}

static inline void fx_stw(uint16_t a, uint16_t v)
{
    fxs.ramaddr = a;
    fx_ram[fx_ram_at(a)] = (uint8_t)v;
    fx_ram[fx_ram_at((uint16_t)(a ^ 1))] = (uint8_t)(v >> 8);
}

static inline void fx_stb(uint16_t a, uint8_t v)
{
    fxs.ramaddr = a;
    fx_ram[fx_ram_at(a)] = v;
}

static inline void fx_sbk(uint16_t v)
{
    fx_stw(fxs.ramaddr, v);
}

static inline uint8_t fx_rom_read(uint8_t bank, uint16_t a)
{
    if (bank >= 0x70)
        return fx_ram[(uint32_t)(bank & 1) << 16 | a];
    return cart[(bank < 0x40 ? (uint32_t)bank << 15 | (a & 0x7FFFu) : (uint32_t)(bank - 0x40) << 16 | a) & (CART_SIZE - 1)];
}

/* R14: the ROM cursor; each write fetches its byte. */
static inline void fx_r14(uint16_t v) { fxs.romdr = fx_rom_read(fxs.rombr, v); }
static inline uint8_t fx_getb(void) { return fxs.romdr; }
static inline uint16_t fx_getbs(void) { return (uint16_t)(int16_t)(int8_t)fxs.romdr; }
static inline uint16_t fx_getbh(uint16_t a) { return (uint16_t)(fxs.romdr << 8 | (a & 0xFF)); }
static inline void fx_romb(uint16_t a) { fxs.rombr = (uint8_t)(a & 0x7F); }
static inline void fx_ramb(uint16_t a) { fxs.rambr = (uint8_t)(a & 1); }

static inline void fx_push(uint16_t *sp, uint16_t v) { fx_stw(*sp, v); *sp = (uint16_t)(*sp + 2); }
static inline uint16_t fx_pull(uint16_t *sp) { *sp = (uint16_t)(*sp - 2); return fx_ldw(*sp); }
static inline void fx_spill(uint16_t *sp) { fx_push(sp, 0); }
static inline void fx_unspill(uint16_t *sp) { (void)fx_pull(sp); }

static inline void fx_color(uint8_t v)
{
    if (fxs.por & 0x04)
        fxs.colr = (uint8_t)((fxs.colr & 0xF0) | v >> 4);
    else if (fxs.por & 0x08)
        fxs.colr = (uint8_t)((fxs.colr & 0xF0) | (v & 0x0F));
    else
        fxs.colr = v;
}

static inline void fx_cmode(uint16_t a) { fxs.por = (uint8_t)(a & 0x1F); }

/* The drawing the Super FX does: going over to draw lists (fx/fx_draw.c). */
void fx_list_begin(void);
void fx_list_end(void);
void fx_op_clear(void);
#ifdef FX_COUNT_PLOTS
void fx_plot_counted(const char *where);
#define fx_plot(x, y) fx_plot_counted(__func__)
#else
void fx_plot(uint8_t x, uint8_t y);
#endif
uint8_t fx_rpix(uint8_t x, uint8_t y);
uint16_t fx_sprite_shrink(uint16_t size, uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask);
uint16_t fx_sprite_magnify(uint16_t c8, uint16_t step, uint16_t left, uint16_t top, uint16_t bank, uint16_t base, uint16_t mask);
void fx_flat_row(uint16_t x, uint16_t y, uint16_t n);

#endif
