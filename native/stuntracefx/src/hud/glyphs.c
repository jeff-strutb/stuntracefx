/*
 * glyphs.c: the HUD's glyphs and sprites, drawn by the Super FX's picture
 * programs (fx/render/hud.c) from the HUD's piece.
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
#include "data/race_tables.h"
#include "fx/fx.h"
#include "hud/hud.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"
#include "video/video.h"

/* The glyph programs' tables are in data/race_tables.h: the big and the
   small digits' glyphs; the time frame's middle, its two tails' widths and
   their texture steps, by the animation step; the laps-to-go words' heights
   by their step; the glyph sprites' starting places and glyphs; their grow
   steps' sizes, scales and offsets; the messages (a length, then a glyph
   word a letter). */

/* The piece the HUD draws next: what it sets before each program, kept
   between them as the original's words were (a program that moves x on
   leaves the next piece's x). */
FxPiece hud_piece;

/* the piece's four colours: pixel 0 draws colour 0, the others b1-b3 */
void GlyphColours(uint8_t b1, uint8_t b2, uint8_t b3)
{
    hud_piece.colours[0] = 0x00;
    hud_piece.colours[1] = b1;
    hud_piece.colours[2] = b2;
    hud_piece.colours[3] = b3;
}

/* the glyph from the word tables at $07:8E3E and $07:8E52 (DigitGlyphSelect, B) */
static void piece_big_digit(uint16_t index) { hud_piece.glyph = hud_big_digit_glyph(index); }
static void piece_small_digit(uint16_t index) { hud_piece.glyph = hud_small_digit_glyph(index); }

void DigitGlyphSelect(uint16_t digit) { piece_big_digit((uint16_t)(digit << 1)); }

/* @routine 07:8B9B GsuF329Entry: a digit (low four bits) through the table to program $07:F329. */
static void small_digit(uint16_t digit)
{
    piece_small_digit((uint16_t)((digit & 0x000F) << 1));
    FxGlyph8x16(&hud_piece);
}

/* @routine 07:A684 GsuF441Player1, 07:A694 GsuF441Player2 (RankBadgePlayer1, 2): program $07:F441,
   the rank badge (first place or not) by the player's rank. */
static void rank_badge(uint8_t kind)
{
    if ((kind & 0x0F) == 1) {
        GlyphColours(0x0F, 0x05, 0x0E);
        hud_piece.glyph = hud_picture(HUD_RANK_FIRST);
    } else {
        GlyphColours(0x0F, 0x08, 0x0E);
        hud_piece.glyph = hud_picture(HUD_RANK_OTHER);
    }
    hud_piece.x = 0x0074;
    hud_piece.y = 0x003C;
    FxGlyph24x16(&hud_piece);
}

void RankBadgePlayer1(void) { rank_badge(g.player_rank[0]); }
void RankBadgePlayer2(void) { rank_badge(g.player_rank[1]); }

/*
 * @routine 07:85E1 GsuF0A5OrF0B4
 * WHAT IT DOES: The input at $020C, then the split-screen picture rise or
 * the one-player one.
 */
void LapBannerRise(uint16_t input)
{
    hud_piece.y = input;
    if (g.split_screen) {
        GlyphColours(0x0F, 0x09, 0x0A);
        FxPictureRise40(hud_piece.colours, hud_piece.y);
    } else {
        GlyphColours(0xBF, 0xBC, 0xBD);
        FxPictureRise60(hud_piece.colours, hud_piece.y);
    }
}

/* @routine 07:8619 GsuEEF5OrEFCD: as above, for the last lap's banner. */
void LastLapBannerRise(uint16_t input)
{
    hud_piece.y = input;
    if (g.split_screen) {
        GlyphColours(0x0F, 0x04, 0x05);
        FxPictureRiseSplit40(hud_piece.colours, hud_piece.y);
    } else {
        GlyphColours(0xBF, 0xB7, 0xB6);
        FxPictureRiseSplit60(hud_piece.colours, hud_piece.y);
    }
}

