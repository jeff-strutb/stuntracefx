/*
 * fullwindow.h: Enhanced mode's full-window race (remaster/fullwindow.c).
 *
 * In Enhanced mode a one-player race fills the window: the screen is as
 * wide as the window's aspect (nativeshvc's wider screen), BG1 is one
 * canvas (nativeshvc's character canvas) over all of it, and every picture
 * shown is the 3D drawn again (tween.c's redraw, the state kept and put
 * back) with the view over the whole screen at k times the console's
 * pixels (k the canvas's scale for the window): the view's words at W k x
 * 224 k, the focal length at k times, every PLOT into the cartridge's own
 * texels in 16-bit coordinates. The HUD's Super FX pieces are drawn there
 * too, at the console's pixel size (k x k texels a pixel), and the
 * panels' characters over the 3D. Off, everything is the original's.
 */
#ifndef SRFX_FULLWINDOW_H
#define SRFX_FULLWINDOW_H

#include <stddef.h>
#include <stdint.h>

/* the full-window race this frame (Enhanced, one player's race screen,
   the console's canvas) */
int fw_on(void);
/* a full-window picture being drawn (the redraw's PLOTs go to the texels);
   fw_hud while its HUD pieces are (the original view's pixels, placed) */
extern int fw_drawing, fw_hud;
/* the picture's size in texels, and k */
int fw_width(void);
int fw_height(void);
int fw_k(void);

/* PLOT and RPIX into the texels at (x, y), with the Super FX's colour and
   plot options (gsu.colr, gsu.por) as PLOT takes them */
void fw_plot(uint16_t x, uint16_t y);
/* a scaled sprite's rows as spans now (the full window's 3D: not the HUD,
   a label or the check's pass) */
int fw_sprite_rows(void);
/* a scaled sprite's row (render/frame.c): columns x0 to x1 of row y, the
   texel of column x the byte at `at` + (frac + (x - x0) step) / 65536 in
   bank (at minus it when back), through COLOR's and PLOT's rules */
void fw_sprite_span(uint16_t y, uint16_t x0, uint16_t x1, uint16_t bank, uint16_t at, uint16_t frac,
                    uint16_t step, int back);
/* The nearest car's label (FxTextNotice) in the full window's 3D: its
   string drawn with each pixel K x K texels, at the original's place on
   the view: the label's X past the view's centre (half the car's side
   offset, in console pixels) K times, its row ($68, the view's centre
   line + 40) K rows a row; x and y the position FxTextNotice set. 0 when
   not in the full window's 3D. fw_label_end after the string. */
int fw_label_begin(uint16_t x, uint16_t y);
void fw_label_end(void);
uint8_t fw_rpix(uint16_t x, uint16_t y);

/* GsuParamsFromDp's eight words (the centre, the edges, the frustum
   planes) for the full-window view, given the six the game set: 1 when
   they are the one-player race's and a full-window picture is being
   drawn (out then set) */
int fw_view_words(const uint16_t w[6], uint16_t out[8]);
/* the focal length (view units to texels) and half-width for the
   full-window view (view.c's projection and cull read them) */
int fw_focal(void);
/* the full-window view is in force (its words set, not the HUD's pieces) */
int fw_view_active(void);
int fw_half_width(void);

/* the redraw's start (race_frame.c RmTweenDraw): the full-window view's
   words in the Super FX's RAM (the redraw puts the RAM back after) */
void fw_view_begin(void);
/* the HUD's pieces drawn into a full-window picture from here to the end
   (tween.c's redraw: the text program, RaceSoundsAndHud): the original's
   view words put back for them, their pixels placed (fw_hud) */
void fw_hud_begin(void);
void fw_hud_end(void);
/* the race screen's stages ran (video/dma.c), this frame */
void fw_race_stage(void);

/* a Super FX program begins (1) or ends (0) (the cartridge's board,
   front/superfx_cart.c): the HUD's pieces placed whole */
void fw_gsu_program(int begin);

