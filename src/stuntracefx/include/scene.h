/*
 * scene.h: the cartridge's GPU renderer's CPU side (remaster/scene.c),
 * docs/stuntracefx/GPU_RENDERER.md: the model command lists decoded once,
 * each picture's display list built from them (matrices, facing,
 * materials, order), the GPU drawing it.
 *
 * Built in stages, each checked against the Super FX's own run beside it.
 * SC_CHECK=geom (stage 1): each point command the model interpreter runs
 * (model.c), its points transformed natively by the chip's integer steps
 * and compared word for word with the ones it wrote ($0340 + 6i), its
 * packed matrix with the one it built; the counts printed at exit.
 * SC_BREAK=1 with it: one native word changed a command, to show the check
 * fails.
 */
#ifndef SRFX_SCENE_H
#define SRFX_SCENE_H

#include <stddef.h>
#include <stdint.h>

/* a model command about to run (model.c, at its dispatch through the
   table at 01:8000): its index (the command byte / 4) and the address of
   its byte (bank, address) */
void sc_command(unsigned index, uint8_t bank, uint16_t at);

/* A point command's points by the chip's integer steps: n entries of three
   bytes (x, y, z as the ROM holds them) at `bytes`, the shift ($0032) and
   scale ($0030), the view matrix m ($0132-$0142, nine words, row i column
   j at m[3 i + j], as the chip holds them after a far object's shift);
   mirrored, two points an entry (x, then x negated). out: 3 words a point,
   x, y, z, as the chip writes them at $0340 + 6i. Returns the points. */
int sc_points(const uint8_t *bytes, int n, int mirrored, uint16_t shift, uint16_t scale, const uint16_t m[9],
              int16_t *out);

/* FxModelDraw begins (model.c, 01:8561): the slot's matrix built natively
   from its inputs for the check */
void sc_model_begin(void);

/* A slot's view matrix by the chip's integer steps (FxModelDraw's set-up,
   01:8661 to 01:8778): the angle words at $0020, $0022, $0024 (their low
   bytes the angles; frac, when not NULL and not all 0, their 1/256
   fractions), the model flags ($0054: bit 13 flattens the object's y), the
   view's matrix e ($00E4, nine words) and a far object's halvings (shift).
   out: nine words, row i column j at out[3 i + j], as the chip leaves them
   at $0132 */
void sc_matrix(const uint16_t angle[3], const uint8_t *frac, uint16_t flags, const uint16_t e[9], int shift,
               uint16_t out[9]);

/* The walk of a model's command lists (scene_walk.c): FxModelDraw begins
   (the list at $0016, bank $001C, the projection going on to $0018, the
   frame $0044), a command dispatched, and a face the chip goes on to draw
   (01:90F0, its record's address); and the walk's own faces and facing
   commands as it meets them */
void sc_walk_begin(void);
void sc_walk_command(uint8_t bank, uint16_t at);
void sc_walk_chip_face(uint16_t at);
/* the projection found every point past one edge (01:924D): the model ends */
void sc_walk_rejected(void);
/* the chip's material for the face it just took (01:9112, R0 its result) */
void sc_walk_chip_material(uint16_t r0);
void sc_walk_face(uint16_t at);
void sc_walk_facing(uint16_t at, uint16_t n);

/* ---- the display list (stage 4): a picture's models for the GPU ----
   SC_GPU=1: the full window's model faces drawn by the cartridge's GPU from
   this list, in the walk's order (the chip still runs beside it: the
   walk's inputs and its face order are taken in step, scene_walk.c). Each
   slot: its matrix (row i column j at m[3 i + j], the chip's after a far
   object's halving), its place, its point scale and shift (below 3 the
   packed matrix's path, sc_points); each point: its three bytes as the ROM
   holds them, mirrored (the x term subtracted), its slot; each face: its
   corners (first, n: 2 a line), flat (its colour) or textured (bank, base,
   mask, the plot options and the colour's high nibble, the texture's
   offsets in 8.8); each corner: its point and its u, v. The GPU
   transforms each point by the chip's integer steps, projects, clips at
   the near plane and fills; the CPU touches no vertex's place. */
typedef struct {
    int16_t m[9], pos[3];
    uint16_t scale, shift, pad[2];      /* pad[0] unused, 0; pad[1] a far object's halvings: its true depth, z << them, for the water */
} ScSlot;                               /* 32 bytes */
typedef struct {
    int8_t b[3];
    uint8_t mirrored;
    uint32_t slot;
} ScPoint;                              /* 8 bytes */
typedef struct {
    uint32_t first, n;
    uint8_t textured, colour, bank, por, hi, pad[3];
    uint16_t base, mask;
    int16_t ou, ov;
} ScFace;                               /* 24 bytes */
typedef struct {
    uint32_t point;
    float u, v;
} ScCorner;                             /* 12 bytes */

