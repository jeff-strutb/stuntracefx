/*
 * programs.c: the S-CPU's uses of the Super FX programs: each sets a
 * program's inputs in the game pak RAM, runs it and takes what it leaves.
 * The HUD's drawing programs (bank 7) take their place and size at
 * $020A-$0216 and their colours at $0200-$0203; $01:CF42 unpacks into a
 * buffer; $01:80BC, $01:81E3, $01:82CE and $01:8325 work on vectors and
 * the view.
 */
#include "game.h"
#include "render.h"
#include "glue.h"

#define fx_draw_x       FXU(0x020A)     /* the HUD programs' inputs */
#define fx_draw_y       FXU(0x020C)
#define fx_draw_advance FXU(0x020E)
#define fx_draw_rows    FXU(0x0210)
#define fx_draw_step_x  FXU(0x0212)
#define fx_draw_step_y  FXU(0x0214)
#define gsu_in_216      FXU(0x0216)     /* the piece */
#define hud_panel_b     W8(0x0D62)      /* the second player's panel drawn (0: the first's) */

static uint16_t rom7(uint16_t at)
{
    return rom_word(0x070000u | at);
}

/* ---- the unpacker ($01:CF42) ---- */

/* GsuCF42ToC000, ...To4000, ...To3000: the stream `in68`, `in6a` unpacked
   to `dest`. */
void gsu_unpack_to(uint16_t in68, uint16_t in6a, uint16_t dest)
{
    FXU(0x0068) = in68;
    FXU(0x002C) = dest;
    FXU(0x00A2) = 0;
    FXU(0x006A) = in6a;
    gsu_run(0x01, 0xCF42);
}

/* ---- the vector programs ---- */

/* ObjectGsu80BC: the length of object `obj`'s offset from the point
   (direct page $04, $05F2) in x and z. */
uint16_t gsu_object_length(uint16_t obj)
{
    FXU(0x0068) = (uint16_t)(W16(0x0004) - W16(0x200C + obj));
    FXU(0x002E) = (uint16_t)(W16(0x05F2) - W16(0x2010 + obj));
    gsu_run(0x01, 0x80BC);
    return FXU(0x006A);
}

/* Gsu82CE: the vector at direct page $02, $08, $94 turned by the view, to
   direct page $B7, $B9, $BB. */
void gsu_vector_turn(void)
{
    int i;

    FXU(0x0068) = W16(0x0002);
    FXU(0x002C) = W16(0x0008);
    FXU(0x002E) = W16(0x0094);
    gsu_run(0x01, 0x82CE);
    for (i = 0; i < 3; i++)
        W16(0x00B7 + 2 * i) = FXU(0x0026 + 2 * i);
}

/* Gsu81E3: the angle of the vector at direct page $02, $08. */
uint16_t gsu_vector_angle(void)
{
    FXU(0x0068) = W16(0x0002);
    FXU(0x002C) = W16(0x0008);
    gsu_run(0x01, 0x81E3);
    return FXU(0x0040);
}

/* Gsu8325: the view matrix of the angles at $065E into $0608. */
void gsu_view_matrix(void)
{
    int i;

    for (i = 0; i < 3; i++)
        FXU(0x0020 + 2 * i) = W16(0x065E + 2 * i);
    gsu_run(0x01, 0x8325);
    for (i = 0; i < 9; i++)
        W16(0x0608 + 2 * i) = FXU(0x00E4 + 2 * i);
}

/* ViewMatrixUpdateAndSave: the view's angles ($0664) kept at $065E, its
   matrix made and kept at $064C. */
void view_matrix_update_and_save(void)
{
    int i;

    for (i = 0; i < 3; i++)
        W16(0x065E + 2 * i) = W16(0x0664 + 2 * i);
    gsu_view_matrix();
    for (i = 0; i < 9; i++)
        W16(0x064C + 2 * i) = W16(0x0608 + 2 * i);
}

/* ViewUpdateAndGsuAEF8: that, then the objects sorted. */
void view_update_and_sort(void)
{
    view_matrix_update_and_save();
    gsu_run(0x01, 0xAEF8);
}

/* ViewReset: the view at the origin facing ahead: its matrix made (kept
   at $064C) and the identity at $063A ($7F00 on the diagonal). */
void view_reset(void)
{
    int i;

    for (i = 0; i < 3; i++)
        W16(0x0664 + 2 * i) = 0;
    for (i = 0; i < 3; i++)
        W16(0x065E + 2 * i) = 0;
    gsu_view_matrix();
    for (i = 0; i < 9; i++)
        W16(0x064C + 2 * i) = W16(0x0608 + 2 * i);
    for (i = 0; i < 9; i++)
        W16(0x063A + 2 * i) = (i % 4) == 0 ? 0x7F00 : 0;
}

/* GsuEE88Ramp: the ramp $80, $90, ... $F0 at $3C00, and the noise fill. */
void gsu_noise_ramp(void)
{
    int i;

    for (i = 0; i < 8; i++)
        FXU(0x3C00 + 2 * i) = (uint16_t)(0x80 + 0x10 * i);
    gsu_run(0x07, 0xEE88);
}

/* ---- the HUD's glyphs and pictures (bank 7) ---- */

/* GsuF329Entry: glyph `n` (its low four bits) of the table at $8E52. */
void gsu_glyph_f329(uint16_t n)
{
    gsu_param_216(0x8E52, n & 0x0F);
    gsu_run(0x07, 0xF329);
}

/* GsuF329Sequence: the five glyphs of the bytes at `at`, with $980A after
   the first and $980C after the third (a time: digits and marks). */
