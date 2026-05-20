# Fassaden / Pfosten-Riegel-Konstruktionen — CurtainWall + Sub-Elemente

Fassaden sind in Archicad Composite-Konstruktionen: ein Top-Level-CurtainWall-Element enthält eine Hierarchie von bis zu 5 Sub-Element-Typen (Segmente, Pfosten, Paneele, Knoten, Zubehör). **Erstellen via MCP v29 nicht verfügbar** — der User zeichnet die Fassade manuell mit dem Curtain-Wall-Werkzeug (nicht das Wand-Werkzeug — das sind verschiedene Tools in Archicad). Sub-Elemente entstehen automatisch mit dem Top-Level und werden über die Hierarchie navigiert, nicht eigenständig erstellt. Vollständige Capability-Tabelle: [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Live-verifizierte Element-Create-Capabilities.

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [CurtainWall-Hierarchie auf einen Blick](#curtainwall-hierarchie-auf-einen-blick)
4. [Discovery-Anker pro Element-Typ](#discovery-anker-pro-element-typ)
5. [Typische Parameter pro Element-Typ](#typische-parameter-pro-element-typ)
6. [Worked Example — CurtainWall (Top-Level) lesen](#worked-example--curtainwall-top-level-lesen)
7. [Worked Example — Sub-Elemente eines CurtainWall finden](#worked-example--sub-elemente-eines-curtainwall-finden)
8. [Worked Example — CurtainWallPanel modifizieren](#worked-example--curtainwallpanel-modifizieren)
9. [Worked Example — CurtainWallFrame modifizieren](#worked-example--curtainwallframe-modifizieren)
10. [Worked Example — CurtainWallJunction operations](#worked-example--curtainwalljunction-operations)
11. [Worked Example — CurtainWallSegment + Accessory Operations](#worked-example--curtainwallsegment--accessory-operations)
12. [Worked Example — CurtainWall klassifizieren](#worked-example--curtainwall-klassifizieren)
13. [Worked Example — CurtainWall löschen (mit Sub-Element-Pre-Check)](#worked-example--curtainwall-löschen-mit-sub-element-pre-check)
14. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
15. [Gotchas pro Element-Typ](#gotchas-pro-element-typ)
16. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang

**Abgedeckt:**
- Alle 6 CurtainWall-Element-Typen: Top-Level + CurtainWallSegment + CurtainWallFrame + CurtainWallPanel + CurtainWallJunction + CurtainWallAccessory.
- Top-Level: Read + Update (Settings) + Delete (mit SAFE-04-Pre-Check) + Classify.
- Sub-Elemente: Read über Hierarchie-Tool, Update-Calls wo MCP das Schema anbietet.
- Hierarchie-Navigation via `elements_get_subelements_of_hierarchical_elements` (live verifiziert).

**Nicht abgedeckt:**
- CurtainWall erstellen via MCP (Capability-Gap in v29 — kein Create-Tool).
- Sub-Elemente eigenständig erstellen (entstehen mit Top-Level, kein separates Create).
- CurtainWallJunction + CurtainWallAccessory Update-Details (in Default-Fassaden nicht vorhanden, siehe Gotchas § Junction und § Accessory).

---

## Erforderlicher Warm-up-Kontext

Details in [`../reference/workflow-context.md`](../reference/workflow-context.md).

| Feld | Warum hier wichtig |
|---|---|
| **Port** | Jeder MCP-Call braucht `"port"`. |
| **Aktive Story** (`floorIndex`) | CurtainWalls listen mit `OnActualFloor` oder projektweitem Scan. |
| **Sichtbare Layer** | SAFE-03: Ziel-Layer prüfen vor jedem Write. |
| **Klassifikations-System-GUID** | Nur für Classify-Calls — immer live ermitteln, nie hartkodieren. |

**Modal-Dialog-Vorab-Check:** Bei Bulk-Operationen den User vor Start bitten, alle offenen Dialoge in Archicad zu schließen. Ein offener Attribute-Manager oder Material-Editor blockiert alle MCP-Calls mit Code 4001 (Details: [Gotcha § Modal-Dialog-Blockade](#modal-dialog-blockade-code-4001--allgemeine-warnung)).

---

## CurtainWall-Hierarchie auf einen Blick

```
CurtainWall (Top-Level)
├── CurtainWallSegment      ← horizontale/vertikale Unterteilung des CW-Feldes
│   ├── CurtainWallFrame    ← Profile: Pfosten, Riegel, Border-Frames
│   ├── CurtainWallPanel    ← Füll-Paneele: Glas, opake Platten, Lüftungselemente
│   └── CurtainWallJunction ← Knotenpunkte zwischen Frames (nur bei komplexen Geometrien)
└── CurtainWallAccessory    ← Zubehör: Beschläge, Sonderelemente (Library-Object-basiert)
```

**Schlüssel-Erkenntnis für Navigation:**
- Sub-Elemente können nicht projektweites Listing → Top-Level-Zuordnung eigenständig leisten.
- Der kanonische Weg zum Zugriff auf Sub-Elemente ist immer: **Top-Level-GUID bekannt → `elements_get_subelements_of_hierarchical_elements` aufrufen → Sub-GUIDs extrahieren**.
- `elements_get_elements_by_type` mit `CurtainWallFrame` liefert alle Frames projektweit — ohne Top-Level-Zuordnung. Nur für Bulk-Szenarien sinnvoll.

**SAFE-05 (Element-ID-Threading):** Die Top-Level-GUID ist der Anker für alle Sub-Operationen. In der Session behalten — nicht erneut suchen.

---

## Discovery-Anker pro Element-Typ

<!-- 2026-05-20 verifiziert AC29 -->

| Element-Typ | Discovery-Query | Verifizierter Tool-Name |
|---|---|---|
| CurtainWall listen | `"get elements by type curtain wall"` | `elements_get_elements_by_type` |
| Sub-Elemente holen | `"get subelements of hierarchical elements"` | `elements_get_subelements_of_hierarchical_elements` |
| CurtainWallFrame listen (projektweit) | `"get elements by type curtain wall frame"` | `elements_get_elements_by_type` |
| CurtainWallPanel listen (projektweit) | `"get elements by type curtain wall panel"` | `elements_get_elements_by_type` |
| CurtainWallSegment listen (projektweit) | `"get elements by type curtain wall segment"` | `elements_get_elements_by_type` |
| Profil-Attribute holen (für Frames) | `"get attributes by type profile"` | `attributes_get_attributes_by_type` |
| Element modifizieren | `"set details of elements"` | `elements_set_details_of_elements` |
| Element löschen | `"delete elements"` | `elements_delete_elements` |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` |
| Klassifikation setzen | `"set classifications of elements"` | `elements_set_classifications_of_elements` |
| Klassifikations-Systeme | `"get all classification systems"` | `classifications_get_all_classification_systems` |
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` |
| Property-Werte holen | `"get property values of elements"` | `properties_get_property_values_of_elements` |
| Bounding Box | `"get 2d bounding boxes of elements"` | `elements_get_2_d_bounding_boxes` |

**NICHT verwenden für CW-Hierarchie:** `elements_get_connected_elements` — das Tool funktioniert für Host-Hosted-Beziehungen (Wand → Fenster), liefert aber für CurtainWall-Sub-Elemente eine leere Antwort. Live verifiziert 2026-05-20.

---

## Typische Parameter pro Element-Typ

### CurtainWall (Top-Level) — `CurtainWallSettings`

Für `typeSpecificDetails` in `elements_set_details_of_elements`. Alle Felder optional.

| Feld | Typ | Beschreibung |
|---|---|---|
| `begCoordinate` | `{x: m, y: m}` | Startpunkt (2D, kein Z). |
| `endCoordinate` | `{x: m, y: m}` | Endpunkt (2D, kein Z). |
| `height` | `number` (m) | Höhe der Fassade. |
| `bottomOffset` | `number` (m) | Abstand Fassaden-Fuß zu Story-Level. |
| `angle` | `number` (rad) | Neigungswinkel (für geneigte Fassaden). |
| `layerIndex` | `number` | Layer-Index (Float, 1-basiert). |

<!-- VERIFY --> Vollständige CurtainWallSettings-Felder über Discovery und Schema-Read in der Session bestätigen — Basis-Felder wie oben sind erwartet, aber AC29-spezifische Ergänzungen möglich.

### CurtainWallSegment — `CurtainWallSegmentSettings`

| Feld | Typ | Beschreibung |
|---|---|---|
| `gridMeshAngle` | `number` (rad) | Winkel des internen Rasters. |
| `gridMeshOffset` | `number` (m) | Raster-Offset. |

<!-- VERIFY --> Segment-Settings sind CW-interne Geometrie-Parameter — Auswirkungen auf Frame/Panel-Layout immer im Test-Sandkasten prüfen, nicht direkt im Realprojekt.

### CurtainWallFrame — `CurtainWallFrameSettings`

| Feld | Typ | Beschreibung |
|---|---|---|
| `profileAttrIndex` | `number` | Index des Profil-Attributs (aus `attributes_get_attributes_by_type` mit `Profile`). |
| `profileAttributeId` | `{guid: "..."}` | Alternativ: Profil-GUID direkt. |
| `cutEndLine` | `boolean` | Rahmen-Abschneidung. |

<!-- VERIFY --> Exakte Feldnamen für Frame-Settings in AC29 via Discovery bestätigen.

### CurtainWallPanel — `CurtainWallPanelSettings`

| Feld | Typ | Beschreibung |
|---|---|---|
| `buildingMaterialIndex` | `number` | Index des Building-Material-Attributs. |
| `buildingMaterialAttributeId` | `{guid: "..."}` | Alternativ: Building-Material-GUID. |
| `thickness` | `number` (m) | Paneel-Dicke. |
| `verticalOffset` | `number` (m) | Vertikaler Versatz des Paneels. |
| `horizontalOffset` | `number` (m) | Horizontaler Versatz. |

<!-- VERIFY --> Panel-Material kann über Building-Material (impliziert Surface) oder direkt über Surface-Attribut zugewiesen werden — Disambiguierung siehe [`surfaces-materials.md`](surfaces-materials.md).

### CurtainWallJunction / CurtainWallAccessory

Beide nur in spezialisierten CurtainWall-Konfigurationen vorhanden. Settings-Felder über Discovery in der konkreten Session ermitteln. <!-- VERIFY -->

---

## Worked Example — CurtainWall (Top-Level) lesen

<!-- 2026-05-20 verifiziert AC29 -->

Wir listen alle CurtainWalls und lesen dann Properties des ersten Treffers. `elements_get_details_of_elements` ist in AC29 wegen eines Pydantic-Bugs unzuverlässig — daher Property-Workaround (Details: wall-operations.md Gotcha 1).

**Schritt 1 — Alle CurtainWalls listen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": <port>,
    "params": {
      "elementType": "CurtainWall"
    }
  }
)
```

Response: Liste von `{elementId: {guid: "..."}}`. Paginierung vollständig durchziehen (`next_page_token`). Die Top-Level-GUIDs für alle Folge-Operationen im Arbeitsgedächtnis behalten (SAFE-05).

**Schritt 2 — Property-IDs holen:**

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}]
    }
  }
)
```

**Schritt 3 — Property-Werte lesen:**

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}],
      "properties": [
        {"propertyId": {"guid": "<height-property-guid>"}},
        {"propertyId": {"guid": "<layer-property-guid>"}}
      ]
    }
  }
)
```

**Schritt 4 — Bounding Box für Geometrie:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_2_d_bounding_boxes",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}]
    }
  }
)
```

Liefert `{xMin, yMin, xMax, yMax}` in Metern — gibt die 2D-Ausdehnung der Fassade im Grundriss.

---

## Worked Example — Sub-Elemente eines CurtainWall finden

<!-- 2026-05-20 verifiziert AC29 -->

Dies ist das **Kern-Pattern** für alle CurtainWall-Sub-Operationen. Ein einziger Call liefert alle Sub-Elemente gruppiert nach Typ.

**WARNUNG:** `elements_get_connected_elements` hier NICHT verwenden — liefert leer für CurtainWall-Hierarchie. Nur `elements_get_subelements_of_hierarchical_elements` funktioniert.

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_subelements_of_hierarchical_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}]
    }
  }
)
```

**Response-Struktur:**

```json
{
  "subelements": [
    {
      "cWallSegments": [
        {"elementId": {"guid": "<segment-guid>"}}
      ],
      "cWallFrames": [
        {"elementId": {"guid": "<frame-guid-1>"}},
        {"elementId": {"guid": "<frame-guid-2>"}},
        "..."
      ],
      "cWallPanels": [
        {"elementId": {"guid": "<panel-guid-1>"}},
        "..."
      ]
    }
  ]
}
```

Bei Fassaden mit Ecken, T-Stößen oder Accessories erscheinen zusätzlich:
```json
{
  "cWallJunctions": [...],
  "cWallAccessories": [...]
}
```

**Live-Test-Resultat (2026-05-20, Port 19723, Default-Sandkasten-Fassade):**
- 1 CurtainWall → 1 Segment + 31 Frames + 12 Panels in einer Antwort.
- `cWallJunctions` und `cWallAccessories` nicht in Response enthalten — in einer Default-CurtainWall ohne Sondergeometrie nicht generiert.

**Nach dem Call:**
- `cWallSegments`-GUIDs → für Segment-Operationen.
- `cWallFrames`-GUIDs → für Frame-Profil-Zuweisung (31 Stück bei einer normalen Fassade).
- `cWallPanels`-GUIDs → für Panel-Material-Zuweisung.
- Alle GUIDs im Arbeitsgedächtnis behalten (SAFE-05) — nicht erneut Discovery-Calls absetzen.

**Hinweis — Pattern gilt auch für andere hierarchische Elemente:** `Stair`, `Railing`, `Beam` (mit BeamSegments), `Column` (mit ColumnSegments) — gleicher Tool-Name, gleiche Aufruf-Struktur.

---

## Worked Example — CurtainWallPanel modifizieren

<!-- VERIFY --> Panel-Settings-Felder noch nicht in AC29 live getestet — Discovery in der Session vor dem ersten Schreib-Aufruf.

Wir weisen einem Panel ein anderes Building-Material zu. Voraussetzung: Sub-Elemente via vorherigem Worked Example bekannt (SAFE-05).

**Schritt 1 — Verfügbare Building-Materials ermitteln:**

```python
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": <port>,
    "params": {
      "attributeType": "BuildingMaterial"
    }
  }
)
```

Response: `{attributes: [{attributeId: {guid: "..."}, name: "Glas klar", ...}, ...]}`.
Gewünschtes Material anhand `name` identifizieren, `guid` merken.

**Schritt 2 — Confirm (SAFE-01) vor dem Schreiben:**

```
Ich werde folgendes ändern:
- CurtainWallPanel <panel-guid>
  Building-Material: (aktuell) → "Glas klar"

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 3 — Panel-Material setzen (nach `ja`):**

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "<panel-guid>"},
          "details": {
            "typeSpecificDetails": {
              "buildingMaterialAttributeId": {"guid": "<building-material-guid>"}
            }
          }
        }
      ]
    }
  }
)
```

**Bulk-Panel-Update (mehrere Panels gleichzeitig):** Mehrere Panels in `elementsWithDetails` akkumulieren — jedes mit demselben `buildingMaterialAttributeId`. Confirm zeigt Summary-Format für > 10 Panels (siehe [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Confirm-Format).

**Material vs. Composite:** Building-Material impliziert eine Standard-Oberfläche. Wenn eine Composite-Zuweisung (mehrschichtiger Aufbau) gewünscht ist, Workflow in [`surfaces-materials.md`](surfaces-materials.md) nachschlagen — dort ist die Disambiguierung ausgearbeitet.

---

## Worked Example — CurtainWallFrame modifizieren

<!-- VERIFY --> Frame-Settings-Felder noch nicht in AC29 live getestet — Discovery in der Session bestätigen.

Wir weisen einem Frame ein anderes Profil-Attribut zu. Typischer Use-Case: Pfosten sollen ein schmaleres Profil erhalten.

**Schritt 1 — Verfügbare Profile ermitteln:**

```python
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": <port>,
    "params": {
      "attributeType": "Profile"
    }
  }
)
```

Response: `{attributes: [{attributeId: {guid: "..."}, name: "CW-Pfosten 50x100", ...}, ...]}`.
Profil anhand `name` identifizieren, `guid` merken.

**Schritt 2 — Confirm (SAFE-01):**

```
Ich werde folgendes ändern:
- CurtainWallFrame <frame-guid>
  Profil: (aktuell) → "CW-Pfosten 50x100"

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 3 — Frame-Profil setzen (nach `ja`):**

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "<frame-guid>"},
          "details": {
            "typeSpecificDetails": {
              "profileAttributeId": {"guid": "<profile-guid>"}
            }
          }
        }
      ]
    }
  }
)
```

**Alle Frames einer Fassade auf dasselbe Profil setzen:**

1. Sub-Elemente holen (Worked Example 2) → alle `cWallFrames`-GUIDs.
2. `elementsWithDetails`-Liste mit allen Frame-GUIDs bauen (gleiches `profileAttributeId` für alle).
3. Confirm: „31 CurtainWallFrames → Profil: (aktuell) → CW-Pfosten 50x100".
4. Nach `ja` ausführen.

Bei > 10 Frames erscheint der Summary-Confirm automatisch.

---

## Worked Example — CurtainWallJunction operations

**Hinweis: Junctions existieren nur in bestimmten CurtainWall-Geometrien.** Eine einfache gerade Fassade generiert keine Junctions — sie entstehen bei Ecken (L-förmige Fassaden), T-Stößen oder Kreuzungspunkten mehrerer Fassadenelemente. Im Live-Test (2026-05-20, Default-Sandkasten) waren `cWallJunctions` nicht in der Sub-Element-Response enthalten.

**Existenz prüfen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_subelements_of_hierarchical_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}]
    }
  }
)
```

