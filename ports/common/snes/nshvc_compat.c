/*
 * nshvc_compat.c: nativeshvc's names for the calls the game's remaster
 * features make (game/include/console_ext.h), answered by this interim
 * console until the game runs on nativeshvc.
 */
#include "snes.h"
#include "host.h"

void nshvc_set_untimed(int on) { snes_set_untimed(on); }
int nshvc_untimed(void) { return snes_untimed(); }
void nshvc_wait_vblank(void) { snes_wait_vblank(); }
const char *nshvc_save_dir(void) { return host_save_dir(); }
uint8_t nshvc_peek(uint32_t addr) { return snes_peek(addr); }
