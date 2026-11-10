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
    MODEL_TOUCH = 0x0A                  /* a model record's touch box x, y, z */
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

uint16_t model_record_touch(uint16_t record, int axis)
{
    return cart16(MODEL_BANK, (uint16_t)(record + MODEL_TOUCH + 2 * axis));
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

/* the model table (bank $00, MODEL_BANK) and each model's number in it */
enum { MODEL_TABLE = 0xA040, MODEL_SIZE = 28 };
static const uint8_t MODEL_NUMBERS[] = {
    [MODEL_NONE] = 0,
    [MODEL_FIREWORK_SPARK] = 5,
    [MODEL_LAMP_POST_LIGHT] = 6,
    [MODEL_LAMP_TOWER] = 7,
    [MODEL_LAMP_TOWER_LIGHT] = 8,
    [MODEL_COMMON] = 10,
    [MODEL_BREAK_OFF_PIECE] = 13,
    [MODEL_TRUCK_WHEEL] = 15,
    [MODEL_BURST_PIECE] = 52,
    [MODEL_PLAYER_AT_GO] = 65,
    [MODEL_SIGNAL_LIGHT_COUNTING] = 67,
    [MODEL_SIGNAL_LIGHT] = 68,
    [MODEL_BLINKER] = 81,
    [MODEL_BALL] = 91,
    [MODEL_BREAKABLE_PIECE] = 107,
    [MODEL_BREAKABLE_PIECE_2] = 108,
    [MODEL_FAN_LIT] = 135,
    [MODEL_WHEEL_LIT] = 136,
    [MODEL_SHARDS] = 149,
    [MODEL_SHARDS_2] = 155,
    [MODEL_RAIL_RISING] = 170,
    [MODEL_RAIL_DROPPING] = 171,
    [MODEL_FLOATER] = 178,
    [MODEL_FINALE_BREAKABLE] = 188,
    [MODEL_BONUS_5] = 199,
    [MODEL_BONUS_2] = 203,
    [MODEL_FIREWORK_SHELL] = 207,
    [MODEL_SCORE_COUNTER_ON] = 211,
    [MODEL_CROWD] = 212,
    [MODEL_RAISE_BALL] = 214,
    [MODEL_LOWER_BALL] = 215,
    [MODEL_POP_MARKER] = 217,
};
/* CarCrashParts' sets (08:A5E4, 08:A88B, 08:A33D, 08:A096) and the ring's parts */
static const uint8_t CRASH_PARTS[4][6] = {{31, 26, 27, 28, 29, 30}, {43, 38, 39, 40, 41, 42},
                                          {37, 36, 32, 33, 34, 35}, {78, 73, 74, 75, 76, 77}};
static const uint8_t RING_PARTS[4] = {56, 60, 57, 51};

static uint16_t model_number_word(unsigned n) { return (uint16_t)(MODEL_TABLE + n * MODEL_SIZE); }
uint16_t model_word(ObjectModel model) { return model_number_word(MODEL_NUMBERS[model]); }
uint16_t model_word_after(ObjectModel first, int k) { return (uint16_t)(model_word(first) + k * MODEL_SIZE); }
uint16_t crash_part_model_word(int set, int part) { return model_number_word(CRASH_PARTS[set][part]); }
uint16_t ring_part_model_word(int part) { return model_number_word(RING_PARTS[part]); }
int model_word_in_cartridge(uint16_t word) { return word >= 0x8000; }   /* the ROM's half of a bank */

/* the looks' lists (bank $08) */
static const uint16_t LOOK_WORDS[] = {
    [LOOK_CAR_F_TYPE] = 0x8F91,
    [LOOK_CAR_COUPE] = 0x9163,
    [LOOK_CAR_4WD] = 0x9335,
    [LOOK_CAR_2WD] = 0x9507,
    [LOOK_FIREWORK_SPARK] = 0x996B,
    [LOOK_LANTERN] = 0x996D,
    [LOOK_LAMP_LIGHT] = 0x9971,
    [LOOK_LAMP_LIGHT_RAISED] = 0x9973,
    [LOOK_STAR_SPARKLE] = 0x997C,
    [LOOK_BREAK_OFF_A] = 0x998D,
    [LOOK_SPARKLE] = 0x9998,
    [LOOK_EYE_PLAIN] = 0x9AAF,
    [LOOK_EYE_STARTLED] = 0x9AB1,
    [LOOK_BREAK_OFF_H] = 0x9B40,
    [LOOK_BREAK_OFF_G] = 0x9B56,
    [LOOK_BREAK_OFF_E_FLAT] = 0x9B63,
    [LOOK_BREAK_OFF_E] = 0x9B79,
    [LOOK_BREAK_OFF_D] = 0x9B8F,
    [LOOK_BREAK_OFF_B] = 0x9B96,
    [LOOK_PUFF] = 0x9BB4,
    [LOOK_WHEEL] = 0x9BBF,
};
uint16_t look_word(ObjectLook look) { return LOOK_WORDS[look]; }
uint16_t look_word_split(uint16_t look) { return (uint16_t)(look + 2); }
uint16_t look_word_tilted(uint16_t look, int8_t tilt) { return (uint16_t)(tilt + 8 + look); }

/* the object scripts' records (bank $00) and the car scripts (bank $09) */
static const uint16_t OBJECT_SCRIPT_WORDS[] = {
    [OBJECT_SCRIPT_SIGNAL] = 0xC6DC,
    [OBJECT_SCRIPT_BLINKER] = 0xD354,
    [OBJECT_SCRIPT_MARKER] = 0xECD0,
};
static const uint16_t CAR_SCRIPT_WORDS[] = {
    [CAR_F_TYPE] = 0x81B9,
    [CAR_COUPE] = 0x853D,
    [CAR_4WD] = 0x8411,
    [CAR_2WD] = 0x82E5,
    [CAR_RIVAL_F_TYPE] = 0x876E,
    [CAR_RIVAL_COUPE] = 0x8AA8,
    [CAR_RIVAL_4WD] = 0x8990,
    [CAR_RIVAL_2WD] = 0x8886,
    [CAR_BONUS_TRUCK] = 0x8669,
};
uint16_t object_script_word(ObjectScript script) { return OBJECT_SCRIPT_WORDS[script]; }
uint16_t car_script_word(CarScript script) { return CAR_SCRIPT_WORDS[script]; }
