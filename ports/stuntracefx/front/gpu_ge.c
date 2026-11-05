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
#include "gpu_bin.h"
#include "scene.h"

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
/* the picture's canvas (fw_canvas_gpu) being drawn: 32 bits a pixel, a
   colour index i as 0xFF0000ii, the ground surface's colour 0x01rrggbb (its
   alpha the stencil's, which keeps it from what is drawn), nothing 0; its
   lookups the same with 32-bit entries */
static int s_c32;
static uint32_t __attribute__((aligned(64))) s_clut32[3][256];

/* the staging textures this fill, in an arena reset each fill */
#define ARENA_BYTES (1024u * 1024u)
static uint8_t __attribute__((aligned(64))) s_arena[ARENA_BYTES];
static size_t s_arena_used;
typedef struct {
    uint8_t bank;
    uint16_t base, mask;
    uint32_t pal;                       /* a 2-bit bitmap's palette (its four bytes) */
    uint8_t bitmap;                     /* a 2-bit bitmap's (1), an 8-bit texture's (0) */
    uint8_t *at;
    int tw, th;                         /* its size, powers of two */
    int um, vm;                         /* u and v's masks in it */
} Stage;
#define STAGES 256
static Stage s_stage[STAGES];
static int s_nstage;
static const Stage *s_tex_now;

static long s_polys_ge, s_polys_cpu, s_apart_far, s_wt_cpu[2];
static long s_cv_opens, s_cv_kept, s_ground_rows, s_ground_flat_rows, s_ground_apart, s_ground_far, s_ground_checked;

static SceInt64 s_wt_t[4];   /* (a measure) the water fill's parts: the runs found and listed, the GE, the picture made */
static long s_fills, s_cpu_fills, s_apart, s_checked, s_water_ge, s_water_cpu, s_water_rows_cpu, s_water_texels;

/* the GPU memory: nativeshvc's loan, or (FW_SPANPLAY, which starts no
   console) all of VRAM, the program's own */
static void *own_vram(size_t *bytes)
{
    *bytes = 2u * 1024u * 1024u;
    return sceGeEdramGetAddr();
}
#ifdef SRFX_NSHVC_HANDOFF
/* the loan with its depth buffer (contract 2.22): s_zb its address, s_zbw its
   stride, NULL with none (paused, the backdrop's there) */
static void *s_zb;
static int s_zbw;
static void *lend_vram(size_t *bytes)
{
    void *p;
    s_zb = NULL;
    s_zbw = 0;
    p = nshvc_ext_ge_vram2(bytes, &s_zb, &s_zbw);
    if (!p)
        s_zb = NULL;
    return p;
}
static void *(*s_lend)(size_t *bytes) = lend_vram;
#else
static void *(*s_lend)(size_t *bytes) = nshvc_ext_ge_vram;
#endif

typedef struct {
    float u, v;
    int16_t x, y, z, pad;
} TV;
typedef struct {
    uint16_t c;
    int16_t x, y, z;
} CV;
typedef struct {
    uint32_t c;
    int16_t x, y, z, pad;
} CV32;

static SceInt64 s_cvt[4];               /* (a measure) the canvas's open, spans, ground and close, in all */
static long x_kind[4];
static SceInt64 x_sync;
static SceInt64 x_kt[4];
#define STAGE_HASH 1024
static int16_t s_hash[STAGE_HASH];      /* a stage's index + 1, 0 none */
static int s_ram_stages;
static long s_stage_made, s_stage_resets;
static long s_quads, s_quads_cut;
static void report(void)
{
    printf("gpu_ge: %ld fills on the GE, %ld on the CPU (no GPU memory lent); %ld polygons the GE's whole, %ld by rows; "
           "%ld water passes on the GE, %ld on the CPU, %ld water rows the CPU's, %ld water texels the GE's", s_fills,
           s_cpu_fills, s_polys_ge, s_polys_cpu, s_water_ge, s_water_cpu, s_water_rows_cpu, s_water_texels);
    printf("; spans by kind (flat, textured, perspective, polygon): %ld %ld %ld %ld, ms %.1f %.1f %.1f %.1f, sync %.1f",
           x_kind[0], x_kind[1], x_kind[2], x_kind[3], x_kt[0] / 1000.0, x_kt[1] / 1000.0, x_kt[2] / 1000.0,
           x_kt[3] / 1000.0, x_sync / 1000.0);
    printf("; textures staged %ld, all made again %ld times; screen quads %ld (%ld cut by the water)", s_stage_made,
           s_stage_resets, s_quads, s_quads_cut);
    printf("; canvas ms in all: open %.1f spans %.1f ground %.1f close %.1f", s_cvt[0] / 1000.0, s_cvt[1] / 1000.0,
           s_cvt[2] / 1000.0, s_cvt[3] / 1000.0);
    printf("; canvas: %ld opened, %ld over the last; ground rows %ld, %ld of one colour", s_cv_opens, s_cv_kept,
           s_ground_rows, s_ground_flat_rows);
    if (s_ground_checked)
        printf("; FW_GPUCHECK ground: %ld passes, %ld texels apart, %ld by more than 16 a channel or of another kind",
               s_ground_checked, s_ground_apart, s_ground_far);
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
    sceGuDrawBufferList(s_c32 ? GU_PSM_8888 : GU_PSM_4444, (void *)off, stride);
    sceGuOffset(2048 - w / 2, 2048 - h / 2);
    sceGuViewport(2048, 2048, w, h);
    sceGuScissor(0, 0, w, h);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    if (s_c32) {                        /* the canvas: what is drawn marked in alpha, the stencil (the GE writes it
                                           only through the stencil's operation) */
        sceGuEnable(GU_STENCIL_TEST);
        sceGuStencilFunc(GU_ALWAYS, 0xFF, 0xFF);
        sceGuStencilOp(GU_REPLACE, GU_REPLACE, GU_REPLACE);
    } else {
        sceGuDisable(GU_STENCIL_TEST);
    }
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
#ifdef SRFX_NSHVC_HANDOFF
    if (s_c32 && s_zb) {
        /* the water's cut (plane_write): by the GE's registers ZBP and ZBW, not
           sceGuDepthBuffer (libgu's kept state, the loan's contract); the host puts
           the GE's state back. Tested per primitive, written only by plane_write. */
        unsigned at = (unsigned)((uintptr_t)s_zb - (uintptr_t)sceGeEdramGetAddr());
        sceGuSendCommandi(158, (int)(at & 0xFFFFFFu));
        sceGuSendCommandi(159, (int)((at >> 24) << 16 | (unsigned)s_zbw));
        sceGuDepthMask(GU_TRUE);
        sceGuDepthFunc(GU_GEQUAL);
    }
#endif
    s_clut_now = s_cmode_now = -1;
    s_tex_now = NULL;
    s_wrap = 0;
}

#ifdef SRFX_NSHVC_HANDOFF
/* The depth test on or off, per primitive (the list opens with it off): on for
   what the water cuts (s_zcut, the plane written). */
static int s_zt, s_zcut;
static void ztest(int on)
{
    if (on == s_zt)
        return;
    if (on)
        sceGuEnable(GU_DEPTH_TEST);
    else
        sceGuDisable(GU_DEPTH_TEST);
    s_zt = on;
}
#else
#define ztest(on) ((void)0)
#define s_zcut 0
#endif

static int s_stride, s_w, s_h;
static unsigned s_off;
static int s_model;                     /* the display list's matrices in force (else poly_ge's) */
static int s_slot_now = -1;

static int s_textured = -1;
static void flats_out(void);

static void list_open(void)
{
    sceGuStart(GU_DIRECT, (void *)((uintptr_t)s_list | 0x40000000u));
    list_state(s_stride, s_w, s_h, s_off);
    s_model = 0;
    s_slot_now = -1;
    s_used = 1024;
    s_textured = -1;
#ifdef SRFX_NSHVC_HANDOFF
    s_zt = 0;
#endif
}

static void list_close(void)
{
    flats_out();
    sceGuFinish();
    sceGuSync(0, 0);
}

/* room for a primitive and its state: the flat spans waiting drawn first
   (the order kept); a list near full run and begun again */
