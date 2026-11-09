/*
 * video.h: the picture's routines other areas call, by their original names.
 *
 * Order within an update: the work of a race frame runs from the vertical
 * blank before its picture to the lines inside it, so the stage that uploads
 * (RasterPaletteUpload and the windows' set-up) runs first and the steps the
 * original ran during the picture's lines run after it. video_draw then
 * draws that one picture.
 */
#ifndef STUNTRACEFX_VIDEO_H
#define STUNTRACEFX_VIDEO_H

#include <stdint.h>

#include "data/cart.h"
#include "nshvc/nshvc.h"

/* tables in the code the original copied to work RAM ($7E:A2D9 up from ROM
   $01:0000, $7F:0800 up from $05:8000), by the address they ran at */
static inline const uint8_t *ram_code_7e(uint16_t a) { return &cart[0x10000u + (uint32_t)(a - 0xA2D9u)]; }
static inline uint16_t ram_code_7e16(uint16_t a) { const uint8_t *p = ram_code_7e(a); return (uint16_t)(p[0] | p[1] << 8); }
static inline uint8_t ram_code_7e8(uint16_t a) { return *ram_code_7e(a); }
static inline const uint8_t *ram_code_7f(uint16_t a) { return &cart[0x58000u + (uint32_t)(a - 0x0800u)]; }
static inline uint16_t ram_code_7f16(uint16_t a) { const uint8_t *p = ram_code_7f(a); return (uint16_t)(p[0] | p[1] << 8); }

/* The race screen's engine objects, by role. */
typedef struct {
    nshvc_tiles tiles;                  /* the 4bpp art the backdrop and sprites share */
    nshvc_map backdrop_one;             /* 32 x 64 */
    nshvc_tiles frame_tiles_two;        /* the two-player frame and panels, 4bpp */
    nshvc_map frame_map_two;            /* and its map */
    nshvc_image backdrop_image, panel_image; /* the two-player backdrop and panel, drawn into the list */
    nshvc_tiles course_tiles;           /* the course screen's backdrop art */
    nshvc_map course_map;               /* and its map, 32 x 64 */
    nshvc_image frame_image;            /* the frame and panels around the 3D, at screen positions */
} RaceVideo;

extern RaceVideo race_video;

void race_video_init(void);
void race_vram_write(uint16_t word, const uint8_t *data, uint32_t bytes); /* by the word address the original sent it to */
void race_course_vram_write(uint16_t word, const uint8_t *data, uint32_t bytes); /* the one-screen destinations, always */
void race_course_maps_keep(void);              /* the unpacked course picture to both maps */
void race_course_preview_reset(void);          /* $7F:C3D7 to $7F:9BD7, $800 bytes */
void race_frame_art_draw(nshvc_draw_list list);    /* the frame and panels, at screen positions, after the list is cleared */
void race_backdrop_two_pages_write(int first, const uint16_t *words, uint32_t n);
void VideoRegsReset(void);
void race_art_write(uint16_t word, const uint8_t *data, uint32_t bytes);
void race_frame_art_write(uint16_t word, const uint8_t *data, uint32_t bytes);
void race_map_page_write(int page, const uint16_t *words);
void race_panel_map_write(const uint16_t *words);
void race_frame_map_two_write(const uint16_t *words);
void race_two_player_list(void);
void race_backdrop_two_write(int page, const uint16_t *words);
void race_picture_shown(int buffer);
void race_backdrop_rows_upload(int buffer);
void race_panel_show(int half, uint8_t sign_of_y, uint8_t y_low, uint8_t y_high_when_plus, uint8_t y_when_minus, uint8_t x_low);
void RaceVideoSetup(void);
void PictureTurnStep(void);
void VramUploadGsuBuffer(void);
void RaceCourseUpload(void);
uint16_t *race_map_buffer(void);
void RaceHudLoad(void);
void NmiFrameUploadSprites(void);
extern uint8_t race_pictures[0x3000];
void race_screen_start(void);
void RaceVideoUpload1P(void);
void RaceVideoWindows1P(void);
void RaceVideoUpload2P(void);
void RaceVideoSplit2P(void);
void RaceScreenSetup(void);

/* ---- palettes ---- */

/* the colours as the game builds them; what is on screen is pic.palette, which
   only the named upload routines below change */
