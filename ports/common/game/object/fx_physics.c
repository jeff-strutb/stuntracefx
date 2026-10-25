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
#define FX_PUSH_FLAGS_AT    0x2480u
#define FX_HIT_DEPTH_AT     0x239Cu
#define fx_push_turn        FXU(0x247E)
#define fx_collide_model_at FXU(0x2486)     /* the record collided with */
#define fx_variant          FXU(0x0044)     /* the model variant of the record collided with */
#define FX_ALL_RECORDS      0x2332u
#define FX_RAM_TEST         0x235Cu
#define FX_PART_Z_SPEED     0x235Eu
#define FX_WHEEL_LOAD       0x2364u
#define FX_WHEEL_SIDE       0x2366u
#define FX_WHEEL_AHEAD      0x236Au
#define FX_SIZES            0x236Cu     /* the contact's direction */
#define FX_CONTACT_DEPTH    0x237Eu
#define FX_CONTACT_FORCE    0x2386u
#define FX_CONTACT_SUM      0x2388u
#define FX_CONTACT_FORCE_SIDE 0x239Eu
#define FX_CONTACT_FORCE_UP 0x23A0u
#define FX_WHEEL_FLAGS      0x2484u
#define FX_WHEEL_CONTACT    0x2640u
#define FX_PARAM_HIT        0x2652u
#define FX_PARAM_PUSH       0x2654u
#define FX_PARAM_BOUNCE     0x2656u
#define FX_PARAM_GRIP_FREE  0x266Cu
#define FX_PARAM_GRIP_SLIDE 0x266Eu
#define FX_PARAM_GRIP_SIDE  0x2674u
#define FX_PARAM_GRIP_AHEAD 0x2676u
#define FX_PARAM_SLIDE_LIMIT 0x2680u
#define FX_MATRIX           0x00E4u     /* the object's matrix, nine words */

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

/* Record `rec` against each record it touches (its contact list, +$18):
   a model's faces, or another record's box. The pushes taken from them
   are summed, the record's own push (from its y) added, and the sums
   applied to it. $239C keeps the first hit's depth (a model's push
   negated). */
void fx_record_collide(uint16_t rec)
{
    uint16_t c;

    fx_push_turn = 0;
    fx_hit_depth = 0;
    fx_sums_clear();
    FXU(rec + 0x1C) &= (uint16_t)~1;
    for (c = FXU(rec + 0x18); c; c = FXU(c + 0x0E)) {
        uint16_t other = FXU(c + 0x0C);
        int16_t depth;

        fx_collide_model_at = other;
        FXU(c + 0x10) &= (uint16_t)~2;
        fx_push_amount = 0;
        if (!(FXU(other + 0x16) & 0x10)) {
            if (!fx_collide_model(rec, other))
                continue;
            fx_pushes_apply();
            depth = wrap16(-fx_push_amount);
        } else {
            uint16_t at = FX_PUSH;

            fx_collide_push(c, other, rec);
            fx_push_apply(&at);
            depth = fx_push_amount;
        }
        if (!fx_hit_depth)
            fx_hit_depth = depth;
        if (!depth)
            continue;
        if (fx_push_flags & 0x20)
            FXU(rec + 0x2C) |= 0x20;
        FXU(c + 0x10) |= 2;
    }
    fx_push_set(FXW(rec + 0x20));
    {
        uint16_t at = FX_PUSH;

        if (fx_push_apply(&at)) {
            FXU(rec + 0x1C) |= 1;
            if (!fx_hit_depth)
                fx_hit_depth = fx_push_amount;
        }
    }
    fx_sums_apply(rec);
}

/* ---- the record handlers ---- */

/* each sum's speed less a 2^`n`th of itself, x and z (+$2E, +$32) */
static void damp(uint16_t rec, int n)
{
    int k, i;

    for (k = 0; k < 2; k++) {
        uint16_t at = (uint16_t)(rec + (k ? 0x32 : 0x2E));
        int16_t v = FXW(at), d = v;

        for (i = 0; i < n; i++)
            d = div2(d);
        FXW(at) = wrap16(v - d);
    }
}

