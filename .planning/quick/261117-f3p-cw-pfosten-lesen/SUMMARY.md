---
status: complete
---
# 261117-f3p – GetCurtainWallFramesELM + Fassaden-Ursprung am Segment-Element (ELM_SAB 0.11.16)

**Anlass (live AC29 Mac, Bundle 0.11.15, 17.11.2026):** `gridOriginAtStart` über das Fassaden-Memo meldet success,
die Rücklese bleibt bei `gridOrigin` {0,0,0}. Ein Pfosten 0,986 m vom Segmentanfang bleibt bei jeder Feldfolge
stehen, auch bei einer einzigen Spalte. Offen: Rasterursprung oder Einzelpfosten.

**Umsetzung:**
- `GetCurtainWallFramesELM {elements:[{elementId}], includeGridLines?}` (nur lesen): je Fassade `nFrames`/`nPanels`/
  `nSegments`, `schemeColumnPattern`/`schemeRowPattern` (Schema aus dem Fassaden-Memo). Je Segment GUID, `category`,
  `segmentType`, `begC`/`endC`, `length`, `gridOrigin`, `gridOriginRunLength`, `gridAngle`, `extrusion`,
  `gridOriginFromSegmentElement` (ACAPI_Element_Get auf die Segment-GUID), aus dem Segment-Memo `columnOrigin`/`rowOrigin`,
  `columnPattern`/`rowPattern` (logic, gridOriginType, nPattern, pattern, endWithID, nDivisions, infillPanelID,
  flexibleIDs) und die Gitterlinien `mainAxisLines`/`secondaryAxisLines` (surfaceParam und 3D-Punkt je Ende). Je
  Rahmen GUID, `classID`, `classRole` (merged/division/corner/boundary/customized/classN), `className`, `objectType`,
  `segmentID`, `cellID` (+ Zerlegung >>16 / &0xFFFF als Hinweis), `contourID`, `begRel`/`endRel`, `begC`/`endC`,
  `orientation`, Profilmaße, Flags, `uBeg`/`uEnd` (Lauflänge ab Segmentanfang), `direction`.
- `SetCurtainWallSegmentELM`: neu `gridOrigin {x,y,z}` (nicht zusammen mit `gridOriginAtStart`) und `gridOriginVia`
  (`auto` = erst Memo, Segment nur wenn die Rücklese das Ziel nicht zeigt; `memo`; `segment`; `both`). Segment-Weg:
  Segment-GUID nach Schritt 1 frisch aus dem Memo, `ACAPI_Element_Get`, Maske `API_CWSegmentType.gridOrigin`,
  `ACAPI_Element_Change (…, nullptr, 0, true)`. Ein Fehler dort bricht den Befehl nicht ab. Antwort: `gridOriginTarget`,
  `gridOriginReached` (0,1 mm), `gridOriginFromSegmentElement`, `memoPath` und `segmentPath` je mit `ran`, `step`,
  `errorCode`, Rücklese; dazu wie bisher `gridOrigin`/`segmentBegin` frisch aus dem Memo.
- MCP 0.2.8: `pfosten`/`riegel` → `frame`, Test `test_search_pfosten_riegel`; 13 Tests grün.
- Skill `recipes/curtain-walls.md`: 0.11.15-Eintrag mit dem Live-Befund berichtigt, 0.11.16 als nicht live getestet,
  Vorbehalt beim Ursprungspunkt-Eintrag.

**DevKit-Fundstellen zu gridOrigin / Einzelpfosten:**
- `gridOrigin` kommt nur in `APIdefs_Elements.h` vor (`API_CWSegmentType`, Doku „The origin coordinate of the grid“),
  kein Beispiel und kein Kommentar, wie es geändert wird. Auch `Examples/` (29.2001) nutzt es nirgends.
- `ACAPinc.h` (ACAPI_Element_GetMemo): im Memo der FASSADE sind `cWSegPrimaryPattern`/`cWSegSecondaryPattern`/
  `cWSegPatternCells` nur das **Standard-Schema**; das Raster des einzelnen Segments liegt im Memo des Segments
  (`API_CurtainWallSegmentID`: `cWSegGridMesh`, `cWSegContour`, Patterns, Zellen). Darum liest der neue Befehl beide.
- `Examples/Element_Test/Src/Element_Modify_ModelElements.cpp` (`Do_CurtainWall_Edit`): Unterelemente (Rahmen,
  Paneele) werden per `ACAPI_Element_Get` auf die GUID aus dem Memo und `ACAPI_Element_Change` mit Typ-Maske geändert;
  dem folgt der Segment-Weg. `TestCurtainWallPanelsOrder` liest das Segment-Memo mit `cWSegGridMesh`.
- `API_CWFrameType`: `classID` mit `APICWFrameClass_Merged/Division/Corner/Boundary/FirstCustomClass/Customized`,
  `contourID` („Contour ID for contour frames; other has invalid value“), `segmentID`, `cellID` (Schlüssel in
  `meshPolygons` des Segments), `begRel`/`endRel` (relativ in der Zelle). Ein eigenes Feld „Einzelpfosten“ gibt es nicht;
  einzeln geänderte Rahmen tragen `classID = APICWFrameClass_Customized`.
- Feldnamen (Rahmen, Segment, Pattern, `cwSegment`, `cWSegGridMesh`, `APIMemoMask_CWSegGridMesh`) in 27.3001, 28.3001,
  29.2001, 29.3100, 30.3000 gleich (Struktur-Diff nur Kommentare).

**Prüfungen:** `clang++ -fsyntax-only` gegen 27.3001/28.3001 (C++17) und 29.3100/30.3000 (C++20) ohne Fehler, für
27/29 auch mit den `-Wall -Wextra -Werror`-Flags der CI; `AddOnMain.cpp` gegen alle vier. 8 JSON-Schemas parsen.
Skript: `~/.scratch/elmonkey/tools/elm_syntax_check_multi.sh` (lokal, nicht im Repo).
CI-Lauf 44042489903: 8/8 grün (AC27–30, Mac/Win), Mac-AC29-Artefakt `mac-AC29`; Release-Schritt übersprungen
(nur auf main), Bundles auf `ci-status`.

**Commits:** ab9be71 (Add-on), c40c8b7 (MCP), ac49540 (Skill) auf Zweig `elm-0.11.14-cw-rasterursprung`.

**Offen:** Live-Test AC29 Mac: erst `GetCurtainWallFramesELM` an der Testfassade (welcher Rahmen liegt bei
`uBeg` ≈ 0,986, `classRole`, gibt es dort eine Gitterlinie in `mainAxisLines`, was zeigt
`gridOriginFromSegmentElement`), dann `SetCurtainWallSegmentELM {gridOriginAtStart: true, gridOriginVia: "segment"}`
und `memoPath`/`segmentPath` auswerten. Ungeklärt, ob `gridOrigin` relativ (z. B. zu `begC`) zu lesen ist, da die
Rücklese {0,0,0} weit weg von `begC` liegt. Danach Merge nach main, Release, Spiegel nach -archicad.
