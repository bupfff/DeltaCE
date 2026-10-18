#!/bin/bash
# Profile-guided code placement, for a release: the flash cache (8 KB, 2-way) misses ~20% less
# when the code that runs together sits in different cache sets. Builds the tree in the linker's
# order, runs the training workloads (scripts/layout/train-*.txt: calculus, algebra, graph
# sessions) on the emulator with 8-byte chunk traces (scripts/emu_linetrace.py), makes their
# temporal relationship graph (scripts/trg.c) and the best of several placements
# (scripts/codelayout.py), and writes <tree>/layout.ld: scripts/build_v15.sh uses it while the
# code is the one it was made for (any change to the code: run this again). About 10 minutes;
# needs the ROM (scripts/emu_env.sh).
# usage: scripts/make_layout.sh <source tree> [variant] [runs]   (KEEP=1: keep the traces, ~4 GB)
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TREE="$(cd "$1" && pwd)"; VARIANT="${2:-steps-units}"; RUNS="${3:-32}"
OUT="$ROOT/work/layout"; mkdir -p "$OUT"
cc -O2 -o "$ROOT/work/cachesim" "$ROOT/scripts/cachesim.c" && cc -O2 -o "$ROOT/work/trg" "$ROOT/scripts/trg.c" || exit 1
ORDER_FILE=none "$ROOT/scripts/build_v15.sh" "$TREE" layout-plain "$VARIANT" || exit 1
source "$ROOT/scripts/emu_env.sh"
rm -f "$KB/emu/states/layout-plain".*
python3 "$ROOT/scripts/emu_install.py" layout-plain layout-plain "$TREE" | tail -1
rm -rf "$ROOT/releases/mine/layout-plain".* "$ROOT/releases/mine/layout-plain-files"
echo "tracing the workloads"
for w in "$ROOT"/scripts/layout/train-*.txt; do
  n="$(basename "$w" .txt)"
  ( python3 "$ROOT/scripts/emu_linetrace.py" layout-plain "$TREE" "$w" "$OUT/$n.bin" 3 > "$OUT/$n.out" 2>&1 ) &
done
wait
lines=()
for w in "$ROOT"/scripts/layout/train-*.txt; do
  n="$(basename "$w" .txt)"
  grep -q '^cycles' "$OUT/$n.out" || { echo "the trace of $n failed:"; cat "$OUT/$n.out"; exit 1; }
  CACHESIM_DUMP="$OUT/$n.lines" "$ROOT/work/cachesim" -c 3 "$OUT/$n.bin" > /dev/null
  lines+=("$OUT/$n.lines")
done
"$ROOT/work/trg" -2 "$OUT/trg.bin" 383 0 "${lines[@]}" || exit 1
echo "placing ($RUNS runs)"
python3 "$ROOT/scripts/codelayout.py" make "$ROOT/work/layout-plain.ldmap" "$OUT/trg.bin" "$TREE/layout.ld" "$RUNS" "$OUT"/train-*.bin || exit 1
[ -n "${KEEP:-}" ] || rm -f "$OUT"/*.bin "$OUT"/*.lines
echo "wrote $TREE/layout.ld"
