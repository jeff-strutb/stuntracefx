/*
 * gpu_ge.c: the cartridge's GPU on the PSP: the full window's spans
 * (fullwindow.h FwSpan) drawn by the GE into the GPU memory nativeshvc
 * lends the game (nshvc_ext_ge_vram, contract 2.15), each pixel as the
 * CPU's fill leaves it (fw_span_fill_cpu, the reference).
 *
 * The GE draws a 4444 target whose low byte is the texel (red the low
 * nibble, green the high): the texels are written into it, the spans drawn
 * over them in their order, and the texels read back. What the GE does
 * here it does exactly:
 *
 *   a flat span        a sprite of its colour (4444 in, 4444 out)
 *   a textured span    each pixel's texel coordinate found on the CPU as
 *                      the reference finds it (32.32 steps, or the
 *                      perspective's float divides), then drawn in runs:
 *                      a run stepping one texel a pixel across a 1:1
 *                      sprite, a run on one texel a sprite of that texel
 *                      at its centre; never a coordinate the GE
 *                      interpolates
 *   the texel's bank   a staging texture per bank, base and mask, the
 *                      texel at (u, v) the byte at ((v << 8 | u) & mask)
 *                      + base, as fw_texel reads it
 *   COLOR and PLOT     the palette lookup's shift, mask and offset (hi |
 *                      v >> 4, hi | v & 15, or v) and its alpha: colour 0,
 *                      or a low nibble of 0, transparent unless the plot
 *                      options say opaque, by the alpha test
 *   the water's cut    each pixel's depth test (fw_water) on the CPU, the
 *                      pixels it leaves out not drawn
 *
 * FW_GPUCHECK=1 (a check): each fill drawn by the CPU too and compared,
 * the count of texels apart printed at exit. Where no GPU memory is lent
 * (none, too little, or the frame's run is over) the CPU fills, counted.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pspge.h>
#include <pspgu.h>
#include <pspkernel.h>

#include "nshvc/nshvc.h"
#include "fullwindow.h"

/* the display list (written uncached: the GE reads it as it is built) */
#define LIST_BYTES (2u * 1024u * 1024u)
#define LIST_ROOM (LIST_BYTES - 64u * 1024u)
static unsigned int __attribute__((aligned(64))) s_list[LIST_BYTES / 4];
static size_t s_used;                   /* the list's bytes, counted generously */

/* the palette lookups: 4444 entries, entry i's low byte i; class 0 every
   one opaque, 1 colour 0 transparent, 2 a low nibble of 0 transparent */
static uint16_t __attribute__((aligned(64))) s_clut[3][256];
static int s_clut_now = -1, s_cmode_now = -1;

/* the staging textures this fill, in an arena reset each fill */
#define ARENA_BYTES (1024u * 1024u)
static uint8_t __attribute__((aligned(64))) s_arena[ARENA_BYTES];
static size_t s_arena_used;
typedef struct {
    uint8_t bank;
    uint16_t base, mask;
    uint8_t *at;
    int tw, th;                         /* its size, powers of two */
    int um, vm;                         /* u and v's masks in it */
} Stage;
#define STAGES 256
static Stage s_stage[STAGES];
static int s_nstage;
static const Stage *s_tex_now;

static long s_fills, s_cpu_fills, s_apart, s_checked;

/* the GPU memory: nativeshvc's loan, or (FW_SPANPLAY, which starts no
   console) all of VRAM, the program's own */
static void *own_vram(size_t *bytes)
{
    *bytes = 2u * 1024u * 1024u;
    return sceGeEdramGetAddr();
}
static void *(*s_lend)(size_t *bytes) = nshvc_ext_ge_vram;

typedef struct {
    float u, v;
    int16_t x, y, z, pad;
} TV;
typedef struct {
    uint16_t c;
    int16_t x, y, z;
} CV;

static void report(void)
{
    printf("gpu_ge: %ld fills on the GE, %ld on the CPU (no GPU memory lent)", s_fills, s_cpu_fills);
    if (s_checked)
        printf("; FW_GPUCHECK: %ld fills checked, %ld texels apart from the CPU's", s_checked, s_apart);
    printf("\n");
    fflush(stdout);
}

/* ---- the list ---- */

