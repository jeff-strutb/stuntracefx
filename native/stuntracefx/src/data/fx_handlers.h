/*
 * fx_handlers.h: the words a Super FX record keeps for its handler (+$0A),
 * by FxHandler (plan section 3). The words are the chip's routines' places
 * in the cartridge, written once, in fx_handlers.c.
 */
#ifndef STUNTRACEFX_FX_HANDLERS_H
#define STUNTRACEFX_FX_HANDLERS_H

#include <stdint.h>

#include "fx/fx.h"

uint16_t fx_handler_word(FxHandler handler);    /* the word a record keeps for it; 0 for none or unknown */
FxHandler fx_handler_of(uint16_t word);         /* FX_HANDLER_UNKNOWN for a word none has */

#endif
