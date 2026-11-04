# Phase 3 Test-Set

Live-Setup vom 2026-05-19 für Phase-3-Recipes. Story 0 (EG, Level 0.0).

| Element-Typ | GUID | Quelle | Position / Eigenschaften |
|---|---|---|---|
| Slab (Decke) | `01b90e9f-a241-dc45-a448-5acc06a186c4` | per MCP (Wave 1 Task 2) | Polygon (10,0)→(12,0)→(12,2)→(10,2), Level 0 |
| Column (Stütze) | `41adb22a-a347-784a-bcba-ac6137ce76e3` | per MCP (Wave 1 Task 2) | (11, 3, 0) |
| Zone | `e0394527-a1fa-b24b-bab2-d75ffce4cc7e` | per MCP (Wave 1 Task 2) | „Test-Zone" T01, Polygon (14,0)→(18,0)→(18,4)→(14,4) |
| Wall | `f1101930-e0bd-7044-a1f2-fdb20e520e21` | per User (Wave 1 Task 3) | gerade Wand ca. (2, 0)→(6, 0), Structure: Composite |
| Window | `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` | per User (Wave 1 Task 3) | in Wall, mittig, Standard-Library |
| Beam | — | **NICHT ANGELEGT** | User hat keinen Beam gezeichnet. Recipe-Section in `slabs-columns-beams.md` mit `<!-- VERIFY -->`-Marker als „Beam-Operationen nicht live verifiziert in Phase 3, Schema aus Discovery dokumentiert" |

## Wave-1-Befunde

### Cleanup
- Delete-Aufruf für Phase-1-Test-Elemente (Zone `0a953674-…`, Polylinie `62f9ca34-…`) hatte einen `'result'`-Wrapper-Fehler. Non-blocking. Möglicherweise wurden die Elemente bereits durch User-Aktion oder Archicad-Restart entfernt.

### MCP-Wrapper-Bug: get_details_of_elements
Beim Versuch, `elements_get_details_of_elements` für die 5 existierenden Elemente aufzurufen, schlug der Call mit 400+ pydantic-Validierungsfehlern fehl. Archicad gibt in AC29 Felder wie `structureType`, `geometryType`, `arcAngle`, `slantAngle`, `verticalCurveHeight` zurück, die im MCP-Server-Schema als `extra='forbid'` markiert sind.

**Auswirkung auf Phase-3-Recipes:**
- Read-Worked-Examples NICHT via `elements_get_details_of_elements` schreiben.
- Stattdessen Workaround verwenden: `elements_get_types_of_elements`, `elements_get_classifications_of_elements`, `properties_get_property_values_of_elements`, `elements_get_connected_elements`.
- Im Recipe-Gotchas-Block den Bug + Workaround dokumentieren.
- Memory-Eintrag: `~/.claude/projects/-Users-ap/memory/issue_archicad_mcp_get_details_bug.md`

### Capabilities bestätigt
- `elements_create_slabs` ✅ live (Slab-GUID erhalten)
- `elements_create_columns` ✅ live (Column-GUID erhalten)
- `elements_create_zones` ✅ live (Zone-GUID erhalten)
- `elements_get_elements_by_type` ✅ live (mit ElementType-Enum)
- `elements_delete_elements` — Aufruf abgesetzt, Response-Format unklar (siehe Cleanup oben)
- `elements_get_details_of_elements` ⚠ unzuverlässig in AC29 (siehe Bug oben)

## Schema-Erkenntnisse für Wave 2

Pro Element-Typ haben wir folgende Quellen für die Subagents:
- **Slab:** Create-Schema aus Discovery (`elements_create_slabs.SlabsDatum`); Update-Schema aus `elements_set_details_of_elements.Details.typeSpecificDetails.SlabSettings` (analog WallSettings, Felder noch zu bestätigen).
- **Column:** Create-Schema (`elements_create_columns.ColumnsDatum`); Update via `SetDetails` mit ColumnSettings.
- **Beam:** Create NICHT verfügbar. Update via `SetDetails` mit BeamSettings (geometryType, level, slantAngle, verticalCurveHeight bekannt aus dem Pydantic-Fehler).
- **Wall:** Create NICHT verfügbar. Update via `SetDetails` mit WallSettings (begCoordinate, endCoordinate, height, bottomOffset, offset, begThickness, endThickness — aus Phase-2-Discovery bekannt) + structureType (neu aus AC29).
- **Window/Door:** Hosted-Elemente. Listen via `elements_get_connected_elements` mit `connectedElementType: Window`. Detail-Read via Property-Workaround.
- **Zone:** Create via `elements_create_zones.ZonesDatum` mit ManualZoneGeometry oder AutomaticZoneGeometry.

## Cleanup nach Phase 3

Die Test-Elemente bleiben im Projekt liegen. User kann sie per Hand löschen oder als Demo-Set behalten. Sammeln in einer eigenen Story / einem eigenen Layer ist denkbar als Phase-3-Folge-Aufräumarbeit.