static void room(void)
{
    flats_out();
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
/* The stages kept from fill to fill: the ROM's bytes never change, so
   its textures are made once and kept until the arena is full (all then
   made again: stages_reset, at a list's end, the GE done with them); the
   Super FX RAM's ($70, $71) may, so a picture's are made in it */

static unsigned stage_key(uint8_t bank, uint16_t base, uint16_t mask)
{
    return ((unsigned)bank * 2654435761u ^ (unsigned)base * 40503u ^ (unsigned)mask * 97u) % STAGE_HASH;
}

static void stages_reset(void)
{
    s_nstage = 0;
    s_arena_used = 0;
    s_ram_stages = 0;
    memset(s_hash, 0, sizeof s_hash);
    s_tex_now = NULL;
    s_stage_resets++;
}

/* at a picture's or a fill's start: the RAM's textures made again */
static void stages_begin(void)
{
    static uint32_t ver[4];
    int k, moved = 0;
    for (k = 0; k < 4; k++)             /* the panels' textures (fw_bank_version) */
        if (fw_bank_version((uint8_t)(0xF0 + k)) != ver[k]) {
            ver[k] = fw_bank_version((uint8_t)(0xF0 + k));
            moved = 1;
        }
    if (s_ram_stages || moved)
        stages_reset();
}

static const Stage *stage_make(uint8_t bank, uint16_t base, uint16_t mask, uint32_t pal, int bitmap);
static const Stage *stage_for(uint8_t bank, uint16_t base, uint16_t mask) { return stage_make(bank, base, mask, 0, 0); }

/* a 2-bit bitmap's texture (fullwindow.h FW_QUAD bit 2): w by h of its
   pixels from at, 32 bytes a row, each its palette byte (pal's bytes, low
   first) */
static const Stage *stage_bitmap(uint8_t bank, uint16_t at, int w, int h, uint32_t pal)
{
    int um = pow2_at_least(w) - 1, vm = pow2_at_least(h) - 1;
    if (um > 0x7F || vm > 0xFF)
        return NULL;
    return stage_make(bank, at, (uint16_t)(vm << 8 | um), pal, 1);
}

static const Stage *stage_make(uint8_t bank, uint16_t base, uint16_t mask, uint32_t pal, int bitmap)
{
    int i, u, v, um = mask & 0xFF, vm = mask >> 8, tw, th, stride;
    unsigned h = (stage_key(bank, base, mask) ^ (pal * 2246822519u + (unsigned)bitmap)) % STAGE_HASH;
    Stage *s;
    for (i = 0; i < STAGE_HASH && s_hash[h]; i++, h = (h + 1) % STAGE_HASH) {
        s = &s_stage[s_hash[h] - 1];
        if (s->bank == bank && s->base == base && s->mask == mask && s->pal == pal && s->bitmap == bitmap)
            return s;
    }
    if (!bitmap && (((um + 1) & um) || ((vm + 1) & vm)))
        um = vm = 0xFF;
    tw = um + 1;
    th = vm + 1;
    stride = tw < 16 ? 16 : tw;
    if (s_nstage == STAGES || s_arena_used + (size_t)stride * th > ARENA_BYTES) {
        list_close();                   /* the GE done with the arena: all made again */
        stages_reset();
        list_open();
        h = (stage_key(bank, base, mask) ^ (pal * 2246822519u + (unsigned)bitmap)) % STAGE_HASH;
    }
    s_hash[h] = (int16_t)(s_nstage + 1);
    s_stage_made++;
    s_ram_stages += bank == 0x70 || bank == 0x71;
    s = &s_stage[s_nstage++];
    s->bank = bank;
    s->base = base;
    s->mask = mask;
    s->pal = pal;
    s->bitmap = (uint8_t)bitmap;
    s->at = s_arena + s_arena_used;
    s->tw = tw;
    s->th = th;
    s->um = um;
    s->vm = vm;
    s_arena_used += ((size_t)stride * th + 63) & ~(size_t)63;
    for (v = 0; v < th; v++)
        for (u = 0; u < tw; u++)
            if (bitmap)                 /* a 2-bit bitmap's pixel: its palette byte */
                s->at[(size_t)v * stride + u] =
                    (uint8_t)(pal >> 8 * ((fw_texel(bank, (uint16_t)(base + v * 32 + (u >> 2))) >> 2 * (u & 3)) & 3));
            else
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
        if (s_c32)
            sceGuClutLoad(32, s_clut32[cls]);
        else
            sceGuClutLoad(32, s_clut[cls]);
        s_clut_now = cls;
    }
    if (mode != s_cmode_now) {
        sceGuClutMode(s_c32 ? GU_PSM_8888 : GU_PSM_4444, (unsigned)(mode & 0x1F), (unsigned)(mode >> 8 & 0xFF),
                      (unsigned)(mode >> 16 & 0x1F));
        s_cmode_now = mode;
    }
}

/* ---- the primitives ---- */

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

/* the flat spans in a row of them, one draw (flats_out: before any other
   primitive, and at a list's end) */
#define FLATS_MAX 1024
static CV32 s_fb32[2 * FLATS_MAX];
static CV s_fb16[2 * FLATS_MAX];
static int s_fbn;

static void flats_out(void)
{
    int n = s_fbn;
    void *m;
    if (!n)
        return;
    s_fbn = 0;
    textured(0);
    ztest(0);
    if (s_c32) {
        m = sceGuGetMemory((unsigned)(2 * n) * sizeof(CV32));
        memcpy(m, s_fb32, (size_t)(2 * n) * sizeof(CV32));
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * n, 0, m);
    } else {
        m = sceGuGetMemory((unsigned)(2 * n) * sizeof(CV));
        memcpy(m, s_fb16, (size_t)(2 * n) * sizeof(CV));
        sceGuDrawArray(GU_SPRITES, GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * n, 0, m);
    }
    s_used += (size_t)(2 * n) * sizeof(CV32) + 64;
}

static void flat(int32_t y, int32_t x0, int32_t x1, uint8_t c)
{
    if (s_break == 1) {
        s_break = 2;
        return;
    }
    if (s_fbn == FLATS_MAX || s_used + (size_t)(2 * s_fbn + 2) * sizeof(CV32) + 256 > LIST_ROOM) {
        flats_out();
        if (s_used + 256 > LIST_ROOM) {
            list_close();
            list_open();
        }
    }
    if (s_c32) {
        CV32 *w = &s_fb32[2 * s_fbn];
        w[0].c = w[1].c = 0xFF000000u | c;
        w[0].x = (int16_t)x0;
        w[0].y = (int16_t)y;
        w[1].x = (int16_t)(x1 + 1);
        w[1].y = (int16_t)(y + 1);
        w[0].z = w[1].z = w[0].pad = w[1].pad = 0;
    } else {
        CV *v = &s_fb16[2 * s_fbn];
        v[0].c = v[1].c = (uint16_t)(0xF000u | c);
        v[0].x = (int16_t)x0;
        v[0].y = (int16_t)y;
        v[1].x = (int16_t)(x1 + 1);
        v[1].y = (int16_t)(y + 1);
        v[0].z = v[1].z = 0;
    }
    s_fbn++;
}

/* n pixels from x on row y: texels (u, v) on, u stepping one a pixel
   (step 1: a 1:1 sprite) or the one texel (step 0: at its centre) */
static void run(int32_t y, int32_t x, int32_t n, int u, int v, int step)
{
    TV *t;
    room();
    textured(1);
    ztest(0);
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
                float k = (float)(int32_t)(x - p->xl), q = fw_fmaf(p->pdq, k, p->pq);
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
typedef struct {
    uint32_t c;
    float x, y, z;
} PCV32;

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
        o[k].x = (float)(int32_t)(i < n ? f->from : f->to) + dx;   /* (within 2^30: a 32-bit conversion, not a library's 64-bit one) */
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
    ztest(0);
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
    } else if (s_c32) {
        PCV32 *c;
        textured(0);
        c = (PCV32 *)sceGuGetMemory((unsigned)n * sizeof(PCV32));
        for (i = 0; i < n; i++) {
            c[i].c = 0xFF000000u | p->colour;
            c[i].x = 2.0f * a[i].x / (float)w - 1.0f;
            c[i].y = 1.0f - 2.0f * a[i].y / (float)h;
            c[i].z = 1.0f;
        }
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, c);
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

/* ---- the display list's faces (scene.h) ----
   The GE turns and projects them itself: each slot's matrix as GU_MODEL
   (the chip's integer steps as floats: the packed matrix's high bytes /
   128, the full one's words / 32768, times the point scale; its place the
   translation), the full window's focal lengths and centre as
   GU_PROJECTION with the near plane at z = 1 (GU_CLIP_PLANES); each face
   a fan of its corners as the ROM holds them (a mirrored point's x
   negated), textured through its stage and COLOR and PLOT's lookup, flat
   in its colour; a line its two ends. A far point's depth is not held at
   $43FE here (the GE's floats). */
static long s_faces;
#define PATCH_DIV 16                    /* face_patches: 2 x 16 x 16 triangles a triangle */

/* The view's projection, w = z. The GE's depth comes out 65536 k / z - 1 (z' = z -
   2 k over w = z; the range 65535, 0 maps ndc -1 to 65535, 1 to -1): at k 1 the
   pixel's 1 / z in 16 bits, what the water's plane is written in. k the factor to
   the true 1 / depth: 1, the inverse of a far object's halvings, a sprite's own. In
   front of z = k. */
static float s_zk = 1.0f;

static void model_projection(void)
{
    ScePspFMatrix4 pr;
    float cx, cy, w = (float)s_w, h = (float)s_h;
    fw_view_centre(&cx, &cy);
    memset(&pr, 0, sizeof pr);
    pr.x.x = 2.0f * (float)fw_focal_x() / w;
    pr.y.y = -2.0f * (float)fw_focal_y() / h;
    pr.z.x = 2.0f * cx / w - 1.0f;
    pr.z.y = 1.0f - 2.0f * cy / h;
    pr.z.z = 1.0f;
    pr.z.w = 1.0f;                      /* w = z */
    pr.w.z = -2.0f * s_zk;              /* z - 2 >= -z: in front of z = 1 */
    sceGuSetMatrix(GU_PROJECTION, &pr);
}

#ifdef SRFX_NSHVC_HANDOFF
static void model_depth(float k)
{
    if (k == s_zk)
        return;
    s_zk = k;
    if (s_model)
        model_projection();
}
#endif

static void model_mode(int on)
{
    static ScePspFMatrix4 pass = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } };
    static ScePspFMatrix4 ident = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
    if (on == s_model)
        return;
    if (on) {
        model_projection();
        sceGuEnable(GU_CLIP_PLANES);
        sceGuPatchDivide(PATCH_DIV, PATCH_DIV);
        sceGuPatchPrim(GU_TRIANGLE_STRIP);
        s_slot_now = -1;
    } else {
        sceGuSetMatrix(GU_PROJECTION, &pass);
        sceGuSetMatrix(GU_MODEL, &ident);
        sceGuDisable(GU_CLIP_PLANES);
    }
    s_model = on;
}

