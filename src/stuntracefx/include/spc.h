/*
 * spc.h: the SPC700 as the sound driver's C sees it.
 *
 * The C of an SPC700 routine works on `spc` (A, X, Y, SP and the PSW flags)
 * and makes every APU RAM and I/O access through spc_rd/spc_wr, in the
 * order and number the assembly makes them: the checks (tools/stuntracefx/
 * sfxbox, spccall.c) compare the accesses, and the hardware's reads (DSP
 * $F3, ports $F4-$F7, timers $FD-$FF) come back with the values the original
 * read, in order. The stack is page 1; direct page is page 0 or 1 by P.
 *
 * A routine is entered by CALL, PCALL or TCALL: the caller pushes the return
 * address (spc_call), the routine's RET pulls it (spc_ret). abi_Name(void)
 * is the routine at its entry, as the checks call it.
 */
#ifndef STUNTRACEFX_SPC_H
#define STUNTRACEFX_SPC_H

#include <stdint.h>

typedef struct {
    uint8_t a, x, y, sp;
    uint8_t n, v, p, b, h, i, z, c;   /* PSW flags, each 0 or 1 */
} SpcRegs;

extern SpcRegs spc;

uint8_t spc_rd(uint16_t addr);
void spc_wr(uint16_t addr, uint8_t v);

/* The sound CPU's waits (nativeshvc contract 2.0): its reads, writes and
   work take no time; time passes only here, the console (and the S-CPU)
   running until ready(ctx) holds. Returns the SPC700 cycles waited. */
uint32_t spc_wait_until(int (*ready)(void *ctx), void *ctx);

/* spc_wr with its time stated after its arguments are evaluated (a value that
   reads: spc_wr_cyc(a, f(spc_rd(b)), n) states the read's time inside the
   argument and the write's here). It is spc_wr with the time after the
   arguments, which spc_wr itself, a primitive the checks provide, cannot take. */
static inline void spc_wr_cyc(uint16_t addr, uint8_t v)
{
    spc_wr(addr, v);
}

static inline uint16_t spc_dp(uint8_t d) { return (uint16_t)(spc.p ? 0x0100 | d : d); }
static inline uint16_t spc_ya(void) { return (uint16_t)(spc.y << 8 | spc.a); }
static inline void spc_set_ya(uint16_t v) { spc.a = (uint8_t)v; spc.y = (uint8_t)(v >> 8); }

static inline void spc_push(uint8_t v) { spc_wr((uint16_t)(0x0100 | spc.sp), v); spc.sp--; }
static inline uint8_t spc_pull(void) { spc.sp++; return spc_rd((uint16_t)(0x0100 | spc.sp)); }

static inline uint8_t spc_psw(void)
{
    return (uint8_t)(spc.n << 7 | spc.v << 6 | spc.p << 5 | spc.b << 4 | spc.h << 3 | spc.i << 2 | spc.z << 1 | spc.c);
}

static inline void spc_set_psw(uint8_t v)
{
    spc.n = v >> 7 & 1; spc.v = v >> 6 & 1; spc.p = v >> 5 & 1; spc.b = v >> 4 & 1;
    spc.h = v >> 3 & 1; spc.i = v >> 2 & 1; spc.z = v >> 1 & 1; spc.c = v & 1;
}

/* CALL, with the cycles before the first push (the opcode and operand
   fetches and the idle cycle, with the push's own), before the second, and
   after it (the idle cycles up to the routine's first fetch): the return
   address pushed (high byte first), then the routine. */
static inline void spc_call(void (*abi)(void), uint16_t ret)
{
    spc_push((uint8_t)(ret >> 8));
    spc_push((uint8_t)ret);
    abi();
}

/* The driver jumps to `at`, code that never comes back to what called it
   (the reload loop at $0675, which ends by restarting the driver): every
   C frame of the calls under it is gone, and `entry`, the C of the code at
   `at`, runs from there. Write the address as a literal (0x0675); the
   checks end the call there and take `entry` on its own. */
void spc_restart(uint16_t at, void (*entry)(void));

/* Which driver's code is in the SPC700's RAM at $10B0-$260E: 1 once the
   battle driver's block list ($1A:F091) is uploaded over the race
   driver's, 0 once a block of any other list covers $10B0 again (apu_upload.c sets it as the blocks go out). The race
   driver's CALL $10B0 (SpcReset) reaches whichever is there. */
extern int spc_battle_resident;

/* JMP $FFC0: the console's boot ROM (the SPC700's own, not the game's): it
   takes the next upload from the S-CPU by its transfer protocol and runs
   it. Never returns. */
void spc_boot_rom(void);

/* RET, with the cycles before the first pull (the opcode fetch, the dummy
   read and the idle cycle included) and before the second: the return address
   pulled (low byte first); the checks end the call at it. */
static inline uint16_t spc_ret(void)
{
    uint16_t lo = spc_pull();
    return (uint16_t)(lo | spc_pull() << 8);
}

#endif
