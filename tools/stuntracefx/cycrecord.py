#!/usr/bin/env python3
"""cycrecord.py: the original's S-CPU cycles beside the C places that ran
there, for every script and play.

    cycrecord.py OUT [--jobs N] [--only NAME,...] [--scripts] [--plays]
                 [--frames A-B] [--timeout S] [--again] [--fresh]

Each script (tools/stuntracefx/scripts) and play (tools/stuntracefx/plays)
runs twice at once: the original on sfxbox (--cyclog) and the decomp on
nativeshvc, built from a copy of it with a mark at every probe and timing
call (cycprobe.py --marks; NSHVC_CYCLOG's M lines). cycpair pairs the two
streams as they come, through FIFOs, into OUT/runs/NAME.txt: each distinct
stretch between two paired accesses, the original's cycles and the C
places passed (cycpair.c says how).

    OUT/tree/            the marked copy (probes.csv, timing.csv beside it)
    OUT/runs/NAME.txt    a run's stretches; NAME.log its two sides' output
    OUT/stretches.txt    every run's stretches together (cycassign.py's input)
    OUT/record.txt       each run's counts, and what failed

A run whose result is there already is not run again (--again runs them
all; --fresh makes the marked copy and its build again too). --frames A-B
records frames A to B only (both sides count from 1). --scripts or --plays
alone runs only those; --only the names given (or paths to .sfx files). Progress is a line each run
and an overall line every 30 seconds.

The nativeshvc side is NSHVC=../nativeshvc (its tests/srfx/build.sh with
GAME_DIR), run as CYC_C_CMD, by default
    {srfx} --rom {rom} --script {script} --headless --out {out}
from this checkout, so a script's paths (its state) are read from here.
"""
import glob, hashlib, os, shlex, signal, subprocess, sys, threading, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(ROOT, 'tools', 'stuntracefx')
NSHVC = os.path.abspath(os.environ.get('NSHVC', os.path.join(ROOT, '..', 'nativeshvc')))
ROM = os.path.join(ROOT, 'reference', 'stuntracefx', 'Stunt Race FX (USA) (Rev 1).sfc')
CORE = os.path.join(ROOT, 'tools', 'toolchains', 'bsnes', 'bsnes_libretro.dylib')
C_CMD = os.environ.get('CYC_C_CMD', '{srfx} --rom {rom} --script {script} --headless --out {out}')


def run(cmd, **kw):
    p = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if p.returncode:
        raise SystemExit('cycrecord: %s failed:\n%s' % (' '.join(cmd[:3]), (p.stdout + p.stderr)[-3000:]))
    return p.stdout


def fingerprint():
    """The decomp's C and the marking tools, as they are now."""
    h = hashlib.sha1()
    files = sorted(glob.glob(os.path.join(ROOT, 'src', 'stuntracefx', '**', '*.[ch]'), recursive=True))
    for f in files + [os.path.join(TOOLS, 'cycprobe.py')]:
        h.update(f.encode())
        h.update(open(f, 'rb').read())
    return h.hexdigest()


def prepare(out, fresh):
    """The marked copy, its nativeshvc build, sfxbox with --cyclog and
    cycpair: (srfx, sfxbox, cycpair)."""
    tree = os.path.join(out, 'tree')
    stamp = os.path.join(out, 'tree.sha1')
    fp = fingerprint()
    old = open(stamp).read().strip() if os.path.exists(stamp) else None
    if fresh or old != fp:
        if old and old != fp and os.path.isdir(os.path.join(out, 'runs')) and os.listdir(os.path.join(out, 'runs')):
            raise SystemExit('cycrecord: the decomp changed since %s was recorded: its runs name probes of the old '
                             'copy. Record into a new OUT, or pass --fresh --again to start this one over.' % out)
        print('cycrecord: marking the decomp (cycprobe.py --marks)', flush=True)
        subprocess.run(['rm', '-rf', tree], check=True)
        print('  ' + run([sys.executable, os.path.join(TOOLS, 'cycprobe.py'), '--marks', tree]).strip().splitlines()[-1],
              flush=True)
        open(stamp, 'w').write(fp + '\n')
    print('cycrecord: building the marked copy on nativeshvc', flush=True)
    log = run(['sh', 'tests/srfx/build.sh'], cwd=NSHVC, env=dict(os.environ, GAME_DIR=tree))
    built = [l for l in log.splitlines() if l.startswith('built ')]
    if not built:
        raise SystemExit('cycrecord: build.sh did not say what it built:\n' + log[-2000:])
    srfx = os.path.join(NSHVC, built[-1][6:].strip())
    box = os.path.join(TOOLS, 'sfxbox')
    sfxbox = os.path.join(out, 'sfxbox')
    srcs = ['sfxbox.c', 'check.c', 'gsucall.c', 'spccall.c', 'gsu_clock.c']
    if not os.path.exists(sfxbox) or any(os.path.getmtime(os.path.join(box, s)) > os.path.getmtime(sfxbox) for s in srcs):
        run(['cc', '-O2', '-Wall', '-I', os.path.join(ROOT, 'tools', 'toolchains', 'src', 'bsnes', 'bsnes', 'target-libretro'),
             '-I', os.path.join(ROOT, 'src', 'stuntracefx', 'include'), '-Wl,-export_dynamic', '-o', sfxbox]
            + [os.path.join(box, s) for s in srcs] + ['-ldl'])
    pair = os.path.join(out, 'cycpair')
    src = os.path.join(TOOLS, 'cycpair.c')
    if not os.path.exists(pair) or os.path.getmtime(src) > os.path.getmtime(pair):
        run(['cc', '-O2', '-Wall', '-o', pair, src])
    return srfx, sfxbox, pair


