/*
 * sort.c: the objects' draw list, the Super FX code of
 * asm/stuntracefx/render/sort.asm (C: src/stuntracefx/render/sort.c).
 *
 * Each frame the main CPU leaves the near objects' draw records in the
 * chip's RAM (GsuRecordsUpload). The sort turns their places into view
 * space, leaves out those behind the view or outside its sides, and links
 * the rest into one list in depth order, the farthest first, for the frame
 * program to paint back to front.
 *
 * Interim (plan section 8.2; the coordinator's ruling, 2026-11-09): the
 * records, their count and the list's head stay in the chip's RAM in the
 * original's layout until fx's piece (c) replaces the queue the frame
 * program walks (render/frame.c, fx_far.c, fx_solid.c and fx_ground.c read
 * them there). The view's matrix and its centre and edges are read where
 * the main CPU leaves them until they are FxShared's frame inputs. Those
 * are this file's 6 places in the chip's RAM (the records, the count, the
 * head, the matrix, the centre, the edges), read and written only through
 * ram_word and ram_put.
 */

#include "data/object_tables.h"
#include "fx/fx.h"

enum {
    RECORDS_FIRST = 0x0AAA,             /* the first draw record */
    RECORD_SIZE = 0x1E,
    /* a draw record's fields (object/draw_list.c writes them) */
    RECORD_NEXT = 0x00,                 /* the next record in the list, 0 at its end: the sort's */
    RECORD_KEY = 0x02,                  /* the depth key: the main CPU's part, then the sort's sum */
    RECORD_FLAGS = 0x07,
    RECORD_MODEL = 0x08,                /* its model record's word */
    RECORD_Y = 0x10,                    /* its place relative to the view, then in view space */
    RECORD_X = 0x12,
    RECORD_Z = 0x14,
    IN_VIEW_SPACE = 0x80,               /* in the flags: the place is already in view space */
    /* the view's words (GsuParamsFromDp) */
    VIEW_CENTRE_X = 0x0034,             /* its centre, in pixels */
    VIEW_LEFT = 0x0038,                 /* its left and right edges */
    VIEW_RIGHT = 0x003A,
    CHIP_SIDE = 128                     /* the chip's sides: 45 degrees, 128 pixels at the focal length 128 */
};

static uint16_t ram_word(uint16_t at) { return (uint16_t)(fx_ram[at] | fx_ram[(uint16_t)(at + 1)] << 8); }
static void ram_put(uint16_t at, uint16_t value)
{
    fx_ram[at] = (uint8_t)value;
    fx_ram[(uint16_t)(at + 1)] = (uint8_t)(value >> 8);
}
static uint16_t field(uint16_t record, unsigned offset) { return ram_word((uint16_t)(record + offset)); }
static void set_field(uint16_t record, unsigned offset, uint16_t value) { ram_put((uint16_t)(record + offset), value); }

/* a value times a 1.15 fraction, as the chip's FMULT and ROL: the product's bits 15-30 */
static uint16_t times(uint16_t value, uint16_t fraction)
{
    return (uint16_t)((uint32_t)((int32_t)(int16_t)value * (int16_t)fraction) >> 15);
}

/* A record's place turned into view space by the view's matrix (row k is
   world axis k's image), each sum kept to 16 bits as on the chip. */
static void to_view_space(uint16_t record)
{
    uint16_t x = field(record, RECORD_X), y = field(record, RECORD_Y), z = field(record, RECORD_Z);
    uint16_t m[9];

    for (int i = 0; i < 9; i++)
        m[i] = ram_word((uint16_t)(FX_MATRIX_W + 2 * i));
    set_field(record, RECORD_X, (uint16_t)(times(x, m[0]) + times(y, m[3]) + times(z, m[6])));
    set_field(record, RECORD_Y, (uint16_t)(times(x, m[1]) + times(y, m[4]) + times(z, m[7])));
    set_field(record, RECORD_Z, (uint16_t)(times(x, m[2]) + times(y, m[5]) + times(z, m[8])));
}

/* The view's half-width in pixels: the larger side of its centre. */
static int32_t view_half_width(void)
{
    int32_t centre = (int16_t)ram_word(VIEW_CENTRE_X);
    int32_t left = centre - (int16_t)ram_word(VIEW_LEFT);
    int32_t right = (int16_t)ram_word(VIEW_RIGHT) + 1 - centre;

    return right > left ? right : left;
}

static uint16_t size_of(uint16_t v) { return (int16_t)v < 0 ? (uint16_t)-v : v; }

/* Whether an object at view-space x, z with this radius can be seen: not
   behind the view (z + radius not negative), and not outside its sides
   (|x| - radius below |z|, the chip's 45 degrees, all in 16 bits as the
   chip's). The chip's sides are wider than the original's view (104 pixels
   each side). A view wider than them (the full window, plan stage 10) takes
   its own: |x| - radius below |z| times its half-width / 128. */
static int in_view(uint16_t x, uint16_t z, uint16_t radius, int32_t half_width)
{
    uint16_t x_size, z_size;

    if ((int16_t)(uint16_t)(z + radius) < 0)
        return 0;
    x_size = size_of(x);
    z_size = size_of(z);
    if (half_width > CHIP_SIDE)
        return (int16_t)(uint16_t)(x_size - radius) < (int32_t)z_size * half_width / CHIP_SIDE;
    return (int16_t)(uint16_t)(x_size - radius - z_size) < 0;
}

/* Links a record into the list, the farthest first: before the first
   record whose key is not above its own, compared as the chip compared
   them (the sign of the 16-bit difference less one). */
static void link_by_depth(uint16_t record, uint16_t key)
{
    uint16_t before = 0, at = ram_word(GSU_DRAW_LIST);

    while (at && (int16_t)(uint16_t)(field(at, RECORD_KEY) - key - 1) >= 0) {
        before = at;
        at = field(at, RECORD_NEXT);
    }
    if (before)
        set_field(before, RECORD_NEXT, record);
    else
        ram_put(GSU_DRAW_LIST, record);
    set_field(record, RECORD_KEY, key);
    set_field(record, RECORD_NEXT, at);
}

/*
 * @routine 01:AEF8 FxSortObjectsProgram
 * WHAT IT DOES: A program the S-CPU starts: builds the draw list of the
 * draw records, the farthest first, leaving out those behind the view or
 * outside its sides.
 * HOW: The list starts empty. For each of the records (the count's low
 * byte, as the chip's LOOP took it), its place is turned into view space
 * unless its flags say it is already; if it can be seen, its key becomes
 * its key + its view-space depth + its model's depth bias (each sum 16 bits)
 * and it is linked in by that key. The chip also left its ROM bank at $00
 * and its cursor, registers and last RAM address as the last record left
 * them.
 */
void FxSortObjectsProgram(void)
{
    int32_t half_width = view_half_width();
    uint16_t record = RECORDS_FIRST;

    ram_put(GSU_DRAW_LIST, 0);
    for (unsigned n = fx_ram[GSU_RECORDS_N]; n; n--, record = (uint16_t)(record + RECORD_SIZE)) {
        uint16_t model, z;

        if (!(fx_ram[(uint16_t)(record + RECORD_FLAGS)] & IN_VIEW_SPACE))
            to_view_space(record);
        model = field(record, RECORD_MODEL);
        z = field(record, RECORD_Z);
        if (in_view(field(record, RECORD_X), z, model_record_radius(model), half_width))
            link_by_depth(record, (uint16_t)(field(record, RECORD_KEY) + model_record_depth_bias(model) + z));
    }
}
