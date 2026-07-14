# Wand-Operationen

Dieses Recipe deckt Lesen, Modifizieren, Löschen und Klassifizieren von Wänden ab.
**Update 2026-07-13:** Seit Tapir ≥ 1.5.3 / tapir-archicad-mcp ≥ 0.4.3 ist Wand-Erstellung
verfügbar: `elements_create_walls` (Pflicht: `begCoordinate`, `endCoordinate`, `zCoordinate`,
`height`, `thickness`; optional `referenceLineLocation`, `structureType`, `compositeId`,
`profileId`, `buildingMaterialId`). Ebenso neu: `elements_modify_walls`, `create_windows`,
`create_doors`, `create_openings`, `rotate_elements`, `get_element_preview_image`.
Die Aussage „kein Create per MCP v29" galt nur bis Tapir 1.4.0 — Details und Update-Prozedur
in [`../reference/mcp-extension.md`](../reference/mcp-extension.md).
(Worked Example für Create folgt nach erstem Live-Einsatz.)

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [Discovery-Anker](#discovery-anker)
4. [Typische Parameter](#typische-parameter)
5. [Worked Example — Wand lesen](#worked-example--wand-lesen)
6. [Worked Example — Wand modifizieren (Höhe ändern)](#worked-example--wand-modifizieren-höhe-ändern)
7. [Worked Example — Wand löschen (mit Hosted-Element-Check)](#worked-example--wand-löschen-mit-hosted-element-check)
8. [Worked Example — Wand klassifizieren](#worked-example--wand-klassifizieren)
9. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
10. [Gotchas](#gotchas)
11. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang

- Alle Wände einer Story oder des gesamten Projekts auflisten.
- Wand-Eigenschaften lesen (via Property-System-Workaround — `elements_get_details_of_elements` ist in AC29 unzuverlässig, siehe Gotchas).
- Wand-Eigenschaften modifizieren: Höhe, Dicke, Geometrie (Anfangs-/Endpunkt), Layer, Boden-Offset.
- Wand löschen, mit vorangehendem Hosted-Element-Pre-Check (SAFE-04).
- Wand-Klassifikation lesen oder setzen (im aktiven Klassifikations-System).

Wand erstellen: seit Tapir 1.5.3 via `elements_create_walls` möglich (siehe Hinweis oben); Worked Example folgt.

---

## Erforderlicher Warm-up-Kontext

Details in `../reference/workflow-context.md`.

| Feld | Warum es hier wichtig ist |
|------|---------------------------|
| **Port** | Jeder MCP-Call braucht `"port"`. |
| **Aktive Story** (floorIndex) | Filter `OnActualFloor` greift per Default; bei anderen Geschossen explizit angeben. |
| **Längeneinheit** | Alle Koordinaten und Höhen in **Meter** (verifiziert). User-Eingaben in cm/mm selbst umrechnen. |
| **Sichtbare Layer** | SAFE-03: Ziel-Layer nicht sichtbar → erst konfirmieren. |
| **Klassifikations-System** | Nur bei Klassifizierungs-Auftrag — System-GUID ist Pflicht für Classify-Calls. |

---

## Discovery-Anker

| Operation | Discovery-Query | Verifizierter Tool-Name |
|-----------|-----------------|------------------------|
| Wände listen | `"get elements by type wall"` | `elements_get_elements_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Element-Typ prüfen | `"get type of element"` | `elements_get_types_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` <!-- verifiziert 2026-07-14 --> |
| Property-Werte holen | `"get property values of elements"` | `properties_get_property_values_of_elements` <!-- verifiziert 2026-07-14 --> |
| Bounding Box | `"get 2d bounding boxes of elements"` | `elements_get2_d_bounding_boxes` (kein Unterstrich in „get2"!) <!-- verifiziert 2026-07-14 --> |
| Wand modifizieren | `"set details of elements"` | `elements_set_details_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Wand löschen | `"delete elements"` | `elements_delete_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Hosted Elemente prüfen | `"get connected elements"` | `elements_get_connected_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Klassifikation setzen | `"set classification of elements"` | `dev_set_classifications_of_elements` (Präfix `dev_`, nicht `elements_`!) <!-- verifiziert 2026-07-14 --> |
| Klassifikations-Systeme | `"get all classification systems"` | `classifications_get_all_classification_systems` <!-- 2026-05-19 verifiziert AC29 --> |

---

## Typische Parameter

### WallSettings (für `typeSpecificDetails` in `elements_set_details_of_elements`)

Alle Felder optional — nur die zu ändernden angeben.

| Feld | Typ | Beschreibung |
|------|-----|-------------|
| `begCoordinate` | `{x: m, y: m}` | Startpunkt (Coord2D, kein Z). |
| `endCoordinate` | `{x: m, y: m}` | Endpunkt (Coord2D, kein Z). |
| `height` | `number` (m) | Wandhöhe in Metern. |
| `bottomOffset` | `number` (m) | Abstand Wand-Fuß zu Story-Level (Standard: 0). |
| `offset` | `number` (m) | Versatz Wand-Mittellinie zur Reference Line. |
| `begThickness` | `number` (m) | Wanddicke am Anfangspunkt. |
| `endThickness` | `number` (m) | Wanddicke am Endpunkt (bei gerader Wand = `begThickness`). |
| `structureType` | `string` | live gesehen: `"Basic"`, `"Composite"`. <!-- verifiziert 2026-07-14 --> |
| `geometryType` | `string` | live gesehen: `"Straight"`, `"Polygonal"`. <!-- verifiziert 2026-07-14 --> |

Details-Rahmen: `{"floorIndex": 0, "layerIndex": 3.0, "drawIndex": 1.0, "typeSpecificDetails": {...}}`.
`layerIndex` ist ein Float (1-basiert, nicht 0-basiert).

---

> **User sagt:** „Lies mir die Eigenschaften der Wand — Höhe, Dicke, Composite."

## Worked Example — Wand lesen

Wir lesen die Eigenschaften der Test-Wand `f1101930-e0bd-7044-a1f2-fdb20e520e21` über das
Property-System. `elements_get_details_of_elements` schlägt in AC29 wegen eines
Pydantic-Wrapper-Bugs fehl (Details: Gotcha 1 + Memory-Eintrag).

**Schritt 1 — Property-IDs holen:**

```
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]
    }
  }
)
```

Response: `{"propertyIds": [{"propertyId": {"guid": "aaa1..."}}, ...]}`.
Typische Wand-Properties: `Height`, `Thickness`, `Layer`, `Composite`, `BuildingMaterial`.
Property-IDs sind projekt-spezifisch.

**Schritt 2 — Werte für gewünschte Properties holen:** <!-- verifiziert 2026-07-14 --> (4.500er-Lauf)

```
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}],
      "properties": [{"propertyId": {"guid": "aaa1..."}}, {"propertyId": {"guid": "bbb2..."}}]
    }
  }
)
```

Response enthält pro Property Typ und Wert (z. B. `{"type": "length", "value": 2.8}`).

**Alternative — Typ + Lage ohne Property-System:**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_types_of_elements",
  arguments={"port": 19723, "params": {"elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]}}
)
```

<!-- 2026-05-19 verifiziert AC29 — liefert {"typesOfElements": [{"type": "Wall"}]}, auch wo get_details scheitert -->

Für die räumliche Lage: `elements_get_2_d_bounding_boxes` mit derselben GUID — gibt
`xMin`, `yMin`, `xMax`, `yMax` in Metern zurück. <!-- verifiziert 2026-07-14 -->

**Element-ID-Threading (SAFE-05):** Die GUID `f1101930-e0bd-7044-a1f2-fdb20e520e21` bleibt
im Arbeitsgedächtnis für alle Folge-Operationen dieser Session. Nicht erneut suchen.

---

> **User sagt:** „Ändere die Höhe dieser Wand von 2,80 auf 3,20 Meter."

## Worked Example — Wand modifizieren (Höhe ändern)

Wir ändern die Höhe der Test-Wand von 2,80 m auf 3,20 m. Gemäß SAFE-01 zeigen wir
zuerst den Confirm-Dialog.

**Confirm (SAFE-01) — vor dem Aufruf:**

```
Ich werde folgendes ändern:
- Wand f1101930-e0bd-7044-a1f2-fdb20e520e21  (Story 0, gerade, ca. 4m)
  Höhe: 2.80m → 3.20m

Ausführen? (ja / nein / details / abbrechen)
```

**Aufruf nach ausdrücklichem `ja`:**

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"},
        "details": {
          "typeSpecificDetails": {"height": 3.20}
        }
      }]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_set_details_of_elements -->

Response: `{"editedElements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]}`.
Die GUID bleibt nach dem Update stabil — für Folge-Operationen dieselbe ID weiterverwenden.

Weitere `typeSpecificDetails`-Beispiele: Dicke ändern (`"begThickness": 0.365, "endThickness": 0.365`),
Endpunkte verschieben (`"begCoordinate": {"x": 2.5, "y": 0.0}, "endCoordinate": {"x": 6.5, "y": 0.0}`).
Hinweis: Koordinaten haben kein `z`-Feld (Coord2D) — vertikale Lage über `floorIndex` + `bottomOffset`.

---

> **User sagt:** „Lösch die Wand, sag mir aber vorher welche Fenster und Türen mit weggehen."

## Worked Example — Wand löschen (mit Hosted-Element-Check)

SAFE-04 und SAFE-01 greifen hier gemeinsam. Die Test-Wand hostet das Fenster
`7185f21a-ca8f-6b44-a8a3-28d0610f0d82`. Archicad löscht gehostete Elemente beim Entfernen
der Wirt-Wand automatisch mit — das ist Archicads eigenes Verhalten. Wir müssen den User
daher informieren, bevor der Delete-Call abgesetzt wird.

**Schritt 1 — Hosted-Element-Pre-Check: Fenster suchen (SAFE-04):**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_connected_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}],
      "connectedElementType": "Window"
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_get_connected_elements -->

Response: `{"connectedElements": [{"elementId": {"guid": "f1101930..."}, "connectedElements": [{"elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"}}]}]}`.
Optional: separater Call mit `"connectedElementType": "Door"` und `"Opening"` falls relevant.

**Schritt 2 — Confirm mit Hosted-Element-Information (SAFE-01):**

```
Ich werde folgendes löschen:
- Wand f1101930-e0bd-7044-a1f2-fdb20e520e21  (Story 0, gerade, ca. 4m)
  Hostet folgende Elemente, die von Archicad automatisch mit-gelöscht werden:
  - Fenster 7185f21a-ca8f-6b44-a8a3-28d0610f0d82

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 3 — Delete erst nach ausdrücklichem `ja`:**

```
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_delete_elements -->

Das Fenster muss nicht separat gelöscht werden. Beim Wand-Löschen mitgerissene Öffnungen
können in IFC-Exporten stille Datenfehler erzeugen — der Pre-Check schützt davor.

---

> **User sagt:** „Klassifizier alle Wände nach SAB Innen/Außen."

## Worked Example — Wand klassifizieren

Wir setzen für die Test-Wand eine Klassifikation im Projekt-Klassifikations-System
(`SAB_Klassifizierung_29` im User-Setup — System-GUID ist projekt-spezifisch).

**Schritt 1 — Klassifikations-Systeme listen (Warm-up, falls System-GUID unbekannt):**

```
mcp__archicad__archicad_call_tool(
  name="classifications_get_all_classification_systems",
  arguments={"port": 19723, "params": {}}
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Response enthält `{"classificationSystemId": {"guid": "<system-guid>"}, "name": "SAB_Klassifizierung_29", ...}`.
System-GUID nie hartkodieren — immer per diesem Call ermitteln.

**Schritt 2 — Bestehende Klassifikation der Wand lesen:**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}],
      "classificationSystemIds": [{"classificationSystemId": {"guid": "<system-guid>"}}]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Response zeigt aktuelle Klasse oder `null`. Damit wissen wir den Ist-Zustand für den Confirm.

Falls die Klassen-GUID der Zielklasse (z. B. „Außenwand") noch unbekannt ist:
`classifications_get_all_classifications_in_system` mit der System-GUID aufrufen — gibt die
vollständige Klassenhierarchie mit GUIDs zurück (THN: 299 Items, eine Antwort). <!-- verifiziert 2026-07-14 -->

**Schritt 3 — Confirm (SAFE-01):**

```
Ich werde folgendes klassifizieren:
- Wand f1101930-e0bd-7044-a1f2-fdb20e520e21
  Klassifikation (SAB_Klassifizierung_29): <alt oder leer> → "Außenwand"

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 4 — Klassifikation setzen nach Bestätigung** <!-- 2026-05-21 live verifiziert AC29 — Schema-Form korrigiert; alte Form "elementsWithClassifications + classifications[]" wird vom Server mit Pydantic-Error abgelehnt -->:

```
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementClassifications": [{
        "elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"},
        "classificationId": {
          "classificationSystemId": {"guid": "<system-guid>"},
          "classificationItemId": {"guid": "<klassen-guid>"}
        }
      }]
    }
  }
)
```

`<klassen-guid>` ist die GUID der konkreten Klasse (z. B. „Außenwand"), nicht die System-GUID.
Beide GUIDs stammen aus den vorherigen Schritten — nie raten.

---

## Bulk-Klassifizierung (Stub)

Bulk-Klassifizierung aller Wände nach Innen/Außen ist ein wiederkehrender Kern-Workflow.
Die vollständigen Worked Examples folgen in Phase 5 nach Live-Verifikation. Bis dahin:
`../reference/bulk-operations.md` dokumentiert das universelle
Read → Filter → Group → Confirm → Apply-Pattern.

Kurzfassung für Wände: `elements_get_elements_by_type` (paginiert) → Properties +
Bounding-Box lesen → Layer/Zone-Zugehörigkeit via `elements_get_elements_related_to_zones`
auswerten → Gruppen bilden → Confirm (Summary-Format für > 10 Elemente) →
`elements_set_classifications_of_elements`.

**Unterklasse-Ableitung (Konstruktionstyp Beton-/Trockenbau-/MW-Wand): per MCP nur eingeschränkt möglich.** <!-- 2026-06-19 -->
Der Baustoff einer **Basic-Wand** ist auf Element-Ebene **nicht** lesbar:
`SurfaceAndMaterials_ComponentBuildingMaterialName` (514C85CB-…) liefert
`status: notAvailable` (es ist eine Pro-Komponenten-Property, kein Element-Wert).
`Construction_CompositeName` (704E9212-…) liefert **nur bei Composite-Wänden** einen
Namen, bei Basic-Wänden `null`. Folge: feine Unterklassen lassen sich für reine
Basic-Wände nicht zuverlässig ableiten. **Fallback-Heuristik: Layer-Name**
(`ModelView_LayerName`) — z. B. Layer „… Trennwand …" → Klasse „Trennwand".
Wo keine eindeutige Heuristik greift (Theke/Entwurf/SEO/2D/3D-Sammelsurium),
ehrlich auf die **Basisklasse „Wand"** klassifizieren statt zu raten.
Hinweis: `General_HotlinkAndElementID` ohne Hotlink-Präfix = lokales, klassifizierbares Element.

---

## Gotchas

1. **`elements_get_details_of_elements` ist in AC29 unzuverlässig.** Der MCP-Server
   blockiert die Response wegen eines Pydantic-Bugs (`extra='forbid'`): Archicad 29 gibt neue
   Felder zurück (`structureType`, `geometryType`, `arcAngle`, `slantAngle`,
   `verticalCurveHeight`), die das Schema nicht kennt. Stattdessen: Property-System,
   `elements_get_types_of_elements` oder `elements_get_2_d_bounding_boxes` verwenden.
   Memory: `~/.claude/projects/-Users-ap/memory/issue_archicad_mcp_get_details_bug.md`.
   <!-- 2026-05-19 -->

2. **Wand-Erstellung nicht via MCP v29.** `elements_create_walls` existiert nicht.
   User zeichnet manuell; wir holen die resultierende GUID und arbeiten damit weiter.
   Vollständige Capability-Tabelle in `../reference/mcp-conventions.md`.

3. **`begCoordinate`/`endCoordinate` sind Coord2D — kein Z-Feld.** Vertikale Lage
   wird über `floorIndex` + `bottomOffset` gesteuert. Ein `z`-Feld würde einen
   Argument-Fehler erzeugen.

4. **`layerIndex` ist ein Float (1-basiert).** Wert `3.0` = dritter Layer in der
   Projekt-Liste. `0.0` wäre ungültig. Index aus `attributes_get_attributes_by_type`
   mit `attributeType: "Layer"` ermitteln.

5. **Hosted-Element-Mit-Delete ist Archicad-Verhalten.** Wir löschen nur die Wand;
   das Fenster verschwindet durch Archicads Hosting-Logik. Trotzdem: SAFE-04-Pre-Check
   ist Pflicht, damit der User die Konsequenz kennt. Beim IFC-Export können verwaiste
   Öffnungs-Einträge zu stillen Datenfehlern führen (Pitfall P5 / Research).

6. **Klassifikations-System-GUIDs sind projekt-spezifisch.** Nie aus einem anderen
   Projekt übernehmen oder raten. Immer per `classifications_get_all_classification_systems`
   ermitteln — Cross-Pollution führt zu stillen Fehlklassifizierungen.

7. **Element-ID bleibt nach Update stabil (SAFE-05).** Nach `elements_set_details_of_elements`
   verändert sich die Wand-GUID nicht. Folge-Operationen (z. B. direkt danach klassifizieren)
   können dieselbe GUID weiterverwenden.

8. **Offene Wände ≠ Tragwerksbestand.** Bei „klassifiziere alle offenen Wände" zuerst
   per `ModelView_LayerName` gruppieren — der unklassifizierte Rest ist erfahrungsgemäß
   ein Sammelsurium (Theke-Entwurf, Bauzaun/Baustelleneinrichtung, Solid-Element-Operator-
   Hilfskörper, 2D-Darstellung, 3D-Massenstudie). Nicht blind auf eine Konstruktions-
   Unterklasse zwingen — SEO-Operatoren oder Bauzaun als „Betonwand" wäre sachlich falsch.
   Eindeutige Layer (Trennwand) → Unterklasse, Rest → Basis „Wand". <!-- 2026-06-19 -->

---

## Verwandte Recipes

- [`openings.md`](openings.md) — Fenster, Türen, Wandöffnungen (hosten auf Wänden).
- [`zones.md`](zones.md) — Zonen / Räume; Wand kann Zone-Grenze sein.
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Confirm-Format,
  Capability-Tabelle, Fehlerklassen.
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Bulk-Klassifizierungs-Pattern.
- [`../reference/workflow-context.md`](../reference/workflow-context.md) — Warm-up-Felder im Detail.

## CLASS-Worked-Example: 4.500 Wände (live 2026-07-14, THN)

Muster: Klasse „Wand" (Item-GUID aus System-Baum) via `SetClassificationsOfElements`
(300er-Batches) → DANACH Properties: Bauteilname (singleEnum, Gebäudeflügel per
kNN über Referenzpunkte) + SAB_Brandschutz (multiEnum, nächste Referenzwand
≤ 2,5 m, gleiche Etage, sonst ehrlich leer). Ergebnis 4.500/4.500 + 2.171
Brandschutz-Werte, 0 Fehler. Confirm-Schleife davor: Summary-Tabelle mit
Pro-Klasse-Counts, `ja` startet. Details: reference/referenzmodell-abgleich.md.
