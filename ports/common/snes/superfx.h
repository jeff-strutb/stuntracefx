/*
 * superfx.h: the Super FX (superfx.c), the cartridge's chip, as the board
 * it sits on sees it: what the chip offers the S-CPU's side, and what it
 * needs from the console it is plugged into (gsu_host_*), which each host
 * gives it: the port's machine (bus.c) or nativeshvc's cartridge connector
 * (ports/stuntracefx/front/superfx_cart.c).
 */
#ifndef PORT_SUPERFX_H
#define PORT_SUPERFX_H

#include <stdint.h>

/* ---- the chip, for the board ---- */
void gsu_reset(void);                         /* power on: its RAM all ones, stopped */
uint8_t gsu_reg_read(uint16_t addr);          /* $3000-$32FF */
void gsu_reg_write(uint16_t addr, uint8_t v);
int gsu_running(void);                        /* SFR's GO */
int gsu_irq(void);                            /* SFR's IRQ flag */
void gsu_irq_put(int on);                     /* put back, the IRQ line with it */
void gsu_hold(int on);                        /* interrupts held: programs end at once; the game's kept */
uint8_t *snes_sfx_ram(void);                  /* its RAM (128 KB; the cartridge's size used) */
/* a program's C at bank:addr (its R15), run to its STOP */
extern void (*snes_gsu_run_hook)(uint8_t bank, uint16_t addr);

/* ---- what the console gives it ---- */
uint64_t gsu_host_clock(void);                /* the console's master clock */
void gsu_host_irq(int level);                 /* the cartridge's IRQ line, now */
void gsu_host_irq_at(int level, uint64_t clock);   /* the same, at a clock of the console's */
/* the cartridge's price for the program at bank:addr (its PBR:R15): a
   load model of the operations the chip sees it make, its duration in
   Super FX clocks = fixed + the weights (in 1/GSU_PRICE_ONE of a clock)
   of its RAM accesses by kind */
#define GSU_PRICE_ONE 4096u
enum { GSU_OP_LOAD, GSU_OP_STORE, GSU_OP_FLUSH, GSU_OP_RPIX, GSU_OPS };
typedef struct {
    uint32_t fixed;
    uint32_t weight[GSU_OPS];
} GsuPrice;
void gsu_host_price(uint8_t bank, uint16_t addr, GsuPrice *out);
uint8_t gsu_host_open_bus(void);              /* the data bus's open value */
uint32_t gsu_host_ram_size(void);             /* the cartridge's RAM (a power of two) */
/* a byte of the cartridge's ROM at file offset off for the Super FX;
   fetch: the ROM buffer's fill, which may never be used: a byte the
   cartridge does not hold reads 0 and sets *unheld */
uint8_t gsu_host_rom(uint32_t off, int fetch, int *unheld);
void gsu_host_fatal(const char *msg);

#endif
