---
plan: 03-03
status: complete
completed: 2026-05-19
agent: general-purpose (Subagent)
---

# 03-03-SUMMARY — openings.md

**Output:** `recipes/openings.md`, **443 Zeilen**.

## Was wurde getan
- TOC mit 11 Sektionen.
- 4 Worked Examples: Read (3+1 Schritte mit SAFE-05 ID-Threading Wand → Fenster) / Sill-Höhe via `elements_set_gdl_parameters_of_elements` / Delete mit SAFE-01 Confirm / Classify im SAB_Klassifizierung_29-System.
- KEIN Create-Section.
- SAFE-05 prominent: Wand-GUID `f1101930-…` als Einstieg, Fenster-GUID `7185f21a-…` durchgereicht.
- 16 voll-qualifizierte MCP-Toolnamen.
- 11 SAFE-Referenzen (01, 04, 05).
- 7 Gotchas inkl. Wand-Delete-Mit-Lösch-Verhalten + Sill-relativ-zur-Wand + GDL-Index-Library-Spezifität + get_details-Bug.
- Bulk-Klassifizierungs-Stub.

## Live-Validation (Wave 3 Sample)
- Read-Beispiel reproduziert: `elements_get_connected_elements` (Wall → Window) → 1 Window, GUID matcht Test-Set. ✅
