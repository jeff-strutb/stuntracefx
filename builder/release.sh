#!/bin/sh
# builder/release.sh: build Stunt Race FX Builder for macOS and Windows and
# publish both on the repository's GitHub Releases page as vVERSION.
#
# VERSION: 0.9 unless set. The release is tagged at HEAD as it is when this
# starts, which must already be on GitHub. Running it again at the same
# version replaces the files of that release.
#
# --no-build publishes the zips already in build/builder/dist, which must
# have been built from HEAD (build.sh writes the commit to dist/REV): for
# zips built and checked before the release.
#
# Usage: builder/release.sh [--dry-run] [--no-build]
set -e
cd "$(dirname "$0")/.."
DRY=0 BUILD=1
for a in "$@"; do
    [ "$a" = --dry-run ] && DRY=1
    [ "$a" = --no-build ] && BUILD=0
done

VERSION=${VERSION:-0.9}
TAG=v$VERSION
DIST=build/builder/dist
MAC=$DIST/StuntRaceFXBuilder-$VERSION-macOS.zip
WIN=$DIST/StuntRaceFXBuilder-$VERSION-Windows.zip

if [ -n "$(git status --porcelain -- builder ports)" ]; then
    echo "release: builder/ or ports/ has uncommitted changes; commit them first" >&2
    exit 1
fi
git fetch -q origin
REV=$(git rev-parse HEAD)          # the commit checked here is the one tagged, whatever lands meanwhile
if [ $DRY = 0 ] && ! git merge-base --is-ancestor $REV origin/main; then
    echo "release: HEAD is not on origin/main; push it first" >&2
    exit 1
fi

if [ $BUILD = 1 ]; then
    VERSION=$VERSION REV=$REV builder/build.sh
elif [ "$(cat $DIST/REV 2>/dev/null)" != "$REV" ] || [ ! -f $MAC ] || [ ! -f $WIN ]; then
    echo "release: the zips in $DIST are not built from HEAD ($REV); build them first" >&2
    exit 1
fi

NOTES=$(mktemp)
cat > $NOTES <<EOF
Stunt Race FX Builder makes a native build of **Stunt Race FX** (Super Nintendo, 1994) from your own copy of the cartridge: the decompilation's C, compiled for your machine and running on nativeshvc, with the cartridge's Super FX as the game's own code. On a Mac it builds the game for the Mac, or for the iPhone and iPad; on Windows, for Windows.

The game's data is copyrighted, so the builder does not include it: you provide your ROM, the builder checks it by MD5, takes the data the game reads from it (the ROM's code is not used: the C stands for it), and produces a game that runs on its own. The ROM is not needed once the build is done.

### Download
- **macOS** (12 or later, Apple silicon and Intel; the game it builds needs the same): \`StuntRaceFXBuilder-$VERSION-macOS.zip\`. Unzip and open Stunt Race FX Builder. It is not notarized: if macOS refuses to open it, Control-click it and choose Open, or allow it in System Settings, Privacy & Security.
- **Windows** (10 or later, 64-bit; the game it builds needs the same, and OpenGL 3.3): \`StuntRaceFXBuilder-$VERSION-Windows.zip\`. Unzip it and run \`StuntRaceFXBuilder.exe\` from the unzipped folder (the \`game\` folder beside it holds the game's code). It is not signed: if SmartScreen stops it, choose More info, then Run anyway.
- Linux is not built yet.

### What it needs
| Your dump | MD5 |
|---|---|
| The No-Intro ROM of Stunt Race FX (USA) (Rev 1), \`.sfc\` (a \`.smc\` with a copier's 512-byte header is accepted) | \`128b316a74caf17fdc216b3ab46d4a9a\` |

The first release (Rev 0) and the European and Japanese cartridges are not supported yet.

### What it makes
- Mac: \`Stunt Race FX.app\`.
- iPhone and iPad (iOS 15 or later), from the Mac builder: \`Stunt Race FX.ipa\`, with the title's PUSH START reading TAP TO START. When your Mac has Xcode signing (a development certificate with a provisioning profile for it), the .ipa is signed with it, ready to install from Xcode (Devices and Simulators) or Apple Configurator. Otherwise the builder says so in red and the .ipa is unsigned, for you to sign and install with a tool such as AltStore or Sideloadly.
- Windows: a folder, \`Stunt Race FX\`, with \`Stunt Race FX.exe\` in it.

### Playing
- **Original** mode is the cartridge's game as it ran. **Enhanced** mode adds a 60 frames a second option (Frame rate 60) and **Widescreen** (the race fills any window shape). Tab switches between them; F1 sets the frame rate, F2 Widescreen.
- A keyboard or a game controller is the SNES pad. On the iPhone and iPad, touch drives it, or a game controller.
- Full screen: View, Enter Full Screen (Control-Command-F) on a Mac, Alt+Enter on Windows.
- Records and settings live in your user folder (Application Support on macOS, the iPhone and the iPad, AppData on Windows), so rebuilding never touches them.

Source: [builder/](https://github.com/jeff-strutb/stuntracefx/tree/$REV/builder), [ports/stuntracefx/](https://github.com/jeff-strutb/stuntracefx/tree/$REV/ports/stuntracefx), [src/stuntracefx/](https://github.com/jeff-strutb/stuntracefx/tree/$REV/src/stuntracefx).
EOF

if [ $DRY = 1 ]; then
    cat $NOTES
    ls -la $MAC $WIN
    rm -f $NOTES
    exit 0
fi
if gh release view $TAG >/dev/null 2>&1; then
    # the same version again: its tag moves to the source these files are from
    git tag -f $TAG $REV >/dev/null
    git push -q -f origin refs/tags/$TAG
    gh release upload $TAG $MAC $WIN --clobber
    gh release edit $TAG --notes-file $NOTES
else
    gh release create $TAG $MAC $WIN --target "$REV" --title "Stunt Race FX Builder $VERSION" --notes-file $NOTES
fi
rm -f $NOTES
gh release view $TAG --json url -q .url
