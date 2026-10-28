#!/usr/bin/env python3
"""gsuprice.py: the Super FX programs' prices, a load model each, fitted to
the original's recordings of their jobs.

    gsuprice.py ORIG_RUN [ORIG_RUN ...] [--write CART_C]

ORIG_RUN is sfxbox's SFXBOX_GSURUN record of a play: "g FRAME PC CLOCKS
INSN ROMBUF RAMBUF BUS FILL FLUSH RPIX RAM LOADS STORES FLUSHB RPIXR", each
job's Super FX clocks, those by kind, and its accesses to its RAM, all and
by what made them. The cartridge's chip (ports/common/snes/superfx.c) sees
the same accesses, which the C makes as the original does, and times a job
of a program as that program's load model:

    clocks = fixed + w_load x loads + w_store x stores
                   + w_flush x flush bytes + w_rpix x RPIX reads

The model is fitted by least squares over all the program's jobs, every
part non-negative, to the job's clocks less those it spent waiting for the
bus (the chip makes those waits itself, from the S-CPU's holds). A program
with fewer than MIN_JOBS jobs, or no fit, takes one multiplier on its
accesses at 5 clocks each (its clocks over that, summed over its jobs). A
program the recordings never run takes the default: the multiplier of all
the recorded jobs together.

Printed per program: jobs, the model, and its error on the original's jobs
(mean and 95th percentile of |model - clocks| as a share of the job's
clocks, the worst job, and the mean on the 5% longest jobs), then each
program whose model fits poorly (a mean error over POOR_MEAN or a 95th
percentile over POOR_P95), with its share of the recorded time, so the
table never hides one. --write puts
the table between the "gsuprice: begin" and "gsuprice: end" marks of
CART_C (ports/stuntracefx/front/superfx_cart.c).
"""
import collections, itertools, re, sys

ONE = 4096          # superfx.h GSU_PRICE_ONE
ACCESS = 5          # a RAM access at CLSR's 21 MHz, which the game always sets
BUS = 3             # the kinds' order after CLOCKS: INSN ROMBUF RAMBUF BUS FILL FLUSH RPIX
OPS = slice(8, 12)  # then RAM, and LOADS STORES FLUSHB RPIXR
MIN_JOBS = 20
POOR_MEAN, POOR_P95 = 0.10, 0.25
NAMES = ('fixed', 'load', 'store', 'flush', 'rpix')


def jobs(path):
    """per program: [(clocks less the bus waits, (loads, stores, flush, rpix))]"""
    d = collections.defaultdict(list)
    for line in open(path):
        p = line.split()
        if p[0] != 'g':
            continue
        k = [int(x) for x in p[4:]]
        if len(k) < 12:
            raise SystemExit('%s: no RAM access counts by kind (an sfxbox before them)' % path)
        d[p[2]].append((int(p[3]) - k[BUS], tuple(k[OPS])))
    return d


def solve(a, b):
    """a x = b by Gaussian elimination with pivoting; None when singular"""
    n = len(b)
    m = [row[:] + [b[i]] for i, row in enumerate(a)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(m[r][c]))
        if abs(m[p][c]) < 1e-9 * max(1.0, max(abs(v) for row in m for v in row[:n])):
            return None
        m[c], m[p] = m[p], m[c]
        for r in range(n):
            if r != c and m[r][c]:
                f = m[r][c] / m[c][c]
                for k in range(c, n + 1):
                    m[r][k] -= f * m[c][k]
    return [m[i][n] / m[i][i] for i in range(n)]


def fit(rows):
    """the non-negative least-squares load model of these jobs: [fixed, w...].
    Each set of non-zero parts is solved exactly; of the solutions all >= 0
    with the least squared error (within 1e-9 of the larger of it and the
    targets' sum of squares: rounding), the one with the fewest non-zero
    parts, then the first in the order fixed, load, store, flush, RPIX (as
    nativeshvc's docs/cost-profile.md)."""
    xs = [(1,) + ops for _, ops in rows]
    ys = [t for t, _ in rows]
    found = []
    for size in range(1, 6):
        for cols in itertools.combinations(range(5), size):
            a = [[sum(x[i] * x[j] for x in xs) for j in cols] for i in cols]
            b = [sum(x[i] * y for x, y in zip(xs, ys)) for i in cols]
            w = solve(a, b)
            if w is None or min(w) < 0:
                continue
            full = [0.0] * 5
            for c, v in zip(cols, w):
                full[c] = v
            err = sum((sum(f * v for f, v in zip(full, x)) - y) ** 2 for x, y in zip(xs, ys))
            found.append((err, cols, full))
    if not found:
        return None
    least = min(f[0] for f in found)
    tol = 1e-9 * max(abs(least), sum(y * y for y in ys), 1.0)
    tied = [f for f in found if f[0] <= least + tol]
    return min(tied, key=lambda f: (len(f[1]), f[1]))[2]


