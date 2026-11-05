/*
 * scene_frame.c: the frame program (01:AB13, render/frame.c) in a full-window
 * picture, natively (scene.h): the far objects, the sorted slots, the parked
 * tyres, the weather, the effects and the nearest car's label, to the display
 * list. Each object's inputs are read from its slot and its shape's header
 * and put where the walk and the solids read them (the Super FX's RAM, which
 * the redraw puts back); no program of the chip's draws and no register of
 * its is read. A model is walked (scene_walk.c), a sprite, a particle or a
 * drop listed in view space for the GPU to project (ScPrim).
 *
 * What runs of the chip's code: FxEffectsAdd, FxWeatherReset, FxEffectsSpawn
 * and FxEffectsAge, which only change the effects' records.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gsu.h"
#include "gsu_ops.h"
#include "console_ext.h"
#include "fullwindow.h"
#include "remaster.h"
#include "scene.h"

void abi_FxEffectsAdd(void);
void abi_FxWeatherReset(void);
void abi_FxEffectsSpawn(void);
void abi_FxEffectsAge(void);

int fw_ref_pass;

static uint8_t *s;                      /* the Super FX's RAM */

static uint16_t rd(uint32_t a) { return (uint16_t)(s[a] | s[a + 1] << 8); }
static void wr(uint32_t a, uint16_t v)
{
    s[a] = (uint8_t)v;
    s[a + 1] = (uint8_t)(v >> 8);
}
static uint8_t rom(uint8_t bank, uint16_t a) { return gsu_peek8((uint32_t)bank << 16 | a); }
static uint16_t romw(uint8_t bank, uint16_t a) { return (uint16_t)(rom(bank, a) | rom(bank, (uint16_t)(a + 1)) << 8); }

/* FxSpriteMaterial (01:9276): a material byte's kind ($0042), colour word
   ($0058) and a textured one's bank ($00A6), base ($00A8) and mask ($00D4) */
static uint16_t sprite_material(uint8_t mat)
{
    uint16_t at = (uint16_t)(mat * 2 + rd(0x004A)), w, c;
    int guard = 0;
    for (;;) {
        w = romw(3, at);
        if (!(w & 0x8000))
            break;
        if (w & 0x4000) {               /* 01:9299: the texture as it stands, its colour word $08n0 */
            wr(0x0042, 0xFFFE);
            wr(0x0058, (uint16_t)(0x0800 | (w >> 8 & 15) << 4));
            return 0xFFFE;
        }
        {                               /* 01:92AA: animated, the mask's entry of a list of n */
            uint16_t list = w & 0x1FFF;
            uint8_t n = rom(3, list);
            at = (uint16_t)(list + ((rd(0x0046) & (uint16_t)(n - 1)) * 2 + 1));
        }
        if (++guard > 16)
            return 0;
    }
    c = rom(3, (uint16_t)(0x9FB1 + (w & 0xFF)));
    wr(0x0058, (uint16_t)(0x0800 + c));
    if (!(w & 0x4000)) {                /* flat: the sprite is not drawn (01:B160) */
        wr(0x0042, 0);
        return 0;
    }
    wr(0x0042, 0xFFFF);
    if (w & 0x2000)
        wr(0x0058, (uint16_t)(0x0C00 + c));
    {
        uint16_t t = (uint16_t)((w & 0xFF) * 3 + 0x9D59), uv = romw(3, (uint16_t)(0xA079 + (w >> 8 & 0x1F) * 2));
        wr(0x00A8, romw(3, t));
        wr(0x00A6, rom(3, (uint16_t)(t + 2)));
        wr(0x00D4, romw(3, uv));
        wr(0x0088, (uint16_t)(uv + 2));
    }
    return 0xFFFF;
}

/* COLOR with the plot options now */
static void colour(uint8_t v)
{
    if (gsu.por & 0x04)
        gsu.colr = (uint8_t)((gsu.colr & 0xF0) | v >> 4);
    else if (gsu.por & 0x08)
        gsu.colr = (uint8_t)((gsu.colr & 0xF0) | (v & 0x0F));
    else
        gsu.colr = v;
}

/* an object in the walk: its slot's bytes, its place, a far one's halvings,
   its true depth and record index */
typedef struct {
    const uint8_t *r;
    int16_t x, y, z;
    int far;
    uint16_t depth;
    unsigned index;
} Obj;

