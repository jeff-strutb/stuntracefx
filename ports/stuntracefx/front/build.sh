#!/bin/sh
# Stunt Race FX on nativeshvc: the game (src/stuntracefx), its
# generated tables and assets, and this front end, linked with libnshvc.
#
#   sh ports/stuntracefx/front/build.sh [check]
#     NSHVC=../nativeshvc         the console's checkout
#     NSHVC_BUILD=build/null      its build (cmake -B ... -DNSHVC_HOST=...)
#     OUT=build/stuntracefx/front/null
#     GAME=src/stuntracefx        the game's C (or a copy of it: cycgame.py's)
#     ASSETS=                     a variant of the assets (tools/assets.py: ios)
#     ASSETS_DATA=file            no asset bytes built in: read from romdata.bin (the
#                                 builder's payload; tools/assets.py, cart.c)
#     CC=clang                    the compiler (with its target, for iOS)
#     DEFS=                       the host's defines (-DNSHVC_HOST_IOS=1 for iOS)
#     LIBS=                       the host's frameworks and libraries
#   check: the touch scheme's check (touch_check.c) only
#
# The game's machine headers are its own; its Super FX (ports/common/snes/
# superfx.c) is the cartridge's, on nativeshvc's connector
# (front/superfx_cart.c).
set -e
cd "$(dirname "$0")/../../.."
CC=${CC:-clang}
NSHVC=${NSHVC:-../nativeshvc}
NB=${NSHVC_BUILD:-build/null}
OUT=${OUT:-build/stuntracefx/front/$(basename "$NB")}
GAME=${GAME:-src/stuntracefx}
FRONT=ports/stuntracefx/front
ROM="reference/stuntracefx/Stunt Race FX (USA) (Rev 1).sfc"
mkdir -p "$OUT/gen" "$OUT/obj" "$OUT/include"

if [ "$1" = check ]; then
  clang -std=gnu11 -Wall -Wextra -I"$FRONT" -I"$NSHVC/include" -o "$OUT/touch_check" \
    "$FRONT/touch_check.c" "$FRONT/touch.c" "$FRONT/art.c" -lm
  "$OUT/touch_check"
  exit
fi

cp "$GAME"/include/*.h "$OUT/include/"
SRFX_GAME="$GAME" python3 ports/stuntracefx/tools/routines.py "$ROM" "$OUT/gen/routines.c" >/dev/null
ASSETS_DATA=$ASSETS_DATA python3 ports/stuntracefx/tools/assets.py "$ROM" "$OUT/gen" $ASSETS >/dev/null
OBJS=
# the newest of the game's headers: an object older than it is built again
HDR=$(ls -t "$GAME"/include/*.h | head -1)
for f in $(find "$GAME" -name '*.c' | sort) "$OUT/gen/routines.c" "$OUT/gen/assets.c"; do
  o="$OUT/obj/$(echo "$f" | sed 's#[/.]#_#g').o"
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$HDR" -nt "$o" ]; then
    $CC -O2 -std=gnu11 -fno-strict-aliasing -ffp-contract=off -w \
      -I"$OUT/include" -I"$OUT/gen" -I"$FRONT" -I"$NSHVC/include" -c "$f" -o "$o"
  fi
  OBJS="$OBJS $o"
done
ASSETS_O="$OUT/obj/assets_S.o"
if [ "$ASSETS_DATA" = file ]; then
  DEFS="$DEFS -DSRFX_ROMDATA_FILE=1"
  case "$($CC -dumpmachine)" in *apple*) LIBS="$LIBS -framework CoreFoundation";; esac   # (the app's Resources)
  ASSETS_O=
  rm -f "$OUT/obj/assets_S.o"
else
  $CC -c "$OUT/gen/assets.S" -I"$OUT/gen" -o "$ASSETS_O"
fi
# GPU_METAL=1 (the macOS and iOS apps): the full window's picture filled by
# the cartridge's GPU (gpu_metal.m)
if [ -n "$GPU_METAL" ]; then
  DEFS="$DEFS -DFW_GPU_METAL=1"
  $CC -O2 -fobjc-arc -Wall $DEFS -I"$FRONT" -I"$OUT/include" -I"$NSHVC/include" -c "$FRONT/gpu_metal.m" -o "$OUT/obj/front_gpu_metal.o"
  OBJS="$OBJS $OUT/obj/front_gpu_metal.o"
fi
for f in main cart superfx_cart touch art cost_profile; do
  $CC -O2 -std=gnu11 -Wall $DEFS -I"$FRONT" -I"$OUT/include" -I"$NSHVC/include" -Iports/common/snes -c "$FRONT/$f.c" -o "$OUT/obj/front_$f.o"
  OBJS="$OBJS $OUT/obj/front_$f.o"
done
# the cartridge's Super FX
for f in superfx; do
  $CC -O2 -std=gnu11 -Wall $DEFS -Iports/common/snes -c "ports/common/snes/$f.c" -o "$OUT/obj/chip_$f.o"
  OBJS="$OBJS $OUT/obj/chip_$f.o"
done
# a Windows build: its name, version and icon (nativeshvc's window class
# loads icon 1)
EXE=
case "$($CC -dumpmachine)" in
  *mingw*|*windows*)
    sh ports/stuntracefx/icon/icons.sh "$OUT/icon" >/dev/null
    ICO="$OUT/icon/game.ico" sh ports/common/host/windows/game_rc.sh "Stunt Race FX" "Stunt Race FX.exe" "$OUT/obj/game_res.o"
    OBJS="$OBJS $OUT/obj/game_res.o"; EXE=.exe;;
esac
$CC -o "$OUT/stuntracefx$EXE" $OBJS $ASSETS_O "$NSHVC/$NB/libnshvc.a" -lm $LIBS
echo "built $OUT/stuntracefx$EXE"
