#!/usr/bin/env python3
"""Install one of our builds into the emulator the way it is done on the calculator, from the ROM:
reset the archive (MEM > Reset > ARCHIVE > Both), send arTIfiCE, the PATCHED INST and the app
pieces, run arTIfiCE > INST, reset, start FlowCE, and save the ready console as a state.
(The project's tools/emu/install.py assumes an empty ROM and the original INST.)

usage: source scripts/emu_env.sh; python3 scripts/emu_install.py <build name> <state> <source tree>
  build name: releases/mine/<name>-files/AppIns*.8xv; its map: work/<name>.map (v15 builds)
  source tree: where tools/emu/cemu.py is (the build's own tree)
Screenshots of every step: work/emu-results/install-<state>/
"""
import glob, os, shutil, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
name, state, tree = sys.argv[1], sys.argv[2], os.path.abspath(sys.argv[3])
sys.path.insert(0, os.path.join(tree, 'tools', 'emu'))
from cemu import Emu, KB, ROM  # noqa: E402

apps = sorted(glob.glob(f'{ROOT}/releases/mine/{name}-files/AppIns*.8xv'))
mapfile = f'{ROOT}/work/{name}.map'
if not apps:
    sys.exit(f'no AppIns pieces in releases/mine/{name}-files')
if not os.path.exists(mapfile):
    if os.environ.get('NOMAP') != '1':
        sys.exit(f'no map {mapfile}: the tests read the console from memory through it (v15 builds '
                 'write one); NOMAP=1 installs anyway, for screenshot comparisons only')
    mapfile = None
shots = f'{ROOT}/work/emu-results/install-{state}'
os.makedirs(shots, exist_ok=True)

e = Emu(rom=ROM, shotdir=shots)
step = [0]
def shot(what):
    step[0] += 1
    return e.shot('%02d_%s' % (step[0], what))
try:
    e.run(4000); e.key('clear'); e.run(500); shot('boot')
    # MEM (2nd +) > 7:Reset... > ARCHIVE tab > 3:Both > 2:Reset
    e.key('2nd'); e.key('+'); e.run(800); shot('mem')
    e.key('7'); e.run(800); e.key('right'); e.run(500); shot('reset_archive_tab')
    e.key('3'); e.run(800); shot('reset_both_confirm')
    e.key('2'); e.wait_stable(timeout=120000); shot('archive_reset_done')
    e.key('clear'); e.run(1000)
    e.send(f'{ROOT}/installer/artific2.8xp', 'ram')
    e.send(f'{ROOT}/installer/INST.8xp', 'ram')
    for a in apps:
        e.send(a, 'archive')
    e.run(1000); shot('sent')
    e.key('prgm'); e.run(500); shot('prgm_menu')
    e.key('enter'); e.run(300); e.key('enter'); e.run(6000); shot('artifice_shell')
    e.run(300); e.key('enter', 40); e.run(3000); shot('inst_prompt')
    e.run(200); e.key('enter', 40); e.run(2000)
    e.wait_stable(timeout=600000); shot('inst_done')
    if not any('written' in l for l in e.debug_output()):
        sys.exit('INST did not finish writing (see %s)' % shots)
    # (not ENTER at "Success! Will now reset": a hardware reset is always safe, see install.py)
    e.reset(); e.run(6000)
    e.key('clear'); e.run(300)
    e.key('apps'); e.run(1000); shot('apps_menu')
    e.key('2'); e.run(1500); shot('splash')                     # 1:Finance 2:FlowCE after the reset
    e.key('enter'); e.run(1500)
    bmp = f'/tmp/emu_install_{os.getpid()}.bmp'
    e.cmd(f'screenshot {bmp}')
    d = open(bmp, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]
    top = d[off + 226 * 320 * 3: off + 236 * 320 * 3]           # status bar rows (BMP is bottom-up)
    b_, g_, r_ = (sum(top[i::3]) * 3 // len(top) for i in range(3))
    if sum(top) <= len(top) * 200 and not g_ - r_ > 40:          # FlowCE's status bar is light (or teal)
        shot('not_started')
        sys.exit('FlowCE did not start (see %s)' % shots)
    print('installed', len(apps), 'pieces ->', shot('console'))
    e.save(f'{KB}/emu/states/{state}.ce')
    if mapfile:
        shutil.copy(mapfile, f'{KB}/emu/states/{state}.map')
finally:
    e.close()