/* The full window's own solids (solids.c: tyres, eyes, wheels, trees),
   in the same list: a float slot (view = pos + p's object coordinates
   through m, row i column j at m[3 i + j], the solid's scale in it) and
   its points in object space; a corner whose point has SC_FLOAT_POINT set
   names a float point (the rest of the index), a flat face as the
   models' are */
typedef struct {
    float m[9], pos[3];
    float zs, pad[3];                   /* a far object's halvings as a factor: its true depth, z zs, for the water */
} ScFSlot;                              /* 64 bytes */
typedef struct {
    float p[3];
    uint32_t fslot;
} ScFPoint;                             /* 16 bytes */
#define SC_FLOAT_POINT 0x80000000u
/* a float slot for a solid (its index, or -1 when no list is being made) */
int sc_list_fslot(const float m[9], const float pos[3]);
/* a solid's flat face of n object-space points in fslot, colour c: its
   place in the list, or -1 */
int sc_list_float_face(int fslot, const float (*p)[3], int n, uint8_t c);
const ScFSlot *sc_list_fslots(size_t *n);
const ScFPoint *sc_list_fpoints(size_t *n);

/* ---- the objects' own primitives: projected by the GPU ----
   A scaled sprite, a particle's dot or line, a drop of the weather: the
   CPU lists what the object is in view space; the GPU makes its screen quad
   (FW_QUAD) by the chip's integer rule (gpu_vk/prims.comp, gpu_metal.m's
   prims). In the picture's order a
   placeholder span (FW_QUAD, pad1[1] bit 6, xl the primitive's index, no
   pixels) stands for it. */
enum { SC_PRIM_SPRITE, SC_PRIM_DOT, SC_PRIM_LINE, SC_PRIM_WDOT, SC_PRIM_WLINE };
typedef struct {
    int16_t x, y, z, own;               /* its place; a sprite's own size ($00CE) */
    uint16_t aa, shift, far, rows;      /* a sprite's size ($00AA), its $0032, a far object's halvings, a fixed size's texel rows */
    uint16_t base, mask;                /* a sprite's texture ($00A8, $00D4) */
    uint8_t bank, por, hi, colour;
    uint8_t kind, n, pad[2];            /* n: a dot's size */
    int16_t x2, y2, z2;                 /* a line's other end: a particle's place, or a drop's last pixel */
    uint16_t pad2;
    float za;                           /* a sprite's 1 / depth (FwSpan's) */
} ScPrim;                               /* 40 bytes */
/* what the GPU needs of the view to make them, before the primitives in its
   buffer: the screen, the centre ($0034, $0036), the edges ($0038-$003E),
   the cap on a sprite's rows, the texels square, the focal lengths as
   ws_focal_q gives them, the texels a console pixel (SXN / SXD, SYN / SYD) */
typedef struct {
    int32_t tw, th, cx, cy, left, right, top, bottom, rows_cap, square;
    int32_t fxn, fxd, fyn, fyd, sxn, sxd, syn, syd, pad[2];
} ScPrimView;                           /* 80 bytes */
int sc_list_prim(const ScPrim *p);
const ScPrim *sc_list_prims(size_t *n);
/* the view they are made in, taken as the walk begins: the RAM is put back
   before the list reaches the GPU */
const ScPrimView *sc_list_prim_view(void);
void sc_list_prim_view_set(void);
/* the ROM's tables the GPU reads for them: the depth scale at $14:896C and
   the reciprocals at $00:94A0 */
#define SC_TABLE_BANK_SCALE 0x14
#define SC_TABLE_BANK_RECIP 0x00

/* The frame program (01:AB13) in a full-window picture, natively: the sorted
   slots to the display list, the CPU projecting nothing and rejecting
   nothing (scene_frame.c). 1 when it stands for the program. FW_GPUCHECK:
   the list made, the state put back and 0, the program run after it for the
   reference alone, until sc_frame_ref_end. In Original, no access of the
   chip's: fw_view_active is 0. */
int sc_frame_native(void);
void sc_frame_ref_end(void);
/* the program being run for the reference alone (FW_GPUCHECK, above) */
extern int fw_ref_pass;
/* a model in the native walk: its inputs as the frame program leaves them
   in the Super FX's RAM ($0016-$002A, $0030-$0054, $00C0, $00C2, $01B6);
   its faces into the picture in the list's order */
void sc_walk_model(void);

/* SC_GPU set */
int sc_gpu_on(void);
/* a full-window picture begins: the list emptied */
void sc_picture_begin(void);
/* the list as it stands */
const ScSlot *sc_list_slots(size_t *n);
const ScPoint *sc_list_points(size_t *n);
const ScFace *sc_list_faces(size_t *n);
const ScCorner *sc_list_corners(size_t *n);
/* the chip queued a face in its draw list (01:9237, the entry's type byte
   at `entry`); fw_draw_list asks which list face an entry is (-1: none),
   and when the list is drawn its entries are forgotten */
void sc_walk_listed(uint16_t entry);
int sc_face_for_entry(uint16_t entry);
void sc_entries_drawn(void);

#endif
