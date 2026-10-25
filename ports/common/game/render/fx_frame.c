/*
 * fx_frame.c: the Super FX's frame program, which the S-CPU starts once a
 * frame: the screen cleared, every object of the sorted draw list drawn
 * (a model, a sprite scaled by its depth, or a burst of particles), the
 * weather, the screen effects and the text.
 *
 * The particles are twenty records of fourteen bytes at FX_PARTICLES: +0
 * the life, +1 flags (bit 0 drawn once, 1 falling, 2 drawn as a streak),
 * +2 the colour, +3 to +5 signed steps, +6, +8, +$A the place and +$C the
 * key of the object they belong to. The weather is eighty drops of ten
 * bytes at FX_DROPS: +0 the life (negative: waiting to fall), +1 the
 * colour, +2, +4, +6 the place and +8 where it was drawn last (rain).
 */
#include <string.h>
#include "game.h"
#include "render.h"

#define fx_clear_screen     pakram[0x01C4]
#define fx_sprites_off      pakram[0x01B4]  /* the draw list's sprites drawn as models */
#define fx_points_total     FXU(0x0144)
#define fx_effect_max       FXU(0x26EE)     /* the most screen effects started at once */
#define fx_seed             FXU(0x01D0)     /* the effects' random seed */
#define fx_tied_record      FXU(0x247A)     /* the record of the object the view follows */
#define fx_weather_now      FXU(0x2B94)
#define fx_weather_turn     FXU(0x2B8E)     /* the view's turn last frame */
#define fx_weather_drift    FXW(0x2B90)
#define fx_drop_life        FXU(0x26FC)
#define fx_drop_colour      FXU(0x26FE)
#define fx_drops_x          FXW(0x26F6)     /* this frame's move of every drop */
#define fx_drops_y          FXW(0x26F8)
#define fx_drops_z          FXW(0x26FA)
#define fx_sprite_x         FXW(0x00BA)     /* the sprite's left edge, once placed */
#define fx_sprite_y         FXW(0x00BC)     /* and its top */
#define fx_sprite_size      FXW(0x00CE)
#define fx_tex_size         FXU(0x00C8)     /* the texture's width (and height) */

#define FX_PARTICLES        0x273Eu
#define FX_DROPS            0x2864u
#define FX_EFFECTS          0x2688u
#define FX_SCALE_TABLE      0x896Cu         /* in bank $14: the scale for each even depth */
#define FX_RECIPROCALS      0x94A0u         /* in bank 0: $8000 / n for each size n */
#define FX_BURST_STEPS      0xD0DCu         /* in bank $14: x, y steps by particle */

static uint8_t rb(uint8_t bank, uint16_t at)
{
    return rom_at((uint32_t)(bank & 0x7F) << 16 | at)[0];
}

static uint16_t rw(uint8_t bank, uint16_t at)
{
    return (uint16_t)(rb(bank, at) | rb(bank, (uint16_t)(at + 1)) << 8);
}

/* MERGE: the high bytes of two words, the first's high */
static uint16_t merge(uint16_t hi, uint16_t lo)
{
    return (uint16_t)((hi & 0xFF00) | lo >> 8);
}

/* The effects' random step: the seed's bytes swapped and turned right
   through the carry, plus the seed; the seed becomes that plus the seed
   plus the carry out. Returns the first sum; the carry goes in and comes
   out through *cy. */
static uint16_t fx_random_step(uint16_t *seed, int *cy)
{
    uint16_t s = (uint16_t)(*seed << 8 | *seed >> 8);
    uint32_t a = (uint32_t)(uint16_t)(s >> 1 | *cy << 15) + *seed;
    uint32_t b = (uint32_t)(uint16_t)a + *seed + (a >> 16);

    *seed = (uint16_t)b;
    *cy = (int)(b >> 16);
    return (uint16_t)a;
}

/* A line from (x, y) to (x2, y2), both ends drawn, by the Super FX's
   Bresenham: the error starts at half the long side less the short and
   steps on the short side when the last subtraction borrowed. Returns the
   carry of the last subtraction. */
