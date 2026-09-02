---
gsd_plan_version: 1.0
type: quick
slug: element-preview-render
created: 2026-09-01
---

# Quick Task — Element-Vorschau per API rendern + Renderer-Hänger

Zwei live verifizierte Befunde aus der ARZ-Möbel-Session (2026-09-01, Port 19724,
AC29 Teamwork, ELM_SAB 0.9.14). Datums-Marker `<!-- 2026-09-01 -->`.

## Befund 1 — `GetElementPreviewImage` liefert Bilder für Objekte ohne Export-Vorschau
Tapir-Befehl, 3D/512 px, Base64-PNG. 18 von 24 bildlosen Positionen gerendert.
2D-Variante ist nur das Grundriss-Symbol.

## Befund 2 — Schwere Legacy-Meshes hängen den Renderer und damit die JSON-API
`Pflanze_Mittel_1800`: ~7 min bei 95–120 % CPU, API taub. Regel: Positivfall zuerst,
einzeln mit Timeout, Abbruch beim ersten Hänger. Diagnose-Falle: bei mehreren
Instanzen Port→PID per `lsof` prüfen, sonst misst man die falsche Instanz.

## Tasks
1. `recipes/library-objects.md` — neuer Abschnitt „Vorschaubild eines Elements per API rendern" vor „Gotchas".
2. `reference/mcp-conventions.md` — Unterabschnitt „Renderer-Hänger" nach „beschäftigtes Modell";
   Absatz „Port → Prozess zuordnen" im Port-Handling.
3. `reference/schedule-pipeline.md` — Ausweg-Hinweis am Gotcha „Property leer, GDL-Parameter befüllt".
4. Commit, Push.