static void list_state(int stride, int w, int h, unsigned off)
{
    sceGuDrawBufferList(GU_PSM_4444, (void *)off, stride);
    sceGuOffset(2048 - w / 2, 2048 - h / 2);
    sceGuViewport(2048, 2048, w, h);
    sceGuScissor(0, 0, w, h);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_STENCIL_TEST);
    sceGuDisable(GU_COLOR_TEST);
    sceGuDisable(GU_COLOR_LOGIC_OP);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xFF);
    sceGuPixelMask(0);
    sceGuShadeModel(GU_FLAT);
    sceGuTexMode(GU_PSM_T8, 0, 0, 0);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    s_clut_now = s_cmode_now = -1;
    s_tex_now = NULL;
}

static int s_stride, s_w, s_h;
static unsigned s_off;

static void list_open(void)
{
    sceGuStart(GU_DIRECT, (void *)((uintptr_t)s_list | 0x40000000u));
    list_state(s_stride, s_w, s_h, s_off);
    s_used = 1024;
}

static void list_close(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
}

/* room for a primitive and its state: a list near full run and begun again */
static void room(void)
{
    s_used += 128;
    if (s_used > LIST_ROOM) {
        list_close();
        list_open();
    }
}

/* ---- the staging textures ---- */

static int pow2_at_least(int n)
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

/* the texture for bank, base and mask: u and v's own masks when the mask
   is one in each byte (low bits set), else the whole 256 x 256 */
static const Stage *stage_for(uint8_t bank, uint16_t base, uint16_t mask)
{
    int i, u, v, um = mask & 0xFF, vm = mask >> 8, tw, th, stride;
    Stage *s;
    for (i = 0; i < s_nstage; i++)
        if (s_stage[i].bank == bank && s_stage[i].base == base && s_stage[i].mask == mask)
            return &s_stage[i];
    if (((um + 1) & um) || ((vm + 1) & vm))
        um = vm = 0xFF;
    tw = um + 1;
    th = vm + 1;
    stride = tw < 16 ? 16 : tw;
    if (s_nstage == STAGES || s_arena_used + (size_t)stride * th > ARENA_BYTES)
        return NULL;
    s = &s_stage[s_nstage++];
    s->bank = bank;
    s->base = base;
    s->mask = mask;
    s->at = s_arena + s_arena_used;
    s->tw = tw;
    s->th = th;
    s->um = um;
    s->vm = vm;
    s_arena_used += ((size_t)stride * th + 63) & ~(size_t)63;
    for (v = 0; v < th; v++)
        for (u = 0; u < tw; u++)
            s->at[(size_t)v * stride + u] = fw_texel(bank, (uint16_t)(((v << 8 | u) & mask) + base));
    sceKernelDcacheWritebackRange(s->at, (unsigned)((size_t)stride * th));
    return s;
}

static void use_texture(const Stage *s)
{
    if (s == s_tex_now)
        return;
    sceGuTexImage(0, pow2_at_least(s->tw), pow2_at_least(s->th), s->tw < 16 ? 16 : s->tw, s->at);
    sceGuTexFlush();
    s_tex_now = s;
}

/* COLOR and PLOT's rules as the lookup: its class and mode */
static void use_rules(uint8_t por, uint8_t hi)
{
    int cls = (por & 0x01) ? 0 : (por & 0x08) ? 2 : 1;
    int mode = (por & 0x04) ? (4 | 0x0F << 8 | (hi >> 4) << 16) : (por & 0x08) ? (0x0F << 8 | (hi >> 4) << 16) : (0xFF << 8);
    if (cls != s_clut_now) {
        sceGuClutLoad(32, s_clut[cls]);
        s_clut_now = cls;
    }
    if (mode != s_cmode_now) {
        sceGuClutMode(GU_PSM_4444, (unsigned)(mode & 0x1F), (unsigned)(mode >> 8 & 0xFF), (unsigned)(mode >> 16 & 0x1F));
        s_cmode_now = mode;
    }
}

/* ---- the primitives ---- */

static int s_textured = -1;

