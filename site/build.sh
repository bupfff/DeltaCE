#!/bin/bash
# Assembles the DeltaCE web page in _site/: the page (site/), the README's media (docs/) and the
# release bundle for the web installer, DeltaCE.b84.
#   site/build.sh <DeltaCE.b84> [version]       e.g. site/build.sh releases/DeltaCE.b84 v1.8
# The GitHub Pages workflow (.github/workflows/pages.yml) runs it with the latest release's bundle.
set -euo pipefail
cd "$(dirname "$0")/.."
B84=${1:?usage: site/build.sh <DeltaCE.b84> [version]}
VER=${2:-}
OUT=_site
rm -rf "$OUT"
mkdir -p "$OUT/media/screenshots" "$OUT/media/install" "$OUT/files"
cp site/index.html site/style.css site/installer.js site/dusb.js "$OUT/"
cp site/icon.svg "$OUT/media/icon.svg"
cp docs/deltace_light.gif docs/deltace_dark.gif "$OUT/media/"
cp docs/screenshots/*.png "$OUT/media/screenshots/"
cp docs/install/*.gif "$OUT/media/install/"
cp "$B84" "$OUT/files/DeltaCE.b84"
printf '%s\n' "$VER" > "$OUT/files/version.txt"
echo "built $OUT ($(du -sh "$OUT" | cut -f1)), DeltaCE ${VER:-(no version)}"
