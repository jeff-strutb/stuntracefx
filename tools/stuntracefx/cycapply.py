#!/usr/bin/env python3
"""cycapply.py: the places' cycle lists written into the decomp as cycs(),
then built on nativeshvc and checked against the original.

    cycapply.py REC [--asm ASM.csv] [--into DIR] [--dry]
                    [--check [--only NAME,...] [--plays] [--scripts] [--jobs N] [--frames A-B]]

REC is cycrecord.py's OUT, with cycassign.py's assign.csv; ASM.csv
(cycasm.py's) gives the places no recording reaches. The decomp must be as
it was recorded (REC/tree.sha1): the places are named by their positions
in it. The C is written into src/stuntracefx, or with --into into a copy
at DIR (the decomp's other files copied as they are); --dry writes
nothing and reports.

What is written:

    a timing call (cyc, cyc_then, cyc_loop, cycs...): its time as the
        list, cycs(0x8868), cycs_then(0x8868, v), cycs_loop(0x8868, test);
        an empty list removes the call (its value, or its test, stays)
    a choice in a timing call, cyc(c ? 32 : 54): each arm its list,
        cycs(c ? 0x8688 : 0x86888); a sum with choices, cyc(8 + (k ?
        104 : 152)), one list per path, the choices nested in order
    a timing call stating a helper's parameter, cyc(lead): the parameter
        a list (uint64_t), and each caller's value as the list for it
    a probe place given time (none stated there now): cycs() where the
        probe stood, as cycwrite.py places a time (a block's start, an
        arm, a skipped arm, a loop's next pass, before an access...)
    not written: a place cycassign.py found merged or in conflict (the
        stretches through it want different lists): left as it is, named
    a list longer than 16 cycles: the first 16 in one call, the rest in
        the next (statements one after another, braced where they are a
        body; cycs_then and cycs_loop nested, the first 16 innermost); in a
        choice, the calls in its arms

Then each recorded stretch is checked against the lists as written (the
path totals: the places passed, one list after another, are the
original's cycles), and every place left as it was is listed with why.

--check builds what was written on nativeshvc, records it again (the plays
by default, as cycrecord.py does, into REC/check) and checks every
stretch of it: the cycles the C now states (cycs and the console's), one
after another, against the original's. That is the one report: what
matches, what does not and where, what is not stated as cycles yet.
"""
import collections, csv, glob, os, re, shutil, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cycassign  # noqa: E402
import cycprobe  # noqa: E402
import cycwrite  # noqa: E402

ROOT = cycprobe.ROOT
TOOLS = os.path.join(ROOT, 'tools', 'stuntracefx')
MAX = 16          # cycles in one cycs()
HELD = '\x01%d\x01'   # a helper's timing call, renamed once its callers pass lists
GONE = '\x00'     # where a statement was taken out: its line goes too when nothing else is left on it
LIT = re.compile(r'^(0[xX][0-9A-Fa-f]+|\d+)[uU]?$')
IDENT = re.compile(r'^[A-Za-z_]\w*$')


def hexv(lst):
    return '0x' + lst if lst else '0'


def chunks(lst):
    return [lst[i:i + MAX] for i in range(0, len(lst), MAX)] or ['']


class Lists:
    """cycwrite.py's forms with a list of cycles for a value."""
    zero = ''

    @staticmethod
    def stmt(v): return ''.join('cycs(%s); ' % hexv(c) for c in chunks(v))
    @staticmethod
    def expr(v): return '(' + ''.join('cycs(%s), ' % hexv(c) for c in chunks(v)), ')'
    @staticmethod
    def loop(v):
        cs = chunks(v)
        return ''.join('cycs_loop(%s, ' % hexv(c) for c in reversed(cs)), ')' * len(cs)
    @staticmethod
    def then(v):
        cs = chunks(v)
        return ''.join('cycs_then(%s, ' % hexv(c) for c in reversed(cs)), ')' * len(cs)


# ---- the first argument of a timing call, as a tree ----

class Lit:
    def __init__(self, v, s, e): self.v, self.s, self.e = v, s, e


class Cond:
    def __init__(self, cond, a, b, sa, sb): self.cond, self.a, self.b, self.sa, self.sb = cond, a, b, sa, sb


class Sum:
    def __init__(self, terms): self.terms = terms        # [(sign, node)]


class Opaque:
    def __init__(self, text): self.text = text


