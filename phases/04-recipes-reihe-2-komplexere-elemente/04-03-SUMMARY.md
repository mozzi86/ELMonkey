---
plan: 04-03
status: complete
completed: 2026-05-20
agent: general-purpose (Subagent) + Orchestrator-Fix (Wave 3)
---

# 04-03-SUMMARY — library-objects.md

**Output:** `recipes/library-objects.md`, **683 Zeilen** (über Plan-Soll 400-550 — gerechtfertigt durch 8 Worked Examples).

## Was wurde getan

- TOC vorhanden.
- 8 Worked Examples:
  1. **Libraries auflisten** — `library_get_libraries` (neues live-verifiziertes Tool)
  2. **Subtype-Discovery + Gruppierung** als Kern-Workflow
  3. Single-Object lesen + Eigenschaften via Property-Workaround
  4. GDL-Parameter abfragen mit Bug-Caveat + Workaround-Verweisen (schedule-pipeline, property-expression-linking)
  5. Object verschieben/Größe ändern
  6. Object löschen
  7. **Property-Bulk-Update Inventar (6-Schritt-Pattern analog zu zones.md)** — D-24 erfüllt
  8. Object klassifizieren
- Library-Type-Tabelle: BuiltInLibrary / EmbeddedLibrary / LocalLibrary / ServerLibrary
- Abgrenzung zu openings.md (Türen/Fenster) + curtain-walls.md (Accessories) in Intro UND Gotchas
- 9 Gotchas inkl. get_details-Bug + gdl_parameters-Bug + Modal-Dialog + Türen/Fenster-Abgrenzung
- 19 voll-qualifizierte MCP-Toolnamen
- Bulk-Klassifizierungs-Stub

## Wave-3-Fix
2 unqualifizierte `archicad_call_tool` (Tabellenzelle Warm-up-Kontext) durch Orchestrator gefixed → jetzt alle qualifiziert.

## Live-Validation (Wave 3 Sample)
- `library_get_libraries` → 19 Libraries inkl. BIMcloud-Server „BIBLIOTHEKEN 28". Matcht TEST-SET. ✅

## Decisions umgesetzt
- D-23 (Subtype-Discovery + Real-Read aus Teamwork — adaptiert auf Sandkasten mit 4 Test-Objects)
- D-24 (Property-Bulk-Pattern wie zones.md) ✓
- D-25 (SAFE-Regeln) ✓
