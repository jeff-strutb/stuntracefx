#!/usr/bin/env python3
"""cycasm.py: cycle lists for the C places no recording reaches, read from
the original's 65816 assembly.

    cycasm.py --table ORIGLOG          check the cycle rules against sfxbox's --cyclog
    cycasm.py REC [--out ASM.csv]      lists for REC's unreached timing calls

The rules (each instruction's cycles in the order the 65816 makes them,
native mode, from its addressing mode and the M and X widths) are checked
first against every instruction of an original's run: --table reads an
sfxbox --cyclog (the opcode fetch, operand fetches, internal cycles, reads
and writes, in order) and names each opcode whose run no rule gives.

A place is read from the assembly where both sides are plain: in the C, a
stretch between two accesses (or the routine's start, or its return) with
timing calls in it and nothing else that takes time or branches; in the
assembly (asm/stuntracefx, the routine of the C's @routine tag), the
instructions between the two that make those accesses, with no branch,
jump, call or label among them. The C's accesses are matched to the
instructions' in order, by the low 16 bits of their address (a longest
common sequence). The stretch's cycles are every cycle of those
instructions that is not one of the C's accesses: fetches, internal
cycles, and the reads and writes the C does not make (the stack's, a
ROM table's). Each timing call takes the next cycles that add up to the
clocks it states now. Where a cycle may or may not be made (an index
crossing a page or 16 bits wide, the direct page register's low byte not
zero), each choice is tried, and the stretch is read only when exactly one
order of cycles fits the clocks stated.

Before any list is given, the same reading is made of the places a
recording did reach (REC/assign.csv, exact ones), and each is compared
with the list the original's run gave: the report says how many agree.
ASM.csv: place, list, how it was read; the report REC/asm.txt says what
was read, what agreed, and why each other place was not read.
"""
import collections, csv, glob, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ASM = os.path.join(ROOT, 'asm', 'stuntracefx')
ROM = os.path.join(ROOT, 'reference', 'stuntracefx', 'Stunt Race FX (USA) (Rev 1).sfc')
OPS = disasm.OPS

READ_M = {'lda', 'adc', 'and', 'cmp', 'eor', 'ora', 'sbc', 'bit'}
READ_X = {'ldx', 'ldy', 'cpx', 'cpy'}
WRITE_M = {'sta', 'stz'}
WRITE_X = {'stx', 'sty'}
RMW = {'asl', 'lsr', 'rol', 'ror', 'inc', 'dec', 'tsb', 'trb'}
BRANCH = {'bpl', 'bmi', 'bvc', 'bvs', 'bcc', 'bcs', 'bne', 'beq'}
FLOW = BRANCH | {'bra', 'brl', 'jmp', 'jml', 'jsr', 'jsl', 'rts', 'rtl', 'rti', 'brk', 'cop', 'wai', 'stp', 'mvn', 'mvp'}

# a cycle: (kind, role, condition) kind f fetch, i internal, r read, w write;
# role d the operand's data, p a pointer's byte, s the stack; a condition
# names when the cycle is made: dl (D's low byte not 0), ix (an index 16
# bits wide or crossing a page), tk (a branch taken)
F, I = ('f', '', None), ('i', '', None)


def cycles(mnem, mode, m16, x16):
    """The instruction's cycles, or None for one read no further (a block
    move, an interrupt, a wait)."""
    if mnem in ('mvn', 'mvp', 'brk', 'cop', 'wai', 'stp', 'wdm'):
        return None
    wm, wx = (2 if m16 else 1), (2 if x16 else 1)
    dl = ('i', '', 'dl')
    if mode in ('immm', 'immx'):
        return [F] + [F] * (wm if mode == 'immm' else wx)
    if mnem in ('rep', 'sep'):
        return [F, F, I]
    if mnem == 'xba':
        return [F, I, I]
    if mode in ('imp', 'acc') and mnem not in ('pha', 'phx', 'phy', 'php', 'phb', 'phk', 'phd', 'pla', 'plx', 'ply',
                                               'plp', 'plb', 'pld', 'rts', 'rtl', 'rti'):
        return [F, I]
    s = lambda k, n: [(k, 's', None)] * n
    if mnem in ('pha', 'phx', 'phy', 'php', 'phb', 'phk', 'phd'):
        n = {'pha': wm, 'phx': wx, 'phy': wx, 'phd': 2}.get(mnem, 1)
        return [F, I] + s('w', n)
    if mnem in ('pla', 'plx', 'ply', 'plp', 'plb', 'pld'):
        n = {'pla': wm, 'plx': wx, 'ply': wx, 'pld': 2}.get(mnem, 1)
        return [F, I, I] + s('r', n)
    if mnem == 'pea':
        return [F, F, F] + s('w', 2)
    if mnem == 'pei':
        return [F, F, dl, ('r', 'p', None), ('r', 'p', None)] + s('w', 2)
    if mnem == 'per':
        return [F, F, F, I] + s('w', 2)
    if mnem == 'jsr' and mode == 'abs':
        return [F, F, F, I] + s('w', 2)
    if mnem == 'jsr':          # (abs,X)
        return [F, F] + s('w', 2) + [F, I, ('r', 'p', None), ('r', 'p', None)]
    if mnem == 'jsl':
        return [F, F, F] + s('w', 1) + [I, F] + s('w', 2)
    if mnem == 'rts':
        return [F, I, I] + s('r', 2) + [I]
    if mnem == 'rtl':
        return [F, I, I] + s('r', 3)
    if mnem == 'rti':
        return [F, I, I] + s('r', 4)
    if mnem in ('jmp', 'jml'):
        p = ('r', 'p', None)
        return {'abs': [F, F, F], 'long': [F, F, F, F], 'absi': [F, F, F, p, p], 'absxi': [F, F, F, I, p, p],
                'absil': [F, F, F, p, p, p]}[mode]
    if mnem == 'bra':
        return [F, F, I]
    if mnem == 'brl':
        return [F, F, F, I]
    if mnem in BRANCH:
        return [F, F, ('i', '', 'tk')]
    # memory operands
    if mnem in READ_M or mnem in WRITE_M or mnem in RMW:
        w = wm
    elif mnem in READ_X or mnem in WRITE_X:
        w = wx
    else:
        return None
    write = mnem in WRITE_M or mnem in WRITE_X
    rmw = mnem in RMW
    p = ('r', 'p', None)
    ix = I if (write or rmw) else ('i', '', 'ix')
    pre = {'dp': [F, F, dl], 'dpx': [F, F, dl, I], 'dpy': [F, F, dl, I], 'dpi': [F, F, dl, p, p],
           'dpil': [F, F, dl, p, p, p], 'dpxi': [F, F, dl, I, p, p], 'dpiy': [F, F, dl, p, p, ix],
           'dpily': [F, F, dl, p, p, p], 'sr': [F, F, I], 'sriy': [F, F, I, p, p, I], 'abs': [F, F, F],
           'absx': [F, F, F, ix], 'absy': [F, F, F, ix], 'long': [F, F, F, F], 'longx': [F, F, F, F]}.get(mode)
    if pre is None:
        return None
    d = lambda k: [(k, 'd', None)] * w
    if rmw:
        return pre + d('r') + [I] + d('w')
    return pre + d('w' if write else 'r')


