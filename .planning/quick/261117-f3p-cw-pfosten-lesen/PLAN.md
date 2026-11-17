---
quick_id: 261117-f3p
date: 2026-11-17
---
# GetCurtainWallFramesELM + Fassaden-Ursprung am Segment-Element (ELM_SAB 0.11.16)

Live gemessen 17.11. (AC29 Mac, Bundle 0.11.15): Fassade mit 1 Segment (7,48 m), `GetCurtainWallPlaneELM` liefert
`gridOrigin` {0,0,0} und `columnOrigin` `start`. `SetCurtainWallSegmentELM {gridOriginAtStart: true}` meldet success,
die Rücklese bleibt bei {0,0,0}: der Memo-Weg (`cWallSegments` + `APIMemoMask_CWallSegments`) wirkt nicht. Ein Pfosten
0,986 m vom Segmentanfang bleibt bei jeder Feldfolge stehen, auch bei `columnWidths: [7.4804]`. Offen: Rasterursprung
oder Einzelpfosten?

Zweig `elm-0.11.14-cw-rasterursprung` (steht auf 0.11.15).

1. Neuer Lesebefehl `GetCurtainWallFramesELM {elements:[{elementId}]}`: alle Rahmen aus `cWallFrames` mit allen
   aussagekräftigen Feldern aus `API_CWFrameType` (GUID, classID/Rolle, className, objectType, segmentID, cellID,
   contourID, begRel/endRel, begC/endC, Flags) plus Lauflänge ab Segmentanfang; je Segment gridOrigin, begC, endC,
   gridAngle, columnOrigin/rowOrigin und die Primary-/Secondary-Pattern-Werte (logic, nPattern, pattern, endWithID,
   flexibleIDs). Nur lesen.
2. `SetCurtainWallSegmentELM`: Ursprung zusätzlich über das Segment-Element (`API_CurtainWallSegmentID`,
   `ACAPI_Element_Get`/`Change`, Maske `API_CWSegmentType.gridOrigin`); neuer Parameter `gridOrigin {x,y,z}`; Antwort mit
   Rücklese `gridOrigin`/`segmentBegin` und `step`/`errorCode` je Weg.
3. Feldnamen in allen DevKits prüfen, `clang++ -fsyntax-only`, JSON-Schemas parsen.
4. Version 0.11.16 (AddOnVersion, AddOnMain, READMEs); MCP 0.2.8 mit `pfosten`/`riegel` → `frame`, Tests grün.
5. Skill `recipes/curtain-walls.md`: 0.11.15-Eintrag mit dem Live-Befund berichtigen, 0.11.16 als nicht live getestet.
6. Commits, Push, CI für den Zweig abwarten.

## Nicht in diesem Task

Merge nach main, Release, Installation, Ausführung in Archicad, Spiegel nach -archicad.
