/*
 * glyphs.c: the HUD's glyphs and sprites, drawn by the Super FX's picture
 * programs from their inputs in the chip's RAM.
 *
 * The colour setters of the original, GsuBytes200_* at 04:D7A2 to 04:DA03
 * (twenty-one of them, one per four bytes), are GlyphColours here.
 *
 * Not converted, nothing calls them (no JSR, JSL, pointer or table):
 * GsuPiecesFromRecord, GsuPiecesListFAF2, GsuPiecesReturn, GsuPiecesListF6DF,
 * GsuPieceOffsetFromView, GsuReciprocal, GsuPieceScaleFromView,
 * GsuReciprocalReturn, GsuPiecesFromBytes, GsuClearScreenIfSet,
 * GsuClear6000IfSet, HudTimesDrawEven, HudTimesPackedEven.
 */
#include "audio/sound.h"
#include "data/cart.h"
#include "fx/fx.h"
#include "hud/hud.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"
#include "video/video.h"

void GlyphColours(uint8_t b1, uint8_t b2, uint8_t b3)
{
    fx8_set(FXIN_GLYPH_BYTES + 0, 0x00);
    fx8_set(FXIN_GLYPH_BYTES + 1, b1);
    fx8_set(FXIN_GLYPH_BYTES + 2, b2);
    fx8_set(FXIN_GLYPH_BYTES + 3, b3);
}

/* $70:0216 from the word tables at $07:8E3E and $07:8E52 (GsuParam216A, B). */
static void param_216_a(uint16_t index) { fx16_set(FXIN_PIECE, cart16(0x07, (uint16_t)(0x8E3E + index))); }
static void param_216_b(uint16_t index) { fx16_set(FXIN_PIECE, cart16(0x07, (uint16_t)(0x8E52 + index))); }

void GsuParam216A(uint16_t digit) { param_216_a((uint16_t)(digit << 1)); }

/* @routine 07:8B9B GsuF329Entry: a digit (low four bits) through the table to program $07:F329. */
static void f329_entry(uint16_t digit)
{
    param_216_b((uint16_t)((digit & 0x000F) << 1));
    FxGlyph8x16();
}

/* @routine 07:A684 GsuF441Player1, 07:A694 GsuF441Player2: program $07:F441, the kind from the player mode. */
static void f441(uint8_t kind)
{
    if ((kind & 0x0F) == 1) {
        GlyphColours(0x0F, 0x05, 0x0E);
        fx16_set(FXIN_PIECE, 0x080E);
    } else {
        GlyphColours(0x0F, 0x08, 0x0E);
        fx16_set(FXIN_PIECE, 0x0A0E);
    }
    fx16_set(FXIN_PIECE_X, 0x0074);
    fx16_set(FXIN_PIECE_Y, 0x003C);
    FxGlyph24x16();
}

void GsuF441Player1(void) { f441(g.player_mode[0]); }
void GsuF441Player2(void) { f441(g.player_mode[1]); }

/*
 * @routine 07:85E1 GsuF0A5OrF0B4
 * WHAT IT DOES: The input at $020C, then the split-screen picture rise or
 * the one-player one.
 */
void GsuF0A5OrF0B4(uint16_t input)
{
    fx16_set(FXIN_PIECE_Y, input);
    if (g.split_screen) {
        GlyphColours(0x0F, 0x09, 0x0A);
        FxPictureRise40();
    } else {
        GlyphColours(0xBF, 0xBC, 0xBD);
        FxPictureRise60();
    }
}

/* @routine 07:8619 GsuEEF5OrEFCD: as above, for the last lap's banner. */
void GsuEEF5OrEFCD(uint16_t input)
{
    fx16_set(FXIN_PIECE_Y, input);
    if (g.split_screen) {
        GlyphColours(0x0F, 0x04, 0x05);
        FxPictureRiseSplit40();
    } else {
        GlyphColours(0xBF, 0xB7, 0xB6);
        FxPictureRiseSplit60();
    }
}

/* @routine 07:9252 GsuDigitPair0E74: the two digits of the byte at $0E74. */
void GsuDigitPair0E74(void)
{
    fx16_set(FXIN_PIECE_W, 0x0010);
    fx16_set(FXIN_PIECE_X, 0x0004);
    fx16_set(FXIN_PIECE_Y, 0x0008);
    param_216_a((uint16_t)((g.digit_pair >> 4 & 0x000F) << 1));
    FxGlyph16RowsFromTop();
    param_216_a((uint16_t)((g.digit_pair & 0x000F) << 1));
    FxGlyph16RowsFromTop();
}

static void f15b_setup(uint16_t x, uint16_t y)
{
    fx16_set(FXIN_PIECE_H, 0x0020);
    fx16_set(FXIN_PIECE_W, 0x0010);
    fx16_set(FXIN_PIECE_X, x);
    fx16_set(FXIN_PIECE_Y, y);
}

static void f15b_pieces(const uint16_t *pieces, int n)
{
    for (int i = 0; i < n; i++) {
        fx16_set(FXIN_PIECE, pieces[i]);
        FxGlyph16RowsFromTop();
    }
}

/* @routine 07:9883 GsuF15BThree */
void GsuF15BThree(void)
{
    static const uint16_t p[3] = {0x9000, 0x8808, 0x881C};

    f15b_setup(0x0030, 0x0018);
    GlyphColours(0x0F, 0x04, 0x05);
    f15b_pieces(p, 3);
}

/* @routine 07:997F GsuF15BFive */
void GsuF15BFive(void)
{
    static const uint16_t p[5] = {0x8410, 0x8C0C, 0x8408, 0x8C10, 0x8804};

    f15b_setup(0x0018, 0x0018);
    GlyphColours(0x0F, 0x09, 0x0A);
    f15b_pieces(p, 5);
}