static void textured(int on)
{
    if (on == s_textured)
        return;
    if (on) {
        sceGuEnable(GU_TEXTURE_2D);
        sceGuEnable(GU_ALPHA_TEST);
    } else {
        sceGuDisable(GU_TEXTURE_2D);
        sceGuDisable(GU_ALPHA_TEST);
    }
    s_textured = on;
}

static int s_break;                     /* FW_GEBREAK=1 (a check that can fail): each fill's first flat span left out */

static void flat(int32_t y, int32_t x0, int32_t x1, uint8_t c)
{
    CV *v;
    if (s_break == 1) {
        s_break = 2;
        return;
    }
    room();
    textured(0);
    v = (CV *)sceGuGetMemory(2 * sizeof(CV));
    v[0].c = v[1].c = (uint16_t)(0xF000u | c);
    v[0].x = (int16_t)x0;
    v[0].y = (int16_t)y;
    v[1].x = (int16_t)(x1 + 1);
    v[1].y = (int16_t)(y + 1);
    v[0].z = v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}

/* n pixels from x on row y: texels (u, v) on, u stepping one a pixel
   (step 1: a 1:1 sprite) or the one texel (step 0: at its centre) */
static void run(int32_t y, int32_t x, int32_t n, int u, int v, int step)
{
    TV *t;
    room();
    textured(1);
    t = (TV *)sceGuGetMemory(2 * sizeof(TV));
    t[0].x = (int16_t)x;
    t[0].y = (int16_t)y;
    t[1].x = (int16_t)(x + n);
    t[1].y = (int16_t)(y + 1);
    t[0].z = t[1].z = 0;
    if (step) {
        t[0].u = (float)u;
        t[0].v = (float)v;
        t[1].u = (float)(u + n);
        t[1].v = (float)(v + 1);
    } else {
        t[0].u = t[1].u = (float)u + 0.5f;
        t[0].v = t[1].v = (float)v + 0.5f;
    }
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, t);
}

/* the reference's water test (fullwindow.c sunk), as written */
static int sunk(const FwSpan *p, int32_t x)
{
    float z, w;
    if (fw_water[3] == 0 || (p->za == 0 && p->zx == 0 && p->zy == 0))
        return 0;
    z = fmaf(p->zy, (float)p->y, fmaf(p->zx, (float)x, p->za));
    w = fmaf(fw_water[2], (float)p->y, fmaf(fw_water[1], (float)x, fw_water[0]));
    return z > 0 && w > 0 && z < w;
}

/* the reference's perspective texel coordinate (fullwindow.c persp_texel) */
static unsigned persp_texel(float a, float q)
{
    float t = a / q;
    if (!(t > -1.0e9f && t < 1.0e9f))
        t = 0.0f;
    return (unsigned)(int)floorf(t) & 0xFF;
}

static void span(const FwSpan *p, int w, int h);

static void span_textured(const FwSpan *p)
{
    const Stage *s = stage_for(p->bank, p->base, p->mask);
    int deep = fw_water[3] != 0 && (p->za != 0 || p->zx != 0 || p->zy != 0);
    int32_t x, rx = 0, rn = 0;
    int ru = 0, rv = 0, rstep = 0;
    if (!s) {                           /* (the arena full: none drawn so far, a check's count shows it) */
        return;
    }
    use_texture(s);
    use_rules(p->por, p->hi);
    for (x = p->x0; x <= p->x1 + 1; x++) {
        int u = 0, v = 0, have = 0;
        if (x <= p->x1 && !(deep && sunk(p, x))) {
            unsigned iu, iv;
            if (p->textured == 2) {
                float k = (float)(x - p->xl), q = fmaf(p->pdq, k, p->pq);
                iu = persp_texel(fmaf(p->pda, k, p->pa), q);
                iv = persp_texel(fmaf(p->pdb, k, p->pb), q);
            } else {
                int64_t k = x - p->xl;
                iu = (unsigned)((p->u + p->du * k) >> 32) & 0xFF;
                iv = (unsigned)((p->v + p->dv * k) >> 32) & 0xFF;
            }
            u = (int)(iu & (unsigned)s->um);
            v = (int)(iv & (unsigned)s->vm);
            have = 1;
        }
        if (rn && have && x == rx + rn && v == rv &&
            ((rstep == 1 && u == ru + rn) || (rstep == 0 && u == ru) || (rn == 1 && (u == ru + 1 || u == ru)))) {
            if (rn == 1)
                rstep = u == ru + 1;
            rn++;
            continue;
        }
        if (rn)
            run(p->y, rx, rn, ru, rv, rn == 1 ? 1 : rstep);
        rn = 0;
        if (have) {
            rx = x;
            rn = 1;
            ru = u;
            rv = v;
            rstep = 1;
        }
    }
}

