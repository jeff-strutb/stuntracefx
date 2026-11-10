# Stunt Race FX on the native engine: conversion plan

Status: **under way, not done** (Progress below says what is not). Branch `native-engine`, cut from `main` at c0c3b709.
Engine: nativeshvc API 4.5. Sections 1 to 10 were written from reading the source before anything was built; the Progress
table's numbers come from the runs since.

How this meets the direction in one line: the game's own typed C calls `nshvc_*` directly from `update` and `draw`, with no
register-shaped state, translation layer or imitated hardware anywhere between them.

> **Read this first (Jeff, 2026-11-09, decision 4 and its repeat).** "Enhanced IS the native game. We are abandoning
> 'original' mode entirely. This isn't an emulation project." So, everywhere in this plan and in the code:
> - There is no original mode, and no option, flag or check-only path that runs the game the original's way: no recorded
>   pacing (`--pacing`), no exact fill rule, no picture held to the original's 256 x 224 screen, no work spent lining frames up
>   with bsnes or matching its timing.
> - The game's own logic (physics, objects, menus, rules, sound commands) is still the game's: a crash, a wrong value, an object
>   in the wrong place or a missing sound is a bug. Recordings of the original are kept only to find such bugs, never as a target.
> - Where the rest of this plan speaks of matching the original, lining frames up, edges against bsnes, `nshvc-check` frame
>   reports or recorded pacing, that part is superseded by this.

## Progress (updated as work lands)

Not done (2026-11-09, evening): stage 10's ground and water (designed, being built), perspective textures, the hills, weather and particles between steps; the objects' Enhanced calls (44 sites); the championship's closing screen and the two-player menu paths, not walked; Windows and Linux not run (other machines, Jeff's permission); the picture, sound and touch on iOS and Android not looked at (3-minute runs from logs only); the final section 8 audit and stage 11. The game plays headless from boot through the menus, races and battles to the results and back, as the Enhanced game (decisions 4 and 5): full window at any shape, high-resolution 3D drawn between steps, solids, unlimited view distance, the game's own pause screen. New C outside `src/fx`: about 39,900 lines; `src/fx`: 44,300 (the Super FX logic from the generator and its drawing on the GPU).

