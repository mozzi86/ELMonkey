# Gelände aus Punktwolke (E57 → Bodenraster → CreateMeshes) <!-- 2026-09-07 -->

Ziel: Ein Gelände (Mesh) erzeugen, das der gescannten Bodenoberfläche einer in Archicad platzierten Punktwolke folgt („Abdruck"). Archicad kann das nicht nativ; das Gelände-Werkzeug sieht Punktwolken nicht.

## Lage-Zuordnung (Schlüsselerkenntnis)
- Die Punktwolke ist in Archicad ein Object mit libPart-Name = Dateiname (z. B. „THN_Außen_richtig"). Tapir GetDetailsOfElements liefert `origin` und `dimensions` (per HTTP-Bypass; das MCP-Schema wirft für Objekte Pydantic-Fehler).
- **Archicad platziert die Wolke mit der Bounding-Box-Minimalecke (xMin,yMin,zMin) am Objektursprung.** `dimensions` = Welt-BBox-Ausdehnung der Wolke. Offset = origin − bboxMin(E57). Verifikation: dimensions stimmen auf mm mit der E57-BBox (268,755 × 214,123 × 47,768 m am THN).
- Die per-Scan `cartesianBounds` im E57-Header sind Scanner-lokal; die Welt-BBox muss aus den transformierten Punkten (`pye57 read_scan(i, transform=True)`) gerechnet werden.
- Die 3D-Bounding-Box des Objekts (Get3DBoundingBoxes) ist NICHT die Wolke, sondern nur das 2D-Symbol — ignorieren.

## Pipeline
1. Quelle finden: `mdfind -name '<libPart-Name>'` (E57 typisch in ~/Downloads; der LCF-Cache in ~/Documents/Graphisoft/Punktwolken/ ist nicht lesbar, E57 nehmen).
2. venv: `uv venv .venv` + `uv pip install --python .venv/bin/python pye57 numpy scipy` (pip fehlt in uv-venvs, `python -m pip` scheitert).
3. Rasterisieren (`e57_grid.py <e57> <grid.npz>`): 1-m-Raster, pro Zelle min(z) + Punktzahl via `np.minimum.at`/`np.add.at`; 50 Mio Punkte in ~6 s.
4. Bodenfilter + Mesh-JSON (`make_mesh.py --grid … --origin X Y Z --step 2 --out mesh.json`): Zellen mit <5 Punkten raus; `minimum_filter` 7×7, Zellen >0,6 m über lokalem Minimum raus (Wände, Bäume, Dächer); 2. Durchgang: >1,5 m über 21×21-NaN-Median raus (isolierte Dachterrassen); 3×3-Median glätten. Downsampling auf 2-m-Raster; **jede Rasterzeile = eine Subline** (Polylinie mit z je Knoten). 87 Sublines / 3.065 Punkte sind für CreateMeshes problemlos (Antwort in Sekunden).
5. Create (`create_mesh.py mesh.json <port>`): Tapir `CreateMeshes` per HTTP-Bypass. `level: 0`, absolute z in polygonCoordinates und sublines, `skirtType: SolidBodyWithSkirt`, `ridges: AllSmooth`, `showLines: false`, `floorIndex` = Zielgeschoss. Umriss = Wolken-BBox-Rechteck mit z = lokaler Boden-Median.
6. Rücklese: Get3DBoundingBoxes (zMax muss dem größten Subline-z entsprechen) + GetDetailsOfElements (Sublines-/Punktzahl).
7. KI-Stempel setzen (siehe SKILL.md); fehlt die Property im Projekt, User anlegen lassen.

## Fallen
- `skirtLevel` ist die **absolute Unterkante** (skirtLevel 3 → bbox zMin −3,0), nicht eine Tiefe.
- Wo keine Punkte sind (unter Gebäuden, außerhalb Scanbereich), interpoliert Archicad geradlinig zwischen Sublines und Umriss — Umriss lieber auf den gescannten Bereich zuschneiden, wenn saubere Ränder gewünscht.
- Bestehendes Gelände nicht modifizieren (Update = Confirm), sondern neu erzeugen und den User das alte löschen lassen.
- KI-Stempel: „KI generiert" fehlt in Untitled-/Fremdprojekten oft → User anlegen lassen, nicht still weglassen.
- `elements_get_elements_by_type` mit `Unknown` ist ungültig (Fehler −2130313112); Punktwolken sind Typ `Object`.

## Worked Example — THN Außenanlage, 2026-09-07
E57 „THN_Außen_richtig" 59 Scans / 50,4 Mio Punkte; Objekt-origin (−386,587 / 127,885 / −0,1), E57-bboxMin (−207,260 / −131,835 / −12,312) → Offset (−179,327 / +259,720 / +12,212). Boden 0,19–11,22 m (Median 6,94 m), 87 Sublines / 3.065 Punkte, Umriss-z 6,95. Mesh-GUID 08EDCBD4-B85E-5143-9319-07A683D26334 im Projekt „Untitled" (Port 19724, EG). Wolke deckt nur ~20.000 der 57.500 m² Umriss ab.

## Skripte
`scripts/gelaende_aus_punktwolke/` — e57_grid.py, make_mesh.py, create_mesh.py (live-verifiziert 2026-09-07).
