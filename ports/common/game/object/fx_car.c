/*
 * fx_car.c: a car's record: made from its script in ROM, posed, its
 * parts moved with it, and the damage from a hit.
 *
 * A car's script (bank 9) holds its words, then a byte giving its number
 * of parts, the parts' mode, seven words for each part and a byte
 * choosing one of the 5-byte entries after it. A part is a $52-byte
 * record on the car's list (the car's +$3E, each part's +$34).
 */
#include "game.h"
#include "physics.h"

typedef struct {
    uint8_t bank;
    uint16_t at;
} Script;

static uint8_t script_byte(Script *s)
{
    return rom_at((uint32_t)(s->bank & 0x7F) << 16 | s->at++)[0];
}

static uint16_t script_word(Script *s)
{
    uint8_t lo = script_byte(s);

    return (uint16_t)(lo | script_byte(s) << 8);
}

/* a part for `car` from the next seven words (none read when the pool has
   no room) */
static void add_part(uint16_t car, Script *s)
{
    static const uint8_t fields[7] = {0x12, 0x3C, 0x3E, 0x40, 0x44, 0x50, 0x46};
    uint16_t p = fx_pool_take(0x1098, 0x52);
    int i;

    if (!p)
        return;
    for (i = 0; i < 7; i++) {
        uint16_t w = script_word(s);

        FXU(p + fields[i]) = fields[i] == 0x3C ? (uint16_t)-w : w;
    }
    FXU(p + 0x34) = FXU(car + 0x3E);
    FXU(car + 0x3E) = p;
    FXU(p + 0x0C) = FXU(p + 0x0E) = FXU(p + 0x10) = 0x14;
    FXU(p + 0x16) |= 4;
    FXU(p + 0x1A) = FXU(0x0068);
    FXU(p + 0x2A) = FXU(car + 0x2A);
}

/* the parts, the chosen entry (+$6E) and the starting words */
static void from_script(uint16_t car, Script *s)
{
    uint8_t n = script_byte(s);
    unsigned i;

    FXU(0x0068) = script_word(s);
    for (i = 0; i < n; i++)
        add_part(car, s);
    FXU(0x2392) = (uint16_t)(s->at - 0x8090);
    i = script_byte(s);
    s->at = (uint16_t)(s->at + i * 5);
    FXU(car + 0x6E) = s->at;
    FXU(car + 0x6A) = 0x0180;
    FXU(car + 0x8C) = 0x1000;
    FXU(car + 0x8A) = FXU(0x26F2);
    FXU(car + 0x7A) = 0;
    FXU(car + 0x86) = 2;
    FXU(car + 0x58) = 1;
}

/* Each part moved with the car: its offset (+$3C, in the car's axes)
   turned by the matrix at $00E4 to +$36, its position the car's
   ($2354) plus that and its velocity eight times the move; a part held
   still (+$46 bit 10) gets velocity 0. */
void fx_car_move_parts(uint16_t car)
{
    uint16_t p;
    int i;

    for (p = FXU(car + 0x3E); p; p = FXU(p + 0x34)) {
        int16_t o[3];

        if (FXU(p + 0x46) & 0x0400) {
            for (i = 0; i < 3; i++)
                FXU(p + 0x2E + 2 * i) = 0;
            continue;
        }
        for (i = 0; i < 3; i++)
            o[i] = FXW(p + 0x3C + 2 * i);
        for (i = 0; i < 3; i++)
            FXW(p + 0x36 + 2 * i) = wrap16(mul15(o[0], FXW(0x00E4 + 2 * i)) + mul15(o[1], FXW(0x00EA + 2 * i))
                                           + mul15(o[2], FXW(0x00F0 + 2 * i)));
        for (i = 0; i < 3; i++) {
            int16_t was = FXW(p + 0x1E + 2 * i), now = wrap16(FXW(p + 0x36 + 2 * i) + FXW(0x2354 + 2 * i));

            FXW(p + 0x1E + 2 * i) = now;
            FXW(p + 0x2E + 2 * i) = wrap16(wrap16(now - was) * 8);
        }
    }
}

/* The matrix at $00E4 from the car's angles (+$26, +$24, +$28, negated),
   transposed; the parts moved from its position (copied to $2354). */
void fx_car_pose(uint16_t car)
{
    static const uint8_t angle[3] = {0x26, 0x24, 0x28};
    uint16_t a[3];
    int16_t m[9];
    int i;

    for (i = 0; i < 3; i++)
        FXU(0x0020 + 2 * i) = a[i] = (uint16_t)-FXU(car + angle[i]);
    fx_matrix_from_angles(a, m);
    fx_matrix_transpose(m);
    for (i = 0; i < 9; i++)
        FXW(0x00E4 + 2 * i) = m[i];
    for (i = 0; i < 3; i++)
        FXU(0x2354 + 2 * i) = FXU(car + 0x1E + 2 * i);
    fx_car_move_parts(car);
}

