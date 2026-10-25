/*
 * fx_model.c: a 3D model drawn from its commands in ROM.
 *
 * A model is a list of command bytes (0, 4, ... $64) with their operands,
 * in the ROM bank at fx_model_bank: commands that turn the model's points
 * by the view matrix into the table at FX_POINTS (six bytes a point), one
 * that projects them all into FX_SCREEN_POINTS (screen x, y and the
 * outcode word), faces queued in the draw list (FX_DRAW_LIST) with their
 * colours and textures, and commands that steer the list: frame choice,
 * jumps, calls, tests of the face-visibility flags (FX_FACING) and a back
 * to front sort. A list ends with a return; what it returns to is kept on
 * a stack: the caller, the end (the draw list drawn), the deferred lists
 * of the list starts, and the second halves of the conditional commands.
 */
#include "game.h"
#include "render.h"
#include "rom.h"

#define fx_model_list       FXU(0x0016)     /* the model's commands */
#define fx_model_faces      FXU(0x0018)     /* where the commands go on after the projection */
#define fx_model_bank       pakram[0x001C]
#define fx_point_out        FXU(0x001E)     /* where the next turned point goes */
#define fx_point_scale      FXU(0x0030)     /* 2 to the power fx_point_shift */
#define fx_point_shift      FXU(0x0032)
#define fx_face_kind        FXU(0x0042)     /* 0 flat, $FFFF textured, $FFFE textured by colour */
#define fx_model_frame      FXU(0x0044)
#define fx_material_mask    FXU(0x0046)
#define fx_material_table   FXU(0x004A)
#define fx_shade_table      FXU(0x004C)
#define fx_shade_base       FXU(0x004E)
#define fx_model_lists      FXU(0x0050)
#define fx_shade_at         FXU(0x0052)
#define fx_model_flags      FXU(0x0054)     /* bit 15 shaded sprites, 14 all on screen, 13 flat, 12 */
#define fx_deferred_at      FXU(0x0056)     /* the next free word of the deferred lists */
#define fx_face_colour      FXU(0x0058)     /* low byte the colour, high byte the plot options */
#define fx_uv_at            FXU(0x0088)     /* the next u, v pair of a textured face */
#define fx_tex_bank         FXU(0x00A6)
#define fx_tex_base         FXU(0x00A8)
#define fx_tex_mask         FXU(0x00D4)
#define fx_object_matrix    ((sw16 *)&pakram[0x00E4])
#define fx_light            ((sw16 *)&pakram[0x0100])
#define fx_light_local      ((sw16 *)&pakram[0x0106])
#define fx_view_matrix      ((sw16 *)&pakram[0x0132])
#define fx_points_total     FXU(0x0144)
#define fx_sort_count       FXU(0x0148)
#define fx_sort_left        FXU(0x014E)
#define fx_sort_table       FXU(0x0150)
#define fx_model_select     pakram[0x01B4]
#define fx_model_lod        pakram[0x01B6]
#define fx_car_lights       pakram[0x108C]

#define FX_SCREEN_POINTS    0x04E4u
#define FX_DEFERRED         0x0788u
#define FX_SORT_DEPTHS      0x07B0u
#define FX_FACING           0x07C6u
#define FX_MATERIAL_BANK    0x03
#define FX_SCALE_BANK       0x14
#define FX_SCALE_TABLE      0x896Cu

static uint8_t rb(uint8_t bank, uint16_t at)
{
    return rom_at((uint32_t)bank << 16 | at)[0];
}

static uint16_t rw(uint8_t bank, uint16_t at)
{
    return (uint16_t)(rb(bank, at) | rb(bank, (uint16_t)(at + 1)) << 8);
}

/* MULT: the low bytes, signed */
static int16_t mult8(uint16_t a, uint16_t b)
{
    return (int16_t)((int8_t)a * (int8_t)b);
}

static uint8_t hib(uint16_t v)
{
    return (uint8_t)(v >> 8);
}

/* ---- what a return returns to ---- */

enum {
    K_CALLER,           /* out of the model */
    K_END,              /* the draw list drawn, then a return */
    K_DEFERRED,         /* a list start's deferred lists, from the first */
    K_DEFERRED_NEXT,    /* the next deferred list */
    K_BOTH_A1, K_BOTH_A2, K_BOTH_B1, K_BOTH_B2,   /* the halves of command $5C */
    K_SKIP_PASSED,      /* command $28 with its flag clear: the list itself */
    K_SKIP_RAN,         /* command $28 with its flag set: what follows the list */
    K_SORT_NEXT,        /* the depth sort's next list */
};

