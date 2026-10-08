#!/bin/bash
# stepsdbg.sh <file> [lines] (WSL): src/steps.cc's search, traced (STEPDBG), for the first problem
# of file (wsl.exe mangles parentheses on its command line), 30 s at most
export ASAN_OPTIONS=detect_leaks=0 STEPDBG=1
e=$(grep -v '^#' "$1" | head -1)
cd /tmp && timeout 30 ${HGBIN:-$HOME/hostgiac/bin/hostgiac-worktree} -S -e "$e" 2>&1 | head -${2:-60}
