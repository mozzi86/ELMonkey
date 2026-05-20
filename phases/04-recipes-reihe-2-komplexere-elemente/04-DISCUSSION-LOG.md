# Phase 4 — Discussion Log

**Date:** 2026-05-20
**Mode:** discuss (default), 4 batched questions.

## Areas Discussed

### Area 1 — Test-Port
**Selected:** Futurelab Teamwork (19724) — Real-Read-Only
**Captured as D-21.** Read-Only-Approach mitigates risk; bietet realistischere Worked Examples als Sandkasten-Test-Set.

### Area 2 — CurtainWall-Sub-Element-Tiefe
**Selected:** Voll ausgearbeitet (alle 5 Sub-Element-Typen + Top-Level)
**Captured as D-22.** Recipe wird ~600-700 Zeilen, aber Fassaden-Workflow ist zentral und der Mehraufwand zahlt sich aus.

### Area 3 — Library-Object Subtypes
**Selected:** Aus Teamwork existierende lesen
**Captured as D-23.** Subtype-Discovery + Gruppierung als Kern-Pattern. Kein Create-Worked-Example in Phase 4.

### Area 4 — Property-Bulk-Pattern in library-objects.md
**Selected:** Ja, dasselbe 6-Schritt-Pattern (Recommended)
**Captured as D-24.** Konsistenz zu zones.md; Möbel-Inventar-Property-Bulk-Updates sind realistischer Use-Case.

## Deferred Ideas
Keine neuen — bestehende Backlog-Items unangetastet:
- Object-Create im Sandkasten (Phase 5 oder spätere Session)
- CurtainWall-Create-Workaround (v1.x)
- Hotlink/XREF (v2)

## Claude's Discretion
- Genaue Sub-Element-Reihenfolge in curtain-walls.md (vermutlich Top-Level → Segment → Frame → Panel → Junction → Accessory).
- Subtype-Gruppierungs-Schema in library-objects.md (z. B. nach Library-Pfad-Top-Level oder nach erkennbarem Functional-Type).
- Exakte Beispiel-Property für library-objects.md Bulk-Worked-Example (z. B. „Hersteller" oder „Modell-Nummer" oder eine Schedule-relevante Property).

## Notes
- Phase 4 mit nur 2 Recipes ist kleiner als Phase 3 (4 Recipes), aber curtain-walls.md wird das längste Recipe bisher (~600-700 Zeilen vs. Phase-3-Max von 596 in slabs-columns-beams.md).
- Wave-0-Setup wird leichter als Phase 3: keine Cleanup nötig, kein Sandkasten-Create. Hauptaufwand: Discovery + Schema-Sammlung für 6 CurtainWall-Sub-Typen + Library-Object-Listing aus Real-Projekt.
- Teamwork-Port-Wahl heißt: jeder MCP-Call hat `"port": 19724` (nicht 19723 wie in Phase 3). Subagents müssen darauf hingewiesen werden.
