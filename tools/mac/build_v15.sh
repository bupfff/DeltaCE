#!/bin/bash
# Build a FlowCE source tree with CE toolchain v15 (macOS port of the project's
# tools/dev/wsl_build_v15.sh) and package it with the patched installer.
# usage: scripts/build_v15.sh <source tree> <name> [normal|steps|units]
#   -> releases/mine/<name>.b84 (+ <name>-files/), the map in work/<name>.map
# The app is linked twice with GNU ld (the toolchain's application linker script and crt0), the
# second time at its final flash address; app_tools/pack_app.py adds the headers INST expects and
# make_segments.py cuts the AppIns pieces. tools/dev/v15_biascheck.py refuses a build hit by
# v15's frame-bias miscompilation (functions with variable-length arrays).
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$(cd "$1" && pwd)"; NAME="$2"; VARIANT="${3:-steps}"
CEDEV15="$ROOT/toolchain/CEdev-v15.0"
BU="$CEDEV15/binutils/bin"
export PATH="$CEDEV15/bin:$PATH"
case "$VARIANT" in
  normal) ARGS="APPLANG=en" ;;
  steps) ARGS="APPLANG=en STEPS=1" ;;
  units) ARGS="APPLANG=en UNITS=1" ;;
  steps-units) ARGS="APPLANG=en STEPS=1 UNITS=1" ;;
  *) echo "variant: normal, steps, units or steps-units"; exit 2 ;;
esac
cd "$SRC"
[ "$(cedev-config --version)" = v15.0 ] || { echo "not CEdev v15.0"; exit 1; }
if [ "$(cat obj/.variant 2>/dev/null)" != "v15-$VARIANT" ]; then rm -rf obj bin; mkdir -p obj; echo "v15-$VARIANT" > obj/.variant; fi
LOG="$ROOT/work/build-$NAME.log"; : > "$LOG"
echo "building $SRC (v15, $VARIANT), log: $LOG"
LDX="--defsym=___heapbot=___heap_low --defsym=___heaptop=___heap_high"
# the code's placement in flash (a section ordering file for ld): ORDER_FILE=<file> (none: the
# linker's own order), else <tree>/layout.ld (scripts/make_layout.sh) if it was made for this code
LD0="$LDX"; LAYOUT="${ORDER_FILE:-}"; AUTO=""
if [ -z "$LAYOUT" ] && [ -f "$SRC/layout.ld" ]; then LAYOUT="$SRC/layout.ld"; AUTO=1; fi
[ "$LAYOUT" != none ] || LAYOUT=""
[ -z "$LAYOUT" ] || LDX="$LDX --section-ordering-file=$(cd "$(dirname "$LAYOUT")" && pwd)/$(basename "$LAYOUT")"
t0=$(date +%s)
PAD=0 # 0xFF bytes after the code (a last piece under 1 KB: below)
link() { # <load address>
  rm -f bin/DEMO.obj
  make -j"$(sysctl -n hw.ncpu)" $ARGS APPLICATION=YES EXTRA_LDFLAGS="$LDX" OUTPUT_MAP=YES STRIP_CMD=: LOAD_ADDR="$1" bin/DEMO.obj >> "$LOG" 2>&1 \
    || { echo "BUILD FAILED - last errors:"; grep -E "error|undefined" "$LOG" | head -20; exit 1; }
  "$BU/z80-none-elf-objcopy" -O binary bin/DEMO.obj bin/body.bin
  [ "$PAD" = 0 ] || head -c "$PAD" /dev/zero | LC_ALL=C tr '\0' '\377' >> bin/body.bin
}
link 0
if [ -n "$AUTO" ] && ! python3 "$ROOT/scripts/codelayout.py" check "$LAYOUT" bin/DEMO.map; then
  echo "layout.ld was made for other code (scripts/make_layout.sh makes a new one): linked without it"
  LDX="$LD0"; LAYOUT=""; link 0
fi
python3 tools/dev/v15_biascheck.py obj/lto.s || { echo "REFUSED: tools/dev/v15_biascheck.py found a miscompiled function"; exit 1; }
pack_all() { # the link at the app's final address (from the size of the body linked at 0), the image, its pieces
  LA=$(python3 app_tools/pack_app.py size bin/body.bin)
  n1=$(stat -f %z bin/body.bin)
  link "$LA"
  n2=$(stat -f %z bin/body.bin)
  [ "$n1" = "$n2" ] || { echo "the app's size changed with its address: $n1 -> $n2"; exit 1; }
  python3 tools/dev/v15_biascheck.py obj/lto.s || { echo "REFUSED (second link)"; exit 1; }
  ENTRY=0x$("$BU/z80-none-elf-nm" bin/DEMO.obj | awk '$3=="__start" {print $1}')
  python3 app_tools/pack_app.py pack bin/body.bin "$ENTRY" "$LA" bin/_DEMO.bin > /dev/null || { echo "pack_app failed"; exit 1; }
  rm -f bin/AppIns*.8xv bin/AppIns*.bin
  python3 app_tools/make_segments.py bin/_DEMO.bin bin > /dev/null || { echo "make_segments failed"; exit 1; }
  size=$(stat -f %z bin/_DEMO.bin)
  segs=$(( (size + 3 + 65231) / 65232 )); last=$(( size + 3 - (segs - 1) * 65232 ))
}
pack_all
if [ "$last" -lt 1024 ]; then
  # INST stops on a last piece under 1 KB (any number of pieces: a 42-piece build with 86 bytes in
  # the last failed, 2026-10-17): 0xFF bytes after the code (the app grows down from its fixed end,
  # as pack_app.py's own alignment bytes) fill it to 1 KB, and the app is linked again
  PAD=$((1024 - last + 64))
  echo "the last piece would hold $last bytes (< 1 KB): $PAD bytes of padding after the code"
  link 0; pack_all
fi
# the linker's own map (input sections, their sizes and order: scripts/layout_search.py)
cp bin/DEMO.map "$ROOT/work/$NAME.ldmap"
# the emulator tools read a map in the fasmg format (name = address); v15 aligns short on 2 bytes,
# so the console's line record is 14 bytes (tools/emu/cemu.py line_record)
"$BU/z80-none-elf-nm" bin/DEMO.obj | python3 -I -c '
import sys
for l in sys.stdin:
    p = l.split()
    if len(p) == 3: print("%s = %06X" % (p[2], int(p[0], 16)))' > bin/DEMO.map
echo "__flowce_line_size = 00000E" >> bin/DEMO.map
cp bin/DEMO.map "$ROOT/work/$NAME.map"
t1=$(date +%s)
max=$((0x2AFFFD))
PL="the linker's order"; [ -z "$LAYOUT" ] || PL="$(basename "$LAYOUT")"
echo "app: $size bytes, $segs pieces, room left: $((max - size)) bytes ($((t1-t0)) s, warnings: $(grep -c 'warning:' "$LOG")), placement: $PL"
[ "$size" -le "$max" ] || { echo "TOO BIG for the calculator"; exit 3; }
if [ "$last" -lt 1024 ]; then echo "the last piece holds $last bytes (< 1 KB) even padded: INST stops on it"; exit 3; fi
OUT="$ROOT/releases/mine"
rm -rf "$OUT/$NAME-files"; mkdir -p "$OUT/$NAME-files"
cp bin/AppIns*.8xv "$OUT/$NAME-files/"
"$ROOT/scripts/make_bundle.sh" "$OUT/$NAME-files" "$OUT/$NAME.b84"