/* FxObjectSprite (01:B0D9) as far as what it holds: the rest the GPU's.
   A shape that is a solid here (solids.c) draws itself. */
static void object_sprite(const Obj *o, uint8_t mat, int16_t own)
{
    uint16_t kind;
    if ((int16_t)(uint16_t)(0x0080 - (uint16_t)o->z) >= 0)
        return;
    fw_depth_sprite(1);
    fw_solid_begin();
    kind = sprite_material(mat);
    if (kind) {
        uint16_t word = rd(0x0058);
        colour((uint8_t)word);
        gsu.por = (uint8_t)(word >> 8 & 0x1F);
        fw_solid_colour(gsu.colr);
        if (!fw_sprite_hidden()) {
            ScPrim p;
            int32_t z = (int32_t)o->z * (1 << o->far);
            memset(&p, 0, sizeof p);
            p.kind = SC_PRIM_SPRITE;
            p.x = o->x;
            p.y = o->y;
            p.z = o->z;
            p.own = own;
            p.aa = rd(0x00AA);
            p.shift = rd(0x0032);
            p.far = (uint16_t)o->far;
            if (own < 0)                /* a fixed size (the rank sign's) in its texel rows */
                p.rows = (uint16_t)fw_rows((int16_t)(own >> o->far));
            p.base = rd(0x00A8);
            p.mask = rd(0x00D4);
            p.bank = (uint8_t)rd(0x00A6);
            p.por = gsu.por;
            p.hi = (uint8_t)(gsu.colr & 0xF0);
            p.za = z > 0 ? (float)(1.0 / z) : 0;
            fw_prim(&p, 1);
        }
    }
    fw_solid_end();
    fw_depth_sprite(0);
}

/* FxParticlesDraw (01:BA13) for key: each record moved and listed */
static void particles_draw(uint16_t key)
{
    unsigned i, a;
    for (i = 0; i < 20; i++) {
        uint32_t rec = 0x273E + 14 * i;
        uint16_t was[3], at[3];
        uint8_t flags;
        ScPrim p;
        int c;
        if (rd(rec + 0x0C) != key)
            continue;
        for (a = 0; a < 3; a++) {
            uint16_t d = (uint16_t)(int16_t)(int8_t)s[rec + 3 + a];
            was[a] = rd(rec + 6 + 2 * a);
            d = rm_share_fx((uint16_t)(rec + 6 + 2 * a), d);    /* a tick's share of the step's move */
            wr(rec + 6 + 2 * a, (uint16_t)(d + was[a]));
            at[a] = (uint16_t)(d + was[a] + rd(0x0026 + 2 * a));
        }
        flags = s[rec + 1];
        s[rec + 1] = (uint8_t)(flags | 1);
        if (flags & 2)
            s[rec + 4] = (uint8_t)(s[rec + 4] + rm_share_fx((uint16_t)(rec + 4), 1));
        gsu.por = 0;
        colour(s[rec + 2]);
        gsu.por = 8;
        c = fw_plot_colour();
        if (c < 0)
            continue;
        memset(&p, 0, sizeof p);
        p.x = (int16_t)at[0];
        p.y = (int16_t)at[1];
        p.z = (int16_t)at[2];
        p.colour = (uint8_t)c;
        p.n = 2;
        if (s[rec + 1] & 4) {           /* a line from where it was */
            p.kind = SC_PRIM_LINE;
            p.x2 = (int16_t)(uint16_t)(was[0] + rd(0x0026));
            p.y2 = (int16_t)(uint16_t)(was[1] + rd(0x0028));
            p.z2 = (int16_t)(uint16_t)(was[2] + rd(0x002A));
        } else
            p.kind = SC_PRIM_DOT;
        fw_prim(&p, 0);
    }
}

/* FxEffectsAdd (01:B6D5): only the records change; its registers kept to it */
static void effects_add(uint16_t kind, uint16_t at, uint16_t key, uint16_t n)
{
    RegsGSU kept = gsu;
    gsu.r[0] = kind;
    gsu.r[1] = at;
    gsu.r[2] = key;
    gsu.r[3] = n;
    gsu.r[10] = 0x0240;
    gsu.r[11] = 0xFFFE;
    abi_FxEffectsAdd();
    {
        uint8_t por = kept.por, colr = kept.colr;
        gsu = kept;
        gsu.por = por;
        gsu.colr = colr;
    }
}

