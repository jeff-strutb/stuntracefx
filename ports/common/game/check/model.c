/*
 * model.c: see model.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cpu old_cpu
#define gsu old_gsu
#define rd8 old_rd8
#define wr8 old_wr8
#define cyc old_cyc
#define cpu_idle old_cpu_idle
#define gsu_rd8 old_gsu_rd8
#define gsu_wr8 old_gsu_wr8
#define gsu_plot old_gsu_plot
#define gsu_rpix old_gsu_rpix
#define gsu_code old_gsu_code
#define gsu_cache old_gsu_cache
#define gsu_cyc old_gsu_cyc
#define gsu_time old_gsu_time
#define gsu_buffer old_gsu_buffer
#include "machine.h"
#include "gsu.h"
#undef cpu
#undef gsu

#include "state.h"
#include "model.h"
#include "gsu_clock.h"
#include "ppu.h"

const uint8_t *srfx_rom_image;
const char *model_current = "";
Regs65816 old_cpu;
RegsGSU old_gsu;
int model_io_n;
uint32_t model_io[4096];
static uint8_t m7_latch, m7b, wrmpya, gsu_pbr;
void (*model_gsu_start)(uint8_t bank, uint16_t start);
static uint16_t m7a, wrdiv, rddiv, rdmpy;

void model_reset(void)
{
    model_io_n = 0;
}

void model_load_rom(const char *path)
{
    static uint8_t rom[0x100000];
    FILE *f = fopen(path, "rb");

    if (!f || fread(rom, 1, sizeof rom, f) != sizeof rom) {
        fprintf(stderr, "check: cannot read the ROM %s\n", path);
        exit(2);
    }
    fclose(f);
    srfx_rom_image = rom;
}

static void die(const char *what, uint32_t a)
{
    fprintf(stderr, "check: %s: the model has no %s at $%06X\n", model_current, what, a);
    exit(2);
}

uint8_t old_rd8(uint32_t a)
{
    uint8_t bank = (uint8_t)(a >> 16);
    uint16_t o = (uint16_t)a;
    int32_t p;

    if (bank == 0x7E || bank == 0x7F)
        return wram[a - 0x7E0000];
    if (bank == 0x70)
        return pakram[o];
    if ((bank & 0x7F) >= 0x40)
        die("read", a);
    if (o < 0x2000)
        return wram[o];
    if (o >= 0x8000)
        return srfx_rom_image[(bank & 0x3F) * 0x8000 + (o & 0x7FFF)];
    if (o >= 0x6000)
        return pakram[o - 0x6000];
    p = (int32_t)(int16_t)m7a * (int8_t)m7b;
    switch (o) {
    case 0x2134: return (uint8_t)p;
    case 0x2135: return (uint8_t)(p >> 8);
    case 0x2136: return (uint8_t)(p >> 16);
    case 0x4214: return (uint8_t)rddiv;
    case 0x4215: return (uint8_t)(rddiv >> 8);
    case 0x3030: case 0x3031: return 0;    /* the Super FX's status: stopped */
    case 0x4216: return (uint8_t)rdmpy;
    case 0x4217: return (uint8_t)(rdmpy >> 8);
    }
    die("read", a);
    return 0;
}

void old_wr8(uint32_t a, uint8_t v)
{
    uint8_t bank = (uint8_t)(a >> 16);
    uint16_t o = (uint16_t)a;

    if (bank == 0x7E || bank == 0x7F) {
        wram[a - 0x7E0000] = v;
        return;
    }
    if (bank == 0x70) {
        pakram[o] = v;
        return;
    }
    if ((bank & 0x7F) >= 0x40 || o >= 0x8000)
        die("write", a);
    if (o < 0x2000) {
        wram[o] = v;
        return;
    }
    if (o >= 0x6000) {
        pakram[o - 0x6000] = v;
        return;
    }
    if (model_io_n < 4096)
        model_io[model_io_n++] = (uint32_t)o << 8 | v;
    if (o >= 0x3000 && o < 0x3020) {        /* the Super FX's registers; R15's high byte starts it */
        uint16_t *r = &old_gsu.r[(o - 0x3000) >> 1];

        *r = (o & 1) ? (uint16_t)((*r & 0x00FF) | v << 8) : (uint16_t)((*r & 0xFF00) | v);
        if (o == 0x301F) {
            if (!model_gsu_start)
                die("Super FX start", a);
            model_gsu_start(gsu_pbr, *r);
        }
        return;
    }
    if ((o >= 0x2100 && o < 0x2134) || (o >= 0x4300 && o < 0x4380) || o == 0x420B || o == 0x420C) {
        ppu_write(o, v);                /* the video hardware (the checks' model of it) */
        if (o != 0x211B && o != 0x211C)
            return;
    }
    switch (o) {
    case 0x211B: m7a = (uint16_t)(v << 8 | m7_latch); m7_latch = v; return;
    case 0x211C: m7b = v; m7_latch = v; return;
    case 0x3030: case 0x3031: return;     /* the status cleared */
    case 0x3034: gsu_pbr = v; return;
    case 0x3038: old_gsu.scbr = v; return;
    case 0x303A: old_gsu.scmr = v; return;
    case 0x303B: return;                  /* (the high byte of a 16-bit store to SCMR) */
    case 0x4202: wrmpya = v; return;
    case 0x4203: rdmpy = (uint16_t)(wrmpya * v); return;
    case 0x4204: wrdiv = (uint16_t)((wrdiv & 0xFF00) | v); return;
    case 0x4205: wrdiv = (uint16_t)((wrdiv & 0x00FF) | v << 8); return;
    case 0x4206:
        rddiv = v ? (uint16_t)(wrdiv / v) : 0xFFFF;
        rdmpy = v ? (uint16_t)(wrdiv % v) : wrdiv;
        return;
    }
    die("write", a);
}

