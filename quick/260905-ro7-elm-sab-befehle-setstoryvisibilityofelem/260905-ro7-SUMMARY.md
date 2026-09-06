# Quick Task 260905-ro7: ELM_SAB StoryVisibility-Befehle — Summary

Zwei neue ELM_SAB-Befehle (`SetStoryVisibilityOfElements`, `GetStoryVisibilityOfElements`)
für „Auf Geschossen zeigen" an Objekten und Lampen, registriert unter Version 0.9.15,
1:1 nach §3 des Recherchedokuments `elm_sab_storyvisibility_design.md` umgesetzt.

## Geänderte/neue Dateien

- `ELM_SAB_Tapir/Sources/StoryVisibilityCommands.hpp` (neu, 25 Zeilen)
- `ELM_SAB_Tapir/Sources/StoryVisibilityCommands.cpp` (neu, ~230 Zeilen)
- `ELM_SAB_Tapir/Sources/AddOnMain.cpp` (Include + 2 RegisterCommand-Zeilen im
  `elmSabCommands`-Block, Version 0.9.15)
- `ELM_SAB_Tapir/Sources/MigrationHelper.hpp` (`ACAPI_Teamwork_HasConnection`-Define
  im `#ifndef ServerMainVers_2700`-Block ergänzt)
- `ELM_SAB_Tapir/Sources/AddOnVersion.hpp` (`ELM_SAB_VERSION` 0.9.14 → 0.9.15)
- `ELM_SAB_Tapir/README.md` (Befehlstabelle um eine Zeile ergänzt)
- `reference/mcp-extension.md` (datierter Abschnitt `## ELM_SAB 0.9.15` angehängt)

## Umsetzung

- Code aus §3 des Entwurfs übernommen, `#include "MigrationHelper.hpp"` in der .cpp ergänzt
  (Pflicht für AC27-Zweig, Plan-Vorgabe).
- Union-Zugriff durchgehend über `element.object` (Object und Lamp, kein `.lamp`-Zweig) —
  `API_LampType` ist ein Alias auf `API_ObjectType`.
- Alle sechs Maskenfelder gesetzt, Rücklese in `ApplyToOne` als alleiniger Erfolgsmaßstab
  (bei `isAuto` nur `isAutoOnStoryVisibility` geprüft).
- Presets `HomeOnly`/`HomeAndOneUp`/`HomeAndOneDown`/`HomeAndOneUpAndDown`/`AllStories`/
  `AllRelevant`, `SpecToPreset` invers dazu, `Custom` als Fallback.
- `CMakeLists.txt` unverändert (GLOB sammelt `Sources/*.cpp` automatisch).

## Abweichungen vom Entwurf/Plan

- **`AddOnVersion.hpp` zusätzlich hochgezogen** (`ELM_SAB_VERSION` 0.9.14 → 0.9.15), obwohl
  weder Plan noch Entwurf diese Datei in `files_modified` nennen. Begründung: Datei selbst
  dokumentiert, dass sie „bei jeder Änderung an den ELM_SAB-Befehlen hoch" muss, sonst ist
  am laufenden Archicad nicht feststellbar, welcher Build geladen ist — Rule 2
  (fehlende Korrektheits-Voraussetzung, hier: Versionskonsistenz).

Sonst keine inhaltlichen Abweichungen von §3 des Entwurfs.

## Verifikation (ohne lokalen Build)

- Klammerbilanz `StoryVisibilityCommands.cpp`: 72/72 `{}`, 143/143 `()`.
- Beide `RegisterCommand`-Zeilen liegen nachweislich zwischen `// ELM_SAB Commands` und
  `AddCommandGroup (elmSabCommands)`.
- `git diff --stat` zeigt `CMakeLists.txt` NICHT als geändert.
- Kein C++20-Konstrukt (`std::span`, `<ranges>`, `co_await`, `consteval`) im neuen Code.
- Alle referenzierten DevKit-Felder (`API_StoryVisibility`, `isAutoOnStoryVisibility`,
  Masken-Makros) stammen 1:1 aus §1 des Recherchedokuments.

## Offene Risiken (aus dem Entwurf übernommen, unverändert gültig)

- `showRelAbove`/`showRelBelow` an Objekten laut DevKit-Doku „not extended" — vor
  Massenlauf an einem Testobjekt gegenprobieren (siehe `reference/mcp-extension.md`).
- Kein lokaler Build ausgeführt; CI (`build-elm-sab-tapir.yml`, Matrix AC27/28/29 × Mac/Win)
  ist der einzige Beleg, ob der AC27-Zweig mit `MigrationHelper.hpp` grün baut.

## Self-Check: PASSED

- `ELM_SAB_Tapir/Sources/StoryVisibilityCommands.hpp` — FOUND
- `ELM_SAB_Tapir/Sources/StoryVisibilityCommands.cpp` — FOUND
- Alle Verifikationsbefehle aus dem Plan liefen mit "OK"/"ok".
