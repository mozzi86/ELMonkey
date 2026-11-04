---
plan: 04-02
status: complete
completed: 2026-05-20
agent: general-purpose (Subagent)
---

# 04-02-SUMMARY — curtain-walls.md

**Output:** `recipes/curtain-walls.md`, **768 Zeilen** (etwas über Plan-Soll 500-700 — gerechtfertigt durch 6 Element-Typen + 8 Worked Examples).

## Was wurde getan

- TOC mit allen Sektionen.
- **ASCII-Hierarchie-Diagramm** prominent.
- 8 Worked Examples:
  1. CurtainWall (Top-Level) lesen — Property-Workaround
  2. **Sub-Elemente finden** — `get_subelements_of_hierarchical_elements` mit Live-Resultat 1+31+12 als konkretem Beispiel; NICHT-`connected_elements`-Warnung doppelt
  3. CurtainWallPanel modifizieren — Building-Material-Zuweisung + Bulk-Pattern
  4. CurtainWallFrame modifizieren — Profil-Zuweisung
  5. CurtainWallJunction Operations (Existenz-Check für Default-CW)
  6. CurtainWallSegment + Accessory (kombiniert)
  7. CurtainWall klassifizieren — mit explizitem „propagiert NICHT auf Subs"-Hinweis
  8. CurtainWall löschen — SAFE-04 mit Live-Counts (1+31+12=44) im Pre-Check
- **12 Gotchas** inkl. G-12 Modal-Dialog Code-4001-Beispiel live verifiziert.
- Bulk-Klassifizierungs-Stub mit Sub-Element-Pass-Hinweis.
- 18 voll-qualifizierte `mcp__archicad__`-Toolnamen.
- 11 `<!-- VERIFY -->`-Marker für ungetestete Update-Schemas.

## Live-Validation (Wave 3 Sample)
- `elements_get_subelements_of_hierarchical_elements` mit Top-Level-CW-GUID → 1+31+12 Sub-Elements, matcht TEST-SET. ✅

## Decisions umgesetzt
- D-22 (voll ausgearbeitet): 6 Element-Typen + Hierarchie-Diagramm ✓
- D-25 (SAFE-Regeln): SAFE-04 Pre-Check + SAFE-05 ID-Threading prominent ✓