static void slot_matrix(const ScSlot *sl)
{
    ScePspFMatrix4 m;
    float k = (float)(int8_t)sl->scale;
    float c[9];
    int i;
    for (i = 0; i < 9; i++)
        c[i] = sl->shift < 3 ? (float)(int8_t)(sl->m[i] >> 8) * k / 128.0f : (float)sl->m[i] * k / 32768.0f;
    m.x.x = c[0], m.x.y = c[1], m.x.z = c[2], m.x.w = 0;
    m.y.x = c[3], m.y.y = c[4], m.y.z = c[5], m.y.w = 0;
    m.z.x = c[6], m.z.y = c[7], m.z.z = c[8], m.z.w = 0;
    m.w.x = (float)sl->pos[0], m.w.y = (float)sl->pos[1], m.w.z = (float)sl->pos[2], m.w.w = 1;
    sceGuSetMatrix(GU_MODEL, &m);
}

/* a solid's float slot as GU_MODEL (scene.h ScFSlot) */
static void fslot_matrix(const ScFSlot *f)
{
    ScePspFMatrix4 m;
    m.x.x = f->m[0], m.x.y = f->m[1], m.x.z = f->m[2], m.x.w = 0;
    m.y.x = f->m[3], m.y.y = f->m[4], m.y.z = f->m[5], m.y.w = 0;
    m.z.x = f->m[6], m.z.y = f->m[7], m.z.z = f->m[8], m.z.w = 0;
    m.w.x = f->pos[0], m.w.y = f->pos[1], m.w.z = f->pos[2], m.w.w = 1;
    sceGuSetMatrix(GU_MODEL, &m);
}

/* a corner's point in its slot's own coordinates (a model's bytes, a
   mirrored one's x negated; a solid's floats) */
static void corner_xyz(const ScPoint *pt, const ScFPoint *fp, uint32_t point, float *x, float *y, float *z)
{
    if (point & SC_FLOAT_POINT) {
        const ScFPoint *q = &fp[point & ~SC_FLOAT_POINT];
        *x = q->p[0], *y = q->p[1], *z = q->p[2];
        return;
    }
    *x = pt[point].mirrored ? -(float)pt[point].b[0] : (float)pt[point].b[0];
    *y = (float)pt[point].b[1];
    *z = (float)pt[point].b[2];
}

/* A face of an object near the eye or far off the sides (scene.h sc_near):
   with its clip enabled the GE clips at the near plane, but a primitive
   with a corner past 0 to 4095 after the viewport is dropped whole (about
   2048 pixels from the centre), so such primitives must be divided first.
   The GE divides them: each triangle of the fan a flat patch of 4 x 4
   control points, bilinear (one edge the third corner, twice over), which
   it cuts into PATCH_DIV x PATCH_DIV before its transform; a piece with a
   corner past the range is dropped, which only shows very near the eye. No
   point is turned by the CPU. */
typedef struct {
    float u, v, x, y, z;
} PatchCorner;

static void patch_point(const PatchCorner *a, const PatchCorner *b, const PatchCorner *c, int i, int j, PatchCorner *o)
{
    float s = (float)i / 3.0f, t = (float)j / 3.0f, wa = (1.0f - t) * (1.0f - s), wb = (1.0f - t) * s;
    o->u = wa * a->u + wb * b->u + t * c->u;
    o->v = wa * a->v + wb * b->v + t * c->v;
    o->x = wa * a->x + wb * b->x + t * c->x;
    o->y = wa * a->y + wb * b->y + t * c->y;
    o->z = wa * a->z + wb * b->z + t * c->z;
}

/* the face's fan as patches (its matrix and texture set) */
static void face_patches(const PatchCorner *pc, uint32_t n, int tex, uint8_t colour)
{
    uint32_t k;
    int i, j;
    for (k = 1; k + 1 < n; k++) {
        PatchCorner o;
        s_used += 16 * sizeof(PTV) + 256;
        room();
        if (tex) {
            PTV *t = (PTV *)sceGuGetMemory(16 * sizeof(PTV));
            for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++) {
                    patch_point(&pc[0], &pc[k], &pc[k + 1], i, j, &o);
                    t[4 * j + i].u = o.u;
                    t[4 * j + i].v = o.v;
                    t[4 * j + i].x = o.x;
                    t[4 * j + i].y = o.y;
                    t[4 * j + i].z = o.z;
                }
            sceGuDrawBezier(GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 4, 0, t);
        } else if (s_c32) {
            PCV32 *v = (PCV32 *)sceGuGetMemory(16 * sizeof(PCV32));
            for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++) {
                    patch_point(&pc[0], &pc[k], &pc[k + 1], i, j, &o);
                    v[4 * j + i].c = 0xFF000000u | colour;
                    v[4 * j + i].x = o.x;
                    v[4 * j + i].y = o.y;
                    v[4 * j + i].z = o.z;
                }
            sceGuDrawBezier(GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 4, 0, v);
        } else {
            PCV *v = (PCV *)sceGuGetMemory(16 * sizeof(PCV));
            for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++) {
                    patch_point(&pc[0], &pc[k], &pc[k + 1], i, j, &o);
                    v[4 * j + i].c = (uint16_t)(0xF000u | colour);
                    v[4 * j + i].pad = 0;
                    v[4 * j + i].x = o.x;
                    v[4 * j + i].y = o.y;
                    v[4 * j + i].z = o.z;
                }
            sceGuDrawBezier(GU_COLOR_4444 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 4, 0, v);
        }
    }
}

static void face_ge(const FwSpan *p)
{
    size_t ns, np, nf, nc, nfs, nfp;
    const ScSlot *sl = sc_list_slots(&ns);
    const ScPoint *pt = sc_list_points(&np);
    const ScFSlot *fs = sc_list_fslots(&nfs);
    const ScFPoint *fp = sc_list_fpoints(&nfp);
    const ScFace *fc = sc_list_faces(&nf);
    const ScCorner *cn = sc_list_corners(&nc);
    const ScFace *f;
    const ScCorner *c;
    const Stage *st = NULL;
    uint32_t n, i, slot;
    float tws = 1, ths = 1, ou = 0, ov = 0, u0 = 0, v0 = 0;

    if ((size_t)p->x1 >= nf)
        return;
    f = &fc[p->x1];
    c = &cn[f->first];
    n = f->n < 8 ? f->n : 8;
    if (n < 2 || (!f->textured && !f->colour))
        return;
    if (f->textured && !(st = stage_for(f->bank, f->base, f->mask)))
        return;
    room();
    s_used += (size_t)(n + 8) * sizeof(PTV) + 256;
    model_mode(1);
    /* the slot's matrix (a solid's float slots numbered after the models') */
    slot = c[0].point & SC_FLOAT_POINT ? (uint32_t)ns + fp[c[0].point & ~SC_FLOAT_POINT].fslot : pt[c[0].point].slot;
    if ((int)slot != s_slot_now) {
        if (slot >= ns)
            fslot_matrix(&fs[slot - ns]);
        else
            slot_matrix(&sl[slot]);
        s_slot_now = (int)slot;
#ifdef SRFX_NSHVC_HANDOFF
        if (s_zcut) {                   /* the true depth: a far object's halvings undone */
            float zs = slot >= ns ? fs[slot - ns].zs : (float)(1u << sl[slot].pad[1]);
            model_depth(zs > 0 ? 1.0f / zs : 1.0f);
        }
#endif
    }
    ztest(s_zcut && n > 2);             /* (a line has no depth there: not cut, the reference's) */
    if (n > 2 && (slot >= ns ? sc_fslot_near(slot - ns) : sl[slot].pad[0])) {
        PatchCorner pc[8];
        if (f->textured) {
            textured(1);
            use_texture(st);
            use_rules(f->por, f->hi);
            if (!s_wrap) {
                sceGuTexWrap(GU_REPEAT, GU_REPEAT);
                s_wrap = 1;
            }
            tws = (float)pow2_at_least(st->tw);
            ths = (float)pow2_at_least(st->th);
            ou = (float)f->ou / 256.0f;
            ov = (float)f->ov / 256.0f;
            u0 = c[0].u;
            v0 = c[0].v;
        } else
            textured(0);
        for (i = 0; i < n; i++) {
            float du = c[i].u - u0, dv = c[i].v - v0;
            du += -256.0f * (du > 128.0f) + 256.0f * (du < -128.0f);
            dv += -256.0f * (dv > 128.0f) + 256.0f * (dv < -128.0f);
            pc[i].u = f->textured ? (u0 + du + ou) / tws : 0;
            pc[i].v = f->textured ? (v0 + dv + ov) / ths : 0;
            corner_xyz(pt, fp, c[i].point, &pc[i].x, &pc[i].y, &pc[i].z);
        }
        face_patches(pc, n, f->textured, f->colour);
        s_faces++;
        return;
    }
    if (f->textured) {
        PTV *t = (PTV *)sceGuGetMemory(n * sizeof(PTV));
        textured(1);
        use_texture(st);
        use_rules(f->por, f->hi);
        if (!s_wrap) {
            sceGuTexWrap(GU_REPEAT, GU_REPEAT);
            s_wrap = 1;
        }
        tws = (float)pow2_at_least(st->tw);
        ths = (float)pow2_at_least(st->th);
        ou = (float)f->ou / 256.0f;
        ov = (float)f->ov / 256.0f;
        u0 = c[0].u;
        v0 = c[0].v;
        for (i = 0; i < n; i++) {
            float du = c[i].u - u0, dv = c[i].v - v0;
            du += -256.0f * (du > 128.0f) + 256.0f * (du < -128.0f);
            dv += -256.0f * (dv > 128.0f) + 256.0f * (dv < -128.0f);
            t[i].u = (u0 + du + ou) / tws;
            t[i].v = (v0 + dv + ov) / ths;
            corner_xyz(pt, fp, c[i].point, &t[i].x, &t[i].y, &t[i].z);
        }
        sceGuDrawArray(n == 2 ? GU_LINES : GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n, 0, t);
    } else if (s_c32) {
        PCV32 *v = (PCV32 *)sceGuGetMemory(n * sizeof(PCV32));
        textured(0);
        for (i = 0; i < n; i++) {
            v[i].c = 0xFF000000u | f->colour;
            corner_xyz(pt, fp, c[i].point, &v[i].x, &v[i].y, &v[i].z);
        }
        sceGuDrawArray(n == 2 ? GU_LINES : GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n, 0, v);
    } else {
        PCV *v = (PCV *)sceGuGetMemory(n * sizeof(PCV));
        textured(0);
        for (i = 0; i < n; i++) {
            v[i].c = (uint16_t)(0xF000u | f->colour);
            v[i].pad = 0;
            corner_xyz(pt, fp, c[i].point, &v[i].x, &v[i].y, &v[i].z);
        }
        sceGuDrawArray(n == 2 ? GU_LINES : GU_TRIANGLE_FAN, GU_COLOR_4444 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n, 0, v);
    }
    s_faces++;
}

