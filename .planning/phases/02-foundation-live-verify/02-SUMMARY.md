---
phase: 02-foundation-live-verify
status: complete
completed: 2026-05-19
---

# 02-SUMMARY — Foundation live verifizieren

## Was wurde getan

Phase 2 wurde inline mit Phase 1 durchgeführt (Sub-Agent-Registry nicht verfügbar; Live-MCP direkt nutzbar). Folgende MCP-Tools live verifiziert am Archicad 29 / Port 19723:

| # | Feld / Item | Tool | Status |
|---|---|---|---|
| 1 | Port der aktiven Instanz | `mcp__archicad__discovery_list_active_archicads` | ✅ verifiziert mit Aufruf |
| 2 | Projekt-Info (85 Felder) | `mcp__archicad__project_get_project_info_fields` | ✅ verifiziert mit Aufruf |
| 3 | Längeneinheit | — | ⚠ MCP-Gap, Workaround „Meter-Default" dokumentiert |
| 4 | Aktive Story | `mcp__archicad__project_get_stories` | ✅ verifiziert (gibt auch STORY-01-Liste) |
| 5 | Layer-Listing | `attributes_get_attributes_by_type` (Layer) | ✅ verifiziert mit Aufruf (paginiert) |
| 5 | Aktive Layer-Sichtbarkeit | `navigator_get_view_settings` → `attributes_get_layer_combination_attributes` | ✅ Tools dokumentiert |
| 6 | Pen-Tables | `attributes_get_attributes_by_type` (PenTable) | ✅ verifiziert mit Aufruf |
| 6 | Aktives Pen-Set | `navigator_get_view_settings` → `penSetName` | ✅ Tools dokumentiert |
| 7 | Klassifikations-Systeme | `classifications_get_all_classification_systems` | ✅ verifiziert mit Aufruf |
| 7 | Klassen in System | `classifications_get_all_classifications_in_system` | ✅ Tool dokumentiert |
| ATTR-01 | Universal-Attribut-Listing | `attributes_get_attributes_by_type` (12 Typen) | ✅ Enum dokumentiert |

## Bonus-Findings

- **Element-Capability-Tabelle für AC29** in `reference/mcp-conventions.md` ergänzt (6 erstellbar, 13 nicht).
- **Negative-Inference-Regel** in Discovery-Pattern aufgenommen: nach 2 erfolglosen Synonym-Versuchen Tool als „nicht existent" einstufen.
- **Konkreter Real-World-Test** mit Zone + Polylinie erfolgreich (Phase-1-Smoke-Test-Erweiterung).

## Definition of Done

- ✅ FOUND-03 inhaltlich vollständig (workflow-context.md mit allen 7 Feldern verifiziert).
- ✅ WARM-01 (STORY-01) verifiziert.
- ✅ WARM-02 (ATTR-01) verifiziert.
- ⚠ Feld 3 (Längeneinheit) bewusst als Gap dokumentiert mit Workaround statt erfundenem Tool-Namen.

## Lessons Learned (eingearbeitet)

- Subagent-Registry-Limit der Claude-Code-Session war beim Setup ein Hindernis (Phase 2 hätte mit Subagent-Spawn schneller laufen können). Workaround inline funktionierte.
- Discovery-Pattern liefert nicht immer den passenden Treffer beim ersten Versuch — Synonym-Familien (`create`/`add`/`insert`) sind oft notwendig. Niedergeschrieben in `reference/mcp-conventions.md` § Discovery.
- Projekt-spezifische Strings (User-Klassifikations-System, Layer-Konvention) sind verlockend in den Skill zu schreiben, aber gehören per Spec-Regel in Memory.