def skip(text, s, e):
    while s < e and text[s] in ' \t\n':
        s += 1
    while e > s and text[e - 1] in ' \t\n':
        e -= 1
    return s, e


def strip_comments(t):
    return re.sub(r'/\*.*?\*/', ' ', t, flags=re.S)


def enclosed(text, s, e):
    """Whether text[s:e] is one parenthesised whole."""
    if text[s] != '(' or text[e - 1] != ')':
        return False
    d = 0
    for i in range(s, e):
        d += {'(': 1, ')': -1}.get(text[i], 0)
        if d == 0 and i < e - 1:
            return False
    return True


def parse(text, s, e):
    s, e = skip(text, s, e)
    while e - s > 1 and enclosed(text, s, e):
        s, e = skip(text, s + 1, e - 1)
    seg = text[s:e]
    d, q = 0, None
    for i in range(s, e):
        c = text[i]
        if c in '([':
            d += 1
        elif c in ')]':
            d -= 1
        elif c == '?' and d == 0:
            q = i
            break
    if q is not None:
        d, nest, colon = 0, 0, None
        for i in range(q + 1, e):
            c = text[i]
            if c in '([':
                d += 1
            elif c in ')]':
                d -= 1
            elif d == 0 and c == '?':
                nest += 1
            elif d == 0 and c == ':':
                if nest == 0:
                    colon = i
                    break
                nest -= 1
        if colon is None:
            return Opaque(seg)
        sa, _ = skip(text, q + 1, colon)
        sb, _ = skip(text, colon + 1, e)
        return Cond(text[s:q].strip(), parse(text, q + 1, colon), parse(text, colon + 1, e), sa, sb)
    m = LIT.match(seg)
    if m:
        return Lit(int(m.group(1), 0), s, e)
    # a sum at the top: + and - between operands
    terms, d, start, sign = [], 0, s, 1
    prev = ''
    for i in range(s, e):
        c = text[i]
        if c in '([':
            d += 1
        elif c in ')]':
            d -= 1
        elif d == 0 and c in '+-' and (prev.isalnum() or prev in ')]_'):
            if text[i:i + 2] in ('++', '--'):
                return Opaque(seg)
            terms.append((sign, start, i))
            sign = 1 if c == '+' else -1
            start = i + 1
        elif d == 0 and c in '<>=&|^!*/%,':
            return Opaque(seg)
        if c not in ' \t\n':
            prev = c
    if not terms:
        return Opaque(seg)
    terms.append((sign, start, e))
    return Sum([(sg, parse(text, a, b)) for sg, a, b in terms])


def paths(node, arm):
    """Each path through the tree: (arm probes chosen in order, value,
    [(cond text, branch)]). Raises ValueError for code that is no number."""
    if isinstance(node, Lit):
        return [((), node.v, ())]
    if isinstance(node, Cond):
        pa, pb = arm.get(node.sa), arm.get(node.sb)
        if pa is None or pb is None:
            raise ValueError('a choice without its arms\' probes')
        out = []
        for p, v, cs in paths(node.a, arm):
            out.append(((pa,) + p, v, ((node.cond, 1),) + cs))
        for p, v, cs in paths(node.b, arm):
            out.append(((pb,) + p, v, ((node.cond, 0),) + cs))
        return out
    if isinstance(node, Sum):
        out = [((), 0, ())]
        for sg, t in node.terms:
            nxt = []
            for p, v, cs in out:
                for p2, v2, cs2 in paths(t, arm):
                    nxt.append((p + p2, v + sg * v2, cs + cs2))
            out = nxt
        return out
    raise ValueError('no number: %s' % node.text)


def only_choices(node):
    return isinstance(node, Lit) or (isinstance(node, Cond) and only_choices(node.a) and only_choices(node.b))


def pure(cond):
    """A condition that may be worked out twice: no call, no assignment."""
    c = strip_comments(cond)
    return not re.search(r'\w\s*\(|\+\+|--|[^=!<>]=[^=]', c)


def tree_text(items, leaf):
    """items: [(conds, list)], every one with the same first condition (or
    none). The ?: text, each leaf leaf(list)."""
    if len(items) == 1 and not items[0][0]:
        return leaf(items[0][1])
    cond = items[0][0][0][0]
    a = [(cs[1:], l) for cs, l in items if cs[0] == (cond, 1)]
    b = [(cs[1:], l) for cs, l in items if cs[0] == (cond, 0)]
    if not a or not b:
        raise ValueError('a path not reached')
    return '%s ? %s : %s' % (cond, wrap(tree_text(a, leaf)), wrap(tree_text(b, leaf)))


