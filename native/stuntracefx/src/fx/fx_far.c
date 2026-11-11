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
 *
 * A parked tyre of a car in the near list (FX_FAR_PARKED: its car's child,
 * which ObjectChildFollow hides) is not drawn with the far ones but among
 * the near objects by depth (c918c678's): once the near list is drawn, it
 * is drawn again with the tyres in it, each before the first slot whose key
 * is not above its own (the sort's order), so a tyre behind its car's body
 * is under it and one in front over it; the chip's RAM put back after.
 * While a pass draws, a slot holds another record than its own place says:
 * fx_far_original gives the near record it holds (none for a far one or a
 * tyre), for the eyes' gaze and the angles between steps.
 */
#include <stdlib.h>
#include <string.h>

#include "fx/fx.h"
#include "fx/fx_draw.h"
#include "fx/fx_far.h"
#include "fx/fx_ground.h"

enum { SLOTS = 0x31, SLOT_SIZE = 0x1E, SLOT_FIRST = 0x0AAA };

typedef struct {
    int64_t key;
    int16_t at[3];
    uint8_t shift;
    uint16_t depth, rec;
    double near;                /* its nearest point's depth (its place's less its radius), no more than the one drawn before's */
} Far;

static Far far[FX_FAR_MAX], parked[FX_FAR_MAX];
static unsigned nparked;
static uint8_t slot_shift[SLOTS];
static uint16_t slot_depth[SLOTS];
static uint16_t slot_original[SLOTS];   /* the near record a slot holds in a pass; 0 none */
static double slot_near[SLOTS];         /* a far slot's nearest depth */
static uint8_t slot_far[SLOTS];         /* the slot holds a far object (not a near one or a tyre) */
static int drawing, shift_now, occlude, far_now;
static uint16_t depth_now;
static double near_now;
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

    nparked = 0;
    if (!count) {
        occlude = 0;
        return;
    }
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
        {
            Far *f = fx_ram[FX_FAR_BANK + (uint16_t)(rec + FX_FAR_PARKED)] ? &parked[nparked++] : &far[n++];

            /* a parked tyre's key as the sort keys a slot: its record's
               +$02, its depth and its shape's bias, in 16 bits */
            if (f >= parked && f < parked + FX_FAR_MAX)
                f->key = (int16_t)(uint16_t)(far_word((uint16_t)(rec + 0x02)) + (uint16_t)v[2] + (uint16_t)bias);
            else
                f->key = (magnitude(d[1]) << 1) + (fx_ram[FX_FAR_BANK + (uint16_t)(rec + FX_FAR_LIFT)] ? 0x1F40 : 0) + bias + v[2];
            for (k = 0; k < 3; k++)
                f->at[k] = (int16_t)(v[k] >> s);
            f->shift = (uint8_t)s;
            f->depth = (uint16_t)(v[2] > 0x7FFF ? 0x7FFF : v[2] < 0 ? 0 : v[2]);
            f->rec = rec;
            f->near = (double)(v[2] - radius);
        }
    }
    if (!n) {
        occlude = 0;
        return;
    }
    qsort(far, n, sizeof far[0], farther_first);
    /* the depths never deeper than the one drawn before's, so that of two far
       objects the later drawn is never hidden by the earlier: their order
       among themselves stays the sort's */
    for (i = 1; i < n; i++)
        if (far[i].near > far[i - 1].near)
            far[i].near = far[i - 1].near;
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
            slot_original[k] = 0;
            slot_near[k] = f->near;
            slot_far[k] = 1;
        }
        ram_put(GSU_DRAW_LIST, SLOT_FIRST);
        *stack = 0x0240;
        fx_stw(FX_MODEL_1B4, 0);
        (void)FxObjectsDraw(0, stack);
    }
    fx_draw_flush();
    drawing = 0;
    shift_now = 0;
    far_now = 0;
    occlude = 0;
    memcpy(fx_ram, kept, sizeof kept);
    fxs = state;
}

/* slot k filled with far record f: its record, its link, key and place */
static void far_slot(unsigned k, const Far *f, int last)
{
    uint16_t slot = (uint16_t)(SLOT_FIRST + k * SLOT_SIZE);
    int64_t key = f->key < 0 ? 0 : f->key > 0xFFFF ? 0xFFFF : f->key;

    memcpy(&fx_ram[slot], &fx_ram[FX_FAR_BANK + f->rec], SLOT_SIZE);
    ram_put(slot, last ? 0 : (uint16_t)(slot + SLOT_SIZE));
    ram_put((uint16_t)(slot + 0x02), (uint16_t)key);
    ram_put((uint16_t)(slot + 0x12), (uint16_t)f->at[0]);
    ram_put((uint16_t)(slot + 0x10), (uint16_t)f->at[1]);
    ram_put((uint16_t)(slot + 0x14), (uint16_t)f->at[2]);
    slot_shift[k] = f->shift;
    slot_depth[k] = f->depth;
    slot_original[k] = 0;
    slot_far[k] = 0;                    /* a tyre: among the near objects */
}

