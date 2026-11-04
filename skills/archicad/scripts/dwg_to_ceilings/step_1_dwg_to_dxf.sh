#!/usr/bin/env bash
# step_1_dwg_to_dxf.sh — DWG -> DXF Konvertierung
#
# Nutzt LibreDWG (dwg2dxf), das im Standard-PATH liegen muss.
# Installation auf Mac:  brew install libredwg
# Installation auf Linux: package "libredwg" oder aus Source bauen
#
# Usage:  ./step_1_dwg_to_dxf.sh INPUT.dwg
# Output: INPUT.dxf  (im selben Ordner)

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 INPUT.dwg" >&2
  exit 1
fi

INPUT="$1"

if ! command -v dwg2dxf >/dev/null 2>&1; then
  echo "FEHLER: dwg2dxf nicht gefunden." >&2
  echo "Installation:  brew install libredwg  (Mac)" >&2
  echo "               oder aus Source: https://www.gnu.org/software/libredwg/" >&2
  exit 2
fi

if [[ ! -f "$INPUT" ]]; then
  echo "FEHLER: Eingabedatei nicht gefunden: $INPUT" >&2
  exit 3
fi

echo "[$(date +%T)] DWG -> DXF: $INPUT"
dwg2dxf -y "$INPUT"

# dwg2dxf legt automatisch INPUT.dxf neben INPUT.dwg
OUT="${INPUT%.dwg}.dxf"
if [[ -f "$OUT" ]]; then
  echo "[$(date +%T)] Fertig: $OUT ($(du -h "$OUT" | cut -f1))"
else
  echo "WARN: erwartete Ausgabe nicht gefunden: $OUT" >&2
  exit 4
fi