/* @routine 07:98F4 GsuF15BFourA */
void GsuF15BFourA(void)
{
    static const uint16_t p[4] = {0x8814, 0x8C00, 0x8C10, 0x8418};

    f15b_setup(0x0028, 0x0018);
    GlyphColours(0x0F, 0x07, 0x08);
    f15b_pieces(p, 4);
}

/* @routine 07:9A24 GsuF15BFourB */
void GsuF15BFourB(void)
{
    static const uint16_t p[4] = {0x8414, 0x8C0C, 0x8408, 0x9000};

    f15b_setup(0x0028, 0x0018);
    GlyphColours(0x0F, 0x0D, 0x03);
    f15b_pieces(p, 4);
}

/* @routine 07:8653 GsuF15BTwoLines: sixteen pieces, three from ($50, 8) and thirteen from (0, $2C). */
void GsuF15BTwoLines(uint16_t input)
{
    static const uint16_t first[3] = {0x8800, 0x8418, 0x8C14};
    static const uint16_t second[13] = {0x8408, 0x8814, 0x8814, 0x0000, 0x8C14, 0x8804, 0x8418,
                                        0x0000, 0x8C10, 0x8C14, 0x8408, 0x8C0C, 0x8C10};

    fx16_set(FXIN_PIECE_H, input);
    fx16_set(FXIN_PIECE_W, 0x0010);
    fx16_set(FXIN_PIECE_X, 0x0050);
    fx16_set(FXIN_PIECE_Y, 0x0008);
    GlyphColours(0xBF, 0xB7, 0xB6);
    f15b_pieces(first, 3);
    fx16_set(FXIN_PIECE_X, 0x0000);
    fx16_set(FXIN_PIECE_Y, 0x002C);
    f15b_pieces(second, 13);
}

static void glyph_rows_at(uint16_t input, uint16_t x, const uint16_t *pieces, int n)
{
    fx16_set(FXIN_PIECE_H, input);
    fx16_set(FXIN_PIECE_W, 0x0010);
    fx16_set(FXIN_PIECE_X, x);
    fx16_set(FXIN_PIECE_Y, 0x0028);
    GlyphColours(0xBF, 0xB7, 0xB6);
    f15b_pieces(pieces, n);
}

/* @routine 07:8821 GlyphRowsAt32 */
void GlyphRowsAt32(uint16_t input)
{
    static const uint16_t p[9] = {0x881C, 0x8418, 0x9004, 0x8C14, 0x0000, 0x8408, 0x8C0C, 0x8418, 0x8408};

    glyph_rows_at(input, 0x0020, p, 9);
}

/* @routine 07:8929 GlyphRowsAt56 */
void GlyphRowsAt56(uint16_t input)
{
    static const uint16_t p[6] = {0x841C, 0x8808, 0x881C, 0x8808, 0x8C10, 0x8804};

    glyph_rows_at(input, 0x0038, p, 6);
}

static void fa5a_scale(uint16_t w, uint16_t h, uint16_t u, uint16_t v)
{
    fx16_set(FXIN_PIECE_W, w);
    fx16_set(FXIN_PIECE_H, h);
    fx16_set(FXIN_PIECE_U, u);
    fx16_set(FXIN_PIECE_V, v);
}

static void fa5a(uint16_t x, uint16_t y, uint16_t piece)
{
    fx16_set(FXIN_PIECE_X, x);
    fx16_set(FXIN_PIECE_Y, y);
    fx16_set(FXIN_PIECE, piece);
    FxScaledSpriteEighth(0);
}

/* @routine 07:976B GsuFA1BFromTables */
void GsuFA1BFromTables(void)
{
    uint16_t v;

    GlyphColours(0xBF, 0xBC, 0xBD);
    fx16_set(FXIN_PIECE_X, g.hud_point_x);
    fx16_set(FXIN_PIECE_Y, (uint16_t)(g.hud_point_y + 8));
    fx16_set(FXIN_PIECE_H, 0x0100);
    fx16_set(FXIN_PIECE_V, 0x0100);
    v = cart16(0x07, (uint16_t)(0x97D6 + g.hud_point_index));
    if (!v)
        return;
    fx16_set(FXIN_PIECE_W, v);
    fx16_set(FXIN_PIECE_U, cart16(0x07, (uint16_t)(0x97E2 + g.hud_point_index)));
    fx16_set(FXIN_PIECE, 0x1E00);
    FxScaledSprite13(0);
}

/* @routine 07:9718 GsuFA35FromTables */
void GsuFA35FromTables(void)
{
    uint16_t v;

    GlyphColours(0xBF, 0xBC, 0xBD);
    fx16_set(FXIN_PIECE_X, g.hud_point_x);
    fx16_set(FXIN_PIECE_Y, (uint16_t)(g.hud_point_y + 8));
    fx16_set(FXIN_PIECE_H, 0x0100);
    fx16_set(FXIN_PIECE_V, 0x0100);
    v = cart16(0x07, (uint16_t)(0x97EE + g.hud_point_index));
    if (!v)
        return;
    fx16_set(FXIN_PIECE_W, v);
    fx16_set(FXIN_PIECE_U, cart16(0x07, (uint16_t)(0x97FA + g.hud_point_index)));
    fx16_set(FXIN_PIECE, 0x1C00);
    FxScaledSprite12(0);
}

/* @routine 07:92B5 GsuFA5AThree: three pieces around the point. */
void GsuFA5AThree(void)
{
    uint16_t x = (uint16_t)(g.hud_point_x - 8), y = g.hud_point_y;

    fa5a_scale(0x0100, 0x0100, 0x0100, 0x0100);
    fa5a(x, y, 0x1A10);
    fa5a(x, (uint16_t)(y + 0x18), 0x1B10);
    fa5a_scale(0x0100, 0x0200, 0x0100, 0x0080);
    fa5a(x, (uint16_t)(y + 8), 0x1A18);
}

