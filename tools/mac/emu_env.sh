# Source this before using the emulator tools: `source scripts/emu_env.sh`
# The project's tools (tools/emu/*.py in a source tree) find everything through these variables.
FLOWCE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
export KB="$FLOWCE_ROOT/work/emu-kb"                    # emulator states live in $KB/emu/states
export CEMU_HEADLESS="$FLOWCE_ROOT/toolchain/CEmu/tests/autotester/cemu-headless"
export KHICAS_ROM="${KHICAS_ROM:-$FLOWCE_ROOT/rom/ti84ce.rom}"  # your own calculator's ROM dump
mkdir -p "$KB/emu/states"
[ -x "$CEMU_HEADLESS" ] || echo "emu_env: cemu-headless not built ($CEMU_HEADLESS)"
[ -f "$KHICAS_ROM" ] || echo "emu_env: no ROM at $KHICAS_ROM (dump it from the calculator with CEmu)"
