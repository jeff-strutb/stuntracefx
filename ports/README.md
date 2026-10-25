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
| render (Super FX) | all the drawing code the game reaches: the geometry (matrices, angles, lengths, projection, sorting, point tables), polygon gathering, clipping and projection, the draw list, the 3D model interpreter, the frame program (the objects as models, scaled sprites and particle bursts, the rain and snow, the heat haze, the car labels), the text and its bitmaps, and the HUD programs (glyphs, rolling digits, the rising picture, scaled sprites, the noise fill), with the Super FX's two pixel caches (a program that stops with pixels in them leaves them for the next). Left out, as no code reaches them: render/unused.c, FxModelProjectEach, FxPolygonClipScreenUV and the two faces paths to it, FxObjectSpritePull, FxObjectSpriteAt, FxEffectsAddUnused, the four FxWeatherUnused entries and FxDrawBitmap120x48 | random states against the decomp: 580 k for the geometry, 21 k for the frame's parts, 46 k for the HUD programs; 2,061 of the original's own Super FX calls (from 48 plays) against its exit RAM; 8,920 synthetic models against the decomp; 0 differ, planted breaks caught |
| data (Super FX) | the unpacker (FxUnpackProgram and its bit readers) and the RAM set-up at power on (FxInit) | 49 of the original's own unpacks, 9,800 of them redone to other places, and 3,000 generated streams (every literal and copy form) against the decomp; 0 differ, planted breaks caught |
