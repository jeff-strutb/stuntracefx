#!/usr/bin/env python3
"""cycassign.py: each C place's cycle list, from the recorded stretches.

    cycassign.py REC [STRETCHES...] [--out ASSIGN.csv]

REC is cycrecord.py's OUT (its tree/probes.csv and tree/timing.csv, and its
stretches.txt unless others are named). A stretch is the original's cycles
between two paired accesses and the C places passed in between, in order
(cycpair.c). Every place gets one list of cycles (6, 8, C for 12) so that
in every stretch the lists of the places passed, one after another, are
the original's cycles.

The places:

    p<id>            the code after a probe, up to the next mark: time
                     there is stated at the probe (cycwrite.py's places)
    t<id>            a timing call (timing.csv), when it states one time
    t<id>/<a>.<b>    a timing call on the path through the arms (the probes
                     inside its ?:) named: cyc(c ? 32 : 54) is two places
    t<id>=<n>        a timing call stating n clocks, when it states several
                     on one path (cyc(lead): a helper its callers pass
                     clocks to; cyc(2 * imm + 30)): one list per value

Time the console states (k, q tokens) is fixed: its cycles are spent where
they are. Where the C now states time (cyc, cycs) is the first guess: a
stretch whose places' clocks add up and fall on the original's cycle edges
splits there. Elsewhere (the C's time off, or an edge inside one of the
original's cycles: a mid-cycle split) each edge may move to the cycle
edges within 24 clocks of where the C's clocks put it, counted from either
end. Each place takes the list most stretches give it; then, round by
round, a place whose other choice meets more stretches (with every other
place as it is) takes that.

What no list can meet is named:

    merged      the C passes the same places in stretches the original
                spends differently: paths of the original the C does not
                tell apart. The place where they first part is named.
    conflict    stretches want different lists of the place (often a
                helper whose callers' time differs): the lists and how
                many stretches want each.
    open        in a stretch no list meets, though another of its
                places is the cause (or several are together).

ASSIGN.csv (REC/assign.csv by default): place, file, line, col, function,
what it is, the clocks the C states there now, the list, its clocks,
stretches through it, those met, status (exact, conflict, merged; a place
in no stretch is not listed: cycasm.py reads those) and the competing
lists. REC/assign.txt reports the totals and the worst places.
"""
import collections, csv, os, sys

CLK = {'6': 6, '8': 8, 'C': 12}
NEAR = 24            # how far (clocks) an edge may move from where the C's time puts it
CAP = 24             # the most splits of one stretch looked at
ROUNDS = 12


def clocks(lst):
    return sum(CLK[c] for c in lst)


def hexlist(h):
    """A cycs token's lengths as a list: 0x886 -> '886', 0xC8 -> 'C8'."""
    return h.upper().lstrip('0')


class Places:
    """The places' names and where they are."""

    def __init__(self, rec):
        self.probe = {}
        self.arm_of = {}
        with open(os.path.join(rec, 'tree', 'probes.csv')) as f:
            for r in csv.DictReader(f):
                i = int(r['id'])
                self.probe[i] = r
                if r.get('timing'):
                    self.arm_of[i] = int(r['timing'])
        self.timing = {}
        with open(os.path.join(rec, 'tree', 'timing.csv')) as f:
            for r in csv.DictReader(f):
                self.timing[int(r['id'])] = r

    def where(self, key):
        if key[0] == 'P':
            r = self.probe.get(key[1], {})
            return r.get('file', '?'), r.get('line', ''), r.get('col', ''), r.get('kind', ''), ''
        r = self.timing.get(key[1], {})
        return r.get('file', '?'), r.get('line', ''), r.get('col', ''), r.get('name', ''), r.get('fn', '')


def name(key):
    if key[0] == 'P':
        return 'p%d' % key[1]
    s = 't%d' % key[1]
    if key[2]:
        s += '/' + '.'.join(str(a) for a in key[2])
    if len(key) > 3:
        s += '=%s' % key[3]
    return s