/* @routine 07:9486 GsuFA5AThreeShifted */
void GsuFA5AThreeShifted(void)
{
    uint16_t x = (uint16_t)(cart16(0x07, (uint16_t)(0x9806 + g.hud_point_index)) + g.hud_point_x);
    uint16_t y = g.hud_point_y;

    fa5a_scale(0x0100, 0x0100, 0x0100, 0x0100);
    fa5a(x, y, 0x1A12);
    fa5a(x, (uint16_t)(y + 0x18), 0x1B12);
    fa5a_scale(0x0100, 0x0200, 0x0100, 0x0080);
    fa5a(x, (uint16_t)(y + 8), 0x1C18);
}

/* @routine 07:9366 GsuFA5ASix */
void GsuFA5ASix(void)
{
    static const uint16_t dy[6] = {0x0000, 0x0008, 0x0010, 0x0018, 0x0008, 0x0010};
    static const uint16_t piece[6] = {0x1A0E, 0x1A16, 0x1B16, 0x1B0E, 0x1A0E, 0x1B0E};
    uint16_t x = (uint16_t)(cart16(0x07, (uint16_t)(0x9806 + g.hud_point_index)) + g.hud_point_x);

    fa5a_scale(0x0100, 0x0100, 0x0100, 0x0100);
    for (int i = 0; i < 6; i++) {
        if (i == 4)
            x = (uint16_t)(x + 8);
        fa5a(x, (uint16_t)(g.hud_point_y + dy[i]), piece[i]);
    }
}

/* @routine 07:9533 GsuFA5ASixLeft */
void GsuFA5ASixLeft(void)
{
    static const uint16_t dy[6] = {0x0000, 0x0008, 0x0010, 0x0018, 0x0008, 0x0010};
    static const uint16_t dx[6] = {0xFFF8, 0xFFF8, 0xFFF8, 0xFFF8, 0xFFF0, 0xFFF0};
    static const uint16_t piece[6] = {0x1A0C, 0x1A14, 0x1B14, 0x1B0C, 0x1A0C, 0x1B0C};

    fa5a_scale(0x0100, 0x0100, 0x0100, 0x0100);
    for (int i = 0; i < 6; i++)
        fa5a((uint16_t)(g.hud_point_x + dx[i]), (uint16_t)(g.hud_point_y + dy[i]), piece[i]);
}

/* @routine 07:9660 GsuFA5AFromTables */
void GsuFA5AFromTables(void)
{
    uint16_t i = g.hud_point_index;

    fa5a_scale(cart16(0x07, (uint16_t)(0x97BE + i)), 0x0100, cart16(0x07, (uint16_t)(0x97CA + i)), 0x0100);
    fa5a(g.hud_point_x, g.hud_point_y, 0x1B18);
    fx16_set(FXIN_PIECE_H, 0x0100);
    fx16_set(FXIN_PIECE_V, 0x0100);
    fa5a(g.hud_point_x, (uint16_t)(g.hud_point_y + 0x18), 0x1D18);
    fx16_set(FXIN_PIECE_H, 0x0200);
    fx16_set(FXIN_PIECE_V, 0x0080);
    fa5a(g.hud_point_x, (uint16_t)(g.hud_point_y + 8), 0x1A55);
}

/* @routine 07:833B GsuPiecesStep10CB: steps the counter at $10CB and draws eleven pieces. */
void GsuPiecesStep10CB(void)
{
    static const uint16_t piece[11] = {0x800C, 0x8814, 0x8408, 0x8C04, 0x8C10, 0x0000,
                                       0x8C14, 0x8C00, 0x0000, 0x8800, 0x8C00};
    uint16_t n = (uint16_t)(g.piece_step + 1);

    if (n >= 0x0014)
        n = 0x0014;
    g.piece_step = n;
    fx16_set(FXIN_PIECE_W, 0x0010);
    fx16_set(FXIN_PIECE_X, 0x0010);
    fx16_set(FXIN_PIECE_Y, 0x0028);
    GlyphColours(0xBF, 0xBA, 0xB9);
    fx16_set(FXIN_PIECE_H, cart8(0x07, (uint16_t)(0x85CC + n)));
    for (int i = 0; i < 11; i++) {
        fx16_set(FXIN_PIECE, piece[i]);
        if (n < 8)
            FxGlyph16RowsFromTop();
        else
            FxGlyph16RowsFromBottom();
        if (i == 0)
            fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) + 4));
    }
}

/* @routine 07:9AB0 HudSixGlyphs */
void HudSixGlyphs(void)
{
    static const uint16_t glyph[6] = {0x8C10, 0x8C04, 0x8814, 0x8408, 0x8C10, 0x8804};

    GlyphColours(0x0F, 0x06, 0x0E);
    f15b_setup(0x0010, 0x0018);
    f15b_pieces(glyph, 6);
}

/* A 24 x 40 sprite, piece src. */
static void sprite24x40(uint16_t src)
{
    fx16_set(FXIN_PIECE, src);
    FxSprite24x40(0);
}

/*
 * @routine 07:9EE9 BigSpritesCountDraw
 * WHAT IT DOES: The sprite rows when $0E5C is set: 24 x 40 sprites as many
 * as the count $1160 allows, then the count goes up by one, to at most 12.
 */
