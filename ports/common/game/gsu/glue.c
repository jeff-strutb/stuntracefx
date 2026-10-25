/*
 * glue.c: the S-CPU's side of the Super FX: starting a program and taking
 * its results, the renderer's parameter block, the records it reads, and
 * which of the two screen buffers it draws into.
 *
 * The original hands the Super FX the game pak's ROM and RAM, starts it
 * and polls its status until it stops (counting the polls in $FE-$FF and
 * $1F21-$1F22 for a debugging panel); here a program simply runs to its
 * end. The S-CPU's work the original overlapped with a running program
 * touches only work RAM, so running it before or after is the same.
 */
#include "game.h"
#include "render.h"
#include "glue.h"

#define gsu_scmr_shadow     W8(0x0374)      /* the screen mode the S-CPU keeps */
#define gsu_buffer_select   W16(0x0D7F)     /* the buffer drawn into: 0 or 1 */
#define gsu_buffer_offset   W16(0x0D7D)     /* 8 for buffer 0, else 0 */
#define gsu_records_count   W16(0x0CEE)
#define gsu_records_left    W16(0x1F2B)

/* The screen the Super FX draws into, from a screen mode value. */
static void screen_mode(uint8_t scmr)
{
    fx_screen.mode = scmr & 3;
    fx_screen.height = (uint8_t)((scmr & 0x20 ? 2 : 0) | (scmr & 0x04 ? 1 : 0));
}

/* GsuRun: the program at `bank`:`start` run to its end, the screen mode
   the S-CPU keeps in force. */
void gsu_run(uint8_t bank, uint16_t start)
{
    screen_mode(gsu_scmr_shadow);
    fx_run(bank, start);
}

/* GsuParamsUpdate: the view the renderer needs, into its parameter block. */
void gsu_params_update(void)
{
    FXU(0x108A) = W16(0x0D12);
    FXU(0x00D8) = W16(0x00C5);
    FXU(0x00DA) = W16(0x00C7);
    FXU(0x00DC) = W16(0x00C9);
    FXU(0x00DE) = W16(0x0664);
    FXU(0x00E0) = W16(0x0666);
    FXU(0x00E2) = W16(0x0668);
    FXU(0x2736) = W16(0x05FA);
}

/* GsuParamsFromDp: $05EF marked, and eight direct page words into the
   renderer's inputs at $0034-$003E, $0092 and $0094. */
void gsu_params_from_dp(void)
{
    static const uint8_t from[8] = {0xCE, 0xD0, 0x2A, 0x2E, 0x2C, 0x30, 0x32, 0x34};
    static const uint16_t to[8] = {0x0034, 0x0036, 0x0038, 0x003A, 0x003C, 0x003E, 0x0092, 0x0094};
    int i;

    W8(0x05EF) = 0xFF;
    for (i = 0; i < 8; i++)
        FXU(to[i]) = W16(from[i]);
}

/* GsuParam216A/B: the renderer input at $0216 from entry `n` of the word
   table at `table` (in ROM bank 7: $8E3E or $8E52). */
void gsu_param_216(uint16_t table, uint16_t n)
{
    FXU(0x0216) = rom_word(0x070000u | (uint16_t)(table + 2 * n));
}

/* The segment table: its bank the byte at $1094, its place the word at
   $1092. GsuTableEntryTo1F4C: entry `n`'s three words (from 2 bytes in)
   into $1F4C-$1F51. */
void gsu_table_entry_to_1f4c(uint16_t n)
{
    uint8_t bank = pakram[0x1094];
    uint16_t at = (uint16_t)(FXU(0x1092) + (uint16_t)((n - 1) << 3) + 2);
    int i;

    for (i = 0; i < 3; i++)
        W16(0x1F4C + 2 * i) = rom_word((uint32_t)bank << 16 | (uint16_t)(at + 2 * i));
}

/* GsuTableEntryNext: the entry after `n` (1 after the last, the table's
   first word). $1F4C keeps `n`. */
uint16_t gsu_table_entry_next(uint16_t n)
{
    W16(0x1F4C) = n;
    if (n == rom_word((uint32_t)pakram[0x1094] << 16 | FXU(0x1092)))
        n = 0;
    return (uint16_t)(n + 1);
}

/* GsuBytes200_*: the renderer's four input bytes at $0200: 0 and the
   three given. */
void gsu_inputs_200(uint8_t b1, uint8_t b2, uint8_t b3)
{
    pakram[0x0200] = 0;
    pakram[0x0201] = b1;
    pakram[0x0202] = b2;
    pakram[0x0203] = b3;
}

/* GsuRecordsUpload: the $0CEE records of 30 bytes at $7E:F77F to $0AAA,
   their count to $01C8 ($1F2B counting them down to 0). */
void gsu_records_upload(void)
{
    uint16_t n = gsu_records_count, d = 0x0AAA;
    uint32_t s = 0xF77F;
    int i;

    FXU(0x01C8) = n;
    if (!n)
        return;
    for (; n; n--, s += 30, d = (uint16_t)(d + 30))
        for (i = 0; i < 30; i++)
            pakram[(uint16_t)(d + i)] = wram[s + i];
    gsu_records_left = 0;
}

/* The draw buffer swapped: its start ($021A and direct page $48) and the
   Super FX's screen base. */
static void buffer(uint16_t start, uint8_t base)
{
    FXU(0x021A) = start;
    W16(0x0048) = start;
    fx_screen.base = base;
}

/* GsuScreenFlip: the race's: buffer 0 or 1 (offset 8 or 0), both at
   $2C00. */
void gsu_screen_flip(void)
{
    if (gsu_buffer_select) {
        gsu_buffer_select = 0;
        gsu_buffer_offset = 8;
    } else {
        gsu_buffer_select++;
        gsu_buffer_offset = 0;
    }
    buffer(0x2C00, 0x0B);
}

/* GsuScreenFlipMenu, GsuScreenToggle: buffer 0 at $2C00 or 1 at $9400. */
void gsu_screen_toggle(void)
{
    if (gsu_buffer_select) {
        gsu_buffer_select = 0;
        buffer(0x2C00, 0x0B);
    } else {
        gsu_buffer_select++;
        buffer(0x9400, 0x25);
    }
}
