---
quick_id: 261117-r7u
date: 2026-11-17
---
# SetCurtainWallSegmentELM: Rasterursprung (columnOrigin/rowOrigin)

Live gemessen 17.11.: `SetCurtainWallSegmentELM` ersetzt per `ReplacePattern` nur `nPattern/pattern/logic/endWithID`
von `cWSegPrimaryPattern`; `gridOriginType` bleibt. Bestehende Fassaden haben oft einen mittigen Ursprung, ungleiche
Feldfolgen ab dem Segmentanfang (z. B. `[0.85, 1.2, 1.2, 0.9, 1.2, 1.2, 0.87]`) sind so nicht treffbar.

Zweig `elm-0.11.14-cw-rasterursprung` von `origin/main`. PR #1 (`elm-diff-plattform`) nicht anfassen.

1. `CreateCurtainWallCommand.cpp`: Schema um `columnOrigin`/`rowOrigin` (`start`, `startFromCenter`,
   `alignToCenter`, `end`) ergänzen; in `Execute` einlesen, unbekannter Wert → `APIERR_BADPARS`; gesetzt →
   `gridOriginType` am Primary/Secondary-Pattern, Raster-Masken mitgeben (auch ohne `columnWidths`, Zellen
   unverändert). Antwort: `columnOrigin`/`rowOrigin` aus dem nach dem Change neu gelesenen Memo. Feldnamen gegen
   DevKit 27.3001, 28.3001, 29.2001/29.3100, 30.3000 prüfen.
2. Version: `ELM_SAB_VERSION` 0.11.14 (main trägt schon 0.11.13, Release heißt „ELMonkey 0.11.13“ – die ursprünglich
   genannte 0.11.12.3 wäre kleiner). Registrierungsbeschreibung, READMEs.
3. MCP 0.2.6: `SYNONYMS` ursprung/rasterursprung → origin, raster → pattern; neuer Test; bestehender
   `test_synonyms_waende_bereinigen` bleibt.
4. Skill `recipes/curtain-walls.md`: Punkt „Rasterursprung“ mit Marker 2026-11-17, „noch nicht live getestet“.
5. Push, CI-Build auf dem Zweig (Push-Trigger, Pfadfilter `archicad-addon/ELM_SAB_Tapir/**`). Kein Release, kein
   Merge, kein Spiegeln.

## Nicht in diesem Task

Installation, Live-Test in Archicad, Merge nach main, Spiegel nach -archicad.
