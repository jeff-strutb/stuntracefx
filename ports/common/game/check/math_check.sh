#!/bin/sh
# The port's maths against the decomp's: builds build/port/check/math_check
# from ports/common/game/math and src/stuntracefx/math (its symbols renamed
# old_*), then runs it on the player's ROM.
#   env: CC (default cc), SRFX_ROM (default reference/stuntracefx/...), N
set -e
cd "$(dirname "$0")/../../../.."
CC=${CC:-cc}
ROM=${SRFX_ROM:-reference/stuntracefx/Stunt Race FX (USA) (Rev 1).sfc}
OUT=build/port/check/math
mkdir -p "$OUT"
CFLAGS="-O2 -std=gnu11 -fno-strict-aliasing -Wall"
OLD=""
for f in src/stuntracefx/math/*.c; do
  o="$OUT/old_$(basename "$f" .c).o"
  $CC $CFLAGS -w -Isrc/stuntracefx/include -c "$f" -o "$o"
  OLD="$OLD $o"
done
# every global the decomp's maths defines or calls, renamed old_*, but the C library's
nm $OLD | awk '$1 ~ /^[UTDBRC]$/ {print $2} NF==3 && $2 ~ /^[TDBRC]$/ {print $3}' | sort -u |
  grep -vE '^(memcpy|memset|memmove|abort|_GLOBAL_OFFSET_TABLE_|__stack_chk_fail|__stack_chk_guard)$' | awk '{print $1, "old_" $1}' > "$OUT/rename"
for o in $OLD; do objcopy --redefine-syms="$OUT/rename" "$o"; done
NEW=""
for f in ports/common/game/math/*.c; do
  o="$OUT/$(basename "$f" .c).o"
  $CC $CFLAGS -Wextra -Iports/common/game/include -c "$f" -o "$o"
  NEW="$NEW $o"
done
$CC $CFLAGS -Isrc/stuntracefx/include -Iports/common/game/include ports/common/game/check/math_check.c $NEW $OLD -o "$OUT/math_check"
"$OUT/math_check" "$ROM" ${N:-200000}