def read(paths, places):
    """The stretches: [(count, O, items)], an item a place key with the
    clocks the C states there now, or ('K', n) / ('H', list) the console's.
    A timing call's key holds its value when one path of it states more
    than one."""
    raw = []
    values = collections.defaultdict(set)
    for p in paths:
        with open(p) as f:
            for line in f:
                if line[0] == '#':
                    continue
                c, o, toks = line.rstrip('\n').split('\t')
                items, arms = [], []
                for t in toks.split():
                    k = t[0]
                    if k == 'p':
                        i = int(t[1:])
                        if i in places.arm_of:
                            arms.append(i)
                        else:
                            items.append((('P', i), 0))
                    elif k in 'ts':
                        i, v = t[1:].split(':')
                        i = int(i)
                        mine = tuple(a for a in arms if places.arm_of.get(a) == i)
                        arms = []
                        v = int(v) if k == 't' else hexlist(v)
                        key = ('T', i, mine)
                        values[key].add(v)
                        items.append((key, v))
                    elif k == 'k':
                        items.append((('K', int(t[1:])), int(t[1:])))
                    elif k == 'q':
                        items.append((('H', hexlist(t[1:])), 0))
                raw.append((int(c), o, items))
    multi = {k for k, vs in values.items() if len(vs) > 1}
    out = []
    for c, o, items in raw:
        fixed = []
        for key, v in items:
            if key in multi:
                key = key + (v,)
            fixed.append((key, v))
        out.append((c, o, fixed))
    return out, multi


def now(key, v):
    """The clocks the C states at an item now."""
    if key[0] == 'P':
        return 0
    if key[0] == 'H':
        return clocks(key[1])
    return v if isinstance(v, int) else clocks(v)


def edges(o):
    """Each cycle edge of o by its clock: {clocks before it: index}."""
    e, c = {0: 0}, 0
    for i, ch in enumerate(o):
        c += CLK[ch]
        e[c] = i + 1
    return e, c


def fits(o, items, got):
    """Whether the places' lists in `got`, one after another, are o."""
    at = 0
    for key, _ in items:
        if key[0] == 'H':
            lst = key[1]
        elif key[0] == 'K':
            # the console's time: any cycles adding up to it
            e, c, n = at, 0, key[1]
            while c < n and e < len(o):
                c += CLK[o[e]]
                e += 1
            if c != n:
                return False
            at = e
            continue
        else:
            lst = got.get(key)
            if lst is None:
                return False
        if not o.startswith(lst, at):
            return False
        at += len(lst)
    return at == len(o)


def hint(o, items):
    """The split where the C's clocks put the edges, if they all fall on
    the original's: {place: list}, or None."""
    e, total = edges(o)
    c, prev, got = 0, 0, {}
    for key, v in items:
        c += now(key, v)
        i = e.get(c)
        if i is None:
            return None
        piece = o[prev:i]
        if key[0] == 'H' and piece != key[1]:
            return None
        if key[0] in ('P', 'T'):
            if got.setdefault(key, piece) != piece:
                return None
        prev = i
    return got if c == total else None


def near_splits(o, items):
    """Splits with each edge within NEAR clocks of where the C's clocks put
    it, from the start or from the end; the console's items exact, a place
    passed twice the same both times. At most CAP."""
    e, total = edges(o)
    clk_at = sorted(e.items())
    pos_clk = {i: c for c, i in clk_at}
    n = len(items)
    pre, c = [], 0
    for key, v in items:
        c += now(key, v)
        pre.append(c)
    stated = c
    cand = []
    for j in range(n):
        lo = pre[j]
        hi = total - (stated - pre[j])
        want = set()
        for target in (lo, hi):
            for clk, i in clk_at:
                if abs(clk - target) <= NEAR:
                    want.add(i)
        # nearest first: the splits found before CAP are the closest
        cand.append(sorted(want, key=lambda i: min(abs(pos_clk[i] - lo), abs(pos_clk[i] - hi))))
    cand[-1] = [len(o)]
    out = []

    def go(j, start, got):
        if len(out) >= CAP:
            return
        if j == n:
            if start == len(o):
                out.append(dict(got))
            return
        key, v = items[j]
        if key[0] == 'H':
            if o.startswith(key[1], start) and (j < n - 1 or start + len(key[1]) == len(o)):
                go(j + 1, start + len(key[1]), got)
            return
        if key[0] == 'K':
            i, cc = start, 0
            while cc < key[1] and i < len(o):
                cc += CLK[o[i]]
                i += 1
            if cc == key[1]:
                go(j + 1, i, got)
            return
        if key in got:
            lst = got[key]
            if o.startswith(lst, start):
                go(j + 1, start + len(lst), got)
            return
        for end in cand[j]:
            if end < start:
                continue
            got[key] = o[start:end]
            go(j + 1, end, got)
            del got[key]
            if len(out) >= CAP:
                return
    go(0, 0, {})
    return out


