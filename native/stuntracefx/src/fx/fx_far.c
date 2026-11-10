/*
 * fx_far.c: the race's view distance (plan stage 10; the earlier work is
 * 218f69f1).
 *
 * The draw list comes from a block of map cells ahead of the view, at most
 * $31 records, so things showed up at the block's edge. Every other object
 * the list would draw, in every cell of the map, gets a far record from the
 * draw list (fx_far.h), with its place from the view in 32 bits. Here each
 * is turned to the view (the matrix at $00E4, as the sort turns the near
 * ones), left out behind the view or past its sides (the sort's tests, its
 * radius and the view's width), and halved until its place fits 14 bits;
 * they are drawn farthest first (keyed as the sort keys the near ones), $31
 * at a time in the near slots' place, through FxObjectsDraw, before the near
 * list. While one draws, its model's matrix and its sprite's size are
 * halved with its place (render/model.c, render/frame.c, fx_solid.c), so it
 * lands where it stands at its size, and its level of detail and colour
 * tier are taken from its true depth. The chip's first 64 KB and its state
 * are kept and put back after, so the near list draws from what it would
 * have: only the picture changes. Which objects are active stays the
 * trigger list's.
 */
#include <stdlib.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_far.h"

enum { SLOTS = 0x31, SLOT_SIZE = 0x1E, SLOT_FIRST = 0x0AAA };

typedef struct {
    int64_t key;
    int16_t at[3];
    uint8_t shift;
    uint16_t depth, rec;
} Far;

static Far far[FX_FAR_MAX];
static uint8_t slot_shift[SLOTS];
static uint16_t slot_depth[SLOTS];
static int drawing, shift_now;
static uint16_t depth_now;
static uint8_t kept[0x10000];

/* words of the chip's RAM (the second 64 KB, the first) and of ROM bank 0,
   read and written without moving the chip's last RAM address */
static uint16_t far_word(uint16_t a)
{
    return (uint16_t)(fx_ram[FX_FAR_BANK + a] | fx_ram[FX_FAR_BANK + (uint16_t)(a + 1)] << 8);
}

static uint16_t ram_word(uint16_t a)
{
    return (uint16_t)(fx_ram[a] | fx_ram[(uint16_t)(a + 1)] << 8);
}

static void ram_put(uint16_t a, uint16_t v)
{
    fx_ram[a] = (uint8_t)v;
    fx_ram[(uint16_t)(a + 1)] = (uint8_t)(v >> 8);
}

static uint16_t rom_word(uint16_t a)
{
    return (uint16_t)(fx_rom_read(0, a) | fx_rom_read(0, (uint16_t)(a + 1)) << 8);
}

static int64_t magnitude(int64_t v) { return v < 0 ? -v : v; }

static int farther_first(const void *a, const void *b)
{
    int64_t x = ((const Far *)a)->key, y = ((const Far *)b)->key;

    return x < y ? 1 : x > y ? -1 : 0;
}

