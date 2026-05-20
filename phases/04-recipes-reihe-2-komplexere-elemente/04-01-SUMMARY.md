---
plan: 04-01
status: complete
completed: 2026-05-20
---

# 04-01-SUMMARY — Wave 1 Orchestrator Setup (Live-Discovery)

## Was wurde getan

Live-Discovery + Listing gegen Sandkasten-Projekt „Ohne Titel" (Port 19723) — der sich als Phase-3-Sandkasten mit gesicherten Phase-3-Test-Elementen herausstellte.

### Live-Befunde

1. **Element-Listing alle 7 Phase-4-Element-Typen:**
   - CurtainWall: 1 (`5d5ad6b2-…`)
   - CurtainWallSegment: 1
   - CurtainWallFrame: 31
   - CurtainWallPanel: 12
   - CurtainWallJunction: 0 (in Default-CW nicht generiert)
   - CurtainWallAccessory: 0
   - Object: 4 GUIDs

2. **Schlüssel-Tool entdeckt:** `elements_get_subelements_of_hierarchical_elements` — liefert in einem Aufruf die vollständige CW-Sub-Element-Map gruppiert nach Typ (cWallSegments, cWallFrames, cWallPanels). Korrekter Pfad für hierarchische Element-Typen.

3. **Negative-Finding:** `elements_get_connected_elements` mit `connectedElementType: "CurtainWallFrame"` / `"CurtainWallPanel"` → **leere Liste**. Funktioniert nur für Host-Hosted-Beziehungen (Wall→Window), nicht für Container-Sub-Element-Hierarchien.

4. **`library_get_libraries`** verifiziert — 19 Libraries inkl. BIMcloud-Server „BIBLIOTHEKEN 28" (https://schwarz.bimcloud.com).

5. **Object hat ~230 Built-in Properties** pro Element — riesiges Set für GDL-Library-Items. Klassifikations-System-Refs (SAB_Klassifizierung + SAB_Klassifizierung_29) erscheinen darin.

6. **Modal-Dialog-Bug live verifiziert** mit Code 4001 und Dialog-Name „Attribute-Manager" — exakt das Pattern aus `reference/mcp-conventions.md`. Schema-zu-Live-Validation-Loop geschlossen.

## Artefakte

- `SCHEMAS.md` (Wave-1-Schema-Sammlung mit allen verifizierten Tools + Workarounds + Bug-Hinweisen)
- `TEST-SET.md` (GUID-Map: 44 CW-Sub-Elemente + 4 Objects + 7 Phase-3-Reste)

## Wave-2-Subagents-Ready
Subagents haben alles für die Recipe-Erstellung — Tool-Namen + Schemas + GUIDs.
