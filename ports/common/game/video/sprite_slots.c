/*
 * sprite_slots.c: eight 8-byte slots at $0595-$05D4, after the work-RAM
 * sprite table. slot_mask has bit i set for the slot at $0595 + 8 i that is
 * in use, and slot_first is the address of the first one in use. A slot's
 * bytes used here: +$00 (its first byte), +$04 to +$06, and +$07, its
 * level, which the fades step.
 */
#include "game.h"
#include "sprite_slots.h"

#define slot_mask       W8(0x05D5)      /* bit i: the slot at $0595 + 8 i is in use */
#define slot_first      W16(0x06D8)     /* the first slot in use, its address; 0 when none */
#define slot_scan       W16(0x1F2B)     /* the scratch word the mask is shifted through */
#define slot1_count     W8(0x08FB)      /* slot 1's countdown; $FF when slot 1 is off */
#define palette_saved   W8(0xFF65)      /* the palette buffer's saved flag; the first countdown sets it */

#define SLOT1           0x059Du         /* slot 1 */
#define SLOT2           0x05A5u         /* slot 2 */

/* The first slot in use. The mask is shifted right through the carry into a
   word, eight times at most, the address stepping by 8 from $0595 for each
   bit passed over. The carry on entry goes into the first shift; after that
   the carry is 0 (a bit passed over was 0, and the step adds no carry). */
uint16_t sprite_slot_first(bool carry)
{
    uint16_t addr = 0;
    int i;

    slot_scan = slot_mask;
    if (slot_mask) {
        addr = 0x0595;
        for (i = 0; i < 8; i++) {
            bool out = (slot_scan & 1) != 0;

            slot_scan = (uint16_t)(((i == 0 && carry) ? 0x8000 : 0) | (slot_scan >> 1));
            if (out)
                break;
            addr = (uint16_t)(addr + 8);
        }
        if (i == 8)
            addr = 0;
    }
    slot_first = addr;
    return addr;
}

/* The same, for callers in other banks. */
uint16_t sprite_slot_first_long(bool carry)
{
    return sprite_slot_first(carry);
}

/* Shows a slot: marks it in use, sets its +$04 to 0, +$05 to 2 and +$06 to
   the given value. */
static void slot_show(uint16_t rec, uint8_t bit, uint8_t value)
{
    slot_mask |= bit;
    W8(rec + 4) = 0x00;
    W8(rec + 5) = 0x02;
    W8(rec + 6) = value;
}

/* Slot 1 counts down. While the count is not $FF it is shown (+$06 = $E7,
   +$00 = $FF) and stepped down. At 0, the first time (the palette flag is
   clear) the flag is set and the slot is shown as before; after that its
   level steps down by 2 each call until 0, and then the count goes on to
   $FF, when the slot is marked unused. The level steps by 2 from $1E. */
uint8_t sprite_slot1_countdown(void)
{
    uint8_t a = (uint8_t)(slot1_count + 1);

    if (!a) {                           /* the count is $FF: slot 1 off */
        a = slot_mask & 0xFD;
        slot_mask = a;
        return a;
    }
    slot_mask |= 0x02;
    W8(SLOT1 + 4) = 0x00;
    W8(SLOT1 + 5) = 0x02;
    W8(SLOT1 + 6) = 0xA7;
    W8(SLOT1 + 0) = (uint8_t)(W8(SLOT1 + 7) | 0xE0);
    if (slot1_count == 0) {
        if (palette_saved) {
            a = W8(SLOT1 + 7);
            if (a == 0) {
                slot1_count--;          /* $00 steps on to $FF */
                return a;
            }
            a = (uint8_t)(a - 2);
            W8(SLOT1 + 7) = a;
            return a;
        }
        palette_saved = 1;
    }
    W8(SLOT1 + 4) = 0x00;
    W8(SLOT1 + 6) = 0xE7;
    a = 0xFF;
    W8(SLOT1 + 0) = a;
    slot1_count--;
    return a;
}

/* Slot 2 is shown and brightened a step: its level goes up to $1F. */
uint8_t sprite_slot2_brighten(void)
{
    uint8_t a;

    slot_show(SLOT2, 0x04, 0x07);
    W8(SLOT2 + 0) = (uint8_t)(W8(SLOT2 + 7) | 0xE0);
    a = W8(SLOT2 + 7);
    if (a != 0x1F) {
        a = (uint8_t)(a + 1);
        W8(SLOT2 + 7) = a;
    }
    return a;
}

/* The same, for callers in other banks. */
uint8_t sprite_slot2_brighten_long(void)
{
    return sprite_slot2_brighten();
}

/* Slot 2 is shown and darkened a step: its level goes down to 0, and then
   the slot is marked unused. */
uint8_t sprite_slot2_darken(void)
{
    uint8_t a;

    slot_show(SLOT2, 0x04, 0x07);
    W8(SLOT2 + 0) = (uint8_t)(W8(SLOT2 + 7) | 0xE0);
    a = W8(SLOT2 + 7);
    if (a) {
        a = (uint8_t)(a - 1);
        W8(SLOT2 + 7) = a;
        return a;
    }
    a = slot_mask & 0xFB;
    slot_mask = a;
    return a;
}

/* The same, for callers in other banks. */
uint8_t sprite_slot2_darken_long(void)
{
    return sprite_slot2_darken();
}
