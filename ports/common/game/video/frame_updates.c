/*
 * frame_updates.c: the updates the game asks for during a frame and has
 * applied when it ends: the sprite table, the palette, transfers of
 * graphics to the store (a queue of up to 64), settings written as data
 * (a list of up to 32 byte writes), and two lists of words the frame code
 * collects (the players' new presses).
 */
#include <stdlib.h>
#include "game.h"
#include "video.h"

#define frame_updates       W8(0x11D7)      /* bit 0 the byte writes, 1 the transfers, 2 the sprites, 3 the palette */
#define byte_writes_size    W16(0x162D)
#define byte_writes         0x162Fu         /* entries: address, value (words) */
#define transfers_size      W16(0x16AF)
#define transfers           0x16B9u         /* entries: source address, bank, word in the store, bytes */
#define transfer_staged     0x16B1u         /* bytes, source address, bank (word), word in the store */

/* a byte the S-CPU reads in bank `bank` at `at` (work RAM, the game pak RAM
   or the ROM) */
static uint8_t bank_byte(uint8_t bank, uint16_t at)
{
    if (bank == 0x7E || bank == 0x7F)
        return wram[(uint32_t)(bank - 0x7E) << 16 | at];
    if (bank == 0x70)
        return pakram[at];
    if ((bank & 0x7F) < 0x40 && at < 0x2000)
        return wram[at];
    return rom_at((uint32_t)bank << 16 | at)[0];
}

/* OamDmaIfPending */
void frame_sprites_if_pending(void)
{
    if (!(frame_updates & 0x04))
        return;
    video_sprites_all();
    frame_updates &= (uint8_t)~0x04;
}

/* CgramDmaIfPending */
void frame_colours_if_pending(void)
{
    if (!(frame_updates & 0x08))
        return;
    video_colours_all();
    frame_updates &= (uint8_t)~0x08;
}

/* a byte the game queued for its address in bank `bank`: a setting of
   the screen read as such, else the RAM there */
static void frame_byte(uint8_t bank, uint16_t at, uint8_t v)
{
    if ((bank & 0x7F) < 0x40 && at >= 0x2100 && at < 0x2134)
        video_register((uint8_t)(at - 0x2100), v);
    else if ((bank & 0x7F) < 0x40 && at == 0x420C)
        video_line_tables_on(v);
    else if (bank == 0x7E || bank == 0x7F)
        wram[(uint32_t)(bank - 0x7E) << 16 | at] = v;
    else if ((bank & 0x7F) < 0x40 && at < 0x2000)
        wram[at] = v;
    else if (bank == 0x70)
        pakram[at] = v;
    else
        abort();                        /* (the game queues none elsewhere) */
}

/* ByteWritesIfPending: the queued byte writes, the last first, to their
   addresses in bank `bank`; then the list emptied. */
void frame_byte_writes_if_pending(uint8_t bank)
{
    int16_t x;

    if (!(frame_updates & 0x01))
        return;
    for (x = (int16_t)(byte_writes_size - 4); x >= 0; x = (int16_t)(x - 4))
        frame_byte(bank, W16(byte_writes + x), W8(byte_writes + 2 + x));
    byte_writes_size = 0;
    frame_updates &= (uint8_t)~0x01;
}

/* FrameByteWritePush: a byte write queued (a full list stops the game) */
void frame_byte_write_push(uint16_t at, uint8_t v)
{
    uint16_t x = byte_writes_size;

    if (x == 0x0080)
        abort();
    W16(byte_writes + 2 + x) = v;
    W16(byte_writes + x) = at;
    byte_writes_size = (uint16_t)(x + 4);
}

/* VramDmaQueueAdd: the transfer staged at $16B1 queued, unless the queue
   is full. */
void frame_transfer_queue(void)
{
    uint16_t x = transfers_size;

    if (x == 0x0200)
        return;
    W16(transfers + x) = W16(transfer_staged + 2);
    W8(transfers + 2 + x) = W8(transfer_staged + 4);
    W16(transfers + 3 + x) = W16(transfer_staged + 6);
    W16(transfers + 5 + x) = W16(transfer_staged);
    transfers_size = (uint16_t)(x + 8);
}

/* VramDmaQueueFlush: the queued transfers, the last first; then the
   queue emptied. */
void frame_transfers_if_pending(void)
{
    int16_t x;

    if (!(frame_updates & 0x02))
        return;
    for (x = (int16_t)(transfers_size - 8); x >= 0; x = (int16_t)(x - 8)) {
        uint16_t at = W16(transfers + x), word = W16(transfers + 3 + x);
        uint8_t bank = W8(transfers + 2 + x), b[2];
        unsigned n = W16(transfers + 5 + x) ? W16(transfers + 5 + x) : 0x10000u;

        for (; n >= 2; n -= 2, at = (uint16_t)(at + 2)) {
            b[0] = bank_byte(bank, at), b[1] = bank_byte(bank, (uint16_t)(at + 1));
            video_store_write(word++, b, 2);
        }
        if (n) {
            b[0] = bank_byte(bank, at);
            video_store_write(word, b, 1);
        }
    }
    transfers_size = 0;
    frame_updates &= (uint8_t)~0x02;
}

/* AnimTilesQueueNext: the next of the eight $400-byte frames of the
   animated tiles ($7F:A7D7, the frame at $18F7) queued for word $2000. */
void frame_anim_tiles_next(void)
{
    uint16_t at = (uint16_t)((W16(0x18F7) + 0x0400) & 0x1FFF);

    W16(0x18F7) = at;
    W16(transfer_staged + 2) = (uint16_t)(at + 0xA7D7);
    W16(transfer_staged) = 0x0400;
    W16(transfer_staged + 4) = 0x007F;
    W16(transfer_staged + 6) = 0x2000;
    frame_transfer_queue();
    frame_updates |= 0x02;
}

/* WordListAAppend, WordListBAppend: a word on list A ($1195) or B ($11B5),
   eight at most */
void frame_word_list_append(int b, uint16_t v)
{
    uint16_t x = W16(b ? 0x1193 : 0x1191);

    if (x == 0x0010)
        return;
    W16((b ? 0x11B5 : 0x1195) + x) = v;
    W16(b ? 0x1193 : 0x1191) = (uint16_t)(x + 2);
}

/* FrameInputAndCountEnd: a menu frame's end: $18BF-$18C2 kept at
   $18BB-$18BE, the players' new presses ($0311, $0313) on the two lists,
   the frame counters ($0E3B, $0306, direct page $06) stepped, and the
   brightness from $11DA. */
void frame_menu_end(void)
{
    W16(0x18BB) = W16(0x18BF);
    W16(0x18BD) = W16(0x18C1);
    frame_word_list_append(0, W16(0x0311));
    frame_word_list_append(1, W16(0x0313));
    W8(0x0E3B)++;
    W8(0x0306)++;
    W8(0x0006)++;
    video_register(0x00, W8(0x11DA));
}