typedef struct {
    uint8_t kind;
    uint16_t a, b;
} Cont;

static Cont conts[256];
static int nconts;

static void push(uint8_t kind, uint16_t a, uint16_t b)
{
    Cont k = {kind, a, b};

    if (nconts < (int)(sizeof conts / sizeof conts[0]))
        conts[nconts++] = k;
}

/* ---- the set-up ---- */

/* The shade tables by the object's depth against the model's three
   distance bytes (bank 3, at fx_model_lists or the level of detail's
   entry), the car's set when its lights are on. */
static void shade_tables(void)
{
    static const uint16_t tables[2][4] = {{0xA1C5, 0xA1F1, 0xA21D, 0xA249}, {0xA275, 0xA2A1, 0xA2CD, 0xA2F9}};
    static const uint16_t offsets[4] = {0, 0x0088, 0x0110, 0x0198};
    uint16_t at = fx_model_lists;
    int k;

    if (fx_model_lod)
        at = (uint16_t)(0xAE45 + 4 * (fx_model_lod - 1));
    for (k = 0; k < 3; k++)
        if (wrap16((rb(FX_MATERIAL_BANK, (uint16_t)(at + k)) << 8) + FXW(0x002A)) < 0)
            break;
    fx_shade_table = tables[fx_car_lights != 0][k];
    fx_shade_at = (uint16_t)(fx_shade_base + offsets[k]);
}

/* The view matrix from the object's angles (bytes, turned into negated
   8.8 words in place): straight from the object's matrix when they are
   all zero, negated in its first and last rows for a half turn about y
   alone; else built (fx_matrix_from_angles, transposed, the vectors
   through the object's matrix). A flat model has the middle row zero. */
static void view_matrix(void)
{
    uint16_t x = fx_angles[0], y = fx_angles[1], z = fx_angles[2];
    bool flat = fx_model_flags & 0x2000;
    int k;

    fx_angles[0] = (uint16_t)-(uint16_t)((x & 0xFF) << 8);
    fx_angles[1] = (uint16_t)-(uint16_t)((y & 0xFF) << 8);
    fx_angles[2] = (uint16_t)-(uint16_t)((z & 0xFF) << 8);
    if (!(x | z) && (y == 0 || y == 0x80)) {
        for (k = 0; k < 9; k++)
            fx_view_matrix[k] = y && (k < 3 || k >= 6) ? (int16_t)-(uint16_t)fx_object_matrix[k] : fx_object_matrix[k];
        if (flat)
            fx_view_matrix[3] = fx_view_matrix[4] = fx_view_matrix[5] = 0;
        return;
    }
    fx_matrix_from_angles_at(0x0116);
    fx_matrix_transpose_at(0x0116);
    if (flat)
        FXW(0x0118) = FXW(0x011E) = FXW(0x0124) = 0;
    fx_matrix_apply_vectors(0x0132);
}

/* The light ($0000, $393E, $727C) turned into the object: each row of
   the view matrix by it, the sum's high byte, signed. With a point scale
   below 8 the matrix's high bytes are packed two to a word for command
   $04. */
static void light_and_packed(void)
{
    int i;

    fx_light[0] = 0;
    fx_light[1] = 0x393E;
    fx_light[2] = 0x727C;
    for (i = 0; i < 3; i++) {
        int16_t s = wrap16(mul15(fx_light[0], fx_view_matrix[3 * i]) + mul15(fx_light[1], fx_view_matrix[3 * i + 1])
                           + mul15(fx_light[2], fx_view_matrix[3 * i + 2]));

        fx_light_local[i] = (int8_t)hib((uint16_t)s);
    }
    if (wrap16(fx_point_shift - 3) < 0) {
        for (i = 0; i < 4; i++)
            FXU(0x0128 + 2 * i) = (uint16_t)(hib((uint16_t)fx_view_matrix[2 * i]) | hib((uint16_t)fx_view_matrix[2 * i + 1]) << 8);
        FXU(0x0130) = hib((uint16_t)fx_view_matrix[8]);
    }
}

/* ---- the points ---- */

