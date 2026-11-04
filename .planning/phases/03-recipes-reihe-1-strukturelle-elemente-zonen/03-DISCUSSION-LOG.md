# Phase 3 — Discussion Log

**Date:** 2026-05-19
**Mode:** discuss (default), batched 4 questions in one round given user efficiency preference established in Phase 1.

## Areas Discussed

### Area 1 — Recipe-Tiefe
**Options:**
- 1 Haupt-Worked-Example + alle Operationen kurz
- Vollständige Worked Examples pro CRUD-Operation
- Minimal — nur Discovery-Anker + Gotchas

**Selected:** Vollständige Worked Examples pro CRUD-Operation
**Captured as D-16.** Token-investiert für gründliche v1.

### Area 2 — Test-Strategie
**Options:**
- Bestehende Zone+Polylinie wiederverwenden + neue minimal anlegen
- Alles neu — sauberes Test-Set anlegen
- Nur lesende Tests — User zeichnet

**Selected:** Alles neu — sauberes Test-Set anlegen
**Captured as D-17.** Cleanup Zone+Polylinie aus Phase 1, dann frisches Set.

### Area 3 — Wall-Recipe-Struktur
**Options:**
- Prominent oben — 'No-Create-Disclaimer'
- Als Gotcha am Ende
- Recipe umbenennen zu 'wall-operations.md' mit Fokus auf Modifikation

**Selected:** wall-operations.md Rename
**Captured as D-18.** Klare Namensgebung > Disclaimer-Block.

### Area 4 — Reihenfolge der 4 Recipes
**Options:**
- Zones → Slabs/Columns/Beams → Walls → Openings (Quick-Win-first)
- Walls → Openings → Slabs/Columns/Beams → Zones (Bootstrap-Plan-Reihenfolge)
- Parallel — alle 4 gleichzeitig durch Subagents

**Selected:** Parallel via Subagents
**Captured as D-19** mit Realitäts-Korrektur: Subagents haben keine `mcp__archicad__*`-Tools. Architektur angepasst:
- Wave 0 (orchestrator): MCP-Cleanup + Test-Set + Schema-Sammlung
- Wave 1 (4 parallele Subagents): Recipe-Schreiben aus Schemas
- Wave 2 (orchestrator): Live-Verifikation + Index-Updates

## Deferred Ideas

Im Verlauf der Diskussion entstand kein Scope-Creep. Bestehende Backlog-Einträge (DWG-Pipeline, Bulk-Klassifizierung in Phase 5) bleiben unangetastet.

## Claude's Discretion

- **Worked-Example-Strukturierungs-Details** pro Recipe — was genau in jedem der 4 Examples gezeigt wird (welche Parameter, welche Element-Typen) — bleibt im Ermessen der jeweiligen Subagents, sofern D-16 (vollständig pro Operation) + D-20 (SAFE-Regeln respektiert) eingehalten werden.
- **Schema-Sammlung-Format** für SCHEMAS.md (das Datei das die Subagents als Input bekommen) — JSON-Snippets vs. Markdown-Tabellen — wähle ich beim Ausführen.
- **User-Aufforderungstexte** für die manuell zu zeichnenden Test-Elemente (Wand, Fenster, Beam) — formuliere ich beim Execute.

## Notes

- Pre-Phase-3-State: 16 Commits, 5 Skill-Files (84+137+108+124+153=606 Zeilen ohne Frontmatter), GSD-Planning-Artefakte komplett.
- Phase 3 wird die größte produzierende Phase — geschätzt 4 Recipes × 300-450 Zeilen = ~1.500 Zeilen neuer Skill-Content + Cleanup + Index-Updates + 4 SUMMARYs.
- Beim Subagent-Spawn: Modell sonnet (config-default „balanced"), parallel: true, isolation per worktree wird durch GSD-execute-phase gehandhabt.
