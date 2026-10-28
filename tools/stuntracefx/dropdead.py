#!/usr/bin/env python3
"""dropdead.py: remove the parameters and variables the cyc removal left dead.

    dropdead.py [--apply] FILE...

From clang's -Wunused-parameter / -Wunused-variable / -Wunused-but-set-variable
on each file:
  a parameter    is removed from its function's definition and every
                 declaration, and its argument from every call in src (the
                 call's argument must make no call; else it is reported)
  a variable     is removed with its declaration and every assignment to it
                 whose right side makes no call (else it is reported)
Prints what it does or would do, and what it leaves.
"""
import glob, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from decyc import close_paren, split_args, has_call, skip_comment_string  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, 'src', 'stuntracefx')
INC = ['-I', os.path.join(SRC, 'include'), '-I', os.path.join(ROOT, 'ports', 'stuntracefx', 'front'),
       '-I', os.path.join(ROOT, '..', 'nativeshvc', 'include')] + sum((['-I', d] for d in os.environ.get('CYC_INC', '').split(':') if d), [])


def warnings(path):
    p = subprocess.run(['clang', '-fsyntax-only', '-std=gnu11', '-Wunused-parameter', '-Wunused-variable',
                        '-Wunused-but-set-variable'] + INC + [path], capture_output=True, text=True)
    out = []
    for m in re.finditer(r'^(.*?):(\d+):(\d+): warning: (unused parameter|unused variable|variable) \'(\w+)\'', p.stderr, re.M):
        if os.path.abspath(m.group(1)) == os.path.abspath(path):
            out.append((int(m.group(2)), int(m.group(3)), 'param' if m.group(4) == 'unused parameter' else 'var', m.group(5)))
    return out


def offset(text, line, col):
    return sum(len(l) + 1 for l in text.split('\n')[:line - 1]) + col - 1


def enclosing_function(text, pos):
    """(name, '(' index) of the function definition whose parameter list holds pos."""
    depth, j = 0, pos
    while j >= 0:
        if text[j] == ')':
            depth += 1
        elif text[j] == '(':
            if depth == 0:
                m = re.search(r'(\w+)\s*$', text[:j])
                return m.group(1), j
            depth -= 1
        j -= 1
    return None, None


def all_files():
    return sorted(glob.glob(os.path.join(SRC, '**', '*.[ch]'), recursive=True))


def drop_param(path, line, col, name, apply_, log):
    text = open(path).read()
    pos = offset(text, line, col)
    fn, lp = enclosing_function(text, pos)
    if not fn:
        log.append('  ? %s:%d %s: no function found' % (os.path.relpath(path, ROOT), line, name))
        return
    params = split_args(text[lp + 1:close_paren(text, lp)])
    idx = next((k for k, p in enumerate(params) if re.search(r'\b%s\s*$' % re.escape(name), p.strip())), None)
    if idx is None:
        log.append('  ? %s:%d %s: not found in %s' % (os.path.relpath(path, ROOT), line, name, fn))
        return
    static = re.search(r'\bstatic\b[^;{}]*\b%s\s*\($' % re.escape(fn), text[:lp + 1]) is not None
    # a static function in a header is in every file that includes it
    files = [path] if static and not path.endswith('.h') else all_files()
    edits, bad = {}, []
    pat = re.compile(r'(?<![\w.>])%s\s*\(' % re.escape(fn))
    for f in files:
        t = open(f).read()
        out, i, changed = [], 0, False
        while True:
            m = pat.search(t, i)
            if not m:
                out.append(t[i:])
                break
            # skip comments and strings between i and the match
            j = i
            inside = False
            while j < m.start():
                k = skip_comment_string(t, j)
                if k != j:
                    if k > m.start():
                        inside = True
                        break
                    j = k
                else:
                    j += 1
            p0 = m.end() - 1
            p1 = close_paren(t, p0)
            if inside:
                out.append(t[i:p1 + 1])
                i = p1 + 1
                continue
            args = split_args(t[p0 + 1:p1])
            if len(args) != len(params) or idx >= len(args):
                out.append(t[i:p1 + 1])
                i = p1 + 1
                continue
            a = args[idx]
            decl = re.search(r'^\s*(const\s+)?(unsigned|int|uint\d+_t|int\d+_t|uint32_t|uint16_t|uint8_t|long|short|char|signed)\b', a) is not None
            if not decl and has_call(a):
                bad.append('%s:%d' % (os.path.relpath(f, ROOT), t.count('\n', 0, p0) + 1))
                out.append(t[i:p1 + 1])
                i = p1 + 1
                continue
            rest = [x for k, x in enumerate(args) if k != idx]
            new = ', '.join(x.strip() for x in rest) if rest else ('void' if decl else '')
            out.append(t[i:p0 + 1] + new + ')')
            i = p1 + 1
            changed = True
        if changed:
            edits[f] = ''.join(out)
    if bad:
        log.append('  left %s(%s): an argument makes a call at %s' % (fn, name, ', '.join(bad)))
        return
    log.append('  param %s(%s) in %d file(s)' % (fn, name, len(edits)))
    if apply_:
        for f, t in edits.items():
            open(f, 'w').write(t)


