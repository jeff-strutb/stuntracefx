#!/bin/sh
# builder/build.sh: build Stunt Race FX Builder for macOS and Windows.
#
#   build/builder/dist/StuntRaceFXBuilder-VERSION-macOS.zip    Stunt Race FX Builder.app, universal:
#                                                              builds Stunt Race FX for the Mac or
#                                                              for the iPhone and iPad
#   build/builder/dist/StuntRaceFXBuilder-VERSION-Windows.zip  Stunt Race FX Builder\StuntRaceFXBuilder.exe
#                                                              and game\ (the payload), x64
#
# Each carries the game (ports/stuntracefx/front on nativeshvc) built with
# none of the cartridge's data in it (ASSETS_DATA=file: the game reads
# romdata.bin, which the builder writes from the player's ROM);
# check_payload.py proves that before anything is packaged. The payload is
# built from HEAD, or REV (a worktree of it at build/builder/tree), never
# from the working tree, so what is released is what is committed.
#
# VERSION: 0.9 unless set.
# Needs nativeshvc's checkout (NSHVC, ../nativeshvc), the ROM under
# reference/stuntracefx/ that the port builds from (its asset tables and
# routine table come from it; check_payload.py reads it to prove its
# absence), Xcode's clang and the iOS SDK, cmake, and mingw-w64
# (x86_64-w64-mingw32-*) for Windows.
set -e
cd "$(dirname "$0")/.."
ROOT=$PWD
VERSION=${VERSION:-0.9}
export VERSION                     # the Windows game's version resource too (game_rc.sh)
NSHVC=$(cd "${NSHVC:-../nativeshvc}" && pwd)
B=$ROOT/build/builder
DIST=$B/dist
TREE=$B/tree
ROM="reference/stuntracefx/Stunt Race FX (USA) (Rev 1).sfc"
echo "Stunt Race FX Builder $VERSION"

# ---- the source: HEAD, as committed ----------------------------------------------------------
REV=${REV:-$(git rev-parse HEAD)}   # (release.sh: the commit it tags)
if [ -d "$TREE" ]; then
    git worktree remove --force "$TREE" 2>/dev/null || rm -rf "$TREE"
    git worktree prune
fi
git worktree add -q --detach "$TREE" "$REV"
mkdir -p "$TREE/reference/stuntracefx"
ln -s "$ROOT/$ROM" "$TREE/$ROM"
echo "source: $REV"
rm -rf $B/payload-macos $B/payload-windows $B/gen $B/obj
mkdir -p $DIST $B/payload-macos/ios $B/payload-windows $B/win $B/gen

# nativeshvc for each system (one architecture a build: its coroutine switch)
nshvc() {   # DIR CMAKE-ARGS...
    d=$1; shift
    [ -f "$NSHVC/$d/CMakeCache.txt" ] || cmake -S "$NSHVC" -B "$NSHVC/$d" -DNSHVC_TESTS=OFF -DNSHVC_EXAMPLES=OFF "$@" >/dev/null
    cmake --build "$NSHVC/$d" -j8 >/dev/null
}
nshvc build/builder-macos-arm64 -DNSHVC_HOST=macos -DNSHVC_RENDER=metal -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_BUILD_TYPE=Release
nshvc build/builder-macos-x86_64 -DNSHVC_HOST=macos -DNSHVC_RENDER=metal -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_BUILD_TYPE=Release
nshvc build/builder-ios -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DNSHVC_HOST=ios -DNSHVC_RENDER=metal -DCMAKE_BUILD_TYPE=Release
nshvc build/builder-windows -DCMAKE_TOOLCHAIN_FILE="$NSHVC/build/mingw.cmake" -DNSHVC_HOST=windows -DNSHVC_RENDER=gl \
    -DCMAKE_BUILD_TYPE=Release

# the game, its data left out: front/build.sh in the worktree
game() {    # OUT NSHVC_BUILD [VAR=VALUE...]
    out=$1 nb=$2; shift 2
    (cd "$TREE" && env ASSETS_DATA=file NSHVC="$NSHVC" NSHVC_BUILD=$nb OUT=$out "$@" \
        sh ports/stuntracefx/front/build.sh | grep -E "error|built")
}
MACLIBS="-framework Cocoa -framework QuartzCore -framework AudioToolbox -framework CoreAudio -framework GameController -framework Metal -framework Foundation"
IOSLIBS="-framework UIKit -framework CoreGraphics -framework QuartzCore -framework AudioToolbox -framework AVFoundation -framework GameController -framework Metal -framework Foundation"

# ---- the payload: the Mac (universal) ---------------------------------------------------------
for A in arm64 x86_64; do
    game $B/game-macos-$A build/builder-macos-$A MACOSX_DEPLOYMENT_TARGET=12.0 CC="clang -arch $A" GPU_METAL=1 LIBS="$MACLIBS"
done
lipo -create $B/game-macos-arm64/stuntracefx $B/game-macos-x86_64/stuntracefx -output $B/payload-macos/stuntracefx
strip -S $B/payload-macos/stuntracefx
sh "$TREE/ports/stuntracefx/icon/icons.sh" $B/icon >/dev/null
cp $B/icon/AppIcon.icns $B/payload-macos/AppIcon.icns

# ---- the payload: the iPhone (unsigned; the builder adds the data and signs it) ----------------
game $B/game-ios build/builder-ios ASSETS=ios GPU_METAL=1 DEFS=-DNSHVC_HOST_IOS=1 \
    CC="xcrun -sdk iphoneos clang -target arm64-apple-ios15.0" LIBS="$IOSLIBS"