Wenn `cWallJunctions` im Response vorhanden und nicht leer: Junctions sind verfügbar.

**Junctions projektweit listen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": <port>,
    "params": {
      "elementType": "CurtainWallJunction"
    }
  }
)
```

**Junction-Properties lesen** (Standard-Property-Workaround): `properties_get_all_property_ids_of_elements` mit Junction-GUID → dann `properties_get_property_values_of_elements`.

**Junction modifizieren:** <!-- VERIFY --> Verbindungs-Typ ist GDL-Parameter-getrieben. `elements_set_details_of_elements` mit `typeSpecificDetails` — Felder über Discovery ermitteln. Alternativ `elements_set_gdl_parameters_of_elements` (Cave: Pydantic-Bug bei großen Param-Sets — Gotcha G-10). Wenn Edit-Call einen Modal-Dialog öffnet: Code 4001 → Gotcha G-12.

---

## Worked Example — CurtainWallSegment + Accessory Operations

### CurtainWallSegment

**Hinweis:** Eine einfache CurtainWall hat typischerweise genau 1 Segment; komplexere Fassaden mit horizontalen Unterteilungen können mehrere haben.

**Segment-GUID holen:** `get_subelements_of_hierarchical_elements` mit Top-Level-GUID → `cWallSegments[0].elementId.guid`.

**Segment-Geometrie abfragen:** `elements_get_2_d_bounding_boxes` mit Segment-GUID → `{xMin, yMin, xMax, yMax}`.

**Segment modifizieren (Raster-Winkel):** <!-- VERIFY -->

```python
# Confirm vorher zeigen (SAFE-01)
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "<segment-guid>"},
          "details": {
            "typeSpecificDetails": {
              "gridMeshAngle": 0.5236  # 30 Grad in Radiant
            }
          }
        }
      ]
    }
  }
)
```

**SAFE-Hinweis:** Segment-Geometrie-Änderungen beeinflussen das gesamte Frame/Panel-Layout. Immer erst im Sandkasten testen, nicht direkt im Realprojekt.

### CurtainWallAccessory

**Hinweis:** Accessories erscheinen nur bei explizit angelegten Zubehör-Elementen (Beschläge, Griffgarnituren, Sonderelemente). Im Live-Test (2026-05-20) waren `cWallAccessories` in der Standard-Fassade nicht vorhanden.

**Existenz prüfen und Accessory-GUIDs holen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_subelements_of_hierarchical_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}]
    }
  }
)
# → cWallAccessories[...].elementId.guid falls vorhanden
```

