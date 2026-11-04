---
phase: quick-260907-lme
plan: 01
status: complete
subsystem: skill-content
tags: [oeffnungen, teamwork, mcp-conventions, thn-sud-kampagne]
requirements: [QUICK-260907-LME]
key-files:
  modified:
    - recipes/oeffnungen-aus-konturen.md
    - reference/mcp-conventions.md
    - .planning/STATE.md
metrics:
  duration: ~20 min
  completed: 2026-09-07
---

# Quick Task 260907-lme: Skill-Lehren THN-SuD-Kampagne 2026-09-06/07 Summary

Die Erkenntnisse der THN-SuD-Kampagne 2026-09-06/07 wurden als dublettenfreie
Einträge mit Marker `<!-- 2026-09-07 -->` in zwei Skill-Dateien ergänzt.

## Was gebaut wurde

**`recipes/oeffnungen-aus-konturen.md`** — neuer Abschnitt „Nachlese in
HL-/Trassenplänen: was ein Fund ist" mit 11 Punkten: Fund-Definition,
⚠️ Kernfalle „/"-Zeichen ist kein Ausschlusskriterium (verlor ~40% der Funde im
WG-Pilotlauf), Maß-Kaskade (Label → Kanalquerschnitt+10cm → `MASS?`), Einheit
mm/cm pro Blatt, Steigschacht-Abgrenzung gegen Treppen-Bruchlinien/Rundstützen/
kreuzschraffierte Stützen, Entdopplung über Label-Signatur/Klappen-ID,
Gewerk-Normalisierung „H/L/S"→„HLS" (Querverweis auf bestehende
`iSymbUse=0`-Regel), Klappen-Führung als BSK, Rohrhülsen als runde HD/DN statt
20/20-WD, Nebenzeichnungs-Skalierung, Registrierung ohne Achsraster über
3D-BoundingBoxen (Residuen ≤ 8mm am THN).

**`reference/mcp-conventions.md`** — zwei neue Abschnitte plus TOC-Einträge:
- „Teamwork: Konflikt trotz success, stille No-Ops" — `ReserveElements` meldet
  `success:true` UND `conflicts`-Array bei Fremdreservierung; Folge-Writes tun
  dann still nichts (Unterschied zum bestehenden 6001-Fall). Bool-Parameter
  müssen `true`/`false` sein, nicht `0`/`1`. Beam-Breite aus BoundingBox statt
  `GetDetailsOfElements`. Wandwirt-Suche auch in Unterzügen.
- „Werkzeug-Hygiene für Hilfsskripte" — Datum zur Laufzeit statt im
  Dateinamen, enge Globs, Exit-Code+Dateialter statt Dateipräsenz,
  zsh-Echo-Falle, ein Schreiblauf gleichzeitig.

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check: PASSED

- FOUND: recipes/oeffnungen-aus-konturen.md enthält Marker 2026-09-07
- FOUND: reference/mcp-conventions.md enthält beide neuen Abschnitte + TOC-Links
- Commit vorhanden (siehe unten)