/* 04:907A: falls ($78 a step), and bounces off what it hits: its fall
   less the depth, its speeds less an eighth */
static void object_bounce(uint16_t rec)
{
    FXW(rec + 0x30) = wrap16(FXW(rec + 0x30) + 0x78);
    fx_record_move(rec, 3);
    fx_record_collide(rec);
    if (!fx_hit_depth)
        return;
    if (fx_hit_depth > 0)
        FXW(rec + 0x30) = wrap16(FXW(rec + 0x30) - fx_hit_depth);
    damp(rec, 3);
}

/* 04:90E8: marks each contact touched */
void fx_object_mark_touched(uint16_t rec)
{
    uint16_t c;

    for (c = FXU(rec + 0x18); c; c = FXU(c + 0x0E))
        FXU(c + 0x10) |= 2;
}

/* 04:910A: falls while it touches something, its speeds less a quarter
   when it hits */
void fx_object_fall(uint16_t rec)
{
    if (FXU(rec + 0x18))
        FXW(rec + 0x30) = wrap16(FXW(rec + 0x30) + 0x78);
    fx_record_move(rec, 3);
    fx_record_collide(rec);
    if (fx_hit_depth)
        damp(rec, 2);
}

/* 04:9185: drops ($3C a step, no z speed) until it lands, where it stays
   (its handler cleared); x and z stay put while it falls */
static void object_drop(uint16_t rec)
{
    int16_t x, z;

    FXW(rec + 0x30) = wrap16(FXW(rec + 0x30) + 0x3C);
    FXU(rec + 0x32) = 0;
    fx_record_move(rec, 3);
    x = FXW(rec + 0x1E), z = FXW(rec + 0x22);
    fx_record_collide(rec);
    if (!fx_hit_depth) {
        FXW(rec + 0x1E) = x, FXW(rec + 0x22) = z;
    } else {
        FXU(rec + 0x0A) = 0;
    }
}

void fx_car_handler(uint16_t car) { (void)car; }

static void handler(uint16_t at, uint16_t rec)
{
    switch (at) {
    case 0x907A: object_bounce(rec); break;
    case 0x90E8: fx_object_mark_touched(rec); break;
    case 0x910A: fx_object_fall(rec); break;
    case 0x916D: fx_record_collide(rec); break;
    case 0x9185: object_drop(rec); break;
    case 0xB1A1: fx_car_handler(rec); break;
    }
}

/* The program the S-CPU starts once a race frame, while $246C is set:
   the frame counted ($2470; $2472 counts down to the next look for what
   touches what), then each record's handler (twice, $263C 0 then 1, when
   +$1C has bit 3), and the record freed after unless +$1C has bit 2
   (cleared for the next frame); then the cars ranked and the nearest
   found. $246E is $FFFF while it runs. */
void fx_physics_program(void)
{
    uint16_t rec;

    if (!FXU(0x246C))
        return;
    if (FXU(0x2472))
        FXU(0x2472)--;
    FXU(0x2470)++;
    FXU(0x246E) = 0xFFFF;
    if (!FXU(0x2472))
        fx_all_touches();
    for (rec = FXU(FX_ALL_RECORDS); rec;) {
        uint16_t at = FXU(rec + 0x0A), next;

        if (at) {
            if (FXU(rec + 0x1C) & 8) {
                fx_hit_on = 0;
                handler(at, rec);
            }
            fx_hit_on = 1;
            handler(at, rec);
        }
        next = FXU(rec + 4);
        if (FXU(rec + 0x1C) & 4)
            FXU(rec + 0x1C) &= (uint16_t)~4;
        else
            fx_object_free(rec);
        rec = next;
    }
    fx_rank_cars();
    fx_nearest_car();
    FXU(0x246E) = 0;
}

/* ---- a car's parts and wheels ---- */

static int16_t absw(int16_t v)
{
    return v < 0 ? wrap16(-v) : v;
}