def work(only, scripts, plays):
    """(name, path) of every script and play to run."""
    got = []
    if scripts:
        got += sorted(glob.glob(os.path.join(TOOLS, 'scripts', '*.sfx')))
    if plays:
        got += sorted(glob.glob(os.path.join(TOOLS, 'plays', '*.sfx')))
    named = [(os.path.splitext(os.path.basename(p))[0], p) for p in got]
    if only:
        # a path to a script file of its own: just that
        files = [os.path.abspath(o) for o in only if o.endswith('.sfx') and os.path.isfile(o)]
        only = [o for o in only if not (o.endswith('.sfx') and os.path.isfile(o))]
        if files and not only:
            return [(os.path.splitext(os.path.basename(p))[0], p) for p in files]
        want = set(only)
        named = [n for n in named if n[0] in want]
        missing = want - {n for n, _ in named}
        if missing:
            raise SystemExit('cycrecord: no script or play named %s' % ', '.join(sorted(missing)))
        named += [(os.path.splitext(os.path.basename(p))[0], p) for p in files]
    return named


def unblock(fifo):
    """Give a reader still opening `fifo` its end (a side that died before
    opening it); nothing when no one reads it."""
    try:
        fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
        os.close(fd)
    except OSError:
        pass


class Run:
    def __init__(self, name, script, out, tools, frames, timeout):
        self.name, self.script, self.timeout = name, script, timeout
        srfx, sfxbox, pair = tools
        d = os.path.join(out, 'work', name)
        subprocess.run(['rm', '-rf', d], check=True)
        os.makedirs(os.path.join(d, 'o'))
        os.makedirs(os.path.join(d, 'n'))
        self.dir = d
        self.of, self.cf = os.path.join(d, 'of'), os.path.join(d, 'cf')
        os.mkfifo(self.of)
        os.mkfifo(self.cf)
        self.result = os.path.join(out, 'runs', name + '.txt')
        self.part = self.result + '.part'
        self.log = open(os.path.join(out, 'runs', name + '.log'), 'w')
        ocmd = [sfxbox, '--core', CORE, '--rom', ROM, '--script', script, '--out', os.path.join(d, 'o'),
                '--cyclog', self.of]
        cenv = dict(os.environ, NSHVC_CYCLOG=self.cf)
        if frames:
            ocmd += ['--cyclog-frames', frames]
            cenv['NSHVC_CYCLOG_FRAMES'] = frames
        ccmd = shlex.split(C_CMD.format(srfx=shlex.quote(srfx), rom=shlex.quote(ROM), script=shlex.quote(script),
                                        out=shlex.quote(os.path.join(d, 'n'))))
        self.log.write('original: %s\nC: %s\n' % (' '.join(ocmd), ' '.join(ccmd)))
        self.log.flush()
        self.start = time.time()
        self.pair = subprocess.Popen([pair, self.of, self.cf, self.part], stdout=self.log, stderr=subprocess.STDOUT)
        self.orig = subprocess.Popen(ocmd, cwd=ROOT, stdout=self.log, stderr=subprocess.STDOUT)
        self.c = subprocess.Popen(ccmd, cwd=ROOT, env=cenv, stdout=self.log, stderr=subprocess.STDOUT)
        self.failed = []

    def poll(self):
        """None while running; else the result line or why it failed."""
        for p, fifo, who in ((self.orig, self.of, 'original'), (self.c, self.cf, 'C')):
            rc = p.poll()
            if rc is not None and rc != 0 and who not in self.failed:
                self.failed.append(who)
                unblock(fifo)
            if rc is not None:
                unblock(fifo)
        if time.time() - self.start > self.timeout:
            for p in (self.orig, self.c, self.pair):
                if p.poll() is None:
                    p.send_signal(signal.SIGKILL)
            for p in (self.orig, self.c, self.pair):
                p.wait()
            self.failed.append('timeout %d s' % self.timeout)
        if self.pair.poll() is None:
            return None
        for p in (self.orig, self.c):
            try:
                p.wait(timeout=30)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
                self.failed.append('a side outlived its pairing')
        self.log.close()
        if self.pair.returncode:
            self.failed.append('cycpair exit %d' % self.pair.returncode)
        if self.failed or not os.path.exists(self.part):
            return 'FAILED (%s)' % ', '.join(self.failed or ['no result'])
        os.replace(self.part, self.result)
        subprocess.run(['rm', '-rf', self.dir])
        return open(self.result).readline()[2:].strip()