/* A program: a car from the script (bank 9) at $238E, a $98-byte record
   whose address goes back to $238E (0 when there is no room). A script
   not among the nine known ones is noted at $2650 and $81B9 used. */
void fx_car_new(void)
{
    static const uint16_t known[9] = {0x81B9, 0x853D, 0x82E5, 0x8411, 0x8669, 0x876E, 0x8AA8, 0x8886, 0x8990};
    static const uint8_t first[8] = {0x5A, 0x5C, 0x16, 0x40, 0x1A, 0x0C, 0x0E, 0x10};
    Script s = {9, FXU(0x238E)};
    uint16_t car, w;
    int i;

    for (i = 0; i < 9 && known[i] != s.at; i++)
        ;
    if (i == 9) {
        FXU(0x2650) = s.at;
        s.at = 0x81B9;
    }
    car = fx_pool_take(0x1098, 0x98);
    FXU(0x238E) = car;
    if (!car)
        return;
    FXU(car + 0x96) = FXU(0x2392);
    FXU(car + 0x1E) = FXU(0x0068);
    FXU(car + 0x20) = FXU(0x002C);
    FXU(car + 0x22) = FXU(0x002E);
    FXU(car + 0x24) = FXU(0x0022);
    FXU(car + 0x64) = FXU(0x2390);
    FXU(car + 0x66) = s.at;
    FXU(car + 0x0A) = 0xB1A1;
    w = script_word(&s);
    FXU(0x2390) = w;
    w = script_word(&s);
    if (pakram[0x108C])
        FXU(0x2390) = w;
    FXU(0x2394) = script_byte(&s);
    script_word(&s);
    for (i = 0; i < 8; i++)
        FXU(car + first[i]) = script_word(&s);
    FXU(0x2396) = (uint16_t)(s.at - 0x8090);
    s.at = (uint16_t)(s.at + 10);
    FXU(car + 0x2A) = script_word(&s);
    FXU(car + 0x14) = script_word(&s);
    if (FXU(0x262E))
        FXU(car + 0x74) = FXU(0x262E);
    FXU(0x262E) = car;
    from_script(car, &s);
    FXU(0x2474) = car;
    fx_car_pose(car);
}

static int16_t magnitude(int16_t v)
{
    return v < 0 ? wrap16(-v) : v;
}

/* The damage from the car's hit this step (the hit point $2334): its
   impact, scaled by $2678 and halved for a car with contacts, added to
   $263E (cleared first unless $263C is set); with $263C set, half the
   sum from 16 up (96 at most) adds to the damage +$82 and sets the hit's
   flags; the player's car hit hard pushes the cars near it apart. */
void fx_car_hit(uint16_t car)
{
    int16_t y4, imp, sum, h;
    uint16_t on;

    if (FXU(car + 0x64) & 8)
        return;
    if ((FXU(car + 0x40) & 0x2201) || (FXU(car + 0x8A) & 0x0200)) {
        FXU(car + 0x62) = 0;
        return;
    }
    y4 = div2(div2(FXW(0x2336)));
    FXW(car + 0x62) = y4;
    imp = magnitude(y4);
    if (wrap16(FXW(car + 0x44) - 2) < 0)
        imp = div2(div2(imp));
    if (FXW(car + 0x30) < 0)
        imp = 0;
    imp = wrap16(imp + div2(div2(div2(magnitude(FXW(0x2334))))));
    imp = wrap16(imp + div2(div2(div2(magnitude(FXW(0x2338))))));
    imp = fmult(imp, FXW(0x2678));
    if (FXU(car + 0x18))
        imp = div2(imp);
    on = FXU(0x263C);
    if (!on)
        FXU(0x263E) = 0;
    sum = wrap16(FXW(0x263E) + imp);
    FXW(0x263E) = sum;
    if (!on)
        return;
    h = div2(sum);
    if (wrap16(h - 0x10) < 0)
        return;
    if (wrap16(h - 0x60) >= 0)
        h = 0x60;
    FXW(car + 0x82) = wrap16(FXW(car + 0x82) + h);
    if (wrap16(h - 0x18) < 0) {
        FXU(car + 0x40) |= 0x80;
    } else {
        FXU(car + 0x70) |= 0x80;
        FXU(car + 0x40) |= wrap16(h - 0x38) < 0 ? 0x40 : 0x20;
    }
    if ((FXU(car + 0x40) & 0x20) && FXU(0x2476) == car)
        fx_spread_cars(car);
    if (wrap16(FXW(car + 0x82) - 0x0200) >= 0)
        FXU(car + 0x40) |= 1;
}
