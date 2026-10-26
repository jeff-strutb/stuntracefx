/*
 * cart.h: the cartridge's data, from the named assets the build extracts
 * (ports/stuntracefx/assets.csv; gen/assets.c, ports/stuntracefx/tools/
 * assets.py). No ROM image is built in: the code is the C, and every block
 * of data is an asset with its name, held at the ROM offset the game's
 * addresses reach it by.
 *
 *   PORT_ROMCENSUS=FILE   at exit, each ROM byte the run read: its offset,
 *                         who read it (snes.h SNES_ROM_*) and the C
 *                         function that read it first (assetmap.py); with
 *   PORT_ROMCENSUS_ROM=ROM  the cartridge's ROM, which answers for bytes no
 *                         asset holds yet (the census finds them)
 */
#ifndef SRFX_PORT_CART_H
#define SRFX_PORT_CART_H

#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t rom, size;        /* its ROM offset and length */
    const uint8_t *data;
} PortAsset;

/* sorted by ROM offset, none overlapping (gen/assets.c) */
extern const PortAsset port_assets[];
extern const int port_asset_count;
extern const uint32_t port_rom_size;

/* the cartridge for snes_power: its pages and reader */
struct SnesCart;
void cart_make(struct SnesCart *cart);
/* the byte at ROM offset off; a byte no asset holds stops the port */
uint8_t cart_byte(uint32_t off);
/* the data laid out by ROM offset, code's bytes 0: for the Super FX fill
   (the GPU's and draw_list.c's), which reads textures by offset */
const uint8_t *cart_linear(void);

#endif