**Accessory-Properties lesen** folgt dem Standard Library-Object-Pattern — Accessories sind Library-Object-basiert. Vollständiges Pattern: [`library-objects.md`](library-objects.md).

**Accessory-Position modifizieren:** <!-- VERIFY -->

```python
# Confirm vorher (SAFE-01)
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "<accessory-guid>"},
          "details": {
            "typeSpecificDetails": {
              "position": {"x": <m>, "y": <m>, "z": <m>}
            }
          }
        }
      ]
    }
  }
)
```

---

## Worked Example — CurtainWall klassifizieren

<!-- 2026-05-20 verifiziert AC29 (Klassifikations-Flow — identisch zu wall-operations.md) -->

Wir setzen die Klassifikation der Top-Level-Fassade im Projekt-Klassifikations-System (`SAB_Klassifizierung_29`).

**WICHTIG:** Klassifikation auf dem Top-Level propagiert **nicht** automatisch auf Sub-Elemente. Wenn Sub-Elemente ebenfalls klassifiziert werden sollen, ist ein separater Bulk-Pass nötig (siehe [Bulk-Klassifizierung § Sub-Element-Klassifikation](#bulk-klassifizierung-stub)).

**Schritt 1 — System-GUID ermitteln:** `classifications_get_all_classification_systems` → `{classificationSystemId: {guid: "<system-guid>"}, name: "SAB_Klassifizierung_29"}`. Nie hartkodieren.

**Schritt 2 — Aktuelle Klassifikation lesen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}],
      "classificationSystemIds": [{"classificationSystemId": {"guid": "<system-guid>"}}]
    }
  }
)
```

**Schritt 3 — Ziel-Klassen-GUID:** `classifications_get_all_classifications_in_system` mit System-GUID → vollständige Klassenhierarchie mit GUIDs.

**Schritt 4 — Confirm (SAFE-01):**

```
Ich werde folgendes klassifizieren:
- CurtainWall <top-level-cw-guid>
  Klassifikation (SAB_Klassifizierung_29): (keine) → "Fassade"