static int line(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2)
{
    uint16_t sx = 1, sy = 1, dx = (uint16_t)(x2 - x), dy = (uint16_t)(y2 - y), e, n;
    int cy, c;

    if ((int16_t)dx < 0) {
        sx = 0xFFFF;
        dx = (uint16_t)-dx;
    }
    if ((int16_t)dy < 0) {
        sy = 0xFFFF;
        dy = (uint16_t)-dy;
    }
    if ((int16_t)(dx - dy) >= 0) {
        n = (uint16_t)(dx + 1);
        e = (uint16_t)((uint16_t)(dx - dy) >> 1);
        do {
            cy = e >= dy;
            e = (uint16_t)(e - dy);
            c = cy;
            fx_plot((uint8_t)x, (uint8_t)y);
            if (!c) {
                e = (uint16_t)(e + dx);
                y = (uint16_t)(y + sy);
            }
            x = (uint16_t)(x + sx);
        } while (--n);
    } else {
        n = (uint16_t)(dy + 1);
        e = (uint16_t)((uint16_t)(dy - dx) >> 1);
        do {
            cy = e >= dx;
            e = (uint16_t)(e - dx);
            c = cy;
            fx_plot((uint8_t)x, (uint8_t)y);
            if (!c) {
                e = (uint16_t)(e + dy);
                x = (uint16_t)(x + sx);
            }
            y = (uint16_t)(y + sy);
        } while (--n);
    }
    return e >= (((int16_t)(dx - dy) >= 0) ? dy : dx);
}

/* Four pixels from (x, y) right and down. */
static void dot(uint16_t x, uint16_t y)
{
    fx_plot((uint8_t)x, (uint8_t)y);
    fx_plot((uint8_t)(x + 1), (uint8_t)y);
    fx_plot((uint8_t)x, (uint8_t)(y + 1));
    fx_plot((uint8_t)(x + 1), (uint8_t)(y + 1));
}

/* ---- the sprites ---- */

/* A texel row of a sprite smaller than its texture, at most `cols` texels
   a row, columns stepped by step (8.8) from the row's first texel; dir -1
   for a mirrored texture (its rows read from the right). */
static void sprite_shrunk(uint16_t size, uint16_t step, int mirror)
{
    uint8_t bank = (uint8_t)FXU(0x00A6);
    uint16_t base = FXU(0x00A8), r7 = 0, r8 = 0, cols = size, rows = size, addr, n;
    int16_t x = fx_sprite_x, y = fx_sprite_y;

    if (mirror)
        base = (uint16_t)(base + fx_tex_size - 1);
    if (x < 0) {                                /* the columns off the left edge skipped */
        n = cols;
        for (;;) {
            r8 = (uint16_t)(mirror ? r8 - step : r8 + step);
            n--;
            x = wrap16(x + 1);
            if (!n)
                return;
            if (x >= 0)
                break;
        }
        cols = n;
        base = (uint16_t)(base + merge(0, r8));
    }
    if (wrap16(FXW(0x00B4) - x) < 0)
        cols = (uint16_t)(cols + wrap16(FXW(0x00B4) - x) + 1);
    do {
        if ((uint16_t)(FXU(0x003E) - 1) >= (uint16_t)y) {
            uint16_t px = (uint16_t)x;

            r8 = 0;
            addr = (uint16_t)(merge(r7, r8) + base);
            n = cols;
            do {
                r8 = (uint16_t)(mirror ? r8 - step : r8 + step);
                fx_set_colour(rb(bank, addr));
                addr = (uint16_t)(merge(r7, r8) + base);
                fx_plot((uint8_t)px++, (uint8_t)y);
            } while (--n);
        }
        r7 = (uint16_t)(r7 + step);
        y = wrap16(y + 1);
    } while (--rows);
}

/* A sprite as large as its texture or larger: each texel drawn until the
   column fraction (0.16, from step) carries, each texel row until the row
   fraction borrows; the last texel of a row clipped at the right edge. */