def closest(splits, items):
    """The splits that move the least time from where the C states it."""
    if not splits:
        return splits
    stated = {}
    for key, v in items:
        if key[0] in ('P', 'T'):
            stated[key] = now(key, v)

    def moved(sp):
        return sum(abs(clocks(lst) - stated[k]) for k, lst in sp.items())
    least = min(moved(sp) for sp in splits)
    return [sp for sp in splits if moved(sp) == least]


def solve_one(o, items, got, q):
    """The list of place q that, every other place as in `got`, makes o:
    or None."""
    occ = sum(1 for key, _ in items if key == q)
    fixed = 0
    for key, _ in items:
        if key == q:
            continue
        if key[0] == 'H':
            fixed += len(key[1])
        elif key[0] == 'K':
            return None         # its cycles' count is not fixed: leave it to the splits
        elif key in got:
            fixed += len(got[key])
        else:
            return None
    rest = len(o) - fixed
    if rest < 0 or rest % occ:
        return None
    ln = rest // occ
    at, lst = 0, None
    for key, _ in items:
        if key == q:
            piece = o[at:at + ln]
            if lst is None:
                lst = piece
            elif piece != lst:
                return None
            at += ln
        else:
            piece = key[1] if key[0] == 'H' else got[key]
            if not o.startswith(piece, at):
                return None
            at += len(piece)
    return lst if at == len(o) else None


def assign(stretches):
    """{place: list}, and each place's stretches (indices)."""
    votes = collections.defaultdict(collections.Counter)
    weak = collections.defaultdict(collections.Counter)
    through = collections.defaultdict(list)
    for si, (c, o, items) in enumerate(stretches):
        for key, _ in items:
            if key[0] in ('P', 'T') and (not through[key] or through[key][-1] != si):
                through[key].append(si)
        h = hint(o, items)
        if h is not None:
            for key, lst in h.items():
                votes[key][lst] += c
            continue
        splits = closest(near_splits(o, items), items)
        for sp in splits:
            for key, lst in sp.items():
                weak[key][lst] += c / len(splits)
    got = {}
    for key in through:
        if votes[key]:
            got[key] = votes[key].most_common(1)[0][0]
        elif weak[key]:
            got[key] = weak[key].most_common(1)[0][0]
    for rnd in range(ROUNDS):
        bad = [si for si, (c, o, items) in enumerate(stretches) if not fits(o, items, got)]
        if not bad:
            break
        want = collections.defaultdict(collections.Counter)
        for si in bad:
            c, o, items = stretches[si]
            for q in {key for key, _ in items if key[0] in ('P', 'T')}:
                lst = solve_one(o, items, got, q)
                if lst is not None:
                    want[q][lst] += c
            # a place with no list yet: the splits near the C's time
            if any(key[0] in ('P', 'T') and key not in got for key, _ in items):
                for sp in closest(near_splits(o, items), items):
                    for key, lst in sp.items():
                        if key not in got:
                            want[key][lst] += c
        changed = 0
        for q, cands in want.items():
            cur = got.get(q)

            def met(lst):
                old = got.get(q)
                got[q] = lst
                w = sum(stretches[si][0] for si in through[q] if fits(stretches[si][1], stretches[si][2], got))
                if old is None:
                    del got[q]
                else:
                    got[q] = old
                return w
            base = met(cur) if cur is not None else -1
            best, bw = cur, base
            for lst, _ in cands.most_common(4):
                if lst == cur:
                    continue
                w = met(lst)
                if w > bw:
                    best, bw = lst, w
            if best != cur:
                got[q] = best
                changed += 1
        print('cycassign: round %d: %d stretches unmet, %d places changed' % (rnd + 1, len(bad), changed), flush=True)
        if not changed:
            break
    return got, through, votes, weak


def explain(stretches, got, through):
    """Each place's status and the lists other stretches want."""
    status, alts = {}, collections.defaultdict(collections.Counter)
    by_seq = collections.defaultdict(list)
    for si, (c, o, items) in enumerate(stretches):
        by_seq[tuple(k for k, _ in items)].append(si)
    merged = collections.Counter()
    for seq, sis in by_seq.items():
        if len(sis) < 2:
            continue
        ok = [si for si in sis if fits(stretches[si][1], stretches[si][2], got)]
        if not ok:
            continue
        good = stretches[ok[0]]
        for si in sis:
            c, o, items = stretches[si]
            if si in ok:
                continue
            # where the original's cycles part from the met stretch's
            g = good[1]
            d = next((i for i in range(min(len(g), len(o))) if g[i] != o[i]), min(len(g), len(o)))
            at = 0
            for key, _ in items:
                ln = len(key[1]) if key[0] == 'H' else len(got.get(key, '')) if key[0] != 'K' else 0
                if at + ln > d or key is items[-1][0]:
                    if key[0] in ('P', 'T'):
                        merged[key] += c
                    break
                at += ln
    for key, sis in through.items():
        unmet = [si for si in sis if not fits(stretches[si][1], stretches[si][2], got)]
        if not unmet:
            status[key] = 'exact'
            continue
        for si in unmet:
            c, o, items = stretches[si]
            lst = solve_one(o, items, got, key)
            if lst is not None:
                alts[key][lst] += c
        # a place only beside the cause (in an unmet stretch, no other list
        # of its own meets it): open
        status[key] = 'merged' if merged[key] else 'conflict' if alts[key] else 'open'
    return status, alts, merged


