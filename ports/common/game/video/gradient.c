/*
 * gradient.c: the sky's colour gradient, drawn by HDMA.
 *
 * A course's colours are run-length coded: a run byte n repeats the next
 * colour word n times. Two lists are expanded into bank $7F, the first at
 * $7F:EF57 and the second at $7F:F357 (from write offset $0400), and the
 * $100 bytes at $7F:F757 are cleared. The screen takes them a line at a
 * time through two HDMA channels: channel 2 writes CGADD from the table at
 * $7F:3497 (a line count and a colour number per entry), and channel 3
 * writes CGDATA indirectly from the table at $7F:3489 (a line count and the
 * address in bank $7F of the colours for those lines). Moving the two
 * colour addresses at $7F:348D and $7F:3493 scrolls the gradient
 * (gradient_pointers_to_bank_7f, scroll.c).
 *
 * The channels are set through video_line_table (video.h); the colours
 * they send reach the picture by video_register 0x21 and 0x22.
 */
#include "game.h"
#include "gradient.h"
#include "video.h"

/* Work RAM index of $7F:a. */
#define BANK7F(a)       (0x10000u + (uint32_t)(a))

/* The write offset into the colour lists, kept in direct page $F3. */
#define expand_offset   W16(0x00F3)

/* A colour word at `at` in `bank`, low byte first. */
static uint16_t bank_word(uint8_t bank, uint16_t at)
{
    return (uint16_t)(bank_byte(bank, at) | bank_byte(bank, (uint16_t)(at + 1)) << 8);
}

/* The runs from run table index `run` (their lengths in the common table
   at $1F:B44E): run n writes the colour word at *y + $20, n times, to the
   first list from expand_offset; each run moves *y on by two and the
   index by one. Stops at a run of length 0. */
static void expand_runs(uint8_t bank, uint16_t run, uint16_t *y)
{
    for (;;) {
        uint8_t n = bank_byte(0x1F, (uint16_t)(0xB44E + run));
        uint16_t colour, out;
        int i;

        if (!n)
            return;
        colour = bank_word(bank, (uint16_t)(0x20 + *y));
        out = expand_offset;
        for (i = 0; i < n; i++, out = (uint16_t)(out + 2))
            W16(BANK7F(0xEF57) + out) = colour;
        expand_offset = out;
        run++;
        *y = (uint16_t)(*y + 2);
    }
}

/* The course test both expansions start with: the course byte at $0E3D of
   the colours' bank. The named course uses the run table from `special`;
   any other course, from 0. */
static void expand_list(uint8_t bank, uint16_t *y, uint16_t course, uint16_t special)
{
    uint16_t c = bank_byte(bank, 0x0E3D);

    expand_runs(bank, c == course ? special : 0, y);
}

void gradient_expand_first(uint8_t bank, uint16_t *y)
{
    expand_list(bank, y, 0x000A, 0x0028);
}

void gradient_expand_second(uint8_t bank, uint16_t *y)
{
    expand_list(bank, y, 0x0001, 0x0050);
}

/* Both lists for the course entry x: the first from offset 0, the second
   (when the course has one) from $0400; then $100 bytes at $7F:F757 are
   cleared. The block table at $03:F6F1 holds, per course, the colour block's
   address (word) and bank (byte); the second list's block address is the
   word at $03:F745. */
uint16_t gradient_build_colours(uint16_t x)
{
    uint8_t bank = *rom_at(0x03F6F3u + x);
    uint16_t y = rom_word(0x03F6F1u + x), second;
    int i;

    expand_offset = 0x0000;
    gradient_expand_first(bank, &y);
    second = rom_word(0x03F745u + x);
    if (second) {
        y = second;
        expand_offset = 0x0400;
        gradient_expand_second(bank, &y);
    }
    for (i = 0xFE; i >= 0; i -= 2)
        W16(BANK7F(0xF757) + i) = 0x0000;
    return y;
}

/* The CGADD table words from $7F:3497 + x, count of them with `value`.
   A count of 0 would mean 65536 words, past the end of work RAM here, so
   callers must not pass it. */
static uint16_t fill_cgadd(uint16_t value, uint16_t x, uint16_t count)
{
    do {
        W16(BANK7F(0x3497) + x) = value;
        x = (uint16_t)(x + 2);
    } while (--count);
    return x;
}

uint16_t gradient_fill_cgadd(uint16_t value, uint16_t x, uint16_t count)
{
    return fill_cgadd(value, x, count);
}

uint16_t gradient_fill_cgadd_b(uint16_t value, uint16_t x, uint16_t count)
{
    return fill_cgadd(value, x, count);
}

/* The gradient's HDMA channels and tables: the CGADD table at $7F:3497 (a
   count of $17 lines of colour 1, then $50 single lines, $18 lines and $50
   more, then the end) and the CGDATA table at $7F:3489 (entries of $17,
   $50, $18 and $50 lines, their colour addresses $EC57, $EC57, $EC57 and
   $ED57 in bank $7F). The writes keep the original's order, so the two
   addresses written twice end as the original leaves them. */
uint16_t gradient_setup(uint8_t bank)
{
    static const struct { uint16_t at, v; } ptr[6] = {
        {0x348A, 0xEC57}, {0x348D, 0xECD7}, {0x3490, 0xEC57},
        {0x3493, 0xECD7}, {0x348D, 0xEC57}, {0x3493, 0xED57}};
    static const struct { uint16_t at; uint8_t v; } cnt[5] = {
        {0x3489, 0x17}, {0x348C, 0xD0}, {0x348F, 0x18}, {0x3492, 0xD0}, {0x3494, 0x00}};
    static const VideoLineTable cgadd = {
        .mode = 0, .indirect = 0, .target = 0x21, .bank = 0x7F, .indirect_bank = 0, .at = 0x3497
    };
    static const VideoLineTable cgdata = {
        .mode = 2, .indirect = 1, .target = 0x22, .bank = 0x7F, .indirect_bank = 0x7F, .at = 0x3489
    };
    /* the course entry: the course byte times three, kept to 8 bits */
    uint8_t entry = (uint8_t)(bank_byte(bank, 0x0E3D) * 3);
    uint16_t x, src;
    unsigned k;

    video_line_table(2, &cgadd);
    video_line_table(3, &cgdata);

    W16(BANK7F(0x3497)) = 0x0117;
    x = gradient_fill_cgadd_b(0x0181, 0x0002, 0x0050);
    W16(BANK7F(0x3497) + x) = 0x0118;
    x = gradient_fill_cgadd_b(0x0181, (uint16_t)(x + 2), 0x0050);
    W16(BANK7F(0x3497) + x) = 0x0000;
    for (k = 0; k < 6; k++)
        W16(BANK7F(ptr[k].at)) = ptr[k].v;
    for (k = 0; k < 5; k++)
        W8(BANK7F(cnt[k].at)) = cnt[k].v;

    /* A: the high byte of the course's block address, and the last of the
       32 bytes at $1F:A800 the original reads (the reads of the block
       and of those bytes change nothing, so only the last is kept) */
    src = rom_word(0x03F6F1u + entry);
    return (uint16_t)((src & 0xFF00) | bank_byte(0x1F, 0xA81F));
}
