/*
 * fx_physics.c: the Super FX's physics program, which the S-CPU starts
 * once a frame, and what it runs: each record's handler (+$0A: falling,
 * bouncing and dropped objects, touch marks, the car), a record's
 * collisions with what it touches (as a box, or against a model's faces),
 * a car's parts, its wheels on the ground and the contacts' pushes back.
 */
#include "game.h"
#include "render.h"
#include "physics.h"

#define fx_hit_on           FXU(0x263C)     /* 0 on a handler's first run of two, else 1 */
#define fx_hit_depth        FXW(0x239C)     /* how deep the record went in this step */
#define fx_push_amount      FXW(0x2378)
#define fx_push_flags       FXU(0x2480)
#define fx_push_turn        FXU(0x247E)
#define fx_collide_model_at FXU(0x2486)     /* the record collided with */
#define fx_variant          FXU(0x0044)     /* the model variant of the record collided with */

/* ---- a record's collisions ---- */

/* The 19 pushes from FX_PUSHES emptied: no flags, amount $7D00. */
static void pushes_clear(void)
{
    int k;

    for (k = 0; k < 19; k++) {
        FXU(FX_PUSHES + 10 * k) = 0;
        FXU(FX_PUSHES + 10 * k + 2) = 0x7D00;
    }
}

/* Whether any of the 19 pushes holds a touch: flags in the low byte
   only (a high byte marks the place behind a face). */
static int contacts_any_touch(void)
{
    int k;

    for (k = 0; k < 19; k++) {
        uint16_t f = FXU(FX_PUSHES + 10 * k);

        if (f && !(f >> 8))
            return 1;
    }
    return 0;
}

static uint8_t rb(uint8_t bank, uint16_t at)
{
    return rom_at((uint32_t)(bank & 0x7F) << 16 | at)[0];
}

static uint16_t rw(uint8_t bank, uint16_t at)
{
    return (uint16_t)(rb(bank, at) | rb(bank, (uint16_t)(at + 1)) << 8);
}

/* the high word of twice (a * b - c * d), 32 bits */
static int16_t cross(int16_t a, int16_t b, int16_t c, int16_t d)
{
    uint32_t v = (uint32_t)((int32_t)a * b) - (uint32_t)((int32_t)c * d);

    return (int16_t)(uint16_t)((v << 1) >> 16);
}

/* Record `rec` against the faces of record `other`'s model (its shape
   +$14, in ROM bank 0): the relative place turned into the other's axes
   (by its heading +$24) to $0020; the model's points (bank and script
   from the shape) unpacked unless already; then each face of the model's
   face list noted in the push slot its flags pick, when the record is in
   front of it and nearer it than what the slot holds (or marked behind).
   Returns whether any slot was filled. */