static void sprite_grown(uint16_t size, uint16_t tex, int mirror)
{
    uint8_t bank = (uint8_t)FXU(0x00A6);
    uint16_t step, base = FXU(0x00A8), r7 = 0, frac0 = 0, texcols = tex, cols, row;
    int16_t x = fx_sprite_x, y = fx_sprite_y, visible;

    if (tex == size)
        step = 0xFFFF;
    else {
        uint16_t d = size;

        step = fx_divide_halved_if_large((uint32_t)tex << 16, &d).quot;
    }
    step &= 0xFFFE;
    if (mirror)
        base = (uint16_t)(base + tex - 1);
    visible = mul15((int16_t)(step >> 1), wrap16(FXW(0x003A) - x));
    if (wrap16(texcols - visible - 1) >= 0)
        texcols = (uint16_t)visible;
    cols = texcols;
    if (x < 0) {
        uint32_t p = (uint32_t)((int32_t)(int16_t)(step >> 1) * (int16_t)(uint16_t)-x);
        uint16_t lo = (uint16_t)p, skip;

        frac0 = (uint16_t)(lo << 1);
        skip = (uint16_t)((uint16_t)(p >> 16) << 1 | lo >> 15);
        if ((int16_t)skip < 0)
            return;
        base = (uint16_t)(mirror ? base - skip : base + skip);
        cols = (uint16_t)(texcols - skip);
        if ((int16_t)cols < 0)
            return;
        x = 0;
    }
    if (!cols)
        return;
    row = 0;
    for (;;) {
        int borrow;

        if (y >= 0) {
            uint16_t addr, px = (uint16_t)x, frac = frac0, n;
            int carry;

            if (wrap16(FXW(0x003E) - 1 - y) < 0)
                return;
            addr = (uint16_t)((uint16_t)(row << 8 | row >> 8) + base);
            fx_set_colour(rb(bank, addr));
            n = (uint16_t)(cols - 1);
            while (n) {
                addr = (uint16_t)(mirror ? addr - 1 : addr + 1);
                do {
                    carry = (uint32_t)frac + step > 0xFFFF;
                    frac = (uint16_t)(frac + step);
                    fx_plot((uint8_t)px++, (uint8_t)y);
                } while (!carry);
                n--;
                fx_set_colour(rb(bank, addr));
            }
            while (wrap16(FXW(0x003A) - (int16_t)px) >= 0) {
                carry = (uint32_t)frac + step > 0xFFFF;
                frac = (uint16_t)(frac + step);
                fx_plot((uint8_t)px++, (uint8_t)y);
                if (carry)
                    break;
            }
        }
        borrow = r7 < step;
        r7 = (uint16_t)(r7 - step);
        y = wrap16(y + 1);
        if (!borrow)
            continue;
        if (++row == fx_tex_size)
            return;
    }
}

/* The object at (fx_vector_out) as a sprite scaled by its depth: its size
   from its slot ($00CE: negative, that size on the screen; else shifted
   by the point shift, plus twice the shape's size, then scaled), at most
   $F0, centred on its place and clipped to the screen. Nothing at a depth
   of $80 or nearer, or for a sprite of no colour. */
void fx_object_sprite(void)
{
    int16_t z = fx_vector_out[2], size = fx_sprite_size, x = fx_vector_out[0], y = fx_vector_out[1], s, zz = z;
    uint16_t half, tex;
    int16_t left, top;

    if (wrap16(0x80 - z) >= 0)
        return;
    if (size >= 0) {
        uint16_t v = (uint16_t)size, n;

        for (n = FXU(0x0032); n && v; n--)
            v = (uint16_t)(v << 1);
        v = (uint16_t)(FXU(0x00AA) * 2 + v);
        s = (int16_t)(v ? v : 1);
        fx_project_scaled(&x, &y, &zz, &s);
        fx_sprite_x = x;
        fx_sprite_y = y;
        if (!s)
            return;
    } else {
        int16_t unused = 8;

        fx_project_scaled(&x, &y, &zz, &unused);
        fx_sprite_x = x;
        fx_sprite_y = y;
        s = wrap16(-size);
    }
    if (wrap16(0xF0 - s) < 0)
        s = 0xF0;
    fx_sprite_size = s;
    fx_sprite_material(pakram[0x00D6]);
    if (!FXU(0x0042))
        return;
    fx_tex_size = (uint16_t)((FXU(0x00D4) & 0xFF) + 1);
    fx_set_colour((uint8_t)FXU(0x0058));
    fx_plot_options = (uint8_t)(FXU(0x0058) >> 8 & 0x1F);

    FXW(0x00B2) = wrap16(-s);
    FXW(0x00B4) = wrap16(FXW(0x003A) - s);
    half = (uint16_t)(((uint16_t)s >> 1) + (s & 1));
    left = wrap16(fx_sprite_x - half);
    fx_sprite_x = left;
    if (wrap16(left - (1 - s)) < 0 || wrap16(left - FXW(0x003A)) >= 0)
        return;
    top = wrap16(fx_sprite_y - half);
    fx_sprite_y = top;
    tex = fx_tex_size;
    if ((uint16_t)s < tex) {
        uint16_t recip = rw(0, (uint16_t)(FX_RECIPROCALS + 2 * s));
        uint16_t t = (uint16_t)(tex << 8 | tex >> 8);

        sprite_shrunk((uint16_t)s, (uint16_t)mul15((int16_t)t, (int16_t)recip), FXW(0x00D4) < 0);
    } else {
        sprite_grown((uint16_t)s, tex, FXW(0x00D4) < 0);
    }
}