/* one slot, as FxObjectsDraw (01:AB90) reads it. The inputs put where the
   walk and the solids read them. */
static void object_draw(const Obj *o, uint8_t first)
{
    const uint8_t *r = o->r;
    uint8_t flags = r[7], k;
    uint16_t shape = (uint16_t)(r[8] | r[9] << 8), model, list, faces, w;
    uint16_t depth;

    (void)first;
    if (!shape)
        return;
    rm_far_shift = o->far;
    rm_far_depth = o->depth;
    rm_angle_frac = rm_redrawing() ? rm_record_frac[o->index] : NULL;
    fw_solid_object(shape, o->index);
    wr(0x0020, r[4]);
    wr(0x0022, r[5]);
    wr(0x0024, r[6]);
    wr(0x0026, (uint16_t)o->x);
    wr(0x0028, (uint16_t)o->y);
    wr(0x002A, (uint16_t)o->z);
    if (flags & 4) {                    /* a burst of particles (01:ABEC) */
        k = r[0x0E] & 7;
        if (k)
            effects_add(k, r[0x0D], (uint16_t)(r[0x0A] | r[0x0B] << 8), r[0x0C]);
        particles_draw((uint16_t)(r[0x0A] | r[0x0B] << 8));
        return;
    }
    if ((flags & 2) && !(flags & 8))    /* the material table (01:AC31) */
        w = 0x8D30;
    else {
        w = (uint16_t)(r[0x16] | r[0x17] << 8);
        if (!w) {
            w = romw(0, (uint16_t)(shape + 0x12));
            if (w == 0x8F91 && rd(0x108A))
                w = rd(0x108A);
        }
    }
    wr(0x004A, w);
    wr(0x0032, rom(0, (uint16_t)(shape + 7)));
    depth = o->far ? o->depth : (uint16_t)o->z;
    model = shape;                      /* the level of detail by its own depth (01:AC80) */
    if (depth >= 0x0BB8)
        model = romw(0, (uint16_t)(shape + 0x1A));
    else if (depth >= 0x07D0)
        model = romw(0, (uint16_t)(shape + 0x18));
    else if (depth >= 0x03E8)
        model = romw(0, (uint16_t)(shape + 0x16));
    list = romw(0, model);
    faces = romw(0, (uint16_t)(model + 3));
    wr(0x0016, list);
    wr(0x001C, rom(0, (uint16_t)(model + 2)));
    wr(0x0018, faces);
    wr(0x0048, r[0x18]);
    wr(0x0044, r[0x19]);
    wr(0x0046, r[0x1A]);
    if (flags & 8) {                    /* a sprite (01:ACF8); it is only ever drawn with $01B4 0 */
        wr(0x00AA, romw(0, (uint16_t)(shape + 0x10)));
        wr(0x00CE, (uint16_t)(int16_t)(int8_t)r[0x1C]);
        if (list)
            object_sprite(o, r[0x1B], (int16_t)(int8_t)r[0x1C]);
        fw_solid_end();
        fw_depth_sprite(0);
        return;
    }
    wr(0x01B6, r[0x1B]);
    wr(0x00C0, (uint16_t)(r[0x1C] << 8));
    wr(0x00C2, (uint16_t)(r[0x1D] << 8));
    if (faces) {
        wr(0x0054, 0x8000);             /* FxModelDraw with $01B4 0 (01:8584) */
        fw_water_view();
        sc_walk_model((int16_t)romw(0, (uint16_t)(shape + 0x10)));
    }
}

/* ---- the far objects and the parked tyres (frame.c far_pass, parked_pass) ---- */
typedef struct {
    int32_t key;
    int16_t at[3];
    uint8_t shift;
    uint16_t rec, depth;
} FarObject;

static int far_cmp(const void *a, const void *b)
{
    int32_t x = ((const FarObject *)a)->key, y = ((const FarObject *)b)->key;
    return x < y ? 1 : x > y ? -1 : 0;
}

