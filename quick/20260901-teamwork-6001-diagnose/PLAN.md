---
gsd_plan_version: 1.0
type: quick
slug: teamwork-6001-diagnose
created: 2026-09-01
---

# Quick Task — Teamwork 6001 richtig deuten, API-Selektion ist ortsgebunden

Live-Befunde ARZ-Möbel-Session 2026-09-01 (Möbel_LOY, AC29 Teamwork, Port 19724),
Datums-Marker `<!-- 2026-09-01 -->`.

## Befund 1 — 6001 „TeamWork permission denied" = zuerst Ebene und Hotlink prüfen
22 Elemente auf ausgeblendeten Ebenen scheiterten trotz Reservierung; nach Einblenden 22/22.
Hotlink-Element (Master-ID „Hochbau") scheitert immer. DeleteElements meldet success ohne
Wirkung auf ausgeblendeter Ebene. Offener Dialog → 4001 mit Dialognamen.

## Befund 2 — API-Selektion sieht der Nutzer nicht, wenn sie auf anderem Geschoss liegt
17/17 per GetSelectedElements bestätigt, Nutzer sah nichts, manuelle Reservierung griff ins Leere.
Regel: FitInWindow vorher, Ebene sichtbar, oder gleich per API reservieren.

## Tasks
1. `reference/bulk-operations.md` — Diagnose-Reihenfolge am Anfang des 6001-Abschnitts.
2. `reference/mcp-conventions.md` — Unterabschnitte „4001 nennt den Dialog" und „API-Selektion ist ortsgebunden".
3. `recipes/library-objects.md` — Gotcha ergänzen: Preis nur in Property, Enum-Wert via Eigenschaften-Manager.
4. Commit + Push.
