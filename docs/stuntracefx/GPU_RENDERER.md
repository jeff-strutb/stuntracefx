# The cartridge's GPU renderer (design; stage 1 being built)

Every picture SRFX draws is drawn by a GPU: Metal (macOS, iOS), Vulkan
(Windows, Linux) and the PSP's GE, from one display list. The CPU runs the
game and builds the display list: meshes, matrices, materials, draw order.
It touches no pixel, no row and no projected vertex.

This covers Enhanced's pictures (the full window, the menus' 3D, two
players, Enhanced without widescreen, the pause panel) and Original's
Super FX bitmap.

## Original mode: the Super FX's bitmap on the GPU

The Super FX was the cartridge's own drawing processor. The S-CPU started
a program, waited for its STOP and sent the bitmap from the cartridge's RAM
to VRAM by DMA. The console saw three things: when a job started, how long
it took, and what the RAM held at STOP.

- **The bitmap.** The GPU draws it with the chip's own rules (edges from
  the draw list's vertices by the chip's 8.8 slopes and reciprocal table,
  texels by its u, v steps, COLOR and PLOT's options), packs it into the
  chip's bitplanes, and the CPU reads it back into the cartridge's RAM at
  STOP. Checked: byte-identical to the CPU reference at every STOP.
- **RPIX.** Every RPIX the census found in the drawing programs is a
  pixel-cache flush whose value is overwritten. The one that ends the frame
  program (01:AB7D) leaves its pixel in R0 at STOP: read from the bitmap
  read back there. FxHazeSample reads real pixels; it waits for the GPU
  (no play reaches it).
- **The job's duration.** The cartridge's statement, as the console sees
  it. It is priced from a model of what the job holds (its polygons, their
  areas and edges, flat or textured, its other primitives), refitted
  against the original's jobs (tools/stuntracefx/gsuprice.py) and the same
  on every host. Jeff's decision, 2026-11-04. Pacing then no longer matches
  HEAD byte for byte; the check is the model's accuracy against the
  original's jobs. Open: where the accesses fall within a job, for the
  S-CPU's bus holds.

## Enhanced: the split

- **CPU, once per game step (unchanged):** the S-CPU code, the Super FX's
  physics, the draw-list build and the sort. Behaviour-exact.
- **CPU, once per picture:** the tweened inputs, the view matrix and the
  sorted slots; per slot its matrix (the chip's integer algorithm), per
  face its facing and material, per command list its flow (frame picks,
  conditional lists, calls); out goes the display list.
- **GPU, per picture:** transform, near clip, projection, rasterization,
  texturing, then the ground and water, then the HUD and panels; the
  finished picture handed to nativeshvc as a GPU texture (contract asked,
  see below).

## Census (2026-11-04, nine dense scripts and race100, both modes)

Model commands reached ($cc the command byte):

| Reached | Not reached |
|---|---|
| $00 end, $04 points, $0C project, $14 faces, $1C frame pick, $20 jump, $28 if-skip, $30 facing, $38 mirrored points, $3C list start, $40 return, $44 push return, $58 list start, $5C if-call, $60 call, $64 return | $08/$4C full points, $10 depth sort, $34 scaled points, $50 sprite, $54 if-sprite, $18/$24/$2C/$48 (returns) |

- Flow reads only the facing flags at $07C6 (set by $30 from three points
  each), the model frame $0044 and its own bytes. No other RAM.
- Faces: 3 to 8 vertices; lines (n = 2) are common (1.5M in pictures),
  their second byte always 0.
- A face is skipped only when its facing byte is positive: the high byte
  of the 32-bit screen cross product (2^24 and up). Small back faces reach
  the fill; stage 2 settles what the fill does with them.
- Materials: plain 8.4M, masked (animated, bit 14) 0.30M, textured
  (bit 15) 34K, kind -2 (bits 15 and 14) none.
- Points: shift below 3 uses the packed 8-bit matrix ($0128-$0130, MULT);
  3 and up the full one ($0132-$0142, FMULT). The GPU's transform repeats
  the chip's integer steps (Metal, Vulkan); the GE's floats come within a
  unit.
- Super FX programs drawing in Enhanced's pictures: 01:AB13 (the frame
  program: objects, sprites, particles, weather, effects, text) and the
  HUD programs 07:F121, F15B, F6DF, FA88, FAF2. Step only: 07:EE88 (the
  screen fill), 07:F441 (battle), 07:F0A5.