def drop_var(path, line, col, name, apply_, log):
    text = open(path).read()
    pos = offset(text, line, col)
    # the function body holding it
    open_ = text.rfind('\n{', 0, pos)
    if open_ < 0:
        log.append('  ? %s:%d %s: no body' % (os.path.relpath(path, ROOT), line, name))
        return
    depth, j = 0, open_ + 1
    while j < len(text):
        k = skip_comment_string(text, j)
        if k != j:
            j = k
            continue
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    body = text[open_:j + 1]
    # assignments: name = expr;  name += expr; ... and in declarations
    uses = list(re.finditer(r'(?<![\w.>])%s\s*(=|\+=|-=|\|=|&=|\^=)(?!=)([^;]*);' % re.escape(name), body))
    for m in uses:
        if has_call(m.group(2)):
            log.append('  left %s:%d %s: an assignment makes a call' % (os.path.relpath(path, ROOT), line, name))
            return
    nb = body
    for m in reversed(uses):
        s0, s1 = m.start(), m.end()
        stmt_start = max(nb.rfind(';', 0, s0), nb.rfind('{', 0, s0), nb.rfind('}', 0, s0)) + 1
        between = nb[stmt_start:s0]
        if between.strip() == '':
            # a statement of its own: drop it (with its line if empty)
            ls = nb.rfind('\n', 0, s0) + 1
            le = nb.find('\n', s1)
            if nb[ls:s0].strip() == '' and nb[s1:le].strip() == '':
                nb = nb[:ls] + nb[le + 1:]
            else:
                nb = nb[:s0] + nb[s1:]
        else:
            # inside a declaration list: "T a = 1, name = 2;" -> drop ", name = 2"
            seg = nb[stmt_start:s1]
            seg2 = re.sub(r',\s*%s\s*=[^,;]*' % re.escape(name), '', seg, count=1)
            seg2 = re.sub(r'(?<![\w])%s\s*=[^,;]*,\s*' % re.escape(name), '', seg2, count=1) if seg2 == seg else seg2
            nb = nb[:stmt_start] + seg2 + nb[s1:]
    # assignments inside a comma list ("i++, name = 0" in a for, "(name = 1, x)")
    nb = re.sub(r',\s*%s\s*=(?!=)[^,;)]*' % re.escape(name), '', nb)
    nb = re.sub(r'(?<![\w.>])%s\s*=(?!=)[^,;)]*,\s*' % re.escape(name), '', nb)
    # a declaration with an initializer that makes no call: "T name = e;" (also static const arrays)
    m = re.search(r'\n[ \t]*(?:static\s+)?(?:const\s+)?[\w ]+?\b%s\s*(?:\[[^\]]*\])?\s*=\s*([^;]*);[^\n]*(?=\n)' % re.escape(name), nb)
    if m and not has_call(m.group(1)) and ',' not in m.group(0).split('=')[0]:
        nb = nb[:m.start()] + nb[m.end():]
    # one of a declaration list: "T a, name = e;" / "T name = e, b;"
    nb = re.sub(r'(\b(?:unsigned|int|uint\d+_t|int\d+_t)\s+[^;]*?),\s*%s\s*=[^,;]*(?=[,;])' % re.escape(name), r'\1', nb, count=1)
    # a bare declaration: "T name;" or "T a, name;" / "T name, b;"
    nb = re.sub(r'\n[ \t]*[\w ]+\b%s\s*;[ \t]*(?=\n)' % re.escape(name), '', nb, count=1)
    nb = re.sub(r',\s*%s\s*(?=[,;])' % re.escape(name), '', nb, count=1)
    nb = re.sub(r'(\b(?:unsigned|int|uint\d+_t|int\d+_t)\s+)%s\s*,\s*' % re.escape(name), r'\1', nb, count=1)
    if re.search(r'(?<![\w.>])%s\b' % re.escape(name), nb):
        log.append('  left %s:%d %s: still referenced after the edit' % (os.path.relpath(path, ROOT), line, name))
        return
    log.append('  var %s:%d %s' % (os.path.relpath(path, ROOT), line, name))
    if apply_:
        open(path, 'w').write(text[:open_] + nb + text[j + 1:])


def main(argv):
    apply_ = '--apply' in argv
    files = [a for a in argv[1:] if not a.startswith('--')]
    for path in files:
        log = []
        # one at a time, re-reading the warnings, since each edit moves the rest
        for _ in range(60):
            ws = warnings(path)
            done = False
            for line, col, kind, name in ws:
                before = len(log)
                (drop_param if kind == 'param' else drop_var)(path, line, col, name, apply_, log)
                if apply_ and log[-1].startswith(('  param', '  var')):
                    done = True
                    break
            if not apply_ or not done:
                break
        print(os.path.relpath(path, ROOT))
        for l in dict.fromkeys(log):
            print(l)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
