# Decken, Stützen, Träger

Dieses Recipe deckt drei strukturelle Standard-Bauteile ab. **Slabs (Decken) und Columns (Stützen)** sind voll CRUD-fähig via MCP — Create wurde live gegen Archicad 29 verifiziert. **Beams (Träger)** können nicht erstellt werden — nur gelesen, modifiziert, gelöscht und klassifiziert. Hintergrund zur Capability-Lücke: [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Capability-Tabelle.

---

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [Discovery-Anker pro Element-Typ](#discovery-anker-pro-element-typ)
4. [Typische Parameter pro Element-Typ](#typische-parameter-pro-element-typ)
5. [Worked Example — Slab erstellen](#worked-example--slab-erstellen)
6. [Worked Example — Column erstellen](#worked-example--column-erstellen)
7. [Worked Example — Alle drei Typen einer Story lesen](#worked-example--alle-drei-typen-einer-story-lesen)
8. [Worked Example — Slab-Offset von Home-Story modifizieren](#worked-example--slab-offset-von-home-story-modifizieren)
9. [Worked Example — Column-Profil ändern](#worked-example--column-profil-ändern)
10. [Worked Example — Element löschen (alle 3 Typen)](#worked-example--element-löschen-alle-3-typen)
11. [Worked Example — Klassifizieren (tragend / nicht-tragend)](#worked-example--klassifizieren-tragend--nicht-tragend)
12. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
13. [Gotchas pro Element-Typ](#gotchas-pro-element-typ)
14. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang

| Element-Typ | Deutsch | Create | Read | Update | Delete | Classify |
|---|---|---|---|---|---|---|
| Slab | Decke | ✓ verifiziert <!-- 2026-05-19 verifiziert AC29 --> | ✓ (Property-Workaround) | ✓ | ✓ | ✓ |
| Column | Stütze | ✓ verifiziert <!-- 2026-05-19 verifiziert AC29 --> | ✓ (Property-Workaround) | ✓ | ✓ | ✓ |
| Beam | Träger | ✗ Capability-Gap | ✓ (Property-Workaround) | ✓ <!-- VERIFY --> | ✓ <!-- VERIFY --> | ✓ <!-- VERIFY --> |

Beam-Create ist in Archicad MCP v29 nicht verfügbar. Der User zeichnet Träger manuell in Archicad; danach kann Claude sie per GUID greifen und modifizieren/klassifizieren.

---

## Erforderlicher Warm-up-Kontext

Vor dem ersten MCP-Aufruf holen wir folgende Felder aus dem Warm-up (Details in [`../reference/workflow-context.md`](../reference/workflow-context.md)):

- **Port** — für jeden MCP-Call nötig (Beispiel: `19723`)
- **Aktive Story + Level** — Slab.level und Column.coordinates.z müssen zur Story-Referenzebene passen
- **Sichtbare Layer** (SAFE-03) — Create nur wenn Ziel-Layer sichtbar
- **Klassifikations-System-GUID** — für Klassifizierungs-Calls (SAB_Klassifizierung_29 in diesem Projekt)

---

## Discovery-Anker pro Element-Typ

### Slab

| Operation | Discovery-Query | Bekannter Tool-Name |
|---|---|---|
| Erstellen | `"create slab element"` | `elements_create_slabs` |
| Alle Decken listen | `"get elements by type"` | `elements_get_elements_by_type` |
| Eigenschaften lesen | `"get property values of elements"` | `properties_get_property_values_of_elements` |
| Modifizieren | `"set details of elements"` | `elements_set_details_of_elements` |
| Löschen | `"delete elements by id"` | `elements_delete_elements` |
| Klassifizieren | `"set classifications of elements"` | `elements_set_classifications_of_elements` |

### Column

| Operation | Discovery-Query | Bekannter Tool-Name |
|---|---|---|
| Erstellen | `"create column element"` | `elements_create_columns` |
| Alle Stützen listen | `"get elements by type"` | `elements_get_elements_by_type` |
| Eigenschaften lesen | `"get property values of elements"` | `properties_get_property_values_of_elements` |
| Modifizieren | `"set details of elements"` | `elements_set_details_of_elements` |
| Löschen | `"delete elements by id"` | `elements_delete_elements` |
| Klassifizieren | `"set classifications of elements"` | `elements_set_classifications_of_elements` |

### Beam <!-- VERIFY -->

| Operation | Discovery-Query | Bekannter Tool-Name |
|---|---|---|
| Create | — | **nicht verfügbar** |
| Alle Träger listen | `"get elements by type"` | `elements_get_elements_by_type` |
| Eigenschaften lesen | `"get property values of elements"` | `properties_get_property_values_of_elements` |
| Modifizieren | `"set details of elements"` | `elements_set_details_of_elements` |
| Löschen | `"delete elements by id"` | `elements_delete_elements` |
| Klassifizieren | `"set classifications of elements"` | `elements_set_classifications_of_elements` |

---

## Typische Parameter pro Element-Typ

### Slab (`SlabsDatum`)

```
{
  "level": <z-Koordinate in Metern — absolut, nicht relativ zur Story>,
  "polygonCoordinates": [{"x": <m>, "y": <m>}, ...],  // mind. 3 Punkte, Reihenfolge beachten
  "polygonArcs": [...],                                 // optional — für gekrümmte Kanten
  "holes": [                                            // optional — für Aussparungen
    {"polygonOutline": [...], "polygonArcs": [...]}
  ]
}
```

Wichtig: `level` ist eine **absolute Z-Koordinate** in Metern. Story EG (Level 0.0) → `"level": 0.0`. Wenn die Decke 30 cm über der Story-Referenzebene liegen soll: `"level": 0.3`.

Update-Felder via `SlabSettings` (aus Pydantic-Fehler-Analyse bekannt): `thickness`, `level`, `offsetFromTop`, `polygonOutline`, `holes`, `structureType`, `geometryType`.

### Column (`ColumnsDatum`)

```
{
  "coordinates": {"x": <m>, "y": <m>, "z": <m>}
}
```

`z` ist die **Unterkante der Stütze** in absoluten Metern. Höhe und Profil-Attribut werden über einen separaten Update-Call gesetzt (`elements_set_details_of_elements` mit `ColumnSettings`).

### Beam (`BeamDetails` — Schema aus Pydantic-Fehler-Analyse) <!-- VERIFY -->

BeamSettings-Felder aus AC29-Pydantic-Fehler bekannt:

```
{
  "begCoordinate": {"x": <m>, "y": <m>},   // Startpunkt 2D
  "endCoordinate": {"x": <m>, "y": <m>},   // Endpunkt 2D
  "level": <z-Koordinate absolut>,
  "bottomOffset": <m>,                      // Versatz von der Story-Ebene
  "offset": <m>,                            // Versatz von der Referenzlinie
  "height": <m>,                            // Trägerhöhe (Profil-Querschnitt)
  "slantAngle": <Grad>,                     // Neigungswinkel
  "verticalCurveHeight": <m>,               // für gebogene Träger
  "geometryType": "Straight",               // bekannte Werte: "Straight"
  "structureType": "Composite"              // bekannte Werte: "Composite"
}
```

In Phase 3 nicht live verifiziert, da kein Beam im Test-Set vorhanden war — Schema aus Discovery und Pydantic-Fehler-Response bekannt.

---

## Worked Example — Slab erstellen

<!-- 2026-05-19 verifiziert AC29 -->

Reproduktion der Test-Decke aus Wave 1: 2×2 m Quadrat an Position (10,0)–(12,2), Story EG (Level 0.0).

```python
mcp__archicad__archicad_call_tool(
  name="elements_create_slabs",
  arguments={
    "port": 19723,
    "params": {
      "slabsData": [{
        "level": 0.0,
        "polygonCoordinates": [
          {"x": 10.0, "y": 0.0},
          {"x": 12.0, "y": 0.0},
          {"x": 12.0, "y": 2.0},
          {"x": 10.0, "y": 2.0}
        ],
        "polygonArcs": [],
        "holes": []
      }]
    }
  }
)
```

Erwartete Response:

```json
{
  "elements": [
    {"elementId": {"guid": "01b90e9f-a241-dc45-a448-5acc06a186c4"}}
  ]
}
```

Hinweis: Die GUID `01b90e9f-a241-dc45-a448-5acc06a186c4` ist die aus Wave 1. Bei einem Neuerstellungs-Aufruf wird eine neue GUID zurückgegeben — wir speichern sie sofort im Arbeitsgedächtnis (SAFE-05) für Folge-Calls.

---

## Worked Example — Column erstellen

<!-- 2026-05-19 verifiziert AC29 -->

Reproduktion der Test-Stütze aus Wave 1: Position (11, 3, 0) auf Story EG.

```python
mcp__archicad__archicad_call_tool(
  name="elements_create_columns",
  arguments={
    "port": 19723,
    "params": {
      "columnsData": [{
        "coordinates": {"x": 11.0, "y": 3.0, "z": 0.0}
      }]
    }
  }
)
```

Erwartete Response:

```json
{
  "elements": [
    {"elementId": {"guid": "41adb22a-a347-784a-bcba-ac6137ce76e3"}}
  ]
}
```

Die Stütze wird mit Archicads Standard-Profil und -Höhe erstellt. Profil und Höhe anpassen: siehe [Column-Profil ändern](#worked-example--column-profil-ändern).

---

## Worked Example — Alle drei Typen einer Story lesen

<!-- 2026-05-19 verifiziert AC29 -->

Wir rufen `elements_get_elements_by_type` drei Mal auf — einmal pro Typ. Filter `OnActualFloor` sorgt dafür, dass nur Elemente der aktuellen Story zurückkommen.

**Schritt 1 — Decken der aktuellen Story listen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {
      "elementType": "Slab",
      "filters": ["OnActualFloor", "IsVisibleByLayer"]
    }
  }
)
```

**Schritt 2 — Stützen der aktuellen Story listen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {
      "elementType": "Column",
      "filters": ["OnActualFloor", "IsVisibleByLayer"]
    }
  }
)
```

**Schritt 3 — Träger der aktuellen Story listen:** <!-- VERIFY -->

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {
      "elementType": "Beam",
      "filters": ["OnActualFloor", "IsVisibleByLayer"]
    }
  }
)
```

Jede Response hat das Format `{"elements": [{"elementId": {"guid": "..."}}, ...]}`. Bei großen Projekten: Paginierung via `next_page_token` vollständig durchziehen (Details in [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Paginierung).

**Schritt 4 — Eigenschaften der gefundenen Elemente lesen (Property-Workaround):**

Da `elements_get_details_of_elements` in AC29 einen Pydantic-Wrapper-Bug hat (siehe [Gotchas — Alle Typen](#alle-typen-get_details_of_elements-bug)), lesen wir Eigenschaften über das Property-System:

```python
# Property-IDs ermitteln
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "01b90e9f-a241-dc45-a448-5acc06a186c4"}}
      ]
    }
  }
)

# Dann Werte abfragen
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "01b90e9f-a241-dc45-a448-5acc06a186c4"}}
      ],
      "properties": [
        {"propertyId": {"guid": "<property-id-aus-schritt-1>"}}
      ]
    }
  }
)
```

Position (Bounding Box) alternativ via `elements_get_2_d_bounding_boxes` ermitteln.

---

## Worked Example — Slab-Offset von Home-Story modifizieren

Ziel: Test-Decke (`01b90e9f-a241-dc45-a448-5acc06a186c4`) um 10 cm absenken — von Level 0.0 auf Level -0.1.

**Confirm-Dialog (SAFE-01):**

```
Ich werde folgendes ändern:
- Slab 01b90e9f  „Test-Decke EG"   level: 0.0m → -0.1m

