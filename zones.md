# Zonen / Räume

Zonen sind Archicads Kernkonzept für Flächen-Erfassung, Raum-Schedules und Klassifikation. Eine Zone definiert sich durch ein 2D-Polygon, einen Namen, eine Nummer und optional eine Kategorie sowie eine Stempel-Position. Zonen sind voll CRUD-fähig via MCP — Create ist live verifiziert seit Phase 1 und Wave 1. <!-- 2026-05-19 verifiziert AC29 -->

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [Discovery-Anker](#discovery-anker)
4. [Typische Parameter](#typische-parameter)
5. [Worked Example — Zone manuell erstellen (Polygon)](#worked-example--zone-manuell-erstellen-polygon)
6. [Worked Example — Zonen einer Story lesen](#worked-example--zonen-einer-story-lesen)
7. [Worked Example — Zone-Properties modifizieren](#worked-example--zone-properties-modifizieren)
8. [Worked Example — Zone löschen](#worked-example--zone-löschen)
9. [Worked Example — Zone klassifizieren](#worked-example--zone-klassifizieren)
10. [Worked Example — Property setzen (Bodenbelag-Pattern für Bulk-Updates)](#worked-example--property-setzen-bodenbelag-pattern-für-bulk-updates)
11. [Bonus-Beispiel — Zone automatisch erstellen (Reference-Point)](#bonus-beispiel--zone-automatisch-erstellen-reference-point)
12. [Verwandte Operationen: elements_get_elements_related_to_zones](#verwandte-operationen-elements_get_elements_related_to_zones)
13. [Zone-Membership für Bulk-Klassifizierung (Stub)](#zone-membership-für-bulk-klassifizierung-stub)
14. [Gotchas](#gotchas)
15. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang

- Zonen erstellen via Polygon (`ManualZoneGeometry`) oder Reference-Point (`AutomaticZoneGeometry`).
- Alle Zonen einer Story lesen — via `elements_get_elements_by_type` mit `elementType: "Zone"`.
- Geometrie und Bounding Box einer Zone abfragen (Property-Workaround, da `get_details_of_elements` in AC29 einen Wrapper-Bug hat).
- Modifizieren: Name, Nummer, Category-Attribute, Stempel-Position.
- Klassifikation lesen und setzen (Standard-Klassifikations-Flow via `SAB_Klassifizierung_29`).
- Löschen — Zone hat keine gehosteten Elemente, kein Pre-Check nötig.
- Zone-Membership: Wände und Stützen einer Zone ermitteln (Basis für Innen/Außen-Wand-Klassifikation).

---

## Erforderlicher Warm-up-Kontext

Vor jedem Zonen-Auftrag benötigen wir:

| Feld | Woher | Wozu |
|---|---|---|
| `port` | `mcp__archicad__discovery_list_active_archicads` | Jeder MCP-Call |
| aktive Story (`floorIndex`) | `mcp__archicad__archicad_call_tool` → „get story info" | `floorIndex` beim Create, Filter `OnActualFloor` beim Read |
| sichtbare Layer | Warm-up | SAFE-03: Create nur wenn Zonen-Layer sichtbar |
| Klassifikations-System-GUID | `mcp__archicad__archicad_call_tool` → „get classification systems" | Klassifikation lesen/setzen |

Vollständiges Warm-up-Protokoll: [`../reference/workflow-context.md`](../reference/workflow-context.md).

---

## Discovery-Anker

Zonen-relevante Tools bei Bedarf per Discovery bestätigen:

| Operation | Query-String | Typischer Tool-Name |
|---|---|---|
| Zone erstellen | `"create zone room"` | `elements_create_zones` |
| Zonen listen | `"get elements by type zone"` | `elements_get_elements_by_type` |
| Zone-Properties schreiben | `"set details of elements zone"` | `elements_set_details_of_elements` |
| Zone löschen | `"delete elements by id"` | `elements_delete_elements` |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` |
| Klassifikation setzen | `"set classifications of elements"` | `elements_set_classifications_of_elements` |
| Zone-Membership | `"get elements related to zones"` | `elements_get_elements_related_to_zones` |
| ZoneCategory-Attribute | `"get attributes by type zone category"` | `attributes_get_attributes_by_type` |
| Bounding Box | `"get 2d bounding boxes"` | `elements_get_2_d_bounding_boxes` |
| Property-Werte | `"get property values of elements"` | `properties_get_property_values_of_elements` |

---

## Typische Parameter

### ZonesDatum (für Create)

| Parameter | Typ | Pflicht | Beschreibung |
|---|---|---|---|
| `floorIndex` | Integer | nein | Story-Index; wenn weggelassen → aktive Story |
| `name` | String | ja | Klartext-Name der Zone, z. B. „Wohnzimmer" |
| `numberStr` | **String** | ja | Zone-Nummer als String — nicht als Integer. Führende Nullen erlaubt: „01", „T01", „EG-01" |
| `categoryAttributeId` | `{guid: "<cat-guid>"}` | nein | Verknüpft die Zone mit einer ZoneCategory (z. B. „Wohnen", „Nebenraum") |
| `stampPosition` | `{x: <m>, y: <m>}` | nein | 2D-Koordinate für die Stempel-Platzierung; wenn weggelassen → Archicad berechnet Schwerpunkt |
| `geometry` | Objekt | ja | Entweder `ManualZoneGeometry` oder `AutomaticZoneGeometry` (s. u.) |

### ManualZoneGeometry

```json
{
  "polygonCoordinates": [
    {"x": 14.0, "y": 0.0},
    {"x": 18.0, "y": 0.0},
    {"x": 18.0, "y": 4.0},
    {"x": 14.0, "y": 4.0}
  ],
  "polygonArcs": [],
  "holes": []
}
```

- Mindestens 3 Punkte. Das Polygon muss geschlossen sein (letzter Punkt ≠ erster Punkt — Archicad schließt automatisch).
- `polygonArcs`: optionale Kreisbögen für geschwungene Kanten.
- `holes`: optionale Inseln / Aussparungen (z. B. Stütze in der Zone).

### AutomaticZoneGeometry

```json
{
  "referencePosition": {"x": 16.0, "y": 2.0}
}
```

Archicad findet die umschließenden Wände selbst und konstruiert das Polygon. Der Punkt muss innerhalb eines vollständig umschlossenen Raums liegen — sonst Fehler. Siehe [Bonus-Beispiel](#bonus-beispiel--zone-automatisch-erstellen-reference-point).

---

## Worked Example — Zone manuell erstellen (Polygon)

Reproduziert den Wave-1-Setup-Aufruf: Test-Zone „Test-Zone" T01, 4 × 4 m, Story 0 (EG).
<!-- 2026-05-19 verifiziert AC29 -->

**Kontext:** Port 19723, Story 0 (floorIndex 0), Zonen-Layer sichtbar.

```python
mcp__archicad__archicad_call_tool(
  name="elements_create_zones",
  arguments={
    "port": 19723,
    "params": {
      "zonesData": [
        {
          "floorIndex": 0,
          "name": "Test-Zone",
          "numberStr": "T01",
          "stampPosition": {"x": 16.0, "y": 2.0},
          "geometry": {
            "polygonCoordinates": [
              {"x": 14.0, "y": 0.0},
              {"x": 18.0, "y": 0.0},
              {"x": 18.0, "y": 4.0},
              {"x": 14.0, "y": 4.0}
            ],
            "polygonArcs": [],
            "holes": []
          }
        }
      ]
    }
  }
)
```

**Erwartete Response:**

```json
{
  "elements": [
    {"elementId": {"guid": "e0394527-a1fa-b24b-bab2-d75ffce4cc7e"}}
  ]
}
```

Die zurückgegebene GUID für den Rest des Auftrags im Arbeitsgedächtnis behalten (SAFE-05).

**Was danach prüfen:** Zone im Grundriss sichtbar, Stempel erscheint bei (16, 2), Fläche = 16 m².

---

## Worked Example — Zonen einer Story lesen

Alle Zonen auf Story 0 auflisten, dann Bounding Box und Properties für die Test-Zone abrufen. `elements_get_details_of_elements` schlägt in AC29 fehl (Wrapper-Bug, siehe [Gotchas](#gotchas)) — daher Workaround via Bounding Box + Property-System.

**Zonen listen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {
      "elementType": "Zone",
      "filters": ["OnActualFloor", "IsVisibleByLayer"]
    }
  }
)
```

Response: Liste von `{elementId: {guid: "..."}}`. `next_page_token` vollständig abarbeiten bevor weitergemacht wird.

**Bounding Box (Geometrie):**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_2_d_bounding_boxes",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "e0394527-a1fa-b24b-bab2-d75ffce4cc7e"}}]
    }
  }
)
```

Response: `{boundingBoxes: [{xMin: 14, yMin: 0, xMax: 18, yMax: 4}]}`.

**Properties (z. B. Fläche):** Erst `properties_get_all_property_ids_of_elements` aufrufen um IDs zu holen, dann `properties_get_property_values_of_elements` mit den gewünschten IDs (z. B. „Calculated Area").

---

## Worked Example — Zone-Properties modifizieren

Test-Zone umbenennen von „Test-Zone" → „Wohnzimmer", Nummer T01 → W01.

**Confirm-Dialog vor dem Aufruf:**

```
Ich werde folgendes ändern:
- Zone e0394527-a1fa-b24b-bab2-d75ffce4cc7e  „Test-Zone" → „Wohnzimmer"  Nummer: T01 → W01

Ausführen? (ja / nein / details / abbrechen)
```

Nach Bestätigung mit `ja`:

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "e0394527-a1fa-b24b-bab2-d75ffce4cc7e"},
          "details": {
            "typeSpecificDetails": {
              "name": "Wohnzimmer",
              "numberStr": "W01"
            }
          }
        }
      ]
    }
  }
)
```

<!-- VERIFY --> Exakte Feldnamen im `typeSpecificDetails`-Objekt für Zonen sind aus Schema-Discovery abgeleitet, aber in AC29 nicht live getestet. Falls Aufruf mit 400-Fehler zurückkommt: Discovery mit Query „set zone details name number" erneut aufrufen und Schema prüfen. Alternativ über `properties_set_property_values_of_elements` arbeiten, wenn Zone-Name und -Nummer als Properties exponiert sind.

**Stempel-Position anpassen** (optional, im selben Call):

```python
"typeSpecificDetails": {
  "name": "Wohnzimmer",
  "numberStr": "W01",
  "stampPosition": {"x": 15.5, "y": 1.5}
}
```

Hinweis: `stampPosition` steuert, wo der Stempel im Plan erscheint — nicht ob er erscheint. Stempel-Sichtbarkeit (an/aus) wird über den Zone-Layer und die Ansichts-Einstellungen gesteuert, nicht über dieses Feld.

---

## Worked Example — Zone löschen

Zonen haben keine gehosteten Elemente — kein Pre-Check wie bei Wänden nötig. Trotzdem zeigen wir den Standard-Confirm.

**Confirm-Dialog:**

```
Ich werde folgendes löschen:
- Zone e0394527-a1fa-b24b-bab2-d75ffce4cc7e  „Test-Zone" (T01)

Diese Zone hat keine gehosteten Elemente. Die Operation kann via Archicad-Undo rückgängig gemacht werden.

Ausführen? (ja / nein / abbrechen)
```

Nach `ja`:

```python
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "e0394527-a1fa-b24b-bab2-d75ffce4cc7e"}}
      ]
    }
  }
)
```

Response enthält typischerweise eine Bestätigungs-Struktur. Falls Response-Format unklar ist (Response-Wrapper-Variante aus Wave-1-Cleanup), prüfen: Zone nicht mehr in `elements_get_elements_by_type`-Listing → Erfolg.

---

## Worked Example — Zone klassifizieren

Klassifikation der Test-Zone in `SAB_Klassifizierung_29` auf „Wohnen" setzen. Voraussetzung: Klassifikations-System-GUID aus dem Warm-up bekannt.

**Aktuelle Klassifikation lesen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "e0394527-a1fa-b24b-bab2-d75ffce4cc7e"}}],
      "classificationSystemIds": [
        {"classificationSystemId": {"guid": "<SAB_Klassifizierung_29-system-guid>"}}
      ]
    }
  }
)
```

GUID für „Wohnen" via Discovery ermitteln (Query: „get classification items in system"), dann Confirm zeigen:

```
Ich werde folgendes ändern:
- Zone e0394527-a1fa-b24b-bab2-d75ffce4cc7e  „Test-Zone"  Klassifikation: (keine) → „Wohnen"

Ausführen? (ja / nein / details / abbrechen)
```

**Klassifikation setzen** (nach `ja`):

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithClassifications": [
        {
          "elementId": {"guid": "e0394527-a1fa-b24b-bab2-d75ffce4cc7e"},
          "classifications": [
            {
              "classificationSystemId": {"guid": "<SAB_Klassifizierung_29-system-guid>"},
              "classificationItemId": {"guid": "<wohnen-item-guid>"}
            }
          ]
        }
      ]
    }
  }
)
```

**ZoneCategory-Attribute** (Archicad-interne Raum-Kategorie, separat von Projektklassifikation): GUIDs via `attributes_get_attributes_by_type` mit `attributeType: "ZoneCategory"` abfragen. Response liefert `{attributeId: {guid: "..."}, name: "..."}` — gewünschte GUID als `categoryAttributeId` beim Zone-Update übergeben. Beeinflusst Flächenberechnungsregeln und Schedule-Darstellung.

---

## Worked Example — Property setzen (Bodenbelag-Pattern für Bulk-Updates)

Wenn der User sagt „setze Bodenbelag aller Wohnräume auf Linoleum 2,5 (R9)" — das ist ein häufiger Bulk-Update-Workflow für Custom-Properties. Diese 6-Schritt-Sequenz ist verifiziert (2026-05-20 gegen BSZ Gunzenhausen V29) und gilt als Pattern für jede Property-Bulk-Operation auf Zonen.

**Voraussetzungen:**
- Modal-Dialoge in Archicad geschlossen (siehe [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Modal-Dialoge).
- Property „Bodenbelag" (oder welche auch immer) existiert in der Property-Definition mit den nötigen Enum-Werten — bei fehlenden Werten: Pre-Flight-Check in Schritt 4 fängt das ab.

### Schritt 1 — Alle Zonen listen <!-- 2026-05-20 verifiziert AC29 -->

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": <port>,
    "params": {"elementType": "Zone"}
  }
)
```

Paginierung bis vollständig durchziehen. Resultat: Liste aller Zone-GUIDs.

### Schritt 2 — Aktuelle Selektion abfragen (optional, wenn User auf Selektion zielt)

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_selected_elements",
  arguments={"port": <port>}
)
```

**Gotcha:** Wenn der User vorher einen modalen Dialog hatte und mit „Abbrechen" geschlossen hat, kann die Selektion leer sein. Wenn das passiert: User bitten, neu zu selektieren — Selektion-Recovery ist nicht via MCP möglich.

### Schritt 3 — Property-IDs der Zone(n) holen <!-- 2026-05-20 verifiziert AC29 -->

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<zone-guid>"}}],
      "propertyType": "UserDefined"
    }
  }
)
```

Liefert die Custom-Property-IDs der Zone. Typisches Test-Projekt: ~26 Custom-Properties pro Zone, organisiert in Gruppen wie „Allgemeine Werte", „Produktinformationen", „Räume".

`propertyType` weglassen, wenn auch Built-in-Properties (z. B. Raumnummer) gebraucht werden.

### Schritt 4 — Property-Definition holen (Enum-Werte + Type) <!-- 2026-05-20 verifiziert AC29 -->

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_details_of_properties",
  arguments={
    "port": <port>,
    "params": {
      "properties": [{"propertyId": {"guid": "<bodenbelag-property-guid>"}}]
    }
  }
)
```

