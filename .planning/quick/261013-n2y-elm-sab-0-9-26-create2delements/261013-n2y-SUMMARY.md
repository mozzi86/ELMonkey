---
phase: quick-261013-n2y
plan: 01
subsystem: ELM_SAB_Tapir
tags: [elm_sab, 2d, hatch, spline, create, archicad-addon]
requires: [ELMCommandBase, ELMResponseSchemaWithError, Tapir CommandBase helpers]
provides: [ELM_SAB.Create2DElements, Get2DGeometryOfElements Spline-Zweig, ELM_SAB_VERSION 0.9.26]
affects: [ELM_SAB_Tapir/Sources/AddOnMain.cpp, reference/mcp-extension.md, recipes/fills-hatches.md, ELM_SAB_Tapir/README.md]
tech-stack:
  added: []
  patterns: [Pruefen -> ein Undo-Lambda -> Ruecklese nach dem Lambda (Muster CreateLabels), C2D-Praefix im anonymen namespace]
key-files:
  created:
    - ELM_SAB_Tapir/Sources/Create2DElementsCommand.hpp
    - ELM_SAB_Tapir/Sources/Create2DElementsCommand.cpp
  modified:
    - ELM_SAB_Tapir/Sources/AddOnMain.cpp
    - ELM_SAB_Tapir/Sources/AddOnVersion.hpp
    - ELM_SAB_Tapir/Sources/Get2DGeometryCommand.cpp
    - ELM_SAB_Tapir/Sources/Get2DGeometryCommand.hpp
    - ELM_SAB_Tapir/README.md
    - reference/mcp-extension.md
    - recipes/fills-hatches.md
decisions:
  - "Rundweg-Aliase floorIndex/penIndex/ltypeIndex (Feldnamen des Lesebefehls) werden als Rueckfall fuer floorInd/pen/lineTypeIndex akzeptiert"
  - "drawIndex-Bereich wird je Element geprueft, nicht per Schema (sonst wuerde ein einzelnes Element den ganzen Aufruf abweisen)"
  - "Zusatzfelder hasForegroundRGB/hasBackgroundRGB in der Hatch-Ruecklese, lengthApproximate bei Ellipsen"
metrics:
  duration: ~8 min
  completed: 2026-10-13
  tasks: 3
  files: 9
---

# Quick 261013-n2y: ELM_SAB 0.9.26 Create2DElements Summary

Neuer Befehl `ELM_SAB.Create2DElements` legt Line, Arc, Circle, PolyLine, Spline und Hatch in der aktiven Grundriss-Datenbank an: Pruefung je Element, ein Undo-Schritt, Ruecklese (Geschoss, Ebene, Koordinaten mit Ringtoleranz, Laenge bzw. Flaeche/Umfang ueber ACAPI_Element_GetQuantities). Dazu ein Spline-Zweig in `Get2DGeometryOfElements` (0-basiert, bezierDirs) und Doku. Eingebaut, noch NICHT live verifiziert.

## Tasks

| # | Task | Commit |
|---|------|--------|
| 1 | Create2DElementsCommand + Registrierung + Version 0.9.26 | e7ec585 |
| 2 | Spline-Zweig in Get2DGeometryOfElements | d5fa36a |
| 3 | Doku (README, mcp-extension.md 0.9.26, fills-hatches.md Worked Example) | 3f38981 |

## Verification (actual output)

- `elm_syntax_check.sh` (DevKit 29.3100): `OK Create2DElementsCommand.cpp`, `OK Get2DGeometryCommand.cpp`, `OK AddOnMain.cpp`
- `elm_schema_scan.py`: `Create2DElementsCommand  Create2DElementsCommand.cpp  errorAllowed=True (ELMResponseSchemaWithError) topLevelErrorReturns=1`
- JSON-Parse der R"(...)"-Schemas: `schemas ok 2` (Create2DElements), `schemas ok` (Get2DGeometry)
- Registrierung genau 1x, `#define ELM_SAB_VERSION "0.9.26"`, Undo-Name, APIHatch_HasFgRGBColor, ACAPI_Element_GetQuantities: alle vorhanden
- C++20-Grep (span, <=>, format, concept, requires, designated initializers): 0 Treffer
- `recipes/lines-polylines.md` in keinem der 3 Commits; 3/3 Commits mit Trailer

## Deviations from Plan

**1. [Rule 1 - Bug] UniString-Konkatenation in ObjectState::Add**
- Found during: Task 1 (Syntaxpruefung)
- Issue: `err.Add ("message", UniString + UniString)` liefert `UniString::Concatenation`, fuer das kein `Store` existiert
- Fix: explizit in `GS::UniString (...)` gewickelt
- Commit: e7ec585

**2. [Rule 2 - Rundweg] Aliase fuer Lesebefehl-Feldnamen**
- Der Lesebefehl liefert `floorIndex`, `penIndex`, `ltypeIndex`; die Spec-Eingabe heisst `floorInd`, `pen`, `lineTypeIndex`. Ohne Aliase waere der 1:1-Rundweg (Spec 1, must_have) still auf das aktuelle Geschoss/Default-Stift gefallen. Aliase gelten nur als Rueckfall; dokumentiert in mcp-extension.md.

**3. [Rule 2] drawIndex ohne minimum/maximum im Schema**
- Bereichspruefung 1..7 laeuft je Element im Code (Spec 4.1: Fehler je Element statt Schema-Ablehnung des ganzen Aufrufs).

**4. Zusatzpruefungen (Rule 2, Eingabevalidierung T-n2y-01):** Line mit zusammenfallenden Punkten, Arc mit begAngle == endAngle, nicht-endliche Zahlen, Stiftbereiche (pen/foregroundPen 1..255, backgroundPen/contourPen 0..255) werden je Element abgelehnt.

**5. Orchestrator-Vorgabe:** Worktree-Base war 6e66cac statt 10238b7 -> laut worktree_branch_check per `git reset --hard 10238b7` korrigiert (vor jeder Arbeit). STATE.md/ROADMAP.md nicht angefasst (Constraint).

## Known Stubs

Keine. Hinweis: Spline ohne bezierDirs verlaesst sich auf `autoSmooth = true` + `bezierDirs = nullptr` (ACAPinc-Create-Tabelle); Rueckfall `Geometry::CalcSpline` nur als Code-Kommentar — live zu pruefen (Spec 8.2).

## Offene Punkte

- Echter AC27/28/29-Build ueber CI und Live-Abnahme nach Spec 8 (Schraffur zuerst, Frist 15.10.).
- AC27/28: `ACAPI_Element_GetQuantities`/`API_QuantitiesMask` nur gegen DevKit 29 syntaxgeprueft.
- Kein Push.

## Self-Check: PASSED

- FOUND: ELM_SAB_Tapir/Sources/Create2DElementsCommand.hpp, Create2DElementsCommand.cpp
- FOUND commits: e7ec585, d5fa36a, 3f38981
