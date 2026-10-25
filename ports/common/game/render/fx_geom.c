/*
 * fx_geom.c: the 3D renderer's geometry: matrices, projection, the depth
 * sort of the objects, and the points of models and polygons.
 *
 * What the Super FX computed in its RAM is computed here in the same
 * place (state.h), so the rest of the renderer finds it where it always
 * did. Products are the Super FX's (fixmath.h: mul15, fmult), sums wrap at
 * 16 bits, and halvings are its DIV2 and ASR.
 */
#include "game.h"
#include "render.h"

/* ---- matrices: the current one at fx_matrix, row by row ---- */

/* The row vector v times the current matrix: out[c] = v . column c. */
void fx_matrix_apply(const int16_t v[3], int16_t out[3])
{
    int c;

    for (c = 0; c < 3; c++)
        out[c] = wrap16(mul15(v[0], fx_matrix[c]) + mul15(v[1], fx_matrix[3 + c]) + mul15(v[2], fx_matrix[6 + c]));
}

/* The current matrix times the column vector v: out[r] = row r . v. */
void fx_matrix_rotate(const int16_t v[3], int16_t out[3])
{
    int r;

    for (r = 0; r < 3; r++)
        out[r] = wrap16(mul15(v[0], fx_matrix[3 * r]) + mul15(v[1], fx_matrix[3 * r + 1])
                        + mul15(v[2], fx_matrix[3 * r + 2]));
}

/* The three vectors at fx_vectors, each through fx_matrix_apply, to `out`
   (nine words, at a game pak RAM address). */
void fx_matrix_apply_vectors(uint16_t out)
{
    int i, k;

    for (i = 0; i < 3; i++) {
        int16_t v[3], r[3];

        for (k = 0; k < 3; k++)
            v[k] = fx_vectors[3 * i + k];
        fx_matrix_apply(v, r);
        for (k = 0; k < 3; k++)
            FXW(out + 6 * i + 2 * k) = r[k];
    }
}

/* The programs the S-CPU starts on the vector ($0068, $002C, $002E): the
   matrix times it, and its transpose times it, to fx_vector_out. */
void fx_matrix_rotate_program(void)
{
    int16_t v[3] = {fx_vector_in_x, fx_vector_in_y, fx_vector_in_z}, r[3];
    int k;

    fx_matrix_rotate(v, r);
    for (k = 0; k < 3; k++)
        fx_vector_out[k] = r[k];
}

void fx_matrix_rotate_back_program(void)
{
    int16_t v[3] = {fx_vector_in_x, fx_vector_in_y, fx_vector_in_z}, r[3];
    int k;

    fx_matrix_apply(v, r);
    for (k = 0; k < 3; k++)
        fx_vector_out[k] = r[k];
}

/* The rotation matrix of the angles at fx_angles, to `out`. */
void fx_matrix_from_angles_at(uint16_t out)
{
    uint16_t a[3] = {fx_angles[0], fx_angles[1], fx_angles[2]};
    int16_t m[9];
    int k;

    fx_matrix_from_angles(a, m);
    for (k = 0; k < 9; k++)
        FXW(out + 2 * k) = m[k];
}

void fx_matrix_transpose_at(uint16_t at)
{
    int16_t m[9];
    int k;

    for (k = 0; k < 9; k++)
        m[k] = FXW(at + 2 * k);
    fx_matrix_transpose(m);
    for (k = 0; k < 9; k++)
        FXW(at + 2 * k) = m[k];
}

/* ---- the programs on single values ---- */

void fx_angle_program(void)
{
    fx_angle_out = fx_angle_of(fx_angle_in_y, fx_angle_in_x);
}

/* The length of (x, z) from the sum of their squares as 32 bits. */
void fx_length_program(void)
{
    uint32_t sum = (uint32_t)((int32_t)fx_length_in_x * fx_length_in_x)
                   + (uint32_t)((int32_t)fx_length_in_z * fx_length_in_z);

    fx_length_out = fx_sqrt(sum).root;
}

/* The words of entry `entry` (from 1) of the 8-byte entry table. */
uint16_t fx_table_entry_words(uint16_t entry)
{
    return (uint16_t)(fx_entry_table + 2 + (uint16_t)(entry - 1) * 8);
}

