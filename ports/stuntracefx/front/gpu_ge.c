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
static int s_wrap;                      /* the texture repeats (a polygon's) or is clamped (a run's) */

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

static long s_polys_ge, s_polys_cpu, s_apart_far, s_wt_cpu[2];
static SceInt64 s_wt_t[4];   /* (a measure) the water fill's parts: the runs found and listed, the GE, the picture made */
static long s_fills, s_cpu_fills, s_apart, s_checked, s_water_ge, s_water_cpu, s_water_rows_cpu, s_water_texels;

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
    printf("gpu_ge: %ld fills on the GE, %ld on the CPU (no GPU memory lent); %ld polygons the GE's whole, %ld by rows; "
           "%ld water passes on the GE, %ld on the CPU, %ld water rows the CPU's, %ld water texels the GE's", s_fills,
           s_cpu_fills, s_polys_ge, s_polys_cpu, s_water_ge, s_water_cpu, s_water_rows_cpu, s_water_texels);
    printf("; water texels the CPU's: %ld in rows not found, %ld in rows found; water fill ms: list %.1f GE %.1f picture %.1f",
           s_wt_cpu[0], s_wt_cpu[1], s_wt_t[0] / 1000.0, s_wt_t[1] / 1000.0, s_wt_t[2] / 1000.0);
    if (s_checked)
        printf("; FW_GPUCHECK: %ld fills checked, %ld texels apart from the CPU's, %ld of them not the CPU's within a pixel",
               s_checked, s_apart, s_apart_far);
    printf("\n");
    fflush(stdout);
}

static void report_once(void)
{
    static int on;
    if (!on) {
        on = 1;
        atexit(report);
    }
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
    /* the polygons' (poly_ge): clip-space vertices, the matrices passing
       them on (x, y, w from the vertex's x, y, z), no clipping by the GE
       (the CPU keeps them to its screen), coordinates in whole textures */
    {
        static ScePspFMatrix4 proj = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } };
        static ScePspFMatrix4 ident = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
        sceGuSetMatrix(GU_PROJECTION, &proj);
        sceGuSetMatrix(GU_VIEW, &ident);
        sceGuSetMatrix(GU_MODEL, &ident);
        sceGuDepthRange(65535, 0);
        sceGuDisable(GU_CULL_FACE);
        sceGuDisable(GU_LIGHTING);
        sceGuDisable(GU_FOG);
        sceGuDisable(GU_CLIP_PLANES);
        sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0);
        sceGuTexScale(1.0f, 1.0f);
        sceGuTexOffset(0.0f, 0.0f);
    }
    s_clut_now = s_cmode_now = -1;
    s_tex_now = NULL;
    s_wrap = 0;
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

static int s_rows;                      /* FW_GEROWS=1: every polygon by its rows, the reference's pixels */
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
    if (s_wrap) {
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        s_wrap = 0;
    }
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
    z = fw_fmaf(p->zy, (float)p->y, fw_fmaf(p->zx, (float)x, p->za));
    w = fw_fmaf(fw_water[2], (float)p->y, fw_fmaf(fw_water[1], (float)x, fw_water[0]));
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
                float k = (float)(x - p->xl), q = fw_fmaf(p->pdq, k, p->pq);
                iu = persp_texel(fw_fmaf(p->pda, k, p->pa), q);
                iv = persp_texel(fw_fmaf(p->pdb, k, p->pb), q);
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

/* ---- a polygon whole: the GE's raster ----
   The polygon's outline from its chains of edges (FwPoly): the left chain
   down from the top, the right chain back up. Each vertex at its edge's
   end, u and v there (each edge's change carried on, so the texture runs
   on across a turn of 256 as the reference's nearer turn does), 1 / z
   there (perspective), the rest affine. Placed so the GE's pixel-centre
   rule fills the reference's pixels to within one at an edge: rows from
   the vertex row (y + 0.5), the left chain half a pixel left, the right
   half right (the reference's rows take both ends' columns). Kept on the
   CPU to a band about the screen (the GE drops a primitive past its 4096
   square), then drawn as a fan of clip-space vertices, w = z: the GE
   interpolates u and v perspective-correct. */
#define PV_MAX 96
#define PV_BAND 1024.0f
typedef struct {
    float x, y, uq, vq, q;
} PV;
typedef struct {
    float u, v;
    float x, y, z;
} PTV;
typedef struct {
    uint16_t c, pad;
    float x, y, z;
} PCV;

