#!/usr/bin/env python3
"""The step-by-step engine's size in the calculator app (Windows: python tools/dev/steps_size.py).

The steps build without a caller drops the engine at link time (LTO). This adds a probe to
src/main.cc's do_run (calls steps_of, steps_math, steps_title and steps_formula when a volatile int
is set), builds tools/dev/build.sh en-steps, restores main.cc and prints the room left. The
engine's cost is the room without it (52,424 B on 2026-10-08) minus this one.
"""
import os, subprocess, sys
repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
path = os.path.join(repo, 'src', 'main.cc')
orig = open(path, 'rb').read()
s = orig.decode('utf-8')
nl = '\r\n' if '\r\n' in s else '\n'
inc = '#include "answer.h"' + nl
call = '    giac::gen g(buf,contextptr);' + nl
assert s.count(inc) == 1 and s.count(call) == 1
s = s.replace(inc, inc + '#ifdef FLOWCE_STEPS // size probe (tools/dev/steps_size.py)' + nl + '#include "steps.h"' + nl
              + 'volatile int steps_probe;' + nl + '#endif' + nl)
s = s.replace(call, call + '#ifdef FLOWCE_STEPS' + nl + '    if (steps_probe){ std::vector<step_t> sv; steps_of(g,sv,contextptr);'
              ' for (unsigned i=0;i<sv.size();++i){ steps_math(sv[i],contextptr); steps_title(sv[i]); steps_formula(sv[i]); }'
              ' steps_probe=sv.size(); }' + nl + '#endif' + nl)
try:
    open(path, 'wb').write(s.encode('utf-8'))
    gitbash = 'C:/Program Files/Git/bin/bash.exe' if os.name == 'nt' else 'bash'
    r = subprocess.run([gitbash, os.path.join(repo, 'tools', 'dev', 'build.sh').replace(os.sep, '/'), 'en-steps'], capture_output=True, text=True)
    out = r.stdout + r.stderr
finally:
    open(path, 'wb').write(orig)
print('\n'.join(l for l in out.splitlines() if l.startswith('[build]')))
