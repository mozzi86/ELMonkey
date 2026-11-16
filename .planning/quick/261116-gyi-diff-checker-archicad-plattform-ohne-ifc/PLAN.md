---
quick_id: 261116-gyi
date: 2026-11-16
---
# Diff-Checker Archicad ↔ Plattform ohne IFC

Anforderung: ZZ_Austausch/„ELMonkey Diff-Checker Plattform-Abgleich 2026-11-16“ (Sophia Pickel, Anlass AFWC).

## Aufteilung

Gerechnet wird wie beim Abgleich im Python-Monkey, nicht im C++. Das Add-on bekommt nur, was ihm fehlt: die Box
der 3D-Körper nach Anschlüssen (`Get3DBoundingBoxes` liefert für Wände die rohe Hüllbox).

1. Bernds Abgleich-Paket vom 10.11. einbauen (`core/elmonkey/abgleich.py`, Skript, Rezept), Auftrag 09 beheben
   (`elementIFCIds`, `externalIFCId` zuerst).
2. `core/elmonkey/diff.py`: Werte normalisieren (IFC-Schreibweisen), Mengen ignorieren, Räume und Öffnungen
   getrennt, Geometrie je Seite mit 3 mm Toleranz, Ausgabe als `ergebnis.json`.
3. Skript `plattform_abgleich.py diff`: Archicad direkt lesen (alle Elemente, IFC-Typ, Körper-Box), vergleichen.
4. Add-on `ELM_SAB.GetSolidBoundsELM` (header-only, registriert in AddOnMain) — Build nur in der CI.
5. Rezept, READMEs, MCP-Katalog.

## Nicht in diesem Task

- Plattform: `bbox` im Stand und Geometrie-Operationen aus `plattform.geometrie` (Plattform-Monkey, eigener PR).
- Live-Lauf gegen Archicad.