/* v held to -limit .. limit */
static int16_t hold(int16_t v, int16_t limit)
{
    if (v >= 0)
        return wrap16(v - limit) < 0 ? v : limit;
    return wrap16(v + limit) >= 0 ? v : wrap16(-limit);
}

/* One contact slot (at *slot, moved past it) for wheel `w`: its flags into
   $2480; unless behind a face, or no depth, the shallowest depth kept,
   the direction turned back to world axes (and stored back), the wheel's
   speed into it (+$42) giving the push along it (the speed scaled by
   +$44, and the depth), marks in $239C for a wall hit hard (+$3C's sign
   picks $100 or $200) and any slope ($400), the push into the world sum
   and the depth's share into the contact sums. A flat face is noted once
   ($2640). */
static void contact_resolve(uint16_t w, uint16_t *slot)
{
    uint16_t at = *slot, f = FXU(at);
    int16_t depth, d[3], along, push, side;
    int i;

    if (!f) {
        *slot = (uint16_t)(at + 10);
        return;
    }
    FXU(FX_PUSH_FLAGS_AT) |= f;
    *slot = (uint16_t)(at + 10);
    if (f >> 8)
        return;
    depth = FXW(at + 2);
    if (depth < 0)
        return;
    if (wrap16(absw(FXW(FX_CONTACT_DEPTH)) - absw(depth)) >= 0)
        FXW(FX_CONTACT_DEPTH) = depth;
    if ((uint16_t)depth == 0x7D00)
        return;
    depth = wrap16(depth * 16);
    for (i = 0; i < 3; i++)
        d[i] = FXW(at + 4 + 2 * i);
    if (!(d[0] | d[2])) {
        if (FXU(FX_WHEEL_CONTACT))
            return;
        FXU(FX_WHEEL_CONTACT) = 1;
    }
    switch (fx_push_turn) {
    case 0:
        break;
    case 0x40: {
        int16_t x = d[0];

        d[0] = wrap16(-d[2]);
        d[2] = x;
        break;
    }
    case 0xC0: {
        int16_t x = d[0];

        d[0] = d[2];
        d[2] = wrap16(-x);
        break;
    }
    case 0x80:
        d[0] = wrap16(-d[0]);
        d[2] = wrap16(-d[2]);
        break;
    default:
        if (d[0] | d[2]) {
            int16_t x = d[0], z = d[2];

            fx_turn(&x, &z, (uint8_t)fx_push_turn);
            d[0] = wrap16(z * 4);
            d[2] = wrap16(x * 4);
        }
        break;
    }
    for (i = 0; i < 3; i++) {
        FXW(at + 4 + 2 * i) = d[i];
        FXW(FX_SIZES + 2 * i) = d[i];
    }
    along = wrap16(-wrap16(wrap16(fmult(wrap16(FXW(w + 0x2E) * 2), d[0]) + fmult(wrap16(FXW(w + 0x30) * 2), d[1]))
                           + fmult(wrap16(FXW(w + 0x32) * 2), d[2])));
    FXW(w + 0x42) = along;
    {
        uint32_t p = (uint32_t)((int32_t)along * FXW(w + 0x44));

        push = (int16_t)(uint16_t)(p >> 8);
    }
    if (push < 0) {
        push = wrap16(div2(push) + depth);
        if (FXU(FX_WHEEL_FLAGS) & 0x100)
            push = 0;
    } else if (wrap16(push - 0x07D0) < 0) {
        push = wrap16(push + depth);
    }
    side = fmult(push, d[1]);
    if (!d[1]) {
        int16_t v = wrap16(wrap16((along < 0 ? 0 : along) * 4) + depth), e = fmult(wrap16(v * 2), 0x1000);

        push = wrap16(v + e);
        if (wrap16(0x64 - absw(e)) < 0)
            FXU(FX_HIT_DEPTH_AT) |= FXW(w + 0x3C) >= 0 ? 0x200 : 0x100;
    }
    if (d[0] | d[2])
        FXU(FX_HIT_DEPTH_AT) |= 0x400;
    {
        int16_t v[3];

        v[0] = fmult(push, d[0]);
        v[1] = side;
        v[2] = fmult(push, d[2]);
        fx_sums_push_world(v);
    }
    for (i = 0; i < 3; i++)
        FXW(FX_CONTACT_SUM + 2 * i) = wrap16(fmult(depth, d[i]) + FXW(FX_CONTACT_SUM + 2 * i));
}

