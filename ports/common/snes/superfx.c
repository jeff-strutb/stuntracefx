/*
 * superfx.c: the Super FX (GSU-1) beside the S-CPU: its registers as the
 * S-CPU sees them ($3000-$32FF), its RAM, and the bus and plotting its C
 * uses (gsu.h).
 *
 * A program starts when the S-CPU writes R15's high byte (or sets GO): its
 * C (found by the board at PBR:R15, snes_gsu_run_hook) runs to its STOP
 * there and then. Its time is the chip's own, not its instructions': the
 * cartridge's price for the program (gsu_host_price) is a load model of
 * the operations the chip sees it make, a fixed part and a weight for each
 * access to its RAM by what made it (a load, a store, a pixel-cache
 * flush's byte, an RPIX read), measured from the original's jobs of that
 * program (tools/stuntracefx/gsuprice.py). Its duration is that sum, in
 * Super FX clocks of its 21.44 MHz crystal: GO reads set and the IRQ waits
 * until the console's clock reaches its start and that duration
 * (gsu_host_irq_at). Its ROM-buffer fills are timed for the S-CPU's holds
 * of the ROM, not priced: the fixed part carries them. A
 * hold of the ROM or the RAM by the S-CPU in that time (SCMR's RON or RAN
 * clear) moves the end on by as long as the program's next access to that
 * bus after the hold's start waits for its end. While the console holds
 * interrupts (code run between frames, outside its time: gsu_hold), a
 * program ends at once, and the game's program is kept as it was.
 *
 * PLOT and RPIX go through the two pixel caches as on the hardware: a row
 * of 8 pixels reaches the RAM's bitplanes when it is flushed (a new row,
 * a full one, RPIX), so the RAM holds what the original's does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "superfx.h"

/* the decomp's register file (gsu.h's RegsGSU), laid out as it declares */
typedef struct {
    uint16_t r[16];
    uint8_t z, cy, s, ov;
    uint8_t rombr, rambr;
    uint8_t colr, por;
    uint8_t scmr, scbr;
    uint16_t ramaddr;
    uint8_t romdr, romdr_unheld;
    uint8_t pre_b, pre_alt1, pre_alt2, pre_sreg, pre_dreg;
} RegsGSU;

RegsGSU gsu;
uint8_t *gsu_ram_mem;

void (*snes_gsu_run_hook)(uint8_t bank, uint16_t addr);
static uint32_t fetch_at;                /* where the ROM buffer was last filled from */

static uint8_t ram_store[0x20000];
static uint8_t cache[0x200];             /* the code cache, as the S-CPU writes and reads it */
static uint16_t cbr;                     /* its base (CBR) */
static uint8_t pbr, cfgr, clsr, bramr;
static uint8_t reg_latch;
static uint8_t sfr_irq, sfr_alt, sfr_b;
static int go;

/* The cartridge clocks the Super FX from a crystal of its own, 21.44 MHz,
   not the console's 21.477 MHz */
#define GSU_HZ 21440000u
#define MASTER_HZ 21477273u
/* the running program: its start on the console's clock, its price (the
   cartridge's), the cost of what it has done so far (Super FX clocks in
   1/GSU_PRICE_ONE), and its accesses by kind (for the run log) */
static uint64_t run_master, cost;
static GsuPrice price;
static uint64_t ops[GSU_OPS];
/* its end shown at the console's clock done_at (shown: GO still reads set);
   for each bus (BUS_ROM, BUS_RAM) the console's clocks of its accesses (at,
   in order) for a hold of that bus to wait on, and the clock that hold
   began at (held_from, NO_HOLD for none) */
#define NO_HOLD UINT64_MAX
enum { BUS_ROM, BUS_RAM };
typedef struct {
    uint64_t *at;
    size_t n, cap;
    uint64_t held_from;
} BusLog;
static int shown;
static uint64_t done_at;
static BusLog bus_log[2] = {{NULL, 0, 0, NO_HOLD}, {NULL, 0, 0, NO_HOLD}};
/* SCMR's bit for each bus: the Super FX has it while set */
static const uint8_t bus_bit[2] = {0x10, 0x08};
/* interrupts held (gsu_hold), and what of the game's program it keeps */
static int holding;
static struct {
    int shown;
    uint64_t done_at;
    BusLog bus_log[2];
    uint8_t sfr_irq;
} kept;

