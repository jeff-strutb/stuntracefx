#!/usr/bin/env python3
"""rmwsites.py: the C's read-modify-writes, found by their time.

    rmwsites.py [GAME] [--apply] [--list]

GAME is the game's C (default ports/stuntracefx/game). A read-modify-write
instruction (ASL, LSR, ROL, ROR, INC, DEC, TSB, TRB on memory) is one
operation on the console (nativeshvc's machine.h): the C makes its read
with rd8_rmw or rd16_rmw and its write with wr8 or wr16_rmw, the word's
high byte first.

The C states each instruction's time between its accesses (cyc), so an
RMW is a read and then a write of the same address with only its internal
cycle between: cyc(6) (or cyc_then) for a byte; cyc(14) for a word, as the decomp's
verification harness timed it (it took the C's word write, low byte first,
at the original's low byte, 8 clocks after the high one, and gave the high
byte no time). No other instruction pair reads and writes one address that
close: a load and store take their opcode and operand fetches between
(24 clocks or more).

--apply rewrites each site: rd8 -> rd8_rmw; rd16 -> rd16_rmw, cyc(14) ->
cyc(6), wr16 -> wr16_rmw (wr16_rmw makes both writes, 8 clocks each, so the
cycle between is the internal one alone).

The report counts the sites, and (--list) names each, with the file's
read-modify-write instructions in the assembly (asm/stuntracefx, the same
path) beside the sites found there: a file whose counts differ has
read-modify-writes written some other way (a helper's callers count once
per call).
"""
import collections, glob, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cycasm  # noqa: E402

ROOT = cycasm.ROOT
CALLS = re.compile(r'(?<![\w.])(rd8|rd16|rd24|wr8|wr16|wr24|rd8_rmw|rd16_rmw|wr16_rmw|op_lda16|op_lda8|'
                   r'op_inc16_mem|put8|cyc|cyc_then|cycs|cycs_then)\s*\(')
TIME = ('cyc', 'cyc_then', 'cycs', 'cycs_then')
FDEF = re.compile(r'^[A-Za-z_][\w \*]*?\b(\w+)\s*\(([^;{]*)\)\s*\{', re.M)


def calls(text):
    """Every access and time call: (end, start, name, first argument,
    its span), in the order made (a call in another's arguments first)."""
    out = []
    for m in CALLS.finditer(text):
        o = text.index('(', m.start())
        c = cycasm.close(text, o)
        args = text[o + 1:c]
        depth, cut = 0, len(args)
        for i, ch in enumerate(args):
            depth += {'(': 1, ')': -1}.get(ch, 0)
            if ch == ',' and depth == 0:
                cut = i
                break
        first = re.sub(r'\s+', '', args[:cut])
        # an address worked out after time stated before it: cyc_then(n, address)
        w = re.fullmatch(r'cyc_then\(\d+,(.*)\)', first)
        out.append((c + 1, m.start(), m.group(1), w.group(1) if w else first, (o + 1, o + 1 + cut)))
    return sorted(out)


def sites_in(text):
    """(read call, time calls, write call) of each read-modify-write."""
    cs = calls(text)
    out = []
    for k, r in enumerate(cs):
        if r[2] not in ('rd8', 'rd16'):
            continue
        j, times = k + 1, []
        while j < len(cs) and cs[j][2] in TIME:
            times.append(cs[j])
            j += 1
        if j >= len(cs) or len(times) != 1 or times[0][2] not in ('cyc', 'cyc_then'):
            continue
        w = cs[j]
        want = ('wr8', '6') if r[2] == 'rd8' else ('wr16', '14')
        if (w[2], times[0][3]) != want or w[3] != r[3]:
            continue
        if re.search(r'\b(if|else|while|for|do|return|switch|case|goto)\b|[{};]\s*\w+\s*:', text[r[0]:w[1]]):
            continue
        out.append((r, times[0], w))
    return out


def asm_counts():
    routines, _bad = cycasm.read_asm()
    n, seen = collections.Counter(), set()
    for ins in routines.values():
        for x in ins:
            if x.mnem in cycasm.RMW and cycasm.data_key(x) is not None and x.line not in seen:
                seen.add(x.line)
                n[x.line.split(':')[0][len('asm/stuntracefx/'):-len('.asm')]] += 1
    return n


def uses(text, s):
    """How many times the site at s counts: once, or, in a function of
    nothing but it (a helper), once per call of that function."""
    fn = None
    for m in FDEF.finditer(text):
        if m.start() < s:
            fn = m
    if not fn:
        return 1
    b = fn.end() - 1
    j = cycasm.close(text.replace('{', '(').replace('}', ')'), b)
    if len(re.findall(r'(?<![\w.])(rd8|rd16|wr8|wr16)\s*\(', text[b:j])) != 2:
        return 1
    n = len(re.findall(r'(?<![\w.])' + fn.group(1) + r'\s*\(', text)) - 1
    return max(n, 1)


def main(argv):
    apply = '--apply' in argv
    listing = '--list' in argv
    args = [a for a in argv[1:] if not a.startswith('--')]
    game = args[0] if args else os.path.join('ports', 'stuntracefx', 'game')
    found = collections.Counter()
    total, words, edits = 0, 0, {}
    for path in sorted(glob.glob(os.path.join(ROOT, game, '**', '*.c'), recursive=True)):
        rel = os.path.relpath(path, os.path.join(ROOT, game))[:-2]
        text = open(path).read()
        ss = sites_in(text)
        for r, t, w in ss:
            found[rel] += uses(text, r[1])
            total += 1
            words += r[2] == 'rd16'
            if listing:
                print('site %s:%d %s' % (os.path.relpath(path, ROOT), text.count('\n', 0, r[1]) + 1, r[2]))
        if apply and ss:
            ed = []
            for r, t, w in ss:
                ed.append((r[1], r[2], r[2] + '_rmw'))
                if r[2] == 'rd16':
                    ed.append((t[4][0], '14', '6'))
                    ed.append((w[1], 'wr16', 'wr16_rmw'))
            for at, old, new in sorted(ed, reverse=True):
                assert text.startswith(old, at), (path, at, old)
                text = text[:at] + new + text[at + len(old):]
            open(path, 'w').write(text)
            edits[path] = len(ss)
    asm = asm_counts()
    print('sites: %d (bytes %d, words %d); read-modify-writes they stand for: %d; in the assembly: %d'
          % (total, total - words, words, sum(found.values()), sum(asm.values())))
    for f in sorted(set(asm) | set(found)):
        if asm[f] != found[f]:
            print('  %-28s assembly %3d  C %3d' % (f, asm[f], found[f]))
    if apply:
        print('applied: %d sites in %d files' % (sum(edits.values()), len(edits)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