Hinweis: Sub-Elemente (Frames, Panels, Segmente) werden NICHT automatisch mit-klassifiziert.

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 5 — Klassifikation setzen (nach `ja`):** <!-- VERIFY -->

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithClassifications": [
        {
          "elementId": {"guid": "<top-level-cw-guid>"},
          "classifications": [
            {
              "classificationSystemId": {"guid": "<system-guid>"},
              "classificationItemId": {"guid": "<fassade-klassen-guid>"}
            }
          ]
        }
      ]
    }
  }
)
```

---

## Worked Example — CurtainWall löschen (mit Sub-Element-Pre-Check)

**SAFE-04 zwingend.** Ein CurtainWall-Delete löscht alle Sub-Elemente (Segments, Frames, Panels, Junctions, Accessories) automatisch mit. Ohne Pre-Check weiß der User nicht, was alles verschwindet.

**Schritt 1 — Sub-Element-Counts vor Delete ermitteln (SAFE-04):**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_subelements_of_hierarchical_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}]
    }
  }
)
```

Response auswerten:
- `cWallSegments.length` → Anzahl Segmente
- `cWallFrames.length` → Anzahl Frames
- `cWallPanels.length` → Anzahl Paneele
- `cWallJunctions.length` → Anzahl Knotenpunkte (falls vorhanden)
- `cWallAccessories.length` → Anzahl Zubehör (falls vorhanden)

