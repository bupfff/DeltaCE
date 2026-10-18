# Building DeltaCE on macOS

These are the scripts DeltaCE's releases are built and tested with. They run from a workspace
folder laid out like this (copy this folder to `<workspace>/scripts`):

```
<workspace>/
  scripts/                 this folder (tools/mac)
  toolchain/CEdev-v15.0/   CE C/C++ toolchain v15 (the build)
  toolchain/CEdev-v14.2/   CE C/C++ toolchain v14.2 (only its convbin, for the .b84 bundle)
  toolchain/CEmu/          CEmu, its headless test runner built and patched (tools/emu/patch_cemu.py)
  installer/INST.8xp       app_tools/INST.8xp from this repo (patched for OS 5.8.4)
  installer/artific2.8xp   arTIfiCE v2.1 (https://github.com/YvanTT/arTIfiCE/releases)
  rom/ti84ce.rom           your own calculator's ROM dump, for the emulator (never share it)
  work/deltace/            this repository (git clone --recursive)
  releases/mine/           the builds
```

- `build_v15.sh work/deltace DeltaCE-X steps-units` builds the app (APPLANG=en STEPS=1 UNITS=1),
  links it twice (at 0, then at its final flash address), packs it (`app_tools/pack_app.py`),
  cuts the AppIns pieces (`app_tools/make_segments.py`, padding a last piece under 1 KB, which
  INST stops on) and makes `releases/mine/DeltaCE-X.b84` (`make_bundle.sh`: the pieces, arTIfiCE
  and INST). It uses `layout.ld` when it was made for the code being built.
- `make_layout.sh work/deltace` makes `layout.ld`, the code's placement in flash: the training
  sessions in `layout/` run on the emulator with traces of the code they run, and the placement
  that misses the 8 KB flash cache least is kept (`cachesim.c`, `trg.c`, `codelayout.py`). Run it
  after the last code change of a release (about 3 minutes).
- `emu_install.py` installs a build in the emulator the way it is installed on a calculator
  (archive reset, arTIfiCE, INST) and saves the console as a state for the tests in `tools/emu/`;
  `source emu_env.sh` first.
