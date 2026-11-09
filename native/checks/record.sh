#!/bin/sh
# record.sh PLAY OUTDIR: records one play of the original in bsnes for the checks.
# CORE (the patched core) and SFXBOX as built by build_tools.sh.
set -e
top=$(cd "$(dirname "$0")/../.." && pwd)
play=$1; out=$2
mkdir -p "$out"
: "${CORE:=$top/build/stuntracefx/native/bsnes_libretro.dylib}"
: "${SFXBOX:=$top/build/stuntracefx/native/sfxbox}"
: "${ROM:=$top/../../../reference/stuntracefx/Stunt Race FX (USA) (Rev 1).sfc}"
SFXBOX_SPCLOG="$out/spc.bin" "$SFXBOX" --rom "$ROM" --script "$top/tools/stuntracefx/plays/$play.sfx" --out "$out" --core "$CORE" \
  --native "$out" --pacing "$out/pacing.txt" --apulog "$out/apu.bin" --sound "$out/sound.raw"
