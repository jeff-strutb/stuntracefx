#!/usr/bin/env python3
"""check_payload.py -- prove the game executables the builder ships carry none
of the cartridge's data.

A release payload is built with the data left out (front/build.sh
ASSETS_DATA=file): the code is ours, the bytes come from the player's ROM at
build time (romdata.bin). This samples the ROM's assets (assets.csv: every
byte the game reads) in 32-byte windows, keeps the windows with enough
distinct bytes to be meaningful, and fails if any binary window appears in
an executable. A window that is text (printable ASCII and NULs) is reported
but does not fail.

    check_payload.py ROM EXE...
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, 'ports', 'stuntracefx', 'assets.csv')
WIN = 32
STEP = 64


def windows(rom):
    for r in csv.DictReader(open(MANIFEST)):
        lo, hi = int(r['start'], 16), int(r['end'], 16)
        for o in range(lo, hi - WIN + 1, STEP):
            w = rom[o:o + WIN]
            if len(set(w)) >= 12:
                yield o, w


def is_text(w):
    return all(32 <= b < 127 or b in (0, 9, 10, 13) for b in w)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    rom = open(sys.argv[1], 'rb').read()
    sample = list(windows(rom))
    bad = 0
    for path in sys.argv[2:]:
        body = open(path, 'rb').read()
        hits = text = 0
        for o, w in sample:
            if w in body:
                if is_text(w):
                    text += 1
                    continue
                hits += 1
                if hits <= 3:
                    print('%s: ROM bytes at $%06X found' % (os.path.basename(path), o))
        print('%s: %d of %d windows of the ROM\'s assets found (and %d text windows)'
              % (os.path.basename(path), hits, len(sample), text))
        bad += hits
    if bad:
        sys.exit('check_payload: game data found in the payload')
    print('check_payload: clean')


if __name__ == '__main__':
    main()