/* ---- particles ---- */

/* The particles of a burst (an object's flags bit 2): `count` of kind 1
   to 7 started in the free records, keyed to the object; each kind's
   life, steps and colour from the effects' random seed. */
void fx_effects_add(uint16_t kind, uint16_t life, uint16_t key, uint16_t count)
{
    uint16_t seed = fx_seed, p = FX_PARTICLES, slots = 20, r;
    int cy;

    do {
        if (!pakram[p]) {
            FXU(p + 6) = 0;
            FXU(p + 8) = 0;
            FXU(p + 10) = 0;
            FXU(p + 12) = key;
            pakram[p] = (uint8_t)life;
            switch (kind) {
            case 6:
            case 7: {
                uint16_t at = (uint16_t)(FX_BURST_STEPS + (uint16_t)(count - 1) * 2);
                int16_t sx = (int8_t)rb(0x14, at), sy = (int8_t)rb(0x14, (uint16_t)(at + 1));

                if (kind == 6) {
                    sx = div2(sx);
                    sy = div2(sy);
                }
                pakram[p] = (uint8_t)((count & 15) + life - 7);
                pakram[p + 1] = 0x10;
                pakram[p + 3] = (uint8_t)sx;
                FXW(p + 6) = sx;
                pakram[p + 4] = (uint8_t)sy;
                FXW(p + 8) = sy;
                cy = 0;
                fx_random_step(&seed, &cy);
                r = kind == 6 ? (uint16_t)((seed & 0x1F) - 15) : (uint16_t)((seed & 0x3F) - 0x1F);
                pakram[p + 5] = (uint8_t)r;
                FXU(p + 10) = r;
                pakram[p + 2] = (slots & 1) ? 0x0E : 0x04;
                break;
            }
            case 2:
            case 3:
            case 4:
                cy = 1;
                r = fx_random_step(&seed, &cy);
                pakram[p] = (uint8_t)(life - (r & 7));
                pakram[p + 1] = kind == 3 ? 0 : kind == 2 ? 2 : 4;
                cy = 0;
                fx_random_step(&seed, &cy);
                pakram[p + 3] = (uint8_t)((seed & 15) - 7);
                cy = 0;
                fx_random_step(&seed, &cy);
                pakram[p + 4] = (uint8_t)(kind == 3 ? (seed & 15) - 7 : (seed & 7) - 3);
                cy = 0;
                fx_random_step(&seed, &cy);
                pakram[p + 5] = (uint8_t)-(seed & (kind == 3 ? 0x7F : 0x1F));
                cy = 0;
                fx_random_step(&seed, &cy);
                pakram[p + 2] = (uint8_t)((seed & 3) + (kind == 3 ? 0x83 : 0x68));
                break;
            }
            if (!--count)
                break;
        }
        p = (uint16_t)(p + 14);
    } while (--slots);
    fx_seed = seed;
}

/* The scale for depth z (held below $4400) from the table: 0 when z is
   nearer than $100. */
static uint16_t particle_scale(uint16_t z)
{
    if ((int16_t)(0x4400 - z) < 0)
        z = 0x43FF;
    if ((int16_t)(0x0100 - z) > 0)
        return 0;
    return rw(0x14, (uint16_t)(FX_SCALE_TABLE + (z & ~1u)));
}

/* (x, y) at depth z on the screen; 0 when off its right or bottom edge
   (or past its left or top, as unsigned). */
static int particle_place(uint16_t *x, uint16_t *y, uint16_t z)
{
    uint16_t scale = particle_scale(z);

    if (!scale)
        return 0;
    *x = (uint16_t)(fmult((int16_t)*x, (int16_t)scale) + fx_screen_centre_x);
    if (*x >= FXU(0x003A))
        return 0;
    *y = (uint16_t)(fmult((int16_t)*y, (int16_t)scale) + fx_screen_centre_y);
    return *y < FXU(0x003E);
}

