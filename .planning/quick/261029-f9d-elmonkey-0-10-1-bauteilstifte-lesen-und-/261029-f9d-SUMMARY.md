---
phase: quick-261029-f9d
plan: 01
subsystem: ELM_SAB_Tapir (ELMonkey add-on), elmonkey-mcp, Doku
tags: [elmonkey, 0.10.1, bauteilstifte, archicad-addon, pens]
requires: [ELMonkey 0.10.0 / Tapir 1.7.2, ElmPreflight.hpp, ELMCommandBase.hpp]
provides: [ELM_SAB.GetConstructionPensOfElements, ELM_SAB.SetConstructionPensOfElements, ELM_SAB_VERSION 0.10.1]
affects: [THN-Bauteil-Inventur (eigener Schritt mit Freigabe)]
tech-stack:
  added: []
  patterns: [eine Feldtabelle mit Feldzeigern fuer Get und Set, Maske per Feld-Offset, ein Undo-Schritt fuer alle Elemente, nur selbst gesetzte Reservierungen freigeben]
key-files:
  created:
    - ELM_SAB_Tapir/Sources/ConstructionPensCommandsELM.hpp
    - ELM_SAB_Tapir/Sources/ConstructionPensCommandsELM.cpp
  modified:
    - ELM_SAB_Tapir/Sources/AddOnMain.cpp
    - ELM_SAB_Tapir/Sources/AddOnVersion.hpp
    - ELM_SAB_Tapir/Sources/ElmPreflight.hpp
    - README.md
    - ELM_SAB_Tapir/README.md
    - reference/mcp-extension.md
    - mcp/elmonkey-mcp/src/elmonkey_mcp/catalog.py
    - mcp/elmonkey-mcp/tests/test_catalog.py
    - ~/.scratch/elmonkey/tools/syn1.sh (außerhalb des Repos, Sicherung syn1.sh.bak_2026-10-29)
    - ~/.scratch/elmonkey/tools/gegenprobe_0100.py (außerhalb des Repos, Sicherung gegenprobe_0100.py.bak_2026-10-29)
decisions:
  - "SetConstructionPensOfElements plant in der Reihenfolge Rollen, dann clearCutFillOverride, dann fields; fields gewinnt"
  - "Synonyme in elmonkey-mcp dürfen mehrere Wörter haben (bauteilstifte -> construction pen), weil der Vorsprung mit nur pen 8:7 und damit wacklig war"
metrics:
  duration: ~45 min
  completed: 2026-10-29
---

# Quick 261029-f9d: ELMonkey 0.10.1 – Bauteilstifte lesen und setzen

Zwei neue ELM_SAB-Befehle mit einer gemeinsamen Feldtabelle nach den DevKit-Namen. Sie decken 8 Bauteiltypen ab. Get liefert zu jedem Element die Stifte, cutFill* als {overridden, pen}, die Linientypen, Ebene, Geschoss, Aufbau und auf Wunsch die Verbundschicht-Stifte. Set bildet die Rollen ab und kann die Schnittflächen-Überschreibung löschen. Dazu kommen Rohfelder, dryRun, Vorprüfung [CODE], Teamwork-Reservierung, ein einziger Undo-Schritt und eine Rücklese. ELM_SAB_VERSION ist jetzt 0.10.1.

## Commits (Zweig elm-0.10.1-bauteilstifte, nicht gepusht)

| Task | Commit | Betreff |
|---|---|---|
| 1 | 541d066 | feat(elm-0.10.1): GetConstructionPensOfElements + Feldtabelle |
| 2 | bb689ef | feat(elm-0.10.1): SetConstructionPensOfElements, Registrierung, Version 0.10.1 |
| 3 | eb61e26 | docs(elm-0.10.1): Bauteilstifte – Doku, MCP-Suchbegriffe |

## Syntaxprüfung (STRICT=1, syn1.sh)

| DevKit | ConstructionPensCommandsELM.cpp | AddOnMain.cpp |
|---|---|---|
| 27.3001 | OK | OK |
| 28.3001 | OK | OK |
| 29.3100 | OK | OK |
| 30.3000 | OK | OK |

Die übrigen Prüfungen:
- Versions-grep = 1.
- Registrierungs-grep = 2.
- `ACAPI_CallUndoableCommand` kommt außerhalb von Kommentaren genau einmal vor.
- `"penIndex"` kommt außerhalb von Kommentaren 0-mal vor.
- elmonkey-mcp meldet „alle Tests ok“, einschließlich test_search_bauteilstifte.
- `py_compile gegenprobe_0100.py` läuft ohne Fehler.

**Code-Review dryRun:** Ab Zeile ~700 (`if (!dryRun) { … }`) umschließt der Code sowohl `ACAPI_Teamwork_ReserveElements` (Z. ~709) als auch den einzigen `ACAPI_Element_Change` (Z. ~741, im Undo-Lambda). Außerhalb davon wird nur gelesen: Vorprüfung, `ACAPI_Element_Get` und die Planung.

