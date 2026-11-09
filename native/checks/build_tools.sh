#!/bin/sh
# build_tools.sh: sfxbox for the checks, and the core with patch 0014, into
# build/stuntracefx/native. Needs the toolchain's patched source in MAIN.
set -e
top=$(cd "$(dirname "$0")/../.." && pwd)
: "${MAIN:=$top/../../..}"
out=$top/build/stuntracefx/native
src=$MAIN/tools/toolchains/src/bsnes/bsnes
mkdir -p "$out"
cp "$out/core-src/bsnes/out/bsnes_libretro.dylib" "$out/"
cd "$top/tools/stuntracefx/sfxbox"
cc -O2 -Wall -include "$top/native/checks/gsu_marks.h" -I"$src/target-libretro" -I"$MAIN/src/stuntracefx/include" -Wl,-export_dynamic \
  -o "$out/sfxbox" sfxbox.c check.c gsucall.c spccall.c gsu_clock.c -ldl