def wrap(t):
    return '(%s)' % t if '?' in t else t


# ---- the plan for one timing call ----

def close_paren(text, i):
    """The ')' closing the '(' at i."""
    d = 0
    for j in range(i, len(text)):
        d += {'(': 1, ')': -1}.get(text[j], 0)
        if d == 0:
            return j
    raise ValueError('unbalanced')


def first_arg(text, a, b):
    d = 0
    for i in range(a, b):
        c = text[i]
        if c in '([{':
            d += 1
        elif c in ')]}':
            d -= 1
        elif c == ',' and d == 0:
            return a, i, i
    return a, b, None


def context(text, cs, ce):
    """How the call at text[cs:ce] stands: 'stmt', 'body' (a statement
    that is an if's, loop's or else's body), 'comma' ((call, X)) or
    'expr'."""
    j = ce
    while j < len(text) and text[j] in ' \t':
        j += 1
    i = cs - 1
    while i >= 0 and text[i] in ' \t\n':
        i -= 1
    before = text[i] if i >= 0 else ';'
    word = re.search(r'(\w+)\s*$', text[:cs])
    if j < len(text) and text[j] == ';':
        if before == ')' or (word and word.group(1) in ('else', 'do')):
            return 'body'
        return 'stmt'
    if before == '(' and j < len(text) and text[j] == ',':
        return 'comma'
    return 'expr'


