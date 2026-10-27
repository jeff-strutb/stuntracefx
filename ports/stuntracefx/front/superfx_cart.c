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
 * routines (the routine at PBR:R15; flags bit 0: its first instruction is
 * CACHE).
 */
#include "nshvc/cart.h"

#include "front.h"
#include "superfx.h"

static uint32_t ram_size;
int front_rom_unheld;

/* ---- what the connector gives the chip (superfx.h) ---- */
uint64_t gsu_host_clock(void) { return nshvc_cart_clock(); }
int gsu_host_untimed(void) { return nshvc_untimed(); }
void gsu_host_irq(int level) { nshvc_cart_irq(level); }
uint8_t gsu_host_open_bus(void) { return nshvc_open_bus(); }
uint32_t gsu_host_ram_size(void) { return ram_size; }
uint8_t gsu_host_rom(uint32_t off, int fetch, int *unheld)
{
    uint8_t v;
    front_rom_unheld = 0;
    v = nshvc_cart_rom(off, NSHVC_ROM_CHIP | (fetch ? FRONT_ROM_FETCH : 0));
    *unheld = front_rom_unheld;
    return v;
}
void gsu_host_fatal(const char *msg) { nshvc_fatal("%s", msg); }
GsuHostCoro *gsu_host_coro_current(void) { return (GsuHostCoro *)nshvc_coro_current(); }
GsuHostCoro *gsu_host_coro_new(void (*fn)(void *), void *arg, size_t stack)
{
    return (GsuHostCoro *)nshvc_coro_new(fn, arg, stack);
}
void gsu_host_coro_switch(GsuHostCoro *from, GsuHostCoro *to)
{
    nshvc_coro_switch((nshvc_coro *)from, (nshvc_coro *)to);
}

/* ---- its programs: the cartridge's routines ---- */
static void run_program(uint8_t bank, uint16_t addr)
{
    const nshvc_routine *r = nshvc_cart_routine((uint32_t)bank << 16 | addr);
    if (!r)
        nshvc_fatal("Super FX program at $%02X:%04X has no C", bank, addr);
    r->abi();
}

static int cache_first(uint8_t bank, uint16_t addr)
{
    const nshvc_routine *r = nshvc_cart_routine((uint32_t)bank << 16 | addr);
    return r && (r->flags & 1);
}

/* ---- the board ---- */
static void power(const nshvc_cart *cart)
{
    ram_size = cart->sram_size;
    snes_gsu_run_hook = run_program;
    snes_gsu_cache_first_hook = cache_first;
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
    gsu_sync();
    *v = reg ? gsu_reg_read((uint16_t)addr) : snes_sfx_ram()[at];
    return 1;
}

static int cpu_write(uint32_t addr, uint8_t v)
{
    uint32_t at;
    int reg;
    if (!decodes(addr, &at, &reg))
        return 0;
    gsu_sync();
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
    if (on)
        held_irq = gsu_irq();
    else
        gsu_irq_put(held_irq);
}

static uint8_t *ram(uint32_t *size)
{
    gsu_sync();
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
    .sync = gsu_sync,
    .hold = hold,
    .ram = ram,
    .busy = gsu_running,
};
