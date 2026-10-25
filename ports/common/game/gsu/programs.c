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

/* the four steps at $020E-$0214 at full size ($100) */
static void unit_scale(void)
{
    fx_draw_advance = 0x0100;
    fx_draw_step_x = 0x0100;
    fx_draw_rows = 0x0100;
    fx_draw_step_y = 0x0100;
}

static void sprite_fa5a(uint16_t x, uint16_t y, uint16_t piece)
{
    fx_draw_x = x;
    fx_draw_y = y;
    gsu_in_216 = piece;
    gsu_run(0x07, 0xFA5A);
}

/* GsuFA5AFromTables: at ($0D6D, $0D6F) $1B18 and $1D18 $18 lower, their
   width and step from entry $0D6B of the tables at $07:97BE and $97CA;
   then $1A55 8 lower, stretched. */
void gsu_sprites_fa5a_from_tables(void)
{
    uint16_t at = W16(0x0D6B);

    fx_draw_advance = rom7((uint16_t)(0x97BE + at));
    fx_draw_step_x = rom7((uint16_t)(0x97CA + at));
    fx_draw_rows = 0x0100;
    fx_draw_step_y = 0x0100;
    sprite_fa5a(W16(0x0D6D), W16(0x0D6F), 0x1B18);
    sprite_fa5a(W16(0x0D6D), (uint16_t)(W16(0x0D6F) + 0x18), 0x1D18);
    fx_draw_rows = 0x0200;
    fx_draw_step_y = 0x0080;
    sprite_fa5a(W16(0x0D6D), (uint16_t)(W16(0x0D6F) + 8), 0x1A55);
}

/* the point's x moved by entry $0D6B of the table at $07:9806 */
static uint16_t shifted_x(void)
{
    return (uint16_t)(rom7((uint16_t)(0x9806 + W16(0x0D6B))) + W16(0x0D6D));
}

/* GsuFA5ASix: four pieces down from the shifted point, two more 8 to its
   right. */
void gsu_sprites_fa5a_six(void)
{
    static const uint16_t dy[6] = {0, 8, 0x10, 0x18, 8, 0x10};
    static const uint16_t piece[6] = {0x1A0E, 0x1A16, 0x1B16, 0x1B0E, 0x1A0E, 0x1B0E};
    uint16_t x;
    int i;

    unit_scale();
    x = shifted_x();
    for (i = 0; i < 6; i++)
        sprite_fa5a(i < 4 ? x : (uint16_t)(x + 8), (uint16_t)(W16(0x0D6F) + dy[i]), piece[i]);
}

/* GsuFA5ASixLeft: its mirror: four pieces 8 left of the point, two more
   $10 left. */
void gsu_sprites_fa5a_six_left(void)
{
    static const uint16_t dy[6] = {0, 8, 0x10, 0x18, 8, 0x10};
    static const uint16_t piece[6] = {0x1A0C, 0x1A14, 0x1B14, 0x1B0C, 0x1A0C, 0x1B0C};
    int i;

    unit_scale();
    for (i = 0; i < 6; i++)
        sprite_fa5a((uint16_t)(W16(0x0D6D) - (i < 4 ? 8 : 0x10)), (uint16_t)(W16(0x0D6F) + dy[i]), piece[i]);
}

/* GsuFA5AThreeShifted: as gsu_sprites_fa5a_three from the shifted point,
   pieces $1A12, $1B12, $1C18. */
void gsu_sprites_fa5a_three_shifted(void)
{
    uint16_t x;

    unit_scale();
    x = shifted_x();
    sprite_fa5a(x, W16(0x0D6F), 0x1A12);
    sprite_fa5a(x, (uint16_t)(W16(0x0D6F) + 0x18), 0x1B12);
    fx_draw_rows = 0x0200;
    fx_draw_step_y = 0x0080;
    sprite_fa5a(x, (uint16_t)(W16(0x0D6F) + 8), 0x1C18);
}

/* GsuFA35FromTables: as gsu_sprite_fa1b_from_tables with the tables at
   $07:97EE and $97FA, piece $1C00, program $07:FA35. */
