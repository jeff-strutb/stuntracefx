#!/bin/sh
# game_rc.sh NAME EXE OUT_O: game.rc compiled for one game's Windows build,
# at VERSION (default 0.0), with the icon ICO (a .ico file) if it is set.
set -e
NAME=$1 EXE=$2 O=$3
V=${VERSION:-0.0}
D=$(dirname "$O")
mkdir -p "$D"
cat > "$D/game_rc.h" <<EOH
#define GAME_NAME "$NAME"
#define GAME_EXE "$EXE"
#define GAME_VERSION "$V"
#define GAME_VER_MAJOR ${V%%.*}
#define GAME_VER_MINOR $(echo "${V#*.}" | sed 's/^0*\([0-9]\)/\1/')
EOH
[ -n "$ICO" ] && echo "#define GAME_ICO \"$(cd "$(dirname "$ICO")" && pwd)/$(basename "$ICO")\"" >> "$D/game_rc.h"
x86_64-w64-mingw32-windres -I"$D" -I"$(dirname "$0")" "$(dirname "$0")/game.rc" -O coff -o "$O"
