/*
 * model.h: the bus the decomp's routines run on in the port's checks.
 *
 * The decomp's C (its symbols renamed old_*) reads and writes the S-CPU's
 * and the Super FX's buses; here those reach the port's own state (state.h:
 * work RAM, the game pak RAM), the ROM, and the few registers the routines
 * under check use (the multipliers and the divider). Time is ignored. A
 * register the model lacks stops the check.
 */
#ifndef SRFX_CHECK_MODEL_H
#define SRFX_CHECK_MODEL_H

#include <stdint.h>

extern const char *model_current;          /* the routine running, for errors */
void model_load_rom(const char *path);
/* The I/O writes the old code made, in order (cleared by model_reset). */
extern int model_io_n;
extern uint32_t model_io[4096];
void model_reset(void);
#endif
