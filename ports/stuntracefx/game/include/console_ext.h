/*
 * console_ext.h: the console's calls beyond the machine model that the
 * game's remaster features use (nativeshvc's nshvc.h declares them the
 * same way; this repo's interim console answers them too).
 */
#ifndef SRFX_CONSOLE_EXT_H
#define SRFX_CONSOLE_EXT_H

#include <stdint.h>

/* Untimed: the S-CPU's code (not its interrupt handlers), its DMA and the
   Super FX keep no time, as native code; a Super FX program runs to its
   STOP. Off is the console's own time. */
void nshvc_set_untimed(int on);
int nshvc_untimed(void);
/* the clock run on, timed, to the next vertical blank's start */
void nshvc_wait_vblank(void);
/* the host's folder for saves */
const char *nshvc_save_dir(void);
/* a byte of the address space as it stands, with no time and no side
   effect (between frames) */
uint8_t nshvc_peek(uint32_t addr);

#endif