/* the pixel caches: the row of 8 pixels each holds, which of them are
   plotted, their colours (bit 7 - x's low bits) */
static struct GsuPixels {
    uint16_t offset;
    uint8_t bitpend;
    uint8_t data[8];
} pix[2];

static inline uint8_t bus_byte(uint32_t addr);
uint8_t gsu_fetch(uint32_t addr);
static int is_ram(uint32_t addr) { uint8_t bank = (uint8_t)(addr >> 16); return bank == 0x70 || bank == 0x71; }

void gsu_reset(void)
{
    memset(&gsu, 0, sizeof gsu);
    /* a cartridge with no save: its RAM reads $FF throughout, as the
       verification harness powers the original on */
    memset(ram_store, 0xFF, sizeof ram_store);
    memset(cache, 0, sizeof cache);
    memset(pix, 0, sizeof pix);
    gsu_ram_mem = ram_store;
    cbr = 0;
    pbr = cfgr = clsr = bramr = 0;
    reg_latch = 0;
    sfr_irq = sfr_alt = sfr_b = 0;
    go = 0;
    run_master = cost = 0;
    memset(&price, 0, sizeof price);
    memset(ops, 0, sizeof ops);
    shown = holding = 0;
    done_at = 0;
    for (int k = 0; k < 2; k++)
        bus_log[k].n = 0, bus_log[k].held_from = NO_HOLD;
}

uint8_t *snes_sfx_ram(void) { return ram_store; }

/* ---- time ---- */
/* the console's clock the running program has reached: its start and its
   cost so far, priced */
static uint64_t master_now(void)
{
    return run_master + (cost / GSU_PRICE_ONE) * MASTER_HZ / GSU_HZ;
}

/* a bus access of the running program, of kind op (GSU_OP_*; GSU_OPS: a
   ROM-buffer fill, not priced): its cost, its time logged for a hold of
   that bus to wait on */
static void bus_access(int bus, int op)
{
    BusLog *b = &bus_log[bus];
    if (!go)
        return;
    if (op < GSU_OPS) {
        cost += price.weight[op];
        ops[op]++;
    }
    if (holding)
        return;
    if (b->n == b->cap) {
        uint64_t *n;
        b->cap = b->cap ? b->cap * 2 : 65536;
        n = realloc(b->at, b->cap * sizeof *b->at);
        if (!n)
            gsu_host_fatal("out of memory for the Super FX's bus accesses");
        b->at = n;
    }
    b->at[b->n++] = master_now();
}

/* a program's next access to a bus at or after console clock t: its index
   in the bus's log, or the log's length for none */
static size_t access_from(const BusLog *b, uint64_t t)
{
    size_t lo = 0, hi = b->n;
    while (lo < hi) {
        size_t m = lo + (hi - lo) / 2;
        if (b->at[m] < t)
            lo = m + 1;
        else
            hi = m;
    }
    return lo;
}

/* a bus held with an access still to make on it: the program waits */
static int stalled(void)
{
    for (int k = 0; k < 2; k++)
        if (bus_log[k].held_from != NO_HOLD && access_from(&bus_log[k], bus_log[k].held_from) < bus_log[k].n)
            return 1;
    return 0;
}

/* a program's end, once the console's clock is there: not while a hold of
   a bus keeps it from an access it has still to make */
static void settle(void)
{
    if (!shown || gsu_host_clock() < done_at || stalled())
        return;
    shown = 0;
}

int gsu_running(void) { settle(); return go || shown; }
int gsu_irq(void) { settle(); return sfr_irq && !shown; }
void gsu_irq_put(int on)
{
    settle();
    if (shown)
        return;   /* the program's interrupt is still to come, at done_at */
    sfr_irq = (uint8_t)(on != 0);
    gsu_host_irq(sfr_irq);
}

