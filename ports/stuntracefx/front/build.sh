#!/bin/sh
# Stunt Race FX on nativeshvc: the game (ports/stuntracefx/game), its
# generated tables and assets, and this front end, linked with libnshvc.
#
#   sh ports/stuntracefx/front/build.sh [check]
#     NSHVC=../nativeshvc         the console's checkout
#     NSHVC_BUILD=build/null      its build (cmake -B ... -DNSHVC_HOST=...)
#     OUT=build/stuntracefx/front/null
#     ASSETS=                     a variant of the assets (tools/assets.py: ios)
#   check: the touch scheme's check (touch_check.c) only
#
# The game's machine headers are its own but gsu.h, which is the console's
# (the register files' layout is nativeshvc's).
set -e
cd "$(dirname "$0")/../../.."
NSHVC=${NSHVC:-../nativeshvc}
NB=${NSHVC_BUILD:-build/null}
OUT=${OUT:-build/stuntracefx/front/$(basename "$NB")}
GAME=ports/stuntracefx/game
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
cp "$NSHVC/include/nshvc/gsu.h" "$OUT/include/gsu.h"
SRFX_GAME="$GAME" python3 ports/stuntracefx/tools/routines.py "$ROM" "$OUT/gen/routines.c" >/dev/null
python3 ports/stuntracefx/tools/assets.py "$ROM" "$OUT/gen" $ASSETS >/dev/null
OBJS=
for f in $(find "$GAME" -name '*.c' | sort) "$OUT/gen/routines.c" "$OUT/gen/assets.c"; do
  o="$OUT/obj/$(echo "$f" | sed 's#[/.]#_#g').o"
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$OUT/include/gsu.h" -nt "$o" ]; then
    clang -O2 -std=gnu11 -fno-strict-aliasing -ffp-contract=off -w \
      -I"$OUT/include" -I"$OUT/gen" -I"$FRONT" -I"$NSHVC/include" -c "$f" -o "$o"
  fi
  OBJS="$OBJS $o"
done
clang -c "$OUT/gen/assets.S" -I"$OUT/gen" -o "$OUT/obj/assets_S.o"
for f in main cart touch art; do
  clang -O2 -std=gnu11 -Wall -I"$FRONT" -I"$OUT/include" -I"$NSHVC/include" -c "$FRONT/$f.c" -o "$OUT/obj/front_$f.o"
  OBJS="$OBJS $OUT/obj/front_$f.o"
done
clang -o "$OUT/stuntracefx" $OBJS "$OUT/obj/assets_S.o" "$NSHVC/$NB/libnshvc.a" -lm $LIBS
echo "built $OUT/stuntracefx"