Response enthält:
- `name`, `description`, `type` (z. B. `"singleEnum"`)
- `group` (Property-Gruppe)
- `possibleEnumValues` (bei Enum-Properties: Liste mit `{enumValueId: {type, displayValue, nonLocalizedValue}, displayValue}`)

**Pre-Flight-Check** (Enum-Normalisierung — siehe [`../reference/bulk-operations.md`](../reference/bulk-operations.md) § Pre-Flight):
- Source-Wert (z. B. GDL-Stempel-Wert „Linoleum 2,5 (R9)") gegen `possibleEnumValues` matchen.
- Bei No-Match: STOPPEN. User informieren, Enum erweitern lassen via Archicad-UI (siehe Sektion „Enum-Wert ergänzen" unten) — niemals einen falschen oder neuen Wert via Set übergeben.

### Schritt 5 — Aktuelle Werte lesen (Diff-Vorbereitung)

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<zone-guid>"}}],
      "properties": [{"propertyId": {"guid": "<bodenbelag-property-guid>"}}]
    }
  }
)
```

Resultat: aktueller Property-Wert pro Element. Damit Diff-Summary aufbauen: alt → neu.

### Schritt 6 — Werte setzen (asymmetrisch sicher — Confirm-Schleife) <!-- VERIFY -->

**SAFE-01 greift hier zwingend.** Vor dem Set-Aufruf User-Confirm mit Diff-Summary anzeigen (siehe [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Confirm-Format). Erst nach `ja`:

```python
mcp__archicad__archicad_call_tool(
  name="properties_set_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementPropertyValues": [{
        "elementId": {"guid": "<zone-guid>"},
        "propertyId": {"guid": "<bodenbelag-property-guid>"},
        "propertyValue": {
          "type": "singleEnum",
          "status": "normal",
          "value": {"type": "displayValue", "displayValue": "Linoleum 2,5 (R9)"}
        }
      }]
    }
  }
)
```

<!-- VERIFY --> Genaues Schema für `propertyValue` bei Enum-Type in Phase 5 verifizieren — möglicherweise wird `nonLocalizedValue` statt `displayValue` erwartet, oder die `enumValueId` direkt.

**Mid-Batch-Fehlerverhalten:** Siehe [`../reference/bulk-operations.md`](../reference/bulk-operations.md) § Mid-Batch-Fehlerverhalten.

### Enum-Wert ergänzen (Archicad-UI, nicht via MCP)

**MCP-Capability-Gap (Stand 2026-05-20):** `properties_create_property_definitions` legt **neue** Properties an, modifiziert aber bestehende Enum-Definitionen nicht. Wenn ein Source-Wert fehlt, muss der User ihn in Archicad selbst hinzufügen:

1. Zonen-Einstellungen-Dialog öffnen
2. Bereich „Klassifizierung und Eigenschaften" → Property „Bodenbelag" aufklappen
3. Enum-Feld → **⊞-Button** → „Bearbeiten" → öffnet „**Optionen-Setup**"
4. „Hinzufügen" → neuer Enum-Wert (oder „Mehrfachauswahl erlauben" für Multi-Enum)
5. OK schließt → Persistiert

**Modal-Dialog-Warnung:** Während „Optionen-Setup" offen ist, blockieren alle MCP-Calls. User „fertig" melden lassen, dann fortfahren.

**Empfehlung für Bodenbelag-Sync:** Wenn viele Werte ergänzt werden müssen, vorher mit dem User die komplette Wunschliste aufnehmen, einmal alle ergänzen, dann erst Bulk-Set starten — statt iterativ Dialog-auf-zu.

---

## Bonus-Beispiel — Zone automatisch erstellen (Reference-Point)

<!-- VERIFY --> Schema-dokumentiert, in Phase 3 nicht live getestet. Verifikation in Phase 5 oder späterer Session.

Statt Polygon manuell anzugeben: Punkt innerhalb des Raums setzen. Archicad findet die umschließenden Wände und konstruiert das Polygon selbst.

```python
mcp__archicad__archicad_call_tool(
  name="elements_create_zones",
  arguments={
    "port": 19723,
    "params": {
      "zonesData": [
        {
          "floorIndex": 0,
          "name": "Küche",
          "numberStr": "K01",
          "geometry": {
            "referencePosition": {"x": 5.0, "y": 3.5}
          }
        }
      ]
    }
  }
)
```

Der Punkt muss vollständig durch Wände umschlossen sein. Liegt er im Freien, schlägt der Call fehl oder erzeugt eine Null-Flächen-Zone. Fallback: `ManualZoneGeometry` verwenden.

---

## Verwandte Operationen: elements_get_elements_related_to_zones

Listet Wände, Stützen und andere Elemente, die räumlich zu einer Zone gehören:

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_related_to_zones",
  arguments={
    "port": 19723,
    "params": {
      "zones": [{"elementId": {"guid": "e0394527-a1fa-b24b-bab2-d75ffce4cc7e"}}],
      "elementType": "Wall"
    }
  }
)
```