/* The view matrix's element (row i, column j), and its high byte. */
#define V(i, j)  fx_view_matrix[3 * (i) + (j)]
#define VH(i, j) ((int8_t)hib((uint16_t)V(i, j)))

/* a point (x, y, z) turned: output j = the sum over i of in[i] by V(i, j) */
static void turned(const int16_t in[3], int16_t out[3])
{
    int j;

    for (j = 0; j < 3; j++)
        out[j] = wrap16(mul15(in[0], V(0, j)) + mul15(in[1], V(1, j)) + mul15(in[2], V(2, j)));
}

static void put3(uint16_t at, const int16_t v[3])
{
    FXW(at) = v[0];
    FXW(at + 2) = v[1];
    FXW(at + 4) = v[2];
}

/* Commands $08 and $4C (words) and $04 at a point scale of 8 or more
   (bytes times the scale's low byte, signed): n points turned. */
static uint16_t points(uint16_t pos, bool bytes)
{
    uint8_t bank = fx_model_bank;
    uint16_t n = rb(bank, pos++), k = n, at = fx_point_out;

    fx_points_total = (uint16_t)(fx_points_total + n);
    do {
        int16_t in[3], out[3];
        int i;

        for (i = 0; i < 3; i++) {
            if (bytes) {
                in[i] = mult8(rb(bank, pos), fx_point_scale);
                pos = (uint16_t)(pos + 1);
            } else {
                in[i] = (int16_t)rw(bank, pos);
                pos = (uint16_t)(pos + 2);
            }
        }
        turned(in, out);
        put3(at, out);
        at = (uint16_t)(at + 6);
    } while (--k);
    fx_point_out = at;
    return pos;
}

/* Commands $34 (words) and $38 at a point scale of 8 or more (bytes): n
   points turned, each written twice, as given and mirrored in x (its x
   term taken off twice). */
static uint16_t points_mirrored(uint16_t pos, bool bytes)
{
    uint8_t bank = fx_model_bank;
    uint16_t n = rb(bank, pos++), k = n, at = fx_point_out;

    fx_points_total = (uint16_t)(fx_points_total + 2 * n);
    do {
        int16_t in[3];
        int i, j;

        for (i = 0; i < 3; i++) {
            if (bytes) {
                in[i] = mult8(rb(bank, pos), fx_point_scale);
                pos = (uint16_t)(pos + 1);
            } else {
                in[i] = (int16_t)rw(bank, pos);
                pos = (uint16_t)(pos + 2);
            }
        }
        for (j = 0; j < 3; j++) {
            int16_t a = mul15(in[0], V(0, j)), s = wrap16(mul15(in[2], V(2, j)) + mul15(in[1], V(1, j)) + a);

            FXW(at + 2 * j) = s;
            FXW(at + 6 + 2 * j) = wrap16(s - a - a);
        }
        at = (uint16_t)(at + 12);
    } while (--k);
    fx_point_out = at;
    return pos;
}

/* Commands $04 and $38 at a point scale below 8: byte points by the
   matrix's high bytes (MULT), each sum doubled and its high byte times
   the scale. */
static int16_t packed(int sum, uint16_t scale)
{
    return mult8(hib((uint16_t)(2 * sum)), scale);
}

static uint16_t points_packed(uint16_t pos, bool mirrored)
{
    uint8_t bank = fx_model_bank;
    uint16_t n = rb(bank, pos++), k = n, at = fx_point_out, s = fx_point_scale;

    fx_points_total = (uint16_t)(fx_points_total + (mirrored ? 2 * n : n));
    do {
        int8_t in[3];
        int i, j;

        for (i = 0; i < 3; i++)
            in[i] = (int8_t)rb(bank, pos++);
        for (j = 0; j < 3; j++) {
            int a = VH(0, j) * in[0], r = wrap16(VH(1, j) * in[1] + VH(2, j) * in[2]);

            if (mirrored) {
                FXW(at + 2 * j) = packed(r + a, s);
                FXW(at + 6 + 2 * j) = packed(r - a, s);
            } else {
                FXW(at + 2 * j) = packed(r + a, s);
            }
        }
        at = (uint16_t)(at + (mirrored ? 12 : 6));
    } while (--k);
    fx_point_out = at;
    return pos;
}

/* ---- the projection (command $0C) ---- */

/* Each point moved by the object's position to the screen: a far one
   (depth $100 or more, held at $43FE) through the scale table, a near
   one and every one after it through fx_project_vertex; the outcodes
   ORed into fx_codes. Returns 0 when every point is outside one side. */