def variants(cyc, conds):
    """The cycle list with each condition in `conds` made, the others not."""
    return [c for c in cyc if c[2] is None or c[2] in conds]


# ---- the rules against an original's run ----

def table_check(log):
    """Each instruction of the run against the rules: opcode -> runs no
    rule gives."""
    seen, bad = collections.Counter(), collections.defaultdict(collections.Counter)
    cur, op = [], None

    def done():
        if op is None or not cur:
            return
        mnem, mode = OPS[op]
        pat = ''.join(cur)
        seen[op] += 1
        ok = False
        for m16 in (0, 1):
            for x16 in (0, 1):
                cy = cycles(mnem, mode, m16, x16)
                if cy is None:
                    ok = True
                    break
                for conds in ((), ('dl',), ('ix',), ('tk',), ('dl', 'ix')):
                    v = variants(cy, conds)
                    want = 'o' + ''.join({'f': 'n', 'i': 'i', 'r': 'r', 'w': 'w'}[c[0]] for c in v[1:])
                    if want == pat:
                        ok = True
                        break
                if ok:
                    break
            if ok:
                break
        if not ok and pat.endswith('iiwwwwrr'):
            seen['interrupt'] += 1        # an NMI or IRQ taken after it
            return
        if not ok and pat.endswith('iiwwwwrr'):
            seen['interrupt'] += 1        # an NMI or IRQ taken after it
            return
        if not ok:
            bad[op][pat] += 1
            if len(examples[op]) < 2:
                examples[op].append(pat)
    examples = collections.defaultdict(list)
    for line in open(log):
        k = line[0]
        if k == 'o':
            done()
            op = int(line.split()[2], 16)
            cur = ['o']
        elif k in 'nirw' and line[1] == ' ':
            cur.append(k)
        # x: the core's look for an interrupt before an instruction's last
        # cycle; an interrupt's own sequence begins with no opcode fetch and
        # is added to the instruction before it, which then fits no rule
    done()
    irq = seen.pop('interrupt', 0)
    irq = seen.pop('interrupt', 0)
    total = sum(seen.values())
    wrong = sum(sum(c.values()) for c in bad.values())
    print('cycasm: %d instructions of %d opcodes (%d with an interrupt after them); %d (%d opcodes) no rule gives' % (
        total, len(seen), irq, wrong, len(bad)))
    for op, pats in sorted(bad.items(), key=lambda kv: -sum(kv[1].values()))[:30]:
        print('  %02X %s %s: %s' % (op, OPS[op][0], OPS[op][1], ' '.join('%s:%d' % kv for kv in pats.most_common(3))))
    return wrong


# ---- the assembly ----

class Ins:
    __slots__ = ('pc', 'off', 'op', 'mnem', 'mode', 'size', 'opnd', 'len', 'line', 'label_before')


