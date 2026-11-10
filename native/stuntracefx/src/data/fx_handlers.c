/*
 * fx_handlers.c: where each Super FX record handler is in the chip's
 * program (bank $04's code), the word a record keeps for it.
 */
#include "data/fx_handlers.h"

static const uint16_t WORDS[FX_HANDLER_UNKNOWN] = {
    [FX_HANDLER_NONE] = 0x0000,
    [FX_HANDLER_BOUNCE] = 0x907A,
    [FX_HANDLER_MARK_TOUCHED] = 0x90E8,
    [FX_HANDLER_FALL] = 0x910A,
    [FX_HANDLER_COLLIDE_ONLY] = 0x916D,
    [FX_HANDLER_DROP] = 0x9185,
    [FX_HANDLER_CAR] = 0xB1A1,
};

uint16_t fx_handler_word(FxHandler handler)
{
    return handler < FX_HANDLER_UNKNOWN ? WORDS[handler] : 0;
}

FxHandler fx_handler_of(uint16_t word)
{
    for (int h = 0; h < FX_HANDLER_UNKNOWN; h++)
        if (WORDS[h] == word)
            return (FxHandler)h;
    return FX_HANDLER_UNKNOWN;
}
