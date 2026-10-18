"""The app's code placement in flash and what it costs in the flash cache (work/cachesim), for
scripts/make_layout.sh.
The linker's map (work/<build>.ldmap, scripts/build_v15.sh) gives the .text input sections in
order. A section ordering file (ld --section-ordering-file) puts the sections it lists first in
.text, in its order; the others follow in theirs, packed (no alignment on the eZ80). A layout is
replayed on a trace of the build (scripts/emu_linetrace.py ... 3: 8-byte chunk visits) with each
section's code at its new address.
  python3 scripts/codelayout.py make <plain build.ldmap> <trg.bin> <out.ld> <seeds> <c*.bin trace>...
  python3 scripts/codelayout.py check <layout.ld> <build.ldmap>   (exit 1: made for other code)"""
import re, os, sys, subprocess, tempfile, hashlib, multiprocessing
import numpy as np

SIM = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'work', 'cachesim')


class Text:
    def __init__(self, ldmap):
        secs, cur, on = [], None, False
        for line in open(ldmap, errors='replace'):
            if re.match(r'^\.text\s', line): on = True; continue
            if on and re.match(r'^\.\S', line): break
            if not on: continue
            m = re.match(r'^ (\.text\S*)\s*(?:(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+\S+)?\s*$', line)
            if m:
                cur = m.group(1)
                if m.group(2): secs.append((cur, int(m.group(2), 16), int(m.group(3), 16))); cur = None
                continue
            m = re.match(r'^\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+\S+\s*$', line)
            if m and cur:
                secs.append((cur, int(m.group(1), 16), int(m.group(2), 16))); cur = None
        self.secs = [s for s in secs if s[2]]
        for (n, a, z), (_, b, _) in zip(self.secs, self.secs[1:]):
            assert a + z == b, 'a gap after %s' % n
        self.t0 = self.secs[0][1]
        self.end = self.secs[-1][1] + self.secs[-1][2]
        self.starts = np.array([a for _, a, _ in self.secs])
        self.byname = {}
        for i, (n, a, z) in enumerate(self.secs): self.byname.setdefault(n, []).append(i)
        self.size = {n: sum(self.secs[i][2] for i in ix) for n, ix in self.byname.items()}

    def section_at(self, addr):
        """the index of the section holding each address (-1: none)"""
        addr = np.asarray(addr)
        k = np.searchsorted(self.starts, addr, side='right') - 1
        return np.where((addr >= self.t0) & (addr < self.end), k, -1)

    def place(self, order):
        """each section's new address: the listed names first (all their sections), then the rest"""
        first = set(order)
        seq = [i for n in order for i in self.byname[n]] + [i for i, s in enumerate(self.secs) if s[0] not in first]
        new, a = [0] * len(self.secs), self.t0
        for i in seq: new[i] = a; a += self.secs[i][2]
        return new

    def simulate(self, new, trace, chunks=True):
        """the misses of a trace (chunks: of 8-byte chunk visits) with the code at its new addresses"""
        with tempfile.TemporaryDirectory() as d:
            om, nm = os.path.join(d, 'old.map'), os.path.join(d, 'new.map')
            with open(om, 'w') as f, open(nm, 'w') as g:
                f.write('___app_start = %06X\n' % self.t0); g.write('___app_start = %06X\n' % self.t0)
                for i, (n, a, z) in enumerate(self.secs):
                    f.write('__s%d = %06X\n' % (i, a)); g.write('__s%d = %06X\n' % (i, new[i]))
            out = subprocess.run([SIM] + (['-c', '3'] if chunks else []) + [trace, om, nm], capture_output=True, text=True).stdout
        return int(re.search(r'misses (\d+) \(2-way\)', out).group(1))

    def write_order(self, order, path):
        with open(path, 'w') as f:
            f.write('.text :\n{\n' + ''.join('  *(%s)\n' % n for n in order) + '}\n')