static void big_sprites_count_draw(void)
{
    static const uint16_t near_[4] = {0xA018, 0xA000, 0xA518, 0xA00C}, far_[4] = {0xAA06, 0xB00C, 0xA00C, 0xAA12};
    static const uint8_t need_far[3] = {7, 8, 9};

    fx16_set(FXIN_PIECE_U, 0x0100);
    fx16_set(FXIN_PIECE_V, 0x0100);
    fx16_set(FXIN_PIECE_X, 0x0030);
    fx16_set(FXIN_PIECE_Y, 0x0010);
    for (int i = 0; i < 4; i++) {
        sprite24x40(near_[i]);
        if (i < 3 && g.sprite_count < i + 1)
            goto count;
    }
    fx16_set(FXIN_PIECE_X, 0x0040);
    fx16_set(FXIN_PIECE_Y, 0x0040);
    if (g.sprite_count < 6)
        goto count;
    for (int i = 0; i < 4; i++) {
        sprite24x40(far_[i]);
        if (i < 3 && g.sprite_count < need_far[i])
            goto count;
    }
count:
    if (++g.sprite_count >= 0x0C)
        g.sprite_count = 0x0C;
}

/* The common start of the sprite rows: returns 0 when $0E5C sent it to the
   counted form instead. */
static int big_sprites_start(uint8_t b1, uint8_t b2, uint8_t b3)
{
    GlyphColours(b1, b2, b3);
    if (g.sprites_counted) {
        big_sprites_count_draw();
        return 0;
    }
    return 1;
}

static void big_sprites_at(uint16_t x, uint16_t y)
{
    fx16_set(FXIN_PIECE_X, x);
    fx16_set(FXIN_PIECE_Y, y);
    fx16_set(FXIN_PIECE_U, 0x0100);
    fx16_set(FXIN_PIECE_V, 0x0100);
}

/* @routine 07:9D67 BigSpritesRow6 */
void BigSpritesRow6(void)
{
    static const uint16_t first[4] = {0xB000, 0xA506, 0xA518, 0xA00C}, second[2] = {0xB006, 0xAA0C};

    if (!big_sprites_start(0xBF, 0xB5, 0xBB))
        return;
    big_sprites_at(0x0018, 0x0028);
    for (int i = 0; i < 4; i++)
        sprite24x40(first[i]);
    fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) + 0x0010));
    for (int i = 0; i < 2; i++)
        sprite24x40(second[i]);
}

/* @routine 07:9C9F BigSpritesRow6B */
void BigSpritesRow6B(void)
{
    static const uint16_t src[6] = {0xAA18, 0xAA0C, 0xA512, 0xA000, 0xAA18, 0xA500};

    if (!big_sprites_start(0xBF, 0xB3, 0xBE))
        return;
    big_sprites_at(0x0018, 0x0028);
    for (int i = 0; i < 6; i++)
        sprite24x40(src[i]);
}

/* @routine 07:9E3B BigSpritesRow5 */
void BigSpritesRow5(void)
{
    static const uint16_t src[5] = {0xA006, 0xAA12, 0xA000, 0xAA18, 0xA500};

    if (!big_sprites_start(0xBF, 0xBC, 0xBD))
        return;
    big_sprites_at(0x0020, 0x0028);
    for (int i = 0; i < 5; i++)
        sprite24x40(src[i]);
}

/* @routine 07:9B6F BigSpritesNine */
void BigSpritesNine(void)
{
    static const uint16_t first[3] = {0xAA06, 0xB006, 0xB000}, second[2] = {0xAA06, 0xA012};
    static const uint16_t third[4] = {0xAA12, 0xA000, 0xAA00, 0xA50C};

    if (!big_sprites_start(0xBF, 0xB7, 0xB6))
        return;
    big_sprites_at(0x0029, 0x0021);
    for (int i = 0; i < 3; i++)
        sprite24x40(first[i]);
    fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) + 0x0006));
    for (int i = 0; i < 2; i++)
        sprite24x40(second[i]);
    fx16_set(FXIN_PIECE_X, 0x0038);
    fx16_set(FXIN_PIECE_Y, 0x0050);
    for (int i = 0; i < 4; i++)
        sprite24x40(third[i]);
}

/* @routine 07:8AA6 GsuF329Sequence: five digits with two separators, at the position set. */
static void f329_sequence(const uint8_t *digits)
{
    f329_entry(digits[0]);
    fx16_set(FXIN_PIECE, 0x980A);
    FxGlyph8x16();
    f329_entry(digits[1]);
    f329_entry(digits[2]);
    fx16_set(FXIN_PIECE, 0x980C);
    FxGlyph8x16();
    f329_entry(digits[3]);
    f329_entry(digits[4]);
}

static void times_at(uint16_t y)
{
    fx16_set(FXIN_PIECE_X, 0x0093);
    fx16_set(FXIN_PIECE_Y, y);
}

/* @routine 07:89E4 HudHoldDraw */
void HudHoldDraw(void)
{
    if (!g.hud_hold)
        return;
    GlyphColours(0xBF, 0xB3, 0xBE);
    times_at(0x006C);
    f329_sequence(g.best_digits);
}

/* @routine 04:DDB0 GlyphAtY60: a glyph at (x, 96). */
void GlyphAtY60(uint16_t glyph, uint16_t x)
{
    fx16_set(FXIN_PIECE_X, x);
    fx16_set(FXIN_PIECE, glyph);
    fx16_set(FXIN_PIECE_Y, 0x0060);
    GlyphColours(0xBF, 0xB5, 0xBB);
    FxGlyph8x16();
}

/* @routine 04:DD61 GlyphRowY60From0 and its entries DD6E, DD7B, DD88, DD95, DDA2 */
static void glyph_row(int from)
{
    static const uint16_t xs[6] = {0x0078, 0x0070, 0x0068, 0x0060, 0x0058, 0x0050};
    static const uint16_t gs[6] = {0x9808, 0x9808, 0x9808, 0x941A, 0x960E, 0x9414};

    for (int i = from; i < 6; i++)
        GlyphAtY60(gs[i], xs[i]);
}

