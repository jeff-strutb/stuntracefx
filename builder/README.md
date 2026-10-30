# Stunt Race FX Builder

A native app for macOS and Windows that builds Stunt Race FX from the
player's own copy of it: the decompilation (`src/stuntracefx`) on nativeshvc,
as `ports/stuntracefx/front` builds it. Players download it from the
repository's Releases page; nothing here ships game data.

1. What to build for (macOS): the Mac, or the iPhone (an .ipa, signed with
   the player's own Xcode signing when the Mac has a code-signing certificate
   with a provisioning profile for it, else unsigned, with a warning in red;
   it runs on the iPad too). On Windows the build is for Windows, and this
   page says what it makes.
2. The ROM and the folder to build into: the No-Intro dump of Stunt Race FX
   (USA) (Rev 1), `.sfc` (a `.smc` with a copier's 512-byte header is taken
   without it). The builder shows the MD5 it expects and the file's own, and
   builds only from a match.
3. The build: the code ships inside the builder; the data comes from the ROM.
4. The result: the app, the .ipa or the folder, or the reason the build stopped.

The result needs no ROM nor this tree: it carries the cartridge's data as
`romdata.bin`, an image of the ROM with each asset the game reads
(`ports/stuntracefx/assets.csv`) at its offset and nothing where the ROM's
code was, as `tools/assets.py` makes it with `ASSETS_DATA=file`. The game
reads it at start-up (`front/cart.c`, `SRFX_ROMDATA_FILE`). The iPhone's has
the title's PUSH START as TAP TO START (`assets.py` `ios_edits`: the ROM's
sprite sheet unpacked, edited and packed again, done in C by
`src/core/srfx.c`). Linux is not built yet.

Version 0.91. The builder and the game it builds run on macOS 12 or later
(Apple silicon and Intel) and Windows 10 or later (64-bit, OpenGL 3.3); the
iPhone and iPad app on iOS 15 or later.

## Layout

| path | what |
|---|---|
| `src/core/` | portable C: MD5, files (`util.c`), the game's data from the ROM (`srfx.c`), the build itself (`build.c`) |
| `src/mac/` | the AppKit front end, the app's Info.plist, and `ios_sign.sh` (finds the player's Xcode signing and signs the iPhone build with it) |
| `src/win/` | the Win32 front end, its manifest and resources |
| `build.sh` | builds both builders into `build/builder/dist/`: the macOS app, and a Windows zip of the exe with the game beside it in `game\` |
| `release.sh` | builds and publishes them as a GitHub release |
| `check_payload.py` | proves the shipped game executables carry none of the cartridge's data |

## Building

```sh
builder/build.sh                 # build/builder/dist/StuntRaceFXBuilder-VERSION-{macOS,Windows}.zip
builder/release.sh --dry-run     # the release notes and files, nothing published
builder/release.sh               # tag vVERSION at HEAD (already pushed) and publish
```

The payload is the game built from HEAD (a worktree of it, so uncommitted
work never ships) with `ASSETS_DATA=file`: on macOS universal (arm64 and
x86_64, each on its own nativeshvc build) with the Super FX's polygons drawn
by Metal, and the iPhone app beside it (unsigned); on Windows drawn with
OpenGL. The builder's asset tables (`romdata_table.h`, `romdata_table_ios.h`)
are the ones `assets.py` wrote for that payload, so the two always agree.
Building needs nativeshvc's checkout (`NSHVC`, `../nativeshvc`), Xcode's clang
and the iOS SDK, cmake, mingw-w64 for Windows, and the ROM under `reference/`
that the port already builds from (`check_payload.py` reads it to prove its
absence).

Both front ends take arguments for a scripted run:
`--dest DIR --rom FILE --build`, and on macOS `--target mac|ios`. A scripted
run quits when the build is done (exit code 0 on success); the Windows one
writes its result to `builder-result.txt` beside the builder.
`RB_SNAPSHOT=DIR` makes the macOS app write each page it shows to DIR as a PNG.