def read_asm():
    """Every 65816 routine of the filed assembly: {address: [Ins]} in text
    order, with a label marked where one stands before an instruction."""
    rom = open(ROM, 'rb').read()
    routines, bad = {}, []
    for path in sorted(glob.glob(os.path.join(ASM, '**', '*.asm'), recursive=True)):
        arch, pc, off, cur, label, ok = '65816', None, None, None, False, False
        for ln, raw in enumerate(open(path), 1):
            line = raw.split(';')[0].rstrip()
            m = re.match(r'\s*;\s*@routine\s+([0-9A-Fa-f]{2}):([0-9A-Fa-f]{4})', raw)
            if m:
                addr = int(m.group(1), 16) << 16 | int(m.group(2), 16)
                cur = routines.setdefault(addr, [])
                continue
            s = line.strip()
            if not s:
                continue
            m = re.match(r'%at\(\$([0-9A-Fa-f]+)\)', s)
            if m:
                pc = int(m.group(1), 16)
                off = ((pc >> 16) & 0x3F) * 0x8000 + (pc & 0x7FFF)
                ok = True
                continue
            m = re.match(r'%ramcode\(\$([0-9A-Fa-f]+),\s*\$([0-9A-Fa-f]+)\)', s)
            if m:
                pc, off, ok = int(m.group(1), 16), int(m.group(2), 16), True
                continue
            if s.startswith('%spc('):
                arch = 'spc700'
                continue
            if s.startswith('%spc_end') or s == 'arch 65816':
                arch = '65816'
                continue
            if s.startswith('arch '):
                arch = s.split()[1]
                continue
            if arch != '65816' or not ok or cur is None:
                continue
            if re.match(r'^[.\w]+:$', s):
                label = True
                continue
            w = s.split(None, 1)
            mn = w[0].lower()
            base = mn.split('.')[0]
            if base in ('db', 'dw', 'dl', 'dd'):
                n = {'db': 1, 'dw': 2, 'dl': 3, 'dd': 4}[base]
                items = re.findall(r'"[^"]*"|[^,]+', w[1]) if len(w) > 1 else []
                size = sum(len(x) - 2 if x.startswith('"') else n for x in items)
                pc += size
                off += size
                continue
            if base in ('incsrc', 'includeonce', 'org', 'base', 'macro', 'endmacro') or s.startswith('!') or '=' in s:
                continue
            if off >= len(rom):
                ok = False
                continue
            op = rom[off]
            mnem, mode = OPS[op]
            if {'jml': 'jmp'}.get(base, base) != {'jml': 'jmp'}.get(mnem, mnem):
                bad.append('%s:%d %s at %06X is %02X %s' % (os.path.relpath(path, ROOT), ln, base, pc, op, mnem))
                ok = False
                continue
            sz = mn.split('.')[1] if '.' in mn else ''
            if mode in ('immm', 'immx'):
                n = 1 + (1 if sz == 'b' else 2)
            else:
                n = 1 + disasm.FIXED[mode]
            ins = Ins()
            ins.pc, ins.off, ins.op, ins.mnem, ins.mode, ins.size = pc, off, op, mnem, mode, sz
            ins.opnd = int.from_bytes(rom[off + 1:off + n], 'little') if n > 1 else None
            ins.len, ins.line, ins.label_before = n, '%s:%d' % (os.path.relpath(path, ROOT), ln), label
            label = False
            cur.append(ins)
            pc += n
            off += n
    return routines, bad


def data_key(ins):
    """The low 16 bits of the address an instruction's operand names (the
    base of an indexed one), or None for no memory operand."""
    if ins.mode in ('dp', 'dpx', 'dpy', 'dpi', 'dpxi', 'dpiy', 'dpil', 'dpily', 'abs', 'absx', 'absy', 'long', 'longx'):
        return ins.opnd & 0xFFFF
    return None


def speed(addr):
    """A non-C access's clocks by its address (LoROM, slow ROM)."""
    if addr is None:
        return 8
    bank, a = addr >> 16 & 0xFF, addr & 0xFFFF
    if (bank & 0x40) == 0 and a < 0x8000:
        if 0x2000 <= a < 0x4000 or 0x4200 <= a < 0x6000:
            return 6
        if 0x4000 <= a < 0x4200:
            return 12
    return 8



# ---- the C beside the assembly ----

ACCESS = re.compile(r'(?<![\w.])(rd8|rd16|rd24|wr8|wr16|wr24)\s*\(')
MASKS = {0xFF, 0xFFFF, 0xFFFFFF, 0xFF00, 0x00FF}
CONTROL = re.compile(r'\b(if|else|while|for|do|switch|case|default|return|goto|break|continue)\b|\?|&&|\|\||^\s*\w+:', re.M)


def defines():
    """The decomp's header constants (hw.h and the rest): NAME -> value."""
    d = {}
    for h in glob.glob(os.path.join(ROOT, 'src', 'stuntracefx', 'include', '*.h')):
        for m in re.finditer(r'#define\s+(\w+)\s+\(?(0x[0-9A-Fa-f]+|\d+)u?\)?\s', open(h).read()):
            d[m.group(1)] = int(m.group(2), 0)
    return d


def close(text, i):
    d = 0
    for j in range(i, len(text)):
        d += {'(': 1, ')': -1}.get(text[j], 0)
        if d == 0:
            return j
    return len(text) - 1


def c_key(arg, defs):
    """The address a C access names, as written (its first number), or None."""
    for tok in re.findall(r'0x[0-9A-Fa-f]+|\b[A-Z][A-Z0-9_]+\b', arg):
        v = int(tok, 16) if tok.startswith('0x') else defs.get(tok)
        if v is None or v in MASKS:
            continue
        return v
    return None


def rom_read(rw, v):
    """Whether a C read is of the ROM (nativeshvc gives it no time: its
    cycle is the list's): 'yes', 'no', or 'maybe' where the bank is not
    written (a data bank in a variable)."""
    if rw != 'r' or v is None or (v & 0xFFFF) < 0x8000:
        return 'no'
    if v > 0xFFFF:
        return 'no' if (v >> 16) in (0x7E, 0x7F) else 'yes'
    return 'maybe'