static bool project(void)
{
    uint16_t n = fx_points_total, k = n, at = FX_POINTS, out = FX_SCREEN_POINTS, codes = 0;
    bool slow = false;

    do {
        int16_t x = wrap16(FXW(at) + fx_vector_out[0]), y = wrap16(FXW(at + 2) + fx_vector_out[1]),
                z = wrap16(FXW(at + 4) + fx_vector_out[2]);
        uint16_t word;

        if (!slow && wrap16(z - 0x0100) < 0)
            slow = true;
        if (slow) {
            word = fx_project_vertex(&x, &y, z);
        } else {
            uint8_t code = 0;
            int16_t scale;

            if (wrap16(z - 0x43FE) >= 0)
                z = 0x43FE;
            scale = (int16_t)rw(FX_SCALE_BANK, (uint16_t)(FX_SCALE_TABLE + (z & ~1)));
            x = wrap16(fmult(x, scale) + fx_screen_centre_x);
            y = wrap16(fmult(y, scale) + fx_screen_centre_y);
            if (x < 0)
                code = 4;
            else if (wrap16(x - fx_edges[1]) >= 0)
                code = 8;
            if (y < 0)
                code |= 1;
            else if (wrap16(y - fx_edges[3]) >= 0)
                code |= 2;
            word = (uint16_t)((code ^ 0x1F) << 8 | code);
        }
        FXW(out) = x;
        FXW(out + 2) = y;
        FXU(out + 4) = word;
        codes |= word;
        at = (uint16_t)(at + 6);
        out = (uint16_t)(out + 6);
    } while (--k);
    fx_codes = codes;
    if ((codes & 0x1F00) != 0x1F00)
        return false;
    if (!(codes & 0x1F))
        fx_model_flags |= 0x4000;
    return true;
}

/* ---- materials ---- */

/* The material word of entry b of the material table (bank 3), through
   the animated ones (bits 15 set, 14 clear: a count and words, picked by
   fx_material_mask). */
static uint16_t material_word(uint8_t b)
{
    uint16_t at = (uint16_t)(fx_material_table + 2 * b);

    for (;;) {
        uint16_t w = rw(FX_MATERIAL_BANK, at), q;

        if (!(w & 0x8000) || (w & 0x4000))
            return w;
        q = w & 0x1FFF;
        at = (uint16_t)(q + 1 + 2 * (fx_material_mask & (uint16_t)(rb(FX_MATERIAL_BANK, q) - 1)));
    }
}

/* A textured material: the colour, then the texture's address, bank,
   mask and u, v list. */
static void texture(uint16_t w, uint8_t c1)
{
    uint16_t e = (uint16_t)(0x9D59 + 3 * (w & 0xFF)), p;

    if (w & 0x2000)
        fx_face_colour = (uint16_t)(0x0C00 + c1);
    fx_tex_base = rw(FX_MATERIAL_BANK, e);
    fx_tex_bank = rb(FX_MATERIAL_BANK, (uint16_t)(e + 2));
    p = rw(FX_MATERIAL_BANK, (uint16_t)(0xA079 + 2 * (hib(w) & 0x1F)));
    fx_tex_mask = rw(FX_MATERIAL_BANK, p);
    fx_uv_at = (uint16_t)(p + 2);
}

/* A shaded colour: the face's normal (three signed bytes after the
   material byte, in the model's bank) against the turned light, from 6
   to 15, into the shade table h. */
static uint8_t shade(uint16_t m, unsigned h)
{
    uint8_t bank = fx_model_bank;
    uint16_t tab = rw(FX_MATERIAL_BANK, (uint16_t)(fx_shade_table + 2 * h));
    int16_t s = wrap16(mult8(rb(bank, (uint16_t)(m + 1)), (uint16_t)fx_light_local[0])
                       + mult8(rb(bank, (uint16_t)(m + 2)), (uint16_t)fx_light_local[1])
                       + mult8(rb(bank, (uint16_t)(m + 3)), (uint16_t)fx_light_local[2]));
    int16_t d = (int8_t)hib((uint16_t)asr16(s, 2));

    if (d < 6)
        d = 6;
    else if (d >= 0x10)
        d = 0x0F;
    return rb(FX_MATERIAL_BANK, (uint16_t)(tab + d - 6));
}

