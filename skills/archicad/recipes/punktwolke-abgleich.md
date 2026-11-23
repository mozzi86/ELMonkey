# Rezept: Punktwolke (E57) mit dem Archicad-Modell abgleichen <!-- 2026-11-16, live Projekt-C 12.11. -->

Projekt-C (AC29 Teamwork, Tapir 1.7.2 unter ELMonkey): drei reduzierte Scans (KG/EG/1.OG, je ca.
15 Mio. Punkte) als Objekte auf „99 Punktwolke“. Frage: Fehlen Wände/Stützen im Modell, liegen
welche falsch? Alles nur lesend, Massendaten per HTTP-Bypass (siehe
[`../reference/mcp-conventions.md`](../reference/mcp-conventions.md)). Ergebnis damals: nichts
fehlt in EG/1.OG; 1.OG-Trennwände 0,15–1 m versetzt (Best-Fit 1,4°), KG eine dünne Abtrennung.

## 1. Platzierung der Wolke

- Objekte: `API.GetElementsByType Object` → Tapir `GetDetailsOfElements`, nach Bibliotheksteil-Name
  (`Projekt-F_1.OG_reduziert_15Mio_0`) bzw. `dimensions` > 15 m filtern. Gelesen: `origin`, `dimensions`, `angle` (rad).
- E57 finden: `mdfind -name 'reduziert_15Mio'` (Dateiname = Bibliotheksteil ohne `_0`).
- Lesen: `uv venv .venv && uv pip install pye57 numpy scipy matplotlib shapely`, dann
  `pye57.E57(f).read_scan(0, transform=True, ignore_missing_fields=True)`, `cartesianX/Y/Z`,
  nicht-endliche Punkte verwerfen (ca. 2 s je Datei). `(P − min)` als float32-npy ablegen.
- **Transformation:** Welt = origin + Rz(angle) · (p − E57-BBox-Min), z = origin.z + p.z.
  Prüfen: E57-Ausdehnung = `dimensions` (mm-genau) und z-Histogramm (0,1-m-Bins) mit Peaks auf den
  Geschossböden. Vorzeichen von `angle` wurde nicht gegen −angle getestet; bei gedrehten Wolken prüfen.

## 2. Vergleich

- **Band je Geschoss:** lv + 0,3 bis lv + 2,2 (lv aus `GetStories` bzw. OK FFB). Raster 0,05 m,
  19 z-Bins à 0,1 m als Bitmaske je Zelle; **„raumhoch“ = mindestens 60 % der Bins belegt**.
  Nur innerhalb der Zonen zählen (`polygonOutline`, shapely-Puffer 0,4 m).
- **Modellmaske:** `API.GetAllElements` → Tapir `GetDetailsOfElements` (1000er-Pakete) +
  `API.Get3DBoundingBoxes` (2000er). Nur Elemente, deren Box das Band schneidet, ohne Zone/Lampe.
  Getrennte Masken für sichtbare und ausgeblendete Ebenen (`isHidden`) plus Owner-Raster
  (Elementindex je Zelle) für Rückfragen.
  - Wände aus Parametern: `begCoordinate`/`endCoordinate`, Dicke max(beg,end) + |offset|, bei
    `referenceLineLocation` ≠ Center beidseitig puffern.
  - Stützen aus `origin`, `width`, `depth`, `axisRotationAngle`, `circleBased`.
  - Rest: Tapir `Get3DGeometryOfElements` (50er-Pakete, ~8 min), Meshes bei lv + 0,5/1,0/1,5/2,0
    schneiden, Segmente in 2 cm rastern. Ohne Körper: Box-Grundfläche (außer > 4 m² und kürzere
    Seite > 0,8 m), damit schlanke Fassadenteile mitkommen.
- **Kandidaten:** Wolke raumhoch und nicht in `dilate(sichtbar, 3)` (±15 cm). `ndi.label` nach
  Dilatation 2 (8er-Nachbarschaft), < 16 Zellen oder L < 0,25 m verwerfen; L/W per PCA.
  Kennwerte: `full` = Anteil mit Punkten bei lv + 2,3…2,8, `hid` = Anteil auf ausgeblendeten Ebenen,
  `dist_med` = Median-Abstand (EDT) zum sichtbaren Modell.
- **Einstufung:** < 25 Zellen oder full < 0,7 → klein/niedrig; hid ≥ 0,3 → nur ausgeblendet;
  dist_med > 1 m → **fehlt**; sonst → **versetzt**. Danach Sichtprüfung je Kandidat.
- **Best-Fit-Drehung:** Score = Anteil raumhoher Wolkenzellen auf `dilate(sichtbar, 2)`. Gittersuche
  Drehung um den Wolkenschwerpunkt + dx/dy, grob → fein (±1°/0,1°, ±0,6 m/0,05; fein ±0,3°/0,05°,
  ±0,15 m/0,025). 1.OG: 46,5 % → 68,1 % bei 1,40°. **Gegenprobe Wolke gegen Wolke** (EG/1.OG in
  überlappenden z-Bändern): passen die Wolken zueinander, liegt der Fehler eher im Modell.

## 3. Bilder

matplotlib (`Agg`) aus der venv: `imshow` der RGB-Maske mit `origin="lower"` und `extent` in
Weltmetern; schwarz = Wolke raumhoch, grau = Modell sichtbar, hellblau = ausgeblendet; Rahmen rot =
fehlt, orange = versetzt, blau = ausgeblendet, nummeriert. Erster Lauf baut den Font-Cache (dauert).
Ergebnisbilder dauerhaft im Arbeitsordner ablegen, nicht nur im Scratchpad.

## Fallen

1. **4010 unter Tapir 1.7.2:** `GetCutPolygonsOfElements`, `GetDrawingPrimitivesOfElements`,
   `GetCurrentLayerCombination`, `GetRenovationFilters`, `GetElementCategories`,
   `Get3DFilterAndCutSettings`, `Get3DCutPlanes`, `Get3DCutawayStatus`. Ersatz: Meshes selbst
   schneiden; statt Ebenenkombination/Renovierungsfilter den aktuellen `isHidden`-Stand und die
   Renovierungs-Property nehmen.
2. **`General_RenovationStatus` per `API.GetPropertyIds` → 4005.** Über Tapir `GetAllProperties`
   suchen (Projekt-C-GUID `9CF14061-6E9C-45BE-BD8F-D3BCF59D218E`, Werte Existing/New/ToBeDemolished).
3. **`Get3DGeometryOfElements` liefert leere `bodies`** für alle Stützen, das ganze 1.OG und
   ausgeblendete Fassaden (vermutlich 3D-Filter). Erst sah das 1.OG nach 0 % Deckung aus — ein
   Datenproblem, keine Fehllage. Darum Wände/Stützen aus Parametern + Box-Ersatz.
4. **Ausgeblendete Fassaden** erscheinen ohne Box-Ersatz als „fehlt“ (EG 16 → 8 Kandidaten).
5. **Zonen-Export:** `holes` kann die Schleife sprengen (`TypeError`); je Zone try/except.
6. **Entkernte Bereiche:** Neue Wände stehen nur im Modell, Rohboden liegt tiefer — erwartbar, nicht melden.
7. `archicad_list_commands` ist zu groß für den Kontext; die Datei mit Python filtern.
