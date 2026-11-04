---
plan: 01-02
status: complete
completed: 2026-05-19
---

# 01-02-SUMMARY — Reference-Dateien skelettieren

## Was wurde getan

Vier Reference-Dateien unter `~/.claude/skills/archicad/reference/`:

| Datei | Zeilen | TOC | TODO-Marker |
|---|---|---|---|
| `mcp-conventions.md` | 137 | ✓ | ≥1 (Pagination-Felder) |
| `workflow-context.md` | 122 | ✓ | 20 (alle Warm-up-Felder außer Feld 1) |
| `bulk-operations.md` | 153 | ✓ | mehrere (Phase-5-Verifikation) |
| `self-improvement.md` | 108 | ✓ | 0 (Methodik fertig) |

### mcp-conventions.md

Discovery-Pattern-Details mit Synonym-Retry-Familien (create/delete/modify/list). Fehlerklassen-Tabelle (7 Symptome). **Vollständiger Confirm-Format-Beispielblock (D-06)** mit Einzelaufzählung (1-10) + Summary-Format (>10) + Antwort-Optionen. Paginierungs-Regeln. Port-Handling für 0/1/N Instanzen. Verhalten bei „nein" + Mid-Batch-Fehler.

### workflow-context.md

Alle 7 Warm-up-Felder skelettiert. **Feld 1 (Port) ist verifiziert** (`mcp__archicad__discovery_list_active_archicads`); alle anderen mit reinen TODO-Markern für Phase 2 (D-08). Feld 4 (Story) explizit als „volatil" markiert. STORY-01 + ATTR-01 als Phase-2-Sub-Recipes vorgemerkt mit 5 Listing-Sub-Tasks.

### bulk-operations.md

Read → Filter → Group → Confirm → Apply als 5-Punkte-Sequenz mit Erklärung pro Schritt. Klassifikations-Spezifika: System-Spezifizität (Pitfall P5), GUIDs vs Klartext, hierarchische Klassen. Pro-Element-Typ-Ableitungslogik (Wand Innen/Außen, Öffnung Tür/Fenster, Stütze tragend, Library-Object Kategorie). Mehrdeutigkeits-Handling mit „NICHT raten"-Regel. Mid-Batch-Fehlerverhalten ohne Auto-Rollback. Phase-5-TODOs.

### self-improvement.md

Content-vollständig (kein TODO). Reflection-Trigger-Frage exakt textuell präsent. Lern-Klassen-Tabelle mit 7 Zeilen. Datums-Marker-Format mit Beispiel. Status-Marker (`<!-- ÜBERPRÜFEN -->`). Verification-Loop (2 Fehler → flag, 3 stabile → clear). 5 explizite Grenzen („was NICHT in den Skill gehört").

## Was nicht getan wurde

- `recipes/*.md`-Dateien NICHT angelegt — gehören zu Phase 3-6.
- Keine konkreten MCP-Tool-Namen außer dem einen verifizierten (Feld 1) — D-08 strikt eingehalten.
- Inline-Tool-Name-Vermutungen mit `<!-- VERIFY -->`-Marker bewusst vermieden — D-08 Variante „rein TODO" gewählt.

## Verifikation

- Alle 4 Dateien haben TOC am Anfang (Pflicht ab 100 Zeilen, alle >100).
- workflow-context.md: 20 TODO-Marker (Plan forderte ≥6).
- self-improvement.md: 0 TODO-Marker (Plan forderte =0).
- Confirm-Format-Beispielblock in mcp-conventions.md (D-06).
- Voll-qualifizierte MCP-Toolnamen.
- One-level-deep-Verweis-Regel eingehalten (alle Links sind `reference/x.md` oder `recipes/x.md`).

## Decisions umgesetzt

- D-06 + D-07: Confirm-Format-Beispiel in mcp-conventions.md, nicht in SKILL.md.
- D-08: Reine TODO-Marker; keine Vermutungs-Tool-Namen.
- D-02: Erklärend-freundlicher Ton, deutsch.