/* The particles keyed to `key`, each moved by its steps (and, falling,
   its downward step grown) and drawn in its colour: a dot of four pixels,
   or a streak from where it was. */
void fx_particles_draw(uint16_t key)
{
    uint16_t p = FX_PARTICLES, left;

    for (left = 20; left; left--, p = (uint16_t)(p + 14)) {
        uint16_t old[3], now[3], x, y, x0, y0;
        uint8_t flags;
        int k;

        if (FXU(p + 12) != key)
            continue;
        for (k = 0; k < 3; k++) {
            old[k] = FXU(p + 6 + 2 * k);
            FXU(p + 6 + 2 * k) = (uint16_t)((int8_t)pakram[p + 3 + k] + old[k]);
            now[k] = (uint16_t)(FXU(p + 6 + 2 * k) + fx_vector_out[k]);
        }
        flags = pakram[p + 1];
        pakram[p + 1] = flags | 1;
        if (flags & 2)
            pakram[p + 4]++;
        if ((int16_t)now[2] < 0)
            continue;
        x = now[0];
        y = now[1];
        if (!particle_place(&x, &y, now[2]))
            continue;
        fx_plot_options = 0;
        fx_set_colour(pakram[p + 2]);
        fx_plot_options = 0x08;
        if (!(pakram[p + 1] & 4)) {
            dot(x, y);
            continue;
        }
        x0 = (uint16_t)(fx_vector_out[0] + old[0]);
        y0 = (uint16_t)(fx_vector_out[1] + old[1]);
        if (!particle_place(&x0, &y0, (uint16_t)(fx_vector_out[2] + old[2])))
            continue;
        FXU(0x0040) = left;
        line(x, y, x0, y0);
    }
}

/* ---- the weather ---- */

/* The weather started afresh: drops waiting 0 to 63 frames to fall, the
   drift and the screen effects' records cleared. */
static void weather_reset(uint16_t kind)
{
    unsigned i;

    fx_weather_now = kind;
    for (i = 0; i < 80; i++) {
        pakram[FX_DROPS + 10 * i] = (uint8_t)-(i & 0x3F);
        memset(&pakram[FX_DROPS + 10 * i + 1], 0, 9);
    }
    fx_weather_turn = 0;
    fx_weather_drift = 0;
    memset(&pakram[FX_EFFECTS], 0, 0x60);
}

/* A word of a drop's place moved by `by` and kept within `lim`: past it,
   put back at the other side and 0 returned. */
static int drop_axis(uint16_t at, int16_t by, int16_t lim, int16_t *v)
{
    int16_t w = wrap16(FXW(at) + by);

    if (w >= 0 ? wrap16(w - lim) >= 0 : wrap16(w + lim) < 0) {
        FXW(at) = w >= 0 ? wrap16(-lim) : wrap16(lim - 1);
        return 0;
    }
    FXW(at) = w;
    *v = w;
    return 1;
}

/* The weather: rain (1) or snow (2) of `drops` drops, each moved with the
   view's turn and speed and a wandering drift, and drawn: rain as a
   streak from where it was drawn last, snow as a pixel, or four near the
   view. A weather not the one drawn last is set up afresh instead. */