## Deviations from Plan

1. **[Rule 3 – Blocking] RoleFieldsELM und SetMaskForFieldELM erst in Task 2.** Der Plan sah beide Helfer schon in Task 1 vor. Sie werden aber nur von Set benutzt, und mit STRICT (`-Wunused-function`) wäre Task 1 rot gewesen. Der Plan verlangt selbst „keine toten Helfer“. Deshalb kamen beide mit Commit bb689ef dazu.
2. **[Rule 2] Zusätzliche Synonyme und mehrwortige Synonyme in catalog.py.**
   - Neu sind `waenden/wänden -> wall`, `schnittflaeche/schnittfläche -> cut` und `ueberschreibung/überschreibung -> override`.
   - Grund: „Schnittflaeche Ueberschreibung loeschen Bauteile“ traf zuerst den Get-Befehl. Mit den neuen Wörtern trifft es jetzt Set (über clearCutFillOverride -> cut/override).
   - `bauteilstifte` zeigt auf `"construction pen"`. Dafür verarbeitet `_tokens` jetzt Synonymwerte mit Leerzeichen. Die bestehenden Werte haben keine Leerzeichen, ihr Verhalten bleibt also gleich.
3. **Wortlaut von [UNDO_SCOPE_FAILED]:** Die Meldung heißt jetzt „Undo-Klammer hat das Lambda nicht gestartet …“, nicht „ACAPI_CallUndoableCommand …“. So bleibt der grep-Zähler für den einen Undo-Aufruf bei 1.
4. **Get-Fehlereinträge mit elementId:** Fehler bei NOT_FOUND oder falschem Typ enthalten zusätzlich die `elementId`, damit man sie zuordnen kann.
5. **Set-Antwort:** Wenn nichts zu schreiben ist, steht in `undoScope` zusätzlich `nothingToWrite: true`.

## Annahmen für den Live-Test (Rollentabelle)

- Bei der Wand belegt `uncutLinePen` das Feld `contPen3D`, entsprechend „Unverdeckte Linien“. Das ist noch am Dialog zu prüfen.
- Bei der Stütze setzt `cutLinePen` zwei Felder: `corePen` UND `venLinePen`.
- Bei der Fassade setzt `cutLinePen` zwei Felder: `frameCutLinePen` UND `panelCutLinePen`.
- Bei der Decke setzt `cutLinePen` das Feld `sectContPen` und `uncutLinePen` das Feld `pen`. Für `overheadLinePen` gibt es kein Feld, der Befehl meldet dann [FIELD_NOT_AVAILABLE].
- Bei Morphs, Stützen und Unterzügen wird `ACAPI_Element_Change` ohne Memo aufgerufen, nur mit Maske auf die Stiftfelder. Ob Archicad das annimmt, ist live zu prüfen.
- Bei nicht überschriebenem cutFill* steht `pen` zwar im Feld, ist aber nicht maßgeblich.

## Offene Punkte

- CI-Build für AC27–30 × Mac/Win, erst nach dem Push des Zweigs durch den Orchestrator.
- Live-Test nur im unbenannten leeren Testprojekt, **nie in THN**: `~/.scratch/elmonkey/tools/gegenprobe_0100.py <port> stifte`. Das Skript bricht ab bei Teamwork, bei einem benannten Projekt und bei einer anderen Version als 0.10.1.
  - Schale: Es gibt keinen Create-Befehl, das Skript meldet SKIP. Die Schale muss man von Hand anlegen.
  - Undo lässt sich per API nicht prüfen, das Skript meldet MANUELL.
  - Die Morph-Ebene wird über ELM_SAB.SetLayerOfElements `layerName` gesetzt; das ist live zu prüfen.
- Das Bundle nur bei geschlossenem Archicad tauschen und vorher eine Rückbau-Kopie von 0.10.0 sichern.
- Danach in THN, als eigener Schritt mit Freigabe: Inventur mit `includeComposites`, Zielstift festlegen, dryRun, echter Lauf.

## Threat Flags

Keine neuen Angriffsflächen außer den im Plan modellierten (T-f9d-01..04). In `fields` sind nur Pen- und OverriddenPen-Felder aus der Feldtabelle zugelassen, und die Maske wird nur für diese Felder gesetzt.

## Self-Check: PASSED

- Die Dateien ConstructionPensCommandsELM.hpp und ConstructionPensCommandsELM.cpp sind vorhanden.
- Die Commits 541d066, bb689ef und eb61e26 sind im Log.
- Die Sicherungen syn1.sh.bak_2026-10-29 und gegenprobe_0100.py.bak_2026-10-29 sind vorhanden.
