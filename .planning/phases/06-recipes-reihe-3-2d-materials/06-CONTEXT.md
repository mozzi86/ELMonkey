# Phase 6: Recipes Reihe 3 — 2D + Materials - Context (Schema-only mode)

**Gathered:** 2026-05-20
**Status:** Schema-only — Archicad MCP unreachable, kein Live-Test in dieser Session möglich

<domain>
## Phase Boundary

Phase 6 liefert drei Recipe-Dateien für 2D-Elemente und Materialien:
- `recipes/surfaces-materials.md` — Surfaces (Oberflächen), Building Materials, Composites + Zuweisung
- `recipes/fills-hatches.md` — 2D-Schraffuren (Hatch-Elemente)
- `recipes/lines-polylines.md` — 2D-Linien-Elemente (Line, PolyLine, Arc, Circle, Spline)

**Modus:** Schema-only. Recipes werden aus dem Phase-2-Wissen + ElementType-Enum + Phase-3-Patterns abgeleitet. Alle Worked Examples mit `<!-- VERIFY -->`-Markern, weil Live-Test gegen MCP nicht möglich war (Archicad-Instanz nicht erreichbar mitten in Phase-4-Wave-1).

**Phase-4 ist NICHT abgeschlossen** — Plans committed, aber Wave 1+2+3 ausstehend. Wird in einer Folgesession nachgeholt sobald Archicad wieder verfügbar.
</domain>

<decisions>
## Implementation Decisions

### D-27: Schema-only Modus für Phase 6
- Recipes werden aus existierendem Wissen geschrieben: Phase-2-Discovery-Befunde (`attributes_get_attributes_by_type` mit Surface/BuildingMaterial/Composite/Fill/Line; `elements_create_polylines` verifiziert; `attributes_create_building_materials` + `attributes_create_composites` verifiziert), ElementType-Enum (Hatch, Line, PolyLine, Arc, Circle, Spline).
- Worked Examples mit `<!-- VERIFY -->`-Markern überall, weil keine Live-Aufrufe möglich.
- Datums-Marker `<!-- 2026-05-20 schema-only -->` als Disambiguierung zum „verifiziert"-Marker.

### D-28: Lines-Polylines combined
- `lines-polylines.md` behandelt alle 5 2D-Linien-Element-Typen: Line, PolyLine, Arc, Circle, Spline.
- Polylines ist die einzige verifiziert-erstellbare (Phase 2). Für andere: Create-Status `<!-- VERIFY -->`.
- Patterns: PolyArc + Coordinate2D + Arcs-Array (aus Phase 2 SchemaSammlung).

### D-29: Surfaces-Materials Recipe-Fokus = Zuweisung
- Building-Materials + Composites Create: verifiziert (Phase 2 Discovery).
- Surface-Erstellung: Status unklar, vermutlich via separate API. `<!-- VERIFY -->`.
- Hauptfokus des Recipes: **Zuweisung** existierender Materialien an Elemente (Wand-Composite-Zuweisung, Object-Surface-Override, etc.), nicht Create.

### D-30: Fills-Hatches Recipe — minimaler Scope
- Hatch ist im ElementType-Enum (Phase 2 known), aber kein `elements_create_hatches` in unserer Discovery aufgetaucht.
- Recipe-Fokus: Hatch-Read (`elements_get_elements_by_type` mit `Hatch`), Hatch-Update via `set_details_of_elements`, Hatch-Delete.
- Create-Status: vermutlich nicht via MCP (analog zu Wall) — `<!-- VERIFY -->`.
- Fill-Attribute (das Fill-Pattern selbst) sind separat via `attributes_get_attributes_by_type` mit `Fill` listbar.

### D-31: Live-Validation aller 3 Recipes deferred
- In Folgesession sobald Archicad verfügbar: Sample-Read pro Recipe + Update-Tests + ggf. Polyline-Create (verifiziert) als Smoke-Test.
- Phase 6 gilt erst dann als „live-verified abgeschlossen". Bis dahin: Schema-only-Skill v0.6.
- Capability-Tests CAP-01..04 aus Phase 3 bleiben offen und werden in derselben Folge-Session abgearbeitet.

### D-32: Subagent-Architektur leicht
- **3 parallele Subagents** schreiben je 1 Recipe.
- **Kein Wave 0** (kein Live-Schema-Sammeln möglich) — Subagents arbeiten aus Skill-Foundation-Files + Phase-3-Patterns + ihrem eigenen Phase-2-Wissen via Read der existierenden Schemas.
- **Kein Wave 3 Live-Validation** — nur Konsistenz-Checks (TOC, voll-qualifizierte Toolnamen, etc.).

</decisions>

<canonical_refs>
## Canonical References

### Skill Foundation
- `SKILL.md` — Hub
- `reference/mcp-conventions.md` — Capability-Tabelle (was kreatabel, was nicht) + Modal-Dialog + AC29-Bugs
- `reference/bulk-operations.md` — Pre-Flight + Identifier-Mapping
- `reference/schedule-pipeline.md` + `reference/property-expression-linking.md` — Workarounds
- `reference/self-improvement.md` — VERIFY-Marker-Konvention

### Phase 2 Findings (Schema-Quelle)
- `reference/workflow-context.md` Feld 6 (Pen-Set), 7 (Klassifikation) — relevant für 2D-Recipes
- Memory `project_archicad_mcp_capabilities.md` — definitive Create-Surface-Liste

### Phase 3 Pattern-Vorlagen
- `recipes/wall-operations.md` — Stil-Referenz für No-Create-Recipes
- `recipes/zones.md` — Property-Bulk-Pattern (6-Schritt) als Vorlage falls relevant
- `recipes/slabs-columns-beams.md` — Stil für multi-Element-Typ-Recipe

### Phase 4 (Schwester-Phase, deferred)
- `.planning/phases/04-recipes-reihe-2-komplexere-elemente/04-CONTEXT.md` — analoge Decisions

</canonical_refs>

<deferred>
## Noted for Later

- **Live-Verifikation aller Phase-6-Worked-Examples** — eigene Folgesession sobald Archicad da.
- **Hatch-Create-Discovery** — vielleicht gibt es einen Endpoint, den wir nicht gefunden haben.
- **Surface-Create-Discovery** — separater API-Pfad möglich.
- **Material-Override pro Element-Face** — komplexer als Composite-Zuweisung, falls MCP das unterstützt. v1.x.

</deferred>

<success_signals>
1. 3 Recipe-Dateien existieren mit TOC.
2. Alle Worked Examples haben `<!-- VERIFY -->`-Marker.
3. Verifizierte Create-Tools (Polylines, BuildingMaterials, Composites) sind klar markiert als „live verifiziert in Phase 2".
4. Capability-Gaps explizit kommuniziert (Hatch-Create, Surface-Create, Line-Create — alle unklar/wahrscheinlich nicht via MCP).
5. Konsistenz-Checks PASS (TOC, voll-qualifizierte Toolnamen, one-level-deep References).
6. SKILL.md Recipe-Index Hinweis-Texte präzisiert.

</success_signals>