/*
 * @routine 04:DDD1 GlyphListDraw
 * WHAT IT DOES: Draws the glyph list: each entry whose flag is set, the
 * first group in one colour and the rest in another.
 */
void GlyphListDraw(void)
{
    int i = 0;

    do {
        if (!g.glyph_on[i])
            continue;
        fx16_set(FXIN_PIECE, g.glyph_word[i]);
        fx16_set(FXIN_PIECE_X, g.glyph_x_y[i * 2]);
        fx16_set(FXIN_PIECE_Y, g.glyph_x_y[i * 2 + 1]);
        if (i >= g.glyph_first_end)
            GlyphColours(0xBF, 0xB3, 0xBE);
        else
            GlyphColours(0xBF, 0xBC, 0xBD);
        FxGlyph8x16();
    } while (++i < g.glyph_end);
}

/* @routine 07:A426 GlyphSpriteDraw: sprite `sprite` at its own place. */
void GlyphSpriteDraw(int sprite)
{
    fx16_set(FXIN_PIECE_X, g.glyph_sprite_x[sprite]);
    fx16_set(FXIN_PIECE_Y, g.glyph_sprite_y[sprite]);
    fx16_set(FXIN_PIECE, g.glyph_sprite_glyph[sprite]);
    FxGlyph8x16();
}

/* @routine 07:A41D GlyphSpriteDrawLeft8 */
static void glyph_sprite_draw_left8(int sprite)
{
    fx16_set(FXIN_PIECE_X, (uint16_t)(g.glyph_sprite_x[sprite] - 8));
    fx16_set(FXIN_PIECE_Y, g.glyph_sprite_y[sprite]);
    fx16_set(FXIN_PIECE, g.glyph_sprite_glyph[sprite]);
    FxGlyph8x16();
}

/* @routine 04:DA3C GlyphSpritesInit */
void GlyphSpritesInit(void)
{
    for (int i = 0; i < 16; i++) {
        g.glyph_sprite_x[i] = cart16(0x04, (uint16_t)(0xDA7A + 2 * i));
        g.glyph_sprite_y[i] = 0x0008;
        g.glyph_sprite_glyph[i] = cart16(0x04, (uint16_t)(0xDA8E + 2 * i));
        g.glyph_sprite_step[i] = 0;
    }
    g.times_step = 0;
    g.times_timer = 0x001E;
    g.finish_lines = 0;
    g.hud_hold &= 0xFF00;
}

static uint16_t digit_glyph(uint8_t digit)
{
    return (uint16_t)(0x9400 + ((digit & 0x0F) << 1));
}

static void time_glyph(int *y, uint16_t glyph, uint8_t x)
{
    g.glyph_word[*y] = glyph;
    g.glyph_on[*y] = 0;
    g.glyph_x_y[*y * 2] = x;
    g.glyph_x_y[*y * 2 + 1] = 0x68;
    (*y)++;
}

/*
 * @routine 04:DE2C GlyphListMessageAndTime
 * WHAT IT DOES: Fills the glyph list with message `index` (its address in
 * the table at $04:DF6D: a length byte, then a glyph word per letter, 0 a
 * space) centred on x $68 at y $58, then the time row at y $68 from x $4C.
 */
static void glyph_list_message_and_time(uint16_t index, uint8_t min, uint8_t sec, uint8_t hund)
{
    uint16_t s = cart16(0x04, (uint16_t)(0xDF6D + index));
    uint16_t letters = (uint16_t)(cart16(0x04, s) & 0x00FF), x;
    int y = 0;

    x = (uint16_t)-(uint16_t)((uint16_t)(letters * 4) - 0x0068);
    g.glyph_first_end = (uint16_t)(letters - 1);
    g.glyph_end = (uint16_t)(g.glyph_first_end + 7);
    for (uint16_t n = letters; n; n--) {
        uint16_t w = cart16(0x04, (uint16_t)(s + 1));

        if (w) {
            g.glyph_word[y] = w;
            g.glyph_x_y[y * 2] = (uint8_t)x;
            g.glyph_x_y[y * 2 + 1] = 0x58;
            g.glyph_on[y] = 0;
            y++;
        }
        x = (uint16_t)(x + 8);
        s = (uint16_t)(s + 2);
    }
    time_glyph(&y, digit_glyph(hund), 0x4C);
    time_glyph(&y, 0x980A, 0x54);
    time_glyph(&y, digit_glyph((uint8_t)(sec >> 4)), 0x5C);
    time_glyph(&y, digit_glyph(sec), 0x64);
    time_glyph(&y, 0x980C, 0x6C);
    time_glyph(&y, digit_glyph((uint8_t)(min >> 4)), 0x74);
    time_glyph(&y, digit_glyph(min), 0x7C);
}

/* @routine 07:A03F GlyphWordAt1C28 */
void GlyphWordAt1C28(void)
{
    static const uint16_t g1[5] = {0x840C, 0x8C00, 0x881C, 0x8C18, 0x8C10};
    static const uint16_t g2[4] = {0x8C14, 0x8C0C, 0x8408, 0x9004};

    f15b_setup(0x001C, 0x0028);
    GlyphColours(0xBF, 0xBC, 0xBD);
    f15b_pieces(g1, 5);
    fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) + 4));
    f15b_pieces(g2, 4);
}

/* Three rows of three 8 x 8 glyphs: the glyphs are row * $100 + $1A, $1C, $1E. */
static void glyph_grid3(uint16_t first, int switch_colours)
{
    for (int r = 0; r < 3; r++) {
        if (r) {
            fx16_set(FXIN_PIECE_Y, (uint16_t)(fx16(FXIN_PIECE_Y) + 8));
            fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) - 0x18));
            if (r == 1 && switch_colours)
                GlyphColours(0xBF, 0xB3, 0xBE);
        }
        for (int c = 0; c < 3; c++) {
            if (r == 0 && c == 2 && switch_colours)
                GlyphColours(0xBF, 0xBA, 0xB9);
            fx16_set(FXIN_PIECE, (uint16_t)((first + r) << 8 | (0x1A + 2 * c)));
            FxGlyph8x8();
        }
    }
}