def multiplier(rows):
    cost = sum(sum(ops) * ACCESS for _, ops in rows)
    return sum(t for t, _ in rows) / cost if cost else None


def model_of(m):
    return m if isinstance(m, list) else [0.0] + [m * ACCESS] * 4


def errors(rows, model):
    """per job |model - clocks| / clocks, and the mean on the 5% longest"""
    e = []
    for t, ops in rows:
        pred = model[0] + sum(w * n for w, n in zip(model[1:], ops))
        e.append((t, abs(pred - t) / t if t else 0.0))
    shares = sorted(x for _, x in e)
    top = sorted(e, reverse=True)[:max(1, len(e) // 20)]
    return (sum(shares) / len(shares), shares[min(len(shares) - 1, int(len(shares) * 0.95))], shares[-1],
            sum(x for _, x in top) / len(top))


def main(argv):
    args = argv[1:]
    out = None
    if '--write' in args:
        i = args.index('--write')
        out = args[i + 1]
        del args[i:i + 2]
    if not args:
        print(__doc__)
        return 2
    allj = collections.defaultdict(list)
    for path in args:
        for k, v in jobs(path).items():
            allj[k].extend(v)
    models = {}
    for k, rows in allj.items():
        m = fit(rows) if len(rows) >= MIN_JOBS else None
        models[k] = m if m is not None else multiplier(rows)
    models = {k: v for k, v in models.items() if v is not None}
    default = multiplier([r for rows in allj.values() for r in rows]) or 1.0
    clocks = sum(t for rows in allj.values() for t, _ in rows) or 1
    print('default: %.3f x 5 clocks an access (all the recorded jobs)' % default)
    print('%-7s %6s %6s  %-58s %6s %6s %7s %7s' % ('program', 'jobs', 'time', 'model (clocks: fixed + per load, store, flush byte, RPIX read)',
                                                    'mean', 'p95', 'worst', 'top 5%'))
    poor = []
    for k in sorted(allj, key=lambda k: -sum(t for t, _ in allj[k])):
        rows = allj[k]
        share = sum(t for t, _ in rows) / clocks
        if k not in models:
            print('%-7s %6d %5.1f%%  default' % (k, len(rows), 100 * share))
            continue
        m = model_of(models[k])
        desc = ('%.0f + %.3f L + %.3f S + %.3f F + %.3f R' % tuple(m)) if isinstance(models[k], list) \
            else 'x %.3f on 5 clocks an access' % models[k]
        mean, p95, worst, top = errors(rows, m)
        if mean > POOR_MEAN or p95 > POOR_P95:
            poor.append((k, len(rows), share, mean, p95, worst))
        print('%-7s %6d %5.1f%%  %-58s %5.1f%% %5.1f%% %6.1f%% %6.1f%%' % (k, len(rows), 100 * share, desc,
                                                                      100 * mean, 100 * p95, 100 * worst, 100 * top))
    if poor:
        print('poor fits (mean over %d%% or p95 over %d%%): their jobs\' times are this far off' % (100 * POOR_MEAN, 100 * POOR_P95))
        for k, n, share, mean, p95, worst in poor:
            print('  POOR %s: %d jobs, %.2f%% of the recorded time; mean %.1f%%, p95 %.1f%%, worst %.1f%%' % (
                k, n, 100 * share, 100 * mean, 100 * p95, 100 * worst))
    else:
        print('poor fits: none')
    if out:
        text = open(out).read()
        dm = model_of(default)
        q = lambda m: '%du, {%du, %du, %du, %du}' % ((round(m[0]),) + tuple(round(w * ONE) for w in m[1:]))
        table = ['/* gsuprice: begin */',
                 'static const GsuPrice price_default = {%s};' % q(dm),
                 'static const GsuPriceRow prices[] = {']
        for k in sorted(models, key=lambda k: int(k, 16)):
            m = model_of(models[k])
            table.append('    {0x%s, {%s}},   /* %s */' % (k, q(m), ' '.join('%s %.3f' % nv for nv in zip(NAMES, m))))
        table += ['};', '/* gsuprice: end */']
        new, n = re.subn(r'/\* gsuprice: begin \*/.*?/\* gsuprice: end \*/', '\n'.join(table), text, flags=re.S)
        if n != 1:
            raise SystemExit('%s: the gsuprice marks not found once' % out)
        open(out, 'w').write(new)
        print('written: %s (%d rows)' % (out, len(models)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