/* @routine 07:9252 GsuDigitPair0E74: the two digits of the byte at $0E74. */
void BonusDigitsDraw(void)
{
    hud_piece.w = 0x0010;
    hud_piece.x = 0x0004;
    hud_piece.y = 0x0008;
    piece_big_digit((uint16_t)((g.digit_pair >> 4 & 0x000F) << 1));
    FxGlyph16RowsFromTop(&hud_piece);
    piece_big_digit((uint16_t)((g.digit_pair & 0x000F) << 1));
    FxGlyph16RowsFromTop(&hud_piece);
}

static void word_at(uint16_t x, uint16_t y)
{
    hud_piece.h = 0x0020;
    hud_piece.w = 0x0010;
    hud_piece.x = x;
    hud_piece.y = y;
}

/* a word in the 16 x 32 letters, each through FxGlyph16RowsFromTop (a space draws nothing) */
static void word_letters(const char *word)
{
    for (; *word; word++) {
        hud_piece.glyph = hud_glyph_16x32(*word);
        FxGlyph16RowsFromTop(&hud_piece);
    }
}

/* @routine 07:9883 GsuF15BThree */
void ShowWordWin(void)
{
    word_at(0x0030, 0x0018);
    GlyphColours(0x0F, 0x04, 0x05);
    word_letters("WIN");
}

/* @routine 07:997F GsuF15BFive */
void ShowWordCrash(void)
{
    word_at(0x0018, 0x0018);
    GlyphColours(0x0F, 0x09, 0x0A);
    word_letters("CRASH");
}

/* @routine 07:98F4 GsuF15BFourA */
void ShowWordLose(void)
{
    word_at(0x0028, 0x0018);
    GlyphColours(0x0F, 0x07, 0x08);
    word_letters("LOSE");
}

/* @routine 07:9A24 GsuF15BFourB */
void ShowWordDraw(void)
{
    word_at(0x0028, 0x0018);
    GlyphColours(0x0F, 0x0D, 0x03);
    word_letters("DRAW");
}

/* @routine 07:8653 GsuF15BTwoLines: sixteen pieces, three from ($50, 8) and thirteen from (0, $2C). */
void ShowGetAllTheStars(uint16_t input)
{
    hud_piece.h = input;
    hud_piece.w = 0x0010;
    hud_piece.x = 0x0050;
    hud_piece.y = 0x0008;
    GlyphColours(0xBF, 0xB7, 0xB6);
    word_letters("GET");
    hud_piece.x = 0x0000;
    hud_piece.y = 0x002C;
    word_letters("ALL THE STARS");
}

static void glyph_rows_at(uint16_t input, uint16_t x, const char *word)
{
    hud_piece.h = input;
    hud_piece.w = 0x0010;
    hud_piece.x = x;
    hud_piece.y = 0x0028;
    GlyphColours(0xBF, 0xB7, 0xB6);
    word_letters(word);
}

/* @routine 07:8821 GlyphRowsAt32 */
void GlyphRowsAt32(uint16_t input)
{
    glyph_rows_at(input, 0x0020, "NEXT AREA");
}

/* @routine 07:8929 GlyphRowsAt56 */
void GlyphRowsAt56(uint16_t input)
{
    glyph_rows_at(input, 0x0038, "FINISH");
}

static void piece_scale(uint16_t w, uint16_t h, uint16_t u, uint16_t v)
{
    hud_piece.w = w;
    hud_piece.h = h;
    hud_piece.u = u;
    hud_piece.v = v;
}

static void scaled_piece(uint16_t x, uint16_t y, uint16_t piece)
{
    hud_piece.x = x;
    hud_piece.y = y;
    hud_piece.glyph = piece;
    FxScaledSpriteEighth(&hud_piece, chip_view_right(), chip_view_bottom());
}