/* The screen clip from the projection's exact results (full window):
   fw_exact_begin as a polygon's projected list at `list` is made, then each
   vertex's view-space point (fw_exact_point) or the model drawer's point by
   index (fw_exact_model_point); fw_exact_clip at FxPolygonClipScreen's
   entry: 1 when it clipped the list (R0 the count), 0 to clip as ever */
void fw_exact_begin(uint16_t list);
void fw_exact_point(int16_t x, int16_t y, int16_t z);
void fw_exact_model_point(uint16_t index);
/* the model drawer's vertex of point `index` written into the draw list
   at `at`: its exact place kept for the fill where the projection held it */
void fw_exact_vertex(uint16_t at, uint16_t index);
/* the face at `list` goes on to a clip (model.c): its vertices' marks gone,
   the clip writes its own */
void fw_exact_unmark(uint16_t list);
int fw_exact_clip(void);
/* FxModelFacing (model.c): each triangle's three points' addresses, then
   its cross product (high and low words) from the exact points */
void fw_facing_point(int slot, uint16_t addr);
void fw_facing_cross(uint16_t *hi, uint16_t *lo);

/* A span of the picture's (fullwindow.c): row y, columns x0 to x1; flat,
   the colour; textured, each pixel's texel at ((v << 8 | u) & mask) + base
   in bank, u and v (32.32) at column x u + du (x - xl) and v + dv (x - xl),
   through COLOR's and PLOT's rules (por, the colour's high nibble hi);
   textured 2, perspective-correct: u = (pa + pda k) / (pq + pdq k) and v =
   (pb + pdb k) / (pq + pdq k) at k = x - xl, each a multiply-add and a
   divide in float as written (fw_span_fill_cpu, the GPU's the same);
   its 1 / depth along each pixel's ray z(x) = fma(zy, y, fma(zx, x, za))
   (0: none, never under the water), for fw_water */
typedef struct {
    int32_t y, x0, x1, pad0;
    int64_t xl, u, du, v, dv;
    uint16_t base, mask;
    uint8_t bank, por, hi, colour, textured, pad1[7];
    float pq, pdq, pa, pda, pb, pdb, za, zx, zy, pad2;
} FwSpan;
/* A polygon of the draw list's whole, in the spans' order in place of its
   rows: a span with textured 3, y its first row, x0 past its last, x1 its
   index in fw_polys. Its rows are made from its edges by fw_poly_row (the
   GPU's the same): rows ya to yb - 1, on the screen; left and right its
   chains of edges in fw_edges (first, count), each row on the one whose
   rows hold it; kind 0 flat (colour), 1 textured (bank, base, mask, por,
   hi; ou and ov the texture's offsets, 8.8; persp: perspective-correct,
   each edge's 1 / z at its ends) */
typedef struct {
    int32_t ya, yb, le, ln, re, rn, ou, ov;
    uint16_t base, mask;
    uint8_t bank, por, hi, colour, kind, persp, pad[6];
    float za, zx, zy, zpad;             /* its rows' 1 / depth (FwSpan's) */
} FwPoly;
/* The water (Enhanced, fullwindow.c): its 1 / depth along each pixel's ray
   w(x, y) = fma(fw_water[2], y, fma(fw_water[1], x, fw_water[0])), with
   fw_water[3] not 0 while there is water. A span's pixel past it, z > 0,
   w > 0 and z < w (the water nearer on the ray), is not drawn: the water
   is put there (fw_span_fill_cpu, the GPU's the same) */
extern float fw_water[4];
/* An edge: its ends' x (texels), its first row and its rows; u and v (the
   texture's 0-255) at its start and their change to its end (-128 to 127,
   the original's walk's); 1 / z at its ends */
typedef struct {
    int64_t from, to;
    int32_t y0, n, ua, va, dua, dva;
    float qa, qb;
    int32_t pad[4];
} FwEdge;
extern FwPoly *fw_polys;
extern FwEdge *fw_edges;
/* polygon p's row `row` as a span (screen width w): 0 when nothing of it
   is on the screen */