Ausführen? (ja / nein / details / abbrechen)
```

**Nach Bestätigung:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "01b90e9f-a241-dc45-a448-5acc06a186c4"},
        "details": {
          "typeSpecificDetails": {
            "level": -0.1
          }
        }
      }]
    }
  }
)
```

Hinweis: Die genaue Struktur des `details`-Wrappers (ob `floorIndex` / `layerIndex` mitgeschickt werden müssen) ist in Phase 3 noch nicht vollständig verifiziert. Falls der Call ein Schemafehler zurückgibt, Discovery mit Query `"set slab details level"` erneut starten und das vollständige Schema lesen.

---

## Worked Example — Column-Profil ändern

<!-- VERIFY — Update-Schema für ColumnSettings in Phase 3 nicht live verifiziert -->

Stütze `41adb22a-a347-784a-bcba-ac6137ce76e3` auf ein anderes Profil-Attribut umstellen.

**Schritt 1 — Verfügbare Profil-Attribute entdecken:**

```python
mcp__archicad__archicad_call_tool(
  name="attributes_get_attribute_ids_by_type",
  arguments={
    "port": 19723,
    "params": {
      "attributeType": "Profile"
    }
  }
)
```

Response enthält eine Liste von Profil-Attributen mit GUIDs und Namen. Den gewünschten Profil-GUID notieren.

