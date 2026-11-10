/*
 * menu_tables.c: where the menus' tables are in the cartridge image (each
 * written once, as the image's offset, as in assets.csv), read as what they
 * hold.
 */
#include "data/cart.h"
#include "data/menu_tables.h"

enum {
    SINE = 0x0016FE,                    /* bytes, by angle */
    SELECTION_ROWS = 0x01B7B1,          /* bytes, by choice */
    SELECTION_MODELS = 0x01B7B5,        /* bytes, 4 a choice, by setting row */
    CAR_SETTINGS = 0x05B00C,            /* words, by choice: where its rows start among CAR_SETTING_ROWS */
    CAR_SETTING_ROWS = 0x05B014,        /* bytes, 3 a combination */
    SAVED_CHOICES = 0x05B9BA,           /* words, by saved value */
    SETTING_OBJECTS = 0x05C15E,         /* words, by setting */
    SETTING_ANIMATIONS = 0x05C1B0,      /* words, 4 a setting, by row */
    INITIAL_TILES = 0x05C528,           /* words, by letter */
    CHOICE_REPEAT = 0x05C758,           /* words, by choice */
    CHOICE_LEFT = 0x05C81F,             /* words, by choice */
    CHOICE_RIGHT = 0x05C827,            /* words, by choice */
    PAIR_ANIMATIONS = 0x05C862,         /* two words a pair */
    BUTTON_SEQUENCE = 0x05D66D          /* words, by step */
};

static uint8_t byte_at(uint32_t at) { return cart[at & (CART_SIZE - 1)]; }
static uint16_t word_at(uint32_t at) { return (uint16_t)(byte_at(at) | byte_at(at + 1) << 8); }

int8_t menu_sine(uint8_t angle) { return (int8_t)byte_at(SINE + angle); }
uint16_t menu_button_sequence(int step) { return word_at(BUTTON_SEQUENCE + (uint32_t)(uint16_t)(step << 1)); }

void menu_pair_animations(int pair, uint16_t *first, uint16_t *second)
{
    *first = word_at(PAIR_ANIMATIONS + (uint32_t)(pair << 2));
    *second = word_at(PAIR_ANIMATIONS + (uint32_t)(pair << 2) + 2);
}

uint16_t menu_choice_repeat_model(int choice) { return word_at(CHOICE_REPEAT + (uint32_t)choice * 2); }
uint16_t menu_choice_step(int right, int choice) { return word_at((right ? CHOICE_RIGHT : CHOICE_LEFT) + (uint32_t)choice * 2); }
uint16_t menu_saved_choice(int saved) { return word_at(SAVED_CHOICES + (uint32_t)(saved & 3) * 2); }
uint16_t menu_initial_tile(int letter) { return word_at(INITIAL_TILES + (uint32_t)letter * 2); }

uint8_t menu_car_setting_row(int choice, int combination, int k)
{
    uint16_t first = word_at(CAR_SETTINGS + (uint32_t)choice * 2);

    return byte_at(CAR_SETTING_ROWS + (uint16_t)(first + combination * 3 + k));
}

uint8_t menu_selection_row(int choice) { return byte_at(SELECTION_ROWS + (uint32_t)choice); }
uint8_t menu_selection_model(int choice, int row) { return byte_at(SELECTION_MODELS + (uint32_t)(choice * 4 + row)); }
uint16_t menu_setting_animation(int setting, int row) { return word_at(SETTING_ANIMATIONS + (uint32_t)(setting * 8 + row * 2)); }
uint16_t menu_setting_object(int i) { return word_at(SETTING_OBJECTS + (uint32_t)i * 2); }