void fx_far_parked(uint16_t *stack)
{
    static struct {
        int tyre;
        unsigned at;
    } order[SLOTS + FX_FAR_MAX];
    static uint8_t used[FX_FAR_MAX];
    FxState state = fxs;
    unsigned n = 0, i, j, b, k, near = 0;
    uint16_t slot;

    if (!nparked)
        return;
    qsort(parked, nparked, sizeof parked[0], farther_first);
    memset(used, 0, nparked);
    for (slot = ram_word(GSU_DRAW_LIST); slot && near < SLOTS; slot = ram_word(slot)) {
        int16_t key = (int16_t)ram_word((uint16_t)(slot + 0x02));

        for (j = 0; j < nparked; j++)   /* the tyres keyed at or above it go before it */
            if (!used[j] && (int16_t)(key - 1 - (int16_t)parked[j].key) < 0) {
                used[j] = 1;
                order[n].tyre = 1;
                order[n++].at = j;
            }
        order[n].tyre = 0;
        order[n++].at = (unsigned)(uint16_t)(slot - SLOT_FIRST) / SLOT_SIZE;
        near++;
    }
    for (j = 0; j < nparked; j++)
        if (!used[j]) {
            order[n].tyre = 1;
            order[n++].at = j;
        }
    memcpy(kept, fx_ram, sizeof kept);
    {
        /* the near slots' bytes by their own place, from before the pass */
        static uint8_t by_slot[SLOTS][SLOT_SIZE];

        for (i = 0; i < SLOTS; i++)
            memcpy(by_slot[i], &kept[SLOT_FIRST + i * SLOT_SIZE], SLOT_SIZE);
        drawing = 1;
        for (b = 0; b < n; b += SLOTS) {
            unsigned in = n - b < SLOTS ? n - b : SLOTS;

            for (k = 0; k < in; k++) {
                if (order[b + k].tyre) {
                    far_slot(k, &parked[order[b + k].at], k + 1 == in);
                    continue;
                }
                slot = (uint16_t)(SLOT_FIRST + k * SLOT_SIZE);
                memcpy(&fx_ram[slot], by_slot[order[b + k].at], SLOT_SIZE);
                ram_put(slot, k + 1 < in ? (uint16_t)(slot + SLOT_SIZE) : 0);
                slot_shift[k] = 0;
                slot_depth[k] = 0;
                slot_far[k] = 0;
                slot_original[k] = (uint16_t)(SLOT_FIRST + order[b + k].at * SLOT_SIZE);
            }
            ram_put(GSU_DRAW_LIST, SLOT_FIRST);
            *stack = 0x0240;
            fx_stw(FX_MODEL_1B4, 0);
            (void)FxObjectsDraw(0, stack);
        }
    }
    fx_draw_flush();
    drawing = 0;
    shift_now = 0;
    nparked = 0;
    memcpy(fx_ram, kept, sizeof kept);
    fxs = state;
}

uint16_t fx_far_original(uint16_t record)
{
    unsigned k;

    if (!drawing)
        return record;
    if ((uint16_t)(record - SLOT_FIRST) % SLOT_SIZE)
        return 0;
    k = (uint16_t)(record - SLOT_FIRST) / SLOT_SIZE;
    return k < SLOTS ? slot_original[k] : 0;
}

void fx_far_record(uint16_t record)
{
    unsigned k;

    if (!drawing) {
        shift_now = 0;
        far_now = 0;
        return;
    }
    k = (uint16_t)(record - SLOT_FIRST) / SLOT_SIZE % SLOTS;
    shift_now = slot_shift[k];
    depth_now = slot_depth[k];
    near_now = slot_near[k];
    far_now = slot_far[k];
}

void fx_far_occlude(int on) { occlude = on; }

float fx_far_z(void)
{
    return drawing && occlude && far_now ? fx_ground_depth_z(near_now) : 0.0f;
}

int fx_far_shift(void) { return shift_now; }
uint16_t fx_far_depth(uint16_t depth) { return shift_now ? depth_now : depth; }
int fx_far_drawing(void) { return drawing; }
