#!/bin/bash
# stepsrun.sh <problems file> [out]  (WSL) The step-by-step solutions (src/steps.cc, the driver's -S)
# of each problem of a file, one process per problem (30 s timeout), JOBS at a time (default 8), in
# the file's order, then a summary by kind.
# HGBIN: another binary (default ~/hostgiac/bin/hostgiac-worktree, tools/hostgiac/build.sh worktree).
export B=${HGBIN:-$HOME/hostgiac/bin/hostgiac-worktree}
export ASAN_OPTIONS=detect_leaks=0
in=$1; out=${2:-/tmp/stepsrun.txt}
export T=$(mktemp -d)
cd /tmp
# one line per problem: its number, a tab, the problem
grep -v -E '^(#|[[:space:]]*$)' "$in" | tr -d '\r' | awk '{printf "%05d\t%s\n",NR,$0}' > "$T/list"
one(){ n=${1%%$'\t'*}; e=${1#*$'\t'}; timeout 30 "$B" -S -e "$e" > "$T/$n" 2>&1 || echo "== $e   [TIMEOUT or crash]" >> "$T/$n"; }
export -f one
tr '\n' '\0' < "$T/list" | xargs -0 -P "${JOBS:-8}" -I{} bash -c 'one "$1"' _ {}
cat "$T"/[0-9]* > "$out"; rm -rf "$T"
for k in diff integrate limit sum series; do
  n=$(grep -c "^== $k(" "$out"); [ "$n" = 0 ] && continue
  echo "$k: $(grep "^== $k(" "$out" | grep -c '\[ok')/$n with steps"
done
echo "other: $(grep '^== ' "$out" | grep -v -E '^== (diff|integrate|limit|sum|series)\(' | grep -c '\[ok')/$(grep '^== ' "$out" | grep -c -v -E '^== (diff|integrate|limit|sum|series)\(') with steps; timeouts/crashes: $(grep -c 'TIMEOUT or crash' "$out")"