#ifdef SUPERFX_RUNLOG
/* -DSUPERFX_RUNLOG, SUPERFX_RUNLOG=FILE: each program as it stops,
   "g START PC LOADS STORES FLUSH RPIX CLOCKS": its start on the console's
   clock, its PBR:R15, its RAM accesses by kind (as sfxbox's SFXBOX_GSURUN
   counts the original's), its duration (the console's clocks) */
static void runlog(uint32_t pc)
{
    static FILE *f;
    static int opened;
    if (!opened) {
        opened = 1;
        if (getenv("SUPERFX_RUNLOG"))
            f = fopen(getenv("SUPERFX_RUNLOG"), "w");
    }
    if (!f)
        return;
    fprintf(f, "g %llu %06X %llu %llu %llu %llu %llu\n", (unsigned long long)run_master, (unsigned)pc,
            (unsigned long long)ops[GSU_OP_LOAD], (unsigned long long)ops[GSU_OP_STORE],
            (unsigned long long)ops[GSU_OP_FLUSH], (unsigned long long)ops[GSU_OP_RPIX],
            (unsigned long long)(master_now() - run_master));
    fflush(f);
}
#else
static void runlog(uint32_t pc) { (void)pc; }
#endif

/* A program run to its STOP; GO reads clear, and the interrupt (unless
   masked) rises, when the console gets to its end; held, at once */
static void program_starts(void)
{
    uint32_t pc = (uint32_t)pbr << 16 | gsu.r[15];
    settle();
    if (go || shown)
        gsu_host_fatal("the S-CPU started a Super FX program while one runs");
    if (!snes_gsu_run_hook)
        gsu_host_fatal("Super FX started with no runtime");
    go = 1;
    run_master = gsu_host_clock();
    gsu_host_price((uint8_t)(pc >> 16), (uint16_t)pc, &price);
    cost = (uint64_t)price.fixed * GSU_PRICE_ONE;
    memset(ops, 0, sizeof ops);
    /* a bus the S-CPU has as the program starts: held from its start */
    for (int k = 0; k < 2; k++) {
        bus_log[k].n = 0;
        bus_log[k].held_from = (gsu.scmr & bus_bit[k]) ? NO_HOLD : run_master;
    }
    gsu.pre_b = gsu.pre_alt1 = gsu.pre_alt2 = gsu.pre_sreg = gsu.pre_dreg = 0;
    snes_gsu_run_hook(pbr, gsu.r[15]);
    go = 0;
    sfr_alt = sfr_b = 0;
    if (!holding) {
        done_at = master_now();
        shown = 1;
        runlog(pc);
    }
    if (!(cfgr & 0x80)) {
        sfr_irq = 1;
        if (holding)
            gsu_host_irq(1);
        else
            gsu_host_irq_at(1, done_at);
    }
}

/* Interrupts held (on) or released: the game's program, shown running,
   kept aside and put back, so programs the held code runs end at once and
   leave it as it was */
void gsu_hold(int on)
{
    if (on == holding)
        return;
    if (on) {
        settle();
        kept.shown = shown;
        kept.done_at = done_at;
        memcpy(kept.bus_log, bus_log, sizeof bus_log);
        kept.sfr_irq = sfr_irq;
        shown = 0;
        for (int k = 0; k < 2; k++)
            bus_log[k] = (BusLog){NULL, 0, 0, NO_HOLD};
        holding = 1;
        return;
    }
    for (int k = 0; k < 2; k++)
        free(bus_log[k].at);
    shown = kept.shown;
    done_at = kept.done_at;
    memcpy(bus_log, kept.bus_log, sizeof bus_log);
    if (shown) {
        /* the game's interrupt at its program's end, not a held one's */
        sfr_irq = kept.sfr_irq;
        gsu_host_irq_at(0, gsu_host_clock());
        if (sfr_irq)
            gsu_host_irq_at(1, done_at);
    }
    holding = 0;
}

/* SCMR written while a program is shown running: a hold of its ROM (RON
   clear) or its RAM (RAN clear) begun or ended at the console's clock; at
   its end, the program's first access to that bus after the hold's start
   waits for it, and its end, its later accesses (to both buses) and its
   interrupt move on as long */
