/*
 * time_bonus.c: the race time on screen and the time bonus counted into it.
 * time_left is the time (BCD) and digit_pair a bonus (BCD) counted into it;
 * hud_point_index steps an animation whose digit size comes from a table.
 *
 * Not converted, nothing reaches them: GsuBonusDigitsBounce,
 * GsuBonusDigitsFull, TimeBonusPiecesMirrored.
 */
#include "audio/sound.h"
#include "data/cart.h"
#include "hud/hud.h"
#include "race/race.h"
#include "system/chip.h"
#include "system/state.h"

/* More of the HUD's tables in bank 7 (HUD_BANK): the bonus digits' sizes
   by step, and the countdown digit's scale, size and place, four words for
   each tenth of a second. */
enum { DIGIT_GROW_SIZES = 0x918F, COUNTDOWN_DIGIT_SHAPES = 0x8DDA };

/* The digit sizes for the animation steps, $07:918F. */
static uint16_t size_grow(void) { return cart16(HUD_BANK, (uint16_t)(DIGIT_GROW_SIZES + g.hud_point_index)); }

static void digit(uint16_t d)
{
    DigitGlyphSelect(d);
    FxGlyph16RowsFromTop();
}

/* @routine 07:91B1 GsuTimeDigits: the time left at X $AC, Y 8; the hundreds first at $9E when it is $100 or more. */
void GsuTimeDigits(void)
{
    uint16_t v = g.time_left;

    fx16_set(FXIN_PIECE_X, 0x00AC);
    fx16_set(FXIN_PIECE_Y, 0x0008);
    fx16_set(FXIN_PIECE_W, 0x0010);
    if (v >= 0x0100) {
        DigitGlyphSelect((v >> 8) & 0x000F);
        fx16_set(FXIN_PIECE_X, 0x009E);
        FxGlyph16RowsFromTop();
        fx16_set(FXIN_PIECE_X, 0x00AC);
    }
    digit((v >> 4) & 0x000F);
    digit(v & 0x000F);
}

/* @routine 07:919D GsuTimeDigitsGrow */
static void time_digits_grow(void)
{
    fx16_set(FXIN_PIECE_H, size_grow());
    GlyphColours(0xBF, 0xB5, 0xBB);
    GsuTimeDigits();
}

/* @routine 07:91A6 GsuTimeDigitsFull */
void GsuTimeDigitsFull(void)
{
    fx16_set(FXIN_PIECE_H, 0x0020);
    GlyphColours(0xBF, 0xB5, 0xBB);
    GsuTimeDigits();
}

/* @routine 07:922F GsuBonusDigitsGrow */
static void bonus_digits_grow(void)
{
    fx16_set(FXIN_PIECE_H, size_grow());
    if (!g.time_shown)
        return;
    GlyphColours(0xBF, 0xB7, 0xB6);
    BonusDigitsDraw();
}

/* @routine 07:90EE TimeDigitBytes: the digits' colours by the time kept. */
static void time_digit_bytes(void)
{
    if (g.time_override)
        GlyphColours(0xBF, 0xBA, 0xB9);
    else if (g.time_taken_from >= 0x0020)
        GlyphColours(0xBF, 0xB5, 0xBB);
    else if (g.time_taken_from >= 0x0010)
        GlyphColours(0xBF, 0xBC, 0xBD);
    else
        GlyphColours(0xBF, 0xB7, 0xB6);
}

static void bonus_x_from_table(void)
{
    g.hud_point_x = (uint16_t)(0x008F - cart16(HUD_BANK, (uint16_t)(FRAME_X_OFFSETS + g.hud_point_index)));
}

/* The frame drawn while the bonus shows, mirrored. */
static void draw_mirrored(void)
{
    GlyphColours(0xBF, 0xB7, 0xB6);
    TimeFrameLeftShifted();
    TimeFrameMiddle();
    TimeFrameRightMirrored();
    TimeFrameTailA();
}

static void draw_bonus_then_time(void)
{
    time_digit_bytes();
    fx16_set(FXIN_PIECE_H, size_grow());
    BonusDigitsDraw();
    time_digit_bytes();
    fx16_set(FXIN_PIECE_H, 0x0020);
    time_digit_bytes();
    GsuTimeDigits();
    draw_mirrored();
}

/* The pieces around the point while the bonus shows, else the eleven pieces. */
static void draw_frame(void)
{
    if (g.time_shown) {
        GlyphColours(0xBF, 0xB7, 0xB6);
        TimeFrameLeft();
        TimeFrameMiddle();
        TimeFrameRight();
        TimeFrameTailA();
    } else {
        ShowLapsToGo();
    }
}

static void draw_time(void)
{
    g.hud_point_x = 0x0030;
    time_digits_grow();
    if (g.time_shown)
        bonus_digits_grow();
    g.hud_point_x = 0x0037;
    draw_frame();
}

static void draw_pieces(void)
{
    g.hud_point_x = 0x0030;
    TimeFrameLeft();
    TimeFrameMiddle();
    TimeFrameRight();
    TimeFrameTailB();
}

