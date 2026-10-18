#!/bin/bash
# Package AppIns*.8xv pieces with the patched installer (installer/INST.8xp, artific2.8xp) into a
# .b84 for TI Connect CE, then check the result.
# usage: scripts/make_bundle.sh <dir with AppIns*.8xv> <out.b84>
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
D="$1"; OUT="$2"
CONVBIN="$ROOT/toolchain/CEdev-v14.2/bin/convbin"
args=()
for f in $(ls "$D"/AppIns*.8xv | sort); do args+=(-i "$f"); done
[ ${#args[@]} -gt 0 ] || { echo "no AppIns*.8xv in $D"; exit 1; }
"$CONVBIN" -j 8x -k b84 "${args[@]}" -i "$ROOT/installer/artific2.8xp" -i "$ROOT/installer/INST.8xp" -o "$OUT" >/dev/null
# check: every piece and the patched installer are in it, unchanged
T="$(mktemp -d)"
unzip -q "$OUT" -d "$T"
cmp -s "$T/INST.8xp" "$ROOT/installer/INST.8xp" || { echo "INST.8xp differs from installer/INST.8xp"; exit 1; }
for f in "$D"/AppIns*.8xv; do cmp -s "$f" "$T/$(basename "$f")" || { echo "$(basename "$f") differs"; exit 1; }; done
n=$(ls "$T" | grep -c '\.8x')
echo "$OUT: $n files ($(ls "$D"/AppIns*.8xv | wc -l | tr -d ' ') app pieces + arTIfiCE + patched INST)"
