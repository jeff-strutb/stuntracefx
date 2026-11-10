/*
 * hud.h: the HUD's drawing and its glyph programs.
 */
#ifndef STUNTRACEFX_HUD_H
#define STUNTRACEFX_HUD_H

#include <stdint.h>

#include "object/object.h"

/* the frame's HUD work (hud_frame.c) */
void HudDrawTime(void);
void HudStuntScoreDigits(void);
void HudDrawNumber3(uint8_t n, int sprite);
void HudDrawValueA(void);
void HudDrawValueB(void);
void HudDrawValueC(void);
void HudDigitSprites62(void);
void StuntScoreUp(Object *o);
void HitsCollect(uint16_t rec);
void HitsToScoreAdd(void);
void HudGaugesPanelA(void);
void HudGaugesPanelB(void);
void MapMarkersPanelA(void);
void MapMarkersPanelB(void);
void HudSlideIn(void);
void HudSlideOut(void);
void HudPanelASlide(void);
void HudPanelBSlide(void);
void HudPanelsStep(void);
void SlideStep(void);
void OamClassName(void);

void DigitGlyphSelect(uint16_t digit);
void RankBadgePlayer1(void);
void RankBadgePlayer2(void);
void LapBannerRise(uint16_t input);
void LastLapBannerRise(uint16_t input);
void BonusDigitsDraw(void);
void ShowWordWin(void);
void ShowWordCrash(void);
void ShowWordLose(void);
void ShowWordDraw(void);
void ShowGetAllTheStars(uint16_t input);
void GlyphRowsAt32(uint16_t input);
void GlyphRowsAt56(uint16_t input);
void TimeFrameTailB(void);
void TimeFrameTailA(void);
void TimeFrameLeft(void);
void TimeFrameLeftShifted(void);
void TimeFrameRight(void);
void TimeFrameRightMirrored(void);
void TimeFrameMiddle(void);
void ShowLapsToGo(void);
void HudSixGlyphs(void);
void BigSpritesRow6(void);
void BigSpritesRow6B(void);
void BigSpritesRow5(void);
void BigSpritesNine(void);
void GlyphAtY60(uint16_t glyph, uint16_t x);
void GlyphListDraw(void);
void GlyphSpriteDraw(int sprite);
void GlyphSpritesDrawAll(void);
void GlyphSpritesInit(void);
void ShowBonusTrax(void);
void GlyphGrid1A(void);
void GlyphGrid1D(void);
void GlyphSlideThree(void);
void GlyphTimesSequence(void);
void GlyphSpritesGrowAndTimes(void);
void GlyphSpritesSlideOut(void);
void GlyphSpritesSlideIn(void);
void HudHoldDraw(void);
void HudTimesDraw(void);
void HudTimesPackedDraw(void);
void HudLeadWordDraw(void);
void HudLapDigitsDraw(void);
void HudBlankDraw(void);
void TimeDigitsStore(uint8_t minutes, uint8_t seconds, uint8_t hundredths);

/* wanted (from the records, menu/save): */
uint16_t BinaryToBcd(uint16_t value);
void TimeUnpackBcd(uint16_t packed, uint16_t *minutes, uint16_t *seconds, uint16_t *hundredths);
uint16_t TimePackBcd(uint8_t hundredths, uint8_t seconds, uint8_t minutes);
void TimeTotalsSum(void);
void TimeRunningTotals(void);
void RecordTimeInsert(void);

/* The HUD's tables in bank 7 the race's and the glyphs' code both read:
   the time frame's X offsets by its animation step. */
enum { HUD_BANK = 0x07, FRAME_X_OFFSETS = 0x9806 };

#endif
