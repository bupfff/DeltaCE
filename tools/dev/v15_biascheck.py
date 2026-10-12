#!/usr/bin/env python3
"""v15_biascheck.py <lto.s>: refuses a CE toolchain v15 build that has a function hit by the frame
bias bug (tools/dev/wsl_build_v15.sh runs it on the LTO step's assembly).

With -mllvm -z80-frame-bias-policy=auto, a function with a big frame gets `lea ix, ix - N` after
__frameset (ix points inside the frame). When it also moves sp at run time (a variable-length
array, alloca), clang 19 ends it with `ld sp, ix` without the bias undone first: sp lands N bytes
too low, `pop ix` and `ret` read the wrong slots, and FlowCE quit (leaving a graph, the Variables
menu: 2026-10-12). Exit 1 and the function names when one is found.
"""
import re, sys

lines = open(sys.argv[1], encoding='utf-8', errors='replace').read().split('\n')
funcs, cur, start = [], None, 0
for i, l in enumerate(lines):
    m = re.match(r'^([A-Za-z_.$][\w.$]*):\s*(;.*)?$', l)
    if m and not m.group(1).startswith('.L'):
        if cur:
            funcs.append((cur, start, i))
        cur, start = m.group(1), i
if cur:
    funcs.append((cur, start, len(lines)))
bad = []
for name, s, e in funcs:
    body = [x.strip() for x in lines[s:e]]
    code = [x for x in body if x and not x.startswith(';') and not x.startswith('.')]
    bias = None
    # the bias: the instruction right after the frame setup (a later `push ix; lea ix, ix - N ...
    # pop ix` only reaches far locals for a moment)
    for j, x in enumerate(code[:8]):
        if '__frameset' in x and j + 1 < len(code):
            m = re.match(r'lea\s+ix, ix - (\d+)$', code[j + 1])
            if m:
                bias = int(m.group(1))
            break
    if bias is None:
        continue
    for j, x in enumerate(body):
        if re.match(r'ld\s+sp, ix$', x):
            prev = [y for y in body[max(0, j - 6):j] if not y.startswith(';')]
            if not any(re.match(r'lea\s+ix, ix \+ %d$' % bias, y) for y in prev):
                bad.append(name)
                break
if bad:
    print('frame bias bug (ld sp, ix with ix biased) in:', ', '.join(bad))
    print('make their variable-length arrays or alloca heap buffers or fixed arrays')
    sys.exit(1)