static int pv_clip(const PV *in, int n, PV *out, int axis, float lim, int keep_below)
{
    int i, m = 0;
    for (i = 0; i < n && m < PV_MAX; i++) {
        const PV *a = &in[i], *b = &in[(i + 1) % n];
        float pa = axis ? a->y : a->x, pb = axis ? b->y : b->x;
        int ain = keep_below ? pa <= lim : pa >= lim, bin = keep_below ? pb <= lim : pb >= lim;
        if (ain)
            out[m++] = *a;
        if (ain != bin && m < PV_MAX) {
            float t = (lim - pa) / (pb - pa);
            out[m].x = a->x + t * (b->x - a->x);
            out[m].y = a->y + t * (b->y - a->y);
            out[m].uq = a->uq + t * (b->uq - a->uq);
            out[m].vq = a->vq + t * (b->vq - a->vq);
            out[m].q = a->q + t * (b->q - a->q);
            if (axis)
                out[m].y = lim;
            else
                out[m].x = lim;
            m++;
        }
    }
    return m;
}

/* the chain's vertices (first, count edges) into o from k on: x shifted by
   dx; u and v carried along it */
static int chain(const FwEdge *e, int n, int persp, float dx, PV *o, int k)
{
    float u = (float)e[0].ua, v = (float)e[0].va;
    int i;
    for (i = 0; i <= n && k < PV_MAX; i++) {
        const FwEdge *f = &e[i < n ? i : n - 1];
        float q = persp ? (i < n ? f->qa : f->qb) : 1.0f;
        o[k].x = (float)(i < n ? f->from : f->to) + dx;
        o[k].y = (float)(i < n ? f->y0 : f->y0 + f->n) + 0.5f;
        o[k].q = q;
        o[k].uq = u * q;
        o[k].vq = v * q;
        k++;
        if (i < n) {
            u += (float)f->dua;
            v += (float)f->dva;
        }
    }
    return k;
}

/* 1 drawn on the GE, 0 left to the reference's rows */
static int poly_ge(const FwPoly *p, int w, int h)
{
    static PV a[PV_MAX], b[PV_MAX];
    PV r[PV_MAX / 2 + 2];
    const FwEdge *L = fw_edges + p->le, *R = fw_edges + p->re;
    int persp = p->kind && p->persp, n, nr, i;
    const Stage *s = NULL;
    float tws = 1.0f, ths = 1.0f, ou = 0, ov = 0;

    if (fw_water[3] != 0 && (p->za != 0 || p->zx != 0 || p->zy != 0))
        return 0;                       /* the water's cut: a pixel's test, the reference's rows */
    if (p->ln < 1 || p->rn < 1 || p->ln + p->rn + 2 > PV_MAX / 2)
        return 0;
    for (i = 0; persp && i < p->ln; i++)
        if (!(L[i].qa > 0 && L[i].qb > 0))
            return 0;
    for (i = 0; persp && i < p->rn; i++)
        if (!(R[i].qa > 0 && R[i].qb > 0))
            return 0;
    if (p->kind && !(s = stage_for(p->bank, p->base, p->mask)))
        return 0;
    n = chain(L, p->ln, persp, -0.5f, a, 0);
    nr = chain(R, p->rn, persp, 0.5f, r, 0);
    for (i = nr - 1; i >= 0 && n < PV_MAX; i--)
        a[n++] = r[i];
    n = pv_clip(a, n, b, 0, -PV_BAND, 0);
    n = pv_clip(b, n, a, 0, (float)w + PV_BAND, 1);
    n = pv_clip(a, n, b, 1, -PV_BAND, 0);
    n = pv_clip(b, n, a, 1, (float)h + PV_BAND, 1);
    if (n < 3)
        return 1;
    room();
    s_used += (size_t)n * sizeof(PTV);
    if (p->kind) {
        PTV *t;
        textured(1);
        use_texture(s);
        use_rules(p->por, p->hi);
        if (!s_wrap) {
            sceGuTexWrap(GU_REPEAT, GU_REPEAT);
            s_wrap = 1;
        }
        tws = (float)pow2_at_least(s->tw);
        ths = (float)pow2_at_least(s->th);
        ou = (float)p->ou / 256.0f;
        ov = (float)p->ov / 256.0f;
        t = (PTV *)sceGuGetMemory((unsigned)n * sizeof(PTV));
        for (i = 0; i < n; i++) {
            float W = 1.0f / a[i].q;
            t[i].u = (a[i].uq * W + ou) / tws;
            t[i].v = (a[i].vq * W + ov) / ths;
            t[i].x = (2.0f * a[i].x / (float)w - 1.0f) * W;
            t[i].y = (1.0f - 2.0f * a[i].y / (float)h) * W;
            t[i].z = W;
        }
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, t);
    } else {
        PCV *c;
        textured(0);
        c = (PCV *)sceGuGetMemory((unsigned)n * sizeof(PCV));
        for (i = 0; i < n; i++) {
            c[i].c = (uint16_t)(0xF000u | p->colour);
            c[i].pad = 0;
            c[i].x = 2.0f * a[i].x / (float)w - 1.0f;
            c[i].y = 1.0f - 2.0f * a[i].y / (float)h;
            c[i].z = 1.0f;
        }
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_4444 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, c);
    }
    return 1;
}