**Schritt 2 — Confirm mit Sub-Element-Counts (SAFE-01 + SAFE-04):**

```
Ich werde folgendes löschen:
- CurtainWall <top-level-cw-guid>
  Mit-gelöscht (Archicad-Verhalten): 1 Segment, 31 Frames, 12 Panels
  (keine Junctions, keine Accessories) — Total: 44 Sub-Elemente
  Rückgängig via Archicad-Undo.

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 3 — Delete erst nach ausdrücklichem `ja`:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<top-level-cw-guid>"}}]
    }
  }
)
```

Sub-Elemente müssen nicht separat gelöscht werden — Archicad entfernt sie automatisch beim Top-Level-Delete. **Nur die Top-Level-GUID in den Delete-Call übergeben.**

**Mehrere CurtainWalls löschen:** Mehrere Top-Level-GUIDs in `elements` akkumulieren. Pro CurtainWall vorher `get_subelements` aufrufen, Counts pro CW in der Confirm-Summary bündeln.

---

## Bulk-Klassifizierung (Stub)

Vollständiges Read → Filter → Group → Confirm → Apply-Muster: [`../reference/bulk-operations.md`](../reference/bulk-operations.md).

### Bulk-Klassifizierung Top-Level-CurtainWalls

Kurzfassung:

