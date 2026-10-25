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

const uint8_t *srfx_rom_image;
const char *model_current = "";
Regs65816 old_cpu;
RegsGSU old_gsu;
int model_io_n;
uint32_t model_io[4096];
static uint8_t m7_latch, m7b, wrmpya;
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
    switch (o) {
    case 0x211B: m7a = (uint16_t)(v << 8 | m7_latch); m7_latch = v; return;
    case 0x211C: m7b = v; m7_latch = v; return;
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
void old_gsu_code(uint32_t a, uint32_t n) { (void)a; (void)n; }
void old_gsu_cache(uint16_t r15) { (void)r15; }
void old_gsu_cyc(uint32_t c) { (void)c; }
void old_gsu_time(int w) { (void)w; }
void old_gsu_buffer(int w) { (void)w; }
void old_gsu_plot(uint8_t x, uint8_t y) { (void)x; (void)y; die("plot", 0); }
uint8_t old_gsu_rpix(uint8_t x, uint8_t y) { (void)x; (void)y; die("rpix", 0); return 0; }