/* A face's material (its byte at m, its normal after it): the colour
   (0 or more) or a textured face (negative), fx_face_kind and the
   texture set. */
static int16_t material(uint16_t m)
{
    uint16_t w;
    uint8_t c1;
    unsigned h;

    if (fx_model_flags & 0x2000) {
        fx_face_kind = 0;
        return 9;
    }
    w = material_word(rb(fx_model_bank, m));
    if (w & 0x8000) {
        fx_face_kind = 0xFFFE;
        fx_face_colour = (uint16_t)(0x0800 | (hib(w) & 15) << 4);
        return -2;
    }
    c1 = rb(FX_MATERIAL_BANK, (uint16_t)(0x9FB1 + (w & 0xFF)));
    fx_face_colour = (uint16_t)(0x0800 + c1);
    fx_face_kind = w & 0x4000 ? 0xFFFF : 0;
    if (fx_face_kind) {
        texture(w, c1);
        return -1;
    }
    h = hib(w) & 0x3F;
    if (h == 0x3F)
        return w & 0xFF;
    if (h == 0x3E)
        return rb(FX_MATERIAL_BANK, (uint16_t)(fx_shade_at + (w & 0xFF)));
    return shade(m, h);
}

/* A sprite's material (its byte at m): as a face's, with no colour kept
   and shading only when the model's flags have bit 15. */
static void sprite_material(uint16_t m)
{
    uint16_t w;
    uint8_t c1;

    if ((fx_model_flags & 0x2000) && !(fx_model_flags & 0x1000)) {
        fx_face_colour = 0;
        fx_face_kind = 0;
        return;
    }
    w = material_word(rb(fx_model_bank, m));
    if (w & 0x8000) {
        fx_face_kind = 0xFFFE;
        fx_face_colour = (uint16_t)(0x0800 | (hib(w) & 15) << 4);
        return;
    }
    c1 = rb(FX_MATERIAL_BANK, (uint16_t)(0x9FB1 + (w & 0xFF)));
    fx_face_colour = (uint16_t)(0x0800 + c1);
    fx_face_kind = w & 0x4000 ? 0xFFFF : 0;
    if (fx_face_kind)
        texture(w, c1);
}

/* ---- faces (command $14) ---- */

/* The face's last part (from fx_pairs_end: a face split in depth has
   its near part before it, ended by a copy of its header) of m points
   kept: its count in its type byte and the list's end past it. */
static void face_keep(uint16_t m)
{
    uint16_t v = fx_pairs_end;

    if (!(fx_face_kind & 0x8000)) {
        fx_draw_list_end = (uint16_t)(v + 4 * m);
        pakram[(uint16_t)(v - 2)] = (uint8_t)m;
    } else {
        fx_draw_list_end = (uint16_t)(v + 8 * m);
        pakram[(uint16_t)(v - 8)] = (uint8_t)~m;
    }
}

/* One face at pos: [n] [a byte] [its facing flag's index] [material,
   normal] [n point indexes]. A face whose flag has bit 7 clear faces
   away.
   Its entry is written at the draw list's end (the list drawn first when
   nearly full): the screen points the indexes name, and for a textured
   face their u, v; a face with a point off the screen is clipped (in
   depth when one is behind) and a face wholly outside a side dropped.
   Returns past it. */