static int far_place(unsigned i, const int16_t m[9], FarObject *f)
{
    uint32_t r = 0x10000u + (uint16_t)(RM_FAR_RECORDS + i * RM_FAR_SIZE);
    int64_t d[3], v[3], big = 0;
    uint16_t shape = rd(r + 0x08);
    int32_t radius = (int16_t)romw(0, (uint16_t)(shape + 0x10)), bias = (int16_t)romw(0, (uint16_t)(shape + 0x05));
    unsigned sh = 0, k, c;

    for (k = 0; k < 3; k++)
        d[k] = (int32_t)(rd(r + RM_FAR_PLACE + 4 * k) | (uint32_t)rd(r + RM_FAR_PLACE + 4 * k + 2) << 16);
    for (c = 0; c < 3; c++)
        v[c] = (d[0] * m[c] >> 15) + (d[1] * m[3 + c] >> 15) + (d[2] * m[6 + c] >> 15);
    if (v[2] + radius < 0)              /* the sort's cull behind the view; the full window keeps its sides */
        return 0;
    for (k = 0; k < 3; k++)
        if (llabs(v[k]) > big)
            big = llabs(v[k]);
    while ((big >> sh) > 0x3FFF)
        sh++;
    f->key = (int32_t)v[2] + bias + (s[r + RM_FAR_LIFT] ? 0x1F40 : 0);
    for (k = 0; k < 3; k++)
        f->at[k] = (int16_t)(v[k] >> sh);
    f->shift = (uint8_t)sh;
    f->depth = (uint16_t)(v[2] > 0x7FFF ? 0x7FFF : v[2] < 0 ? 0 : v[2]);
    f->rec = (uint16_t)(RM_FAR_RECORDS + i * RM_FAR_SIZE);
    return 1;
}

static void far_object(const FarObject *f)
{
    Obj o;
    o.r = s + 0x10000 + f->rec;
    o.x = f->at[0];
    o.y = f->at[1];
    o.z = f->at[2];
    o.far = f->shift;
    o.depth = f->depth;
    o.index = 0x31 + (f->rec - RM_FAR_RECORDS) / RM_FAR_SIZE;
    object_draw(&o, 0);
}

static void near_object(uint16_t slot)
{
    Obj o;
    o.r = s + slot;
    o.x = (int16_t)rd((uint32_t)slot + 0x12);
    o.y = (int16_t)rd((uint32_t)slot + 0x10);
    o.z = (int16_t)rd((uint32_t)slot + 0x14);
    o.far = 0;
    o.depth = 0;
    o.index = (uint16_t)(slot - 0x0AAA) / 0x1E % 0x31;
    object_draw(&o, 0);
}

static FarObject far_list[RM_FAR_MAX], parked[RM_FAR_MAX];
static unsigned nparked;

/* the near list as FxObjectsDraw takes it */
static void near_pass(void)
{
    uint16_t slot;
    int guard = 0;
    for (slot = rd(0x023C); slot && guard < 0x100; slot = rd(slot), guard++)
        near_object(slot);
}

static void far_pass(void)
{
    unsigned count = rd(0x10000u + RM_FAR_COUNT), i, n = 0;
    int16_t m[9];

    nparked = 0;
    if (!count || count > RM_FAR_MAX)
        return;
    wr(0x10000u + RM_FAR_COUNT, 0);
    for (i = 0; i < 9; i++)
        m[i] = (int16_t)rd(0x00E4 + 2 * i);
    for (i = 0; i < count; i++) {
        uint32_t r = 0x10000u + (uint16_t)(RM_FAR_RECORDS + i * RM_FAR_SIZE);
        if (s[r + RM_FAR_PARKED]) {
            if (far_place(i, m, &parked[nparked]))
                nparked++;
        } else if (far_place(i, m, &far_list[n]))
            n++;
    }
    if (!n)
        return;
    qsort(far_list, n, sizeof far_list[0], far_cmp);
    memcpy(s + 0x10000 + RM_FAR_KEEP, s, 0x2C00);
    memcpy(s + 0x10000 + RM_FAR_KEEP + 0x2C00, s + 0xFC00, 0x400);
    for (i = 0; i < n; i++)
        far_object(&far_list[i]);
    rm_far_shift = 0;
    memcpy(s, s + 0x10000 + RM_FAR_KEEP, 0x2C00);
    memcpy(s + 0xFC00, s + 0x10000 + RM_FAR_KEEP + 0x2C00, 0x400);
}

/* the near list again with the parked tyres among its objects, by the sort's
   keys: what the near list painted is painted again, as frame.c's does */