Response: Liste der Wand-GUIDs der Zone. Analog für `"Column"`, `"Slab"` etc. Grundlage für den Innen/Außen-Wand-Klassifikations-Workflow (Phase 5).

---

## Zone-Membership für Bulk-Klassifizierung (Stub)

In Phase 5: zonenbasierte Innen/Außen-Klassifikation aller Wände. Schematisch:

1. Alle Zonen via `elements_get_elements_by_type` (`elementType: "Zone"`) laden.
2. Pro Zone Wände via `elements_get_elements_related_to_zones` ermitteln.
3. Heuristik: Wand in Zone → Innenwand; Wand in keiner Zone → Außenwand; Grenze zweier Zonen → Trennwand.
4. Confirm mit Counts, dann `elements_set_classifications_of_elements`.

Vollständiges Read → Filter → Group → Confirm → Apply-Muster: [`../reference/bulk-operations.md`](../reference/bulk-operations.md).

---

## Gotchas

### 1. `numberStr` ist String, nicht Integer

Das Feld heißt `numberStr` und erwartet immer einen String. Führende Nullen sind erwünscht und bleiben erhalten: `"01"`, `"T01"`, `"EG-01"`. Wenn man `1` (Integer) übergibt, kann der API-Call mit einem Schema-Fehler scheitern oder die Nummer inkonsistent aussehen.

