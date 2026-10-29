/*
 * cart.c: Stunt Race FX as a cartridge for nativeshvc (front.h): a Super FX
 * board with 64 KB of battery-backed RAM, its routines with C (gen/
 * routines.c), and its ROM as the named assets the build extracts (gen/
 * assets.c, tools/assets.py), with no image of the ROM: the code the C
 * stands for is held nowhere. A byte no asset holds reads 0 where the Super
 * FX's ROM buffer fills from it (it may never be used: FRONT_ROM_FETCH);
 * anywhere else it stops the game. The board and its Super FX are the
 * cartridge's own (superfx_cart.c).
 *
 * SRFX_ROMDATA_FILE (the builder's payload, builder/): no asset's bytes are
 * built in; they are read at start-up from romdata.bin, the ROM's image
 * with each asset at its offset (tools/assets.py ASSETS_DATA=file), which
 * the builder writes from the player's ROM: beside the program (Windows),
 * in the app's Resources (macOS) or the app (iOS), or SRFX_ROMDATA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if SRFX_ROMDATA_FILE && defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif SRFX_ROMDATA_FILE && defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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

#if SRFX_ROMDATA_FILE
/* romdata.bin's path: SRFX_ROMDATA, else where the program's own files are */
static void romdata_path(char *out, size_t n)
{
    const char *e = getenv("SRFX_ROMDATA");
    if (e) {
        snprintf(out, n, "%s", e);
        return;
    }
#if defined(__APPLE__)
    {
        CFBundleRef b = CFBundleGetMainBundle();
        CFURLRef u = b ? CFBundleCopyResourcesDirectoryURL(b) : NULL;
        char dir[1024] = ".";
        if (u) {
            CFURLRef abs = CFURLCopyAbsoluteURL(u);
            if (!abs || !CFURLGetFileSystemRepresentation(abs, true, (UInt8 *)dir, sizeof dir))
                snprintf(dir, sizeof dir, ".");
            if (abs)
                CFRelease(abs);
            CFRelease(u);
        }
        snprintf(out, n, "%s/romdata.bin", dir);
    }
#elif defined(_WIN32)
    {
        wchar_t w[MAX_PATH], *sl;
        char dir[MAX_PATH * 3] = ".";
        if (GetModuleFileNameW(NULL, w, MAX_PATH) && (sl = wcsrchr(w, L'\\')) != NULL) {
            *sl = 0;
            WideCharToMultiByte(CP_UTF8, 0, w, -1, dir, sizeof dir, NULL, NULL);
        }
        snprintf(out, n, "%s\\romdata.bin", dir);
    }
#else
    snprintf(out, n, "romdata.bin");
#endif
}

/* the assets' bytes from romdata.bin, into held_bytes */
static void romdata_load(void)
{
    char path[4096];
    FILE *f;
    long size;
    romdata_path(path, sizeof path);
#if defined(_WIN32)
    {
        wchar_t w[4096];
        MultiByteToWideChar(CP_UTF8, 0, path, -1, w, 4096);
        f = _wfopen(w, L"rb");
    }
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        nshvc_fatal("cannot open %s: the game's data, which the builder writes from your ROM", path);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size != (long)port_rom_size || fread(held_bytes, 1, port_rom_size, f) != port_rom_size) {
        fclose(f);
        nshvc_fatal("%s is not this build's data (%ld bytes, not %u): build the game again", path, size,
                    (unsigned)port_rom_size);
    }
    fclose(f);
}
#endif

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
#if SRFX_ROMDATA_FILE
    romdata_load();
#endif
    for (p = 0; p < (uint32_t)port_asset_count; p++) {
        const PortAsset *a = &port_assets[p];
        uint32_t i;
        for (i = 0; i < a->size && a->rom + i < port_rom_size; i++) {
#if !SRFX_ROMDATA_FILE
            held_bytes[a->rom + i] = a->data[i];
#endif
            held_bits[(a->rom + i) >> 3] |= (uint8_t)(1u << ((a->rom + i) & 7));
        }
    }
    /* each page one asset holds whole, for reads with no lookup */
    for (p = 0; p < n; p++) {
        const PortAsset *a = find(p << 8);
        if (a && a->rom + a->size >= (p << 8) + 256)
            pages[p] = held_bytes + (p << 8);
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