def c_items(text, s, e, defs):
    """The C accesses in text[s:e]: (offset, end, rw groups [(rw, key)], width)."""
    out = []
    for m in ACCESS.finditer(text, s, e):
        o = text.index('(', m.start())
        c = close(text, o)
        args = text[o + 1:c]
        depth, cut = 0, len(args)
        for i, ch in enumerate(args):
            depth += {'(': 1, ')': -1}.get(ch, 0)
            if ch == ',' and depth == 0:
                cut = i
                break
        fn = m.group(1)
        out.append((m.start(), c + 1, fn[0], c_key(args[:cut], defs), int(fn[2:]) // 8))
    return out


def tagged_functions(defs):
    """{(file, function): routine address} from the decomp's @routine tags,
    with each body's span."""
    got = {}
    for path in glob.glob(os.path.join(ROOT, 'src', 'stuntracefx', '**', '*.c'), recursive=True):
        text = open(path).read()
        rel = os.path.relpath(path, ROOT)
        for m in re.finditer(r'@routine\s+([0-9A-Fa-f]{2}):([0-9A-Fa-f]{4})', text):
            end = text.find('*/', m.end())
            d = re.compile(r'^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;{]*\)\s*\{', re.M).search(text, end)
            if not d:
                continue
            b = d.end() - 1
            depth, j = 0, b
            while j < len(text):
                depth += {'{': 1, '}': -1}.get(text[j], 0)
                if depth == 0:
                    break
                j += 1
            got[(rel, d.group(1))] = (int(m.group(1), 16) << 16 | int(m.group(2), 16), b, j)
    return got


def asm_groups(ins, m16, x16):
    """An instruction's cycles split as [(group, cycles)]: group None for
    cycles that are no access of its operand, ('r'|'w', key) for the
    operand's data or a pointer's bytes."""
    cy = cycles(ins.mnem, ins.mode, m16, x16)
    if cy is None:
        return None
    key = data_key(ins)
    out = []
    for c in cy:
        g = None
        if c[1] == 'd':
            g = (c[0], key if ins.mode not in ('dpi', 'dpil', 'dpxi', 'dpiy', 'dpily', 'sriy') else None)
        elif c[1] == 'p':
            g = ('r', key)
        if out and out[-1][0] == g and g is not None:
            out[-1][1].append(c)
        else:
            out.append((g, [c]))
    return out


def lcs(a, b):
    """Pairs (i, j) of a longest common sequence of a and b."""
    n, m = len(a), len(b)
    if n * m > 4000000:
        return []
    L = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n - 1, -1, -1):
        for j in range(m - 1, -1, -1):
            L[i][j] = L[i + 1][j + 1] + 1 if a[i] == b[j] else max(L[i + 1][j], L[i][j + 1])
    i = j = 0
    out = []
    while i < n and j < m:
        if a[i] == b[j]:
            out.append((i, j))
            i += 1
            j += 1
        elif L[i + 1][j] >= L[i][j + 1]:
            i += 1
        else:
            j += 1
    return out


CLK = {'f': 8, 'i': 6}


def cyc_char(c, addr):
    """A cycle as a list digit: fetches 8 (slow ROM, or work RAM for code
    run there), internal 6, a non-C access by its address."""
    if c[0] == 'f':
        return '8'
    if c[0] == 'i':
        return '6'
    v = speed(addr)
    return {6: '6', 8: '8', 12: 'C'}[v]


def read_places(rec):
    """Every simple timing call the assembly reading gives a list:
    {place: (list, how)} and {place: why not}."""
    defs = defines()
    routines, _bad = read_asm()
    funcs = tagged_functions(defs)
    timing = {}
    with open(os.path.join(rec, 'tree', 'timing.csv')) as f:
        for r in csv.DictReader(f):
            timing[int(r['id'])] = r
    by_fn = collections.defaultdict(list)
    for tid, t in timing.items():
        by_fn[(t['file'], t['fn'])].append(tid)
    got, why = {}, {}
    texts = {}
    for (rel, fn), tids in by_fn.items():
        if (rel, fn) not in funcs:
            for t in tids:
                why['t%d' % t] = 'in a function with no @routine tag (a helper)'
            continue
        addr, bs, be = funcs[(rel, fn)]
        ins = routines.get(addr)
        if not ins:
            for t in tids:
                why['t%d' % t] = 'its routine is not in the filed assembly'
            continue
        text = texts.setdefault(rel, open(os.path.join(ROOT, rel)).read())
        acc = c_items(text, bs, be, defs)
        cg = []        # C groups: (rw, key) per access byte group
        for o, e, rw, key, width in acc:
            cg.append((rw, key, o, e, width))
        # the assembly's groups, widths unknown: the operand's own groups
        ag = []
        for k, x in enumerate(ins):
            key = data_key(x)
            if key is None:
                continue
            if x.mnem in RMW:
                ag += [(('r', key), k), (('w', key), k)]
            elif x.mnem in WRITE_M or x.mnem in WRITE_X:
                ag.append((('w', key if x.mode not in ('dpi', 'dpil', 'dpxi', 'dpiy', 'dpily') else None), k))
            elif x.mnem in READ_M or x.mnem in READ_X:
                if x.mode in ('dpi', 'dpil', 'dpxi', 'dpiy', 'dpily'):
                    ag.append((('r', key), k))       # the pointer
                    ag.append((('r', None), k))
                else:
                    ag.append((('r', key), k))
        pairs = lcs([(g[0], g[1]) for g in cg], [g[0] for g in ag])
        match = {i: ag[j][1] for i, j in pairs}
        for tid in tids:
            t = timing[tid]
            place = 't%d' % tid
            to = int(t['offset'])
            if t['name'] not in ('cyc', 'cycs'):
                why[place] = 'a timing call inside an expression (cyc_then, cyc_loop)'
                continue
            prev = max((i for i, g in enumerate(cg) if g[3] <= to), default=None)
            nxt = min((i for i, g in enumerate(cg) if g[2] >= int(t['end'])), default=None)
            lo = cg[prev][3] if prev is not None else bs + 1
            hi = cg[nxt][2] if nxt is not None else be
            seg = text[lo:hi]
            if CONTROL.search(re.sub(r'/\*.*?\*/', '', seg, flags=re.S)):
                why[place] = 'the C between its accesses branches'
                continue
            calls = [m.group(1) for m in re.finditer(r'(?<![\w.])(\w+)\s*\(', re.sub(r'/\*.*?\*/', '', seg, flags=re.S))
                     if m.group(1) not in ('cyc', 'cycs', 'sizeof')]
            if calls:
                why[place] = 'the C between its accesses calls %s' % calls[0]
                continue
            # the timing calls in the stretch, in order
            here = sorted((int(timing[x]['offset']), x) for x in tids if lo <= int(timing[x]['offset']) < hi)
            stated = []
            for _o, x in here:
                a = timing[x]['args'].split(',')[0].strip()
                if re.fullmatch(r'0x[68Cc]+', a) and timing[x]['name'] == 'cycs':
                    stated.append(sum({'6': 6, '8': 8, 'C': 12}[c] for c in a[2:].upper()))
                elif re.fullmatch(r'\d+', a):
                    stated.append(int(a))
                else:
                    stated = None
                    break
            if stated is None:
                why[place] = 'a timing call in its stretch states no number'
                continue
            if (prev is not None and prev not in match) or (nxt is not None and nxt not in match):
                why[place] = 'an access beside it is not matched in the assembly'
                continue
            k0 = match[prev] if prev is not None else None
            k1 = match[nxt] if nxt is not None else None
            if k0 is not None and k1 is not None and k1 < k0:
                why[place] = 'its accesses come in the other order in the assembly'
                continue
            span = ins[(k0 if k0 is not None else 0):(k1 + 1 if k1 is not None else len(ins))]
            # straight: no branch or label between, and to the routine's end only through its return
            inner = span[1:-1] if (k0 is not None and k1 is not None) else span[1:] if k0 is not None else span[:-1] if k1 is not None else span
            if any(x.label_before for x in span[1:]) or any(x.mnem in FLOW for x in inner):
                why[place] = 'the assembly between its accesses branches, jumps or has a label'
                continue
            if k1 is None:
                ends = [i for i, x in enumerate(span) if x.mnem in ('rts', 'rtl')]
                if not ends or any(x.mnem in FLOW for x in span[1 if k0 is not None else 0:ends[0]]):
                    why[place] = 'the routine does not return straight after it'
                    continue
                span = span[:ends[0] + 1]
            res = read_span(span, k0 is not None, k1 is not None, cg[prev] if prev is not None else None,
                            cg[nxt] if nxt is not None else None, stated)
            if isinstance(res, str):
                why[place] = res
                continue
            for (_o, x), lst in zip(here, res):
                got['t%d' % x] = (lst, '%s %s' % (span[0].line, 'straight'))
    return got, why


def read_span(span, has_prev, has_next, cprev, cnext, stated):
    """The cycles of the span between the C's two accesses, split by the
    clocks each timing call states: [list per call], or why not."""
    # each instruction's widths: an access's width from the C, else both
    choices = []
    for k, x in enumerate(span):
        opts = []
        for m16 in (0, 1):
            for x16 in (0, 1):
                if x.mode in ('immm',) and x.size and (x.size == 'w') != bool(m16):
                    continue
                if x.mode in ('immx',) and x.size and (x.size == 'w') != bool(x16):
                    continue
                gs = asm_groups(x, m16, x16)
                if gs is None:
                    return 'an instruction read no further (%s)' % x.mnem
                opts.append(gs)
        choices.append(opts)
    lists = set()

    def walk(k, acc, conds_done):
        if len(lists) > 8:
            return
        if k == len(span):
            for conds in ((), ('dl',), ('ix',), ('dl', 'ix')):
                lists.add(''.join(c for c, cond in acc if cond is None or cond in conds))
            return
        for gs in choices[k]:
            part = []
            for gi, (g, cyc) in enumerate(gs):
                first, last = k == 0 and has_prev, k == len(span) - 1 and has_next
                if first:
                    # after the C's access in this instruction
                    gidx = [i for i, (gg, _c) in enumerate(gs) if gg is not None and gg[0] == cprev[0]]
                    if not gidx:
                        return
                    if gi <= gidx[-1]:
                        continue
                if last:
                    gidx = [i for i, (gg, _c) in enumerate(gs) if gg is not None and gg[0] == cnext[0]]
                    if not gidx:
                        return
                    if gi >= gidx[0]:
                        break
                for c in cyc:
                    addr = None
                    if c[1] == 's':
                        addr = 0x0001FF
                    elif c[1] in ('d', 'p') and data_key(span[k]) is not None:
                        addr = span[k].opnd
                    part.append((cyc_char(c, addr), c[2]))
            walk(k + 1, acc + part, conds_done)
    walk(0, [], ())
    total = sum(stated)
    fit = set()
    for lst in lists:
        if sum({'6': 6, '8': 8, 'C': 12}[c] for c in lst) != total:
            continue
        pieces, at = [], 0
        for n in stated:
            c, j = 0, at
            while c < n and j < len(lst):
                c += {'6': 6, '8': 8, 'C': 12}[lst[j]]
                j += 1
            if c != n:
                break
            pieces.append(lst[at:j])
            at = j
        else:
            fit.add(tuple(pieces))
    if not fit:
        return 'no order of the instructions\' cycles fits the clocks stated'
    if len(fit) > 1:
        return 'more than one order of cycles fits the clocks stated'
    return list(next(iter(fit)))


def read_one(tid, timing, tids, text, bs, be, cg, match, ins, rom_set):
    """The lists of the timing calls in tid's stretch, the C's reads in
    rom_set being the ROM's (cycles, no boundary): (group, pieces, where)
    or why not."""
    t = timing[tid]
    to, te = int(t['offset']), int(t['end'])
    bound = [i for i in range(len(cg)) if i not in rom_set]
    prev = max((i for i in bound if cg[i][3] <= to), default=None)
    nxt = min((i for i in bound if cg[i][2] >= te), default=None)
    lo = cg[prev][3] if prev is not None else bs + 1
    hi = cg[nxt][2] if nxt is not None else be
    group = tuple(x for _o, x in sorted((int(timing[x]['offset']), x) for x in tids if lo <= int(timing[x]['offset']) < hi))
    if any(timing[x]['name'] not in ('cyc', 'cycs') for x in group):
        return 'a timing call inside an expression (cyc_then, cyc_loop) in its stretch'
    seg = re.sub(r'/\*.*?\*/', '', text[lo:hi], flags=re.S)
    if CONTROL.search(seg):
        return 'the C between its accesses branches'
    calls = [m.group(1) for m in re.finditer(r'(?<![\w.])(\w+)\s*\(', seg)
             if m.group(1) not in ('cyc', 'cycs', 'sizeof', 'rd8', 'rd16', 'rd24')]
    if calls:
        return 'the C between its accesses calls %s' % calls[0]
    inner = [i for i in range(len(cg)) if cg[i][2] >= lo and cg[i][3] <= hi]
    if any(i not in rom_set for i in inner):
        return 'the C between its accesses calls rd8'
    stated = []
    for x in group:
        a = timing[x]['args'].strip()
        if timing[x]['name'] == 'cycs' and re.fullmatch(r'0x[68Cc]+', a):
            stated.append(sum(DIG[c] for c in a[2:].upper()))
        elif timing[x]['name'] == 'cyc' and re.fullmatch(r'\d+', a):
            stated.append(int(a))
        else:
            return 'a timing call in its stretch states no number'
    if any(i not in match for i in inner + [j for j in (prev, nxt) if j is not None]):
        return 'an access beside it is not matched in the assembly'
    k0 = match[prev] if prev is not None else 0
    k1 = match[nxt] if nxt is not None else None
    if k1 is None:
        ends = [i for i in range(k0, len(ins)) if ins[i].mnem in ('rts', 'rtl')]
        if not ends:
            return 'the routine does not return after it'
        k1 = ends[0]
    if k1 < k0:
        return 'its accesses come in the other order in the assembly'
    span = ins[k0:k1 + 1]
    if any(x.label_before for x in span[1:]) or any(x.mnem in FLOW for x in span[:-1]) or \
            (span[-1].mnem in FLOW and nxt is not None):
        return 'the assembly between its accesses branches, jumps, calls or has a label'
    lists = span_lists(span, cg[prev][0] if prev is not None else None, cg[nxt][0] if nxt is not None else None)
    if lists is None:
        return 'an instruction there is read no further, or too many choices'
    fits = {sp for sp in (split(l, stated) for l in lists) if sp is not None}
    if not fits:
        return 'no order of the instructions\' cycles fits the clocks stated'
    if len(fits) > 1:
        return 'more than one order of cycles fits the clocks stated'
    return group, next(iter(fits)), span[0].line


def report(rec, out):
    got, why = read_places(rec)
    known = {}
    p = os.path.join(rec, 'assign.csv')
    if os.path.exists(p):
        with open(p) as f:
            for r in csv.DictReader(f):
                if r['status'] == 'exact' and r['place'].startswith('t') and '/' not in r['place'] and '=' not in r['place']:
                    known[r['place']] = '' if r['list'] == '-' else r['list']
    agree = sum(1 for k, (l, _h) in got.items() if k in known and known[k] == l)
    tested = sum(1 for k in got if k in known)
    wrong = [(k, got[k][0], known[k], got[k][1]) for k in got if k in known and known[k] != got[k][0]]
    new = {k: v for k, v in got.items() if k not in known}
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['place', 'list', 'how'])
        for k in sorted(new, key=lambda k: int(re.match(r't(\d+)', k).group(1))):
            w.writerow([k, new[k][0] or '-', new[k][1]])
    lines = ['cycasm: %d timing calls read from the assembly; of %d a recording also gave, %d agree (%.2f%%)' % (
        len(got), tested, agree, 100.0 * agree / max(1, tested)),
        'cycasm: %d lists for places no recording reaches written to %s' % (len(new), out),
        'cycasm: %d timing calls not read:' % len(why)]
    for k, n in collections.Counter(why.values()).most_common():
        lines.append('  %6d  %s' % (n, k))
    if wrong:
        lines.append('  disagreeing with the recording (first 15):')
        for k, a, b, h in wrong[:15]:
            lines.append('    %-8s asm %-20s run %-20s %s' % (k, a or '-', b or '-', h))
    rep = '\n'.join(lines)
    print(rep)
    with open(os.path.splitext(out)[0] + '.txt', 'w') as f:
        f.write(rep + '\n')
    return 0 if not wrong else 1