| Stage | State |
|---|---|
| 0 Set up | Done: `native/stuntracefx` builds and opens empty on the engine |
| 1 Record and replay | Picture done: `sfxbox --native`, core patch 0014, `native/checks/replay` and `compare.py`. On the attract play 5 of 5 recorded frames match bsnes, one of them a race frame. Not yet run: two-player, more than one play. Voice replay through the engine not written; the sequencer is checked directly instead |
| 3 Sound | Written; the routine map with the dead ones is `audio/ROUTINES.md`. Driver, with the original's pacing, on three recordings (attract, race, battle): all 11,897 key-ons and every voice's pitch and volumes at every pass match (`native/checks/music_check`). Loads: the recordings' sound memory byte for byte. Through the engine's mixer (`music_check/mix`): the original's own register writes correlate 0.99 to 1.0 with bsnes; the driver's 0.4 to 0.99, the rest being when within a pass each write lands. The game's own run of the attract play (title, demo, title, demo, 105 s) against its recording: the same sound throughout at a steady offset (the game reaches the title 1.8 s sooner, 3.4 s after the first demo, drifting 0.1% as the known difference says), loudness correlation 0.68 to 0.94 per 4 s window, waveform 0.06 to 0.88. Not done: sounds $09, $4D, $67 (noise) and the second course set are not in any recording. Race at the steady step (decision 4), the menus' race walk, 11,500 updates: every port-0 command the game sends reaches the driver in order (227 of 227, 2.2 frames typical, 10.5 at most, with the sends now every frame instead of the original's odd frames); the countdown ($49 three times 54 frames apart, $4A, $01) and the finish ($31, $51) come out as in race1. Fixed on the way: the engine bytes now come from the race's own fields, and a command sent twice running is no longer heard once (the original's echo could not tell a fresh 0 from a stale one; 2 of 227 merged). The time bonus's command queues as any other (the original's decimal-mode slot is gone). The engine follows the speed: with the race's gauge fix (574371ef) the speed byte runs 0-252 over a race and the engine pitch follows it (correlation 0.89, the rest the driver's own smoothing). Three port-1 commands dropped in that race are the driver's own rule (19:CE82: $09 and $0D ignored while channel 5 plays $17 or $21). |
| 2 Game state | Under way, a screen at a time: the menu objects (`menu/actors.c`), the frame's input, brightness and sound sides (`system/frame.c`, `audio/sound.c`) |
| 4 Boot to title | Logo and title run on the engine with no VRAM imitation, the objects and the per-line tables ported. Against bsnes: 6 logo frames and 19 title frames (fade in, the drop, the idle screen) exact in 5-bit colour. The original is 60 frames slower to reach the logo (sound upload) and about 127 frames to reach the title (picture unpacking on the Super FX while blanked); the native game does that work in one update. Leaving the title, the attract demos and the menus since done (rows 6 and 7) |
| 5 Super FX logic | Under way. `src/fx` (41,700 lines) runs recorded jobs: `native/checks/fx_check` over five plays leaves RAM the same apart from register spills and each job's last store, which the recording's STOP mark catches unfinished. On the GPU through draw lists with the exact fill rule: polygons, lines, object sprites as textured quads, particles, HUD glyphs and pictures as image quads, scaled HUD sprites, weather as points. Measured by `native/checks/fx_view` on 1,100 frames of a race with its HUD against the original's bitmap: mean 989 of 26,624 pixels apart (3.7%) in the picture, the differences in the median frame being polygon edges (accepted, decision 3). Known differences: the static noise is rows from a square of the original's generator's numbers at seed-chosen places, not the original's exact pixels. The game's own frame draws through this since (row "Race 3D"; the exact rule since removed, decision 5). Not done: mirrored sprites, the unreached glyph sizes and weather are written from the source and not measured |
| 7 Race (S-CPU side) | Interim. Every routine of `race/`, `hud/`, `math/`, `input/`, `system/` and `gsu/` (outside `fx_*.c`) is converted or listed in `race/NOTES` (folded, replaced by the engine by design, dead with the original's proof, or owed by another session). Pacing (stage 8, decisions 4 and 5): a step every 6 updates in every mode, its work told 6 frames; no recorded pacing (17e97ac4). Runs headless with no crash or warning: the attract demos (6,400 updates) and a race from the menus to game over, the results and back to the menus (11,500 updates). Bugs found with the recordings and fixed: the demo text's bank, the first step's camera at work RAM $0000 (with the objects' `camera_at_link`), fields named twice ($06CF, $0E37, $0F2B; $18D3 waits on the menus), the race's layers left on behind the menus (now handed back through `race_screen_end`). The battle traced (notes: race-battle-trace): its parting at frame 2676 is when the original reads the buttons within its step (not a logic bug); one logic bug found on the way, a car part's break-off handler given to the part instead of the new piece (objects, a81bdbbd). With the original's button reads as input and its seed (through the menu exit's timing, input only), a scratch run of the battle with the recorded pacing had the random numbers, both cars, their records and the whole object list equal to the recording at all 898 two-player steps. The speed gauges read the car records again (574371ef; the port number had been passed). Stage 10's race side (574371ef): the steps' clock, `race_step_progress`, `RaceViewForPicture`; the fx session's `fx_tween_step_end` and `fx_tween_reset` in `step_end` and the clock's restarts (5e877dfc); the full window's view words (35718c30). Checked at HEAD 362244a6 against a copy with the pictures between steps off: the menus' race walk (11,500 updates) and the attract demos (6,400) give byte-identical sound and run with no crash or message, and with them on the race's picture changes every update (a step holds it six). Touch auto-accelerate (e8920e96) not run on a touch device. Section 8 in these directories: the hardware finder reports nothing; no carry passed (the decimal sums as values, 889fc601, 9bb77f35, each byte-identical in sound against its parent on the race walk and the demos); 59 routines and the fields named by what they do, the dead fields gone with the original's proof of no reader; no cartridge table or chip RAM by number. The pause screen's AUTO-ACCELERATE switch (b51eee0c, compiled, not run). Not done: rebinding a controller's buttons, which the engine (4.5) does not offer |
| 6 Menus and saves | Interim: the title's exit, loader, selection, settings, initials and erase-save screens, the course preview flow, the championship screen, the records pages, the two-player screen's set-up and the save file with its checks are converted (`menu/`, `save/`). Walked since by input only: the two-player (Battle Trax) menus against bsnes, matching but for timing (the slide, the car camera's swing, the granite's and highlight's animation phase); the championship's closing screen, which a game over in a championship reaches (its boxes' course names fixed, f5536bbb; one line at the boxes' bottom edge still differs, cause not found). Not reached: the closing screen of a won championship (it needs four races won by input). Found with the recordings and fixed: the fade's fixed colour (the original's queue keeps the first one queued in a frame), the sprite art rows queued to the frame's end as the original's VRAM queue (after a race the fourth choice read FREE TRAX), the records pages' sprite slots in the shared words $0D71-$0D7B (the records panel's colours). Against bsnes, 5-bit colour, outside the car window: title, exit and loader exact; erase-save exact on every shot; `menus_settings` 657 of 720 shots exact, 52 under 300 pixels; the menu walk to the car selection exact; the course screen's records panel exact, the rest of that page being the course screen's backdrop (video). The car window: the car's pose and colours match; it now shows the newest picture (decision 5), one step ahead of the original. After a race the car window also shows the race frame's HUD boxes, drawn into every composed picture (`fx_draw_picture`, reported to its owners). The slide from the game selection to the car selection runs 2-4 frames ahead for about 40 frames, cause not found Stage 10: the menus fill the window at any shape (ad628fd1, menu_window.c): each screen's background goes on past the original picture, the course menu's granite through a map built at the window's size, the other layers and sprites kept to the picture, the windows moved by the origin; seen at 16:9, 21:9 and 9:16 on the title, selection, car selection with the car, after a race and after a demo. Panels slide in at the picture's edge, not the window's. Typing the initials on a keyboard (32bb46cd, engine 4.5) and the auto-accelerate option kept in the save (050c477a; the race applies it, e8920e96): compiled, not run on a keyboard or touch device; the options' switch waits on the pause screen. Section 8 in menu/ and save/: the finder reports nothing; pad bits by name; the per-line tables, cartridge tables, pictures, palettes and sound sets named once; the panel words named and two unread words gone. |
| 8 to 10 | 8 Pacing: done, a step every 6 updates (row 7). 9 Input and options: the engine's native inputs (4.5) in; the pause screen the game's own (`src/pause`: Escape, Start or the corner button; the picture behind blurred by a shader, 0cf4e86b); typing the initials and auto-accelerate compiled, not run on a keyboard or touch device. 10 Enhanced: see the menus, video and 3D rows; unlimited view distance done (56ea23c6); the ground and water designed (stage 10) and being built |
| 11 Platforms | macOS (Metal): runs headless through the menus and races. iOS Simulator and Android emulator (arm64): built and run 3 minutes each through the title and attract race with no crash or error, judged from logs and the game's state; picture, sound and touch not looked at. Windows (Vulkan, cross-built with mingw) not run, Linux not built: both need another machine, which needs Jeff's permission |
| Race objects | Interim: run only inside the race session's runs: the attract demos, the menu walk to a race and back, and the recorded battle, whose objects equalled the recording at all 898 two-player steps in a scratch run. A one-player race is not compared past its first step. `native/stuntracefx/src/object`: 9,900 lines, every live routine of the 752 in `src/stuntracefx/object` converted or among the 228 dead listed with proof in `object/NOTES`; handlers found by number in `object/handler_table.c` (376 cases). Section 8 audit (2026-11-09): `nshvc-find-hardware` reports nothing; no work RAM addresses; the course script dispatches on its command byte, not the jump table's routine addresses; the Super FX RAM words, the ROM tables read through and the trigger areas are named by meaning; the shared fields once named by address are named (1e610f7e). The earlier remaster's Enhanced calls (44 sites) settled against stage 10 in `object/NOTES`: most are whole steps' (places and angles set between by fx_tween; frames, colours and counts a step at a time as in the original). The far view's rules are in `object/animate.c` and `draw_list.c`: nothing drawn far before its set-up has run, stopped short lives left out, decorations kept running, race objects' looks going on, parked cars' tyres. So are the gaze between steps and cars kept models however far. The looks and gazes between steps go by the 3D session's `fx_tween_t`. Not done: circling pieces kept on their circle between steps (`ObjectOrbitBetween`, written, waits on fx_tween to call it); a parked tyre of a near car is marked for drawing among the near ones, which waits on fx_far.c. Compiled, not run. The values the code hands on are named too: models by their number in the cartridge's model table (28-byte records from $A040), looks, object scripts, the car scripts (F-Type, Coupe, 4WD and 2WD in the menu's order, their rivals, the bonus truck, told apart by their wheels in the ROM), the records' Super FX handlers and the camera paths as offsets into the path table. Record field offsets (58 raw) and the Object record's offset-named fields (`count48`, `byte4a` and the like, each used differently by kind) are fields, not addresses; named only where the meaning is shown |
| Race picture (video) | Interim. Not done: the full-window screens (stage 10; design in the notes folder, `video-fullwindow-design.md`; step 1, the one-player race at any size, approved and under way, its 3D geometry waiting on the Enhanced 3D session); the title after a demo shows the demo's last 3D frame until the menus' screen opens call `race_screen_end` (sent to the menus session). Done: every routine of `src/stuntracefx/video` (without `unused_screen.c`) has a function or is listed in `video/NOTES` (folded, replaced by the engine, converted by the code that drives it, or dead with the cartridge's call counts). The two-player battle runs from the menus to the race's end; its bugs found against the battle recording are fixed (raster chain, the link-0 crash, two duplicated state fields, the frozen half's stripes, the 3D's palette row, the second car's colours). The race starts 61 frames sooner than the original's because the original's loads take 64 blank frames the game does in one update. |
| Race 3D (fx, stage 10) | Interim: one-player race and course menu done and seen in runs; not done: the hills of a picture between steps (video reading `fx_tween_view`; they still move per step), weather drops and particles move per step, perspective textures (textured faces are cut by depth instead), the course menu's in-between pictures not looked at, the full window's ground beyond the original's view cone (the line-by-line sky shows where no course polygon is: object triggers or video's ground colour), solids (F). Since then: the sort's side cull follows the view's width (it was the chip's fixed 45 degrees, 128 pixels a side: in a 16:9 race it now keeps 1-6% more objects, those at the wide edges); a paused step's picture stays the last step's; objects made in a step are drawn where they stand (the objects' slot counts); no exact rule, chip-screen picture or switch back to them remains (decision 5). Seen in a race from the menus at 1280 x 720 (16:9): the 3D fills the window, drawn at the window's pixels with the smooth rule, the panel boxes and HUD anchored, and the 3D changes at every update (1,800-7,000 pixels an update, none frozen). Checked on the recorded race at 4 times (step A): every on-screen vertex within the chip's own rounding (0 of 32,894 past it; a planted centre + 2: 32,891 past). Done: A-D on the fx side (`fx_face.c`, `fx_tween.c`, `fx_draw.c`) Solids: tyres, the bonus trucks' wheels and trees written (fx/fx_solid.c, 2ba1ab24) and shown to Jeff side by side with the sprites; on in the game since he approved the look (2026-11-09); the 4WD's eyes on too (the objects' gaze per draw record), approved from the pictures. A sprite's quads are kept to its own texels (thin lines beside magnified sprites, found in the tree pictures). Two players in the full window on (with video's layout and race's words): seen in a battle at 16:9 side by side and 4:3 above and below, each player's 3D in its rectangle; their 4-bit pictures keep sprite tyres. |

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
  updates on displays faster than 60 Hz. The engine (4.5) does this: each backend shows a picture a refresh (Metal's
  `nextDrawable`, Vulkan's FIFO, D3D12's `Present(1)`) and the game is drawn once a picture with `between`, the part of the
  update's time gone. The race's 3D and its hills take it (`race_step_progress`); the HUD, the menus and the other 2D layers
  move once an update.
- Menus in the whole window.
- Unlimited view distance: nothing culled or faded out by distance where the original cut it off for the chip's sake
  (earlier work: 218f69f1).
- The ground as a real surface: each course's colour-band ground and water replaced by a generated ground or water plane
  with its own polygons, textures and layers, the track drawn over it, water seen from above and overhead when under it
  (earlier work, merged on main then: 8931a216..c68f1373, af4032ea, be60bbb9; `remaster/fullwindow.c`'s surface table;
  Aqua Tunnel's sea level -1280).
- Solid shapes for the sprites that show solid things: the cars' tyres, the 4WD's eyes, the bonus trucks' wheels, the trees
  (faceted, as approved); effects stay sprites. No filtered or upscaled art.
- The pause screen with the controller panel and key remapping on every host, the options (auto-accelerate), and typing the
  initials on a keyboard. In (`src/pause`): the panel, a keyboard's keys rebound and saved, the screen opened by Escape,
  Select + Start (a touch screen's own controls) or the corner button, moved through with any pad, and the AUTO-ACCELERATE
  switch (b51eee0c). Not done: a controller's buttons rebound; the engine binds keys only (`nshvc_control_bind`), so it
  waits on the engine. A touch screen's controls are the engine's own, with nothing to rebind.
`fullwindow.c`'s CPU span generator is not brought over; its decisions (view words, anchors, sky) are the reference.

#### Stage 10's 3D (the fx session's design, 2026-11-09)

How it meets the direction in one line: the CPU makes vertices (geometry) and lists them; the GPU fills every pixel with the
smooth rule at the window's resolution; no span, row or pixel is made on the CPU.

Facts it rests on, from reading: the engine renders a draw list into a target of the list's own size and samples it across the
whole screen (`rr_metal.m` draw_lists, `picture_body.h` kind 3), so the 3D's resolution is the size the list is cleared at;
`nshvc_vertex` is 2D (x, y, u, v, colour), so projection stays on the CPU and texture mapping is affine per triangle unless a
shader is given the depth. Every polygon and line the race draws is queued by one routine, `FxModelFaces` (01:90CD), from the
model's projected point table ($04E4 + 6i), which `FxModelProject` (01:8BAA) makes from view-space points ($0340 + 6i plus the
object's place at $0026-$002A, summed in 16 bits) with focal length 128 (`FxRatioClamp`: R1 * 256 / R3 of the halved pair; the
far table at $14:896C gives the same). The chip holds a point's offset at $3FFF, clamps far depths at $43FE and clips at z = 0
and at the view's edges in 2D, at whole pixels.

**Where the precision comes from.** The frame program goes on deciding everything at the console's scale: which objects
(the sort and its frustum), level of detail, facing, painter's order, materials, which faces are queued. Only positions are made
again, in floats, from the same view-space points:
- Faces. `FxModelProject` keeps each point's view-space sum as floats beside its table; `FxModelFaces` notes each face's queue
  entry, its point indices and its u, v. A face is committed when the list's end has moved past its entry (settled at the next
  face and at `FxDrawList`); its shape is then made from its points: clipped in view space against a near plane, projected
  x = cx + 128 X / Z, y = cy + 128 Y / Z unrounded and unheld, clipped in 2D against the screen with a margin (the GPU's guard
  band), and a textured face cut by depth planes until each piece's far over near depth is at most 1.25 (affine pieces; the
  original cut once, at 3). `FxDrawList` draws that shape in place of the chip's entries from the face's first entry to its
  last (the chip's near split makes two). The chip's held, clamped and edge-clipped vertices are not used. This is
  `fullwindow.c`'s `project_exact` done once per face in floats; neither a 16-bit list nor view words scaled by k are needed for
  the precision.
- Scaled sprites (`FxObjectSprite`): centre and size from `FxProjectScaled`'s inputs in floats; the chip's row quads mapped
  from its whole-pixel box onto that one.
- Particles, weather, labels and the HUD's chip-drawn pieces: the console's pixels, scaled with nearest texels (not sub-pixel
  until step E).

**Resolution.** The list is cleared at the window pixels the screen covers (`nshvc_display_info` and the screen's shape; 4 times
the screen when no window is known, as in a check), rule smooth. Recordings keep vertices in the screen's pixels as floats;
composing scales them. Quads put u, v at their corners for nearest texels: no filtered or upscaled art.

**The window's shape (the full window).** The view words come from the screen, not the original's window: centre across at
W / 2, down at the original's horizon row (screen line 96) plus half of any rows a taller screen adds; edges the whole screen
(each half for two players); the frustum constants $92 = 128 * 2^14 / half-width and $94 = 128 * 2^13 / the larger
half-height, as the original's are made (0x4EC4, 0x4000 for 208 x 128). The sort's cull and the frame program's clips then cover
the window. The words stay 16-bit (W up to 16,383). The race sets `g.view_*` from `fx_view_words()` at its set-up and when the
screen's shape changes. The HUD's chip-drawn pieces are placed by the video session's anchors.

**In-between pictures.** At each step's end the positions and angles are kept: the camera records (+08 point, +0E angles), the
objects (+0C place, +14 angles), the Super FX records (the list at $2332: +1E place, +24 angles, the car parts). A picture at
draw(between) is t = (updates since the step + between) / 6 of the way (the course menu's steps: / 4): the whole state is saved
(`g`, `objects`, `cameras`, `fx_ram`, `fxs`), the camera, objects and records set t of the way from the last step's end to this
one's (angles the short way; a field that moved more than $800 in a step, or an object made in the step, keeps the end's), the
picture's part of a step run (`ViewFromRecord`, `ObjectDrawListBuild`, `GsuRecordsUpload`, `ViewUpdateAndGsuAEF8`,
`BackdropAnglesFromView`, `GsuParamsUpdate`, `FxFrameProgram` into a third recording), the backdrop's scrolls for that picture
handed to the video session, and the state put back. Nothing the game keeps changes, so the redraw's weather drops, particles
and seed move per step (a known limit). The step's end shows a step late, as the original's picture showed after its step's
frames.

**Needed from other sessions.**
- Race: a call of `fx_tween_step_end()` at the end of `RaceFrameRun`, `RaceFrameRunTwoPlayers` and `RaceFrameStep` after the
  picture; `race_step_progress(float between)` (0 at a step, 1 at the next); that the calls above are a picture's whole part of
  a step and touch nothing but `g`, `objects`, `cameras` and `fx_ram`; `g.view_*` from `fx_view_words()`.
- Objects: that `ObjectDrawListBuild` and `GsuRecordsUpload` change nothing but `g.frame_records` and its count; how to tell an
  object made or freed during a step.
- Video: the screen's size and where the 3D's picture origin goes; `race_frame_art_draw` scaled to the list
  (`fx_draw_list_scale`); in the full window, anchors for the chip-drawn HUD pieces; the backdrop scroll of an in-between
  picture taken from `fx_tween_backdrop()`.

**Steps, each with its check (fx_view on the recorded race, 1,100 frames, unless said).**
- A. Sub-pixel picture in today's layout: faces and sprites in floats, the list at the output's pixels, smooth rule. Checks:
  every vertex of a face whose points all lay on the screen within 1.02 pixels of the chip's (the chip floors or ceils by sign,
  so 0 apart is expected; a planted centre + 2 must fail it); the picture drawn 4 times larger and read at each console pixel's
  centre against the exact rule's picture of the same frames: a pixel apart only where the exact picture has a pixel of another
  colour among its 8 neighbours (the shapes sit at their unfloored places, on average half a pixel right and down of the chip's
  floored vertices, so edges move by up to a pixel and no further), except inside textured faces, whose texels now follow the
  depth cuts; the count against the original's bitmap is reported, not predicted; game shots at 4 times for looking.
  Result (2026-11-09, fx_view on the recorded race, 184 pictures): geometry 0 of 32,894 on-screen vertices past the chip's own
  error (59 more than 1.02 pixels apart: the chip's far-depth table is truncated and indexed by the depth rounded down to even,
  and a near point's pair is halved, so the bound is 1 + |x| / 65536 + 128 |x| / z^2 for a far point and 1 + 128 / z for a near
  one, both read from the chip's code and its table); planted centre + 2: 32,891 past. Picture: 4.67% of pixels apart from the
  exact rule's, of which 2,999 in all (16 a picture) have no pixel of another colour among their 8 neighbours, so the criterion
  above is not met as written; in the two worst pictures every cluster is one of: the view's top row and last column (floored
  vertices clipped to the border; the original fills column 207, the exact rule leaves it empty), posts thinner than a pixel that
  the exact rule drops, sprite sizes the chip floors (21 against 21.7), and the chip's clipped vertices on long shallow edges (up to
  1.44 pixels). Against the original's bitmap: 3.92% apart (the exact rule 2.79%).
