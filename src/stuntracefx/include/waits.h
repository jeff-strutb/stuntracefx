/*
 * waits.h: the game's waits on the hardware and on its own interrupts, as
 * the console's waits (behavior-exact: the main CPU's work takes no time,
 * and time passes only where the original waited).
 *
 * Each original wait polled a byte or word until it changed; here the
 * console runs (lines, DMA, HDMA, the NMI and IRQs, the Super FX, the sound
 * CPU) until the same condition holds, then the caller makes the poll's
 * last read itself, so the registers end as the original's exit read left
 * them.
 */
#ifndef SRFX_WAITS_H
#define SRFX_WAITS_H

#include <stdint.h>
#include "machine.h"
#include "remaster.h"

/* the console's general wait: runs until ready(ctx) holds; returns the
   clocks waited (nativeshvc contract 2.0) */
uint64_t nshvc_wait_until(int (*ready)(void *ctx), void *ctx);
/* the console's master clock (cart.h) */
uint64_t nshvc_cart_clock(void);

typedef struct {
    uint32_t addr;
    uint16_t mask, value;
    uint8_t wide, equal;
} WaitMem;

static int wait_mem_ready(void *ctx)
{
    const WaitMem *w = (const WaitMem *)ctx;
    uint16_t v = w->wide ? rd16(w->addr) : rd8(w->addr);

    return ((v & w->mask) == w->value) == w->equal;
}

/* until (the byte, or word, at addr & mask) == value (equal) or != value */
static inline uint64_t wait_mem(uint32_t addr, int wide, uint16_t mask, uint16_t value, int equal)
{
    WaitMem w = {addr, mask, value, (uint8_t)wide, (uint8_t)equal};

    if (rm_time_still() && (addr & 0xFFFF) == 0x4212)  /* the H-blank's status: time alone changes it */
        return 0;
    rm_wait_point();
    return nshvc_wait_until(wait_mem_ready, &w);
}

static int wait_clock_ready(void *ctx)
{
    return nshvc_cart_clock() >= *(const uint64_t *)ctx;
}

/* a fixed span of console time, `clocks` master clocks: where the original
   waited a set time for the hardware (the sound CPU settling) and polled
   nothing */
static inline void wait_clocks(uint64_t clocks)
{
    uint64_t until;

    if (rm_time_still())
        return;
    rm_wait_point();
    until = nshvc_cart_clock() + clocks;
    nshvc_wait_until(wait_clock_ready, &until);
}

/* until the byte at addr is not 0 / is 0 */
static inline void wait_nonzero8(uint32_t addr) { wait_mem(addr, 0, 0xFF, 0, 0); }
static inline void wait_zero8(uint32_t addr) { wait_mem(addr, 0, 0xFF, 0, 1); }
/* the same for a word */
static inline void wait_nonzero16(uint32_t addr) { wait_mem(addr, 1, 0xFFFF, 0, 0); }
static inline void wait_zero16(uint32_t addr) { wait_mem(addr, 1, 0xFFFF, 0, 1); }

#endif