static void parked_pass(void)
{
    unsigned i, j, used[RM_FAR_MAX];
    uint16_t slot;
    int guard = 0;

    if (!nparked)
        return;
    memset(used, 0, nparked * sizeof used[0]);
    for (i = 0; i < nparked; i++) {
        uint16_t shape = rd(0x10000u + (uint16_t)(parked[i].rec + 0x08));
        parked[i].key = (int16_t)(rd(0x10000u + (uint16_t)(parked[i].rec + 0x02)) + parked[i].at[2] +
                                  romw(0, (uint16_t)(shape + 0x05)));
    }
    qsort(parked, nparked, sizeof parked[0], far_cmp);
    memcpy(s + 0x10000 + RM_FAR_KEEP, s, 0x2C00);
    memcpy(s + 0x10000 + RM_FAR_KEEP + 0x2C00, s + 0xFC00, 0x400);
    for (slot = rd(0x023C); slot && guard < 0x31; slot = rd(slot), guard++) {
        int16_t key = (int16_t)rd((uint32_t)slot + 0x02);
        for (j = 0; j < nparked; j++)
            if (!used[j] && (int16_t)(key - 1 - (int16_t)parked[j].key) < 0) {
                used[j] = 1;
                far_object(&parked[j]);
            }
        near_object(slot);
    }
    for (j = 0; j < nparked; j++)
        if (!used[j])
            far_object(&parked[j]);
    rm_far_shift = 0;
    memcpy(s, s + 0x10000 + RM_FAR_KEEP, 0x2C00);
    memcpy(s + 0xFC00, s + 0x10000 + RM_FAR_KEEP + 0x2C00, 0x400);
}

/* ---- the weather (FxWeatherDraw, 01:BC7D) ----
   Each drop in use moved by the view's turn and listed. A picture's writes
   to its drops go with its RAM, so what the chip's loop does for the next
   step (a new drop, one gone off the screen) has no pixels here and is left
   out: only the first random step, the drift's, is taken. */
static void weather_draw(uint16_t kind, unsigned drops)
{
    uint16_t seed, r0, drift, closing, up, me_z, v, turn;
    unsigned i;
    int rain = kind == 1, cy;

    if (kind != rd(0x2B94)) {           /* set afresh, no drop drawn */
        RegsGSU kept = gsu;
        gsu.r[0] = kind;
        gsu.r[10] = 0x0240;
        gsu.r[11] = 0xFFFE;
        abi_FxWeatherReset();
        gsu = kept;
        return;
    }
    seed = rd(0x01D0);
    if (!drops)
        return;
    /* the random step at 01:BCE7, the carry set (kind - 1's subtraction) */
    r0 = (uint16_t)(seed << 8 | seed >> 8);
    r0 = (uint16_t)(r0 >> 1 | 0x8000);
    cy = (uint32_t)r0 + seed > 0xFFFF;
    r0 = (uint16_t)(r0 + seed);
    seed = (uint16_t)(r0 + seed + cy);
    drift = rd(0x2B90);
    {
        uint16_t next = drift;
        if (!(r0 & 0x1F))
            next = (uint16_t)((r0 >> 8 & (rain ? 0x3F : 0x0F)) - (rain ? 0x1F : 0x07));
        v = (uint16_t)(next - drift);
        v = gsu_div2(gsu_div2(gsu_div2(v)));
        drift = (uint16_t)(v + drift);
    }
    wr(0x2B90, drift);
    turn = rd(0x2B8E);
    r0 = rd(0x00E0);
    wr(0x2B8E, r0);
    v = (uint16_t)(r0 - turn);
    v = gsu_div2(gsu_div2(gsu_div2(gsu_div2(gsu_div2(v)))));
    closing = (uint16_t)(drift - v);
    wr(0x26F6, closing);
    up = rain ? 0x28 : 0x05;
    wr(0x26F8, up);
    wr(0x26FC, rain ? 0x1E : 0x64);
    wr(0x26FE, rain ? 0x2A : 0x20);
    wr(0x01D0, seed);
    v = rd((uint32_t)rd(0x247A) + 0x4A);
    if ((int16_t)v < 0)
        return;
    me_z = (uint16_t)-(int16_t)v;
    me_z = gsu_div2(gsu_div2(gsu_div2(me_z)));
    wr(0x26FA, me_z);
    gsu.por = 1;                        /* CMODE 1 (01:BCAF) */
    for (i = 0; i < drops; i++) {
        uint32_t d = 0x2864 + 10 * i;
        int8_t state = (int8_t)s[d];
        int16_t x, y, z;
        ScPrim p;
        if (state <= 0)                 /* a new drop, or one waiting: nothing drawn */
            continue;
        colour(s[d + 1]);
        x = (int16_t)(uint16_t)(rd(d + 2) + closing);
        if (x < -0x1FF || x >= 0x1FF)   /* past its range: set back, not drawn */
            continue;
        y = (int16_t)(uint16_t)(rd(d + 4) + up);
        if (y < -0xFF || y >= 0xFF)
            continue;
        z = (int16_t)(uint16_t)(rd(d + 6) + me_z);
        if (z < 0 || z >= 0x1FF)
            continue;
        memset(&p, 0, sizeof p);
        p.x = x;
        p.y = y;
        p.z = z;
        p.colour = gsu.colr;
        if (rain) {
            uint16_t last = rd(d + 8);
            if (!last)                  /* no pixel of its own yet */
                continue;
            p.kind = SC_PRIM_WLINE;
            p.x2 = (int16_t)(last & 0xFF);
            p.y2 = (int16_t)(last >> 8);
        } else {
            p.kind = SC_PRIM_WDOT;
            p.n = z <= 0x100 ? 2 : 1;
        }
        fw_prim(&p, 0);
    }
}

