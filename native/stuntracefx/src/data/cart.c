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

    /* a player who runs the program in the builder's game folder (the
       builder's copy of the game, without its data) is told to run the
       builder (GitHub issue 9) */
    if (!a.id) {
        fprintf(stderr, "Stunt Race FX: the game's data, cart.dat, is missing. Build the game with Stunt Race FX Builder: "
                        "it takes the data from your ROM and puts it beside the program it builds (the program in the "
                        "builder's own game folder has none). Working from the source: tools/extract_assets.py.\n");
        exit(1);
    }
    while (got < CART_SIZE && (n = nshvc_asset_read(a, cart + got, CART_SIZE - got)) != 0)
        got += n;
    nshvc_asset_close(a);
    if (got < CART_SIZE) {
        fprintf(stderr, "Stunt Race FX: cart.dat is %u bytes, not %u: build the game again with Stunt Race FX Builder.\n",
                (unsigned)got, (unsigned)CART_SIZE);
        exit(1);
    }
}