class Writer:
    def __init__(self, rec, extra):
        self.rec = rec
        self.lists, self.status = {}, {}
        self.disputed = {}
        with open(os.path.join(rec, 'assign.csv')) as f:
            for r in csv.DictReader(f):
                if r['status'] in ('merged', 'conflict'):
                    # stretches want other lists of it: as it is, named
                    self.disputed[r['place']] = '%s, others %s' % (r['status'], r['others'])
                    continue
                if r['list'] != '':
                    self.lists[r['place']] = '' if r['list'] == '-' else r['list']
                    self.status[r['place']] = r['status']
        self.from_asm = set()
        for p in extra:
            with open(p) as f:
                for r in csv.DictReader(f):
                    if r['place'] not in self.lists and r['list'] != '':
                        self.lists[r['place']] = '' if r['list'] == '-' else r['list']
                        self.from_asm.add(r['place'])
        self.timing = {}
        with open(os.path.join(rec, 'tree', 'timing.csv')) as f:
            for r in csv.DictReader(f):
                self.timing[int(r['id'])] = r
        self.arm = collections.defaultdict(dict)     # timing id -> {arm offset: probe id}
        self.probe_file = {}
        with open(os.path.join(rec, 'tree', 'probes.csv')) as f:
            for r in csv.DictReader(f):
                if r.get('timing'):
                    self.arm[int(r['timing'])][int(r['offset'])] = int(r['id'])
        self.left = collections.Counter()     # why a place stays as it is
        self.left_at = collections.defaultdict(list)
        self.written = set()                  # place names written
        self.params = []                      # (file, fn, param, timing id): helpers whose callers pass lists

    def leave(self, why, where):
        self.left[why] += 1
        if len(self.left_at[why]) < 8:
            self.left_at[why].append(where)

    def lookup(self, tid, path, value):
        base = 't%d' % tid + ('/' + '.'.join(str(p) for p in path) if path else '')
        if base in self.lists:
            return base, self.lists[base]
        v = base + '=%d' % value
        if v in self.lists:
            return v, self.lists[v]
        return base, None

    def timing_edits(self, path, text, rel):
        """(start, end, rank, text) replacements for the file's timing calls."""
        edits = []
        for tid, t in self.timing.items():
            if t['file'] != rel:
                continue
            where = '%s:%s %s' % (t['file'], t['line'], t['fn'])
            cs, ce = int(t['offset']), int(t['end'])
            name = t['name']
            call = text[cs:ce]
            if not call.startswith(name):
                raise SystemExit('cycapply: %s does not hold %s() at %d any more: record it again' % (rel, name, cs))
            a, b, comma = first_arg(text, int(t['arg_from']), int(t['arg_to']))
            node = parse(text, a, b)
            try:
                ps = paths(node, self.arm[tid])
            except ValueError:
                ps = None
            new = 'cycs' + name[len('cycs' if name.startswith('cycs') else 'cyc'):]
            kind = name.split('_')[1] if '_' in name else ''
            if ps is not None:
                got = [(cs_, self.lookup(tid, p, v)) for p, v, cs_ in ps]
                miss = [n for _, (n, l) in got if l is None]
                hard = [n for n in miss if n in self.disputed]
                if hard:
                    self.leave('a merged path or conflict: stretches want other lists (as it is)',
                               '%s %s: %s' % (where, hard[0], self.disputed[hard[0]][:120]))
                    continue
                if miss:
                    self.leave('a path with no list (not recorded, not read from the assembly)', where)
                    continue
                lists = {n: l for _, (n, l) in got}
                if len(ps) == 1:
                    e = self.plain(text, cs, ce, a, b, comma, name, new, kind, got[0][1][1])
                    if e is None:
                        self.leave('an empty list where the call cannot go', where)
                        continue
                    edits += e
                    self.written |= set(lists)
                    continue
                e = self.choice(tid, text, cs, ce, a, b, comma, name, new, kind, node, ps, got, where)
                if e is not None:
                    edits += e
                    self.written |= set(lists)
                continue
            # no number: a value per path (cyc(lead), cyc(2 * imm + 30))
            arg = text[a:b].strip()
            if IDENT.match(arg):
                vals = {n[n.index('=') + 1:]: l for n, l in self.lists.items() if n.startswith('t%d=' % tid)}
                if vals:
                    self.params.append((rel, t['fn'], arg, tid, vals, where, name, new))
                    # the name stays until every caller's value is a list
                    edits.append((cs, cs + len(name), 0, HELD % tid))
                    continue
            vals = sorted(n for n in self.lists if n.startswith('t%d=' % tid) or n.startswith('t%d/' % tid))
            if not vals:
                self.leave('a path with no list (not recorded, not read from the assembly)', where)
                continue
            self.leave('a time worked out from data, not a parameter (a list per value, written by hand)',
                       '%s %s: %s' % (where, arg, ' '.join('%s=%s' % (v.split('=')[-1], self.lists[v]) for v in vals[:6])))
        return edits

    def plain(self, text, cs, ce, a, b, comma, name, new, kind, lst):
        """One list for the call."""
        ctx = context(text, cs, ce)
        if lst == '':
            if kind in ('then', 'loop'):
                # the call's value: its second argument
                v0 = comma + 1
                while text[v0] in ' \t\n':
                    v0 += 1
                return [(cs, v0, 0, ''), (ce - 1, ce, 0, '')]
            if ctx in ('stmt', 'body'):
                end = ce
                while text[end] in ' \t':
                    end += 1
                end += 1        # its ;
                if ctx == 'body':
                    return [(cs, end, 0, ';')]
                while end < len(text) and text[end] in ' \t':
                    end += 1
                return [(cs, end, 0, GONE)]
            if ctx == 'comma':
                j = ce
                while text[j] in ' \t,':
                    j += 1
                return [(cs, j, 0, '')]
            return None
        cs_ = chunks(lst)
        if len(cs_) == 1:
            return [(cs, cs + len(name), 0, new), (a, b, 0, hexv(lst))]
        if kind in ('then', 'loop'):
            fn = 'cycs_' + kind
            v0 = comma + 1
            while text[v0] in ' \t\n':
                v0 += 1
            head = ''.join('%s(%s, ' % (fn, hexv(c)) for c in reversed(cs_))
            return [(cs, v0, 0, head), (ce - 1, ce, 0, ')' * len(cs_))]
        calls = ['cycs(%s)' % hexv(c) for c in cs_]
        if ctx == 'stmt':
            ind = text[text.rfind('\n', 0, cs) + 1:cs]
            sep = ';\n' + ind if ind.strip() == '' else '; '
            return [(cs, ce, 0, sep.join(calls))]
        if ctx == 'body':
            end = ce
            while text[end] in ' \t':
                end += 1
            return [(cs, end + 1, 0, '{ ' + '; '.join(calls) + '; }')]
        return [(cs, ce, 0, '(' + ', '.join(calls) + ')')]

    def choice(self, tid, text, cs, ce, a, b, comma, name, new, kind, node, ps, got, where):
        """A list per path through the call's choices."""
        longest = max(len(l) for _, (n, l) in got)
        if only_choices(node) and longest <= MAX:
            # each arm's number in place: the author's text kept
            by_path = {p: l for (p, _v, _c), (_cc, (_n, l)) in zip(ps, got)}
            arms = self.arm[tid]
            edits = [(cs, cs + len(name), 0, new)]

            def leaves(nd, path):
                if isinstance(nd, Lit):
                    edits.append((nd.s, nd.e, 0, hexv(by_path[path])))
                    return
                leaves(nd.a, path + (arms[nd.sa],))
                leaves(nd.b, path + (arms[nd.sb],))
            leaves(node, ())
            return edits
        items = [(cs_, l) for cs_, (n, l) in got]
        conds = {c for cs_, _ in items for c, _ in cs_}
        if longest <= MAX:
            try:
                t = tree_text(items, hexv)
            except ValueError as e:
                self.leave('a choice with a path not reached (%s)' % e, where)
                return None
            if not only_choices(node) and not all(pure(c) for c in conds):
                # a sum's choices nest: a later condition is written once
                # in each arm of an earlier one
                self.leave('a sum of choices whose condition is a call (written by hand)', where)
                return None
            return [(cs, cs + len(name), 0, new), (a, b, 0, t)]
        # longer than one call: the calls in the arms
        ctx = context(text, cs, ce)
        if kind in ('then', 'loop'):
            if not all(pure(c) for c in conds):
                self.leave('over 16 cycles in a choice whose condition is a call', where)
                return None
            n = max(len(chunks(l)) for _, l in items)
            fn = 'cycs_' + kind
            heads = []
            for k in reversed(range(n)):
                heads.append('%s(%s, ' % (fn, tree_text([(c, (chunks(l) + [''] * n)[k]) for c, l in items], hexv)))
            v0 = comma + 1
            while text[v0] in ' \t\n':
                v0 += 1
            return [(cs, v0, 0, ''.join(heads)), (ce - 1, ce, 0, ')' * n)]

        def arm(l):
            cl = ['cycs(%s)' % hexv(c) for c in chunks(l)]
            return cl[0] if len(cl) == 1 else '(' + ', '.join(cl) + ')'
        t = tree_text(items, arm)
        if ctx == 'body':
            end = ce
            while text[end] in ' \t':
                end += 1
            return [(cs, end + 1, 0, '{ ' + t + '; }')]
        return [(cs, ce, 0, t if ctx in ('stmt',) else '(' + t + ')')]