/* ---- the nearest car's label (FxTextNotice 01:D12F, FxTextDraw 01:D311) ---- */
static void text_bitmap(uint16_t x, uint16_t y, uint8_t bank, uint16_t at, int rows, int bytes, uint16_t pal)
{
    fw_bitmap(x, y, bank, at, rows, bytes, pal);
    fw_sprite_rows_done();
}

static void text_draw(void)
{
    uint16_t x0 = rd(0x2738), y = rd(0x273A), at = rd(0x2732), x, pal = 0x2702;
    uint8_t bank = (uint8_t)rd(0x2734);
    int small = 0;
    uint16_t line = 0x0020;
    int guard = 0;

    gsu.por = 0;
    if (!at)
        return;
    x = x0;
    while (guard++ < 4096) {
        int8_t c = (int8_t)rom(bank, at++);
        uint8_t u = (uint8_t)c;
        if (!c)
            break;
        if (c < 0) {
            switch (u) {
            case 0xFF:
                x0 = rom(bank, at);
                y = rom(bank, (uint16_t)(at + 1));
                at = (uint16_t)(at + 2);
                x = x0;
                continue;
            case 0xFE:
                pal = (uint16_t)(0x2702 + 4 * rom(bank, at++));
                continue;
            case 0xFD:
                small = rom(bank, at++) != 0;
                line = small ? 0x0010 : 0x0020;
                continue;
            case 0xFC:
                x = (uint16_t)(x + (small ? 4 : 8));
                continue;
            case 0xF5:
                x = x0;
                y = (uint16_t)(y + gsu_div2(line));
                continue;
            /* a picture leaves Y past its rows (a glyph's is put back) */
            case 0xFB:
                text_bitmap(x, y, 0x15, 0x8000 + 0x3700, 32, 30, pal);
                y = (uint16_t)(y + 32);
                continue;
            case 0xFA:
                text_bitmap(x, y, 0x15, 0x8000 + 0x3B00, 16, 30, pal);
                y = (uint16_t)(y + 16);
                continue;
            case 0xF9:
                text_bitmap(x, y, 0x15, 0x8000 + 0x1810, 16, 16, pal);
                y = (uint16_t)(y + 16);
                continue;
            case 0xF8:
                text_bitmap(x, y, 0x15, 0x8000 + 0x3700, 24, 30, pal);
                y = (uint16_t)(y + 24);
                continue;
            case 0xF7:
                text_bitmap(x, y, 0x15, 0x8000 + 0x3A00, 24, 30, pal);
                y = (uint16_t)(y + 24);
                continue;
            case 0xF6:
                text_bitmap(x, y, 0x15, 0x8000 + 0x3D1C, 24, 4, pal);
                y = (uint16_t)(y + 24);
                continue;
            default:
                break;
            }
        }
        if (u == 0x0D) {
            x = x0;
            y = (uint16_t)(y + line);
            continue;
        }
        if ((int16_t)(u >= 0x80 ? (uint16_t)(int16_t)c : u) - 0x30 >= 0) {
            uint16_t i = (uint16_t)(((uint16_t)(int16_t)c - 0x30) * 2);
            uint16_t off = romw(1, (uint16_t)((small ? 0xF52E : 0xF4D2) + i));
            if (small)
                text_bitmap(x, y, 0x15, (uint16_t)(0x8000 + off), 16, 2, pal);
            else
                text_bitmap(x, y, 0x15, (uint16_t)(0x8000 + off), 32, 4, pal);
        }
        x = (uint16_t)(x + (small ? 0x0A : 0x14));
    }
}

