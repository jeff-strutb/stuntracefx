#!/usr/bin/env python3
"""inputonly.py: the plays and scripts that reach the game through input alone.

    inputonly.py [--why]

A check of the decomp counts only runs that drive the game the way a player
does: nativeshvc's script steps wait, hold, press, repeat, shot, dump, save,
load, plug, pad, mouse and gun. A step that injects state or calls (poke,
set, call, fx, spcset, spccall) makes the run not count, and so does a load
of a save made by a run that does not count (a play's `save NAME`, which a
script's `load` of build/stuntracefx/states/NAME.state stands for).

Prints the names of the runs that count, one a line (--why: every run that
does not, with the step that rules it out).
"""
import glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(ROOT, 'tools', 'stuntracefx')
INPUT = {'wait', 'hold', 'press', 'repeat', 'shot', 'dump', 'save', 'load', 'plug', 'pad', 'mouse', 'gun', 'end'}


def steps(path):
    for line in open(path):
        s = line.split('#')[0].split()
        if s:
            yield s


def main(argv):
    files = sorted(glob.glob(os.path.join(TOOLS, 'plays', '*.sfx')) + glob.glob(os.path.join(TOOLS, 'scripts', '*.sfx')))
    saves, bad = {}, {}
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        for s in steps(f):
            if s[0] not in INPUT and name not in bad:
                bad[name] = 'step %s' % s[0]
            if s[0] == 'save' and len(s) > 1:
                saves[s[1]] = name
    # a load stands for the play that saved it
    changed = True
    while changed:
        changed = False
        for f in files:
            name = os.path.splitext(os.path.basename(f))[0]
            if name in bad:
                continue
            for s in steps(f):
                if s[0] == 'load' and len(s) > 1:
                    st = os.path.splitext(os.path.basename(s[1]))[0]
                    src = saves.get(st)
                    if src is None or src in bad:
                        bad[name] = 'loads %s (%s)' % (st, 'no play saves it' if src is None else 'saved by ' + src)
                        changed = True
                        break
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        if '--why' in argv:
            if name in bad:
                print('%-32s %s' % (name, bad[name]))
        elif name not in bad:
            print(name)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