void fx_table_2702_program(void)
{
    static const uint8_t table[48] = {
        0x00, 0x1F, 0x1B, 0x1E, 0x00, 0xBF, 0xB7, 0xB6, 0x00, 0xBF, 0xBC, 0xBD, 0x00, 0xBF, 0xB5, 0xBB,
        0x00, 0x1F, 0x5B, 0x58, 0x00, 0xBF, 0xB2, 0xB3, 0x00, 0x1E, 0x15, 0x14, 0x00, 0x17, 0x19, 0x1A,
        0x00, 0x17, 0x18, 0x1E, 0x00, 0x17, 0x19, 0x1A, 0x00, 0x17, 0x18, 0x15, 0x00, 0x14, 0x15, 0x1E,
    };
    int i;

    for (i = 0; i < 48; i++)
        pakram[FX_TABLE_2702 + i] = table[i];
}

/* The screen buffer cleared to colour 0: $3400 words from its start. */
void fx_screen_clear(void)
{
    uint16_t at = fx_screen_start;
    int i;

    for (i = 0; i < 0x3400; i++, at = (uint16_t)(at + 2))
        FXU(at) = 0;
}

/* ---- projection ---- */

/* The flags the original's SUB and BGE test: a - b >= 0 as a signed
   compare (S equal to OV). */
static bool signed_ge(int16_t a, int16_t b)
{
    return (int32_t)a - b >= 0;
}

/* One axis of the outcode on top of `code`: one more past the low edge,
   two more past the high one, the two bits swapped (XOR 3) when the bit
   `behind` is in the code. */
static uint8_t outcode_axis(uint8_t code, int16_t v, int16_t low, int16_t high, uint8_t behind)
{
    if (signed_ge(low, v))
        code++;
    else if (!signed_ge(high, v))
        code = (uint8_t)(code + 2);
    else
        return code;
    if (code & behind)
        code ^= 3;
    return code;
}

/* The point (x, y) at depth z to the screen: over the depth with
   fx_ratio_clamp (the larger magnitude first), moved to the screen centre.
   A negative depth is behind the view: taken positive, the signs come back
   reversed and the outcode has 4 (then 16). Returns the outcode: X's edges
   in bits 2-3, Y's in bits 0-1, with the code's complement in the high
   byte; *z = the depth as used (0 counts as 1). */
uint16_t fx_project_point(int16_t *x, int16_t *y, int16_t *z)
{
    bool behind = *z < 0, xneg = *x < 0, yneg = *y < 0, swap;
    uint16_t ax = xneg ? (uint16_t)-*x : (uint16_t)*x, ay = yneg ? (uint16_t)-*y : (uint16_t)*y;
    int16_t a, b;
    uint8_t code;

    if (behind)
        *z = (int16_t)-(uint16_t)*z;
    else if (*z == 0)
        *z = 1;
    code = behind ? 4 : 0;
    swap = ax < ay;
    a = (int16_t)(swap ? ay : ax);
    b = (int16_t)(swap ? ax : ay);
    fx_ratio_clamp(&a, &b, (uint16_t)*z);
    *x = swap ? b : a;
    *y = swap ? a : b;
    if (yneg != behind)
        *y = (int16_t)-(uint16_t)*y;
    if (xneg != behind)
        *x = (int16_t)-(uint16_t)*x;
    *x = wrap16(*x + fx_screen_centre_x);
    *y = wrap16(*y + fx_screen_centre_y);
    code = outcode_axis(code, *x, fx_edges[0], fx_edges[1], 0x04);
    code = (uint8_t)((int8_t)code * 4);
    code = outcode_axis(code, *y, fx_edges[2], fx_edges[3], 0x10);
    return (uint16_t)((code ^ 0x1F) << 8 | code);
}

#define FX_SCALE_TABLE 0x896Cu      /* in bank $14: the scale for each even depth */

/* the scale for an even depth, its address kept to 16 bits as R14 */
static int16_t scale_at(uint16_t z)
{
    uint16_t at = (uint16_t)(FX_SCALE_TABLE + z);

    return (int16_t)(rom_at(0x140000u | at)[0] | rom_at(0x140000u | (uint16_t)(at + 1))[0] << 8);
}

/* (x, y) and a size at depth z, through the scale table: x and y moved to
   the screen centre. */
void fx_project_scaled(int16_t *x, int16_t *y, int16_t *z, int16_t *size)
{
    int16_t scale;

    while (wrap16(0x4400 - *z) < 0) {
        *x = asr16(*x, 1);
        *y = asr16(*y, 1);
        *size = asr16(*size, 1);
        *z = asr16(*z, 1);
    }
    while (wrap16(0x0100 - *z) >= 0) {
        *size = wrap16(*size * 2);
        *x = wrap16(*x * 2);
        *y = wrap16(*y * 2);
        *z = wrap16(*z * 2);
    }
    *z = (int16_t)(*z & ~1);
    scale = scale_at((uint16_t)*z);
    *x = wrap16(fmult(*x, scale) + fx_screen_centre_x);
    *y = wrap16(fmult(*y, scale) + fx_screen_centre_y);
    *size = fmult(*size, scale);
}