def main(argv):
    args = argv[1:]
    out = None
    if '--out' in args:
        i = args.index('--out')
        out = args[i + 1]
        del args[i:i + 2]
    if not args:
        print(__doc__)
        return 2
    rec = os.path.abspath(args[0])
    paths = args[1:] or [os.path.join(rec, 'stretches.txt')]
    out = out or os.path.join(rec, 'assign.csv')
    places = Places(rec)
    stretches, multi = read(paths, places)
    print('cycassign: %d distinct stretches, %d in all; %d timing calls state more than one time on one path' % (
        len(stretches), sum(s[0] for s in stretches), len({k[1] for k in multi})), flush=True)
    got, through, votes, weak = assign(stretches)
    status, alts, merged = explain(stretches, got, through)
    weight = {k: sum(stretches[si][0] for si in sis) for k, sis in through.items()}
    met = {k: sum(stretches[si][0] for si in sis if fits(stretches[si][1], stretches[si][2], got))
           for k, sis in through.items()}
    stated = collections.defaultdict(collections.Counter)
    for c, o, items in stretches:
        for key, v in items:
            if key[0] in ('P', 'T'):
                stated[key]['0x' + v if isinstance(v, str) else str(v)] += c
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['place', 'file', 'line', 'col', 'fn', 'what', 'now', 'list', 'clocks', 'stretches', 'met',
                    'status', 'others'])
        for key in sorted(through, key=lambda k: (k[0], k[1], name(k))):
            fl, ln, col, what, fn = places.where(key)
            lst = got.get(key)
            w.writerow([name(key), fl, ln, col, fn, what, stated[key].most_common(1)[0][0] if stated[key] else '',
                        '' if lst is None else (lst or '-'), '' if lst is None else clocks(lst), weight[key], met[key],
                        status[key] if lst is not None else 'unsolved',
                        ' '.join('%s:%d' % (a or '-', n) for a, n in alts[key].most_common(3))])
    sts = [s for s in stretches]
    ok = sum(s[0] for s in sts if fits(s[1], s[2], got))
    tot = sum(s[0] for s in sts)
    cnt = collections.Counter(status[k] if got.get(k) is not None else 'unsolved' for k in through)
    def stated_clocks(k):
        v = stated[k].most_common(1)[0][0]
        return clocks(v[2:]) if v.startswith('0x') else int(v)
    changed = sum(1 for k in through if k[0] == 'T' and got.get(k) is not None and stated[k] and
                  clocks(got[k]) != stated_clocks(k))
    added = sum(1 for k in through if k[0] == 'P' and got.get(k))
    lines = ['cycassign: %d of %d stretches met (%.4f%%; %d of %d distinct)' % (
        ok, tot, 100.0 * ok / max(1, tot), sum(1 for s in sts if fits(s[1], s[2], got)), len(sts)),
        '  places: %s' % ', '.join('%d %s' % (n, s) for s, n in cnt.most_common()),
        '  timing calls whose clocks change: %d; probes given time (new timing calls): %d' % (changed, added)]
    worst = sorted((k for k in through if status.get(k) in ('merged', 'conflict')), key=lambda k: -(weight[k] - met[k]))
    if worst:
        lines.append('  places unmet the most:')
    for k in worst[:25]:
        fl, ln, col, what, fn = places.where(k)
        lines.append('    %-22s %s:%s %s %s  %d of %d unmet  list %s  others %s' % (
            name(k), fl, ln, fn, status.get(k, 'unsolved'), weight[k] - met[k], weight[k], got.get(k),
            ' '.join('%s:%d' % (a or '-', n) for a, n in alts[k].most_common(3))))
    rep = '\n'.join(lines)
    print(rep)
    with open(os.path.splitext(out)[0] + '.txt', 'w') as f:
        f.write(rep + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