int gsu_sprite_fa35_from_tables(void)
{
    uint16_t at = W16(0x0D6B), w;

    gsu_inputs_200(0xBF, 0xBC, 0xBD);
    fx_draw_x = W16(0x0D6D);
    fx_draw_y = (uint16_t)(W16(0x0D6F) + 8);
    fx_draw_rows = 0x0100;
    fx_draw_step_y = 0x0100;
    w = rom7((uint16_t)(0x97EE + at));
    if (!w)
        return 0;
    fx_draw_advance = w;
    fx_draw_step_x = rom7((uint16_t)(0x97FA + at));
    gsu_in_216 = 0x1C00;
    gsu_run(0x07, 0xFA35);
    return 1;
}

/* GsuF15BTwoLines: "..." lines: three pieces from ($50, 8), thirteen
   from (0, $2C), `rows` high. */
void gsu_glyphs_two_lines(uint16_t rows)
{
    static const uint16_t first[3] = {0x8800, 0x8418, 0x8C14};
    static const uint16_t second[13] = {0x8408, 0x8814, 0x8814, 0x0000, 0x8C14, 0x8804, 0x8418,
                                        0x0000, 0x8C10, 0x8C14, 0x8408, 0x8C0C, 0x8C10};

    gsu_inputs_200(0xBF, 0xB7, 0xB6);
    glyphs_f15b(0x0050, 0x0008, rows, first, 3);
    fx_draw_x = 0x0000;
    fx_draw_y = 0x002C;
    {
        int i;

        for (i = 0; i < 13; i++) {
            gsu_in_216 = second[i];
            gsu_run(0x07, 0xF15B);
        }
    }
}

/* GlyphRowsAt32, GlyphRowsAt56: the Stunt Trax panel's lines at y $28. */
void gsu_glyph_rows_at_32(uint16_t rows)
{
    static const uint16_t piece[9] = {0x881C, 0x8418, 0x9004, 0x8C14, 0x0000, 0x8408, 0x8C0C, 0x8418, 0x8408};

    gsu_inputs_200(0xBF, 0xB7, 0xB6);
    glyphs_f15b(0x0020, 0x0028, rows, piece, 9);
}

void gsu_glyph_rows_at_56(uint16_t rows)
{
    static const uint16_t piece[6] = {0x841C, 0x8808, 0x881C, 0x8808, 0x8C10, 0x8804};

    gsu_inputs_200(0xBF, 0xB7, 0xB6);
    glyphs_f15b(0x0038, 0x0028, rows, piece, 6);
}

/* HudSixGlyphs: six pieces from ($10, $18). */
void gsu_glyphs_six(void)
{
    static const uint16_t piece[6] = {0x8C10, 0x8C04, 0x8814, 0x8408, 0x8C10, 0x8804};

    gsu_inputs_200(0x0F, 0x06, 0x0E);
    glyphs_f15b(0x0010, 0x0018, 0x0020, piece, 6);
}

/* GsuPiecesStep10CB: the counter $10CB stepped (to $14 at most) and eleven
   pieces from ($10, $28), their rows from its entry in the table at
   $07:85CC; rows of program $07:F15B below 8, of $07:F121 from then; the
   x the first leaves moved on by 4. */
void gsu_pieces_step_10cb(void)
{
    static const uint16_t piece[11] = {0x800C, 0x8814, 0x8408, 0x8C04, 0x8C10, 0x0000,
                                       0x8C14, 0x8C00, 0x0000, 0x8800, 0x8C00};
    uint16_t n = (uint16_t)(W16(0x10CB) + 1);
    int i;

    if (n >= 0x14)
        n = 0x14;
    W16(0x10CB) = n;
    fx_draw_advance = 0x0010;
    fx_draw_x = 0x0010;
    fx_draw_y = 0x0028;
    gsu_inputs_200(0xBF, 0xBA, 0xB9);
    fx_draw_rows = rom_at(0x070000u | (uint16_t)(0x85CC + n))[0];
    for (i = 0; i < 11; i++) {
        gsu_in_216 = piece[i];
        gsu_run(0x07, n < 8 ? 0xF15B : 0xF121);
        if (!i)
            fx_draw_x = (uint16_t)(fx_draw_x + 4);
    }
}

/* ---- the big 24 x 40 sprites ---- */

static void sprite_24x40(uint16_t src)
{
    gsu_in_216 = src;
    gsu_run(0x07, 0xFA88);
}

/* BigSpritesCountDraw: as many of eight sprites as the count $1160
   allows, four from ($30, $10), four from ($40, $40); the count up one, to
   12 at most. */