/* The nineteen contact slots resolved for wheel `w` of `car`, the marks
   gathered in the car's +$2C; returns the flags the slots had. */
uint16_t fx_contacts_resolve(uint16_t car, uint16_t w)
{
    uint16_t at = FX_PUSHES;
    int k;

    fx_push_flags = 0;
    FXU(FX_HIT_DEPTH_AT) = FXU(car + 0x2C);
    for (k = 0; k < 19; k++) {
        if (!FXU(at)) {
            at = (uint16_t)(at + 10);
            continue;
        }
        contact_resolve(w, &at);
        FXU(FX_CONTACT_FORCE) = FXU(FX_CONTACT_FORCE_UP);
    }
    FXU(car + 0x2C) = FXU(FX_HIT_DEPTH_AT);
    return fx_push_flags;
}

void fx_contact_resolve(uint16_t w, uint16_t *slot)
{
    contact_resolve(w, slot);
}

/* the grip mark the car's surface bits (+$8A) give a wheel */
static uint16_t surface_mark(uint16_t car)
{
    uint16_t b = FXU(car + 0x8A);

    return b & 4 ? 0x40 : b & 0x20 ? 0xC0 : b & 0x40 ? 0x1040 : b & 2 ? 0x0840 : 0x60;
}

/* Wheel `w` of `car`: the car's heading and the like copied (steered when
   +$46 has bit 1); unless bit 7, its collision with what it touches (the
   marks of the ground it is on into +$2C), the ground's push from its y,
   then, when it touches anything: the load (bit 3), the steering's
   side and ahead speeds and the grip that holds the side speed, the push
   ahead from the throttle, all summed and pushed at the wheel's place;
   and its bounce (+$4A-+$4E) from the contact sums. */