- Not run: speed_all_courses.sfx (its `poke` step is not one nativeshvc's
  script runner takes).

Unreached commands stay supported in the decoder, each failing loudly
until a play reaches it and its check passes.

## Model cache

A command list is decoded once, on first use, per (bank, address): each
command becomes an op with its operands and its targets resolved, and each
points command a vertex block (the bytes as stored, the path: packed or
full matrix, mirrored or not). Blocks live in one GPU buffer per shape.

## Per-picture walk (CPU)

For each slot, in the sort's order (farthest first):

1. Its matrix by the chip's integer steps (FxMatrixFromAngles01, the
   transpose, the vectors), from the angles and their 1/256 fractions.
2. Its level of detail by view depth, as the frame program picks it.
3. The command list walked: frame picks from $0044, calls and returns,
   conditional lists from the facing flags. Each $30 facing test computes
   its three points' cross product from the transformed points (the
   reference's exact form in Enhanced: fw_facing_cross).
4. Faces: material resolved as FxModelMaterial does (flat, depth-shaded,
   lit, animated, textured), facing decided, index list emitted in the
   list's order.

## Drawing order and depth

Painter's order exactly as the original: slots far to near, faces in the
order their lists give. Depth is written (1/z) and never tested, so decals
and coplanar faces stay as drawn and the water pass has every pixel's
depth.

## Pixels

- Target: the canvas encoding of nshvc_ext_canvas_screen32 (0
  transparent, 1 CGRAM colour 1, 0x000000ii a CGRAM index, 0xFFrrggbb a
  colour).
- Flat faces: the index. Textured: the texel at ((v << 8 | u) & mask) +
  base in its bank, through COLOR and PLOT's rules, perspective-correct;
  on the GE a T8 texture with a palette lookup and power-of-two repeat.
- Lines, scaled sprites, particles, weather, text: GPU lines, textured
  quads and points.
- HUD pieces and panels: the game's own art as textures (the HUD
  programs' 2-bit bitmaps in bank $15, BG1's characters), placed as quads.
- The pause panel: baked textures and shaded shapes.
- Water and ground: a full-screen pass against the depth buffer.

## Contracts asked of nativeshvc (2026-11-04)

Sent to its session, "Console redesign: lazy, not an emulator": the canvas
as a GPU texture (device and queue lent per backend, a fence the console
waits on), the PSP's present by the GE from VRAM, and the overlay from a
GPU texture. It plans them as 2.18, after its core stages; shapes to be
approved by Jeff.

## Baselines (2026-11-04, SRFX 464c9dca, race100.sfx frames 1401-1500)

| Host, nativeshvc | Original: run + present = frame (ms) | Enhanced: run + present = frame (ms) |
|---|---|---|
| PSP, cc5549d | 23.218 + 17.796 = 41.253 | 125.890 + 501.508 = 627.681 |
| PSP, 2.17 (023f51a) | 24.234 + 17.712 = 42.187 | 136.301 + 501.515 = 638.118 |
| macOS null host, 2.17 | 0.330 + 1.488 | 1.999 + 2.722 |

PSP in PPSSPPHeadless with GPU_GE=1; macOS with GPU_METAL=1. Shots
byte-identical across the two nativeshvc versions. nativeshvc has the 2.17
slowdown's cause (its inline-access path) and is fixing it.

## Stages

Each stage verified and committed with numbers; every check shown able to
fail with a planted break. macOS Metal first, then Vulkan and the GE from
the same display list.

1. Geometry: the decoder and transform beside the existing redraw, each
   slot's points against the chip's ($0340 + 6i plus the object's place).
   Expected: 0 apart in the integer form.
2. Faces and order: the walk's faces and order against the draw list the
   frame program builds. Expected: 0 apart.
3. Materials: each face's colour or texture. Expected: 0 apart.
4. Pixels: the GPU's picture against the CPU reference, pixels apart and
   how far. Original shots and dumps unchanged.
5. The other passes, the menus' 3D and two players.
6. Vulkan, then the GE.
7. Original's bitmap on the GPU, read back at STOP; the job-duration model
   refitted.
8. The hand-off and the pause panel, once the contracts land; the CPU
   paths kept for checks only.
9. Cost, every host, against the baselines above.
