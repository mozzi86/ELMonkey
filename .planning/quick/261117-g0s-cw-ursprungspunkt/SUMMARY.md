---
status: complete
---
# 261117-g0s – Ursprungspunkt des Fassadenrasters (SetCurtainWallSegmentELM gridOriginAtStart)

**Anlass (live THN, AC29, 17.11.2026):** 0.11.14 `columnOrigin` setzt nur `gridOriginType`; der stand schon auf
`start`, das Raster lief trotzdem ab dem festen Punkt `API_CWSegmentType.gridOrigin` (27BB6F2C: 0,986 m hinter
`begC`).

**Umsetzung (ELM_SAB 0.11.15):**
- `SetCurtainWallSegmentELM {gridOriginAtStart: true}` setzt `cWallSegments[0].gridOrigin = begC`, changeMask um
  `APIMemoMask_CWallSegments` erweitert; Antwort liest `gridOrigin` und `segmentBegin` {x,y,z} frisch aus dem Memo.
  Schema-Text von `columnOrigin` berichtigt (setzt nur den Typ).
- `GetCurtainWallPlaneELM` meldet je Segment `gridOrigin` {x,y,z} und `columnOrigin`/`rowOrigin` (aus dem Pattern
  des Memos; das Memo trägt ein Raster je Richtung, nicht je Segment).
- MCP 0.2.7: `ursprungspunkt` → `origin`, Test; 12 Tests grün.
- Skill `recipes/curtain-walls.md`: 0.11.14-Punkt berichtigt, 0.11.15 als „noch nicht live getestet“; ältere Aussagen
  (Raster ab Segmentanfang, Befehl nur in 0.11.12.1) richtiggestellt.

**Prüfungen:** `begC`/`gridOrigin` (API_Coord3D) in DevKits 27.3001, 28.3001, 29.2001, 29.3100, 30.3000 gleich;
`clang++ -fsyntax-only` gegen 27.3001, 28.3001, 29.3100, 30.3000 ohne Fehler; JSON-Schemas parsen.
CI-Lauf 44039561318: 8/8 grün (AC27–30, Mac/Win), Mac-AC29-Artefakt `mac-AC29`; Release-Schritt nur auf main,
Bundles liegen auf `ci-status`.

**Commits:** fae8628 (Merge main), 592e091, fcbb6a0, dd67ba0 (Zweig `elm-0.11.14-cw-rasterursprung`).

**Offen:** Live-Test AC29 Mac (vorher/nachher `GetCurtainWallPlaneELM`, dann `gridOriginAtStart` + ungleiche
Feldfolge an 27BB6F2C); offen ist auch, ob `ACAPI_Element_Change` die Segment-Maske für `gridOrigin` annimmt (kein
DevKit-Beispiel). Danach Merge nach main, Release, Spiegel.