void gsu_glyph_f329_sequence(uint16_t at)
{
    gsu_glyph_f329(W8(at));
    gsu_in_216 = 0x980A;
    gsu_run(0x07, 0xF329);
    gsu_glyph_f329(W8(at + 1));
    gsu_glyph_f329(W8(at + 2));
    gsu_in_216 = 0x980C;
    gsu_run(0x07, 0xF329);
    gsu_glyph_f329(W8(at + 3));
    gsu_glyph_f329(W8(at + 4));
}

/* GsuF441Player1/2: the player's picture: its kind's colours and piece
   (kind 1, or any other), at ($74, $3C). */
void gsu_player_picture(int player)
{
    if ((W8(player ? 0x0DF5 : 0x0DF4) & 0x0F) == 1) {
        gsu_inputs_200(0x0F, 0x05, 0x0E);
        gsu_in_216 = 0x080E;
    } else {
        gsu_inputs_200(0x0F, 0x08, 0x0E);
        gsu_in_216 = 0x0A0E;
    }
    fx_draw_x = 0x0074;
    fx_draw_y = 0x003C;
    gsu_run(0x07, 0xF441);
}

/* GsuF0A5OrF0B4, GsuEEF5OrEFCD: the rising picture at height `y`, split
   for the second player's panel ($0D62) or whole, in its colours. */
void gsu_picture_rise(uint16_t y, int split)
{
    fx_draw_y = y;
    if (hud_panel_b) {
        gsu_inputs_200(0x0F, split ? 0x04 : 0x09, split ? 0x05 : 0x0A);
        gsu_run(0x07, split ? 0xEEF5 : 0xF0A5);
    } else {
        gsu_inputs_200(0xBF, split ? 0xB7 : 0xBC, split ? 0xB6 : 0xBD);
        gsu_run(0x07, split ? 0xEFCD : 0xF0B4);
    }
}

/* GsuDigitPair0E74: the two digits of the byte at $0E74 at (4, 8). */
void gsu_digit_pair_0e74(void)
{
    fx_draw_advance = 0x0010;
    fx_draw_x = 0x0004;
    fx_draw_y = 0x0008;
    gsu_param_216(0x8E3E, W8(0x0E74) >> 4 & 0x0F);
    gsu_run(0x07, 0xF15B);
    gsu_param_216(0x8E3E, W8(0x0E74) & 0x0F);
    gsu_run(0x07, 0xF15B);
}

/* the pieces at (x, y), w x h (16 high), each with the program $07:F15B */
static void glyphs_f15b(uint16_t x, uint16_t y, uint16_t h, const uint16_t *pieces, int n)
{
    int i;

    fx_draw_rows = h;
    fx_draw_advance = 0x0010;
    fx_draw_x = x;
    fx_draw_y = y;
    for (i = 0; i < n; i++) {
        gsu_in_216 = pieces[i];
        gsu_run(0x07, 0xF15B);
    }
}

/* GsuF15BThree: three pieces at ($30, $18). */
void gsu_glyphs_three(void)
{
    static const uint16_t pieces[3] = {0x9000, 0x8808, 0x881C};

    gsu_inputs_200(0x0F, 0x04, 0x05);
    glyphs_f15b(0x0030, 0x0018, 0x0020, pieces, 3);
}

/* GsuF15BFive: five pieces at ($18, $18). */
void gsu_glyphs_five(void)
{
    static const uint16_t pieces[5] = {0x8410, 0x8C0C, 0x8408, 0x8C10, 0x8804};

    gsu_inputs_200(0x0F, 0x09, 0x0A);
    glyphs_f15b(0x0018, 0x0018, 0x0020, pieces, 5);
}

/* GsuFA1BFromTables: at the point ($0D6D, $0D6F + 8), full size, the
   scaled sprite of entry $0D6B of the tables at $07:97D6 (its width; none
   when 0) and $07:97E2 (its horizontal scale). Returns whether drawn. */
int gsu_sprite_fa1b_from_tables(void)
{
    uint16_t at = W16(0x0D6B), w;

    gsu_inputs_200(0xBF, 0xBC, 0xBD);
    fx_draw_x = W16(0x0D6D);
    fx_draw_y = (uint16_t)(W16(0x0D6F) + 8);
    fx_draw_rows = 0x0100;
    fx_draw_step_y = 0x0100;
    w = rom7((uint16_t)(0x97D6 + at));
    if (!w)
        return 0;
    fx_draw_advance = w;
    fx_draw_step_x = rom7((uint16_t)(0x97E2 + at));
    gsu_in_216 = 0x1E00;
    gsu_run(0x07, 0xFA1B);
    return 1;
}

/* GsuFA5AThree: three eighth-scale sprites about the point ($0D6D - 8,
   $0D6F): $1A10, $1B10 $18 lower, then $1A18 8 lower, stretched. */
void gsu_sprites_fa5a_three(void)
{
    uint16_t x = (uint16_t)(W16(0x0D6D) - 8);

    fx_draw_advance = 0x0100;
    fx_draw_step_x = 0x0100;
    fx_draw_rows = 0x0100;
    fx_draw_step_y = 0x0100;
    fx_draw_x = x;
    fx_draw_y = W16(0x0D6F);
    gsu_in_216 = 0x1A10;
    gsu_run(0x07, 0xFA5A);
    fx_draw_x = (uint16_t)(W16(0x0D6D) - 8);
    fx_draw_y = (uint16_t)(W16(0x0D6F) + 0x18);
    gsu_in_216 = 0x1B10;
    gsu_run(0x07, 0xFA5A);
    fx_draw_rows = 0x0200;
    fx_draw_step_y = 0x0080;
    fx_draw_x = (uint16_t)(W16(0x0D6D) - 8);
    fx_draw_y = (uint16_t)(W16(0x0D6F) + 8);
    gsu_in_216 = 0x1A18;
    gsu_run(0x07, 0xFA5A);
}