/* @routine 07:976B GsuFA1BFromTables */
void TimeFrameTailB(void)
{
    uint16_t v;

    GlyphColours(0xBF, 0xBC, 0xBD);
    hud_piece.x = g.hud_point_x;
    hud_piece.y = (uint16_t)(g.hud_point_y + 8);
    hud_piece.h = 0x0100;
    hud_piece.v = 0x0100;
    v = hud_frame_tail_b_width(g.hud_point_index);
    if (!v)
        return;
    hud_piece.w = v;
    hud_piece.u = hud_frame_tail_b_step(g.hud_point_index);
    hud_piece.glyph = hud_pieces_at(0, 32);
    FxScaledSprite13(&hud_piece, chip_view_right(), chip_view_bottom());
}

/* @routine 07:9718 GsuFA35FromTables */
void TimeFrameTailA(void)
{
    uint16_t v;

    GlyphColours(0xBF, 0xBC, 0xBD);
    hud_piece.x = g.hud_point_x;
    hud_piece.y = (uint16_t)(g.hud_point_y + 8);
    hud_piece.h = 0x0100;
    hud_piece.v = 0x0100;
    v = hud_frame_tail_a_width(g.hud_point_index);
    if (!v)
        return;
    hud_piece.w = v;
    hud_piece.u = hud_frame_tail_a_step(g.hud_point_index);
    hud_piece.glyph = hud_pieces_at(0, 16);
    FxScaledSprite12(&hud_piece, chip_view_right(), chip_view_bottom());
}

/* @routine 07:92B5 GsuFA5AThree: three pieces around the point. */
void TimeFrameLeft(void)
{
    uint16_t x = (uint16_t)(g.hud_point_x - 8), y = g.hud_point_y;

    piece_scale(0x0100, 0x0100, 0x0100, 0x0100);
    scaled_piece(x, y, hud_pieces_at(64, 0));
    scaled_piece(x, (uint16_t)(y + 0x18), hud_pieces_at(64, 8));
    piece_scale(0x0100, 0x0200, 0x0100, 0x0080);
    scaled_piece(x, (uint16_t)(y + 8), hud_pieces_at(96, 0));
}

/* @routine 07:9486 GsuFA5AThreeShifted */
void TimeFrameLeftShifted(void)
{
    uint16_t x = (uint16_t)(hud_frame_x_offset(g.hud_point_index) + g.hud_point_x);
    uint16_t y = g.hud_point_y;

    piece_scale(0x0100, 0x0100, 0x0100, 0x0100);
    scaled_piece(x, y, hud_pieces_at(72, 0));
    scaled_piece(x, (uint16_t)(y + 0x18), hud_pieces_at(72, 8));
    piece_scale(0x0100, 0x0200, 0x0100, 0x0080);
    scaled_piece(x, (uint16_t)(y + 8), hud_pieces_at(96, 16));
}

/* @routine 07:9366 GsuFA5ASix */
void TimeFrameRight(void)
{
    static const uint16_t dy[6] = {0x0000, 0x0008, 0x0010, 0x0018, 0x0008, 0x0010};
    static const uint8_t piece_x[6] = {56, 88, 88, 56, 56, 56}, piece_line[6] = {0, 0, 8, 8, 0, 8};
    uint16_t x = (uint16_t)(hud_frame_x_offset(g.hud_point_index) + g.hud_point_x);

    piece_scale(0x0100, 0x0100, 0x0100, 0x0100);
    for (int i = 0; i < 6; i++) {
        if (i == 4)
            x = (uint16_t)(x + 8);
        scaled_piece(x, (uint16_t)(g.hud_point_y + dy[i]), hud_pieces_at(piece_x[i], piece_line[i]));
    }
}

