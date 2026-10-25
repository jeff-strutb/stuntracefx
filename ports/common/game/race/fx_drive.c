/*
 * fx_drive.c: a computer car's controls, and a car's steering from its
 * controls.
 *
 * The controls are bits in the car's +$42: 0 steer one way, 1 the other,
 * 2 and 3 their harder forms, 4 accelerate, 5 a reversing turn, 6 brake,
 * 10 a boost. +$7C counts down a time of not accelerating, +$7E how long
 * the car has been stuck.
 */
#include "game.h"
#include "physics.h"

/* A computer car's controls toward (x, z) at speed `want`. */
void fx_drive_decide(uint16_t car, int16_t x, int16_t z, int16_t want)
{
    int16_t err, e, speed;
    uint16_t bits = 0;

    if (FXU(car + 0x70) & 0x20) {
        want = div2(want);
        want = wrap16(want + div2(want));
    }
    err = wrap16((int16_t)fx_bearing_to(car, x, z) - FXW(car + 0x24));
    if (FXU(car + 0x40) & 0x0100)
        FXW(car + 0x26) = wrap16(FXW(car + 0x26) - div2(div2(FXW(car + 0x26))));
    speed = FXW(car + 0x4A);
    if (wrap16(speed - want) >= 0)
        bits |= 0x40;
    else
        bits |= wrap16(want - 0x0227) < 0 ? 0x10 : 0x0400;
    e = wrap16(err - FXW(car + 0x3A));
    if (e < 0) {
        if (wrap16(e + 0x038E) < 0) {
            if (wrap16(e + 0x0FA4) < 0)
                bits &= 0xFFEF;
            bits |= 2;
            if (FXU(car + 0x40) & 0x4000)
                bits |= 8;
        }
    } else if (wrap16(e - 0x038E) >= 0) {
        if (wrap16(e - 0x0FA4) >= 0)
            bits &= 0xFFEF;
        bits |= 1;
        if (FXU(car + 0x40) & 0x4000)
            bits |= 4;
    }
    if (wrap16(FXW(car + 0x4A) - 200) < 0)
        bits |= 0x10;
    if (FXU(car + 0x7C)) {
        FXU(car + 0x7C)--;
        bits &= 0xFBEF;
    }
    {
        int16_t stuck = FXW(car + 0x7E);

        if (stuck >= 0 && wrap16(stuck - 20) < 0) {
            if (wrap16(FXW(car + 0x4A) - 10) < 0)
                FXW(car + 0x7E) = wrap16(FXW(car + 0x7E) + 1);
            else if (FXW(car + 0x7E))
                FXW(car + 0x7E) = wrap16(FXW(car + 0x7E) - 1);
        } else {
            if (stuck >= 0)
                stuck = -40;
            stuck = wrap16(stuck + 1);
            FXW(car + 0x7E) = stuck;
            if (wrap16(stuck + 20) < 0) {
                bits &= 0xFBAF;
                bits |= 0x20;
                bits ^= 3;
            }
        }
    }
    if (FXU(car + 0x44))
        bits &= 0xFBEF;
    FXU(car + 0x42) = bits;
}

/* The car's new steering from its old (+$3A) and the controls: bits 0 and
   1 steer by $2662 a step ($80 when already turned; back to 0 first from
   the other side), up to $2666; bits 2 and 3 by $2664 more, the limit
   raised by $2668; none returns it toward 0 by $266A. With +$40 bit 10 the
   step scales with the speed and a value stays when nothing is held. */
int16_t fx_steer_from_input(uint16_t car, uint16_t bits)
{
    int16_t step = FXW(0x2662), v, lim = 0;
    int scaled = 0;

    if (FXU(car + 0x40) & 0x0400) {
        int16_t s = FXW(car + 0x4A);

        scaled = 1;
        if (s < 0)
            s = wrap16(-s);
        if (wrap16(s - 0x7F) >= 0)
            s = 0x7F;
        step = fmult(wrap16(step * 2), (int16_t)(uint16_t)(s << 8 | (uint16_t)s >> 8));
    }
    v = FXW(car + 0x3A);
    if (!(bits & 0x0F)) {
        int16_t back = FXW(0x266A);

        if (scaled || !v)
            return v;
        if (v < 0) {
            v = wrap16(v + back);
            if (v < 0)
                return v;
        } else {
            v = wrap16(v - back);
            if (v >= 0)
                return v;
        }
        return 0;
    }
    if (bits & 3) {
        lim = FXW(0x2666);
        if (bits & 1) {
            if (v < 0 && !scaled)
                v = 0;
            v = wrap16(v + (v ? 0x80 : step));
        } else {
            if (v >= 0 && !scaled)
                v = 0;
            v = wrap16(v ? v - 0x80 : v - step);
        }
    }
    if (bits & 0x0C) {
        int16_t step2 = FXW(0x2664);

        lim = wrap16(lim + FXW(0x2668));
        if (bits & 4)
            v = wrap16(v + step2);
        if (bits & 8)
            v = wrap16(v - step2);
    }
    if (v >= 0) {
        if (wrap16(v - lim) >= 0)
            v = lim;
    } else if (wrap16(v + lim) < 0) {
        v = wrap16(-lim);
    }
    return v;
}