def apply_edits(text, edits):
    """Replacements and insertions: (start, end, rank, text); none may
    overlap another's inside."""
    edits = sorted(edits, key=lambda e: (e[0], e[1] - e[0] > 0, e[2]))
    out, at = [], 0
    for s, e, _r, t in edits:
        if s < at:
            raise ValueError('edits overlap at %d' % s)
        out.append(text[at:s])
        out.append(t)
        at = e
    out.append(text[at:])
    return ''.join(out)


MERGE = re.compile(r'cycs\(0x([68C]+)\);(\s*)cycs\(0x([68C]+)\);')


def merge(text):
    """cycs(0xA); cycs(0xB); next to each other as one, where it fits."""
    pos = 0
    while True:
        m = MERGE.search(text, pos)
        if not m:
            return text
        if len(m.group(1)) + len(m.group(3)) <= MAX and '\n\n' not in m.group(2) and '}' not in m.group(2):
            text = text[:m.start()] + 'cycs(0x%s%s);' % (m.group(1), m.group(3)) + text[m.end():]
            pos = m.start()
        else:
            pos = m.start() + 1


def write_params(w, texts):
    """Helpers whose timing call states a parameter: the parameter as a
    list, each caller's value as its list."""
    for rel, fn, param, tid, vals, where, old, new in w.params:
        p = os.path.join(cycprobe.SRC, os.path.relpath(os.path.join(ROOT, rel), cycprobe.DECOMP))
        if write_param(w, texts, p, fn, param, tid, vals, where):
            texts[p] = texts[p].replace(HELD % tid, new)
        else:
            texts[p] = texts[p].replace(HELD % tid, old)