void fx_wheel_update(uint16_t car, uint16_t w)
{
    uint16_t flags = FXU(w + 0x46);

    FXU(FX_WHEEL_FLAGS) = flags;
    FXU(w + 0x2C) = 0;
    flags &= (uint16_t)~0x400;
    if (flags & 0x200) {
        FXU(FX_WHEEL_FLAGS) = flags | 0x400;
        goto store;
    }
    FXU(FX_WHEEL_FLAGS) = flags;
    FXU(w + 0x24) = FXU(car + 0x24);
    FXU(w + 0x28) = FXU(car + 0x28);
    if (FXU(FX_WHEEL_FLAGS) & 2)
        FXU(w + 0x24) = (uint16_t)(FXU(car + 0x3A) + FXU(w + 0x24));
    if (FXU(FX_WHEEL_FLAGS) & 0x80)
        goto steer;
    fx_sums_clear();
    pushes_clear();
    FXU(FX_CONTACT_SUM) = FXU(FX_CONTACT_SUM + 2) = FXU(FX_CONTACT_SUM + 4) = 0;
    FXU(FX_CONTACT_DEPTH) = 0x7D00;
    FXU(FX_WHEEL_CONTACT) = 0;
    FXU(FX_WHEEL_FLAGS) &= (uint16_t)~0x30;
    if (!FXU(w + 0x18)) {
        FXU(w + 0x2C) |= surface_mark(car);
    } else {
        uint16_t other = FXU(FXU(w + 0x18) + 0x0C), f, o;
        int ground = 1;

        fx_collide_model_at = other;
        if (fx_collide_model(w, other) && (f = fx_contacts_resolve(car, w)) != 0) {
            if (!(f & 0x1F)) {
                FXU(w + 0x2C) |= surface_mark(car);
                goto pushed;
            }
            ground = !(f & 0x20) ? 0 : 1;
        }
        o = FXU(fx_collide_model_at + 0x16);
        if (ground && !(o & 0x40)) {
            uint16_t m = o & 0x3800, mark = 0;

            if (!m) {
                FXU(car + 0x2C) &= (uint16_t)~0x40;
                FXU(w + 0x2C) |= surface_mark(car);
                goto pushed;
            }
            mark = m == 0x0800 ? 0x00A0 : m == 0x2000 ? 0x2020 : m == 0x1000 ? 0x1020 : m == 0x1800 ? 0x0820 : 0;
            if (mark) {
                FXU(w + 0x2C) |= mark;
                FXU(car + 0x2C) &= (uint16_t)~0x40;
                goto pushed;
            }
        }
        o = FXU(fx_collide_model_at + 0x16);
        if (!(o & 0x4000) && (FXU(car + 0x8A) & 0x40))
            FXU(w + 0x2C) |= 0x1000;
        FXU(car + 0x2C) &= (uint16_t)~0x40;
    }
pushed:
    if (FXW(w + 0x20) >= 0) {
        int16_t y = FXW(w + 0x20);

        if (FXU(w + 0x2C) & 0x20)
            y = wrap16(y + 0x0A + (div2(div2(div2(wrap16(FXW(w + 0x1E) + FXW(w + 0x22))))) & 0x0F));
        if (fx_push_set(y)) {
            uint16_t at = FX_PUSH;

            contact_resolve(w, &at);
        }
    }
    FXU(FX_CONTACT_FORCE) = FXU(FX_CONTACT_FORCE_UP);
    if (FXU(w + 0x2C) & 0x20) {
        FXU(FX_CONTACT_FORCE) = FXU(FX_CONTACT_FORCE_SIDE);
        FXU(car + 0x46)++;
        if (!FXU(FX_WORLD_SUM + 2))
            FXU(w + 0x2C) &= (uint16_t)~0x20;
    }
    if (FXU(FX_CONTACT_DEPTH) == 0x7D00) {
        FXU(w + 0x4A) = FXU(w + 0x4C) = FXU(w + 0x4E) = 0;
        FXU(car + 0x44)++;
        goto steer;
    }
    {
        uint16_t f2 = FXU(FX_WHEEL_FLAGS);
        int16_t side, ahead, lateral, grip, turn, sway, v[3];
        uint8_t d;

        FXU(car + 0x40) &= (uint16_t)~0x100;
        FXU(FX_WHEEL_FLAGS) = f2 = f2 | 0x10;
        if (f2 & 8) {
            int16_t load, r6;

            FXU(FX_WHEEL_LOAD) = 0;
            load = fmult(wrap16(FXW(w + 0x50) * 2), FXW(FX_CONTACT_FORCE));
            r6 = FXW(car + 0x3C);
            if (wrap16(FXW(car + 0x4E) - 2) < 0) {
                if (!r6)
                    goto loaded;
                if (r6 >= 0 && wrap16(load - r6) < 0) {
                    r6 = load;
                    FXU(FX_WHEEL_FLAGS) = 0x20 | f2;
                    FXU(car + 0x56) = (uint16_t)(0x1F40 + FXU(car + 0x56));
                    FXW(FX_CONTACT_FORCE) = div2(FXW(FX_CONTACT_FORCE));
                }
            }
            r6 = wrap16(r6 * 2);
            FXW(FX_WHEEL_LOAD) = r6;
            v[0] = fmult(FXW(FX_MATRIX + 12), r6);
            v[1] = fmult(FXW(FX_MATRIX + 14), r6);
            v[2] = fmult(FXW(FX_MATRIX + 16), r6);
            fx_sums_push_world(v);
        }
    loaded:
        d = (uint8_t)((uint16_t)(FXU(w + 0x24) - FXU(car + 0x24)) >> 8);
        turn = div2(div2((int16_t)(uint16_t)(rb(0, (uint16_t)(0x96FE + d)) << 8)));
        if (turn >= 0) {
            if (wrap16(turn - 0x0FA0) >= 0)
                turn = 0x0FA0;
        } else if (wrap16(turn + 0x0FA0) < 0) {
            turn = (int16_t)0xF060;
        }
        FXW(0x0136) = turn;
        sway = absw((int16_t)(uint16_t)(rb(0, (uint16_t)(0x973E + d)) << 8));
        FXW(0x0142) = sway;
        {
            int16_t a = wrap16(-FXW(w + 0x2E)), b = wrap16(-FXW(w + 0x30)), c = wrap16(-FXW(w + 0x32));

            side = wrap16(wrap16(mul15(a, FXW(FX_MATRIX)) + mul15(b, FXW(FX_MATRIX + 2))) + mul15(c, FXW(FX_MATRIX + 4)));
            FXW(FX_WHEEL_SIDE) = side;
            ahead = wrap16(wrap16(mul15(a, FXW(FX_MATRIX + 12)) + mul15(b, FXW(FX_MATRIX + 14))) + mul15(c, FXW(FX_MATRIX + 16)));
            FXW(FX_WHEEL_AHEAD) = ahead;
        }
        FXW(w + 0x26) = wrap16(FXW(w + 0x26) + ahead);
        lateral = wrap16(wrap16(fmult(wrap16(ahead * 2), turn) + fmult(side, sway)) * 4);
        grip = FXW(w + 0x50);
        if (!FXU(car + 0x3C))
            grip = wrap16(grip + fmult(wrap16(grip * 2), FXW(FX_PARAM_GRIP_FREE)));
        {
            int16_t steer = FXW(car + 0x3A), speed = FXW(car + 0x34);

            if ((!steer || (steer < 0) != (speed < 0)) && (FXU(FX_WHEEL_FLAGS) & 2))
                grip = fmult(wrap16(grip * 2), FXW(FX_PARAM_GRIP_SLIDE));
        }
        f2 = FXU(FX_WHEEL_FLAGS);
        if (FXU(car + 0x42) & 0x0C) {
            if (f2 & 2)
                grip = wrap16(grip + fmult(wrap16(grip * 2), FXW(FX_PARAM_GRIP_SIDE)));
            else if (f2 & 1)
                grip = wrap16(grip + fmult(wrap16(grip * 2), FXW(FX_PARAM_GRIP_AHEAD)));
        }
        if (f2 & 2)
            FXU(FX_CONTACT_FORCE) = 0x4650;
        grip = fmult(wrap16(grip * 2), FXW(FX_CONTACT_FORCE));
        if (wrap16(absw(lateral) - grip) >= 0) {
            int16_t held = lateral < 0 ? wrap16(-grip) : grip, over = absw(wrap16(lateral - held));

            lateral = held;
            if (wrap16(FXW(FX_PARAM_SLIDE_LIMIT) - over) < 0) {
                FXU(car + 0x56) = (uint16_t)(FXU(car + 0x56) + over);
                FXU(FX_WHEEL_FLAGS) |= 0x20;
            }
        }
        v[0] = lateral, v[1] = 0, v[2] = 0;
        fx_sums_push_local(v);
        if (FXW(car + 0x48)) {
            v[0] = 0, v[1] = 0;
            v[2] = hold(fmult(wrap16(ahead * 2), FXW(car + 0x48)), 0xC8);
            fx_sums_push_local(v);
        }
        fx_sums_rotate_local();
        if (FXU(FX_WORLD_SUM) | FXU(FX_WORLD_SUM + 2) | FXU(FX_WORLD_SUM + 4)) {
            int16_t p[3];

            v[0] = FXW(w + 0x36), v[1] = FXW(w + 0x38), v[2] = FXW(w + 0x3A);
            fx_hit_add_scaled(v, FXW(FX_PARAM_HIT));
            p[0] = FXW(w + 0x3C);
            p[1] = wrap16(FXW(w + 0x3E) + FXW(car + 0x5A));
            p[2] = wrap16(FXW(w + 0x40) + FXW(car + 0x5C));
            fx_push_at(FXU(FX_PARAM_PUSH), p);
        }
        {
            int16_t b = FXW(FX_PARAM_BOUNCE);

            FXW(w + 0x4A) = hold(div2(div2(div2(FXW(FX_CONTACT_SUM)))), b);
            FXW(w + 0x4E) = hold(div2(div2(div2(FXW(FX_CONTACT_SUM + 4)))), b);
            FXW(w + 0x4C) = hold(div2(div2(div2(FXW(FX_CONTACT_SUM + 2)))), b);
        }
    }
steer:
    if (FXU(w + 0x46) & 2)
        FXU(w + 0x24) = (uint16_t)(FXU(w + 0x24) + FXU(car + 0x3A));
store:
    FXU(w + 0x46) = FXU(FX_WHEEL_FLAGS);
}