static void scmr_written(uint8_t v)
{
    uint64_t now = gsu_host_clock();
    uint64_t first[2];
    int order[2] = {0, 1}, n = 0;
    if (!shown)
        return;
    for (int k = 0; k < 2; k++) {
        BusLog *b = &bus_log[k];
        size_t i;
        if (!(v & bus_bit[k])) {
            if (b->held_from == NO_HOLD)
                b->held_from = now;
            continue;
        }
        if (b->held_from == NO_HOLD)
            continue;
        i = access_from(b, b->held_from);
        first[k] = i < b->n ? b->at[i] : NO_HOLD;
        order[n++] = k;
    }
    /* the buses given back, by the program's first access to each in the
       hold: the program stops at the first of them, so a bus whose access
       comes after it waits no more once that wait has moved it on */
    if (n == 2 && first[order[1]] < first[order[0]])
        order[0] = 1, order[1] = 0;
    for (int o = 0; o < n; o++) {
        int k = order[o];
        BusLog *b = &bus_log[k];
        size_t i = access_from(b, b->held_from);
        uint64_t a, wait;
        b->held_from = NO_HOLD;
        if (i == b->n || b->at[i] >= now)
            continue;
        a = b->at[i];
        wait = now - a;
        for (int j = 0; j < 2; j++)
            for (size_t m = access_from(&bus_log[j], a); m < bus_log[j].n; m++)
                bus_log[j].at[m] += wait;
        done_at += wait;
        if (sfr_irq) {
            gsu_host_irq_at(0, now);
            gsu_host_irq_at(1, done_at);
        }
    }
}

/* ---- the S-CPU's side ---- */
static uint16_t sfr(void)
{
    int run = go || shown, irq = sfr_irq && !shown;
    return (uint16_t)(gsu.z << 1 | gsu.cy << 2 | gsu.s << 3 | gsu.ov << 4 | run << 5 |
                      (sfr_alt & 3) << 8 | sfr_b << 12 | irq << 15);
}

uint8_t gsu_reg_read(uint16_t addr)
{
    settle();
    if (addr >= 0x3100 && addr < 0x3300)
        return cache[(addr - 0x3100 + cbr) & 511];
    if (addr < 0x3020) {
        uint16_t r = gsu.r[(addr >> 1) & 15];
        return (uint8_t)((addr & 1) ? r >> 8 : r);
    }
    switch (addr) {
    case 0x3030: return (uint8_t)sfr();
    case 0x3031: {
        uint8_t v = (uint8_t)(sfr() >> 8);
        if (!shown) {             /* shown running: its interrupt is still to come */
            sfr_irq = 0;
            gsu_host_irq(0);
        }
        return v;
    }
    case 0x3034: return pbr;
    case 0x3036: return gsu.rombr;
    case 0x303B: return 0x01;            /* VCR: the GSU-1 */
    case 0x303C: return gsu.rambr;
    case 0x303E: return (uint8_t)cbr;
    case 0x303F: return (uint8_t)(cbr >> 8);
    }
    return gsu_host_open_bus();
}

void gsu_reg_write(uint16_t addr, uint8_t v)
{
    settle();
    if (addr >= 0x3100 && addr < 0x3300) {
        cache[(addr - 0x3100 + cbr) & 511] = v;   /* the code cache, CBR on */
        return;
    }
    if (addr < 0x3020) {
        int n = (addr >> 1) & 15;
        if (!(addr & 1)) {
            reg_latch = v;
            return;
        }
        gsu.r[n] = (uint16_t)(v << 8 | reg_latch);
        if (n == 14)
            gsu.romdr = gsu_fetch((uint32_t)gsu.rombr << 16 | gsu.r[14]);
        if (n == 15)
            program_starts();   /* R15 written: the program runs */
        return;
    }
    switch (addr) {
    case 0x3030:
        gsu.z = (v >> 1) & 1;
        gsu.cy = (v >> 2) & 1;
        gsu.s = (v >> 3) & 1;
        gsu.ov = (v >> 4) & 1;
        if (!(v & 0x20) && shown)
            gsu_host_fatal("the S-CPU stopped a running Super FX program (not supported)");
        if (v & 0x20)
            program_starts();
        return;
    case 0x3031:
        sfr_alt = v & 3;
        sfr_b = (v >> 4) & 1;
        sfr_irq = (v >> 7) & 1;
        return;
    case 0x3033: bramr = v; return;
    case 0x3034: pbr = v; return;
    case 0x3037: cfgr = v; return;
    case 0x3038: gsu.scbr = v; return;
    case 0x3039: clsr = v; return;
    case 0x303A: scmr_written(v); gsu.scmr = v; return;
    }
}