void fx_far_pass(uint16_t *stack)
{
    unsigned count = far_word(FX_FAR_COUNT), i, n = 0, b, k;
    int32_t m[9], centre, half;
    FxState state = fxs;

    if (!count)
        return;
    if (count > FX_FAR_MAX)
        count = FX_FAR_MAX;
    fx_ram[FX_FAR_BANK + FX_FAR_COUNT] = fx_ram[FX_FAR_BANK + FX_FAR_COUNT + 1] = 0;
    for (i = 0; i < 9; i++)
        m[i] = (int16_t)ram_word((uint16_t)(0x00E4 + 2 * i));
    /* the view's half-width, as the sort takes it */
    centre = (int16_t)ram_word(0x0034);
    half = centre - (int16_t)ram_word(0x0038);
    if ((int16_t)ram_word(0x003A) + 1 - centre > half)
        half = (int16_t)ram_word(0x003A) + 1 - centre;
    for (i = 0; i < count; i++) {
        uint16_t rec = (uint16_t)(FX_FAR_RECORDS + i * FX_FAR_SIZE), shape = far_word((uint16_t)(rec + 0x08));
        int32_t radius = (int16_t)rom_word((uint16_t)(shape + 0x10)), bias = (int16_t)rom_word((uint16_t)(shape + 0x05));
        int64_t d[3], v[3], big = 0, x, z;
        unsigned s = 0;
        int c;

        for (k = 0; k < 3; k++)
            d[k] = (int32_t)(far_word((uint16_t)(rec + FX_FAR_PLACE + 4 * k))
                             | (uint32_t)far_word((uint16_t)(rec + FX_FAR_PLACE + 4 * k + 2)) << 16);
        /* the slot's +$12, +$10, +$14 turned as the sort turns them: each
           product's top 16 bits doubled with bit 15 below, summed */
        for (c = 0; c < 3; c++)
            v[c] = (d[0] * m[c] >> 15) + (d[1] * m[3 + c] >> 15) + (d[2] * m[6 + c] >> 15);
        if (v[2] + radius < 0)
            continue;
        x = magnitude(v[0]);
        z = magnitude(v[2]);
        if (!(half > 128 ? x - radius < z * half / 128 : x - radius < z))
            continue;
        for (k = 0; k < 3; k++)
            if (magnitude(v[k]) > big)
                big = magnitude(v[k]);
        while ((big >> s) > 0x3FFF)
            s++;
        far[n].key = (magnitude(d[1]) << 1) + (fx_ram[FX_FAR_BANK + (uint16_t)(rec + FX_FAR_LIFT)] ? 0x1F40 : 0) + bias + v[2];
        for (k = 0; k < 3; k++)
            far[n].at[k] = (int16_t)(v[k] >> s);
        far[n].shift = (uint8_t)s;
        far[n].depth = (uint16_t)(v[2] > 0x7FFF ? 0x7FFF : v[2] < 0 ? 0 : v[2]);
        far[n].rec = rec;
        n++;
    }
    if (!n)
        return;
    qsort(far, n, sizeof far[0], farther_first);
    memcpy(kept, fx_ram, sizeof kept);
    drawing = 1;
    for (b = 0; b < n; b += SLOTS) {
        unsigned in = n - b < SLOTS ? n - b : SLOTS;

        for (k = 0; k < in; k++) {
            const Far *f = &far[b + k];
            uint16_t slot = (uint16_t)(SLOT_FIRST + k * SLOT_SIZE);
            int64_t key = f->key < 0 ? 0 : f->key > 0xFFFF ? 0xFFFF : f->key;

            memcpy(&fx_ram[slot], &fx_ram[FX_FAR_BANK + f->rec], SLOT_SIZE);
            ram_put(slot, k + 1 < in ? (uint16_t)(slot + SLOT_SIZE) : 0);
            ram_put((uint16_t)(slot + 0x02), (uint16_t)key);
            ram_put((uint16_t)(slot + 0x12), (uint16_t)f->at[0]);
            ram_put((uint16_t)(slot + 0x10), (uint16_t)f->at[1]);
            ram_put((uint16_t)(slot + 0x14), (uint16_t)f->at[2]);
            slot_shift[k] = f->shift;
            slot_depth[k] = f->depth;
        }
        ram_put(GSU_DRAW_LIST, SLOT_FIRST);
        *stack = 0x0240;
        fx_stw(FX_MODEL_1B4, 0);
        (void)FxObjectsDraw(0, stack);
    }
    fx_draw_flush();
    drawing = 0;
    shift_now = 0;
    memcpy(fx_ram, kept, sizeof kept);
    fxs = state;
}

void fx_far_record(uint16_t record)
{
    unsigned k;

    if (!drawing) {
        shift_now = 0;
        return;
    }
    k = (uint16_t)(record - SLOT_FIRST) / SLOT_SIZE % SLOTS;
    shift_now = slot_shift[k];
    depth_now = slot_depth[k];
}

int fx_far_shift(void) { return shift_now; }
uint16_t fx_far_depth(uint16_t depth) { return shift_now ? depth_now : depth; }
int fx_far_drawing(void) { return drawing; }
