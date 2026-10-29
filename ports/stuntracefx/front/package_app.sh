#!/bin/sh
# Stunt Race FX for macOS on nativeshvc, as build/stuntracefx/front/macos/
# Stunt Race FX.app: the front end (front/build.sh) on nativeshvc's macOS
# host, the game's data built in, an Info.plist, the icon (icon/icons.sh).
#   NSHVC        the console's checkout (../nativeshvc); its macOS library is
#                built at NSHVC/build/macos if it is not there
# The app is signed ad hoc.
set -e
cd "$(dirname "$0")/../../.."
NSHVC=${NSHVC:-../nativeshvc}
NB=build/macos
if [ ! -f "$NSHVC/$NB/libnshvc.a" ]; then
  cmake -S "$NSHVC" -B "$NSHVC/$NB" -DNSHVC_HOST=macos -DNSHVC_RENDER=metal >/dev/null
fi
cmake --build "$NSHVC/$NB" >/dev/null
OUT=build/stuntracefx/front/macos
GPU_METAL=1 NSHVC="$NSHVC" NSHVC_BUILD=$NB OUT=$OUT \
  LIBS="-framework Cocoa -framework QuartzCore -framework AudioToolbox -framework CoreAudio -framework GameController -framework Metal -framework Foundation" \
  sh ports/stuntracefx/front/build.sh
APP="$OUT/Stunt Race FX.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$OUT/stuntracefx" "$APP/Contents/MacOS/Stunt Race FX"
sh ports/stuntracefx/icon/icons.sh "$OUT/icon" >/dev/null
cp "$OUT/icon/AppIcon.icns" "$APP/Contents/Resources/"
cat > "$APP/Contents/Info.plist" <<'EOP'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Stunt Race FX</string>
  <key>CFBundleDisplayName</key><string>Stunt Race FX</string>
  <key>CFBundleExecutable</key><string>Stunt Race FX</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundleIdentifier</key><string>com.strutb.stuntracefx</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict>
</plist>
EOP
printf 'APPL????' > "$APP/Contents/PkgInfo"
codesign --force --sign - "$APP"
echo "package_app: $APP"
