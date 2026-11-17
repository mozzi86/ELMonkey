---
quick_id: 261117-k7x
date: 2026-11-17
---
# MCP-Suchbegriffe: Wände bereinigen

Am 17.11. gelernte Arbeitsweisen (Wände bereinigen: GetConnectedElements, TeamworkReceive, ReleaseElements,
ModifyWalls height) sollen über die Stichwortsuche des MCP auffindbar sein.

1. `SYNONYMS` in `mcp/elmonkey-mcp/src/elmonkey_mcp/addons/archicad/catalog.py` um einen kommentierten Block
   ergänzen (empfangen, freigeben, Wirtswand, verbunden, Umriss, Höhe, Wandhöhe, Überlappung, Doppelung, Rundung,
   Eckstück); keine bestehenden Einträge ändern, keine Duplikate.
2. Test in `tests/test_catalog.py`: alle neuen Schlüssel vorhanden und auf das Zielwort abgebildet.
3. Version MCP 0.2.5 (`pyproject.toml` und `__init__.py`, vorher 0.2.4 / 0.2.3 abweichend).

## Nicht in diesem Task

`core/`, `archicad-addon/`, `skills/`; kein Push.
