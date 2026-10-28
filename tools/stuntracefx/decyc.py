#!/usr/bin/env python3
"""decyc.py: take the restated instruction time (cyc, cycs) out of the
cartridge's C, for the behavior-exact standard.

    decyc.py [--apply] [--spc|--gsu] [FILE...]   (default: every .c under src/stuntracefx)

The C states the original's time between bus accesses with cyc(N) and
cycs(L); under the behavior-exact standard the main CPU's time passes only at
synchronisation points (console waits), so these go:

  cyc(...);  cycs(...);            the statement is removed
  cyc(...), X   /  X, cyc(...)     the comma operand is removed
  cyc_then(N, v)  cycs_then(L, v)  v
  cyc_loop(N, t)  cycs_loop(L, t)  t

An argument that makes an access or a call keeps it: cyc(f(x)); becomes
(void)(f(x));. Comments and strings are left as they are. Without --apply it
prints what it would change, file by file.
"""
import glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src', 'stuntracefx')
CALL = re.compile(r'\b(cycs?_then|cycs?_loop|cycs?)\s*\(')
# --spc: the sound CPU's stated time (spc_cyc), gone too in contract 2.0
SPC_CALL = re.compile(r'\b(spc_cyc)\s*\(')
# --gsu: the Super FX's stated time (the original's code fetches and clocks)
GSU_CALL = re.compile(r'\b(gsu_(?:codes?|cache|cyc)(?:_then|_loop)?)\s*\(')


def skip_comment_string(s, i):
    """the index after a comment or string starting at i, or i."""
    if s.startswith('/*', i):
        j = s.find('*/', i + 2)
        return len(s) if j < 0 else j + 2
    if s.startswith('//', i):
        j = s.find('\n', i)
        return len(s) if j < 0 else j
    if s[i] in '"\'':
        q, j = s[i], i + 1
        while j < len(s) and s[j] != q:
            j += 2 if s[j] == '\\' else 1
        return j + 1
    return i


def close_paren(s, i):
    """s[i] == '(': the index of its ')'."""
    depth = 0
    j = i
    while j < len(s):
        k = skip_comment_string(s, j)
        if k != j:
            j = k
            continue
        if s[j] == '(':
            depth += 1
        elif s[j] == ')':
            depth -= 1
            if depth == 0:
                return j
        j += 1
    raise ValueError('unbalanced')


def split_args(t):
    out, depth, cur, j = [], 0, '', 0
    while j < len(t):
        k = skip_comment_string(t, j)
        if k != j:
            cur += t[j:k]
            j = k
            continue
        ch = t[j]
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
        j += 1
    out.append(cur)
    return out


def has_call(t):
    return re.search(r'\b[A-Za-z_]\w*\s*\(', t) is not None


def prev_nonspace(s, i):
    """the last significant character before i: whitespace and comments skipped."""
    j = i - 1
    while True:
        while j >= 0 and s[j] in ' \t\n':
            j -= 1
        if j >= 1 and s[j] == '/' and s[j - 1] == '*':
            j = s.rfind('/*', 0, j - 1) - 1
            continue
        # a // comment ending the previous line
        ls = s.rfind('\n', 0, j + 1) + 1
        line = s[ls:j + 1]
        c = line.find('//')
        if c >= 0 and line.count('"', 0, c) % 2 == 0:
            j = ls + c - 1
            continue
        return j


def next_nonspace(s, i):
    """the next significant character at or after i: whitespace and comments skipped."""
    j = i
    while True:
        while j < len(s) and s[j] in ' \t\n':
            j += 1
        if s.startswith('/*', j) or s.startswith('//', j):
            j = skip_comment_string(s, j)
            continue
        return j


