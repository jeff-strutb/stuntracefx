#!/usr/bin/env python3
"""mkentries.py FX_SRC OUT: from the Super FX C as the generator wrote it, the
routines' entries by address, with their arguments taken from the registers
recorded as a job starts. For the check only."""
import glob, os, re, sys
src, out = sys.argv[1], sys.argv[2]
DEAD = {'render/unused.c', 'object/fx_unused.c', 'data/unpack.c'}
w = ['/* written by mkentries.py */', '#include "fx/fx.h"', '#include "fx_check.h"', '']
tab = []
for f in sorted(glob.glob(src + '/**/*.c', recursive=True)):
    rel = os.path.relpath(f, src)
    if rel in DEAD:
        continue
    s = open(f).read()
    for m in re.finditer(r'@routine ([0-9A-F]{2}):([0-9A-F]{4}) (\w+)(.*?)\nvoid abi_\3\(void\)\n(\{.*?\n\}\n)', s, re.S):
        bank, addr, name, between, body = m.groups()
        if '@routine' in between:
            continue
        body = re.sub(r'\bgsu_', 'fx_', body)
        body = re.sub(r'^[ \t]*(fx_code|fx_cache|fx_time)\([^;]*\);\n', '', body, flags=re.M)
        body = re.sub(r'^[ \t]*fx_(?:cycles_begin|mark|code_begin|cost)\w*\([^;]*\);\n', '', body, flags=re.M)
        w.append('static void entry_%s(void)\n%s' % (name, body))
        tab.append((int(bank, 16) << 16 | int(addr, 16), name))
w.append('const FxEntry fx_entries[] = {')
for a, n in sorted(tab):
    w.append('    {0x%06X, entry_%s, "%s"},' % (a, n, n))
w.append('    {0, 0, 0}\n};')
open(out, 'w').write('\n'.join(w) + '\n')
