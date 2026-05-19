# Archicad-Skill für Claude Code

## What This Is

Ein Claude-Code-Skill, der Claude befähigt, mit dem Archicad-MCP-Server (`mcp__archicad__*`) umfassend zu arbeiten: alle gängigen Architektur- und 2D-Modellierungsoperationen, in allen CRUD-Variationen, mit kontextbewusster Discovery, asymmetrischer Sicherheit und Bulk-Klassifizierungs-Workflows. Zielgruppe: ein einzelner Architekt/Planer (der User dieses Setups), der täglich in Archicad arbeitet und Claude als Assistent für wiederkehrende Modellier-, Klassifizierungs- und Bestands-Aufgaben einsetzt.

## Core Value

**Claude kann jederzeit in einem laufenden Archicad-Projekt arbeiten — neue Elemente erstellen, Bestand abfragen, Bulk-Operationen ausführen — ohne dass Tool-Namen oder Parameter zur Laufzeit erfunden werden müssen.** Discovery-zentriert, asymmetrisch sicher, selbst-verbessernd.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

(None yet — ship to validate)

### Active

<!-- Current scope. Building toward these. -->

- [ ] **FOUND-01**: Universelle Foundation in `SKILL.md` (Sicherheitsregeln, Warm-up, Discovery-Pattern, Verweise).
- [ ] **FOUND-02**: `reference/mcp-conventions.md` mit Discovery-Strategien, Fehlerklassen, Paginierungs-Handling.
- [ ] **FOUND-03**: `reference/workflow-context.md` mit live-verifizierten MCP-Tool-Namen für alle 7 Warm-up-Felder.
- [ ] **FOUND-04**: `reference/bulk-operations.md` mit Read → Filter → Group → Confirm → Apply-Muster + Klassifikations-Spezifika.
- [ ] **FOUND-05**: `reference/self-improvement.md` mit Reflection-Trigger, Lern-Klassen, Format-Konventionen.
- [ ] **WALL-01**: Recipe `walls.md` für gerade/geschwungene/polygonale Wände, CRUD vollständig, Worked Example live verifiziert.
- [ ] **OPEN-01**: Recipe `openings.md` für Fenster/Türen/Wandöffnungen, CRUD vollständig.
- [ ] **STRU-01**: Recipe `slabs-columns-beams.md` für Decken/Stützen/Träger, CRUD vollständig.
- [ ] **CURT-01**: Recipe `curtain-walls.md` für Fassaden/Pfosten-Riegel-Konstruktionen, CRUD vollständig.
- [ ] **LIB-01**: Recipe `library-objects.md` für platzierte Objects (Möbel/Sanitär/GDL-Library-Items), CRUD vollständig.
- [ ] **SURF-01**: Recipe `surfaces-materials.md` für Surfaces/Building Materials/Composites — CRUD soweit MCP zulässt (Create unsicher, Update/Zuweisung sicher).
- [ ] **FILL-01**: Recipe `fills-hatches.md` für 2D-Schraffuren, CRUD vollständig.
- [ ] **LINE-01**: Recipe `lines-polylines.md` für 2D-Linien/Polylinien/Bögen/Splines, CRUD vollständig.
- [ ] **CLASS-01**: Bulk-Klassifizierungs-Workflows pro Recipe (Wand→Innen/Außen, Öffnung→Tür/Fenster, Stütze→tragend/nicht-tragend, Object→Kategorie).
- [ ] **INT-01**: Cross-Recipe-Integrationstest „Wand + Fenster + Material + Hilfslinie" am laufenden Archicad.
- [ ] **INT-02**: Cross-Recipe-Integrationstest „komplette Projekt-Klassifizierung" am laufenden Archicad.
- [ ] **SAFE-01**: Asymmetrisches Sicherheits-Modell durchgängig umgesetzt (Create/Read frei; Update/Delete confirm; keine Batch-Obergrenze).

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- IFC-Import/-Export — v2-Thema; separater Funktionsbereich.
- Layout-Buch / Pläne / Plot — Output-Workflow, nicht Modellierung.
- Erstellung neuer Views (Schnitte, Ansichten, 3D-Dokumente) — Sichten ≠ Elemente.
- Bemaßungen + Beschriftungs-Stile — eigener Komplex, später.
- GDL-Code schreiben — separate Domäne (Library-Entwicklung statt -Nutzung).
- BIMx, Renderings, Visualisierung — Output, nicht Bearbeitung.
- Mehrere Archicad-Instanzen parallel orchestrieren — Komplexität nicht gerechtfertigt.
- Teamwork / BIMcloud-Sync-Operationen — kollaborativer Workflow, eigene Sicherheitsmodell-Frage.