/* Part `part` of `car` pulled toward the car's body (+$8E: its first part
   with +$46 bit 11, or the body itself): the gap (each axis under $C8
   ignored, unless the car is pinned, +$64 bit 0, when the part takes the
   hit point's place as its speed) times 4, plus the body's x and z
   speeds, less the part's own where they oppose; pushed at the part's
   place. The first time (+$46 bit 2 clear) only marks it. */
static void part_pull(uint16_t car, uint16_t part)
{
    int pinned = FXU(car + 0x64) & 1;
    uint16_t body, r;
    int16_t d[3], zero[3] = {0, 0, 0}, p[3];
    int i;

    if (pinned)
        for (i = 0; i < 3; i++)
            FXU(part + 0x2E + 2 * i) = FXU(FX_HIT_POINT + 2 * i);
    if (!(FXU(part + 0x46) & 4)) {
        FXU(part + 0x46) |= 4;
        return;
    }
    body = FXU(car + 0x8E);
    if (!pinned)
        FXU(car + 0x28) = FXU(body + 0x28);
    for (r = FXU(body + 0x3E); r; r = FXU(r + 0x34)) {
        body = r;
        if (FXU(r + 0x46) & 0x800)
            break;
    }
    for (i = 0; i < 3; i++)
        d[i] = wrap16(FXW(body + 0x1E + 2 * i) - FXW(part + 0x1E + 2 * i));
    if (!pinned) {
        int16_t s[3];

        FXW(FX_RAM_TEST) = d[2];
        for (i = 0; i < 3; i++) {
            if (wrap16(absw(d[i]) - 0xC8) < 0)
                d[i] = 0;
            d[i] = wrap16(d[i] * 4);
        }
        s[0] = wrap16(FXW(body + 0x2E) * 4), s[1] = 0, s[2] = wrap16(FXW(body + 0x32) * 4);
        if ((int16_t)(FXW(part + 0x2E) ^ d[0]) < 0)
            d[0] = wrap16(d[0] - FXW(part + 0x2E));
        if ((int16_t)(FXW(part + 0x30) ^ d[1]) < 0)
            d[1] = wrap16(d[1] - FXW(part + 0x30));
        FXW(FX_PART_Z_SPEED) = FXW(part + 0x32);
        if ((int16_t)(FXW(part + 0x32) ^ d[2]) < 0)
            d[1] = wrap16(d[1] - FXW(part + 0x32));     /* (the y, as the original) */
        for (i = 0; i < 3; i++)
            d[i] = wrap16(d[i] + s[i]);
    } else {
        for (i = 0; i < 3; i++)
            d[i] = wrap16(d[i] * 4);
    }
    fx_sums_push_world(d);
    fx_hit_add_unit(zero);
    for (i = 0; i < 3; i++)
        p[i] = FXW(part + 0x3C + 2 * i);
    fx_push_at(FXU(FX_PARAM_PUSH), p);
}

/* Each part of `car` (its list at +$3E): a body part (+$46 bit 11)
   pulled, a wheel updated; their marks (+$2C) gathered in the car's +$92.
   The car's counts +$44, +$46 and +$56 start at 0. */
void fx_parts_update(uint16_t car)
{
    uint16_t p;

    FXU(car + 0x44) = FXU(car + 0x46) = FXU(car + 0x56) = 0;
    for (p = FXU(car + 0x3E); p; p = FXU(p + 0x34)) {
        if (FXU(p + 0x46) & 0x800)
            part_pull(car, p);
        else
            fx_wheel_update(car, p);
        FXU(car + 0x92) |= FXU(p + 0x2C);
    }
}