### 2. `AutomaticZoneGeometry` braucht umschließende Wände

`referencePosition` funktioniert nur, wenn der Punkt vollständig durch Wände umschlossen ist. Liegt der Punkt im Freien, schlägt Archicad den Call fehl oder erzeugt eine unbrauchbare Zone. In offenen oder unvollständig modellierten Grundrissen ist `ManualZoneGeometry` die robustere Wahl.

### 3. Polygon-Selbstüberschneidung führt zu Fehler

Selbstüberschneidende `polygonCoordinates` sind ungültig — Archicad lehnt den Call ab oder erzeugt eine Zone mit falscher Fläche. Koordinaten vor dem Call auf Überschneidungsfreiheit prüfen (Uhrzeigersinn-Reihenfolge empfohlen).

### 4. ZoneCategory-GUID kommt aus dem Attribute-Typ `ZoneCategory`

`categoryAttributeId` muss aus dem Archicad-Attribute-Register stammen — nicht erfinden oder aus anderen Projekten kopieren. Immer via `attributes_get_attributes_by_type` mit `attributeType: "ZoneCategory"` abfragen. Falsche GUIDs → Fehler oder stille Ignorierung.

### 5. Stempel-Position vs. Stempel-Sichtbarkeit

`stampPosition` steuert, **wo** der Stempel im Plan gedruckt wird (2D-Koordinate). Es steuert nicht, **ob** der Stempel erscheint. Die Sichtbarkeit des Stempels hängt vom aktiven Layer der Zone und den Ansichts-Einstellungen in Archicad ab. Wenn ein Stempel nicht erscheint obwohl `stampPosition` gesetzt ist: Layer-Sichtbarkeit und Zone-Darstellungs-Optionen in Archicad prüfen.

