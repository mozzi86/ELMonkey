# Phase 3: Recipes Reihe 1 — Strukturelle Elemente + Zonen - Context

**Gathered:** 2026-05-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 3 liefert vier Recipe-Dateien für die strukturellen Standardelemente + Zonen, jede mit vollständigen Worked Examples pro CRUD-Operation. Live-verifiziert am laufenden Archicad-Projekt.

**In scope:**
- `recipes/wall-operations.md` (umbenannt von walls.md — Fokus auf Modifikation, kein Create)
- `recipes/openings.md` (Read+Update+Delete+Classify, kein Create)
- `recipes/slabs-columns-beams.md` (Slabs+Columns voll CRUD, Beams ohne Create)
- `recipes/zones.md` (voll CRUD)
- Test-Set Cleanup (Zone „Raum 01" + Polyline aus Phase 1 löschen) und frisches Test-Set
- Update SKILL.md Recipe-Index (walls.md → wall-operations.md)
- Update REQUIREMENTS.md WALL-01 Pfad

**Out of scope:**
- Recipes Reihe 2 (Curtain Walls, Library Objects) → Phase 4
- 2D-Recipes (Fills, Lines/Polylines, Surfaces) → Phase 6
- Bulk-Klassifizierungs-Worked-Examples → Phase 5 (wird in vorhandene Recipes nachgezogen)
- DWG-zu-BIM-Pipeline → Backlog (Phase 9 / v1.1)
</domain>

<decisions>
## Implementation Decisions

### D-16: Recipe-Tiefe — vollständige Worked Examples pro CRUD-Operation
Pro Recipe-Datei: 4 Worked Examples (Create wenn verfügbar / Read / Update / Delete), jeweils mit realem Aufruf-Beispiel und erwarteter Ergebnis-Struktur. Plus Classify-Sub-Example wo sinnvoll. Geschätzte Datei-Größe: 300–450 Zeilen pro Recipe.

**Warum:** Tiefe Doku heute spart Discovery-Detours später. User akzeptiert die Token-Investition.

### D-17: Test-Strategie — alles neu, sauberes Test-Set
- **Cleanup zuerst:** Zone „Raum 01" (GUID `0a953674-5992-7441-943f-e464b80f2c23`) und Polylinie (GUID `62f9ca34-2a34-704c-a4ee-6775d08e0792`) aus Phase 1 löschen.
- **Frisches Test-Set in Story 0 (EG):** 1 Slab + 1 Column + 1 Zone (per MCP erstellbar). User-Zeichnung notwendig: 1 Wand (z. B. 4m gerade) + 1 Fenster in der Wand + 1 Beam (sonst keine Beams da). Diese User-Schritte werden in der Phase-3-Execute-Phase explizit angefragt.
- **Nach Phase 3:** Test-Elemente bleiben im Projekt — User kann sie selbst weglöschen oder als Demo behalten.

**Warum:** Saubere Trennung zwischen Phase-1-Smoke-Test-Resten und Phase-3-Verifikations-Set. Verhindert ID-Verwechslungen.

### D-18: walls.md → wall-operations.md umbenennen
Der Recipe-Name signalisiert direkt, dass die Wand-Operationen nur Read/Update/Delete/Classify abdecken (kein Create per MCP v29). Verhindert, dass Claude im Recipe nach einem Create-Section sucht.

**Folgeaktionen:**
- SKILL.md Recipe-Index: Link `recipes/walls.md` → `recipes/wall-operations.md` ändern.
- REQUIREMENTS.md WALL-01: Dateiname-Verweis aktualisieren.
- ROADMAP.md Phase 3 Plan-Liste: Plan-Name auf `wall-operations` ändern.

### D-19: Parallel-Subagents mit Orchestrator-Vorbereitung
GSD-Executor-Subagents haben **keine** direkten `mcp__archicad__*`-Tools. Architektur für Phase 3 Execute:

1. **Wave 0 (orchestrator-only, kein Subagent):**
   - Cleanup: Delete-Calls auf Phase-1-Test-Elemente.
   - Setup: Create-Calls für Test-Set (Slab, Column, Zone). User-Aufforderung für Wand + Fenster + Beam.
   - Schema-Sammlung: pro Element-Typ Discovery + Schema-Read.
   - Schema-Paket-Erstellung: konsolidierte Schemas als Datei `.planning/phases/03-.../SCHEMAS.md` ablegen für Subagent-Konsumierung.

2. **Wave 1 (4 parallele gsd-executor Subagents):**
   - Jeder Subagent bekommt: SCHEMAS.md, PROJECT.md, REQUIREMENTS.md (sein REQ-ID), Phase-1+2 SUMMARYs, Skill-State-Files (SKILL.md, reference/*.md), eigenen PLAN.md.
   - Jeder Subagent schreibt seine Recipe-Datei mit 4 Worked Examples basierend auf den vorab-gesammelten Schemas.

3. **Wave 2 (orchestrator + ggf. Subagent für Validation):**
   - SKILL.md-Index + REQUIREMENTS-Pfad aktualisieren (D-18 Folgeaktionen).
   - Cross-Read der 4 Recipes für Konsistenz (Stil, Voll-Qualifikation, TOC bei >100 Zeilen).
   - Live-Verifikation der Worked Examples gegen den realen MCP-Server (orchestrator-only).

**Warum:** Maximiert Parallelität für die 4 großen Recipe-Schreibaufgaben, ohne die MCP-Realitäts-Constraint zu ignorieren.

### D-20: SAFE-Regeln gelten unverändert weiter
Phase 3 Worked Examples respektieren SAFE-01..05. Update-Operations zeigen Confirm-Format-Beispiele aus `reference/mcp-conventions.md`. Delete-Operations zeigen Hosted-Element-Pre-Check (relevant z. B. bei Wand-Delete → Fenster). Element-ID-Threading durch alle Worked Examples konsistent.

</decisions>

<canonical_refs>
## Canonical References

### Project-Level
- `.planning/PROJECT.md` — Core Value + Constraints (insb. MCP-Abhängigkeit, Live-Verifikation Pflicht)
- `.planning/REQUIREMENTS.md` — WALL-01, OPEN-01, STRU-01, ZONE-01 (die 4 Items dieser Phase)
- `.planning/ROADMAP.md` § Phase 3 — Goal + Success Criteria + 4 Plans

### Prior Phase Outputs
- `.planning/phases/01-skill-ger-st-safety/01-CONTEXT.md` — Phase-1-Decisions (D-01..D-09: Ton, Description, Confirm-Position, Skelett-Stil)
- `.planning/phases/02-foundation-live-verify/02-CONTEXT.md` — Phase-2-Decisions (D-10..D-15: Live-Probe-Methodik, Feld-3-Gap, projekt-spezifische Strings in Memory)
- `.planning/phases/01-skill-ger-st-safety/01-03-SUMMARY.md` — Phase-1-Smoke-Test-Erkenntnisse
- `.planning/phases/02-foundation-live-verify/02-SUMMARY.md` — Phase-2-verifizierte Tools

### Skill Foundation (live-verifiziert)
- `SKILL.md` — Hub mit allen Safety-Rules
- `reference/mcp-conventions.md` § Capability-Tabelle — was creatable/nicht-creatable
- `reference/workflow-context.md` — die 7 Warm-up-Felder + ATTR-01
- `reference/bulk-operations.md` — Read→Filter→Group→Confirm→Apply-Pattern (vorgemerkt für Phase 5)
- `reference/self-improvement.md` — Reflection-Trigger, Lern-Klassen

### Research
- `.planning/research/PITFALLS.md` § P5, P6 — Klassifikations-System-Cross-Pollution + Hosted-Element-Orphaning (relevant für Wall+Opening Delete)
- `.planning/research/ARCHITECTURE.md` — Build-Order (Walls vor Openings wegen Host-ID — gilt weiter, auch wenn Walls nur modifizierbar)

### Memory (nicht im Skill, aber für Phase 3 Live-Tests wichtig)
- `~/.claude/projects/-Users-ap/memory/project_archicad_mcp_capabilities.md` — AC29 Create-Surface
- `~/.claude/projects/-Users-ap/memory/project_archicad_user_setup.md` — User-Klassifikationssysteme (SAB_Klassifizierung_29) für Classify-Beispiele

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- **SKILL.md Recipe-Index** verlinkt bereits alle 9 Recipe-Dateien — wir müssen nur den Link `walls.md` → `wall-operations.md` umbenennen, die anderen 3 (openings, slabs-columns-beams, zones) sind schon korrekt verlinkt.
- **Recipe-Template** ist in PROJECT.md / 01-CONTEXT.md / writing-skills-best-practices implizit definiert: Umfang → Erforderlicher Warm-up-Kontext → Discovery-Anker → Typische Parameter → Operationen (CRUD-Sektionen) → Bulk-Klassifizierung-Stub → Gotchas → Worked Example. Wird jetzt mit 4-Worked-Examples-Variante angereichert.

### Established Patterns

- **Datums-Marker** in Skill-Files: `<!-- 2026-05-19 verifiziert AC29 -->` für live-verifizierte Tool-Namen.
- **Voll-qualifizierte MCP-Toolnamen** durchgängig (`mcp__archicad__elements_create_zones`, nicht `elements_create_zones`).
- **One-level-deep References:** Recipe verweist auf `reference/foo.md`, nicht auf `reference/foo/bar.md`.
- **TOC bei >100 Zeilen** Pflicht. Mit 4 Worked Examples pro Recipe werden alle 4 Recipes >100 Zeilen → alle brauchen TOC.

### Integration Points

- **Element-ID-Threading-Beispiel:** Im `openings.md`-Recipe Worked Example demonstrieren, wie Wand-ID aus Phase-3-Test-Wand als Host für Fenster verwendet wird. Das ist die kanonische SAFE-05-Demonstration.
- **Hosted-Element-Pre-Check:** Im `wall-operations.md` Delete-Worked-Example zeigen, wie vor Wand-Löschen `elements_get_connected_elements` mit `connectedElementType: Window` aufgerufen wird (Tool-Name aus Phase-2-Discovery bekannt).

</code_context>

<deferred>
## Noted for Later

- **Bulk-Klassifizierungs-Worked-Examples in den 4 Recipes** → Phase 5. Sektion „Bulk-Klassifizierung" bleibt in Phase 3 als Stub mit Verweis auf `reference/bulk-operations.md`.
- **DWG-zu-BIM-Pipeline** → Backlog (Phase 9 / v1.1-Milestone). Bereits in REQUIREMENTS.md § v2 + ROADMAP § Backlog notiert.
- **Performance-Tests bei großen Element-Mengen** (z. B. 1000+ Wände lesen) → Phase 5 oder Phase 7.
- **IFC-Klassifikations-Konsistenz-Check** (Wand hat Klassifikation, aber nicht in „IfcWall"-kompatibler Klasse) → v2.

</deferred>

<success_signals>
## Success Signals (for Plan-Phase to verify)

Phase 3 ist erfolgreich, wenn:

1. Alle 4 Recipe-Dateien existieren in `recipes/`:
   - `wall-operations.md` (umbenannt)
   - `openings.md`
   - `slabs-columns-beams.md`
   - `zones.md`
2. Jede Recipe-Datei enthält 4 Worked Examples (Create wenn verfügbar / Read / Update / Delete) plus Bulk-Klassifizierungs-Stub.
3. Alle Worked Examples sind live am Archicad-Projekt verifiziert — jede tatsächlich abgesetzte MCP-Call und Ergebnis dokumentiert mit Datums-Marker.
4. SKILL.md Recipe-Index aktualisiert (walls.md → wall-operations.md).
5. REQUIREMENTS.md WALL-01 Dateiname-Verweis aktualisiert.
6. TOC in allen 4 Recipes (alle voraussichtlich >100 Zeilen).
7. Voll-qualifizierte MCP-Toolnamen, one-level-deep References, deutsche erklärende Prosa (D-02 Ton aus Phase 1).
8. SAFE-01..05-Regeln in jedem Update-/Delete-Worked-Example sichtbar respektiert.

</success_signals>
