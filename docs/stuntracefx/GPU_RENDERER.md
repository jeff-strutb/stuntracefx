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
- A face is drawn only when its facing byte is negative (turned to the
  eye). The BEQ after the BMI tests the INC R14 in the BMI's delay slot,
  not the byte, so a byte of 0 is skipped too (stage 2 found it; the
  census's flag counts read the wrong byte of the face record and are
  withdrawn). Face record: count, a byte not yet known, the facing-flag
  index, the material, a normal of three bytes, the vertex indices.
- $5C runs both its lists, the flag's sign choosing their order: the
  painter's switch. $28 runs the list after it then the one in line, or the
  one in line with a target deferred.
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

## Contracts asked of nativeshvc (drafts, 2026-11-05, for Jeff's approval)

None of these is agreed or built: nativeshvc dab5da4 and its lazy-console tip
(5e33c21, contract 2.20) hold none of them. The game side of the hand-off and
turning the GPU on by default wait for them. Each is new, optional and
reported by a query, so a host without it goes on as now.

1. **The canvas as a GPU texture.** `nshvc_ext_gpu_device(NshvcGpuLoan *)`
   lends the host's device and queue: Metal `id<MTLDevice>` and
   `id<MTLCommandQueue>`; Vulkan the instance, physical device, device, queue
   and queue family; 0 on a host with none. `nshvc_ext_canvas_screen_gpu(layer,
   handle, w, h, format, priority, fence)` takes the game's finished picture in
   place of `nshvc_ext_canvas_screen32`'s texels: `handle` an `MTLTexture` or a
   `VkImage` with its view, `format` the same four encodings (0 transparent, 1
   CGRAM colour 1, `0x000000ii` an index, `0xFFrrggbb` a colour) as 32-bit
   unsigned texels, `fence` an `MTLSharedEvent` value or a `VkSemaphore` the
   console waits on before it samples. The console keeps the handle until the
   next one is handed or the frame hook ends. Why: read back to the CPU and
   copied up again is the last CPU step of a picture.