/* ---- the screen quads (fullwindow.h FW_QUAD) ----
   A quad the GE's raster over its rectangle of pixels: its texture a stage
   of its bank from its base, 256 across and as many rows as its rule
   reaches (the texel at base + row * 256 + column, as the rule reads it,
   repeating every 256 across); u and v at its corners from its rule at
   their pixels' centres (affine across and down, the rows' divide taken
   as its line: the GE's interpolation, which the check measures). The
   water's cut is the half-plane the reference's test leaves (the quad's
   1 / depth against the water's plane, both planes in x and y): the
   rectangle clipped by its line. */
/* the polygon in (x, y) kept where a x + b y + c <= 0 (pixel coordinates) */
static int half_clip(const PV *in, int n, PV *out, float a, float b, float c)
{
    int i, m = 0;
    for (i = 0; i < n; i++) {
        const PV *p = &in[i], *q = &in[(i + 1) % n];
        float fp = a * p->x + b * p->y + c, fq = a * q->x + b * q->y + c;
        if (fp <= 0)
            out[m++] = *p;
        if ((fp <= 0) != (fq <= 0)) {
            float t = fp / (fp - fq);
            out[m].x = p->x + (q->x - p->x) * t;
            out[m].y = p->y + (q->y - p->y) * t;
            out[m].uq = out[m].vq = out[m].q = 0;
            m++;
        }
    }
    return m;
}

/* The bias that keeps the GE's interpolated coordinate on the texel the
   rule gives: the rule's value at each pixel centre a / den (den 2^32, or
   the rows' divide), its texel floor(a / den); half the least distance
   from any such value up to its next texel, so a value the GE lands just
   below a whole texel (its interpolation's rounding) is lifted onto it and
   none is lifted past the next. Over the quad's columns (or rows), not
   its pixels. */
static double quad_bias(int64_t a, int64_t d, int32_t n, int64_t den)
{
    int64_t least = den;
    int32_t k;
    for (k = 0; k < n; k++, a += d) {
        int64_t r = a % den;
        if (r < 0)
            r += den;
        if (r && den - r < least)
            least = den - r;
    }
    return (double)least / (2.0 * (double)den);
}

#ifdef SRFX_NSHVC_HANDOFF
/* z over w = 1 for the plane a + b x + c y of 1 / depth (x, y pixel numbers, here
   screen points, a pixel's centre at number + 0.5): the GE's depth 65536 q - 1, as
   model_projection's. q held to 0 and 1, past which the test has its answer. */
static float plane_z(float a, float b, float c, float X, float Y)
{
    float q = a + b * (X - 0.5f) + c * (Y - 0.5f);
    q = q < 0.0f ? 0.0f : q > 1.0f ? 1.0f : q;
    return 1.0f - 2.0f * q;
}
#endif

/* a line quad (the weather's streaks): the GE's line from its first
   pixel's centre to its last's */
static void line_ge(const FwSpan *p, int w, int h)
{
    int ymaj = p->mask & 1, sx = p->mask & 2 ? -1 : 1, sy = p->mask & 4 ? -1 : 1;
    int64_t n = p->du * p->dv - p->u, st = n <= 0 ? 0 : (n + p->du - 1) / p->du;
    float x0 = (float)p->xl + 0.5f, y0 = (float)p->v + 0.5f;
    float x1 = (float)(p->xl + (ymaj ? sx * st : sx * p->du)) + 0.5f, y1 = (float)(p->v + (ymaj ? sy * p->du : sy * st)) + 0.5f;
    room();
    s_used += 2 * sizeof(PCV32) + 256;
    model_mode(0);
    textured(0);
    ztest(0);
    if (s_c32) {
        PCV32 *c = (PCV32 *)sceGuGetMemory(2 * sizeof(PCV32));
        c[0].c = c[1].c = 0xFF000000u | p->colour;
        c[0].x = 2.0f * x0 / (float)w - 1.0f;
        c[0].y = 1.0f - 2.0f * y0 / (float)h;
        c[1].x = 2.0f * x1 / (float)w - 1.0f;
        c[1].y = 1.0f - 2.0f * y1 / (float)h;
        c[0].z = c[1].z = 1.0f;
        sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, 0, c);
    } else {
        PCV *c = (PCV *)sceGuGetMemory(2 * sizeof(PCV));
        c[0].c = c[1].c = (uint16_t)(0xF000u | p->colour);
        c[0].pad = c[1].pad = 0;
        c[0].x = 2.0f * x0 / (float)w - 1.0f;
        c[0].y = 1.0f - 2.0f * y0 / (float)h;
        c[1].x = 2.0f * x1 / (float)w - 1.0f;
        c[1].y = 1.0f - 2.0f * y1 / (float)h;
        c[0].z = c[1].z = 1.0f;
        sceGuDrawArray(GU_LINES, GU_COLOR_4444 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 2, 0, c);
    }
    s_quads++;
}

static void quad_ge(const FwSpan *p, int w, int h)
{
    PV a[8], b[8];
    int n = 4, i, vm = 0xFF;
    const Stage *s = NULL;
    double k32 = 1.0 / 4294967296.0, dn = p->mask ? 1.0 / (double)p->mask : k32;
    uint32_t cden = p->pad1[1] & 8 ? fw_quad_word(&p->pq) : 0, mx = fw_quad_word(&p->pdq), my = fw_quad_word(&p->pda);
    double dc = cden ? 1.0 / (double)cden : k32;
    int64_t tl = p->v + p->dv * (p->pad0 - 1);
    float X0 = (float)p->x0, X1 = (float)(p->x1 + 1), Y0 = (float)p->y, Y1 = (float)(p->y + p->pad0);

    memset(a, 0, sizeof a);
    a[0].x = a[3].x = X0;
    a[1].x = a[2].x = X1;
    a[0].y = a[1].y = Y0;
    a[2].y = a[3].y = Y1;
    int cut = s_zcut && (p->za != 0 || p->zx != 0 || p->zy != 0);
    if (!cut && fw_water[3] != 0 && (p->za != 0 || p->zx != 0 || p->zy != 0)) {
        /* kept where the water's 1 / depth is not above the quad's at the
           pixel's centre (x - 0.5, y - 0.5 in pixel numbers) */
        float ca = fw_water[1] - p->zx, cb = fw_water[2] - p->zy, cc = fw_water[0] - p->za;
        n = half_clip(a, 4, b, ca, cb, cc - 0.5f * ca - 0.5f * cb);
        memcpy(a, b, sizeof a);
        s_quads_cut++;
        if (n < 3)
            return;
    }
    if (p->pad1[1] & 4) {               /* a 2-bit bitmap: its pixels the rule reaches */
        int64_t sl = p->u + p->du * (p->x1 - p->xl), tb;
        uint32_t pal;
        sl = cden ? sl / cden : sl >> 32;
        tb = p->mask ? tl / p->mask : tl >> 32;
        if (mx)
            sl = (sl * mx) >> 8;
        if (my)
            tb = (tb * my) >> 8;
        memcpy(&pal, p->pad1 + 3, 4);
        if (sl < 0 || tb < 0 || !(s = stage_bitmap(p->bank, p->base, (int)sl + 1, (int)tb + 1, pal)))
            return;
    } else if (!(p->pad1[1] & 2)) {
        tl = p->mask ? tl / p->mask : tl >> 32;
        if (tl >= 0 && tl < 256)
            vm = pow2_at_least((int)tl + 1) - 1;
        if (!(s = stage_for(p->bank, p->base, (uint16_t)(vm << 8 | 0xFF))))
            return;
    }
    room();
    s_used += (size_t)n * sizeof(PTV) + 256;
    model_mode(0);
    ztest(cut);
#ifdef SRFX_NSHVC_HANDOFF
    if (cut) {                          /* its 1 / depth as z, over w = 1: plane_write's scale */
        static ScePspFMatrix4 ident = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
        sceGuSetMatrix(GU_PROJECTION, &ident);
    }
#define QUAD_Z(i) (cut ? plane_z(p->za, p->zx, p->zy, a[i].x, a[i].y) : 1.0f)
#else
#define QUAD_Z(i) 1.0f
#endif
    if (s) {
        PTV *t = (PTV *)sceGuGetMemory((unsigned)n * sizeof(PTV));
        float tws = (float)pow2_at_least(s->tw), ths = (float)pow2_at_least(s->th);
        int64_t one = (int64_t)1 << 32;
        double bu = mx ? 0 : quad_bias(p->u + p->du * (p->x0 - p->xl), p->du, p->x1 - p->x0 + 1, cden ? (int64_t)cden : one);
        double bv = my ? 0 : quad_bias(p->v, p->dv, p->pad0, p->mask ? (int64_t)p->mask : one);
        textured(1);
        use_texture(s);
        use_rules(p->por, p->hi);
        if (!s_wrap) {
            sceGuTexWrap(GU_REPEAT, GU_REPEAT);
            s_wrap = 1;
        }
        for (i = 0; i < n; i++) {
            double cu = ((double)p->u + (double)p->du * ((double)a[i].x - 0.5 - (double)p->xl)) * dc;
            double cv = ((double)p->v + (double)p->dv * ((double)a[i].y - 0.5 - (double)p->y)) * dn;
            if (mx)                     /* the 8.8 steps (the GE's line through them) */
                cu = cu * mx / 256.0;
            if (my)
                cv = cv * my / 256.0;
            t[i].u = (float)(cu + bu) / tws;
            t[i].v = (float)(cv + bv) / ths;
            t[i].x = 2.0f * a[i].x / (float)w - 1.0f;
            t[i].y = 1.0f - 2.0f * a[i].y / (float)h;
            t[i].z = QUAD_Z(i);
        }
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, t);
    } else if (s_c32) {
        PCV32 *c = (PCV32 *)sceGuGetMemory((unsigned)n * sizeof(PCV32));
        textured(0);
        for (i = 0; i < n; i++) {
            c[i].c = 0xFF000000u | p->colour;
            c[i].x = 2.0f * a[i].x / (float)w - 1.0f;
            c[i].y = 1.0f - 2.0f * a[i].y / (float)h;
            c[i].z = QUAD_Z(i);
        }
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, c);
    } else {
        PCV *c = (PCV *)sceGuGetMemory((unsigned)n * sizeof(PCV));
        textured(0);
        for (i = 0; i < n; i++) {
            c[i].c = (uint16_t)(0xF000u | p->colour);
            c[i].pad = 0;
            c[i].x = 2.0f * a[i].x / (float)w - 1.0f;
            c[i].y = 1.0f - 2.0f * a[i].y / (float)h;
            c[i].z = 1.0f;
        }
        sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_4444 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, c);
    }
