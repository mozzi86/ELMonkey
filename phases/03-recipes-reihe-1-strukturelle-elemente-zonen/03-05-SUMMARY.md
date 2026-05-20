---
plan: 03-05
status: complete
completed: 2026-05-19
agent: general-purpose (Subagent)
---

# 03-05-SUMMARY — zones.md

**Output:** `recipes/zones.md`, **455 Zeilen**.

## Was wurde getan
- TOC mit 14 Sektionen.
- **4 + 1 Worked Examples:**
  - Zone manuell erstellen (Polygon, Wave-1 reproduziert)
  - Bonus: Zone automatisch erstellen (Reference-Point + AutomaticZoneGeometry, `<!-- VERIFY -->`)
  - Zonen einer Story lesen
  - Zone-Properties modifizieren (Name + Number, Confirm-Dialog)
  - Zone löschen
  - Zone klassifizieren
- **Sektion „Zone-Membership für Bulk-Klassifizierung"** via `elements_get_elements_related_to_zones` — Hinweis Richtung Phase 5.
- Test-Zone-GUID `e0394527-…` durchgängig.
- 12 voll-qualifizierte MCP-Toolnamen.
- 7 Gotchas: `numberStr` als String, AutomaticZoneGeometry-Voraussetzung, Polygon-Selbstüberschneidung, ZoneCategory-GUID, Stempel-Position vs. Sichtbarkeit, get_details-Wrapper-Bug, Fläche via Property-System.
- Bulk-Klassifizierungs-Stub.

## Live-Validation (Wave 3 Sample)
- Read-Beispiel reproduziert: `elements_get_elements_by_type` mit `Zone` → 1 Zone, GUID matcht Test-Set. ✅