/* ---- the Super FX's bus, for its C ---- */
static inline uint8_t bus_byte_by(uint32_t addr, int fetch, int *unheld)
{
    uint8_t bank = (uint8_t)(addr >> 16);
    uint16_t off = (uint16_t)addr;
    if (bank == 0x70 || bank == 0x71)
        return ram_store[((uint32_t)(bank & 1) << 16 | off) & (gsu_host_ram_size() - 1)];
    if (bank < 0x40)
        return gsu_host_rom((uint32_t)(bank & 0x3F) << 15 | (off & 0x7FFF), fetch, unheld);
    if (bank < 0x60)
        return gsu_host_rom((uint32_t)(bank & 0x1F) << 16 | off, fetch, unheld);
    return 0;
}

static inline uint8_t bus_byte(uint32_t addr) { int unheld; return bus_byte_by(addr, 0, &unheld); }

/* The ROM buffer's fill (an R14 write): the byte at addr, which the program
   may never read; one no asset holds (code) marks the buffer instead of
   stopping the port, and only a GETB or GETC of it does (gsu_fetch_used). */
uint8_t gsu_fetch(uint32_t addr)
{
    uint8_t v;
    int unheld = 0;
    addr &= 0xFFFFFF;
    fetch_at = addr;
    bus_access(is_ram(addr) ? BUS_RAM : BUS_ROM, GSU_OPS);   /* the ROM buffer's fill */
    v = bus_byte_by(addr, 1, &unheld);
    gsu.romdr_unheld = (uint8_t)unheld;
    return v;
}

void gsu_fetch_used(void)
{
    gsu.romdr = bus_byte(fetch_at);
    gsu.romdr_unheld = 0;
}

static uint8_t *ram_byte(uint32_t addr)
{
    return &ram_store[((uint32_t)((addr >> 16) & 1) << 16 | (uint16_t)addr) & (gsu_host_ram_size() - 1)];
}

/* a load or a store, through the RAM buffer */
uint8_t gsu_rd8(uint32_t addr)
{
    addr &= 0xFFFFFF;
    bus_access(is_ram(addr) ? BUS_RAM : BUS_ROM, is_ram(addr) ? GSU_OP_LOAD : GSU_OPS);
    return bus_byte(addr);
}

void gsu_wr8(uint32_t addr, uint8_t value)
{
    addr &= 0xFFFFFF;
    if (!is_ram(addr))
        return;
    bus_access(BUS_RAM, GSU_OP_STORE);
    *ram_byte(addr) = value;
}

/* ---- plotting ---- */
/* The screen as PLOT and RPIX lay it out: ht the height (POR's OBJ: 3, the
   sprite layout), md SCMR's colour depth (2, 4, 4, 8 bits), scbr its base
   in 1 KB. */
typedef struct { uint8_t ht, md, scbr; } GsuScreen;

/* SCMR's colour depth (bits 0-1) and height (bits 2 and 5; POR's OBJ, bit
   4, the sprite layout), its base SCBR x 1 KB */
static GsuScreen screen(void)
{
    GsuScreen s;
    s.ht = (uint8_t)((gsu.por & 0x10) ? 3 : ((gsu.scmr >> 2) & 1) | ((gsu.scmr >> 4) & 2));
    s.md = (uint8_t)(gsu.scmr & 3);
    s.scbr = gsu.scbr;
    return s;
}

static unsigned bpp_of(const GsuScreen *s) { return 2u << (s->md - (s->md >> 1)); }