void big_sprites_count_draw(void)
{
    static const uint16_t near_[4] = {0xA018, 0xA000, 0xA518, 0xA00C}, far_[4] = {0xAA06, 0xB00C, 0xA00C, 0xAA12};
    static const uint8_t need[8] = {0, 1, 2, 3, 6, 7, 8, 9};
    int i;

    fx_draw_step_x = 0x0100;
    fx_draw_step_y = 0x0100;
    fx_draw_x = 0x0030;
    fx_draw_y = 0x0010;
    for (i = 0; i < 8 && W8(0x1160) >= need[i]; i++) {
        if (i == 4) {
            fx_draw_x = 0x0040;
            fx_draw_y = 0x0040;
        }
        sprite_24x40(i < 4 ? near_[i] : far_[i - 4]);
    }
    if (i == 4)                         /* (the second row's place set before its count is looked at) */
        fx_draw_x = 0x0040, fx_draw_y = 0x0040;
    {
        uint8_t n = (uint8_t)(W8(0x1160) + 1);

        W8(0x1160) = n >= 0x0C ? 0x0C : n;
    }
}

/* BigSpritesRow6, BigSpritesRow5 (and the counted draw instead when $0E5C
   is set): six sprites from ($18, $28), the last two $10 past where the
   fourth left off; or five from ($20, $28). */
void big_sprites_row(int six)
{
    static const uint16_t row6[6] = {0xB000, 0xA506, 0xA518, 0xA00C, 0xB006, 0xAA0C};
    static const uint16_t row5[5] = {0xA006, 0xAA12, 0xA000, 0xAA18, 0xA500};
    int i;

    if (six)
        gsu_inputs_200(0xBF, 0xB5, 0xBB);
    else
        gsu_inputs_200(0xBF, 0xBC, 0xBD);
    if (W8(0x0E5C)) {
        big_sprites_count_draw();
        return;
    }
    fx_draw_x = six ? 0x0018 : 0x0020;
    fx_draw_y = 0x0028;
    fx_draw_step_x = 0x0100;
    fx_draw_step_y = 0x0100;
    for (i = 0; i < (six ? 6 : 5); i++) {
        if (six && i == 4)
            fx_draw_x = (uint16_t)(fx_draw_x + 0x10);
        sprite_24x40(six ? row6[i] : row5[i]);
    }
}

/* ---- the glyph list and the glyph sprites ---- */

/* GsuF329 at ($020A, $60): GlyphAtY60 */
static void glyph_at_y60(uint16_t glyph)
{
    gsu_in_216 = glyph;
    fx_draw_y = 0x0060;
    gsu_inputs_200(0xBF, 0xB5, 0xBB);
    gsu_run(0x07, 0xF329);
}

/* GlyphRowY60From0-5: the row of six glyphs at y $60 from x $78 down by
   8, from glyph `from` on. */
void glyph_row_y60(int from)
{
    static const uint16_t glyph[6] = {0x9808, 0x9808, 0x9808, 0x941A, 0x960E, 0x9414};
    int i;

    for (i = from; i < 6; i++) {
        fx_draw_x = (uint16_t)(0x0078 - 8 * i);
        glyph_at_y60(glyph[i]);
    }
}

/* GlyphListDraw: each entry of the list that is on ($110E), its glyph
   ($10E8) at its place (bytes $1134, $1135), the first $10E4 in one set of
   colours and the rest ($10E6 in all) in another. */
void glyph_list_draw(void)
{
    uint16_t first = (uint16_t)(W16(0x10E4) << 1), end = (uint16_t)(W16(0x10E6) << 1), x = 0;

    do {
        if (W16(0x110E + x)) {
            gsu_in_216 = W16(0x10E8 + x);
            fx_draw_x = W8(0x1134 + x);
            fx_draw_y = W8(0x1135 + x);
            if (x >= first)
                gsu_inputs_200(0xBF, 0xB3, 0xBE);
            else
                gsu_inputs_200(0xBF, 0xBC, 0xBD);
            gsu_run(0x07, 0xF329);
        }
        x = (uint16_t)(x + 2);
    } while (x < end);
}

/* GlyphSpriteDraw: glyph sprite `k` (glyph $1010, place $0FB0, $0FE0). */
void glyph_sprite_draw(uint16_t k)
{
    fx_draw_x = W16(0x0FB0 + 2 * k);
    fx_draw_y = W16(0x0FE0 + 2 * k);
    gsu_in_216 = W16(0x1010 + 2 * k);
    gsu_run(0x07, 0xF329);
}

