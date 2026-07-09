#!/usr/bin/env bash
# run_pipeline.sh — DWG -> IFC4 Decken End-to-End
#
# Verkettet alle 5 Schritte und schreibt das finale IFC.
# Optional: Schritt 6 (Auto-Import nach Archicad) am Ende, wenn -i gesetzt.
#
# Usage:
#   ./run_pipeline.sh  INPUT.dwg  [OUTPUT.ifc]  [-i]
#
#   INPUT.dwg   — DWG-Lageplan
#   OUTPUT.ifc  — Ziel-IFC (Default: <INPUT_stem>_Decken.ifc)
#   -i          — am Ende IFC ins offene Archicad öffnen (macOS `open -a`)
#
# Voraussetzungen:
#   - dwg2dxf (LibreDWG)
#   - Python 3 mit: ezdxf, shapely, ifcopenshell
#     pip install ezdxf shapely ifcopenshell

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# --- Args
DWG="${1:-}"
OUT_IFC="${2:-}"
DO_IMPORT=false
for arg in "$@"; do
  if [[ "$arg" == "-i" ]]; then DO_IMPORT=true; fi
done

if [[ -z "$DWG" ]]; then
  echo "Usage: $0 INPUT.dwg [OUTPUT.ifc] [-i]" >&2
  exit 1
fi
if [[ ! -f "$DWG" ]]; then
  echo "FEHLER: $DWG nicht gefunden" >&2
  exit 2
fi

STEM="$(basename "$DWG" .dwg)"
WORKDIR="$(dirname "$(realpath "$DWG")")"
DXF="${WORKDIR}/${STEM}.dxf"
OUT_IFC="${OUT_IFC:-${WORKDIR}/${STEM}_Decken.ifc}"

echo "=========================================="
echo "DWG -> IFC Decken-Pipeline"
echo "  DWG:    $DWG"
echo "  DXF:    $DXF"
echo "  Output: $OUT_IFC"
echo "  Workdir: $WORKDIR"
echo "=========================================="

cd "$WORKDIR"

# --- Step 1: DWG -> DXF
echo ""
echo "### [1/5] DWG -> DXF"
"$SCRIPT_DIR/step_1_dwg_to_dxf.sh" "$DWG"

# --- Step 2: Polygonize pro Klasse (Strasse, Bordstein, Gebaeude)
echo ""
echo "### [2/5] Polygonize"
python3 "$SCRIPT_DIR/step_2_polygonize.py" "$DXF" Strasse   "$WORKDIR/strasse_v4.pkl"
python3 "$SCRIPT_DIR/step_2_polygonize.py" "$DXF" Bordstein "$WORKDIR/bordstein_v4.pkl"
python3 "$SCRIPT_DIR/step_2_polygonize.py" "$DXF" Gebaeude  "$WORKDIR/gebaeude_v4.pkl"

# Symlinks für step_3 (das erwartet strasse.pkl/bordstein.pkl/gebaeude.pkl als globaler Origin)
ln -sf "$WORKDIR/strasse_v4.pkl"   "$WORKDIR/strasse.pkl"
ln -sf "$WORKDIR/bordstein_v4.pkl" "$WORKDIR/bordstein.pkl"
ln -sf "$WORKDIR/gebaeude_v4.pkl"  "$WORKDIR/gebaeude.pkl"

# --- Step 3: Concave-Hull-Filler für Lückenfreiheit
echo ""
echo "### [3/5] Fill gaps (concave hull)"
python3 "$SCRIPT_DIR/step_3_fill_gaps.py" "$DXF"

# --- Step 4: Pro Klasse ein IFC bauen
echo ""
echo "### [4/5] Build IFC per class"
python3 "$SCRIPT_DIR/step_4_build_ifc.py" "$WORKDIR/strasse_v4.pkl"   Strasse      "$WORKDIR/${STEM}_Strasse.ifc"
python3 "$SCRIPT_DIR/step_4_build_ifc.py" "$WORKDIR/bordstein_v4.pkl" Bordstein    "$WORKDIR/${STEM}_Bordstein.ifc"
python3 "$SCRIPT_DIR/step_4_build_ifc.py" "$WORKDIR/gebaeude_v4.pkl"  Gebaeude     "$WORKDIR/${STEM}_Gebaeude.ifc"
python3 "$SCRIPT_DIR/step_4_build_ifc.py" "$WORKDIR/fill_v5.pkl"      Strasse_Fill "$WORKDIR/${STEM}_StrasseFill.ifc"

# --- Step 5: Merge alles
echo ""
echo "### [5/5] Merge IFCs"
python3 "$SCRIPT_DIR/step_5_merge.py" "$OUT_IFC" \
  "$WORKDIR/${STEM}_StrasseFill.ifc" \
  "$WORKDIR/${STEM}_Strasse.ifc" \
  "$WORKDIR/${STEM}_Bordstein.ifc" \
  "$WORKDIR/${STEM}_Gebaeude.ifc"

echo ""
echo "=========================================="
echo "FERTIG: $OUT_IFC  ($(du -h "$OUT_IFC" | cut -f1))"
echo "=========================================="

# --- Step 6 (optional): Import ins Archicad
if [[ "$DO_IMPORT" == "true" ]]; then
  echo ""
  echo "### [6] Optional: Import ins Archicad"
  if [[ "$(uname)" == "Darwin" ]]; then
    # macOS — versuche Archicad-Versionen
    for app in "Archicad 29" "Archicad 28" "Archicad 27" "Archicad"; do
      if [[ -d "/Applications/Graphisoft/${app}/${app}.app" ]] || [[ -d "/Applications/${app}.app" ]]; then
        echo "Öffne IFC in $app: $OUT_IFC"
        open -a "$app" "$OUT_IFC"
        break
      fi
    done
  else
    echo "Auto-Import nur unter macOS implementiert. Manuelle Anleitung:"
    echo "  Archicad öffnen -> Datei -> Interoperabilität -> IFC öffnen -> $OUT_IFC"
  fi
fi
