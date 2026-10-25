#!/bin/sh
# Build and run one of the port's checks against the decomp.
#   check/check.sh NAME [N [DIR]]  NAME_check.c (DIR: recorded calls), against the decomp files listed
#                                in its first line ("old: path ..."); its second line may give
#                                link flags ("ldflags: ...")
# The decomp's files are built with every symbol renamed old_*; a routine
# they call that is not among them gets a stub that stops the check (a case
# that reaches one is out of the check's scope). The port's side is every C
# file of ports/common/game. Output under build/port/check/NAME.
#   env: CC (default cc), SRFX_ROM (default reference/stuntracefx/...)
set -e
cd "$(dirname "$0")/../../../.."
NAME=$1
CC=${CC:-cc}
ROM=${SRFX_ROM:-reference/stuntracefx/Stunt Race FX (USA) (Rev 1).sfc}
G=ports/common/game
OUT=build/port/check/$NAME
mkdir -p "$OUT"
CFLAGS="-O2 -std=gnu11 -fno-strict-aliasing"
OLD=""
for f in $(sed -n '1s/^.*old: *//p' $G/check/${NAME}_check.c | sed 's#\*/##'); do
  o="$OUT/old_$(echo "$f" | tr / _ | sed 's/\.c$//').o"
  $CC $CFLAGS -w -Isrc/stuntracefx/include -c "src/stuntracefx/$f" -o "$o"
  OLD="$OLD $o"
done
nm $OLD | awk '$1 ~ /^[UTDBRC]$/ {print $2} NF==3 && $2 ~ /^[TDBRC]$/ {print $3}' | sort -u |
  grep -vE '^(memcpy|memset|memmove|memcmp|abort|_GLOBAL_OFFSET_TABLE_|__stack_chk_fail|__stack_chk_guard)$' |
  awk '{print $1, "old_" $1}' > "$OUT/rename"
for o in $OLD; do objcopy --redefine-syms="$OUT/rename" "$o"; done
# stubs for what the decomp's files call outside themselves, but the model's
DEF=$(nm $OLD | awk 'NF==3 && $2 ~ /^[TDBRC]$/ {print $3}' | sort -u)
{ echo '#include <stdio.h>'; echo '#include <stdlib.h>'
  nm -u $OLD | awk '{print $2}' | sort -u | grep '^old_' | while read s; do
    case "$s" in old_rd8|old_wr8|old_cyc|old_cpu_idle|old_cpu|old_gsu|old_gsu_*) continue;; esac
    echo "$DEF" | grep -qx "$s" && continue
    echo "void $s(void) { fprintf(stderr, \"check: the decomp's code called $s, outside the check\\\\n\"); exit(2); }"
  done; } > "$OUT/stubs.c"
NEW=""
for f in $(find $G -name '*.c' -not -path "$G/check/*" | sort); do
  o="$OUT/$(echo "$f" | tr / _ | sed 's/\.c$//').o"
  $CC $CFLAGS -Wall -Wextra -I$G/include -c "$f" -o "$o"
  NEW="$NEW $o"
done
$CC $CFLAGS -w -Isrc/stuntracefx/include -I$G/include -I$G/check -Itools/stuntracefx/sfxbox -c $G/check/model.c -o "$OUT/model.o"
$CC $CFLAGS -w -Itools/stuntracefx/sfxbox -c tools/stuntracefx/sfxbox/gsu_clock.c -o "$OUT/gsu_clock.o"
$CC $CFLAGS -Wall -Isrc/stuntracefx/include -I$G/include -I$G/check $G/check/${NAME}_check.c \
  "$OUT/model.o" "$OUT/gsu_clock.o" "$OUT/stubs.c" $NEW $OLD -o "$OUT/${NAME}_check" -lm $(sed -n '2s/^.*ldflags: *//p' $G/check/${NAME}_check.c)
"$OUT/${NAME}_check" "$ROM" ${2:-20000} ${3:-}
