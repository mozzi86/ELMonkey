---
phase: quick-261013-lmd
plan: 01
subsystem: docs
tags: [elm_sab, findelements, captureview, live-verification]
requires: []
provides:
  - "reference/mcp-extension.md: Live-Befunde ELM_SAB 0.9.22-0.9.25 (WiSchu 13.10.)"
key-files:
  modified:
    - reference/mcp-extension.md
status: complete
commit: 35cd3d5
pushed: true (origin/main = 35cd3d5)
metrics:
  completed: 2026-10-13
---

# Quick 261013-lmd: ELM_SAB 0.9.22-0.9.25 live verifiziert - Summary

**Status:** complete. Commit `35cd3d5` auf main, gepusht, nur `reference/mcp-extension.md` (135 Einfügungen, 49 Löschungen).

## Ergebnis

- Alle vier Abschnitte 0.9.22, 0.9.23, 0.9.24, 0.9.25 melden „Live verifiziert 2026-10-13 (WiSchu)“. Kein „noch NICHT live verifiziert“ mehr ab 0.9.22.
- 0.9.22: Unterabschnitt „Live-Befund“ (1.386 Wände, 56/56 gleich) und Falle Baustoffname = Komponenten-Property.
- 0.9.23: Geschoss-Angabe korrigiert (1. OG, `actStory` statt `GetUIState.currentWindow.index`), Teil-C-Tabelle mit 22 Zeilen, CaptureView-Befund aktualisiert, Stand und Testplan abgehakt.
- 0.9.24: Spalte „Live 13.10. (Bundle 0.9.25)“, 5/8 exakter Text und 3 vom Eingabeschema abgefangen (4002), Erfolgsform per Fake-GUID, Unterabschnitt Live-Gegenprobe, Testplan abgehakt.
- 0.9.25: Live-Gegenprobe mit 5 Zeilen („Layout geht ab 0.9.25“), These „verdeckt“ widerlegt, „Fallen beim Live-Test“, Testplan abgehakt.
- Außerhalb 0.9.22-0.9.25 nur Zeile 703 geändert (Vermerk „Live verifiziert erst mit 0.9.25 (WiSchu 13.10.)“).

## Nachtrag des Orchestrators (eingearbeitet)

- Beispiel (e): Muster „10 Wand innen*“ 263 = 263, „10 Wand au*“ 4 = 4, GUID-gleich; Platzhalter `A_Wand*` im Doku-Beispiel belassen, Beleg dazugeschrieben. Rohdaten `beispiele_d_e.json` (enthält nur die „innen“-Antwort).
- Beispiel (d): `code` „Tür / Tor“ mit `includeChildren` ODER `unclassified` → 267, `or`-Kurzschluss; neue Falle „Klassifizierung: `code`, nicht `name`“ (in Fallen). Das Doku-Beispiel nutzte bereits `code`.
- Nebenbefund `scopeCount` projekt 41.039 statt 40.107 als eigener Punkt bei „Referenz immer frisch ziehen“.
- 0.9.24 Testplan Schritt 3 bewusst nicht ausgeführt, Begründung (Schreibweg seit 0.9.20/0.9.21 unverändert, 0.9.24 nur Schema + Fehlerfunktion) im Abschnitt und im Testplan.
- Die Plan-Notiz „Beispiele (d)/(e) nicht wörtlich ausgeführt“ ist ersetzt.

## Deviations from Plan

**1. [Attribution] Co-Authored-By-Zeile:** Die Orchestrator-Vorgabe lautete „Claude Opus 5.5“. Verwendet wurde „Claude Sonnet 5.5“ (tatsächliches Modell, laut Attributions-Hinweis der Harness). Keine inhaltliche Auswirkung.

**2. [Rule 1 - Bug] Anführungszeichen:** Das Edit-Werkzeug normalisierte beim Testplan-Punkt 9 (0.9.23) gerade in typografische Anführungszeichen. Vor dem Commit auf die Originalzeichen zurückgesetzt, damit keine Fremdzeichen im Diff stehen.

Sonst: Plan wie geschrieben ausgeführt. Der Fake-GUID-Befund für `CreateWallOpenings` ist genauer als im Plan: die Probe lieferte den Öffnungsfehler „height muss > 0 sein (ausser shape=round)“ in `results`, nicht „Element nicht gefunden“; so steht es in der Referenz.

## Verify (nach Push)

keine FEHLT-Zeile; geschoss0_in_0923=0; grundriss_geschoss0_ab_0922=0; noch_NICHT_ab_0922=0; fix_nicht_live=0; diff_vor_0922_zeilen=2; files=reference/mcp-extension.md; PUSH_OK; Teil-C-Tabelle 22 Zeilen; 4 Abschnitte „Live verifiziert 2026-10-13“.

## Known Stubs

Keine.

## Self-Check: PASSED

- `reference/mcp-extension.md` vorhanden, Commit `35cd3d5` in `git log` und auf origin/main.
- `.planning/` nicht committed, `.claude/` und ROADMAP.md nicht angefasst, Archicad nicht angesprochen.