/* GlyphSpritesInit: the 16 glyph sprites at y 8, their x and glyph from
   the tables at $04:DA7A and $04:DA8E, off ($1040); the times' display
   not started ($10D8), $115A = $1E, $0D5B and the HUD hold cleared. */
void glyph_sprites_init(void)
{
    int k;

    for (k = 0; k < 16; k++) {
        W16(0x0FB0 + 2 * k) = rom_word(0x04DA7Au + 2 * k);
        W16(0x0FE0 + 2 * k) = 0x0008;
        W16(0x1010 + 2 * k) = rom_word(0x04DA8Eu + 2 * k);
        W16(0x1040 + 2 * k) = 0;
    }
    W16(0x10D8) = 0;
    W16(0x115A) = 0x001E;
    W8(0x0D5B) = 0;
    W8(0x0E5D) = 0;
}

/* one entry of the glyph list: its glyph, on, at (x, y) */
static void list_entry(uint16_t e, uint16_t glyph, uint8_t x, uint8_t y)
{
    W16(0x10E8 + e) = glyph;
    W8(0x1134 + e) = x;
    W8(0x1135 + e) = y;
}

/* GlyphListMessageAndTime: message `m` (the string its word in the table
   at $04:DF6D points to: a length, then a glyph a letter, 0 a space)
   centred on x $68 at y $58, then the time at y $68 from x $4C by 8: the
   digits of direct page $F7 (one), $F6 and $F5 (two each) with the marks
   $980A and $980C between. $10E4 = the letters - 1, $10E6 = that + 7. */
void glyph_list_message_and_time(uint16_t m)
{
    uint16_t at = rom_word(0x040000u | (uint16_t)(0xDF6D + 2 * m)), n, e = 0;
    uint8_t x;
    int i;

    n = rom_at(0x040000u | at)[0];
    x = (uint8_t)(0x68 - n * 4);
    W16(0x10E4) = (uint16_t)(n - 1);
    W16(0x10E6) = (uint16_t)(n + 6);
    do {
        uint16_t g = rom_word(0x040000u | (uint16_t)(at + 1));

        if (g) {
            list_entry(e, g, x, 0x58);
            W8(0x110E + e) = 0;         /* (only its low byte) */
            e = (uint16_t)(e + 2);
        }
        x = (uint8_t)(x + 8);
        at = (uint16_t)(at + 2);
    } while (--n);
    {
        static const struct {
            uint8_t from, high;         /* the direct page byte, its high digit; or a mark */
            uint16_t mark;
        } time[7] = {{0xF7, 0, 0}, {0, 0, 0x980A}, {0xF6, 1, 0}, {0xF6, 0, 0}, {0, 0, 0x980C}, {0xF5, 1, 0}, {0xF5, 0, 0}};

        for (i = 0; i < 7; i++, e = (uint16_t)(e + 2)) {
            uint16_t g = time[i].mark;

            if (!g) {
                uint8_t b = W8(time[i].from);

                g = (uint16_t)(0x9400 + 2 * (time[i].high ? b >> 4 : b & 0x0F));
            }
            list_entry(e, g, (uint8_t)(0x4C + 8 * i), 0x68);
            W16(0x110E + e) = 0;
        }
    }
}

/* HudHoldDraw: with the HUD hold $0E5D set, the time at $0F69 at ($93,
   $6C). */
void hud_hold_draw(void)
{
    if (!W16(0x0E5D))
        return;
    gsu_inputs_200(0xBF, 0xB3, 0xBE);
    fx_draw_x = 0x0093;
    fx_draw_y = 0x006C;
    gsu_glyph_f329_sequence(0x0F69);
}

/* ---- objects ---- */

/* ObjectGsu80BCThen81E3: object `obj`'s length from the point (to direct
   page $08), its height from $000A (to $02), and the angle of the two. */
uint16_t object_length_and_angle(uint16_t obj)
{
    W16(0x0008) = gsu_object_length(obj);
    W16(0x0002) = (uint16_t)(W16(0x000A) - W16(0x200E + obj));
    return gsu_vector_angle();
}

/* ObjectGsu81E3: the angle of object `obj`'s offset from the point
   (direct page $04, $05F2) in x and z. */
uint16_t object_angle(uint16_t obj)
{
    W16(0x0002) = (uint16_t)(W16(0x0004) - W16(0x200C + obj));
    W16(0x0008) = (uint16_t)(W16(0x05F2) - W16(0x2010 + obj));
    return gsu_vector_angle();
}
