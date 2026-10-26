/*
 * cart.c: the cartridge's data, from the named assets (cart.h).
 *
 * A page of 256 ROM bytes held whole by one asset is read straight from
 * that asset (snes.h snes_rom_byte); a page split between assets, or with
 * code in it, goes through cart_read, which finds the asset by its offset.
 */
#define _GNU_SOURCE
#ifndef _WIN32
#include <dlfcn.h>
#include <execinfo.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snes.h"
#include "cart.h"

#define PAGES (1u << 13)                       /* 256-byte pages of up to 2 MB */
static const uint8_t *pages[PAGES];

/* PORT_ROMCENSUS: who read each byte, and where the first read came from */
static const char *census_file;
static uint8_t *census_who;
static void **census_at;
static uint8_t *census_rom;

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

uint8_t cart_byte(uint32_t off)
{
    const PortAsset *a = find(off);
    if (!a)
        snes_fatal("ROM byte $%02X:%04X (offset %06X) is in no asset: the original runs it as code",
                   off >> 15, 0x8000 | (off & 0x7FFF), off);
    return a->data[off - a->rom];
}

/* the first C function on the stack that is the game's, not the parts'
   (none on Windows: the census is a tool run on macOS or Linux) */
#ifdef _WIN32
static void *reader(void) { return NULL; }
static const char *name_of(void *at) { (void)at; return "?"; }
#else
static const char *name_of(void *at)
{
    Dl_info d;
    return at && dladdr(at, &d) && d.dli_sname ? d.dli_sname : "?";
}

static void *reader(void)
{
    static const char *const parts[] = { "cart_", "snes_", "bus_", "access_", "rd8", "rd16", "rd24", "hw_rd", "io_",
                                         "gsu_", "dma_", "move", "fx_rom", "rom_", "check_", "take_", NULL };
    void *bt[24];
    int n = backtrace(bt, 24), i, k;
    for (i = 1; i < n; i++) {
        Dl_info d;
        int part = 0;
        if (!dladdr(bt[i], &d) || !d.dli_sname)
            continue;
        for (k = 0; parts[k]; k++)
            if (!strncmp(d.dli_sname, parts[k], strlen(parts[k])))
                part = 1;
        if (!part)
            return d.dli_saddr;
    }
    return NULL;
}
#endif

static uint8_t cart_read(uint32_t off, int who)
{
    if (census_who && off < port_rom_size) {
        if (!census_who[off])
            census_at[off] = reader();
        census_who[off] |= (uint8_t)who;
    }
    if ((who & SNES_ROM_FETCH) && !find(off)) {
        snes_rom_unheld = 1;
        return census_rom ? census_rom[off] : 0;
    }
    if (census_rom && !find(off))
        return census_rom[off];
    return cart_byte(off);
}

static void census_write(void)
{
    FILE *f = fopen(census_file, "w");
    uint32_t off;
    if (!f)
        return;
    for (off = 0; off < port_rom_size; off++)
        if (census_who[off])
            fprintf(f, "%06X %u %s\n", off, census_who[off], name_of(census_at[off]));
    fclose(f);
}

void cart_make(SnesCart *cart)
{
    int i;
    memset(pages, 0, sizeof pages);
    if (port_rom_size > PAGES << 8)
        snes_fatal("cart: %u bytes of ROM space; the page table holds %u", port_rom_size, PAGES << 8);
    census_file = getenv("PORT_ROMCENSUS");
    if (census_file) {
        /* every read through cart_read: no page is read straight */
        census_who = calloc(port_rom_size, 1);
        census_at = calloc(port_rom_size, sizeof *census_at);
        if (getenv("PORT_ROMCENSUS_ROM")) {
            FILE *f = fopen(getenv("PORT_ROMCENSUS_ROM"), "rb");
            census_rom = calloc(port_rom_size, 1);
            if (!f || fread(census_rom, 1, port_rom_size, f) != port_rom_size)
                snes_fatal("PORT_ROMCENSUS_ROM: cannot read %s", getenv("PORT_ROMCENSUS_ROM"));
            fclose(f);
        }
        atexit(census_write);
    } else {
        for (i = 0; i < port_asset_count; i++) {
            const PortAsset *a = &port_assets[i];
            uint32_t p;
            for (p = (a->rom + 0xFF) >> 8; (p << 8) + 0x100 <= a->rom + a->size; p++)
                pages[p] = a->data + ((p << 8) - a->rom);
        }
    }
    cart->rom_pages = pages;
    cart->rom_read = cart_read;
    cart->rom_size = port_rom_size;
}

const uint8_t *cart_linear(void)
{
    static uint8_t *lin;
    int i;
    if (!lin) {
        lin = calloc(port_rom_size, 1);
        for (i = 0; i < port_asset_count; i++)
            memcpy(lin + port_assets[i].rom, port_assets[i].data, port_assets[i].size);
    }
    return lin;
}