static uint16_t face(uint16_t pos, uint8_t n)
{
    uint8_t bank = fx_model_bank;
    uint16_t at, e, v, acc = 0, k = n, idx = (uint16_t)(pos + 7);
    int16_t r;

    if (n != 2) {
        uint8_t g = pakram[(uint16_t)(FX_FACING + rb(bank, (uint16_t)(pos + 2)))];

        /* bit 7 shows it; the test for a zero flag tests the INC of the
           command pointer in its branch's delay slot instead, zero only
           when the pointer wraps */
        if (!(g & 0x80) && (uint16_t)(pos + 3) != 0)
            return (uint16_t)(pos + 7 + n);
    }
    fx_points_n = n;
    if (wrap16(0x09AA - fx_draw_list_end) < 0)
        fx_draw_list_program();
    r = material((uint16_t)(pos + 3));
    e = fx_draw_list_end;
    if (r >= 0) {
        FXU(e) = (uint16_t)((r & 0xFF) << 8 | n);
        v = (uint16_t)(e + 2);
    } else {
        FXU(e) = (uint16_t)((uint8_t)~n | (fx_tex_bank & 0xFF) << 8);
        FXU(e + 2) = fx_face_colour;
        FXU(e + 4) = fx_tex_base;
        FXU(e + 6) = fx_tex_mask;
        v = (uint16_t)(e + 8);
    }
    at = v;
    do {
        uint16_t q = (uint16_t)(FX_SCREEN_POINTS + rb(bank, idx++) * 6);

        FXU(at) = FXU(q);
        FXU(at + 2) = FXU(q + 2);
        acc |= FXU(q + 4);
        at = (uint16_t)(at + 4);
        if (r < 0) {
            FXU(at) = (uint16_t)(rb(FX_MATERIAL_BANK, fx_uv_at) << 8);
            FXU(at + 2) = (uint16_t)(rb(FX_MATERIAL_BANK, (uint16_t)(fx_uv_at + 1)) << 8);
            fx_uv_at = (uint16_t)(fx_uv_at + 2);
            at = (uint16_t)(at + 4);
        }
    } while (--k);
    if (!(acc & 0xFF)) {
        fx_draw_list_end = at;
        return idx;
    }
    if ((acc & 0x1F00) != 0x1F00)
        return idx;
    fx_codes = acc;
    fx_pairs_end = v;
    if (hib(fx_face_kind)) {
        uint16_t m = fx_polygon_draw(acc, bank, idx);

        if (m)
            face_keep(m);
    } else {
        uint16_t m = n;

        if (acc & 0x10)
            m = fx_polygon_gather(n, bank, (uint16_t)(pos + 7));
        if (m && (m = fx_polygon_clip_screen(m)) != 0)
            face_keep(m);
    }
    return idx;
}

/* ---- the sprite command ($50) ---- */

static uint16_t sprite(uint16_t pos)
{
    uint8_t bank = fx_model_bank;
    uint16_t j = (uint16_t)(rb(bank, pos++) * 6);

    FXU(0x00BA) = FXU(FX_SCREEN_POINTS + j);
    FXU(0x00BC) = FXU(FX_SCREEN_POINTS + 2 + j);
    FXW(0x00BE) = wrap16(FXW(FX_POINTS + 4 + j) + fx_vector_out[2]);
    if (wrap16(FXW(0x00BE) - 0x20) < 0)
        return (uint16_t)(pos + 2);
    sprite_material(pos);
    FXU(0x00C8) = (uint16_t)((fx_tex_mask & 0xFF) + 1);
    FXU(0x00CE) = FXU(0x00C8) & 0x40 ? 0x0080 : 0x0100;
    FXU(0x00D2) = 0;
    return (uint16_t)(pos + 2);
}

/* ---- facing (command $30) ---- */

/* n triangles of three screen points: the sign of their cross product
   (the high byte of its high word) as each one's flag; with a point
   behind the view (outcode bit 4 of an odd number of them) reversed.
   When every point is on the screen only their low bytes are used. */
static uint16_t facing(uint16_t pos)
{
    uint8_t bank = fx_model_bank;
    uint16_t n = rb(bank, pos++), k = n, out = FX_FACING;
    bool bytes = fx_model_flags & 0x4000;

    do {
        int16_t x[3], y[3];
        uint8_t a = 0;
        int i;
        uint32_t d;

        for (i = 0; i < 3; i++) {
            uint8_t b = rb(bank, pos++);
            uint16_t q = (uint16_t)(FX_SCREEN_POINTS + (bytes ? b * 6 : (int8_t)b * 6));

            if (bytes) {
                x[i] = pakram[q];
                y[i] = pakram[(uint16_t)(q + 2)];
            } else {
                x[i] = FXW(q);
                y[i] = FXW(q + 2);
                a ^= pakram[(uint16_t)(q + 4)];
            }
        }
        d = (uint32_t)((int32_t)wrap16(y[2] - y[0]) * wrap16(x[1] - x[0]))
            - (uint32_t)((int32_t)wrap16(y[1] - y[0]) * wrap16(x[2] - x[0]));
        pakram[out++] = (uint8_t)((bytes ? 0 : a << 3) ^ (uint16_t)(d >> 16) >> 8);
    } while (--k);
    return pos;
}

/* ---- the depth sort (command $10) ---- */

/* The deepest of the sort's points left: its depth marked done, its list
   (a word in the table after the indexes) to run. */
