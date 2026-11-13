/* srfx.c: the game's data, cart.dat, from the player's ROM: the image of the
 * cartridge the game loads at start-up (native/stuntracefx/src/data/cart.c),
 * exactly as native/stuntracefx/tools/extract_assets.py writes it.
 *
 * Each range of the ROM the game reads at its own place, 0 everywhere else
 * (where the original's code was): the ranges are the game's own list
 * (native/stuntracefx/tools/assets.csv), written into the builder's build as
 * cart_table.h by builder/build.sh, so they and the payload's game always
 * agree. */
#include <stdlib.h>
#include <string.h>
#include "rb_int.h"
#include "cart_table.h"

uint8_t *rb_cart(const uint8_t *rom, size_t *n, char *err, size_t errlen)
{
    uint8_t *img = calloc(1, RB_ROM_SIZE);
    int i;

    if (!img) {
        rb_err(err, errlen, "out of memory");
        return NULL;
    }
    for (i = 0; i < SRFX_CART_RANGES; i++) {
        uint32_t start = k_srfx_cart[i][0], end = k_srfx_cart[i][1];

        if (start > end || end > RB_ROM_SIZE) {
            free(img);
            rb_err(err, errlen, "the builder's data table does not fit the ROM");
            return NULL;
        }
        memcpy(img + start, rom + start, end - start);
    }
    *n = RB_ROM_SIZE;
    return img;
}