def read_order(p):
    return re.findall(r'\*\((\.text[^)\s]*)\)', open(p).read())


def color_units(text, trg, glue='line', seed=None, noise=0.2, align=True):
    """A layout from a temporal relationship graph (scripts/trg.c: line pairs of the old layout and
    the misses each would cost sharing a set). Units: runs of sections (in the old order) that share
    a line the trace uses (glue='line'; 'run': all in the graph) stay together, so the code keeps
    its packing into lines. The units with the most weight first, each at the offset where its
    lines share sets with the least weight among the lines already placed and the fixed ones (the
    OS, constant data); the offset comes from a filler before it, one of the cold sections (no pair
    names them), which moves it by its size mod 4 KB, keeping its address mod 32 (align): its lines
    are the old ones, in other sets. With a seed, the weights that order the units are multiplied
    by exp(noise x a normal variate): another greedy run (they differ by a few %: pick the best).
    Returns the ordering (section names) and the filler's bytes."""
    r = np.fromfile(trg, dtype='<u4').reshape(-1, 3)
    l1, l2, w = r[:, 0].astype(np.int64), r[:, 1].astype(np.int64), r[:, 2].copy().view('<f4').astype(np.float64)
    names = [s[0] for s in text.secs]
    k1, k2 = text.section_at(l1 * 32 + 16), text.section_at(l2 * 32 + 16)
    keep = ~((k1 == k2) | ((k1 < 0) & (k2 < 0)))
    used = np.zeros(1 << 17, dtype=bool); used[l1] = True; used[l2] = True  # (lines the trace reuses)
    l1, l2, w, k1, k2 = l1[keep], l2[keep], w[keep], k1[keep], k2[keep]
    wk = np.zeros(len(text.secs))
    np.add.at(wk, k1[k1 >= 0], w[k1 >= 0]); np.add.at(wk, k2[k2 >= 0], w[k2 >= 0])
    dup = {n for n, ix in text.byname.items() if len(ix) > 1}
    # the units: (first, last) section indexes
    units, i = [], 0
    while i < len(text.secs):
        j = i
        if wk[i] and names[i] not in dup:
            while j + 1 < len(text.secs) and wk[j + 1] and names[j + 1] not in dup:
                b0 = text.secs[j + 1][1]
                if glue == 'line' and not (b0 % 32 and used[b0 >> 5]): break
                j += 1
        units.append((i, j)); i = j + 1
    a = np.concatenate([l1, l2]); b = np.concatenate([l2, l1]); ww = np.concatenate([w, w])
    o = np.argsort(a, kind='stable'); a, b, ww = a[o], b[o], ww[o]
    nl = 1 << 17
    ptr = np.searchsorted(a, np.arange(nl + 1))
    set_of = np.full(nl, -1, dtype=np.int64)
    fixed = np.unique(np.concatenate([l1[k1 < 0], l2[k2 < 0]]))
    set_of[fixed] = fixed & 127
    hot = [u for u in units if wk[u[0]]]
    uw = {u: wk[u[0]:u[1] + 1].sum() for u in hot}
    if seed is not None:
        rng = np.random.default_rng(seed)
        for u in hot: uw[u] *= float(np.exp(noise * rng.standard_normal()))
    hot.sort(key=lambda u: -uw[u])
    gl = np.unique(np.concatenate([l1[k1 >= 0], l2[k2 >= 0]]))
    gk = text.section_at(gl * 32 + 16)
    cold = {}  # the fillers by their size mod 4 KB
    for u in units:
        if not wk[u[0]]:
            for k in range(u[0], u[1] + 1): cold.setdefault(text.secs[k][2] % 4096, []).append(k)
    order, cur, filled = [], text.t0, 0
    for u in hot:
        s0 = text.secs[u[0]][1]
        m = (gk >= u[0]) & (gk <= u[1])
        L = gl[m]; off = L * 32 + 16 - s0
        cs = np.zeros((len(L), 128))
        for j, l in enumerate(L):
            nb, nw = b[ptr[l]:ptr[l + 1]], ww[ptr[l]:ptr[l + 1]]
            s = set_of[nb]; mm = s >= 0
            if mm.any(): cs[j] += np.bincount(s[mm], weights=nw[mm], minlength=128)
        cand = np.array([0] + sorted(z for z in cold if cold[z]), dtype=np.int64)
        if align:
            ok = (cur + cand - s0) % 32 == 0
            if ok.any(): cand = cand[ok]
        sets = ((cur + cand[None, :] + off[:, None]) >> 5) & 127
        cost = cs[np.arange(len(L))[:, None], sets].sum(0)
        z = int(cand[int(np.argmin(cost))])
        if z:
            f = cold[z].pop()
            order.append(names[f]); cur += text.secs[f][2]; filled += text.secs[f][2]
        set_of[L] = ((cur + off) >> 5) & 127
        for k in range(u[0], u[1] + 1):
            order.append(names[k]); cur += text.secs[k][2]
    return order, filled