void fx_weather_draw(uint16_t kind, uint16_t drops)
{
    uint16_t seed, target, r, p, m2, sub, speed;
    int16_t drift;
    int cy;

    if (kind != fx_weather_now) {
        weather_reset(kind);
        return;
    }
    if (kind != 1 && kind != 2)
        return;
    seed = fx_seed;
    if (!drops) {
        fx_seed = seed;
        return;
    }
    fx_plot_options = 1;
    if (kind == 1) {
        m2 = 0x3F, sub = 0x1F, fx_drops_y = 0x28;
        fx_drop_colour = 0x2A, fx_drop_life = 0x1E;
    } else {
        m2 = 0x0F, sub = 7, fx_drops_y = 5;
        fx_drop_colour = 0x20, fx_drop_life = 0x64;
    }
    target = (uint16_t)fx_weather_drift;
    cy = 1;
    r = fx_random_step(&seed, &cy);
    if (!(r & 0x1F))
        target = (uint16_t)((r >> 8 & m2) - sub);
    drift = fx_weather_drift;
    fx_weather_drift = drift = wrap16(drift + div2(div2(div2(wrap16(target - drift)))));
    r = FXU(0x00E0);
    {
        int16_t turn = wrap16(r - fx_weather_turn);
        int k;

        fx_weather_turn = r;
        for (k = 0; k < 5; k++)
            turn = div2(turn);
        fx_drops_x = wrap16(-turn + drift);
    }
    speed = FXU((uint16_t)(fx_tied_record + 0x4A));
    if ((int16_t)speed < 0) {
        fx_seed = seed;
        return;
    }
    {
        int16_t v = div2(div2(wrap16(-speed)));

        cy = v & 1;
        fx_drops_z = div2(v);
    }
    for (p = FX_DROPS; drops; drops--, p = (uint16_t)(p + 10)) {
        int8_t life = (int8_t)pakram[p];
        int16_t x, y, z, sx, sy;
        uint16_t scale, w;

        if (!life) {                            /* a new drop */
            uint16_t z0;

            pakram[p] = (uint8_t)fx_drop_life;
            fx_random_step(&seed, &cy);
            pakram[p + 1] = (uint8_t)fx_drop_colour;
            z0 = fx_drop_colour & 0x1FF;
            r = fx_random_step(&seed, &cy);
            FXU(p + 2) = (uint16_t)((r & 0x3FF) - 0x1FF);
            FXU(p + 4) = (uint16_t)-(uint16_t)(div2((int16_t)z0) + FXU(0x0036));
            FXU(p + 6) = z0;
            FXU(p + 8) = 0;
            cy = 1;
            continue;
        }
        if (life < 0) {                         /* waiting */
            pakram[p] = (uint8_t)(life + 1);
            cy = 0;
            continue;
        }
        pakram[p] = (uint8_t)(life - 1);
        fx_set_colour(pakram[p + 1]);
        cy = 1;
        if (!drop_axis(p + 2, fx_drops_x, 0x1FF, &x) || !drop_axis(p + 4, fx_drops_y, 0xFF, &y)) {
            FXU(p + 8) = 0;
            continue;
        }
        z = wrap16(FXW(p + 6) + fx_drops_z);
        if (z < 0 || wrap16(z - 0x1FF) >= 0) {
            FXW(p + 6) = z < 0 ? 0x1FF : 0;
            FXU(p + 8) = 0;
            continue;
        }
        FXW(p + 6) = z;
        scale = rw(0x14, (uint16_t)(FX_SCALE_TABLE + (uint16_t)(0x12C + z) * 2));
        sx = wrap16(mul15(x, (int16_t)scale) + fx_screen_centre_x);
        sy = wrap16(fx_screen_centre_y - 0x14 + mul15(y, (int16_t)scale));
        if (wrap16(fx_edges[0] - sx) >= 0 || wrap16(fx_edges[1] - 2 - sx) < 0 || wrap16(fx_edges[2] - sy) >= 0) {
            FXU(p + 8) = 0;
            continue;
        }
        if (wrap16(fx_edges[3] - 4 - sy) < 0) {
            pakram[p] = 1;
            cy = 0;
            continue;
        }
        if (kind != 1) {
            cy = 0x100 >= z;
            if (cy)
                dot((uint16_t)sx, (uint16_t)sy);
            else
                fx_plot((uint8_t)sx, (uint8_t)sy);
            continue;
        }
        w = FXU(p + 8);
        pakram[p + 8] = (uint8_t)sx;
        pakram[p + 9] = (uint8_t)sy;
        if (w)
            cy = line((uint16_t)sx, (uint16_t)sy, w & 0xFF, w >> 8);
    }
    fx_seed = seed;
}

/* ---- the screen effects ---- */

/* Sixteen records of six bytes at FX_EFFECTS: the time left (0 free), its
   kind (1 or 2), and its X and Y on the screen. */

/* Up to n new effects in the free ones of the first n records: time 8, a
   random kind and place (a random word's two bytes). The carry going in
   is the one the caller's last comparison left. */
static void effects_spawn(uint16_t n, int cy)
{
    uint16_t seed = fx_seed, p = FX_EFFECTS, r;

    do {
        if (pakram[p]) {
            cy = 0;
        } else {
            pakram[p] = 8;
            r = fx_random_step(&seed, &cy);
            pakram[p + 1] = (uint8_t)((r & 1) + 1);
            r = fx_random_step(&seed, &cy);
            FXU(p + 2) = r & 0xFF;
            FXU(p + 4) = r >> 8;
        }
        p = (uint16_t)(p + 6);
    } while (--n);
    fx_seed = seed;
}

