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