/* State 6's count: the bonus into the time, the step at a time. */
static void count_bonus(void)
{
    if (!g.digit_pair)
        return;
    if (g.digit_pair < g.bonus_step) {  /* decimal digits compare as their values */
        g.time_left = BcdAdd(g.time_left, g.digit_pair);
        g.digit_pair = 0;
    } else {
        g.digit_pair = BcdSub(g.digit_pair, g.bonus_step);
        g.time_left = BcdAdd(g.time_left, g.bonus_step);
    }
}

/* State 14: 3 (1 below 9) moved from the time to the bonus, up to $99. */
static void take_from_time(void)
{
    uint16_t v = g.time_left;

    if (v >= 0x0009) {
        g.time_left = BcdSub(v, 0x0003);
        v = BcdAdd(g.digit_pair, 0x0003);
    } else {
        v = BcdSub(v, 0x0001);
        g.time_left = v;
        if (v == 0)
            SoundPort0QueuePushRaw(0x44);
        v = BcdAdd(g.digit_pair, 0x0001);
    }
    if (v >= 0x0099) {
        SoundPort0QueuePushRaw(0x44);
        v = 0x0099;
    }
    g.digit_pair = v;
}

/*
 * @routine 07:8E90 TimeBonusTally
 * WHAT IT DOES: One step of the time bonus animation, a state machine: the
 * digits grow in, the bonus is counted into the time with sounds, the digits
 * shrink away; states 18-24 animate a second set of pieces.
 */
void TimeBonusTally(void)
{
    uint16_t a;

    g.hud_point_y = 0x0006;
    switch (g.tally_state) {
    case 0:
        return;
    case 2:
        g.hud_point_y = 0x0006;
        g.tally_state += 2;
        g.hud_point_index = 0xFFFE;
        g.digit_pair = g.time_shown;
        return;
    case 4:
        a = g.hud_point_index;
        if (a != 0x0008) {
            g.hud_point_index = (uint16_t)(a + 2);
        } else {
            g.tally_state += 2;
            a = g.digit_pair;
            if (a) {
                SoundPort0QueuePushRaw(0x0B);
                a = a < 0x0032 ? 1 : (a < 0x0048 ? 2 : a < 0x0064 ? 3 : 4);
            }
            g.bonus_step = a;
        }
        draw_time();
        return;
    case 6:
        if (g.start_substate >= 0x08) {
            g.time_left = BcdAdd(g.time_left, g.digit_pair);
            g.digit_pair = 0;
            g.finish_lines--;
            g.tally_state += 2;
            if (g.time_shown)
                SoundPort0QueuePushRaw(0x27);
        } else {
            count_bonus();
        }
        draw_time();
        return;
    case 8:
        if (!g.hud_point_index) {
            g.tally_state = 0;
            return;
        }
        g.hud_point_index = (uint16_t)(g.hud_point_index - 2);
        bonus_x_from_table();
        bonus_digits_grow();
        draw_frame();
        return;
    case 10:
        g.hud_point_y = 0x0006;
        g.tally_state += 2;
        g.hud_point_index = 0xFFFE;
        g.digit_pair = 0;
        if (g.time_left)
            g.time_taken_from = g.time_left;
        else
            g.tally_state = 0;
        g.finish_lines = 0;
        return;
    case 12:
        a = g.hud_point_index;
        if (a == 0x0008) {
            g.tally_state += 2;
            SoundPort0QueuePushRaw(0x40);
        } else {
            g.hud_point_index = (uint16_t)(a + 2);
            bonus_x_from_table();
        }
        draw_bonus_then_time();
        return;
    case 14:
        if (g.time_left && g.digit_pair < 0x0099)
            take_from_time();
        draw_bonus_then_time();
        return;
    case 16:
        a = g.hud_point_index;
        if (!a) {
            g.tally_state = g.tally_state;
            return;
        }
        g.hud_point_index = (uint16_t)(a - 2);
        g.hud_point_x = 0x0037;
        time_digit_bytes();
        fx16_set(FXIN_PIECE_H, size_grow());
        BonusDigitsDraw();
        time_digits_grow();
        draw_mirrored();
        return;
    case 18:
        if (!g.stunt_score_add) {
            g.tally_state = 0;
            return;
        }
        g.hud_point_y = 0x0030;
        g.tally_state += 2;
        SoundPort0QueuePushRaw(0x0B);
        g.hud_point_index = 0;
        g.digit_pair = 0x0028;
        return;
    case 20:
        a = g.hud_point_index;
        if (a == 0x0008)
            g.tally_state += 2;
        g.hud_point_index = (uint16_t)(a + 2);
        draw_pieces();
        return;
    case 22:
        draw_pieces();
        return;
    case 24:
        a = g.hud_point_index;
        if (!a) {
            g.tally_state = 0;
            return;
        }
        g.hud_point_index = (uint16_t)(a - 2);
        SoundPort0QueuePushRaw(0x27);
        draw_pieces();
        return;
    default:
        return;
    }
}

static void transfer_digits(void)
{
    GlyphColours(0xBF, 0xBA, 0xB9);
    fx16_set(FXIN_PIECE_H, g.time_bonus_2);
    BonusDigitsDraw();
}