/* Each effect's time counted down; one within 8 of the screen's edges, or
   past them, freed. */
static void effects_age(void)
{
    int16_t left = wrap16(fx_edges[0] + 8), right = wrap16(fx_edges[1] - 8);
    int16_t top = wrap16(fx_edges[2] + 8), bottom = wrap16(fx_edges[3] - 8);
    uint16_t p;

    for (p = FX_EFFECTS; p < FX_EFFECTS + 0x60; p += 6) {
        int16_t x = FXW(p + 2), y = FXW(p + 4);

        if (!pakram[p])
            continue;
        pakram[p]--;
        if (wrap16(x - left) < 0 || wrap16(x - right) >= 0 || wrap16(y - top) < 0 || wrap16(y - bottom) >= 0)
            FXU(p) = 0;
    }
}

/* A shimmer: the pixels of the 16 x 16 shape for time t (bank 8: a word
   to its bytes, a word to its rows' signed offsets) around (x, y), read
   with each row moved by its offset and each column by its byte, then
   plotted back at their places in the shape. */
static void haze(uint16_t t, uint16_t x, uint16_t y)
{
    uint16_t shape = rw(8, (uint16_t)(0xF92E + 4 * t)), offsets = rw(8, (uint16_t)(0xF92E + 4 * t + 2));
    uint16_t out = 0x0342, n;
    int row, col;

    FXU(0x26F6) = x;
    FXU(0x26F8) = (uint16_t)(x - 8);
    for (row = 0; row < 16; row++) {
        uint16_t ry = (uint16_t)((int8_t)rb(8, offsets++) + y);

        for (col = 0; col < 16; col++) {
            int8_t b = (int8_t)rb(8, shape++);

            if (!b)
                continue;
            FXU(out) = fx_read_pixel((uint8_t)(b + x), (uint8_t)ry);
            pakram[(uint16_t)(out + 2)] = (uint8_t)(x - 8 + col);
            pakram[(uint16_t)(out + 3)] = (uint8_t)(y - 8 + row);
            out = (uint16_t)(out + 4);
        }
    }
    FXU(0x0340) = (uint16_t)(div2(div2((int16_t)(out - 0x0342))) + FXU(0x0340));
    n = FXU(0x0340);
    if (!n)
        return;
    fx_plot_options = 1;
    for (out = 0x0342; n; n--, out = (uint16_t)(out + 4)) {
        fx_set_colour(pakram[out]);
        fx_plot(pakram[(uint16_t)(out + 2)], pakram[(uint16_t)(out + 3)]);
    }
}

/* The screen effects (when $2736 has bit 14): as many new ones as the
   followed object's speed over 8 (at most fx_effect_max), then each aged
   and drawn. */
void fx_effects_run(void)
{
    uint16_t speed, n = 0, p;
    int cy = 0;

    if (!(FXU(0x2736) & 0x4000))
        return;
    speed = FXU((uint16_t)(fx_tied_record + 0x4A));
    if ((int16_t)speed >= 0) {
        n = (uint16_t)div2(div2(div2((int16_t)speed)));
        cy = fx_effect_max >= n;
        if (wrap16(fx_effect_max - n) < 0)
            n = fx_effect_max;
    }
    if (n)
        effects_spawn(n, cy);
    effects_age();
    for (p = FX_EFFECTS; p < FX_EFFECTS + 0x60; p += 6) {
        if (!pakram[p])
            continue;
        FXU(0x0340) = 0;
        haze(pakram[p], FXU(p + 2), FXU(p + 4));
    }
}

/* ---- the frame ---- */

/* An object's model for its depth: the shape's first, or from $3E8, $7D0
   and $BB8 on (unsigned) its farther ones. */
static uint16_t model_for_depth(uint16_t shape, uint16_t z)
{
    if (z >= 0x0BB8)
        return rw(0, (uint16_t)(shape + 0x1A));
    if (z >= 0x07D0)
        return rw(0, (uint16_t)(shape + 0x18));
    if (z >= 0x03E8)
        return rw(0, (uint16_t)(shape + 0x16));
    return shape;
}