2. **The PSP's present by the GE from VRAM.** `nshvc_ext_canvas_screen_ge(layer,
   vram_address, stride, w, h, psm, priority)`: the picture in VRAM, in the loan
   below, drawn straight by the console's GE list; the game's list synced before
   the frame hook returns. Why: on the PSP the picture crosses to the CPU today,
   501 ms a picture in the baseline below.
3. **The overlay from a GPU texture.** `nshvc_ext_overlay_gpu(handle, w, h,
   fence)`, as the first's handle and fence, in the place of the overlay's CPU
   texels, for the pause panel; on the PSP a VRAM address and stride as the
   second's. Why: the panel is the last picture the game draws on the CPU.
4. **The GE's loan with a depth buffer.** `nshvc_ext_ge_vram` lends 557,056
   bytes today, the canvas alone. Asked: `nshvc_ext_ge_vram2(size_t *bytes,
   void **depth, int *depth_stride)`, the same colour memory plus a 16-bit depth
   buffer of the screen's size (at most 512 x 272: 278,528 bytes), its contents
   not kept. Why: the GE's water cut on faces, sprites and quads tests each
   pixel against the water's plane written first.

## The water cut

The reference cuts a face's pixel where the water's plane is nearer on its ray.
Two causes of Aqua Tunnel's errors were found by reading the water path, fixed
on Metal and Vulkan, and not yet measured: the shaders compared a far object's
halved depth (the reference's `depth_plane` undoes the halvings, so far objects
missed the cut), and they cut lines, which the reference never does. The GE
still ignores the cut on faces, sprites and particles until the loan above.

## GE clipping (the GE Users Manual, chapter 10)

An earlier note here said the GE has no near clip. With clipping enabled it
clips triangles at the near plane; what it drops whole is a primitive with a
corner past 0 to 4095 after the viewport, and such primitives must be divided
first. The CPU's clip is gone: faces of objects near the eye (`sc_near`, by the
object's place and radius, no point turned) go as flat Bezier patches the GE
divides itself. A piece with a corner past the range is still dropped, very
near the eye (Jeff's decision, 2026-11-05). Not yet checked on real hardware.

## The bitmap pictures (items 3 and 4): written, compiled on Vulkan and the GE, Metal unbuilt, none run

Jeff's decisions, 2026-11-05: two players, the menus' 3D and Enhanced without
widescreen keep their layouts; Original on the PSP is drawn by the GE and is not
byte-identical there; the job-duration refit is written with placeholder
coefficients and fitted in the final pass.

What was found by reading, and what is not the GPU's yet:

- The menus' bitmap: 01:AB13 is the only program that plots into it (a car's
  label within it); no menu or object code starts another plotter. Two players:
  AB13 and, on a step's picture, the phases under RaceSoundsPlayer1 and 2 (their
  glyphs, scaled pieces and banners, quads). The gauges, digits and map markers
  are sprites, not plots.
- rows.comp and fill.comp did not walk by the chip's slopes (`edge_x` is the
  full window's exact form): polygons with FwPoly pad[0] now do, in closed form.
- A bitmap picture keeps its sprites (no solids) and has no water cut: as the
  chip's pictures always had.
- Not the GPU's: with Frame rate off the tween's state does not exist, so those
  windowed pictures are still the chip's bitmap on the CPU (the full window is
  off there too); the race's pause shows the chip's bitmap from the copies; a
  step's own frame program still plots on the CPU, unseen, beside the pictures;
  in Original, 07:EE88's noise and any loop no quad stands for are 1-pixel spans
  the CPU makes, and the screen clear is the chip's own stores.
- A job at its STOP is taken as flushed: a program ending with pixels in the
  chip's caches and no RPIX would differ until its next flush.

One engine serves both: a picture in the bitmap's own coordinates times k, drawn
by the GPU to a linear 8-bit target, handed as the characters it stands for.

- **The target.** fullwindow.c's `tw`, `th`, `SXN`-`SYD` set to the bitmap's
  shape times k, the view words the game's own times k (its FW_CHECK and
  FW_KCHECK paths already draw in the bitmap's coordinates; `checking` writes
  `tex` on the CPU and must make spans instead).
- **Enhanced (k from `nshvc_ext_canvas_scale`).** `sc_frame_native` runs for
  every 01:AB13 in a picture, its `fw_view_active` widened from the one-player
  words to these. Layouts: 416 characters 8bpp (208 x 128, one player), 360 at
  4bpp (two players, `rm_canvas_show_players`), 143 at 8bpp (menus,
  `menu_flip`); all column-major (`gsu_pixel`). The linear target is cut into
  characters of 8k x 8k texels for `nshvc_ext_canvas_chars` in `picture.c`, in
  place of `rm_picture_chars`, until the hand-off. Two players walk twice, one
  target each, between `RmTweenDrawTwoPlayers`' two views. A step's own picture
  must be redrawn as `rm_race_frame` does for the full window, not taken from
  the chip's bitmap (`race_flip`, `split` stages in video/dma.c). To find by
  reading: which other programs plot into the menus' bitmap; each needs its
  quads as the HUD's have (`fw_bitmap`, or a new `ScPrim`).
- **Original (k 1).** The chip's C goes on making the draw list, exact. Every
  PLOT leaves the bitmap: FxDrawList's entries as `FwPoly` (rows.comp and
  fill.comp already walk them by the chip's slopes), the sprites' and blits'
  loops as quads (their hooks exist, gated off in Original today), lines, the
  screen clear. A new kernel packs the linear target into the chip's bitplanes;
  the bytes read back into its RAM at STOP. RPIX per the census above. No chip
  access added while off. The GE reads its target back the same way, not
  byte-identical.
- **Duration.** gsuprice.py's load model gains a form over what a job holds
  (polygons, area, edges, flat or textured, other primitives), its table's
  coefficients marked not fitted until the final pass collects the jobs.

FxHazeSample on the GPU, written, no play reaches it, shipped unproven: a marker
span (FW_HAZE) breaks the run, a kernel samples each of its 256 shape entries
(bank 8, $F92E + 4 x time) from a copy of the picture and plots them opaque; the
GE draws 1-pixel sprites from its canvas.

## Baselines (2026-11-04, SRFX 464c9dca, race100.sfx frames 1401-1500)

| Host, nativeshvc | Original: run + present = frame (ms) | Enhanced: run + present = frame (ms) |
|---|---|---|
| PSP, cc5549d | 23.218 + 17.796 = 41.253 | 125.890 + 501.508 = 627.681 |
| PSP, 2.17 (023f51a) | 24.234 + 17.712 = 42.187 | 136.301 + 501.515 = 638.118 |
| macOS null host, 2.17 | 0.330 + 1.488 | 1.999 + 2.722 |

PSP in PPSSPPHeadless with GPU_GE=1; macOS with GPU_METAL=1. Frames
1401-1500 of race100 are not all race: 1401-1430 are forced blank and the
full window's takeover, and 1431-1460 fade in (INIDISP $00 to $0F). Shots
byte-identical across the two nativeshvc versions. nativeshvc has the 2.17
slowdown's cause (its inline-access path) and is fixing it.

## Stages

Stages 1 to 3 below were each verified with numbers when made. The pixel counts
in stages 4 and 5 were measured before the native walk and are out of date;
the last measured figures (race100: Metal = Vulkan 345,208 apart, 17,348 more
than 3 pixels off; the GE 617,394, 37,113 off; Aqua Tunnel 9,131,853, 1,496,143
off) predate every change since, and nothing has been measured after them.

The frame program's pictures are walked natively (scene_frame.c, scene_walk.c
sc_walk_model): the sorted slots to the display list, facing by the determinant
over only the points each test needs, nothing projected or rejected by the CPU.
Sprites, particles and drops are ScPrim, projected by the GPU. Compiled on
Vulkan and the GE, written and not compiled on Metal, not run anywhere.

1. Geometry: the decoder and transform beside the existing redraw, each
   slot's points against the chip's ($0340 + 6i plus the object's place).
   Expected: 0 apart in the integer form.
2. Faces and order: the walk's faces and order against the draw list the
   frame program builds. Expected: 0 apart. Done (scene_walk.c,
   SC_CHECK=walk): over the nine dense scripts and race100 in Enhanced,
   29.1 million commands at the chip's own addresses and 17.9 million
   faces in its order, 0 apart; SC_BREAK shows both fail. Facing by the
   walk's own 3D determinant (no projection): the same as the chip's on
   81.5% of 19.0 million full-window tests; every one apart is the
   chip's projection failing it (points of no area at the original's
   pixels 89%, within a whole pixel's rounding of the projected points
   11%, held at the projection's limit or behind 23 tests), 0 otherwise. The renderer
   keeps the determinant; stage 4 measures the pixels it costs.
3. Materials: each face's colour or texture. Expected: 0 apart. Done
   (scene_walk.c, in SC_CHECK=walk): FxModelMaterial's rules natively
   (a shadow's colour 9, animated lists by the mask, flat, depth-shaded by
   the slot's tier, lit by the normal against the light turned into the
   object, textured: bank, colour word, base, mask, u, v list). Over the
   nine dense scripts and race100 in Enhanced, 14.0 million faces (510
   thousand textured) 0 apart, 1.82 million slots' lights and every
   slot's shade tier 0 apart; SC_BREAK shows it fails. Kind 2 met by none.
4. Pixels: the GPU's picture against the CPU reference, pixels apart and
   how far. Original shots and dumps unchanged.
5. The other passes, the menus' 3D and two players. In progress:
   - Correction (2026-11-04): until the fix below, the display list held
     only the model faces the chip clips at the screen's edges (the
     tag at l9237); the faces wholly on the screen are listed at 01:9199's
     other branch, which had no tag, so the CPU edge-walked them (race100:
     4,001 of 51,802 entries were the GPU's). The pixel counts for stage 4
     and the solids were measured with that share. Now tagged there too:
     every entry in the full window's pictures is a display-list face
     (FW_CENSUS: speed_expert_4wd 1,994,647, race100 51,802; 0 drawn by
     the CPU; 0 whose colour or bank is not its face's). Race100, all
     faces the GPU's: Metal and Vulkan 345,208 pixels apart from the CPU
     reference (273,081 a texel away, 17,348 farther), identical to each
     other; the GE 617,394 (474,791 a texel away, 37,113 farther), with
     faces near the eye or far off the sides clipped first (sc_needs_clip:
     the GE has no near clip and drops them whole). Aqua Tunnel: Metal and
     Vulkan 9,131,853 (1,496,143 farther), the cause not yet known. Earlier
     counts were too low: FW_GPUCHECK's polygon spans had lost their
     reference-only flag, so the CPU's polygons were drawn over the GPU's
     faces. Fixed.
   - Scaled sprites: one screen quad each (fullwindow.h FW_QUAD; the
     three paths of FxObjectSprite: 01:B1CB, 01:B2ED and the full window's
     own for texels not square), each pixel's texel by the chip's rule in
     integers on the GPU, the chip's row loops skipped. FW_QUADCHECK
     (each quad against the chip's loops, on the CPU): 0 texels apart over
     4,059 small, 1,989 large and 30,359 not-square quads; SC_BREAK 30,805
     apart. Metal and Vulkan: every shot identical to the chip's loops on
     the GPU's fill (541 shots, two plays; SC_BREAK: 42 differ). The GE:
     its interpolation (speed_expert_4wd: 16,607 more pixels apart over
     458 quads than the loops' rows).
   - The 2D on the GPU too, each a screen quad drawn by the GPU's own
     raster with the chip's rule per pixel: the HUD's glyphs and scaled
     sprites (07:F121, F15B, F6DF, FA88, FAF2 and every 2-bit blit: 2-bit
     bitmap quads, each kept with its piece and drawn with its join's
     shift, block-scaled by a column and row divide), the labels
     (FxTextNotice's glyphs), the panels' boxes (each box a quad from a
     texture of its pixels, made again only when the band's tiles change),
     the weather's and particles' dots and streaks (a line quad: the
     chip's Bresenham steps in closed form, 0 apart from its loop for
     every length to 300). FW_QUADCHECK: 0 texels apart over 94,846 quads
     at square texels and 106,805 not square; SC_BREAK 228,933. Metal and
     Vulkan: every shot identical to the CPU's rows on the GPU's fill.
     FW_CENSUS after: no PLOT reaches the CPU's raster in the full window.
   - The water's cut (fullwindow.c sunk) in the face shaders too (Metal,
     Vulkan), from the pixel's view depth.
   - FW_CENSUS (macOS): what the CPU still draws, by kind and routine.
     speed_expert_4wd: the HUD's programs 9.4 million plots (07:F121,
     F15B, F6DF glyphs, FA88 and FAF2 scaled sprites), labels 144,204
     (FxTextNotice), the frame program's other plots 24,052 (weather).
6. Vulkan, then the GE.
7. Original's bitmap on the GPU, read back at STOP; the job-duration model
   refitted.
8. The hand-off and the pause panel, once the contracts land; the CPU
   paths kept for checks only.
9. Cost, every host, against the baselines above.