typedef struct {
    uint16_t buffer[256];               /* $7E:FD65: the colours built for the screen */
    uint16_t blended[256];              /* $7F:35D7: the course's colours as the fog has left them */
    uint16_t day[256];                  /* $7F:37D7: the course's colours */
    uint16_t night[256];                /* $7F:39D7: the course's other set, the weather's */
    /* the fog and gradient step's values */
    uint16_t band_end;                  /* $7F:345A */
    uint16_t blend_request;             /* $7F:345C: < 0 the target's low 15 bits and 2 more of amount, > 0 3 less */
    uint16_t weather;                   /* $7F:3460: how far the weather has gone, 0-31 */
    uint16_t weather_shown;             /* $7F:3464 */
    uint16_t weather_value;             /* $19FE: its fraction, $19FF the whole part */
    uint16_t weather_step, weather_goal; /* $1A00, $1A02 */
    uint16_t amount;                    /* $7F:3468: the blend toward the target, 0-31 */
    uint16_t amount_shown;              /* $7F:346C */
    uint16_t target;                    /* $7F:3470: the fog's colour; bit 15 none */
    uint16_t target_shown;              /* $7F:3474 */
    uint16_t gradient_was_off;          /* $7F:3478 */
    uint16_t band;                      /* $7F:347C: which of the three gradient bands is rebuilt */
    uint16_t hud_colours[6];            /* $0E21: the HUD's colours, blended */
    uint8_t cycle_phase[3];             /* $7E:FD5B, $FD5D, $FD5F: 0-15 */
    uint8_t course_cycle;               /* $0D30 */
    uint8_t tile_cycle;                 /* $0DD5 */
} Palettes;

extern Palettes palettes;

void PaletteLoad16(uint8_t bank, uint16_t src);
void PaletteCourseLoad(int course);
void PaletteDarkenFour(uint16_t request);

void RasterColours(void);
void RasterPaletteUpload(void);
void RasterColoursUnless0D22(void);
void PaletteKeptColoursStep(int player, int pad_index);
void PlayerColoursFlash(uint8_t level, int player);
void StatusColourPick(int item);
void PaletteCycle(void);
void PalettesStep(void);
void CoursePaletteCycle(void);
void HudColourStep(void);
void FadeValueStep(void);
void PaletteFadeStep(void);
void CgramDmaAll(void);                 /* the whole buffer to the screen */
void CgramUploadAll(void);              /* the same, and notes the player's offset */
void palette_shown_all(void);

uint16_t FadeStep(void);
uint16_t FadeStepFixed(void);

/* ---- the gradient ---- */

/* Colour 1 by line: three bands of 128 and the lists they are made from. */
#define GRADIENT_BAND_COLOURS 128
typedef struct {
    uint16_t band[3 * GRADIENT_BAND_COLOURS];   /* $7F:EC57 */
    uint16_t day[512];                  /* $7F:EF57 */
    uint16_t night[512];                /* $7F:F357 */
    uint16_t black[128];                /* $7F:F757 */
    uint16_t view_offset[2];            /* $0D96: byte offset into the lists, by view */
    uint16_t source[2];                 /* $0D9A: each view's start, a colour number into this block */
} Gradient;

extern Gradient gradient;

enum { GRADIENT_AT_BANDS = 0, GRADIENT_AT_DAY = 3 * GRADIENT_BAND_COLOURS,
       GRADIENT_AT_NIGHT = GRADIENT_AT_DAY + 512, GRADIENT_AT_BLACK = GRADIENT_AT_NIGHT + 512 };

/* colour number `n` from the block's start, past the black list counted as black */
uint16_t gradient_colour(uint32_t n);

void GradientBuildColours(int course);
void GradientSetup(void);
void GradientPointersToBank7F(void);

/* ---- windows and colour maths ---- */

/* a window setting: both windows' edges */
typedef struct { uint8_t left1, right1, left2, right2; } WindowEdges;

enum { WINDOW_BANDS_MAX = 16 };
typedef struct { uint8_t lines; uint16_t edges; } WindowBand;   /* the edges by the original table's number for them */