int fx_collide_model(uint16_t rec, uint16_t other)
{
    uint16_t shape, faces, points, at, scale;
    int16_t rel[3];
    uint8_t bank;
    int i;

    fx_variant = FXU(other + 0x12);
    pushes_clear();
    shape = FXU(other + 0x14);
    if (!shape || !rw(0, (uint16_t)(shape + 0x0C)))
        return 0;
    for (i = 0; i < 3; i++)
        FXW(0x0020 + 2 * i) = rel[i] = wrap16(FXW(rec + 0x1E + 2 * i) - FXW(other + 0x1E + 2 * i));
    fx_push_turn = 0;
    if (FXU(other + 0x24)) {
        uint8_t turn = (uint8_t)(FXU(other + 0x24) >> 8);
        int16_t x = rel[0], z = rel[2];

        fx_push_turn = turn;
        if (turn == 0xC0) {
            rel[0] = wrap16(-z);
            rel[2] = x;
        } else if (turn == 0x40) {
            rel[0] = z;
            rel[2] = wrap16(-x);
        } else if (turn == 0x80) {
            rel[0] = wrap16(-x);
            rel[2] = wrap16(-z);
        } else {
            fx_turn(&x, &z, (uint8_t)-turn);
            rel[0] = wrap16(z * 4);
            rel[2] = wrap16(x * 4);
        }
    }
    faces = rw(0, (uint16_t)(shape + 3));
    points = rw(0, shape);
    {
        int16_t shift = (int8_t)rb(0, (uint16_t)(shape + 7));
        uint16_t n = (uint16_t)shift, v = 1;

        do
            v = (uint16_t)(v << 1);
        while (--n);
        FXU(0x2384) = scale = v;
    }
    bank = rb(0, (uint16_t)(shape + 2));
    if (FXU(0x2380) != shape || FXU(0x2382) != fx_variant) {
        uint16_t end;

        FXU(0x2380) = shape;
        FXU(0x2382) = fx_variant;
        fx_model_points(bank, points, &end);
    }
    /* the face list: skip to the frame's, then past $40, $28 and $44
       commands to the faces, each a size byte (negative: a new list), a
       flags byte, two skipped, the normal or three points, a point */
    at = (uint16_t)(faces + 2);
    at = (uint16_t)(at + rb(bank, (uint16_t)(faces + 1)) * 3);
    if ((int8_t)rb(bank, at++) == 0x3C) {
        for (;;) {
            int8_t c = (int8_t)rb(bank, at);

            if (c == 0x40)
                at = (uint16_t)(at + 1);
            else if (c == 0x28)
                at = (uint16_t)(at + 5);
            else if (c == 0x44)
                at = (uint16_t)(at + 3);
            else
                break;
        }
        at++;
        if (!(int8_t)rb(bank, (uint16_t)(at - 1)))
            goto done;
    }
    for (;;) {
        int8_t size = (int8_t)rb(bank, at++), flags;
        uint16_t slot;
        int16_t n[3], dist;

        if (size < 0) {                     /* the next list, or the end */
            int8_t c = (int8_t)rb(bank, at++);

            if (!c)
                break;
            continue;
        }
        flags = (int8_t)rb(bank, at++);
        if (!flags) {
            at = (uint16_t)(at + size + 5);
            continue;
        }
        FXW(0x237C) = flags;
        slot = (uint16_t)(FX_PUSH + 10 * (int16_t)(int8_t)(flags & ~0xE0));
        if (FXU(slot) >> 8) {
            at = (uint16_t)(at + size + 5);
            continue;
        }
        at = (uint16_t)(at + 2);
        if (fx_variant && (FXU(0x237C) & 0x40)) {
            int16_t q[3][3];
            int k, j;

            at = (uint16_t)(at + 3);
            for (k = 0; k < 3; k++) {
                uint16_t pt = (uint16_t)(0x2488 + rb(bank, (uint16_t)(at + k)) * 6);

                for (j = 0; j < 3; j++)
                    q[k][j] = FXW(pt + 2 * j);
            }
            FXW(0x0068) = q[0][0], FXW(0x002C) = q[0][1], FXW(0x002E) = q[0][2];
            FXW(0x006A) = q[1][0], FXW(0x00A2) = q[1][1], FXW(0x00A4) = q[1][2];
            FXW(0x0068) = wrap16(FXW(0x0068) - FXW(0x006A));
            FXW(0x002C) = wrap16(FXW(0x002C) - FXW(0x00A2));
            FXW(0x002E) = wrap16(FXW(0x002E) - FXW(0x00A4));
            FXW(0x006A) = wrap16(FXW(0x006A) - q[2][0]);
            FXW(0x00A2) = wrap16(FXW(0x00A2) - q[2][1]);
            FXW(0x00A4) = wrap16(FXW(0x00A4) - q[2][2]);
            n[0] = FXW(0x0068), n[1] = FXW(0x002C), n[2] = FXW(0x002E);
            fx_normalize(n);
            FXW(0x0068) = n[0], FXW(0x002C) = n[1], FXW(0x002E) = n[2];
            n[0] = FXW(0x006A), n[1] = FXW(0x00A2), n[2] = FXW(0x00A4);
            fx_normalize(n);
            FXW(0x006A) = cross(n[2], FXW(0x002C), n[1], FXW(0x002E));
            FXW(0x00A2) = cross(n[0], FXW(0x002E), n[2], FXW(0x0068));
            FXW(0x00A4) = cross(n[1], FXW(0x0068), n[0], FXW(0x002C));
            n[0] = FXW(0x006A), n[1] = FXW(0x00A2), n[2] = FXW(0x00A4);
        } else {
            for (i = 0; i < 3; i++)
                n[i] = wrap16(-(int16_t)(uint16_t)(rb(bank, at++) << 8));
        }
        {
            uint16_t pt = (uint16_t)(0x2488 + rb(bank, at) * 6);

            for (i = 0; i < 3; i++)
                FXW(0x2372 + 2 * i) = (int16_t)((int8_t)FXU(pt + 2 * i) * (int8_t)scale);
        }
        at = (uint16_t)(at + size);
        if (FXU(slot) >> 8)
            continue;
        FXW(0x237A) = wrap16(fmult(n[0], rel[0]) + fmult(n[1], rel[1]) + fmult(n[2], rel[2]));
        dist = wrap16(wrap16(fmult(n[0], FXW(0x2372)) + fmult(n[1], FXW(0x2374)) + fmult(n[2], FXW(0x2376))) - FXW(0x237A));
        if (dist < 0) {                     /* behind the face */
            FXU(slot) = (uint16_t)((FXU(0x237C) << 8 | FXU(0x237C) >> 8 | FXU(slot)) & 0xFF00);
            continue;
        }
        if (FXU(0x237C) & 0x80) {
            FXU(slot) |= FXU(0x237C) & 0xFF;
            continue;
        }
        {
            int16_t had = FXW(slot + 2), a = had < 0 ? wrap16(-had) : had, d = dist < 0 ? wrap16(-dist) : dist;

            if (wrap16(a - d) < 0)
                continue;
            FXW(slot + 2) = dist;
            FXU(slot) = FXU(0x237C);
            FXW(slot + 4) = n[0], FXW(slot + 6) = n[1], FXW(slot + 8) = n[2];
        }
    }
done:
    return contacts_any_touch();
}
