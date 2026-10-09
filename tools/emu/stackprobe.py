#!/usr/bin/env python3
"""How deep FlowCE's stack goes on the calculator, per problem. main() moves the stack to 0xD2A800
(it grows down, toward userMem at 0xD1A881). The area under the idle frames is painted, the problem
is solved (with -s, its steps are shown too) and the lowest byte no longer painted gives the depth.

Usage (WSL): stackprobe.py <state> problems.txt [more ...] [-s] [-t S] [-n N]
  problems: plain inputs or numcheck corpora (kind ; expr ; ...), one per line
  -s  also the steps (enter on the answer, then clear)
  -t S  ON after S emulated seconds (default 90);  -n N  only the first N problems
Prints, per problem, the stack depth in bytes of the answer (and of the steps), then the maximum."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

TOP, LO, HI = 0xD2A800, 0xD1AA00, 0xD29F00   # stack top (main.cc); painted area: under the idle frames, over
                                              # what the OS keeps at the start of userMem (RAM variables)
PAINT = 0xA5
args = sys.argv[1:]
state, files, STEPS, T, N = args.pop(0), [], False, 90, 10 ** 9
while args:
    a = args.pop(0)
    if a == '-s': STEPS = True
    elif a == '-t': T = int(args.pop(0))
    elif a == '-n': N = int(args.pop(0))
    else: files.append(a)
KINDS = {'int': lambda a: f'integrate({a[0]},x)', 'defint': lambda a: f'integrate({a[0]},x,{a[1]},{a[2]})',
         'diff': lambda a: f'diff({a[0]},x)', 'lim': lambda a: f'limit({a[0]},x,{a[1]})',
         'limr': lambda a: f'limit({a[0]},x,{a[1]},1)', 'liml': lambda a: f'limit({a[0]},x,{a[1]},-1)',
         'sum': lambda a: f'sum({a[0]},n,{a[1]},inf)', 'solve': lambda a: f'solve({a[0]},x)',
         'eval': lambda a: a[0], 'raw': lambda a: a[0]}
inputs = []
for fn in files:
    for line in open(fn, encoding='utf-8', errors='replace'):
        line = line.rstrip('\r\n')
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        p = [x.strip() for x in line.split(' #')[0].split(' ; ')]
        if len(p) == 1:
            inputs.append(p[0])
        elif p[0] in KINDS:
            inputs.append(KINDS[p[0]](p[1:] + [''] * 3))
inputs = list(dict.fromkeys(inputs))[:N]


class Calc:
    def __init__(self):
        self.e, self.used, self.low = None, 0, LO

    def start(self):
        if self.e:
            self.e.close()
        self.e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
        self.polls = map_symbol('_getkey_polls', mapfile=self.e.mapfile)
        self.e.run(800)
        self.e.wait_idle(timeout=5000)
        self.used, self.low = 0, LO
        b = self.e.peek(0xD1A881, LO - 0xD1A881)
        assert not any(b[LO - 0xD1A881 - 16:]), 'data just under the painted area'

    def u24(self, a):
        b = self.e.peek(a, 3)
        return b[0] | b[1] << 8 | b[2] << 16

    def paint(self):
        a = self.low                                  # only what the last problem used was repainted
        while a < HI:
            n = min(4096, HI - a)
            self.e.poke(a, bytes([PAINT]) * n)
            a += n

    def depth(self):
        # from the top down: the stack's lowest byte is the last one written before 2 KB in a row
        # still painted (the bottom of userMem gets the session AppVar, saved before each problem)
        b = b''
        a = HI
        while a > LO:
            n = min(4096, a - LO)
            a -= n
            b = self.e.peek(a, n) + b
        low, run = HI, 0
        for i in range(len(b) - 1, -1, -1):
            if b[i] == PAINT:
                run += 1
                if run >= 2048:
                    break
            else:
                low, run = LO + i, 0
        self.low = LO                                 # repaint everything (the AppVar may have moved)
        return TOP - low if low < HI else 0

    def wait(self, limit_ms):
        c0, t = self.u24(self.polls), 0
        while t < limit_ms:
            step = 10 if t < 1000 else 50 if t < 10000 else 250
            self.e.run(step)
            t += step
            if self.u24(self.polls) != c0:
                return t
        return None

    def key(self, k):
        self.paint()
        self.e.key(k)
        t = self.wait(T * 1000)
        if t is None:
            self.e.key('on', hold=400)
            self.wait(30000)
            self.used = 20
        self.e.run(100)
        return self.depth(), t

    def solve(self, x):
        if not self.e or self.used >= 20:
            self.start()
        self.used += 1
        self.e.paste(x)
        r = [self.key('enter')]
        if STEPS:
            r.append(self.key('enter'))
            self.e.key('clear'); self.wait(20000); self.e.run(200)
        return r


calc, worst = Calc(), [0, 0]
try:
    for i, x in enumerate(inputs):
        r = calc.solve(x)
        for j, (d, t) in enumerate(r):
            worst[j] = max(worst[j], d)
        print(f'{i + 1:4} ' + '  '.join(f'{d:6} B {"%6.1f s" % (t / 1000) if t else "  (ON)"}' for d, t in r) + f'  {x}',
              flush=True)
finally:
    if calc.e:
        calc.e.close()
print(f'deepest: answer {worst[0]} B' + (f', steps {worst[1]} B' if STEPS else '') + f' (stack {TOP - 0xD1A881} B)')
