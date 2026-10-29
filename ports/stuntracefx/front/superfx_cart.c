/*
 * superfx_cart.c: the cartridge's Super FX (ports/common/snes/superfx.c) on
 * nativeshvc's cartridge connector (cart.h), with the board's decoding:
 *
 *   banks $00-$3F, $80-$BF: $3000-$32FF the Super FX's registers, $6000-
 *     $7FFF its RAM's first 8 KB, $8000-$FFFF the ROM in 32 KB banks;
 *   banks $40-$5F, $C0-$DF: the whole ROM in 64 KB banks;
 *   banks $70-$71, $F0-$F1: its RAM (the cartridge's size, mirrored).
 *
 * The console gives the chip its clock, IRQ line, open bus and the ROM
 * (gsu_host_*); a program the S-CPU starts runs its C from the cartridge's
 * routines (the routine at PBR:R15) to its STOP, and takes the time its
 * row of the cartridge's cost profile gives it (gsu_host_price); in
 * Enhanced mode (rm_enhanced), none.
 */
#include "nshvc/cart.h"

#include "front.h"
#include "remaster.h"
#include "fullwindow.h"
#include "superfx.h"

static uint32_t ram_size;
int front_rom_unheld;

/* ---- what the connector gives the chip (superfx.h) ---- */
uint64_t gsu_host_clock(void) { return nshvc_cart_clock(); }
void gsu_host_irq(int level) { nshvc_cart_irq(level); }
void gsu_host_irq_at(int level, uint64_t clock) { nshvc_cart_irq_at(level, clock); }
uint8_t gsu_host_open_bus(void) { return nshvc_open_bus(); }
uint32_t gsu_host_ram_size(void) { return ram_size; }
int cart_rom_held(uint32_t off, uint8_t *v);

uint8_t gsu_host_rom(uint32_t off, int fetch, int *unheld)
{
    uint8_t v;
    if (cart_rom_held(off, &v)) {       /* the cartridge's ROM, its own chip's read */
        *unheld = 0;
        return v;
    }
    front_rom_unheld = 0;
    v = nshvc_cart_rom(off, NSHVC_ROM_CHIP | (fetch ? FRONT_ROM_FETCH : 0));
    *unheld = front_rom_unheld;
    return v;
}
void gsu_host_fatal(const char *msg) { nshvc_fatal("%s", msg); }

/* ---- its programs' prices ---- */
/* Each program's load model (superfx.h GsuPrice) from the cartridge's
   cost profile (nshvc_cart.cost_profile, docs/cost-profile.md, made by
   nativeshvc's tools/nshvc-profile from the original's jobs): a fixed part
   in Super FX clocks and a weight for each RAM access it makes by kind
   (LOAD, STORE, FLUSH, RPIX, the profile's order), in 1/GSU_PRICE_ONE of a
   clock; its rows sorted by entry (PBR:R15), and a row for a program not
   listed. With no profile, a program costs its accesses at the chip's bus
   speed, 5 clocks each. In Enhanced mode a program costs nothing: the
   game's pace does not hang on the chip's speed. */
static const nshvc_cost_profile *profile;

static void price_of(const nshvc_chip_program *row, GsuPrice *out)
{
    out->fixed = row->fixed;
    for (int k = 0; k < GSU_OPS; k++)
        out->weight[k] = row->weight[k];
}

void gsu_host_price(uint8_t bank, uint16_t addr, GsuPrice *out)
{
    uint32_t pc = (uint32_t)bank << 16 | addr;
    size_t lo = 0, hi;
    if (rm_enhanced()) {
        out->fixed = 0;
        for (int k = 0; k < GSU_OPS; k++)
            out->weight[k] = 0;
        return;
    }
    if (!profile) {
        out->fixed = 0;
        for (int k = 0; k < GSU_OPS; k++)
            out->weight[k] = 5 * GSU_PRICE_ONE;
        return;
    }
    hi = (size_t)profile->program_count;
    while (lo < hi) {
        size_t m = lo + (hi - lo) / 2;
        if (profile->programs[m].entry < pc)
            lo = m + 1;
        else
            hi = m;
    }
    if (lo < (size_t)profile->program_count && profile->programs[lo].entry == pc)
        price_of(&profile->programs[lo], out);
    else
        price_of(&profile->chip_default, out);
}

