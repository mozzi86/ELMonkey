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