IAPP=$B/payload-macos/ios/app
mkdir -p $IAPP
cp $B/game-ios/stuntracefx "$IAPP/Stunt Race FX"
strip -S "$IAPP/Stunt Race FX"
# the Info.plist as ports/stuntracefx/front/package_ios.sh writes it, at this version
sed -n '/^cat > "\$APP\/Info.plist" <<EOP$/,/^EOP$/p' "$TREE/ports/stuntracefx/front/package_ios.sh" | sed '1d;$d' |
    sed "s/\$PLAT/iPhoneOS/; s#<key>CFBundleShortVersionString</key><string>1.0</string>#<key>CFBundleShortVersionString</key><string>$VERSION</string>#; s#<key>CFBundleVersion</key><string>1</string>#<key>CFBundleVersion</key><string>$VERSION</string>#" \
    > "$IAPP/Info.plist"
plutil -lint "$IAPP/Info.plist" >/dev/null
printf 'APPL????' > "$IAPP/PkgInfo"
IOS_SDK=iphoneos sh "$TREE/ports/stuntracefx/icon/icons.sh" $B/icon-ios >/dev/null
cp $B/icon-ios/ios/Assets.car $B/icon-ios/ios/AppIcon*.png "$IAPP/"
/usr/libexec/PlistBuddy -c "Merge $B/icon-ios/ios/partial.plist" "$IAPP/Info.plist" >/dev/null
cp builder/src/mac/ios_sign.sh $B/payload-macos/ios_sign.sh

# ---- the payload: Windows ---------------------------------------------------------------------
game $B/game-windows build/builder-windows CC=x86_64-w64-mingw32-gcc \
    LIBS="-lole32 -luuid -lxinput9_1_0 -lshell32 -lgdi32 -luser32 -lopengl32 -lwinmm -mwindows -static"
x86_64-w64-mingw32-strip -o $B/payload-windows/stuntracefx.exe $B/game-windows/stuntracefx.exe

python3 builder/check_payload.py "$ROOT/$ROM" $B/payload-macos/stuntracefx "$IAPP/Stunt Race FX" \
    $B/payload-windows/stuntracefx.exe

# ---- the builder's asset tables: assets.py's, as the payload was built with --------------------
cp $B/game-macos-arm64/gen/romdata_table.h $B/gen/romdata_table.h
cp $B/game-ios/gen/romdata_table.h $B/gen/romdata_table_ios.h

# ---- macOS: Stunt Race FX Builder.app ---------------------------------------------------------
APP="$B/mac/Stunt Race FX Builder.app"
rm -rf "$B/mac"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/payload"
clang -arch arm64 -arch x86_64 -mmacosx-version-min=12.0 -O2 -Wall -Wextra -fobjc-arc -DRB_VERSION="\"$VERSION\"" \
    -Ibuilder/src/core -I$B/gen builder/src/core/*.c builder/src/mac/main.m -framework Cocoa \
    -o "$APP/Contents/MacOS/StuntRaceFXBuilder"
sed "s/@VERSION@/$VERSION/g" builder/src/mac/Info.plist > "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
cp $B/icon/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
cp -R $B/payload-macos/ "$APP/Contents/Resources/payload/"
codesign --force --sign - "$APP/Contents/Resources/payload/stuntracefx"
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"
rm -f $DIST/StuntRaceFXBuilder-*-macOS.zip
ditto -c -k --keepParent "$APP" $DIST/StuntRaceFXBuilder-$VERSION-macOS.zip

# ---- Windows: StuntRaceFXBuilder.exe ----------------------------------------------------------
MAJOR=${VERSION%%.*}
MINOR=$(echo "${VERSION#*.}" | sed 's/^0*\([0-9]\)/\1/')
cat > $B/win/payload.h <<EOF
#define RB_VER_MAJOR $MAJOR
#define RB_VER_MINOR $MINOR
#define RB_VERSION_STR "$VERSION"
EOF
cp $B/game-windows/icon/game.ico $B/win/builder.ico
x86_64-w64-mingw32-windres -I$B/win -Ibuilder/src/win builder/src/win/builder.rc -O coff -o $B/win/builder.res.o
rm -rf $DIST/StuntRaceFXBuilder-*-Windows.* "$B/win/Stunt Race FX Builder"
mkdir -p "$B/win/Stunt Race FX Builder/game"
OBJS=
for s in builder/src/core/*.c builder/src/win/main.c; do
    o=$B/win/$(basename $s .c).o
    x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -DUNICODE -D_UNICODE -DRB_VERSION="\"$VERSION\"" -Ibuilder/src/core -I$B/gen \
        -c $s -o $o
    OBJS="$OBJS $o"
done
x86_64-w64-mingw32-gcc -municode -mwindows -static -s -o "$B/win/Stunt Race FX Builder/StuntRaceFXBuilder.exe" \
    $OBJS $B/win/builder.res.o -lcomctl32 -lole32 -lshell32 -luuid
# the game beside it as a plain file: the builder copies it, it carries no
# executable inside itself
cp $B/payload-windows/stuntracefx.exe "$B/win/Stunt Race FX Builder/game/"
(cd $B/win && zip -qrX "$DIST/StuntRaceFXBuilder-$VERSION-Windows.zip" "Stunt Race FX Builder")

git worktree remove --force "$TREE"
echo $REV > $DIST/REV          # the source these zips are from (release.sh --no-build)
ls -la $DIST
