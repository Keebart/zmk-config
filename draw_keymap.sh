#!/usr/bin/env bash
# Zeichnet jede Ebene der Keymap als PNG nach img/.
# Braucht uvx (aus uv) und rsvg-convert (aus librsvg).
# Aufruf: ./draw_keymap.sh
set -euo pipefail
cd "$(dirname "$0")"

KEYMAP=config/sofle_choc_pro.keymap
LAYOUT=boards/arm/sofle_choc_pro/sofle_choc_pro-layouts.dtsi
KD=(uvx --from keymap-drawer keymap -c keymap_drawer.config.yaml)

mkdir -p img
"${KD[@]}" parse -z "$KEYMAP" -o img/keymap.yaml

# $1 = Dateiname ohne Endung, Rest = zusaetzliche Argumente fuer "draw"
draw() {
  local out=$1; shift
  "${KD[@]}" draw -d "$LAYOUT" img/keymap.yaml "$@" -o "img/$out.svg"
  rsvg-convert -z 2 -b white "img/$out.svg" -o "img/$out.png"
  rm "img/$out.svg"
  echo "img/$out.png"
}

draw keymap
for layer in base nav sym adjust; do
  draw "keymap-$layer" -s "$layer"
done