int fw_poly_row(const FwPoly *p, const FwEdge *edges, int32_t row, int32_t w, FwSpan *s);
/* a textured face's planes (model.c, after its points): its 1 / z, u / z
   and v / z for its draw-list vertices at `list`, from its points in view
   space (R3 negative: the face is textured) */
void fw_tex_plane(uint16_t list, uint16_t r3);
/* the spans to the texels (out, w x h): the reference on the CPU, and the
   one in use, which a front end with a GPU sets; fw_texel the bytes it
   reads, the Super FX's memory */
void fw_span_fill_cpu(uint8_t *out, int w, int h, const FwSpan *sp, size_t n);
extern void (*fw_span_filler)(uint8_t *out, int w, int h, const FwSpan *sp, size_t n);
uint8_t fw_texel(uint8_t bank, uint16_t at);
/* the texels' memory: malloc's, or a front end's the GPU shares (NULL: malloc) */
extern uint8_t *(*fw_texels_alloc)(size_t len);
/* FxDrawList's fill into the texels (raster.c, while fw_drawing) */
void fw_draw_list(void);
/* FxScreenClear's, while fw_drawing: the texels to colour 0 */
void fw_clear(void);
/* the sprite being drawn left out of the picture (1) or not (0) */
void fw_sprite_hide(int on);
/* a flat convex face of n points in view space, colour c, into the
   picture (near-clipped and projected exactly) */
void fw_view_face(const double (*p)[3], int n, uint8_t c);
/* Enhanced's sprites drawn as solid shapes (solids.c), from
   FxObjectsDraw (frame.c): the object's shape and its draw record's index
   (rm_record_object), then for a sprite past its depth test begin, its colour once its
   material is read, and end as it returns */
void fw_solid_object(uint16_t shape, unsigned record);
void fw_solid_begin(void);
/* FxObjectSprite past its depth test (1) and at its end (0): the sprite's
   depth for the water's (fullwindow.c) */
void fw_depth_sprite(int on);
void fw_solid_colour(uint8_t colr);
void fw_solid_end(void);
/* FxPolygonDraw's, while the full window's picture draws: 1 when done here */
int fw_exact_tex_draw(void);
/* FxPolygonGather's, while the full window's picture draws: begin with its
   count and index bytes' R14 (1 when it is done here), end after the
   original's run, its list, count and R0 replaced */
int fw_exact_gather_begin(uint16_t count, uint16_t r14);
void fw_exact_gather_end(void);

/* A picture j / RM_TICKS of the way through the step just run, drawn and
   handed to the canvas (remaster.c, at a race frame's wait); 0 when no
   picture could be drawn (the last one stays) */
int fw_picture(int j);
/* The race's upload stage (system/raster.c, 7E:A56B): the line of its IRQ
   (208, the original's; in the full window 240, a fallback, the stage run
   from the frame's hook instead: fw_vblank_uploads, at each frame's end,
   first); and at its end, in the full window, the sprites and BG1 set for
   it and the screen on again */
int fw_upload_line(void);
/* a race frame's wait over, its step to run (remaster.c): the work RAM the
   full window borrowed for its own HDMA given back */
void fw_before_step(void);
/* the sky's lines for the picture shown (remaster.c, after the backdrop's
   own for it) */
void fw_sky(void);
void fw_sky_take(void);
/* the hills' page on VRAM page 1 made now, for a backdrop put in before
   the frame's lines (the step's own picture's) */
void fw_backdrop_update(void);
/* the menus' screen changed the screen's size: the full window sets its
   own again when it next takes the screen */
void fw_screen_lost(void);
/* FW_PROF's frame times: at rm_frame_end's start (1) and end (0) */
void fw_prof_frame(int begin);
void fw_vblank_uploads(void);
void fw_upload_done(void);
/* at each frame's end: the console's screen and BG1 set for the mode */
void fw_frame_end(void);

#endif
