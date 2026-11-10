---
slug: 261110-gyd
status: incomplete
date: 2026-11-10
---

# ELM_SAB 0.11.12.2 – liegende Fassaden und „Bodenfliese aus Auswahl“

## Ehrlicher Stand

- Zwischenstand, übernommen aus zwei lokal gebauten Patches (0.11.12.1 und 0.11.12.2). Live erprobt nur auf AC29 Mac.
- **Nicht gebaut:** Der CI-Build (AC27–30, Mac/Win, Windows /W4 /WX) steht aus. Darum `status: incomplete`.
- Nichts gepusht, kein Tag, kein Release.
- Das Bibliothekselement „SAB Bodenfliese liegend“ (.gsm/HSF) liegt bewusst nicht im Repo; der Menübefehl braucht es im Projekt.

## Commits

- `7395b23` feat(elm_sab): GetCurtainWallPlaneELM, SetCurtainWallSegmentELM, Menü „Bodenfliese aus Auswahl“
- `c7971af` docs(skill): Abschnitt „Liegende Fassaden“ in `skills/archicad/recipes/curtain-walls.md`
- `65e5509` chore: elmonkey-mcp 0.2.3 (Synonyme + Test)
- `db7f767` docs: Versionsstände in READMEs und `docs/UMBAU.md`
- `2a30413` docs(planning): PLAN.md und STATE.md-Zeile

## Prüfungen

- MCP: `uv run --with pytest pytest -q` in `mcp/elmonkey-mcp` → 9 passed (neu: `test_search_liegende_fassade`).
- JSON-Schemas: alle 6 Raw-Strings in `CreateCurtainWallCommand.cpp` parsen.
- Neue `.cpp` wird vom CMake automatisch erfasst (`Sources`-Ordner).

## Änderungen gegenüber den Patches

1. `BodenflieseAusAuswahlELM.cpp`: `GetAttributeIndex (…)` statt `ToInt32_Deprecated ()` (Muster `MigrationHelper.hpp`, `CommandBase.cpp`).
2. Dort expliziter `(Int32)`-Cast auf `BMGetHandleSize (…) / sizeof (…)` (Muster `AttributeCommands.cpp`).
3. `AddOnMain.cpp`: neuer Include hinter `ACAPinc.h`.
4. Projektname aus zwei Kommentaren entfernt; Kommentar nannte ein nicht vorhandenes `SetCurtainWallPlaneELM`.
5. Registrierungsversion der neuen Befehle 0.11.12.2.
6. Skill-Doku: `angle` 0 wirkt beim Anlegen derzeit nicht (die Patch-Fassung behauptete das Gegenteil).

## Abweichungen vom Plan

- Der Projektname steht in Bestandsdateien außerhalb dieses Tasks weiterhin; hier nur die neuen Stellen bereinigt.
- `core/README.md` und `mcp/elmonkey-mcp/README.md` haben keine Versionsstellen.

## Offene Risiken

- MSVC-Warnungen in rund 700 neuen Zeilen möglich.
- Zweites Menü mit Titel „ELMonkey SAB“ (ID 32050): Zusammenführung durch Archicad ungeprüft.
- `SetCurtainWallSegmentELM` nur an Fassaden mit genau einem Segment; nach Kontur-Änderung greift das Raster nicht; `height` wirkt nicht.
- `planeMatrix`/`planeOffset`/`angle` 0 beim Create ohne Wirkung.