typedef struct {
    WindowBand band[WINDOW_BANDS_MAX];  /* $7F:F85B: a table of line counts and edges */
    uint8_t bands;
    WindowEdges normal;                 /* $7F:F857 */
    WindowEdges ffd1, ffd5, ffd9, ffdd; /* $7F:FFD1, $FFD5, $FFD9, $FFDD */
    uint8_t setup;                      /* $19A9: 0, 2, 4, 6 or 8 */
    uint8_t colours[6];                 /* $19AC-$19B1: the fixed colour's three parts for each half */
    uint8_t layout_request;             /* $19BD */
    uint8_t layout_moving;              /* $19BC: $FF while it moves */
    int16_t layout;                     /* $19B6 */
    int16_t layout_step;                /* $19B8 */
    /* the sliding, sweeping and bobbing windows */
    uint8_t bands_state;                /* $19D8 */
    uint8_t bands_kind;                 /* $19DF */
    uint16_t bands_pos, bands_step, bands_end;  /* $19D4 ($19D5 its high byte), $19D6, $19D9 */
    uint16_t split_pos, split_step;     /* $19DB ($19DC its high byte), $19DD */
    uint8_t split_tops[2];              /* $19E2, $19E3 */
    uint8_t split_bottoms[2];           /* $19E4, $19E5 */
    uint8_t windows_colour;             /* $19E0 */
    uint8_t slide_pos;                  /* $1A14 */
    uint8_t oscillator[2], oscillator_step[2], oscillator_count[2], oscillator_rate, oscillator_level[2]; /* $1A08, $1A0C, $1A0A, $1A10, $1A0E */
    uint8_t oscillator_input;           /* $1A11 */
    uint8_t sweep[2], sweep_step[2], sweep_halve[2], sweep_steps; /* $1A15, $1A19, $1A17, $1A1B */
    /* the fixed colour by line, for the weather and water */
    uint8_t level_table[64];            /* $7F:FF8B */
    uint8_t level_table_bytes;
    uint16_t level;                     /* $19C3 */
    int16_t level_step;                 /* $19C5 */
    int8_t top_step, low_step;          /* $19C7, $19CD */
    uint16_t player_y;                  /* $19C9 */
    uint8_t top_band, low_band;         /* $19CB, $19CF */
    uint8_t band_limits;                /* $19D1 */
    uint8_t player_raised;              /* $19D3 */
    /* the wipe that opens the race */
    uint8_t wipe_table[36];             /* $7F:3410 */
    uint8_t wipe_left, wipe_a, wipe_top, wipe_right, wipe_b, wipe_bottom; /* $7F:3433, $3434, $3435, $3437, $3438, $3439 */
    uint8_t wipe_width, wipe_middle;    /* $7F:3402, $3406 */
    /* the colour maths */
    uint8_t maths;                      /* $0D47, as the original's CGADSUB */
    uint8_t fixed_step;                 /* $7F:3408 */
    uint8_t fixed_level;                /* $7F:340D */
} Windows;

extern Windows windows;

void WindowsInit(void);
void WindowHdmaNormal(void);
void WindowHdmaBands(void);
void WindowHdmaFFD1(void);
void WindowLayoutStepForScreen(void);
void WindowSlideOrOscillate(void);
void WindowSweepIfOn(void);
void WindowSlideCheck(void);
void BobWindowsRun(void);
void WindowBandsAnimate(void);
void WindowSplitAnimate(void);
void WindowsSetupSelect(void);
const WindowEdges *window_edges_at(uint16_t addr);
void WindowsColourAddBg12(void);
void WindowsColourWindowOnly(void);
void WindowsAllInverted(void);
void WindowTable7F(void);
void WindowEdgesStep(void);
void ColourMathA3(void);
void ColourMathA7Start(void);
void ColourMathOff(void);
void WindowHdmaFFD5(void);
void WindowHdmaFFD9(void);
void WindowHdmaFFDD(void);
uint8_t FixedColourFromStep(void);

/* ---- the backdrop and its scroll ---- */

void RaceScreenSetup(void);
uint16_t BackdropScrollFromView(void);
void BackdropRowsUpdate(void);
void BackdropAnglesFromView(void);
void ScrollHdmaSet(void);
void Bg2ScrollFromTable(int half);

/* ---- tile sets and maps ---- */

void RaceVideoInit(void);
void TilemapPaletteCycle87(void);
void TilemapPaletteCycle107(void);
void TilemapPaletteCycle187(void);
void TilemapRow87Palette5(void);
void TilemapRow107Palette5(void);
void TilemapRow187Palette5(void);
void TilemapRow187Palette4(void);
void TilemapRow207Palette4(void);
void TilemapRow287Palette4(void);
void TilemapLevelColumns(void);
void DigitTiles1184(void);
void VramNoiseTiles(void);
void TimesKeepAndAdvance(void);
void ChampionshipStageStart(void);
void ChampionshipStageWaitOpen(void);
void ChampionshipStagePicture(void);
void ChampionshipStageMap(void);
void ChampionshipStageWaitClosed(void);
int WindowClosedDigitStep(void);