def transform(s):
    out, i, n = [], 0, 0
    while i < len(s):
        k = skip_comment_string(s, i)
        if k != i:
            out.append(s[i:k])
            i = k
            continue
        m = CALL.match(s, i)
        if not m or (i > 0 and (s[i - 1].isalnum() or s[i - 1] == '_')) or s[i - 1] == '_' and CALL is SPC_CALL:
            out.append(s[i])
            i += 1
            continue
        name = m.group(1)
        lp = m.end() - 1
        rp = close_paren(s, lp)
        args = split_args(s[lp + 1:rp])
        n += 1
        if name.endswith('_then') or name.endswith('_loop'):
            # the value (or the loop's test) stays; the time goes
            keep = transform(args[-1].strip())[0]      # a timing call inside it goes too
            lead = ','.join(args[:-1])
            out.append(('((void)(%s), (%s))' % (lead.strip(), keep)) if has_call(lead) else '(%s)' % keep)
            i = rp + 1
            continue
        arg = transform(s[lp + 1:rp].strip())[0]
        kept = '(void)(%s)' % arg if has_call(arg) else ''
        a = next_nonspace(s, rp + 1)
        b = prev_nonspace(s, i)
        ctrl = b >= 0 and ((s[b] == ')' and is_ctrl_close(s, b)) or re.search(r'\b(else|do)$', s[:b + 1]) is not None)
        if a < len(s) and s[a] == ';' and ctrl and not kept:
            # the whole body of an if, else, while or for: an empty statement stays
            out.append(';')
            i = a + 1
            continue
        if a < len(s) and s[a] == ';' and (b < 0 or s[b] in ';{}:' or ctrl):
            # a statement of its own
            if kept:
                out.append(kept + ';')
                i = a + 1
            else:
                # drop it with its line when the line holds nothing else
                lstart = s.rfind('\n', 0, i) + 1
                lend = s.find('\n', a)
                lend = len(s) if lend < 0 else lend
                cur = ''.join(out)
                line_before = cur[cur.rfind('\n') + 1:] if '\n' in cur else cur
                rest = s[a + 1:lend]
                if line_before.strip() == '' and rest.strip() == '':
                    out = [cur[:len(cur) - len(line_before)]]
                    i = lend + 1
                else:
                    i = a + 1
                    # a following space on the same line goes too
                    while i < len(s) and s[i] == ' ':
                        i += 1
            continue
        if a < len(s) and s[a] == ',':
            # the left operand of a comma
            if kept:
                out.append(kept)
                i = rp + 1
            else:
                i = next_nonspace(s, a + 1)
            continue
        if b >= 0 and s[b] == ',':
            # the right operand of a comma: drop ", cyc(...)"
            cur = ''.join(out).rstrip()
            out = [cur[:-1] + (', ' + kept if kept else '')]
            i = rp + 1
            continue
        raise ValueError('cyc in an unhandled place: %r' % s[max(0, i - 60):rp + 20])
    return ''.join(out), n


def is_ctrl_close(s, b):
    """s[b] == ')' closes an if/while/for/switch head (so what follows is a statement)."""
    depth, j = 0, b
    while j >= 0:
        if s[j] == ')':
            depth += 1
        elif s[j] == '(':
            depth -= 1
            if depth == 0:
                break
        j -= 1
    head = s[:j].rstrip()
    return re.search(r'\b(if|while|for|switch)$', head) is not None


def tidy(s):
    # an else, or a brace pair, left empty
    s = re.sub(r'\s*else\s*\{\s*\}', '', s)
    s = re.sub(r'\{\s*\}(?=\s*else)', '{ }', s)
    return s


def main(argv):
    global CALL
    if '--spc' in argv:
        CALL = SPC_CALL
    if '--gsu' in argv:
        CALL = GSU_CALL
    files = [a for a in argv[1:] if not a.startswith('--')] or sorted(glob.glob(os.path.join(SRC, '**', '*.c'), recursive=True))
    total = 0
    for p in files:
        s = open(p).read()
        if not CALL.search(s):
            continue
        t, n = transform(s)
        t = tidy(t)
        total += n
        print('%-60s %5d' % (os.path.relpath(p, ROOT), n))
        if '--apply' in argv:
            open(p, 'w').write(t)
    print('decyc: %d timing calls %s' % (total, 'removed' if '--apply' in argv else 'to remove'))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