def write_param(w, texts, p, fn, param, tid, vals, where):
    if True:
        text = texts[p]
        dm = re.search(r'\b%s\s*\(([^;{)]*)\)\s*\{' % re.escape(fn), text)
        if not dm:
            w.leave('a helper whose definition is not found', where)
            return False
        params = [x.strip() for x in dm.group(1).split(',')]
        idx = next((i for i, x in enumerate(params) if re.search(r'\b%s$' % re.escape(param), x)), None)
        if idx is None:
            w.leave('a time from a local, not a parameter (written by hand)', where)
            return False
        # every caller: the value at idx must be numbers (or choices of them)
        calls, decls = [], []
        ok = True
        for q, t in texts.items():
            for m in re.finditer(r'(?<![\w.>])%s\s*\(' % re.escape(fn), t):
                if t[m.start():].startswith(dm.group(0)[:len(fn)]) and m.start() == dm.start() and q == p:
                    continue
                o = t.index('(', m.start())
                c = close_paren(t, o)
                decl = t[c + 1:].lstrip().startswith(('{', ';')) and \
                    re.match(r'\s*[\w\s\*]+\s+$', t[t.rfind('\n', 0, m.start()) + 1:m.start()])
                args, d, st = [], 0, o + 1
                for i in range(o + 1, c):
                    ch = t[i]
                    if ch in '([{':
                        d += 1
                    elif ch in ')]}':
                        d -= 1
                    elif ch == ',' and d == 0:
                        args.append((st, i))
                        st = i + 1
                args.append((st, c))
                if decl:
                    # a prototype: its parameter's type changes with the definition's
                    if idx < len(args):
                        a0, a1 = args[idx]
                        decls.append((q, a0, a1))
                    continue
                if idx >= len(args):
                    ok = False
                    break
                s, e = args[idx]
                node = parse(t, s, e)
                if not only_choices(node):
                    ok = False
                    w.leave('a helper\'s caller passes a value worked out from data', '%s (caller in %s)' % (where, os.path.relpath(q, ROOT)))
                    break
                calls.append((q, node))
            if not ok:
                break
        if not ok:
            return False
        need = set()

        def lits(nd):
            if isinstance(nd, Lit):
                yield nd
            elif isinstance(nd, Cond):
                yield from lits(nd.a)
                yield from lits(nd.b)
        for q, node in calls:
            for l in lits(node):
                need.add(l.v)
        missing = [v for v in need if str(v) not in vals]
        if missing or any(len(vals[str(v)]) > MAX for v in need):
            w.leave('a helper\'s caller passes a value with no list, or one over 16 cycles', '%s values %s' % (
                where, ' '.join(str(v) for v in sorted(missing)) or 'over 16'))
            return False
        edits = collections.defaultdict(list)
        for q, node in calls:
            for l in lits(node):
                edits[q].append((l.s, l.e, 0, hexv(vals[str(l.v)])))
        # the parameter's type
        ps, pe = dm.start(1), dm.end(1)
        newp = list(params)
        newp[idx] = re.sub(r'^(?:const\s+)?(?:uint\d+_t|unsigned(?: int)?|int)\b', 'uint64_t', newp[idx])
        edits[p].append((ps, pe, 0, ', '.join(newp)))
        for q, a0, a1 in decls:
            if q == p and a0 >= ps and a1 <= pe:
                continue
            d0 = texts[q][a0:a1]
            edits[q].append((a0, a1, 0, re.sub(r'^(\s*)(?:const\s+)?(?:uint\d+_t|unsigned(?: int)?|int)\b', r'\1uint64_t', d0)))
        for q, es in edits.items():
            texts[q] = apply_edits(texts[q], es)
        w.written |= {'t%d=%s' % (tid, v) for v in need}
        return True


def fingerprint_ok(rec):
    sys.path.insert(0, TOOLS)
    import cycrecord
    stamp = os.path.join(rec, 'tree.sha1')
    return os.path.exists(stamp) and open(stamp).read().strip() == cycrecord.fingerprint()


def static_check(rec, w):
    """The recorded stretches against the lists as written (places left as
    they were keep what they state now: a list, or no list)."""
    places = cycassign.Places(rec)
    stretches, _ = cycassign.read([os.path.join(rec, 'stretches.txt')], places)
    got = {}
    for n in w.written:
        key = name_key(n)
        if key is not None:
            got[key] = w.lists.get(n, '')
    for p, l in w.lists.items():
        if p.startswith('p') and p in w.written:
            got[('P', int(p[1:]))] = l
    met = unstated = wrong = 0
    for c, o, items in stretches:
        g = dict(got)
        miss = False
        for key, v in items:
            if key[0] == 'P' and key not in g:
                g[key] = ''
            elif key[0] == 'T' and key not in g:
                if isinstance(v, str):
                    g[key] = v           # a cycs left as it was
                else:
                    miss = True
        if miss:
            unstated += c
        elif cycassign.fits(o, items, g):
            met += c
        else:
            wrong += c
    return met, unstated, wrong


