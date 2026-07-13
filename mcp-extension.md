# MCP-Stack: Architektur, Grenzen, Erweiterung (ELM_SAB_Add-On)

*Stand: 2026-07-13, live verifiziert am THN-Projekt (AC29, Teamwork).*

## Der Stack — wer spricht mit wem

```
Claude ──► tapir-archicad-mcp (Python, uv tool "archicad-server")
              │  nutzt: multiconn_archicad
              ▼
         Archicad JSON-API  +  Tapir Add-On (C++, TapirAddOn_AC29_Mac.bundle)
                            +  ELM_SAB_Add-On (C++, eigenes Bundle, Namespace ELM_SAB)
```

- MCP-Server: `uv tool list` → `tapir-archicad-mcp`; Update: `uv tool upgrade tapir-archicad-mcp`
  (danach Claude Desktop neu starten — laufende `archicad-server`-Prozesse bleiben sonst alt).
- Tapir-Bundle: `/Applications/Graphisoft/Archicad 29/Add-Ons/TapirAddOn_AC29_Mac.bundle`;
  Releases: github.com/ENZYME-APD/tapir-archicad-automation/releases (Archicad vorher beenden,
  nach Download `xattr -rd com.apple.quarantine` auf das Bundle).
- Eigene Befehle: `ELM_SAB_Add-On/` in diesem Repo (CMake, Vorbild tlorantfy/archicad-additional-json-commands).

## Was der MCP NICHT kann (verifiziert 2026-07-13, Tapir ≤1.5.3)

- **Stift/Farbe von Elementen ändern** — `elements_set_details_of_elements` kann nur
  Ebene/Geschoss/drawIndex (+ Wand-Geometrie). Kein Pen-Befehl in Tapir, keiner in der
  offiziellen JSON-API. → Genau dafür gibt es `SetPenOfElements` im ELM_SAB_Add-On.
- **Grafische Überschreibungen anlegen** — nur per Ansicht zuweisen
  (`navigator_set_view_settings`, Feld `graphicOverrideCombination`).
- `elements_get_details_of_elements` hat in AC29 einen Schema-Bug (pydantic
  `additionalProperties`-Fehler) → property-basiert lesen (siehe Memory/Workaround).

## Was der MCP KANN (oft übersehen)

- **Layout-Buch komplett lesen**: `navigator_get_navigator_item_tree` mit
  `{"navigatorTreeId": {"type": "LayoutBook"}}` — liefert Layouts, Subsets, Master
  und **platzierte Zeichnungen (DrawingItem) pro Layout mit Namen**. THN: 264 Layouts,
  357 Zeichnungen. Achtung: Antwort >500 KB → landet als Datei, mit Python parsen.
- Ansicht-Settings lesen/schreiben: `navigator_get_view_settings` / `navigator_set_view_settings`
  (LayerCombination, PenSet, MVO, GraphicOverride, DimStyle — pro View).
- 2D-Elemente inkl. Line/PolyLine/Arc/Circle/Spline/Hatch/Text/Label/Dimension per
  `elements_get_elements_by_type`, Filter `OnActualFloor`/`IsVisibleByLayer`;
  Positionen via `elements_get2_d_bounding_boxes`.
- Ebenenkombinationen: `attributes_get_attributes_by_type {"attributeType": "LayerCombination"}`
  (das parameterlose `attributes_get_layer_combinations` mit leerer Liste gibt LEER zurück — Falle!).

## Tapir ≥1.5.3 bringt neu (ggü. altem Stand „Wände nicht erstellbar")

*Live verifiziert 2026-07-13 (`app_get_add_on_version` → 1.5.3, MCP 0.4.3):*
`CreateWalls`, `CreateBeams`, `CreateWindows/Doors/Openings`, `CreateRoofs/Stairs/Morphs/
Lamps/Texts/Labels`, `ModifyWalls/Slabs/Columns/Beams/Windows/Doors/Roofs/Morphs/Meshes`,
`RotateElements`, `LockElements/UnlockElements`, `GetElementPreviewImage` (2D/Section/3D-PNG),
`GetRoomImage`, `CreateSections`, `CreateAssociativeDimensions`, `GetDimensionData`;
neue Modul-Gruppen `design_options`, `element_grouping`, `ifc`.
`recipes/wall-operations.md` ist entsprechend korrigiert.

## Update-Stolperfalle: Zweit-Instanzen (live erlebt 2026-07-13)