#ifdef SRFX_NSHVC_HANDOFF
    if (cut) {                          /* model_mode(0)'s again */
        static ScePspFMatrix4 pass = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } };
        sceGuSetMatrix(GU_PROJECTION, &pass);
    }
#endif
    s_quads++;
}

/* ---- the objects' primitives (scene.h ScPrim): the GE's own transform ----
   Each in view space under the view's projection, no matrix of an object's:
   the GE projects it and its scissor keeps it to the screen. A sprite a
   rectangle at its one depth, its texture across it (mirrored: backwards);
   its rows capped as the chip's, a fixed size's in texel rows. A dot is its
   pixels' size at its depth, a line its two ends; a drop at the depth the
   chip's table gives it (the scale at 2 ($012C + z), doubled: $012C + z), a
   streak's other end its last pixel taken back to that depth. */
static void prim_model(void)
{
    static ScePspFMatrix4 ident = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
    model_mode(1);
    if (s_slot_now != -2) {
        sceGuSetMatrix(GU_MODEL, &ident);
        s_slot_now = -2;
    }
}

static void prim_flat(int prim, const float (*q)[3], int n, uint8_t colour)
{
    int i;
    s_used += (size_t)n * sizeof(PCV32) + 256;
    room();
    prim_model();
    textured(0);
    ztest(0);                           /* (dots and streaks: not cut, as prims.comp leaves them) */
    if (s_c32) {
        PCV32 *v = (PCV32 *)sceGuGetMemory((unsigned)n * sizeof(PCV32));
        for (i = 0; i < n; i++) {
            v[i].c = 0xFF000000u | colour;
            v[i].x = q[i][0];
            v[i].y = q[i][1];
            v[i].z = q[i][2];
        }
        sceGuDrawArray(prim, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, v);
    } else {
        PCV *v = (PCV *)sceGuGetMemory((unsigned)n * sizeof(PCV));
        for (i = 0; i < n; i++) {
            v[i].c = (uint16_t)(0xF000u | colour);
            v[i].pad = 0;
            v[i].x = q[i][0];
            v[i].y = q[i][1];
            v[i].z = q[i][2];
        }
        sceGuDrawArray(prim, GU_COLOR_4444 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, v);
    }
    s_quads++;
}

static void prim_dot(float x, float y, float z, int n, uint8_t colour)
{
    float wd = (float)n * z / (float)fw_focal_x(), ht = (float)n * z / (float)fw_focal_y();
    float q[4][3] = { { x, y, z }, { x + wd, y, z }, { x + wd, y + ht, z }, { x, y + ht, z } };
    prim_flat(GU_TRIANGLE_FAN, q, 4, colour);
}

static void prim_sprite(const ScPrim *p)
{
    const ScPrimView *pv = sc_list_prim_view();
    float z = (float)p->z, fy = (float)fw_focal_y(), hy, cap = (float)pv->rows_cap * z / (2.0f * fy), tws, ths;
    int c8 = (p->mask & 0xFF) + 1, back = (p->mask & 0x8000) != 0;
    const Stage *st;
    PTV *t;
    if (p->z <= 0x80)
        return;
    if (p->own >= 0) {
        int size = (2 * (int)p->aa + ((int)p->own << p->shift)) & 0xFFFF;
        if (!size)
            size = 1;
        if (p->far)
            size = (int16_t)size >> p->far;
        hy = (float)size / 2.0f;
    } else
        hy = (float)-(int16_t)p->rows * z / (2.0f * fy);
    if (hy <= 0)
        return;
    if (hy > cap)
        hy = cap;
    st = stage_for(p->bank, p->base, (uint16_t)((pow2_at_least(c8) - 1) << 8 | 0xFF));
    if (!st)
        return;
    s_used += 4 * sizeof(PTV) + 256;
    room();
    prim_model();
#ifdef SRFX_NSHVC_HANDOFF
    if (s_zcut && p->za > 0)            /* the sprite's one depth: its 1 / depth at z */
        model_depth(p->za * z);
    ztest(s_zcut && p->za > 0);
#endif
    textured(1);
    use_texture(st);
    use_rules(p->por, p->hi);
    if (!s_wrap) {
        sceGuTexWrap(GU_REPEAT, GU_REPEAT);
        s_wrap = 1;
    }
    tws = (float)pow2_at_least(st->tw);
    ths = (float)pow2_at_least(st->th);
    t = (PTV *)sceGuGetMemory(4 * sizeof(PTV));
    t[0].x = t[3].x = (float)p->x - hy;
    t[1].x = t[2].x = (float)p->x + hy;
    t[0].y = t[1].y = (float)p->y - hy;
    t[2].y = t[3].y = (float)p->y + hy;
    t[0].z = t[1].z = t[2].z = t[3].z = z;
    t[0].u = t[3].u = (back ? (float)c8 : 0) / tws;
    t[1].u = t[2].u = (back ? 0 : (float)c8) / tws;
    t[0].v = t[1].v = 0;
    t[2].v = t[3].v = (float)c8 / ths;
    sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, t);
    s_quads++;
}

static void prim_ge(const FwSpan *sp)
{
    size_t np;
    const ScPrim *ps = sc_list_prims(&np), *p;
    float cx, cy, fx = (float)fw_focal_x(), fy = (float)fw_focal_y();
    if ((size_t)sp->xl >= np)
        return;
    p = &ps[sp->xl];
    fw_view_centre(&cx, &cy);
    switch (p->kind) {
    case SC_PRIM_SPRITE:
        prim_sprite(p);
        break;
    case SC_PRIM_DOT:
        if (p->z >= 0x100)
            prim_dot((float)p->x, (float)p->y, (float)(p->z > 0x4400 ? 0x43FF : p->z), p->n, p->colour);
        break;
    case SC_PRIM_LINE:
        if (p->z >= 0x100 && p->z2 >= 0x100) {
            float q[2][3] = { { (float)p->x, (float)p->y, (float)p->z }, { (float)p->x2, (float)p->y2, (float)p->z2 } };
            prim_flat(GU_LINES, q, 2, p->colour);
        }
        break;
    case SC_PRIM_WDOT: {
        float z = (float)(0x012C + p->z);
        prim_dot((float)p->x, (float)p->y - 0x14 * z / fy, z, p->n, p->colour);
        break;
    }
    case SC_PRIM_WLINE: {
        float z = (float)(0x012C + p->z);
        float q[2][3] = { { (float)p->x, (float)p->y - 0x14 * z / fy, z },
                          { ((float)p->x2 - cx) * z / fx, ((float)p->y2 - cy) * z / fy, z } };
        prim_flat(GU_LINES, q, 2, p->colour);
        break;
    }
    }
}

/* one effect's haze (fullwindow.h FW_HAZE, fullwindow.c haze_fill's steps): the
   GE's work so far finished and the target read at each entry's source (the
   canvas's or the 4444 target's low byte), all before any is drawn, then each
   texel a 1-pixel sprite of what was there */