### 6. `elements_get_details_of_elements` schlägt in AC29 fehl (Wrapper-Bug)

Pydantic-Validierungsfehler wegen neuer AC29-Felder (`structureType`, `geometryType`, etc.). Workarounds: Geometrie via `elements_get_2_d_bounding_boxes`, Properties via `properties_get_property_values_of_elements`, Typ via `elements_get_types_of_elements`. Memory-Eintrag: `~/.claude/projects/-Users-ap/memory/issue_archicad_mcp_get_details_bug.md`.

### 7. Zone-Fläche kommt aus dem Property-System

Die berechnete Netto/Brutto-Fläche ist eine Property (z. B. „Calculated Area"), keine direkte Geometrie-Eigenschaft. Via `properties_get_property_values_of_elements` abfragen. Der Wert hängt von `categoryAttributeId` (Flächenberechnungsregel) ab — dieselbe Polygon-Fläche kann je nach Kategorie unterschiedlich ausgewiesen werden.

---

## Verwandte Recipes

- [`slabs-columns-beams.md`](slabs-columns-beams.md) — Zonen definieren sich oft über einer Slab als Bodenfläche; Slab-Polygon und Zonen-Polygon können deckungsgleich sein.
- [`walls.md`](walls.md) — Innen/Außen-Klassifikation von Wänden basiert auf Zone-Membership (s. Phase-5-Stub oben).
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Universelles Bulk-Klassifizierungs-Muster für zonenbasierte Workflows.
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Confirm-Format-Details, Fehlerklassen, Paginierung.
