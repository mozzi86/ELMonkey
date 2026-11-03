---
phase: quick-261103-h6w
plan: 01
status: complete
date: 2026-11-03
---

# Summary — Learnings THN-Schalenbau 2026-11-02

- recipes/wall-operations.md: neues Worked Example „Wand in Schichten aufteilen (Kern + Schalen)“ (11 Schritte, TOC-Eintrag), Gotchas 9–11 (offset bei Bezugslinie Mitte wirkungslos → Achse verschieben; neue Elemente bleiben reserviert; ohne floorPlanPolygons abbrechen + Fortsetzen mit Erstlesungs-Kanten).
- reference/mcp-extension.md: Bullet „Neu erstellte Elemente bleiben reserviert“ im Abschnitt Teamwork-Reservierung per API.
- Außerhalb des Repos: ~/.scratch/thn/tools/schalen_bauen.py nimmt jetzt jede neue Schale/Türöffnung sofort in die Freigabeliste auf (Backup .bak_2026-11-03); Kopie im THN-Projektordner skripte/.
- Keine Projekt-GUIDs/Ebenennamen im Skill (bleiben in Memory).
