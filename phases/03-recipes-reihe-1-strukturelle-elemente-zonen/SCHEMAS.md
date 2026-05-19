# Phase 3 SCHEMAS

Konsolidierte MCP-Schemata pro Element-Typ, gesammelt am 2026-05-19 gegen Archicad 29 / Port 19723. Input für die 4 Wave-2-Subagents.

## Inhaltsverzeichnis

1. [Universal-Pattern: Delete + Type + Classification](#universal-pattern-delete--type--classification)
2. [Wall](#wall)
3. [Window / Door / Opening (hosted)](#window--door--opening-hosted)
4. [Slab](#slab)
5. [Column](#column)
6. [Beam](#beam)
7. [Zone](#zone)
8. [Wichtige Wrapper-Bug-Workarounds](#wichtige-wrapper-bug-workarounds)

---

## Universal-Pattern: Delete + Type + Classification

Diese drei Tools funktionieren elementtyp-übergreifend identisch:

### Delete (für jeden Typ)

```
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [
        {"elementId": {"guid": "<guid>"}},
        ...
      ]
    }
  }
)
```

### Element-Typ ermitteln

```
mcp__archicad__archicad_call_tool(
  name="elements_get_types_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<guid>"}}]
    }
  }
)
```

Response: `{typesOfElements: [{type: "Wall" | "Slab" | ...}]}`. **Funktioniert auch wo `get_details_of_elements` scheitert.**

### Klassifikationen lesen

```
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<guid>"}}],
      "classificationSystemIds": [{"classificationSystemId": {"guid": "<system-guid>"}}]
    }
  }
)
```

### Alle Elemente eines Typs listen

```
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": <port>,
    "params": {
      "elementType": "Wall",
      "filters": ["OnActualFloor", "IsVisibleByLayer"]
    }
  }
)
```

ElementType-Enum (Auszug für Phase 3): `Wall`, `Window`, `Door`, `Slab`, `Column`, `Beam`, `Zone`, `Opening`.
ElementFilter-Enum: `IsEditable`, `IsVisibleByLayer`, `IsVisibleByRenovation`, `IsVisibleByStructureDisplay`, `IsVisibleIn3D`, `OnActualFloor`, `OnActualLayout`, `InMyWorkspace`, `IsIndependent`, `InCroppedView`, `HasAccessRight`, `IsOverriddenByRenovation`. Paginiert via `next_page_token`.

---

## Wall

### Create

**NICHT VERFÜGBAR in MCP v29.** Siehe `reference/mcp-conventions.md` § Capability-Tabelle. User zeichnet manuell.

### Read

⚠ **`elements_get_details_of_elements` schlägt in AC29 fehl** (Wrapper-Bug, siehe Sektion 8). Workaround:

1. Element via `elements_get_elements_by_type` mit `elementType: "Wall"` listen.
2. Typ bestätigen via `elements_get_types_of_elements`.
3. Eigenschaften über Property-System lesen: `properties_get_all_property_ids_of_elements` + `properties_get_property_values_of_elements` (Property-IDs typischer Wand-Eigenschaften: Height, Thickness, Layer, Composite, BuildingMaterial).

### Update

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "<wall-guid>"},
        "details": {
          "floorIndex": 0,
          "layerIndex": <number>,
          "drawIndex": <number>,
          "typeSpecificDetails": {
            "begCoordinate": {"x": <m>, "y": <m>},
            "endCoordinate": {"x": <m>, "y": <m>},
            "height": <m>,
            "bottomOffset": <m>,
            "offset": <m>,
            "begThickness": <m>,
            "endThickness": <m>
          }
        }
      }]
    }
  }
)
```

WallSettings-Felder (alle optional): `begCoordinate`, `endCoordinate` (Coord2D), `height`, `bottomOffset`, `offset` (Wand-Basis-Linie relativ zur Reference Line), `begThickness`, `endThickness` (für Trapez-Wände). **Neu in AC29 (aus Pydantic-Fehler entdeckt):** `structureType` (Werte: „Composite" und vermutlich andere), `geometryType` (Werte: „Straight" und vermutlich „Trapezoidal", „Polygonal", „Curved").

### Delete + Hosted-Element-Pre-Check (SAFE-04)

Vor Wand-Löschung **immer** zuerst `elements_get_connected_elements` aufrufen:

```
mcp__archicad__archicad_call_tool(
  name="elements_get_connected_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<wall-guid>"}}],
      "connectedElementType": "Window"
    }
  }
)
```

(Optional separater Call für `Door` und `Opening`.) Dann Confirm-Dialog mit gehosteten Elementen auflisten. Erst nach `ja`: `elements_delete_elements`.

---

## Window / Door / Opening (hosted)

### Create

**NICHT VERFÜGBAR in MCP v29.** User zeichnet manuell in eine existierende Wand.

### Read

- Listen via `elements_get_elements_by_type` mit `elementType: "Window"` / `"Door"` / `"Opening"`.
- Host-Wand finden via `elements_get_connected_elements` (umgekehrt zur Wand-Pre-Check-Richtung).
- Eigenschaften via Property-System (siehe Wall Read Workaround).

Bekannte Properties: Sill-Höhe, Width, Height, Wall-Host-GUID.

### Update

Über `elements_set_details_of_elements` mit `typeSpecificDetails` — exakte Schema-Felder noch nicht aus Discovery isoliert (Subagent kann nach „update window" / „set window details" suchen falls nötig, oder Property-basiert via `properties_set_property_values_of_elements` arbeiten).

GDL-Parameter (Sill-Höhe, Schwenk-Richtung, Library-Variante): via `elements_set_gdl_parameters_of_elements`:

```
mcp__archicad__archicad_call_tool(
  name="elements_set_gdl_parameters_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithGDLParameters": [{
        "elementId": {"guid": "<window-guid>"},
        "gdlParameters": [
          {
            "index": "<param-index-string>",
            "type": "<param-type>",
            "value": <value>
          }
        ]
      }]
    }
  }
)
```

GDL-Parameter-Namen + Indices via `elements_get_gdl_parameters_of_elements` ermitteln (kein extra Discovery nötig).

### Delete

Standard `elements_delete_elements`. Keine eigenen gehosteten Elemente (außer evtl. Sub-Library-Items in komplexen Türen — selten).

---

## Slab

### Create ✅ verifiziert

```
mcp__archicad__archicad_call_tool(
  name="elements_create_slabs",
  arguments={
    "port": <port>,
    "params": {
      "slabsData": [{
        "level": <z-coord>,
        "polygonCoordinates": [{"x": <m>, "y": <m>}, ...],   // min 3 Punkte
        "polygonArcs": [...],                                  // optional, für gekrümmte Kanten
        "holes": [{"polygonOutline": [...], "polygonArcs": [...]}]  // optional, Aussparungen
      }]
    }
  }
)
```

Response: `{elements: [{elementId: {guid: "<new-guid>"}}]}`. **Datums-Marker:** `<!-- 2026-05-19 verifiziert AC29 -->`

### Read / Update

Wie bei Wall — Read via Property-Workaround, Update via `SetDetails` mit SlabSettings (Felder aus Pydantic-Fehler bekannt: `thickness`, `level`, `offsetFromTop`, `polygonOutline`, `holes` + `structureType`, `geometryType` neu in AC29).

### Delete

Standard. Wenn Wände auf der Slab hosten: keine automatische Mit-Löschung wie bei Wall-Window — Slabs sind Container, keine Hosts.

---

## Column

### Create ✅ verifiziert

```
mcp__archicad__archicad_call_tool(
  name="elements_create_columns",
  arguments={
    "port": <port>,
    "params": {
      "columnsData": [{
        "coordinates": {"x": <m>, "y": <m>, "z": <m>}
      }]
    }
  }
)
```

Response wie bei Slab. **Datums-Marker:** `<!-- 2026-05-19 verifiziert AC29 -->`

### Update

Via `SetDetails` mit ColumnSettings (Höhe, Profil-Attribut-GUID, etc.). Felder noch nicht im Detail isoliert — Subagent kann via `elements_get_details_of_elements` Schema-Read versuchen (Bug-betroffen) oder Update direkt mit bekannten Feldern probieren.

### Delete / Classify

Standard.

---

## Beam

### Create

**NICHT VERFÜGBAR in MCP v29.** User zeichnet manuell.

⚠ **In Phase-3-Test-Set kein Beam vorhanden** — Recipe-Section markiert mit `<!-- VERIFY -->` für Phase 5 oder 8 Folge-Verifikation.

### Read / Update / Delete

BeamDetails-Felder aus Pydantic-Fehler bekannt: `level`, `slantAngle`, `verticalCurveHeight`, `geometryType` (z. B. „Straight"), `structureType` (z. B. „Composite") + die bekannten Wall-ähnlichen (`begCoordinate`, `endCoordinate`, `height`, `bottomOffset`, `offset`).

Update via `elements_set_details_of_elements` mit `typeSpecificDetails` analog zu WallSettings.

---

## Zone

### Create ✅ verifiziert (Phase 1 + Wave 1)

```
mcp__archicad__archicad_call_tool(
  name="elements_create_zones",
  arguments={
    "port": <port>,
    "params": {
      "zonesData": [{
        "floorIndex": 0,                                    // optional
        "name": "Test-Zone",                                // Pflicht
        "numberStr": "T01",                                 // Pflicht (String!)
        "categoryAttributeId": {"guid": "<cat-guid>"},      // optional
        "stampPosition": {"x": <m>, "y": <m>},              // optional
        "geometry": {
          "polygonCoordinates": [{"x": <m>, "y": <m>}, ...],
          "polygonArcs": [],
          "holes": []
        }
      }]
    }
  }
)
```

ManualZoneGeometry (siehe oben) oder AutomaticZoneGeometry mit nur `referencePosition: {x, y}` — Archicad findet das Polygon aus umschließenden Wänden.

### Read / Update / Delete

Zone-Detail-Schema noch unbekannt aus Pydantic-Fehler (Zone wurde als MeshDetails / NotYetSupportedElementTypeDetails fehlklassifiziert — vermutlich noch nicht im pydantic-Modell). Workaround: Property-basiert lesen. Update via SetDetails versuchen, ggf. via `categoryAttributeId`-Reassignment.

### Verwandte Operationen

- `elements_get_elements_related_to_zones` — listet Wände/Stützen/etc., die zu einer Zone gehören. Nutzbar für Innen/Außen-Wand-Klassifizierung in Phase 5.

---

## Wichtige Wrapper-Bug-Workarounds

### Bug: `elements_get_details_of_elements` schlägt in AC29 fehl

Pydantic-Validierung blockiert die Response wegen `extra='forbid'` auf neuen AC29-Feldern (`structureType`, `geometryType`, `arcAngle`, `slantAngle`, `verticalCurveHeight`).

**Workarounds für Read-Operationen:**

1. **Property-System:**
   ```
   # Schritt 1: Property-IDs
   mcp__archicad__archicad_call_tool(
     name="properties_get_all_property_ids_of_elements",
     arguments={"port": <port>, "params": {"elements": [...]}}
   )
   # Schritt 2: Werte
   mcp__archicad__archicad_call_tool(
     name="properties_get_property_values_of_elements",
     arguments={"port": <port>, "params": {"elements": [...], "properties": [...]}}
   )
   ```

2. **Connected Elements** (für Hosting-Beziehungen): `elements_get_connected_elements`.

3. **Bounding Box** (für Position): `elements_get_2_d_bounding_boxes`.

4. **GDL-Parameter** (für Library-Object-Eigenschaften wie Sill-Höhe): `elements_get_gdl_parameters_of_elements`.

**In allen 4 Recipes:** Read-Worked-Examples nutzen diese Workarounds, NICHT `get_details_of_elements`. Im Gotchas-Block den Bug + Memory-Link erwähnen.

---

## Hinweise für Subagents

- **Voll-qualifizierte MCP-Toolnamen** überall (`mcp__archicad__elements_create_slabs`, nicht `elements_create_slabs`).
- **Test-GUIDs aus TEST-SET.md** in Worked Examples einsetzen, nicht erfinden.
- **D-02 Ton** (erklärend-freundlich, deutsch), keine CAPS-Direktiven.
- **Datums-Marker** `<!-- 2026-05-19 verifiziert AC29 -->` bei tatsächlich live aufgerufenen Tools; `<!-- VERIFY -->` bei aus Schemas abgeleiteten, aber nicht live getesteten Aufrufen.
- **One-level-deep References** — `../reference/foo.md`, niemals `../reference/foo/bar.md`.
- **Bulk-Klassifizierungs-Sektion** ist Stub mit Verweis auf `../reference/bulk-operations.md`.
- **TOC bei >100 Zeilen** Pflicht.
