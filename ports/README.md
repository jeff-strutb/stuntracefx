# Stunt Race FX, native

The game's code rewritten by what it does, from the decompilation in
`src/stuntracefx`, as plain C for any platform: no bus, no processor
registers, no clock counting, no console model. The decomp stays the
reference and is never edited for the port.

## Layout

    ports/common/game/        the game, platform-agnostic C
      include/state.h         the game's state by name (work RAM, game pak RAM,
                              laid out as the original's)
      include/game.h          each area's functions
      include/fixmath.h       the maths
      math/ hud/ audio/ video/  the areas rewritten so far
      check/                  the checks against the decomp and the original
    ports/<platform>/         each platform's own code (to come)

## The standard

Each routine is rewritten by meaning, and computes exactly what the
original computes: its roundings, lost bits, carries taken from an earlier
instruction, and other quirks are kept and named in a comment where they
are. Work the original did only for its own machine (scratch in the direct
page, flags no caller reads, the order of its memory accesses, its timing)
is not kept.

A rewrite is checked two ways before it is committed, and each check is
first shown able to fail (a planted break caught):

- against the decomp's routine, on random states within what the game
  leaves there: both run on the same RAM, all of it compared but the direct
  page (`check/check.sh NAME`);
- against the original game's own calls, recorded from the running
  cartridge (sfxbox, `SFXBOX_RECORD`): the port's routine, given what each
  call read, must leave exactly what the original left
  (`check/check.sh NAME N DIR`).

Routines whose callers are not rewritten yet are not yet linked into a
game; until then the decomp's routine serves those callers.

## Progress

| area | routines rewritten | checked |
|---|---|---|
| math | all 63 | 11.5 M cases against the decomp, 0 differ |
| hud | 44 of 47 (not RaceHudSetup, RaceHudSetupTwoPlayers, HudCountMove19EE: they wait on the video interface and the Super FX) | 761 k random states against the decomp and 14,250 of the original's own calls (228 scripts), 0 differ |
| audio | the sound queue (3 of 213) | with hud |
| video | the bar gauges and SpriteYOnScreen | with hud |
| render (Super FX) | 69 of 166: the geometry (matrices, angles, lengths, projection, sorting, point tables), polygon gathering, clipping (near plane and screen sides) and projection, the draw list (flat, line and textured shapes into the screen bitmap), and the 3D model interpreter (every command; four entries no code reaches, FxModelProjectEach, FxPolygonClipScreenUV and the two faces paths to it, left out) | 121 k random states against the decomp; 910 of the original's own Super FX calls (FxModelDraw and FxDrawList, from 48 plays) against its exit RAM, and 4,460 synthetic models (every reachable path) against the decomp, 0 differ |