1. Alle CurtainWalls via `elements_get_elements_by_type` (`elementType: "CurtainWall"`) listen — paginiert.
2. Layer/Geometrie-Properties lesen für Gruppenbildung (z. B. Fassade Nord vs. Süd).
3. Confirm: Summary-Format für > 10 Elemente.
4. `elements_set_classifications_of_elements` mit Top-Level-GUIDs.

### Sub-Element-Klassifikation — separater Pass

Klassifikation propagiert nicht automatisch auf Sub-Elemente. Falls Frames, Panels etc. ebenfalls klassifiziert werden sollen:

1. Alle CurtainWalls listen → Sub-Elemente via `get_subelements_of_hierarchical_elements` sammeln.
2. Sub-GUIDs nach Typ trennen (alle Frame-GUIDs, alle Panel-GUIDs etc.).
3. Pro Typ: Bulk-Classify mit eigenem Confirm-Pass.

Hinweis: In der Praxis werden Sub-Elemente selten direkt klassifiziert — die Klassifikation greift meist auf Top-Level. Sub-Element-Klassifikation nur bei explizitem User-Wunsch oder bei IFC-Exporten mit Sub-Element-Granularität.

---

## Gotchas pro Element-Typ

### CurtainWall (Top-Level)

**G-01 — Create nicht via MCP v29.**
`elements_create_curtain_walls` existiert nicht. User zeichnet manuell mit dem Curtain-Wall-Werkzeug in Archicad (nicht das normale Wand-Werkzeug — das sind separate Werkzeuge). Danach Top-Level-GUID via `elements_get_elements_by_type` holen und weiterarbeiten.