def name_key(n):
    m = re.match(r'^t(\d+)(?:/([\d.]+))?(?:=(\d+))?$', n)
    if not m:
        return None
    key = ('T', int(m.group(1)), tuple(int(x) for x in m.group(2).split('.')) if m.group(2) else ())
    return key + (int(m.group(3)),) if m.group(3) else key


def verify(path, timing):
    """A recording of the written C: each stretch's stated cycles, one
    after another, against the original's."""
    met = wrong = unstated = 0
    bad = collections.Counter()
    for line in open(path):
        if line[0] == '#':
            continue
        c, o, toks = line.rstrip('\n').split('\t')
        c = int(c)
        lst, odd, first = '', False, None
        for t in toks.split():
            if t[0] == 's':
                tid, h = t[1:].split(':')
                if first is None and not o.startswith(lst + cycassign.hexlist(h)):
                    first = int(tid)
                lst += cycassign.hexlist(h)
            elif t[0] == 'q':
                lst += cycassign.hexlist(t[1:])
            elif t[0] == 't':
                odd = True
                tid = int(t[1:].split(':')[0])
                first = first or tid
            elif t[0] == 'k':
                n, cc = int(t[1:]), 0
                i = len(lst)
                while cc < n and i < len(o):
                    cc += cassign_clk(o[i])
                    i += 1
                lst += o[len(lst):i] if cc == n else '?'
        if odd:
            unstated += c
            bad[('unstated', first)] += c
        elif lst == o:
            met += c
        else:
            wrong += c
            bad[('wrong', first)] += c
    lines = []
    for (why, tid), n in bad.most_common(25):
        t = timing.get(tid, {})
        lines.append('    %-8s %8d  %s:%s %s %s(%s)' % (why, n, t.get('file', '?'), t.get('line', ''), t.get('fn', ''),
                                                      t.get('name', ''), t.get('args', '')))
    return met, wrong, unstated, lines


def cassign_clk(ch):
    return cycassign.CLK.get(ch, 0)