# ---- the C beside the assembly ----

ACCESS = re.compile(r'(?<![\w.])(rd8|rd16|rd24|wr8|wr16|wr24)\s*\(')
MASKS = {0xFF, 0xFFFF, 0xFFFFFF, 0xFF00, 0x00FF}
CONTROL = re.compile(r'\b(if|else|while|for|do|switch|case|default|return|goto|break|continue)\b|\?|&&|\|\||^\s*\w+\s*:(?!:)', re.M)
DIG = {'6': 6, '8': 8, 'C': 12}
INDIRECT = ('dpi', 'dpil', 'dpxi', 'dpiy', 'dpily', 'sriy')


def defines():
    """The decomp's header constants: NAME -> value."""
    d = {}
    for h in glob.glob(os.path.join(ROOT, 'src', 'stuntracefx', 'include', '*.h')):
        for m in re.finditer(r'#define\s+(\w+)\s+\(?(0x[0-9A-Fa-f]+|\d+)u?\)?\s', open(h).read()):
            d[m.group(1)] = int(m.group(2), 0)
    return d


def close(text, i):
    d = 0
    for j in range(i, len(text)):
        d += {'(': 1, ')': -1}.get(text[j], 0)
        if d == 0:
            return j
    return len(text) - 1


def c_key(arg, defs):
    """The address a C access names, as written (its first number), or None."""
    for tok in re.findall(r'0x[0-9A-Fa-f]+|\b[A-Z][A-Z0-9_]+\b', arg):
        v = int(tok, 16) if tok.startswith('0x') else defs.get(tok)
        if v is None or v in MASKS:
            continue
        return v
    return None