- B. The full window: view words from the screen, the 3D over the whole screen, HUD pieces anchored. Needs video and race.
- C. In-between pictures. Needs race, objects and video. Check: a race run's pictures between two steps lie between the two
  steps' pictures (vertices of the same face moving monotonically), and the state after a run with in-between pictures equals one
  without.
- D. The course menu's 3D: A's precision in its window, C at its 4-update steps.
- E. Perspective-correct textures (a game shader given the face's 1/z plane, u/z and v/z as vertex u, v), angle fractions for
  turning objects between steps, particles, weather and labels sub-pixel, facing from the 3D determinant if slivers show.
- F. Solid tyres, eyes and trees (`remaster/solids.c`).
- G. The view distance (218f69f1 ported, 56ea23c6): the draw list writes a far record for every other object it would draw, in
  every map cell, into the chip's RAM's second 64 KB (`fx/fx_far.h`); the frame program draws them first, farthest first,
  turned and culled as the sort does, each halved until its place fits 14 bits with its matrix and sprite size, its level of
  detail and colour tier from its true depth; the chip's first 64 KB put back after. Kept as the original has them: the levels
  of detail and the four colour tiers (they change a model's detail and colour, they leave nothing out).
- H. The ground and water as a surface (design below).

#### The ground and water (design, 2026-11-09)

What carries over from the earlier work (be60bbb9, 8931a216..c68f1373, af4032ea; `remaster/fullwindow.c`): each course's surface
by its number ($0E3D, `g.course`): none (Sky Ramp), land or water by its band's ground colours, at the height its scenery is
planted at (Aqua Tunnel's sea level -1280); none in Aqua Tunnel's sealed tunnel (bit 3 of $0DA5 set by its dark gates); the
looks: water as the underwater backdrop's ripple cells at the size the original shows them, their colours by distance from its
strip, the water between them the band's colour for that distance; land as tufts in its band's shades on cells of the same size;
the track occludes the ground, a road dipping below a surface is never left out, and only Aqua Tunnel's water cuts the track and
is seen from under. What does not carry over: the per-pixel depth test and the water pass, which were the old front end's own
fill kernels; the engine's draw lists paint in order and have no depth.

How it is drawn, in one line against the direction: the CPU makes one polygon and eight numbers a picture; the GPU works out
every pixel of the surface, its texel and its colour.
- **The polygon.** The plane at the surface's height, turned to the view (the matrix at $00E4 and the eye's height), clipped in
  view space against a near plane and the view's sides and out to a depth where it meets the horizon within a hundredth of a
  pixel, projected as the faces are (fx_face.c): one convex polygon covering the ground's part of the picture, made per picture
  and per player, so the pictures between steps and the wide window follow.
- **The shader** (`shaders/ground.shade`, fragment only, as the engine's game shaders are). The vertices' u, v are their screen
  places, so the GPU hands every pixel its own place exactly. The plane's coordinates from a screen place are one projective map
  (x' = (a x + b y + c) / (g x + h y + 1), y' the same with d, e, f): its eight numbers are the material's eight inputs. From
  them the pixel's texel (the art repeating across the plane) and its depth (the map's denominator), which picks the colour by
  distance. The material's image holds the cell art and a strip of the band's colours by depth, written when the course's
  colours change (art prepared as the sprites' textures are, never pixels of the picture).
- **The order.** Land, and water other than Aqua Tunnel's: the surface is drawn first, after the screen clear and before the far
  and near objects, so everything the race draws covers it (the track occludes the ground; a road below it is never hidden), and
  it is not drawn while the eye is below it. Aqua Tunnel's water: with one flat surface, anything on the far side of it from the
  eye is behind it along every ray, so the depth test's picture comes from order alone. Each face's shape (fx_face.c, already in
  view space) is cut by the plane; the part on the eye's side goes to a layer drawn after the surface, the other part to one
  drawn before it, each layer keeping the frame program's order; a sprite or solid goes whole by its place's side; particles,
  weather and labels after. Under the water (the glass tube) the same with the sides swapped: the surface seen overhead.
- **Two players**: each panel draws its own surface in its rectangle (the frame program runs per panel).
- No engine feature is needed: draw lists take a material with a game shader, its image read at any texel (`nshvc_image`, as
  `pause_blur.shade` does) and eight inputs.
- Shown to Jeff on a couple of courses (land and water, Aqua Tunnel from above and in the tube) before it is switched on.

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
| 5 | (Jeff, unasked, the same day) | "Enhanced IS the native game. we are abandoning 'original' mode entirely. This isn't an emulation project. Be sure." |

## 10. What is not known

- Whether the voice calls cover both drivers. Stage 1 and stage 3 will show.
- Whether a draw-list layer's priority and per-line scroll behave as the race screen needs. Stage 1 will show.
- How the edge difference looks in numbers.
- The state comparison's masks: which work-RAM bytes are scratch the new game no longer has. Each masked byte needs a reason.
- Whether every use of a computed address (1,821 lines) resolves to a typed table or record by reading. If any does not, it is reported.
