---
status: complete
date: 2026-11-17
---
# MCP-Suchbegriffe: Wände bereinigen

- `SYNONYMS`: 19 neue Schlüssel (empfangen→receive, freigeben/freigabe→release, wirtswand/wirt/verbunden/verbundene→connected,
  umriss/grundrissumriss/überlappung/doppelung→geometry, höhe→height, wandhöhe→wall height, rundung/eckstück→polygon, mit
  ae/oe/ue-Schreibweisen).
- Neuer Test `test_synonyms_waende_bereinigen`.
- MCP 0.2.5; die Abweichung `__init__.py` 0.2.3 gegenüber `pyproject.toml` 0.2.4 ist behoben. Das README führt keine
  Versionszeile, daher dort nichts geändert.

Tests: `uv run --with pytest pytest -q` in `mcp/elmonkey-mcp`: 10 bestanden. Nicht gepusht.

## Nachtrag: Skill-Kopie angeglichen

Heutige Lerneinträge aus `mozzi86/-archicad` (Zweig `elm-0.10.4-fixes`, Commit 8e16d9a) nach `skills/archicad` übernommen: `recipes/wall-operations.md` (Überlappungen bereinigen, Polygonwand-Neubau, gerundete Ecken, Durchbruch-Wirtswand), `recipes/curtain-walls.md` (Spaltenraster nachträglich ändern), `reference/mcp-conventions.md` (Teamwork Receive, Pause-Abschluss), `reference/ui-aufgaben.md`, Halbsatz in `SKILL.md`. Nicht übernommen: Abschnitt „Holen aus Archicad“ und `recipes/archicad-nach-plattform.md` (Projekt- und Plattform-Namen im öffentlichen Repo noch nicht geklärt).