/* @routine 07:9533 GsuFA5ASixLeft */
void TimeFrameRightMirrored(void)
{
    static const uint16_t dy[6] = {0x0000, 0x0008, 0x0010, 0x0018, 0x0008, 0x0010};
    static const uint16_t dx[6] = {0xFFF8, 0xFFF8, 0xFFF8, 0xFFF8, 0xFFF0, 0xFFF0};
    static const uint8_t piece_x[6] = {48, 80, 80, 48, 48, 48}, piece_line[6] = {0, 0, 8, 8, 0, 8};

    piece_scale(0x0100, 0x0100, 0x0100, 0x0100);
    for (int i = 0; i < 6; i++)
        scaled_piece((uint16_t)(g.hud_point_x + dx[i]), (uint16_t)(g.hud_point_y + dy[i]), hud_pieces_at(piece_x[i], piece_line[i]));
}

/* @routine 07:9660 GsuFA5AFromTables */
void TimeFrameMiddle(void)
{
    uint16_t i = g.hud_point_index;

    piece_scale(hud_frame_middle_width(i), 0x0100, hud_frame_middle_step(i), 0x0100);
    scaled_piece(g.hud_point_x, g.hud_point_y, hud_pieces_at(96, 8));
    hud_piece.h = 0x0100;
    hud_piece.v = 0x0100;
    scaled_piece(g.hud_point_x, (uint16_t)(g.hud_point_y + 0x18), hud_pieces_at(96, 24));
    hud_piece.h = 0x0200;
    hud_piece.v = 0x0080;
    scaled_piece(g.hud_point_x, (uint16_t)(g.hud_point_y + 8), hud_pieces_at(84, 2));
}

/* @routine 07:833B GsuPiecesStep10CB: steps the counter at $10CB and draws eleven pieces. */
void ShowLapsToGo(void)
{
    static const char words[] = "3LAPS TO GO";   /* the 3, then the rest four pixels further */
    uint16_t n = (uint16_t)(g.piece_step + 1);

    if (n >= 0x0014)
        n = 0x0014;
    g.piece_step = n;
    hud_piece.w = 0x0010;
    hud_piece.x = 0x0010;
    hud_piece.y = 0x0028;
    GlyphColours(0xBF, 0xBA, 0xB9);
    hud_piece.h = hud_laps_to_go_height(n);
    for (int i = 0; i < 11; i++) {
        hud_piece.glyph = hud_glyph_16x32(words[i]);
        if (n < 8)
            FxGlyph16RowsFromTop(&hud_piece);
        else
            FxGlyph16RowsFromBottom(&hud_piece);
        if (i == 0)
            hud_piece.x = (uint16_t)(hud_piece.x + 4);
    }
}

/* @routine 07:9AB0 HudSixGlyphs */
void HudSixGlyphs(void)
{
    GlyphColours(0x0F, 0x06, 0x0E);
    word_at(0x0010, 0x0018);
    word_letters("SPLASH");
}

/* A big sprite's letter, 24 x 40. */
static void sprite24x40(char letter)
{
    hud_piece.glyph = hud_glyph_24x40(letter);
    FxSprite24x40(&hud_piece, chip_view_right(), chip_view_bottom());
}

/*
 * @routine 07:9EE9 BigSpritesCountDraw
 * WHAT IT DOES: The sprite rows when $0E5C is set: 24 x 40 sprites as many
 * as the count $1160 allows, then the count goes up by one, to at most 12.
 */
static void big_sprites_count_draw(void)
{
    static const char near_[] = "GAME", far_[] = "OVER";
    static const uint8_t need_far[3] = {7, 8, 9};

    hud_piece.u = 0x0100;
    hud_piece.v = 0x0100;
    hud_piece.x = 0x0030;
    hud_piece.y = 0x0010;
    for (int i = 0; i < 4; i++) {
        sprite24x40(near_[i]);
        if (i < 3 && g.sprite_count < i + 1)
            goto count;
    }
    hud_piece.x = 0x0040;
    hud_piece.y = 0x0040;
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
    hud_piece.x = x;
    hud_piece.y = y;
    hud_piece.u = 0x0100;
    hud_piece.v = 0x0100;
}