def fingerprint(text):
    """the code's sections (names and sizes, in any order): a layout is for this code only"""
    return hashlib.sha1('\n'.join('%s %d' % (n, z) for n, a, z in sorted(text.secs)).encode()).hexdigest()[:16]


def check(layout, ldmap):
    m = re.search(r'sections fingerprint ([0-9a-f]+)', open(layout).read())
    try:
        return bool(m) and m.group(1) == fingerprint(Text(ldmap))
    except Exception:  # (a map it cannot read: not the code it was made for)
        return False


def _trial(args):
    text, trg, seed, traces = args
    order, filled = color_units(text, trg, 'line', seed)
    new = text.place(order)
    return seed, order, [text.simulate(new, t) for t in traces]


def make(ldmap, trg, out, seeds, traces):
    """the best of several greedy runs: the misses saved on each trace, as a share of its time
    (its cycles from <trace>.out, emu_linetrace.py's output), averaged over the traces"""
    text = Text(ldmap)
    base = [text.simulate(text.place([]), t) for t in traces]
    cyc = []
    for t in traces:
        m = re.search(r'cycles ([\d.]+) M', open(t[:-4] + '.out').read()) if os.path.exists(t[:-4] + '.out') else None
        cyc.append(float(m.group(1)) * 1e6 if m else None)
    def score(r):
        return sum((x - b) * 197 / c if c else (x - b) / b for x, b, c in zip(r, base, cyc)) / len(r)
    with multiprocessing.get_context('fork').Pool(min(8, os.cpu_count() or 1)) as pool:
        runs = pool.map(_trial, [(text, trg, s, traces) for s in range(seeds)])
    runs.sort(key=lambda r: score(r[2]))
    seed, order, r = runs[0]
    names = [os.path.basename(t)[:-4] for t in traces]
    summary = '  '.join('%s %+.1f%%' % (n, 100.0 * (x - b) / b) for n, x, b in zip(names, r, base))
    with open(out, 'w') as f:
        f.write('/* DeltaCE\'s code placement (scripts/make_layout.sh) for the code with sections fingerprint %s:\n'
                '   the best of %d runs (seed %d), flash cache misses %s, time %+.1f%% */\n'
                % (fingerprint(text), seeds, seed, summary, 100 * score(r)))
        f.write('.text :\n{\n' + ''.join('  *(%s)\n' % n for n in order) + '}\n')
    print('seed %d: %s, time %+.1f%% (median run %+.1f%%)' % (seed, summary, 100 * score(r), 100 * score(runs[len(runs) // 2][2])))


if __name__ == '__main__':
    if sys.argv[1] == 'make':
        make(sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), sys.argv[6:])
    elif sys.argv[1] == 'check':
        sys.exit(0 if check(sys.argv[2], sys.argv[3]) else 1)
