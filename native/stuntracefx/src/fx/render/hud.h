/*
 * hud.h: the Super FX programs that draw the HUD's 2D pieces (hud.c): the
 * glyphs and digits, the banner's rising picture and the scaled sprites.
 * Each takes the piece it draws; the race keeps its own (hud/glyphs.c).
 */
#ifndef STUNTRACEFX_FX_RENDER_HUD_H
#define STUNTRACEFX_FX_RENDER_HUD_H

#include <stdint.h>

/* A piece as the programs take it. Which fields a program reads, and which
   it moves on, is said at each one. */
typedef struct {
    uint8_t colours[4];                 /* the colour a 2-bit pixel 0-3 draws */
    uint16_t x, y;                      /* the piece's left and top */
    uint16_t w, h;                      /* the advance or the size across; the rows or the size down */
    uint16_t u, v;                      /* a scaled sprite's steps through its picture, across and down (8.8) */
    uint16_t glyph;                     /* the picture: its address in its cartridge bank; 0 is none */
} FxPiece;

/* Glyphs, 2 bits a pixel, rows 32 bytes apart. Each moves p->x on. */
void FxGlyph8x8(FxPiece *p);            /* on by 8; nothing for glyph 0 */
void FxGlyph8x16(FxPiece *p);           /* on by 8; nothing for glyph 0 */
void FxGlyph16x16(FxPiece *p);          /* on by 16 */
void FxGlyph16x32(FxPiece *p);          /* on by 16; nothing for glyph 0 */
void FxGlyph24x16(FxPiece *p);          /* on by 16 */
void FxGlyph24x32Bank04(FxPiece *p);    /* on by 16; from bank $04 */
void FxGlyph16RowsFromBottom(FxPiece *p); /* the glyph's bottom p->h rows at the cell's top; on by p->w */
void FxGlyph16RowsFromTop(FxPiece *p);  /* the glyph's top p->h rows at the 32-line cell's bottom; on by p->w */

/* The banner's picture rising out of a line, its top `rows` rows. */
void FxPictureRise40(const uint8_t colours[4], uint16_t rows);
void FxPictureRise60(const uint8_t colours[4], uint16_t rows);
void FxPictureRiseSplit40(const uint8_t colours[4], uint16_t rows);
void FxPictureRiseSplit60(const uint8_t colours[4], uint16_t rows);

/* Scaled sprites from p->glyph at (p->x, p->y), stepping p->u and p->v
   through the picture, clipped to the view's right and bottom edges. */
void FxScaledSprite13(const FxPiece *p, uint16_t right, uint16_t bottom);   /* p->w / 2 * 13 / 16 across, p->h / 16 down */
void FxScaledSprite12(const FxPiece *p, uint16_t right, uint16_t bottom);   /* p->w / 2 * 12 / 16 across, p->h / 16 down */
void FxScaledSpriteEighth(const FxPiece *p, uint16_t right, uint16_t bottom); /* p->w / 32 across, p->h / 32 down */
void FxScaledSpriteAdvance(FxPiece *p, uint16_t right, uint16_t bottom);    /* p->w / 16 across, p->h / 8 down; x on by its width */
void FxSprite24x40(FxPiece *p, uint16_t right, uint16_t bottom);            /* 24 x 40; x on by 24 */
void FxSprite16x32(FxPiece *p, uint16_t right, uint16_t bottom);            /* unscaled (both steps set to 1); see hud.c */

#endif