/* @routine 07:9D67 BigSpritesRow6 */
void BigSpritesRow6(void)
{
    static const char first[] = "TIME", second[] = "UP";

    if (!big_sprites_start(0xBF, 0xB5, 0xBB))
        return;
    big_sprites_at(0x0018, 0x0028);
    for (int i = 0; i < 4; i++)
        sprite24x40(first[i]);
    hud_piece.x = (uint16_t)(hud_piece.x + 0x0010);
    for (int i = 0; i < 2; i++)
        sprite24x40(second[i]);
}

/* @routine 07:9C9F BigSpritesRow6B */
void BigSpritesRow6B(void)
{
    static const char src[] = "SPLASH";

    if (!big_sprites_start(0xBF, 0xB3, 0xBE))
        return;
    big_sprites_at(0x0018, 0x0028);
    for (int i = 0; i < 6; i++)
        sprite24x40(src[i]);
}

/* @routine 07:9E3B BigSpritesRow5 */
void BigSpritesRow5(void)
{
    static const char src[] = "CRASH";

    if (!big_sprites_start(0xBF, 0xBC, 0xBD))
        return;
    big_sprites_at(0x0020, 0x0028);
    for (int i = 0; i < 5; i++)
        sprite24x40(src[i]);
}

/* @routine 07:9B6F BigSpritesNine */
void BigSpritesNine(void)
{
    static const char first[] = "OUT", second[] = "OF", third[] = "RANK";

    if (!big_sprites_start(0xBF, 0xB7, 0xB6))
        return;
    big_sprites_at(0x0029, 0x0021);
    for (int i = 0; i < 3; i++)
        sprite24x40(first[i]);
    hud_piece.x = (uint16_t)(hud_piece.x + 0x0006);
    for (int i = 0; i < 2; i++)
        sprite24x40(second[i]);
    hud_piece.x = 0x0038;
    hud_piece.y = 0x0050;
    for (int i = 0; i < 4; i++)
        sprite24x40(third[i]);
}

/* @routine 07:8AA6 GsuF329Sequence: five digits with two separators, at the position set. */
static void time_digits(const uint8_t *digits)
{
    small_digit(digits[0]);
    hud_piece.glyph = hud_glyph_8x16('\'');
    FxGlyph8x16(&hud_piece);
    small_digit(digits[1]);
    small_digit(digits[2]);
    hud_piece.glyph = hud_glyph_8x16('"');
    FxGlyph8x16(&hud_piece);
    small_digit(digits[3]);
    small_digit(digits[4]);
}

static void times_at(uint16_t y)
{
    hud_piece.x = 0x0093;
    hud_piece.y = y;
}

/* @routine 07:89E4 HudHoldDraw */
void HudHoldDraw(void)
{
    if (!g.hud_hold)
        return;
    GlyphColours(0xBF, 0xB3, 0xBE);
    times_at(0x006C);
    time_digits(g.best_digits);
}

/* @routine 04:DDB0 GlyphAtY60: a glyph at (x, 96). */
void GlyphAtY60(uint16_t glyph, uint16_t x)
{
    hud_piece.x = x;
    hud_piece.glyph = glyph;
    hud_piece.y = 0x0060;
    GlyphColours(0xBF, 0xB5, 0xBB);
    FxGlyph8x16(&hud_piece);
}

