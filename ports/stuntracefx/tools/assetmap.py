#!/usr/bin/env python3
"""assetmap.py: decide the port's assets: every block of the ROM's data,
named, in ports/stuntracefx/assets.csv.

    assetmap.py [CENSUS...]

A ROM byte is code when the original only ever ran it (the decomp's traces:
build/stuntracefx/box/*/rommap.bin, through tools/stuntracefx/codemap.py)
and the port never reads it; the C stands for it and no asset holds it.
Every other byte is data: read by the original (by the S-CPU, DMA, the
Super FX or the SPC700's upload), read by the port, or never touched (so
content no trace reached still comes along). Data runs between code are cut
where the game's C addresses a table by a constant (ports/stuntracefx/game:
the table is named for the function that does) and where the C function
reading them changes (the CENSUS files the port writes under
PORT_ROMCENSUS, cart.h); each piece is named for its reader and its
address: MenuOamBuild_0B8B56. A piece neither read by the port nor named by
the C is named for how the original used it (cpu_, dma_, fx_ or spc_ data)
or unread_.
A piece whose range is already in assets.csv keeps the name it has there
(the game's C names assets: assets.h), so names change only by hand.
"""
import csv, glob, os, re, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'stuntracefx'))
import codemap  # noqa: E402

OUT = os.path.join(ROOT, 'ports', 'stuntracefx', 'assets.csv')
CPU_OP, DATA = 0x33, 0x4C | 0x4000
MIN_PIECE = 16          # a reader's piece shorter than this joins its neighbour
GAME = os.path.join(ROOT, 'ports', 'stuntracefx', 'game')


def c_tables():
    """{ROM offset: function} for each constant address in the game's C that
    is ROM data (banks $00-$3F, $8000 up), the function it is in; a table
    the C reaches at several offsets close together is one table, at the
    lowest"""
    lit = re.compile(r'\b0x([0-9A-Fa-f]{6})\b')
    head = re.compile(r'^[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;]*$')
    out = {}
    for path in sorted(glob.glob(os.path.join(GAME, '**', '*.c'), recursive=True)):
        fn = None
        for line in open(path):
            m = head.match(line)
            if m and not line.startswith(('if', 'for', 'while', 'switch', 'return', 'else')):
                fn = m.group(1)
            if line.lstrip().startswith(('*', '/*', '//')) or not fn:
                continue
            for m in lit.finditer(line):
                v = int(m.group(1), 16)
                if v >> 16 < 0x40 and v & 0xFFFF >= 0x8000:
                    out.setdefault(v >> 16 << 15 | (v & 0x7FFF), re.sub(r'^abi_', '', fn))
    return out


def addr(off):
    return '%02X%04X' % (off >> 15, 0x8000 | (off & 0x7FFF))


def main(census_files):
    flags = codemap.merged()
    n = len(flags)
    fl = np.array(flags, np.uint32)
    reader = [None] * n
    for path in census_files:
        for line in open(path):
            o, who, name = line.split()
            o, who = int(o, 16), int(who)
            if not who & 15:
                continue                           # only the ROM buffer's fill, never used
            if reader[o] is None and name != '?':
                reader[o] = re.sub(r'^abi_', '', name)
            fl[o] |= 0x10000                       # read by the port
    code = ((fl & CPU_OP) != 0) & ((fl & DATA) == 0) & ((fl & 0x10000) == 0)
    tables = {o: f for o, f in c_tables().items() if o < n and not code[o]}
    starts = sorted(tables)
    # offsets within MIN_PIECE of a lower one are fields of that table
    keep = []
    for o in starts:
        if not keep or o - keep[-1] >= MIN_PIECE:
            keep.append(o)
    tables = {o: tables[o] for o in keep}
    # how the original used a byte the port never read
    def kind(o):
        v = int(fl[o])
        if v & 0x4000 or v & 0x2000 or v & 0x80:
            return 'spc'
        if v & 0x40:
            return 'fx'
        if v & 0x08:
            return 'dma'
        if v & 0x04:
            return 'cpu'
        return 'unread'
    pieces = []
    o = 0
    while o < n:
        if code[o]:
            o += 1
            continue
        start = o
        while o < n and not code[o]:
            o += 1
        # one run of data: label each byte (the port's reader, else how the
        # original used it), cut where the label changes and at each table
        # the C names; a piece at a table is named for the C's function
        labels = [reader[k] or kind(k) for k in range(start, o)]
        segs = []
        for k, lab in enumerate(labels):
            at = start + k
            if segs and segs[-1][2] == lab and at not in tables:
                segs[-1][1] = at + 1
            else:
                segs.append([at, at + 1, lab, at in tables])
        # small pieces join the piece before (or after, at a run's start),
        # except at a table
        merged = []
        for s in segs:
            if merged and not s[3] and (s[1] - s[0] < MIN_PIECE or (merged[-1][1] - merged[-1][0] < MIN_PIECE
                                                                     and not merged[-1][3])):
                keep = merged[-1] if merged[-1][3] or merged[-1][1] - merged[-1][0] >= s[1] - s[0] else s
                merged[-1] = [merged[-1][0], s[1], keep[2], merged[-1][3]]
            else:
                merged.append(s)
        # and neighbours with one reader are one piece, unless the second
        # starts a table
        for s in merged:
            if pieces and pieces[-1][1] == s[0] and pieces[-1][2] == s[2] and not s[3]:
                pieces[-1][1] = s[1]
            else:
                pieces.append(s)
    pieces = [[s, e, tables[s] if t else lab] for s, e, lab, t in pieces]
    old = {}
    if os.path.exists(OUT):
        for r in csv.DictReader(open(OUT)):
            old[(r['start'], r['end'])] = r['name']
    with open(OUT, 'w', newline='') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(['name', 'start', 'end', 'reader'])
        for s, e, lab in pieces:
            key = ('%06X' % s, '%06X' % e)
            name = old.get(key, '%s_%s' % (re.sub(r'\W', '_', lab), addr(s)))
            w.writerow([name, key[0], key[1], lab])
    total = sum(e - s for s, e, _ in pieces)
    print('assetmap: %d assets, %d bytes of data; %d bytes of code left to the C' % (len(pieces), total, int(code.sum())))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