**G-02 — Delete löscht alle 44+ Sub-Elemente automatisch.**
Kein Vorwarning von Archicad selbst — das SAFE-04-Pre-Check-Pattern in diesem Recipe ist der einzige Schutz. Bei IFC-Exporten können verwaiste CW-Einträge ohne Sub-Elemente stille Strukturfehler erzeugen.

**G-03 — `elements_get_details_of_elements` unzuverlässig in AC29.**
Gleicher Pydantic-Bug wie bei Wänden. Property-basiertes Lesen verwenden (Worked Example 1). Memory: `~/.claude/projects/-Users-ap/memory/issue_archicad_mcp_get_details_bug.md`.

### CurtainWallSegment

**G-04 — Segment-Geometrie-Änderungen haben Kaskaden-Effekte.**
Segmente steuern die interne Raster-Geometrie der Fassade. Eine Änderung am Raster-Winkel oder Raster-Offset verschiebt automatisch alle Frames und Panels des Segments — Positionen und Größen ändern sich. Immer erst im Sandkasten testen, nie direkt im Realprojekt. Bei unerwarteten Ergebnissen: Archicad-Undo vor weiteren Operationen.

**G-05 — Projektweit-Listing ohne Top-Level-Zuordnung.**
`elements_get_elements_by_type` mit `CurtainWallSegment` liefert alle Segmente — aber ohne Information, zu welchem Top-Level-CurtainWall sie gehören. Für gezielte Segment-Operationen immer über `get_subelements_of_hierarchical_elements` vom Top-Level aus navigieren (SAFE-05).

### CurtainWallFrame

**G-06 — 31 Frames in einer normalen Fassade sind keine Ausnahme.**
Eine Default-CurtainWall generiert sofort viele Frames (live: 31 Stück). Bulk-Frame-Updates sind der Normalfall — Einzelframe-Update nur wenn der User gezielt einen Rahmen ansprechen will. Confirm-Summary bei > 10 Frames automatisch.

**G-07 — Profil-Attribut-Index vs. GUID.**
Profile-Felder heißen in manchen AC-Versionen `profileAttrIndex` (Float, 1-basiert) und in anderen `profileAttributeId` (GUID-Objekt). Discovery in der Session klärt das Schema — nicht raten. GUID-basierter Zugriff ist robuster gegen Attribute-Reihenfolge-Änderungen.

### CurtainWallPanel