static volatile uint32_t *s_cv;
static volatile uint16_t *s_t16;
static uint8_t haze_read(int32_t x, int32_t y, int w, int h)
{
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 0;
    if (s_c32) {
        uint32_t v = s_cv[(size_t)y * s_stride + x];
        return (v >> 24) == 0xFF ? (uint8_t)v : 0;
    }
    return (uint8_t)s_t16[(size_t)y * s_stride + x];
}

static int32_t haze_at(int32_t v, int64_t n, int64_t d)
{
    int64_t p = v * n;
    return (int32_t)((p >= 0 ? p : p - d + 1) / d);
}

uint16_t fx_noise_step(uint16_t s);

static void haze_ge(const FwSpan *p, int w, int h)
{
    static struct { int16_t x, y; uint8_t c; } put[256 * 16];
    int32_t X = (int32_t)p->xl, Y = p->pad0, r, c, np = 0, i;
    uint16_t at = (uint16_t)(0xF92E + 4 * p->base);
    uint16_t shape = (uint16_t)(fw_texel(8, at) | fw_texel(8, (uint16_t)(at + 1)) << 8);
    uint16_t offs = (uint16_t)(fw_texel(8, (uint16_t)(at + 2)) | fw_texel(8, (uint16_t)(at + 3)) << 8);

    list_close();
    for (r = 0; r < 16; r++) {
        int32_t sy = Y + (int8_t)fw_texel(8, (uint16_t)(offs + r)), ty = (Y - 8 + r) & 255;
        for (c = 0; c < 16; c++) {
            int8_t b = (int8_t)fw_texel(8, (uint16_t)(shape + r * 16 + c));
            int32_t sx = X + b, tx = (X - 8 + c) & 255, sya = sy, tya = ty, u, v, c0, c1, r0, r1, s0, t0;
            if (!b)
                continue;
            if (p->pad1[2]) {
                sx &= 255;
                sya &= 255;
                if (sya >= h) {
                    sya -= h;
                    sx += 8;
                }
                if (tya >= h) {
                    tya -= h;
                    tx += 8;
                }
            }
            c0 = haze_at(tx, p->u, p->du);
            c1 = haze_at(tx + 1, p->u, p->du);
            r0 = haze_at(tya, p->v, p->dv);
            r1 = haze_at(tya + 1, p->v, p->dv);
            s0 = haze_at(sx, p->u, p->du);
            t0 = haze_at(sya, p->v, p->dv);
            for (v = 0; v < r1 - r0; v++)
                for (u = 0; u < c1 - c0 && np < 256 * 16; u++) {
                    put[np].x = (int16_t)(c0 + u);
                    put[np].y = (int16_t)(r0 + v);
                    put[np++].c = haze_read(s0 + u, t0 + v, w, h);
                }
        }
    }
    list_open();
    for (i = 0; i < np; i++)
        if (put[i].x >= 0 && put[i].x < w && put[i].y >= 0 && put[i].y < h)
            flat(put[i].y, put[i].x, put[i].x, put[i].c);
}

/* a job's noise (fullwindow.h FW_NOISE, fullwindow.c noise_fill's steps), each
   pixel a 1-pixel sprite in the list: the GE has no kernel to run the generator */
static void noise_ge(const FwSpan *p, int w, int h)
{
    uint16_t s = p->base;
    uint64_t ramp = (uint64_t)p->u;
    int32_t cols = (int32_t)p->xl, rows = (int32_t)p->v, x, y;

    model_mode(0);
    for (y = 0; y < rows; y++)
        for (x = 0; x < cols; x++) {
            uint8_t c;
            int32_t px = x & 255, py = y & 255;
            s = fx_noise_step(s);
            c = (uint8_t)(ramp >> ((s & 7) * 8));
            if (p->por & 8 ? !(c & 15) : !c)
                continue;
            if (py >= h) {
                py -= h;
                px += 8;
            }
            if (px < w && py < h)
                flat(py, px, px, c);
        }
}

static void span(const FwSpan *p, int w, int h)
{
    int32_t x, a;
    (void)h;
    if (gpu_bin_ref_only(p))            /* the reference's own (a check's) */
        return;
    if (p->textured == FW_HAZE) {
        haze_ge(p, w, h);
        return;
    }
    if (p->textured == FW_NOISE) {
        noise_ge(p, w, h);
        return;
    }
    if (p->textured == FW_QUAD) {       /* a screen quad: the GE's whole */
        if (p->pad1[1] & 64)
            prim_ge(p);
        else if (p->pad1[1] & 32)
            line_ge(p, w, h);
        else
            quad_ge(p, w, h);
        return;
    }
    if (gpu_bin_face(p)) {              /* a display-list face: the GE's whole */
        face_ge(p);
        return;
    }
    model_mode(0);
    if (p->textured == 3) {             /* a polygon: the GE's whole, else its rows, in its place */
        int32_t row;
        FwSpan r;
        if (!s_rows && poly_ge(&fw_polys[p->x1], w, h)) {
            s_polys_ge++;
            return;
        }
        s_polys_cpu++;
        if (!s_rows)
            FW_CPU_FALLBACK("a polygon's rows");
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
static GpuCheck s_check;
static void check_report_ge(void) { gpu_bin_check_report("GE", &s_check, s_faces); }

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
    s_t16 = t;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            t[(size_t)y * stride + x] = (uint16_t)(0xF000u | out[(size_t)y * w + x]);
    s_stride = stride;
    s_w = w;
    s_h = h;
    s_off = (unsigned)((uintptr_t)lent - (uintptr_t)sceGeEdramGetAddr());
    stages_begin();
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
        if (!s_check.fills)
            atexit(check_report_ge);
        gpu_bin_check(&s_check, out, s_ref, w, h);
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
        if (getenv("FW_GPUDUMP") && !strcmp(getenv("FW_GPUDUMP"), "q")) {
            /* (a check) the first fill with 5 quads or more: its texels and its quads */
            static int done;
            size_t nq = 0;
            for (i = 0; i < n; i++)
                nq += sp[i].textured == FW_QUAD && !gpu_bin_ref_only(&sp[i]);
            if (!done && nq >= 5) {
                FILE *f = fopen("host0:/gdump.bin", "wb");
                done = 1;
                if (f) {
                    int32_t wh[2] = { w, h };
                    fwrite(wh, sizeof wh, 1, f);
                    fwrite(out, 1, len, f);
                    fwrite(s_ref, 1, len, f);
                    fclose(f);
                }
                f = fopen("host0:/gquads.bin", "wb");
                if (f) {
                    fwrite(fw_water, sizeof fw_water, 1, f);
                    for (i = 0; i < n; i++)
                        if (sp[i].textured == FW_QUAD && !gpu_bin_ref_only(&sp[i]))
                            fwrite(&sp[i], sizeof sp[i], 1, f);
                    fclose(f);
                }
            }
        } else if (getenv("FW_GPUDUMP") && s_checked == atol(getenv("FW_GPUDUMP"))) {
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

/* ---- the picture's canvas (fw_canvas_gpu) ----
   The whole picture kept in the lent GPU memory (480 x 272 at 32 bits, a
   512 stride: the loan's 557,056 bytes) from its first span to its
   hand-off: the spans drawn into it as they come, the ground surface put
   in after where nothing is drawn, read out once. s_cv the canvas, uncached. */
#ifdef SRFX_NSHVC_HANDOFF
static int s_plane;                     /* the water's plane stands in the depth buffer: as s_plane_at */
static float s_plane_at[4];
/* the last picture handed, while nothing has drawn over it: the console keeps that
   buffer whole while it is handed again (contract 2.22) */
static void *s_hand_at;
static int s_hand_stride, s_hand_w, s_hand_h;
#endif

static int cv_open(int w, int h, const uint8_t *keep)
{
    size_t bytes = 0;
    void *lent = s_lend(&bytes);
    int stride = (w + 63) & ~63, x, y;
    report_once();
    if (!lent || (size_t)stride * (size_t)h * 4 > bytes || w > 512 || h > 512)
        return 0;
    s_cv_opens++;
    s_c32 = 1;
#ifdef SRFX_NSHVC_HANDOFF
    s_plane = 0;
    s_zcut = 0;
    s_zk = 1.0f;
    if (lent == s_hand_at)              /* drawn over: the last picture handed is gone */
        s_hand_at = NULL;
    if (s_zb && (w > s_zbw || h > 272))
        s_zb = NULL;                    /* (past the loan's: none in the game, the screen's own size) */
#endif
    s_stride = stride;
    s_w = w;
    s_h = h;
    s_off = (unsigned)((uintptr_t)lent - (uintptr_t)sceGeEdramGetAddr());
    s_cv = (volatile uint32_t *)((uintptr_t)lent | 0x40000000u);
    s_textured = -1;
    if (keep) {                         /* the last picture's, to draw over */
        s_cv_kept++;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                uint8_t c = keep[(size_t)y * w + x];
                s_cv[(size_t)y * stride + x] = c ? 0xFF000000u | c : 0;
            }
        return 1;
    }
    list_open();                        /* cleared: nothing drawn, alpha (the stencil) 0 */
    sceGuStencilFunc(GU_ALWAYS, 0, 0xFF);
    {
        CV32 *v = (CV32 *)sceGuGetMemory(2 * sizeof(CV32));
        textured(0);
        v[0].c = v[1].c = 0;
        v[0].x = v[0].y = 0;
        v[1].x = (int16_t)w;
        v[1].y = (int16_t)h;
        v[0].z = v[1].z = v[0].pad = v[1].pad = 0;
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
    }
    list_close();
    return 1;
}

#ifdef SRFX_NSHVC_HANDOFF
/* The water's plane into the depth buffer, before what it cuts: the buffer 0 (the
   water behind the eye: nothing is cut), then the screen's rectangle where its 1 /
   depth q is above 0 as that plane, where past 1 at 1. No colour is written. The
   list's own depth test then passes a pixel with its 1 / depth at or above the
   plane's: the reference's !(z < w), in 16 bits of 1 / z (a unit 1 / 65536 of it:
   16 of view depth at 1024, 256 at 4096). */
static void plane_fan(const PV *a, int n, int held)
{
    PCV32 *c;
    int i;
    if (n < 3)
        return;
    c = (PCV32 *)sceGuGetMemory((unsigned)n * sizeof(PCV32));
    for (i = 0; i < n; i++) {
        c[i].c = 0;
        c[i].x = 2.0f * a[i].x / (float)s_w - 1.0f;
        c[i].y = 1.0f - 2.0f * a[i].y / (float)s_h;
        c[i].z = held ? -1.0f : plane_z(fw_water[0], fw_water[1], fw_water[2], a[i].x, a[i].y);
    }
    sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, c);
}

static void plane_write(void)
{
    static ScePspFMatrix4 ident = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
    static ScePspFMatrix4 pass = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } };
    PV r[4], up[8], low[8], high[8];
    float wb = fw_water[1], wc = fw_water[2], wa = fw_water[0] - 0.5f * wb - 0.5f * wc;
    int nu, nl, nh;
    CV32 *v;

    flats_out();
    model_mode(0);
    textured(0);
    sceGuDisable(GU_STENCIL_TEST);
    sceGuPixelMask(0xFFFFFFFFu);        /* no colour, no alpha */
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_FALSE);
    sceGuDepthFunc(GU_ALWAYS);
    v = (CV32 *)sceGuGetMemory(2 * sizeof(CV32));   /* through the transform, z the depth itself */
    v[0].c = v[1].c = 0;
    v[0].x = v[0].y = 0;
    v[1].x = (int16_t)s_w;
    v[1].y = (int16_t)s_h;
    v[0].z = v[1].z = v[0].pad = v[1].pad = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
    memset(r, 0, sizeof r);
    r[0].x = r[3].x = 0;
    r[1].x = r[2].x = (float)s_w;
    r[0].y = r[1].y = 0;
    r[2].y = r[3].y = (float)s_h;
    nu = half_clip(r, 4, up, -wb, -wc, -wa);                /* q >= 0 */
    nl = half_clip(up, nu, low, wb, wc, wa - 1.0f);         /* of those, q <= 1 */
    nh = half_clip(up, nu, high, -wb, -wc, 1.0f - wa);      /* of those, q >= 1 */
    sceGuSetMatrix(GU_PROJECTION, &ident);
    plane_fan(low, nl, 0);
    plane_fan(high, nh, 1);
    sceGuSetMatrix(GU_PROJECTION, &pass);
    sceGuDisable(GU_DEPTH_TEST);
    s_zt = 0;
    sceGuDepthMask(GU_TRUE);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuPixelMask(0);
    sceGuEnable(GU_STENCIL_TEST);
    s_used += 4 * 8 * sizeof(PCV32) + 1024;
    memcpy(s_plane_at, fw_water, sizeof s_plane_at);
    s_plane = 1;
}
#endif

