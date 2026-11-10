/*
 * object_tables.c: where the objects' tables are in the cartridge (bank and
 * address, the original's), each written once here, and their reads. Every
 * read is the original's: the address sum kept to 16 bits, a word
 * little-endian.
 */
#include "data/cart.h"
#include "data/object_tables.h"

enum {
    SCRIPT_BANK_START = 0x8000,         /* a script bank's first entry: where a restart goes */
    MODEL_BANK = 0x00,                  /* the model records' bank */
    MODEL_OFFSETS = 0x0A                /* a model record's offsets x, y, z */
};

CartRef course_script_next(CartRef entry, unsigned bytes)
{
    entry.addr = (uint16_t)(entry.addr + bytes);
    return entry;
}

CartRef course_script_restart(CartRef entry)
{
    entry.addr = SCRIPT_BANK_START;
    return entry;
}

uint8_t course_script_byte(CartRef entry, unsigned field) { return cart8(entry.bank, (uint16_t)(entry.addr + field)); }
uint16_t course_script_word(CartRef entry, unsigned field) { return cart16(entry.bank, (uint16_t)(entry.addr + field)); }

uint16_t model_record_offset(uint16_t record, int axis)
{
    return cart16(MODEL_BANK, (uint16_t)(record + MODEL_OFFSETS + 2 * axis));
}
