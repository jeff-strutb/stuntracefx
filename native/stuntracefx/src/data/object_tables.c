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

typedef struct { uint8_t bank; uint16_t at; } Place;

static const Place
    OBJECT_DATA = {0x09, 0x8000}, OBJECT_LISTS = {0x09, 0x8090},
    FACING_FRAMES_32 = {0x08, 0xE8AF}, FACING_FRAMES_64 = {0x08, 0xC056},
    JUMP_RISE = {0x08, 0xB32B},
    EYE_BLINKS[] = {{0x08, 0xE57F}, {0x08, 0xE5AD}, {0x08, 0xE5DB}, {0x08, 0xE609}}, /* by EyeBlink */
    PATHS = {0x08, 0x85B9},
    CODE_BLOCK = {0x02, 0x8000};        /* the block of code and data the original runs from work RAM */

/* where the block runs, the places its data names being from there; the
   trigger areas' tables in it */
enum {
    CODE_BLOCK_RUN = 0xA2D9,
    TRIGGER_AREAS_BY_HEIGHT = 0xC61D,   /* a word for each step of the view's height: its area */
    TRIGGER_AREA_SPLIT = 0xC661,
    TRIGGER_AREA_SINGLE = 0xC696
};

static uint8_t byte_at(Place p, unsigned offset) { return cart8(p.bank, (uint16_t)(p.at + offset)); }
static uint16_t word_at(Place p, unsigned offset) { return cart16(p.bank, (uint16_t)(p.at + offset)); }
static const uint8_t *code_block(uint16_t at) { return cart_at(CODE_BLOCK.bank, (uint16_t)(CODE_BLOCK.at + (uint16_t)(at - CODE_BLOCK_RUN))); }

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

uint16_t object_entry_word(uint16_t entry, unsigned field) { return word_at(OBJECT_DATA, (unsigned)(uint16_t)(entry + field)); }
uint8_t object_list_byte(uint16_t at) { return byte_at(OBJECT_LISTS, at); }
uint16_t object_list_word(uint16_t at) { return word_at(OBJECT_LISTS, at); }

uint8_t facing_frame_32(uint8_t step) { return byte_at(FACING_FRAMES_32, step); }
uint8_t facing_frame_64(uint8_t step) { return byte_at(FACING_FRAMES_64, step); }
uint8_t jump_rise(uint16_t timer) { return byte_at(JUMP_RISE, timer); }
uint16_t eye_blink_look(EyeBlink blink, uint8_t step) { return word_at(EYE_BLINKS[blink], (uint8_t)(step << 1)); }

uint8_t path_byte(uint16_t at) { return byte_at(PATHS, at); }
uint16_t path_word(uint16_t at) { return word_at(PATHS, at); }

uint16_t track_table_last(CartRef table) { return cart16(table.bank, table.addr); }

void track_table_place(CartRef table, uint16_t entry, uint16_t out[3])
{
    uint16_t at = (uint16_t)((uint16_t)((uint16_t)(entry - 1) << 3) + table.addr + 2);

    for (int i = 0; i < 3; i++)
        out[i] = cart16(table.bank, (uint16_t)(at + 2 * i));
}

TriggerAreaRef trigger_area_for_height(uint16_t at)
{
    const uint8_t *e = code_block((uint16_t)(TRIGGER_AREAS_BY_HEIGHT + at));

    return (TriggerAreaRef)(e[0] | e[1] << 8);
}

TriggerAreaRef trigger_area_fixed(TriggerFixed which)
{
    return which == TRIGGER_FIXED_SPLIT ? TRIGGER_AREA_SPLIT : TRIGGER_AREA_SINGLE;
}

TriggerArea trigger_area(TriggerAreaRef area)
{
    const uint8_t *p = code_block(area);
    TriggerArea a = {p[0], p[1], p[2], p[3], p + 4};

    return a;
}