static void span(const FwSpan *p, int w, int h)
{
    int32_t x, a;
    (void)h;
    if (p->textured == 3) {             /* a polygon: the GE's whole, else its rows, in its place */
        int32_t row;
        FwSpan r;
        if (!s_rows && poly_ge(&fw_polys[p->x1], w, h)) {
            s_polys_ge++;
            return;
        }
        s_polys_cpu++;
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

    report_once();
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
    s_rows = getenv("FW_GEROWS") != NULL;
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
            if (out[i] != s_ref[i]) {
                /* apart by a shift of at most a pixel (the GE's edges, its
                   texel centres) when the reference has the GE's texel
                   within a pixel of it; else a texel nowhere near */
                int px = (int)(i % (size_t)w), py = (int)(i / (size_t)w), dx, dy, near = 0;
                for (dy = -1; dy <= 1 && !near; dy++)
                    for (dx = -1; dx <= 1 && !near; dx++)
                        if (px + dx >= 0 && px + dx < w && py + dy >= 0 && py + dy < h)
                            near = s_ref[(size_t)(py + dy) * w + px + dx] == out[i];
                s_apart++;
                s_apart_far += !near;
            }
        s_checked++;
        if (getenv("FW_GPUDUMP") && s_checked == atol(getenv("FW_GPUDUMP"))) {
            /* (a check) this fill's texels, the GE's and the reference's, as two 8-bit images */
            FILE *f = fopen("host0:/gdump.bin", "wb");
            if (f) {
                int32_t wh[2] = { w, h };
                fwrite(wh, sizeof wh, 1, f);
                fwrite(out, 1, len, f);
                fwrite(s_ref, 1, len, f);
                fclose(f);
            }
        }
    }
}


/* ---- the water pass ----
   The reference (fullwindow.c water_texel) gives each texel nothing was
   drawn on the water surface's colour along its ray: the line's colour
   g mixed toward the ripples' colour by the share of ripple texels in
   the pixel's footprint on the surface. Here the CPU keeps what it can
   say exactly: the texels drawn on, which of a row's pixels see the
   surface (one run of them: the ray's slope to it is monotone along a
   row), the row's colours, and the pixels at the run's ends; the GE
   draws each run as a one-row quad with the surface's true perspective
   (each corner at its 1 / depth, its texture coordinates the surface's),
   the ripple cells a texture whose mip levels average them (for the
   footprint's share), mixed by the GE's blend function: g (the vertex)
   toward the ripples' colour (the texture's environment colour) by the
   texel's share. A row the line's colour leaves to the console (colour
   1), or the horizon crosses, is the CPU's. */
static uint32_t __attribute__((aligned(16))) s_cells[64 * 16 + 32 * 8 + 16 * 4 + 8 * 2];

static uint32_t abgr(uint32_t argb) { return (argb & 0xFF00FF00u) | (argb >> 16 & 0xFFu) | (argb & 0xFFu) << 16; }

typedef struct {
    float s, t;
    uint32_t c;
    float x, y, z;
} WV;

/* the reference's ray hit at pixel (x, y)'s centre: 1 and its 1 / depth
   rank (the strip row by depth, as water_texel finds it), or 0 */
