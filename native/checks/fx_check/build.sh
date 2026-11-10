#!/bin/sh
# build.sh OUT: builds fx_check from the game's Super FX logic.
set -e
top=$(cd "$(dirname "$0")/../../.." && pwd)
out=$1
src=$top/native/stuntracefx/src
cc -std=gnu11 -O2 -w -I"$src" -I"$top/native/checks/fx_check" -I"${NSHVC:-/Users/jeffreywilbur/projects/strutb/nativeshvc}/include" \
  "$top/native/checks/fx_check/fx_check.c" "$top/native/checks/fx_check/entries.c" "$top/native/checks/fx_check/stubs.c" "$top/native/checks/fx_check/stubs_engine.c" \
  $(find "$src/fx" -name '*.c') "$src/data/race_tables.c" "$src/data/unpack.c" "$src/data/fx_handlers.c" "$src/data/object_tables.c" -o "$out"