static void text_notice(void)
{
    uint16_t car, xv;
    int16_t x;
    int16_t right;

    if (!(rd(0x273C) & 1))
        return;
    car = rd(0x2648);
    if (!car || car == rd(0x2476) || car == rd(0x2478))
        return;
    if ((int16_t)rd(0x264C) < 0)
        return;
    xv = (uint16_t)(gsu_div2(rd(0x264A)) + rd(0x0034));
    x = (int16_t)xv;
    if (x < 0)
        x = 0;
    right = (int16_t)(uint16_t)(rd(0x003A) - 0x11);
    if ((int16_t)(uint16_t)((uint16_t)x - (uint16_t)right) >= 0)
        x = right;
    wr(0x2738, (uint16_t)x);
    wr(0x273A, 0x0068);
    wr(0x2734, 0x0001);
    wr(0x2732, romw(1, (uint16_t)(0xD1CB + 2 * rd((uint32_t)car + 0x96))));
    if (fw_label_begin(rd(0x2738), rd(0x273A))) {
        text_draw();
        fw_label_end();
    }
    wr(0x2732, 0);
}

/* FxEffectsRun (01:BF6D): new effects and their ages as the chip's; their
   drawing reads the picture's own pixels (FxHazeSample), which no play has
   reached */
static void effects_run(void)
{
    uint16_t n = 0, v;
    if (!(rd(0x2736) & 0x4000))
        return;
    v = rd((uint32_t)rd(0x247A) + 0x4A);
    if ((int16_t)v >= 0) {
        n = gsu_div2(gsu_div2(gsu_div2(v)));
        if ((int16_t)(uint16_t)(rd(0x26EE) - n) < 0)
            n = rd(0x26EE);
    }
    {
        RegsGSU kept = gsu;
        static int said;
        if (n) {
            gsu.r[0] = n;
            abi_FxEffectsSpawn();
        }
        abi_FxEffectsAge();
        gsu = kept;
        if (!said++)
            fprintf(stderr, "scene frame: the haze effects (FxHazeSample) are not drawn\n");
    }
}

static int native_on(void)
{
    static int off = -1;
    if (off < 0)
        off = getenv("SC_CHECK") != NULL || getenv("FW_NOQUADS") != NULL || getenv("FW_QUADCHECK") != NULL;
    return !off && fw_view_active() && sc_gpu_on();
}

int sc_frame_native(void)
{
    static uint8_t keep[0x2C00], keep2[0x400];
    static int check = -1;
    RegsGSU kept;

    if (!native_on())
        return 0;
    s = nshvc_sram();
    if (check < 0)
        check = getenv("FW_GPUCHECK") != NULL;
    if (check) {
        memcpy(keep, s, sizeof keep);
        memcpy(keep2, s + 0xFC00, sizeof keep2);
        kept = gsu;
    }
    sc_list_prim_view_set();
    if (s[0x01C4])
        fw_clear();
    far_pass();
    near_pass();
    parked_pass();
    rm_far_shift = 0;
    {
        uint16_t tied = rd(0x247A);
        if (tied) {
            uint16_t w = rd((uint32_t)tied + 0x8A);
            if (w & 1) {
                wr(0x26EE, 0x0010);
                weather_draw(1, 0x50);
            } else if (w & 2)
                weather_draw(2, 0x50);
            else
                wr(0x2B94, 0);
        } else
            wr(0x2B94, 0);
    }
    effects_run();
    wr(0x26EE, 0);
    text_notice();
    rm_angle_frac = NULL;
    if (check) {                        /* the program after it, for the reference alone */
        memcpy(s, keep, sizeof keep);
        memcpy(s + 0xFC00, keep2, sizeof keep2);
        gsu = kept;
        fw_ref_pass = 1;
        return 0;
    }
    return 1;
}

void sc_frame_ref_end(void) { fw_ref_pass = 0; }