/* Every object of the sorted draw list (from fx_list_head, linked by
   +$00): its angles (+4 to +6), flags (+7), shape (+8: none skips it) and
   place (+$10 to +$14); with flags bit 2 a burst of particles (its kind,
   life and count at +$0C to +$0E, keyed by +$0A), else a model chosen by
   depth from the shape (bank 0), drawn as a sprite (flags bit 3) or with
   its frame, material and texture offsets from the slot. */
void fx_objects_draw(void)
{
    uint16_t slot = fx_list_head;

    while (slot) {
        uint8_t flags = pakram[(uint16_t)(slot + 7)];
        uint16_t shape = FXU(slot + 8), model;

        fx_plot_options = 0;
        fx_points_total = 0;
        fx_angles[0] = pakram[(uint16_t)(slot + 4)];
        fx_angles[1] = pakram[(uint16_t)(slot + 5)];
        fx_angles[2] = pakram[(uint16_t)(slot + 6)];
        if (!shape)
            goto next;
        fx_vector_out[1] = FXW(slot + 0x10);
        fx_vector_out[0] = FXW(slot + 0x12);
        fx_vector_out[2] = FXW(slot + 0x14);
        if (flags & 4) {
            uint16_t kind = pakram[(uint16_t)(slot + 0x0E)] & 7;

            FXU(0x0148) = FXU(slot + 0x0A);
            FXU(0x014E) = pakram[(uint16_t)(slot + 0x0C)];
            FXU(0x0150) = pakram[(uint16_t)(slot + 0x0D)];
            fx_points_total = kind ? kind : 0xFFFF;
            if (kind)
                fx_effects_add(kind, FXU(0x0150), FXU(0x0148), FXU(0x014E));
            fx_particles_draw(FXU(0x0148));
            goto next;
        }
        if ((flags & 2) && !(flags & 8)) {
            FXU(0x004A) = 0x8D30;
        } else {
            uint16_t m = FXU(slot + 0x16);

            if (!m) {
                m = rw(0, (uint16_t)(shape + 0x12));
                if (m == 0x8F91 && FXU(0x108A))
                    m = FXU(0x108A);
            }
            FXU(0x004A) = m;
        }
        FXU(0x0032) = rb(0, (uint16_t)(shape + 7));
        model = model_for_depth(shape, (uint16_t)fx_vector_out[2]);
        FXU(0x0016) = rw(0, model);
        FXU(0x001C) = rb(0, (uint16_t)(model + 2));
        FXU(0x0018) = rw(0, (uint16_t)(model + 3));
        FXU(0x0048) = pakram[(uint16_t)(slot + 0x18)];
        FXU(0x0044) = pakram[(uint16_t)(slot + 0x19)];
        FXU(0x0046) = pakram[(uint16_t)(slot + 0x1A)];
        if ((flags & 8) && !fx_sprites_off) {
            FXU(0x00AA) = rw(0, (uint16_t)(shape + 0x10));
            FXU(0x00D6) = pakram[(uint16_t)(slot + 0x1B)];
            FXW(0x00CE) = (int8_t)pakram[(uint16_t)(slot + 0x1C)];
            if (FXU(0x0016))
                fx_object_sprite();
        } else {
            FXU(0x01B6) = pakram[(uint16_t)(slot + 0x1B)];
            FXU(0x00C0) = (uint16_t)(pakram[(uint16_t)(slot + 0x1C)] << 8);
            FXU(0x00C2) = (uint16_t)(pakram[(uint16_t)(slot + 0x1D)] << 8);
            if (FXU(0x0018))
                fx_model_draw();
        }
next:
        slot = FXU(slot);
    }
}

/* The frame: the screen cleared (unless $01C4's byte is 0), the objects,
   the weather of the record the view follows (+$8A: bit 0 rain, else bit
   1 snow), the screen effects and the text. */
void fx_frame_program(void)
{
    uint16_t rec = fx_tied_record;

    if (fx_clear_screen)
        fx_screen_clear();
    FXU(0x01B4) = 0;
    fx_objects_draw();
    fx_plot_options = 0;
    if (rec && (FXU(rec + 0x8A) & 3)) {
        if (FXU(rec + 0x8A) & 1) {
            fx_effect_max = 0x10;
            fx_weather_draw(1, 0x50);
        } else {
            fx_weather_draw(2, 0x50);
        }
    } else {
        fx_weather_now = 0;
    }
    fx_effects_run();
    fx_effect_max = 0;
    fx_text_notice();
    fx_pixels_flush();
}