/* ---- sprites ---- */

extern nshvc_sprite sprites[128];
/* the tile set and page starts sprite tile numbers count in: each screen sets its own */
void sprites_use_tiles(nshvc_tiles set, uint16_t first_page, uint16_t second_page);
uint8_t OamByte(uint16_t offset);                 /* a byte of the table, 0-$21F */
void OamBufferByteSet(uint16_t offset, uint8_t value);
void OamDmaAll(void);
void OamDirectStage(void);
void OamHideRange(int first, int count);
void OamBufferClear(void);
void OamBufferHideLarge(void);
void SpriteSlot1Countdown(void);
int SpriteSlotFirst(void);
void OamFromList(uint8_t bank, int *slot, uint16_t *list);
void OamSprite0AddY(uint8_t lines);
void oam_attr_fill(int first, int count, uint8_t attr);
void OamAttr36x4(int first);
void OamAttr36x6(int first);
void OamAttr36x7(int first);
void OamAttr38x4(int first);
void OamAttr38x6(int first);
void OamAttr38x7(int first);
void OamHideEntries3And9(int first);
void OamHideEntries0To2(int first);
void OamSetPalette(uint8_t bits, int count, int first);
void OamPaletteFromTableE30F(void);
void OamPaletteFromTableE26A(void);
void OamPaletteFromTableE26AByMode(void);
void OamPalettesE28D(void);
void OamPaletteForD75(void);
void OamBlock52Left(void);
void OamBlock52Right(void);
void OamBlock59Left(void);
void OamBlock59Right(void);
void OamBlock123Left(void);
void OamBlock123Right(void);
void OamColumn8(int first, uint16_t pos, uint16_t tile);
void OamHighBits18(void);
void OamHighBits58(void);
void OamHighBits18Park48(void);
void OamHighBits58Park112(void);
void OamHide36And96(void);
void OamLargeColumns(void);
void OamPlaceRows75And112(void);
void SpriteStrip4(uint16_t selector, int first, uint16_t pos);
void SpriteStrip6(uint16_t selector, int first, uint16_t pos);
int MetaspriteDraw(uint16_t group, int slot);              /* returns the sprites used */
void FlashingMetaspriteDraw(void);
void FlashingMetaspriteDraw0(void);
void FlashingMetaspriteDrawByMode(void);
void SpritePiecesAtLeft(uint16_t set, int slot);
void SpritePiecesAtRight(uint16_t set, int slot);
void SpritePiecesAt46Y71(uint16_t set, int slot);
void SpritePiecesAt46Y82(uint16_t set, int slot);
void SpritePiecesAt46Y93(uint16_t set, int slot);
void SpritePiecesAt62Y71(uint16_t set, int slot);
void SpritePiecesAt62Y83(uint16_t set, int slot);
void SpritePiecesAt62Y95(uint16_t set, int slot);
void SpritePiecesCarAt51(uint16_t car, int slot);
void SpritePiecesTwoCars(void);
void SpritePiecesTwoCarsTop(void);
void OverlaySpritesStep(void);
void OamTextLine(int slot, uint8_t tile_base, uint8_t x, uint8_t y, uint8_t attr);
void SpriteTextAt895E(void);
void SpriteTextAt8152(void);
void SpriteTextAt8952(void);

/* wanted (menu/records_screen.c): a row's car, three pieces */
void SpritePieces3(uint16_t x, uint8_t y, uint16_t set, uint16_t palette, int slot);
/* wanted by the race (race/phases.c, the lap and results panels) */
void race_unpacked_to_art(uint16_t out, uint16_t in);  /* a packed block to its VRAM place, as RaceLoad's GsuCF42ToVram */
void OamByteSet(uint16_t offset, uint8_t value);   /* a byte of the sprite table, 0-$21F */
uint8_t SpriteYOnScreen(uint8_t y);                /* 7E:B005 */
void SpriteBar4WithMarker(uint8_t value, uint16_t pos, uint16_t slot);
void SpriteBar6WithMarker(uint8_t value, uint16_t pos, uint16_t slot);
void OamHide75Up(void);
void OamHideForHudMode(void);

#endif