def rom_read(rw, v):
    """Whether a C read is of the ROM (nativeshvc gives it no time: its
    cycle is the list's): 'yes', 'no', or 'maybe' where the bank is not
    written (a data bank in a variable)."""
    if rw != 'r' or v is None or (v & 0xFFFF) < 0x8000:
        return 'no'
    if v > 0xFFFF:
        return 'no' if (v >> 16) in (0x7E, 0x7F) else 'yes'
    return 'maybe'


def c_accesses(text, s, e, defs):
    """The C's accesses in text[s:e], one per byte group: (rw, key, start, end)."""
    out = []
    for m in ACCESS.finditer(text, s, e):
        o = text.index('(', m.start())
        c = close(text, o)
        args = text[o + 1:c]
        depth, cut = 0, len(args)
        for i, ch in enumerate(args):
            depth += {'(': 1, ')': -1}.get(ch, 0)
            if ch == ',' and depth == 0:
                cut = i
                break
        v = c_key(args[:cut], defs)
        out.append((m.group(1)[0], None if v is None else v & 0xFFFF, m.start(), c + 1, rom_read(m.group(1)[0], v)))
    return sorted(out, key=lambda a: a[2])


def tagged_functions():
    """{(file, function): (routine address, body start, body end)} from the
    decomp's @routine tags."""
    got = {}
    fdef = re.compile(r'^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;{]*\)\s*\{', re.M)
    for path in glob.glob(os.path.join(ROOT, 'src', 'stuntracefx', '**', '*.c'), recursive=True):
        text = open(path).read()
        rel = os.path.relpath(path, ROOT)
        for m in re.finditer(r'@routine\s+([0-9A-Fa-f]{2}):([0-9A-Fa-f]{4})', text):
            d = fdef.search(text, text.find('*/', m.end()))
            if not d:
                continue
            b = d.end() - 1
            depth, j = 0, b
            while j < len(text):
                depth += {'{': 1, '}': -1}.get(text[j], 0)
                if depth == 0:
                    break
                j += 1
            got.setdefault((rel, d.group(1)), (int(m.group(1), 16) << 16 | int(m.group(2), 16), b, j))
    return got