static int hit_at(const FwWaterJob *j, int32_t x, int32_t y, int32_t *lo_out)
{
    const float *e = j->e;
    float vx = ((float)x + 0.5f - j->ox) / j->fx, vy = ((float)y + 0.5f - j->oy) / j->fy;
    float wy = fw_fmaf(e[4], vx, fw_fmaf(e[5], vy, e[6])), t = j->dy / wy;
    int32_t lo, hi;
    if (!(wy != 0) || !(t > 0))
        return 0;
    if (lo_out) {
        for (lo = 0, hi = 79; lo < hi;) {
            int32_t m = (lo + hi) / 2;
            if (j->depth[m] >= t)
                hi = m;
            else
                lo = m + 1;
        }
        *lo_out = lo;
    }
    return 1;
}

/* the surface at the screen point (X, Y) (a pixel's corner): its 1 / depth
   q and texture coordinates in cells, s along 64 and t along 16 */
static void surface_at(const FwWaterJob *j, float X, float Y, float *q, float *s, float *t)
{
    const float *e = j->e;
    float vx = (X - j->ox) / j->fx, vy = (Y - j->oy) / j->fy, wy = e[4] * vx + e[5] * vy + e[6], d;
    *q = wy / j->dy;
    d = j->dy / wy;
    *s = (d * (e[0] * vx + e[1] * vy + e[2]) + j->cam[0]) / j->wu / 64.0f;
    *t = (d * (e[8] * vx + e[9] * vy + e[10]) + j->cam[2]) / j->wv / 16.0f;
}

static void cells_make(const FwWaterJob *j)
{
    static const int size[4][2] = { { 64, 16 }, { 32, 8 }, { 16, 4 }, { 8, 2 } };
    uint32_t *lv[4];
    int l, x, y;
    lv[0] = s_cells;
    for (l = 1; l < 4; l++)
        lv[l] = lv[l - 1] + size[l - 1][0] * size[l - 1][1];
    for (y = 0; y < 16; y++)
        for (x = 0; x < 64; x++) {
            int32_t n = j->sat[(y + 1) * 65 + x + 1] - j->sat[y * 65 + x + 1] - j->sat[(y + 1) * 65 + x] + j->sat[y * 65 + x];
            lv[0][y * 64 + x] = n ? 0xFFFFFFFFu : 0xFF000000u;
        }
    for (l = 1; l < 4; l++)              /* each level the one below's 2 x 2 averaged */
        for (y = 0; y < size[l][1]; y++)
            for (x = 0; x < size[l][0]; x++) {
                const uint32_t *b = lv[l - 1];
                int w = size[l - 1][0];
                unsigned v = (b[(2 * y) * w + 2 * x] & 255) + (b[(2 * y) * w + 2 * x + 1] & 255) +
                             (b[(2 * y + 1) * w + 2 * x] & 255) + (b[(2 * y + 1) * w + 2 * x + 1] & 255);
                v = (v + 2) / 4;
                lv[l][y * size[l][0] + x] = 0xFF000000u | v * 0x010101u;
            }
    sceKernelDcacheWritebackRange(s_cells, sizeof s_cells);
    sceGuTexMode(GU_PSM_8888, 3, 0, 0);
    for (l = 0; l < 4; l++)
        sceGuTexImage(l, size[l][0], size[l][1], size[l][0], lv[l]);
    sceGuTexFlush();
}

/* one run of row y, pixels xa to xb, its line colour g and ripple colour r */
static void water_run(const FwWaterJob *j, int32_t y, int32_t xa, int32_t xb, uint32_t g, uint32_t r)
{
    WV *v;
    int i;
    room();
    v = (WV *)sceGuGetMemory(4 * sizeof(WV));
    for (i = 0; i < 4; i++) {
        float X = (float)((i & 1) ? xb + 1 : xa), Y = (float)((i & 2) ? y + 1 : y), q, s, t, w;
        surface_at(j, X, Y, &q, &s, &t);
        w = 1.0f / q;
        v[i].s = s;
        v[i].t = t;
        v[i].c = abgr(g);
        v[i].x = (2.0f * X / (float)j->w - 1.0f) * w;
        v[i].y = (1.0f - 2.0f * Y / (float)j->h) * w;
        v[i].z = w;
    }
    sceGuTexEnvColor(abgr(r));
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
}