uint8_t old_gsu_rd8(uint32_t a)
{
    uint8_t bank = (uint8_t)(a >> 16);

    if (bank < 0x60)
        return srfx_rom_image[(bank & 0x3F) * 0x8000 + (a & 0x7FFF)];
    if (bank == 0x70)
        return pakram[(uint16_t)a];
    die("Super FX read", a);
    return 0;
}

void old_gsu_wr8(uint32_t a, uint8_t v)
{
    if ((a >> 16) == 0x70)
        pakram[(uint16_t)a] = v;
    else
        die("Super FX write", a);
}

void old_cyc(uint32_t c) { (void)c; }
void old_cpu_idle(uint32_t c) { (void)c; }
int model_trace;   /* the decomp's Super FX code addresses printed as it runs */
uint8_t model_cover[0x10000];   /* bank $01's code addresses the decomp has run, while model_covering */
int model_covering;
void old_gsu_code(uint32_t a, uint32_t n)
{
    if (model_covering && (a >> 16) == 1)
        for (uint32_t i = 0; i < n; i++)
            model_cover[(a + i) & 0xFFFF] = 1;
    if (model_trace)
        fprintf(stderr, "  %06X+%u R0=%04X R1=%04X R2=%04X R14=%04X 5C=%04X\n", a, n, old_gsu.r[0], old_gsu.r[1], old_gsu.r[2], old_gsu.r[14], pakram[0x5C] | pakram[0x5D] << 8);
}
void old_gsu_cache(uint16_t r15) { (void)r15; }
void old_gsu_cyc(uint32_t c) { (void)c; }
void old_gsu_time(int w) { (void)w; }
void old_gsu_buffer(int w) { (void)w; }
/* PLOT and RPIX through the Super FX's two pixel caches (the sfxbox
   model, tools/stuntracefx/sfxbox/gsu_clock.c), on the game pak RAM */
static uint8_t pix_read(GsuClock *c, uint32_t a) { (void)c; return pakram[(uint16_t)a]; }
static void pix_write(GsuClock *c, uint32_t a, uint8_t v) { (void)c; pakram[(uint16_t)a] = v; }
static GsuClock pixels = {.pix_read = pix_read, .pix_write = pix_write};

static GsuScreen model_screen(void)
{
    unsigned ht = (old_gsu.scmr & 0x20 ? 2u : 0u) | (old_gsu.scmr & 0x04 ? 1u : 0u);
    GsuScreen s = {(uint8_t)(old_gsu.por & 0x10 ? 3 : ht), (uint8_t)(old_gsu.scmr & 3), old_gsu.scbr};

    return s;
}

void old_gsu_plot(uint8_t x, uint8_t y)
{
    unsigned md = old_gsu.scmr & 3u;
    uint8_t color = old_gsu.colr;
    GsuScreen s = model_screen();

    if (!(old_gsu.por & 0x01)) {
        if (md == 3) {
            if (old_gsu.por & 0x08) {
                if ((old_gsu.colr & 0x0F) == 0)
                    return;
            } else if (old_gsu.colr == 0)
                return;
        } else if ((old_gsu.colr & 0x0F) == 0)
            return;
    }
    if ((old_gsu.por & 0x02) && md != 3) {
        if ((x ^ y) & 1)
            color >>= 4;
        color &= 0x0F;
    }
    gsuclk_plot(&pixels, &s, x, y, color);
}

uint8_t old_gsu_rpix(uint8_t x, uint8_t y)
{
    GsuScreen s = model_screen();

    return gsuclk_rpix(&pixels, &s, x, y);
}

void model_pixels_empty(void)
{
    pixels.pix[0].bitpend = pixels.pix[1].bitpend = 0;
    pixels.pix[0].offset = pixels.pix[1].offset = 0xFFFF;
}

void model_pixels_flush(void)
{
    GsuScreen s = model_screen();

    gsuclk_rpix(&pixels, &s, 0, 0);
}

/* a pixel cache as a recording left it */
void model_pixels_set(int i, uint16_t offset, uint8_t bitpend, const uint8_t data[8])
{
    pixels.pix[i].offset = offset;
    pixels.pix[i].bitpend = bitpend;
    memcpy(pixels.pix[i].data, data, 8);
}