/* @routine 07:A158 GlyphGrid1A */
void GlyphGrid1A(void)
{
    fx16_set(FXIN_PIECE_X, 0x0038);
    fx16_set(FXIN_PIECE_Y, 0x0030);
    GlyphColours(0xBF, 0xB7, 0xB6);
    glyph_grid3(0x1A, 1);
}

/* @routine 07:A27D GlyphGrid1D */
void GlyphGrid1D(void)
{
    fx16_set(FXIN_PIECE_X, 0x0038);
    fx16_set(FXIN_PIECE_Y, 0x0048);
    GlyphColours(0xBF, 0xBB, 0xBE);
    glyph_grid3(0x1D, 0);
}

/* The next step of the times display. */
static void times_next(void) { g.times_step++; }

/* A wait on the frame counter; at 0 the reload (unless 0) and the next
   step. Returns 1 when it ran out. */
static int times_wait(uint16_t reload)
{
    if (--g.times_timer)
        return 0;
    if (reload)
        g.times_timer = reload;
    times_next();
    return 1;
}

/* One time's bytes and its message. */
static void times_load(const uint8_t *from, uint16_t extra)
{
    g.times_entry = 0;
    times_next();
    glyph_list_message_and_time((uint16_t)(g.course_class * 8 + extra), from[0], from[1], from[2]);
}

/* One more entry of the list gets flag `flag`; after the last, the next
   step and the timer `reload`. Then the list is drawn. */
static void times_flag_step(uint16_t flag, uint16_t reload)
{
    g.glyph_on[g.times_entry / 2] = flag;
    g.times_entry = (uint16_t)(g.times_entry + 2);
    if ((uint16_t)(g.times_entry >> 1) >= g.glyph_end) {
        times_next();
        g.times_timer = reload;
    }
    GlyphListDraw();
}

/*
 * @routine 04:DB87 GlyphTimesSequence
 * WHAT IT DOES: One frame of the three best times' display, by its step:
 * two waits; then for each of the course's three times its message and
 * time as a glyph list shown a glyph a frame, a wait of 10, cleared a glyph
 * a frame, a wait of 4; then the row of six glyphs built from its last
 * glyph back to its first; at the end the split window on with sound 4, and
 * after $14 frames the championship's end.
 */
void GlyphTimesSequence(void)
{
    uint16_t step = g.times_step & 0x00FF;

    switch (step) {
    case 0:
        times_wait(0x001E);
        break;
    case 1:
        times_wait(0);
        break;
    case 2:
        times_load(g.best_times[0], 0);
        break;
    case 6:
        times_load(g.best_times[1], 2);
        break;
    case 10:
        times_load(g.best_times[2], 4);
        break;
    case 3: case 7: case 11:
        times_flag_step(0x00FF, 0x000A);
        break;
    case 4: case 8: case 12:
        if (!--g.times_timer) {
            g.times_entry = 0;
            times_next();
        }
        GlyphListDraw();
        break;
    case 5: case 9: case 13:
        times_flag_step(0x0000, 0x0004);
        break;
    case 14:
        g.times_timer = 0x0004;
        times_next();
        break;
    case 15: case 16: case 17: case 18: case 19: case 20: {
        static const uint16_t reload[6] = {0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0004};

        times_wait(reload[step - 15]);
        glyph_row(5 - (int)(step - 15));
        break;
    }
    case 21:
        if (times_wait(0x0014)) {
            windows.bands_state = 0x01;
            windows.bands_kind = 0x00;
            sound_port1_push(0x04);
        }
        glyph_row(0);
        break;
    default:
        if (!--g.times_timer) {
            ChampionshipFinish();
            return;
        }
        glyph_row(0);
        break;
    }
}

/*
 * @routine 04:DAE2 GlyphSpritesGrowAndTimes
 * WHAT IT DOES: Once the display has started: each glyph sprite's size step
 * grows by one once the one before it has reached 3, and every sprite with
 * a step is drawn scaled. Then GlyphTimesSequence.
 */
void GlyphSpritesGrowAndTimes(void)
{
    if (g.times_step) {
        for (int i = 1; i < 10; i++)
            if (g.glyph_sprite_step[i - 1] >= 3 && g.glyph_sprite_step[i] < 7)
                g.glyph_sprite_step[i]++;
        if (g.glyph_sprite_step[0] < 7)
            g.glyph_sprite_step[0]++;
        GlyphColours(0xBF, 0xB7, 0xB6);
        for (int i = 0; i < 16; i++) {
            uint16_t step = g.glyph_sprite_step[i], y;

            if (!step)
                continue;
            y = (uint16_t)(step * 2);
            fx16_set(FXIN_PIECE_W, cart16(0x04, (uint16_t)(0xDAA2 + y)));
            fx16_set(FXIN_PIECE_H, cart16(0x04, (uint16_t)(0xDAA2 + y)));
            fx16_set(FXIN_PIECE_U, cart16(0x04, (uint16_t)(0xDAB2 + y)));
            fx16_set(FXIN_PIECE_V, cart16(0x04, (uint16_t)(0xDAB2 + y)));
            fx16_set(FXIN_PIECE_X, (uint16_t)(g.glyph_sprite_x[i] + cart16(0x04, (uint16_t)(0xDAC2 + y))));
            fx16_set(FXIN_PIECE_Y, (uint16_t)(g.glyph_sprite_y[i] + cart16(0x04, (uint16_t)(0xDAD2 + y))));
            fx16_set(FXIN_PIECE, g.glyph_sprite_glyph[i]);
            FxScaledSpriteAdvance(0);
        }
    }
    GlyphTimesSequence();
}