/* A polygon's vertex to the screen: far ones (past $100 once brought in
   to $4400) through the scale table, near ones through fx_project_point.
   Returns the outcode word as fx_project_point's; a far vertex's has X
   below 0 in bit 2, at or past the right edge in bit 3, Y below 0 in bit
   0, at or past the bottom edge in bit 1. */
uint16_t fx_project_vertex(int16_t *x, int16_t *y, int16_t z)
{
    int16_t scale;
    uint8_t code = 0;

    while (wrap16(0x4400 - z) < 0) {
        *x = asr16(*x, 1);
        *y = asr16(*y, 1);
        z = asr16(z, 1);
    }
    if (wrap16(0x0100 - z) >= 0)
        return fx_project_point(x, y, &z);
    z = (int16_t)(z & ~1);
    scale = scale_at((uint16_t)z);
    *x = wrap16(fmult(*x, scale) + fx_screen_centre_x);
    *y = wrap16(fmult(*y, scale) + fx_screen_centre_y);
    if (*x < 0)
        code = 4;
    else if (wrap16(*x - fx_edges[1]) >= 0)
        code = 8;
    if (*y < 0)
        code |= 1;
    else if (wrap16(*y - fx_edges[3]) >= 0)
        code |= 2;
    return (uint16_t)((code ^ 0x1F) << 8 | code);
}

/* ---- the depth sort ---- */

#define SLOT(s, off) FXU((s) + (off))

/* The object slots into the draw list, farthest first: each turned into
   view space unless already (bit 7 of +$07), culled when behind the view
   (z + radius negative) or past its sides (|x| - radius - |z| not
   negative), its key the old one plus z plus its shape's bias, and linked
   in before the first slot whose key is not above it. The shapes (+$08)
   are in ROM bank 0: the radius at +$10, the bias at +$05. */
void fx_sort_objects_program(void)
{
    uint16_t slot = FX_SLOTS;
    unsigned n = fx_slot_count, i;

    fx_list_head = 0;
    for (i = 0; i < n; i++, slot = (uint16_t)(slot + FX_SLOT_SIZE)) {
        uint16_t shape, key, prev, next;
        int16_t radius, z, ax, az;

        if (!(pakram[(uint16_t)(slot + 0x07)] & 0x80)) {
            int16_t v[3] = {(int16_t)SLOT(slot, 0x12), (int16_t)SLOT(slot, 0x10), (int16_t)SLOT(slot, 0x14)}, r[3];

            fx_matrix_apply(v, r);
            SLOT(slot, 0x12) = (uint16_t)r[0];
            SLOT(slot, 0x10) = (uint16_t)r[1];
            SLOT(slot, 0x14) = (uint16_t)r[2];
        }
        shape = SLOT(slot, 0x08);
        radius = (int16_t)rom_word((uint16_t)(shape + 0x10));
        z = (int16_t)SLOT(slot, 0x14);
        if (wrap16(z + radius) < 0)
            continue;
        az = (int16_t)(z < 0 ? -(uint16_t)z : (uint16_t)z);
        ax = (int16_t)SLOT(slot, 0x12);
        ax = (int16_t)(ax < 0 ? -(uint16_t)ax : (uint16_t)ax);
        if (wrap16(wrap16(ax - radius) - az) >= 0)
            continue;
        key = (uint16_t)(rom_word((uint16_t)(shape + 5)) + (uint16_t)z + SLOT(slot, 0x02));
        /* the list walked from the head, the slot put before the first
           whose key - 1 - this key is negative */
        prev = 0;
        next = fx_list_head;
        while (next && wrap16(SLOT(next, 0x02) - 1 - key) >= 0) {
            prev = next;
            next = SLOT(next, 0x00);
        }
        if (prev)
            SLOT(prev, 0x00) = slot;
        else
            fx_list_head = slot;
        SLOT(slot, 0x02) = key;
        SLOT(slot, 0x00) = next;
    }
}

/* ---- points ---- */

/* A script byte of the current ROM bank, signed or not. */
static uint8_t script_u8(uint8_t bank, uint16_t *at)
{
    return rom_at((uint32_t)bank << 16 | (*at)++)[0];
}

/* A polygon's records at FX_RECORDS (ten bytes each): from the pair list
   (eight-byte entries, from the last back) each pair's two words halved
   (DIV2) to records count to 1; then the points the `count` index bytes
   at `script` name (from the table at FX_POINTS) moved by the vector at
   fx_vector_out, to records 0 to count - 1. Only the count's low byte
   places the first loop's start, as its UMULT takes a byte. Returns the
   script past its index bytes. */