/* the address of the row of pixel (x, y)'s character */
static uint32_t char_addr(const GsuScreen *s, uint8_t x, uint8_t y)
{
    unsigned cn = 0;
    switch (s->ht) {
    case 0: cn = ((x & 0xF8u) << 1) + ((y & 0xF8u) >> 3); break;
    case 1: cn = ((x & 0xF8u) << 1) + ((x & 0xF8u) >> 1) + ((y & 0xF8u) >> 3); break;
    case 2: cn = ((x & 0xF8u) << 1) + ((x & 0xF8u) << 0) + ((y & 0xF8u) >> 3); break;
    default: cn = ((y & 0x80u) << 2) + ((x & 0x80u) << 1) + ((y & 0x78u) << 1) + ((x & 0x78u) >> 3); break;
    }
    return 0x700000u + cn * (bpp_of(s) << 3) + ((uint32_t)s->scbr << 10) + (uint32_t)(y & 7) * 2;
}

/* A cache's row written out, a bitplane byte at a time: the pixels not
   plotted keep the RAM's bits. */
static void flush_pixels(const GsuScreen *s, struct GsuPixels *p)
{
    if (!p->bitpend)
        return;
    uint32_t addr = char_addr(s, (uint8_t)(p->offset << 3), (uint8_t)(p->offset >> 5));
    unsigned bpp = bpp_of(s);
    for (unsigned n = 0; n < bpp; n++) {
        uint32_t byte = ((n >> 1) << 4) + (n & 1);
        uint8_t data = 0;
        for (unsigned k = 0; k < 8; k++)
            data |= (uint8_t)(((p->data[k] >> n) & 1) << k);
        if (p->bitpend != 0xFF) {
            bus_access(BUS_RAM, GSU_OP_FLUSH);   /* a row not all plotted: read first */
            data &= p->bitpend;
            data |= (uint8_t)(*ram_byte(addr + byte) & ~p->bitpend);
        }
        bus_access(BUS_RAM, GSU_OP_FLUSH);
        *ram_byte(addr + byte) = data;
    }
    p->bitpend = 0;
}

void (*snes_gsu_plot_hook)(uint8_t x, uint8_t y, uint8_t c);

void gsu_plot(uint8_t x, uint8_t y)
{
    GsuScreen s = screen();
    uint8_t c = gsu.colr;
    uint16_t offset;
    if (!(gsu.por & 0x01)) {
        if (s.md == 3) {
            if (gsu.por & 0x08) {
                if (!(c & 0x0F))
                    return;
            } else if (!c) {
                return;
            }
        } else if (!(c & 0x0F)) {
            return;
        }
    }
    if ((gsu.por & 0x02) && s.md != 3) {
        if ((x ^ y) & 1)
            c >>= 4;
        c &= 0x0F;
    }
    if (snes_gsu_plot_hook)
        snes_gsu_plot_hook(x, y, c);
    offset = (uint16_t)((y << 5) + (x >> 3));
    if (offset != pix[0].offset) {
        flush_pixels(&s, &pix[1]);
        pix[1] = pix[0];
        pix[0].bitpend = 0;
        pix[0].offset = offset;
    }
    x = (uint8_t)((x & 7) ^ 7);
    pix[0].data[x] = c;
    pix[0].bitpend |= (uint8_t)(1u << x);
    if (pix[0].bitpend == 0xFF) {
        flush_pixels(&s, &pix[1]);
        pix[1] = pix[0];
        pix[0].bitpend = 0;
    }
}

uint8_t gsu_rpix(uint8_t x, uint8_t y)
{
    GsuScreen s = screen();
    uint32_t addr;
    unsigned bpp = bpp_of(&s);
    uint8_t data = 0;
    flush_pixels(&s, &pix[1]);
    flush_pixels(&s, &pix[0]);
    addr = char_addr(&s, x, y);
    x = (uint8_t)((x & 7) ^ 7);
    for (unsigned n = 0; n < bpp; n++) {
        uint32_t byte = ((n >> 1) << 4) + (n & 1);
        bus_access(BUS_RAM, GSU_OP_RPIX);
        data |= (uint8_t)(((*ram_byte(addr + byte) >> x) & 1) << n);
    }
    return data;
}