/* q at the run's corners all well in front of the eye */
static int corners_ok(const FwWaterJob *j, int32_t y, int32_t xa, int32_t xb)
{
    int i;
    for (i = 0; i < 4; i++) {
        float q, s, t;
        surface_at(j, (float)((i & 1) ? xb + 1 : xa), (float)((i & 2) ? y + 1 : y), &q, &s, &t);
        if (!(q > 1e-6f))
            return 0;
    }
    return 1;
}

typedef struct {
    int32_t y, xa, xb;
} WRun;

static void water_fill(const uint8_t *in, uint32_t *out, const FwWaterJob *j, const uint32_t *gap)
{
    size_t bytes = 0;
    void *lent = s_lend(&bytes);
    int w = j->w, h = j->h, stride = (w + 63) & ~63, x, y, nr = 0;
    static WRun *runs;
    static int cap_runs;
    /* each row's run of pixels that see the surface, when found exactly
       (known): every pixel past it is the reference's no-hit texel */
    static int32_t *hit_a, *hit_b;
    static uint8_t *known;
    static int cap_rows;
    volatile uint32_t *tg;
    static ScePspFMatrix4 proj = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } };
    static ScePspFMatrix4 ident = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };

    if (!lent || (size_t)stride * (size_t)h * 4 > bytes || w > 1024 || h > 1024 || j->point) {
        s_water_cpu++;
        fw_water_fill_cpu(in, out, j, gap);
        return;
    }
    if (cap_rows < h) {
        free(hit_a);
        free(hit_b);
        free(known);
        hit_a = malloc((size_t)h * sizeof *hit_a);
        hit_b = malloc((size_t)h * sizeof *hit_b);
        known = malloc((size_t)h);
        cap_rows = hit_a && hit_b && known ? h : 0;
        if (!cap_rows) {
            fw_water_fill_cpu(in, out, j, gap);
            return;
        }
    }
    memset(known, 0, (size_t)h);
    if (cap_runs < 4 * h) {
        free(runs);
        cap_runs = 4 * h;
        runs = malloc((size_t)cap_runs * sizeof *runs);
        if (!runs) {
            cap_runs = 0;
            fw_water_fill_cpu(in, out, j, gap);
            return;
        }
    }
    report_once();
    s_water_ge++;
    SceInt64 t0 = sceKernelGetSystemTimeWide();
    sceGuStart(GU_DIRECT, (void *)((uintptr_t)s_list | 0x40000000u));
    s_used = 1024;
    sceGuDrawBufferList(GU_PSM_8888, (void *)((uintptr_t)lent - (uintptr_t)sceGeEdramGetAddr()), stride);
    sceGuOffset(2048 - w / 2, 2048 - h / 2);
    sceGuViewport(2048, 2048, w, h);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, w, h);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_STENCIL_TEST);
    sceGuDisable(GU_COLOR_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_COLOR_LOGIC_OP);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_CLIP_PLANES);
    sceGuPixelMask(0);
    sceGuShadeModel(GU_FLAT);
    sceGuSetMatrix(GU_PROJECTION, &proj);
    sceGuSetMatrix(GU_VIEW, &ident);
    sceGuSetMatrix(GU_MODEL, &ident);
    sceGuEnable(GU_TEXTURE_2D);
    cells_make(j);
    sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.0f);
    sceGuTexFilter(GU_LINEAR_MIPMAP_LINEAR, GU_NEAREST);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuTexFunc(GU_TFX_BLEND, GU_TCC_RGB);
    for (y = 0; y < h; y++) {
        uint32_t g = gap[y], r0 = j->land ? gap[h + y] : 0;
        int32_t a = 0, b = w - 1, lo_a, lo_b;
        int ha, hb;
        if (g == 1 || (j->land && r0 == 1)) {   /* colour 1 left to the console: the CPU's */
            s_water_rows_cpu++;
            continue;
        }
        /* the run of pixels that see the surface: its ends by halving, the
           hit monotone along the row */
        ha = hit_at(j, 0, y, NULL);
        hb = hit_at(j, w - 1, y, NULL);
        if (!ha && !hb) {
            if (hit_at(j, w / 2, y, NULL)) {    /* (not monotone: the CPU's) */
                s_water_rows_cpu++;
                continue;
            }
            known[y] = 1;               /* no pixel sees the surface */
            hit_a[y] = 1;
            hit_b[y] = 0;
            continue;
        }
        if (!ha) {                      /* a = the first hit */
            int32_t l = 0, r = w - 1;
            while (r - l > 1) {
                int32_t m = (l + r) / 2;
                if (hit_at(j, m, y, NULL))
                    r = m;
                else
                    l = m;
            }
            a = r;
        }
        if (!hb) {                      /* b = the last hit */
            int32_t l = a, r = w - 1;
            while (r - l > 1) {
                int32_t m = (l + r) / 2;
                if (hit_at(j, m, y, NULL))
                    l = m;
                else
                    r = m;
            }
            b = l;
        }
        known[y] = 1;
        hit_a[y] = a;
        hit_b[y] = b;
        a++;                            /* its ends the CPU's (the horizon may cross them) */
        b--;
        while (a <= b && !corners_ok(j, y, a, a))
            a++;
        while (a <= b && !corners_ok(j, y, b, b))
            b--;
        if (a > b || !corners_ok(j, y, a, b))
            continue;
        /* split where the ripples' colour changes (Aqua Tunnel's: by depth) */
        while (a <= b) {
            int32_t e = b;
            hit_at(j, a, y, &lo_a);
            if (!j->land) {
                hit_at(j, b, y, &lo_b);
                if (lo_b != lo_a) {     /* the last pixel at a's colour, by halving */
                    int32_t l = a, r = b;
                    while (r - l > 1) {
                        int32_t m = (l + r) / 2, lm;
                        hit_at(j, m, y, &lm);
                        if (lm == lo_a)
                            l = m;
                        else
                            r = m;
                    }
                    e = l;
                }
            }
            if (nr == cap_runs)
                break;
            water_run(j, y, a, e, g, j->land ? r0 : j->ripple[lo_a]);
            runs[nr].y = y;
            runs[nr].xa = a;
            runs[nr].xb = e;
            nr++;
            a = e + 1;
        }
    }
    SceInt64 t1 = sceKernelGetSystemTimeWide();
    sceGuFinish();
    sceGuSync(0, 0);
    SceInt64 t2 = sceKernelGetSystemTimeWide();
    /* the picture: the GE's runs, the rest the reference's own */
    tg = (volatile uint32_t *)((uintptr_t)lent | 0x40000000u);
    {
        /* a row at a time: drawn texels kept (the reference's first rule);
           left and right of the row's hits its no-hit texel; in its runs
           the GE's; elsewhere the reference's own */
        int k = 0;
        for (y = 0; y < h; y++) {
            const uint8_t *ri = in + (size_t)y * w;
            uint32_t *ro = out + (size_t)y * w, none = j->under ? gap[y] : 0;
            volatile const uint32_t *rg = tg + (size_t)y * stride;
            int32_t a = 0, b = w - 1;
            if (known[y]) {
                a = hit_a[y];
                b = hit_b[y];
                for (x = 0; x < w && x < a; x++)
                    ro[x] = ri[x] ? ri[x] : none;
                for (x = b + 1 < 0 ? 0 : b + 1; x < w; x++)
                    ro[x] = ri[x] ? ri[x] : none;
            }
            while (k < nr && runs[k].y < y)
                k++;
            for (x = a; x <= b;) {
                int32_t end = b;
                int in_run = k < nr && runs[k].y == y && x >= runs[k].xa;
                if (in_run)
                    end = runs[k].xb;
                else if (k < nr && runs[k].y == y && runs[k].xa - 1 < end)
                    end = runs[k].xa - 1;
                for (; x <= end; x++) {
                    if (ri[x]) {
                        ro[x] = ri[x];
                    } else if (in_run) {
                        uint32_t p = rg[x];
                        s_water_texels++;
                        ro[x] = 0xFF000000u | (p & 0xFFu) << 16 | (p & 0xFF00u) | (p >> 16 & 0xFFu);
                    } else {
                        s_wt_cpu[known[y] ? 1 : 0]++;
                        ro[x] = fw_water_texel(in, j, gap, x, y);
                    }
                }
                if (in_run)
                    k++;
            }
        }
    }
    SceInt64 t3 = sceKernelGetSystemTimeWide();
    s_wt_t[0] += t1 - t0;
    s_wt_t[1] += t2 - t1;
    s_wt_t[2] += t3 - t2;
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
    fw_water_filler = water_fill;
    return 1;
}