static uint16_t sort_pick(void)
{
    uint16_t k = fx_sort_count, at = FX_SORT_DEPTHS, best = 0;
    int16_t deepest = (int16_t)0x8000;

    do {
        if (FXW(at) >= deepest) {
            deepest = FXW(at);
            best = at;
        }
        at = (uint16_t)(at + 2);
    } while (--k);
    FXU(best) = 0x8000;
    return rw(fx_model_bank, (uint16_t)(fx_sort_table + best));
}

/* ---- the interpreter ---- */

static void run(uint16_t pos)
{
    uint8_t bank;

    for (;;) {
        uint8_t c;

        bank = fx_model_bank;
        c = rb(bank, pos++);
        switch (c) {
        case 0x00:                              /* the end */
            fx_draw_list_program();
            goto ret;
        case 0x04:
            pos = wrap16(fx_point_shift - 3) < 0 ? points_packed(pos, false) : points(pos, true);
            break;
        case 0x08:
        case 0x4C:
            pos = points(pos, false);
            break;
        case 0x0C:
            if (!project())
                goto ret;
            pos = fx_model_faces;
            break;
        case 0x10: {                            /* the depth sort */
            uint16_t n = rb(bank, pos++), k = n, at = FX_SORT_DEPTHS;

            fx_sort_table = (uint16_t)(pos + n - FX_SORT_DEPTHS);
            fx_sort_left = n;
            fx_sort_count = n;
            do {
                FXU(at) = FXU(FX_POINTS + 4 + rb(bank, pos++) * 6);
                at = (uint16_t)(at + 2);
            } while (--k);
            push(K_SORT_NEXT, 0, 0);
            pos = sort_pick();
            break;
        }
        case 0x14:                              /* faces, to a count byte with bit 7 */
            for (;;) {
                uint8_t n = rb(bank, pos);

                if (n & 0x80)
                    break;
                pos = face(pos, n);
            }
            if (rb(bank, pos) == 0xFF)
                goto ret;
            pos++;
            break;
        case 0x18:
        case 0x24:
        case 0x2C:
        case 0x40:
        case 0x48:
        case 0x64:
            goto ret;
        case 0x1C: {                            /* the frame's list */
            uint16_t n = rb(bank, pos++), f = fx_model_frame & 0x3F;

            while (wrap16(f - n) >= 0)
                f = (uint16_t)(f - n);
            pos = (uint16_t)(pos + 2 * f);
        }
            /* fall through */
        case 0x20:                              /* a jump: from the offset's high byte */
            pos = (uint16_t)(pos + 1 + rw(bank, pos));
            break;
        case 0x28: {                            /* a list run or passed by a flag */
            uint8_t g = pakram[(uint16_t)(FX_FACING + rb(bank, pos))];
            uint16_t p = (uint16_t)(pos + 1);
            uint8_t len = rb(bank, (uint16_t)(p + 2));

            if (g & 0x80) {
                push(K_SKIP_RAN, (uint16_t)(p + 2), (uint16_t)(rw(bank, p) + p + 1));
                pos = (uint16_t)(p + 3);
            } else if (!len) {
                pos = (uint16_t)(p + 3);
            } else {
                push(K_SKIP_PASSED, (uint16_t)(p + 2), 0);
                pos = (uint16_t)(p + 2 + len);
            }
            break;
        }
        case 0x30:
            pos = facing(pos);
            break;
        case 0x34:
            pos = points_mirrored(pos, false);
            break;
        case 0x38:
            pos = wrap16(fx_point_shift - 3) < 0 ? points_packed(pos, true) : points_mirrored(pos, true);
            break;
        case 0x3C:
        case 0x58:                              /* a list start */
            fx_deferred_at = FX_DEFERRED;
            push(K_DEFERRED, 0, 0);
            break;
        case 0x44:                              /* a deferred list kept, then a return */
            FXU(fx_deferred_at) = (uint16_t)(rw(bank, pos) + pos + 1);
            fx_deferred_at = (uint16_t)(fx_deferred_at + 2);
            goto ret;
        case 0x50:
            pos = sprite(pos);
            break;
        case 0x54:                              /* a sprite if its flag says so */
            if (pakram[(uint16_t)(FX_FACING + rb(bank, pos))] & 0x80)
                pos = sprite((uint16_t)(pos + 1));
            else
                pos = (uint16_t)(pos + 4);
            break;
        case 0x5C: {                            /* both lists, in the order a flag picks */
            uint16_t w1 = (uint16_t)(pos + 1);

            if (pakram[(uint16_t)(FX_FACING + rb(bank, pos))] & 0x80) {
                push(K_BOTH_B1, w1, 0);
                pos = (uint16_t)(w1 + 3 + rw(bank, (uint16_t)(w1 + 2)));
            } else {
                push(K_BOTH_A1, w1, 0);
                pos = (uint16_t)(w1 + 1 + rw(bank, w1));
            }
            break;
        }
        case 0x60:                              /* a call: its return kept as a deferred list */
            FXU(fx_deferred_at) = (uint16_t)(pos + 2);
            fx_deferred_at = (uint16_t)(fx_deferred_at + 2);
            pos = (uint16_t)(pos + 1 + rw(bank, pos));
            break;
        default:                                /* not a command: the models have none */
            return;
        }
        continue;
    ret:
        /* a return: to what the stack holds */
        for (;;) {
            Cont k;

            if (!nconts)
                return;
            k = conts[--nconts];
            switch (k.kind) {
            case K_CALLER:
                return;
            case K_END:
                fx_draw_list_program();
                continue;
            case K_DEFERRED:
                FXU(fx_deferred_at) = 0;
                fx_deferred_at = FX_DEFERRED;
                /* fall through */
            case K_DEFERRED_NEXT: {
                uint16_t a = FXU(fx_deferred_at);

                fx_deferred_at = (uint16_t)(fx_deferred_at + 2);
                if (!a) {
                    fx_draw_list_program();
                    continue;
                }
                push(K_DEFERRED_NEXT, 0, 0);
                pos = a;
                break;
            }
            case K_BOTH_A1:
                push(K_BOTH_A2, (uint16_t)(k.a + 2), 0);
                pos = (uint16_t)(k.a + 3 + rw(fx_model_bank, (uint16_t)(k.a + 2)));
                break;
            case K_BOTH_A2:
                pos = (uint16_t)(k.a + 2);
                break;
            case K_BOTH_B1:
                push(K_BOTH_B2, k.a, 0);
                pos = (uint16_t)(k.a + 1 + rw(fx_model_bank, k.a));
                break;
            case K_BOTH_B2:
                pos = (uint16_t)(k.a + 4);
                break;
            case K_SKIP_PASSED:
                pos = (uint16_t)(k.a + 1);
                break;
            case K_SKIP_RAN: {
                uint8_t len = rb(fx_model_bank, k.a);

                FXU(fx_deferred_at) = k.b;
                fx_deferred_at = (uint16_t)(fx_deferred_at + 2);
                if (!len)
                    continue;
                pos = (uint16_t)(k.a + len);
                break;
            }
            case K_SORT_NEXT:
                if (!--fx_sort_left) {
                    fx_draw_list_program();
                    continue;
                }
                push(K_SORT_NEXT, 0, 0);
                pos = sort_pick();
                break;
            }
            break;
        }
    }
}