/*
 * @routine 07:8BB3 TimeBonusTransfer
 * WHAT IT DOES: While the panel shows, moves the time bonus into the race
 * time, showing what is left and starting the tally's second animation; with
 * no bonus, counts the shown digits up towards the score. Then a small
 * animation that grows the bonus digits and draws them.
 */
void TimeBonusTransfer(void)
{
    if (!g.finish_lines)
        return;
    if (!g.stunt_score_add) {
        g.bonus_step = 0;
        if (g.tally_state)
            g.tally_state = 0x18;
        if (g.hits_bcd != g.digit_pair)
            g.digit_pair = BcdAdd(g.digit_pair, 0x0001);
    } else {
        uint16_t step = g.stunt_score_add >= 0x18 ? 0x0017 : g.stunt_score_add >= 0x05 ? 0x0003 : 0x0001;
        uint16_t left;

        left = BcdSub(g.stunt_score_add, step);
        g.stunt_score_add = (uint8_t)left;
        g.digit_pair = left;
        g.time_left = BcdAdd(g.time_left, step);
        if (!g.tally_state)
            g.tally_state = 0x12;
    }
    switch (g.time_bonus) {
    case 2:
        if (g.time_bonus_2 >= 0x0020)
            g.time_bonus = (uint16_t)(g.time_bonus + 2);
        else
            g.time_bonus_2 = (uint16_t)(g.time_bonus_2 + 4);
        transfer_digits();
        return;
    case 4:
        transfer_digits();
        return;
    case 6:
        if (!g.time_bonus_2) {
            g.time_bonus = 0;
            return;
        }
        g.time_bonus_2++;
        transfer_digits();
        return;
    default:
        return;
    }
}

/*
 * @routine 07:8C82 TimeLeftDigits
 * WHAT IT DOES: Draws the time left: two digits, three from $100, or below
 * 10 one large digit whose size and place come from the table at $07:8DDA
 * by the countdown, with sound $42 each time it changes.
 */
void TimeLeftDigits(void)
{
    uint16_t v;

    if (!g.finish_lines || g.player_phase[0] == 0x0C)
        return;
    v = g.time_left;
    if (!v) {
        if (g.time_left_shown)
            g.time_left_shown = 0;
        return;
    }
    if (v >= 0x0100) {
        g.time_left_shown = v;
        DigitGlyphSelect((v >> 8) & 0x000F);
        GlyphColours(0xBF, 0xB5, 0xBB);
        fx16_set(FXIN_PIECE_X, 0x009E);
        fx16_set(FXIN_PIECE_Y, 0x0008);
        FxGlyph16x32();
        fx16_set(FXIN_PIECE_X, 0x00AC);
        DigitGlyphSelect((v >> 4) & 0x000F);
        FxGlyph16x32();
        DigitGlyphSelect(v & 0x000F);
        FxGlyph16x32();
        return;
    }
    if (v >= 0x0010) {
        g.time_left_shown = v;
        DigitGlyphSelect(v >> 4);
        if (v >= 0x0020)
            GlyphColours(0xBF, 0xB5, 0xBB);
        else
            GlyphColours(0xBF, 0xBC, 0xBD);
        fx16_set(FXIN_PIECE_X, 0x00AC);
        fx16_set(FXIN_PIECE_Y, 0x0008);
        FxGlyph16x32();
        DigitGlyphSelect(v & 0x000F);
        FxGlyph16x32();
        return;
    }
    if (v != g.time_left_shown) {
        g.time_left_shown = v;
        SoundPort0QueuePush(0x42);
    }
    DigitGlyphSelect(v);
    {
        uint16_t at = (uint16_t)(COUNTDOWN_DIGIT_SHAPES + (uint16_t)(((g.countdown_fraction >> 8) >> 4) * 8));

        fx16_set(FXIN_PIECE_W, cart16(HUD_BANK, at));
        fx16_set(FXIN_PIECE_H, cart16(HUD_BANK, at));
        fx16_set(FXIN_PIECE_U, cart16(HUD_BANK, (uint16_t)(at + 2)));
        fx16_set(FXIN_PIECE_V, cart16(HUD_BANK, (uint16_t)(at + 2)));
        fx16_set(FXIN_PIECE_X, cart16(HUD_BANK, (uint16_t)(at + 4)));
        fx16_set(FXIN_PIECE_Y, cart16(HUD_BANK, (uint16_t)(at + 6)));
    }
    GlyphColours(0xBF, 0xB7, 0xB6);
    FxScaledSpriteAdvance(0);
}

/* @routine 07:925C GsuDigitPair0E74At: the two digits of the bonus at (x, 8). */
void BonusDigitsDrawAt(uint16_t x)
{
    fx16_set(FXIN_PIECE_X, x);
    fx16_set(FXIN_PIECE_Y, 0x0008);
    DigitGlyphSelect((g.digit_pair >> 4) & 0x000F);
    FxGlyph16RowsFromTop();
    DigitGlyphSelect(g.digit_pair & 0x000F);
    FxGlyph16RowsFromTop();
}
