/*
 * fx_traffic.c: the Super FX's look at the other cars: which are about to
 * be caught up with, pushing them apart, and the nearest one to the
 * player's car. The cars' records are a list from $262E, linked through
 * +$74.
 */
#include "game.h"
#include "physics.h"

static int16_t magnitude(int16_t v)
{
    return v < 0 ? wrap16(-v) : v;
}

/* (a, b, c) dotted with the words at $0068, $002C, $002E (1.15) */
static int16_t dot(int16_t a, int16_t b, int16_t c)
{
    return wrap16(mul15(a, FXW(0x0068)) + mul15(b, FXW(0x002C)) + mul15(c, FXW(0x002E)));
}

/* For each other car not flagged out (+$16 bit 10) and not faster (+$4A)
   that closes on the line between them (plus 20) at least as fast as its
   distance / 32, within $200 to the side: bit 1 of +$42 for one to the
   left (bit 0 cleared if it was set at the start), bit 0 for one to the
   right (likewise bit 1). */
void fx_traffic_sides(uint16_t car)
{
    uint16_t old = FXU(car + 0x42), c;

    FXU(0x26F6) = FXU(car + 0x1E);
    FXU(0x26FA) = FXU(car + 0x22);
    FXU(0x26FC) = FXU(car + 0x2E);
    FXU(0x2700) = FXU(car + 0x32);
    for (c = FXU(0x262E); c; c = FXU(c + 0x74)) {
        int16_t dx, dz, side;
        uint16_t sides;
        int k;

        if (c == car || (FXU(c + 0x16) & 0x0400) || wrap16(FXW(car + 0x4A) - FXW(c + 0x4A)) < 0)
            continue;
        FXW(0x0068) = dx = wrap16(FXW(c + 0x1E) - FXW(0x26F6));
        FXW(0x002E) = dz = wrap16(FXW(c + 0x22) - FXW(0x26FA));
        FXU(0x002C) = 0;
        FXW(0x263A) = wrap16(magnitude(dx) + magnitude(dz));
        for (k = 0; k < 5; k++)
            FXW(0x263A) = div2(FXW(0x263A));
        FXW(0x2638) = wrap16(dot(wrap16(FXW(0x26FC) - FXW(c + 0x2E)), 0, wrap16(FXW(0x2700) - FXW(c + 0x32))) + 0x14);
        if (wrap16(FXW(0x2638) - FXW(0x263A)) < 0)
            continue;
        FXW(0x263A) = side = dot(FXW(0x00E4), FXW(0x00E6), FXW(0x00E8));
        if (wrap16(magnitude(side) - 0x0200) >= 0)
            FXU(0x2638) = 0xFFFF;
        sides = FXU(car + 0x42);
        if (FXW(0x2638) >= 0) {
            if (side < 0) {
                if (old & 1)
                    sides &= 0xFFFE;
                sides |= 2;
            } else {
                if (old & 2)
                    sides &= 0xFFFD;
                sides |= 1;
            }
        }
        FXU(car + 0x42) = sides;
    }
}

/* Each other car whose distance (|dx| + |dy| + |dz|) / 2 is below 2000
   pushed away from `car` by twice the shortfall along the line between
   them on the ground plane, its turn (+$34) by a share of it, and its
   +$7C = 30. */
void fx_spread_cars(uint16_t car)
{
    int16_t x = FXW(car + 0x1E), y = FXW(car + 0x20), z = FXW(car + 0x22);
    uint16_t c;

    for (c = FXU(0x262E); c; c = FXU(c + 0x74)) {
        int16_t n[3], d, push, turn;

        if (c == car)
            continue;
        n[0] = wrap16(FXW(c + 0x1E) - x);
        n[1] = 0;
        n[2] = wrap16(FXW(c + 0x22) - z);
        d = wrap16(wrap16(magnitude(div2(n[2])) + wrap16(magnitude(div2(n[0])) + magnitude(div2(wrap16(FXW(c + 0x20) - y)))))
                   - 2000);
        if (d >= 0)
            continue;
        push = wrap16(magnitude(d) * 2);
        fx_normalize(n);
        FXW(c + 0x2E) = wrap16(fmult(n[0], push) + FXW(c + 0x2E));
        FXW(c + 0x32) = wrap16(fmult(n[2], push) + FXW(c + 0x32));
        turn = FXW(c + 0x34);
        FXW(c + 0x34) = wrap16(fmult(turn >= 0 ? 0x3E80 : (int16_t)0xC180, wrap16(push * 4)) + turn);
        FXU(c + 0x7C) = 30;
    }
}

static void near_none(void)
{
    FXU(0x2642) = 0xFFFF;
    FXU(0x2648) = 0;
}

/* The car nearest the player's ($2476; from the view's place when its
   +$64 has bit 3) by |dx| + |dz|, each car's kept in its +$8C: $2648 the
   car, $2642 a level smoothed from how fast it closed, $2646 its distance
   / 512 (7 at most), $264A/$264C its offset in the view's axes and $2644
   which side, 0 to 15. */
void fx_nearest_car(void)
{
    uint16_t player = FXU(0x2476), c, best_car = 0;
    int16_t px, pz, best = 0x1000, was = 0, level, side, ox, oz;
    int k;

    if (!player || !FXU(0x262E)) {
        near_none();
        return;
    }
    px = FXW(player + 0x1E);
    pz = FXW(player + 0x22);
    if (FXU(player + 0x64) & 8) {
        player = 0;
        px = FXW(0x00D8);
        pz = FXW(0x00DC);
    }
    for (c = FXU(0x262E); c; c = FXU(c + 0x74)) {
        int16_t dx, dz;
        int32_t d;

        if (c == player)
            continue;
        dx = magnitude(wrap16(FXW(c + 0x1E) - px));
        dz = magnitude(wrap16(FXW(c + 0x22) - pz));
        d = (int32_t)dx + dz;
        if (d != wrap16(d))
            continue;
        if (wrap16(d - best) < 0) {
            was = FXW(c + 0x8C);
            best = (int16_t)d;
            best_car = c;
        }
        FXW(c + 0x8C) = (int16_t)d;
    }
    if (!was) {
        near_none();
        return;
    }
    level = wrap16(div2(wrap16(was - best)) + 0x80);
    if (level < 0)
        level = 0;
    if (FXW(0x2642) < 0)
        FXW(0x2642) = level;
    FXW(0x2642) = wrap16(FXW(0x2642) - div2(div2(wrap16(FXW(0x2642) - level))));
    FXW(0x2646) = wrap16(fmult(best, 0x80) - 7) >= 0 ? 7 : fmult(best, 0x80);
    if (wrap16(FXW(0x2642) - 0xFF) >= 0)
        FXU(0x2642) = 0xFF;
    FXU(0x2648) = best_car;
    ox = wrap16(FXW(best_car + 0x1E) - px);
    oz = wrap16(FXW(best_car + 0x22) - pz);
    fx_turn(&ox, &oz, (uint8_t)(FXU(0x00E0) >> 8));
    FXW(0x264A) = oz;
    FXW(0x264C) = wrap16(-ox);
    side = oz;
    for (k = 0; k < 5; k++)
        side = div2(side);
    side = wrap16(side + 7);
    if (side < 0)
        side = 0;
    if (wrap16(side - 15) >= 0)
        side = 15;
    FXW(0x2644) = side;
}