**Schritt 2 — Confirm-Dialog:**

```
Ich werde folgendes ändern:
- Column 41adb22a  „Stütze (11,3,0)"   Profil: Standard → HEA 200 (guid: <profil-guid>)

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 3 — Update-Call:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "41adb22a-a347-784a-bcba-ac6137ce76e3"},
        "details": {
          "typeSpecificDetails": {
            "profileAttributeId": {"guid": "<profil-guid>"}
          }
        }
      }]
    }
  }
)
```

Wenn `profileAttributeId` kein gültiges Schema-Feld ist: Discovery mit `"set column profile attribute"` + vollständiges `ColumnSettings`-Schema lesen. Das exakte Feld-Mapping von ColumnSettings wurde in Phase 3 noch nicht isoliert (kein vollständiger Pydantic-Fehler verfügbar).

---

## Worked Example — Element löschen (alle 3 Typen)

Das Löschen funktioniert für Slab, Column und Beam identisch — `elements_delete_elements` ist typ-unabhängig.

**Confirm-Dialog (SAFE-01) — Beispiel für alle 3 Typen gleichzeitig:**

```
Ich werde folgendes löschen:
- Slab   01b90e9f  „Test-Decke EG"     Level: 0.0m, Polygon 2×2m
- Column 41adb22a  „Stütze (11,3,0)"   Position: x=11, y=3
- Beam   <beam-guid>  „Träger EG"      von (x1,y1) → (x2,y2)

Ausführen? (ja / nein / details / abbrechen)
```