/* @routine 07:9814 GlyphSlideThree */
void GlyphSlideThree(void)
{
    for (int i = 0; i < 3; i++) {
        uint16_t x = g.glyph_sprite_x[i], step = g.glyph_sprite_step[i];

        x = x >= step && (uint16_t)(x - step) >= g.glyph_sprite_target[i] ? (uint16_t)(x - step) : g.glyph_sprite_target[i];
        g.glyph_sprite_x[i] = x;
        fx16_set(FXIN_PIECE_X, x);
        fx16_set(FXIN_PIECE_Y, 0x0050);
        fx16_set(FXIN_PIECE, g.glyph_sprite_glyph[i]);
        switch (g.glyph_sprite_glyph[i]) {
        case 0x8000:
            GlyphColours(0xBF, 0xB7, 0xB6);
            break;
        case 0x8006:
            GlyphColours(0xBF, 0xBC, 0xBD);
            break;
        case 0x800C:
            GlyphColours(0xBF, 0xBA, 0xB9);
            break;
        default:
            GlyphColours(0xBF, 0xB3, 0xBE);
            break;
        }
        FxGlyph24x32Bank04();
    }
}

/* @routine 07:A39A GlyphSpritesDrawAll */
void GlyphSpritesDrawAll(void)
{
    int s = 1;

    GlyphColours(0xBF, 0xB3, 0xBE);
    GlyphSpriteDraw(s++);
    GlyphColours(0xBF, 0xBC, 0xBD);
    GlyphSpriteDraw(s++);
    GlyphSpriteDraw(s++);
    GlyphColours(0xBF, 0xB3, 0xBE);
    for (int i = 0; i < 5; i++)
        GlyphSpriteDraw(s++);
    GlyphColours(0xBF, 0xBC, 0xBD);
    for (int i = 0; i < 3; i++)
        GlyphSpriteDraw(s++);
    if (!g.glyphs_extended) {
        GlyphColours(0xBF, 0xB3, 0xBE);
        for (int i = 0; i < 4; i++)
            glyph_sprite_draw_left8(s++);
    } else {
        GlyphColours(0xBF, 0xB3, 0xBE);
        for (int i = 0; i < 4; i++)
            GlyphSpriteDraw(s++);
        GlyphColours(0xBF, 0x12, 0xB6);
        for (int i = 0; i < 9; i++)
            GlyphSpriteDraw(s++);
    }
    GlyphGrid1A();
    GlyphGrid1D();
}

/* The times' colours by player 1's lead: negative, positive or none. */
static void lead_colours(int8_t lead)
{
    if (lead < 0)
        GlyphColours(0xBF, 0xB2, 0xB3);
    else if (lead > 0)
        GlyphColours(0xBF, 0x11, 0xBD);
    else
        GlyphColours(0xBF, 0x10, 0xB9);
}

/* @routine 07:8AF5 TimeDigitsStore: a time as five digits at $0F64. */
void TimeDigitsStore(uint8_t minutes, uint8_t seconds, uint8_t hundredths)
{
    g.time_digits_a[0] = minutes;
    g.time_digits_a[1] = (uint8_t)(seconds >> 4);
    g.time_digits_a[2] = (uint8_t)(seconds & 0x0F);
    g.time_digits_a[3] = (uint8_t)(hundredths >> 4);
    g.time_digits_a[4] = (uint8_t)(hundredths & 0x0F);
}

static void packed_time_draw(uint16_t packed)
{
    uint16_t min, sec, hund;

    TimeUnpackBcd(packed, &min, &sec, &hund);
    TimeDigitsStore((uint8_t)min, (uint8_t)sec, (uint8_t)hund);
    f329_sequence(g.time_digits_a);
}

/* @routine 07:8B17 HudTimesPackedDraw */
void HudTimesPackedDraw(void)
{
    int8_t lead = (int8_t)g.lead;

    if (lead < 0)
        GlyphColours(0xBF, 0xB3, 0xBE);
    else
        GlyphColours(0xBF, 0xB2, 0xB3);
    times_at(0x0058);
    if (g.packed_time_a != 0xFFFF)
        packed_time_draw(g.packed_time_a);
    lead_colours((int8_t)g.lead);
    times_at(0x006C);
    packed_time_draw(g.packed_time_b);
}

/*
 * @routine 07:89F5 HudTimesDraw
 * WHAT IT DOES: The two rows of times, by the HUD's mode and the lead: only
 * while the hold flag is set and not in the demo.
 */
void HudTimesDraw(void)
{
    if (g.race_skipped)
        return;
    if (!g.hud_hold)
        return;
    if (g.hud_mode == 3 && g.time_override) {
        HudTimesPackedDraw();
        return;
    }
    if (g.record_times[0] == 0xFFFF) {
        GlyphColours(0xBF, 0xB3, 0xBE);
        times_at(0x006C);
        f329_sequence(g.best_digits);
        return;
    }
    if (g.lap_shown) {
        HudLapDigitsDraw();
        if ((int8_t)g.lead > 0)
            HudLeadWordDraw();
        return;
    }
    if ((int8_t)g.lead < 0)
        GlyphColours(0xBF, 0xB3, 0xBE);
    else
        GlyphColours(0xBF, 0xB2, 0xB3);
    times_at(0x0058);
    f329_sequence(g.time_digits_a);
    lead_colours((int8_t)g.lead);
    times_at(0x006C);
    f329_sequence(g.best_digits);
}

