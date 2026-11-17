---
quick_id: 261117-g0s
date: 2026-11-17
---
# SetCurtainWallSegmentELM: Ursprungspunkt des Rasters (gridOriginAtStart)

Live gemessen 17.11. (AC29): 0.11.14 `columnOrigin` setzt nur `gridOriginType`; der stand bei den bestehenden
Fassaden schon auf `start`, das Raster lief trotzdem ab einem festen Punkt 0,986 m hinter dem Segmentanfang. Das ist
`API_CWSegmentType.gridOrigin` (API_Coord3D) im Memo `cWallSegments`.

Zweig `elm-0.11.14-cw-rasterursprung`, vorher `origin/main` (Lernstand Teil 3, 261117-l3t) hineinmergen.

1. `CreateCurtainWallCommand.cpp`, `SetCurtainWallSegmentCommand`: optionaler Parameter `gridOriginAtStart`
   (boolean); bei true `cWallSegments[0].gridOrigin = cWallSegments[0].begC`, `APIMemoMask_CWallSegments` in die
   changeMask. Antwort: `gridOrigin` und `segmentBegin` {x,y,z} nach der Änderung frisch aus dem Memo; Response-Schema.
   Feldnamen in allen DevKits (27.3001, 28.3001, 29.2001, 29.3100, 30.3000) prüfen.
2. `GetCurtainWallPlaneELM`: je Segment `gridOrigin` und `columnOrigin`/`rowOrigin` ergänzen (nur ergänzen).
3. Version 0.11.15: `AddOnVersion.hpp`, Beschreibungen in `AddOnMain.cpp`, READMEs.
4. Skill `recipes/curtain-walls.md`: 0.11.14-Punkt berichtigen (live gemessen, nur Typ), 0.11.15
   `gridOriginAtStart` (noch nicht live getestet); ältere Aussagen „Raster beginnt am Segmentanfang“ und „Befehl nur in
   0.11.12.1“ richtigstellen.
5. MCP 0.2.7: `ursprungspunkt` → `origin`, Test.
6. Commits, Push, CI auf dem Zweig abwarten.

## Nicht in diesem Task

Merge nach main, Release, Installation, Ausführung in Archicad, Spiegel nach -archicad.