**Nach Bestätigung:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "01b90e9f-a241-dc45-a448-5acc06a186c4"}},
        {"elementId": {"guid": "41adb22a-a347-784a-bcba-ac6137ce76e3"}},
        {"elementId": {"guid": "<beam-guid>"}}
      ]
    }
  }
)
```

Wichtig: Slabs sind keine Hosts im Sinne von SAFE-04 — auf einer Decke platzierte Wände werden nicht automatisch mitgelöscht. Es gibt keinen Pre-Check für Slab-Delete (anders als bei Wänden mit gehosteten Fenstern/Türen). Column- und Beam-Delete ebenfalls ohne Pre-Check nötig.

---

## Worked Example — Klassifizieren (tragend / nicht-tragend)

Klassifizierung des Test-Slab und der Test-Column als „tragend" im System `SAB_Klassifizierung_29`.

**Schritt 1 — Klassifikations-System-GUID aus Warm-up** (oder via Discovery holen falls nicht gecacht):

```python
mcp__archicad__archicad_call_tool(
  name="classifications_get_all_classification_systems",
  arguments={
    "port": 19723,
    "params": {}
  }
)
# Response: [{classificationSystemId: {guid: "<system-guid>"}, name: "SAB_Klassifizierung_29", ...}]
```

**Schritt 2 — Verfügbare Klassifikations-Einträge lesen:**

```python
mcp__archicad__archicad_call_tool(
  name="classifications_get_all_classification_items_in_system",
  arguments={
    "port": 19723,
    "params": {
      "classificationSystemId": {"guid": "<system-guid>"}
    }
  }
)
# Relevante Items: "tragend" (guid: <tragend-guid>), "nicht-tragend" (guid: <nicht-tragend-guid>)
```

**Schritt 3 — Confirm-Dialog:**

```
Ich werde folgendes klassifizieren:
- Slab   01b90e9f  → tragend (SAB_Klassifizierung_29)
- Column 41adb22a  → tragend (SAB_Klassifizierung_29)

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 4 — Klassifikation setzen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithClassifications": [
        {
          "elementId": {"guid": "01b90e9f-a241-dc45-a448-5acc06a186c4"},
          "classifications": [{
            "classificationId": {"guid": "<tragend-guid>"}
          }]
        },
        {
          "elementId": {"guid": "41adb22a-a347-784a-bcba-ac6137ce76e3"},
          "classifications": [{
            "classificationId": {"guid": "<tragend-guid>"}
          }]
        }
      ]
    }
  }
)
```

Für Beam-Klassifizierung: identisches Muster, GUID des Beam-Elements einsetzen. <!-- VERIFY -->

**Klassifikation lesen (Verifikation):**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "01b90e9f-a241-dc45-a448-5acc06a186c4"}},
        {"elementId": {"guid": "41adb22a-a347-784a-bcba-ac6137ce76e3"}}
      ],
      "classificationSystemIds": [
        {"classificationSystemId": {"guid": "<system-guid>"}}
      ]
    }
  }
)
```

