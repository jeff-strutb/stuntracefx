#!/bin/sh
# build.sh OUT: the sound comparison, built from the game's own driver files.
set -e
top=$(cd "$(dirname "$0")/../../.." && pwd)
a=$top/native/stuntracefx/src/audio
clang -std=gnu11 -O2 -g -Wall -Wno-unused-function -Wno-unused-result -I "$top/native/stuntracefx/src" -I "$NSHVC/include" -o "$1" \
  "$top/native/checks/music_check/music_check.c" "$a/driver_music.c" "$a/driver_race.c" "$a/driver_battle.c" "$a/music.c"
