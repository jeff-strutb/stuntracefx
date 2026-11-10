/*
 * ref.h: a reference from the cartridge's data to more of its data (plan
 * section 3, Data): where a script, a model or a list is, as the data
 * itself names it. Game code takes one from data/ and passes it on; data/
 * reads what it names.
 */
#ifndef STUNTRACEFX_DATA_REF_H
#define STUNTRACEFX_DATA_REF_H

#include <stdint.h>

typedef struct { uint8_t bank; uint16_t addr; } CartRef;

/* the reference in the cartridge's own form, for what takes it in that
   form (the Super FX's hand-offs) and for messages */
static inline uint8_t cart_ref_bank(CartRef r) { return r.bank; }
static inline uint16_t cart_ref_address(CartRef r) { return r.addr; }
/* and back from that form, as a program's state keeps it (the Super FX's RAM) */
static inline CartRef cart_ref(uint8_t bank, uint16_t address)
{
    CartRef r = {bank, address};

    return r;
}

#endif