def main(argv):
    args = argv[1:]
    if not args or args[0].startswith('-'):
        print(__doc__)
        return 2
    rec = os.path.abspath(args.pop(0))

    def opt(n):
        if n in args:
            i = args.index(n)
            v = args[i + 1]
            del args[i:i + 2]
            return v
    asm = opt('--asm')
    into = opt('--into')
    only, jobs, frames = opt('--only'), opt('--jobs'), opt('--frames')
    dry, check = '--dry' in args, '--check' in args
    which = [a for a in args if a in ('--plays', '--scripts')]
    if not fingerprint_ok(rec):
        raise SystemExit('cycapply: the decomp is not as %s recorded it (tree.sha1): record it again' % rec)
    src = cycprobe.SRC
    dest = os.path.abspath(into) if into else src
    w = Writer(rec, [asm] if asm else [])
    files = sorted(glob.glob(os.path.join(src, 'include', '*.h'))) + \
        sorted(glob.glob(os.path.join(src, '**', '*.c'), recursive=True))
    texts = {f: open(f).read() for f in files}
    edits = collections.defaultdict(list)
    for f in files:
        edits[f] += w.timing_edits(f, texts[f], cycprobe.rel_name(f))
    # the probe places given time: cycwrite.py's places
    cost = {}
    for n, l in w.lists.items():
        if n[0] == 'p' and l:
            cost[int(n[1:])] = l
    for n in w.disputed:
        if n[0] == 'p':
            w.leave('a merged path or conflict at a probe: stretches want other lists (none written)', n)
    for path, text, got in cycprobe.all_probes():
        es, why = cycwrite.plan(path, text, got, cost, set(), Lists)
        for k, v in why.items():
            w.left['probe place: ' + k] += v
        edits[path] += [(off, off, rank, t) for off, rank, t in es]
        for pid, kind, m in got:
            if pid in cost:
                w.written.add('p%d' % pid)
    new = {}
    for f in files:
        try:
            new[f] = apply_edits(texts[f], [(s, e, r if isinstance(r, tuple) else (r,), t) for s, e, r, t in edits[f]])
        except ValueError as e:
            raise SystemExit('cycapply: %s: %s' % (cycprobe.rel_name(f), e))
    write_params(w, new)
    for f in files:
        t = re.sub(r'(?m)^[ \t]*%s\n' % GONE, '', new[f])
        new[f] = merge(t.replace(GONE, ''))
    met, unstated, wrong = static_check(rec, w)
    tot = max(1, met + unstated + wrong)
    t_places = [n for n in w.lists if n[0] == 't']
    print('cycapply: %d places written (%d timing, %d new at probes; %d from the assembly reading), %d left as they are'
          % (len(w.written), sum(1 for n in w.written if n[0] == 't'), sum(1 for n in w.written if n[0] == 'p'),
             len(w.written & w.from_asm), sum(w.left.values())))
    print('cycapply: path totals against the recording: %d met (%.4f%%), %d wrong, %d through a place not written'
          % (met, 100.0 * met / tot, wrong, unstated))
    for why, n in w.left.most_common():
        print('  %6d left: %s' % (n, why))
        for at in w.left_at[why][:3]:
            print('           %s' % at)
    unconverted = 0
    for f in files:
        unconverted += len(re.findall(r'(?<![\w.])cyc(?:_then|_loop)?\(\s*[^)]', strip_comments(new[f])))
    print('cycapply: timing calls not stated as cycles after writing: %d' % unconverted)
    del t_places
    if dry:
        return 0
    if into:
        for f in glob.glob(os.path.join(src, '**', '*'), recursive=True):
            if os.path.isfile(f):
                o = os.path.join(dest, os.path.relpath(f, src))
                os.makedirs(os.path.dirname(o), exist_ok=True)
                if f not in new:
                    shutil.copyfile(f, o)
    for f in files:
        o = os.path.join(dest, os.path.relpath(f, src))
        os.makedirs(os.path.dirname(o), exist_ok=True)
        if not os.path.exists(o) or open(o).read() != new[f]:
            with open(o, 'w') as fh:
                fh.write(new[f])
    print('cycapply: written into %s' % dest)
    if not check:
        return 0
    # build what was written, record it again, check every stretch
    nshvc = os.path.abspath(os.environ.get('NSHVC', os.path.join(ROOT, '..', 'nativeshvc')))
    print('cycapply: building it on nativeshvc', flush=True)
    p = subprocess.run(['sh', 'tests/srfx/build.sh'], cwd=nshvc, capture_output=True, text=True,
                       env=dict(os.environ, GAME_DIR=dest))
    if p.returncode:
        print('cycapply: the written C does not build:\n' + (p.stdout + p.stderr)[-3000:])
        return 1
    out = os.path.join(rec, 'check')
    cmd = [sys.executable, os.path.join(TOOLS, 'cycrecord.py'), out, '--fresh', '--again'] + (which or ['--plays'])
    if only:
        cmd += ['--only', only]
    if jobs:
        cmd += ['--jobs', jobs]
    if frames:
        cmd += ['--frames', frames]
    print('cycapply: recording it: %s' % ' '.join(cmd[2:]), flush=True)
    rc = subprocess.run(cmd, env=dict(os.environ, CYC_SRC=dest)).returncode
    timing = {}
    with open(os.path.join(out, 'tree', 'timing.csv')) as f:
        for r in csv.DictReader(f):
            timing[int(r['id'])] = r
    met, wrong, unstated, lines = verify(os.path.join(out, 'stretches.txt'), timing)
    tot = max(1, met + wrong + unstated)
    # how much of the run paired, against the recording the lists came from
    def paired(d, names):
        n = 0
        for nm in names:
            f = os.path.join(d, 'runs', nm + '.txt')
            if os.path.exists(f):
                n += int(open(f).readline().split()[2])
        return n
    names = [os.path.splitext(os.path.basename(f))[0] for f in glob.glob(os.path.join(out, 'runs', '*.txt'))]
    before, after = paired(rec, names), paired(out, names)
    if before:
        print('cycapply: paired stretches %d, the recording had %d for the same runs (%.1f%%): fewer means the C now '
              'runs apart from the original in places (a poll loop counting differently, an access moved)' % (
                  after, before, 100.0 * after / before))
    print('cycapply: REPORT: %d stretches; %d match the original cycle for cycle (%.4f%%), %d do not (%.4f%%), '
          '%d pass a time not stated as cycles (%.4f%%)%s' % (
              tot, met, 100.0 * met / tot, wrong, 100.0 * wrong / tot, unstated, 100.0 * unstated / tot,
              '' if rc == 0 else '; some runs failed (REC/check/record.txt)'))
    if lines:
        print('  where (the first timing call off in each stretch):')
        print('\n'.join(lines))
    return 0 if wrong == 0 and unstated == 0 and rc == 0 else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
