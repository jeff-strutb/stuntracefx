/*
 * cart.h: the assets' table as tools/assets.py writes it (gen/assets.c),
 * sorted by ROM offset, none overlapping.
 */
#ifndef SRFX_CART_H
#define SRFX_CART_H

#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t rom, size;        /* its ROM offset and length */
    const uint8_t *data;
} PortAsset;

extern const PortAsset port_assets[];
extern const int port_asset_count;
extern const uint32_t port_rom_size;

#endif
