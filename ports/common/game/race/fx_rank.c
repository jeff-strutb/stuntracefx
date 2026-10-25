/*
 * fx_rank.c: the cars ranked into race positions: +$88 (1 leading), by
 * laps (+$7A) then the last segment counted (+$86), both higher first,
 * then +$84, lower first.
 */
#include "game.h"
#include "physics.h"

/* rank `rank` to the leading car not ranked yet (the first car when there
   is none) */
static void rank_next(uint16_t rank)
{
    uint16_t c, pick = FXU(0x262E);
    int16_t b1 = -1, b2 = 0, b3 = 0;

    for (c = FXU(0x262E); c; c = FXU(c + 0x74)) {
        int16_t k1 = FXW(c + 0x7A), k2 = FXW(c + 0x86), k3 = FXW(c + 0x84);

        if (FXU(c + 0x88))
            continue;
        if (wrap16(k1 - b1) < 0)
            continue;
        if (k1 == b1) {
            if (wrap16(k2 - b2) < 0)
                continue;
            if (k2 == b2 && wrap16(k3 - b3) >= 0)
                continue;
        }
        pick = c;
        b1 = k1, b2 = k2, b3 = k3;
    }
    FXU(pick + 0x88) = rank;
}

void fx_rank_cars(void)
{
    uint16_t c, rank = 1;

    for (c = FXU(0x262E); c; c = FXU(c + 0x74))
        FXU(c + 0x88) = 0;
    for (c = FXU(0x262E); c; c = FXU(c + 0x74))
        rank_next(rank++);
}