/* ---- its programs: the cartridge's routines ---- */
static void run_program(uint8_t bank, uint16_t addr)
{
    const nshvc_routine *r = nshvc_cart_routine((uint32_t)bank << 16 | addr);
    if (!r)
        nshvc_fatal("Super FX program at $%02X:%04X has no C", bank, addr);
    fw_gsu_program(1);                  /* Enhanced's full window: the HUD's pieces whole (fullwindow.h) */
    r->abi();
    fw_gsu_program(0);
}

/* ---- the board ---- */
static void power(const nshvc_cart *cart)
{
    profile = cart->cost_profile;
    if (profile && profile->chip_ops != GSU_OPS)
        nshvc_fatal("the cost profile prices %d Super FX op kinds, the chip makes %d", profile->chip_ops, GSU_OPS);
    ram_size = cart->sram_size;
    snes_gsu_run_hook = run_program;
    gsu_reset();
}

/* the S-CPU's address in the chip's RAM (*at) or its registers (*reg);
   0: neither */
static int decodes(uint32_t addr, uint32_t *at, int *reg)
{
    uint8_t bank = (uint8_t)(addr >> 16);
    uint16_t off = (uint16_t)addr;
    *reg = 0;
    if (!(bank & 0x40)) {
        if (off >= 0x3000 && off < 0x3300) {
            *reg = 1;
            return 1;
        }
        if (off >= 0x6000 && off < 0x8000) {
            *at = (uint32_t)(off - 0x6000) & (ram_size - 1);
            return 1;
        }
        return 0;
    }
    bank &= 0x7F;
    if (bank == 0x70 || bank == 0x71) {
        *at = ((uint32_t)(bank & 1) << 16 | off) & (ram_size - 1);
        return 1;
    }
    return 0;
}

static int cpu_read(uint32_t addr, uint8_t *v)
{
    uint32_t at;
    int reg;
    if (!decodes(addr, &at, &reg))
        return 0;
    *v = reg ? gsu_reg_read((uint16_t)addr) : snes_sfx_ram()[at];
    return 1;
}

static int cpu_write(uint32_t addr, uint8_t v)
{
    uint32_t at;
    int reg;
    if (!decodes(addr, &at, &reg))
        return 0;
    if (reg)
        gsu_reg_write((uint16_t)addr, v);
    else
        snes_sfx_ram()[at] = v;
    return 1;
}

/* a look with no effect: the RAM; the registers read 0 */
static int cpu_peek(uint32_t addr, uint8_t *v)
{
    uint32_t at;
    int reg;
    if (!decodes(addr, &at, &reg))
        return 0;
    *v = reg ? 0 : snes_sfx_ram()[at];
    return 1;
}

static int rom_at(uint32_t addr, uint32_t *off)
{
    uint8_t bank = (uint8_t)(addr >> 16);
    uint16_t o = (uint16_t)addr;
    if (!(bank & 0x40)) {
        if (o < 0x8000)
            return 0;
        *off = (uint32_t)(bank & 0x3F) << 15 | (o & 0x7FFF);
        return 1;
    }
    bank &= 0x7F;
    if (bank < 0x60) {
        *off = (uint32_t)(bank & 0x1F) << 16 | o;
        return 1;
    }
    return 0;
}

/* the interrupt as a hold found it, put back as the hold ends: a program
   the held code started (its STOP raising the interrupt) leaves the game's
   own handlers nothing to take */
static int held_irq;

static void hold(int on)
{
    if (on) {
        held_irq = gsu_irq();
        gsu_hold(1);
    } else {
        gsu_hold(0);
        gsu_irq_put(held_irq);
    }
}

static uint8_t *ram(uint32_t *size)
{
    *size = ram_size;
    return snes_sfx_ram();
}

const nshvc_chip front_superfx = {
    .name = "Super FX",
    .power = power,
    .read = cpu_read,
    .write = cpu_write,
    .peek = cpu_peek,
    .rom_at = rom_at,
    .hold = hold,
    .ram = ram,
    .busy = gsu_running,
};