## Context

**Technisches Umfeld:**
- Archicad 29 läuft lokal, MCP-Plugin aktiv (Port 19723 zum Zeitpunkt der Skill-Erstellung).
- MCP-Server ist Discovery-zentriert: `discovery_list_active_archicads` → `archicad_discover_tools` → `archicad_call_tool`. Tool-Namen sind nicht statisch dokumentiert, müssen pro Operation entdeckt werden.
- Skill-Inhalte deutsch (User-Lesbarkeit), Skill-Description englisch (Matching-Konsistenz), MCP-Queries englisch (Server-Index).

**Prior Work:**
- Vollständige Design-Spec liegt vor unter `/Users/ap/docs/superpowers/specs/2026-05-19-archicad-skill-design.md` — Brainstorming-Resultat, vom User explizit freigegeben.
- Skill-Verzeichnis `~/.claude/skills/archicad/` ist initialisiert (git, .planning/).
- User-Memory enthält: keine Batch-Obergrenze; Bulk-Klassifizierung ist Kern-Workflow.

**Known Issues:**
- **Archicad MCP v29 hat eine limitierte Create-Surface** (Live-Befund 2026-05-19). Erstellbar: Slabs, Columns, Objects, Polylines, Meshes, Zones, Building Materials, Composites, Property Groups. **NICHT** erstellbar: Walls, Beams, Windows, Doors, Curtain Walls, Fills, Standalone Lines, Stairs, Railings, Morphs, Shells, Skylights, Roofs. Modifikation/Read/Delete existierender Elemente bleibt für die meisten Typen verfügbar via `elements_set_details_of_elements`. Spec-Auswirkung: Recipes für Walls/Openings/Curtain-Walls/Fills/Beams werden Read+Update+Delete+Classify (kein Create-Section). Siehe `reference/mcp-conventions.md` § Capabilities-Tabelle.
- Geometrische „Innen vs Außen"-Ableitung für Klassifizierung ist nicht trivial — kombiniert Layer-Heuristik + Raumzugehörigkeit + Geometrie.
- `gsd-sdk` in PATH ist v1.50.0-canary, GSD-Installer ist v1.42.3 — Versions-Mismatch ist bekannt aber nicht blockierend.

## Constraints

- **Tech Stack**: Markdown-Skill-Dateien für Claude Code, kein eigener Code. Hub-and-Spoke-Architektur (1 SKILL.md + reference/* + recipes/*).
- **MCP-Abhängigkeit**: Skill ist nur funktional, wenn der Archicad-MCP-Server (`mcp__archicad__*`) konfiguriert und Archicad mit MCP-Plugin geöffnet ist.
- **Discovery-First**: Konkrete MCP-Tool-Namen werden nicht hartkodiert, sondern als „typischer Name (Hinweis)" dokumentiert; Validierung via Discovery zur Laufzeit. Begründung: Robustheit gegen Archicad-Versionswechsel.
- **Live-Verifikation Pflicht**: Jede Recipe-Datei wird gegen das laufende Archicad geprüft, bevor sie als „fertig" gilt. Worked Example dient als Smoke-Test.
- **Sicherheit asymmetrisch**: Create + Read frei; Update + Delete mit User-Confirm + Element-ID-Aufzählung. Keine Batch-Obergrenze (User-Präferenz).
- **Self-Improvement on**: Skill wächst pro Session durch Reflection-Trigger; Einträge mit Datums-Marker, Verifikation-Loop bei Wiederholfehlern.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Hub-and-Spoke + Discovery-verifiziert (Ansatz 3) | Robust gegen MCP-Versionswechsel; Token-effizient (nur relevante Recipes geladen); Recipes als Leitplanken statt Vertrag | — Pending |
| Sprache: Beschreibung Englisch, Inhalte Deutsch | User editiert auf Deutsch; Claude-Matching ist mit englischen Triggern konsistenter | — Pending |
| Asymmetrische Sicherheit (Update/Delete mit Confirm) | Create/Read ist halluzinationssicher dank Undo; Update/Delete kann Bestand zerstören | — Pending |
| Keine Batch-Obergrenze | User arbeitet regelmäßig an Massenoperationen; ein Cap würde echte Architektur-Workflows blockieren | — Pending |
| Self-Improvement-Pattern eingebaut | Statische Doku veraltet; Skill soll mit Erfahrung wachsen | — Pending |
| GSD-Workflow (B1 voll) als Implementierungs-Treiber | 7 natürliche Phasen mit UAT zwischen den Recipe-Reihen; Phasen-Granularität passt zu GSD | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-19 after initialization (synthesized from design spec)*