def asm_accesses(ins):
    """The routine's operand accesses in text order: ((rw, key), index)."""
    out = []
    for k, x in enumerate(ins):
        key = data_key(x)
        if key is None:
            continue
        if x.mnem in RMW:
            out += [(('r', key), k), (('w', key), k)]
        elif x.mnem in WRITE_M or x.mnem in WRITE_X:
            if x.mode in INDIRECT:
                out.append((('r', key), k))
            out.append((('w', None if x.mode in INDIRECT else key), k))
        elif x.mnem in READ_M or x.mnem in READ_X:
            out.append((('r', key), k))
            if x.mode in INDIRECT:
                out.append((('r', None), k))
    return out


def lcs(a, b):
    """Pairs (i, j) of a longest common sequence of a and b."""
    n, m = len(a), len(b)
    if not n or not m or n * m > 4000000:
        return []
    L = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n - 1, -1, -1):
        Li, Ln = L[i], L[i + 1]
        for j in range(m - 1, -1, -1):
            Li[j] = Ln[j + 1] + 1 if a[i] == b[j] else max(Ln[j], Li[j + 1])
    i = j = 0
    out = []
    while i < n and j < m:
        if a[i] == b[j]:
            out.append((i, j))
            i += 1
            j += 1
        elif L[i + 1][j] >= L[i][j + 1]:
            i += 1
        else:
            j += 1
    return out


def digit(c, x):
    """A cycle as a list digit: a fetch 8 (slow ROM; work RAM for code run
    there), an internal cycle 6, an access the C does not make by its
    address (the stack's in work RAM)."""
    if c[0] == 'f':
        return '8'
    if c[0] == 'i':
        return '6'
    if c[1] == 's':
        return '8'
    if c[1] == 'd' and x.mode in ('abs', 'absx', 'absy', 'long', 'longx'):
        addr = x.opnd if x.mode in ('long', 'longx') else (x.pc & 0xFF0000) | x.opnd
        return {6: '6', 8: '8', 12: 'C'}[speed(addr)]
    return '8'


def span_lists(span, cut_first, cut_last):
    """Every cycle list the span makes, each instruction with each width
    and each optional cycle: from after the C's access `cut_first` (rw) in
    the first instruction to before `cut_last` in the last."""
    per = []
    for k, x in enumerate(span):
        opts = set()
        for m16 in (0, 1):
            for x16 in (0, 1):
                if x.mode == 'immm' and x.size and (x.size == 'w') != bool(m16):
                    continue
                if x.mode == 'immx' and x.size and (x.size == 'w') != bool(x16):
                    continue
                cy = cycles(x.mnem, x.mode, m16, x16)
                if cy is None:
                    return None
                for conds in ((), ('dl',), ('ix',), ('dl', 'ix')):
                    v = variants(cy, conds)
                    lo, hi = 0, len(v)
                    if k == 0 and cut_first:
                        idx = [i for i, c in enumerate(v) if c[0] == cut_first and c[1] in ('d', 'p')]
                        if not idx:
                            continue
                        lo = idx[-1] + 1
                    if k == len(span) - 1 and cut_last:
                        idx = [i for i, c in enumerate(v) if c[0] == cut_last and c[1] in ('d', 'p')]
                        if not idx:
                            continue
                        hi = idx[0]
                    opts.add(''.join(digit(c, x) for c in v[lo:hi]))
        if not opts:
            return None
        per.append(opts)
    out = {''}
    for opts in per:
        out = {a + b for a in out for b in opts}
        if len(out) > 4096:
            return None
    return out