static void cv_spans(const FwSpan *sp, size_t n)
{
    size_t i;
    stages_begin();
    s_textured = -1;
    s_break = getenv("FW_GEBREAK") ? 1 : 0;
    s_rows = getenv("FW_GEROWS") != NULL;
    list_open();
#ifdef SRFX_NSHVC_HANDOFF
    /* each flush by the water as it stands then, the reference's fill's */
    s_zcut = s_zb && fw_water[3] != 0;
    if (s_zcut && (!s_plane || memcmp(s_plane_at, fw_water, sizeof s_plane_at)))
        plane_write();
#endif
    for (i = 0; i < n; i++) {
        x_kind[sp[i].textured > 3 ? 0 : sp[i].textured]++;
        span(&sp[i], s_w, s_h);
    }
    {
        SceInt64 t = sceKernelGetSystemTimeWide();
        list_close();
        x_kt[0] -= 0;
        s_cvt[3] += 0;
        x_sync += sceKernelGetSystemTimeWide() - t;
    }
}

static uint8_t cv_read(int x, int y)
{
    uint32_t v = s_cv[(size_t)y * s_stride + x];
    return (v >> 24) == 0xFF ? (uint8_t)v : 0;
}

/* the canvas's pixel as nshvc_ext_canvas_screen32 takes it */
static uint32_t cv_word(uint32_t v)
{
    uint32_t a = v >> 24, rgb = v & 0xFFFFFFu;
    if (a == 0xFF)
        return v & 0xFFu;
    if (a == 0x01)
        return rgb == 1 ? 1 : 0xFF000000u | rgb;
#ifdef SRFX_NSHVC_HANDOFF
    if (a == 0x02)                      /* (gflat's mark) */
        return 1;
#endif
    return 0;
}

static void cv_close(uint32_t *out32, uint8_t *out8)
{
    int x, y;
#ifdef SRFX_NSHVC_HANDOFF
    s_zcut = 0;
    if (!out32 && !out8) {              /* handed: none of it read back */
        s_c32 = 0;
        return;
    }
#endif
    for (y = 0; y < s_h; y++) {
        volatile const uint32_t *r = s_cv + (size_t)y * s_stride;
        for (x = 0; x < s_w; x++) {
            uint32_t v = r[x];
            size_t i = (size_t)y * s_w + x;
            if (out32)
                out32[i] = cv_word(v);
            out8[i] = (v >> 24) == 0xFF ? (uint8_t)v : 0;
        }
    }
    s_c32 = 0;
}

/* The ground surface on the GE. Row y's pixels whose centres' rays meet
   the surface are one run, the ray's 1 / depth there linear along the
   row (wy = A + B x), so its ends are a division; its pixels whose
   corners all meet it (each corner's linear the same way) drawn as one
   quad with the surface's perspective (water_run's), split where Aqua
   Tunnel's ripples change colour with depth (each split a division too).
   Its few pixels at the horizon, a corner past it, take the row's colour
   between the ripples; a row whose colour is the console's (1), the
   same. The pixels past the run: under the surface, the row's colour;
   above it nothing. Where the 3D is drawn the stencil keeps it. */
static void gflat(int32_t y, int32_t x0, int32_t x1, uint32_t c)
{
    CV32 *v;
    if (x1 < x0)
        return;
    room();
    textured(0);
    v = (CV32 *)sceGuGetMemory(2 * sizeof(CV32));
    v[0].c = v[1].c = c;
    v[0].x = (int16_t)x0;
    v[0].y = (int16_t)y;
    v[1].x = (int16_t)(x1 + 1);
    v[1].y = (int16_t)(y + 1);
    v[0].z = v[1].z = v[0].pad = v[1].pad = 0;
#ifdef SRFX_NSHVC_HANDOFF
    if (c == 1) {                       /* colour 1, an index, marked apart from a colour (hand_picture): alpha 2, where it is 0 */
        sceGuStencilFunc(GU_EQUAL, 2, 0xFD);
        sceGuStencilOp(GU_KEEP, GU_KEEP, GU_REPLACE);
        sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
        sceGuStencilFunc(GU_EQUAL, 0, 0xFF);
        sceGuStencilOp(GU_KEEP, GU_KEEP, GU_INCR);
        return;
    }
#endif
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}

static void grun(const FwWaterJob *j, int32_t y, int32_t xa, int32_t xb, uint32_t g, uint32_t r)
{
    WV *v;
    int i;
    room();
    textured(1);
    v = (WV *)sceGuGetMemory(4 * sizeof(WV));
    for (i = 0; i < 4; i++) {
        float X = (float)((i & 1) ? xb + 1 : xa), Y = (float)((i & 2) ? y + 1 : y), q, s, t, w;
        surface_at(j, X, Y, &q, &s, &t);
        w = 1.0f / q;
        v[i].s = s;
        v[i].t = t;
        v[i].c = g;
        v[i].x = (2.0f * X / (float)j->w - 1.0f) * w;
        v[i].y = (1.0f - 2.0f * Y / (float)j->h) * w;
        v[i].z = w;
    }
    sceGuTexEnvColor(r);
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
}

/* the strip row (Aqua Tunnel's ripple colour) at depth t: the first as deep */
static int32_t band_of(const FwWaterJob *j, float t)
{
    int32_t lo = 0, hi = 79;
    while (lo < hi) {
        int32_t m = (lo + hi) / 2;
        if (j->depth[m] >= t)
            hi = m;
        else
            lo = m + 1;
    }
    return lo;
}

/* x's with lin(x) = a + b x of the sign s (> 0 after scaling) in [lo, hi]:
   the range, as whole pixels, kept to where the test holds at its ends */
static int sign_range(float a, float b, float sgn, float eps, int32_t lo, int32_t hi, int32_t *from, int32_t *to)
{
    int32_t f = lo, t = hi;
    if (b * sgn != 0) {
        float x = -a / b;
        if (b * sgn > 0) {
            if (x >= (float)hi + 1)
                return 0;
            if (x > (float)lo)
                f = (int32_t)floorf(x);
        } else {
            if (x <= (float)lo - 1)
                return 0;
            if (x < (float)hi)
                t = (int32_t)ceilf(x);
        }
    } else if (!(a * sgn > eps)) {
        return 0;
    }
    while (f <= t && !((a + b * (float)f) * sgn > eps))
        f++;
    while (t >= f && !((a + b * (float)t) * sgn > eps))
        t--;
    *from = f;
    *to = t;
    return f <= t;
}