/* The model at fx_model_list drawn with the model flags: the draw list
   emptied, the shade tables and point scale set, the view matrix and the
   turned light; then its commands, to the end (which draws the list). */
void fx_model_draw_with_flags(uint16_t flags)
{
    uint16_t n, s = 1;

    fx_model_flags = flags;
    fx_draw_list_end = FX_DRAW_LIST;
    shade_tables();
    n = fx_point_shift;
    if (n) {
        uint16_t k = n;

        do
            s = (uint16_t)(s << 1);
        while (--k);
    }
    fx_point_scale = s;
    view_matrix();
    light_and_packed();
    fx_points_total = 0;
    fx_point_out = FX_POINTS;
    nconts = 0;
    push(K_CALLER, 0, 0);
    push(K_END, 0, 0);
    run(fx_model_list);
}

/* FxModelDrawFlagged: the flags kept to bit 12, with bit 13 (flat) set. */
void fx_model_draw_flagged(void)
{
    fx_model_draw_with_flags((uint16_t)((fx_model_flags & 0x1000) | 0x2000));
}

/* FxModelDraw: flagged when $01B4 is set, else with the flags $8000. */
void fx_model_draw(void)
{
    if (fx_model_select)
        fx_model_draw_flagged();
    else
        fx_model_draw_with_flags(0x8000);
}