/* @routine 04:DD61 GlyphRowY60From0 and its entries DD6E, DD7B, DD88, DD95, DDA2 */
static void glyph_row(int from)
{
    static const char text[] = "AND...";     /* its letters from x $50, 8 apart, drawn from the right */

    for (int i = from; i < 6; i++)
        GlyphAtY60(hud_glyph_8x16(text[5 - i]), (uint16_t)(0x0050 + 8 * (5 - i)));
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
        hud_piece.glyph = g.glyph_word[i];
        hud_piece.x = g.glyph_x_y[i * 2];
        hud_piece.y = g.glyph_x_y[i * 2 + 1];
        if (i >= g.glyph_first_end)
            GlyphColours(0xBF, 0xB3, 0xBE);
        else
            GlyphColours(0xBF, 0xBC, 0xBD);
        FxGlyph8x16(&hud_piece);
    } while (++i < g.glyph_end);
}

/* @routine 07:A426 GlyphSpriteDraw: sprite `sprite` at its own place. */
void GlyphSpriteDraw(int sprite)
{
    hud_piece.x = g.glyph_sprite_x[sprite];
    hud_piece.y = g.glyph_sprite_y[sprite];
    hud_piece.glyph = g.glyph_sprite_glyph[sprite];
    FxGlyph8x16(&hud_piece);
}

/* @routine 07:A41D GlyphSpriteDrawLeft8 */
static void glyph_sprite_draw_left8(int sprite)
{
    hud_piece.x = (uint16_t)(g.glyph_sprite_x[sprite] - 8);
    hud_piece.y = g.glyph_sprite_y[sprite];
    hud_piece.glyph = g.glyph_sprite_glyph[sprite];
    FxGlyph8x16(&hud_piece);
}

/* @routine 04:DA3C GlyphSpritesInit */
void GlyphSpritesInit(void)
{
    for (int i = 0; i < 16; i++) {
        g.glyph_sprite_x[i] = glyph_sprite_start_x((uint16_t)i);
        g.glyph_sprite_y[i] = 0x0008;
        g.glyph_sprite_glyph[i] = glyph_sprite_start_glyph((uint16_t)i);
        g.glyph_sprite_step[i] = 0;
    }
    g.times_step = 0;
    g.times_timer = 0x001E;
    g.finish_lines = 0;
    g.hud_hold &= 0xFF00;
}

static uint16_t digit_glyph(uint8_t digit)
{
    return hud_glyph_8x16_number((uint8_t)(digit & 0x0F));
}

/* The glyph list's next slot: a glyph of the time row at x, line $68. */
static void time_glyph(int *slot, uint16_t glyph, uint8_t x)
{
    g.glyph_word[*slot] = glyph;
    g.glyph_on[*slot] = 0;
    g.glyph_x_y[*slot * 2] = x;
    g.glyph_x_y[*slot * 2 + 1] = 0x68;
    (*slot)++;
}

/*
 * @routine 04:DE2C GlyphListMessageAndTime
 * WHAT IT DOES: Fills the glyph list with message `index` (its address in
 * the table at $04:DF6D: a length byte, then a glyph word per letter, 0 a
 * space) centred on x $68 at y $58, then the time row at y $68 from x $4C.
 */
static void glyph_list_message_and_time(uint16_t index, uint8_t min, uint8_t sec, uint8_t hund)
{
    uint16_t letters = hud_message_letters(index), x, k = 0;
    int slot = 0;

    x = (uint16_t)-(uint16_t)((uint16_t)(letters * 4) - 0x0068);
    g.glyph_first_end = (uint16_t)(letters - 1);
    g.glyph_end = (uint16_t)(g.glyph_first_end + 7);
    for (uint16_t n = letters; n; n--) {
        uint16_t w = hud_message_glyph(index, k);

        if (w) {
            g.glyph_word[slot] = w;
            g.glyph_x_y[slot * 2] = (uint8_t)x;
            g.glyph_x_y[slot * 2 + 1] = 0x58;
            g.glyph_on[slot] = 0;
            slot++;
        }
        x = (uint16_t)(x + 8);
        k++;
    }
    time_glyph(&slot, digit_glyph(hund), 0x4C);
    time_glyph(&slot, hud_glyph_8x16('\''), 0x54);
    time_glyph(&slot, digit_glyph((uint8_t)(sec >> 4)), 0x5C);
    time_glyph(&slot, digit_glyph(sec), 0x64);
    time_glyph(&slot, hud_glyph_8x16('"'), 0x6C);
    time_glyph(&slot, digit_glyph((uint8_t)(min >> 4)), 0x74);
    time_glyph(&slot, digit_glyph(min), 0x7C);
}

