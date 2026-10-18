"""A trace of the flash lines entered (patch_cemu.py 8: u32 line numbers, address >> 5) while a list
of problems is evaluated, for cache simulations (scripts/cachesim.c). With <bits> (3: 8-byte chunks,
patch_cemu.py 10) the first and last address read in each visit to a chunk of 2^bits bytes instead.
A problem may go on with actions after '|' (a graph: 'plot(x^3)|h:right:2000|h:+:1500'): h:key:ms
a key held for ms, k:key a key press; then clear (the graph closes).
usage: source scripts/emu_env.sh; python3 scripts/emu_linetrace.py <state> <tree> <problems> <out.bin> [bits]"""
import os, sys
state, tree, pf, out = sys.argv[1], os.path.abspath(sys.argv[2]), sys.argv[3], sys.argv[4]
bits = int(sys.argv[5]) if len(sys.argv) > 5 else 0
sys.path.insert(0, os.path.join(tree, 'tools', 'emu'))
from cemu import Emu, KB  # noqa: E402
probs = [l.rstrip('\n') for l in open(pf) if l.strip()]
e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
def run(p):
    acts = p.split('|')
    e.paste(acts[0]); e.run(300); e.wait_idle(timeout=600000); e.run(100)
    e.key('enter'); e.wait_idle(step=10, timeout=120000)
    if len(acts) == 1: return
    e.run(300)
    for a in acts[1:]:
        q = a.split(':')
        if q[0] == 'h':
            e.cmd(f'keydown {q[1]}'); e.run(int(q[2])); e.cmd(f'keyup {q[1]}'); e.wait_idle(step=10, timeout=60000); e.run(200)
        elif q[0] == 'k':
            e.key(q[1]); e.wait_idle(step=10, timeout=60000); e.run(200)
    e.key('clear'); e.wait_idle(step=10, timeout=60000); e.run(300)
try:
    e.run(800); e.wait_idle(timeout=600000); e.run(200)
    e.paste('1'); e.run(300); e.key('enter'); e.wait_idle(timeout=600000); e.run(300)
    for p in probs[:1]:  # (warm: the first problem untraced)
        run(p)
    s0 = e.stats()
    e.cmd(f'chunktrace {bits}')
    e.cmd(f'linetrace on {os.path.abspath(out)}')
    for p in probs[1:]:
        run(p)
    e.cmd('linetrace off')
    s1 = e.stats()
    print('cycles %.1f M  misses %.1f K' % ((s1['cycles'] - s0['cycles']) / 1e6, (s1['misses'] - s0['misses']) / 1e3))
finally:
    e.close()
