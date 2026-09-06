---
phase: quick-260906-hs0
plan: 01
subsystem: skill-docs
tags: [layer-index, oeffnungen, mcp-conventions, thn-live]
requires: []
provides: [layerIndex-gotcha, durchbruch-symbol-geschosslogik, mcp-betriebs-fallen]
affects: [reference/attribute-und-ebenen.md, recipes/oeffnungen-aus-konturen.md, reference/mcp-conventions.md]
tech-stack:
  added: []
  patterns: [selbst-verbessernder Skill mit Datums-Markern]
key-files:
  created: []
  modified:
    - reference/attribute-und-ebenen.md
    - recipes/oeffnungen-aus-konturen.md
    - reference/mcp-conventions.md
decisions: []
metrics:
  duration: ~10 min
  completed: 2026-09-06
---

# Quick Task 260906-hs0: Live-Erkenntnisse THN 2026-09-06 Summary

Drei am laufenden THN-Teamwork-Projekt verifizierte Erkenntnisse in die passenden
Referenz-/Recipe-Dateien eingetragen, alle mit Datums-Marker `<!-- 2026-09-06 -->`.

## Was gemacht wurde

1. **`reference/attribute-und-ebenen.md`** — Abschnitt „## Ebenen": „Zwei Dinge" →
   „Drei Dinge", neuer dritter Bullet zum `layerIndex`-Gotcha (Index ≠ Listenposition,
   nativer Weg `API.GetAttributesIndices`, live THN-Werte).
2. **`recipes/oeffnungen-aus-konturen.md`** — neuer Abschnitt am Dateiende
   „## Geschosslogik + Beschriftung der Durchbruch-Symbole": Symbol-Eigenlogik
   (`lineTypeFloor`/`lineTypeCeiling`, BD/DD-Textwechsel), Verweis per Markdown-Link
   auf `reference/mcp-extension.md#elm_sab-0915--auf-geschossen-zeigen` statt
   erneuter Befehlsdoku, plus `iSymbUse`/`bShowPrefix`-Beschriftungsgrammatik.
3. **`reference/mcp-conventions.md`** — neuer Abschnitt am Dateiende
   „## Betriebs-Fallen (THN-Live-Sitzung)": 600-s-Zeitgrenze inkl. „timed out"-Retry,
   Aktionsbefehle nie mit leeren Parametern testen (`FitInWindow`/`ChangeWindow`
   löschen Nutzer-Selektion), Archicad nie per `kill` beenden.

## Deviations from Plan

None — plan executed exactly as written. Alle drei Textblöcke wortgleich aus dem
Plan übernommen (nur die plan-eigenen äußeren „…"-Zitatzeichen entfernt, die im
PLAN.md lediglich den einzufügenden Literal-Text markierten).

## Verifikation

- `grep -c '2026-09-06'`: attribute-und-ebenen.md=2, oeffnungen-aus-konturen.md=2,
  mcp-conventions.md=3 (jeweils ≥1, wie gefordert).
- `git diff --stat`: nur Zusätze in den drei Zieldateien (attribute-und-ebenen.md
  zusätzlich 1 Zeile geändert: „Zwei"→„Drei"), keine Löschungen bestehender Zeilen.
- Alle drei Task-Verify-Greps (TASK1/2/3 OK) sowie die drei Gesamt-Verifikationspunkte
  aus dem Plan bestanden.
- Kein Inhalt aus Commit 0803c3f (`SetStoryVisibilityOfElements`-Parameter-Schema)
  wurde dupliziert — nur Markdown-Link auf `mcp-extension.md`.

## Self-Check: PASSED

- FOUND: reference/attribute-und-ebenen.md (enthält „Drei Dinge"/`GetAttributesIndices`)
- FOUND: recipes/oeffnungen-aus-konturen.md (enthält „## Geschosslogik …")
- FOUND: reference/mcp-conventions.md (enthält „## Betriebs-Fallen (THN-Live-Sitzung)")