/* @routine 07:A03F GlyphWordAt1C28 */
void ShowBonusTrax(void)
{
    word_at(0x001C, 0x0028);
    GlyphColours(0xBF, 0xBC, 0xBD);
    word_letters("BONUS");
    hud_piece.x = (uint16_t)(hud_piece.x + 4);
    word_letters("TRAX");
}

/* Three rows of three 8 x 8 pieces from the HUD's pieces picture, from
   pixel 104 of line `line` on. */
static void glyph_grid3(uint16_t line, int switch_colours)
{
    for (int r = 0; r < 3; r++) {
        if (r) {
            hud_piece.y = (uint16_t)(hud_piece.y + 8);
            hud_piece.x = (uint16_t)(hud_piece.x - 0x18);
            if (r == 1 && switch_colours)
                GlyphColours(0xBF, 0xB3, 0xBE);
        }
        for (int c = 0; c < 3; c++) {
            if (r == 0 && c == 2 && switch_colours)
                GlyphColours(0xBF, 0xBA, 0xB9);
            hud_piece.glyph = hud_pieces_at((uint16_t)(104 + 8 * c), (uint16_t)(line + 8 * r));
            FxGlyph8x8(&hud_piece);
        }
    }
}

/* @routine 07:A158 GlyphGrid1A */
void GlyphGrid1A(void)
{
    hud_piece.x = 0x0038;
    hud_piece.y = 0x0030;
    GlyphColours(0xBF, 0xB7, 0xB6);
    glyph_grid3(0, 1);
}

/* @routine 07:A27D GlyphGrid1D */
void GlyphGrid1D(void)
{
    hud_piece.x = 0x0038;
    hud_piece.y = 0x0048;
    GlyphColours(0xBF, 0xBB, 0xBE);
    glyph_grid3(24, 0);
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
            hud_piece.w = glyph_grow_size(y);
            hud_piece.h = glyph_grow_size(y);
            hud_piece.u = glyph_grow_scale(y);
            hud_piece.v = glyph_grow_scale(y);
            hud_piece.x = (uint16_t)(g.glyph_sprite_x[i] + glyph_grow_x_offset(y));
            hud_piece.y = (uint16_t)(g.glyph_sprite_y[i] + glyph_grow_y_offset(y));
            hud_piece.glyph = g.glyph_sprite_glyph[i];
            FxScaledSpriteAdvance(&hud_piece, chip_view_right(), chip_view_bottom());
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
        hud_piece.x = x;
        hud_piece.y = 0x0050;
        hud_piece.glyph = g.glyph_sprite_glyph[i];
        if (g.glyph_sprite_glyph[i] == hud_place_glyph('1'))     /* the place number's colours by place */
            GlyphColours(0xBF, 0xB7, 0xB6);
        else if (g.glyph_sprite_glyph[i] == hud_place_glyph('2'))
            GlyphColours(0xBF, 0xBC, 0xBD);
        else if (g.glyph_sprite_glyph[i] == hud_place_glyph('3'))
            GlyphColours(0xBF, 0xBA, 0xB9);
        else
            GlyphColours(0xBF, 0xB3, 0xBE);
        FxGlyph24x32Bank04(&hud_piece);
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
    time_digits(g.time_digits_a);
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
        time_digits(g.best_digits);
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
    time_digits(g.time_digits_a);
    lead_colours((int8_t)g.lead);
    times_at(0x006C);
    time_digits(g.best_digits);
}