/* @routine 07:A7AE HudLeadWordDraw */
void HudLeadWordDraw(void)
{
    static const uint16_t first[4] = {0x9416, 0x941C, 0x9618, 0x961A};
    static const uint16_t rest[6] = {0x9616, 0x941C, 0x9418, 0x9610, 0x9616, 0x941A};

    GlyphColours(0xBF, 0x11, 0xBD);
    fx16_set(FXIN_PIECE_X, 0x003C);
    fx16_set(FXIN_PIECE_Y, 0x0048);
    for (int i = 0; i < 4; i++) {
        fx16_set(FXIN_PIECE, first[i]);
        FxGlyph8x16();
    }
    fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) + 8));
    for (int i = 0; i < 6; i++) {
        fx16_set(FXIN_PIECE, rest[i]);
        FxGlyph8x16();
    }
}

static void lap_digit(uint8_t digit)
{
    param_216_a((uint16_t)(digit << 1));
    FxGlyph16x32();
}

/* @routine 07:A8D3 HudLapDigitsDraw: five digits as M:SS:HH at (48, 40). */
void HudLapDigitsDraw(void)
{
    GlyphColours(0xBF, 0xB3, 0xBE);
    fx16_set(FXIN_PIECE_X, 0x0030);
    fx16_set(FXIN_PIECE_Y, 0x0028);
    lap_digit(g.best_digits[0]);
    fx16_set(FXIN_PIECE, 0x9014);
    FxSprite16x32(0);
    lap_digit(g.best_digits[1]);
    lap_digit(g.best_digits[2]);
    fx16_set(FXIN_PIECE, 0x9018);
    FxSprite16x32(0);
    lap_digit(g.best_digits[3]);
    lap_digit(g.best_digits[4]);
}

/* @routine 07:A6DD HudBlankDraw: Stunt Trax's three marks and the used ones over them. */
void HudBlankDraw(void)
{
    static const uint16_t xs[3] = {0x0004, 0x0016, 0x0028};
    uint16_t n;

    GlyphColours(0xBF, 0xBA, 0xBD);
    fx16_set(FXIN_PIECE, 0x0C08);
    fx16_set(FXIN_PIECE_Y, 0x006C);
    for (int i = 0; i < 3; i++) {
        fx16_set(FXIN_PIECE_X, xs[i]);
        FxGlyph16x16();
    }
    GlyphColours(0xBF, 0xB7, 0xB6);
    fx16_set(FXIN_PIECE, 0x0E08);
    n = g.script_byte_1f71 & 0x000F;
    if (n > 2)
        return;
    for (int i = 2; i >= (int)n; i--) {
        fx16_set(FXIN_PIECE_X, xs[i]);
        FxGlyph16x16();
    }
}

/* The two words at the glyph sprites' places, 8 further after the third glyph. */
static void glyph_slide_draw(void)
{
    static const uint16_t one[6] = {0x8804, 0x8808, 0x8C14, 0x8408, 0x8814, 0x8814};
    static const uint16_t two[8] = {0x8C14, 0x8804, 0x8418, 0x8410, 0x8408, 0x8C0C, 0x8C10, 0x901C};

    GlyphColours(0xBF, 0xB7, 0xB6);
    fx16_set(FXIN_PIECE_X, g.glyph_sprite_x[0]);
    fx16_set(FXIN_PIECE_Y, g.glyph_sprite_y[0]);
    for (int i = 0; i < 6; i++) {
        if (i == 3)
            fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) + 8));
        fx16_set(FXIN_PIECE, one[i]);
        FxSprite16x32(0);
    }
    GlyphColours(0xBF, 0xBC, 0xBD);
    fx16_set(FXIN_PIECE_X, g.glyph_sprite_x[1]);
    fx16_set(FXIN_PIECE_Y, g.glyph_sprite_y[1]);
    for (int i = 0; i < 8; i++) {
        if (i == 3)
            fx16_set(FXIN_PIECE_X, (uint16_t)(fx16(FXIN_PIECE_X) + 8));
        fx16_set(FXIN_PIECE, two[i]);
        FxSprite16x32(0);
    }
}

/* @routine 07:A451 GlyphSpritesSlideOut */
void GlyphSpritesSlideOut(void)
{
    uint16_t a = (uint16_t)(g.glyph_sprite_x[0] - 0x0016);

    if ((a & 0x8000) && a < 0xFF98)
        a = 0xFF98;
    g.glyph_sprite_x[0] = a;
    a = (uint16_t)(g.glyph_sprite_x[1] + 0x0016);
    if (a >= 0x00D0)
        a = 0x00D0;
    g.glyph_sprite_x[1] = a;
    if (g.glyph_sprite_x[0] == 0xFF98 && g.glyph_sprite_x[1] == 0x00D0) {
        g.player_phase[0] = 6;
        g.player_phase[1] = 0;
        return;
    }
    glyph_slide_draw();
}

/* @routine 07:A492 GlyphSpritesSlideIn */
void GlyphSpritesSlideIn(void)
{
    uint16_t a = (uint16_t)(g.glyph_sprite_x[0] - 0x0016);

    if (a < 0x0034)
        a = 0x0034;
    g.glyph_sprite_x[0] = a;
    a = (uint16_t)(g.glyph_sprite_x[1] + 0x0016);
    if (!(a & 0x8000) && a >= 0x0024)
        a = 0x0024;
    g.glyph_sprite_x[1] = a;
    if (g.glyph_sprite_x[0] == 0x0034 && g.glyph_sprite_x[1] == 0x0024) {
        if (g.race_timer == 0x10) {
            g.finish_mark = 0x10;
            sound_port0_push_raw(0x01);
            sound_port0_push(0x3A);
        }
        if (!--g.race_timer)
            g.player_phase[0] = 4;
    } else {
        g.race_timer = 0x10;
    }
    glyph_slide_draw();
}
