#!/usr/bin/env bash
# Holt die neueste gruene Firmware aus GitHub Actions und spielt sie auf.
#
#   ./flash.sh            beide Haelften nacheinander
#   ./flash.sh left       nur die linke Haelfte
#   ./flash.sh --reset    beide Haelften, aber der settings_reset-Build
#
# Braucht gh (angemeldet) und udisksctl.
set -euo pipefail
cd "$(dirname "$0")"

prefix=""
halves=(left right)
for arg in "$@"; do
  case $arg in
    left|right) halves=("$arg") ;;
    --reset)    prefix="settings_reset-" ;;
    *) echo "Aufruf: ./flash.sh [left|right] [--reset]" >&2; exit 1 ;;
  esac
done

# Wartet auf das Bootloader-Laufwerk und gibt seinen Mountpunkt aus.
# Erkennungsmerkmal ist INFO_UF2.TXT. Nur ein Laufwerk mit dieser Datei
# wird beschrieben.
wait_for_volume() {
  local info dev
  for _ in $(seq 90); do
    info=$(find /run/media/"$USER" /media/"$USER" -maxdepth 2 -name INFO_UF2.TXT 2>/dev/null | head -1)
    if [[ -n $info ]]; then
      dirname "$info"
      return 0
    fi
    # Ohne Desktop mountet nichts von selbst. Die erste vfat-Wechselpartition
    # anhaengen und im naechsten Durchlauf auf INFO_UF2.TXT pruefen.
    # ponytail: mountet notfalls auch einen USB-Stick, schreibt aber nie
    # darauf. Nach Label filtern, falls das je stoert.
    dev=$(lsblk -rno PATH,FSTYPE,RM,MOUNTPOINT | awk '$2=="vfat" && $3==1 && NF==3 {print $1; exit}')
    [[ -n $dev ]] && udisksctl mount -b "$dev" >/dev/null 2>&1 || true
    sleep 1
  done
  echo "Kein Bootloader-Laufwerk gefunden. Reset zweimal druecken." >&2
  return 1
}

branch=$(git branch --show-current)
run=$(gh run list -b "$branch" -s success -L 1 --json databaseId -q '.[0].databaseId')
[[ -n $run ]] || { echo "Kein gruener Build fuer Branch '$branch'. Erst pushen." >&2; exit 1; }

dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
echo "Lade Firmware aus Lauf $run ..."
gh run download "$run" -n firmware -D "$dir"

for half in "${halves[@]}"; do
  uf2="$dir/${prefix}sofle_choc_pro_${half}-zmk.uf2"
  if [[ ! -f $uf2 ]]; then
    echo "Nicht im Archiv: $(basename "$uf2")" >&2
    ls "$dir" >&2
    exit 1
  fi
  echo
  echo "== ${half}e Haelfte =="
  echo "Anschliessen und zweimal auf Reset druecken. Warte ..."
  vol=$(wait_for_volume)
  cp "$uf2" "$vol/"
  sync
  echo "Aufgespielt: $(basename "$uf2") -> $vol"
done