/* @routine 07:A7AE HudLeadWordDraw */
void HudLeadWordDraw(void)
{
    static const char first[] = "BEST", rest[] = "RECORD";

    GlyphColours(0xBF, 0x11, 0xBD);
    hud_piece.x = 0x003C;
    hud_piece.y = 0x0048;
    for (int i = 0; i < 4; i++) {
        hud_piece.glyph = hud_glyph_8x16(first[i]);
        FxGlyph8x16(&hud_piece);
    }
    hud_piece.x = (uint16_t)(hud_piece.x + 8);
    for (int i = 0; i < 6; i++) {
        hud_piece.glyph = hud_glyph_8x16(rest[i]);
        FxGlyph8x16(&hud_piece);
    }
}

static void lap_digit(uint8_t digit)
{
    piece_big_digit((uint16_t)(digit << 1));
    FxGlyph16x32(&hud_piece);
}

/* @routine 07:A8D3 HudLapDigitsDraw: five digits as M:SS:HH at (48, 40). */
void HudLapDigitsDraw(void)
{
    GlyphColours(0xBF, 0xB3, 0xBE);
    hud_piece.x = 0x0030;
    hud_piece.y = 0x0028;
    lap_digit(g.best_digits[0]);
    hud_piece.glyph = hud_glyph_16x32('\'');
    FxSprite16x32(&hud_piece, chip_view_right(), chip_view_bottom());
    lap_digit(g.best_digits[1]);
    lap_digit(g.best_digits[2]);
    hud_piece.glyph = hud_glyph_16x32('"');
    FxSprite16x32(&hud_piece, chip_view_right(), chip_view_bottom());
    lap_digit(g.best_digits[3]);
    lap_digit(g.best_digits[4]);
}

/* @routine 07:A6DD HudBlankDraw: Stunt Trax's three marks and the used ones over them. */
void HudBlankDraw(void)
{
    static const uint16_t xs[3] = {0x0004, 0x0016, 0x0028};
    uint16_t n;

    GlyphColours(0xBF, 0xBA, 0xBD);
    hud_piece.glyph = hud_picture(HUD_STUNT_MARK);
    hud_piece.y = 0x006C;
    for (int i = 0; i < 3; i++) {
        hud_piece.x = xs[i];
        FxGlyph16x16(&hud_piece);
    }
    GlyphColours(0xBF, 0xB7, 0xB6);
    hud_piece.glyph = hud_picture(HUD_STUNT_MARK_USED);
    n = g.rivals_left & 0x000F;
    if (n > 2)
        return;
    for (int i = 2; i >= (int)n; i--) {
        hud_piece.x = xs[i];
        FxGlyph16x16(&hud_piece);
    }
}

/* The two words at the glyph sprites' places, 8 further after the third glyph. */
static void glyph_slide_draw(void)
{
    static const char one[] = "HITALL", two[] = "THECARS!";    /* HIT ALL and THE CARS!, the gap after the third */

    GlyphColours(0xBF, 0xB7, 0xB6);
    hud_piece.x = g.glyph_sprite_x[0];
    hud_piece.y = g.glyph_sprite_y[0];
    for (int i = 0; i < 6; i++) {
        if (i == 3)
            hud_piece.x = (uint16_t)(hud_piece.x + 8);
        hud_piece.glyph = hud_glyph_16x32(one[i]);
        FxSprite16x32(&hud_piece, chip_view_right(), chip_view_bottom());
    }
    GlyphColours(0xBF, 0xBC, 0xBD);
    hud_piece.x = g.glyph_sprite_x[1];
    hud_piece.y = g.glyph_sprite_y[1];
    for (int i = 0; i < 8; i++) {
        if (i == 3)
            hud_piece.x = (uint16_t)(hud_piece.x + 8);
        hud_piece.glyph = hud_glyph_16x32(two[i]);
        FxSprite16x32(&hud_piece, chip_view_right(), chip_view_bottom());
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
