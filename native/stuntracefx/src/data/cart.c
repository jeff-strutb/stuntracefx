/*
 * cart.c: the cartridge data loaded from the game's package.
 */
#include <stdio.h>
#include <stdlib.h>

#include "nshvc/nshvc.h"
#include "data/cart.h"

uint8_t cart[CART_SIZE];

void cart_load(void)
{
    nshvc_asset a = nshvc_asset_open("cart.dat");
    uint32_t got = 0, n;

    if (!a.id) {
        fprintf(stderr, "cart.dat is not in the game's package: run tools/extract_assets.py on your cartridge\n");
        exit(1);
    }
    while (got < CART_SIZE && (n = nshvc_asset_read(a, cart + got, CART_SIZE - got)) != 0)
        got += n;
    nshvc_asset_close(a);
}