/* a ground colour as the target holds it: read back, 0xFFrrggbb's low bytes; handed
   (contract 2.22), the GE's own order, 0xttbbggrr; colour 1 left, an index */
#ifdef SRFX_NSHVC_HANDOFF
static uint32_t ground_word(uint32_t c) { return fw_ground_handed && c != 1 ? abgr(c) : c; }
#else
#define ground_word(c) (c)
#endif

static void cv_ground(const FwWaterJob *j, const uint32_t *gap)
{
    const float *e = j->e;
    int w = j->w, h = j->h, y;
    float sg = j->dy > 0 ? 1.0f : -1.0f;
    uint32_t *ref = NULL;
    uint8_t *in = NULL;
    int check = getenv("FW_GPUCHECK") != NULL;

    if (check && (ref = malloc((size_t)w * h * 4)) && (in = malloc((size_t)w * h))) {
        for (y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                in[(size_t)y * w + x] = cv_read(x, y);
        fw_water_fill_cpu(in, ref, j, gap);
    }
    list_open();
    sceGuEnable(GU_STENCIL_TEST);       /* where nothing is drawn (alpha, the stencil, 0); its alpha made 1 */
    sceGuStencilFunc(GU_EQUAL, 0, 0xFF);
    sceGuStencilOp(GU_KEEP, GU_KEEP, GU_INCR);
    sceGuEnable(GU_TEXTURE_2D);
    cells_make(j);
    sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.0f);
    sceGuTexFilter(GU_LINEAR_MIPMAP_LINEAR, GU_NEAREST);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuTexFunc(GU_TFX_BLEND, GU_TCC_RGB);
    sceGuDisable(GU_ALPHA_TEST);
    s_textured = 1;
    for (y = 0; y < h; y++) {
        uint32_t g = ground_word(gap[y]), r0 = j->land ? ground_word(gap[h + y]) : 0, none = j->under ? g : 0;
        float vy = ((float)y + 0.5f - j->oy) / j->fy, A = e[4] * (0.5f - j->ox) / j->fx + e[5] * vy + e[6],
              B = e[4] / j->fx;
        int32_t a, b, a2, b2, c0, c1, d0, d1;
        float yt = ((float)y - j->oy) / j->fy, yb = ((float)y + 1.0f - j->oy) / j->fy;
        float At = -e[4] * j->ox / j->fx + e[5] * yt + e[6], Ab = -e[4] * j->ox / j->fx + e[5] * yb + e[6];
        s_ground_rows++;
        if (!sign_range(A, B, sg, 0.0f, 0, w - 1, &a, &b)) {
            if (none)
                gflat(y, 0, w - 1, none);
            continue;
        }
        if (none) {
            gflat(y, 0, a - 1, none);
            gflat(y, b + 1, w - 1, none);
        }
        if (g == 1 || (j->land && r0 == 1)) {     /* the console's colour: its row's own */
            s_ground_flat_rows++;
            gflat(y, a, b, g);
            continue;
        }
        /* the quad's pixels: both corner rows meet the surface at x and x + 1 */
        a2 = a;
        b2 = b;
        if (!sign_range(At, B, sg, 1e-6f * fabsf(j->dy), a, b + 1, &c0, &c1) ||
            !sign_range(Ab, B, sg, 1e-6f * fabsf(j->dy), a, b + 1, &d0, &d1)) {
            gflat(y, a, b, g);
            continue;
        }
        a2 = c0 > d0 ? c0 : d0;
        b2 = (c1 < d1 ? c1 : d1) - 1;
        if (a2 > b2) {
            gflat(y, a, b, g);
            continue;
        }
        gflat(y, a, a2 - 1, g);
        gflat(y, b2 + 1, b, g);
        if (j->land) {
            grun(j, y, a2, b2, g, r0);
            continue;
        }
        while (a2 <= b2) {              /* Aqua Tunnel's: a run each strip colour */
            int32_t lo = band_of(j, j->dy / (A + B * (float)a2)), end = b2;
            int32_t lb = band_of(j, j->dy / (A + B * (float)b2));
            if (lb != lo) {             /* the last pixel at lo: where t crosses the next band's depth */
                int32_t l = a2, r = b2;
                while (r - l > 1) {
                    int32_t m = (l + r) / 2;
                    if (band_of(j, j->dy / (A + B * (float)m)) == lo)
                        l = m;
                    else
                        r = m;
                }
                end = l;
            }
            grun(j, y, a2, end, g, ground_word(j->ripple[lo]));
            a2 = end + 1;
        }
    }
    list_close();
    if (ref && in) {                    /* FW_GPUCHECK: the reference's texels against the GE's */
        for (y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                uint32_t got = cv_word(s_cv[(size_t)y * s_stride + x]), want = ref[(size_t)y * w + x];
                if (got != want) {
                    int far = (got >> 24) != (want >> 24) || got < 256 || want < 256;
                    for (int k = 0; !far && k < 24; k += 8) {
                        int d = (int)(got >> k & 255) - (int)(want >> k & 255);
                        far = d > 16 || d < -16;
                    }
                    s_ground_apart++;
                    s_ground_far += far;
                }
            }
        s_ground_checked++;
    }
    free(ref);
    free(in);
}

static int t_open(int w, int h, const uint8_t *keep) { SceInt64 t = sceKernelGetSystemTimeWide(); int r = cv_open(w, h, keep); s_cvt[0] += sceKernelGetSystemTimeWide() - t; return r; }
static void t_spans(const FwSpan *sp, size_t n) { SceInt64 t = sceKernelGetSystemTimeWide(); cv_spans(sp, n); s_cvt[1] += sceKernelGetSystemTimeWide() - t; }
static void t_ground(const FwWaterJob *j, const uint32_t *gap) { SceInt64 t = sceKernelGetSystemTimeWide(); cv_ground(j, gap); s_cvt[2] += sceKernelGetSystemTimeWide() - t; }
static void t_close(uint32_t *o32, uint8_t *o8) { SceInt64 t = sceKernelGetSystemTimeWide(); cv_close(o32, o8); s_cvt[3] += sceKernelGetSystemTimeWide() - t; }
static const FwCanvasGpu s_canvas_ops = { t_open, t_spans, cv_read, t_ground, t_close };

#ifdef SRFX_NSHVC_HANDOFF
/* ---- the hand-off (nshvc_handoff.h, contract 2.22) ----
   The canvas's own coding is alpha the stencil's: an index 0xFF, the ground's colour
   1, colour 1 2, nothing 0. The console takes the words as the GE drew them, the
   stencil not 0 for a colour, 0 for an index and clear: the GE zeroes the indices'
   and colour 1's in place, their colours untouched. */
static void alpha_zero(int ref)
{
    CV32 *v = (CV32 *)sceGuGetMemory(2 * sizeof(CV32));
    sceGuStencilFunc(GU_EQUAL, ref, 0xFF);
    sceGuStencilOp(GU_KEEP, GU_KEEP, GU_ZERO);
    v[0].c = v[1].c = 0;
    v[0].x = v[0].y = 0;
    v[1].x = (int16_t)s_w;
    v[1].y = (int16_t)s_h;
    v[0].z = v[1].z = v[0].pad = v[1].pad = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}

static int hand_ready(void)
{
    size_t bytes = 0;
    return s_lend == lend_vram && s_lend(&bytes) != NULL;
}

static int hand_picture(int w, int h, int ground)
{
    size_t bytes = 0;
    void *lent = s_lend(&bytes);
    if (!lent || w != s_w || h != s_h)
        return 0;
    s_c32 = 1;
    list_open();
    textured(0);
    model_mode(0);
    sceGuPixelMask(0x00FFFFFFu);        /* alpha alone */
    alpha_zero(0xFF);
    if (ground)
        alpha_zero(2);
    sceGuPixelMask(0);
    list_close();
    s_c32 = 0;
    nshvc_ext_canvas_screen_ge(0, lent, s_stride, w, h, GU_PSM_8888, 1);
    s_hand_at = lent;
    s_hand_stride = s_stride;
    s_hand_w = w;
    s_hand_h = h;
    return 1;
}

/* a frame with no new picture: the same address, none drawn since */
static void hand_again(int w, int h)
{
    if (s_hand_at && w == s_hand_w && h == s_hand_h)
        nshvc_ext_canvas_screen_ge(0, s_hand_at, s_hand_stride, w, h, GU_PSM_8888, 1);
}

static const FwHandoff s_hand_ops = { hand_ready, NULL, hand_picture, hand_again, NULL, NULL };
#endif

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
    for (c = 0; c < 3; c++)
        for (i = 0; i < 256; i++) {
            int clear = (c == 1 && i == 0) || (c == 2 && (i & 0x0F) == 0);
            s_clut32[c][i] = clear ? 0 : 0xFF000000u | (uint32_t)i;
        }
    sceKernelDcacheWritebackRange(s_clut32, sizeof s_clut32);
    if (!getenv("FW_GECANVAS_OFF"))
        fw_canvas_gpu = &s_canvas_ops;
#ifdef SRFX_NSHVC_HANDOFF
    if (fw_canvas_gpu)
        fw_handoff = &s_hand_ops;
#endif
    fw_span_filler = fill;
    fw_water_filler = water_fill;
    return 1;
}
