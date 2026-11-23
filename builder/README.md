# Stunt Race FX Builder

A native app for macOS, Windows and Linux that builds Stunt Race FX from the
player's own copy of it: the native game (`native/stuntracefx`) on
nativeshvc. Players download it from the repository's Releases page; nothing
here ships game data.

1. What to build for (macOS): the Mac, or the iPhone (an .ipa, signed with
   the player's own Xcode signing when the Mac has a code-signing certificate
   with a provisioning profile for it, else unsigned, with a warning in red;
   it runs on the iPad too). On Windows the build is for Windows, on Linux
   for Linux, and this page says what it makes.
2. The ROM and the folder to build into: the No-Intro dump of Stunt Race FX
   (USA) (Rev 1), `.sfc` (a `.smc` with a copier's 512-byte header is taken
   without it). The builder shows the MD5 it expects and the file's own, and
   builds only from a match.
3. The build: the code ships inside the builder; the data comes from the ROM.
4. The result: the app, the .ipa or the folder, or the reason the build stopped.

The result needs no ROM nor this tree: it carries the cartridge's data as
`cart.dat`, an image of the ROM with each range the game reads
(`native/stuntracefx/tools/assets.csv`) at its offset and nothing where the
ROM's code was, as `native/stuntracefx/tools/extract_assets.py` makes it
(`src/core/srfx.c`). The game loads it at start-up (`src/data/cart.c`), with
its shaders beside it. The game, and the builder itself, have the game's own
icon (`native/stuntracefx/icon/icon.png`).

**Records and settings** live in the game's own folder, so a built game copied
to another machine or drive carries them (the game opts in with the engine's
`save_in_game_folder`, `native/stuntracefx/src/system/main.c`): a folder `save`
inside the game's folder on Windows and Linux, `stunt-race-fx-save` beside the
`.app` on the Mac (beside it, not inside, so the app's signature stays valid).
Where the place cannot be written (a read-only disk, Program Files, a disk
image, a quarantined app) the engine falls back to the system's folder, and
the files an earlier build kept there are copied into the game's folder the
first time it is used, the originals left alone. The iPhone and iPad keep them
in the app's own data (the engine ignores the flag there): installing a build
over the app keeps them, deleting the app deletes them. Rebuilding into the
same place never deletes them: on the Mac the app is replaced and the folder
beside it is not touched; on Windows and Linux an earlier build that has a
`save` folder is updated in place (everything but `save` and the marker file
replaced, `rb_clear_except` and `finish` in `src/core/`), so a rebuild that
stops partway leaves `save` where it was.

Version 0.992. The builder and the game it builds run on macOS 12 or later
(Apple silicon and Intel), Windows 10 or later (64-bit, with a Direct3D 12
graphics driver; no Vulkan driver is needed) and Linux with glibc 2.35 or later (x86_64 or arm64, X11 or
XWayland, a Vulkan driver; GTK 3 for the builder's window, else it builds
from the terminal with `--rom FILE --dest DIR --build`); the iPhone and iPad
app on iOS 15 or later.

## Layout

| path | what |
|---|---|
| `src/core/` | portable C: MD5, files (`util.c`), the game's data from the ROM (`srfx.c`), the build itself (`build.c`) |
| `src/mac/` | the AppKit front end, the builder's Info.plist, the iPhone game's (`ios-Info.plist`), and `ios_sign.sh` (finds the player's Xcode signing and signs the iPhone build with it) |
| `src/win/` | the Win32 front end, its manifest and resources |
| `src/linux/` | the GTK 3 front end (GTK opened while it runs, so the builder needs only the C library to start) |
| `linux/` | the Linux build: `build-linux.sh` (run on a Linux machine by `build.sh` over ssh), `toolchain.cmake` and `sysroot.py` (Ubuntu 22.04's C library, so the builds need glibc 2.35 at most) |
| `build.sh` | builds the builders into `build/builder/dist/`: the macOS app, a Windows zip of the exe with the game beside it in `game\`, and a Linux tar.gz per CPU laid out the same way |
| `release.sh` | builds and publishes them as a GitHub release |
| `check_payload.py` | proves the shipped game executables carry none of the cartridge's data |

## Building

```sh
builder/build.sh                 # build/builder/dist/: the macOS and Windows zips, a Linux tar.gz per CPU
builder/release.sh --dry-run     # the release notes and files, nothing published
builder/release.sh               # tag vVERSION at HEAD (already pushed) and publish
```

The payload is the native game built from HEAD (a worktree of it, so
uncommitted work, and `cart.dat`, which is never committed, cannot ship), as
nativeshvc's `nshvc-build` configures it: on macOS universal (arm64 and x86_64
built apart and joined) drawn with Metal, with its shaders and icon, and the
iPhone app beside it (unsigned); on Windows drawn with Direct3D 12, its shaders
(DXIL) beside it; on Linux (x86_64 and arm64) drawn with Vulkan, built on a Linux
machine over ssh (`LINUX_HOST`) against Ubuntu 22.04's C library. The builder's data table (`cart_table.h`) is written from the
game's `assets.csv` at the same commit, so the two always agree. Building
needs nativeshvc's checkout (`NSHVC`, `../nativeshvc`), Xcode's clang and the
iOS SDK, cmake and ninja, glslangValidator and spirv-cross (dxc for the D3D12
shaders; nativeshvc's own is used), mingw-w64 for Windows, brew's vulkan-headers
for Linux, and the ROM (`ROM`,
default under `reference/`; `check_payload.py` reads it to prove its absence).

Every front end takes arguments for a scripted run:
`--dest DIR --rom FILE --build`, and on macOS `--target mac|ios`. A scripted
run quits when the build is done (exit code 0 on success); the Windows one
writes its result to `builder-result.txt` beside the builder.
`RB_SNAPSHOT=DIR` makes the macOS app write each page it shows to DIR as a PNG.
