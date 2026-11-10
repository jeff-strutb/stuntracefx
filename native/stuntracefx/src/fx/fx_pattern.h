/*
 * fx_pattern.h: simple pattern textures drawn as geometry (plan stage 10,
 * "Pattern textures as geometry").
 */
#ifndef STUNTRACEFX_FX_PATTERN_H
#define STUNTRACEFX_FX_PATTERN_H

#include <stdint.h>

typedef struct FxPattern FxPattern;

/* The pattern for a textured face's texture: the ROM bank, base and mask
   its draw-list entry gives, and the options' choice of the byte's part
   (0x04 the high four bits, else 0x08 the low four, else all eight). NULL
   when the texture is not one. */
const FxPattern *fx_pattern_find(uint8_t bank, uint16_t base, uint16_t mask, uint8_t options);

/* A flat piece of the pattern: n points in view space and the texel value
   the piece shows (the byte's part, as the texture's own texels give it). */
typedef void (*FxPatternPiece)(void *context, const double (*p)[3], int n, uint8_t value);

/* The pattern laid on a face of n corners (view-space points `xyz`, texel
   coordinates `uv` as the GPU would lay the texture: the entry's u, v with
   the offsets added), fanned from its first corner; each piece goes to
   `piece`. Returns 0, having made no piece, when the face covers too many
   tiles or its texture coordinates do not span an area. */
int fx_pattern_cut(const FxPattern *pattern, const double (*xyz)[3], const double (*uv)[2], int n,
                   FxPatternPiece piece, void *context);

#endif