---

## Bulk-Klassifizierung (Stub)

Wenn alle Decken, Stützen oder Träger einer Story auf einmal klassifiziert werden sollen, folgt Phase 5 dem Read → Filter → Group → Confirm → Apply-Muster:

1. Alle Elemente des Typs via `elements_get_elements_by_type` mit Pagination sammeln.
2. Nach Kriterien filtern (z. B. Layer-Name, Bounding-Box-Zone).
3. In Gruppen aufteilen: „eindeutig tragend", „eindeutig nicht-tragend", „unklar".
4. Confirm-Dialog mit Summary (Format: >10 Elemente als Klassen-Count).
5. Apply mit `elements_set_classifications_of_elements`.

Details und vollständiges Muster: [`../reference/bulk-operations.md`](../reference/bulk-operations.md).

---

## Gotchas pro Element-Typ

### Slabs

**`level` ist eine absolute Z-Koordinate, nicht relativ zur Story.** Der häufigste Fehler: Story EG liegt bei Level 0.0, also `"level": 0.0` für eine bündig mit dem Boden liegende Decke. Wenn die Decke 25 cm unterhalb des EG-Niveaus liegen soll (z. B. als Kellerdecke): `"level": -0.25`. Den Story-Level immer aus dem Warm-up holen, nicht annehmen.

**Polygon-Koordinaten-Reihenfolge.** Die Punkte müssen in einer konsistenten Umlaufrichtung angegeben werden. Archicad erwartet vermutlich CCW (gegen den Uhrzeigersinn), aber das wurde in Phase 3 nicht explizit verifiziert — bei Fehlern (leere Response oder Geometrie-Fehler) Reihenfolge umkehren.