uint16_t fx_points_gather(uint16_t count, uint8_t bank, uint16_t script)
{
    uint32_t n = count ? count : 0x10000u, i;
    uint16_t src = (uint16_t)(fx_pairs_end - 4 + (count & 0xFF) * 8);
    uint16_t dst = (uint16_t)(FX_RECORDS + (count & 0xFF) * 10);
    int k;

    for (i = 0; i < n; i++, src = (uint16_t)(src - 8), dst = (uint16_t)(dst - 10)) {
        int16_t a = div2(FXW(src)), b = div2(FXW(src + 2));

        FXW(dst) = a;
        FXW(dst + 2) = b;
    }
    dst = FX_RECORDS + 4;
    for (i = 0; i < n; i++, dst = (uint16_t)(dst + 10)) {
        uint16_t p = (uint16_t)(FX_POINTS + script_u8(bank, &script) * 6);

        for (k = 0; k < 3; k++)
            FXW(dst + 2 * k) = wrap16(FXW(p + 2 * k) + fx_vector_out[k]);
    }
    return script;
}

/* A model's points from its byte script (in the given ROM bank) into the
   list at FX_MODEL_POINTS, three words each: 4, a count, then the points
   as signed bytes; $38, a count, then points each also mirrored (-x, y,
   z); $20, a jump (a word, counted from its second byte); $1C, a byte
   skipped and a jump from the table of them by fx_model_variant; any
   other byte ends it. A count of 0 runs 65536 times. Returns the list's end;
   *end = the script past the ending byte. */
uint16_t fx_model_points(uint8_t bank, uint16_t script, uint16_t *end)
{
    uint16_t out = FX_MODEL_POINTS;

    for (;;) {
        int8_t c = (int8_t)script_u8(bank, &script);

        if (c == 0x04 || c == 0x38) {
            uint32_t n = script_u8(bank, &script);

            if (!n)
                n = 0x10000;       /* LOOP's count from 0 */
            while (n--) {
                int16_t x = (int8_t)script_u8(bank, &script), y = (int8_t)script_u8(bank, &script);
                int16_t z = (int8_t)script_u8(bank, &script);

                FXW(out) = x;
                FXW(out + 2) = y;
                FXW(out + 4) = z;
                out = (uint16_t)(out + 6);
                if (c == 0x38) {
                    FXW(out) = (int16_t)-x;
                    FXW(out + 2) = y;
                    FXW(out + 4) = z;
                    out = (uint16_t)(out + 6);
                }
            }
            continue;
        }
        if (c == 0x1C) {
            script = (uint16_t)(script + 1 + fx_model_variant * 2);
        } else if (c != 0x20) {
            *end = script;
            return out;
        }
        {
            uint16_t lo = script_u8(bank, &script);

            script = (uint16_t)(script + (uint16_t)(lo | rom_at((uint32_t)bank << 16 | script)[0] << 8));
        }
    }
}

/* ---- where a segment crosses z = 0 ---- */

/* The ends a and b: 0 when both have z >= 0, -1 when both are negative;
   otherwise the end with negative z moved to the crossing, found by
   halving toward it (DIV2 of the sums) until the midpoint's z is 0, and
   1. */
int fx_segment_cross_z(int16_t a[3], int16_t b[3])
{
    int16_t keep[3], *neg, *pos;
    int k;

    if (a[2] >= 0 && b[2] >= 0)
        return 0;
    if (a[2] < 0 && b[2] < 0)
        return -1;
    neg = a[2] < 0 ? a : b;
    pos = a[2] < 0 ? b : a;
    for (k = 0; k < 3; k++)
        keep[k] = pos[k];
    FXW(0x009A) = keep[0];                  /* the end kept, where the search saves it */
    FXW(0x009C) = keep[1];
    FXW(0x009E) = keep[2];
    for (;;) {
        int16_t m = div2(wrap16(neg[2] + pos[2]));

        if (m == 0) {
            neg[0] = div2(wrap16(neg[0] + pos[0]));
            neg[1] = div2(wrap16(neg[1] + pos[1]));
            neg[2] = 0;
            for (k = 0; k < 3; k++)
                pos[k] = keep[k];
            return 1;
        }
        if (m < 0) {
            neg[2] = m;
            neg[0] = div2(wrap16(neg[0] + pos[0]));
            neg[1] = div2(wrap16(neg[1] + pos[1]));
        } else {
            pos[2] = m;
            pos[0] = div2(wrap16(neg[0] + pos[0]));
            pos[1] = div2(wrap16(neg[1] + pos[1]));
        }
    }
}