Nach Bundle-Tausch meldete `app_get_add_on_version` weiter die alte Version. Ursache:
**zwei Archicad-Prozesse** — eine Instanz lief seit Tagen im Hintergrund weiter (nur das
Projektfenster war zu). Vor dem Update mit `pgrep -fl Archicad` prüfen, dass ALLE
Archicad-Prozesse beendet sind (Cmd+Q, nicht nur Fenster schließen). Außerdem: keine
Backup-Kopien im `Add-Ons/`-Ordner lassen — Archicad versucht sie zu laden
(„Einige Add-Ons konnten nicht geladen werden").

## Erweiterungs-Entscheidungsbaum

1. **Kombi aus vorhandenen Befehlen?** → Python, `tools/custom/functions.py` im
   tapir-archicad-mcp-Paket (oder Skill-Script).
2. **Neues Primitiv (C++-API nötig)?** → Befehl ins **ELM_SAB_Add-On** (nicht Tapir forken).
   Muster: Command-Klasse von `ELMCommandBase` ableiten, in `AddOnMain.cpp` registrieren.
3. **Allgemein nützlich?** → Zusätzlich als PR an Tapir (Maintainer-Kontakt: tlorantfy,
   sehr aktiv — Forks tagesaktuell).

## Referenz-Repos

- github.com/ENZYME-APD/tapir-archicad-automation — Tapir (Add-On + Befehlsquellen unter
  `archicad-addon/Sources/*Commands.cpp`)
- github.com/tlorantfy/archicad-additional-json-commands — Blaupause „eigenes Zusatz-Add-On"
- github.com/tlorantfy/archicad-python-scripts — fertige Script-Patterns (recurring_publish u.a.)
- github.com/GRAPHISOFT/archicad-addon-cmake — Build-Template (Basis von ELM_SAB_Add-On)

## ELM_SAB_Add-On v0.1 — Live-Erkenntnisse (2026-07-13, THN-Farbabgleich)

**Status: FUNKTIONIERT.** Erster Produktiveinsatz: 32.806 Elemente gelesen (0 Fehler),
~9.200 Stift-Zuweisungen geändert (13→90, 240/19→91, 21→226, 93→253, 7→6).

**MDID-Pflicht:** Add-Ons laufen nur mit offiziell registrierter MDID (Developer-ID +
Local-ID, beide aus archicadapi.graphisoft.com/profile/add-ons — SAB: Dev-ID 944131939,
ELM_SAB Local-ID 1033975726). Selbstgewählte IDs ⇒ „kann nicht freigeschaltet werden /
Distributor" und stilles Nicht-Laden. Signatur (ad-hoc) ist dagegen KEIN Blocker.

**Schreib-Limits von ACAPI_Element_Change (v0.1):**
1. Nur Elemente der **aktiven Datenbank** änderbar (Lesen geht überall). Workaround:
   User klickt Arbeitsblätter durch, Auto-Retry-Schleife stiftet um. → v0.2: DB-Wechsel einbauen.
2. Elemente auf **ausgeblendeten Ebenen** sind nicht änderbar (err -2130312912-Familie).
   Vorher Ebenen sichtbar schalten (Ansicht mit passender Ebenenkombination öffnen).
3. **Teamwork:** Elemente müssen reserviert sein.
4. Fehler-Response ist verschachtelt: `executionResults[i].error.error.message`.

**Farbabgleich-Rezept (wiederverwendbar):** Inventur via GetPenOfElements (Batch 500)
→ RGB-Lookup in Stifttabelle (API.GetPenTableAttributes) → Mapping-Tabelle → Confirm
→ SetPenOfElements pro (Rolle, Ziel)-Gruppe → Nachher-Inventur als Verifikation.

## v0.2 (2026-07-13, gleicher Tag) — RGB-Overrides + Auto-DB-Wechsel

DWG-Importe brennen Füllungsfarben oft als **RGB-Override direkt ins Element**
(API_HatchType.hatchFlags: APIHatch_HasFgRGBColor/HasBkgRGBColor + foregroundRGB/backgroundRGB)
— unsichtbar für jede Stift-Logik. v0.2 kann beides:
- `GetPenOfElements` liefert `hasForeground/BackgroundRGB` + Farbwerte
- `SetPenOfElements` löscht Overrides (`clearForegroundRGB`/`clearBackgroundRGB`) und setzt
  zugleich den Ziel-Stift; optionaler `databases`-Param wechselt selbst durch die
  Arbeitsblätter (Tapir-Muster: ACAPI_Window_GetDatabaseInfo → ACAPI_Database_ChangeCurrentDatabase,
  Ausgangs-DB wird wiederhergestellt) — v0.1-Klickerei entfällt.
Produktiv: 1.878 Override-Schraffuren (27 Farben) in EINEM Lauf auf nächstliegende
SAB-Stifte gemappt, 0 Fehler. Anschließend Projekt-Vollscan (48 DBs: Grundriss, 41
Arbeitsblätter, Schnitte/Ansichten/Details): weitere 49.046 Overrides in 10 Import-
Arbeitsblättern (PDF Übersichtspläne 40k!, Möblierung, Sammel-Blätter, Plankopf) —
alle in einem Batch normalisiert, 49.046/49.046 ok. Voraussetzung: Ebenenkombination
„alles an" (Sichtbarkeit) + Teamwork-Reservierung. Nearest-Pen-Matching = RGB-Distanz gegen Stifttabelle;
Treffer meist exakt, da DWG-Farben ≈ AutoCAD-Palette ≈ Stift-Slots.
**DevKit-Header lokal:** ~/Developer/APIDevKit29/Support/Inc — Feldnamen IMMER dort
verifizieren statt raten.

## ELM_SAB v0.3 + v0.4 (2026-07-13 nachmittags) — Wände aus 2D

- **v0.3 `CreatePolygonWalls`**: Polygon-Wände (APIWtyp_Poly + Memo coords/pends,
  Muster: Tapir CreateZones-Allokation). Tapir 1.5.3 kann nur gerade Wände.
  Params: polygonCoordinates, floorIndex, height, bottomOffset, layerIndex,
  compositeIndex/buildingMaterialIndex. Braucht aktives Grundriss-Fenster.
- **v0.4 `Get2DGeometryOfElements`**: Line (beg/end), Arc/Circle (origin, r, ratio,
  angle, beg/endAngle, reflected, whole), PolyLine (coords + arcs mit arcAngle,
  1-basierte Indizes) + layerIndex/floorIndex. Liest cross-DB.
- Workflow-Doku: `recipes/konturen-zu-waende.md`. Erster Produktivlauf: 2.032 Wände.
- DevKit-Feld-Verifikation vor jedem neuen Befehl: ~/Developer/APIDevKit29/Support/Inc.
