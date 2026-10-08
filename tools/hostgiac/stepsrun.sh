#!/bin/bash
# stepsrun.sh <problems file> [out]  (WSL) The step-by-step solutions (src/steps.cc, the driver's -S)
# of each problem of a file, one process per problem (30 s timeout), then a summary by kind.
# HGBIN: another binary (default ~/hostgiac/bin/hostgiac-worktree, tools/hostgiac/build.sh worktree).
B=${HGBIN:-$HOME/hostgiac/bin/hostgiac-worktree}
export ASAN_OPTIONS=detect_leaks=0
in=$1; out=${2:-/tmp/stepsrun.txt}
cd /tmp && : > "$out"
while IFS= read -r e; do
  case "$e" in ''|'#'*) continue;; esac
  timeout 30 "$B" -S -e "$e" >> "$out" 2>&1 || echo "== $e   [TIMEOUT or crash]" >> "$out"
done < "$in"
for k in diff integrate limit sum series; do
  n=$(grep -c "^== $k(" "$out"); [ "$n" = 0 ] && continue
  echo "$k: $(grep "^== $k(" "$out" | grep -c '\[ok')/$n with steps"
done
echo "other: $(grep '^== ' "$out" | grep -v -E '^== (diff|integrate|limit|sum|series)\(' | grep -c '\[ok')/$(grep '^== ' "$out" | grep -c -v -E '^== (diff|integrate|limit|sum|series)\(') with steps; timeouts/crashes: $(grep -c 'TIMEOUT or crash' "$out")"