**G-08 — Building-Material vs. Surface-Zuweisung.**
Ein Panel kann über ein Building-Material (mit implizierter Oberfläche) oder direkt über ein Surface-Attribut verwaltet werden. Welches Feld der MCP-Wrapper akzeptiert: via Discovery in der Session prüfen. Vollständige Disambiguierung: [`surfaces-materials.md`](surfaces-materials.md).

### CurtainWallJunction

**G-09 — Junctions nur bei komplexer CW-Geometrie.**
In einfachen geraden Fassaden (die häufigste Konfiguration) sind keine Junctions vorhanden. `elements_get_elements_by_type` mit `CurtainWallJunction` liefert dann ein leeres Ergebnis. Existenz immer erst via `get_subelements_of_hierarchical_elements` prüfen.

**G-10 — GDL-Parameter-Bug trifft Junctions potenziell.**
Junction-Verbindungstyp ist GDL-Parameter-gesteuert. `elements_get_gdl_parameters_of_elements` hat in AC29 einen Pydantic-Validierungs-Bug bei großen Param-Sets. Bei Junctions mit vielen Parametern kann das Tool fehlschlagen — Workaround: Schedule-Export-Pipeline (siehe [`../reference/schedule-pipeline.md`](../reference/schedule-pipeline.md)).

### CurtainWallAccessory

**G-11 — Accessories sind Library-Object-basiert.**
Für alle Property- und GDL-Parameter-Operationen auf Accessories: Pattern aus [`library-objects.md`](library-objects.md) verwenden. Subtype-Identifikation über Library-Part-Name.

### Modal-Dialog-Blockade (Code 4001) — allgemeine Warnung

**G-12 — Live verifiziert 2026-05-20 gegen AC29.**

Wenn ein MCP-Call mit folgendem Fehler zurückkommt:

```
Error 4001: Invalid program status (there is an open modal dialog: Attribute-Manager)
```

Bedeutet: Archicad hat einen modalen Dialog offen. **Kein Retry** — bringt nichts.

Reaktion:
1. Dialog-Name aus der Fehlermeldung extrahieren (im Beispiel: „Attribute-Manager").
2. User informieren: „Ein modaler Dialog ist offen: `Attribute-Manager`. Bitte schließen — OK wenn Änderungen behalten werden sollen, Abbrechen wenn nicht."
3. **Daten-Verlust-Risiko kommunizieren:** Wenn der Dialog Property-Definitions, Enum-Werte oder Profil-Definitionen editiert, verwirft Abbrechen die Änderungen.
4. **Selektion-Risiko:** Manche Dialoge löschen beim Abbrechen die aktive Selektion — Element-GUIDs vorher sichern.
5. Nach User-Bestätigung „fertig": Call wiederholen.

Bekannte Dialog-Auslöser bei CurtainWall-Arbeit: Attribute-Manager, Profil-Editor, Building-Material-Editor, Element-Settings-Dialog, Klassifikations-System-Manager. Vor langen Bulk-Operationen vorbeugend fragen: „Bitte schließe alle offenen Dialoge in Archicad."

---

## Verwandte Recipes

- [`wall-operations.md`](wall-operations.md) — Wand-Operationen. CurtainWalls und Wände sind in Archicad grundlegend verschiedene Elemente (verschiedene Werkzeuge, verschiedene Element-Typen, verschiedene Hierarchien) — aber Properties, Klassifikation und Delete-Pattern sind analog.
- [`openings.md`](openings.md) — Fenster und Türen sind Wand-hosted (nicht CurtainWall-Sub-Elemente). Eine Glasfassade ist ein CurtainWall; ein Fenster in einer Massivwand ist ein Opening. Die Grenze ist konzeptuell wichtig.
- [`library-objects.md`](library-objects.md) — CurtainWallAccessories sind Library-Object-basiert; GDL-Parameter-Pattern dort.
- [`surfaces-materials.md`](surfaces-materials.md) — Für Panel-Material-Disambiguierung (Building-Material vs. Surface).
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Read → Filter → Group → Confirm → Apply für Bulk-Klassifizierung und Bulk-Property-Updates.
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Confirm-Format, Modal-Dialog-Details (§ Modal-Dialoge), Capability-Tabelle, Fehlerklassen.