static void span(const FwSpan *p, int w, int h)
{
    int32_t x, a;
    (void)h;
    if (p->textured == 3) {             /* a polygon's rows, in its place */
        int32_t row;
        FwSpan r;
        for (row = p->y; row < p->x0; row++)
            if (fw_poly_row(&fw_polys[p->x1], fw_edges, row, w, &r))
                span(&r, w, h);
        return;
    }
    if (p->textured) {
        span_textured(p);
        return;
    }
    if (!(fw_water[3] != 0 && (p->za != 0 || p->zx != 0 || p->zy != 0))) {
        flat(p->y, p->x0, p->x1, p->colour);
        return;
    }
    for (x = a = p->x0; x <= p->x1 + 1; x++)   /* the water's cut: the runs it leaves */
        if (x > p->x1 || sunk(p, x)) {
            if (x > a)
                flat(p->y, a, x - 1, p->colour);
            a = x + 1;
        }
}

/* ---- the fill ---- */

static uint8_t *s_ref;
static size_t s_ref_len;

static void fill(uint8_t *out, int w, int h, const FwSpan *sp, size_t n)
{
    size_t bytes = 0, i;
    void *lent = s_lend(&bytes);
    int stride = (w + 63) & ~63, x, y;
    volatile uint16_t *t;
    static int reported;

    if (!reported) {
        reported = 1;
        atexit(report);
    }
    if (!lent || (size_t)stride * (size_t)h * 2 > bytes || w > 1024 || h > 1024) {
        s_cpu_fills++;
        fw_span_fill_cpu(out, w, h, sp, n);
        return;
    }
    s_fills++;
    if (getenv("FW_GPUCHECK")) {
        size_t len = (size_t)w * (size_t)h;
        if (s_ref_len < len) {
            free(s_ref);
            s_ref = malloc(len);
            s_ref_len = s_ref ? len : 0;
        }
        if (s_ref) {
            memcpy(s_ref, out, len);
            fw_span_fill_cpu(s_ref, w, h, sp, n);
        }
    }
    /* the texels into the target, the low byte each */
    t = (volatile uint16_t *)((uintptr_t)lent | 0x40000000u);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            t[(size_t)y * stride + x] = (uint16_t)(0xF000u | out[(size_t)y * w + x]);
    s_stride = stride;
    s_w = w;
    s_h = h;
    s_off = (unsigned)((uintptr_t)lent - (uintptr_t)sceGeEdramGetAddr());
    s_nstage = 0;
    s_arena_used = 0;
    s_textured = -1;
    s_break = getenv("FW_GEBREAK") ? 1 : 0;
    list_open();
    for (i = 0; i < n; i++)
        span(&sp[i], w, h);
    list_close();
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            out[(size_t)y * w + x] = (uint8_t)t[(size_t)y * stride + x];
    if (getenv("FW_GPUCHECK") && s_ref) {
        size_t len = (size_t)w * (size_t)h;
        for (i = 0; i < len; i++)
            s_apart += out[i] != s_ref[i];
        s_checked++;
    }
}

/* FW_SPANPLAY: the GE the program's own (no console runs, so libgu is
   started here and all of VRAM is the fill's) */
void gpu_ge_standalone(void)
{
    sceGuInit();
    s_lend = own_vram;
}

/* the GE's fill in fw_span_filler's place */
int gpu_ge_init(void)
{
    int c, i;
    for (c = 0; c < 3; c++)
        for (i = 0; i < 256; i++) {
            int clear = (c == 1 && i == 0) || (c == 2 && (i & 0x0F) == 0);
            s_clut[c][i] = (uint16_t)(i | (clear ? 0 : 0xF000));
        }
    sceKernelDcacheWritebackRange(s_clut, sizeof s_clut);
    fw_span_filler = fill;
    return 1;
}