def split(lst, stated):
    pieces, at = [], 0
    for n in stated:
        c, j = 0, at
        while c < n and j < len(lst):
            c += DIG[lst[j]]
            j += 1
        if c != n:
            return None
        pieces.append(lst[at:j])
        at = j
    return tuple(pieces) if at == len(lst) else None


def read_places(rec):
    """{place: (list, where)} for the simple timing calls the assembly
    gives, and {place: why not} for the rest."""
    defs = defines()
    routines, _ = read_asm()
    funcs = tagged_functions()
    timing = {}
    with open(os.path.join(rec, 'tree', 'timing.csv')) as f:
        for r in csv.DictReader(f):
            timing[int(r['id'])] = r
    by_fn = collections.defaultdict(list)
    for tid, t in timing.items():
        by_fn[(t['file'], t['fn'])].append(tid)
    got, why, texts = {}, {}, {}
    for (rel, fn), tids in by_fn.items():
        if (rel, fn) not in funcs:
            for t in tids:
                why['t%d' % t] = 'in a function with no @routine tag (a helper, a header)'
            continue
        addr, bs, be = funcs[(rel, fn)]
        ins = routines.get(addr)
        if not ins:
            for t in tids:
                why['t%d' % t] = 'its routine is not in the filed assembly'
            continue
        text = texts.setdefault(rel, open(os.path.join(ROOT, rel)).read())
        cg = c_accesses(text, bs, be, defs)
        ag = asm_accesses(ins)
        match = {i: ag[j][1] for i, j in lcs([(g[0], g[1]) for g in cg], [g[0] for g in ag])}
        done = set()
        for tid in sorted(tids, key=lambda t: int(timing[t]['offset'])):
            if tid in done:
                continue
            maybes = [i for i, g in enumerate(cg) if g[4] == 'maybe']
            results, reasons = set(), []
            # the reads whose bank the C does not name: as the ROM's, and not
            for rom_set in ({i for i, g in enumerate(cg) if g[4] != 'no'}, {i for i, g in enumerate(cg) if g[4] == 'yes'}):
                r = read_one(tid, timing, tids, text, bs, be, cg, match, ins, rom_set)
                if isinstance(r, str):
                    reasons.append(r)
                else:
                    results.add(r)
                if not maybes:
                    break
            if maybes and reasons and results:
                # one reading of a read whose bank is in a variable failed:
                # the other is not known to be the right one
                why['t%d' % tid] = 'beside a read whose bank is in a variable (the ROM\'s or not)'
                continue
            if len(results) == 1:
                group, pieces, where = next(iter(results))
                for x, lst in zip(group, pieces):
                    got['t%d' % x] = (lst, where)
                    done.add(x)
            elif len(results) > 1:
                why['t%d' % tid] = 'a read of the ROM or not (its bank in a variable) gives two lists'
            else:
                why['t%d' % tid] = reasons[0]
    return got, why


def report(rec, out):
    got, why = read_places(rec)
    known = {}
    p = os.path.join(rec, 'assign.csv')
    if os.path.exists(p):
        with open(p) as f:
            for r in csv.DictReader(f):
                if r['status'] == 'exact' and re.fullmatch(r't\d+', r['place']):
                    known[r['place']] = '' if r['list'] == '-' else r['list']
    tested = [k for k in got if k in known]
    wrong = [(k, got[k][0], known[k], got[k][1]) for k in tested if known[k] != got[k][0]]
    new = {k: v for k, v in got.items() if k not in known}
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['place', 'list', 'how'])
        for k in sorted(new, key=lambda k: int(k[1:])):
            w.writerow([k, new[k][0] or '-', new[k][1]])
    lines = ['cycasm: %d timing calls read from the assembly; of the %d a recording also gave, %d agree (%.2f%%)' % (
        len(got), len(tested), len(tested) - len(wrong), 100.0 * (len(tested) - len(wrong)) / max(1, len(tested))),
        'cycasm: %d lists for places no recording reaches, in %s' % (len(new), out),
        'cycasm: %d timing calls not read:' % len(why)]
    for k, n in collections.Counter(why.values()).most_common():
        lines.append('  %6d  %s' % (n, k))
    if wrong:
        lines.append('  disagreeing with the recording (first 20):')
        for k, a, b, h in wrong[:20]:
            lines.append('    %-8s asm %-22s run %-22s %s' % (k, a or '-', b or '-', h))
    rep = '\n'.join(lines)
    print(rep)
    with open(os.path.splitext(out)[0] + '.txt', 'w') as f:
        f.write(rep + '\n')
    return 0 if not wrong else 1


def main(argv):
    if len(argv) == 3 and argv[1] == '--table':
        return 1 if table_check(argv[2]) else 0
    if len(argv) >= 2 and argv[1] == '--asm':
        routines, bad = read_asm()
        print('cycasm: %d routines, %d instructions; %d places the text and the ROM part' % (
            len(routines), sum(len(v) for v in routines.values()), len(bad)))
        for b in bad[:10]:
            print('  ' + b)
        return 0
    if len(argv) >= 2 and not argv[1].startswith('-'):
        rec = os.path.abspath(argv[1])
        out = argv[argv.index('--out') + 1] if '--out' in argv else os.path.join(rec, 'asm.csv')
        return report(rec, out)
    if len(argv) >= 2 and not argv[1].startswith('-'):
        rec = os.path.abspath(argv[1])
        out = argv[argv.index('--out') + 1] if '--out' in argv else os.path.join(rec, 'asm.csv')
        return report(rec, out)
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
