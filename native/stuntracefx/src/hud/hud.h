/*
 * hud.h: the HUD's drawing and its glyph programs.
 */
#ifndef STUNTRACEFX_HUD_H
#define STUNTRACEFX_HUD_H

#include <stdint.h>

void GsuF441Player1(void);
void GsuF441Player2(void);
void GsuF0A5OrF0B4(uint16_t input);
void GsuEEF5OrEFCD(uint16_t input);
void GsuDigitPair0E74(void);
void GsuF15BThree(void);
void GsuF15BFive(void);
void GsuF15BFourA(void);
void GsuF15BFourB(void);
void GsuF15BTwoLines(uint16_t input);
void GlyphRowsAt32(uint16_t input);
void GlyphRowsAt56(uint16_t input);
void GsuFA1BFromTables(void);
void GsuFA35FromTables(void);
void GsuFA5AThree(void);
void GsuFA5AThreeShifted(void);
void GsuFA5ASix(void);
void GsuFA5ASixLeft(void);
void GsuFA5AFromTables(void);
void GsuPiecesStep10CB(void);
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
void GlyphWordAt1C28(void);
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

#endif
