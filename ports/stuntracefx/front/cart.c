/*
 * cart.c: Stunt Race FX as a cartridge for nativeshvc (front.h): a Super FX
 * board with 64 KB of battery-backed RAM, its routines with C (gen/
 * routines.c), and its ROM as the named assets the build extracts (gen/
 * assets.c, tools/assets.py), with no image of the ROM: the code the C
 * stands for is held nowhere. A byte no asset holds reads 0 where the Super
 * FX's ROM buffer fills from it (it may never be used: FRONT_ROM_FETCH);
 * anywhere else it stops the game. The board and its Super FX are the
 * cartridge's own (superfx_cart.c).
 */
#include <stdlib.h>

#include "front.h"
#include "cart.h"
#include "routines.h"

static const PortAsset *find(uint32_t off)
{
    int lo = 0, hi = port_asset_count - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        const PortAsset *a = &port_assets[m];
        if (off < a->rom)
            hi = m - 1;
        else if (off >= a->rom + a->size)
            lo = m + 1;
        else
            return a;
    }
    return NULL;
}

/* the assets' bytes at their ROM offsets, and which offsets one holds: a
   read in a page no asset holds whole looks there, not through the list
   (the Super FX's textures and bitmaps, read a byte at a time); the
   bytes no asset holds are none of them */
static uint8_t *held_bytes, *held_bits;

/* a ROM byte an asset holds, for the cartridge's own Super FX: 1 and the
   byte, or 0 (none holds it: nativeshvc's path decides, cart.c's rules) */
int cart_rom_held(uint32_t off, uint8_t *v)
{
    if (!held_bits || off >= port_rom_size || !(held_bits[off >> 3] >> (off & 7) & 1))
        return 0;
    *v = held_bytes[off];
    return 1;
}

static uint8_t asset_read(uint32_t off, int who)
{
    if (held_bits && off < port_rom_size && held_bits[off >> 3] >> (off & 7) & 1)
        return held_bytes[off];
    if (!held_bits) {
        const PortAsset *a = find(off);
        if (a)
            return a->data[off - a->rom];
    }
    if (who & FRONT_ROM_FETCH) {
        front_rom_unheld = 1;
        return 0;
    }
    if (who & NSHVC_ROM_PEEK)
        return 0;
    nshvc_fatal("ROM $%06X read (by %s): no asset holds it (code the C stands for)", (unsigned)off,
                who & NSHVC_ROM_DMA ? "DMA" : who & NSHVC_ROM_CHIP ? "the Super FX" : "the S-CPU");
    return 0;
}

void cart_describe(nshvc_cart *cart)
{
    static const uint8_t **pages;
    uint32_t n = port_rom_size >> 8, p;
    pages = (const uint8_t **)calloc(n, sizeof *pages);
    if (!pages)
        nshvc_fatal("no memory for the cartridge's pages");
    held_bytes = (uint8_t *)calloc(port_rom_size, 1);
    held_bits = (uint8_t *)calloc((port_rom_size + 7) / 8, 1);
    if (!held_bytes || !held_bits)
        nshvc_fatal("no memory for the cartridge's assets");
    for (p = 0; p < (uint32_t)port_asset_count; p++) {
        const PortAsset *a = &port_assets[p];
        uint32_t i;
        for (i = 0; i < a->size && a->rom + i < port_rom_size; i++) {
            held_bytes[a->rom + i] = a->data[i];
            held_bits[(a->rom + i) >> 3] |= (uint8_t)(1u << ((a->rom + i) & 7));
        }
    }
    /* each page one asset holds whole, for reads with no lookup */
    for (p = 0; p < n; p++) {
        const PortAsset *a = find(p << 8);
        if (a && a->rom + a->size >= (p << 8) + 256)
            pages[p] = a->data + ((p << 8) - a->rom);
    }
    cart->title = "Stunt Race FX";
    cart->save_name = "stuntracefx";
    cart->map = NSHVC_CHIP;
    cart->chip = &front_superfx;
    cart->region = NSHVC_NTSC;
    cart->rom = NULL;
    cart->rom_pages = pages;
    cart->rom_read = asset_read;
    cart->rom_size = port_rom_size;
    /* the Super FX's RAM, battery backed: 128 KB, the board's largest;
       the original uses the first 64 KB, Enhanced's view distance the
       second (remaster.h RM_FAR_*) */
    cart->sram_size = 0x20000;
    cart->battery = 1;
    cart->save_from = 0xFC00;       /* the records; the rest is the Super FX's work */
    cart->save_size = 0x400;
    cart->routines = port_routines;
    cart->routine_count = port_routine_count;
    cart->spc_routines = port_spc_routines;
    cart->spc_routine_count = port_spc_routine_count;
}
