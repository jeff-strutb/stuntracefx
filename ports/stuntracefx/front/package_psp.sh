#!/bin/sh
# Stunt Race FX for the PSP on nativeshvc, as build/stuntracefx/front/psp/
# PSP/GAME/STUNTRACEFX/EBOOT.PBP (the folder for a memory stick's root): the
# front end (front/build.sh) built with pspdev on nativeshvc's PSP host and
# GE renderer, the game's data built in, its Enhanced 3D filled on the GE
# (gpu_ge.c).
#   PSPDEV   pspdev's install (psp-gcc in $PSPDEV/bin)
#   NSHVC    the console's checkout (../nativeshvc); its library for the PSP
#            is built at NSHVC/build/psp if it is not there
# The program's imports are fixed up (psp-fixup-imports) as pspsdk's own
# build does; one it cannot fix (its warning) fails the build. MEMSIZE 1:
# all of a PSP-2000 or later's memory, which the console's machine needs.
#
# PSP_CHECK=1 (a check): build/stuntracefx/front/psp-check/check.elf
# instead, which takes its command line from args.txt beside it, an
# argument a line (nativeshvc's tests/psp/args.c), for a headless play in
# PPSSPP's headless runner (--headless --script ... --out ...), to compare
# with the same play on the desktop's build.
set -e
cd "$(dirname "$0")/../../.."
[ -n "$PSPDEV" ] || { echo "package_psp: set PSPDEV (pspdev's install)" >&2; exit 2; }
NSHVC=${NSHVC:-../nativeshvc}
NB=build/psp
if [ ! -f "$NSHVC/$NB/libnshvc.a" ]; then
  cmake -S "$NSHVC" -B "$NSHVC/$NB" -DCMAKE_TOOLCHAIN_FILE="$PSPDEV/psp/share/pspdev.cmake" \
    -DNSHVC_HOST=psp -DCMAKE_BUILD_TYPE=Release >/dev/null
fi
cmake --build "$NSHVC/$NB" >/dev/null
P=$PSPDEV
CC="$P/bin/psp-gcc -I$P/psp/include -I$P/psp/sdk/include -DPSP -D__PSP__ -D_PSP_FW_VERSION=600"
OUT=build/stuntracefx/front/psp
CHECK_DEFS= CHECK_O=
if [ -n "$PSP_CHECK" ]; then
  OUT=build/stuntracefx/front/psp-check
  mkdir -p "$OUT/obj"
  $CC -O2 -c "$NSHVC/tests/psp/args.c" -o "$OUT/obj/args.o"
  CHECK_DEFS=-Dmain=nshvc_test_main CHECK_O="$OUT/obj/args.o"
fi
GPU_GE=1 NSHVC="$NSHVC" NSHVC_BUILD=$NB OUT=$OUT CC="$CC" DEFS="$CHECK_DEFS" \
  LIBS="$CHECK_O -L$P/lib -L$P/psp/lib -L$P/psp/sdk/lib -Wl,-zmax-page-size=128 -lpspgu -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspaudio -lpsppower -lm" \
  sh ports/stuntracefx/front/build.sh
ELF=$OUT/stuntracefx.elf
cp "$OUT/stuntracefx" "$ELF"
"$P/bin/psp-strip" "$ELF"
fix=$("$P/bin/psp-fixup-imports" "$ELF" 2>&1) || { echo "$fix" >&2; exit 1; }
case "$fix" in *Warning*) echo "$fix" >&2; rm -f "$ELF"; exit 1;; esac
if [ -n "$PSP_CHECK" ]; then
  mv "$ELF" "$OUT/check.elf"
  echo "built $OUT/check.elf"
  exit
fi
DIR=$OUT/PSP/GAME/STUNTRACEFX
mkdir -p "$DIR"
"$P/bin/mksfoex" -d MEMSIZE=1 -s APP_VER=01.00 "Stunt Race FX" "$OUT/PARAM.SFO"
# the game's icon (icon/icon.jpg, the box art, 800x800) as the menu's
# pictures: ICON0 144x80 and PIC1 480x272, each the top of the art cut to
# its shape (the title and the cars), by ffmpeg
ICON=ports/stuntracefx/icon/icon.jpg
ffmpeg -v error -y -i "$ICON" -vf "crop=800:444:0:0,scale=144:80:flags=lanczos" "$OUT/ICON0.PNG"
ffmpeg -v error -y -i "$ICON" -vf "crop=800:453:0:0,scale=480:272:flags=lanczos" "$OUT/PIC1.PNG"
"$P/bin/pack-pbp" "$DIR/EBOOT.PBP" "$OUT/PARAM.SFO" "$OUT/ICON0.PNG" NULL NULL "$OUT/PIC1.PNG" NULL "$ELF" NULL >/dev/null
echo "built $DIR/EBOOT.PBP"