**`elements_get_details_of_elements` schlägt fehl.** Eigenschafts-Reads immer über Property-System-Workaround (siehe [Gotchas — Alle Typen](#alle-typen-get_details_of_elements-bug)).

**Decke ≠ Host.** Slabs hosten keine Elemente — anders als Wände. SAFE-04 (Hosted-Element-Pre-Check) gilt hier nicht.

### Columns

**`coordinates.z` ist die Unterkante.** Die Stütze wächst von `z` nach oben. Höhe wird separat über `ColumnSettings` im Update-Call gesetzt, nicht beim Create. Eine frisch erstellte Stütze hat die Archicad-Standard-Höhe (meist Story-Höhe).

**Profil-Zuweisung ist kein Create-Parameter.** Das Profil-Attribut (HEA, HEB, Rundstahl, etc.) kann nicht beim Create mitgegeben werden — nur über einen nachfolgenden `elements_set_details_of_elements`-Call. Wenn Profil und Position gemeinsam gesetzt werden sollen: Create, GUID speichern (SAFE-05), dann Update.

**Profil-Attribut-GUIDs sind projektspezifisch.** GUIDs für Profil-Attribute (z. B. „HEA 200") sind projektabhängig und können sich zwischen Projekten unterscheiden. Immer über `attributes_get_attribute_ids_by_type` mit `"Profile"` frisch ermitteln.

**`elements_get_details_of_elements` schlägt fehl.** Gilt für Columns gleichermaßen — Property-Workaround verwenden.

### Beams

**Kein Create per MCP v29.** Der User zeichnet Träger manuell in Archicad. Nach dem Zeichnen kann Claude die Beam-GUIDs via `elements_get_elements_by_type` mit `"elementType": "Beam"` ermitteln.

**Alle Beam-Worked-Examples sind nicht live verifiziert.** In Phase 3 war kein Beam im Test-Set vorhanden (User hat keinen Beam gezeichnet). Schema aus Discovery und Pydantic-Fehler-Response bekannt, aber nicht durch tatsächliche MCP-Calls bestätigt. Marker: `<!-- VERIFY -->` — bei der ersten Beam-Operation in einer späteren Session bitte prüfen und dieses Recipe aktualisieren.

**BeamSettings-Felder aus Pydantic-Fehler.** Die Felder `slantAngle`, `verticalCurveHeight`, `geometryType`, `structureType` sind AC29-spezifische Erweiterungen, die den `elements_get_details_of_elements`-Bug verursachen. Update-Calls via `elements_set_details_of_elements` sollten funktionieren — aber das exakte Schema-Mapping für BeamSettings ist noch nicht durch einen erfolgreichen Call belegt.

**Beam-Profil-Update analog zu Column.** Wenn Träger-Profile geändert werden sollen: `profileAttributeId` im `typeSpecificDetails`-Block setzen — gleicher Mechanismus wie bei Stützen.

### Alle Typen: `get_details_of_elements`-Bug

`elements_get_details_of_elements` schlägt in AC29 mit einem Pydantic-Validierungsfehler fehl. Ursache: AC29 gibt neue Felder zurück (`structureType`, `geometryType`, `arcAngle`, `slantAngle`, `verticalCurveHeight`), die im MCP-Server-Schema als `extra='forbid'` markiert sind.

Workarounds für Read-Operationen:

- **Eigenschaften:** `properties_get_all_property_ids_of_elements` + `properties_get_property_values_of_elements`
- **Position:** `elements_get_2_d_bounding_boxes`
- **Typ:** `elements_get_types_of_elements`
- **Klassifikation:** `elements_get_classifications_of_elements`
- **GDL-Parameter** (Library-Objekte): `elements_get_gdl_parameters_of_elements`

Details und Update-Calls (die trotz Bug funktionieren): [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Wrapper-Bug.

---

## Verwandte Recipes

- [`zones.md`](zones.md) — Zonen können auf Decken-Niveau verankert sein; `elements_get_elements_related_to_zones` verknüpft Stützen mit Zonen
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Bulk-Klassifizierung tragend/nicht-tragend für viele Elemente gleichzeitig
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Capability-Tabelle (alle Create-fähigen Typen), Confirm-Format, Paginierung, Wrapper-Bug-Workarounds
