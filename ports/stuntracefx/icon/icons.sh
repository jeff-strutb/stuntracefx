#!/bin/sh
# icons.sh OUT: the game's icon (icon.jpg, the box art's title, 800x800) in
# each platform's form, under OUT:
#   AppIcon.icns     macOS (CFBundleIconFile AppIcon)
#   game.ico         Windows (game.rc's icon 1, which nativeshvc's window
#                    class loads); PNG entries at 16 to 256
#   icon-256.png     a plain PNG (a desktop file's Icon=)
#   ios/Assets.car   iOS, compiled by actool (IOS_SDK: iphonesimulator, the
#                    default, or iphoneos), with ios/partial.plist, the keys
#                    to merge into the app's Info.plist
# Needs macOS's sips and iconutil; iOS needs Xcode's actool.
set -e
OUT=$1
[ -n "$OUT" ] || { echo "icons: OUT?" >&2; exit 2; }
SRC="$(cd "$(dirname "$0")" && pwd)/icon.jpg"
mkdir -p "$OUT/png"
png() {   # size: OUT/png/SIZE.png, square, RGB (no alpha: iOS refuses one)
  [ -f "$OUT/png/$1.png" ] && [ ! "$SRC" -nt "$OUT/png/$1.png" ] && return
  sips -s format png -z "$1" "$1" "$SRC" --out "$OUT/png/$1.png" >/dev/null
}
for s in 16 32 48 64 128 256 512 1024; do png $s; done

# macOS: an iconset (each size at 1x and 2x) to .icns
SET="$OUT/AppIcon.iconset"
rm -rf "$SET"; mkdir -p "$SET"
for s in 16 32 128 256 512; do
  cp "$OUT/png/$s.png" "$SET/icon_${s}x$s.png"
  cp "$OUT/png/$((s * 2)).png" "$SET/icon_${s}x$s@2x.png"
done
iconutil -c icns "$SET" -o "$OUT/AppIcon.icns"
rm -rf "$SET"

# Windows: an ICONDIR of PNG entries (Vista on); 256 is written as 0
python3 - "$OUT/game.ico" "$OUT"/png/16.png "$OUT"/png/32.png "$OUT"/png/48.png "$OUT"/png/64.png "$OUT"/png/128.png "$OUT"/png/256.png <<'EOPY'
import struct, sys
out, pngs = sys.argv[1], [open(p, 'rb').read() for p in sys.argv[2:]]
head = struct.pack('<HHH', 0, 1, len(pngs))
off, dirs = 6 + 16 * len(pngs), b''
for d in pngs:
    w, h = struct.unpack('>II', d[16:24])
    dirs += struct.pack('<BBBBHHII', w & 255, h & 255, 0, 0, 1, 32, len(d), off)
    off += len(d)
open(out, 'wb').write(head + dirs + b''.join(pngs))
EOPY
cp "$OUT/png/256.png" "$OUT/icon-256.png"

# iOS: a one-size (1024) app icon catalog; actool makes the sizes
if [ "$(uname)" = Darwin ] && xcrun --find actool >/dev/null 2>&1; then
  SDK=${IOS_SDK:-iphonesimulator}
  CAT="$OUT/ios/Assets.xcassets"
  rm -rf "$OUT/ios"; mkdir -p "$CAT/AppIcon.appiconset"
  echo '{"info":{"version":1,"author":"xcode"}}' > "$CAT/Contents.json"
  cp "$OUT/png/1024.png" "$CAT/AppIcon.appiconset/icon-1024.png"
  cat > "$CAT/AppIcon.appiconset/Contents.json" <<'EOJ'
{"images":[{"filename":"icon-1024.png","idiom":"universal","platform":"ios","size":"1024x1024"}],
 "info":{"version":1,"author":"xcode"}}
EOJ
  xcrun actool --compile "$OUT/ios" --platform "$SDK" --minimum-deployment-target 15.0 \
    --target-device iphone --target-device ipad --app-icon AppIcon \
    --output-partial-info-plist "$OUT/ios/partial.plist" "$CAT" >/dev/null
fi
echo "icons: $OUT"