def merge(out, names):
    """Every run's stretches into OUT/stretches.txt, counts added."""
    got = {}
    for n in names:
        p = os.path.join(out, 'runs', n + '.txt')
        if not os.path.exists(p):
            continue
        with open(p) as f:
            for line in f:
                if line[0] == '#':
                    continue
                c, rest = line.rstrip('\n').split('\t', 1)
                got[rest] = got.get(rest, 0) + int(c)
    with open(os.path.join(out, 'stretches.txt'), 'w') as f:
        f.write('# %d distinct stretches, %d in all, from %d runs\n' % (len(got), sum(got.values()), len(names)))
        for k, c in sorted(got.items(), key=lambda kv: -kv[1]):
            f.write('%d\t%s\n' % (c, k))
    return len(got), sum(got.values())


def main(argv):
    args = argv[1:]
    if not args or args[0].startswith('-'):
        print(__doc__)
        return 2
    out = os.path.abspath(args.pop(0))

    def opt(name, default=None):
        if name in args:
            i = args.index(name)
            v = args[i + 1]
            del args[i:i + 2]
            return v
        return default
    jobs = int(opt('--jobs', '4'))
    only = opt('--only')
    frames = opt('--frames')
    timeout = int(opt('--timeout', '3600'))
    flags = {a for a in args if a.startswith('--')}
    unknown = flags - {'--scripts', '--plays', '--again', '--fresh'} or [a for a in args if not a.startswith('--')]
    if unknown:
        raise SystemExit('cycrecord: what is %s?' % ' '.join(sorted(unknown)))
    scripts = '--scripts' in flags or '--plays' not in flags
    plays = '--plays' in flags or '--scripts' not in flags
    os.makedirs(os.path.join(out, 'runs'), exist_ok=True)
    todo = work(only.split(',') if only else None, scripts, plays)
    tools = prepare(out, '--fresh' in flags)
    names = [n for n, _ in todo]
    if '--again' not in flags:
        todo = [(n, p) for n, p in todo if not os.path.exists(os.path.join(out, 'runs', n + '.txt'))]
    print('cycrecord: %d runs to make (%d recorded already), %d at a time' % (len(todo), len(names) - len(todo), jobs),
          flush=True)
    t0, running, done, failed = time.time(), [], 0, []
    report = open(os.path.join(out, 'record.txt'), 'a')
    last = time.time()
    queue = list(todo)
    while queue or running:
        while queue and len(running) < jobs:
            n, p = queue.pop(0)
            running.append(Run(n, p, out, tools, frames, timeout))
        time.sleep(0.5)
        for r in list(running):
            res = r.poll()
            if res is None:
                continue
            running.remove(r)
            done += 1
            if res.startswith('FAILED'):
                failed.append(r.name)
            line = '%-32s %5.0f s  %s' % (r.name, time.time() - r.start, res)
            report.write(line + '\n')
            report.flush()
            print('[%d/%d %3.0f%%] %s' % (done, len(todo), 100.0 * done / len(todo), line), flush=True)
        if time.time() - last >= 30 and running:
            last = time.time()
            print('cycrecord: %d/%d done (%.0f%%), %.0f s in; running %s' % (
                done, len(todo), 100.0 * done / max(1, len(todo)), time.time() - t0,
                ', '.join('%s %.0f s' % (r.name, time.time() - r.start) for r in running)), flush=True)
    report.close()
    distinct, total = merge(out, names)
    print('cycrecord: %d runs in %.0f s, %d failed%s; %d distinct stretches (%d in all) in %s' % (
        done, time.time() - t0, len(failed), (': ' + ', '.join(failed)) if failed else '', distinct, total,
        os.path.join(out, 'stretches.txt')), flush=True)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
