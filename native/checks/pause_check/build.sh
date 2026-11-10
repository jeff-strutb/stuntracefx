#!/bin/sh
# build.sh OUT: the pause screen's art check, built from the game's own art files.
set -e
top=$(cd "$(dirname "$0")/../../.." && pwd)
p=$top/native/stuntracefx/src/pause
clang -std=gnu11 -O2 -g -Wall -I "$top/native/stuntracefx/src" -I "$NSHVC/include" -o "$1" \
  "$top/native/checks/pause_check/pause_check.c" "$p/pause_art.c" "$p/pad_art.c" -lm
