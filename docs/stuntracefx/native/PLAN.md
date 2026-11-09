# Stunt Race FX on the native engine: conversion plan

Status: **written for review. Nothing is built.** Branch `native-engine`, cut from `main` at c0c3b709. Engine: nativeshvc
6456fe0, API 4.4. All numbers come from reading the source; no game, gate or check was run for this plan. The only program
run was `tools/nshvc-find-hardware`.

How this meets the direction in one line: the game's own typed C calls `nshvc_*` directly from `update` and `draw`, with no
register-shaped state, translation layer or imitated hardware anywhere between them.

## Progress (updated as work lands)

Not done: most of the game. Since decision 4 (2026-11-09) the native game is the Enhanced game; stage 10 says what that means. Of about 120,000 lines to convert, about 5,100 lines of new code exist so far, plus the Super FX logic below brought over unchanged from the generator's output.

| Stage | State |
|---|---|
| 0 Set up | Done: `native/stuntracefx` builds and opens empty on the engine |
| 1 Record and replay | Picture done: `sfxbox --native`, core patch 0014, `native/checks/replay` and `compare.py`. On the attract play 5 of 5 recorded frames match bsnes, one of them a race frame. Not yet run: two-player, more than one play. Voice replay through the engine not written; the sequencer is checked directly instead |
| 3 Sound | Written; the routine map with the dead ones is `audio/ROUTINES.md`. Driver, with the original's pacing, on three recordings (attract, race, battle): all 11,897 key-ons and every voice's pitch and volumes at every pass match (`native/checks/music_check`). Loads: the recordings' sound memory byte for byte. Through the engine's mixer (`music_check/mix`): the original's own register writes correlate 0.99 to 1.0 with bsnes; the driver's 0.4 to 0.99, the rest being when within a pass each write lands. The game's own run of the attract play (title, demo, title, demo, 105 s) against its recording: the same sound throughout at a steady offset (the game reaches the title 1.8 s sooner, 3.4 s after the first demo, drifting 0.1% as the known difference says), loudness correlation 0.68 to 0.94 per 4 s window, waveform 0.06 to 0.88. Not done: a played race through the game (the menus do not reach one yet); sounds $09, $4D, $67 (noise) and the second course set are not in any recording |
| 2 Game state | Under way, a screen at a time: the menu objects (`menu/actors.c`), the frame's input, brightness and sound sides (`system/frame.c`, `audio/sound.c`) |
| 4 Boot to title | Logo and title run on the engine with no VRAM imitation, the objects and the per-line tables ported. Against bsnes: 6 logo frames and 19 title frames (fade in, the drop, the idle screen) exact in 5-bit colour. The original is 60 frames slower to reach the logo (sound upload) and about 127 frames to reach the title (picture unpacking on the Super FX while blanked); the native game does that work in one update. Not done: leaving the title (course menu), the attract demos, the rest of the menus |
| 5 Super FX logic | Under way. `src/fx` (41,700 lines) runs recorded jobs: `native/checks/fx_check` over five plays leaves RAM the same apart from register spills and each job's last store, which the recording's STOP mark catches unfinished. On the GPU through draw lists with the exact fill rule: polygons, lines, object sprites as textured quads, particles, HUD glyphs and pictures as image quads, scaled HUD sprites, weather as points. Measured by `native/checks/fx_view` on 1,100 frames of a race with its HUD against the original's bitmap: mean 989 of 26,624 pixels apart (3.7%) in the picture, the differences in the median frame being polygon edges (accepted, decision 3). Known differences: the static noise is rows from a square of the original's generator's numbers at seed-chosen places, not the original's exact pixels. Not done: the game's own frame does not yet draw through this (`fx_draw_begin`, `fx_draw_picture` in `fx/fx_draw.h`); mirrored sprites, the unreached glyph sizes and weather are written from the source and not measured |
| 7 Race (S-CPU side) | Interim. Every routine of `race/`, `hud/`, `math/`, `input/`, `system/` and `gsu/` (outside `fx_*.c`) is converted or listed in `race/NOTES` (folded, replaced by the engine by design, dead with the original's proof, or owed by another session). Pacing (stage 8 as rewritten): one step every 6 updates in every mode, the step's work told 6 frames (748ca732); `--pacing FILE` still replays a recording's step lengths, for finding bugs. Found against the recordings so far (now only for finding bugs): the attract demos' race state equal to `state.bin` at all 400 steps compared (view, backdrop scrolls, the player car's object and Super FX record), the demo text's bank fixed, four fields named twice fixed ($06CF, $0E37, $0F2B; $18D3 waits on the menus). Not done: a race from the menus stops in its first step until the objects' `camera_at_link` lands (the camera at work RAM $0000 while the car is not yet linked, as named state); Enhanced features of stage 10 |
| 6 Menus and saves | Interim: the title's exit, loader, selection, settings, initials and erase-save screens, the course preview flow, the championship screen, the records pages, the two-player screen's set-up and the save file with its checks are converted (`menu/`, `save/`). The course menu's car shows (the race's picture on BG3, BG2 masked under it) but does not match bsnes: no native frame comes within 4,000 of the window's 9,152 pixels; that is on the race's drawing side. Against bsnes outside the car window, 5-bit colour: title, exit and loader exact; erase-save (both questions, the erase, the cancel) exact on every shot; `menus_settings` 657 of 720 shots exact, 52 under 300 pixels; the slide from the game selection to the car selection runs 2-4 frames ahead for about 40 frames, cause not found. Not walked yet: the records pages and the championship (they need a race's flow aligned with a recording) |
| 8 to 10 | Not started apart from what the rows above say |
| 11 Platforms | Builds, as of the parallel sessions' work at 15:08: macOS (Metal, runs headless from the title into the attract race), Windows (Vulkan, cross-built with mingw; not run: no Windows machine reachable without permission), iOS (device build; not run yet: the simulator needs its own build), Android (arm64 APK; not run yet). Linux not built: no cross compiler here, and the Linux VM is another machine, which needs Jeff's permission |
| Race objects | Interim: compiled, never run (nothing can until the race flow runs). `native/stuntracefx/src/object`: 8,500 lines, every live routine of the 752 in `src/stuntracefx/object` converted or among the 244 dead listed with proof in `object/NOTES`; handlers found by number in `object/handler_table.c` (355 cases). Not done: the Enhanced calls (44 sites in `object/NOTES`, stage 10); 15 shared fields still named by address (`byte_19e6`, `byte_1a29`, `flag_0e59` and others, used by video, race and menus too); no frame compared with a recording |
| Race picture (video) | Interim. Not done: the two-player 3D draws in palette row 1 where the original's 3D tilemap gives row 2 (the fix is in `fx/fx_draw.c`, decision asked); the title after a demo shows the demo's last 3D frame until the menus' screen opens call `race_screen_end` (sent to the menus session); no one-player race from the menus compared; Enhanced (`rm_*`, `mw_*`, `fw_*`) left out. Done: every routine of `src/stuntracefx/video` (without `unused_screen.c`) has a function or is listed in `video/NOTES` (folded, replaced by the engine, converted by the code that drives it, or dead with the cartridge's call counts). The battle run in the game from the menus (`battle.sfx` as replay input, the race inputs 61 frames earlier because the course screen hands over 61 frames sooner, recorded pacing): at the race start the two-player screen outside the 3D views is 0 pixels apart (frame, panels, HUD, the blank lines, the mid-screen switch); with the race session's two state duplicates and the colour row patched in a scratch copy, 5,255 of 57,344 pixels apart, all inside the 3D views (the cars' and road-side textures' colour row, polygon edges); both cars' objects equal the recording for 123 steps, the first difference at frame 2676 (race side). |

Known differences so far, for the final report: sound $23's noise sample is rewritten while it plays in the original; here it is one long noise made once, at the same rates, because samples do not change on the audio thread. The driver runs one pass per timer period instead of the original's variable passes, so it never loses the music tick a pass of 8 or more periods loses at this tempo; the title song is about 0.1% quicker than on the console (some 2,400 samples after a minute).

## How the conversion is shared between sessions

Several sessions convert different areas at once, all in this worktree and on this branch. The rules every one of them follows:

**The goal and its bar.** Section 8 is the definition of done. The game's C calls `nshvc_*` directly. No bridge, no imitated PPU, DMA, DSP, APU, bus or clock, and no `cpu.`, `abi_`, carried registers, flag helpers, computed goto or SNES addresses in code. Real parameters and return values. `tools/nshvc-find-hardware` must end at zero and that alone is not enough. Dead routines (section 1 item 7) are not converted; list them.

**The source.** Read the C in `src/stuntracefx` (the proven record, never edited) and `asm/stuntracefx` where the C is unclear. Write new C in `native/stuntracefx/src`. Do not copy register-style code across and tidy it later: write what it means. `native/stuntracefx/src/menu/actors.c`, `menu/screens.c` and `audio/driver_music.c` are examples of the form.

**The frame.** Each `update` is one video frame of the original. A blocking loop becomes an explicit state machine; a wait becomes returning from `update`. Hardware writes queued for the vertical blank become changes to the fields of `pic` (`video/picture.h`) or object writes (`nshvc_tiles_write`, `nshvc_map_write_snes`). HDMA tables become `nshvc_lines` calls.

**The state.** Shared variables live in `src/system/state.h` as fields of `g`, under the section of the code that mostly writes them, each with its original address in a comment. Search that file for the address before adding a field. Use the Edit tool with a small exact match, never rewrite the whole file, because others are editing it too. Variables only one file uses stay `static` in that file. Super FX RAM is `fx_ram`, named through the `FX...` constants in `src/fx/fx.h`.

**Cross-area calls.** A converted routine keeps its original name (`ObjectsRunHandlers`) with real parameters, declared in its area's header (`object/object.h`, `race/race.h`, `hud/hud.h`, `video/video.h`, `menu/menu.h`, `save/save.h`, `audio/sound.h`). If you need a routine from another area and its prototype is not there yet, add the prototype you need under a `/* wanted */` comment in that header; its owner implements it or corrects it. An unconverted routine is never replaced by a silent stub. If you must leave one, make it print its name once to stderr and list it in your report.

**Your files.** Each session owns the directories named in its brief. Do not edit another session's files apart from the two cases above. Save a file only when it compiles: other sessions build the same tree. Commit often with `git commit -- <your paths>`, never `git add -A` or a bare commit, because the index is shared. Never push. Commit messages and comments describe the work and nothing else.

**Building.** Use absolute paths, the shell's working folder does not persist. Check your own files with `clang -std=gnu11 -Wall -I native/stuntracefx/src -I <nativeshvc>/include -c FILE -o /dev/null`, where `<nativeshvc>` is `/Users/jeffreywilbur/projects/strutb/nativeshvc`. For a full build use your own build folder so that sessions do not collide: `cmake -S native/stuntracefx -B <your scratch folder>/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DNSHVC_ROOT=<nativeshvc> -DNSHVC_HOST=macos -DNSHVC_RENDER=metal && cmake --build <your scratch folder>/build`.

**Checks.** `build/stuntracefx/native` holds `sfxbox` and the core. `native/checks/record.sh PLAY OUT` records a play of the original (plays in `tools/stuntracefx/plays`); `sfxbox --script FILE` with your own `wait N` / `shot NAME` script records any frames you want. `native/checks/compare.py BSNES_DIR SHOTS_DIR OFFSET` compares pictures exactly. The game takes `--headless --updates N --shots DIR --input FILE`. Do not change `nativeshvc`; if the engine seems wrong, get the evidence and tell the session that started you.

**Reporting.** Lead with what is not done. Never call a stepping stone finished. Update your row of the Progress table when something lands.

## 1. What this plan cannot promise

Read these first.

1. **This is a rewrite of about 120,000 live lines, plus 12,447 of the game's own features, not an engine hookup.** Of the finder's
   32,613 lines, about 1,900 touch real S-CPU-side hardware. The rest are register-style code or data at SNES addresses: `cpu.`
   11,715, `rd8`/`wr8` on data 8,033, `abi_` 5,997, `op_` 3,476, the SPC700's 1,640. zelda3 was already ordinary C; this game is not.
2. **The finder sees only part of it.** It does not look for the Super FX's register state (`R[`, `gsu.`: 21,137 uses), the
   SPC700's (`spc.`: 6,691) or `goto` over translated assembly (3,573). A finder result of 0 is required and not sufficient.
   Section 8 defines done.
3. **Polygon edges will not all match bsnes.** `NSHVC_FILL_EXACT` rounds vertices to whole pixels and lets the GPU decide
   coverage, so every GPU fills the same spans (`src/engine/picture.c:507`). The chip walks each edge with an 8.8 slope from a
   reciprocal table in ROM, with its own rounding and clamping (`render/draw_list.c:131`). Those are different rules. Expected
   result: flat areas and interiors identical, edges off by up to a pixel on some rows, most on long shallow edges. The number is
   not known until it is measured. Generating the chip's spans on the CPU and drawing them row by row would match exactly, but
   it is the CPU deciding pixels, so this plan does not do it. Matching byte for byte would need a general edge rule in the
   engine, which is your call (decision 3).
4. **Races will not line up with bsnes in free play.** The original's step took 4 to 9 video frames depending on the work. With
   the game's own pacing the steps differ, so the race's frames are comparable only under the check's recorded pacing
   (section 6).
5. **Sound will be compared by voice events first, and sample for sample only where it aligns.** The music code runs on its own
   thread in the engine's time; zelda3's music never aligned with its ROM.
6. **It cannot be seen working until most of it exists.** Stage 4 is the first end-to-end run (boot, title, sound). A race needs
   stages 2 to 7.
7. **Dead code is not converted.** Routines the census proves never run in retail stay in the record and are listed, not
   rewritten: `render/unused.c`, `video/unused_screen.c`, `object/fx_unused.c`, the debug panels and the like (about 4,700 lines
   in those files). `FxHazeSample` (`render/effects.c:197`) reads real pixels with RPIX; the census found no play reaching it.
   Stage 5 settles from the assembly whether it can run. If it can, I stop and tell you, because a GPU picture cannot be read back
   for game logic.
8. **The Builder app is not converted by this plan.** `builder/` packages the old front end and stops working on this branch once
   the front end is deleted. It needs its own decision after the game builds.

## 2. What is there

`src/stuntracefx`: 143,219 lines, 149 C files, 2,402 tagged routines.

| Area | Lines | State today |
|---|---:|---|
| `object` | 36,847 | S-CPU register style, plus Super FX physics translated with `goto` |
| `audio` | 18,936 | two SPC700 drivers, 105 and 70 routines, translated instruction by instruction, 17,240 lines |
| `render`, `gsu` | 21,713 | Super FX programs in register style; a typed draw-list fill exists |
| `race` | 13,721 | S-CPU register style |
| `remaster` | 12,447 | newer C on the old console's extensions, including a CPU span generator (`fullwindow.c`, 6,988) |
| `video`, `system`, `hud`, `input` | 20,140 | registers, DMA, HDMA, interrupt stages |
| `menu`, `save` | 13,358 | register style; loops that wait |
| `math`, `data`, `include` | 6,057 | helpers, the hardware headers |

Blocking: 276 functions wait for a vertical blank, directly or through a callee (106 menu, 89 race, 36 system, 17 video, 11 audio,
7 object, 7 hud, 3 chip). Every one becomes a state of its screen. 124 further sites use `call_routine`, `jump_routine`,
restarts or pushed frames.

The shelved branches hold usable work, and this plan starts from them where it can (decision 1):

| Branch | Holds | Use |
|---|---|---|
| `native-port` (d82f1cd6) | Work RAM named from the assembly: `wram_struct.h`, 1,438 fields; record field names; `tools/wramname.py`; some truly typed functions | Field names. Its 2,734 "carried register" functions are not acceptable as a finished form and are not reused as such |
| `fx3-merge`, `fx3-phys` | Hand-typed Super FX: `fx_unused`, unpack, HUD, effects, frame, `fx_sums` | Reading and reuse, re-proved here |
| `fxgen-w` | Generated Super FX C: 0 gotos, 0 register references, data-flow names | Approved as the starting text for the Super FX logic (decision 1) |

The full hardware list is in [`hardware-touches.md`](hardware-touches.md), with every site in `hardware-touches-sites.txt`.
The real hardware touches:

| Hardware | Sites | Replaced by |
|---|---:|---|
| DMA channels and start | 388 | object writes when the data changes |
| HDMA | 76 | `nshvc_lines` in `draw` |
| VRAM, palette and sprite ports, sprite sizes | 325 | `nshvc_tiles_write`, `nshvc_map_write_snes`, `nshvc_palette_write`, `nshvc_sprites_set` |
| Interrupts and waits | 127 | work in `update` and `draw` |
| Layers, windows, colour maths, mode, brightness | 558 | `nshvc_layer_set`, `nshvc_windows_set`, `nshvc_blend_set`, `nshvc_mode_set`, `nshvc_screen_set`, per line `nshvc_lines` |
| Calls by address, the console connector | 115 | direct C calls; `nshvc_game` for the entry |
| Super FX registers | 92 | a C call of the program's function |
| Sound ports | 65 | `nshvc_music_send` |
| Joypads | 56 | `nshvc_input` |
| Multiplier, divider, M7A/M7B as a multiplier | 99 | ordinary C. The game shows no Mode 7 picture: `BGMODE` is only ever 1 or 3 |
| Beam position, work RAM port | 13 | the beam reads are debug time marks, not converted; the port becomes plain copies |

## 3. The converted game

The new tree is `native/stuntracefx`, with the same area names as the record. `src/stuntracefx` and `asm/stuntracefx` are not
touched on any branch, so the record survives a merge. (`docs/stuntracefx/PLAN.md` rule 20, "no separate copy", is superseded by
your brief for this work only.) Each function keeps its `@routine` tag and its `WHAT IT DOES`.

**Loop.** `nshvc_game` with `update_hz` 60. `update` does what one vertical blank's worth of the original did: the interrupt
stages' work (frame counters, pads, sound sends, fades, windows, palettes), then the current screen's next state. `draw` sets the
whole picture from the game's state. There are no threads, fibres or coroutines imitating a wait: the 276 waiting functions become
explicit states.

**State.** One typed `struct` for work RAM, with records as records (objects, actors, cameras). Stack and direct-page scratch
become locals. The Super FX's RAM splits into shared fields, which go in the game state, and the chip's scratch, which becomes
locals and arguments. No field is at an SNES address and none holds a register's byte.

**Picture.** Screen setup says which picture kind this is; each kind holds its own tile sets and maps as engine objects. `draw`
describes the layers from named game fields. This is deliberately not a register shadow: there is no `bg12nba`, and no function
that takes such state and works out calls. DMA queues become object writes where they are queued, the one numeric part being data
tables that name destinations, which one function maps to objects (as zelda3's did). Sprite builders write `nshvc_sprite` entries
rather than an OAM image. HDMA tables become per-line arrays passed to `nshvc_lines`.

The split screen changes BG2's map and tile base at a line (`system/raster.c`, `split_bg2`), and `nshvc_line_field` has no such
field. The conversion puts both halves in one map and one tile set and uses a per-line scroll, which the API allows. No engine
change is expected; stage 1's replay confirms it.

**Super FX logic.** The chip's compute programs (physics, matrices, sorting, unpacking) become ordinary functions run where the
S-CPU started them. Its drawing feeds `nshvc_draw_list`s:

| Drawn by the chip | Becomes |
|---|---|
| Flat polygons, 3 to 8 vertices | `nshvc_draw_tris` under `NSHVC_FILL_EXACT`, fanned, palette index as colour |
| Lines | `nshvc_draw_lines` |
| Textured polygons | triangles on an indexed `nshvc_image` from the ROM's texels |
| Scaled sprites, HUD art, glyphs | `nshvc_draw_image` from the game's art as images |
| Particles, weather, single plots | one-pixel quads |
| Dither (the chip's plot option) | a small shader in the engine's language, the game's own |

The chip's bitmap, its tile layout and the staged uploads of it do not exist in the new game. No pixel is made on the CPU; a
picture becomes visible at the update on which the original's last upload stage would have completed.

**Sound.** `nshvc_music.tick` at 32,040 Hz counts samples and steps the driver at its timer's rate, the method zelda3 uses. The
driver is structured C written from the record: sequencer, voice state, vibrato, tremolo, slides, sound effects. It calls
`nshvc_voice_*`, `nshvc_echo` and `nshvc_sound_*`. It never waits, locks, allocates or does file work. The game thread loads a
bank's samples with `nshvc_sample_load`, then sends a message saying it is ready; commands go by `nshvc_music_send`. Both drivers
convert; they share code only where reading shows the behaviour is the same.

**Data.** A tool reads the player's own ROM at build time, verified by checksum, and writes the game's data (not its code) to one
asset, read through `nshvc_asset_*`. Correction to the first version of this plan, which promised no address lookup would remain: the
game's tables, models and scripts refer to each other by bank and address in their own format, so that data stays in that format,
read through one accessor (`data/cart.h`). Named tables replace addresses in the game's C where the code names them. `ports/stuntracefx/assets.csv`
(2,052 ranges) is the starting inventory.

**Saves and input.** `save/records.c`'s data as the game's own struct through `nshvc_save_*`, written where the SRAM was written.
`nshvc_input` replaces the joypad code. The keyboard, touch and controller code in `ports/stuntracefx/front` goes, since the engine
provides it. No per-platform code in the game.

**Pacing.** See stage 8. The race sim already takes "video frames since the last step" as an input (`$0306` into `$06D4`,
`race/race_frame.c:54`), so its pace is that one number.

## 4. Stages

Each stage deletes what it replaces from the new tree in the same change. Compiling is allowed throughout. Every run is listed in
section 7 and asked for first.

### Stage 0. Set up
New tree, `CMakeLists.txt` with `nshvc_game()`, an empty game that opens. **Delete:** nothing. **Check:** compiles.

### Stage 1. Record and replay (section 6)
The tooling section 10 of the engine plan requires, before any conversion, because it tests the API's fit while a misfit is still
cheap. **Check:** the replay matches bsnes on the frames in section 6, and is shown able to fail by planting a change.

### Stage 2. Data and state
Asset extraction; typed tables; the game-state struct with records; the math helpers as plain functions (`math`, 2,595 lines).
**Delete:** `machine.h`, `cpu_ops.h`, `wram.h`, `hw.h` are never brought over. **Check:** every table's extraction against the ROM
bytes it came from; compile.

### Stage 3. Sound (18,936 lines)
Both drivers as music code; the sample and bank loaders; the game's sound queue. It goes early because it is self-contained and can
be proved without the rest of the game. **Delete:** `spc.h`, `spc_ops.h`, `apu_upload.c`, every `spc_` access. **Check:** the
recorded port commands fed to the driver give the recorded voice events, tick by tick, on the race and battle drivers. Expected: the
same events and values, with differences only in the order of writes inside one driver step.

### Stage 4. Loop, video, boot to title (about 20,000 lines; first end-to-end)
`update`, `draw`, interrupt stages as ordered work, HDMA to `nshvc_lines`, DMA to object writes, boot to title and attract's start.
**Delete:** `waits.h`, `console_ext.h`, NMI and IRQ handlers, every DMA and PPU access. **Check:** `nshvc-check` picture and sound on
the boot and title frames. Expected: byte identical on frames with no chip picture.

### Stage 5. Super FX logic (about 30,000 lines; the highest risk)
`gsu/`, `render/`, `object/fx_*`, `gsu_record`: compute programs to functions, drawing to draw lists. The 3D frame program, physics
and `render/model.c` are the parts that stalled last time. **Delete:** `gsu.h`, `gsu_ops.h`, the fill in `draw_list.c`,
`ports/common/snes/superfx.c`'s role. **Check:** each compute program against the recording of its job (inputs at start, outputs at
STOP), and each frame's polygons against the recorded draw list. Expected: compute results identical; polygons identical in vertices,
colours and order.

### Stage 6. Menus and saves (13,358 lines)
Menus, course preview, records, the 3D car display, saves. **Check:** `nshvc-check` on the menu plays. Expected: frames with no
polygons byte identical; 3D frames differ on polygon edges only (section 1, item 3).

### Stage 7. Race and objects (about 40,000 lines; the largest)
`race/`, the rest of `object/`, HUD logic. **Check:** the game state compared with the recording at each race step under the recorded
pacing (section 6); then `nshvc-check` on race plays. Expected: state identical at every step; pictures differ on polygon edges only.

### Stage 8. Pacing (decision 4)
The game is processor-speed independent. The race runs one whole step of the original's code every 6 updates at 60 updates a
second, with the step's frame count given as 6 (the approved whole-step design of 2026-10-28), so the race's speed in real time
is fixed and there is no slowdown. Every picture between two steps is drawn from the state between them (`draw(between)` and
the updates in between), so the 3D moves at the display's rate. The recordings' medians (6 one player, 7 two players, 5 demos)
are no longer a target. **Check:** steady 60 updates a second in every mode; distance per real second the same in every run.

### Stage 9. Input, options and platform glue
**Delete:** `ports/stuntracefx/front`, `ports/stuntracefx/runtime`, `ports/common`.

### Stage 10. The Enhanced game (decisions 2 and 4)
This is the game now, not an option beside an original mode. Matching the original's pictures and pace is no longer the bar; the
game's own logic (its physics, objects, menus, rules) stays the game's, and recordings are kept only to find bugs.
- The race fills the window at any size and shape, the view's width following the window's aspect; the HUD and panels float at
  the screen's edges and corners; the sky and hills reach any width; view distance unlimited.
- High-resolution 3D: the frame's shapes projected at the output's resolution (the view's words scaled, sub-pixel vertices) and
  filled by the GPU with the smooth rule. The exact rule and the 256 x 224 Super FX screen are not kept.
- Frame rate: whole steps every 6 updates, the 3D drawn between them from interpolated cameras and objects, and pictures between
  updates on displays faster than 60 Hz.
- Menus in the whole window.
- Solid shapes for the sprites that show solid things: the cars' tyres, the 4WD's eyes, the bonus trucks' wheels, the trees
  (faceted, as approved); effects stay sprites. No filtered or upscaled art.
- The pause screen with the controller panel and key remapping on every host, the options (auto-accelerate), and typing the
  initials on a keyboard.
`fullwindow.c`'s CPU span generator is not brought over; its decisions (view words, anchors, sky) are the reference.

### Stage 11. Whole game
Finder zero on `native/stuntracefx`; section 8's criteria; every play through `nshvc-check`; every reachable platform.

## 5. What gets deleted

From the new tree, by never being brought over: `machine.h`, `cpu_ops.h`, `hw.h`, `wram.h`, `gsu.h`, `gsu_ops.h`, `spc.h`, `spc_ops.h`,
`waits.h`, `console_ext.h`, `nshvc_handoff.h`, every `abi_` adapter, every `cpu.`, `gsu.`, `spc.` reference, the flag helpers, the
interrupt handlers, the chip connector.

From the branch, at the end: `ports/stuntracefx` (its icon moved over), `ports/common`.

Kept: `src/stuntracefx`, `asm/stuntracefx`, `config/stuntracefx`, `tools/stuntracefx` including `sfxbox`, as the record and the
reference.

## 6. Recording and comparing

Built in stage 1, with this tooling and nothing more.

**What is recorded from the ROM in bsnes** (`sfxbox`, on the input-only plays in `tools/stuntracefx/plays`, using its existing
`--pacing`, `--apulog`, `--voices` and `--sound`, extended where needed):

- Picture state: for each frame, per-line effective PPU state, the palette, the sprite table and video memory changes, and the bsnes
  picture.
- Voice events: every DSP write with its sample time, and the port commands.
- Polygons: at each Super FX STOP, the draw list (`$08AA`), the other drawn primitives, and the inputs and outputs of each job.
- Logic: the work RAM and chip RAM at each logical step, and `$0306` for the pacing.

**Replay** (`native/checks`, a small program on the engine, not part of the game): takes one recording and makes the API calls a
converted game would make. Its picture compared with the bsnes picture tests the mapping and the engine with no game logic involved;
its voice output compared with the bsnes samples does the same for sound.

**State comparison.** A CSV made from the game's struct gives each field's offset and its original address. A check-only tool
compares the native game's state with the recording at each logical step, field by field. Addresses live in that CSV only; the
game's C has none.

**Recorded pacing.** Race frames can line up only if the steps do, so a check-only option given to the game's `main` reads each
step's frame count from the recording in place of N. The option is one branch in the pacing function and nothing else.

**Which frames line up.** `nshvc-check` compares what is alignable, and the final report says, per play: frames compared, frames
identical, frames with a difference and the cause of each kind of difference. Expected kinds: polygon edges; frames during the
original's staged upload, where the ROM shows part of a picture being replaced; sound offset from thread timing.

Anything the replay shows is treated by the boundary rule: establish from evidence whether the conversion or the engine is wrong.
If it is the engine, I stop and tell you before changing nativeshvc.

## 7. Runs

Under the standing rule each run below needs your typed approval, and I will state its measured length before it. Compile-only
builds are the only thing done without asking.

| Run | Stage | Length (estimate) |
|---|---|---|
| Record all input-only plays in bsnes | 1 | about 10 min |
| Replay against bsnes, then a planted break to show it fails | 1 | about 5 min |
| Driver voice-event comparison, both drivers | 3 | about 2 min |
| `nshvc-check` boot and title | 4 | about 2 min |
| Chip job and polygon comparison | 5 | about 5 min |
| `nshvc-check` menu plays | 6 | about 5 min |
| Race state comparison; `nshvc-check` race plays | 7 | about 10 min |
| Pacing record, and the speed comparison | 8 | about 10 min |
| Every play on macOS (Metal, Vulkan), iOS Simulator, Android emulator, Windows under Wine | 11 | about 30 min each |

A Linux VM counts as another machine and needs its own permission.

## 8. Done means

All of these, for `native/stuntracefx`:

1. `tools/nshvc-find-hardware` reports nothing.
2. No `R[`, `gsu.`, `spc.`, flag helpers, register-named carried values, computed `goto`, or SNES addresses in the game's C, under any
   name. Renaming a register file does not satisfy this; a function passing an `a`, `x`, `y` or status value along as its working state
   is a failure, however it is spelt.
3. Every function has real parameters and returns, and structured control flow.
4. No platform code; builds through `nshvc_game()` and `tools/nshvc-build`.
5. Every live routine in the record has a converted form, or an entry in the list of dead routines with its proof.
6. Each stage's check has passed, and the final report states which frames match and which do not, and why.

Interim forms will be named as interim, with their counts, in the same sentence each time.

## 9. Decisions

Settled by the maintainer, 2026-11-09:

| # | Question | Answer |
|---|---|---|
| 1 | May the physics and `model.c` start from the generated `fxgen-w` output, then be renamed and restructured by hand? | Yes |
| 2 | Are Frame rate 60, the wide picture, solid tyres, the pause screen and the other remaster features part of this conversion? | Yes, all of them, as stage 10 |
| 3 | Polygon edges: accept the engine's exact rule, with edges measured against bsnes? | Yes |
| 4 | Race pacing: a fixed step length from the recordings' median makes races 0.8-1.8% slow; which way? (asked 2026-11-09) | Jeff: "We can abandon matching the original and go all in on the enhanced. This isn't an emulation effort at all anymore." |

## 10. What is not known

- Whether the voice calls cover both drivers. Stage 1 and stage 3 will show.
- Whether a draw-list layer's priority and per-line scroll behave as the race screen needs. Stage 1 will show.
- How the edge difference looks in numbers.
- The state comparison's masks: which work-RAM bytes are scratch the new game no longer has. Each masked byte needs a reason.
- Whether every use of a computed address (1,821 lines) resolves to a typed table or record by reading. If any does not, it is reported.
